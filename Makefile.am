AUTOMAKE_OPTIONS = subdir-objects

EXTRA_DIST = AUTHORS COPYING INSTALL

systemduserunitdir = @SYSTEMD_USERUNITDIR@
systemduserunit_DATA = xorg.service xorg.target

systemduserunit-install-hook:
	mkdir -p $(DESTDIR)$(systemduserunitdir)/basic.target.wants
	ln -sf ../xorg.service $(DESTDIR)$(systemduserunitdir)/basic.target.wants/xorg.service

install-data-hook: systemduserunit-install-hook

AM_CFLAGS = $(LIBSYSTEMD_DAEMON_CFLAGS) -Wall -Wno-uninitialized $(TTRACE) $(GLIB_20_CFLAGS) $(LIBX11_CFLAGS)
AM_LDFLAGS = $(TTRACE) $(GLIB_20_LIBS) $(LIBX11_LIBS)

bin_PROGRAMS = xorg-launch-helper
xorg_launch_helper_SOURCES = src/main.c
xorg_launch_helper_LDADD = $(LIBSYSTEMD_DAEMON_LIBS) $(TTRACE) $(GLIB_20_LIBS) $(LIBX11_LIBS)

