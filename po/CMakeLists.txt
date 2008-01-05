# Localization
FIND_PROGRAM(GETTEXT_MSGFMT_EXECUTABLE msgfmt)

IF(NOT GETTEXT_MSGFMT_EXECUTABLE)
	MESSAGE( "------
			NOTE: msgfmt not found. Translations will *not* be installed
			------")
ELSE(NOT GETTEXT_MSGFMT_EXECUTABLE)

SET(catalogname sakura)

ADD_CUSTOM_TARGET(translations ALL)

FILE(GLOB PO_FILES *.po)

# TODO: Regenerate *.po files
FOREACH(_poFile ${PO_FILES})
	GET_FILENAME_COMPONENT(_lang ${_poFile} NAME_WE)
	SET(_moFile ${_lang}.mo)
	ADD_CUSTOM_COMMAND(TARGET translations
			COMMAND ${GETTEXT_MSGFMT_EXECUTABLE} --check -o ${_moFile} ${_poFile}
			DEPENDS ${_poFile})
	INSTALL(FILES ${_moFile}
		   	DESTINATION share/locale/${_lang}/LC_MESSAGES/
		   	RENAME ${catalogname}.mo)
ENDFOREACH(_poFile ${PO_FILES})

ENDIF(NOT GETTEXT_MSGFMT_EXECUTABLE)


