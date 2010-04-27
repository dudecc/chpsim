/*
var_string.h: variable length strings.

Copyright (c) 1999 Marcel R. van der Goot
All rights reserved.

This file is only for use as part of software written by
Marcel van der Goot. Any other use requires a separate license.
This file contains confidential information, and should not be
redistributed to third parties.

*/

#ifndef VAR_STRING_H
#define VAR_STRING_H

#include "standard.h"
#include <string.h>
#include <ctype.h>

typedef struct var_string
   { int size;
     char *s;
   } var_string;

/* If a function returns a var_string *, then, unless specified otherwise,
   it is the argument that is the target of the function.
*/

extern var_string *var_str_init(var_string *s, int size);
 /* Allocate (at least) size chars for s->s.
    Must be called before s is used for anything else (size = 0 is ok).
    New string is empty.
 */

extern void var_str_free(var_string *s);
 /* Deallocate s->s. Can continue to use s. */

extern exception_class(var_str_err)
   { EXCEPTION_CLASS;
     int idx; /* if code == VAR_STR_ERR_idx */
   };
enum { VAR_STR_ERR_fmt, /* format error */
       VAR_STR_ERR_idx /* negative index */
     };

extern var_string *var_str_ensure(var_string *s, int i);
 /* Ensure that s->s[i] exists. Raises var_str_err/idx if i < 0. */

extern var_string *var_str_limit(var_string *s, int size);
 /* Limit s so that s->s takes at most size chars; this never grows s.
    If s is truncated, s->s[size-1] is set to 0 (if size > 0).
    size <= 0 ==> var_str_free(s).
 */

extern var_string *var_str_exact(var_string *s, int size);
 /* Grow or shrink s so that s->s takes size chars.
    s->s[size-1] is set to 0 (if size > 0).
    size = 0 ==> var_str_free(s); size = -1 ==> make s exactly fit current
    content.
 */

#define VAR_STR_X(S, I) \
    ((((S)->size <= (I)) ? var_str_ensure((S), (I)) : (S))->s[I])
 /* VAR_STR_X(var_string *s, int i);
    The same as s->s[i], but makes sure that index i actually exists.
    s and i are evaluated multiple times, so do not use
      VAR_STR_X(s, i++)    <-- BAD
 */

#define VAR_STR_LEN(S) (((S)->size)? strlen((S)->s) : 0)
 /* size_t VAR_STR_LEN(var_string *s);
    Return length of string. Same as strlen(s->s), except works if
    no space is allocated for s->s.
 */

#define VAR_STR_IS_EMPTY(S) (!(S)->size || !(S)->s[0])
 /* true if S is empty */

extern int var_str_slice_copy(var_string *s, int i, const char *t, int n);
 /* Overwrite s[i...] with the n chars from t[0..n).
    Special cases: i or n = -1 indicate whole string (+ the 0 for t).
    Warning: no 0 is written, unless it is part of t[0..n).
    Also, if i is beyond the original size of s, no particular values
    are written in the skipped space.
    This may cause an error if n > strlen(t)+1.
    Return is nr of chars copied, not counting final 0s.
 */

#define var_str_cat(S, T) var_str_slice_copy(S, -1, T, -1)

#define var_str_copy(S, T) var_str_slice_copy(S, 0, T, -1)

extern void var_str_swap(var_string *s, var_string *t);
 /* Swap s and t.
    This is much faster than copying.
 */

extern int var_str_vprintf(var_string *s, int i, const char *fmt, va_list a);
 /* Same as vsprintf(s->s+i, fmt, a), except s grows. Use i = -1 for append.
    A terminating 0 is added.
    Return is number of chars written (without the terminating 0).
    Errors in the fmt string cause a var_str_err/fmt exception.
 */

extern int var_str_printf(var_string *s, int i, const char *fmt, ...);
 /* Same as sprintf(s->s+i, fmt, ...), except s grows. Use i = -1 for append.
    A terminating 0 is added.
    Return is number of chars written (without the terminating 0).
    Errors in the fmt string cause a var_str_err/fmt exception.
 */

extern int var_str_gets(var_string *s, int i, FILE *f);
 /* Read a line from f into s[i...] (use i = -1 for append).
    A line ends at a '\n', which is always included in s (even if it was
    missing at eof). The line is 0 terminated.
    If no line could be read at all, s[i] = 0, and return is 0.
    Otherwise, return is the number of characters read (always including
    the '\n').
 */

/* var_str_printf() and var_str_vprintf() accommodate %v and %V conversions,
   which use a user-specified function to print an argument.
   These conversions normally take more than one argument:
     %v:  var_str_func_tp *f, void *a;
          calls f(s, i, a);
     %V: var_str_func2_tp *f, void *a1, void *a2;
	  calls f(s, i, a1, a2);
   In both cases, if the alternate flag is specified (%#v or %#V), the
   f argument should not be specified: instead, the f from the previous
   %v or %V is used. The first %v and first %V in a var_str_printf() or
   var_str_vprintf() call must not have the alternate flag set.

   Function f should print to s[i...], and add a terminating 0 (unless
   nothing is printed at all). The return value is ignored.
*/
typedef int var_str_func_tp(var_string *s, int i, void *a);
typedef int var_str_func2_tp(var_string *s, int i, void *a1, void *a2);


#endif /* VAR_STRING_H */
