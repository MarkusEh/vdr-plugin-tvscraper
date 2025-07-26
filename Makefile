#
# Makefile for a Video Disk Recorder plugin
#
# $Id$

# The official name of this plugin.
# This name will be used in the '-P...' option of VDR to load the plugin.
# By default the main source file also carries this name.

PLUGIN = tvscraper

### The version number of this plugin (taken from the main source file):

VERSION = $(shell grep 'static const char \*VERSION *=' $(PLUGIN).c | awk '{ print $$6 }' | sed -e 's/[";]//g')

### The directory environment:

# Use package data if installed...otherwise assume we're under the VDR source directory:
PKG_CONFIG ?= pkg-config
PKGCFG = $(if $(VDRDIR),$(shell $(PKG_CONFIG) --variable=$(1) $(VDRDIR)/vdr.pc),$(shell PKG_CONFIG_PATH="$$PKG_CONFIG_PATH:../../.." $(PKG_CONFIG) --variable=$(1) vdr))
LIBDIR = $(call PKGCFG,libdir)
LOCDIR = $(call PKGCFG,locdir)
PLGCFG = $(call PKGCFG,plgcfg)
RESDIR = $(call PKGCFG,resdir)
PLGCONFDIR = $(call PKGCFG,configdir)/plugins/$(PLUGIN)

TMPDIR ?= /tmp
PLGSRCDIR = ./PLUGINS

include Make.config-tvscraper

### The compiler options:

export CXXFLAGS = $(call PKGCFG,cxxflags)

CXXFLAGS += -std=c++17 -rdynamic
CXXFLAGS += $(shell $(PKG_CONFIG) --cflags-only-other libcurl)
CXXFLAGS += $(shell $(PKG_CONFIG) --cflags-only-other sqlite3)

### The version number of VDR's plugin API:

APIVERSION = $(call PKGCFG,apiversion)

### Allow user defined options to overwrite defaults:

-include $(PLGCFG)

### The name of the distribution archive:

ARCHIVE = $(PLUGIN)-$(VERSION)
PACKAGE = vdr-$(ARCHIVE)

### The name of the shared object file:

SOFILE = libvdr-$(PLUGIN).so

### Includes and Defines (add further entries here):

INCLUDES += $(shell $(PKG_CONFIG) --cflags-only-I libcurl)
INCLUDES += $(shell $(PKG_CONFIG) --cflags-only-I sqlite3)

DEFINES += -DPLUGIN_NAME_I18N='"$(PLUGIN)"' -DPLGDIR='"$(PLGDEST)"'

LIBS += -lstdc++fs
LIBS += $(shell $(PKG_CONFIG) --libs libcurl)
LIBS += $(shell $(PKG_CONFIG) --libs sqlite3)

### The object files (add further files here):

OBJS = $(PLUGIN).o

### The main target:

all: $(SOFILE) i18n plugins

### Implicit rules:

%.o: %.c
	@echo CC $@
	$(Q)$(CXX) $(CXXFLAGS) -c $(DEFINES) $(INCLUDES) -o $@ $<

### Dependencies:

MAKEDEP = $(CXX) -MM -MG
DEPFILE = .dependencies
$(DEPFILE): Makefile
	@$(MAKEDEP) $(CXXFLAGS) $(DEFINES) $(INCLUDES) $(OBJS:%.o=%.c) > $@

-include $(DEPFILE)

### Internationalization (I18N):

PODIR     = po
I18Npo    = $(wildcard $(PODIR)/*.po)
I18Nmo    = $(addsuffix .mo, $(foreach file, $(I18Npo), $(basename $(file))))
I18Nmsgs  = $(addprefix $(DESTDIR)$(LOCDIR)/, $(addsuffix /LC_MESSAGES/vdr-$(PLUGIN).mo, $(notdir $(foreach file, $(I18Npo), $(basename $(file))))))
I18Npot   = $(PODIR)/$(PLUGIN).pot

%.mo: %.po
	@echo MO $@
	$(Q)msgfmt -c -o $@ $<

$(I18Npot): $(wildcard *.c)
	@echo GT $@
	$(Q)xgettext -C -cTRANSLATORS --no-wrap --no-location -k -ktr -ktrNOOP --package-name=vdr-$(PLUGIN) --package-version=$(VERSION) --msgid-bugs-address='<see README>' -o $@ `ls $^`

%.po: $(I18Npot)
	@echo PO $@
	$(Q)msgmerge -U --no-wrap --no-location --backup=none -q -N $@ $<
	@touch $@

$(I18Nmsgs): $(DESTDIR)$(LOCDIR)/%/LC_MESSAGES/vdr-$(PLUGIN).mo: $(PODIR)/%.mo
	install -D -m644 $< $@

.PHONY: i18n
i18n: $(I18Nmo) $(I18Npot)

install-i18n: $(I18Nmsgs)

### Targets:

$(SOFILE): $(OBJS)
	@echo LD $@
	$(Q)$(CXX) $(CXXFLAGS) $(LDFLAGS) -shared $(OBJS) $(LIBS) -o $@

plugins:
	@find $(PLGSRCDIR) -maxdepth 1 -type d -name "[a-z0-9]*" -exec \
      $(MAKE) VDRDIR="$(VDRDIR)" PLGDEST="$(PLGDEST)" DESTDIR="$(DESTDIR)" \-\-no-print-directory -C {} \;
clean-plugins:
	@find $(PLGSRCDIR) -maxdepth 1 -type d -name "[a-z0-9]*" -exec \
      $(MAKE) \-\-no-print-directory -C {} clean \;
install-plugins: plugins
	@find $(PLGSRCDIR) -maxdepth 1 -type d -name "[a-z0-9]*" -exec \
      $(MAKE) \-\-no-print-directory -C {} install \;

install-lib: $(SOFILE)
	install -D $^ $(DESTDIR)$(LIBDIR)/$^.$(APIVERSION)

install-conf:
	@mkdir -p $(DESTDIR)$(PLGCONFDIR)
	@if [ ! -f $(DESTDIR)$(PLGCONFDIR)/override.conf ]; then cp conf/override.conf $(DESTDIR)$(PLGCONFDIR); fi;
	@mkdir -p $(DESTDIR)$(RESDIR)/plugins/$(PLUGIN)
	@cp -a conf/networks.json $(DESTDIR)$(RESDIR)/plugins/$(PLUGIN)/
	@cp -a conf/override_tvs.conf $(DESTDIR)$(RESDIR)/plugins/$(PLUGIN)/

install: install-lib install-i18n install-conf install-plugins

dist: $(I18Npo) clean
	@-rm -rf $(TMPDIR)/$(ARCHIVE)
	@mkdir $(TMPDIR)/$(ARCHIVE)
	@cp -a * $(TMPDIR)/$(ARCHIVE)
	@tar czf $(PACKAGE).tgz -C $(TMPDIR) $(ARCHIVE)
	@-rm -rf $(TMPDIR)/$(ARCHIVE)
	@echo Distribution package created as $(PACKAGE).tgz

clean: clean-plugins
	@-rm -f $(PODIR)/*.mo $(PODIR)/*.pot
	@-rm -f $(OBJS) $(DEPFILE) *.so *.tgz core* *~
