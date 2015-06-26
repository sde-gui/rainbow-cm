AM_CFLAGS = -I$(top_srcdir) -DPARCELLITELOCALEDIR=\""$(parcellitelocaledir)"\"
INCLUDES = $(GTK_CFLAGS)
LDADD = $(GTK_LIBS) -lX11 -lgdk-x11-2.0 -lpango-1.0 -lgobject-2.0 -lglib-2.0

bin_PROGRAMS = rainbow-cm

DISTCLEANFILES = *.bak *.log *~ .deps/*.P

rainbow_cm_SOURCES = main.c main.h \
                     utils.c utils.h \
                     parcellite-i18n.h\
                     history.c history.h \
                     keybinder.c keybinder.h \
                     preferences.c preferences.h \
                     eggaccelerators.c eggaccelerators.h \
                     attr_list.c attr_list.h

