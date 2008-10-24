CFLAGS+=-Wall -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wshadow -Wpointer-arith -Wcast-qual -Wsign-compare
CFLAGS+=-g
CFLAGS+=-DPREFIX=\"\"

wmiistatus: wmiistatus.o wmiistatus.h config.h config.o

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
	tar cf wmiistatus.tar *.c *.h *.1 *.conf *.init Makefile
	bzip2 -9 wmiistatus.tar

all: wmiistatus
