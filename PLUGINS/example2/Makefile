#
# Makefile
#
# See the README file for copyright information and how to reach the author.
#
#


PKG_CONFIG ?= pkg-config
PKGCFG = $(if $(VDRDIR),$(shell $(PKG_CONFIG) --variable=$(1) $(VDRDIR)/vdr.pc),$(shell PKG_CONFIG_PATH="$$PKG_CONFIG_PATH:../../.." $(PKG_CONFIG) --variable=$(1) vdr))
LIBDIR = $(call PKGCFG,libdir)
PLGCFG = $(call PKGCFG,plgcfg)

TVSCRAPER_SRC ?= ../..
include $(TVSCRAPER_SRC)/Make.config-tvscraper

-include $(PLGCFG)

export CXXFLAGS = $(call PKGCFG,cxxflags)

CXXFLAGS += -std=c++17
CXXFLAGS += $(shell $(PKG_CONFIG) --cflags-only-other libcurl)

INCLUDES += -I$(TVSCRAPER_SRC)
INCLUDES += $(shell $(PKG_CONFIG) --cflags-only-I libcurl)

LIBS += $(shell $(PKG_CONFIG) --libs libcurl)

DEFINES += -DPLGDIR='"$(PLGDEST)"'

SOFILE = libtvscraper-example2.so
OBJS = example2.o

all: $(SOFILE)

### Implicit rules:

%.o: %.c
	$(CXX) $(CXXFLAGS) -c $(DEFINES) $(INCLUDES) -o $@ $<


$(SOFILE): $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -shared $(OBJS) $(LIBS) -o $@

install: $(SOFILE)
	mkdir -p "$(DESTDIR)$(PLGDEST)"
	install -D $(SOFILE) $(DESTDIR)$(PLGDEST)/$(SOFILE)

clean:
	@-rm -f $(OBJS) core* *~ *.so


#***************************************************************************
# dependencies
#***************************************************************************

example2.o : example2.c example2.h
