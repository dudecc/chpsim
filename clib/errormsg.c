/*
errormsg.c: error message

Copyright (c) 1999 Marcel R. van der Goot
All rights reserved.

This file is only for use as part of software written by
Marcel van der Goot. Any other use requires a separate license.
This file contains confidential information, and should not be
redistributed to third parties.

*/

volatile static const char rcsid[] =
  "$Id: errormsg.c,v 1.1 2003/01/14 20:29:18 marcel Exp $\0$Copyright: 1999 Marcel R. van der Goot $";

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

