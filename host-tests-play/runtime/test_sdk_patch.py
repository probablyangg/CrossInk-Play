"""Exercise the build's patcher against disposable SDK source copies."""

import importlib.util
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


PROJECT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("play_sdk_patcher", PROJECT / "scripts/apply_play_sdk_patches.py")
PATCHER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PATCHER)


class PlaySdkPatchTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="crossink-play-sdk-test-")
        self.addCleanup(self.temp.cleanup)
        self.project = Path(self.temp.name)
        self.sdk = self.project / "freeink-sdk"
        self.patch = self.project / "scripts/play_sdk_patches" / PATCHER.PATCH
        self.patch.parent.mkdir(parents=True)
        shutil.copy2(PROJECT / "scripts/play_sdk_patches" / PATCHER.PATCH, self.patch)
        for relative, _, _ in PATCHER.TARGETS:
            target = self.sdk / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(PROJECT / "freeink-sdk" / relative, target)
        patched = [PATCHER.digest(self.sdk / relative) == expected
                   for relative, _, expected in PATCHER.TARGETS]
        self.assertEqual(any(patched), all(patched), "Local SDK has a partial patch")
        if all(patched):
            process_env = os.environ.copy()
            process_env["GIT_CEILING_DIRECTORIES"] = str(self.project)
            subprocess.run(["git", "apply", "--reverse", str(self.patch)], cwd=self.sdk,
                           env=process_env, check=True, capture_output=True)
        for relative, upstream, _ in PATCHER.TARGETS:
            self.assertEqual(PATCHER.digest(self.sdk / relative), upstream)

    def test_clean_apply_and_idempotent_second_run(self):
        PATCHER.apply_patches(self.project)
        before = {}
        for relative, _, patched in PATCHER.TARGETS:
            target = self.sdk / relative
            self.assertEqual(PATCHER.digest(target), patched)
            before[relative] = target.stat().st_mtime_ns
        PATCHER.apply_patches(self.project)
        for relative, _, patched in PATCHER.TARGETS:
            target = self.sdk / relative
            self.assertEqual(PATCHER.digest(target), patched)
            self.assertEqual(target.stat().st_mtime_ns, before[relative])

    def test_source_drift_stops_before_any_write(self):
        untouched, upstream, _ = PATCHER.TARGETS[0]
        changed = self.sdk / PATCHER.TARGETS[1][0]
        changed.write_bytes(changed.read_bytes() + b"\n// unreviewed SDK change\n")
        with self.assertRaisesRegex(RuntimeError, "Unrecognized Play SDK source"):
            PATCHER.apply_patches(self.project)
        self.assertEqual(PATCHER.digest(self.sdk / untouched), upstream)

    def test_partial_patch_stops_before_any_write(self):
        pristine = {relative: (self.sdk / relative).read_bytes() for relative, _, _ in PATCHER.TARGETS}
        PATCHER.apply_patches(self.project)
        first = PATCHER.TARGETS[0][0]
        (self.sdk / first).write_bytes(pristine[first])
        before = {relative: (self.sdk / relative).read_bytes() for relative, _, _ in PATCHER.TARGETS}
        with self.assertRaisesRegex(RuntimeError, "Partially patched Play SDK"):
            PATCHER.apply_patches(self.project)
        for relative, _, _ in PATCHER.TARGETS:
            self.assertEqual((self.sdk / relative).read_bytes(), before[relative])


if __name__ == "__main__":
    unittest.main()
