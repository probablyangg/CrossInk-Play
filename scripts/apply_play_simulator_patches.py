"""Add Play's storage helpers to the pinned CrossInk simulator.

PlatformIO runs this only for x4-pro-simulator. Exact source hashes keep the
patch reproducible and stop the build if the dependency has drifted.
"""

import hashlib
from pathlib import Path
import subprocess
import sys


SIMULATOR_COMMIT = "27655f1d36ef5f8610fedd798d5564d04c8f4320"
TARGETS = (
    ("src/HalStorage.h",
     "efd7b72ab1d5915896554132dd7ff7694727bd6de12c58a25a4468cfded3eacf",
     "f016e806bc0a52ba786eea96e3c559050c1e9440cf9fed392d0e65b4d5397c24"),
    ("src/HalStorage.cpp",
     "7fa28a9c829437a2623832c77f4ebd2c1b363c5dd344db92b7ee65e66b5159a4",
     "b32a9c0f7f3326f30811125545fc3bb258f64f9e28d20c68f2b49c07d37cb69f"),
)


def apply_patches(project_dir, dependency_dir):
    dependency = Path(dependency_dir).resolve()
    patch = Path(project_dir).resolve() / "scripts/play_simulator_patches/0001-play-storage-api.patch"
    if not dependency.is_dir():
        raise RuntimeError("Missing pinned CrossInk simulator dependency")
    if (dependency / ".git").exists():
        commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=dependency, text=True).strip()
        if commit != SIMULATOR_COMMIT:
            raise RuntimeError("CrossInk Play simulator patches require " + SIMULATOR_COMMIT)

    states = []
    for relative, upstream, patched in TARGETS:
        target = dependency / relative
        target.resolve().relative_to(dependency)
        current = hashlib.sha256(target.read_bytes()).hexdigest()
        if current not in (upstream, patched):
            raise RuntimeError("Unrecognized Play simulator source: " + relative)
        states.append(current == patched)
    if any(states) != all(states):
        raise RuntimeError("Partially patched simulator; reinstall the pinned dependency")
    check = ["git", "apply", "--check"]
    if all(states):
        check.append("--reverse")
    subprocess.run(check + [str(patch)], cwd=dependency, check=True)
    if all(states):
        return
    subprocess.run(["git", "apply", str(patch)], cwd=dependency, check=True)
    for relative, _, patched in TARGETS:
        if hashlib.sha256((dependency / relative).read_bytes()).hexdigest() != patched:
            raise RuntimeError("Unexpected patched simulator source: " + relative)
    print("Applied CrossInk Play simulator storage patch")


if "Import" in globals():
    Import("env")  # noqa: F821 -- supplied by PlatformIO/SCons
    apply_patches(env["PROJECT_DIR"], env.subst("$PROJECT_LIBDEPS_DIR/$PIOENV/simulator"))  # noqa: F821
    # The simulator's Linux MD5Builder uses OpenSSL; macOS uses CommonCrypto.
    if sys.platform.startswith("linux"):
        env.Append(LIBS=["crypto"])  # noqa: F821
elif __name__ == "__main__":
    project = Path(__file__).resolve().parent.parent
    apply_patches(project, project / ".pio/libdeps/x4-pro-simulator/simulator")
