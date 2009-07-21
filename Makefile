CFLAGS+=-Wall -Wshadow -Wpointer-arith -Wcast-qual -Wsign-compare
CFLAGS+=-g
CFLAGS+=-std=gnu99
CFLAGS+=-pedantic
CFLAGS+=-DPREFIX=\"\"
CFLAGS+=-I.

VERSION=$(shell git describe --tags --abbrev=0)

ifeq ($(shell uname),Linux)
CFLAGS+=-DLINUX
CFLAGS+=-D_GNU_SOURCE
endif

# Define this if you want i3status to spit out dzen2-compatible output on stdout
CFLAGS+=-DDZEN
CFLAGS+=$(EXTRA_CFLAGS)

src/%.o: src/%.c
	@$(CC) $(CFLAGS) -c -o $@ $<
	@echo " CC $<"

%.o: %.c %.h
	@$(CC) $(CFLAGS) -c -o $@ $<
	@echo " CC $<"

i3status: src/general.o src/config.o src/get_load.o src/output.o src/get_cpu_temperature.o src/process_runs.o src/get_eth_info.o src/get_ip_addr.o src/get_wireless_info.o src/get_battery_info.o src/get_ipv6_addr.o i3status.o
	@$(CC) $(LDFLAGS) -o $@ src/*.o *.o
	@echo " LD $@"

clean:
	rm -f *.o src/*.o

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
	find . -maxdepth 1 -type f \( -regex ".*\.\(c\|conf\|1\|h\)" -or -name "Makefile" \) -exec cp '{}' i3status-${VERSION} \;
	tar cjf i3status-${VERSION}.tar.bz2 i3status-${VERSION}
	rm -rf i3status-${VERSION}

all: i3status
