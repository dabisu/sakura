# Makefile - mobs automatically generated Makefile.
# This file will be overwritten, DO NOT EDIT!
#
# This makefile is distributed under the same terms
# than the file 'Makefile.in'. The copyright holder
# of 'Makefile.in' is considered to be the copyright
# holder of this file. See 'Makefile.in' for details.
#
###################
include mobs.mk   #
include config.mk #
###################
.PHONY: all clean distclean install

override _CFLAGS=`pkg-config --cflags gtk+-2.0 vte`
override _LDFLAGS=`pkg-config --libs gtk+-2.0 vte`

ifdef ENABLE_DEBUG
override _CPPFLAGS=-UNDEBUG
override _CFLAGS+=-g -O0 -Wall
else
override _CPPFLAGS=-DNDEBUG
override _CFLAGS+=-O2
endif

SAKURA_OBJECTS=$(patsubst %.c,%.o,$(wildcard src/*.c))
DEPENDENCIES=$(patsubst %.o,%.d,$(SAKURA_OBJECTS))

all: src/sakura

src/sakura: $(SAKURA_OBJECTS)
	$(MAKE.binary) $(SAKURA_OBJECTS)

clean:
	@rm -rf $(SAKURA_OBJECTS) $(DEPENDENCIES) src/sakura 

distclean: clean
	@rm -rf Makefile config.mk config.h

DIRMODE="a+rx,u+w"
install: all

##### Dependency handling
ifneq ($(DEPENDENCIES),)
-include $(sort $(DEPENDENCIES))
endif
