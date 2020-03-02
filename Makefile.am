AUTOMAKE_OPTIONS = gnu

SUBDIRS = po data src 

DISTCLEANFILES = *.bak *.log *~

EXTRA_DIST = \
	m4 \
	autogen.sh \
	ChangeLog \
	$(desktop_DATA) \
	$(autostart_DATA) \
	intltool-merge.in \
	intltool-update.in \
	intltool-extract.in

ACLOCAL_AMFLAGS = -I m4

cppcheck cppcheck-inconclusive cppcheck-check-config:
	make -C src $@

