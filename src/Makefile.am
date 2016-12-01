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


CPPCHECK_FLAGS = -q --force --enable=all --std=posix --library=gtk --suppress=variableScope
CPPCHECK = cppcheck

CPPCHECK_CONFIGURATION_DEFAULT = FAST
CPPCHECK_CONFIGURATION_FLAGS_ = $(CPPCHECK_CONFIGURATION_FLAGS_$(CPPCHECK_CONFIGURATION_DEFAULT))
CPPCHECK_CONFIGURATION_FLAGS_UNLIMITED = $(filter -I%,$(COMPILE))
CPPCHECK_CONFIGURATION_FLAGS_LIMITED = $(filter -I% -D% -U%,$(COMPILE))
CPPCHECK_CONFIGURATION_FLAGS_FAST = $(filter -D% -U%,$(COMPILE)) $(filter -I%,$(DEFAULT_INCLUDES))
CPPCHECK_CONFIGURATION_FLAGS = $(CPPCHECK_CONFIGURATION_FLAGS_$(CPPCHECK_CONFIGURATION))

cppcheck:
	$(CPPCHECK) $(CPPCHECK_FLAGS) $(CPPCHECK_CONFIGURATION_FLAGS) $(rainbow_cm_SOURCES)

cppcheck-check-config:
	$(CPPCHECK) $(CPPCHECK_FLAGS) $(CPPCHECK_CONFIGURATION_FLAGS) --check-config $(rainbow_cm_SOURCES)

cppcheck-inconclusive:
	$(CPPCHECK) $(CPPCHECK_FLAGS) $(CPPCHECK_CONFIGURATION_FLAGS) --inconclusive $(rainbow_cm_SOURCES)

