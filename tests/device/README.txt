Handkit device test
===================

This package checks whether Handkit apps run on your handheld. It takes about
20 seconds, then leaves a report file to attach to a device report:
https://github.com/code-beans/handkit/issues/new?template=device-report.yml

Install
-------

NextUI or MinUI (TrimUI devices), from handkit-test-nextui-*.zip:
  Copy the "Tools" folder to the root of the SD card, merging it with the
  existing one. The test appears as Tools > Handkit Test.

knulli, muOS and other firmwares with a ports menu, from handkit-test-ports-*.zip:
  knulli: copy "Handkit Test.sh" and the "handkit-test" folder into
          roms/ports (/userdata/roms/ports on the device).
  muOS:   copy "Handkit Test.sh" into ROMS/Ports and the "handkit-test"
          folder into the "ports" folder at the root of the SD card.
  The test appears in the Ports list as "Handkit Test".

Run
---

1. Start "Handkit Test". The screen cycles through three demo modes twice.
2. The demo then stays open for a minute. Check the buttons:
     X / Y      switch between the three modes
     A / B      pause / reset the cube in the 3D mode
     D-pad      moves the square or rotates the cube
3. It closes by itself after a minute, or press Start + Select to exit sooner.

Report
------

The results are in "handkit-report.txt" inside the test's folder
("Handkit Test.pak" for NextUI/MinUI, "handkit-test" for ports). Open a device
report at the link above, paste the file's contents, and say whether the
buttons matched their labels. The report also lists the buttons you pressed. A test that fails or shows nothing is just as
useful to report.

The report lists your device model, firmware and kernel version, the graphics
driver files and the connected controllers. It isn't meant to contain anything
personal, but read it before posting.
