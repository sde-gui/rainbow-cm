@INTLTOOL_DESKTOP_RULE@

xdgdatadir=@XDGDATADIR@
app_name=rainbow-cm

app_icon_files = \
	$(app_name:%=%.png) \
	$(app_name:%=%.svg) \
	$(app_name:%=%.xpm) 

desktopdir = $(datadir)/applications
desktop_in_file = $(app_name:%=%.desktop.in)
desktop_DATA = $(app_name:%=%.desktop)

autostartdir = $(sysconfdir)/xdg/autostart
autostart_in_file = $(app_name:%=%-startup.desktop.in)
autostart_DATA = $(app_name:%=%-startup.desktop)

pixmapdir = $(datadir)/pixmaps

pixmap_DATA = $(app_icon_files)


EXTRA_DIST = \
  $(desktop_in_file) \
  $(autostart_in_file) \
  $(app_icon_files) 

CLEANFILES = \
  $(desktop_DATA) \
  $(autostart_DATA)

DISTCLEANFILES = \
  $(desktop_DATA) \
  $(autostart_DATA) 
