/*
 *   opengalax touchscreen daemon
 *   Copyright 2012 Pau Oliva Fora <pof@eslack.org>
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation; either version 2
 *   of the License, or (at your option) any later version.
 *
 */

#define DEFAULT_PID_FILE "/var/run/opengalax.pid"

#include "opengalax.h"

int running_as_root (void) {
	uid_t uid, euid;	
	uid = getuid();
	euid = geteuid();
	if (uid != 0)
		return 0;
	if (euid != 0)
		return 0;
	return 1;
}

int time_elapsed_ms (struct timeval *start, struct timeval *end, int ms) {
	int difference = (end->tv_usec + end->tv_sec * 1000000) - (start->tv_usec + start->tv_sec * 1000000);
	if (difference > ms * 1000)
		return 1;
	return 0;
}

int configure_uinput (void) {

	calibration_data calibration;
	calibration = calibration_parse();

	if (ioctl (fd_uinput, UI_SET_EVBIT, EV_KEY) < 0)
		die ("error: ioctl");

	if (ioctl (fd_uinput, UI_SET_KEYBIT, BTN_LEFT) < 0)
		die ("error: ioctl");

	if (ioctl (fd_uinput, UI_SET_KEYBIT, BTN_RIGHT) < 0)
		die ("error: ioctl");

	if (ioctl (fd_uinput, UI_SET_EVBIT, EV_ABS) < 0)
		die ("error: ioctl");

	if (ioctl (fd_uinput, UI_SET_ABSBIT, ABS_X) < 0)
		die ("error: ioctl");

	if (ioctl (fd_uinput, UI_SET_ABSBIT, ABS_Y) < 0)
		die ("error: ioctl");


	memset (&uidev, 0, sizeof (uidev));
	snprintf (uidev.name, UINPUT_MAX_NAME_SIZE, "opengalax");
	uidev.id.bustype = BUS_I8042;
	uidev.id.vendor = 0xeef;
	uidev.id.product = 0x1;
	uidev.id.version = 1;
	/*
	uidev.absmin[ABS_X] = 0;
	uidev.absmax[ABS_X] = X_AXIS_MAX-1;
	uidev.absmin[ABS_Y] = 0;
	uidev.absmax[ABS_Y] = Y_AXIS_MAX-1;
	*/
	uidev.absmin[ABS_X] = calibration.xmin;
	uidev.absmax[ABS_X] = calibration.xmax;
	uidev.absmin[ABS_Y] = calibration.ymin;
	uidev.absmax[ABS_Y] = calibration.ymax;

	if (write (fd_uinput, &uidev, sizeof (uidev)) < 0)
		die ("error: write");

	if (ioctl (fd_uinput, UI_DEV_CREATE) < 0)
		die ("error: ioctl");

	return 0;
}

int setup_uinput_dev (const char *ui_dev) {
	fd_uinput = open (ui_dev, O_WRONLY | O_NONBLOCK);
	if (fd_uinput < 0) 
		die ("error: uinput");
	return configure_uinput ();
}


int open_serial_port (const char *fd_device) {
	fd_serial = open (fd_device, O_RDWR | O_NOCTTY | O_NDELAY);
	if (fd_serial == -1) {
		perror ("open_port: Unable to open serial port");
		exit (1);
	}
	else
		fcntl (fd_serial, F_SETFL, 0);

	return 0;
}

int init_panel (void) {

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

		if (r != CMD_OK ) {
			fprintf (stderr,"panel initialization failed: 0x%.02X != 0x%.02X\n", r, CMD_OK);
			ret=0;
		}

	}

	return ret;
}

void initialize_panel (int sig) {

	(void) sig;
	int init_ok=0, i;

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
		close(fd_serial);
		exit (-1);
	}
}

void signal_handler (int sig) {

        (void) sig;

	remove_pid_file();

	if (ioctl (fd_uinput, UI_DEV_DESTROY) < 0)
		die ("error: ioctl");

	close (fd_uinput);

	if (use_psmouse) {
		uinput_destroy();
		psmouse_disconnect();
		uinput_close();
	}

	close(fd_serial);

        exit(1);
}

void signal_installer (void) {

	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGHUP, signal_handler);
	signal(SIGQUIT, signal_handler);
	signal(SIGCHLD, signal_handler);
	signal(SIGABRT, signal_handler);
	signal(SIGUSR1, initialize_panel);
}

int file_exists (char *file) {
	struct stat buf;
	if (stat(file, &buf) == 0)
		return 1;
	return 0;
}

char* default_pid_file (void) {
	char* file = malloc(strlen(DEFAULT_PID_FILE)+2);
	sprintf(file, "%s", DEFAULT_PID_FILE);
	return file;
}

int create_pid_file (void) {

	int fd;
	char *pidfile;
	char buf[100];
	ssize_t cnt;
	char* procpid = malloc( sizeof(buf) + 15 );

	pidfile = default_pid_file();

	if (file_exists(pidfile)) {

		// check if /proc/{pid}/cmdline exists and contains opengalax
		// if it does, means opengalax is already running, so we exit cowardly
		// if it does not contain opengalax, then we remove the old pid file and continue

		fd = open(pidfile, O_RDONLY);
		if (fd < 0) {
			fprintf (stderr,"Could not open pid file: %s\n", pidfile);
			return 0;
		}
		cnt=read(fd, buf, sizeof(buf)-1);
		buf[cnt]='\0';
		
		close(fd);

		strcpy(procpid, "");
		strcat(procpid, "/proc/");
		strcat(procpid, buf);
		strcat(procpid, "/cmdline");

		if (file_exists(procpid)) {
			fd = open(procpid, O_RDONLY);
			if (fd < 0) {
				fprintf (stderr,"Could not open file: %s\n", procpid);
				return 0;
			}

			cnt=read(fd, buf, sizeof(buf)-1);
			buf[cnt]='\0';
			
			close(fd);

			if (strstr(buf,"opengalax") != NULL) {
				fprintf (stderr,"Refusing to start as another instance is already running\n");
				return 0;
			} else {
				if (!remove_pid_file()) 
					return 0;
			}
		}
	}

	fd = open(pidfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0 ) {
		fprintf(stderr,"Could not write pid file: %s\n", pidfile);
		return 0;
	}

	sprintf( buf, "%d", getpid() );
	if (write(fd, buf, strlen(buf)) < 1) {
		perror("Something wrong happening while writing pid file");
		close(fd);
		return 0;
	}
	close(fd);

	free(procpid);

	return 1;
}

int remove_pid_file (void) {

	char *pidfile;

	pidfile = default_pid_file();

	if (!file_exists(pidfile)) {
		fprintf (stderr,"pid file does not exist: %s\n", pidfile);
		return 1;
	}

	if (unlink(pidfile) != 0) {
		fprintf (stderr,"Could not delete pid file: %s\n", pidfile);
		return 0;
	}
	return 1;
}
