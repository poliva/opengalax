/*
 *   opengalax touchscreen daemon
 *   Copyright 2012 Pau Oliva Fora <pof@eslack.org>
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation; either version 2
 *   of the License, or (at your option) any later version.
 *
 *
 * PS/2 mouse driver
 * 
 * Copyright (c) 1999-2002 Vojtech Pavlik
 * Copyright (c) 2004 Sau Dan LEE
 */


#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <stdlib.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include "psmouse.h"
#include "opengalax.h"


static unsigned int psmouse_max_proto = -1U;
int psmouse_resolution = 200;
unsigned int psmouse_rate = 100;
static const char *psmouse_protocols[] = { "None", "PS/2", "PS2++", "PS2T++", "GenPS/2", "ImPS/2", "ImExPS/2", "SynPS/2"};

static struct psmouse psmouse_data;
struct psmouse * const psmouse = &psmouse_data;

/********** functions for interacting with 'uinput' **********/

static int psmouse_uinput_fd;

void uinput_open(const char *uinput_dev_name) {
	psmouse_uinput_fd = open(uinput_dev_name, O_WRONLY);
}

void uinput_set_evbit(int bit) {
	int r;

	r = ioctl(psmouse_uinput_fd, UI_SET_EVBIT, bit);
	if (r==-1) { pferrx(); }
}

void uinput_set_ledbit(int bit) {
	int r;

	r = ioctl(psmouse_uinput_fd, UI_SET_LEDBIT, bit);
	if (r==-1) { pferrx();}
}

void uinput_set_keybit(int bit) {
	int r;

	r = ioctl(psmouse_uinput_fd, UI_SET_KEYBIT, bit);
	if (r==-1) { pferrx(); }
}

void uinput_set_relbit(int bit) {
	int r;

	r = ioctl(psmouse_uinput_fd, UI_SET_RELBIT, bit);
	if (r==-1) { pferrx(); }
}


void uinput_create() {
	struct uinput_user_dev uinput = {
		.name = "psmouse ",
	};
	int r;

	snprintf(uinput.name, UINPUT_MAX_NAME_SIZE, "psmouse");

	r = write(psmouse_uinput_fd, &uinput, sizeof(uinput));
	if (r==-1) { pferrx(); }

	r = ioctl(psmouse_uinput_fd, UI_DEV_CREATE);
	if (r==-1) { pferrx(); }
}

void uinput_destroy() {
	int r;

	r = ioctl(psmouse_uinput_fd, UI_DEV_DESTROY);
	if (r==-1) { pferrx(); }
}

void uinput_close() {
	int r;

	r = close(psmouse_uinput_fd);
	if (r==-1) { pferrx(); }
}

void uinput_event(__u16 type, __u16 code, __s32 value) {
	struct input_event ev = {
		.type = type,
		.code = code,
		.value = value
	};
	int r;

	r = write(psmouse_uinput_fd, &ev, sizeof(ev));
	if (r!=sizeof(ev)) { pferrx(); }
}



/********** functions for accessing the raw keyboard device **********/

static int phys_write(unsigned char byte) {
	int r;
	r = write(fd_serial, &byte, 1);
	if (r==-1) { pferrx(); }
	if (r!=1) { err("cannot write"); exit(1); }
	return 0;
}

void psmouse_interrupt(unsigned char data);

int phys_wait_for_input(int *ptimeout) {
	unsigned char byte;
	int r;

	if (ptimeout != NULL) {
		fd_set fds;
		struct timeval tv;

		FD_ZERO(&fds);
		FD_SET(fd_serial, &fds);
		tv.tv_sec=0; tv.tv_usec=*ptimeout;
		r = select(fd_serial+1, &fds, NULL, &fds, &tv);
		*ptimeout = tv.tv_sec*1000000 + tv.tv_usec;
		if (r==-1) { pferrx(); }
		if (r==0) { return -1; } /* timeout */
		if (r!=1) { err("cannot select"); exit(1); }
	}

	r = read(fd_serial, &byte, 1);
	if (r==-1) { pferrx(); }
	if (r!=1) { err("cannot read"); exit(1); }

	psmouse_interrupt(byte);
	return 0;
}

static int psmouse_reconnect();

static void phys_rescan() {
	psmouse_disconnect();
	psmouse_connect();
}

void phys_reconnect() {
	psmouse_reconnect();
}



/*
 * psmouse_process_packet() analyzes the PS/2 mouse packet contents and
 * reports relevant events to the input module.
 */

static void psmouse_process_packet()
{
	unsigned char *packet = psmouse->packet;

/*
 * Generic PS/2 Mouse
 */

	uinput_report_key(BTN_LEFT,    packet[0]       & 1);
	uinput_report_key(BTN_MIDDLE, (packet[0] >> 2) & 1);
	uinput_report_key(BTN_RIGHT,  (packet[0] >> 1) & 1);

	uinput_report_rel(REL_X, packet[1] ? (int) packet[1] - (int) ((packet[0] << 4) & 0x100) : 0);
	uinput_report_rel(REL_Y, packet[2] ? (int) ((packet[0] << 3) & 0x100) - (int) packet[2] : 0);

	uinput_sync();
}

/*
 * psmouse_interrupt() handles incoming characters, either gathering them into
 * packets or passing them to the command routine as command output.
 */

void psmouse_interrupt(unsigned char data)
{
	if (psmouse->state == PSMOUSE_IGNORE)
		goto out;

	if (psmouse->acking) {
		switch (data) {
			case PSMOUSE_RET_ACK:
				psmouse->ack = 1;
				break;
			case PSMOUSE_RET_NAK:
				psmouse->ack = -1;
				break;
			default:
				psmouse->ack = 1;	/* Workaround for mice which don't ACK the Get ID command */
				if (psmouse->cmdcnt)
					psmouse->cmdbuf[--psmouse->cmdcnt] = data;
				break;
		}
		psmouse->acking = 0;
		goto out;
	}

	if (psmouse->cmdcnt) {
		psmouse->cmdbuf[--psmouse->cmdcnt] = data;
		goto out;
	}

	{
	struct timeval tv;
	long long jiffies;
	int r;

	r = gettimeofday(&tv, NULL);
	if (r==-1) { pferrx(); }
	jiffies = tv.tv_sec * 1000000 + tv.tv_usec;

	if (psmouse->state == PSMOUSE_ACTIVATED &&
	    psmouse->pktcnt 
	    &&  jiffies > psmouse->last + 500000) {
		warn("%s lost synchronization, throwing %d bytes away.\n",
		       psmouse->name, psmouse->pktcnt);
		psmouse->pktcnt = 0;
	}

	psmouse->last = jiffies;
	}

	psmouse->packet[psmouse->pktcnt++] = data;

	if (psmouse->packet[0] == PSMOUSE_RET_BAT) {
		if (psmouse->pktcnt == 1)
			goto out;

		if (psmouse->pktcnt == 2) {
			if (psmouse->packet[1] == PSMOUSE_RET_ID) {
				psmouse->state = PSMOUSE_IGNORE;
				phys_rescan();
				goto out;
			}
		}
	}

	if (psmouse->pktcnt == 3 + (psmouse->type >= PSMOUSE_GENPS)) {
		psmouse_process_packet();
		psmouse->pktcnt = 0;
		goto out;
	}
out:
	return;
}

/*
 * psmouse_sendbyte() sends a byte to the mouse, and waits for acknowledge.
 * It doesn't handle retransmission, though it could - because when there would
 * be need for retransmissions, the mouse has to be replaced anyway.
 */

static int psmouse_sendbyte(unsigned char byte)
{
	int timeout = 10000; /* 100 msec */
	psmouse->ack = 0;
	psmouse->acking = 1;

	if (phys_write(byte)) {
		psmouse->acking = 0;
		return -1;
	}

	while (!psmouse->ack && timeout>0)
		phys_wait_for_input(&timeout);

	return -(psmouse->ack <= 0);
}

/*
 * psmouse_command() sends a command and its parameters to the mouse,
 * then waits for the response and puts it in the param array.
 */

int psmouse_command(unsigned char *param, int command)
{
	int timeout = 500000; /* 500 msec */
	int send = (command >> 12) & 0xf;
	int receive = (command >> 8) & 0xf;
	int i;

	psmouse->cmdcnt = receive;

	if (command == PSMOUSE_CMD_RESET_BAT)
                timeout = 4000000; /* 4 sec */

	/* initialize cmdbuf with preset values from param */
	if (receive)
	   for (i = 0; i < receive; i++)
		psmouse->cmdbuf[(receive - 1) - i] = param[i];

	if (command & 0xff)
		if (psmouse_sendbyte(command & 0xff))
			return (psmouse->cmdcnt = 0) - 1;

	for (i = 0; i < send; i++)
		if (psmouse_sendbyte(param[i]))
			return (psmouse->cmdcnt = 0) - 1;

	while (psmouse->cmdcnt && timeout>0) {

		if (psmouse->cmdcnt == 1 && command == PSMOUSE_CMD_RESET_BAT &&
				timeout > 100000) /* do not run in a endless loop */
			timeout = 100000; /* 1 sec */

		if (psmouse->cmdcnt == 1 && command == PSMOUSE_CMD_GETID &&
		    psmouse->cmdbuf[1] != 0xab && psmouse->cmdbuf[1] != 0xac) {
			psmouse->cmdcnt = 0;
			break;
		}

		phys_wait_for_input(&timeout);
	}

	for (i = 0; i < receive; i++)
		param[i] = psmouse->cmdbuf[(receive - 1) - i];

	if (psmouse->cmdcnt)
		return (psmouse->cmdcnt = 0) - 1;

	return 0;
}


/*
 * psmouse_reset() resets the mouse into power-on state.
 */
int psmouse_reset()
{
	unsigned char param[2];

	if (psmouse_command(param, PSMOUSE_CMD_RESET_BAT))
		return -1;

	if (param[0] != PSMOUSE_RET_BAT && param[1] != PSMOUSE_RET_ID)
		return -1;

	return 0;
}


/*
 * psmouse_extensions() probes for any extensions to the basic PS/2 protocol
 * the mouse may have.
 */

static int psmouse_extensions()
{
	psmouse->vendor = "Generic";
	psmouse->name = "Mouse";
	psmouse->model = 0;

	return PSMOUSE_PS2;
}

/*
 * psmouse_probe() probes for a PS/2 mouse.
 */

static int psmouse_probe()
{
	unsigned char param[2];

/*
 * First, we check if it's a mouse. It should send 0x00 or 0x03
 * in case of an IntelliMouse in 4-byte mode or 0x04 for IM Explorer.
 */

	param[0] = 0xa5;

	if (psmouse_command(param, PSMOUSE_CMD_GETID))
		return -1;

	if (param[0] != 0x00 && param[0] != 0x03 && param[0] != 0x04)
		return -1;

/*
 * Then we reset and disable the mouse so that it doesn't generate events.
 */

	if (psmouse_command(NULL, PSMOUSE_CMD_RESET_DIS))
		warn("Failed to reset mouse\n");

/*
 * And here we try to determine if it has any extensions over the
 * basic PS/2 3-button mouse.
 */

	return psmouse->type = psmouse_extensions();
}

/*
 * Here we set the mouse resolution.
 */

static void psmouse_set_resolution()
{
	unsigned char param[1];

	if (!psmouse_resolution || psmouse_resolution >= 200)
		param[0] = 3;
	else if (psmouse_resolution >= 100)
		param[0] = 2;
	else if (psmouse_resolution >= 50)
		param[0] = 1;
	else if (psmouse_resolution)
		param[0] = 0;

        psmouse_command(param, PSMOUSE_CMD_SETRES);
}

/*
 * Here we set the mouse report rate.
 */

static void psmouse_set_rate()
{
	unsigned char rates[] = { 200, 100, 80, 60, 40, 20, 10, 0 };
	int i = 0;

	while (rates[i] > psmouse_rate) i++;
	psmouse_command(rates + i, PSMOUSE_CMD_SETRATE);
}

/*
 * psmouse_initialize() initializes the mouse to a sane state.
 */

static void psmouse_initialize()
{
	unsigned char param[2];

/*
 * We set the mouse report rate, resolution and scaling.
 */

	if (psmouse_max_proto != PSMOUSE_PS2) {
		psmouse_set_rate();
		psmouse_set_resolution();
		psmouse_command(NULL, PSMOUSE_CMD_SETSCALE11);
	}

/*
 * We set the mouse into streaming mode.
 */

	psmouse_command(param, PSMOUSE_CMD_SETSTREAM);
}

/*
 * psmouse_activate() enables the mouse so that we get motion reports from it.
 */

static void psmouse_activate()
{
	if (psmouse_command(NULL, PSMOUSE_CMD_ENABLE))
		warn("Failed to enable mouse\n");

	psmouse->state = PSMOUSE_ACTIVATED;
}

/*
 * psmouse_disconnect() closes and frees.
 */

void psmouse_disconnect()
{
	psmouse->state = PSMOUSE_CMD_MODE;

	if (psmouse->disconnect)
		psmouse->disconnect(psmouse);

	psmouse->state = PSMOUSE_IGNORE;
}

/*
 * psmouse_connect() is a callback from the serio module when
 * an unhandled serio port is found.
 */
int psmouse_connect()
{

	int probe_ok=0, i;
	memset(psmouse, 0, sizeof(struct psmouse));

	uinput_set_evbit(EV_KEY);
	uinput_set_evbit(EV_REL);

	uinput_set_keybit(BTN_LEFT);
	uinput_set_keybit(BTN_MIDDLE);
	uinput_set_keybit(BTN_RIGHT);

	uinput_set_relbit(REL_X);
	uinput_set_relbit(REL_Y);

	psmouse->state = PSMOUSE_CMD_MODE;

	for (i=0;i<100;i++) {
		if (DEBUG) printf("probing mouse\n");
		if (probe_ok==1) 
			break;
		probe_ok = psmouse_probe();
		if (DEBUG) printf("probe=%d\n",probe_ok);
	}

	if (probe_ok <= 0)
		return -1;

	sprintf(psmouse->devname, "%s %s %s",
		psmouse_protocols[psmouse->type], psmouse->vendor, psmouse->name);
	info("%s\n", psmouse->devname);

	psmouse_initialize();

	psmouse_activate();

	return 0;
}

static int psmouse_reconnect()
{
	int old_type;

	old_type = psmouse->type;

	psmouse->state = PSMOUSE_CMD_MODE;
	psmouse->type = psmouse->acking = psmouse->cmdcnt = psmouse->pktcnt = 0;
	if (psmouse->reconnect) {
	       if (psmouse->reconnect())
			return -1;
	} else if (psmouse_probe() != old_type)
		return -1;

	/* ok, the device type (and capabilities) match the old one,
	 * we can continue using it, complete intialization
	 */
	psmouse->type = old_type;
	psmouse_initialize();

	psmouse_activate();
	return 0;
}
