/*
errormsg.c: error message

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
#include "errormsg.h"

/* HINT: You can omit this file from the library and instead add
   a different source file that exports the same functionality (as
   declared in errormsg.h). E.g., this is useful in systems where
   stderr is not visible to the user.

   Alternatively, you can make a different library containing the
   alternative error/warning functions, then link with that library before
   linking with the library containing this file.

   Or you can simply include the alternative error/warning functions
   as source file in your project, before linking with the library.
*/

/*extern*/ const char *application_nm = 0;
  /* name used in error/warning messages. */

extern void error(const char *fmt, ...)
 /* Error message. "Error ...\n". exit(-1) */
 { va_list a;
   va_start(a, fmt);
   if (application_nm)
     { fprintf(stderr, "(%s) ", application_nm); }
   fprintf(stderr, "Error: ");
   vfprintf(stderr, fmt, a);
   putc('\n', stderr);
   va_end(a);
   /* I used to do an abort(), but that causes a core dump, which looks bad */
   exit(-1);
 }

extern void warning(const char *fmt, ...)
 /* Warning message: "Warning ...\n". */
 { va_list a;
   va_start(a, fmt);
   if (application_nm)
     { fprintf(stderr, "(%s) ", application_nm); }
   fprintf(stderr, "Warning: ");
   vfprintf(stderr, fmt, a);
   putc('\n', stderr);
   va_end(a);
 }

