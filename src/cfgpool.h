// $Rev: 38 $
#ifndef __CFGPOOL_H_
#define __CFGPOOL_H_

#include <fcntl.h>
#include <wchar.h>


// Error codes
enum {
    CFGPOOL_SUCCESS=0,

    CFGPOOL_EBADPOOL,     // Bad CfgPool object in call to method

    CFGPOOL_ENOMEMORY,      // Not enough memory

    CFGPOOL_EBADARG,        // Invalid argument in call to method

    CFGPOOL_EILLKEY,        // Key contains an invalid mb or wide character
    CFGPOOL_EILLVAL,        // Value contains an invalid mb or wide character

    CFGPOOL_EFULL,          // Pool is full!

    CFGPOOL_ENOTFOUND,      // Key not found in pool
};


typedef struct CfgPool *CfgPool;

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

// Pool filling
int cfgpool_addfile  (CfgPool, const char *);
int cfgpool_addfd    (CfgPool, int);
int cfgpool_additem  (CfgPool, const char *, const char *);
int cfgpool_addwitem (CfgPool, const wchar_t *, const wchar_t *);

// Data access
int cfgpool_getvalue      (CfgPool, const char    *, char    **);
int cfgpool_getwvalue     (CfgPool, const wchar_t *, wchar_t **);
int cfgpool_getallvalues  (CfgPool, const char    *, char    **);
int cfgpool_getallwvalues (CfgPool, const wchar_t *, wchar_t **);

// Data dumping
int cfgpool_dumptofile (CfgPool, const char *, int, mode_t);
int cfgpool_dumptofd   (CfgPool, int);

int cfgpool_humanreadable (CfgPool);

#endif
