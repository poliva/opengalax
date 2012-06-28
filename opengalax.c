/*
 *   opengalax PS2 touchscreen daemon
 *   Copyright 2012 Pau Oliva Fora <pof@eslack.org>
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation; either version 2
 *   of the License, or (at your option) any later version.
 *
 *   inspired on: 
 *     http://github.com/superatrain/SPF-Panel-Driver
 *     http://thiemonge.org/getting-started-with-uinput
 *
 */

#include "opengalax.h"

#define VERSION "0.2"

void usage() {
	printf("opengalax v%s - (c)2012 Pau Oliva Fora <pof@eslack.org>\n", VERSION);
	printf("Usage: opengalax [options]\n");
	printf("	-c                   : calibration mode\n");
	printf("	-f                   : run in foreground (do not daemonize)\n");
	printf("	-s <serial-device>   : default=/dev/serio_raw0\n");
	printf("	-u <uinput-device>   : default=/dev/uinput\n");
	exit (1);
}

int main (int argc, char *argv[]) {

	int x, y;
	int xa, xb, ya, yb;

	int prev_x = 0;
	int prev_y = 0;

	int btn1_state = BTN1_RELEASE;
	int btn2_state = BTN2_RELEASE;

	int old_btn1_state = BTN1_RELEASE;
	int old_btn2_state = BTN2_RELEASE;
	int first_click = 0;

	int foreground = 0;
	int opt;

	int calibration_mode=0;
	int calib_xmin=X_AXIS_MAX;
	int calib_xmax=0;
	int calib_ymin=Y_AXIS_MAX;
	int calib_ymax=0;

	pid_t pid;
	ssize_t res;

	unsigned char c;
	unsigned char buffer[5];

	struct input_event ev[2];
	struct input_event ev_button[4];
	struct input_event ev_sync;

	struct timeval tv_start_click;
	struct timeval tv_current;
	struct timeval tv;

	conf_data conf;
	calibration_data calibration;

	conf = config_parse();
	calibration = calibration_parse();

	while ((opt = getopt(argc, argv, "chfs:u:?")) != EOF) {
		switch (opt) {
			case 'h':
				usage();
				break;
			case 'c':
				calibration_mode=1;
				break;
			case 'f':
				foreground=1;
				break;
			case 's':
				sprintf(conf.serial_device, "%s", optarg);
				break;
			case 'u':
				sprintf(conf.uinput_device, "%s", optarg);
				break;
			default:
				usage();
				break;
		}
	}

	if (!running_as_root()) {
		fprintf(stderr,"this program must be run as root user\n");
		exit (-1);
	}

	if (calibration_mode) {
		foreground=1;
		calibration.xmin=0;
		calibration.xmax=X_AXIS_MAX-1;
		calibration.ymin=0;
		calibration.ymax=Y_AXIS_MAX-1;
	}

	printf("opengalax v%s ", VERSION);
	fflush(stdout);

	if (!foreground) {

		if ((pid = fork()) < 0)
			exit(1);
		else
			if (pid != 0)
			exit(0);

		/* daemon running here */
		setsid();
		if (chdir("/") != 0) 
			die("Could not chdir");
		umask(0);
		printf("forked into background\n");
	} else
		printf("\n");

	/* create pid file */
	if (!create_pid_file())
		exit(-1);

	if (foreground) {
		printf ("\nConfiguration data:\n");
		printf ("\tserial_device=%s\n",conf.serial_device);
		printf ("\tuinput_device=%s\n",conf.uinput_device);
		printf ("\trightclick_enable=%d\n",conf.rightclick_enable);
		printf ("\trightclick_duration=%d\n",conf.rightclick_duration);
		printf ("\trightclick_range=%d\n",conf.rightclick_range);
		printf ("\tdirection=%d\n",conf.direction);
		printf ("\nCalibration data:\n");
		printf ("\txmin=%d\n",calibration.xmin);
		printf ("\txmax=%d\n",calibration.xmax);
		printf ("\tymin=%d\n",calibration.ymin);
		printf ("\tymax=%d\n\n",calibration.ymax);
	}

	// Open serial port
	open_serial_port (conf.serial_device);

	// configure uinput
	setup_uinput_dev(conf.uinput_device);

	// handle signals
	signal_installer();

	// input sync signal:
	memset (&ev_sync, 0, sizeof (struct input_event));
	ev_sync.type = EV_SYN;
	ev_sync.code = 0;
	ev_sync.value = 0;

	// button press signals:
	memset (&ev_button, 0, sizeof (ev_button));
	ev_button[BTN1_RELEASE].type = EV_KEY;
	ev_button[BTN1_RELEASE].code = BTN_LEFT;
	ev_button[BTN1_RELEASE].value = 0;
	ev_button[BTN1_PRESS].type = EV_KEY;
	ev_button[BTN1_PRESS].code = BTN_LEFT;
	ev_button[BTN1_PRESS].value = 1;
	ev_button[BTN2_RELEASE].type = EV_KEY;
	ev_button[BTN2_RELEASE].code = BTN_RIGHT;
	ev_button[BTN2_RELEASE].value = 0;
	ev_button[BTN2_PRESS].type = EV_KEY;
	ev_button[BTN2_PRESS].code = BTN_RIGHT;
	ev_button[BTN2_PRESS].value = 1;

	// panel initialization
	initialize_panel(0);

	if (foreground)
		printf("pannel initialized\n");

	if (calibration_mode) {
		printf("Move the mouse around the screen to calibrate.\n");
		printf("When done click Ctrl+C to exit.\n");
		printf("Remember to edit /etc/opengalax.conf and save your calibration values\n\n");
	}

	// main bucle
	while (1) {

		// Should have select timeout, because finger down garantees many results..

		tv.tv_sec = 1;
		tv.tv_usec = 0;

		fd_set serial;
		FD_ZERO (&serial);
		FD_SET (fd_serial, &serial);

		// Use select to use timeout...
		if (select (fd_serial + 1, &serial, NULL, NULL, &tv) < 1) {

			first_click = 0;
			btn1_state = 0;
			btn2_state = 2;

			continue;
		}

		memset (buffer, 0, sizeof (buffer));

		// buffer[0] must be 0x80 (release) or 0x81 (press)
		do {
			res = read (fd_serial, &c, sizeof (c));
			if (c!=RELEASE && c!=PRESS) printf ("ERROR: buffer[0]=%.02X\n", c);
		} while (c!=RELEASE && c!=PRESS);
		buffer[0]=c;

		res = read (fd_serial, &c, sizeof (c));
		if (c > XA_MAX) printf ("ERROR: buffer[1]=%.02X\n", c);		
		buffer[1]=c;

		res = read (fd_serial, &c, sizeof (c));
		if (c > XB_MAX) printf ("ERROR: buffer[2]=%.02X\n", c);		
		buffer[2]=c;

		res = read (fd_serial, &c, sizeof (c));
		if (c > YA_MAX) printf ("ERROR: buffer[3]=%.02X\n", c);		
		buffer[3]=c;

		res = read (fd_serial, &c, sizeof (c));
		if (c > YB_MAX) printf ("ERROR: buffer[4]=%.02X\n", c);		
		buffer[4]=c;

		if (res < 0)
			die ("error reading from serial port");

		if (DEBUG)
			fprintf (stderr,"PDU: %.2X %.2X %.2X %.2X %.2X\n", buffer[0], buffer[1], buffer[2], buffer[3], buffer[4]);

		xa=(int)(buffer[1]);
		ya=(int)(buffer[3]);
		xb=(int)(buffer[2]);
		yb=(int)(buffer[4]);

		x = (xa * XB_MAX) + (xb);
		y = Y_AXIS_MAX - (ya * YB_MAX) + (YB_MAX - yb);

		switch (conf.direction) {
			case 1:
				x = X_AXIS_MAX - x;
				break;
			case 2:
				y = Y_AXIS_MAX - y;
				break;
			case 3:
				x = X_AXIS_MAX - (xa * XB_MAX) + (XB_MAX - xb);
				y = (ya * YB_MAX) + (yb);
				break;
			case 4:
				x = Y_AXIS_MAX - (ya * YB_MAX) + (YB_MAX - yb);
				y = (xa * XB_MAX) + (xb);
				break;
		}

		if (calibration_mode) {
			// show calibration values
			if (x > calib_xmax)
				calib_xmax=x;
			if (y > calib_ymax)
				calib_ymax=y;
			if (x < calib_xmin && x!=0)
				calib_xmin=x;
			if (y < calib_ymin && y!=0)
				calib_ymin=y;
			printf("     xmin=%d  xmax=%d  ymin=%d  ymax=%d          \r", calib_xmin, calib_xmax, calib_ymin, calib_ymax);
			fflush(stdout);

			continue;
		}

		old_btn1_state = btn1_state;
		old_btn2_state = btn2_state;

		if (buffer[0] == PRESS) {
			if (btn2_state != BTN2_PRESS) 
				btn1_state = BTN1_RELEASE;
		} else {
			btn1_state = BTN1_RELEASE;
			btn2_state = BTN2_PRESS;
		}

		// If this is the first panel event, track time for no-drag timer
		if (btn1_state != old_btn1_state && btn1_state == BTN1_PRESS)
		{
			first_click = 1;
			gettimeofday (&tv_start_click, NULL);
		}
		else
			first_click = 0;

		// load X,Y into input_events
		memset (ev, 0, sizeof (ev));	//resets object
		ev[0].type = EV_ABS;
		ev[0].code = ABS_X;
		ev[0].value = x;
		ev[1].type = EV_ABS;
		ev[1].code = ABS_Y;
		ev[1].value = y;

		// send X,Y
		gettimeofday (&tv_current, NULL);
		// Only move to posision of click for first while - prevents accidental dragging.
		if (time_elapsed_ms (&tv_start_click, &tv_current, 200) || first_click)
		{
			if (write (fd_uinput, &ev[0], sizeof (struct input_event)) < 0)
				die ("error: write");
			if (write (fd_uinput, &ev[1], sizeof (struct input_event)) < 0)
				die ("error: write");

		} else {
			prev_x = x;
			prev_y = y;
		}


		if (conf.rightclick_enable) {

			// emulate right click by press and hold
			if (time_elapsed_ms (&tv_start_click, &tv_current, conf.rightclick_duration)) {
				if ( ( x-(conf.rightclick_range/2) < prev_x && prev_x < x+(conf.rightclick_range/2) ) && 
				     ( y-(conf.rightclick_range/2) < prev_y && prev_y < y+(conf.rightclick_range/2) ) ) {
					btn2_state=BTN2_PRESS;
					btn1_state=BTN1_RELEASE;
				}
			}

			// reset the start click counter
			if (time_elapsed_ms (&tv_start_click, &tv_current, conf.rightclick_duration*2) && btn2_state == BTN2_RELEASE) {
				gettimeofday (&tv_start_click, NULL);
			}

			// force button2 transition
			if (btn2_state != old_btn2_state && btn2_state == BTN2_PRESS)
			{
				if (write(fd_uinput, &ev_button[BTN1_RELEASE], sizeof (struct input_event)) < 0)
					die ("error: write");
				if (write(fd_uinput, &ev_button[BTN2_RELEASE], sizeof (struct input_event)) < 0)
					die ("error: write");
				if (write (fd_uinput, &ev_sync, sizeof (struct input_event)) < 0)
					die ("error: write");
				if (foreground)
					printf ("X: %d Y: %d BTN1: OFF BTN2: OFF FIRST: %s\n", x, y,
					first_click == 0 ? "No" : first_click == 1 ? "Yes" : "Unknown");

				usleep (10000);

				if (write(fd_uinput, &ev_button[BTN1_RELEASE], sizeof (struct input_event)) < 0)
					die ("error: write");
				if (write(fd_uinput, &ev_button[BTN2_PRESS], sizeof (struct input_event)) < 0)
					die ("error: write");
				if (write (fd_uinput, &ev_sync, sizeof (struct input_event)) < 0)
					die ("error: write");
				if (foreground)
					printf ("X: %d Y: %d BTN1: OFF BTN2: ON  FIRST: %s\n", x, y,
					first_click == 0 ? "No" : first_click == 1 ? "Yes" : "Unknown");
			}
		}

		// clicking
		if (btn1_state != old_btn1_state)
		{
			if (write(fd_uinput, &ev_button[btn1_state], sizeof (struct input_event)) < 0)
				die ("error: write");

			if (write(fd_uinput, &ev_button[btn2_state], sizeof (struct input_event)) < 0)
				die ("error: write");
		}

		// Sync
		if (write (fd_uinput, &ev_sync, sizeof (struct input_event)) < 0)
			die ("error: write");

		if (foreground)
			printf ("X: %d Y: %d BTN1: %s BTN2: %s FIRST: %s\n", x, y,
				btn1_state == BTN1_RELEASE ? "OFF" : btn1_state == BTN1_PRESS ? "ON " : "Unknown",
				btn2_state == BTN2_RELEASE ? "OFF" : btn2_state == BTN2_PRESS ? "ON " : "Unknown",
				first_click == 0 ? "No" : first_click == 1 ? "Yes" : "Unknown");

	}

	if (ioctl (fd_uinput, UI_DEV_DESTROY) < 0)
		die ("error: ioctl");

	close (fd_uinput);

	return 0;
}
