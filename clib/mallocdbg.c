/*
mallocdbg.c: Functions replacing malloc and free to check for consistency
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

/* prevent inclusion of mallocdbg.h, because then we lose the
   original malloc:
*/
#define MALLOCDBG_H
#include "standard.h"

/* void *dbg_malloc(unsigned long);
   void dbg_free(void *);
   void check_dbg_malloc(void);

   The first two functions replace malloc() and free(). They check that
   the data immediately before and after the allocated area is not corrupted.
   dbg_free() performs this check for the element which is deallocated;
   check_dbg_malloc checks it for all blocks allocated so far.
   dbg_malloc calls ACTUALMALLOC() to allocate the actual memory.
   dbg_free only checks, but does not actually deallocate.
*/

#ifndef ACTUALMALLOC
#define ACTUALMALLOC malloc
#endif

#ifndef ACTUALFREE
#define ACTUALFREE free
#endif

#ifndef ACTUALSTRDUP
#define ACTUALSTRDUP strdup
#endif

/*extern*/ void (*no_dbg_free)(void *) = ACTUALFREE;
/*extern*/ char* (*no_dbg_strdup)(const char *) = ACTUALSTRDUP;

/* Internal representation:

   For a call dbg_malloc(s), the following data is allocated. The
   return value is q (data is the user's data).
   The /// parts indicate padding. The padding is necessary to make
   sure that q satisfies the strongest alignment requirements
   possible (because the return value of malloc must satisfy that).
   Note that the long field with p following the data may not be properly
   aligned for a long. Such alignment might require separation of this
   field from the data by padding; we want this field to be directly
   adjacent to the data. (Since q is properly aligned for a long, the
   long field with p immediately before q is also properly aligned.)
   The two fields containing p are to check for consistency: if they
   are incorrect, the user must have written outside the allocated
   area. The tail field is used to make a linked list of all allocated
   memory (the starting point is all_ptr). If a pointer is freed, the
   data is overwritten with 0xBAD2, which is also used for consistency
   checking. In case of a freed pointer, p[1] is negated.
   Global variables are used to keep track of the maximum size block
   that has been allocated, as well as the nr of blocks allocated and
   freed. malloc() initializes the data to 0xBAD1, to help catch fields
   that are unitialized.
   

        extra_size
    <---------------->
      long[2]     long     byte[s]   long
    <-------->     <-> <-----------> <->
    ____________________________________
   |_tail_|_s_|///|_p_|_____data____|_p_|
   ^  ^     ^         ^             ^
   |  |     |         |             |
   p p[0]  p[1]       q             q+s

   long *p; byte *q;
*/


static void *all_ptr = 0;
static unsigned long max_size = 0;
static unsigned long nr_alloc = 0, nr_free = 0;
static unsigned long curr_usage = 0, max_usage = 0;
static long extra_size = 0;
static long bad1, bad2;

/*extern*/ void *mallocdbg_log = 0;

static void malloc_error(const char *src, int ln, const char *fmt, ...)
 /* Print error msg to stderr and log file; adds newline.
    If !src, then the message is printed without "(E)".
 */
 { va_list a;
   FILE *log = mallocdbg_log;
   va_start(a, fmt);
   if (src) fprintf(stderr, "(E) %s[%d] ", src, ln);
   vfprintf(stderr, fmt, a);
   putc('\n', stderr);
   fflush(stderr);
   va_end(a);
   if (log)
     { va_start(a, fmt);
       if (src) fprintf(log, "(E) %s[%d] ", src, ln);
       else putc('\t', log);
       vfprintf(log, fmt, a);
       putc('\n', log);
       fflush(log); /* in case program crashes later */
       va_end(a);
     }
 }

static void malloc_log(const char *src, int ln, void *p, const char *fmt, ...)
 /* Print msg to log file; adds newline. p should be the relevant pointer.
    If !src, then src, ln, and p are omitted.
 */
 { va_list a;
   FILE *log = mallocdbg_log;
   va_start(a, fmt);
   if (log)
     { if (src)
         { fprintf(log, "%p\t", p); }
       else
	 { putc('\t', log); }
       vfprintf(log, fmt, a);
       if (src)
         { fprintf(log, "\t%s[%d]", src, ln); }
       putc('\n', log);
       fflush(log); /* in case program crashes later */
     }
   va_end(a);
 }

typedef struct
   { char x;
     long y;
   } align_long;

typedef struct
   { char x;
     long double y;
   } align_long_double;

static void compute_extra(void)
 /* set extra_size to the smallest number X so that
    - X >= 3*sizeof(long)
    - X satisfies the strongest possible alignment requirement
      (alignment(tp) = A if every tp* must be a multiple of A;
      hence, we want X to be a multiple of alignment(strongest).)
    It is assumed that either long or long double has the strongest
    possible alignment.
 */
 { long a;
   a = MAX((long)&(((align_long*)0)->y), (long)&(((align_long_double*)0)->y));
   extra_size = 3*sizeof(long);
   if (extra_size % a)
     { extra_size = (extra_size/a + 1) * a; }
   bad1 = bad2 = 0;
   a = 0;
   while (a >= 0)
     { a = (a << 16) | 0xffff;
       bad1 = (bad1 << 16) | 0xbad1;
       bad2 = (bad2 << 16) | 0xbad2;
     }
   malloc_log(0, 0, 0, "(set LC_COLLATE=C when sorting this file)");
   malloc_log(0, 0, 0, "(extra_size = %ld)\n", extra_size);
 }

static void copy_long(char *p, long n, long x)
 /* fill p[0..n) with copies of x. The last copy may be partial. */
 { int i=0, j=0;
   char *q;
   q = (char*)&x;
   while (i < n)
     { if (j == sizeof(long))
	 { j = 0; q = (char*)&x; }
       *p++ = *q++;
       i++; j++;
     }
 }

static long check_long(char *p, long n, long x)
 /* verify that p[0..n) is filled with copies of x.
    Return -1 if correct, otherwise return index of first wrong character
 */
 { int i=0, j=0;
   char *q;
   q = (char*)&x;
   while (i < n)
     { if (j == sizeof(long))
	 { j = 0; q = (char*)&x; }
       if (*p != *q)
	 { return i; }
       p++; q++;
       i++; j++;
     }
   return -1;
 }

extern void check_dbg_malloc(const char *src, int ln);

extern void *dbg_malloc(const char *src, int ln, long s, int z)
 /* source file, line number, nr of bytes; z => fill with 0 */
 { long *p, n;
   char *q, *tmp;
   if (!extra_size) compute_extra();
   if (s == 0)
     { malloc_error(src, ln, "malloc(0): called with size 0");
       return 0;
     }
   if (s < 0 )
     { malloc_error(src, ln, "malloc(%lu): called with size %lu, "
		    "too large for this routine", s, s);
       return 0;
     }
   /* allocate: long[3] + padding + s + sizeof(long) =
                    extra_size    + s + sizeof(long)
   */
   p = ACTUALMALLOC(extra_size + s + sizeof(long));
   if (!p)
     { malloc_error(src, ln, "malloc(%lu): ran out of memory.", s);
       check_dbg_malloc(src, ln);
       return 0;
     }
   p[0] = (long)all_ptr;
   p[1] = s;
   q = (char*)p + extra_size;
   copy_long(q - sizeof(long), sizeof(long), (long)p);
   copy_long(q + s, sizeof(long), (long)p);
   if (max_size < s) max_size = s;
   all_ptr = p;
   nr_alloc++;
   curr_usage += s;
   if (curr_usage > max_usage) max_usage = curr_usage;
   if (z)
     { tmp = q;
       for (n = 0; n < s; n++, tmp++)
	 { *tmp = 0; }
       malloc_log(src, ln, q, "   calloc(%lu)", s);
     }
   else
     { copy_long(q, s, bad1);
       malloc_log(src, ln, q, "   malloc(%lu)", s);
     }
   return q;
 }

static void verify(const char *src, int ln, const char *s, long *p)
 /* source file, line number, calling function, pointer */
 { long n, e;
   char *q;
   q = (char*)p + extra_size;
   n = ABS(p[1]);
   if (n > max_size)
     { malloc_error(src, ln, "%s(%p): size = %ld > %ld (max_size)",
		    s, q, n, max_size);
     }
   if (n == 0)
     { malloc_error(src, ln, "%s(%p): size = 0", s, q); }
   e = check_long(q - sizeof(long), sizeof(long), (long)p);
   if (e != -1)
     { malloc_error(src, ln, "%s(%p): p[%ld] (before data) corrupted",
		    s, q, e-sizeof(long));
     }
   if (p[1] < 0)
     { e = check_long(q, n, bad2);
       if (e != -1)
	 { malloc_error(src, ln, "%s(%p): freed block of size %ld corrupted\n"
			"\tstarting at p[%ld]",
			s, q, n, e);
	 }
     }
   e = check_long(q+n, sizeof(long), (long)p);
   if (e != -1)
     { malloc_error(src, ln, "%s(%p): p[%ld] (after data) corrupted",
		    s, q, n+e);
     }
 }

extern void dbg_free(const char *src, int ln, void *q)
 /* Source file, line number, pointer */
 /* Note that dbg_free() only checks, but does not really deallocate. If you
    want to do that, you must make the list of blocks a doubly-linked list.
 */
 { long *p, n;
   if (!q)
     { malloc_error(src, ln, "free(0): null pointer");
       return;
     }
   if (!extra_size) compute_extra();
   p = (long*)((char*)q - extra_size);
   n = p[1];
   if (n < 0)
     { malloc_error(src, ln, "free(%p): %p is freed more than once", q, q); }
   verify(src, ln, "free", p);
   if (n > 0)
     { p[1] = -n;
       copy_long(q, n, bad2);
     }
   nr_free++;
   curr_usage -= n;
   malloc_log(src, ln, q, "free()  ");
 }

extern void *dbg_realloc(const char *src, int ln, void *vq, long s)
 /* source file, line number, pointer, nr of bytes */
 /* Note: error messages may refer to malloc or free. */
 { char *t, *q = vq;
   long *p, n, i;
   if (!q)
     { return dbg_malloc(src, ln, s, 0); }
   if (!extra_size) compute_extra();
   p = (long*)(q - extra_size);
   n = p[1];
   if (n < 0)
     { malloc_error(src, ln, "realloc(%p, %ld): %p has been freed", q, s, q);
       return dbg_malloc(src, ln, s, 0);
     }
   t = dbg_malloc(src, ln, s, 0);
   if (n > s) n = s;
   for (i = 0; i < n; i++)
     { t[i] = q[i]; }
   malloc_log(src, ln, q, " realloc=%p", t);
   dbg_free(src, ln, q);
   return t;
 }

extern void check_dbg_malloc(const char *src, int ln)
 /* source file, line number */
 { long *p;
   unsigned long na = 0, nf = 0, asz = 0, fsz = 0;
   p = all_ptr;
   malloc_error(0, 0, "\n________________________________________");
   malloc_error(0, 0, "%s[%d] check_dbg_malloc: checking ...", src, ln);
   if (!extra_size) compute_extra();
   while (p)
     { verify(src, ln, "check_dbg_malloc", p);
       na++;
       if (p[1] < 0)
	 { asz -= p[1]; fsz -= p[1]; nf++; }
       else
	 { asz += p[1]; }
       p = (long*)p[0];
     }
   if (na != nr_alloc)
     { malloc_error(src, ln,
		    "check_dbg_malloc: found %lu elements instead of %lu",
		    na, nr_alloc);
     }
   if (nf != nr_free)
     { malloc_error(src, ln, 
		    "check_dbg_malloc: found %lu free elements instead of %lu",
		    nf, nr_free);
     }
   if (asz - fsz != curr_usage)
     { malloc_error(src, ln,
	            "check_dbg_malloc: found usage of %lu instead of %lu",
		    asz - fsz, curr_usage);
     }
   malloc_error(0, 0, "%s[%d] check_dbg_malloc: done\n"
		"\t(%lu elements total, max size = %lu, %lu elements freed,\n"
		"\t max usage: %lu bytes\n"
		"\t current usage: %lu - %lu = %lu bytes)",
		src, ln, na, max_size, nf, max_usage, asz, fsz, asz - fsz);
   malloc_error(0, 0, "________________________________________");
 }

extern void *dbg_use_ptr(const char *src, int ln, void *q)
/* Source file, line number, pointer */
 { long *p, n;
   if (!q)
     { malloc_error(src, ln, "use_ptr(0): null pointer");
       return q;
     }
   if (!extra_size) compute_extra();
   p = (long*)((char*)q - extra_size);
   n = p[1];
   if (n < 0)
     { malloc_error(src, ln, "use_ptr(%p): %p has been freed", q, q);
     }
   verify(src, ln, "use_ptr", p);
   malloc_log(src, ln, q, "  use_ptr");
   return q;
 }

extern void dbg_leak(const char *src, int ln, void *q)
 /* report this as an intentional leak in the log file */
 { dbg_use_ptr(src, ln, q);
   malloc_log(src, ln, q, " leak   ");
 }

extern char *dbg_strdup(const char *src, int ln, const char *s)
 /* create new copy of s */
 { long n;
   char *t, *r;
   if (!s)
     { malloc_error(src, ln, "strdup(0): null pointer");
       return 0;
     }
   for (t = (char*)s, n = 1; *t; t++, n++)
     { }
   r = t = dbg_malloc(src, ln, n, 0);
   if (!t) return 0;
   while (*s)
     { *t++ = *s++; }
   *t = 0;
   return r;
 }
