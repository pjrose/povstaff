# Validation and hardware acceptance

## Reproducible automated checks

On Windows with Visual Studio C++ build tools and Node.js installed:

```powershell
.\tools\test.cmd
.\tools\build.ps1
```

The test runner puts generated binaries in `.build/`. C++ checks can also be compiled with a C++17 compiler on other platforms; the storage tests need `-Itests/support` and `code/POV4.1/Sequences.cpp`. Engine tests link `code/POV4.1/Patterns.cpp`.

Verified in this implementation session:

- [x] Target firmware compiles for the 8 MB QT Py ESP32-S3 using core 3.3.11.
- [x] Walking animation remains continuous when gyro speed changes after long uptime; entering POV restarts the selected image's display interval without reloading it.
- [x] Battery values: 3.20 → 0%, 3.40 → 7% / five pixels, 3.70 → 40%, 4.00 → 88%, 4.20 → 100%; interpolation is monotonic.
- [x] Limiter clamps the first excessive frame and releases smoothly; bounded angular phase advancement.
- [x] Ten procedural patterns produce finite/nonnegative values across 10,000 test frames.
- [x] Filename traversal/length/type rejection; truncated/oversized/compressed/wrong-width BMP rejection; top-down and bottom-up BMP handling; both real sample files accepted.
- [x] Actual sequence module under a host filesystem adapter: mount failure does not format, abandoned staging cleanup, sorted discovery, selection/reload/deletion, row order, empty storage, explicit format, duration zero, and millis wraparound.
- [x] Strict numeric parsing/overflow, Wi-Fi settings validation, boot snapshot independence, exact transfer-length validation.
- [x] Dashboard JavaScript syntax and behavior: telemetry mapping, input editing/focus protection, text-only filenames, mutation headers, raw upload body/headers, and polling suspension while busy.
- [x] Browser visual inspection at desktop and 390-pixel phone width, without horizontal overflow. Preview uses explicitly simulated telemetry; it is not hardware proof.

Preview the actual embedded dashboard with a mock API:

```powershell
node tests/dashboard-preview.cjs 8765
```

Open `http://127.0.0.1:8765/`. [Desktop preview](docs/images/dashboard-desktop.png) is labeled MOCK.

## Hardware checks completed in this session

These observations come from the connected physical staff, separate from the simulated dashboard and host tests:

- [x] Version 5.0.2 USB upload completed on **COM5** after hardware CDC re-enumeration.
- [x] Version **5.0.3** installed through Arduino OTA and returned on the home network with all eight images intact. Dashboard/status access needs no login.
- [x] Final version **5.0.4** built and installed through password-free official `espota.py`; post-reboot version, home-Wi-Fi connection and all eight retained images were verified.
- [x] Saved client mode connected to the home LAN; the dashboard answered at **192.168.4.44**, and **povstaff.local** resolved. No home SSID or password is recorded here.
- [x] Browser OTA completed in both AP and client modes, with the staff returning after reboot.
- [x] The open staff AP and fallback were tested on 5.0.3. The AP was stable after refreshing the Windows network scan. Saved home client mode was restored afterward.
- [x] Valid BMP upload and replacement completed. Malformed images were rejected, and an aborted replacement recovered without destroying the existing file. The original eight images were preserved.
- [x] The official `espota.py` uploader completed firmware transfer with Python 3.12 and explicit host binding; the staff rebooted and reported the installed version. The standalone `espota.exe` path was blocked by existing Windows firewall rules; no firewall settings were changed.

The installed release is **5.1.1**, using **1,135,713 bytes** of program storage and **122,320 bytes** of static RAM. Earlier versions remain identified above to preserve the history of completed hardware checks.

Version 5.0.3 reported **4.71–4.72 V** during USB bench testing and held the main LEDs black because this exceeded its battery range. Version 5.0.4 addresses this by classifying 4.45–5.50 V as a USB supply only when hardware CDC positively detects a USB host. Host detection does not require an open serial monitor. With any detected USB host, brightness is capped at 16/255 and estimated LED current at 500 mA, including at battery-range voltage. The supply reading remains uncalibrated. The completed network/upload results do **not** validate visible lighting, battery voltage accuracy, operating current, cold-start endurance or Android behavior.

## Completed preview checks for 5.1.1

- [x] Password-free official `espota.py` completed the 5.1.1 update with exit code zero. Post-reboot status confirmed 5.1.1, home client connection, USB power without critical lockout and all eight images.
- [x] Storage usage remained **331,776 bytes**; no generated videos or preview assets were written to LittleFS.
- [x] All host tests passed, including preview timing, the 12-image browser cache and revision-based invalidation. Live replacement/invalidation has not been separately exercised.
- [x] On 5.1.0, all eight live BMP downloads matched their advertised lengths, both repository sample hashes matched, invalid paths returned HTTP 400 and a missing file returned 404. Previewing another image left actual selection and mode unchanged.
- [x] Live browser geometry checks passed on installed 5.1.1 for spinning, stationary and full-exposure views, without console errors. The model has two identical 72-pixel, 50 cm arms, a 41 cm dark grip and pixel 0 at the inner ends / 71 at the tips. [Live preview screenshot](docs/images/sequence-preview.png).
- [x] Desktop and 390-pixel viewport layouts were checked. Actual Android testing remains outstanding.
- [x] After OTA, user settings were restored to locked POV, requested brightness 18, color `ba66ff`, pattern `pixel_walk` and sequence duration 15 seconds. The active sequence advances automatically; preview selection remains independent.
- [ ] Observe physical lighting continuity during downloads and compare real spinning appearance with the illustrative preview.
- [ ] Test preview controls on an actual Android device and exercise replacement/invalidation live.

## Historical USB power checks on 5.0.4

The serial monitor remained closed throughout these checks.

- [x] Post-update status reports 5.0.4, all eight images and a working home-Wi-Fi connection.
- [x] `usbHostPresent` is true, `powerSource` is `usb`, supply readings are **4.689–4.719 V**, `critical` is false, and battery voltage/percentage are JSON `null`.
- [x] Walking Stick, solid white and Flashlight were each requested at brightness 64 and sampled six times at 250 ms intervals. Every sample reported applied brightness above zero and at most 16, estimated current at most 500 mA, and no critical-voltage lockout.
- [x] Off reports applied brightness zero.
- [x] The live browser shows **USB POWER**, approximately **4.69 V**, the **16 / 500 mA** caps, and no voltage warning.
- [x] [Live USB dashboard screenshot](docs/images/dashboard-usb.png) records the physical staff's telemetry; the separate mock screenshot is only a design preview.
- [x] Final runtime setting is locked **LED Test / Rainbow**, requested brightness **24**, color **0088ff**; USB caps remain active.
- [ ] Observe visible main-strip operation and measure actual electrical current. The passed checks above establish firmware output telemetry and browser behavior, not direct LED or current measurement.
- [ ] Confirm USB caps also apply when a USB host is present but the measured voltage lies in the 2.00–4.30 V battery range.
- [ ] Confirm a battery-only reading at/below 3.20 V remains black, recovery requires 3.35 V, and unsupported supply readings remain black. Check against a meter before treating reported voltage as calibrated.

## Repeatable physical smoke checks

Use PowerShell 7. The LAN form leaves the computer on its current Wi-Fi and preserves the staff's saved client configuration:

```powershell
.\tools\hardware-smoke.ps1 -Address 192.168.4.44
# Include a browser update using .build/release/POV4.1.ino.bin:
.\tools\hardware-smoke.ps1 -Address 192.168.4.44 -BrowserOTA
# Optional ArduinoOTA: bind the uploader to this computer's LAN IPv4 address.
# 192.168.1.50 below is an example; replace it with the computer's actual address.
.\tools\hardware-smoke.ps1 -Address 192.168.4.44 -ArduinoOTA `
  -PythonExe 'C:\path\to\python.exe' -HostAddress 192.168.1.50
```

The staff address may change with DHCP; use the current address from the dashboard/router. The script writes its report to `.build/hardware-smoke.json`. Its temporary test file is removed after testing; it does not format storage or remove preexisting images.

For an intentional direct AP test, supply the staff's actual AP name and the existing Windows Wi-Fi profile to return to afterward:

```powershell
.\tools\hardware-smoke.ps1 -StaffSSID POVSTAFFxxxx -ReturnProfile 'Your existing Wi-Fi profile'
```

`-ReturnProfile` is needed only when the script switches the computer to the staff AP. Routine testing of the current saved home-Wi-Fi setup should use `-Address` instead. AP tests require the staff already to be hosting its AP; avoid changing the saved client choice merely to run a LAN smoke check.

## Cold-start acceptance (physical tests)

These endurance and electrical checks require the assembled staff and a suitable power source; automated software tests do not establish them.

| Battery voltage | Cold boots | Pass criteria |
| --- | ---: | --- |
| ~4.1 V | 50 | No white lock, bright startup flash, stuck LEDs or reset loop; responsive dashboard. |
| ~3.8 V | 50 | Same. |
| ~3.6 V | 30 | Same. |
| ~3.4 V | 20 | Same; approximately first five pixels during battery display. |

Watch DATA/CLK and supply behavior if instrumentation is available. A software black frame cannot control LEDs before the MCU begins executing. Check the voltage divider/calibration against a meter at several charge states. At/below 3.20 V, normal output must remain black; recovery requires at least 3.35 V. Out-of-range divider readings also lock out output.

## Wi-Fi, dashboard and sequence acceptance

- [x] Version 5.0.3 AP appears with a unique name and accepts connections without a Wi-Fi password.
- [ ] Dashboard works on Android using numerical IP and, where supported, mDNS; no Internet/CDN needed.
- [x] Version 5.0.3 dashboard and status load without a login; control mutations without `X-POV-Request` return HTTP 403.
- [ ] Save client credentials: active AP and session stay unchanged. After power cycle, valid client credentials connect to home Wi-Fi and expose the same dashboard.
- [x] Version 5.0.3 fallback AP was physically reachable and stable; saved home client mode was restored.
- [ ] Verify fallback timing and lighting continuity during router loss with calibrated power; confirm the recovery AP stays available until reboot.
- [ ] Save AP mode while on LAN: no immediate disconnect; next boot starts AP only.
- [ ] Upload sample BMPs, replace one, select next/previous, set duration, hold with zero, rescan, and delete a test file. Power cycle retains files.
- [ ] Abort a replacement mid-transfer: old file remains. Reject malformed/truncated/wrong-width images and path traversal. Run out of filesystem space: report failure without deleting old destination.
- [ ] Begin with blank/corrupt storage: dashboard remains available, no automatic format, main LEDs black. Explicit initialization recovers storage.
- [ ] Mode, brightness, color, presets and lock work; unlock restores Auto. Battery overlay restores previous mode.

## OTA acceptance

- [x] Browser `.bin` update in AP mode completed on the physical staff.
- [x] Browser `.bin` update in client mode completed on the physical staff.
- [ ] Repeat browser OTA from Android; desktop success does not establish this.
- [x] Password-free ArduinoOTA through official `espota.py` completed on 5.0.3; reboot/version and retained files/settings verified. IDE UI upload remains untested.
- [x] Random/invalid image and interrupted valid firmware prefix rejected; no reboot, no stuck maintenance state. Subsequent successful OTA and boot verified.
- [x] Browser and Arduino application OTA preserve the original eight BMPs and saved Wi-Fi configuration.
- [ ] Exercise filesystem OTA rejection on a disposable filesystem. The code aborts before writes; a destructive bench trial was not run on the existing image library.
- [ ] Keep USB recovery available for a structurally valid firmware image that contains a runtime bug. This build does not configure signed images or automatic application rollback.

## Lighting/runtime acceptance

- [ ] Pixel walk exercises every position in R/G/B/low-white; channel fades and rainbow remain smooth for ten minutes.
- [ ] Walking and sword effects react smoothly to motion; all manual locked modes ignore automatic transitions.
- [ ] Check visual performance while polling the dashboard. Transfers deliberately blank LEDs; ordinary control/status requests should not cause sustained animation stalls.
- [ ] Disconnect/reconnect IMU: dashboard continues; POV blanks, automatic mode idles, manual procedural scenes remain available when battery/storage are healthy.
- [ ] Spin-rate ramps do not create unbounded POV catch-up, watchdog resets or stale bright frames.
- [ ] Flashlight respects its brightness cap and total current scaling; measure real battery/LED current and temperature before increasing limits.
- [ ] Off leaves main strips dark; dim onboard heartbeat indicates state. Verify it is visible through the enclosure.

## Known limits

LED current is an estimate for four strips, including a conservative idle allowance. Battery percentage is a voltage estimate and changes with load. Bench calibration and thermal/endurance testing are still necessary. There is no onboard physical power cutoff controlled by this firmware.

The synchronous web server serializes requests and intentionally pauses lighting for uploads/OTA. BMP catalogue reload reads validated images from flash; POV rows themselves are served from RAM. Standard ESP32 WebServer parsing remains in use for control requests. The staff AP is open, HTTP has no TLS or login, and ArduinoOTA has no password. Anyone with network access to the staff can operate it or replace its firmware; keep it local.

There is no Wi-Fi captive portal, enterprise Wi-Fi, cloud access, per-image duration file, legacy playback path, preference persistence beyond Wi-Fi, or automatic firmware rollback. The image height is limited to 296 rows without PSRAM. Arduino OTA and browser OTA use the two app slots supplied by the documented partition table.
