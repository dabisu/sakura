// $Rev: 21 $
#include <stddef.h>
#include <fcntl.h>
#include <stdio.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <wchar.h>
#include <wctype.h>
#include "cfgpool.h"
#include "mobs.h"

// FIXME: error handling: use the extended error handling!!! specially for EINVALIDARG

// Prototypes for internal functions 
static        int    internal_additem  (CfgPool, const wchar_t *, const wchar_t *, const char *);
static        int    internal_mungefd  (CfgPool, int, const char *);
static        int    internal_parse    (const wchar_t *, wchar_t **, wchar_t **);
static        char  *internal_xnprintf (const char *, ...);
static        int    internal_mbstowcs (wchar_t **, const char *);
static        int    internal_wcstombs (char **, const wchar_t *);
static inline size_t internal_one_at_a_time (const wchar_t *);

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
    size_t buckets;             // The number of buckets in the hash table
    size_t numitems;            // The number of elements in the hash table
    size_t bits;                // The hash table "bit-size" (see above)
    struct CfgItem {
        CfgItem next;           // To implement the linked list
        size_t numvalues;       // Number of values this item has
        wchar_t *key;           // This item's key
        struct CfgValue {
            unsigned char *src; // Where each value was defined
            wchar_t *value;     // The value itself
        } *values;              // This item's set of values
    } **data;                   // The data (the array of buckets)
};

#define CFGPOOL_MAXBUCKETS  ((SIZE_MAX>>1)/sizeof(CfgItem))

// FIXME: should these be exported trhu accessor methods?
#define CFGPOOL_MAXVALUES   (SIZE_MAX/sizeof(struct CfgValue))
#define CFGPOOL_MAXKEYS     (SIZE_MAX)

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
    if (!self) return self;

    self->buckets = 0;
    self->numitems = 0;
    self->data = NULL;

    self->data = calloc(1 << CFGPOOL_DEFAULT_BITSIZE, sizeof(CfgItem));
    if (self->data) self->buckets = 1 << CFGPOOL_DEFAULT_BITSIZE;
    else {  // No memory, free the object and return NULL
        free(self);
        self=NULL;
    };

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
        while ((tmpitem=self->data[self->buckets])) {

            // OK, the bucket is not empty

            // Go on...
            self->data[self->buckets]=tmpitem->next;

            // Free the element
            while (tmpitem->numvalues--) {
                struct CfgValue tmpvalue=tmpitem->values[tmpitem->numvalues];
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


int                             // Error code
cfgpool_addwitem (
CfgPool self,                   // The pool where the item will be added
const wchar_t *key,             // The key of the item
const wchar_t *value            // The value of the item
){

    if (!self)  return CFGPOOL_EINVALIDARG;
    if (!key)   return CFGPOOL_EINVALIDARG;
    if (!value) return CFGPOOL_EINVALIDARG;

    wchar_t *_key;
    wchar_t *_value;

    size_t len;
    len = wcslen(key);
    if (len == SIZE_MAX) return CFGPOOL_EINVALIDARG;

    len++;    
    _key = malloc(len * sizeof(wchar_t));
    if (!_key) return CFGPOOL_EOUTOFMEMORY;

    len = wcslen(value);
    if (len == SIZE_MAX) return CFGPOOL_EINVALIDARG;

    len++;
    _value = malloc(len * sizeof(wchar_t));
    if (!_value) {
        free(_key);
        return CFGPOOL_EOUTOFMEMORY;
    }

    wcscpy(_key, key);
    wcscpy(_value, value);
    int result=internal_additem(self, _key, _value, NULL);
    
    free(_key);
    free(_value);
    
    return result;
}


int                             // Error code
cfgpool_additem (
CfgPool self,                   // The pool where the item will be added
const char *key,                // The key of the item
const char *value               // The value of the item
){

    if (!self)  return CFGPOOL_EINVALIDARG;
    if (!key)   return CFGPOOL_EINVALIDARG;
    if (!value) return CFGPOOL_EINVALIDARG;

    wchar_t *wkey;
    wchar_t *wvalue;
    int result=0;

    result=internal_mbstowcs(&wkey, key);
    if (result != 0) return result;
    result=internal_mbstowcs(&wvalue, value);
    if (result != 0) return result;

    result = cfgpool_addwitem(self, wkey, wvalue);
    
    free(wkey);
    free(wvalue);
    
    return result;
}


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



int                             // Error code
cfgpool_addfile (
CfgPool self,                   // The pool where the data will be added
const char *filename            // The name of the file to parse
){

    if (!self)     return CFGPOOL_EINVALIDARG;
    if (!filename) return CFGPOOL_EINVALIDARG;

    int inputfd = open(filename, O_RDONLY);

    // FIXME: store the real reason in the extended error handling structure!
    if (inputfd < 0) return CFGPOOL_ECANTREADFILE;

    int result = internal_mungefd(self, inputfd, filename);

    close(inputfd);
    
    return result;
}


int                             // Error code
cfgpool_addfd (
CfgPool self,                   // The pool where the data will be added
int inputfd                     // The descriptor of the file to add
){

    if (!self) return CFGPOOL_EINVALIDARG;

    return internal_mungefd(self, inputfd, NULL);
}

////////////////////////////////////////////////////////////


 /////////////////////////////////////////////////
//                                               //
//  Low level item addition function (internal)  //
//                                               //
 /////////////////////////////////////////////////

static
int                             // Error code
internal_additem (
CfgPool self,                   // The pool where the data will be added
const wchar_t *key,             // The key of the item
const wchar_t *value,           // The value of the item
const char *src                 // The source of the item (encoded)
){

    if (!self)          return CFGPOOL_EINVALIDARG;
    if (!self->data)    return CFGPOOL_EINVALIDARG;
    if (!key)           return CFGPOOL_EINVALIDARG;
    if (!wcslen(key))   return CFGPOOL_EINVALIDARG;
    if (!value)         return CFGPOOL_EINVALIDARG;
    if (!wcslen(value)) return CFGPOOL_EINVALIDARG;

    // First of all, see if we have space
    if (self->numitems == CFGPOOL_MAXKEYS) return CFGPOOL_TOOMANYKEYS;

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
            if (len > CFGPOOL_MAXVALUES-1) return CFGPOOL_TOOMANYVALUES;
            len++;
            len *= sizeof(struct CfgValue);
            
            tmpvalue = realloc(dataiterator->values, len);
            if (!tmpvalue) return CFGPOOL_EOUTOFMEMORY;

            // Now, add the element
            free((wchar_t *) key);  // We won't use this!
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
    if (!item) return CFGPOOL_EOUTOFMEMORY;
    item->key = (wchar_t *) key;
    item->numvalues = 1;
    item->values = malloc(sizeof(struct CfgValue));
    if (!item->values) {  // Oops!
        free(item);
        return CFGPOOL_EOUTOFMEMORY;
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
        };
        
    }

    // Add the element
    item->next=self->data[hash];
    self->data[hash]=item;
    
    return 0;
}

////////////////////////////////////////////////////////////


 /////////////////////////////////////////////////
//                                               //
//  Low level file addition function (internal)  //
//                                               //
 /////////////////////////////////////////////////

/*

    This function parses an entire file (from a file descriptor). It reads the
file in fixed sized chunks until an entire line has been read. After that it
parses the line and adds the parsed item (if any) to the pool given in "self".

*/
int                             // Error code
internal_mungefd (
CfgPool self,                   // The pool where data will be added
int inputfd,                    // The file descriptor of the file to be added
const char *filename            // The corresponding filename (can be NULL)
) {

    if (!self) return       CFGPOOL_EINVALIDARG;
    if (inputfd < 0) return CFGPOOL_EINVALIDARG;
    // FIXME: we should check if "inputfd" is a valid file descriptor :?

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
    unsigned char buffer[BUFSIZ+MB_LEN_MAX];    // Read buffer
    size_t blen = 0;            // 'buffer' filled length, in bytes
    size_t bpos = 0;            // Current convert position in 'buffer', in bytes

    wchar_t *line = NULL;       // Stored line
    size_t lpos = 0;            // Current store position in 'line', in wchars
    size_t lsize = 0;           // 'line' capacity, in wchars
    
    size_t lineno = 0;          // Current line number (for error reporting)

    
    int eof = 0;  // So we know that we hit EOF

    while (!eof) {

        // Get line
        ssize_t count = read(inputfd, buffer+blen, BUFSIZ);
        if (count < BUFSIZ) {
            eof = 1;  // Signal EOF to the main loop
        }
        if (count < 0) return CFGPOOL_ECANTREADFILE;  //FIXME: return the cause in the extended information for the error, using the error handling system

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
                    return CFGPOOL_LINE2BIG;
                }

                /* The line is full, we have to make it bigger! */
                lsize += BUFSIZ;
                line = realloc(line, (lsize+1)*sizeof(wchar_t));
                if (!line) return CFGPOOL_EOUTOFMEMORY;
            }

            // Copy another chunk of the buffer into the line
            len = mbrtowc(line+lpos,buffer+bpos, blen, NULL);

            if (len == (size_t)-2) {
                memmove(buffer+bpos,buffer,blen);
                break;
            }

            if (len == (size_t)-1 || !len) {
                // Invalid character or embedded NUL byte FIXME
                len = 1; // Skip it
                line[lpos] = L'?';  //FIXME: we shouldn't store a fake char
            }

            bpos += len;
            blen -= len;

                
            if (line[lpos] == L'\n' || (blen == 0 && eof)) {

                // We get a line!
                if (lineno == SIZE_MAX) {
                    free(line);
                    return CFGPOOL_FILE2BIG;
                }
                lineno++;

                // Where the item was defined (item source)
                char *isrc = NULL;

                // To store the parsed line
                wchar_t *key = NULL;
                wchar_t *value = NULL;

                // Null terminate the string
                if (line[lpos] != L'\n') lpos++;
                line[lpos] = L'\0';

                // Parse the line and add the item
                int result = internal_parse(line, &key, &value);

                switch (result) {
                    case 0: break;
                    case CFGPOOL_EMPTYLINE:
                    case CFGPOOL_COMMENTLINE:
                        result = 0;
                    default:
                        free(line);
                        return result;
                }

                if (filename)
                    isrc = internal_xnprintf("F%x:%s", lineno, filename);
                else
                    isrc = internal_xnprintf("D%x:%x", lineno, inputfd);

                if (!isrc) {
                    free(value);
                    free(key);
                    free(line);
                    return CFGPOOL_EOUTOFMEMORY;
                }

                internal_additem(self, key, value, isrc);

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











////
////
////  Internal (static) functions  ////
////
////

////////////////////////////////////////////////////////////




////////////////////////////////////////
/*
    Internal routines
*/




/*

    This function parses a line into a keyword and its corresponding value.
Note that this item is NOT added to the configuration pool, the function just
does the parsing. The returned wide-character strings (the keyword and the
corresponding item) are dynamically allocated COPIES to the material found in
the input line, so they MUST be freed when no longer needed and can be added
directly

    The function assumes that the given line doesn't have NULL characters in
the middle, so it can be used as string terminator.

*/
int
internal_parse
(const wchar_t *line, wchar_t **key, wchar_t **value) {

    if (!line) return CFGPOOL_EINVALIDARG;
    if (!key) return CFGPOOL_EINVALIDARG;
    if (!value) return CFGPOOL_EINVALIDARG;

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
    if (!*current) return CFGPOOL_EMPTYLINE;
    if (*current == L'#') return CFGPOOL_COMMENTLINE;

    // OK, we have the "key" starting point, now compute the length
    keystart = current;

    // FIXMEs: by now the separator is hardcoded and it's a space
    // Start searching the separator
    while (*current && *current != L' ') current++;
    if (! *current) return CFGPOOL_MISSINGSEP;

    // Cool, we have the separator! Compute the key length
    if ((size_t)(current-keystart) > SIZE_MAX)
        return CFGPOOL_KEY2BIG;  // Ooops, too big!
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
        return CFGPOOL_VALUE2BIG;  // Ooops, too big!
    valuelen = current-valuestart;
    if (valuelen == 0) return CFGPOOL_MISSINGVALUE;

    // Store the values and return them
    *key = malloc(sizeof(wchar_t)*(keylen+1));
    if (! *key) return CFGPOOL_EOUTOFMEMORY;
    *value = malloc(sizeof(wchar_t)*(valuelen+1));
    if (! *value) {
        free(*key);
        return CFGPOOL_EOUTOFMEMORY;
    }

    wcsncpy(*key,   keystart,   keylen);
    wcsncpy(*value, valuestart, valuelen);
    (*key)[keylen]     = L'\0';
    (*value)[valuelen] = L'\0';

    return 0;
}


/*

    This function prints its arguments according to the format in a
dinamically allocated string and returns it.

*/
char *
internal_xnprintf
(const char *format, ...) {

    if (!format) return NULL;  // FIXME???

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


int
internal_mbstowcs (
wchar_t **dst,
const char *src
) {

    if (!dst) return CFGPOOL_EINVALIDARG;
    if (!src) return CFGPOOL_EINVALIDARG;

    const char *tmp=src;
    size_t len=0;
    int i=0;
    
    while (1) {
        i=mblen(tmp, MB_CUR_MAX);
        if (i == -1) return CFGPOOL_EINVALIDARG; // FIXME: fill the extended error handling structure so we know why the source was invalid
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
        if (len == SIZE_MAX) return CFGPOOL_EINVALIDARG;

        tmp+=i;
    }

    if (len == 0) return CFGPOOL_EINVALIDARG; // FIXME: EH

    len++;  // Make room for the final NUL

    // Allocate memory for destination string
    *dst=malloc(len * sizeof(wchar_t));
    if (!dst) return CFGPOOL_EOUTOFMEMORY;

    // Do the copy
    mbstate_t state;
    memset(&state, '\0', sizeof(state));

    i=errno;
    
    errno=0;
    size_t result=mbsrtowcs(*dst, &src, len, &state);
    if (result == (size_t) -1 && errno == EILSEQ) {
        free(dst);
        errno=i;
        return CFGPOOL_EINVALIDARG; // FIXME: EH
    }

    errno=i;
    return 0;

}


int
internal_wcstombs (
char **dst,
const wchar_t *src
) {

    if (!dst) return CFGPOOL_EINVALIDARG;
    if (!src) return CFGPOOL_EINVALIDARG;

    size_t len=wcslen(src);
    
    if (len == 0) return CFGPOOL_EINVALIDARG;
    
    /*

        If the length is SIZE_MAX, we cannot tell if a NUL byte was actually
    present or not, so we don't try the conversion (which would, probably,
    take ages to do anyway...). Please note that this check DOESN'T prevent
    accessing memory beyond the given string if it is not NUL terminated!.
    It's the caller duty to make sure that such memory violation doesn't
    occur, because the funcion cannot check that!

    */

    if (len == SIZE_MAX) return CFGPOOL_EINVALIDARG;

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
    if (!dst) return CFGPOOL_EOUTOFMEMORY;

    // Do the copy
    mbstate_t state;
    memset(&state, '\0', sizeof(state));

    int i=errno;

    errno=0;
    size_t result=wcsrtombs(*dst, &src, len, &state);
    if (result == (size_t) -1 && errno == EILSEQ) {
        free(dst);
        errno=i;
        return CFGPOOL_EINVALIDARG; // FIXME: EH
    }

    errno=i;
    return 0;
}
