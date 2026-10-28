# Install CrossInk Play on an Xteink X4 Pro

This guide is for **CrossInk Play 0.1.0-alpha.1**, built for the **Xteink X4 Pro
with ESP32-S3 and 16 MB flash**. It is a desktop-validated alpha; physical-device
checks are still required. X3/X4 ESP32-C3 images and X4 Classic/Sticky images are
different firmware.

## Before installing

Copy the SD card to your computer, including the hidden `/.crosspoint/` folder
that contains reading progress, statistics, settings, and many game saves.
Keep the original CrossInk X4 Pro firmware available for reverting. The
installer does not format or write the SD card.

Use a USB data cable and keep the reader powered throughout flashing. This
initial installation requires working USB data; an app-only SD update cannot
install a different partition table.

## Package contents

The package is named `crossink-play-x4-pro-0.1.0-alpha.1.zip`. Extract it before
running the installer.

| File | Purpose |
| --- | --- |
| `flash_play.py` | Checks hashes, ESP32-S3 images and partition/erase bounds before USB installation |
| `bootloader.bin`, `partitions.bin`, `boot_app0.bin` | Boot and partition components for the first installation |
| `crossink-play-x4-pro-0.1.0-alpha.1-app.bin` | Application; also used for later SD updates on the Play layout |
| `crossink-play-x4-pro-0.1.0-alpha.1-usb.bin` | Merged image for tools that accept a complete image at address `0x0` |
| `SHA256SUMS.txt`, `build-info.json` | Integrity hashes, exact size, source provenance, and offsets |
| `GAMES.md`, `VALIDATION.md`, `licenses/` | Game/data guide, validation record, and license notices |

## First installation over USB

Install Python 3 if needed, then install the same esptool version used for this
package:

```sh
python3 -m pip install 'esptool==5.1.2'
```

Open a terminal in the extracted package directory. Verify the package without
contacting hardware:

```sh
python3 flash_play.py --check-only
```

Connect the X4 Pro. Find its serial port with:

```sh
python3 -m serial.tools.list_ports
```

Typical port names are `/dev/cu.usbmodem...` on macOS, `/dev/ttyACM0` on Linux,
and `COM3` on Windows. Use your actual port in the install command:

```sh
python3 flash_play.py --port /dev/ttyACM0
```

On Windows, use `py` in place of `python3` and a port such as `COM3`. Esptool
selects the ESP32-S3 ROM protocol and normally resets a connected native USB
device into download mode. If it cannot connect, use the X4 Pro's documented
download-mode procedure and retry. A slower transfer can be selected with
`--baud 115200`.

The script writes these ranges and verifies them through esptool:

| Offset | Image |
| --- | --- |
| `0x0000` | Bootloader |
| `0x8000` | Play partition table |
| `0xe000` | Initial OTA boot selection |
| `0x10000` | Play application |

It verifies the image chip IDs, the X4 Pro application tag, image integrity,
the partition table and each component's bounds before starting esptool. The
component writes and their 4 KiB sector erase ranges leave internal NVS at
`0x9000–0xdfff` alone; the installer does not issue a whole-chip erase. Let flashing complete before disconnecting. The reader should
restart into CrossInk; **Play** is available from Home.

### Using the merged USB image

The `-usb.bin` image includes the same boot components and application, with
padding between them. Flash it at **`0x0`**, with ESP32-S3, DIO, 80 MHz and 16 MB
flash settings. Because the padding crosses NVS, this method resets internal
NVS; the supplied component installer above preserves that range. Both methods
leave the SD card alone. Do not feed the merged image to the SD firmware updater.

## Why the first installation needs USB

CrossInk v1.6.1's standard layout has two 6,553,600-byte application slots.
Play reserves two 8,323,072-byte (`0x7f0000`) slots in the X4 Pro's 16 MB flash.
This alpha's application is about 7 MB, which exceeds either stock slot;
the exact size and remaining space are recorded in `build-info.json`.
The Play layout uses the old, unused internal SPIFFS allocation for the larger
application slots; CrossInk's books, settings and game saves use the SD card. The existing SD updater writes an
application slot and cannot move or resize partitions.

## Later updates

Once this Play layout is installed, copy a compatible **`-app.bin`** to the
SD card and select **Settings → System → SD Card Firmware Update**.
Use firmware explicitly built for the X4 Pro and this layout.

The upstream online update action is disabled in Play builds so it cannot
silently replace Play with standard CrossInk. The ordinary upstream installers
still install upstream CrossInk/CrossPlay; they do not install this fork.

## Reverting to CrossInk

To remove Play while retaining the larger partition layout, use the SD updater
with the original, board-tagged **CrossInk v1.6.1 X4 Pro application**. It fits
the larger slots and uses the same reader settings on the SD card. Do not feed
a merged/factory image into the SD updater.

To restore the complete upstream layout as well, use CrossInk's official USB
installation flow for **X4 Pro**. Restore your SD-card backup only if needed;
neither procedure requires deleting books or game saves. The app-only revert
uses the current partition table and is supported by the source's runtime
partition lookup; it has not been physically tested in this alpha. Restoring
the upstream USB layout also removes the larger Play slots, so installing Play
again then requires the initial USB procedure.

## First-run checks on the reader

Open your current book and confirm its position, fonts, and reading statistics.
Then open Play, start Minesweeper or another offline game, return Home, and
reopen the book. Check sleep/wake and the frontlight panel. Nearby multiplayer
requires a second compatible device. Connections, Wordle, and Trivia require
their data downloads unless compatible files are already on the SD card.

See `VALIDATION.md` for what was checked on the desktop and what remains to be
verified on hardware.
