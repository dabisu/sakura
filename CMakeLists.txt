PROJECT(sakura)

INCLUDE (FindPkgConfig)

IF (NOT PKG_CONFIG_FOUND)
	MESSAGE (FATAL_ERROR "pkg-config not found...")
ENDIF (NOT PKG_CONFIG_FOUND)

pkg_check_modules (GTK REQUIRED gtk+-2.0>=2.6)
IF (NOT GTK_FOUND)
	MESSAGE("You don't seem to have gtk >= 2.6 development libraries installed...")
ENDIF (NOT GTK_FOUND)

pkg_check_modules (VTE REQUIRED vte>=0.11)
IF (NOT VTE_FOUND)
	MESSAGE("You don't seem to have vte >= 0.11 development libraries installed...")
ENDIF (NOT VTE_FOUND)
