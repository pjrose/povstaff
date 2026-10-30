Uploading and previewing sequences
====================================

Image requirements
------------------

* Uncompressed **24-bit RGB BMP**, exactly **72 pixels wide**, up to **296 rows**.
* A filename with 1-44 letters, digits, hyphens or underscores, followed by
  ``.bmp``. Do not include spaces or directory paths.
* At most 50 images, subject to free flash space. Pixel data is limited to
  64,000 bytes and the complete file to 68,096 bytes.

The staff reads each image one horizontal row at a time, from visual top to
bottom. The repository's ``images`` directory contains sample BMPs. An
``imagelist.txt`` file is not used: valid images form a filename-sorted playlist.
Screen colors and LED colors will differ.

Upload through the dashboard
----------------------------

1. Connect to the staff AP or its configured home network as described in
   :doc:`usage-basic`. Open http://povstaff.local/; the AP address is
   http://192.168.4.1/.
2. If storage is unavailable, open **Device → Maintenance & storage recovery**
   and explicitly erase/initialize sequence storage. A failed mount never
   silently formats the filesystem.
3. Open the sequence library and choose BMP files to upload. Multiple files
   are queued individually, with progress and errors shown in the dashboard.
4. Select an image for playback or use next/previous. Set the playlist duration
   to zero to hold the selected image. Delete unwanted images from the library.

There is no separate upload mode or required staff orientation. Uploading blanks
the main LEDs temporarily. Replacing a file stages and validates the new BMP
before publication; interrupted or invalid replacements preserve the old file.
Files remain stored across power cycles and application firmware updates.

.. figure:: images/dashboard-library.png
   :alt: Sequence library with upload and playback controls
   :width: 100%

Preview before playback
-----------------------

Choose Preview for a file to inspect it without changing the real staff's mode
or selected sequence. Three views are available: spinning with an 85 ms light
trail, stationary animated LEDs, and a full-rotation exposure. Pause, play or
scrub individual frames; speed defaults to 80 RPM, or 750 ms per image cycle,
and can be adjusted from 20 to 200 RPM.

The model uses two identical 72-pixel, 50 cm arms and a 41 cm unlit central grip,
following the documented assembly. Pixel 0 is at each inner end and pixel 71 at
each tip. It approximates one BMP per turn rather than predicting calibrated
physical appearance.

The browser fetches an uncached image once and animates it locally, caching up
to 12 decoded BMPs in memory. No preview videos or generated assets are stored
on the staff.

.. figure:: images/sequence-preview.png
   :alt: Two-arm staff sequence preview
   :width: 100%
