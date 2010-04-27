/*
string_table.c:

Copyright (c) 1999 Marcel R. van der Goot
All rights reserved.

This file is only for use as part of software written by
Marcel van der Goot. Any other use requires a separate license.
This file contains confidential information, and should not be
redistributed to third parties.

*/

volatile static const char rcsid[] =
  "$Id: string_table.c,v 1.2 2004/04/13 18:34:53 marcel Exp $\0$Copyright: 1999 Marcel R. van der Goot $";

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

