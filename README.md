# POV Staff 5

A new firmware for the **Island Robotics V4.1 staff / Adafruit QT Py ESP32-S3 with 8 MB flash**. It controls 72 logical DotStar pixels across four physical strips, with an LSM6DSO IMU, a local dashboard, and wireless updates. This is a breaking replacement of the original firmware.

Start with [BUILD.md](BUILD.md) for the first USB flash, [TESTING.md](TESTING.md) for verification, and [MIGRATION.md](MIGRATION.md) for differences from the original. The maintained sketch is `code/POV4.1/POV4.1.ino`.

## Connect and load sequences

1. Power on. By default the staff creates **POVSTAFFxxxx**; the suffix identifies the board.
2. Join this **open Wi-Fi network**, with no password. On Android, choose to stay connected even though this network has no Internet.
3. Open **http://192.168.4.1/** or **http://povstaff.local/**. The dashboard opens without a login.
4. If storage is uninitialized, explicitly initialize it in Settings. Mount failures never silently erase files.
5. Upload BMP sequences in the dashboard. Each must be **72 pixels wide, uncompressed 24-bit RGB, at most 296 rows**. Uploading multiple files queues them individually. The dashboard reports rejected files and progress.
6. Select POV mode and spin, or choose a procedural lighting mode. All valid uploaded images join the alphabetically ordered playlist. Set duration to zero to hold the selected image. Prefix names with `01-`, `02-`, etc. to control order.

`imagelist.txt` is no longer used. No filesystem flashing tool or separate uploader mode is needed. The two sample BMPs in `images/` can be uploaded directly.

To use home Wi-Fi, select **Client** in Settings, enter the SSID/password, save, then **power cycle**. Saving never changes the current connection. Client mode has a ten-second connection window, then enables the staff AP if unsuccessful. Losing the home network also enables the AP. Fallback stops client scans and stays in AP mode until the next boot, when the saved client choice is retried. The same dashboard, uploads and OTA are available on either connection. Client mode remains an explicit saved choice; factory default is always AP.

Installed version **5.0.4** adds USB-powered lighting to the password-free networking introduced in 5.0.3. The staff AP has no password, the dashboard has no login, and ArduinoOTA needs no password. Home Wi-Fi credentials remain saved privately in NVS. Anyone connected to the staff AP or able to reach it on the home LAN can control it and upload files or firmware.

The current bench setup uses saved home Wi-Fi; open **http://povstaff.local/** or **http://192.168.4.44/** while on that LAN. The numerical address can change with DHCP. See [TESTING.md](TESTING.md) for completed hardware checks and the remaining electrical and lighting checks.

## Lighting modes

| Mode | Behavior |
| --- | --- |
| Auto | Debounced motion selects POV; stationary horizontal selects LED Test; stationary upright selects Walking Stick. |
| POV | Motion-synchronized BMP playback with bounded phase advancement. |
| LED Test | Smooth rainbow, R/G/B/white pixel walk, fading channel test, or gradient fade. |
| Walking Stick | Calm moving gradient, with movement increasing activity. |
| Sword | Selectable blade color with shimmer and motion accents. |
| Flashlight | Warm-neutral white with a separate brightness cap and the shared current limit. |
| Solid / Breathing | Color picker controls steady or breathing light. |
| Rainbow / Chase | Procedural animations without an image file. |
| Battery | Brief, dim voltage-based state-of-charge meter. |
| Idle / Off | Main strips dark; onboard status indication remains available. |

Selecting a manual mode locks it. Unlocking returns to Auto. Color, requested/applied brightness, battery, IMU, Wi-Fi, heap, storage, and estimated current are visible on the dashboard. Procedural scenes target 100 updates/second. Uploads and firmware updates deliberately blank the main LEDs until the operation ends.

Startup takes control of DATA/CLK, sends black twice, averages supply samples, and briefly shows a dim meter when a battery reading is available before enabling normal output. At 3.40 V the meter fills approximately five of 72 pixels. Low/invalid voltage blocks normal light output. Battery-operation defaults are brightness 24/255, maximum 64/255, flashlight cap 20/255, and an estimated total LED-current budget of 1,800 mA. These are conservative starting settings in `Config.h`, **not measured electrical guarantees**.

Version 5.0.4 distinguishes a USB supply from a battery reading. A positively detected USB host and a measured supply of 4.45–5.50 V select USB power instead of treating the reading as an invalid battery. The dashboard shows supply voltage without inventing a battery percentage. Whenever a USB host is detected, brightness is capped at **16/255** and estimated LED current at **500 mA**, even if the voltage is in the battery range. Detection works without an open serial monitor. Battery operation retains its 3.20 V cutoff and 3.35 V recovery threshold; readings outside the supported battery/USB ranges keep the main LEDs black. Installed 5.0.4 correctly reported USB power and nonzero, capped output during bench tests with the serial monitor closed. Visible LED operation and actual electrical current still need physical measurement/observation; see [TESTING.md](TESTING.md).

## Development

The new implementation uses the supported ESP32 Arduino Wi-Fi, WebServer, LittleFS, Preferences, Update and ArduinoOTA APIs, plus Adafruit DotStar/NeoPixel. It removes `pov-esp32`, `ESPxWebFlMgr`, and direct inclusion of internal HTTP headers. Rendering, pattern definitions, sequence storage and networking are separate modules. See [API.md](API.md) for automation endpoints.

## Original project

This repository contains all files related to Persistence of Vision (POV) staff project. A detailed write-up of Version 1 of this project is available on  Instructables at [https://www.instructables.com/Persistence-of-Vision-LED-Staff/](https://www.instructables.com/Persistence-of-Vision-LED-Staff/); you can also find there photos  of the staff in action.

The original hardware/firmware version 4.1 is documented at
https://povstaff.readthedocs.io





## Structure of this repository  
This repository is organized as follows:

* `docs`: documentation
* `code`: this directory contains the source code for the staff software
* `hardware`:  design files for the custom PCBs used in
  the project, as well as STEP files for the 3d printed components.
* `images`: sample image(s) and  `imagelist.txt` file


## Authors and license
This project was created by Alexander Kirillov <irobotics.store@gmail.com>; it is
based on [earlier work](https://learn.adafruit.com/pov-dotstar-double-staff) of
Phillip Burgess and Erin St Blaine from Adafruit. The project is available under
MIT license; see LICENSE file for details.
