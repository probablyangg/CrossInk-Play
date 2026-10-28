#!/usr/bin/env python3
"""Offline checks that packaging records current, verified source provenance."""

import copy
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import package_play


class ProvenanceTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="play-package-provenance-")
        self.project = Path(self.temporary.name)
        self.upstreams = json.loads((package_play.ROOT / "maintenance/upstreams.json").read_text())
        (self.project / "maintenance").mkdir()
        (self.project / "freeink-sdk").mkdir()
        (self.project / "freeink-sdk/.git").write_text("fixture; git is mocked\n")
        self.write_manifest(self.upstreams)
        self.write_simulator(self.upstreams["dependencies"]["simulator"])
        self.git = patch.object(package_play.subprocess, "check_output",
                                return_value=self.upstreams["dependencies"]["freeink_sdk"] + "\n")
        self.git.start()

    def tearDown(self):
        self.git.stop()
        self.temporary.cleanup()

    def write_manifest(self, data):
        (self.project / "maintenance/upstreams.json").write_text(json.dumps(data))

    def write_simulator(self, commit):
        (self.project / "platformio.ini").write_text(
            "[simulator-base]\nlib_deps =\n"
            "  simulator=https://github.com/uxjulia/crossink-simulator#" + commit + "\n"
        )

    def test_records_updated_baselines_instead_of_hardcoded_first_release(self):
        changed = copy.deepcopy(self.upstreams)
        changed["crossink"].update(tag="v2.0.0", commit="a" * 40)
        changed["crossplay"].update(tag="v3.0.0", commit="b" * 40)
        self.write_manifest(changed)
        actual = package_play.load_upstream_provenance(self.project)
        self.assertEqual(actual["crossink"]["commit"], "a" * 40)
        self.assertEqual(actual["crossplay"]["tag"], "v3.0.0")
        self.assertEqual(actual["dependencies"], self.upstreams["dependencies"])

    def test_missing_manifest_does_not_fall_back_to_old_baselines(self):
        (self.project / "maintenance/upstreams.json").unlink()
        with self.assertRaisesRegex(RuntimeError, "valid upstream provenance"):
            package_play.load_upstream_provenance(self.project)

    def test_wrong_upstream_repository_is_rejected(self):
        changed = copy.deepcopy(self.upstreams)
        changed["crossplay"]["repository"] = "someone/another-firmware"
        self.write_manifest(changed)
        with self.assertRaisesRegex(RuntimeError, "Unexpected upstream repository"):
            package_play.load_upstream_provenance(self.project)

    def test_missing_dependency_is_rejected(self):
        changed = copy.deepcopy(self.upstreams)
        del changed["dependencies"]["simulator"]
        self.write_manifest(changed)
        with self.assertRaisesRegex(RuntimeError, "valid upstream provenance"):
            package_play.load_upstream_provenance(self.project)

    def test_sdk_checkout_must_match_recorded_dependency(self):
        package_play.subprocess.check_output.return_value = "c" * 40 + "\n"
        with self.assertRaisesRegex(RuntimeError, "SDK checkout does not match"):
            package_play.load_upstream_provenance(self.project)

    def test_simulator_pin_must_match_recorded_dependency(self):
        self.write_simulator("d" * 40)
        with self.assertRaisesRegex(RuntimeError, "simulator pin does not match"):
            package_play.load_upstream_provenance(self.project)


if __name__ == "__main__":
    unittest.main()
