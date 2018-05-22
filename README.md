KNOWN ISSUES
============

- This driver does not support DVB-S2 (yet).
- The driver always reports a fixed SNR value, because the DVB API does not define a specific unit of measurement.
- When using VDR, it is possible that there are regular audio/video glitches when watching transponders with a large number of programs on them.
- There may be short audio/video glitches in the first five seconds after switching channels. This also affects simultaneous recordings from the same transponder.
- Only tested with VDR, MythTV, Kaffeine and szap (with dvr output), but other programs might work as well.
- Disconnecting the device for more than 30 seconds and then closing the DVB viewer will crash the driver, so always close your DVB viewer before disconnecting the device.
