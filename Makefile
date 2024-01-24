#
# Makefile for a Video Disk Recorder plugin
#
# $Id$
include Make.config

### The name of the distribution archive:

ARCHIVE = $(PLUGIN)-$(VERSION)
PACKAGE = vdr-$(ARCHIVE)

### The name of the shared object file:

SOFILE = libvdr-$(PLUGIN).so

### Includes and Defines (add further entries here):

INCLUDES +=

DEFINES += -DPLUGIN_NAME_I18N='"$(PLUGIN)"'

LIBS += -lstdc++fs
LIBS += $(shell pkg-config --libs libcurl)
LIBS += $(shell pkg-config --libs sqlite3)
LIBS += $(shell pkg-config --cflags)

### The object files (add further files here):

OBJS = $(PLUGIN).o

### The main target:

all: $(SOFILE) i18n

### Implicit rules:

%.o: %.c
	$(CXX) $(CXXFLAGS) -rdynamic -c $(DEFINES) $(INCLUDES) -o $@ $<

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
	msgfmt -c -o $@ $<

$(I18Npot): $(wildcard *.c)
	xgettext -C -cTRANSLATORS --no-wrap --no-location -k -ktr -ktrNOOP --package-name=vdr-$(PLUGIN) --package-version=$(VERSION) --msgid-bugs-address='<see README>' -o $@ `ls $^`

%.po: $(I18Npot)
	msgmerge -U --no-wrap --no-location --backup=none -q -N $@ $<
	@touch $@

$(I18Nmsgs): $(DESTDIR)$(LOCDIR)/%/LC_MESSAGES/vdr-$(PLUGIN).mo: $(PODIR)/%.mo
	install -D -m644 $< $@

.PHONY: i18n
i18n: $(I18Nmo) $(I18Npot)

install-i18n: $(I18Nmsgs)

### Targets:

$(SOFILE): $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -rdynamic -shared $(OBJS) $(LIBS) -o $@

plugins:
	@find $(PLGSRCDIR) -maxdepth 1 -type d -name "[a-z0-9]*" -exec \
      $(MAKE) \-\-no-print-directory -C {} \;

install-lib: $(SOFILE)
	install -D $^ $(DESTDIR)$(LIBDIR)/$^.$(APIVERSION)

install-conf:
	@mkdir -p $(DESTDIR)$(PLGCONFDIR)
	@if [ ! -f $(DESTDIR)$(PLGCONFDIR)/override.conf ]; then cp conf/override.conf $(DESTDIR)$(PLGCONFDIR); fi;

install-conf_tvs:
	@mkdir -p $(DESTDIR)$(RESDIR)/plugins/$(PLUGIN)
	@cp -a conf/override_tvs.conf $(DESTDIR)$(RESDIR)/plugins/$(PLUGIN)/

install-plugins: plugins
	mkdir -p "$(PLGDEST)"
	mkdir -p "$(_PLGDEST)"
	for i in ${PLGSRCDIR}/*/Makefile; do\
      grep -q "PLUGIN.*=" "$$i" || continue;\
      i=`dirname $$i`;\
      (cd "$$i" && $(MAKE) install);\
  done;

install: install-lib install-i18n install-conf install-conf_tvs

dist: $(I18Npo) clean
	@-rm -rf $(TMPDIR)/$(ARCHIVE)
	@mkdir $(TMPDIR)/$(ARCHIVE)
	@cp -a * $(TMPDIR)/$(ARCHIVE)
	@tar czf $(PACKAGE).tgz -C $(TMPDIR) $(ARCHIVE)
	@-rm -rf $(TMPDIR)/$(ARCHIVE)
	@echo Distribution package created as $(PACKAGE).tgz

clean:
	@-rm -f $(PODIR)/*.mo $(PODIR)/*.pot
	@-rm -f $(OBJS) $(DEPFILE) *.so *.tgz core* *~
