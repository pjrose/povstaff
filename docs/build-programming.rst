Firmware installation
=======================

These instructions apply to the Island Robotics V4.1 staff with the **Adafruit
QT Py ESP32-S3, 8 MB flash, no PSRAM**. The maintained Arduino sketch is
``code/POV4.1/POV4.1.ino``. Earlier ItsyBitsy/UF2 instructions do not apply.

Build and USB installation
--------------------------

Use Arduino CLI 1.5.1, ESP32 Arduino core 3.3.11, Adafruit DotStar 1.2.5,
Adafruit NeoPixel 1.15.5 and Adafruit BusIO 1.17.4. The repository's
``tools/build.ps1 -Setup`` installs the pinned core/libraries and builds the
firmware on Windows. Run it from the repository root.

Select these Arduino board settings:

* Adafruit QT Py ESP32-S3 No PSRAM.
* ``default_8MB`` partition scheme.
* Hardware CDC and JTAG; USB CDC on boot enabled.
* Erase all flash disabled.

Connect a data-capable USB-C cable and select the current serial port. Use
``tools/build.ps1 -Port COM5`` with the actual port substituted, or open the
sketch in Arduino IDE and Upload. If connection fails, hold BOOT, press and
release RESET, release BOOT and retry.

The first USB installation establishes the OTA layout. It does not upload a
filesystem image. On a fresh configuration, join the open ``POVSTAFFxxxx``
network and open http://192.168.4.1/ or http://povstaff.local/. There is no login.
Initialize storage under Device → Maintenance & storage recovery only if
needed, then follow :doc:`usage-upload`
to load BMPs. Saved client Wi-Fi settings survive application updates.

Wireless firmware updates
-------------------------

Build with the same board and partition settings. Open Firmware Update in the
dashboard and upload **POV4.1.ino.bin** from ``.build/release``. Do not upload
merged, bootloader or partition binaries. A validated update reboots onto the
saved network. The dashboard and preview module are built into program flash;
no separate filesystem upload is required.

ArduinoOTA is also available without a password. Select the ``povstaff``
network port in Arduino IDE, or use the official ESP32 core uploader:

.. code-block:: console

   python espota.py -i povstaff.local -p 3232 -f .build/release/POV4.1.ino.bin

Use ``192.168.4.1`` for the staff AP if mDNS is unavailable, or the staff's LAN
address in client mode. The computer must allow UDP 3232 and the TCP callback
from the staff. Filesystem OTA is disabled.

The repository's ``BUILD.md`` contains the complete CLI command and test setup;
``API.md`` documents dashboard automation. On Windows, run ``tools/test.cmd``
with Visual Studio C++ Build Tools and Node.js installed for the host tests.
