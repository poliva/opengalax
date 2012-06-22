opengalax
=========

opengalax touchscreen daemon

This is a Linux userland input device driver for touchscreen panels manufactured by [EETI](http://home.eeti.com.tw/web20/eGalaxTouchDriver/linuxDriver.htm): eGalax, eMPIA, TouchKit, HanTouch, etc.

At the moment only PS/2 (via serio_raw) interface is supported because this is the hardware I have, but it shouldn't be
difficult to adapt the source for USB or Serial (RS232) devices. Feel free to fork and send a pull request if you can
adapt it for other devices.

**Why?** Because EETI only offers closed source binary drivers for those touch panels, the eGalax Touch driver is outdated
and doesn't work properly on new Xorg servers (the module ABI differs), and the newer closed source eGTouch daemon driver
doesn't work properly with PS2 devices like mine, so I wrote opengalax to have an alternative Open Source (GPL) driver.