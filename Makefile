wmiistatus: wmiistatus.c wmiistatus.h config.h Makefile
	gcc -Wall -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wshadow -Wpointer-arith -Wcast-qual -Wsign-compare -g -O2 -o wmiistatus wmiistatus.c

install:
	install -m 755 -d $(DESTDIR)/usr/bin
	install -m 755 -d $(DESTDIR)/etc/init.d
	install -m 755 wmiistatus $(DESTDIR)/usr/bin/wmiistatus
	install -m 755 wmiistatus.init $(DESTDIR)/etc/init.d/wmiistatus

all: wmiistatus
