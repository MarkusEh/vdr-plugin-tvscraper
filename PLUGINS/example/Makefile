#
# Makefile
#
# See the README file for copyright information and how to reach the author.
#
#

TVSCRAPER_SRC ?= ../..

include $(TVSCRAPER_SRC)/Make.config

PLUGIN = example

LIBS += $(shell pkg-config --libs libcurl)
LIBS += $(shell pkg-config --cflags)


SOFILE = libtvscraper-example.so
OBJS = example.o

CFLAGS += -I$(TVSCRAPER_SRC)

all: $(SOFILE)

### Implicit rules:

%.o: %.c
	$(CXX) $(CXXFLAGS) -c $(DEFINES) $(INCLUDES) -o $@ $<


$(SOFILE): $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -shared $(OBJS) $(LIBS) -o $@

install: $(SOFILE)
	install -D $(SOFILE) $(_PLGDEST)/$(SOFILE)

clean:
	@-rm -f $(OBJS) core* *~ *.so


#***************************************************************************
# dependencies
#***************************************************************************

example.o : example.c example.h
