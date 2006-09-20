// $Rev: 26 $
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <stddef.h>
#include <fcntl.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <wchar.h>
#include <wctype.h>
#include "cfgpool.h"


// Private error codes for all the internal functions
enum {
    CFGPOOL__EBADPOOL=1,        // Bad CfgPool object in call to method

    CFGPOOL__ENULLSRC,          // Source pointer argument is NULL
    CFGPOOL__ENULLDST,          // Destination pointer argument is NULL
    CFGPOOL__EBADMB,            // An invalid multibyte sequence detected
    CFGPOOL__EBADWC,            // An invalid wide-character sequence detected

    CFGPOOL__ES2BIG,            // The string is too large
    CFGPOOL__EK2BIG,            // Key length is too large
    CFGPOOL__EV2BIG,            // Value length is too large
    CFGPOOL__EL2BIG,            // Line too big
    CFGPOOL__EF2BIG,            // File too big

    CFGPOOL__EVOID,             // The string is empty

    CFGPOOL__E2MANYK,           // Too many keys in the pool
    CFGPOOL__E2MANYV,           // Too many values in a key

    CFGPOOL__EKNOMEM,           // Not enough memory to allocate a key
    CFGPOOL__EVNOMEM,           // Not enough memory to allocate a value
    CFGPOOL__EINOMEM,           // Not enough memory to allocate a config item
    CFGPOOL__ESNOMEM,           // Not enough memory to allocate a string
    CFGPOOL__ELNOMEM,           // Not enough memory to allocate a line

    CFGPOOL__EMISSING,          // Missing separator on a line
    CFGPOOL__EIGNORE,           // Empty or comment line, ignore
};


/*

    NOTE about "goto": Yes, I use the "goto" keyword, so I'm a sinner! The
fact is that it improves readability and code flow because that way all
functions have only ONE entry point (of course) and ONE exit point.

*/

/*

    This is the default "bit-size" for a cfgpool hash table. The "bit-size" is
the size of the hash expressed as the exponent of the corresponding power of
two. For example, if the "bit-size" is 8, hash table will have 2^8 buckets.

*/
#define CFGPOOL_DEFAULT_BITSIZE 8

#if (1 << CFGPOOL_DEFAULT_BITSIZE) >= SIZE_MAX
#error "CFGPOOL_DEFAULT_BITSIZE" is too large
#endif

////////////////////////////////////////////////////////////


         ///////////////////////
        //                     //
        //  CfgPool structure  //
        //                     //
         ///////////////////////

 
/*

    By now, the CfgPool is implemented using a hash table. The hash table is
implemented as an array, being every slot in the array a bucket. A bucket is a
pointer to a CfgItem structure. The CfgItem structure has a "next" pointer to
be able to store them in a single linked list, because we are using chaining
to handle collisions.

    Each CfgItem is capable of storing multiple values for a key. It does that
using an array. I've chosen arrays because a linked list will be overkill for
this task: in the common case, only a bunch of values will be stored in a key,
and then we can pay the price for an array resize and some memory copying, it
won't be that slow. Moreover, if the number of values grow a lot, the pool
won't perform very good anyway, so...
    
*/
typedef struct CfgItem *CfgItem;
struct CfgPool {
    size_t buckets;         // The number of buckets in the hash table
    size_t numitems;        // The number of elements in the hash table
    struct CfgItem {
        CfgItem next;       // To implement the linked list
        size_t numvalues;   // Number of values this item has
        wchar_t *key;       // This item's key
        struct CfgValue {
            char *src;      // Where each value was defined
            wchar_t *value; // The value itself
        } *values;          // This item's set of values
    } **data;               // The data (the array of buckets)
    int error;              // The last error
    int xerror;             // The last extended error
};

#define CFGPOOL_MAXBUCKETS  ((SIZE_MAX>>1)/sizeof(CfgItem))

// FIXME: should these be exported trhu accessor methods?
#define CFGPOOL_MAXVALUES   (SIZE_MAX/sizeof(struct CfgValue))
#define CFGPOOL_MAXKEYS     (SIZE_MAX)


// Prototypes for internal functions 
static        int     internal_additem       (CfgPool, wchar_t *, wchar_t *, char *);
static        int     internal_mungefd       (CfgPool, int, const char *, uintmax_t *);
static        int     internal_parse         (const wchar_t *, wchar_t **, wchar_t **);
static        char   *internal_xnprintf      (const char *, ...);
static        int     internal_mbstowcs      (wchar_t **, const char *);
static        int     internal_wcstombs      (char **, const wchar_t *);
static inline size_t  internal_one_at_a_time (const wchar_t *);
static        CfgItem internal_getdata       (CfgPool, const wchar_t *);


////////////////////////////////////////////////////////////


         ///////////////////////////////////////////////////////
        //                                                     //
        //  Library initialization/deinitialization functions  //
        //                                                     //
         //////////////////////////////////////////////////////


/*

    This function initializes the entire library.
    It doesn't do anything (yet).

*/
int                             // Error code
cfgpool_init (
void                            // void
){
    return 0;
}


/*

    This function de-initializes the entire library.
    It doesn't do anything (yet).

*/
int                             // Error code
cfgpool_done (
void                            // void
){
    return 0;
}

////////////////////////////////////////////////////////////


         //////////////////////////////////////////////
        //                                            //
        //  CfgPool objects construction/destruction  //
        //                                            //
         //////////////////////////////////////////////
        

/*

    This function creates a new CfgPool object.
    It returns NULL if no valid object could be created due to lack of memory.

*/
CfgPool                         // The newly created pool
cfgpool_create (
void                            // void
){

    CfgPool self = malloc(sizeof(struct CfgPool));
    if (!self) goto out;

    self->buckets = 0;
    self->numitems = 0;
    self->error = 0;
    self->xerror = 0;

    self->data = calloc(1 << CFGPOOL_DEFAULT_BITSIZE, sizeof(CfgItem));
    if (self->data) self->buckets = 1 << CFGPOOL_DEFAULT_BITSIZE;
    else {  // No memory, free the object and return NULL
        free(self);
        self=NULL;
    };

out:
    return self;

}


/*

    This function destroys a CfgPool object.

*/
void                            // void
cfgpool_delete (
CfgPool self                    // The pool to destroy
){

    if (!self) return;

    while (self->buckets--) {
        // Process next bucket
        CfgItem tmpitem;
        
        // Double parentheses so GCC doesn't complain
        while ((tmpitem = self->data[self->buckets])) {

            // OK, the bucket is not empty

            // Go on...
            self->data[self->buckets] = tmpitem->next;

            // Free the element
            while (tmpitem->numvalues--) {
                struct CfgValue tmpvalue = tmpitem->values[tmpitem->numvalues];
                if (tmpvalue.src) free(tmpvalue.src);
                free(tmpvalue.value);
            }
            free (tmpitem->key);
            free (tmpitem->values);
            free (tmpitem);
        }
        
    }
    if (self->data) free(self->data);
    free(self);
}

////////////////////////////////////////////////////////////


         ////////////////////////////////////////
        //                                      //
        //  High level item addition functions  //
        //                                      //
         ////////////////////////////////////////


/*

    This function adds a new configuration item, that is, a keyword (throught
the "key" argument) and its corresponding value (in the "value" argument) to
the pool specified in the "self" argument. This is the "wchar_t *" interface.

    The given strings must be NUL-terminated, wide-character strings, and they
are not added to the pool: a copy is made and then added to the pool, so you
can safely use dynamic duration strings. The original contents of "key" and
"value" are preserved.

    If no error occurs the function returns 0, and an error code otherwise. If
the caller wants additional information about the error, he must call the
"cfgpool_error()" method (except for the CFGPOOL_EBADPOOL error, of course).
Error codes that this function may return are:

    - CFGPOOL_EBADPOOL if "self" is a NULL pointer

    - CFGPOOL_EBADARG if "key" or "value" are invalid. The extended error
      codes in this case can be:
      
          - CFGPOOL_XENULLKEY if "key" is a NULL pointer

          - CFGPOOL_XENULLVALUE if "value" is a NULL pointer

          - CFGPOOL_XEKEY2BIG if "key" string is too big

          - CFGPOOL_XEVALUE2BIG if "value" string is too big
          
          - CFGPOOL_XEVOIDKEY if "key" is empty
          
          - CFGPOOL_XEVOIDVALUE if "value" is empty

    - CFGPOOL_EOUTOFMEMORY if the function runs out of memory. The extended
      error codes in this case can be:

          - CFGPOOL_XEKEYCOPY if the function did run out of memory when
            trying to make a copy of "key"

          - CFGPOOL_XEVALUECOPY if the function did run out of memory when
            trying to make a copy of "value"

          - CFGPOOL_XEADDITEM if the function did run out of memory when
            trying to add the item to the pool

    - CFGPOOL_FULLPOOL if there are already too many keys in the pool or too
      many values for "key". The extended error codes are:

          - CFGPOOL_XEMANYKEYS if there are too many keys in the pool

          - CFGPOOL_XEMANYVALUES if "key" already has too many values

*/
int                             // Error code
cfgpool_addwitem (
CfgPool self,                   // The pool where the item will be added
const wchar_t *key,             // The key of the item
const wchar_t *value            // The value of the item
){

    // FIXME: we shouldn't allow to make operations in a pool with a pending error?
    if (!self) return CFGPOOL_EBADPOOL;
    self->error  = 0;
    self->xerror = 0;

    if (!key) {
        self->error  = CFGPOOL_EBADARG;
        self->xerror = CFGPOOL_XENULLKEY;
        goto out;
    }
    if (!value) {
        self->error  = CFGPOOL_EBADARG;
        self->xerror = CFGPOOL_XENULLVALUE;
        goto out;
    }

    wchar_t *_key = NULL;
    wchar_t *_value = NULL;

    size_t len;
    len = wcslen(key);
    if (len == SIZE_MAX) {
        self->error  = CFGPOOL_EBADARG;
        self->xerror = CFGPOOL_XEKEY2BIG;
        goto out;
    }
    if (len == 0) {
        self->error  = CFGPOOL_EBADARG;
        self->xerror = CFGPOOL_XEVOIDKEY;
        goto out;
    }

    len++;    
    _key = malloc(len * sizeof(wchar_t));
    if (!_key) {
        self->error  = CFGPOOL_EOUTOFMEMORY;
        self->xerror = CFGPOOL_XEKEYCOPY;
        goto out;
    }

    len = wcslen(value);
    if (len == SIZE_MAX) {
        self->error  = CFGPOOL_EBADARG;
        self->xerror = CFGPOOL_XEVALUE2BIG;
        goto out;
    }
    if (len == 0) {
        self->error  = CFGPOOL_EBADARG;
        self->xerror = CFGPOOL_XEVOIDVALUE;
        goto out;
    }

    len++;
    _value = malloc(len * sizeof(wchar_t));
    if (!_value) {
        self->error  = CFGPOOL_EOUTOFMEMORY;
        self->xerror = CFGPOOL_XEVALUECOPY;
        goto out;
    }

    wcscpy(_key, key);
    wcscpy(_value, value);

    switch(internal_additem(self, _key, _value, NULL)) {
        case CFGPOOL__E2MANYK:
            self->error  = CFGPOOL_EFULLPOOL;
            self->xerror = CFGPOOL_XEMANYKEYS;
            break;
        case CFGPOOL__E2MANYV:
            self->error  = CFGPOOL_EFULLPOOL;
            self->xerror = CFGPOOL_XEMANYVALUES;
            break;
        case CFGPOOL__EVNOMEM:
        case CFGPOOL__EINOMEM:
            self->error  = CFGPOOL_EOUTOFMEMORY;
            self->xerror = CFGPOOL_XEADDITEM;
            break;
    }

out:    
    if (!_value && _key) free(_key);
    return self->error;
}


/*

    This function adds a new configuration item, that is, a keyword (throught
the "key" argument) and its corresponding value (in the "value" argument) to
the pool specified in the "self" argument. This is the "char *" interface.

    The given strings must be NUL-terminated, multibyte or monobyte strings,
and they are not added to the pool: a copy is made and then added to the pool,
so you can safely use dynamic duration strings. The original contents of "key"
and "value" are preserved.

    If no error occurs the function returns 0, and an error code otherwise. If
the caller wants additional information about the error, he must call the
"cfgpool_error()" method (except for the CFGPOOL_EBADPOOL error, of course).
Error codes that this function may return are:

    - CFGPOOL_EBADPOOL if "self" is a NULL pointer

    - CFGPOOL_EBADARG if "key" or "value" are invalid. The extended error
      codes in this case can be:

          - CFGPOOL_XEBADKEY if "key" is invalid in current locale
          
          - CFGPOOL_XEBADVALUE if "value" is invalid in current locale
      
          - CFGPOOL_XENULLKEY if "key" is a NULL pointer

          - CFGPOOL_XENULLVALUE if "value" is a NULL pointer

          - CFGPOOL_XEKEY2BIG if "key" string is too big

          - CFGPOOL_XEVALUE2BIG if "value" string is too big
          
          - CFGPOOL_XEVOIDKEY if "key" is empty
          
          - CFGPOOL_XEVOIDVALUE if "value" is empty
          
    - CFGPOOL_EOUTOFMEMORY if the function runs out of memory. The extended
      error codes in this case can be:

          - CFGPOOL_XEKEYCOPY if the function did run out of memory when
            trying to make a copy of "key"

          - CFGPOOL_XEVALUECOPY if the function did run out of memory when
            trying to make a copy of "value"

          - CFGPOOL_XEADDITEM if the function did run out of memory when
            trying to add the item to the pool

    - CFGPOOL_FULLPOOL if there are already too many keys in the pool or too
      many values for "key". The extended error codes are:

          - CFGPOOL_XEMANYKEYS if there are too many keys in the pool

          - CFGPOOL_XEMANYVALUES if "key" already has too many values

*/
int                             // Error code
cfgpool_additem (
CfgPool self,                   // The pool where the item will be added
const char *key,                // The key of the item
const char *value               // The value of the item
){

    // FIXME: we shouldn't allow to make operations in a pool with a pending error?
    if (!self) return CFGPOOL_EBADPOOL;
    self->error  = 0;
    self->xerror = 0;

    if (!key) {
        self->error  = CFGPOOL_EBADARG;
        self->xerror = CFGPOOL_XENULLKEY;
        goto out;
    }
    if (!value) {
        self->error  = CFGPOOL_EBADARG;
        self->xerror = CFGPOOL_XENULLVALUE;
        goto out;
    }

    wchar_t *wkey = NULL;
    wchar_t *wvalue = NULL;

    switch (internal_mbstowcs(&wkey, key)) {
        case CFGPOOL__ESNOMEM:
            self->error  = CFGPOOL_EOUTOFMEMORY;
            self->xerror = CFGPOOL_XEKEYCOPY;
            goto out;
        case CFGPOOL__EBADMB:
            self->error  = CFGPOOL_EBADARG;
            self->xerror = CFGPOOL_XEBADKEY;
            goto out;
        case CFGPOOL__ES2BIG:
            self->error  = CFGPOOL_EBADARG;
            self->xerror = CFGPOOL_XEKEY2BIG;
            goto out;
        case CFGPOOL__EVOID:
            self->error  = CFGPOOL_EBADARG;
            self->xerror = CFGPOOL_XEVOIDKEY;
            goto out;
    }

    switch (internal_mbstowcs(&wvalue, value)) {
        case CFGPOOL_EOUTOFMEMORY:
            self->error  = CFGPOOL_EOUTOFMEMORY;
            self->xerror = CFGPOOL_XEVALUECOPY;
            goto out;
        case CFGPOOL__EBADMB:
            self->error  = CFGPOOL_EBADARG;
            self->xerror = CFGPOOL_XEBADVALUE;
            goto out;
        case CFGPOOL__ES2BIG:
            self->error  = CFGPOOL_EBADARG;
            self->xerror = CFGPOOL_XEVALUE2BIG;
            goto out;
        case CFGPOOL__EVOID:
            self->error  = CFGPOOL_EBADARG;
            self->xerror = CFGPOOL_XEVOIDVALUE;
            goto out;
    }

    if (wcslen(wkey) == SIZE_MAX) {
        self->error  = CFGPOOL_EBADARG;
        self->xerror = CFGPOOL_XEKEY2BIG;
        goto out;
    }

    if (wcslen(wvalue) == SIZE_MAX) {
        self->error  = CFGPOOL_EBADARG;
        self->xerror = CFGPOOL_XEVALUE2BIG;
        goto out;
    }

    switch(internal_additem(self, wkey, wvalue, NULL)) {
        case CFGPOOL__E2MANYK:
            self->error  = CFGPOOL_EFULLPOOL;
            self->xerror = CFGPOOL_XEMANYKEYS;
            break;
        case CFGPOOL__E2MANYV:
            self->error  = CFGPOOL_EFULLPOOL;
            self->xerror = CFGPOOL_XEMANYVALUES;
            break;
        case CFGPOOL__EVNOMEM:
        case CFGPOOL__EINOMEM:
            self->error  = CFGPOOL_EOUTOFMEMORY;
            self->xerror = CFGPOOL_XEADDITEM;
            break;
    }

out:
    if (!wvalue && wkey) free(wkey);
    return self->error;
}


/*

    This function reads the file specified in "filename", parses it searching
for key-value pairs and adds those pairs to the configuration pool specified
in "pool". It returns 0 if no error occurs, or the number of the last line
read from "filename" (returned in "lineno") and an error code otherwise:

    - CFGPOOL_EBADPOOL if "self" is a NULL pointer

    - CFGPOOL_EBADFILE if there was any problem while reading the file. If the
      extended return code is negative then it represents a value for "errno",
      directly from the OS. The extended error codes are these:

          - CFGPOOL_XENULLFILE if "filename" is a NULL pointer

          - The negative value of "errno" if the error came from the OS

    - CFGPOOL_EILLFORMED if an ill-formed line is found in the file. In this
      case the extended error codes are:

          - CFGPOOL_XEVOIDVALUE if a line with no value is found

          - CFGPOOL_XEKEY2BIG if a line with a too big key is found

          - CFGPOOL_XEVALUE2BIG if a line with a too big value is found

          - CFGPOOL_XELINE2BIG if a line too big is found

    - CFGPOOL_EFULLPOOL if the pool already contains too many items. In this
      case, the extended error codes are these:
      
          - CFGPOOL_XEMANYKEYS if there are too many keys in the pool

          - CFGPOOL_XEMANYVALUES if a key already has too many values
    
*/
int                             // Error code
cfgpool_addfile (
CfgPool self,                   // The pool where the data will be added
const char *filename,           // The name of the file to parse
uintmax_t *lineno               // The line number returned on errors
){

    if (!self) return CFGPOOL_EBADPOOL;
    self->error  = 0;
    self->xerror = 0;

    if (!filename) {
        self->error  = CFGPOOL_EBADARG;
        self->xerror = CFGPOOL_XENULLFILE;
        goto out;
    }
    
    int inputfd;
    
    while ((inputfd = open(filename, O_RDONLY)) == -1 && errno == EINTR);

    if (inputfd < 0) {
        self->error  = CFGPOOL_EBADFILE;
        self->xerror = -errno;
        goto out;
    }

    cfgpool_addfd(self, inputfd, lineno);

out:
    if (inputfd >= 0) close(inputfd);
    return self->error;
}


/*

    This function reads the file whose file descriptor is "inputfd", parses it
searching for key-value pairs and adds those pairs to the configuration pool
specified in "pool". It returns 0 if no error occurs, or the number of the
last line read from "inputfd" (which is returned in "lineno" ) and one of
these error codes otherwise:

    - CFGPOOL_EBADPOOL if "self" is a NULL pointer

    - CFGPOOL_EBADFILE if there was any problem while reading the file. The
      extended return code is negative and it represents a value for "errno",
      directly from the OS.

    - CFGPOOL_EILLFORMED if an ill-formed line is found in the file. In this
      case the extended error codes are:

          - CFGPOOL_XEVOIDVALUE if a line with no value is found

          - CFGPOOL_XEKEY2BIG if a line with a too big key is found

          - CFGPOOL_XEVALUE2BIG if a line with a too big value is found

          - CFGPOOL_XELINE2BIG if a line too big is found

    - CFGPOOL_EFULLPOOL if the pool already contains too many items. In this
      case, the extended error codes are these:
      
          - CFGPOOL_XEMANYKEYS if there are too many keys in the pool

          - CFGPOOL_XEMANYVALUES if a key already has too many values
    
*/
int                             // Error code
cfgpool_addfd (
CfgPool self,                   // The pool where the data will be added
int inputfd,                    // The descriptor of the file to add
uintmax_t *lineno               // The line number returned on errors
){

    if (!self) return CFGPOOL_EBADPOOL;
    self->error  = 0;
    self->xerror = 0;

    int result=internal_mungefd(self, inputfd, NULL, lineno);
    if (result < 0) {
        self->error  = CFGPOOL_EBADFILE;
        self->xerror = -errno;
        goto out;
    }

    switch (result) {
        case CFGPOOL__EMISSING:
            self->error  = CFGPOOL_EILLFORMED;
            self->xerror = CFGPOOL_XEVOIDVALUE;
            goto out;
        case CFGPOOL__EK2BIG:
            self->error  = CFGPOOL_EILLFORMED;
            self->xerror = CFGPOOL_XEKEY2BIG;
            goto out;
        case CFGPOOL__EV2BIG:
            self->error  = CFGPOOL_EILLFORMED;
            self->xerror = CFGPOOL_XEVALUE2BIG;
            goto out;
        case CFGPOOL__EL2BIG:
            self->error  = CFGPOOL_EILLFORMED;
            self->xerror = CFGPOOL_XELINE2BIG;
            goto out;
        case CFGPOOL__EF2BIG:
            self->error  = CFGPOOL_EBADFILE;
            self->xerror = -EFBIG;
            goto out;
        case CFGPOOL__E2MANYK:
            self->error  = CFGPOOL_EFULLPOOL;
            self->xerror = CFGPOOL_XEMANYKEYS;
            goto out;
        case CFGPOOL__E2MANYV:
            self->error  = CFGPOOL_EFULLPOOL;
            self->xerror = CFGPOOL_XEMANYVALUES;
            goto out;
        case CFGPOOL__EKNOMEM:
        case CFGPOOL__EVNOMEM:
        case CFGPOOL__EINOMEM:
        case CFGPOOL__ESNOMEM:
        case CFGPOOL__ELNOMEM:
            self->error  = CFGPOOL_EOUTOFMEMORY;
            self->xerror = CFGPOOL_XEADDITEM;
            goto out;
    }

out:
    return self->error;

}

////////////////////////////////////////////////////////////


         /////////////////////////////////////// 
        //                                     //
        //  Low level item addition functions  //
        //                                     //
         ///////////////////////////////////////


/*

    This function does the real job of adding an item to the pool, resizing it
if necessary. The function adds the information in "key", "value" and "src" to
the pool specified in "self". Don't make assumptions about what the function
does with its parameters, and NEVER use this function with a dynamic duration
object, since no copy is made of "key", "value" or "src", only a reference is
stored in the pool. You can add empty values if you want.

    If the number of buckets in the hash table which is the pool is too low
for the number of items, the function tries to grow the table and perform a
rehashing. If no memory is available, this is retried in further calls.

    If all goes OK, the function returns 0. Otherwise, the return value can be
one of these:

    - CFGPOOL__E2MANYK if there are already too many keys in the pool
    - CFGPOOL__E2MANYV if there are already too many values in the "key"
    - CFGPOOL__EVNOMEM if there was no free memory to add the value
    - CFGPOOL__EINOMEM if there was no free memory to add the entire item
    
*/
int                             // Error code
internal_additem (
CfgPool self,                   // The pool where the data will be added
wchar_t *key,                   // The key of the item
wchar_t *value,                 // The value of the item
char *src                       // The source of the item (encoded)
){

    // First of all, see if we have space
    if (self->numitems == CFGPOOL_MAXKEYS) return CFGPOOL__E2MANYK;

    /*

        Fast path (more or less): if the key already exists in the table, we
    don't have to allocate memory for the new item, nor resize the table,
    because we are just adding another value to an existing key.

    */
    
    // Search the key
    size_t hash = internal_one_at_a_time(key);
    hash &= self->buckets-1;
    CfgItem dataiterator=self->data[hash];

    
    while (dataiterator) {
        // See if the key exists or if it is a collision
        if (! wcscmp(dataiterator->key,key)) {
            // OK, it exists! Add the value

            // First, realloc the "values" array
            struct CfgValue *tmpvalue;
            size_t len = dataiterator->numvalues;
            if (len > CFGPOOL_MAXVALUES-1) return CFGPOOL__E2MANYV;

            len++;
            len *= sizeof(struct CfgValue);
            
            tmpvalue = realloc(dataiterator->values, len);
            if (!tmpvalue) return CFGPOOL__EVNOMEM;

            // Now, add the element
            free(key);  // We won't use this!
            dataiterator->values = tmpvalue;
            tmpvalue = &(dataiterator->values[dataiterator->numvalues]);
            tmpvalue->src   = (char *) src;
            tmpvalue->value = (wchar_t *) value;
            dataiterator->numvalues++;

            return 0;
        }
        
        // Bad luck, try the next one
        dataiterator=dataiterator->next;
    };


    // If we are here, we must create a new item...
    struct CfgItem *item = malloc(sizeof(struct CfgItem));
    if (!item) return CFGPOOL__EINOMEM;

    item->key = (wchar_t *) key;
    item->numvalues = 1;
    item->values = malloc(sizeof(struct CfgValue));
    if (!item->values) {  // Oops!
        free(item);
        return CFGPOOL__EVNOMEM;
    }

    // Store the data    
    item->values->src   = (char *) src;
    item->values->value = (wchar_t *) value;

    /*

        If the number of elements is greater than twice the number of buckets,
    do a resize of the hash table if possible. We cannot do a simple realloc,
    because we must ensure that the new spaces are filled with NULL pointers
    and because we have to rehash all elements in the table...

        When resizing, we are bound in size. First, we cannot allocate more
    than SIZE_MAX bytes, so no matter if we want to grow, we can't do further.
    Second, the number of buckets cannot be more than SIZE_MAX/2 or we will
    have an overflow in the size of the hash table (the real number of items
    stored in the table). The last bound is the maximum number of buckets we
    can fit in SIZE_MAX bytes, since it must be a power of two (for speed).
    So, we must check for all these conditions.

    */

    // Do we need to (and can) grow?
    if (self->numitems/2 > self->buckets &&
        self->buckets < CFGPOOL_MAXBUCKETS) {

        // Create the new table
        CfgItem *tmptable = calloc(self->buckets << 1, sizeof(CfgItem));

        if (tmptable) {
            // Perform rehashing and free the old hash table
            
            size_t i=0;
            
            for (i=0; i < self->buckets; i++) {

                CfgItem tmpitem;

                // Double parentheses so GCC doesn't complain
                while ((tmpitem=self->data[i])) {
                    self->data[i]=tmpitem->next;
                    hash=internal_one_at_a_time(tmpitem->key);
                    hash &= (self->buckets << 1)-1;
                    tmpitem->next=tmptable[hash];  // Relink node
                    tmptable[hash]=tmpitem;  // Add node to the new table
                }
            }

            free(self->data);

            self->data = tmptable;
            self->buckets <<= 1;

            // Calculate new hash for the current item
            hash &= internal_one_at_a_time(key);
            hash &= self->buckets-1;

        };
        
    }
    

    // Add the element
    item->next=self->data[hash];
    self->data[hash]=item;
    self->numitems++;

    return 0;
}


/*

    This function parses an entire file (from a file descriptor). It reads the
file in fixed sized chunks until an entire line has been read. After that it
parses the line and adds the parsed item (if any) to the pool given in "self".

    It returns 0 on success, otherwise it returns an error code and the line
number where the error happened in "lineno". The error codes are:

    - CFGPOOL__EMISSING if it found a line without a separator
    - CFGPOOL__EK2BIG if a parsed key length is too large
    - CFGPOOL__EV2BIG if a parsed value length is too large
    - CFGPOOL__EL2BIG if the line length is too large
    - CFGPOOL__EF2BIG if the file is too large
    - CFGPOOL__E2MANYK if there are already too many keys in the pool
    - CFGPOOL__E2MANYV if there are already too many values in the "key"
    - CFGPOOL__ESNOMEM if no memory is available to allocate the source
    - CFGPOOL__EKNOMEM if no memory is available to allocate the key
    - CFGPOOL__EVNOMEM if there was no free memory to add the value
    - CFGPOOL__EINOMEM if there was no free memory to add the entire item
    - CFGPOOL__ELNOMEM if no memory is available to allocate the line

*/
int                             // Error code
internal_mungefd (
CfgPool self,                   // The pool where data will be added
int inputfd,                    // The file descriptor of the file to be added
const char *filename,           // The corresponding filename (can be NULL)
uintmax_t *lineno               // Last line processed (return value)
) {

    /*

        This looks weird, I know, but here is the explanation.

        If we just make the buffer "BUFSIZ" bytes, we must be careful when
    reading a new chunk because there may be a few bytes already left in the
    buffer from the last conversion (e.g. if a character was partially read). 
    This is more complex than the solution I use (which comes from an example
    in the glibc documentation) because each time we will be reading a
    different size.
    
        The solution I use is to make the buffer "BUFSIZ+MB_LEN_MAX" bytes,
    but only read "BUFSIZ" bytes in each read operation! This way, even if a
    partial character is left in the buffer, we are sure we have enough space
    to store another load of BUFSIZ bytes. OK, this may waste up to
    "MB_LEN_MAX" bytes if we always read "BUFSIZ" bytes and no partial
    characters are read, but it is a very small price to pay for a cleaner and
    simpler code.

    */
    char buffer[BUFSIZ+MB_LEN_MAX];
    size_t blen = 0;            // 'buffer' filled length, in bytes
    size_t bpos = 0;            // Current convert position in 'buffer', in bytes

    wchar_t *line = NULL;       // Stored line
    size_t lpos = 0;            // Current store position in 'line', in wchars
    size_t lsize = 0;           // 'line' capacity, in wchars
    
    // Just in case we didn't receive one
    uintmax_t internal_lineno;
    
    int eof = 0;  // So we know that we hit EOF

    // FIXME: document that lineno is optional
    if (!lineno) lineno = &internal_lineno;
    *lineno=0;

    while (!eof) {

        // Get line
        ssize_t count = read(inputfd, buffer+blen, BUFSIZ);
        if (count < BUFSIZ) {
            eof = 1;  // Signal EOF to the main loop
        }
        if (count < 0) return -errno;

        /* There are now more 'count' bytes in the 'buffer' */
        blen += count;

        while (blen) {
            /*

                We have to convert the buffer to wchar_t, as
            many chars as we can. We use 'mbrtowc()' for that.

            */
            size_t len;  // Length of current converted char

            if (lpos == lsize) {
                /* See if we exceeded the limit */
                if (lsize > SIZE_MAX-BUFSIZ) {
                    free(line);
                    return CFGPOOL__EL2BIG;
                }

                /* The line is full, we have to make it bigger! */
                lsize += BUFSIZ;
                line = realloc(line, (lsize+1)*sizeof(wchar_t));
                if (!line) return CFGPOOL__ELNOMEM;
            }

            // Copy another char of the buffer into the line
            len = mbrtowc(line+lpos,buffer+bpos, blen, NULL);

            if (len == (size_t)-2) {
                memmove(buffer+bpos,buffer,blen);
                break;
            }

            if (len == (size_t)-1 || !len) {
                // Invalid character or embedded NUL byte
                len = 1; // Skip it
                line[lpos] = L'?';
            }

            bpos += len;
            blen -= len;

                
            if (line[lpos] == L'\n' || (blen == 0 && eof)) {

                // We get a line!
                if (*lineno == UINTMAX_MAX) {
                    free(line);
                    return CFGPOOL__EF2BIG;
                }
                (*lineno)++;

                // Where the item was defined (item source)
                char *isrc = NULL;

                // To store the parsed line
                wchar_t *key = NULL;
                wchar_t *value = NULL;

                // Null terminate the string
                if (line[lpos] != L'\n') lpos++;
                line[lpos] = L'\0';

                // Parse the line and add the item
                int result=internal_parse(line, &key, &value);
                if (result != 0) {
                    free(line);
                    if (result == CFGPOOL__EIGNORE) continue;
                    return result;
                }

                if (filename)
                    isrc = internal_xnprintf("F%jx:%s", *lineno, filename);
                else
                    isrc = internal_xnprintf("D%jx:%x", *lineno, inputfd);

                if (!isrc) {
                    free(key);
                    free(value);
                    free(line);
                    return CFGPOOL__ESNOMEM;
                }

                result=internal_additem(self, key, value, isrc);
                if (result != 0) {
                    free(key);
                    free(value);
                    free(line);
                    free(isrc);
                    return result;
                }

                free(line);
                line = NULL;

                lsize = 0;
                lpos = 0;
            } else lpos++;
        }
        bpos = 0;

    }

    return 0;
}

////////////////////////////////////////////////////////////


         //////////////////
        //                //
        //  Hash function //
        //                //
         //////////////////

/*

    This hash function is NOT mine! It is the well known "one-at-a-time" hash
function by Bob Jenkins, a true genius of hash tables and hash functions. This
function was designed from requirements from Colin Plumb and you can find it
in the article that Bob wrote for Dr. Dobbs Journal in September 1997. You can
read an updated version in Bob's website, at

    http://burtleburtle.net/bob/hash/doobs.html
     
    There you can find other good hash functions, specially "lookup3", which
seems to be the best hash function out there ;)

    This is the same function used by Perl (at least, in v5.8.8).
    
    I've adapted the function to work with wchar_t strings.

*/

inline
size_t                          // The computed hash value
internal_one_at_a_time (
const wchar_t *key              // The data we want to hash
){
    size_t len=wcslen(key);
    
    if (!len) return 0;
    
    // We don't want overflows!
    len *= len <= SIZE_MAX / sizeof(wchar_t) ? sizeof(wchar_t) : 1;

    unsigned char *data = (unsigned char *) key;

    size_t hash=0;

    while (len--) {
        hash += data[len];
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);

    // Just to make sure...
    return hash;
}

////////////////////////////////////////////////////////////


         /////////////////////
        //                   //
        //  Parser function  //
        //                   //
         /////////////////////

/*

    This function parses a line into a keyword and its corresponding value.
Note that this item is NOT added to the configuration pool, the function just
does the parsing. The returned wide-character strings (the keyword and the
corresponding item) are dynamically allocated COPIES to the material found in
the input line, so they MUST be freed when no longer needed and can be added
directly. In case of error, no memory is allocated and so no strings can be
returned.

    The function assumes that the given line doesn't have NULL characters in
the middle, so it can be used as string terminator.

    It returns 0 in case of success, an error code otherwise:
    
    - CFGPOOL__EMISSING if the "line" doesn't have a separator
    - CFGPOOL__EK2BIG if the key length is too large
    - CFGPOOL__EV2BIG if the value length is too large
    - CFGPOOL__EKNOMEM if no memory is available to allocate the key
    - CFGPOOL__EVNOMEM if no memory is available to allocate the value
    - CFGPOOL__EIGNORE if the line is empty or is a comment
*/
int
internal_parse
(const wchar_t *line, wchar_t **key, wchar_t **value) {

    const wchar_t *current;     // Temporary marker
    const wchar_t *keystart;    // Key starting point
    size_t keylen = 0;          // Key length (in wide characters)
    const wchar_t *valuestart;  // Value starting point
    size_t valuelen = 0;        // Value length (in wide characters)

    current=line;
    keystart=line;
    valuestart=line;
    
    // Start parsing!

    // Skip leading whitespace
    while (*current && iswblank(*current)) current++;

    // Ignore comment and empty lines
    if (!*current || *current == L'#') return CFGPOOL__EIGNORE;

    // OK, we have the "key" starting point, now compute the length
    keystart = current;

    // FIXMEs: by now the separator is hardcoded and it's a space
    // Start searching the separator
    while (*current && *current != L' ') current++;
    if (! *current) return CFGPOOL__EMISSING;

    // Cool, we have the separator! Compute the key length
    if ((size_t)(current-keystart) > SIZE_MAX)
        return CFGPOOL__EK2BIG;  // Ooops, too big!
    keylen = current-keystart;  // The length DOESN'T include the final NUL
    
    // Go on and look for the value
    current++;

    /*

        Now find the starting point of the value,
    ignoring trailing whitespace in the separator

    */
    while (*current && iswblank(*current)) current++;
    valuestart = current;

    // Get the end of the value to compute the length
    while (*current) current++;
    
    // Ignore trailing whitespace in the value
    while (*current && iswblank(*current)) current--;

    // Compute value length
    if ((size_t) (current-valuestart) > SIZE_MAX)
        return CFGPOOL__EV2BIG;  // Ooops, too big!
    valuelen = current-valuestart;
    if (valuelen == 0) return CFGPOOL__EMISSING;

    // Store the values and return them
    *key = malloc(sizeof(wchar_t)*(keylen+1));
    if (! *key) return CFGPOOL__EKNOMEM;
    *value = malloc(sizeof(wchar_t)*(valuelen+1));
    if (! *value) {
        free(*key);
        return CFGPOOL__EVNOMEM;
    }

    wcsncpy(*key,   keystart,   keylen);
    wcsncpy(*value, valuestart, valuelen);
    (*key)[keylen]     = L'\0';
    (*value)[valuelen] = L'\0';

    return 0;
}

////////////////////////////////////////////////////////////


         /////////////////////////////
        //                           //
        //  Miscellaneous functions  //
        //                           //
         /////////////////////////////


/*

    This function prints its arguments according to the format in a
dinamically allocated string and returns it. It returns NULL in case of
errors, the allocated string otherwise.

*/
char *
internal_xnprintf
(const char *format, ...) {

    va_list args;

    char *buffer = NULL;
    size_t len = 0;

    va_start(args, format);

    // Let's see how many chars will we need...
    len = vsnprintf(NULL, 0, format, args);
    // Make room for the final NUL byte
    len++;

    // Do the Bartman!
    buffer = malloc(len);
    if (!buffer) return NULL;

    // Print the data    
    vsnprintf(buffer, len, format, args);

    va_end(args);

    return buffer;
}


/*

    This function takes a multibyte (or monobyte) string at "src" (char *) and
converts it to "wchar_t *", allocating the memory for the result. It returns 0
if no error occurred while converting, and an error code otherwise:

    - CFGPOOL__ESNOMEM if the function cannot allocate memory for the copy
    - CFGPOOL__ENULLDST if "dst" is a NULL pointer
    - CFGPOOL__ENULLSRC if "src" is a NULL pointer
    - CFGPOOL__EBADMB if "src" contains a bad multibyte character sequence
    - CFGPOOL__ES2BIG if "src" length is >= SIZE_MAX
    - CFGPOOL__EVOID if "src" is empty

*/
int
internal_mbstowcs (
wchar_t **dst,
const char *src
) {

    if (!dst) return CFGPOOL__ENULLDST;
    if (!src) return CFGPOOL__ENULLSRC;

    const char *tmp=src;
    size_t len=0;
    int i=0;
    
    while (1) {
        i=mblen(tmp, MB_CUR_MAX);
        if (i == -1) return CFGPOOL__EBADMB;
        if (i == 0) break;  // End of string found!
        len++;

        /*

            If the length is SIZE_MAX, we cannot tell if a NUL byte was
        actually present or not, so we don't try the conversion (which would,
        probably, take ages to do anyway...). Please note that this check
        DOESN'T prevent accessing memory beyond the given string if it is not
        NUL terminated!.  It's the caller duty to make sure that such memory
        violation doesn't occur, because the funcion cannot check that!

        */
        if (len == SIZE_MAX) return CFGPOOL__ES2BIG;

        tmp+=i;
    }

    if (len == 0) return CFGPOOL__EVOID;

    len++;  // Make room for the final NUL

    // Allocate memory for destination string
    *dst=malloc(len * sizeof(wchar_t));
    if (!dst) return CFGPOOL__ESNOMEM;

    // Do the copy
    mbstate_t state;
    memset(&state, '\0', sizeof(state));

    /*
    
        This function call CANNOT fail because nor "state" has an invalid
    state nor the contents of "src" are invalid (we have already tested above,
    when computing its length) so we are going to ignore the return value.
    Anyway, we couldn't handle an error we don't expect, so...
    
    */
    mbsrtowcs(*dst, &src, len, &state);
    return 0;

}


/*

    This function takes a wide-character string at "src" (char *) and
converts it to "char *", allocating the memory for the result. It returns 0
if no error occurred while converting, and an error code otherwise:

    - CFGPOOL__ESNOMEM if the function cannot allocate memory for the copy
    - CFGPOOL__ENULLDST if "dst" is a NULL pointer
    - CFGPOOL__ENULLSRC if "src" is a NULL pointer
    - CFGPOOL__EBADWC if "src" contains a bad wide-character sequence
    - CFGPOOL__ES2BIG if "src" length is >= SIZE_MAX
    - CFGPOOL__EVOID if "src" is empty

*/
int
internal_wcstombs (
char **dst,
const wchar_t *src
) {

    size_t len=wcslen(src);
    
    if (len == 0) return CFGPOOL__EVOID;
    
    /*

        If the length is SIZE_MAX, we cannot tell if a NUL byte was actually
    present or not, so we don't try the conversion (which would, probably,
    take ages to do anyway...). Please note that this check DOESN'T prevent
    accessing memory beyond the given string if it is not NUL terminated!.
    It's the caller duty to make sure that such memory violation doesn't
    occur, because the funcion cannot check that!

    */

    if (len == SIZE_MAX) return CFGPOOL__ES2BIG;

    len++; // Make room for the final NUL

    /*

        Allocate memory for destination string. Please note that,
    unfortunately, we are going to waste some memory when doing that, because
    we don't know exactly how many bytes will the wide-character string when
    converted to multibyte unless we convert it twice (one to get the number
    of bytes necessary, another one to do the actual conversion). So, I do a
    space for speed tradeoff (for now).

    */
    *dst=malloc(len*MB_CUR_MAX);
    if (!dst) return CFGPOOL__ESNOMEM;

    // Do the copy
    mbstate_t state;
    memset(&state, '\0', sizeof(state));

    int i=errno;

    errno=0;
    size_t result=wcsrtombs(*dst, &src, len, &state);
    if (result == (size_t) -1 && errno == EILSEQ) {
        free(dst);
        errno=i;
        return CFGPOOL__EBADWC;
    }

    errno=i;
    return 0;
}

////////////////////////////////////////////////////////////


         //////////////////////////
        //                        //
        //  Error code accessors  //
        //                        //
         //////////////////////////

/*

    This function returns the exit code (the CFGPOOL_E* constants) of the last
method call (which may be 0), or CFGPOOL_EBADPOOL if "self" is a NULL pointer.
    
*/
int
cfgpool_geterror (
CfgPool self
){
    if (!self) return CFGPOOL_EBADPOOL;
    return self->error;
}


/*

    This function returns the extended exit code (the CFGPOOL_XE* constants)
of the last method call (which may be 0), or CFGPOOL_XEBADPOOL if "self" is a
NULL pointer.
    
*/
int
cfgpool_getxerror (
CfgPool self
){

    if (!self) return CFGPOOL_XEBADPOOL;
    return self->xerror;
}

////////////////////////////////////////////////////////////


         ////////////////////
        //                  //
        //  Data retrieval  //
        //                  //
         ////////////////////


/*

    This function retrieves a the last value given to "key", returning a
dinamycally allocated copy in "data". The function returns 0 in case of
success (that is, "data" will contain the requested value) and an error code
otherwise. The error codes that the function can return are:

    - CFGPOOL_EBADPOOL if "self" is a NULL pointer

    - CFGPOOL_EBADARG if "key" or "data" are invalid. The extended error
      codes in this case can be:

          - CFGPOOL_XENULLKEY if "key" is a NULL pointer

          - CFGPOOL_XENULL if "data" is a NULL pointer

          - CFGPOOL_XEBADKEY if "key" contains an invalid multibyte character

          - CFGPOOL_XEKEY2BIG if "key" string is too big

          - CFGPOOL_XEVOIDKEY if "key" is empty

    - CFGPOOL_EOUTOFMEMORY if the function runs out of memory. The extended
      error codes in this case can be:

          - CFGPOOL_XEKEYCOPY if the function did run out of memory when
            trying to make a copy of "key"

          - CFGPOOL_XEVALUECOPY if the function did run out of memory when
            trying to make a copy of "value"

    - CFGPOOL_ENOTFOUND if "key" couldn't be found in pool

*/
int
cfgpool_getvalue (
CfgPool self,
const char *key,
char **data
){
    if (!self) return CFGPOOL_EBADPOOL;

    wchar_t *wkey = NULL;

    if (!data) {
        self->error  = CFGPOOL_EBADARG;
        self->xerror = CFGPOOL_XENULL;
        goto out;
    }

    self->error  = 0;
    self->xerror = 0;

    if (self->numitems == 0) {
        self->error  = CFGPOOL_ENOTFOUND;
        goto out;
    }
    *data = NULL;

    switch (internal_mbstowcs(&wkey, key)) {
        case CFGPOOL__ESNOMEM:
            self->error  = CFGPOOL_EOUTOFMEMORY;
            self->xerror = CFGPOOL_XEKEYCOPY;
            goto out;
        case CFGPOOL__EBADMB:
            self->error  = CFGPOOL_EBADARG;
            self->xerror = CFGPOOL_XEBADKEY;
            goto out;
        case CFGPOOL__ES2BIG:
            self->error  = CFGPOOL_EBADARG;
            self->xerror = CFGPOOL_XEKEY2BIG;
            goto out;
        case CFGPOOL__EVOID:
            self->error  = CFGPOOL_EBADARG;
            self->xerror = CFGPOOL_XEVOIDKEY;
            goto out;
        case CFGPOOL__ENULLSRC:
            self->error  = CFGPOOL_EBADARG;
            self->xerror = CFGPOOL_XENULLKEY;
            goto out;
    }

    CfgItem tmpitem = internal_getdata(self, wkey);
    if (!tmpitem) {
        self->error = CFGPOOL_ENOTFOUND;
        goto out;
    }

    wchar_t *wvalue = tmpitem->values[0].value;
    if (internal_wcstombs(data, wvalue) == CFGPOOL__ESNOMEM) {
        self->error  = CFGPOOL_EOUTOFMEMORY;
        self->xerror = CFGPOOL_XEVALUECOPY;
        goto out;
    }

out:
    if (wkey) free(wkey);
    return self->error;
}


/*

    This function retrieves a the last value given to "wkey", returning a
dinamycally allocated copy in "wdata". The function returns 0 in case of
success (that is, "wdata" will contain the requested value) and an error code
otherwise. The error codes that the function can return are:

    - CFGPOOL_EBADPOOL if "self" is a NULL pointer

    - CFGPOOL_EBADARG if "wkey" or "wdata" are invalid. The extended error
      codes in this case can be:

          - CFGPOOL_XENULLKEY if "wkey" is a NULL pointer

          - CFGPOOL_XENULL if "wdata" is a NULL pointer

          - CFGPOOL_XEVOIDKEY if "key" is empty

          - CFGPOOL_XEKEY2BIG if "key" string is too big


    - CFGPOOL_EOUTOFMEMORY if the function runs out of memory. The extended
      error code in this case can be:

          - CFGPOOL_XEVALUECOPY if the function did run out of memory when
            trying to make a copy of "value"


    - CFGPOOL_ENOTFOUND if "key" couldn't be found in pool

*/

int
cfgpool_getwvalue (
CfgPool self,
const wchar_t *wkey,
wchar_t **wdata
){

    if (!self) return CFGPOOL_EBADPOOL;

    if (!wdata) {
        self->error  = CFGPOOL_EBADARG;
        self->xerror = CFGPOOL_XENULL;
        goto out;
    }
    
    if (!wkey) {
        self->error  = CFGPOOL_EBADARG;
        self->xerror = CFGPOOL_XENULLKEY;
        goto out;
    }

    if (wcslen(wkey) == 0) {
        self->error  = CFGPOOL_EBADARG;
        self->xerror = CFGPOOL_XEVOIDKEY;
        goto out;
    }

    if (wcslen(wkey) == SIZE_MAX) {
        self->error  = CFGPOOL_EBADARG;
        self->xerror = CFGPOOL_XEKEY2BIG;
        goto out;
    }


    self->error  = 0;
    self->xerror = 0;

    if (self->numitems == 0) {
        self->error  = CFGPOOL_ENOTFOUND;
        goto out;
    }

    *wdata=NULL;

    CfgItem tmpitem = internal_getdata(self, wkey);
    
    if (!tmpitem) {
        self->error = CFGPOOL_ENOTFOUND;
        goto out;
    }
    
    wchar_t *wvalue=tmpitem->values[0].value;
    size_t len = wcslen(wvalue);
    len++;

    *wdata=malloc(len * sizeof(wchar_t));
    if (!*wdata) {
        self->error = CFGPOOL_EOUTOFMEMORY;
        self->error = CFGPOOL_XEVALUECOPY;
        goto out;
    }
    wcscpy(*wdata, wvalue);

out:
    return self->error;
}


/*

    This function returns the CfgItem corresponding to "wkey" if "wkey" is in
the pool, otherwise it returns NULL.

*/
CfgItem
internal_getdata (
CfgPool self,
const wchar_t *wkey
){

    size_t hash = internal_one_at_a_time(wkey);
    hash &= self->buckets-1;

    CfgItem tmpitem=self->data[hash];
    
    if (tmpitem) {
        while (tmpitem) {
            if (!wcscmp(tmpitem->key, wkey)) {
                return tmpitem;
            }
            tmpitem = tmpitem->next;
        }
    }

    return NULL;

}





////////////////////////////////////////////////////////////


char * cfgpool_dontuse(CfgPool self, char *key) {

    wchar_t *wkey;
    const wchar_t *wvalue;
    char *value;
    size_t keylen=strlen(key)+1;

    mbstate_t state;

    memset(&state, '\0', sizeof(mbstate_t));

    wkey=malloc(keylen*sizeof(wchar_t));
    if (!wkey) return NULL;

    // FIXME: check for overflows (keylen must be < (size_t) -1)
    if (mbsrtowcs(wkey, (const char **)&key, keylen, &state) != keylen-1) {
        free(wkey);
        return NULL;
    }

    size_t hash=internal_one_at_a_time(wkey);
    hash &= self->buckets-1;
    free(wkey);

    if (self->data[hash]) {
        CfgItem tmpitem=self->data[hash];
        size_t tos=self->data[hash]->numvalues-1;
        wvalue = (const wchar_t *) tmpitem->values[tos].value;
    } else return NULL;

    size_t valuelen=wcslen(wvalue)+1;
    value=malloc(valuelen);
    if (!value) return NULL;
    memset(&state, '\0', sizeof(mbstate_t));  //FIXME: state should be in initial state :?
    if (wcsrtombs(value, &wvalue, valuelen, &state) != valuelen-1) {
        free(value);
        return NULL;
    }
    return value;
}

