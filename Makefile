CFLAGS+=-Wall -Wshadow -Wpointer-arith -Wcast-qual -Wsign-compare
CFLAGS+=-g
CFLAGS+=-std=gnu99
CFLAGS+=-pedantic
CFLAGS+=-DPREFIX=\"\"

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
	tar cjf i3status.tar.bz2 *.c *.h *.1 *.conf Makefile

all: i3status
