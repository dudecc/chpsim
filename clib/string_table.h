/*
string_table.h: strings with unique pointer.

Copyright (c) 1999 Marcel R. van der Goot
All rights reserved.

This file is only for use as part of software written by
Marcel van der Goot. Any other use requires a separate license.
This file contains confidential information, and should not be
redistributed to third parties.

*/

#ifndef STRING_TABLE_H
#define STRING_TABLE_H

/* A str* is just a char*, but identifiable by the pointer value. I.e.,
   if str *x and str *y point to the same string, then x == y. This is
   achieved by storing all str values in a hash table.
   Pointers should always be of type
      const str *
   because you are not allowed to change the contents of the memory
   where the string is stored. Always use "str" instead of "char", so you
   remember when pointer comparison is valid, and also to remind you
   not to deallocate the string.

   Using str pointers instead of char pointers is of course particularly
   convenient if you must often compare strings for equality. It can also save
   memory if you need to store the same string multiple times, and is
   in general a convenient way of allocating space for strings. Note however
   that the space cannot be deallocated, which may be a problem.
*/

typedef char str;

extern void init_string_table(void);
 /* Call this before anything else in this file. (Multiple calls ok.) */

extern str *make_str(const char *s);
 /* Make s into a str.
    The memory pointed to by s can be reused.
 */


#endif /* STRING_TABLE_H */
