# config.mk - mobs automatically generated makefile.
# This file will be overwritten, DO NOT EDIT!
#
# Copyright (C) 2005 Raúl Núñez de Arenas Coronado
#
# This makefile is free (as in speech) software;
# the copyright holder gives unlimited permission
# to copy, distribute and modify it.
#
# This makefile is distributed in the hope that it
# will be useful, but WITHOUT ANY WARRANTY; without
# even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.
################

##### Project information
override PROJECT:=sakura
override VERSION:=0.0

##### User configurable directories
override   PREFIX:="$(DESTDIR)/usr/local"
override   BINDIR:="$(DESTDIR)/usr/local/bin"
override  SBINDIR:="$(DESTDIR)/usr/local/sbin"
override  XBINDIR:="$(DESTDIR)/usr/local/lib/sakura"
override  CONFDIR:="$(DESTDIR)/usr/local/etc"
override  DATADIR:="$(DESTDIR)/usr/local/share/sakura"
override  INFODIR:="$(DESTDIR)/usr/local/share/info"
override   MANDIR:="$(DESTDIR)/usr/local/share/man"
override   DOCDIR:="$(DESTDIR)/usr/local/share/doc/sakura"
override   LIBDIR:="$(DESTDIR)/usr/local/lib"
override   INCDIR:="$(DESTDIR)/usr/local/include/sakura"
override STATEDIR:="$(DESTDIR)/usr/local/var/lib/sakura"
override SPOOLDIR:="$(DESTDIR)/usr/local/var/spool/sakura"

##### Package features
override ENABLE_DEBUG:=1
override F_DEBUG:=enabled

