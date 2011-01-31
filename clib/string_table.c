/*
string_table.c:

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
#include "string_table.h"

static hash_table str_table;

extern str *make_str(const char *s)
 /* Make s into a str.
    The memory pointed to by s can be reused.
 */
 { hash_entry *q;
   hash_insert(&str_table, s, &q);
   LEAK(q->key);
   /* we don't know whether q was dynamically allocated, so we cannot
      do LEAK(q).
   */
   return q->key;
 }

extern void init_string_table(void)
 /* Call this before anything else in this file. (Multiple calls ok.) */
 { static int done = 0;
   if (!done)
     { hash_table_init(&str_table, 100, HASH_no_free_keys, 0); }
   LEAK(str_table.table);
   done = 1;
 }

