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

#define XA_MAX 0xF
#define YA_MAX 0xF

#define XB_MAX	0x7F
#define YB_MAX	0x7F

#define X_AXIS_MAX XA_MAX*XB_MAX
#define Y_AXIS_MAX YA_MAX*YB_MAX

#define PRESS 0x81
#define RELEASE 0x80

#define DEBUG 0

#define VERSION "0.2"

int init_panel() {

	int i;
	unsigned char r;
	ssize_t res;
	unsigned char init_seq[8] = { 0xf5, 0xf3, 0x0a, 0xf3, 0x64, 0xf3, 0xc8, 0xf4 };
	int ret=1;

	for (i=0;i<8;i++) {

		usleep (10000);
		res = write (fd_serial, &init_seq[i], 1);
		res = read (fd_serial, &r, 1);

		if (res < 0)
			die ("error reading from serial port");

		if (DEBUG)
			printf ("SENT: %.02X READ: %.02X\n", init_seq[i], r);

		if (r != 0xFA ) {
			fprintf (stderr,"panel initialization failed: 0x%.02X != 0xFA\n", r);
			ret=0;
		}

	}

	return ret;
}

void usage() {
	printf("opengalax v%s - (c)2012 Pau Oliva Fora <pof@eslack.org>\n", VERSION);
	printf("Usage: opengalax [options]\n");
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

	int btn1_state = 0;
	int btn2_state = 0;

	int old_btn1_state = 0;
	int old_btn2_state = 0;
	int first_click = 0;

	int foreground = 0;
	int init_ok=0, i;
	int opt;

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

	while ((opt = getopt(argc, argv, "hfs:u:?")) != EOF) {
		switch (opt) {
			case 'h':
				usage();
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
	ev_button[0].type = EV_KEY;
	ev_button[0].code = BTN_LEFT;
	ev_button[0].value = 0;
	ev_button[1].type = EV_KEY;
	ev_button[1].code = BTN_LEFT;
	ev_button[1].value = 1;
	ev_button[2].type = EV_KEY;
	ev_button[2].code = BTN_RIGHT;
	ev_button[2].value = 0;
	ev_button[3].type = EV_KEY;
	ev_button[3].code = BTN_RIGHT;
	ev_button[3].value = 1;

	// panel initialization
	for (i=0; i<10; i++) {
		if (init_ok)
			break;
		init_ok = init_panel();
	}

	if (!init_ok) {
		fprintf(stderr, "error: failed to initialize panel\n");
		remove_pid_file();
		if (ioctl (fd_uinput, UI_DEV_DESTROY) < 0)
			die ("error: ioctl");
		close (fd_uinput);
		exit (-1);
	}

	if (foreground)
		printf("pannel initialized\n");

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

			if (write (fd_uinput, &ev_button[btn1_state], sizeof (struct input_event)) < 0)
				die ("error: write");

			if (write (fd_uinput, &ev_button[btn2_state], sizeof (struct input_event)) < 0)
				die ("error: write");

			// Sync
			if (write (fd_uinput, &ev_sync, sizeof (struct input_event)) < 0)
				die ("error state");

			continue;
		}

		memset (buffer, 0, sizeof (buffer));

		// buffer[0] must be 0x80 (release) or 0x81 (press)
		do {
			res = read (fd_serial, &c, sizeof (c));
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


		old_btn1_state = btn1_state;
		old_btn2_state = btn2_state;

		if (buffer[0] == PRESS) {
			if (btn2_state!=3) 
				btn1_state = 1;
		} else {
			btn1_state = 0;
			btn2_state=2;
		}

		// If this is the first panel event, track time for no-drag timer
		if (btn1_state != old_btn1_state && btn1_state == 1)
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


		// emulate right click by press and hold
		if (time_elapsed_ms (&tv_start_click, &tv_current, 350)) {
			if (prev_x == x && prev_y == y) {
				btn2_state=3;
				btn1_state=0;
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
			printf ("X: %d Y: %d BTN1: %d BTN2: %d FIRST: %d\n", x, y, btn1_state, btn2_state, first_click); 
	}

	if (ioctl (fd_uinput, UI_DEV_DESTROY) < 0)
		die ("error: ioctl");

	close (fd_uinput);

	return 0;
}
