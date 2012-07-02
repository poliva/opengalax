opengalax - touchscreen daemon
==============================

&copy; 2012 Pau Oliva Fora - pof[at]eslack(.)org

Opengalax is a Linux userland input device driver for touchscreen panels manufactured by [EETI](http://home.eeti.com.tw/web20/eGalaxTouchDriver/linuxDriver.htm): eGalax, eMPIA, TouchKit, Touchmon, HanTouch, D-WAV Scientific Co, eTurboTouch, Faytech, PanJit, 3M MicroTouch EX II, ITM, etc.

The panels can be connected using 3 different interfaces: 
- USB (Hardware ID: USB\VID_0EEF&PID_0001 or USB\VID_0EEF&PID_0002)
- RS232 (Hardware ID: SERNUM\EGX5800, SERNUM\EGX5900, SERNUM\EGX6000, SERNUM\EGX5901 and SERNUM\EGX5803)
- PS/2 (Hardware ID: *PNP0F13)

At the moment only PS/2 (via serio_raw) interface is supported because this is the hardware I have, but it shouldn't be
difficult to adapt the source for USB or Serial (RS232) devices. Feel free to fork and send a pull request if you can
adapt it for other devices/interfaces.

**Why?** Because EETI only offers closed source binary drivers for those touch panels, the eGalax Touch driver is outdated
and doesn't work properly on new Xorg servers (the module ABI differs), and the newer closed source eGTouch daemon driver
doesn't work properly with PS2 devices like mine, so I wrote opengalax to have an alternative Open Source (GPL) driver.


Configuration
-------------

When first launched opengalax will create a configuration file in `/etc/opengalax.conf' with the default configuration values:

    # opengalax configuration file

    #### config data:
    serial_device=/dev/serio_raw0
    uinput_device=/dev/uinput
    rightclick_enable=0
    rightclick_duration=350
    rightclick_range=10
    # direction: 0 = normal, 1 = invert X, 2 = invert Y, 4 = swap X with Y
    direction=0
    # set psmouse=1 if you have a mouse connected into the same port
    # this usually requires i8042.nomux=1 and i8042.reset kernel parameters
    psmouse=0

    #### calibration data:
    # - values should range from 0 to 2047
    # - right/bottom must be bigger than left/top
    # left edge value:
    xmin=0
    # right edge value:
    xmax=2047
    # top edge value:
    ymin=0
    # bottom edge value:
    ymax=2047


When launched without parameters, opengalax will read the configuration from this
config file, some configuration values can also be overwritten via the command line:

    Usage: opengalax [options]
        -c                   : calibration mode
    	-f                   : run in foreground (do not daemonize)
    	-s <serial-device>   : default=/dev/serio_raw0
    	-u <uinput-device>   : default=/dev/uinput


Calibration
-----------

Altough opengalax provides a basic calibration mode (-c command line switch), for best results
it is recommended to use xinput_calibrator and leave the default values in opengalax configuration file.

Usage in Xorg
-------------

Opengalax will configure evdev xorg driver to handle right click emulation by default, so you don't need
to enable right click emulation in the configuration file. If for some reason you do not want to use evdev,
opengalax can handle the right click emulation itself by enabling it in the configuration file.