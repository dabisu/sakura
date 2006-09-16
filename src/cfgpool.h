// $Rev: 19 $
#ifndef __CFGPOOL_H_
#define __CFGPOOL_H_

#include <wchar.h>

#define CFGPOOL_SUCCESS          0
#define CFGPOOL_ENOMEM           1

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
#define CFGPOOL_CANTREAD        12
#define CFGPOOL_ENOENT          13
#define CFGPOOL_EINVAL          14

// The structure to notify errors. FIXME: how about making it A POINTER? This
// way the end user cannot create the errors and we can make the structure
// opaque in the future????
typedef struct {
    int code;
    int oserr;
} cfgpool_error;

typedef struct CfgPool *CfgPool;

// Library initialization
int cfgpool_init (void);
int cfgpool_done (void);

// Pool creation and destruction
CfgPool cfgpool_create (void);
void    cfgpool_delete (CfgPool);

// Pool filling
int cfgpool_addfile (CfgPool, const unsigned char *);
int cfgpool_addfd   (CfgPool, int);
int cfgpool_additem (CfgPool, const char *, const char *);
int cfgpool_addwitem (CfgPool, const wchar_t *, const wchar_t *);


// Error handling
int cfgpool_geterror (CfgPool, cfgpool_error *);

char * cfgpool_dontuse(CfgPool, char *);

#endif
