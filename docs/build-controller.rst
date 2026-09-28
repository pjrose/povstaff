Step 7: Controller assembly
===========================
Materials

* Bag E from the kit


  .. figure:: images/kit-bagE-annotated.png
      :alt: Kit of parts, bag E
      :width: 60%


  1. QT Py ESP32-S3 MCU, preprogrammed

  2. Custom-made POV controller board (blue), containing Inertial Motion Unit, battery
     charge circuit, and more.

  3. Switch  connector board (black)

  4. Two 7-pin headers



* 4-pin wire, left over from the previous step

**Steps:**

1. Before assembly, connect the QT Py ESP32-S3 to a computer using a
   data-capable USB-C cable. If current firmware is not installed, follow
   :doc:`build-programming` first.

   On a fresh configuration, join the open ``POVSTAFFxxxx`` Wi-Fi network;
   no password is required. Open http://192.168.4.1/ or
   http://povstaff.local/. If home client mode was already saved, use that
   network instead. A missing saved network enables the staff AP after about
   ten seconds.

   .. figure:: images/dashboard-live.png
       :alt: Current staff dashboard
       :width: 80%

   Confirm that the dashboard loads and reports firmware and network status.
   An unattached controller may report an absent IMU or unavailable supply
   reading; this check establishes network/software access before assembly.
   Uploaded files are not required for this check. Disconnect USB before
   soldering and continue below. Use :doc:`usage-upload` to load BMPs after
   assembly; there is no separate uploader mode or image-list file.

2. Solder the headers to the custom  POV controller shield as shown below.
   **Make sure to solder it on the correct side of the board.**  The easiest way
   to keep the headers straight while soldering is to plug them into a breadboard.

   .. figure:: images/controller-2.jpg
       :alt: Soldering headers to POV controller shield
       :width: 60%



   .. figure:: images/controller-1.jpg
       :alt: Soldering headers to POV controller shield
       :width: 60%

3. Solder the ESP32-S2 QT Py  board to the headers, forming a "sandwich" of two boards
   as shown in the photos below. Make sure to solder it in the correct orientation:
   the USB connector should be on top and facing the same way as the black 6-pin header on
   the controller shield.

   .. figure:: images/controller-3.jpg
      :alt: Controller with shield
      :width: 80%

4. Take the 4-pin wire left over from the previous step and solder it to the
   controller board on one side and to
   the switch connector board on the other as shown below. On each board, the
   **wire with white stripe should go into the hole labeled "VCC"**.

   .. figure:: images/controller-4.jpg
      :alt: Controller, shield and wire
      :width: 80%
