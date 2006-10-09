/*
Configuration pool (headers).
$Rev: 44 $

   Copyright (C) 2006 Rau'l Nu'n~ez de Arenas Coronado
   Report bugs to DervishD <bugs@dervishd.net>

         This program is free software; you can redistribute it and/or
          modify it under the terms of the GNU General Public License
            version 2 as published by the Free Software Foundation.

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

#ifndef __CFGPOOL_H_
#define __CFGPOOL_H_

#include <fcntl.h>
#include <wchar.h>


// Error codes
enum {
    CFGPOOL_SUCCESS=0,

    CFGPOOL_EBADPOOL,       // Bad CfgPool object in call to method

    CFGPOOL_ENOMEMORY,      // Not enough memory

    CFGPOOL_EBADARG,        // Invalid argument in call to method
    
    CFGPOOL_EBADITEM,       // Bad config item was read from file

    CFGPOOL_EILLKEY,        // Key contains an invalid mb or wide character
    CFGPOOL_EILLVAL,        // Value contains an invalid mb or wide character

    CFGPOOL_EFULL,          // Pool is full!

    CFGPOOL_ENOTFOUND,      // Key not found in pool
};


// Exported types
typedef struct CfgPool *CfgPool;
typedef struct {
    char *filename;
    int fildes;
    uintmax_t lineno;
} CIL;  // Config Item Location
typedef int VHook (wchar_t *, wchar_t *, CIL);


/*

    NOTE about the validation hook: the validation hook is a callback function
to perform "early validation", that is, to validate keyword-value pairs as
soon as they are read and parsed from the file (or memory buffer, or...).

    The function gets three parameters. The first one is the parsed keyword,
as a wchar_t string, the second one is the parsed value (which may be NULL or
empty), as a wchar_t string too. Both parameters may be modified by the
function while validating them, prior to add them to the pool. The third
parameter is de "CIL" (Config Item Location) and says where the item was read
from. Currently only filenames and file descriptors are supported.

    The function can use the given information e.g. to validate the keyword
against a list of valid keywords, to check type and range of value, print a
warning about a potentially invalid config item, etc.

    If the function decides that the item is valid, it must return 0. If, on
the contrary, the function decides that the item is invalid, it must return a
positive or a negative value. A positive value tells the library that the item
is invalid and that it must be considered a fatal exception, so no further
data adquiring or parsing must be done. The library then returns to the caller
the "CFGPOOL_EBADITEM" error code (it is suggested that the validation hook
itself uses this code, too, for indicating such condition). A negative value
indicates that although the config item read is invalid and cannot be added to
the pool, further processing can continue. It is advised that the validation
hook returns "-CFGPOOL_EBADITEM" in this case.

    Please note that you can "exit()", "abort()" or whatever from this hook,
and you may even call any library function (they're reentrant), but in the
common case you should just evaluate the arguments and return one of 0,
"CFGPOOL_EBADITEM" or "-CFGPOOL_EBADITEM", for simplicity.

    If the function returns 0 for a NULL value, it will be regarded as valid,
but it WON'T be added to the pool. This is current library policy.

*/



/*
    NOTE about the use of "plain char": I use "plain char" and not "unsigned
char" even when dealing with multibyte strings because the ANSI C99 standard
clearly states that "plain char" is the type to use when dealing with chars.
The use of "unsigned char" and "signed char" must be reserved when dealing
with short integers, as these two types are integral types unrelated with
characters. Probably it was a misnomer and an error to introduce signedness to
plain char: in the end, you could always use "unsigned short" and "signed
short" for the same purpose. Myself, I'm going to try to avoid "unsigned char"
and "signed char" in my code...
*/


// Library initialization
int cfgpool_init (void);
int cfgpool_done (void);

// Pool creation and destruction
CfgPool cfgpool_create (void);
void    cfgpool_delete (CfgPool);

// Accessors
int cfgpool_setvhook (CfgPool, VHook *);

// Pool filling
int cfgpool_addfile  (CfgPool, const char *);
int cfgpool_addfd    (CfgPool, int, const char *);
int cfgpool_additem  (CfgPool, const char *, const char *);
int cfgpool_addwitem (CfgPool, const wchar_t *, const wchar_t *);

// Data access
int cfgpool_getvalue      (CfgPool, const char    *, char    **);
int cfgpool_getwvalue     (CfgPool, const wchar_t *, wchar_t **);
int cfgpool_getallvalues  (CfgPool, const char    *, char    ***);
int cfgpool_getallwvalues (CfgPool, const wchar_t *, wchar_t ***);

// Data dumping
int cfgpool_dumptofile (CfgPool, const char *, int, mode_t);
int cfgpool_dumptofd   (CfgPool, int);

int cfgpool_humanreadable (CfgPool);

#endif
