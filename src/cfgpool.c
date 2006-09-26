// $Rev: 35 $
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

    Each CfgItem is capable of storing multiple values for a keyword. It does
that using an array. I've chosen arrays because a linked list will be overkill
for this task: in the common case, only a bunch of values will be stored in a
keyword, and then we can pay the price for an array resize and some memory
copying, it won't be that slow. Moreover, if the number of values grow a lot,
the pool won't perform very good anyway, so...
    
*/
typedef struct CfgItem *CfgItem;
struct CfgPool {
    size_t buckets;         // The number of buckets in the hash table
    uintmax_t keys;         // The number of keywords in the hash table
    struct CfgItem {
        CfgItem next;       // To implement the linked list
        size_t vals;        // Number of values this item has
        wchar_t *key;       // This item's keyword
        struct CfgValue {
            char *src;      // Where each value was defined
            wchar_t *val;   // The value itself
        } *varray;          // This item's set of values
    } **htbl;               // The hash table (the array of buckets)
};

// Maximum number of buckets we can have in the hash table
#define CFGPOOL_MAXBUCKETS  ((SIZE_MAX>>1)/sizeof(CfgItem))

// Maximum number of values that a keyword can have
#define CFGPOOL_MAXVALUES   (SIZE_MAX/sizeof(struct CfgValue))

// Maximum length of a wchar_t string we can handle
#define CFGPOOL_MAXWLEN     ((SIZE_MAX/sizeof(wchar_t))-1)

// Prototypes for internal functions 
static        int     internal_additem       (CfgPool, wchar_t *, wchar_t *, char *);
static        int     internal_parse         (const wchar_t *, wchar_t **, wchar_t **);
static        char   *internal_xnprintf      (const char *, ...);
static inline size_t  internal_one_at_a_time (const wchar_t *);


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

    if (self) {
        self->buckets = 0;
        self->keys = 0;

        self->htbl = calloc(1 << CFGPOOL_DEFAULT_BITSIZE, sizeof(CfgItem));
        if (self->htbl) self->buckets = 1 << CFGPOOL_DEFAULT_BITSIZE;
        else {  // No memory, free the object and return NULL
            free(self);
            self=NULL;
        };
    }

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
        while ((tmpitem = self->htbl[self->buckets])) {

            // OK, the bucket is not empty

            // Go on...
            self->htbl[self->buckets] = tmpitem->next;

            // Free the element
            while (tmpitem->vals--) {
                struct CfgValue tmpvalue = tmpitem->varray[tmpitem->vals];
                if (tmpvalue.src) free(tmpvalue.src);
                free(tmpvalue.val);
            }
            free (tmpitem->key);
            free (tmpitem->varray);
            free (tmpitem);
        }
        
    }
    if (self->htbl) free(self->htbl);
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
the "wkey" argument) and its corresponding value (in the "wval" argument) to
the pool specified in the "self" argument. This is the "wchar_t *" interface.

    The given strings must be NUL-terminated, wide-character strings, and they
are not added to the pool: a copy is made and then added to the pool, so you
can safely use dynamic duration strings. The original contents of "wkey" and
"wval" are preserved. Empty keywords or values are silently ignored.

    If no error occurs the function returns 0, and an error code otherwise. If
the caller wants additional information about the error, he must call the
"cfgpool_error()" method (except for the CFGPOOL_EBADPOOL error, of course).
Error codes that this function may return are:

    - CFGPOOL_EBADPOOL if "self" is a NULL pointer
    - CFGPOOL_EBADARG if "wkey" or "wval" are NULL pointers
    - CFGPOOL_EKEY2BIG if "wkey" is too long
    - CFGPOOL_EVAL2BIG if "wval" is too long
    - CFGPOOL_ENOMEMORY if the function runs out of memory
    - CFGPOOL_EFULL if there are already too many keywords in the pool

*/
int                             // Error code
cfgpool_addwitem (
CfgPool self,                   // The pool where the item will be added
const wchar_t *wkey,            // The keyword of the item
const wchar_t *wval             // The value of the item
){

    if (!self) return CFGPOOL_EBADPOOL;

    if (!wkey) return CFGPOOL_EBADARG;
    if (!wval) return CFGPOOL_EBADARG;

    size_t len;

    // Copy the keyword
    len = wcslen(wkey);
    if (len == 0) return 0;

    if (len >= CFGPOOL_MAXWLEN) return CFGPOOL_EKEY2BIG;

    len++;  // Make room for the NUL byte at the end
    wchar_t *key = malloc(len * sizeof(wchar_t));
    if (!key) return CFGPOOL_ENOMEMORY;

    wcsncpy(key, wkey, len-1);
    key[len]=L'\0';


    // Copy the value
    len = wcslen(wval);
    if (len == 0) return 0;

    if (len >= CFGPOOL_MAXWLEN) return CFGPOOL_EVAL2BIG;

    len++;  // Make room for the NUL byte at the end
    wchar_t *val = malloc(len * sizeof(wchar_t));
    if (!val) {
        free(key);
        return CFGPOOL_ENOMEMORY;
    }

    wcsncpy(val, wval, len-1);
    val[len]=L'\0';

    // Add the item
    return internal_additem(self, key, val, NULL);

}


/*

    This function adds a new configuration item, that is, a keyword (throught
the "mbkey" argument) and its corresponding value (in the "mbval" argument) to
the pool specified in the "self" argument. This is the multibyte interface.

    The given strings must be NUL-terminated, multibyte or monobyte strings,
and they are not added to the pool: a copy is made and then added to the pool,
so you can safely use dynamic duration strings. The original contents of both
"mbkey" and "mbval" are preserved.

    If no error occurs the function returns 0, and an error code otherwise. If
the caller wants additional information about the error, he must call the
"cfgpool_error()" method (except for the CFGPOOL_EBADPOOL error, of course).
Error codes that this function may return are:

    - CFGPOOL_EBADPOOL if "self" is a NULL pointer
    - CFGPOOL_EBADARG if "mbkey" or "mbval" are NULL pointers
    - CFGPOOL_EKEY2BIG if "mbkey" is too long
    - CFGPOOL_EVAL2BIG if "mbval" is too long
    - CFGPOOL_EILLKEY if "mbkey" contains an invalid multibyte sequence
    - CFGPOOL_EILLVAL if "mbval" contains an invalid multibyte sequence
    - CFGPOOL_ENOMEMORY if the function runs out of memory
    - CFGPOOL_EFULL if there are already too many keywords in the pool

*/
int                             // Error code
cfgpool_additem (
CfgPool self,                   // The pool where the item will be added
const char *mbkey,              // The keyword of the item
const char *mbval               // The value of the item
){

    if (!self) return CFGPOOL_EBADPOOL;

    if (!mbkey) return CFGPOOL_EBADARG;
    if (!mbval) return CFGPOOL_EBADARG;

    mbstate_t mbs;
    size_t len;

    // Copy the key
    len=strlen(mbkey);
    if (len == 0) return 0;  // Ignore empty keywords
    if (len == SIZE_MAX) return CFGPOOL_EKEY2BIG;
    len++;  // Make room for the final NUL

    // Allocate memory for destination string
    wchar_t *key=malloc(len * sizeof(wchar_t));
    if (!key) return CFGPOOL_ENOMEMORY;

    // Do the copy
    memset(&mbs, '\0', sizeof(mbs));
    if (mbsrtowcs(key, &mbkey, len, &mbs) == (size_t) -1 && errno == EILSEQ) {
        free(key);
        return CFGPOOL_EILLKEY;
    }


    // Copy the value
    len=strlen(mbval);
    if (len == 0) return 0;  // Ignore empty values
    if (len == SIZE_MAX) return CFGPOOL_EVAL2BIG;
    len++;  // Make room for the final NUL

    // Allocate memory for destination string
    wchar_t *val=malloc(len * sizeof(wchar_t));
    if (!val) {
        free(key);
        return CFGPOOL_ENOMEMORY;
    }

    // Do the copy
    memset(&mbs, '\0', sizeof(mbs));
    if (mbsrtowcs(val, &mbval, len, &mbs) == (size_t) -1 && errno == EILSEQ) {
        free(key);
        return CFGPOOL_EILLKEY;
    }

    // Add the item
    return internal_additem(self, key, val, NULL);
}


/*

    This function reads the file specified in "filename", parses it searching
for configuration pairs (a keyword and a value) and adds those pairs to the
"self" configuration pool, marking them as coming from "filename". It returns
0 if no error occurs, or the number of the last line read from "filename"
(returned in "lineno" if it is not NULL) and an error code otherwise:

    - CFGPOOL_ENOMEMORY if the function runs out of memory
    - CFGPOOL_EBADPOOL if "self" is a NULL pointer
    - CFGPOOL_EBADARG if "filename" is a NULL pointer
    - "-errno" if an error ocurred while handling the file
    - CFGPOOL_ELN2BIG if the file has a line which is too big
    - CFGPOOL_EKEY2BIG if the parsed keyword is too long
    - CFGPOOL_EVAL2BIG if the parsed value is too long
    - CFGPOOL_EFULL if there are already too many keywords in the pool

*/
int                             // Error code
cfgpool_addfile (
CfgPool self,                   // The pool where the data will be added
const char *filename,           // The name of the file to parse
uintmax_t *lineno               // The line number returned on errors
){

    if (!self) return CFGPOOL_EBADPOOL;

    if (!filename) return CFGPOOL_EBADARG;
    
    int inputfd;

    // Retry if an interrupt caught us    
    while ((inputfd = open(filename, O_RDONLY)) == -1 && errno == EINTR);

    if (inputfd < 0) return -errno;

    int code = cfgpool_addfd(self, inputfd, filename, lineno);

    close(inputfd);
    return code;
}


/*

    This function reads the file whose file descriptor is "fd", parses it
searching for keyword-value pairs and adds those pairs to the configuration
pool specified in "pool" as coming from "filename" (which can be empty or even
NULL). It returns 0 if no error occurs, or the number of the last line read
from "fd" (which is returned in "lineno" if it is not NULL) and one of these
error codes otherwise:

    - CFGPOOL_ENOMEMORY if the function runs out of memory
    - CFGPOOL_EBADPOOL if "self" is a NULL pointer
    - CFGPOOL_EBADARG if "fd" is "-1"
    - "-errno" if an error ocurred while handling the file
    - CFGPOOL_ELN2BIG if the file has a line which is too big
    - CFGPOOL_EKEY2BIG if the parsed keyword is too long
    - CFGPOOL_EVAL2BIG if the parsed value is too long
    - CFGPOOL_EFULL if there are already too many keywords in the pool

*/
int                             // Error code
cfgpool_addfd (
CfgPool self,                   // The pool where the data will be added
int fd,                         // The file descriptor to read from
const char *filename,           // Name of the associated file
uintmax_t *lineno               // The line number returned on errors
){

    if (!self) return CFGPOOL_EBADPOOL;
    if (fd == -1) return CFGPOOL_EBADARG;

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

    if (!lineno) lineno = &internal_lineno;
    *lineno=0;

    while (!eof) {

        // Get line
        ssize_t count = read(fd, buffer+blen, BUFSIZ);
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
                    return CFGPOOL_ELN2BIG;
                }

                /* The line is full, we have to make it bigger! */
                lsize += BUFSIZ;
                line = realloc(line, (lsize+1)*sizeof(wchar_t));
                if (!line) return CFGPOOL_ENOMEMORY;
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
                if (*lineno < UINTMAX_MAX) (*lineno)++;

                // Where the item was defined (item source)
                char *isrc = NULL;

                // To store the parsed line
                wchar_t *key = NULL;
                wchar_t *val = NULL;

                // Null terminate the string
                if (line[lpos] != L'\n') lpos++;
                line[lpos] = L'\0';

                // Parse the line and add the item
                int result=internal_parse(line, &key, &val);
                if (result > 0) {
                    free(line);
                    return result;
                }

                if (result == 0) {

                    if (filename)
                        isrc = internal_xnprintf("F%jx:%s", *lineno, filename);
                    else
                        isrc = internal_xnprintf("D%jx:%x", *lineno, fd);

                    if (!isrc) {
                        free(key);
                        free(val);
                        free(line);
                        return CFGPOOL_ENOMEMORY;
                    }

                    result=internal_additem(self, key, val, isrc);
                    if (result != 0) {
                        free(key);
                        free(val);
                        free(line);
                        free(isrc);
                        return result;
                    }
                }

                free(line);
                line = NULL;

                lsize = 0;
                lpos = 0;
            } else lpos++;
        }
        bpos = 0;

    }

    free(line);
    return 0;

}

////////////////////////////////////////////////////////////


         /////////////////////////////////////// 
        //                                     //
        //  Low level item addition functions  //
        //                                     //
         ///////////////////////////////////////


/*

    This function does the real job of adding an item to the pool, resizing it
if necessary. The function adds the information in "key", "val" and "src" to
the pool specified in "self". Don't make assumptions about what the function
does with its parameters, and NEVER use this function with a dynamic duration
object, since no copy is made of "key", "val" or "src", only a reference is
stored in the pool. You can add empty values if you want.

    If the number of buckets in the hash table which is the pool is too low
for the number of items, the function tries to grow the table and perform a
rehashing. If no memory is available, this is retried in further calls.

    If all goes OK, the function returns 0. Otherwise, the return value can be
one of these:

    - CFGPOOL_EFULL if there are already too many keywords in the pool
    - CFGPOOL_ENOMEMORY if the function runs out of memory

*/
int                             // Error code
internal_additem (
CfgPool self,                   // The pool where the data will be added
wchar_t *key,                   // The keyword of the item
wchar_t *val,                   // The value of the item
char *src                       // The source of the item (encoded)
){

    // First of all, see if we have space
    if (self->keys == UINTMAX_MAX) return CFGPOOL_EFULL;

    /*

        Fast path (more or less): if the keyword already exists in the table,
    we don't have to allocate memory for the new item, nor resize the table,
    because we are just adding another value to an existing keyword.

    */
    
    // Search the keyword
    size_t hash = internal_one_at_a_time(key);
    hash &= self->buckets-1;
    CfgItem tmpitem = self->htbl[hash];

    
    while (tmpitem) {
        // See if the keyword exists or if it is a collision
        if (! wcscmp(tmpitem->key,key)) {
            // OK, it exists! Add the value!

            free(key);  // We won't use this!

            // First, realloc the "varray" array
            struct CfgValue *tmpvalue;
            size_t len = tmpitem->vals + 1;
            fprintf(stderr, "len is %d\n", CFGPOOL_MAXVALUES);
            if (len > CFGPOOL_MAXVALUES) {

                // We can't grow more, so discard the older value
                void *src = tmpitem->varray + 1;
                void *dst = tmpitem->varray;
                size_t bytes = tmpitem->vals - 1;
                bytes *= sizeof(struct CfgValue);

                // Free the item we want to discard
                if (tmpitem->varray[0].src)
                    free(tmpitem->varray[0].src);
                free(tmpitem->varray[0].val);
                tmpitem->vals--;

                // Move the data so we can add another item at the end
                memmove(dst, src, bytes);
            } else {
                len *= sizeof(struct CfgValue);
            
                tmpvalue = realloc(tmpitem->varray, len);
                if (!tmpvalue) return CFGPOOL_ENOMEMORY;
                tmpitem->varray = tmpvalue;

            }

            // Now, add the element
            tmpvalue = &(tmpitem->varray[tmpitem->vals]);
            tmpvalue->src = (char *)    src;
            tmpvalue->val = (wchar_t *) val;

            tmpitem->vals++;

            return 0;
        }
        
        // Bad luck, try the next one
        tmpitem=tmpitem->next;
    };


    // If we are here, we must create a new item...
    struct CfgItem *item = malloc(sizeof(struct CfgItem));
    if (!item) return CFGPOOL_ENOMEMORY;

    item->key = (wchar_t *) key;
    item->vals = 1;
    item->varray = malloc(sizeof(struct CfgValue));
    if (!item->varray) {  // Oops!
        free(item);
        return CFGPOOL_ENOMEMORY;
    }

    // Store the data    
    item->varray->src = (char *) src;
    item->varray->val = (wchar_t *) val;

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
    if (self->keys/2 > self->buckets && self->buckets < CFGPOOL_MAXBUCKETS) {

        // Create the new table
        CfgItem *tmptable = calloc(self->buckets << 1, sizeof(CfgItem));

        if (tmptable) {
            // Perform rehashing and free the old hash table
            
            size_t i=0;
            
            for (i=0; i < self->buckets; i++) {

                // Double parentheses so GCC doesn't complain
                while ((tmpitem=self->htbl[i])) {
                    self->htbl[i]=tmpitem->next;
                    hash=internal_one_at_a_time(tmpitem->key);
                    hash &= (self->buckets << 1)-1;
                    tmpitem->next=tmptable[hash];  // Relink node
                    tmptable[hash]=tmpitem;  // Add node to the new table
                }
            }

            free(self->htbl);

            self->htbl = tmptable;
            self->buckets <<= 1;

            // Calculate new hash for the current item
            hash &= internal_one_at_a_time(key);
            hash &= self->buckets-1;

        };
        
    }
    
    // Add the element
    item->next=self->htbl[hash];
    self->htbl[hash]=item;
    self->keys++;

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

    - "-1" if the line can be ignored
    - CFGPOOL_EKEY2BIG if the parsed keyword is too long
    - CFGPOOL_EVAL2BIG if the parsed value is too long

*/
int
internal_parse (
const wchar_t *line,            // Line to parse
wchar_t **key,                  // Where to put the parsed key
wchar_t **val                   // Where to put the parsed value
){

    const wchar_t *current;   // Temporary marker
    const wchar_t *keystart;  // Keyword starting point
    size_t keylen = 0;        // Keyword length (in wide characters)
    const wchar_t *valstart;  // Value starting point
    size_t vallen = 0;        // Value length (in wide characters)

    current=line;
    keystart=line;
    valstart=line;
    
    // Start parsing!

    // Skip leading whitespace
    while (*current && iswblank(*current)) current++;

    // Ignore comment and empty lines
    if (!*current || *current == L'#') return -1;
    
    // Now, find if this is a variable or a command
    
    keystart = current;
    // Commands start with "+"
    if (current[0] == L'+') {  // OK, this is a command
        // Start searching the separator (a "blank")
        while (*current && !iswblank(*current)) current++;
        // If no keyword exists after the "+" marker, ignore the line
        if ((current - keystart) == 1) return -1;
    } else {  // OK, this is a variable
        // Now the separator is "="
        while (*current && *current != L'=') current++;
    }

    // Mark where should we go on
    valstart = current + 1;

    // Ignore whitespaces before separator
    while (iswblank(*(current - 1))) current--;

    if (! *current) return -1;  // Empty line or missing separator, ignore

    // We have found a separator, compute the keyword length
    if ((size_t)(current - keystart) > SIZE_MAX)
        return CFGPOOL_EKEY2BIG;  // Ooops, too big!
    keylen = current - keystart;  // This DOESN'T include the final NUL

    current = valstart;  // Go on and look for the value
    /*

        Now find the starting point of the value,
    ignoring trailing whitespace in the separator

    */
    while (*current && iswblank(*current)) current++;
    valstart = current;

    // Get the end of the value to compute the length
    while (*current) current++;
    
    // Ignore trailing whitespace in the value
    while (*(current-1) && iswblank(*(current-1))) current--;

    // Compute value length
    if ((size_t) (current-valstart) > SIZE_MAX)
        return CFGPOOL_EVAL2BIG;  // Ooops, too big!
    vallen = current-valstart;
    if (vallen == 0) return -1;  // Ignore empty values

    // Store the values and return them
    *key = malloc(sizeof(wchar_t)*(keylen+1));
    if (! *key) return CFGPOOL_ENOMEMORY;
    *val = malloc(sizeof(wchar_t)*(vallen+1));
    if (! *val) {
        free(*key);
        return CFGPOOL_ENOMEMORY;
    }

    wcsncpy(*key, keystart, keylen);
    wcsncpy(*val, valstart, vallen);
    (*key)[keylen] = L'\0';
    (*val)[vallen] = L'\0';

    return 0;
}

////////////////////////////////////////////////////////////


         ////////////////////
        //                  //
        //  Data retrieval  //
        //                  //
         ////////////////////


/*

    This function retrieves the last value given to "mbkey", returning a
dinamycally allocated copy in "data". The function returns 0 in case of
success (that is, "data" will contain the requested value) and an error code
otherwise. The error codes that the function can return are:

    - CFGPOOL_EBADPOOL if "self" is a NULL pointer
    - CFGPOOL_EBADARG if "mbkey" or "data" are NULL pointers
    - CFGPOOL_EKEY2BIG if "mbkey" string is too big
    - CFGPOOL_EILLKEY if "mbkey" contains an invalid multibyte sequence
    - CFGPOOL_EOUTOFMEMORY if the function runs out of memory
    - CFGPOOL_ENOTFOUND if "key" couldn't be found in pool

*/
int
cfgpool_getvalue (
CfgPool self,
const char *mbkey,
char **data
){
    if (!self) return CFGPOOL_EBADPOOL;

    if (self->keys == 0) return CFGPOOL_ENOTFOUND;

    if (!mbkey) return CFGPOOL_EBADARG;
    if (!data) return CFGPOOL_EBADARG;
    *data = NULL;

    size_t len;
    mbstate_t mbs;

    // Copy the key
    len=strlen(mbkey);
    if (len == 0) return CFGPOOL_ENOTFOUND;  // Ignore empty keywords
    if (len == SIZE_MAX) return CFGPOOL_EKEY2BIG;
    len++;  // Make room for the final NUL

    // Allocate memory for destination string
    wchar_t *key=malloc(len * sizeof(wchar_t));
    if (!key) return CFGPOOL_ENOMEMORY;

    // Do the copy
    memset(&mbs, '\0', sizeof(mbs));
    if (mbsrtowcs(key, &mbkey, len, &mbs) == (size_t) -1 && errno == EILSEQ) {
        free(key);
        return CFGPOOL_EILLKEY;
    }

    wchar_t *val = NULL;

    int status = cfgpool_getwvalue(self, key, &val);
    if (status != 0) {
        free(key);
        return status;
    }

    len = wcslen(val);
    len++; // Make room for the final NUL

    /*

        Allocate memory for destination string. Please note that,
    unfortunately, we are going to waste some memory when doing that, because
    we don't know exactly how many bytes will the wide-character string when
    converted to multibyte unless we convert it twice (one to get the number
    of bytes necessary, another one to do the actual conversion). So, I do a
    space for speed tradeoff (for now).

    */
    *data = malloc(len*MB_CUR_MAX);
    if (! *data) {
        free(key);
        free(val);
        return CFGPOOL_ENOMEMORY;
    }

    // Do the copy
    memset(&mbs, '\0', sizeof(mbs));

    /*

        This indirection is needed because "wcsrtombs()" modifies the source
    pointer (so the caller can call it in a loop until an entire buffer is
    converted, for example). Otherwise we can't free "val".

    */
    const wchar_t *src=val;
    if (wcsrtombs(*data, &src, len, &mbs) == (size_t) -1 && errno == EILSEQ) {
        free(key);
        free(val);
        free(*data);
        return CFGPOOL_EILLKEY;
    }

    free(key);
    free(val);
    return 0;
}


/*

    This function retrieves a the last value given to "wkey", returning a
dinamycally allocated copy in "wdata". The function returns 0 in case of
success (that is, "wdata" will contain the requested value) and an error code
otherwise. The error codes that the function can return are:

    - CFGPOOL_EBADPOOL if "self" is a NULL pointer
    - CFGPOOL_EBADARG if "wkey" or "wdata" are NULL pointers
    - CFGPOOL_ENOMEMORY if the function runs out of memory
    - CFGPOOL_ENOTFOUND if "wkey" couldn't be found in pool

*/
int
cfgpool_getwvalue (
CfgPool self,
const wchar_t *wkey,
wchar_t **wdata
){

    if (!self) return CFGPOOL_EBADPOOL;

    if (self->keys == 0) return CFGPOOL_ENOTFOUND;

    if (!wkey) return CFGPOOL_EBADARG;

    if (!wdata) return CFGPOOL_EBADARG;
    *wdata = NULL;

    size_t len=wcslen(wkey);
    if (len == 0) return CFGPOOL_ENOTFOUND;  // Ignore empty keys
    if (len == SIZE_MAX) return CFGPOOL_EKEY2BIG;

    size_t hash = internal_one_at_a_time(wkey);
    hash &= self->buckets-1;

    CfgItem tmpitem=self->htbl[hash];
    
    while (tmpitem) {
        if (!wcscmp(tmpitem->key, wkey)) break;
        tmpitem = tmpitem->next;
    }
    if (!tmpitem) return CFGPOOL_ENOTFOUND;

    wchar_t *wval=tmpitem->varray[tmpitem->vals-1].val;
    len = wcslen(wval);
    len++;

    *wdata=malloc(len * sizeof(wchar_t));
    if (!*wdata) return CFGPOOL_ENOMEMORY;
    wcsncpy(*wdata, wval, len);

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


int
cfgpool_humanreadable (
CfgPool self
){

    if (!self) return CFGPOOL_EBADPOOL;

    fprintf(stderr, "=================================\n");
    fprintf(stderr, "Pool @ %p\n", self);
    fprintf(stderr, "Number of buckets: %zu\n", self->buckets);
    fprintf(stderr, "Number of keys   : %ju\n", self->keys);

    size_t i = 0;
    while (i < self->buckets) {
        CfgItem tmpitem = self->htbl[i];
        if (tmpitem) {
            size_t len=1;
            while ((self->buckets - 1) >> (len * 4)) len++;
            fprintf(stderr, "  Bucket %#.*zx (@%p)\n", len, i, tmpitem);
        }
        while (tmpitem) {
            fprintf(stderr, "    Keyword is '%ls'\n", tmpitem->key);
            size_t v = tmpitem->vals;
            
            fprintf(stderr, "    %zu value(s):\n", v);
            while (v--) {
                fprintf(stderr, "      Value: '%ls'\n", tmpitem->varray[v].val);
                fprintf(stderr, "      Src  : '%s'\n", tmpitem->varray[v].src);
                fprintf(stderr, "      ------\n");
            }
            tmpitem = tmpitem->next;
        }
        i++;
    }
    fflush(stderr);
    return 0;
}
