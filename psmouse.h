#ifndef _PSMOUSE_H
#define _PSMOUSE_H

#define PSMOUSE_CMD_SETSCALE11	0x00e6
#define PSMOUSE_CMD_SETRES	0x10e8
#define PSMOUSE_CMD_GETINFO	0x03e9
#define PSMOUSE_CMD_SETSTREAM	0x00ea
#define PSMOUSE_CMD_POLL	0x03eb
#define PSMOUSE_CMD_GETID	0x02f2
#define PSMOUSE_CMD_SETRATE	0x10f3
#define PSMOUSE_CMD_ENABLE	0x00f4
#define PSMOUSE_CMD_RESET_DIS	0x00f6
#define PSMOUSE_CMD_RESET_BAT	0x02ff

#define PSMOUSE_RET_BAT		0xaa
#define PSMOUSE_RET_ID		0x00
#define PSMOUSE_RET_ACK		0xfa
#define PSMOUSE_RET_NAK		0xfe

/* psmouse states */
#define PSMOUSE_CMD_MODE	0
#define PSMOUSE_ACTIVATED	1
#define PSMOUSE_IGNORE		2

struct psmouse;

struct psmouse {
	void *private;
	const char *vendor;
	const char *name;
	unsigned char cmdbuf[8];
	unsigned char packet[8];
	unsigned char cmdcnt;
	unsigned char pktcnt;
	unsigned char type;
	unsigned char model;
	unsigned char state;
	char acking;
	volatile char ack;
	char error;
	char devname[64];
	long long last;

	int (*reconnect)();
	void (*disconnect)();
};

#define PSMOUSE_PS2		1
#define PSMOUSE_PS2PP		2
#define PSMOUSE_PS2TPP		3
#define PSMOUSE_GENPS		4
#define PSMOUSE_IMPS		5
#define PSMOUSE_IMEX		6
#define PSMOUSE_SYNAPTICS 	7

extern int psmouse_command(unsigned char *param, int command);
extern int psmouse_reset(void);

extern void phys_reconnect();

extern int psmouse_smartscroll;
extern unsigned int psmouse_rate;
extern unsigned int psmouse_resetafter;

extern struct psmouse * const psmouse;


extern void uinput_set_evbit(int bit);
extern void uinput_set_ledbit(int bit);
extern void uinput_set_keybit(int bit);
extern void uinput_set_relbit(int bit);

extern void uinput_event(__u16 type, __u16 code, __s32 value);
#define uinput_report_key(code,value) uinput_event(EV_KEY, (code), (value))
#define uinput_report_rel(code,value) uinput_event(EV_REL, (code), (value))
#define uinput_report_abs(code,value) uinput_event(EV_ABS, (code), (value))
#define uinput_sync() uinput_event(EV_SYN, 0, 0)



#define warn(...) fprintf(stderr, "psmouse: (warning) " __VA_ARGS__)
#define err(...)  fprintf(stderr, "psmouse: ERROR " __VA_ARGS__)
#define info(...) printf("psmouse: " __VA_ARGS__)
#define notice(...) printf("psmouse: NOTE " __VA_ARGS__)
#define perr(msg)  do{fprintf(stderr, "psmouse: ERROR: "); perror(msg);}while(0)
#define perrx(msg) do{perr(msg); exit(1);}while(0)
#define pferrx() perrx(__FUNCTION__)
#define unlikely /* */

#endif /* _PSMOUSE_H */
