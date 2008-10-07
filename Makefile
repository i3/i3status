wmiistatus: wmiistatus.c wmiistatus.h config.h config.c Makefile
	gcc -Wall -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wshadow -Wpointer-arith -Wcast-qual -Wsign-compare -g -c -o wmiistatus.o wmiistatus.c
	gcc -Wall -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wshadow -Wpointer-arith -Wcast-qual -Wsign-compare -g -c -o config.o config.c
	gcc -Wall -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wshadow -Wpointer-arith -Wcast-qual -Wsign-compare -g -o wmiistatus *.o

install:
	install -m 755 -d $(DESTDIR)/usr/bin
	install -m 755 -d $(DESTDIR)/etc/init.d
	install -m 755 wmiistatus $(DESTDIR)/usr/bin/wmiistatus
	install -m 755 wmiistatus.init $(DESTDIR)/etc/init.d/wmiistatus
	install -m 644 wmiistatus.conf $(DESTDIR)/etc/wmiistatus.conf

all: wmiistatus
