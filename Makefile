DESTDIR?=/
SHELL = /bin/sh
CC?=gcc
CFLAGS = -Wall -Wextra -Wwrite-strings -O -g
LDFLAGS= 
INSTALL = /usr/bin/install -c
INSTALLDATA = /usr/bin/install -c -m 644

srcdir = .
prefix = $(DESTDIR)
bindir = $(prefix)/usr/bin
docdir = $(prefix)/usr/share/doc
mandir = $(prefix)/usr/share/man

OBJ=functions.o configfile.o psmouse.o opengalax.o
BIN=opengalax

all: ${OBJ}
	$(CC) $(CFLAGS) ${OBJ} $(LDFLAGS) -o ${BIN}

install: all
	mkdir -p $(bindir)
	$(INSTALL) $(BIN) $(bindir)/$(BIN)
	mkdir -p $(docdir)/$(BIN)/
	$(INSTALLDATA) $(srcdir)/README.md $(docdir)/$(BIN)/
	$(INSTALLDATA) $(srcdir)/LICENSE $(docdir)/$(BIN)/
	mkdir -p $(prefix)/etc/pm/sleep.d/
	$(INSTALL) $(srcdir)/75_opengalax $(prefix)/etc/pm/sleep.d/
	mkdir -p $(prefix)/etc/X11/xorg.conf.d/
	$(INSTALLDATA) $(srcdir)/10-opengalax.conf $(prefix)/etc/X11/xorg.conf.d/
	mkdir -p $(prefix)/etc/init.d/
	$(INSTALL) $(srcdir)/$(BIN)-init $(prefix)/etc/init.d/$(BIN)
#	/usr/sbin/update-rc.d $(BIN) defaults
#	mkdir -p $(mandir)/man1/
#	$(INSTALLDATA) $(srcdir)/$(BIN).1 $(mandir)/man1/

uninstall:
	rm -rf $(bindir)/$(BIN)
	rm -rf $(docdir)/$(BIN)/
	rm -rf $(prefix)/etc/pm/sleep.d/75_opengalax
	rm -rf $(prefix)/etc/X11/xorg.conf.d/10-opengalax.conf
	rm -rf $(prefix)/etc/init.d/$(BIN)
#	/usr/sbin/update-rc.d -f $(BIN) remove
#	rm -rf $(mandir)/man1/$(BIN).1

clean:
	rm -f $(BIN) *.o
