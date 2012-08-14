/*
mallocdbg.h: Functions replacing malloc and free to check for consistency
   of dynamically allocated data.

Copyright (c) 1996 Marcel R. van der Goot
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

#ifndef MALLOCDBG_H
#define MALLOCDBG_H

/* Usage:
        #define MALLOCDBG
	#include "mallocdbg.h"
   This must be done after possible declarations of malloc and free,
   i.e., after inclusion of <stdlib.h>.
   If MALLOCDBG is not defined, then the functionality of this file is
   not enabled.

   Functionality:

	void *malloc(unsigned long);
	void *calloc(unsigned long);
	void free(void *);
	void *realloc(void *, unsigned long);

   are used as normal. They have been redefined
   to perform some consistency checks; in particular, they check
   that the space just before and after the allocated data has not
   been overwritten. In order to check that freed pointers are not used
   anymore, free() does not actually free the memory, so that the total
   amount of memory used with these functions may be significantly larger
   than with the normal malloc and free. Likewise, realloc() allocates
   new space, then marks the old space as freed.

	void check_malloc(void);

   Verifies that all data blocks allocated so far are consistent.

	type *USE_PTR(type *p);

   Function that verifies that p is consistent and not free. Returns
   p, so that you can replace code like p->n = 5 by USE_PTR(p)->n = 5.
   This is a macro which evaluates p twice! Note: this should only be
   used for pointers allocated with malloc, not for addresses of variables
   on the stack.

	void *use_ptr(void *p);

   Same functionality as USE_PTR(p), but return is a void *, so must be cast
   to proper type before dereferencing. Evaluates p only once.

	FILE *mallocdbg_log;

   If you assign this pointer, malloc(), free(), and  use_ptr() will write
   a log of all their calls to this file, so you can determine where a
   particular pointer value came from. While this may seem excessive, this
   file turns out to be very convenient during debugging; its use is
   recommended. Messages in the file have the form
        ptr  function  source[line] 
   To trace a particular pointer, run this file through the 'sort'
   program (use LC_COLLATE=C in the locale). Error messages are logged as well.

   There is a 'leaks' utility that reads the log file to determine memory
   leaks. It will identify malloc calls whose memory was never freed.
   Some memory is needed for the duration of the program; is seems
   pointless to add code to free such memory when the program exits. You
   can indicate such intentional leaks with LEAK(p). The 'leaks' utility
   will not report that memory as a leak. (Currently, there is no runtime
   check to verify that intentional leaks are indeed not freed.)

   While it is possible to mix the normal malloc and free with these debug
   versions in the same program (in different object files), you should
   take care that pointers are always allocated and freed with the
   same version, either both normal or both debug versions. Note that
   these debug versions are implemented using macros, not by actually
   replacing malloc etc.. Hence, they do not affect files that are already
   compiled; in particular, they do not affect standard libraries. As a
   design principle, if a library routine allocates memory, the same library
   should provide a function to deallocate the memory, rather than asking
   the user to call free() (this provides better abstraction, and keeps the
   door open for future changes). In cases where this principle has been
   violated, you can call no_dbg_free() to get the normal free() function.
   An exception is strdup(), which has a debug version defined in this
   file. Hence, use free(), not no_dbg_free(), for memory returned by strdup().

   Malloc initializes the allocated data with a repetition of 0xBAD1. This
   may help you catch cases where you forget to initialize fields.
   When a pointer is freed, the data is overwritten with 0xBAD2 (this
   is verified during consistency checking). (If the data is a string,
   on a Latin-1 terminal 0xBAD1BAD1 looks sort of like "NoNo" or "oNoN",
   0xBAD2BAD2 looks sort of like "OoOo" or "oOoO".)

   The error messages all have the form

      (E) source[line] function(args): ...

   where source and line identify where the function(args) call occurred.
   Checking for corruption is done by storing a specific (non-obvious)
   number immediately before and immediately after the allocated data.

   The consistency of data is verified by

      free(p), USE_PTR(p), use_ptr(p), realloc(p) : for p only
      check_malloc() : all allocated data

   Some possible error messages:

   size = 0 : allocation of this size is not allowed; if later this size
	is found, obviously something went wrong.
   null pointer : you cannot dereference or free a null pointer.
   ptr[...] corrupted : first position found before or after the data, or
	in free'd data, which has been corrupted.
   size > max_size : something has been corrupted. (size refers to the
	stored size, max_size is the largest size which has been allocated.)
   found ... elements instead of ... elements : indicates field before
        allocated data has been overwritten (this is unlikely to happen
	without other detected inconsistencies). (The message compares
	the number of elements found with the number of elements that
	is known to have been allocated.)

   realloc() calls malloc and free, so any error messages may refer to
   those functions rather than to realloc.

   When it finishes, check_malloc() prints some informative messages. These
   These do not indicate errors, and don't start with "(E)".

   These functions do not check every corruption possible. In particular,
   they only check blocks which have actually been allocated, not other
   parts of the heap. Also, they only check the area immediately surrounding
   the allocated data. On the other hand, it seems unlikely that a corruption
   of that area would go undetected.

   Note: malloc and free are macros; in the unlikely case that you need
   their addresses, see below for the actual functions, or write a wrapper
   function around them.
*/

#ifdef MALLOCDBG
#define malloc(S) dbg_malloc(__FILE__, __LINE__, S, 0)
#define calloc(S) dbg_malloc(__FILE__, __LINE__, S, 1)
#define free(P) dbg_free(__FILE__, __LINE__, P)
#define realloc(P, S) dbg_realloc(__FILE__, __LINE__, P, S)
#define check_malloc() check_dbg_malloc(__FILE__, __LINE__)
#define USE_PTR(P) ((void)dbg_use_ptr(__FILE__, __LINE__, P), P)
#define use_ptr(P) dbg_use_ptr(__FILE__, __LINE__, P)
#define LEAK(P) dbg_leak(__FILE__, __LINE__, P)

extern void *dbg_malloc(const char *src, int ln, long s, int z);
extern void dbg_free(const char *src, int ln, void *q);
extern void *dbg_realloc(const char *src, int ln, void *q, long s);
extern void check_dbg_malloc(const char *src, int ln);
extern void *dbg_use_ptr(const char *src, int ln, void *q);
extern void dbg_leak(const char *src, int ln, void *q);
extern void (*no_dbg_free)(void *);
extern char* (*no_dbg_strdup)(const char *);

#define strdup(S) dbg_strdup(__FILE__, __LINE__, S)
extern char *dbg_strdup(const char *src, int ln, const char *s);

#else

#define check_malloc()
#define USE_PTR(P) (P)
#define use_ptr(P) (P)
#define LEAK(P)
#define no_dbg_free free
#define no_dbg_strdup strdup

#endif /* MALLOCDBG */

extern void *mallocdbg_log; /* really: FILE *mallocdbg_log */

#endif /* MALLOCDBG_H */
