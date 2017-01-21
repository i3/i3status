TOPDIR=$(shell pwd)

ifndef PREFIX
  PREFIX=/usr
endif
ifndef MANPREFIX
  MANPREFIX=$(PREFIX)
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
CPPFLAGS+=-DVERSION=\"${I3STATUS_VERSION}\"
CFLAGS+=-Iinclude
LIBS+=-lconfuse
LIBS+=-lyajl
LIBS+=-lpulse
LIBS+=-lm
LIBS+=-lpthread

ifeq ($(wildcard .git),)
  # not in git repository
  VERSION := $(shell [ -f $(TOPDIR)/I3STATUS_VERSION ] && cat $(TOPDIR)/I3STATUS_VERSION | cut -d '-' -f 1)
  I3STATUS_VERSION := '$(shell [ -f $(TOPDIR)/I3STATUS_VERSION ] && cat $(TOPDIR)/I3STATUS_VERSION)'
else
  VERSION:=$(shell git describe --tags --abbrev=0)
  I3STATUS_VERSION:="$(shell git describe --tags --always) ($(shell git log --pretty=format:%cd --date=short -n1))"
endif
OS:=$(shell uname)

ifeq ($(OS),Linux)
CPPFLAGS+=-DLINUX
CPPFLAGS+=-D_GNU_SOURCE
CFLAGS += $(shell pkg-config --cflags libnl-genl-3.0)
LIBS += $(shell pkg-config --libs libnl-genl-3.0)
LIBS+=-lasound
endif

ifeq ($(OS),GNU/kFreeBSD)
LIBS+=-lbsd
endif

ifneq (, $(filter $(OS), DragonFly FreeBSD OpenBSD))
CFLAGS+=-I/usr/local/include/
LDFLAGS+=-L/usr/local/lib/
endif

ifeq ($(OS),NetBSD)
LIBS+=-lprop
endif

# This probably applies for any pkgsrc based system
ifneq (, $(filter $(OS), NetBSD DragonFly))
CFLAGS+=-I/usr/pkg/include/
LDFLAGS+=-L/usr/pkg/lib/
endif

V ?= 0
ifeq ($(V),0)
# Don’t print command lines which are run
.SILENT:
endif

CFLAGS+=$(EXTRA_CFLAGS)

# Fallback for libyajl 1 which did not include yajl_version.h. We need
# YAJL_MAJOR from that file to decide which code path should be used.
CFLAGS += -idirafter yajl-fallback

OBJS:=$(sort $(wildcard src/*.c *.c))
OBJS:=$(OBJS:.c=.o)

ifeq ($(OS),OpenBSD)
OBJS:=$(filter-out src/pulse.o, $(OBJS))
LIBS:=$(filter-out -lpulse, $(LIBS))
endif

src/%.o: src/%.c include/i3status.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<
	@echo " CC $<"

%.o: %.c include/%.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<
	@echo " CC $<"

all: i3status manpage

i3status: ${OBJS}
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)
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
	install -m 755 -d $(DESTDIR)$(MANPREFIX)/share/man/man1
	install -m 755 i3status $(DESTDIR)$(PREFIX)/bin/i3status
	# Allow network configuration for getting the link speed
	(which setcap && setcap cap_net_admin=ep $(DESTDIR)$(PREFIX)/bin/i3status) || true
	install -m 644 i3status.conf $(DESTDIR)$(SYSCONFDIR)/i3status.conf
	install -m 644 man/i3status.1 $(DESTDIR)$(MANPREFIX)/share/man/man1

release:
	[ -f i3status-${VERSION} ] || rm -rf i3status-${VERSION}
	mkdir i3status-${VERSION}
	find . -maxdepth 1 -type f \( -regex ".*\.\(c\|conf\|h\)" -or -name "README.md" -or -name "Makefile" -or -name "LICENSE" -or -name "CHANGELOG" \) -exec cp '{}' i3status-${VERSION} \;
	mkdir i3status-${VERSION}/src
	mkdir i3status-${VERSION}/man
	find src -maxdepth 1 -type f \( -regex ".*\.\(c\|h\)" \) -exec cp '{}' i3status-${VERSION}/src \;
	find man -maxdepth 1 -type f \( -regex ".*\.\(1\|man\|conf\)" -or -name "Makefile" \) -exec cp '{}' i3status-${VERSION}/man \;
	cp -r include i3status-${VERSION}
	cp -r yajl-fallback i3status-${VERSION}
	cp -r contrib i3status-${VERSION}
	echo ${I3STATUS_VERSION} > i3status-${VERSION}/I3STATUS_VERSION
	tar cjf i3status-${VERSION}.tar.bz2 i3status-${VERSION}
	rm -rf i3status-${VERSION}
