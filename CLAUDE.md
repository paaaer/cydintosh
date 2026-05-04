# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What This Is

**Cydintosh** is a Macintosh Plus emulator running on ESP32-based Cheap-Yellow-Display (CYD) hardware. It runs actual Mac Plus ROM and HFS disk images, with custom Mac OS applications communicating with the ESP32 over a memory-mapped IPC bus.

## Build System

PlatformIO with ESP-IDF framework. Multiple board variants are defined in `platformio.ini` (`cyd`, `cyd2usb`, `2432s028r`, `2432s028r_fast`). Use `cyd2usb` as the default target.

```bash
pio run -e cyd2usb              # Build firmware
pio run -e cyd2usb -t upload    # Flash firmware
pio run -e cyd2usb -t uploadfs  # Flash filesystem (LittleFS disk image)
pio device monitor              # Serial monitor
```

IDF component dependencies are declared in `src/idf_component.yml`; run `pio run` once to populate `managed_components/` (esp_lcd_ili9341, esp_lcd_touch_xpt2046, littlefs).

There are no automated tests — this is embedded firmware.

## Board Selection Warning

Choosing the wrong PlatformIO env produces wrong BGR/RGB color order and/or mirrored display. After switching env, always rebuild, re-upload firmware, AND re-upload filesystem (`uploadfs`) with the same env name.

## Initial Setup

```bash
git submodule update --init --recursive
ln -sf ../../../../include/m68kconf.h external/umac/external/Musashi/m68kconf.h
cd external/umac && make prepare && cd ../..
cp include/user_config.h.tmpl include/user_config.h
# Edit include/user_config.h with WiFi credentials, MQTT broker, etc.

# Flash the patched ROM (one-time, addresses for 240×320 display):
python3 tools/generate_patched_rom.py rom.bin -o rom_patched.bin
esptool --port /dev/ttyUSB0 write_flash 0x210000 rom_patched.bin

# Set up the HFS disk image:
cp cyd_800k.dsk data/disk.img
```

## Architecture

### Three-Layer Design

1. **Emulation core** (`external/umac/` + Musashi 68k)  
   - `umac`: Mac Plus emulator — disc, ROM, SCC (serial), VIA (timer/interrupts)
   - `Musashi`: Motorola 68000 CPU emulator
   - `m68kconf.h` in `include/` is symlinked into the Musashi source tree

2. **ESP32 firmware** (`src/`, `lib/`, `include/`)  
   - `main.c`: entry point, boots all subsystems, starts the UMAC FreeRTOS task
   - `display.c`: converts 1-bit Mac framebuffer → ILI9341 RGB565 via DMA
   - `umac_ipc.c`: handles IPC commands dispatched from Mac apps
   - `mqtt_cyd.c`: MQTT client, parses Home Assistant weather topics
   - `mqtt_discovery.c`: publishes Home Assistant MQTT discovery config and handles backlight commands
   - `wifi_info.c`: WiFi scan and status responses (fills IPC response buffers)
   - `video.c`: thin bridge between umac framebuffer and `display_notify_update()`
   - `hw_control.c`: backlight PWM, RGB LED, NVS persistence
   - `touch_cyd.c`: XPT2046 touch → Mac mouse events (supports double-tap)
   - `disc_lfs.c`: LittleFS-backed disk image
   - `lib/lcd_cyd/`: custom ILI9341 LCD init/config library (`lcd_cyd_init()` called from main)

3. **Mac applications** (`mac-app/`)  
   - Built with Retro68 (m68k cross-compiler)
   - `CydCtlApp/`: controls backlight and LED from Mac side
   - `WeatherApp/`: displays weather data fetched via IPC
   - `WiFiApp/`: WiFi network scanner
   - `common/`: shared IPC client library used by all Mac apps

### IPC Protocol

Mac apps communicate with ESP32 firmware via a 256-byte memory-mapped region at `0xF00000`. Protocol defined in `include/umac_ipc.h`: command byte + length + payload. Commands are namespaced: 0x0x system, 0x1x WiFi, 0x2x Weather, 0x3x HW control. `umac_ipc.c` on the ESP32 side polls this region and dispatches responses.

### Display Pipeline

UMAC produces a 240×320 1-bit monochrome framebuffer → `display_task()` converts packed bits to 16-bit RGB565 in vertical strips → pushed to ILI9341 via SPI DMA. Frame updates are triggered with `xTaskNotifyGive()`.

### Storage Layout

| Region | Content |
|--------|---------|
| Flash 0x210000 | Patched Mac Plus ROM (128 KB) |
| LittleFS (`data/`) | 800 KB HFS disk image (`disk.img`) |
| NVS | Persistent hardware state (backlight %, LED RGB) |

## Key Configuration

`include/user_config.h` (not committed, created from `.tmpl`) contains WiFi SSID/password, MQTT broker address, and Home Assistant topic prefixes.

Hardware pin assignments are in `include/hw.h`.

## Rebuilding the Mac Disk Image

```bash
tools/update-disk.sh   # Rebuilds Retro68 apps, mounts HFS image, copies binaries
```

Requires: Retro68 toolchain, HFS tools (`hmount`, `hcopy`, `hdel`).

## Code Style

`.clang-format` is present — run clang-format before committing C changes. Logging uses ESP-IDF's `ESP_LOGx(TAG, ...)` pattern. Concurrency is FreeRTOS tasks + queues; use `ESP_ERROR_CHECK` for IDF API calls.
