# CrossInk Play 0.1.0-alpha.1 validation

Validation date: **2026-10-28**. Target: **Xteink X4 Pro / ESP32-S3**.
This is a desktop validation record. No physical reader was connected or flashed.

## Source baseline

| Component | Pinned source |
| --- | --- |
| CrossInk v1.6.1 | `9914146eeae7b46b300f475a16c32426fc02ec1f` |
| CrossPlay v1.14.2 | `0a1d17f41e1605d3b033d4327c81b3bcfb2ca2ab` |
| FreeInk SDK | `699370183fa3a0e33c9cb83a36f701bbb6022095` |
| CrossInk simulator | `27655f1d36ef5f8610fedd798d5564d04c8f4320` |

SDK and simulator changes are small, checked patches stored in this repository.
The game source provenance and adaptations are described in
[`src/apps_local/README.CrossInkPlay.md`](../src/apps_local/README.CrossInkPlay.md).

## Completed checks

| Check | Result |
| --- | --- |
| X4 Pro PlatformIO build | Passed with PlatformIO 6.2.0 and pinned pioarduino 55.03.37 |
| ESP32-S3 image validation | Correct chip ID, DIO/80 MHz/16 MB header, valid checksum and validation hash |
| Flash fit | Application fits both `0x7f0000` slots; exact package size is in `build-info.json` |
| Imported native game/support suites | 31 suites passed |
| Nearby multiplayer protocol/transport tests | Passed full matches, malformed/corrupt packets, and soak scenarios over local UDP |
| UI compatibility | All imported screen builders compile with `-Wall -Wextra -Werror` against the pinned SDK |
| UI layout and shelf access | 72 assertions passed for portrait/landscape, explicit header bounds, and all 25 game rows |
| Runtime rendering/input gates | 77 assertions passed, including asynchronous paint completion and overlay restoration |
| Actual patched SDK free-space query | Full card, filesystem errors, fresh values, and values exceeding 4 GB checked |
| SDK patch reproducibility | Clean/idempotent application succeeds; source drift and partial patch state are rejected |
| Online updater guard | Actual Play `OtaUpdater.cpp` compiled with strict host warnings; check/install return `NO_UPDATE` |
| Translations and Play icon | Generated using the repository's I18n and SDK icon generators |

The UDP test's first sandboxed attempt could not open sockets. Re-running with
local-loopback socket access passed. That checks protocol and transport logic,
not physical ESP-NOW radio behavior.

The firmware build retains warnings from upstream source, including integer
narrowing in reader UI and const qualifiers in the imported Michi C engine.
These are not build failures, and they are not silently reported as warning-free.

## Simulator integration

The opt-in `--play` scenario uses real simulated touch and physical-key events.
It opens Play from Lyra, Minimal, Dashboard, and Cover Grid; launches
Minesweeper; opens and closes the frontlight panel; reveals a cell; returns
through the shelf to Home; and reopens the EPUB. It compares reading position,
visible content, and stored reading statistics before and after the game trip.

The `--home-themes` scenario checks ten theme/scale transitions and compares
returned Home framebuffers with a fresh Home render.

**Integration run status: passed.** The Play journey completed in approximately
13.7 seconds; all ten Home theme/scale cases passed in approximately 11.6
seconds. The full reader framebuffers before and after Play were byte-identical
(PGM SHA256 `0a422d722a2a9e0b46f24210eeadee1d77c2062c52b7fe5dbc529dc52e1b7885`).
All eight captured screens were visually inspected for clipping and missing icons.

The Linux simulator uses unsigned plain `char`, matching the actual ESP32-S3
compiler, and links OpenSSL for its existing MD5 adapter. The Home-key smoke
waits for the real single-tap/double-tap disambiguation interval while continuing
normal event processing; this required no production input change.

```sh
pio run -e x4-pro-simulator
python3 scripts/run_simulator_smoke_test.py --env x4-pro-simulator --no-build --play --timeout 120
python3 scripts/run_simulator_smoke_test.py --env x4-pro-simulator --no-build --home-themes --timeout 90
```

The optional `--play-captures DIR` saves actual framebuffer images for visual
inspection, including Home, the shelf, Minesweeper, and the reader before/after.
The smoke runner uses a temporary SD-card directory and a checked-in EPUB fixture.

## Package checks

The packager verifies the X4 Pro board marker and Play version, image types,
application size, and the complete expected partition table before creating
USB and application images. The component installer checks hashes and flash
ranges before invoking esptool. `--check-only` never opens a device.

**Package run status: passed.** The finished package passes its own offline
installer verification. Nine offline review checks included rejection of a
wrong-chip image, corrupt/truncated data, a partition image overlapping NVS,
an unexpected OTA initializer, modified build information, and an NVS write
offset. The separate component ranges preserve NVS, including sector erase
boundaries. No serial device was opened during these checks.

## What remains to verify on hardware

- First USB installation and a later SD-card firmware update.
- Real e-ink waveform/ghosting, physical button and touch behavior, orientation,
  frontlight controls, and sleep/wake recovery.
- Battery drain, free heap under extended play, AI performance, and SD-card
  behavior across actual cards.
- Nearby multiplayer against a second compatible device using ESP-NOW.
- Live downloads and TLS behavior for Connections, Wordle, and Trivia.

Every game is compiled into the X4 Pro firmware and has its imported native
coverage where provided upstream. The interactive smoke exercises Minesweeper,
not a full manual playthrough of every game. No battery-life, frame-rate, or
on-device stability claim is made for this alpha.
