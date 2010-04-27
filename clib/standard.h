/*
standard.h: standard includes and definitions.

Copyright (c) 1999 Marcel R. van der Goot
All rights reserved.

This file is only for use as part of software written by
Marcel van der Goot. Any other use requires a separate license.
This file contains confidential information, and should not be
redistributed to third parties.

*/

#ifndef STANDARD_H
#define STANDARD_H

/* In this file, define WORD_16 for 16-bit machine.

   Define MALLOCDBG if you want the functionality of mallocdbg.h.

   Before including this file, you can
     define NOUINT to prevent definition of uint;
     define NOULONG to prevent definition of ulong;
     defining DONEUINT is equivalent to defining both NOUINT and NOULONG.
   After including this file, DONEUINT will be defined.
   You can also define NOOWNINT, in which case none of the integer
   types (ubyte, int2, etc.) are defined.

   It is assumed that sizeof returns the size in units of BYTE_NRBITS;
   see BYTE_NRBITS below.
*/

/********** standard include files *******************************************/

#include <stdarg.h> /* for vprintf etc. */
#include <stddef.h> /* for size_t, ptrdiff_t */
#include <limits.h> /* INT_MAX etc. */
#include <stdio.h>  /* almost always used */
#include <stdlib.h> /* almost always used */
#if defined(__unix__) && !defined(__FreeBSD__)
#include <alloca.h> /* for alloca; not an ansi header */
#endif
#include <assert.h>
#include "c_exception.h"
#include "errormsg.h"
extern char *strdup(const char *s); /* must be declared before mallocdbg.h */
#include "mallocdbg.h"

/* The other standard include files are not as often used, so they
   are not included here.

   Any fixes for problems with a particular architecture can be
   inserted here.
*/

/* The behavior of va_list after being passed to subroutines is undefined,
   but we rely on it anyway.  Using __va_copy when available makes this more
   reliable.
*/
#ifndef __va_copy
#define __va_copy(X,Y) (X)=(Y)
#endif

/********** standard types ***************************************************/

#ifndef NOOWNINT

/* integer types:
     ubyte, sbyte -- don't use for char
     int2, uint2
     int, uint   -- can be int2 or int4
     int4, uint4
     long, ulong -- largest; may be int4

   To print int4 x: printf("%ld", (long)x);
*/

typedef unsigned char ubyte;
typedef signed char sbyte;
typedef short int2;
typedef unsigned short uint2;

#ifndef BYTE_NRBITS
#ifdef _GCC_LIMITS_H_
#define BYTE_NRBITS CHAR_BIT
#else
#define BYTE_NRBITS 8
#endif
#endif /* BYTE_NRBITS */

#ifndef LONG_NRBITS
#define LONG_NRBITS (sizeof(long) * BYTE_NRBITS)
	/* bits in a long */
#endif

#ifdef WORD_16
typedef long int4;
typedef unsigned long uint4;
#else
typedef int int4;
typedef unsigned int uint4;
#endif /* WORD_16 */
#define INT4_MAX  (2147483647L)
  /* -1 is to avoid warning about 2147483648 being too large: */
#define INT4_MIN (-2147483647L - 1)
#define UINT4_MAX (4294967295UL)

#ifndef DONEUINT
#define DONEUINT
#ifndef NOUINT
typedef unsigned int uint;
#endif
#ifndef NOULONG
typedef unsigned long ulong;
#endif
#endif /* DONEUINT */

#endif /* !NOOWNINT */


/********** bit sets *********************************************************/
/* often used with flags stored as bits in an integer */

#define IS_SET(X, Y) ((X) & (Y))
  /* x \intersect y != {} */
#define IS_ALLSET(X, Y) (((X) & (Y)) == (Y))
  /* y \subseteq x, or, equivalently, x \intersect y == y */
#define SET_FLAG(X, Y) ((X) |= (Y))
#define RESET_FLAG(X, Y) ((X) &= ~(Y))
#define SET_IF_SET(X, Y, Z) ((X) |= (Y) & (Z))
#define ASSIGN_FLAG(X, Y, Z) ((X) = ((X) & ~(Z)) | ((Y) & (Z)))

#define FLAGS(X) \
        typedef enum X X; \
        enum X

#define FIRST_FLAG(X) X = 1
#define NEXT_FLAG(X) X, _MASK_FLAG_ ## X = (2*(X) - 1)

/********** generic operations ***********************************************/

#ifndef MIN
#define MIN(X, Y) (((X) <= (Y)? (X) : (Y)))
#endif
#ifndef MAX
#define MAX(X, Y) (((X) >= (Y)? (X) : (Y)))
#endif

#ifndef ABS
#define ABS(X) ((X) < 0? -(X) : (X))
#endif

#ifndef ODD
#define ODD(X) ((X) & 1)
#endif
#ifndef EVEN
#define EVEN(X) ((~((X) & 1)) & 1)
#endif

#define CONST_ARRAY_SIZE(A) (sizeof(A)/sizeof((A)[0]))
  /* only for true arrays, not pointers */

#define NO_INIT (0)
  /* use when no init is necessary, but you want to prevent a compiler
     warning about an unitialized variable.
  */

#define promoted_to_int(T) int
  /* use as 'type' (2nd) argument in va_arg() if it is a type that
     is passed promoted as int.
  */


/********** memory allocation ************************************************/

/* Preferably use the statements (not functions)
     NEW(P)
     NEW_ARRAY(P, N)
     REALLOC_ARRAY(P, N)
     STACK_ARRAY(P, N)
   Each assigns to P, which must have the proper type. N is the number of
   array elements. STACK_ARRAY allocates on the stack instead of the heap,
   with alloca(). This is much faster, but maybe not standard. (Do not
   free() a stack_array!)

   If you cannot use a P of the proper type, use the statements (not functions)
     MALLOC(P, S)
     REALLOC(P, S)
     ALLOCA(P, S)

   All statements check whether allocation succeeded, and raise an
   except_malloc exception if not.
   The REALLOC statements evaluate P twice; don't use N or S = 0 to free
   using REALLOC.
   All functions evaluate N or S twice in case of an error only.

   There is also
     FREE(P)
   which is like free(P) but also sets P to 0xBAD. This evaluates P twice.
*/

#define NEW(P) MALLOC((P), sizeof(*(P)))

#define NEW_ARRAY(P, N) MALLOC((P), (N)*sizeof(*(P)))

#define REALLOC_ARRAY(P, N) REALLOC((P), (N)*sizeof(*(P)))

#define STACK_ARRAY(P, N) ALLOCA((P), (N)*sizeof(*(P)))

#define ALLOCA(P, S)  if (!((P) = alloca(S))) \
   except_Raise(except_malloc, 0, "%s = alloca(%lu) failed: out of memory?", \
	 	#P, except_malloc->size = (unsigned long)(S))

#define REALLOC(P, S) if (!((P) = realloc((P), (S)))) \
   except_Raise(except_malloc, 0, "realloc(%s, %lu) failed: out of memory.", \
		#P, except_malloc->size = (unsigned long)(S))

#define MALLOC(P, S) if (!((P) = malloc(S))) \
   except_Raise(except_malloc, 0, "%s = malloc(%lu) failed: out of memory.", \
		#P, except_malloc->size = (unsigned long)(S))

#define FREE(P) (free(P), (P) = (void*)0xBAD)

/********** files ************************************************************/

/* FOPEN(F, name, mode) is the same as F = fopen(name, mode), but will
   give an abortive error if it fails to open the file.
*/

#define FOPEN(F, N, M) if (!(F = fopen(N, M))) \
   error("Cannot open file %s with mode %s.", N, M)


/********** inlining *********************************************************/

/* If inlining is possible, define USE_INLINE. Then define the following
   as appropriate qualifiers:
     INLINE_H : "inline extern" preceding a function def in a .h file;
     INLINE_C : "inline extern" preceding a function def in a .c file;
     INLINE_EXTERN_PROTO: "inline extern" preceding a prototype in a .h file;
     INLINE_STATIC: "inline static" preceding a function def in a .c file;
     INLINE_STATIC_PROTO: "inline static" preceding a prototype in a .c file;
   If the compiler does not support inlining, define the last four
   as appropriate for non-inlined functions.

   File abc.h with inline extern function f() should contain
        INLINE_EXTERN_PROTO f();
	...
	#if defined(USE_INLINE) || defined(INLINE_ABC_C)
	#undef INLINE_EXTERN
	#ifdef INLINE_ABC_C
	#define INLINE_EXTERN INLINE_C
	#else
	#define INLINE_EXTERN INLINE_H
	#endif
	INLINE_EXTERN f(){...}
	...
	#endif USE_INLINE
   The abc.c file should contain:
	#define INLINE_ABC_C
	#include "abc.h"
   This way we don't need to repeat the function definitions in both abc.h
   and abc.c (the function defs need to be in the abc.c file to generate
   non-inlined translations of the functions).

   Example: for gcc, use
	#define USE_INLINE
	#define INLINE_H __inline__ extern
	#define INLINE_C __inline__
	#define INLINE_EXTERN_PROTO extern
	#define INLINE_STATIC __inline__ static
	#define INLINE_STATIC_PROTO __inline__ static

   You can disable inlining when including this file by using -DNO_USE_INLINE.
   Note that the compiler may need extra options to actually inline. (E.g.,
   gcc only inlines with -O.)
*/

#ifndef NO_USE_INLINE
#define USE_INLINE
#endif

#ifdef USE_INLINE
/* for gcc */
#define INLINE_H  __inline__ extern
#define INLINE_C  __inline__
#define INLINE_EXTERN_PROTO extern
#define INLINE_STATIC  __inline__ static
#define INLINE_STATIC_PROTO __inline__ static

#else /* no inlining supported */
#define INLINE_C extern
#define INLINE_EXTERN_PROTO extern
#define INLINE_STATIC static
#define INLINE_STATIC_PROTO static

#endif /* USE_INLINE */


/*****************************************************************************/

#endif /* STANDARD_H */
