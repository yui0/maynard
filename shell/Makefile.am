libexec_PROGRAMS = maynard

AM_CFLAGS = $(GCC_CFLAGS) -I/usr/include/gnome-desktop-3.0 -I/usr/include/gsettings-desktop-schemas -I/usr/include/gnome-menus-3.0/
AM_CPPFLAGS = $(CLIENT_CFLAGS) $(GTK_CFLAGS)

#pkg-config --libs gnome-desktop-3.0
#pkg-config --libs libgnome-menu-3.0
#pkg-config --libs alsa

maynard_SOURCES =				\
	maynard.c				\
	app-icon.c				\
	app-icon.h				\
	clock.c					\
	clock.h					\
	favorites.c				\
	favorites.h				\
	shell-app-system.c			\
	shell-app-system.h			\
	panel.c					\
	panel.h					\
	vertical-clock.c			\
	vertical-clock.h			\
	launcher.c				\
	launcher.h				\
	maynard-resources.c			\
	maynard-resources.h			\
	desktop-shell-client-protocol.h		\
	desktop-shell-protocol.c		\
	shell-helper-client-protocol.h		\
	shell-helper-protocol.c
maynard_LDADD = $(GTK_LIBS) -lm -lwayland-client -lgnome-desktop-3 -lgtk-3 -lgdk-3 -lpangocairo-1.0 -lpango-1.0 -latk-1.0 -lcairo-gobject -lcairo -lgdk_pixbuf-2.0 -lgio-2.0 -lgobject-2.0 -lglib-2.0 -lgnome-menu-3 -lgio-2.0 -lgobject-2.0 -lglib-2.0 -lasound

BUILT_SOURCES =					\
	desktop-shell-client-protocol.h		\
	desktop-shell-protocol.c		\
	shell-helper-client-protocol.h		\
	shell-helper-protocol.c			\
	maynard-resources.c			\
	maynard-resources.h

# gresource for css
resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/maynard.gresource.xml)
maynard-resources.c: maynard.gresource.xml $(resource_files)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-source --c-name maynard $<
maynard-resources.h: maynard.gresource.xml $(resource_files)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-header --c-name maynard $<

EXTRA_DIST =			\
	style.css		\
	maynard.gresource.xml

# shell-helper weston module
moduledir = $(libdir)/weston
module_LTLIBRARIES = shell-helper.la

# otherwise shell-helper-protocol.c is generated twice- once with
# libtool and once without
mod-%-protocol.c : $(wayland_protocoldir)/%.xml
	$(AM_V_GEN)$(wayland_scanner) code < $< > $@

shell_helper_la_LDFLAGS = -module -avoid-version
shell_helper_la_LIBADD = $(GTK_LIBS)
shell_helper_la_SOURCES =				\
	shell-helper.c
nodist_shell_helper_la_SOURCES =			\
	mod-shell-helper-protocol.c 			\
	shell-helper-server-protocol.h

BUILT_SOURCES += $(nodist_shell_helper_la_SOURCES)
CLEANFILES = $(BUILT_SOURCES)

@wayland_scanner_rules@
