AM_CFLAGS = -I$(top_srcdir) -DPACKAGE_LOCALE_DIR=\""$(localedir)"\"
INCLUDES = $(GTK_CFLAGS)
LDADD = $(GTK_LIBS) -lX11 -lgdk-x11-2.0 -lpango-1.0 -lgobject-2.0 -lglib-2.0

NULL = 

bin_PROGRAMS = rainbow-cm

DISTCLEANFILES = *.bak *.log *~ .deps/*.P

rainbow_cm_SOURCES = \
	about.c about.h \
	attr_list.c attr_list.h \
	eggaccelerators.c eggaccelerators.h \
	history.c history.h \
	history-menu.c.h \
	i18n.h \
	keybinder.c keybinder.h \
	main.c main.h \
	main-menu.c.h \
	preferences.c preferences.h \
	rainbow-cm.h \
	utils.c utils.h \
	$(NULL)


CPPCHECK_FLAGS = -q --force --enable=all --std=posix --library=gtk --suppress=variableScope
CPPCHECK = cppcheck

CPPCHECK_CONFIGURATION_DEFAULT = FAST
CPPCHECK_CONFIGURATION_FLAGS_ = $(CPPCHECK_CONFIGURATION_FLAGS_$(CPPCHECK_CONFIGURATION_DEFAULT))
CPPCHECK_CONFIGURATION_FLAGS_UNLIMITED = $(filter -I%,$(COMPILE))
CPPCHECK_CONFIGURATION_FLAGS_LIMITED = $(filter -I% -D% -U%,$(COMPILE))
CPPCHECK_CONFIGURATION_FLAGS_FAST = $(filter -D% -U%,$(COMPILE)) $(filter -I%,$(DEFAULT_INCLUDES))
CPPCHECK_CONFIGURATION_FLAGS = $(CPPCHECK_CONFIGURATION_FLAGS_$(CPPCHECK_CONFIGURATION))

CPPCHECK_SRC = $(filter-out %.h,$(rainbow_cm_SOURCES))

cppcheck:
	$(CPPCHECK) $(CPPCHECK_FLAGS) $(CPPCHECK_CONFIGURATION_FLAGS) $(CPPCHECK_SRC)

cppcheck-check-config:
	$(CPPCHECK) $(CPPCHECK_FLAGS) $(CPPCHECK_CONFIGURATION_FLAGS) --check-config $(CPPCHECK_SRC)

cppcheck-inconclusive:
	$(CPPCHECK) $(CPPCHECK_FLAGS) $(CPPCHECK_CONFIGURATION_FLAGS) --inconclusive $(CPPCHECK_SRC)

