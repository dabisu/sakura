/*  $Revision: 63 $
    This file contains development & diagnostic helpers
    
    Copyright (C) 2005,2006 Rau'l Nu'n~ez de Arenas Coronado
    Report bugs to DervishD <bugs@dervishd.net>

       This program is free software; you can redistribute it and/or
        modify it under the terms of the GNU General Public License
               as published by the Free Software Foundation;
                     either version 2 of the License,
                  or (at your option) any later version.

      This program is distributed in the hope that it will be useful,
        but WITHOUT ANY WARRANTY; without even the implied warranty
          of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
           See the GNU General Public License for more details.

      You should have received a copy of the GNU General Public License
             ('GPL') along with this program; if not, write to:

                       Free Software Foundation, Inc.
                         59 Temple Place, Suite 330
                         Boston, MA 02111-1307  USA
*/
#ifndef __MOBS_H__
#define __MOBS_H__
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <wchar.h>
#include "config.h"
#endif


/*
    This is the error handling structure.
    By now it is considered to be in alpha stage, so use it
at your own risk. It won't break anything, but may pollute the
namespace and the fields may change without notice.

FIXME: We need an EASY way of initialize the structure to all 0's.
*/
#ifndef __mobs_ehstruct
#define __mobs_ehstruct
typedef struct {
    unsigned int code;
    unsigned char *msg;

    uintmax_t natural;
    intmax_t integer;
    long double real;
    void *pointer;
} Error;
#endif


/*
    This macro is optional, but we assign here a default value so
the code of the rest of function and macros is a bit easier.
*/
#ifndef AUTHOR
#define AUTHOR ""
#endif


/*
    This macro should have been set to the module (object file) name.
I think that "(?)" is a sane default if it has not.
*/
#ifndef OBJNAME
#define OBJNAME "(?)"
#endif


/*
    This macro should have been set by the developer to
the project's name. Otherwise we complain, the project
MUST have a name. It's the Tao.
*/
#ifndef PROJECT
#error "'PROJECT' is undefined, and 'mobs.h' needs it!"
#endif


/*
    This macro should have been set by the developer to
the project's version. Otherwise we complain, because the
project should have a version. This is also the Tao...
*/
#ifndef VERSION
#error "'VERSION' is undefined, and 'mobs.h' needs it!"
#endif


/*
    This macro just stringifies its argument, no matter if it
is a literal or another macro. It's because of that it needs
the additional level of indirection. Only 'STRFY()' is meant
to be used. The other macro is just a helper.
*/
#ifndef STRFY
#define STRFY(mobs_expression) __MOBS_H_STRFY(mobs_expression)
    #ifndef __MOBS_H_STRFY
    #define __MOBS_H_STRFY(mobs_expression) #mobs_expression
    #endif
#endif

/*
    This macro does the same that 'BUG()', but with the semantics of
the C9x 'assert()' macro, and ignoring the value of 'errno'. The main
differences between 'assert()' and 'ASSERT()' may be the verbosity of
the latter and the format of the messages. Moreover, 'ASSERT()' cannot
be evaluated: take this into account when using it. A trace mark is
also printed for helping to locate this assertion.

    Use this macro to make assertions. Quite easy, isn't it? ;)))
*/
#undef ASSERT
#undef WASSERT
#ifndef NDEBUG
#define ASSERT(mobs_expr) do { if (!(mobs_expr)) {\
    fwide(stderr, -1);\
    fprintf(stderr, "*** ASSERTION FAILED, process %d aborting...\n", getpid());\
    fprintf(stderr, "*** Assertion \"(%s)\" failed at %s()@%s:%d\n", #mobs_expr, __func__, __FILE__, __LINE__);\
    if (strlen(AUTHOR))\
        fputs("Report bug to "AUTHOR"\n", stderr);\
    fflush(stderr);\
    abort();\
}} while (0)
#define WASSERT(mobs_expr) do { if (!(mobs_expr)) {\
    fwide(stderr, 1);\
    fwprintf(stderr, L"*** ASSERTION FAILED, process %d aborting...\n", getpid());\
    fwprintf(stderr, L"*** Assertion \"(%s)\" failed at %s()@%s:%d\n", #mobs_expr, __func__, __FILE__, __LINE__);\
    if (wcslen(L"" AUTHOR ""))\
        fputws(L"Report bug to "AUTHOR"\n", stderr);\
    fflush(stderr);\
    abort();\
}} while (0)
#else
#define ASSERT(mobs_expr) do {(void)(mobs_expr);} while(0)
#define WASSERT(mobs_expr) do {(void)(mobs_expr);} while(0)
#endif



/* This is a helper function to prettyprint the program name */
#ifndef __MOBS_progname
#define __MOBS_progname
static inline const char *progname (const char *);
static inline const char *progname (const char *mobs_string) {
    char *mobs_where=NULL;
    
    ASSERT(mobs_string != NULL);

    mobs_where=strrchr(mobs_string, '/');
    return mobs_where?mobs_where+1:mobs_string;
}

static inline const wchar_t *wprogname (const wchar_t *);
static inline const wchar_t *wprogname (const wchar_t *mobs_string) {
    wchar_t *mobs_where=NULL;
    
    WASSERT(mobs_string != NULL);

    mobs_where=wcsrchr(mobs_string, L'/');
    return mobs_where?mobs_where+1:mobs_string;
}
#endif


/* This one is pretty large ;))). The GPL disclaimer, just in case... */
#ifndef GPL_DISCLAIMER
#define GPL_DISCLAIMER() do {\
    fwide(stdout, -1);\
    fputs("This program is part of '"PROJECT"-"VERSION"'\n", stdout);\
    fputc('\n', stdout);\
    fputs("This program is free software;\n", stdout);\
    fputs("you can redistribute it and/or modify it under the terms of the\n", stdout);\
    fputs("GNU General Public License as published by the Free Software Foundation;\n", stdout);\
    fputs("either version 2 of the License, or (at your option) any later version.\n", stdout);\
    fputc('\n', stdout);\
    fputs("This program is distributed in the hope that it will be useful,\n", stdout);\
    fputs("but WITHOUT ANY WARRANTY; without even the implied warranty\n", stdout);\
    fputs("of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n", stdout);\
    fputs("See the GNU General Public License for more details.\n", stdout);\
    fputc('\n', stdout);\
    fputs("You should have received a copy of the GNU General Public License\n", stdout);\
    fputs("('GPL') along with this program; if not, write to:\n", stdout);\
    fputs("\tFree Software Foundation, Inc.\n", stdout);\
    fputs("\t59 Temple Place, Suite 330\n", stdout);\
    fputs("\tBoston, MA 02111-1307  USA\n", stdout);\
    fflush(stdout);\
} while(0)
#endif

#ifndef WGPL_DISCLAIMER
#define WGPL_DISCLAIMER() do {\
    fwide(stdout, 1);\
    fputws(L"This program is part of '"PROJECT"-"VERSION"'\n", stdout);\
    fputwc(L'\n', stdout);\
    fputws(L"This program is free software;\n", stdout);\
    fputws(L"you can redistribute it and/or modify it under the terms of the\n", stdout);\
    fputws(L"GNU General Public License as published by the Free Software Foundation;\n", stdout);\
    fputws(L"either version 2 of the License, or (at your option) any later version.\n", stdout);\
    fputwc(L'\n', stdout);\
    fputws(L"This program is distributed in the hope that it will be useful,\n", stdout);\
    fputws(L"but WITHOUT ANY WARRANTY; without even the implied warranty\n", stdout);\
    fputws(L"of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n", stdout);\
    fputws(L"See the GNU General Public License for more details.\n", stdout);\
    fputwc(L'\n', stdout);\
    fputws(L"You should have received a copy of the GNU General Public License\n", stdout);\
    fputws(L"('GPL') along with this program; if not, write to:\n", stdout);\
    fputws(L"\tFree Software Foundation, Inc.\n", stdout);\
    fputws(L"\t59 Temple Place, Suite 330\n", stdout);\
    fputws(L"\tBoston, MA 02111-1307  USA\n", stdout);\
    fflush(stdout);\
} while(0)
#endif



/*
    This function makes the program die nicely with 'EXIT_FAILURE',
outputting a formatted message to 'stderr', followed by '\n'.

    By now there is no further information in the output message.
*/
#ifndef __MOBS_die
#define __MOBS_die
static inline void die (const unsigned char *, ...);
static inline void die (const unsigned char *mobs_format, ...) {

    va_list mobs_args;

    ASSERT(mobs_format);
    
    fwide(stdout, -1);

    va_start(mobs_args, mobs_format);
    vfprintf(stderr, mobs_format, mobs_args);
    va_end(mobs_args);

    fflush(stderr);

    exit(EXIT_FAILURE);
}

static inline void wdie (const wchar_t *, ...);
static inline void wdie (const wchar_t *mobs_format, ...) {

    va_list mobs_args;

    WASSERT(mobs_format);

    fwide(stdout, 1);
    
    va_start(mobs_args, mobs_format);
    vfwprintf(stderr, mobs_format, mobs_args);
    va_end(mobs_args);

    fflush(stderr);

    exit(EXIT_FAILURE);
}
#endif


/*
    This macro logs an internal error and aborts.

    Use this to report *impossible* error conditions, that is, failed
assertions (consider using 'ASSERT()' instead for a more classical
semantics). It's not intended to produce information useful to the user.

    It prints the 'errno' value (only if nonzero), its associated message
and a trace mark, but the caller must take into account that this information
is not always meaningful, specially the trace mark.

    Since BUG's occur in production releases, this is unconditionally defined
in order to help the users on sending bug reports with good information ;)
It outputs the project name and version for filling the bug report :)
*/
#ifndef BUG
#define BUG() do {\
    int mobs_errno=errno;\
    fwide(stderr, -1);\
    fputs("*** INTERNAL ERROR, please report so it can be fixed!\n",stderr);\
    fprintf(stderr, "*** Process %d aborting...\n", getpid());\
    fprintf(stderr, "*** errno <%d> (%s)\n", mobs_errno, strerror(mobs_errno));\
    fputs("*** '"PROJECT"-"VERSION" (" OBJNAME ") ", stderr);\
    fprintf(stderr, "%s()@%s:%d\n", __func__, __FILE__, __LINE__);\
    if (strlen(AUTHOR))\
        fputs("*** Report bug to "AUTHOR"\n", stderr);\
    fflush(stderr);\
    abort();\
} while (0)
#endif

#ifndef WBUG
#define WBUG() do {\
    int mobs_errno=errno;\
    fwide(stderr, 1);\
    fputws(L"*** INTERNAL ERROR, please report so it can be fixed!\n",stderr);\
    fwprintf(stderr, L"*** Process %d aborting...\n", getpid());\
    fwprintf(stderr, L"*** errno <%d> (%s)\n", mobs_errno, strerror(mobs_errno));\
    fputws(L"*** '"PROJECT"-"VERSION" (" OBJNAME ") ", stderr);\
    fwprintf(stderr, L"%s()@%s:%d\n", __func__, __FILE__, __LINE__);\
    if (wcslen(L"" AUTHOR ""))\
        fputws(L"*** Report bug to "AUTHOR"\n", stderr);\
    fflush(stderr);\
    abort();\
} while (0)
#endif


/*
    This function shows the message (the format string, you know...)
together with information about the current 'errno' value (strerror...)
Just for convenience... The actual format may change...

    'errno' is not changed.
*/
#ifndef __MOBS_bang
#define __MOBS_bang
static inline void bang (const char *, ...);
static inline void bang (const char *mobs_format, ...) {

    va_list mobs_args;
    int mobs_errno=errno;

    ASSERT(mobs_format);

    fwide(stderr, -1);
    
    va_start(mobs_args, mobs_format);
    vfprintf(stderr, mobs_format, mobs_args);
    va_end(mobs_args);

    fprintf(stderr, "errno <%d> (%s)\n", mobs_errno, strerror(mobs_errno));
    fflush(stderr);

    errno=mobs_errno;
}

static inline void wbang (const wchar_t *, ...);
static inline void wbang (const wchar_t *mobs_format, ...) {

    va_list mobs_args;
    int mobs_errno=errno;

    WASSERT(mobs_format);

    fwide(stderr, 1);
    
    va_start(mobs_args, mobs_format);
    vfwprintf(stderr, mobs_format, mobs_args);
    va_end(mobs_args);

    fwprintf(stderr, L"errno <%d> (%s)\n", mobs_errno, strerror(mobs_errno));
    fflush(stderr);

    errno=mobs_errno;
}


#endif


/*
    This function outputs a formatted string as specified by 'format', but
preceeded by the module name and PID, followed by a '\n'.

    Please note that there is no way of testing whether the arguments are
correct or meaningful according with 'format'. Double check the format
string and the parameters, since 'SAY()' will silently fail and fuck up
your code if you don't use it correctly ;)))))

    Use this function to watch variables and the like. If you just want to
trace code use the 'T' macro instead. This is useful too if you want to say
something before 'BUG()'.

    Please note that if 'NDEBUG' is defined, the funcion does nothing, and
that the 'T' macro is really an alias to 'SAY()'.

    'errno' is not changed.
*/
#undef SAY
#undef WSAY
#ifndef NDEBUG
#define SAY(mobs_format,...) do {\
    int mobs_errno=errno;\
    fwide(stderr, -1);\
    fprintf(stderr, OBJNAME" [%d] ", getpid());\
    if (mobs_format) fprintf(stderr, mobs_format, ##__VA_ARGS__);\
    fputc('\n', stderr);\
    fflush(stderr);\
    errno=mobs_errno;\
} while (0)
#define WSAY(mobs_format,...) do {\
    int mobs_errno=errno;\
    fwide(stderr, 1);\
    fwprintf(stderr, L""OBJNAME" [%d] ", getpid());\
    if (mobs_format) fwprintf(stderr, mobs_format, ##__VA_ARGS__);\
    fputwc('\n', stderr);\
    fflush(stderr);\
    errno=mobs_errno;\
} while (0)
#else
#define SAY(mobs_format,...) do {} while(0)
#define WSAY(mobs_format,...) do {} while(0)
#endif
#undef T
#undef WT
#define T() SAY("%s()@%s:%d", __func__, __FILE__, __LINE__)
#define WT() WSAY(L"%s()@%s:%d", __func__, __FILE__, __LINE__)
