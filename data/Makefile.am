# data/

SUBDIRS    = challenges fonts gfx gui karts models music po sfx textures tracks \
             grandprix tutorials shaders

pkgdatadir = $(datadir)/games/$(PACKAGE)/data

dist_pkgdata_DATA = \
 $(shell find $(srcdir) -maxdepth 1 -name "*.challenge") \
 stk_config.xml  powerup.xml  items.xml                  \
 CREDITS run_me.sh 

desktopdir = $(prefix)/share/applications
desktop_DATA = supertuxkart.desktop

icondir = $(prefix)/share/pixmaps
dist_icon_DATA = supertuxkart_32.xpm supertuxkart_64.xpm


# The desktop file needs the absolute path to the binary
# since e.g. /usr/games might not be in the standard path
EXTRA_DIST = supertuxkart_desktop.template
CLEANFILES = supertuxkart.desktop
supertuxkart.desktop: Makefile supertuxkart_desktop.template
	sed 's#@PREFIX@#$(prefix)#' '$(srcdir)/supertuxkart_desktop.template' >supertuxkart.desktop
