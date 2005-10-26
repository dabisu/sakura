/*  $Id: mobs.h 27 2005-05-13 10:57:20Z DervishD $
    This file contains development & diagnostic helpers
    
    Copyright (C) 2005 Raúl Núñez de Arenas Coronado
    Report bugs to Raúl Núñez de Arenas Coronado <bugs@dervishd.net>

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
#include "config.h"
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
MUST have a name. It's the tao.
*/
#ifndef PROJECT
#error "'PROJECT' is undefined, and 'mobs.h' needs it!"
#endif


/*
    This macro should have been set by the developer to
the project's version. Otherwise we complain, because the
project should have a version. This is also the tao...
*/
#ifndef VERSION
#error "'VERSION' is undefined, and 'mobs.h' needs it!"
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
#ifndef NDEBUG
#ifdef AUTHOR
#define ASSERT(mobs_expr) if (!(mobs_expr)) {\
    fprintf(stderr, "\nASSERTION FAILED, process %d aborting...\n", getpid());\
    fprintf(stderr, "Assertion \"(%s)\" failed at %s()@%s:%d\n", #mobs_expr, __func__, __FILE__, __LINE__);\
    fputs("Report bugs to " AUTHOR "\n\n", stderr);\
    fflush(stderr);\
    abort();\
}
#else
#define ASSERT(mobs_expr) if (!(mobs_expr)) {\
    fprintf(stderr, "\nASSERTION FAILED, process %d aborting...\n", getpid());\
    fprintf(stderr, "Assertion \"(%s)\" failed at %s()@%s:%d\n\n", #mobs_expr, __func__, __FILE__, __LINE__);\
    fflush(stderr);\
    abort();\
}
#endif
#else
#define ASSERT(mobs_expr) (void)(mobs_expr)
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
#endif


/* This one is pretty large ;))). The GPL disclaimer, just in case... */
#ifndef GPL_DISCLAIMER
#define GPL_DISCLAIMER(mobs_string) do {\
    const char *mobs_whoami= strcmp(#mobs_string,"") ? progname(#mobs_string) : "This program";\
    if (strcasecmp(mobs_whoami, PROJECT)) fprintf(stdout, "%s is part of \'" PROJECT "-" VERSION "\'\n\n", mobs_whoami);\
    fprintf(stdout, "%s is free software;\n", mobs_whoami);\
    fputs("you can redistribute it and/or modify it under the terms of the\n", stdout);\
    fputs("GNU General Public License as published by the Free Software Foundation;\n", stdout);\
    fputs("either version 2 of the License, or (at your option) any later version.\n", stdout);\
    fputs("\n", stdout);\
    fprintf(stdout, "%s is distributed in the hope that it will be useful,\n", mobs_whoami);\
    fputs("but WITHOUT ANY WARRANTY; without even the implied warranty\n", stdout);\
    fputs("of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n", stdout);\
    fputs("See the GNU General Public License for more details.\n", stdout);\
    fputs("\n", stdout);\
    fputs("You should have received a copy of the GNU General Public License\n", stdout);\
    fputs("('GPL') along with this program; if not, write to:\n", stdout);\
    fputs("\tFree Software Foundation, Inc.\n", stdout);\
    fputs("\t59 Temple Place, Suite 330\n", stdout);\
    fputs("\tBoston, MA 02111-1307  USA\n", stdout);\
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
static inline void die (const char *, ...);
static inline void die (const char *mobs_format, ...) {

    va_list mobs_args;

    ASSERT(mobs_format);
    
    va_start(mobs_args, mobs_format);
    vfprintf(stderr, mobs_format, mobs_args);
    va_end(mobs_args);

    fputs("\n", stderr);
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
#ifndef AUTHOR
#define BUG() do {\
    fputs("\n*** INTERNAL ERROR, please report so it can be fixed!\n",stderr);\
    fprintf(stderr, "* Process %d aborting...\n", getpid());\
    if (errno > 0) fprintf(stderr, "* errno <%d> (%s)\n", errno, strerror(errno));\
    else           fputs("* (Undefined error) This is bad, dude...\n", stderr);\
    fprintf(stderr, "* " PROJECT "-" VERSION " (" OBJNAME ") %s()@%s:%d\n\n", __func__, __FILE__, __LINE__);\
    fflush(stderr);\
    abort();\
} while (0)
#else
#define BUG() do {\
    fputs("\n*** INTERNAL ERROR, please report so it can be fixed!\n",stderr);\
    fprintf(stderr, "* Process %d aborting...\n", getpid());\
    if (errno > 0) fprintf(stderr, "* errno <%d> (%s)\n", errno, strerror(errno));\
    else           fputs("* (Undefined error) This is bad, dude...\n", stderr);\
    fprintf(stderr, "* " PROJECT "-" VERSION " (" OBJNAME ") %s()@%s:%d\n", __func__, __FILE__, __LINE__);\
    fputs("Report bugs to " AUTHOR "\n\n", stderr);\
    fflush(stderr);\
    abort();\
} while (0)
#endif
#endif


/*
    This function shows the message (the format string, you know...)
together with information about the current 'errno' value (strerror...)
Just for convenience... The actual format may change...
*/
#ifndef __MOBS_bang
#define __MOBS_bang
static inline void bang (const char *, ...);
static inline void bang (const char *mobs_format, ...) {

    va_list mobs_args;
    int mobs_errno=errno;

    ASSERT(mobs_format);

    va_start(mobs_args, mobs_format);
    vfprintf(stderr, mobs_format, mobs_args);
    va_end(mobs_args);

    fprintf(stderr, "\nerrno: <%d> %s\n", errno, errno?strerror(errno):"Undefined value!");
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

    'errno' is guaranteed not to change.
*/
#undef SAY
#ifndef NDEBUG
#define SAY(mobs_format,...) do {\
    int mobs_errno=errno;\
    fprintf(stderr, "%s [%d] ", OBJNAME, getpid());\
    if (mobs_format) fprintf(stderr, mobs_format, ##__VA_ARGS__);\
    fputs("\n", stderr);\
    fflush(stderr);\
    errno=mobs_errno;\
} while (0)
#else
#define SAY(mobs_format,...)
#endif
#undef T
#define T() SAY("%s()@%s:%d", __func__, __FILE__, __LINE__)
