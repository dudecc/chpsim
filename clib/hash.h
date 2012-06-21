/*
hash.h: hash table for strings or pointers.

Copyright (c) 1999 Marcel R. van der Goot
All rights reserved.

This file is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This file is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this file.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifndef HASH_H
#define HASH_H

#include "standard.h"

/* The hash table stores elements (key, data).
   The data can either be a pointer (data.p) or an int (data.i).

   The key can have one of three forms:
	a 0-terminated string (default);
	an uninterpreted pointer (HASH_ptr_is_key), where the address itself
	    is the key, not what it points to (the 0 pointer cannot be
	    inserted);
	a fixed-length byte sequence (HASH_fixed_len_key), similar to a
	    string, but without 0-termination (hash_table->key_len must
	    be set).
   Each key can occur only once.

   Some functions give access to a hash_entry. You may read the key,
   but only the data field may be modified.

   Normally, the hash table is automatically resized. Generally, the
   size is not critical for performance, and you don't need to worry about
   it. (If size is >> nr_elements, you're wasting space;
   if size <<< nr_elements, operations may slow down.)
*/

/* both must be powers of 2 */
#define HASH_MIN_SIZE 4U /* >= 1U */
#define HASH_MAX_SIZE 32768U /* <= 65536U */

/********** data structure ***************************************************/

typedef struct hash_entry hash_entry;
struct hash_entry
   { char *key;
     hash_entry *tail;
     union { long i;
	     void *p;
	   } data;
   };
/* if key = 0, the entry is empty/non-existent (0 is not a legal key) */

typedef enum hash_flags
   { HASH_no_alloc_keys = 1
        /* don't alloc new memory for keys */
   , HASH_no_free_keys = HASH_no_alloc_keys << 1
	/* don't free memory of keys */
   , HASH_const_keys = HASH_no_alloc_keys | HASH_no_free_keys

   , HASH_no_auto_size = HASH_no_free_keys << 1
	/* don't adjust size automatically */
   , HASH_ptr_is_key = HASH_no_auto_size << 1
	/* the pointer itself is the (fixed-size) key (implies const_keys) */
   , HASH_fixed_len_key = HASH_ptr_is_key << 1
	/* the key is a fixed-size byte sequence */
   } hash_flags;

typedef int hash_func(hash_entry *, void *);
  /* used by hash_delete, free_hash_table, and hash_apply */

typedef int hash_vfunc(void *, void *);
  /* used by hash_apply_key and hash_apply_data */

typedef struct hash_table
   { uint size; /* power of 2 */
     ulong nr_entries;
     hash_entry *table; /* hash_entry table[size]; */
     hash_flags flags;
     hash_func *del_data;
     void *del_info; /* default 0 */
     uint key_len; /* must set to > 0 if HASH_fixed_len_key is used */
   } hash_table;
 /* The del_data, del_info, and key_len fields may be changed.
    The HASH_no_auto_size flag may be changed.
 */

extern void hash_table_init
 (hash_table *h, uint size, hash_flags flags, hash_func *del_data);
 /* Pre: h has been allocated, but not initialized.
    Initializes h to the smallest power of 2 >= size (subject to HASH_MIN_SIZE
    and HASH_MAX_SIZE), with the given flags.
    If del_data != 0, del_data(entry, del_info) is called whenever an
    element is deleted. (del_info is set to 0 by default.)
    If HASH_fixed_len_key is set, you must set h->key_len immediately
    after calling this function.
 */

extern void hash_table_free(hash_table *h);
 /* Delete all elements from h (as with hash_delete()), then free
    the memory allocated for the table. h itself is not freed.
    (OK to call several times.)
 */

/********** element operations ***********************************************/

extern hash_entry *hash_find(hash_table *h, const char *key);
 /* Pre: key != 0.
    If the key is in the hash table, returns the corresponding entry;
    otherwise, returns 0.
 */

extern int hash_insert(hash_table *h, const char *key, hash_entry **q);
 /* Pre: key != 0.
    If the key is already in h, makes *q point to the corresponding item and
    returns 1; otherwise, the key is inserted, *q is pointed to the new item,
    and return is 0. q may be zero if you don't need to access the item.
    May increase the table size, unless HASH_no_auto_size is set.
    New memory is allocated for the key, unless HASH_no_alloc_keys or
    HASH_const_keys is set.
 */

extern int hash_delete(hash_table *h, const char *key);
 /* Pre: key != 0.
    Removes k from h. Return is 1 if key was in h; 0 otherwise.
    May decrease the table size, unless HASH_no_auto_size is set.
    The memory for the key is deallocated, unless HASH_no_free_keys or
    HASH_const_keys is set.
 */

extern int hash_apply(hash_table *h, hash_func *f, void *x);
 /* Call f(e, x) for every element in the table, as long as f returns 0.
    If f returns non-zero, hash_apply returns immediately with the same
    value. (Otherwise, hash_apply returns 0.)
    f may not modify the hash table in any way, other than modifying
    e->data.
 */

extern int hash_apply_key(hash_table *h, hash_vfunc *f, void *x);
 /* Call f(e->key, x) for every element in the table, as long as f returns 0.
    If f returns non-zero, hash_apply returns immediately with the same
    value. (Otherwise, hash_apply returns 0.)
    f may not modify the contents of the key unless HASH_ptr_is_key is set.
 */

extern int hash_apply_data(hash_table *h, hash_vfunc *f, void *x);
 /* Call f(e->data.p, x) for every element in the table, as long as f returns 0.
    If f returns non-zero, hash_apply returns immediately with the same
    value. (Otherwise, hash_apply returns 0.)
 */

extern int hash_apply_delete(hash_table *h, hash_func *f, void *x);
 /* Call f(e, x) for every element in the table.
    If f returns non-zero, delete the element from the table (without calling
    h->del_data).
    Return value is the number of elements that were deleted.
    f may not modify the hash table in any way, other than by
    returning non-zero or by modifying e->data.
    May decrease the table size, unless HASH_no_auto_size is set.
 */

/********** sizing functions *************************************************/

extern int hash_set_size(hash_table *h, uint size);
 /* Pre: h has been initialized.
    Change size of h to size (rounded up to power of 2, subject to
    HASH_MIN_SIZE and HASH_MAX_SIZE).
    Return is 1, except if size could not be adjusted because there
    was not enough memory.
 */

#endif /* HASH_H */
