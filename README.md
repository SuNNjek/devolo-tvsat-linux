# Devolo TV-Sat Driver for Linux

This is a driver for the Devolo TV-Sat network tuner.

## Installation

There's two ways to install the driver. You can either install it with DKMS or without. DKMS is preferred because it takes
care of recompiling the kernel module after a kernel update.

### With DKMS
If you want to compile the driver with DKMS there's a few commands you need to execute:

First off, you want to prepare the module for DKMS. After cloning the repository you need to create a link so DKMS can find the module.

```
# ln -s <path of the repo> /usr/src/tvsat-1.0.0 && dkms add -m tvsat -v 1.0.0 
```

Then you need to build the module and install it:

```
# dkms build -m tvsat -v 1.0.0 && dkms install -m tvsat -v 1.0.0
```

To load the module on startup you need to edit `/etc/modules`:

```
# echo 'tvsat' >> /etc/modules
```

Finally you need to compile the daemon and register it as a service:

```
# make install_app install_init
```

This is it, the driver is installed
I will try to streamline this process in the future so it's less complicated but for now it works.

### Without DKMS
If you wish to not have DKMS support, you can simply use Make to install the driver. Simply execute from the command line:

```
# make && make install
```


## Known Issues

- This driver supports DVB-S2, but it's still a bit flaky.
- The driver always reports a fixed SNR value, because I don't know how the TV-Sat tuner communicates this data. If someone knows, please let me know.
- When using VDR, it is possible that there are regular audio/video glitches when watching transponders with a large number of programs on them.
- There may be short audio/video glitches in the first five seconds after switching channels. This also affects simultaneous recordings from the same transponder.
- Only tested with VDR, MythTV, Kaffeine szap (with dvr output) and Tvheadend, but other programs might work as well.
- Disconnecting the device for more than 30 seconds and then closing the DVB viewer will crash the driver, so always close your DVB viewer before disconnecting the device.
