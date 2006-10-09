/*
Configuration pool (source).
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

    In addition to this, the object contains a "VHook *", a function pointer
for the validation hook, a function callback to perform validation of config
items as soon as they're read from the data source.

*/
typedef struct CfgItem {
    struct CfgItem *next;   // To implement the linked list
    size_t vals;            // Number of values this item has
    wchar_t *key;           // This item's keyword
    wchar_t **varray;       // This item's set of values
} *CfgItem;

struct CfgPool {
    size_t buckets;         // The number of buckets in the hash table
    uintmax_t keys;         // The number of keywords in the hash table
    CfgItem *htbl;          // The hash table (the array of buckets)
    VHook *validate;        // Validation function
};

// CIL 0, to reset CIL's
static const CIL cil0 = {
    .filename = NULL,
    .fildes = -1,
    .lineno = 0,
};


// Prototypes for internal functions 
static        int     internal_additem       (CfgPool, wchar_t *, wchar_t *);
static        int     internal_getitem       (CfgPool, const wchar_t *, CfgItem *);
static        int     internal_parse         (const wchar_t *, wchar_t **, wchar_t **);
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
        self->validate = NULL;

        // The initial size of the pool is 2^7 (128) buckets
        self->htbl = calloc(1 << 7, sizeof(CfgItem));
        if (self->htbl) self->buckets = 1 << 7;
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

    while (self->buckets--) {  // For each bucket...
        CfgItem tmpitem;
        
        // Double parentheses so GCC doesn't complain
        while ((tmpitem = self->htbl[self->buckets])) {  // For each key

            // OK, the bucket is not empty

            // Go on...
            self->htbl[self->buckets] = tmpitem->next;

            // Free the element
            while (tmpitem->vals--)
                free(tmpitem->varray[tmpitem->vals]);

            // Free the rest of fields
            free (tmpitem->key);
            free (tmpitem->varray);
            
            // Free the item itself
            free (tmpitem);
        }
        
    }
    // If the table exists, free it
    if (self->htbl) free(self->htbl);

    // Free the object
    free(self);
}

////////////////////////////////////////////////////////////


         ///////////////
        //             //
        //  Accessors  //
        //             //
         ///////////////


/*

    This function sets the validation hook "vhook" for the "self" pool. It
returns 0 on success and one of these error codes otherwise:

    - CFGPOOL_EBADPOOL if "self" is a NULL pointer
    - CFGPOOL_EBADARG if "vhook" is a NULL pointer
    
    The validation hook must be a function pointer. The function pointed must
have the following prototype:

    int (*) (wchar_t *, wchar_t *, CIL)

*/
int                             // Error code
cfgpool_setvhook (
CfgPool self,                   // The pool where the hook will be set
VHook vhook                     // The validation callback
){

    if (!self) return CFGPOOL_EBADPOOL;
    if (!vhook) return CFGPOOL_EBADARG;

    // Impossible to make it simpler...    
    self->validate=vhook;

    return 0;
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
"wval" are preserved. Empty keywords or values are silently ignored. Too large
keywords or values are silently truncated.

    If no error occurs the function returns 0, and an error code otherwise. If
the caller wants additional information about the error, he must call the
"cfgpool_error()" method (except for the CFGPOOL_EBADPOOL error, of course).
Error codes that this function may return are:

    - CFGPOOL_EBADPOOL if "self" is a NULL pointer
    - CFGPOOL_EBADARG if "wkey" or "wval" are NULL pointers
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

    size_t len;  // To store lengths for copying strings

    // Copy the keyword
    len = wcslen(wkey);
    if (len == 0) return 0;  // Silently ignore empty keywords

    // If lenght is too large, make it smaller
    if (len >= (SIZE_MAX / sizeof(wchar_t)) - 1)
        len = (SIZE_MAX / sizeof(wchar_t)) - 2;

    len++;  // Make room for the NUL byte at the end
    wchar_t *key = malloc(len * sizeof(wchar_t));
    if (!key) return CFGPOOL_ENOMEMORY;

    wcsncpy(key, wkey, len-1);
    key[len] = L'\0';  // Add the final NUL byte


    // Copy the value
    len = wcslen(wval);
    if (len == 0) {  // Silently ignore empty values
        free(key);
        return 0;
    }

    // If lenght is too large, make it smaller
    if (len >= (SIZE_MAX / sizeof(wchar_t)) - 1)
        len = (SIZE_MAX / sizeof(wchar_t)) - 2;

    len++;  // Make room for the NUL byte at the end
    wchar_t *val = malloc(len * sizeof(wchar_t));
    if (!val) {  // Oops! No memory...
        // Don't forget to free already allocated key!
        free(key);
        return CFGPOOL_ENOMEMORY;
    }

    wcsncpy(val, wval, len-1);
    val[len] = L'\0';  // Add the final NUL byte

    // Add the item
    return internal_additem(self, key, val);

}


/*

    This function adds a new configuration item, that is, a keyword (throught
the "mbkey" argument) and its corresponding value (in the "mbval" argument) to
the pool specified in the "self" argument. This is the multibyte interface.

    The given strings must be NUL-terminated, multibyte or monobyte strings,
and they are not added to the pool: a copy is made and then added to the pool,
so you can safely use dynamic duration strings. The original contents of both
"mbkey" and "mbval" are preserved. Both empty keywords and empty values are
silently ignored. Too large keywords or values are silently truncated.

    If no error occurs the function returns 0, and an error code otherwise. If
the caller wants additional information about the error, he must call the
"cfgpool_error()" method (except for the CFGPOOL_EBADPOOL error, of course).
Error codes that this function may return are:

    - CFGPOOL_EBADPOOL if "self" is a NULL pointer
    - CFGPOOL_EBADARG if "mbkey" or "mbval" are NULL pointers
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

    mbstate_t mbs;  // For doing the MB->WC conversions
    size_t len;  // For doing the copying

    // Copy the key
    len = strlen(mbkey);
    if (len == 0) return 0;  // Ignore empty keywords
    if (len == SIZE_MAX) len = SIZE_MAX - 1;
    len++;  // Make room for the final NUL

    // Allocate memory for destination string
    wchar_t *key = malloc(len * sizeof(wchar_t));
    if (!key) return CFGPOOL_ENOMEMORY;

    // Do the copy
    memset(&mbs, '\0', sizeof(mbs));
    if (mbsrtowcs(key, &mbkey, len, &mbs) == (size_t) -1 && errno == EILSEQ) {
        free(key);
        return CFGPOOL_EILLKEY;
    }


    // Copy the value
    len = strlen(mbval);
    if (len == 0) {  // Ignore empty values
        free(key);
        return 0;
    }
    if (len == SIZE_MAX) len = SIZE_MAX - 1;
    len++;  // Make room for the final NUL

    // Allocate memory for destination string
    wchar_t *val = malloc(len * sizeof(wchar_t));
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
    return internal_additem(self, key, val);
}


/*

    This function reads the file specified in "filename", parses it searching
for configuration pairs (a keyword and a value) and adds those pairs to the
"self" configuration pool, marking them as coming from "filename". It returns
0 if no error occurs, or an error code otherwise:

    - CFGPOOL_ENOMEMORY if the function runs out of memory
    - CFGPOOL_EBADPOOL if "self" is a NULL pointer
    - CFGPOOL_EBADARG if "filename" is a NULL pointer
    - CFGPOOL_EBADPAIR if the parsed keyword-value pair is invalid
    - "-errno" if an error ocurred while handling the file
    - CFGPOOL_EFULL if there are already too many keywords in the pool

*/
int                             // Error code
cfgpool_addfile (
CfgPool self,                   // The pool where the data will be added
const char *filename            // The name of the file to parse
){

    if (!self) return CFGPOOL_EBADPOOL;

    if (!filename) return CFGPOOL_EBADARG;
    
    int fd;

    // Retry if an interrupt caught us    
    while ((fd = open(filename, O_RDONLY)) == -1 && errno == EINTR);

    if (fd < 0) return -errno;

    // Use "cfgpool_addfd()" internally, it's much simpler
    int result = cfgpool_addfd(self, fd, filename);

    close(fd);

    return result;
}


/*

    This function reads the file whose file descriptor is "fd", parses it
searching for keyword-value pairs and adds those pairs to the configuration
pool specified in "self", treating them as coming from "filename" (that can be
NULL). It returns 0 if no error occurs, or one of these error codes otherwise:

    - CFGPOOL_ENOMEMORY if the function runs out of memory
    - CFGPOOL_EBADPOOL if "self" is a NULL pointer
    - CFGPOOL_EBADARG if "fd" is "-1"
    - CFGPOOL_EBADPAIR if the parsed keyword-value pair is invalid
    - "-errno" if an error ocurred while handling the file
    - CFGPOOL_EFULL if there are already too many keywords in the pool

*/
int                             // Error code
cfgpool_addfd (
CfgPool self,                   // The pool where the data will be added
int fd,                         // The file descriptor to read from
const char *filename            // Associated filename (for error reporting)
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
    size_t bpos = 0;            // Current convert position in 'buffer' (bytes)

    wchar_t *line = NULL;       // Stored line
    size_t lpos = 0;            // Current store position in 'line' (wchars)
    size_t lsize = 0;           // 'line' capacity, in wchars
    
    int eof = 0;  // So we know that we hit EOF
    uintmax_t lineno = 0;  // Line number, for error reporting

    while (!eof) {

        // Get a chunk
        ssize_t count = read(fd, buffer+blen, BUFSIZ);
        if (count < BUFSIZ) {
            eof = 1;  // Signal EOF to the main loop
        }
        if (count < 0) return -errno;

        /* There are now more "count" bytes in the "buffer" */
        blen += count;

        while (blen) {  // While there are bytes in the buffer
            /*

                We have to convert the buffer to wchar_t, as
            many chars as we can. We use 'mbrtowc()' for that.

            */
            size_t len;  // Length of current converted char

            if (lpos == lsize) {  // Line is full
                if (lsize < SIZE_MAX - 1) {  // Make it grow
                    /* See if we exceeded the limit */

                    if (lsize > SIZE_MAX - 1 - BUFSIZ)
                        lsize = SIZE_MAX - 1;
                    else
                        lsize += BUFSIZ;
                    
                    // The line is full, we have to make it bigger!
                    // Please note that we always reserve space for the NUL
                    line = realloc(line, ( lsize + 1 ) *sizeof(wchar_t));
                    if (!line) return CFGPOOL_ENOMEMORY;
                } else {
                    // Cannot grow!
                    // Overwrite last character until L'\n' is found
                    lpos--;
                }
            }

            // Copy another char of the buffer into the line
            len = mbrtowc(line+lpos, buffer+bpos, blen, NULL);

            // If we got a partial sequence...
            if (len == (size_t)-2) {
                // 1. Move the current position to the start of the buffer
                // 2. Get more bytes from the file to complete sequence
                memmove(buffer, buffer+bpos, blen);
                break;
            }

            // Invalid character or embedded NUL byte
            if (len == (size_t)-1 || !len) {
                len = 1; // Skip it
                line[lpos] = L'_';  // Store a fake char?
            }

            bpos += len;  // Advance position
            blen -= len;  // Less chars to process...
                
                
            // If we have a full line or we run out of bytes...
            if (line[lpos] == L'\n' || (blen == 0 && eof)) {

                // Increment line number if possible
                if (lineno != UINTMAX_MAX) lineno++;

                // Placeholders to store the parsed line
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

                // Line parsed OK
                if (result == 0) {

                    // Validate if possible
                    if (self->validate) {
                        CIL cil = cil0;
                        cil.filename = (char *) filename;
                        cil.fildes = fd;
                        cil.lineno = lineno;
                        result = self->validate(key, val, cil);
                    }

                    /*
                    
                        Please note that if the pool doesn't have a validation
                    hook, "result" is still 0, so the pair will appear as valid
                    no matter if it really is or not. But that's good! ;)
                    
                    */

                    if (result > 0)  // Pair is invalid
                        result = CFGPOOL_EBADITEM;

                    if (result == 0 && val)  // Pair is valid and not empty
                        result = internal_additem(self, key, val);

                    /*

                        Here "result" may be greater than 0 if:
                       
                            - self->validate() was successful but
                              internal_additem() was not
                            - self->validate() sais that the pair
                              is invalid and the function must break

                        The fact is that if we are here, the item couldn't be
                    added to the pool, no matter why ("result" contains why).

                    */

                    if (result > 0) {
                        if (key) free(key);
                        if (val) free(val);
                        free(line);
                        return result;
                    }
                    
                    // Free keyword here ONLY if the value is empty
                    if (!val && key) free(key);
                    
                }

                // Start with a new line
                free(line);
                line = NULL;

                lsize = 0;
                lpos = 0;
            } else lpos++;  // Next char, please
        }
        bpos = 0;  // Next buffer, please
    }

    free(line);
    return 0;

}

////////////////////////////////////////////////////////////


         ////////////////////////////////////// 
        //                                    //
        //  Low level item addition function  //
        //                                    //
         //////////////////////////////////////


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
wchar_t *val                    // The value of the item
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
    hash &= self->buckets - 1;
    CfgItem tmpitem = self->htbl[hash];

    
    while (tmpitem) {  // For each config item in this bucket
        // See if the keyword exists or if it is a collision
        if (!wcscmp(tmpitem->key, key)) {
            // OK, it exists! Add the value!

            free(key);  // We won't use this!

            // First, realloc the "varray" array
            wchar_t **tmpvalue;

            /*
            
                There's no point in storing one value more, since the
            functions that get all values will return a NULL terminated array
            of values. If, in the future, those functions return a size
            instead of a NULL terminated array, we will need to fix this.
            
            */
            if (tmpitem->vals == (SIZE_MAX / sizeof (wchar_t *)) - 1) {

                // We can't grow more, so discard the older value
                void *src = tmpitem->varray + 1;
                void *dst = tmpitem->varray;
                size_t bytes = tmpitem->vals - 1;
                bytes *= sizeof(wchar_t *);

                // Free the item we want to discard
                free(tmpitem->varray[0]);
                tmpitem->vals--;

                // Move the data so we can add another item at the end
                memmove(dst, src, bytes);
            } else {
                size_t len = tmpitem->vals + 1;

                // Resize the "varray" array
                len *= sizeof(wchar_t);
            
                tmpvalue = realloc(tmpitem->varray, len);
                if (!tmpvalue) return CFGPOOL_ENOMEMORY;
                tmpitem->varray = tmpvalue;

            }

            // Now, add the element
            tmpitem->varray[tmpitem->vals] = (wchar_t *) val;
            tmpitem->vals++;

            return 0;
        }
        
        // Bad luck, try the next one
        tmpitem=tmpitem->next;
    };


    // If we are here, we must create a new item...
    CfgItem item = malloc(sizeof(struct CfgItem));
    if (!item) return CFGPOOL_ENOMEMORY;

    item->key = (wchar_t *) key;
    item->vals = 1;
    item->varray = malloc(sizeof(wchar_t *));
    if (!item->varray) {  // Oops!
        free(item);
        return CFGPOOL_ENOMEMORY;
    }

    // Store the data    
    item->varray[0] = (wchar_t *) val;

    /*

        If the number of elements is greater than twice the number of buckets,
    do a resize of the hash table if possible. We cannot do a simple realloc,
    because we must ensure that the new spaces are filled with NULL pointers
    and because we have to rehash all elements in the table...

        When resizing, we are bound in size. First, we cannot allocate more
    than SIZE_MAX bytes, so no matter if we want to grow, we can't do further.
    Second, the number of buckets cannot be more than SIZE_MAX / 2 or we will
    have an overflow in the size of the hash table (the real number of items
    stored in the table). The last bound is the maximum number of buckets we
    can fit in SIZE_MAX bytes, since it must be a power of two (for speed).
    So, we must check for all these conditions.

    */

    // Do we need to (and can) grow?
    if (self->keys/2 > self->buckets &&
        self->buckets < (SIZE_MAX >> 1) / sizeof(CfgItem)) {

        // Create the new table
        CfgItem *tmptable = calloc(self->buckets << 1, sizeof(CfgItem));

        if (tmptable) {
            // Perform rehashing and free the old hash table
            
            size_t i=0;
            
            for (i=0; i < self->buckets; i++) {  // For each bucket

                // Double parentheses so GCC doesn't complain
                while ((tmpitem=self->htbl[i])) {  // For each item

                    // Unlink item from old table
                    self->htbl[i]=tmpitem->next;
                    
                    // Rehash item
                    hash=internal_one_at_a_time(tmpitem->key);
                    hash &= (self->buckets << 1)-1;
                    
                    // Relink item in new table
                    tmpitem->next=tmptable[hash];
                    tmptable[hash]=tmpitem;

                }
            }
            // Free old table
            free(self->htbl);

            // Set up new table in object
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

    // Do the math...
    while (len--) {
        hash += data[len];
        hash += (hash << 10);
        hash ^= (hash >>  6);
    }
    hash += (hash <<  3);
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
the middle, so it can be used as string terminator. If the parsed key or value
are too long (larger than SIZE_MAX), they're silently truncated.

    It returns 0 in case of success, a negative value if the line can be
ignored (it is a comment or is empty) and an error code otherwise:

    - CFGPOOL_ENOMEMORY if the function runs out of memory

*/
int
internal_parse (
const wchar_t *line,            // Line to parse
wchar_t **key,                  // Where to put the parsed key
wchar_t **val                   // Where to put the parsed value
){

    const wchar_t *current;   // Temporary marker
    const wchar_t *keystart;  // Keyword starting point
    const wchar_t *valstart;  // Value starting point
    size_t len = 0;

    current=line;
    keystart=line;
    valstart=line;
    
    // Start parsing!
    *key = NULL;
    *val = NULL;

    // Skip leading whitespace
    while (*current && iswblank(*current)) current++;

    // Ignore comment and empty lines
    if (!*current || *current == L'#') return -1;
    keystart = current;
    
    // Now, find if this is a variable or a command
    // Commands start with "+"
    if (current[0] == L'+') {  // OK, this is a command
        // Start searching the separator (a "blank")
        while (*current && !iswblank(*current)) current++;
    } else {  // OK, this is a variable
        // Now the separator is "="
        while (*current && *current != L'=') current++;
    }

    // Mark where should we go on
    valstart = current;

    // Ignore whitespaces before separator
    current--;
    while (*current && iswblank(*current)) current--;

    // Store keyword
    current++;
    if ((size_t)(current - keystart) >= SIZE_MAX)
        len = SIZE_MAX - 1;  // This DOESN'T include the final NUL
    else 
        len = current - keystart;  // This DOESN'T include the final NUL
    *key = malloc(sizeof(wchar_t) * (len + 1));
    if (!*key) return CFGPOOL_ENOMEMORY;
    wcsncpy(*key, keystart, len);
    (*key)[len] = L'\0';
    
    if (!*valstart)  // Missing separator (and value)
        return 0;

    current = valstart + 1;  // Go on and look for the value

    /*

        Now find the starting point of the value,
    ignoring trailing whitespace in the separator

    */
    while (*current && iswblank(*current)) current++;
    valstart = current;  // Mark the beginning of the value

    // Get the end of the value to compute the length
    while (*current) current++;
    
    // Ignore trailing whitespace in the value
    current--;
    while (*current && iswblank(*current)) current--;

    // Compute value length
    current++;
    if ((size_t) (current-valstart) > SIZE_MAX)
        len = SIZE_MAX - 1;  // This DOESN'T include the final NUL
    else
        len = current-valstart;  // This DOESN'T include the final NUL

    if (len == 0) return 0;  // Return only the keyword

    // Store the values and return them
    *val = malloc(sizeof(wchar_t) * (len + 1));
    if (!*val) {
        free(*key);
        return CFGPOOL_ENOMEMORY;
    }

    wcsncpy(*val, valstart, len);
    (*val)[len] = L'\0';

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
dinamically allocated copy in "data". The function returns 0 in case of
success (that is, "data" will contain the requested value) and an error code
otherwise. The error codes that the function can return are:

    - CFGPOOL_EBADPOOL if "self" is a NULL pointer
    - CFGPOOL_EBADARG if "mbkey" or "data" are NULL pointers
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
    len = strlen(mbkey);
    if (len == 0) return CFGPOOL_ENOTFOUND;  // Ignore empty keywords
    if (len == SIZE_MAX) len = SIZE_MAX - 1;
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

    CfgItem tmpitem;

    // We get a copy of the data in wchar_t format and do the conversion
    if (internal_getitem(self, key, &tmpitem)) {
        free(key);
        return CFGPOOL_ENOTFOUND;
    }
    
    wchar_t *val = tmpitem->varray[tmpitem->vals];
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
    *data = malloc(len * MB_CUR_MAX);
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
    const wchar_t *src = val;
    wcsrtombs(*data, &src, len, &mbs);

    free(key);
    free(val);
    return 0;
}


/*

    This function retrieves the last value given to "wkey", returning a copy
(dinamically allocated) in "wdata". The function returns 0 in case of success
(and "wdata" will contain the requested value) and an error code otherwise.
The error codes that the function can return are:

    - CFGPOOL_EBADPOOL if "self" is a NULL pointer
    - CFGPOOL_EBADARG if "wkey" or "wdata" are NULL pointers
    - CFGPOOL_ENOMEMORY if the function runs out of memory
    - CFGPOOL_ENOTFOUND if "wkey" couldn't be found in pool

*/
int                             // Error code
cfgpool_getwvalue (
CfgPool self,                   // The pool to get the value from
const wchar_t *wkey,            // The keyword to find
wchar_t **wdata                 // The returned value
){

    if (!self) return CFGPOOL_EBADPOOL;

    if (self->keys == 0) return CFGPOOL_ENOTFOUND;

    if (!wkey) return CFGPOOL_EBADARG;

    if (!wdata) return CFGPOOL_EBADARG;
    *wdata = NULL;

    if (wcslen(wkey) == 0) return CFGPOOL_ENOTFOUND;  // Ignore empty keys

    CfgItem tmpitem;

    if (internal_getitem(self, wkey, &tmpitem))
        return CFGPOOL_ENOTFOUND;
    
    // Data found, copy it
    wchar_t *wval = tmpitem->varray[tmpitem->vals - 1];
    size_t len = wcslen(wval);
    len++;

    *wdata = malloc(len * sizeof(wchar_t));
    if (!*wdata) return CFGPOOL_ENOMEMORY;
    wcsncpy(*wdata, wval, len);

    return 0;
}


/*

    This function retrieves all the value ever given to "mbkey" in the order
they were assigned, returning a copy (dinamically allocated) through a NULL
terminated array whose address is "mbdata". The function returns 0 in case of
success (and "mbdata" will point to the array containing the requested values)
and an error code otherwise. The error codes that the function can return are:

    - CFGPOOL_EBADPOOL if "self" is a NULL pointer
    - CFGPOOL_EBADARG if "wkey" or "wdata" are NULL pointers
    - CFGPOOL_EILLKEY if "mbkey" contains an invalid multibyte sequence
    - CFGPOOL_ENOMEMORY if the function runs out of memory
    - CFGPOOL_ENOTFOUND if "wkey" couldn't be found in pool

*/
int                             // Error code
cfgpool_getallvalues (
CfgPool self,                   // The pool to get the values from
const char *mbkey,              // The keyword to find
char ***mbdata                  // The returned value
){

    if (!self) return CFGPOOL_EBADPOOL;

    if (self->keys == 0) return CFGPOOL_ENOTFOUND;

    if (!mbkey) return CFGPOOL_EBADARG;

    if (!mbdata) return CFGPOOL_EBADARG;

    *mbdata = NULL;
    
    if (strlen(mbkey) == 0) return CFGPOOL_ENOTFOUND;  //Ignore empty keys

    size_t len;
    mbstate_t mbs;

    // Copy the key
    len = strlen(mbkey);
    if (len == 0) return CFGPOOL_ENOTFOUND;  // Ignore empty keywords
    if (len == SIZE_MAX) len = SIZE_MAX - 1;
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

    CfgItem tmpitem;
    if (internal_getitem(self, key, &tmpitem)) {
        free(key);
        return CFGPOOL_ENOTFOUND;
    }


    // Data found, copy all values
    
    // Build a new array
    *mbdata = malloc((tmpitem->vals + 1) * sizeof(char *));
    if (!*mbdata) {
        free(key);
        return CFGPOOL_ENOMEMORY;
    }

    // Now copy all values to the new array
    size_t i = 0;
    while (i < tmpitem->vals) {
        wchar_t *wval = tmpitem->varray[i];
        size_t len = wcslen(wval);
        len++;
        (*mbdata)[i] = malloc(len * MB_CUR_MAX);
        if (!(*mbdata)[i]) {
            while (i--) free((*mbdata)[i]);
            free(*mbdata);
            free(key);
            return CFGPOOL_ENOMEMORY;
        }

        const wchar_t *src = wval;
        memset(&mbs, '\0', sizeof(mbs));
        wcsrtombs((*mbdata)[i], &src, len, &mbs);
        i++;
    }

    (*mbdata)[i] = NULL;

    free(key);

    return 0;

}


/*

    This function retrieves all the value ever given to "wkey" in the order
they were assigned, returning a copy (dinamically allocated) through a NULL
terminated array whose address is "wdata". The function returns 0 in case of
success (and "wdata" will point to the array containing the requested values)
and an error code otherwise. The error codes that the function can return are:

    - CFGPOOL_EBADPOOL if "self" is a NULL pointer
    - CFGPOOL_EBADARG if "wkey" or "wdata" are NULL pointers
    - CFGPOOL_ENOMEMORY if the function runs out of memory
    - CFGPOOL_ENOTFOUND if "wkey" couldn't be found in pool

*/
int                             // Error code
cfgpool_getallwvalues (
CfgPool self,                   // The pool to get the values from
const wchar_t *wkey,            // The keyword to find
wchar_t ***wdata                // The returned value
){

    if (!self) return CFGPOOL_EBADPOOL;

    if (self->keys == 0) return CFGPOOL_ENOTFOUND;

    if (!wkey) return CFGPOOL_EBADARG;

    if (!wdata) return CFGPOOL_EBADARG;
    *wdata = NULL;

    if (wcslen(wkey) == 0) return CFGPOOL_ENOTFOUND;  // Ignore empty keys

    CfgItem tmpitem;
    if (internal_getitem(self, wkey, &tmpitem))
        return CFGPOOL_ENOTFOUND;

    // Data found, copy all values
    
    // Build a new array
    *wdata = malloc((tmpitem->vals + 1) * sizeof(wchar_t *));
    if (!*wdata) return CFGPOOL_ENOMEMORY;

    // Now copy all values to the new array
    size_t i = 0;
    while (i < tmpitem->vals) {
        wchar_t *wval = tmpitem->varray[i];
        size_t len = wcslen(wval);
        len++;
        (*wdata)[i] = malloc(len * sizeof(wchar_t));
        if (!(*wdata)[i]) {
            while (i--) free((*wdata)[i]);
            free(*wdata);
            return CFGPOOL_ENOMEMORY;
        }
        wcsncpy((*wdata)[i], wval, len);
        i++;
    }

    (*wdata)[i] = NULL;

    return 0;
}


/*

    This function returns in "data" a pointer to the item in pool "self"
corresponding to "key". It returns 0 on success and "CFGPOOL_ENOTFOUND"
otherwise. Please note that the function returns a reference, NOT a copy.

*/
int                             // Error code
internal_getitem (
CfgPool self,                   // The pool to get the item from
const wchar_t *key,             // The key to find
CfgItem *data                   // The configuration item returned
){

    // Do the lookup
    size_t hash = internal_one_at_a_time(key);
    hash &= self->buckets - 1;

    // Process bucket
    CfgItem tmpitem = self->htbl[hash];
    
    while (tmpitem) {  // Handle collisions when looking up
        if (!wcscmp(tmpitem->key, key)) break;
        tmpitem = tmpitem->next;
    }
    if (!tmpitem) return CFGPOOL_ENOTFOUND;

    // Data found, return it
    *data = tmpitem;

    return 0;
}

////////////////////////////////////////////////////////////


         ///////////////////////
        //                     //
        //  Dumping functions  //
        //                     //
         ///////////////////////


/*

    This function dumps the "self" configuration pool to file "filename". The
file is opened using "flags" and is created with the permissions in "mode".
The flags are *always* OR'ed with "O_WRONLY" and both "O_RDONLY" and "O_RDWR"
are silently ignored. It returns 0 on success, or an error code otherwise:

    - CFGPOOL_EBADPOOL if "self" is a NULL pointer.
    - CFGPOOL_EBADARG if "fd" is negative.
    - CFGPOOL_ENOMEMORY if the function runs out of memory
    - "-errno" if there is any problem while writing to the file

*/
int                             // Error code
cfgpool_dumptofile (
CfgPool self,                   // The pool to dump
const char *filename,           // The filename to dump to
int flags,                      // Flags to open the file
mode_t mode                     // Permissions for creating the file
) {

    if (!self) return CFGPOOL_EBADPOOL;

    int fd;

    // Use write-only mode no matter what the user said
    flags &= ~(O_RDONLY | O_RDWR);
    flags |= O_WRONLY;

    // Retry if an interrupt caught us    
    while ((fd = open(filename, flags, mode)) == -1 && errno == EINTR);
    if (fd < 0) return -errno;

    int result = cfgpool_dumptofd(self, fd);
    
    close(fd);
    
    return result;
}


/*

    This function dumps the "self" configuration pool to file descriptor "fd",
which is assumed to be opened for writing. It returns 0 in case of success, or
an error code otherwise:

    - CFGPOOL_EBADPOOL if "self" is a NULL pointer.
    - CFGPOOL_EBADARG if "fd" is negative.
    - CFGPOOL_ENOMEMORY if the function runs out of memory
    - "-errno" if there is any problem while writing to the file

*/
int                             // Error code
cfgpool_dumptofd (
CfgPool self,                   // The pool to dump
int fd                          // The file descriptor to dump to
) {

    if (!self) return CFGPOOL_EBADPOOL;
    if (fd < 0) return CFGPOOL_EBADARG;

    size_t i=0;
    
    while (i < self->buckets) {  // For each bucket in the hash table...

        CfgItem tmpitem = self->htbl[i];

        while (tmpitem) {  // For each key in the bucket...

            size_t v;  // Number of first value to dump
            wint_t sep;  // Separator to use
            
            if (tmpitem->key[0] != L'+') {  // Dump only command's last value
                v = tmpitem->vals - 1;
                sep = L'=';
            } else {  // This is a variable, dump ALL values
                v = 0;
                sep = L' ';
            }

            while (v < tmpitem->vals) {  // Process values from "v" up

                /*
                
                    This time speed is not critical (writing to a file is
                going to be much slower than doing any conversion), and in
                fact we gain a bit of speed for doing this buffering, so
                there's no problem in converting the strings twice: once for
                getting the size and once to do the actual conversion.
                
                */

                wchar_t *key = tmpitem->key;
                wchar_t *val = tmpitem->varray[v];

                // Compute the number of bytes to perform allocation
                int len = snprintf(NULL, 0, "%ls %lc %ls\n", key, sep, val);
                if (len < 0) len = INT_MAX - 1;
                len++;  // Make room for the final NUL byte
                
                char *buffer = malloc(len);
                if (!buffer) return CFGPOOL_ENOMEMORY;
                
                snprintf(buffer, len, "%ls %lc %ls\n", key, sep, val);

                char *current = buffer; 
                ssize_t count = 0;
                len--; // We won't write the final NUL
                while (len) {
                    count = write(fd, current, len);
                    
                    if (count < 0) {
                        if (errno == EINTR) continue;
                        free(buffer);
                        return -errno;
                    }

                    // Prepare for next chunk
                    len -= count;
                    current += count;
                }
                free(buffer);
                v++;  // Next value
            }

            tmpitem = tmpitem->next;  // Next key
        }
        i++;  // Next bucket
    }

    return 0;
}

////////////////////////////////////////////////////////////

    
         ////////////////////////////
        //                          //
        //  Human readable dumping  //
        //                          //
         ////////////////////////////

// FIXME: substitute for an iterator???
int                             // Error code
cfgpool_humanreadable (
CfgPool self                    // Pool to dump
){

    if (!self) return CFGPOOL_EBADPOOL;

    fprintf(stderr, "=================================\n");

    size_t buckets = self->buckets;
    size_t bits = 0;
    
    while (buckets >>= 1) bits++;
    
    fprintf(stderr, "@%p [%zu/%zu b | %ju k]\n",
                    self, bits, self->buckets, self->keys);

    size_t i = 0;
    while (i < self->buckets) {
        CfgItem tmpitem = self->htbl[i];
        if (tmpitem) {
            size_t len=1;
            while ((self->buckets - 1) >> (len * 4)) len++;
            fprintf(stderr, "B %#.*zx @%p\n", len, i, tmpitem);
        }
        while (tmpitem) {

            fprintf(stderr, "    K @%p %ls\n", tmpitem->key, tmpitem->key);

            size_t v = tmpitem->vals;
            while (v--) {
                wchar_t *val = tmpitem->varray[v];
                fprintf(stderr, "    V @%p %ls\n", val, val);
            }

            tmpitem = tmpitem->next;
        }
        i++;
    }
    fflush(stderr);
    return 0;
}
