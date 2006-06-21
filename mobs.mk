#   $Revision: 60 $
#   This file contains helpers for building a makefile.
#
#   Copyright (C) 2005,2006 Rau'l Nu'n~ez de Arenas Coronado
#   Report bugs to DervishD <bugs@dervishd.net>
#
#       This program is free software; you can redistribute it and/or
#        modify it under the terms of the GNU General Public License
#               as published by the Free Software Foundation;
#                     either version 2 of the License,
#                  or (at your option) any later version.
#
#      This program is distributed in the hope that it will be useful,
#        but WITHOUT ANY WARRANTY; without even the implied warranty
#          of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#           See the GNU General Public License for more details.
#
#     You should have received a copy of the GNU General Public License
#            ('GPL') along with this program; if not, write to:
#
#                      Free Software Foundation, Inc.
#                        59 Temple Place, Suite 330
#                        Boston, MA 02111-1307  USA
################
.PHONY:
.DELETE_ON_ERROR:
.SECONDARY:
.PRECIOUS:
.SUFFIXES:
.DEFAULT:;@:

override .LIBPATTERNS=
export


##### Default values for commands
GCC=gcc
GXX=g++
MAKEINFO=makeinfo
YACC=bison -y
LEX=flex
AR=ar
RANLIB=ranlib
LD=gcc

##### Default values for variables
CPPFLAGS=
CFLAGS=
CXXFLAGS=
TEXIFLAGS=
YFLAGS=
LFLAGS=
ARFLAGS=
LDFLAGS=
_CPPFLAGS=
ifeq "$(origin _CPPFLAGS)" "command line"
$(error "You cannot set '_CPPFLAGS' on the command line!")
endif
_CFLAGS=
ifeq "$(origin _CFLAGS)" "command line"
$(error "You cannot set '_CFLAGS' on the command line!")
endif
_CXXFLAGS=
ifeq "$(origin _CXXFLAGS)" "command line"
$(error "You cannot set '_CXXFLAGS' on the command line!")
endif
_TEXIFLAGS=
ifeq "$(origin _TEXIFLAGS)" "command line"
$(error "You cannot set '_TEXIFLAGS' on the command line!")
endif
_YFLAGS=
ifeq "$(origin _YFLAGS)" "command line"
$(error "You cannot set '_YFLAGS' on the command line!")
endif
_LFLAGS=
ifeq "$(origin _LFLAGS)" "command line"
$(error "You cannot set '_LFLAGS' on the command line!")
endif
_ARFLAGS=
ifeq "$(origin _ARFLAGS)" "command line"
$(error "You cannot set '_ARFLAGS' on the command line!")
endif
_LDFLAGS=
ifeq "$(origin _LDFLAGS)" "command line"
$(error "You cannot set '_LDFLAGS' on the command line!")
endif
_SONAME=
ifeq "$(origin _SONAME)" "command line"
$(error "You cannot set '_SONAME' on the command line!")
endif


# This function creates installation directories.
# $(1) is the directory you want to create.
# $(2) is the directory mode, octal or symbolic.
override makedir={ \
    [ -z "$(1)" ] && { \
        printf -- "*** Missing directory name in call to 'makedir'.\n" >&2; \
        exit 1; \
    }; \
    [ -z "$(2)" ] && { \
        printf -- "*** Missing mode in call to 'makedir'.\n" >&2; \
        exit 1; \
    }; \
    [ -d "$(1)" ] || { \
    	printf -- "Creating directory \"$(1)\"...\n" >&2; \
    	mkdir -m "$(2)" -p "$(1)" > /dev/null 2>&1 || { \
        	printf -- "*** Cannot create directory \"$(1)\".\n" >&2; \
        	exit 1; \
		}; \
	}; \
	true; \
}


#   This function installs files, *just files*, to their proper
# location, logging the names of those files in the process...
# $(1) are the file names or pattern (globbing is performed).
# $(2) is the destination directory.
# $(3) is the file mode, octal or symbolic.
override install_files={ \
    [ -z "$(1)" ] && { \
        printf -- "*** Missing file names in call to 'install_files'.\n" >&2; \
        exit 1; \
    }; \
    [ -z "$(wildcard $(1))" -o -d "$(wildcard $(1))" ] && { \
        printf -- "*** Wildcard '$(1)' didn't produce a list of files!!!\n" >&2; \
	exit 1; \
    }; \
    [ -z "$(2)" ] && { \
        printf -- "*** Missing destination directory in call to 'install_files'.\n" >&2; \
        exit 1; \
    }; \
    [ -z "$(3)" ] && { \
        printf -- "*** Missing mode in call to 'install_files'.\n" >&2; \
        exit 1; \
    }; \
    for file in $(wildcard $(1)); \
    do [ -f "$$file" -a -r "$$file" ] && { \
            cp -f "$$file" "$(2)" > /dev/null 2>&1 || { \
                printf -- "*** Couldn't install file \"$$file\".\n" >&2; \
                exit 1; \
            }; \
            chmod $(3) "$(2)/`basename "$$file"`" > /dev/null 2>&1 || { \
                printf -- "*** Couldn't change permissions of file \"$$file\".\n" >&2; \
                exit 1; \
            }; \
            printf -- "$(2)/`basename "$$file"`\n"; \
    } done; true; \
}


# 	This function "munges" the input file into the output file
# substituting variables of the form @VARIABLE@ for their values.
# $(1) is the input file (the file to munge).
# $(2) is the output file (the munged file).
override munge_file={ \
    [ -z "$(1)" ] && { \
        printf -- "*** Missing input file name in call to 'munge_file'.\n" >&2; \
        exit 1; \
    }; \
    [ -z "$(2)" ] && { \
        printf -- "*** Missing output file name in call to 'munge_file'.\n" >&2; \
        exit 1; \
    }; \
    sed -e ' \
        s|\\@|<<<>>>|g; \
        s|@PROJECT@|$(PROJECT)|g; \
        s|@VERSION@|$(VERSION)|g; \
        s|@AUTHOR@|$(AUTHOR)|g; \
        s|@PREFIX@|$(PREFIX)|g; \
        s|@BINDIR@|$(SBINDIR)|g; \
        s|@SBINDIR@|$(SBINDIR)|g; \
        s|@XBINDIR@|$(XBINDIR)|g; \
        s|@CONFDIR@|$(CONFDIR)|g; \
        s|@DATADIR@|$(DATADIR)|g; \
        s|@INFODIR@|$(INFODIR)|g; \
        s|@MANDIR@|$(MANDIR)|g; \
        s|@DOCDIR@|$(DOCDIR)|g; \
        s|@LIBDIR@|$(LIBDIR)|g; \
        s|@INCDIR@|$(INCDIR)|g; \
        s|@STATEDIR@|$(STATEDIR)|g; \
        s|@SPOOLDIR@|$(SPOOLDIR)|g; \
        s|<<<>>>|@|g; \
    ' "$(1)" > "$(2)" || { \
        printf -- "*** Couldn't munge file \"$1\".\n" >&2; \
        exit 1; \
    } \
}

##### Recipes

# Recipes and builtin rules for C and C++ files (non-shared objects)
override define MAKE.o.C
    @rm -f $(basename $@).d > /dev/null 2>&1 || printf -- "* Warning: cannot delete old dependency file '%s'!\n" $(basename $@).d >&2; true
    $(strip DEPENDENCIES_OUTPUT="$(basename $@).d $@" $(GCC) -I. $(_CPPFLAGS) $(CPPFLAGS) $(_CFLAGS) $(CFLAGS) -DOBJNAME=\"$(basename $(notdir $@))\" -c -o)
endef
override define MAKE.o.C++
    @rm -f $(basename $@).d > /dev/null 2>&1 || printf -- "* Warning: cannot delete old dependency file '%s'!\n" $(basename $@).d >&2; true
    $(strip DEPENDENCIES_OUTPUT="$(basename $@).d $@" $(GXX) -I. $(_CPPFLAGS) $(CPPFLAGS) $(_CXXFLAGS) $(CXXFLAGS) -DOBJNAME=\"$(basename $(notdir $@))\" -c -o)
endef
%.o : %.c   Makefile ;$(MAKE.o.C) $@ $<
%.o : %.c++ Makefile ;$(MAKE.o.C++) $@ $<
%.o : %.C   Makefile ;$(MAKE.o.C++) $@ $<
%.o : %.cc  Makefile ;$(MAKE.o.C++) $@ $<
%.o : %.cxx Makefile ;$(MAKE.o.C++) $@ $<
%.o : %.cpp Makefile ;$(MAKE.o.C++) $@ $<
%.o : %.cp  Makefile ;$(MAKE.o.C++) $@ $<

# Recipes and builtin rules for C and C++ files (shared objects)
override define MAKE.lo.C
    @rm -f $(basename $@).d > /dev/null 2>&1 || printf -- "* Warning: cannot delete old dependency file '%s'!\n" $(basename $@).d >&2; true
    $(strip DEPENDENCIES_OUTPUT="$(basename $@).d $@" $(GCC) -I. $(_CPPFLAGS) $(CPPFLAGS) $(_CFLAGS) $(CFLAGS) -DPIC -fPIC -DOBJNAME=\"$(basename $(notdir $@))\" -c -o)
endef
override define MAKE.lo.C++
    @rm -f $(basename $@).d > /dev/null 2>&1 || printf -- "* Warning: cannot delete old dependency file '%s'!\n" $(basename $@).d >&2; true
    $(strip DEPENDENCIES_OUTPUT="$(basename $@).d $@" $(GXX) -I. $(_CPPFLAGS) $(CPPFLAGS) $(_CXXFLAGS) $(CXXFLAGS) -DPIC -fPIC -DOBJNAME=\"$(basename $(notdir $@))\" -c -o)
endef
%.lo : %.c   Makefile ;$(MAKE.lo.C) $@ $<
%.lo : %.c++ Makefile ;$(MAKE.lo.C++) $@ $<
%.lo : %.C   Makefile ;$(MAKE.lo.C++) $@ $<
%.lo : %.cc  Makefile ;$(MAKE.lo.C++) $@ $<
%.lo : %.cxx Makefile ;$(MAKE.lo.C++) $@ $<
%.lo : %.cpp Makefile ;$(MAKE.lo.C++) $@ $<
%.lo : %.cp  Makefile ;$(MAKE.lo.C++) $@ $<

# Recipes for TeXinfo files
override MAKE.info.texi    =$(strip $(MAKEINFO) $(_TEXIFLAGS) $(TEXIFLAGS) -o)
override MAKE.html.texi    =$(strip $(MAKEINFO) $(_TEXIFLAGS) $(TEXIFLAGS) --html -o)
override MAKE.docbook.texi =$(strip $(MAKEINFO) $(_TEXIFLAGS) $(TEXIFLAGS) --docbook -o)
override MAKE.xml.texi     =$(strip $(MAKEINFO) $(_TEXIFLAGS) $(TEXIFLAGS) --xml -o)
override MAKE.txt.texi     =$(strip $(MAKEINFO) $(_TEXIFLAGS) $(TEXIFLAGS) --plaintext -o)
%.info:    %.texi Makefile ;$(MAKE.info.texi) $@ $<
%.html:    %.texi Makefile ;$(MAKE.html.texi) $@ $<
%.docbook: %.texi Makefile ;$(MAKE.docbook.texi) $@ $<
%.xml:     %.texi Makefile ;$(MAKE.xml.texi) $@ $<
%.txt:     %.texi Makefile ;$(MAKE.txt.texi) $@ $<

# Recipes for Bison (yacc) files
override MAKE.C.yacc=$(strip $(YACC) $(_YFLAGS) $(YFLAGS) -o)
%.c   : %.y   Makefile ; $(MAKE.C.yacc) $@ $<
%.c++ : %.y++ Makefile ; $(MAKE.C.yacc) $@ $<
%.C   : %.Y   Makefile ; $(MAKE.C.yacc) $@ $<
%.cc  : %.yy  Makefile ; $(MAKE.C.yacc) $@ $<
%.cxx : %.yxx Makefile ; $(MAKE.C.yacc) $@ $<
%.cpp : %.ypp Makefile ; $(MAKE.C.yacc) $@ $<
%.cp  : %.yp  Makefile ; $(MAKE.C.yacc) $@ $<

# Recipes for Flex (lex) files
override MAKE.C.lex=$(strip $(LEX) $(_LFLAGS) $(LFLAGS) -o)
%.c   : %.l Makefile ; $(MAKE.C.lex) $@ $<
%.c++ : %.l Makefile ; $(MAKE.C.lex) $@ $<
%.C   : %.l Makefile ; $(MAKE.C.lex) $@ $<
%.cc  : %.l Makefile ; $(MAKE.C.lex) $@ $<
%.cxx : %.l Makefile ; $(MAKE.C.lex) $@ $<
%.cpp : %.l Makefile ; $(MAKE.C.lex) $@ $<
%.cp  : %.l Makefile ; $(MAKE.C.lex) $@ $<

# Recipes for static libraries, shared libraries and binaries
override MAKE.static=$(strip $(AR) $(_ARFLAGS) $(ARFLAGS) -rucs)
override MAKE.shared=$(strip $(LD) $(_LDFLAGS) $(LDFLAGS) -shared $(if $(_SONAME),-Xlinker -soname -Xlinker $(_SONAME)) -o)
override MAKE.binary=$(strip $(LD) $(_LDFLAGS) $(LDFLAGS) -o)
