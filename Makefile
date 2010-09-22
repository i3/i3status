CFLAGS+=-Wall -Wshadow -Wpointer-arith -Wcast-qual -Wsign-compare
CFLAGS+=-g
CFLAGS+=-std=gnu99
CFLAGS+=-pedantic
CFLAGS+=-DPREFIX=\"\"
CFLAGS+=-DVERSION=\"${GIT_VERSION}\"
CFLAGS+=-Iinclude
LDFLAGS+=-lconfuse

VERSION:=$(shell git describe --tags --abbrev=0)
GIT_VERSION:="$(shell git describe --tags --always) ($(shell git log --pretty=format:%cd --date=short -n1))"

ifeq ($(shell uname),Linux)
CFLAGS+=-DLINUX
CFLAGS+=-D_GNU_SOURCE
LDFLAGS+=-liw -lasound
endif

ifeq ($(shell uname),GNU/kFreeBSD)
CFLAGS+=-lbsd
endif

CFLAGS+=$(EXTRA_CFLAGS)

OBJS:=$(wildcard src/*.c *.c)
OBJS:=$(OBJS:.c=.o)

src/%.o: src/%.c
	@$(CC) $(CFLAGS) -c -o $@ $<
	@echo " CC $<"

%.o: %.c %.h
	@$(CC) $(CFLAGS) -c -o $@ $<
	@echo " CC $<"

all: i3status manpage

i3status: ${OBJS}
	@$(CC) -o $@ src/*.o *.o $(LDFLAGS)
	@echo " LD $@"

clean:
	rm -f *.o src/*.o

distclean: clean
	rm -f i3status

manpage:
	make -C man

install:
	install -m 755 -d $(DESTDIR)/usr/bin
	install -m 755 -d $(DESTDIR)/etc
	install -m 755 -d $(DESTDIR)/usr/share/man/man1
	install -m 755 i3status $(DESTDIR)/usr/bin/i3status
	# Allow network configuration for getting the link speed
	(which setcap && setcap cap_net_admin=ep $(DESTDIR)/usr/bin/i3status) || true
	install -m 644 i3status.conf $(DESTDIR)/etc/i3status.conf
	install -m 644 man/i3status.1 $(DESTDIR)/usr/share/man/man1

release:
	[ -f i3status-${VERSION} ] || rm -rf i3status-${VERSION}
	mkdir i3status-${VERSION}
	find . -maxdepth 1 -type f \( -regex ".*\.\(c\|conf\|h\)" -or -name "Makefile" -or -name "LICENSE" \) -exec cp '{}' i3status-${VERSION} \;
	mkdir i3status-${VERSION}/src
	mkdir i3status-${VERSION}/man
	find src -maxdepth 1 -type f \( -regex ".*\.\(c\|h\)" \) -exec cp '{}' i3status-${VERSION}/src \;
	find man -maxdepth 1 -type f \( -regex ".*\.\(1\|man\|conf\)" -or -name "Makefile" \) -exec cp '{}' i3status-${VERSION}/man \;
	sed -e 's/^GIT_VERSION:=\(.*\)/GIT_VERSION=${GIT_VERSION}/g;s/^VERSION:=\(.*\)/VERSION=${VERSION}/g' Makefile > i3status-${VERSION}/Makefile
	tar cjf i3status-${VERSION}.tar.bz2 i3status-${VERSION}
	rm -rf i3status-${VERSION}
