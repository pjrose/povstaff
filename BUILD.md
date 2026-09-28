# Build and flash

## Verified target and dependency pins

Target reported by the connected board: ESP32-S3 QFN56 revision v0.2, 240 MHz, 40 MHz crystal, **8 MB embedded flash**, USB Serial/JTAG. Select **Adafruit QT Py ESP32-S3 No PSRAM**, not the 4 MB/2 MB PSRAM variant. No PSRAM is required.

| Component | Version / setting |
| --- | --- |
| Arduino CLI | 1.5.1 |
| ESP32 Arduino core | 3.3.11 |
| Xtensa compiler | GCC 14.2.0, esp-x32 tool package 2601 |
| Adafruit DotStar | 1.2.5 |
| Adafruit NeoPixel | 1.15.5 |
| Adafruit BusIO | 1.17.4 (DotStar dependency) |
| Board | `esp32:esp32:adafruit_qtpy_esp32s3_nopsram` |
| Partition scheme | `default_8MB` — two 0x330000-byte app slots, 0x180000-byte LittleFS partition |
| USB mode | Hardware CDC and JTAG (`hwcdc`) |
| USB CDC on boot | Enabled (`cdc`) |
| Erase all flash | Disabled (`none`) |

Wi-Fi, networking, WebServer, ESPmDNS, ArduinoOTA, Update, Preferences, FS, LittleFS, SPI and Wire come with the core. No ArduinoJson, asynchronous web framework, cloud service or CDN is required. The stock `pov-esp32` and `ESPxWebFlMgr` dependencies are not part of this build.

## Build on Windows

The script uses `arduino-cli` on PATH, or the CLI bundled with Arduino IDE in its standard Windows location. It does not flash unless a port is explicitly supplied.

```powershell
cd D:\povstaff
# First build only: installs the pinned core/libraries.
.\tools\build.ps1 -Setup
# Subsequent builds:
.\tools\build.ps1
```

If needed, pass `-ArduinoCli 'C:\path\to\arduino-cli.exe'`. Output is in `.build/release/`. Keep the application file **POV4.1.ino.bin** for OTA. `merged.bin`, partition binaries and bootloader binaries must never be sent to OTA.

Installed release 5.0.4 retains the open staff AP, dashboard without a login, and password-free ArduinoOTA verified in 5.0.3. No staff credentials need configuring before flashing. Home Wi-Fi credentials are entered in the dashboard and saved in NVS; passwords are never returned by the status API. Application flashing with erase disabled preserves these saved settings. The 5.0.4 build uses 1,120,041 bytes of program storage and 122,312 bytes of static RAM.

Keep **Hardware CDC and JTAG** selected: USB power handling uses the core's `HWCDC::isPlugged()` host detection, independently of whether a serial monitor is open. With a USB host detected, the firmware caps brightness at 16/255 and estimated LED current at 500 mA. A 4.45–5.50 V reading is then classified as USB supply rather than battery voltage. Installed 5.0.4 passed host detection, supply classification, output telemetry and live dashboard checks with the serial monitor closed. Visible LEDs and actual electrical current remain unmeasured; see [TESTING.md](TESTING.md).

Equivalent CLI command on any supported host:

```sh
arduino-cli compile --fqbn 'esp32:esp32:adafruit_qtpy_esp32s3_nopsram:PartitionScheme=default_8MB,USBMode=hwcdc,CDCOnBoot=cdc,EraseFlash=none' --output-dir .build/release code/POV4.1
```

## First USB-C flash

1. Connect the QT Py with a data-capable USB-C cable. The verified port after hardware CDC re-enumeration is **COM5**; recheck Device Manager if it changes.
2. Build with your configuration, then run:

   ```powershell
   .\tools\build.ps1 -Port COM5
   ```

3. If the uploader cannot connect, hold **BOOT**, press/release **RESET**, release **BOOT**, then retry. Do not hold BOOT across an ordinary power cycle.
4. After uploading, reset or power cycle. A serial monitor at 115200 baud prints firmware version, reset reason, filesystem, IMU, battery, brightness/current settings and network information. USB enumeration can miss the earliest messages; the dashboard also exposes live status.
5. On a new configuration, join the open staff AP and open `http://192.168.4.1/`. If client mode is already saved, use its LAN address or `http://povstaff.local/`; flashing does not reset the Wi-Fi choice. Initialize storage explicitly only if the dashboard reports it unavailable, then upload sequences.

The CLI writes the bootloader, partition table and application on the initial USB upload. It does not load a LittleFS image. Changing partition layouts may invalidate old storage; a storage reset erases sequences only and requires an explicit dashboard confirmation.

## Arduino IDE

Install the pinned ESP32 core and Adafruit libraries, then open `code/POV4.1/POV4.1.ino`. Choose the board and Tools options in the table above. Keep all `.h` and `.cpp` files beside the sketch. The `.ino` name remains historical; this is version 5 firmware. Use Verify/Upload normally. No internal library edits are required.

## Subsequent browser OTA (including Android)

1. Build the same target/partition scheme. Copy **POV4.1.ino.bin** to the phone if needed.
2. Connect to the staff AP or its configured home LAN and open the dashboard.
3. In Firmware Update, choose the application `.bin` and upload. The staff blanks its main LEDs during the transfer. The UI shows transfer progress, then validation/result.
4. Only a completed, validated application update triggers reboot. Wait for the staff to return on its saved network, reconnect if necessary, and reload the page. Saved Wi-Fi settings take effect at this reboot too.

The browser sends an exact-size binary stream; failed/truncated transfers do not finalize an OTA slot. Firmware must fit the inactive 3.19 MiB slot. ESP32's image checks are used; images are not cryptographically signed and automatic rollback of a valid-but-buggy application is not configured. Keep USB access available during development.

## Subsequent Arduino OTA

Connect the development computer to the same network as the staff (its AP also works). In Arduino IDE, choose the **povstaff network port**, build and Upload. No OTA password is required. mDNS and UDP 3232 must be allowed on the computer; Arduino OTA also uses a TCP transfer connection back to the uploader. If discovery is unavailable, the core's `tools/espota.py` can address the IP directly:

```sh
python espota.py -i 192.168.4.44 -p 3232 -f POV4.1.ino.bin
# If the computer has multiple interfaces, specify its actual LAN IPv4 address:
python espota.py -i 192.168.4.44 -p 3232 -I 192.168.1.50 -P 32320 -f POV4.1.ino.bin
```

Use the script shipped with the pinned ESP32 core. Application updates only are supported; Arduino filesystem OTA is rejected to protect sequence storage.

Use `192.168.4.1` for a direct AP connection instead of the staff LAN address. The example `192.168.1.50` is a placeholder for the development computer's own IPv4 address. Browser OTA has completed on the physical staff in both AP and client modes. The official `espota.py` script also completed a transfer under Python 3.12 with explicit `-I` host binding and `-P 32320`, returning exit code zero. Existing Windows firewall block rules prevented the standalone `espota.exe` callback; no firewall settings were changed. See [TESTING.md](TESTING.md) for reboot/version verification and remaining acceptance checks.

## Audit of the original

The untouched source was built from a separate Git snapshot with core 3.3.11. It failed at link time with duplicate `RequestHandler` definitions from `detail/RequestHandlersImpl.h` and the old web file manager. The user explicitly authorized a breaking rewrite and a modern toolchain, so no obsolete-core compatibility work was pursued.

Audited old dependencies: `pov-esp32` commit `1ad8792102be660162d818d145dd2aabe72272ce`, file manager commit `fd6e763f57b534396b8585b0a103212c1c3f889c`. The POV library set brightness to 127 in `begin()`, used blocking blink delays, and had unchecked image-list copies/null paths. None are used in version 5.

References: [ESP32 Wi-Fi API](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/wifi.html), [ESP32 OTA](https://docs.espressif.com/projects/arduino-esp32/en/latest/ota_web_update.html), [LSM6DSO datasheet](https://www.st.com/resource/en/datasheet/lsm6dso.pdf).
