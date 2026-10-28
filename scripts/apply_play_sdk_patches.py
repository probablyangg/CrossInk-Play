"""Add Play's fresh free-space query to the pinned FreeInk SDK.

Used as a PlatformIO pre: script by Play environments only. The SDK stays at
699370183fa3a0e33c9cb83a36f701bbb6022095; this patch adds one guarded API without
changing reader behavior. A source mismatch stops the build. All targets are
validated before applying the patch, and subsequent runs leave files intact.

May also be run directly from a source archive:
    python3 scripts/apply_play_sdk_patches.py
"""

import hashlib
import os
from pathlib import Path
import subprocess


SDK_COMMIT = "699370183fa3a0e33c9cb83a36f701bbb6022095"
PATCH = "0001-report-free-space-errors.patch"
TARGETS = (
    ("libs/hardware/SDCardManager/include/SDCardManager.h",
     "4d6d5ced3b8763b645519baf74c34a4385b78478766cce6fa79d2c28accdfed2",
     "58ae0d4ddd4d6faf223c1b120a37bd1cc85de0534cb25993e0f6834f00680610"),
    ("libs/hardware/SDCardManager/src/SDCardManager.cpp",
     "24e058799d39d5758fab004af2289371ae20e826f9fd0491c13128e8694ec6ed",
     "f9d12f2d08e9d2b20f669c5b5223864276d7d1fd9394bb91f9be4be68bffbfe8"),
)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def apply_patches(project_dir):
    project = Path(project_dir).resolve()
    sdk = project / "freeink-sdk"
    patch = project / "scripts" / "play_sdk_patches" / PATCH
    if sdk.is_symlink() or not sdk.is_dir():
        raise RuntimeError("CrossInk Play requires a local freeink-sdk checkout")

    # Archives have no Git metadata; the exact target hashes below still pin
    # every changed byte. Check the SDK commit as well when metadata exists.
    process_env = {key: os.environ[key] for key in ("HOME", "PATH", "TMPDIR", "LANG") if key in os.environ}
    process_env.update(GIT_CEILING_DIRECTORIES=str(project),
                       GIT_CONFIG_GLOBAL=os.devnull, GIT_CONFIG_NOSYSTEM="1")
    if (sdk / ".git").exists():
        result = subprocess.run(["git", "rev-parse", "HEAD"], cwd=sdk, env=process_env,
                                capture_output=True, text=True, check=True)
        if result.stdout.strip() != SDK_COMMIT:
            raise RuntimeError("CrossInk Play SDK patches require commit " + SDK_COMMIT)

    states = []
    for relative, upstream, patched in TARGETS:
        target = sdk / relative
        try:
            target.resolve().relative_to(sdk)
        except ValueError:
            raise RuntimeError("Escaping Play SDK patch target: " + relative)
        if not target.is_file():
            raise RuntimeError("Missing Play SDK patch target: " + relative)
        current = digest(target)
        if current not in (upstream, patched):
            raise RuntimeError("Unrecognized Play SDK source: " + relative)
        states.append(current == patched)
    if any(states) != all(states):
        raise RuntimeError("Partially patched Play SDK; restore the pinned SDK before building")

    command = ["git", "apply", "--check"]
    if all(states):
        command.append("--reverse")
    result = subprocess.run(command + [str(patch)], cwd=sdk, env=process_env,
                            capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError("Play SDK patch does not apply: " + PATCH + "\n" + result.stderr)
    if all(states):
        return

    subprocess.run(["git", "apply", str(patch)], cwd=sdk, env=process_env, check=True)
    for relative, _, patched in TARGETS:
        if digest(sdk / relative) != patched:
            raise RuntimeError("Unexpected patched Play SDK source: " + relative)
    print("Applied CrossInk Play SDK patch: " + PATCH)


if "Import" in globals():
    Import("env")  # noqa: F821 -- supplied by PlatformIO/SCons
    apply_patches(env["PROJECT_DIR"])  # noqa: F821
elif __name__ == "__main__":
    apply_patches(Path(__file__).resolve().parent.parent)
