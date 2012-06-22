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

OBJ=functions.o configfile.o opengalax.o
BIN=opengalax

all: ${OBJ}
	$(CC) $(CFLAGS) ${OBJ} $(LDFLAGS) -o ${BIN}

install: all
	mkdir -p $(bindir)
	$(INSTALL) $(BIN) $(bindir)/$(BIN)
	mkdir -p $(docdir)/$(BIN)/
	$(INSTALLDATA) $(srcdir)/README.md $(docdir)/$(BIN)/
	$(INSTALLDATA) $(srcdir)/LICENSE $(docdir)/$(BIN)/
#	mkdir -p $(mandir)/man1/
#	$(INSTALLDATA) $(srcdir)/$(BIN).1 $(mandir)/man1/

uninstall:
	rm -rf $(bindir)/$(BIN)
	rm -rf $(docdir)/$(BIN)/
	rm -rf $(mandir)/man1/$(BIN).1

clean:
	rm -f $(BIN) *.o
