// $Rev: 21 $
#ifndef __CFGPOOL_H_
#define __CFGPOOL_H_

#include <wchar.h>

#define CFGPOOL_EOUTOFMEMORY    1
#define CFGPOOL_EINVALIDARG     2
#define CFGPOOL_ECANTREADFILE   3


// FIXME: Put these on cfgpool.c???? Some of them are internal
#define CFGPOOL_EMPTYLINE        2
#define CFGPOOL_COMMENTLINE      3
#define CFGPOOL_MISSINGSEP       4
#define CFGPOOL_KEY2BIG          5
#define CFGPOOL_VALUE2BIG        6
#define CFGPOOL_MISSINGVALUE     7
#define CFGPOOL_LINE2BIG         8
#define CFGPOOL_FILE2BIG         9
#define CFGPOOL_TOOMANYKEYS     10
#define CFGPOOL_TOOMANYVALUES   11

// The structure to notify errors. FIXME: how about making it A POINTER? This
// way the end user cannot create the errors and we can make the structure
// opaque in the future????
typedef struct {
    int code;
    int oserr;
} cfgpool_error;

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


// Error handling
int cfgpool_geterror (CfgPool, cfgpool_error *);

char *cfgpool_dontuse(CfgPool, char *);

#endif
