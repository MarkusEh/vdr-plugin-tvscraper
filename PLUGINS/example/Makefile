#
# Makefile
#
# See the README file for copyright information and how to reach the author.
#
#

TVSCRAPER_SRC ?= ../..
include $(TVSCRAPER_SRC)/Make.config-tvscraper

PKG_CONFIG ?= pkg-config
PKGCFG = $(if $(VDRDIR),$(shell $(PKG_CONFIG) --variable=$(1) $(VDRDIR)/vdr.pc),$(shell PKG_CONFIG_PATH="$$PKG_CONFIG_PATH:../../.." $(PKG_CONFIG) --variable=$(1) vdr))

export CXXFLAGS = $(call PKGCFG,cxxflags)

CXXFLAGS += -std=c++17
CXXFLAGS += $(shell $(PKG_CONFIG) --cflags-only-other libcurl)
CXXFLAGS += $(shell $(PKG_CONFIG) --cflags gumbo)

INCLUDES += -I$(TVSCRAPER_SRC)
INCLUDES += $(shell $(PKG_CONFIG) --cflags-only-I libcurl)

LIBS += $(shell $(PKG_CONFIG) --libs libcurl)
LIBS += $(shell $(PKG_CONFIG) --libs gumbo)

DEFINES += -DPLGDIR='"$(PLGDEST)"'

SOFILE = libtvscraper-example.so
OBJS = example.o

all: $(SOFILE)

### Implicit rules:

%.o: %.c
	$(CXX) $(CXXFLAGS) -c $(DEFINES) $(INCLUDES) -o $@ $<


$(SOFILE): $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -shared $(OBJS) $(LIBS) -o $@

install: $(SOFILE)
	mkdir -p "$(_PLGDEST)"
	install -D $(SOFILE) $(_PLGDEST)/$(SOFILE)

clean:
	@-rm -f $(OBJS) core* *~ *.so


#***************************************************************************
# dependencies
#***************************************************************************

example.o : example.c example.h
