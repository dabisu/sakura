CMAKE_MINIMUM_REQUIRED (VERSION 2.4 FATAL_ERROR)
PROJECT (sakura)
SET (AUTHOR "David Gómez Espinosa" INTERNAL "Author")
SET (VERSION "1.3.0")
MESSAGE("${VERSION}")

# TODO: support user defined directories
# BINDIR, DATADIR

INCLUDE (FindPkgConfig)

IF (NOT PKG_CONFIG_FOUND)
	MESSAGE (FATAL_ERROR "pkg-config not found...")
ENDIF (NOT PKG_CONFIG_FOUND)

pkg_check_modules (GTK REQUIRED gtk+-2.0>=2.6)
IF (NOT GTK_FOUND)
	MESSAGE(FATAL_ERROR "You don't seem to have gtk >= 2.6 development libraries installed...")
ENDIF (NOT GTK_FOUND)

pkg_check_modules (VTE REQUIRED vte>=0.11)
IF (NOT VTE_FOUND)
	MESSAGE(FATAL_ERROR "You don't seem to have vte >= 0.11 development libraries installed...")
ENDIF (NOT VTE_FOUND)

ADD_DEFINITIONS (-DVERSION=\\\"${VERSION}\\\")

INCLUDE_DIRECTORIES (. ${GTK_INCLUDE_DIRS} ${VTE_INCLUDE_DIRS})
LINK_DIRECTORIES (${GTK_LIBRARY_DIRS} ${VTE_LIBRARY_DIRS})
LINK_LIBRARIES (${GTK_LIBRARIES} ${VTE_LIBRARIES})
ADD_EXECUTABLE (sakura src/sakura.c)
ADD_DEPENDENCIES (sakura src/sakura.c src/sakura.h)

# TODO: check permissions	
INSTALL (TARGETS sakura
		 RUNTIME DESTINATION bin)	
INSTALL (FILES sakura.desktop
		 DESTINATION share/applications)
INSTALL (FILES terminal-tango.png
		 DESTINATION share/pixmaps)
