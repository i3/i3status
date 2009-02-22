CFLAGS+=-Wall -Wshadow -Wpointer-arith -Wcast-qual -Wsign-compare
CFLAGS+=-g
CFLAGS+=-DPREFIX=\"\"

ifeq ($(shell uname),Linux)
CFLAGS+=-DLINUX
endif

# Define this if you want wmiistatus to spit out dzen2-compatible output on stdout
CFLAGS+=-DDZEN

wmiistatus: wmiistatus.o wmiistatus.h

clean:
	rm -f *.o

distclean: clean
	rm -f wmiistatus

install:
	install -m 755 -d $(DESTDIR)/usr/bin
	install -m 755 -d $(DESTDIR)/etc/init.d
	install -m 755 -d $(DESTDIR)/usr/share/man/man1
	install -m 755 wmiistatus $(DESTDIR)/usr/bin/wmiistatus
	install -m 755 wmiistatus.init $(DESTDIR)/etc/init.d/wmiistatus
	install -m 644 wmiistatus.conf $(DESTDIR)/etc/wmiistatus.conf
	install -m 644 wmiistatus.1 $(DESTDIR)/usr/share/man/man1

release:
	tar cjf wmiistatus.tar.bz2 *.c *.h *.1 *.conf *.init Makefile

all: wmiistatus
