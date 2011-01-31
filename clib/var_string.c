/*
var_string.c: variable length strings.

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
#include "var_string.h"

/* Note: Maybe it would be better if size were of type size_t, but
   that might complicate VAR_STR_X, and also it is more convenient if you
   can use -1 as special value for var_str_slice_copy() etc.
*/
/* Note: We cannot have a version which allocates on the stack, because
   realloc() wouldn't work.
*/
/* Note: We assume that REALLOC_ARRAY(s, n) where *s == 0, is the same as
   NEW_ARRAY(s, n). We don't use REALLOC with n = 0.
*/

#define VAR_STR_INC 100 /* extra increment to avoid too many mallocs */

def_exception_class(var_str_err);


/********** allocation *******************************************************/

extern var_string *var_str_init(var_string *s, int size)
 /* Allocate (at least) size chars for s->s.
    Must be called before s is used for anything else (size = 0 is ok).
    New string is empty.
 */
 { if (size > 0)
     { s->size = size;
       NEW_ARRAY(s->s, size);
       s->s[0] = 0;
     }
   else
     { s->size = 0;
       s->s = 0;
     }
   return s;
 }

extern void var_str_free(var_string *s)
 /* Deallocate s->s. Can continue to use s. */
 { if (s->size)
     { s->size = 0;
       FREE(s->s);
       s->s = 0;
     }
 }

extern var_string *var_str_ensure(var_string *s, int i)
 /* Ensure that s->s[i] exists. Raises var_str_err/idx if i < 0. */
 { if (i < 0)
     { var_str_err->idx = i;
       except_Raise(var_str_err, VAR_STR_ERR_idx,
                    "var_str_ensure(%p, %d): negative index.", (void*)s, i);
     }
   if (s->size <= i)
     { s->size += VAR_STR_INC;
       if (s->size <= i) s->size = i + 1;
       REALLOC_ARRAY(s->s, s->size);
     }
   return s;
 }

extern var_string *var_str_limit(var_string *s, int size)
 /* Limit s so that s->s takes at most size chars; this never grows s.
    If s is truncated, s->s[size-1] is set to 0 (if size > 0).
    size <= 0 ==> var_str_free(s).
 */
 { if (size <= 0)
     { var_str_free(s); }
   else if (s->size > size)
     { s->size = size;
       REALLOC_ARRAY(s->s, s->size);
       s->s[size-1] = 0;
     }
   return s;
 }

extern var_string *var_str_exact(var_string *s, int size)
 /* Grow or shrink s so that s->s takes size chars.
    s->s[size-1] is set to 0 (if size > 0).
    size = 0 ==> var_str_free(s); size = -1 ==> make s exactly fit current
    content.
 */
 { if (size < 0)
     { if (s->size)
         { size = strlen(s->s) + 1; }
       else
         { return s; }
     }
   if (size == 0)
     { var_str_free(s);
       return s;
     }
   else if (s->size != size)
     { s->size = size;
       REALLOC_ARRAY(s->s, size);
     }
   s->s[size-1] = 0;
   return s;
 }


/********** copying **********************************************************/

extern int var_str_slice_copy(var_string *s, int i, const char *t, int n)
 /* Overwrite s[i...] with the n chars from t[0..n).
    Special cases: i or n = -1 indicate whole string (+ the 0 for t).
    Warning: no 0 is written, unless it is part of t[0..n).
    Also, if i is beyond the original size of s, no particular values
    are written in the skipped space.
    This may cause an error if n > strlen(t)+1.
    Return is nr of chars copied, not counting final 0s.
 */
 { if (i < 0)
     { i = VAR_STR_LEN(s); }
   if (n < 0)
     { n = strlen(t) + 1; }
   else if (n == 0)
     { return 0; }
   if (s->size < i+n) var_str_ensure(s, i+n-1);
   strncpy(s->s + i, t, n);
   n--;
   while (n && !t[n]) n--;
   if (t[n]) return n+1;
   else return 0;
 }

extern void var_str_swap(var_string *s, var_string *t)
 /* Swap s and t.
    This is much faster than copying.
 */
 { var_string k;
   k = *s;
   *s = *t;
   *t = k;
 }


/********** printf ***********************************************************/

/* This has gotten excessively complicated, but that is due to the
   complexity of the printf conversions.

   Note: The GNU C library contains functions asprintf() and
   parse_printf_format() that could have been used for this; those were
   not available (to me, at least) when I first wrote these functions.
   Also, that would limit portability to Gnu-based systems.
*/

typedef struct printf_conv
   { int left_adjust;
     int add_sign;
     int add_space;
     int alternate;
     int pad; /* '0', ' ' */
     int min_wd;
     int precision;
     int modifier; /* 0, 'h', 'l', 'L' */
     int skip; /* number int arguments used */
   } printf_conv;

/* The code below assumes that if you have a
     va_list arg;
   and pass it to a function
     f(arg);
   then va_arg(arg, type) calls in f() have no effect on what arg points
   to after returning from f.
   I don't know whether that is a generally valid assumption.
   (Note that it is actually inconvenient in this case.)
*/

static int int_field(char *tmp, int *i, const char **fmt, va_list a, int *skip)
 /* read integer (which may be '*') as part of format description */
 { int x;
   if (**fmt == '*')
     { x = va_arg(a, int);
       if (x <= 0)
         { x = 0; }
       else
         { sprintf(tmp + *i, "%d", x);
           while (tmp[*i]) (*i)++;
         }
       (*fmt)++;
       (*skip)++;
     }
   else
     { x = atoi(*fmt);
       while (isdigit(**fmt))
         { tmp[(*i)++] = *(*fmt)++; }
     }
   return x;
 }

static int printf_conversion
 (char *tmp, printf_conv *p, const char *fmt, va_list a)
 /* Pre: *fmt == '%'.
    Read conversion spec from fmt, fill in p and write it to tmp.
    Return is position in fmt of the conversion character.
    Afterwards, p->skip int arguments should be skipped.
 */
 { int done, i;
   const char *orig_fmt = fmt;
   va_list b;
   p->left_adjust = p->add_sign = p->add_space = p->alternate = 0;
   p->pad = ' ';
   p->min_wd = 0;
   p->precision = 0;
   p->modifier = 0;
   p->skip = 0;
   tmp[0] = '%'; i = 1; fmt++;
   done = 0;
   while (*fmt && !done)
     { switch (*fmt)
         { case '-':
             p->left_adjust = 1; break;
           case '+':
             p->add_sign = 1; break;
           case ' ':
             p->add_space = 1; break;
           case '#':
             p->alternate = 1; break;
           case '0':
             p->pad = 0; break;
           default: done = 1; break;
         }
       if (!done) tmp[i++] = *fmt++;
     }
   tmp[i] = 0;
   if (!fmt)
     { except_Raise(var_str_err, VAR_STR_ERR_fmt,
                    "Missing conversion character: \"%s\"", tmp);
     }
   if (isdigit(*fmt) || *fmt == '*')
     { __va_copy(b, a);
       p->min_wd = int_field(tmp, &i, &fmt, b, &p->skip);
       va_end(b);
       if (p->skip)
         { (void)va_arg(a, int); }
     }
   if (*fmt == '.')
     { tmp[i++] = *fmt++;
       if (isdigit(*fmt) || *fmt == '*')
         { p->precision = int_field(tmp, &i, &fmt, a, &p->skip); }
     }
   if (*fmt == 'h' || *fmt == 'l' || *fmt == 'L')
     { p->modifier = *fmt;
       tmp[i++] = *fmt++;
     }
   tmp[i] = 0;
   if (!isalpha(*fmt) && *fmt != '%')
     { except_Raise(var_str_err, VAR_STR_ERR_fmt,
                    "Unknown conversion: \"%s\"", tmp);
     }
   tmp[i++] = *fmt;
   tmp[i] = 0;
   return fmt - orig_fmt;
 }

extern int var_str_vprintf(var_string *s, int i, const char *fmt, va_list a)
 /* Same as vsprintf(s->s+i, fmt, a), except s grows. Use i = -1 for append.
    A terminating 0 is added.
    Return is number of chars written (without the terminating 0).
    Errors in the fmt string cause a var_str_err/fmt exception.
 */
 { char tmpfmt[1000], tmp[1000];
   printf_conv p;
   int orig_i, *n, len;
   const char *str_arg;
   void *a1, *a2;
   va_list b;
   var_str_func_tp *user_func = 0;
   var_str_func2_tp *user_func2 = 0;
   if (i < 0)
     { i = VAR_STR_LEN(s); }
   orig_i = i;
   while (*fmt)
     { if (*fmt != '%')
         { VAR_STR_X(s, i) = *fmt;
           i++; fmt++;
         }
       else
         { __va_copy(b, a);
           fmt += printf_conversion(tmpfmt, &p, fmt, b);
           va_end(b);
           while (p.skip)
             { (void)va_arg(a, int);
               p.skip--;
             }
           if (*fmt == '%')
             { VAR_STR_X(s, i) = '%';
               i++; fmt++;
             }
           else if (*fmt == 'n')
             { n = va_arg(a, int*);
               if (p.modifier == 'l')
                 { *(long*)n = i - orig_i; }
               else if (p.modifier == 'h')
                 { *(short*)n = i - orig_i; }
               else
                 { *n = i - orig_i; }
               fmt++;
             }
           else if (*fmt == 's')
             { if (p.modifier == 'l')
                 { except_Raise(var_str_err, VAR_STR_ERR_fmt,
                                "var_str_printf() has no wide character"
                                " support: \"%s\"", tmpfmt);
                 }
               str_arg = va_arg(a, char*);
               if (!str_arg) str_arg = "(null)";
               len = strlen(str_arg);
               if (p.min_wd || p.precision)
                 { if (p.precision && p.precision < len)
                     { len = p.precision; }
                   if (p.min_wd > len)
                     { p.min_wd -= len;
                       if (p.left_adjust)
                         { var_str_slice_copy(s, i, str_arg, len); i += len; }
                       var_str_ensure(s, i + p.min_wd);
                       while (p.min_wd)
                         { s->s[i] = ' ';
                           i++; p.min_wd--;
                         }
                       if (!p.left_adjust)
                         { var_str_slice_copy(s, i, str_arg, len); i += len; }
                     }
                   else
                     { var_str_slice_copy(s, i, str_arg, len); i += len; }
                 }
               else
                 { var_str_slice_copy(s, i, str_arg, len); i += len; }
               fmt++;
             }
           else if (*fmt == 'v' || *fmt == 'V')
             { VAR_STR_X(s, i) = 0;
               if (*fmt == 'v')
                 { if (!p.alternate)
                     { user_func = va_arg(a, var_str_func_tp*); }
                   if (!user_func)
                     { except_Raise(var_str_err, VAR_STR_ERR_fmt,
                                         "No function argument for %v");
                     }
                   a1 = va_arg(a, void*);
                   user_func(s, i, a1);
                 }
               else
                 { if (!p.alternate)
                     { user_func2 = va_arg(a, var_str_func2_tp*); }
                   if (!user_func2)
                     { except_Raise(var_str_err, VAR_STR_ERR_fmt,
                                         "No function argument for %V");
                     }
                   a1 = va_arg(a, void*);
                   a2 = va_arg(a, void*);
                   user_func2(s, i, a1, a2);
                 }
               i += strlen(s->s+i);
               fmt++;
             }
           else
             { __va_copy(b, a);
               vsprintf(tmp, tmpfmt, b);
               va_end(b);
               len = strlen(tmp);
               var_str_slice_copy(s, i, tmp, len);
               switch (*fmt)
                 { case 'f': case 'e': case 'E': case 'g': case 'G':
                     if (p.modifier == 'L')
                       { (void)va_arg(a, long double); }
                     else
                       { (void)va_arg(a, double); }
                   break;
                   case 'p':
                     (void)va_arg(a, void*);
                   break;
                   case 'd': case 'i': case 'o': case 'x': case 'X': case 'u':
                     if (p.modifier == 'l')
                       { (void)va_arg(a, long); }
                     else /* 'h' is passed as int */
                       { (void)va_arg(a, int); }
                   break;
                   case 'c':
                     if (p.modifier == 'l')
                       { except_Raise(var_str_err, VAR_STR_ERR_fmt,
                                      "var_str_printf() has no wide character"
                                      " support: \"%s\"", tmpfmt);
                       }
                     (void)va_arg(a, int);
                   break;
                   default:
                     except_Raise(var_str_err, VAR_STR_ERR_fmt,
                                  "Unknown conversion: \"%s\"", tmpfmt);
                   break;
                 }
               i += len; fmt++;
             }
         }
     }
   VAR_STR_X(s, i) = 0;
   return i - orig_i;
 }

extern int var_str_printf(var_string *s, int i, const char *fmt, ...)
 /* Same as sprintf(s->s+i, fmt, ...), except s grows. Use i = -1 for append.
    A terminating 0 is added.
    Return is number of chars written (without the terminating 0).
    Errors in the fmt string cause a var_str_err/fmt exception.
 */
 { va_list a;
   int n;
   va_start(a, fmt);
   n = var_str_vprintf(s, i, fmt, a);
   va_end(a);
   return n;
 }

/********** gets *************************************************************/

extern int var_str_gets(var_string *s, int i, FILE *f)
 /* Read a line from f into s[i...] (use i = -1 for append).
    A line ends at a '\n', which is always included in s (even if it was
    missing at eof). The line is 0 terminated.
    If no line could be read at all, s[i] = 0, and return is 0.
    Otherwise, return is the number of characters read (always including
    the '\n').
 */
 { int c;
   int orig_i;
   if (i < 0) i = VAR_STR_LEN(s);
   orig_i = i;
   c = getc(f);
   if (c == EOF)
     { VAR_STR_X(s, i) = 0;
       return 0;
     }
   while (c != '\n' && c != EOF)
     { VAR_STR_X(s, i) = c;
       i++;
       c = getc(f);
     }
   VAR_STR_X(s, i) = '\n';
   i++;
   VAR_STR_X(s, i) = 0;
   return i - orig_i;
 }

