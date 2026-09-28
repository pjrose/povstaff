Basic usage
===========

Power and startup
-----------------

Use the three-position switch at one end of the staff. The middle position is
OFF; one of the other positions is ON. Turn the staff off after use to avoid
battery drain.

At startup the firmware sends black frames, checks supply voltage and briefly
shows a dim battery meter when a battery reading is available. The startup meter
uses brightness 3/255; normal operation starts at 5/255. At about 3.40 V, the
meter lights roughly five pixels. Recharge a low battery as described in
:doc:`usage-charging`.

A detected USB host allows limited USB-powered operation without opening a
serial monitor. The dashboard shows USB supply voltage instead of a battery
percentage. USB brightness is capped at 16/255 and estimated LED current at
500 mA. Battery operation retains low-voltage protection. The dashboard's
voltage, percentage and current values are estimates.

Connect to the dashboard
------------------------

On a fresh configuration the staff creates an open network named
``POVSTAFFxxxx``. Join it without a password and open http://192.168.4.1/ or
http://povstaff.local/. The dashboard requires no login. On a phone, choose to
stay connected when warned that the staff network has no Internet.

To use a home network, open **Device**, then **Wi-Fi & connection**, choose
**Home Wi-Fi · client**, enter its SSID and password, and save.
**Power cycle to apply the change.** Saving does not interrupt the
current connection. If the saved network is unavailable for about ten seconds,
or is later lost, the staff starts its AP and stays there until the next boot.
The same dashboard is available in either mode.

.. figure:: images/dashboard-live.png
   :alt: Staff dashboard with lighting and power controls
   :width: 100%

Modes and controls
------------------

Auto uses motion and orientation: spinning selects POV, stationary horizontal
selects LED Test, and stationary upright selects Walking Stick. Selecting a
manual mode locks it; unlocking returns to Auto.

* **POV:** plays uploaded BMPs one row at a time, synchronized to motion.
* **Walking Stick:** each swing triggers a smooth pulse of the selected color;
  the pulse fades to darkness over time.
* **LED Test:** rainbow, pixel walk, channel test and gradient patterns.
* **Sword, Flashlight and Solid:** procedural scenes controlled from the
  dashboard, without requiring an image file. Breathing, Rainbow and Chase
  are also available through the API.
* **Battery:** a brief dim meter; **Idle/Off:** main strips dark.

The dashboard controls brightness, color, lock, selected sequence and playlist
duration. Files play in filename order; duration defaults to 15 seconds and
zero holds the selected image. Prefix names with ``01-``, ``02-`` and so on to
control order. See :doc:`usage-upload` for image requirements and previews.

Runtime lighting controls reset on boot; Wi-Fi settings and uploaded files
persist. Uploads and firmware updates temporarily blank the main LEDs. Use the
physical switch to turn the staff fully off.
