/*
hash.c: hash table for strings or pointers.

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

#include "standard.h"
#include "hash.h"

/********** table data structure *********************************************/

extern void hash_table_init
 (hash_table *h, uint size, hash_flags flags, hash_func *del_data)
 /* Pre: h has been allocated, but not initialized.
    Initializes h to the smallest power of 2 >= size (subject to HASH_MIN_SIZE
    and HASH_MAX_SIZE), with the given flags.
    If del_data != 0, del_data(entry, del_info) is called whenever an
    element is deleted. (del_info is set to 0 by default.)
    If HASH_fixed_len_key is set, you must set h->key_len immediately
    after calling this function.
 */
 { uint k;
   hash_entry *e;
   k = HASH_MIN_SIZE;
   if (size > HASH_MAX_SIZE) size = HASH_MAX_SIZE;
   while (k < size)
     { k = k << 1; }
   h->size = k;
   NEW_ARRAY(h->table, k);
   for (e = h->table; k; e++, k--)
     { e->key = 0; }
   h->nr_entries = 0;
   h->flags = flags;
   h->del_data = del_data;
   h->del_info = 0;
   h->key_len = 0;
 }

extern void hash_table_free(hash_table *h)
 /* Delete all elements from h (as with hash_delete()), then free
    the memory allocated for the table. h itself is not freed.
    (OK to call several times.)
 */
 { uint x;
   hash_entry *e, *tmp;
   for (x = 0; x < h->size; x++)
     { e = &h->table[x];
       if (!e->key) continue;
       if (h->del_data)
	 { while (e)
	     { h->del_data(e, h->del_info);
	       e = e->tail;
	     }
	   e = &h->table[x];
	 }
       if (!IS_SET(h->flags, HASH_no_free_keys|HASH_ptr_is_key))
	 { while (e)
	     { FREE(e->key);
	       e = e->tail;
	     }
	   e = &h->table[x];
	 }
       e = e->tail;
       while (e)
	 { tmp = e;
	   e = e->tail;
	   free(tmp);
	 }
     }
   if (h->table)
     { free(h->table); h->table = 0; }
   h->size = 0;
   h->nr_entries = 0;
 }

/********** hash function ****************************************************/
/* Hash function from:
   Peter K. Pearson, "Fast Hashing of Variable-Length Text Strings",
   Communications of the ACM, 33:6, June 1990, pp. 677-680.
*/

static ubyte T[256] = 
  {  81,  41, 228,   0, 157, 204,  32,  26,  47, 110,  24, 149,  95, 170,
    210, 223, 177, 218, 129,  72,  62,  84, 119, 229,  59,  19,  83, 220,
    128, 133,   4,  82, 227, 103, 201,   7,  80,  55,  29, 175, 143, 136,
    106, 184,  21, 104,  20,  94, 145,  96, 135, 219, 203, 200,  42, 159,
     75, 253,  13, 161,  91,  57, 148,  37, 138, 102, 154, 249,  31,  10,
      5, 238, 121,  78, 212, 155, 109, 114,  39, 208, 242, 196, 117, 235,
    194, 188, 132, 199, 198, 246,  66, 164, 113, 152,  45, 231, 190, 236,
      3, 215, 171, 187, 125, 156, 221, 209,  56, 100, 222, 107,  38, 245,
    130,  40, 140,  58,  99,  88,  90, 214,  74,  71, 186, 239, 224,  68,
     30, 134,  27, 165, 162, 248, 179, 111,  64, 207,  11, 237, 195,  52,
     77,  65,  43,  69,  18, 183,  89, 101, 244,  22, 254, 226,  14,  48,
    108, 123, 211, 112,  15, 147, 205, 160,  46,  98, 122, 251, 105,  67,
    213,  76, 166,   2, 115, 174,  54, 163,  79, 151,  23, 191, 153,  73,
     12, 144, 139,  93,  87, 116,  44, 185, 168,  16, 189, 126,  97, 118,
     92, 197, 206,  50,  34, 158, 230, 178,  35, 255, 127,  49, 146,  61,
    124, 176,  17, 217, 131,   1, 182,  28, 233, 169,  33,  86, 241, 193,
    243, 173, 181,   9, 192, 240,  51, 247, 150,  63, 167,  36, 232, 216,
    141,  25,  60,  70, 137,  53,   6, 172, 225, 252,   8, 250,  85, 202,
    120, 234, 142, 180
  };

static uint hash(hash_table *h, const char *s)
 /* Pre: s != 0 (but s = "" is ok).
    Returns the hash value associated with s.
 */
 /* Note: s != 0 is because we use s = 0 to indicate the absence of
    an entry; this has nothing to do with the hash function.
 */
 { uint h0, h1;
   ubyte *c;
   uint m, len = sizeof(s);
   int i;
   if (!IS_SET(h->flags, HASH_ptr_is_key))
     { c = (ubyte*)s;
       if (IS_SET(h->flags, HASH_fixed_len_key)) len = h->key_len;
     }
   else
     { c = (ubyte*)&s; }
   m = h->size - 1;
   if (m < 256)
     { h0 = 0;
       if (!IS_SET(h->flags, HASH_ptr_is_key|HASH_fixed_len_key))
	 { while (*c)
	     { h0 = T[h0 ^ *c];
	       c++;
	     }
	 }
       else
	 { for (i = 0; i < len; i++)
	     { h0 = T[h0 ^ *c];
	       c++;
	     }
	 }
       return h0 & m; /* = h0 % (m+1), because m = 2^k - 1 */
     }
   else
     { h0 = T[*c];
       h1 = T[(1 + *c) & 255];
       if (!IS_SET(h->flags, HASH_ptr_is_key|HASH_fixed_len_key))
	 { if (*c)
	     { c++;
	       while (*c)
		 { h0 = T[h0 ^ *c];
		   h1 = T[h1 ^ *c];
		   c++;
		 }
	     }
	 }
       else
	 { c++;
	   for (i = 1; i < len; i++)
	     { h0 = T[h0 ^ *c];
	       h1 = T[h1 ^ *c];
	       c++;
	     }
	 }
       return ((h0 << 8) | h1) & m;
     }
 }

/********** element operations ***********************************************/

extern hash_entry *hash_find(hash_table *h, const char *key)
 /* Pre: key != 0.
    If the key is in the hash table, returns the corresponding entry;
    otherwise, returns 0.
 */
 { uint x;
   hash_entry *l;
   x = hash(h, key);
   l = &h->table[x];
   if (!l->key) return 0;
   if (IS_SET(h->flags, HASH_ptr_is_key))
     while (l && l->key != key) { l = l->tail; }
   else if (IS_SET(h->flags, HASH_fixed_len_key))
     while (l && memcmp(l->key, key, h->key_len)) { l = l->tail; }
   else
     while (l && strcmp(l->key, key)) { l = l->tail; }
   return l;
 }

extern int hash_insert(hash_table *h, const char *key, hash_entry **q)
 /* Pre: key != 0.
    If the key is already in h, makes *q point to the corresponding item and
    returns 1; otherwise, the key is inserted, *q is pointed to the new item,
    and return is 0.
    May increase the table size, unless HASH_no_auto_size is set.
 */
 { uint x;
   hash_entry *l, *e;
   x = hash(h, key);
   e = l = &h->table[x];
   if (l->key)
     { if (IS_SET(h->flags, HASH_ptr_is_key))
	 while (l && l->key != key) { l = l->tail; }
       else if (IS_SET(h->flags, HASH_fixed_len_key))
         while (l && memcmp(l->key, key, h->key_len)) { l = l->tail; }
       else
         while (l && strcmp(l->key, key)) { l = l->tail; }
       if (l)
	 { if (q) *q = l; return 1; }
       NEW(l);
       l->tail = e->tail;
       e->tail = l;
     }
   else
     { l->tail = 0; }
   h->nr_entries++;
   if (IS_SET(h->flags, HASH_no_alloc_keys|HASH_ptr_is_key))
     { l->key = (char*)key; }
   else if (IS_SET(h->flags, HASH_fixed_len_key))
     { NEW_ARRAY(l->key, h->key_len);
       memcpy(l->key, key, h->key_len);
     }
   else
     { NEW_ARRAY(l->key, strlen(key)+1);
       strcpy(l->key, key);
     }
   if (!IS_SET(h->flags, HASH_no_auto_size) && h->size < HASH_MAX_SIZE
       && h->nr_entries >= 4*h->size)
     { hash_set_size(h, 4*h->size);
       if (q) *q = hash_find(h, key);
     }
   else if (q) *q = l;
   return 0;
 }

extern int hash_delete(hash_table *h, const char *key)
 /* Pre: key != 0.
    Removes k from h. Return is 1 if key was in h; 0 otherwise.
    May decrease the table size, unless HASH_no_auto_size is set.
 */
 { uint x;
   hash_entry *l, *prev, *tmp;
   x = hash(h, key);
   l = &h->table[x];
   prev = 0;
   if (!l->key) return 0;
   if (IS_SET(h->flags, HASH_ptr_is_key))
     while (l && l->key != key)
       { prev = l; l = l->tail; }
   else if (IS_SET(h->flags, HASH_fixed_len_key))
     while (l && memcmp(l->key, key, h->key_len))
       { prev = l; l = l->tail; }
   else
     while (l && strcmp(l->key, key))
       { prev = l; l = l->tail; }
   if (!l) return 0;
   if (h->del_data)
     { h->del_data(l, h->del_info); }
   if (!IS_SET(h->flags, HASH_no_free_keys|HASH_ptr_is_key))
     { free(l->key); }
   l->key = 0;
   if (prev)
     { prev->tail = l->tail;
       free(l);
     }
   else
     { tmp = l->tail;
       if (tmp)
	 { *l = *tmp;
	   free(tmp);
	 }
     }
   h->nr_entries--;
   if (!IS_SET(h->flags, HASH_no_auto_size) && h->size > HASH_MIN_SIZE
       && h->nr_entries <= h->size/2)
     { hash_set_size(h, h->size/2); }
   return 1;
 }

extern int hash_apply(hash_table *h, hash_func *f, void *x)
 /* Call f(e, x) for every element in the table, as long as f returns 0.
    If f returns non-zero, hash_apply returns immediately with the same
    value. (Otherwise, hash_apply returns 0.)
    f may not modify the hash table in any way, other than modifying
    e->data.
 */
 { uint j;
   hash_entry *e;
   int r = 0;
   for (j = 0; !r && j < h->size; j++)
     { e = &h->table[j];
       if (!e->key) continue;
       while (e && !r)
	 { r = f(e, x);
	   e = e->tail;
	 }
     }
   return r;
 }

extern int hash_apply_key(hash_table *h, hash_vfunc *f, void *x)
 /* Call f(e->key, x) for every element in the table, as long as f returns 0.
    If f returns non-zero, hash_apply returns immediately with the same
    value. (Otherwise, hash_apply returns 0.)
    f may not modify the contents of the key unless HASH_ptr_is_key is set.
 */
 { uint j;
   hash_entry *e;
   int r = 0;
   for (j = 0; !r && j < h->size; j++)
     { e = &h->table[j];
       if (!e->key) continue;
       while (e && !r)
	 { r = f(e->key, x);
	   e = e->tail;
	 }
     }
   return r;
 }

extern int hash_apply_data(hash_table *h, hash_vfunc *f, void *x)
 /* Call f(e->data.p, x) for every element in the table, as long as f returns 0.
    If f returns non-zero, hash_apply returns immediately with the same
    value. (Otherwise, hash_apply returns 0.)
 */
 { uint j;
   hash_entry *e;
   int r = 0;
   for (j = 0; !r && j < h->size; j++)
     { e = &h->table[j];
       if (!e->key) continue;
       while (e && !r)
	 { r = f(e->data.p, x);
	   e = e->tail;
	 }
     }
   return r;
 }

extern int hash_apply_delete(hash_table *h, hash_func *f, void *x)
 /* Call f(e, x) for every element in the table.
    If f returns non-zero, delete the element from the table (without calling
    h->del_data).
    Return value is the number of elements that were deleted.
    f may not modify the hash table in any way, other than by
    returning non-zero or by modifying e->data.
 */
 { uint j, n = 0;
   hash_entry *e, *prev, *tmp;
   for (j = 0; j < h->size; j++)
     { e = &h->table[j];
       prev = 0;
       while (e && e->key)
	 { tmp = e->tail;
	   if (f(e, x))
	     { n++;
	       if (!IS_SET(h->flags, HASH_no_free_keys|HASH_ptr_is_key))
		 { free(e->key); }
	       e->key = 0;
	       if (prev)
		 { prev->tail = tmp;
		   free(e);
		   e = tmp;
		 }
	       else
		 { if (tmp)
		     { *e = *tmp;
		       free(tmp);
		     }
		 }
	     }
	   else
	     { prev = e;
	       e = tmp;
	     }
	 }
     }
   h->nr_entries -= n;
   if (!IS_SET(h->flags, HASH_no_auto_size) && h->size > HASH_MIN_SIZE
       && h->nr_entries <= h->size/2)
     { hash_set_size(h, h->size/2); }
   return n;
 }

/********** resizing *********************************************************/

static void insert_entry(hash_table *h, hash_entry *d, int use)
 /* Pre: d->key is not in h.
    Inserts d in h. If use, d itself should either be used or freed;
    otherwise, d should be copied. The values of key and data fields are just
    copied (no new memory for the key).
 */
 { uint x;
   hash_entry *e, *l;
   x = hash(h, d->key);
   e = &h->table[x];
   if (e->key)
     { if (!use)
	 { NEW(l);
	   *l = *d;
	 }
       else l = d;
       l->tail = e->tail;
       e->tail = l;
     }
   else
     { *e = *d;
       e->tail = 0;
       if (use) free(d);
     }
 }

extern int hash_set_size(hash_table *h, uint size)
 /* Pre: h has been initialized.
    Change size of h to size (rounded up to power of 2, subject to
    HASH_MIN_SIZE and HASH_MAX_SIZE).
    Return is 1, except if size could not be adjusted because there
    was not enough memory.
 */
 { hash_entry *e, *t, *d, *tmp;
   uint k, i, old_size;
   k = HASH_MIN_SIZE;
   if (size > HASH_MAX_SIZE) size = HASH_MAX_SIZE;
   while (k < size)
     { k = k << 1; }
   if (k == h->size) return 1;
   t = malloc(k*sizeof(*t)); /* no errors */
   if (!t) return 0;
   tmp = t; t = h->table; h->table = tmp;
   old_size = h->size; h->size = k;
   for (e = h->table, i = h->size; i; e++, i--)
     { e->key = 0; }
   for (e = t, i = old_size; i; e++, i--)
     { if (e->key)
	 { insert_entry(h, e, 0); }
     }
   for (e = t, i = old_size; i; e++, i--)
     { if (e->key)
	 { d = e->tail;
	   while (d)
	     { tmp = d->tail;
	       insert_entry(h, d, 1);
	       d = tmp;
	     }
	 }
     }
   free(t);
   return 1;
 }
