// $Rev: 26 $
#ifndef __CFGPOOL_H_
#define __CFGPOOL_H_

#include <wchar.h>


// Error codes
enum {
    CFGPOOL_EBADPOOL=1,     // Bad CfgPool object in call to method

    CFGPOOL_EOUTOFMEMORY,   // Not enough memory
    CFGPOOL_EBADARG,        // Invalid argument in call to method
    CFGPOOL_EFULLPOOL,      // Pool is full!
    CFGPOOL_EBADFILE,       // File cannot be read
    CFGPOOL_EILLFORMED,     // Ill formed line
    CFGPOOL_ENOTFOUND,      // Key not found in pool
};


// Extended error codes
enum {
    CFGPOOL_XEBADPOOL=1,    // Bad CfgPool object in call to method
    
    CFGPOOL_XEBADKEY,       // Key is invalid in the current locale
    CFGPOOL_XENULLKEY,      // NULL given as key in call to method
    CFGPOOL_XEVOIDKEY,      // Empty string given as key in call to method
    CFGPOOL_XEKEY2BIG,      // Key size >= SIZE_MAX
    CFGPOOL_XEKEYCOPY,      // Not enough memory to copy key
    CFGPOOL_XEMANYKEYS,     // Too many keys in the pool

    CFGPOOL_XEBADVALUE,     // Value is invalid in the current locale
    CFGPOOL_XENULLVALUE,    // NULL given as value in call to method
    CFGPOOL_XEVOIDVALUE,    // Empty string given as value in call to method
    CFGPOOL_XEVALUE2BIG,    // Value size >= SIZE_MAX
    CFGPOOL_XEVALUECOPY,    // Not enough memory to copy value
    CFGPOOL_XEMANYVALUES,   // Too many values in a key

    CFGPOOL_XENULL,         // Null pointer given to method
    CFGPOOL_XEADDITEM,      // Not enough memory to add a new item to the pool

    CFGPOOL_XENULLFILE,     // NULL given as filename in call to method
    CFGPOOL_XELINE2BIG,     // A line read from a file is too big
};


// The structure to notify errors.
typedef struct {
    int code;           // This is the same as the last error returned by a method
    int xcode;          // Extended error code
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
int cfgpool_addfile  (CfgPool, const char *, uintmax_t *);
int cfgpool_addfd    (CfgPool, int, uintmax_t *);
int cfgpool_additem  (CfgPool, const char *, const char *);
int cfgpool_addwitem (CfgPool, const wchar_t *, const wchar_t *);

// Error code retrieving
int cfgpool_geterror  (CfgPool);
int cfgpool_getxerror (CfgPool);

// Data access
int cfgpool_getvalue      (CfgPool, const char    *, char    **);
int cfgpool_getwvalue     (CfgPool, const wchar_t *, wchar_t **);
int cfgpool_getallvalues  (CfgPool, const char    *, char    **);
int cfgpool_getallwvalues (CfgPool, const wchar_t *, wchar_t **);
int cfgpool_getinfo       (CfgPool, const char    *, char    **);
int cfgpool_getallinfo    (CfgPool, const char    *, char    **);

char *cfgpool_dontuse(CfgPool, char *);

#endif
