ifndef PREFIX
  PREFIX=/usr
endif
ifndef SYSCONFDIR
  ifeq ($(PREFIX),/usr)
    SYSCONFDIR=/etc
  else
    SYSCONFDIR=$(PREFIX)/etc
  endif
endif

CFLAGS+=-Wall -Wshadow -Wpointer-arith -Wcast-qual -Wsign-compare
CFLAGS+=-g
CFLAGS+=-std=gnu99
CFLAGS+=-pedantic
CPPFLAGS+=-DSYSCONFDIR=\"$(SYSCONFDIR)\"
CPPFLAGS+=-DVERSION=\"${GIT_VERSION}\"
CFLAGS+=-Iinclude
LIBS+=-lconfuse
LIBS+=-lyajl

VERSION:=$(shell git describe --tags --abbrev=0)
GIT_VERSION:="$(shell git describe --tags --always) ($(shell git log --pretty=format:%cd --date=short -n1))"

ifeq ($(shell uname),Linux)
CPPFLAGS+=-DLINUX
CPPFLAGS+=-D_GNU_SOURCE
LIBS+=-liw
LIBS+=-lasound
endif

ifeq ($(shell uname),GNU/kFreeBSD)
LIBS+=-lbsd
endif

ifeq ($(shell uname),OpenBSD)
CFLAGS+=-I/usr/local/include/
LDFLAGS+=-L/usr/local/lib/
LIBS+=-lossaudio
endif

CFLAGS+=$(EXTRA_CFLAGS)

# Fallback for libyajl 1 which did not include yajl_version.h. We need
# YAJL_MAJOR from that file to decide which code path should be used.
CFLAGS += -idirafter yajl-fallback

OBJS:=$(wildcard src/*.c *.c)
OBJS:=$(OBJS:.c=.o)

src/%.o: src/%.c
	@$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<
	@echo " CC $<"

%.o: %.c include/%.h
	@$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<
	@echo " CC $<"

all: i3status manpage

i3status: ${OBJS}
	@$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)
	@echo " LD $@"

clean:
	rm -f *.o src/*.o

distclean: clean
	rm -f i3status man/i3status.1

manpage:
	$(MAKE) -C man

install:
	install -m 755 -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 -d $(DESTDIR)$(SYSCONFDIR)
	install -m 755 -d $(DESTDIR)$(PREFIX)/share/man/man1
	install -m 755 i3status $(DESTDIR)$(PREFIX)/bin/i3status
	# Allow network configuration for getting the link speed
	(which setcap && setcap cap_net_admin=ep $(DESTDIR)$(PREFIX)/bin/i3status) || true
	install -m 644 i3status.conf $(DESTDIR)$(SYSCONFDIR)/i3status.conf
	install -m 644 man/i3status.1 $(DESTDIR)$(PREFIX)/share/man/man1

release:
	[ -f i3status-${VERSION} ] || rm -rf i3status-${VERSION}
	mkdir i3status-${VERSION}
	find . -maxdepth 1 -type f \( -regex ".*\.\(c\|conf\|h\)" -or -name "Makefile" -or -name "LICENSE" \) -exec cp '{}' i3status-${VERSION} \;
	mkdir i3status-${VERSION}/src
	mkdir i3status-${VERSION}/man
	find src -maxdepth 1 -type f \( -regex ".*\.\(c\|h\)" \) -exec cp '{}' i3status-${VERSION}/src \;
	find man -maxdepth 1 -type f \( -regex ".*\.\(1\|man\|conf\)" -or -name "Makefile" \) -exec cp '{}' i3status-${VERSION}/man \;
	cp -r include i3status-${VERSION}
	sed -e 's/^GIT_VERSION:=\(.*\)/GIT_VERSION=${GIT_VERSION}/g;s/^VERSION:=\(.*\)/VERSION=${VERSION}/g' Makefile > i3status-${VERSION}/Makefile
	tar cjf i3status-${VERSION}.tar.bz2 i3status-${VERSION}
	rm -rf i3status-${VERSION}
