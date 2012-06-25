opengalax
=========

opengalax touchscreen daemon

This is a Linux userland input device driver for touchscreen panels manufactured by [EETI](http://home.eeti.com.tw/web20/eGalaxTouchDriver/linuxDriver.htm): eGalax, eMPIA, TouchKit, Touchmon, HanTouch, D-WAV Scientific Co, eTurboTouch, Faytech, PanJit, 3M MicroTouch EX II, ITM, etc.

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