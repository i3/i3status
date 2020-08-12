@CODE_COVERAGE_RULES@

echo-version:
	@echo "@I3STATUS_VERSION@"

bin_PROGRAMS = i3status

if BUILD_MANS
dist_man1_MANS = \
	$(asciidoc_MANS)

asciidoc_MANS = \
	man/i3status.1

$(asciidoc_MANS): man/%.1: man/%.xml man/$(dirstamp)
	$(AM_V_GEN) out='$@'; @PATH_XMLTO@ man -o "$${out%/*}" $<
	@stamp='$@'; $(MKDIR_P) "$${stamp%/*}"

man/%.xml: man/%.man man/asciidoc.conf man/$(dirstamp)
	$(AM_V_GEN) @PATH_ASCIIDOC@ -d manpage -b docbook -f $(top_builddir)/man/asciidoc.conf -o $@ $<
else
asciidoc_MANS =
endif

AM_CPPFLAGS = \
	-DSYSCONFDIR="\"$(sysconfdir)\"" \
	-I$(top_srcdir)/include \
	@AX_EXTEND_SRCDIR_CPPFLAGS@

i3status_CFLAGS = \
	$(AM_CFLAGS) \
	$(CONFUSE_CFLAGS) \
	$(X11_CFLAGS) \
	$(XKBFILE_CFLAGS) \
	$(YAJL_CFLAGS) \
	$(PULSE_CFLAGS) \
	$(NLGENL_CFLAGS) \
	$(ALSA_CFLAGS) \
	$(PTHREAD_CFLAGS)

i3status_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	$(CODE_COVERAGE_CPPFLAGS)

i3status_LDADD = \
	$(CONFUSE_LIBS) \
	$(X11_LIBS) \
	$(XKBFILE_LIBS) \
	$(YAJL_LIBS) \
	$(PULSE_LIBS) \
	$(NLGENL_LIBS) \
	$(ALSA_LIBS) \
	$(PTHREAD_LIBS)

i3status_SOURCES = \
	include/i3status.h \
	include/queue.h \
	i3status.c \
	src/auto_detect_format.c \
	src/first_network_device.c \
	src/format_placeholders.c \
	src/general.c \
	src/output.c \
	src/print_battery_info.c \
	src/print_cpu_temperature.c \
	src/print_cpu_usage.c \
	src/print_ddate.c \
	src/print_disk_info.c \
	src/print_eth_info.c \
	src/print_ip_addr.c \
	src/print_ipv6_addr.c \
	src/print_load.c \
	src/print_mem.c \
	src/print_path_exists.c \
	src/print_run_watch.c \
	src/print_time.c \
	src/print_volume.c \
	src/print_wireless_info.c \
	src/print_file_contents.c \
	src/print_keyboard_layout.c \
	src/process_runs.c

if PULSE
i3status_SOURCES += src/pulse.c
endif

dist_sysconf_DATA = \
	i3status.conf

check_SCRIPTS = \
	travis/run-tests.pl

TESTS = travis/run-tests.pl

EXTRA_DIST = \
	CHANGELOG \
	LICENSE \
	README.md \
	I3STATUS_VERSION \
	contrib/check_mail.py \
	contrib/net-speed.sh \
	contrib/wrapper.pl \
	contrib/wrapper.py \
	$(asciidoc_MANS:.1=.man) \
	man/asciidoc.conf.in \
	travis/run-tests.pl.in

# dirstamps contains directories which we want to be created in $(top_builddir)
# so that our custom rules can store files in them.
dirstamp = .dirstamp
dirstamps = \
	man/$(dirstamp)
DISTCLEANFILES = $(dirstamps)

$(dirstamps):
	@stamp='$@'; $(MKDIR_P) "$${stamp%/*}"
	@: > $@
