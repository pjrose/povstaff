# Local dashboard API

HTTP port 80 on either the open staff AP or configured LAN. No login or Authorization header is required. Every mutation requires `X-POV-Request: 1`; this is a browser request check, not an authentication secret. No CORS access is granted. Ordinary POST bodies use `application/x-www-form-urlencoded`; binary uploads use the streaming contract below.

Successful mutations return `{"ok":true}`. Errors return `{"ok":false,"error":"..."}` and a non-2xx status. Missing mutation headers return HTTP 403. A deliberately rejected/aborted transfer can close the socket without a JSON body.

| Method | Path | Arguments / response |
| --- | --- | --- |
| GET | `/api/status` | `version`, `uptime` seconds, `heap` bytes, `engine`, `network`, `storage` objects |
| GET | `/api/files` | `files[]` (`index`, `name`, `bytes`, `width`, `height`, `active`), current selection, duration, count, error, catalogue `revision` |
| GET | `/api/files/content?name=example.bmp` | Raw validated BMP bytes for one allowed basename |
| GET | `/preview.js` | Browser preview module served from firmware flash |
| POST | `/api/renderer` | `name`: `baseline` or `dither`; switches actual LED output, resets timing counters; defaults to baseline at boot |
| POST | `/api/mode` | `name`: `auto`, `pov`, `led_test`, `idle`, `walking_stick`, `sword`, `flashlight`, `solid`, `breathing`, `rainbow`, `chase`, `battery`, `off` |
| POST | `/api/pattern` | `name`: registered pattern, including `rainbow`, `pixel_walk`, `channels`, `fade`, `breathing`, `chase`; affects LED Test |
| POST | `/api/brightness` | `value`: integer 0–64 by default, bounded by `Config::MAX_BRIGHTNESS` |
| POST | `/api/color` | `value`: six hex digits with optional `#` |
| POST | `/api/lock` | `value`: 1 locks current mode; 0 returns to Auto |
| POST | `/api/battery/show` | No arguments; 400 ms dim overlay |
| POST | `/api/sequences/select` | `index`: index in `/api/files` |
| POST | `/api/sequences/next` | `direction`: `1` or `-1` |
| POST | `/api/sequences/duration` | `seconds`: 0–3600; zero holds selection |
| POST | `/api/sequences/reload` | Rescan stored BMPs and reload selected image |
| POST | `/api/files/upload` | Raw BMP bytes; headers below |
| DELETE | `/api/files?name=example.bmp` | Removes one validated basename and reloads catalogue |
| POST | `/api/wifi` | `mode`: `ap` or `sta`; STA also requires `ssid` and `password` (blank only for open networks). Optional `forget=1` with AP clears saved client credentials. Saves for next boot; never applies live. |
| POST | `/api/storage/format` | `confirm=ERASE`; permanently clears sequence storage only |
| POST | `/api/ota` | Raw application `.bin`; headers below. Validated success schedules reboot. |
| POST | `/api/reboot` | Responds, then reboots; saved Wi-Fi settings apply |

## Renderer measurements

`engine.renderer` is `baseline` or `dither`. `engine.renderStats` contains `actualFPS` (last approximately one-second output window), `sendMeanUs` and `sendMaxUs` (SPI call duration), `frames` (measured packets), `lateFrames` (missed one-millisecond intervals in B), and `targetFPS` (100 for A procedural scenes, 1000 for B output). POV scene cadence depends on motion and image height. These are firmware measurements, not optical measurements. Blackout writes are excluded from transfer counters.

Counters reset on every successful `/api/renderer` request. Renderer choice is runtime-only and preserves requested mode, brightness and color. B caches fractional scene values, rounds to one-quarter output codes, staggers four pixel phases, and quantizes each packet within the current budget; it does not interpolate motion or increase sensor sampling speed.

## Sequence preview reads

`/api/files/content` accepts the same restricted BMP basenames as uploads. It validates the filename and BMP header before serving bytes, with no arbitrary filesystem access. The response streams in 1 KB chunks and updates the lighting engine between writes. Reading a preview does not select a real sequence or change the staff's mode.

The catalogue `revision` changes on reload, upload, deletion and formatting, but not on ordinary sequence selection. The dashboard uses it to invalidate decoded preview images when file contents may have changed. Its cache holds at most 12 decoded BMPs in browser memory. Once a file is fetched, animation, speed adjustment, pause and frame scrubbing run locally without further image requests. `/preview.js` is embedded in program flash; neither that module nor generated preview videos consume sequence storage.

## Streaming uploads

Send exactly one file per HTTP request with:

```text
Content-Type: application/octet-stream
Content-Length: <exact file byte length>
X-POV-Filename: <URL-encoded basename>
X-POV-Size: <exact file byte length>
X-POV-Request: 1
```

The browser supplies `Content-Length` automatically when sending a File. Do **not** send FormData/multipart or chunked transfer encoding. Header metadata avoids a current ESP32 WebServer raw-request limitation with query arguments. The byte counts in the stream, header and parser must agree before publication/finalization.

Example from Windows PowerShell (uses the real curl executable):

```powershell
$image = Get-Item .\images\rg-lines.bmp
curl.exe -H 'X-POV-Request: 1' `
  -H 'Content-Type: application/octet-stream' `
  -H "X-POV-Filename: $($image.Name)" -H "X-POV-Size: $($image.Length)" `
  --data-binary "@$($image.FullName)" http://192.168.4.1/api/files/upload
```

Image names allow 1–44 letters, digits, hyphens and underscores followed by `.bmp` (case-insensitive), no directories. Pixel data is capped at 64,000 bytes and complete image files at 68,096 bytes. At most 50 catalogue entries are supported, subject to free flash space. Replacement needs room for both the old file and staging file until publication.

OTA accepts only an application image fitting the inactive OTA slot and uses the supported ESP32 Update image checks. Transfer maximum duration is 180 seconds. Both upload transports blank the main LEDs. Status requests and other requests share the synchronous server; the dashboard suspends polling during transfers.

## Status fields

`engine`: `mode`, `requestedMode`, `locked`, `pattern`, `brightness`, `appliedBrightness`, `maxBrightness`, `brightnessLimit`, `currentMa`, `currentLimitMa`, `powerScale`, `batteryVoltage`, `batteryPercent`, `batteryRaw`, `batteryAdcMv`, `powerSource`, `externalPower`, `supplyVoltage`, `usbHostPresent`, `imu`, `orientation`, `speed`, `acceleration`, `booting`, `critical`, `maintenance`, `color`.

`brightness` is the requested 0–255-scale value, bounded by the configured maximum of 64. It resets to 5 on boot; the startup battery indication uses 3. `appliedBrightness` reflects active output limits and can be lower. Lighting controls are runtime-only; Wi-Fi settings persist.

`powerSource` is `battery`, `usb` or `invalid`; `externalPower` is true only for the USB classification. `supplyVoltage` is the measured supply reading, and `usbHostPresent` reports hardware CDC host detection independently of an open serial monitor. A detected host plus a 4.45–5.50 V reading selects USB; a 2.00–4.30 V reading selects battery. USB and invalid classifications make `batteryVoltage` and `batteryPercent` JSON `null`; clients must not render them as zero or a charged battery. A detected USB host always imposes the 16/255 brightness and 500 mA estimated LED-current caps, including with a battery-range supply reading. `brightnessLimit` and `currentLimitMa` expose the active limits; the current estimate includes the four strips' 288-pixel idle allowance. Battery operation retains the 3.20 V cutoff and 3.35 V recovery threshold; unsupported readings keep output black.

`network`: boot-selected `mode`, `savedMode`, `savedSSID`, `pendingReboot`, `connected`, `ssid`, `ip`, `apSSID`, `apIP`, `hostname`, `fallback`, `rssi` (null when disconnected). In fallback, boot mode still reads `sta`; `fallback` identifies the active AP. Passwords are never serialized.

`storage`: `mounted`, `used`, `total`, `count`, `error`. Units are bytes. Battery charge and LED current are estimates, not calibrated measurements.
