# dune-weaver-firmware

Firmware that turns a **single MKS-DLC32 (ESP32) board** into the whole brain of a
[Dune Weaver](https://github.com/tuanchris/dune-weaver) kinetic sand table — no
Raspberry Pi, no separate WLED controller. It is a fork of
[**FluidNC**](https://github.com/bdring/FluidNC) **v3.9.5** with the sand-table host's
features ported down into the firmware itself.

## Dune Weaver Mini (this fork)

This fork also runs the **free 3D-printed
[DW Mini](https://makerworld.com/en/models/896314-mini-dune-weaver-not-your-typical-marble-run)**:
a bare ESP32 with 4 MB flash, two 28BYJ-48 motors on ULN2003 boards, and no SD
card or limit switches. The official firmware no longer supports this hardware, and
this fork is not supported by the Dune Weaver team. What it adds:

- **Unipolar stepper driver** (`motor: unipolar:`): a backport of FluidNC
  [PR #1797](https://github.com/bdring/FluidNC/pull/1797) that drives the ULN2003
  phase pins directly.
- **Pattern cache in flash instead of an SD card.** The Mini keeps two firmware
  slots for over-the-air updates; the slot that is not running holds a LittleFS
  pattern cache (about 1.9 MB). Patterns are stored zlib-compressed (about 4x
  smaller) and decompressed while playing.
- **Safe updates.** A new firmware stays on probation for its first 90 seconds
  after starting: if it crashes or restarts in that time, the board goes back to
  the previous firmware. The cache is unavailable during probation, then rebuilt
  empty (the host re-uploads patterns as they are needed).
- **Mini config**
  ([`configs/dune_weaver_mini/config.yaml`](configs/dune_weaver_mini/config.yaml)):
  the stock Mini pins and steps/mm, ThetaRho kinematics, and crash homing at
  startup.

The table only holds the patterns it is about to play, so it needs the host app
from the companion fork
[MrSco/dune-weaver-pi](https://github.com/MrSco/dune-weaver-pi) (branch
`mini-cache`), which keeps the full library and uploads patterns on demand. It
runs on a Raspberry Pi or as a Home Assistant add-on. A playlist must fit in the
cache (typically 15–25 patterns).

### Installing on a Mini

1. **Back up the board first** (the only way back to the old firmware), with the
   table connected over USB:
   `esptool --chip esp32 read-flash 0 0x400000 mini-backup.bin`
2. Download `dw-mini-full-4MB.bin` from the
   [latest release](https://github.com/MrSco/dune-weaver-firmware/releases/latest)
   and flash it: `esptool --chip esp32 write-flash 0x0 dw-mini-full-4MB.bin`
3. Join the table's **DuneWeaver-XXXX** Wi-Fi hotspot (password `12345678`), open
   **http://192.168.0.1** and choose your home Wi-Fi. The table restarts and
   appears as **DWMINI.local**.
4. Point the host app at it (it also finds the table on its own over mDNS).

Later releases install over Wi-Fi from the host app's settings. To build it
yourself, flash with `pio run -e sandtable_mini -t upload`, then copy
`configs/dune_weaver_mini/config.yaml` and `FluidNC/data/favicon.ico` into an
empty folder and upload that as the board's file system:
`PLATFORMIO_DATA_DIR=<folder> pio run -e sandtable_mini -t uploadfs`
(the same steps as [`release-mini.yml`](.github/workflows/release-mini.yml)).

## What it does

A Dune Weaver normally splits its brain across a host (Raspberry Pi running the
ThetaRho / `.thr` logic, playlists, scheduler, LEDs) and a motion controller. This
firmware folds all of that onto the ESP32:

- **ThetaRho kinematics** and `.thr` pattern playback
- **Playlists** with shuffle, pauses, adaptive clear-between-patterns
- **WS2812 / SK6812 RGBW LED** effects driven directly from the board (no WLED)
- **Scheduler / quiet hours** ("Still Sands")
- **Selectable homing** — limit-switch (sensor) or blind crash-into-stop, plus a
  configurable theta zero offset
- A stateless **HTTP/JSON API** for clients to drive the table

## Headless by design

The table holds **no web UI**. It exposes an HTTP API and is driven by clients (a
phone/desktop/web app, Home Assistant, scripts). Motion is controlled over stateless
HTTP — `POST`/`GET` to `/command` and the `/sand_*` action routes — and status is
polled from `/sand_status` (~1 Hz). The WebUI WebSocket is disabled (it raced motion
on this hardware).

## Documentation

- [`COMMANDS.md`](COMMANDS.md) — copy-paste command / HTTP cheatsheet
- [`API.md`](API.md) — the stable HTTP contract and `$Sand/Status` JSON schema
- [`PORTING.md`](PORTING.md) — what was ported from the Dune Weaver host and how
- Upstream FluidNC config reference: <http://wiki.fluidnc.com>

## Build & flash

Uses [PlatformIO](https://platformio.org/).

```bash
pio test -e tests                 # fast native unit tests (googletest + ASan/UBSan)
pio run  -e sandtable             # build the firmware
pio run  -e sandtable -t upload --upload-port /dev/cu.usbserial-XXXX   # flash over USB
```

The machine is described by a `config.yaml` on the board's littlefs (theta/rho axes,
homing, LEDs, etc.) — see the upstream FluidNC [config docs](http://wiki.fluidnc.com)
and the examples in this repo.

## Credits

Built on the work of others, and distributed under the same license (GPLv3):

- [**FluidNC**](https://github.com/bdring/FluidNC) by Bart Dring and contributors —
  the CNC firmware this is forked from.
- [**Grbl**](https://github.com/gnea/grbl) by Sungeun (Sonny) Jeon and Simen Svale
  Skogsrud — the motion-control foundation FluidNC itself builds on.
- The Wi-Fi / WebUI plumbing derives from [ESP3D-WebUI](https://github.com/luc-github/ESP3D-WEBUI).
- The [Dune Weaver](https://github.com/tuanchris/dune-weaver) project — the sand table
  this firmware is built to drive.

## License

GPLv3 — see [`LICENSE`](LICENSE). As a fork of FluidNC, all upstream copyrights and
the GPLv3 terms are preserved; see [`AUTHORS`](AUTHORS).
