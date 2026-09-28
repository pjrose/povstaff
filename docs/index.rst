###############################
Persistence of Vision LED staff
###############################

This guide covers the **Island Robotics V4.1 hardware** and this fork's
**version 5 firmware** for the Adafruit QT Py ESP32-S3 with 8 MB flash.
The original staff was designed by Alexander Kirillov, building on work by
Phillip Burgess and Erin St Blaine at Adafruit.

.. figure:: images/main-1.jpg
   :alt: POV staff in action
   :width: 90%

The 141 cm staff has two 50 cm illuminated ends around a central grip.
Each end carries two 72-pixel strips, for 288 physical LEDs sharing 72 logical
pixel positions. An LSM6DSO motion sensor synchronizes BMP playback to rotation.
The hardware assembly and charging guides below describe the existing design.

Wireless controls
*****************

The staff starts its own open Wi-Fi network by default. Connect and open
http://192.168.4.1/ to upload BMP sequences, choose lighting modes, preview
patterns and install firmware updates. Optional home Wi-Fi configuration takes
effect after a power cycle. See :doc:`usage-basic` and :doc:`usage-upload`.

.. figure:: images/dashboard-live.png
   :alt: Live dashboard with Walking Stick selected and brightness 5
   :width: 100%

Normal brightness starts at 5/255. Walking Stick pulses with each swing and fades
to darkness; Sword stays illuminated with motion accents. The browser can
preview BMPs spinning at 80 RPM, as stationary animated LEDs, or as a full
rotation exposure. Previewing does not change actual playback.

.. toctree::
   :caption: Table of Contents
   :maxdepth: 2

   build
   usage
   license
