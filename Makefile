CFLAGS+=-Wall -Wshadow -Wpointer-arith -Wcast-qual -Wsign-compare
CFLAGS+=-g
CFLAGS+=-std=gnu99
CFLAGS+=-pedantic
CFLAGS+=-DPREFIX=\"\"

VERSION=$(shell git describe --tags --abbrev=0)

ifeq ($(shell uname),Linux)
CFLAGS+=-DLINUX
CFLAGS+=-D_GNU_SOURCE
endif

# Define this if you want i3status to spit out dzen2-compatible output on stdout
CFLAGS+=-DDZEN

i3status: i3status.o i3status.h

clean:
	rm -f *.o

distclean: clean
	rm -f i3status

install:
	install -m 755 -d $(DESTDIR)/usr/bin
	install -m 755 -d $(DESTDIR)/etc
	install -m 755 -d $(DESTDIR)/usr/share/man/man1
	install -m 755 i3status $(DESTDIR)/usr/bin/i3status
	install -m 644 i3status.conf $(DESTDIR)/etc/i3status.conf
	install -m 644 i3status.1 $(DESTDIR)/usr/share/man/man1

release:
	[ -f i3status-${VERSION} ] || rm -rf i3status-${VERSION}
	mkdir i3status-${VERSION}
	cp *.c *.h *.1 *.conf Makefile i3status-${VERSION}
	tar cjf i3status-${VERSION}.tar.bz2 i3status-${VERSION}
	rm -rf i3status-${VERSION}

all: i3status
