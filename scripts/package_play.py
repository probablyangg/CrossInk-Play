#!/usr/bin/env python3
"""Package a completed X4 Pro Play build; never connects to a device."""

from __future__ import annotations

import argparse
import configparser
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import zipfile


from flash_play import APP_LIMIT, APP_OFFSET, check_esp32s3, check_initial_ota, check_partitions, verify_package
from play_upstream import MaintenanceError, validate_manifest


ROOT = Path(__file__).resolve().parents[1]


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load_upstream_provenance(project: Path) -> dict:
    """Read recorded baselines and reject dependency provenance that is stale."""
    try:
        upstreams = validate_manifest(json.loads((project / "maintenance/upstreams.json").read_text()))
    except (OSError, ValueError, KeyError, TypeError, MaintenanceError) as error:
        raise RuntimeError("Cannot package without valid upstream provenance: " + str(error)) from error
    sdk = project / "freeink-sdk"
    if not (sdk / ".git").exists():
        raise RuntimeError("Cannot verify upstream provenance: initialize the FreeInk SDK submodule")
    try:
        sdk_commit = subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=sdk, text=True, stderr=subprocess.PIPE
        ).strip()
    except (OSError, subprocess.CalledProcessError) as error:
        raise RuntimeError("Cannot verify the FreeInk SDK commit for this package") from error
    if sdk_commit != upstreams["dependencies"]["freeink_sdk"]:
        raise RuntimeError("The FreeInk SDK checkout does not match maintenance/upstreams.json")
    try:
        config = configparser.ConfigParser(interpolation=None)
        config.read_string((project / "platformio.ini").read_text())
        simulator_deps = config.get("simulator-base", "lib_deps")
        simulator_pins = re.findall(
            r"^\s*simulator\s*=\s*https://github\.com/uxjulia/crossink-simulator(?:\.git)?#([0-9a-f]{40})\s*$",
            simulator_deps, flags=re.MULTILINE,
        )
    except (OSError, configparser.Error) as error:
        raise RuntimeError("Cannot verify the simulator dependency in platformio.ini") from error
    if simulator_pins != [upstreams["dependencies"]["simulator"]]:
        raise RuntimeError("The simulator pin does not match maintenance/upstreams.json")
    return upstreams


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=ROOT / ".pio/build/x4-pro")
    parser.add_argument("--output-dir", type=Path, default=ROOT / "dist-publish")
    args = parser.parse_args()
    upstreams = load_upstream_provenance(ROOT)
    upstream_bytes = (json.dumps(upstreams, indent=2) + "\n").encode()
    version_text = (ROOT / "include/PlayVersion.h").read_text()
    version = re.search(r'#define CROSSINK_PLAY_VERSION "([A-Za-z0-9.-]+)"', version_text).group(1)
    name = f"crossink-play-x4-pro-{version}"
    output = args.output_dir.resolve() / name
    output.mkdir(parents=True, exist_ok=True)
    build = args.build_dir.resolve()
    core = Path(os.environ.get("PLATFORMIO_CORE_DIR", str(Path.home() / ".platformio")))
    boot_app = core / "packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin"

    firmware = check_esp32s3(build / "firmware.bin")
    if b"CROSSPOINT-BOARD-V1:x4pro;" not in firmware or b"CrossInk Play " + version.encode() not in firmware:
        raise RuntimeError("Application is missing the expected X4 Pro and CrossInk Play version tags")
    if len(firmware) > APP_LIMIT:
        raise RuntimeError("Application exceeds the smaller Play OTA slot")
    if len(check_esp32s3(build / "bootloader.bin")) > 0x8000:
        raise RuntimeError("Bootloader overlaps the partition table")
    check_partitions(build / "partitions.bin")
    check_initial_ota(boot_app)

    application_name = f"{name}-app.bin"
    components = [
        (0x0, build / "bootloader.bin", "bootloader.bin"),
        (0x8000, build / "partitions.bin", "partitions.bin"),
        (0xE000, boot_app, "boot_app0.bin"),
        (APP_OFFSET, build / "firmware.bin", application_name),
    ]
    for _offset, source, destination in components:
        shutil.copy2(source, output / destination)

    esptool_script = core / "packages/tool-esptoolpy/esptool.py"
    tool_python = core / "penv" / ("Scripts/python.exe" if os.name == "nt" else "bin/python")
    python = str(tool_python) if tool_python.is_file() else sys.executable
    merge = [python, str(esptool_script), "--chip", "esp32s3", "merge-bin",
             "--flash-mode", "dio", "--flash-size", "16MB", "--flash-freq", "80m",
             "-o", str(output / f"{name}-usb.bin")]
    for offset, _source, destination in components:
        merge.extend([hex(offset), str(output / destination)])
    subprocess.run(merge, check=True)

    for source, destination in (
        ("docs/play-installation.md", "INSTALL.md"),
        ("docs/play-validation.md", "VALIDATION.md"),
        ("docs/play-games.md", "GAMES.md"),
        ("scripts/flash_play.py", "flash_play.py"),
        ("partitions-x4-pro-play.csv", "partitions-x4-pro-play.csv"),
    ):
        shutil.copy2(ROOT / source, output / destination)
    (output / "UPSTREAMS.json").write_bytes(upstream_bytes)

    # Include the SDK's root notices and the actual linked Arduino libraries,
    # not only notices found in the application source tree.
    licenses = [(ROOT / "LICENSE", Path("LICENSE")),
                (ROOT / "freeink-sdk/LICENSE", Path("freeink-sdk/LICENSE")),
                (ROOT / "freeink-sdk/NOTICE", Path("freeink-sdk/NOTICE"))]
    notice_roots = [(ROOT / name, Path(name))
                    for name in ("src/apps_local", "assets", "assets_local", "lib", "freeink-sdk/libs")]
    library_dir = build.parent.parent / "libdeps" / build.name
    notice_roots.append((library_dir, Path("platformio-libraries") / build.name))
    for source_root, destination_root in notice_roots:
        if not source_root.is_dir():
            raise RuntimeError(f"Missing dependency sources for license collection: {source_root}")
        for path in source_root.rglob("*"):
            if path.is_file() and (path.name.upper().startswith(("LICENSE", "LICENCE", "OFL", "COPYING", "NOTICE", "THIRD-PARTY"))
                                   or path.name == "UPSTREAM-README.md"):
                licenses.append((path, destination_root / path.relative_to(source_root)))
    for path, relative in licenses:
        destination = output / "licenses" / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, destination)

    source_commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
    tracked_diff = subprocess.check_output(["git", "diff", "--binary", "HEAD"], cwd=ROOT)
    manifest = {
        "product": "CrossInk Play", "version": version, "board": "x4pro", "chip": "esp32s3",
        "flash_bytes": 0x1000000, "app_slot_bytes": APP_LIMIT,
        "application_bytes": len(firmware), "application_free_bytes": APP_LIMIT - len(firmware),
        "source_commit": source_commit, "tracked_diff_sha256": hashlib.sha256(tracked_diff).hexdigest(),
        "crossink_base": upstreams["crossink"]["commit"],
        "crossplay_base": upstreams["crossplay"]["commit"],
        "crossink_tag": upstreams["crossink"]["tag"],
        "crossplay_tag": upstreams["crossplay"]["tag"],
        "dependencies": upstreams["dependencies"],
        "upstream_manifest": "UPSTREAMS.json",
        "upstream_manifest_sha256": hashlib.sha256(upstream_bytes).hexdigest(),
        "physical_device_tested": False,
        "usb_components": [{"offset": hex(offset), "file": destination} for offset, _, destination in components],
    }
    (output / "build-info.json").write_text(json.dumps(manifest, indent=2) + "\n")
    files = sorted(path for path in output.rglob("*") if path.is_file() and path.name != "SHA256SUMS.txt")
    (output / "SHA256SUMS.txt").write_text("".join(f"{sha256(path)}  {path.relative_to(output).as_posix()}\n" for path in files))
    verify_package(output)  # The shipped offline installer must accept the exact package.
    archive = args.output_dir.resolve() / f"{name}.zip"
    with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as package:
        for path in sorted(output.rglob("*")):
            if path.is_file():
                package.write(path, Path(name) / path.relative_to(output))
    print(f"Packaged: {archive}")
    print(f"Application: {len(firmware):,} bytes; {APP_LIMIT - len(firmware):,} bytes free per slot")
    print(f"SHA256: {sha256(archive)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
