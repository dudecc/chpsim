/* print.h: printing the parse structure
 * 
 * COPYRIGHT 2010. California Institute of Technology
 * 
 * This file is part of chpsim.
 * 
 * Chpsim is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version, and under the terms of the
 * following disclaimer of liability:
 * 
 * The California Institute of Technology shall allow RECIPIENT to use and
 * distribute this software subject to the terms of the included license
 * agreement with the understanding that:
 * 
 * THIS SOFTWARE AND ANY RELATED MATERIALS WERE CREATED BY THE CALIFORNIA
 * INSTITUTE OF TECHNOLOGY (CALTECH). THE SOFTWARE IS PROVIDED "AS-IS" TO THE
 * RECIPIENT WITHOUT WARRANTY OF ANY KIND, INCLUDING ANY WARRANTIES OF
 * PERFORMANCE OR MERCHANTABILITY OR FITNESS FOR A PARTICULAR USE OR PURPOSE
 * (AS SET FORTH IN UNITED STATES UCC Sect. 2312-2313) OR FOR ANY PURPOSE
 * WHATSOEVER, FOR THE SOFTWARE AND RELATED MATERIALS, HOWEVER USED.
 * 
 * IN NO EVENT SHALL CALTECH BE LIABLE FOR ANY DAMAGES AND/OR COSTS,
 * INCLUDING, BUT NOT LIMITED TO, INCIDENTAL OR CONSEQUENTIAL DAMAGES OF ANY
 * KIND, INCLUDING ECONOMIC DAMAGE OR INJURY TO PROPERTY AND LOST PROFITS,
 * REGARDLESS OF WHETHER CALTECH BE ADVISED, HAVE REASON TO KNOW, OR, IN FACT,
 * SHALL KNOW OF THE POSSIBILITY.
 * 
 * RECIPIENT BEARS ALL RISK RELATING TO QUALITY AND PERFORMANCE OF THE
 * SOFTWARE AND ANY RELATED MATERIALS, AND AGREES TO INDEMNIFY CALTECH FOR
 * ALL THIRD-PARTY CLAIMS RESULTING FROM THE ACTIONS OF RECIPIENT IN THE
 * USE OF THE SOFTWARE.
 * 
 * In addition, RECIPIENT also agrees that Caltech is under no obligation to
 * provide technical support for the Software.
 * 
 * Finally, Caltech places no restrictions on RECIPIENT's use, preparation of
 * Derivative Works, public display or redistribution of the Software other
 * than those specified in the GNU General Public License and the requirement
 * that all copies of the Software released be marked with the language
 * provided in this notice.
 * 
 * You should have received a copy of the GNU General Public License
 * along with chpsim.  If not, see <http://www.gnu.org/licenses/>.
 * 
 * Authors: Marcel van der Goot and Chris Moore
 */

#ifndef PRINT_H
#define PRINT_H

#include <standard.h>
#include <var_string.h>
#include <llist.h>

FLAGS(print_flags)
   { FIRST_FLAG(PR_short), /* abbreviate complex statements; no newlines */
     NEXT_FLAG(PR_flush), /* flush string to file when it gets too long */
     NEXT_FLAG(PR_prs), /* Print only the prs body of a process */
     NEXT_FLAG(PR_meta), /* Print only the meta body of a process */
     NEXT_FLAG(PR_simple_var), /* Reduce arrays/records to individual vars */
     NEXT_FLAG(PR_cast), /* Print declarations using cast syntax */
     NEXT_FLAG(PR_reset), /* Add reset transistors where necessary */
     NEXT_FLAG(PR_user) /* first value to be used for app-specific flags*/
   };

typedef struct print_info
   { var_string *s; /* string to print to */
     int pos; /* current pos in s */
     print_flags flags;
     FILE *f; /* used when PR_flush is set */
     int nl; /* indicates whether last flush ended with nl */
     int rpos; /* general usage marker of a position in s */
     char *rsep; /* general usage extra string (usually a seperator) */
     struct exec_info *exec; /* Not usually valid... */
   } print_info;

extern int flush_limit; /* = 100; limit for PR_flush */
 /* When PR_flush is set, print_obj() optionally flushes s (or part of s)
    to f->f [default: stdout]. It will do this whenever the size (pos)
    exceeds flush_limit, but may also flush at other times.
    You can use print_info_flush (or just printf) to flush all of s.
 */

extern void print_info_init(print_info *f);
 /* initialize f without var_string */

extern void print_info_init_new(print_info *f);
 /* initialize f with new var_string */

extern void print_info_term(print_info *f);
 /* terminate f (free var_string, but not f) (also flushes if PR_flush) */

extern void print_obj(void *obj, print_info *f);
 /* print obj to f->s[f->pos...], adjust f->pos */

extern void print_obj_file(void *obj, FILE *f);
 /* print obj to file (newline added if missing) */

extern void print_ensure_nl(print_info *f);
 /* if f does not end with a newline, add one */

extern void print_info_flush(print_info *f);
 /* Flush f->s to f->f [default: stdout]. No newline is added. */

/* var_str_func_tp: use as first arg for %v */
extern int vstr_obj(var_string *s, int pos, void *obj);
 /* print obj to s[pos...] */

/* var_str_func2_tp: use as first arg for %V */
extern int vstr_obj2(var_string *s, int pos, void *obj, void *f);
 /* print obj to s[pos...] using flags from print_info f */

/* var_str_func_tp: use as first arg for %v */
extern int vstr_stmt(var_string *s, int pos, void *obj);
 /* same as vstr_obj, but only show beginning of complex stmts */

INLINE_EXTERN_PROTO void print_char(char c, print_info *f);
 /* print c to f->s; advances f->pos but does not add terminating 0
    or verify that new f->s[f->pos] exists.
 */

INLINE_EXTERN_PROTO void print_string(const char *s, print_info *f);
 /* print s to f->s; advances f->pos */

extern void print_obj_list(llist *l, print_info *f, const char *sep);
 /* llist(parse_obj) *l; print elements of l, separated by sep */

extern int app_print;
#define set_print(C) set_app(CLASS_ ## C, app_print, (obj_func*)print_ ## C)
 /* To set print_abc as print function for class abc.
    print_abc() must print to print_info->s[print_info->pos...] and adjust
    pos; it does not need to put the terminating 0. print_abc() should
    not use the vstr_...() functions, because those do not pass the
    print_info flags.
 */
#define set_print_cp(C,D) set_app(CLASS_ ## C, app_print, (obj_func*)print_ ## D)
 /* Used if C is a copy of D, and uses the same print function */

extern void init_print(int app);
 /* call at startup; pass unused object app index */

#if defined(USE_INLINE) || defined(INLINE_PRINT_C)
#undef INLINE_EXTERN
#ifdef INLINE_PRINT_C
#define INLINE_EXTERN INLINE_C
#else
#define INLINE_EXTERN INLINE_H
#endif

INLINE_EXTERN void print_char(char c, print_info *f)
 /* print c to f->s; advances f->pos but does not add terminating 0
    or verify that new f->s[f->pos] exists.
 */
 { VAR_STR_X(f->s, f->pos) = c;
   f->pos++;
 }

INLINE_EXTERN void print_string(const char *s, print_info *f)
 /* print s to f->s; advances f->pos */
 { f->pos += var_str_slice_copy(f->s, f->pos, s, -1); }
 

#endif /* USE_INLINE */

#endif /* PRINT_H */

