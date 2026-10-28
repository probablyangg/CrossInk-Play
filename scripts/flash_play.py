#!/usr/bin/env python3
"""Install the packaged X4 Pro Play images over USB, preserving the NVS range."""

import argparse
from functools import reduce
import hashlib
import json
from operator import xor
from pathlib import Path
import struct
import subprocess
import sys


FLASH_BYTES = 0x1000000
APP_OFFSET = 0x10000
APP_LIMIT = 0x7F0000
PARTITIONS = {
    "nvs": (1, 2, 0x9000, 0x5000),
    "otadata": (1, 0, 0xE000, 0x2000),
    "app0": (0, 16, APP_OFFSET, APP_LIMIT),
    "app1": (0, 17, 0x800000, APP_LIMIT),
    "coredump": (1, 3, 0xFF0000, 0x10000),
}


def check_esp32s3(path: Path) -> bytes:
    """Validate the unsigned ESP image, including the checksum/hash trailer."""
    data = path.read_bytes()
    if len(data) < 24 or data[0] != 0xE9 or struct.unpack_from("<H", data, 12)[0] != 9:
        raise RuntimeError(f"Not an ESP32-S3 image: {path.name}")
    if not 1 <= data[1] <= 16 or data[23] not in (0, 1):
        raise RuntimeError(f"Invalid ESP image header: {path.name}")
    offset = 24
    checksum = 0xEF
    for _ in range(data[1]):
        if offset + 8 > len(data):
            raise RuntimeError(f"Truncated ESP segment header: {path.name}")
        length = struct.unpack_from("<I", data, offset + 4)[0]
        offset += 8
        if offset + length > len(data):
            raise RuntimeError(f"Truncated ESP segment: {path.name}")
        checksum = reduce(xor, memoryview(data)[offset:offset + length], checksum)
        offset += length
    padded_end = (offset + 16) & ~15
    expected_size = padded_end + (32 if data[23] else 0)
    if expected_size != len(data) or data[padded_end - 1] != checksum:
        raise RuntimeError(f"Invalid ESP image size/checksum: {path.name}")
    if data[23] and hashlib.sha256(data[:padded_end]).digest() != data[padded_end:]:
        raise RuntimeError(f"Invalid ESP image SHA256 trailer: {path.name}")
    return data


def check_partitions(path: Path) -> None:
    """Refuse tables that differ from Play or extend into the NVS sector."""
    data = path.read_bytes()
    if not data or len(data) > 0x1000 or len(data) % 32:
        raise RuntimeError("Partition image must fit the sector before NVS")
    found = {}
    digest_seen = False
    for offset in range(0, len(data), 32):
        record = data[offset:offset + 32]
        if record == b"\xff" * 32:
            if data[offset:] != b"\xff" * (len(data) - offset):
                raise RuntimeError("Unexpected data after partition table")
            break
        if record[:16] == b"\xeb\xeb" + b"\xff" * 14:
            if digest_seen or hashlib.md5(data[:offset]).digest() != record[16:]:
                raise RuntimeError("Invalid partition table MD5")
            digest_seen = True
            continue
        magic, kind, subtype, start, size, label, flags = struct.unpack("<HBBII16sI", record)
        if magic != 0x50AA or digest_seen or flags != 0:
            raise RuntimeError("Invalid partition table entry")
        name = label.split(b"\0", 1)[0].decode("ascii")
        if name in found:
            raise RuntimeError("Duplicate partition label")
        found[name] = (kind, subtype, start, size)
    if found != PARTITIONS or not digest_seen:
        raise RuntimeError("Partition table does not match the X4 Pro Play layout")


def check_initial_ota(path: Path) -> None:
    data = path.read_bytes()
    # The framework's boot_app0 initializer selects ota_0. A different 8 KiB
    # file could select the previous app instead of the image just installed.
    expected_first_entry = b"\x01\0\0\0" + b"\xff" * 24 + bytes.fromhex("9a984347")
    if len(data) != 0x2000 or data[:32] != expected_first_entry:
        raise RuntimeError("Unexpected initial OTA data; app0 must be selected")
    if data[32:0x1000] != b"\xff" * (0x1000 - 32):
        raise RuntimeError("Unexpected padding in initial OTA data")
    if data[0x1000:] != b"\0" * 4 + b"\xff" * (0x1000 - 4):
        raise RuntimeError("Unexpected second entry in initial OTA data")


def verify_package(root: Path):
    sums = {}
    for line in (root / "SHA256SUMS.txt").read_text().splitlines():
        digest, filename = line.split("  ", 1)
        if filename in sums or len(digest) != 64 or any(c not in "0123456789abcdef" for c in digest):
            raise RuntimeError("Invalid package checksum manifest")
        sums[filename] = digest

    def verified_file(filename):
        if Path(filename).name != filename:
            raise RuntimeError("Invalid package component path")
        path = root / filename
        if hashlib.sha256(path.read_bytes()).hexdigest() != sums.get(filename):
            raise RuntimeError("Missing or damaged package file: " + filename)
        return path

    info = json.loads(verified_file("build-info.json").read_text())
    if info.get("board") != "x4pro" or info.get("chip") != "esp32s3":
        raise RuntimeError("This installer accepts an X4 Pro Play package only")
    if info.get("flash_bytes") != FLASH_BYTES or info.get("app_slot_bytes") != APP_LIMIT:
        raise RuntimeError("Unexpected flash or application slot size")
    expected_offsets = ["0x0", "0x8000", "0xe000", "0x10000"]
    components = info["usb_components"]
    if [item["offset"] for item in components] != expected_offsets:
        raise RuntimeError("Unexpected flash offsets")
    if len({item["file"] for item in components}) != len(components):
        raise RuntimeError("A flash component was supplied more than once")
    paths = [verified_file(item["file"]) for item in components]
    if len(check_esp32s3(paths[0])) > 0x8000:
        raise RuntimeError("Bootloader overlaps the partition table")
    check_partitions(paths[1])
    check_initial_ota(paths[2])
    application = check_esp32s3(paths[3])
    if not 0x10000 <= len(application) <= APP_LIMIT:
        raise RuntimeError("Application does not fit a Play OTA slot")
    if b"CROSSPOINT-BOARD-V1:x4pro;" not in application:
        raise RuntimeError("Application has no X4 Pro board tag")
    if ("CrossInk Play " + info["version"]).encode() not in application:
        raise RuntimeError("Application has no matching CrossInk Play version tag")
    if info.get("application_bytes") != len(application):
        raise RuntimeError("Application size disagrees with build information")
    # All four offsets and maximum lengths are aligned to 4 KiB erase sectors.
    # Neither their data nor their erase ranges can cover 0x9000..0xdfff (NVS).
    return info, paths


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="X4 Pro serial port, such as COM3 or /dev/ttyACM0")
    parser.add_argument("--baud", type=int, default=460800)
    parser.add_argument("--check-only", action="store_true", help="Verify package files without contacting hardware")
    args = parser.parse_args()
    if not args.port and not args.check_only:
        parser.error("--port is required to install; use --check-only for offline verification")
    root = Path(__file__).resolve().parent
    info, paths = verify_package(root)
    command = [sys.executable, "-m", "esptool", "--chip", "esp32s3", "--port", args.port or "",
               "--baud", str(args.baud), "write-flash", "--flash-mode", "dio",
               "--flash-freq", "80m", "--flash-size", "16MB"]
    for item, path in zip(info["usb_components"], paths):
        command.extend([item["offset"], str(path)])
    print("Verified CrossInk Play " + info["version"] + " for Xteink X4 Pro.", flush=True)
    if args.check_only:
        print("Offline verification complete; no device was contacted.")
        return 0
    print("Writing bootloader, Play partitions, initial OTA data, and application.", flush=True)
    print("The SD card and internal NVS range are not written by this command.", flush=True)
    return subprocess.run(command).returncode


if __name__ == "__main__":
    raise SystemExit(main())
