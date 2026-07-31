# AGENTS.md

## Cursor Cloud specific instructions

This repo is a single **embedded firmware** product (`Peanut2Shield`): C++/Arduino firmware
for a Waveshare ESP32-S3-Zero, built with [PlatformIO](https://platformio.org/). There is no
web/backend/frontend service, database, or long-running dev server. "Running" the product means
building the firmware and (on real hardware) flashing it. See `README.md` and `platformio.ini`
for the authoritative build/flash/monitor commands.

### Toolchain / invocation
- PlatformIO is installed via `pip install --user platformio`, so the `pio` CLI lives in
  `~/.local/bin`. That directory is added to `PATH` in `~/.bashrc`; if `pio` is not found in a
  fresh non-login shell, either `export PATH="$HOME/.local/bin:$PATH"` or invoke it as
  `python3 -m platformio`.
- The first `pio run` downloads the `espressif32` platform, the Xtensa toolchain, and libraries
  (hundreds of MB) into `~/.platformio`. This is slow only the first time; subsequent builds use
  the cache and take a couple of seconds. The update script only installs the PlatformIO CLI, so
  expect the toolchain to download lazily on the first build in a fresh VM.

### Build / test / lint
- Build (this is the primary validation): `pio run` (or `pio run -e waveshare-esp32-s3-zero`).
  Output: `.pio/build/waveshare-esp32-s3-zero/firmware.bin`.
- There are **no** automated tests, no `pio test` environments, and **no** lint/format config
  (clang-tidy/cppcheck/clang-format) in this repo. Compile success is the validation signal.
- The `CONFIG_ESP_TASK_WDT_TIMEOUT_S redefined` compiler warning during the build is benign
  (the build flag intentionally overrides the SDK default) and does not indicate a problem.

### End-to-end / hardware
- True end-to-end testing requires physical hardware not present in the cloud VM: the ESP32-S3-Zero
  board, a TiVo Stream 4K remote, and an Nvidia Shield TV paired over BLE. In the VM, validation
  stops at a successful firmware build/link.
- Flashing/monitoring commands (`pio run --target upload`, `pio device monitor`) require a USB
  serial device and cannot run in the VM.

### USB update kit packaging (`usb-drive/`)
- `bash usb-drive/pack/pack.sh` (wrapper: `pack-usb-drive.sh`) rebuilds the firmware and copies the
  binaries into `usb-drive/firmware/` plus a merged `combined.bin`. The `*.sh` scripts are not
  marked executable in git, so invoke them with `bash <script>` rather than `./<script>`.
- Known quirk: when `usb-drive/pack/vendor/boot_app0.bin` already exists (it is vendored in the
  repo), `pack.sh` aborts with a "same file" `cp` error under `set -e` before creating
  `combined.bin`. The packaging kit is an optional distribution path; the core `pio run` build is
  unaffected. The kit's build outputs are gitignored.
