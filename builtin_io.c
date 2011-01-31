/* builtin_io.c: support for stdio.chp
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

#include <standard.h>
#include <errno.h>
#include "builtin_io.h"
#include "parse_obj.h"
#include "routines.h"
#include "exec.h"
#include "types.h"
#include "expr.h"

static llist file_list = 0; /* llist(FILE*); identified by index */

static FILE *get_file(value_tp *fval, exec_info *f, void *obj)
 /* Return the (open) file corresponding to fval. f and obj are used
    for error messages.
 */
 { FILE *fl = 0;
   if (!fval->rep)
     { exec_error(f, obj, "No value passed for file"); }
   int_simplify(fval, f);
   if (fval->rep == REP_int && fval->v.i >= 0)
     { fl = llist_idx(&file_list, fval->v.i);
       if (!fl)
         { if (fval->v.i < llist_size(&file_list))
	     { exec_error(f, obj, "File (%v) has been closed", vstr_val, fval);
	     }
	   exec_error(f, obj, "Invalid 'file' value (%v)", vstr_val, fval);
	 }
     }
   else
     { exec_error(f, obj, "Invalid 'file' value (%v)", vstr_val, fval); }
   return fl;
 }

/* exec_builtin_func */
static void builtin_fopen(function_def *x, exec_info *f)
 { value_tp *nameval, *modeval, *fval;
   call *y;
   FILE *fl;
   int pos;
   y = (call*)f->curr->stack->obj;
   fval = get_call_arg(x, 0, f); /* res f: file */
   nameval = get_call_arg(x, 1, f); /* name: string */
   modeval = get_call_arg(x, 2, f); /* mode: string */
   if (!nameval->rep)
     { exec_error(f, y, "No value passed for file name"); }
   if (!modeval->rep)
     { exec_error(f, y, "No value passed for mode"); }
   pos = print_string_value(&f->scratch, 0, nameval);
   print_string_value(&f->scratch, pos + 1, modeval);
   fl = fopen(f->scratch.s, f->scratch.s + pos + 1);
   if (!fl)
     { exec_error(f, y, "Cannot open \"%s\" with mode \"%s\":\n\t%s",
		     f->scratch.s, f->scratch.s+pos+1, strerror(errno));
     }
   llist_append(&file_list, fl);
   fval->rep = REP_int;
   fval->v.i = llist_size(&file_list) - 1;
 }

/* exec_builtin_func */
static void builtin_fclose(function_def *x, exec_info *f)
 { value_tp *fval;
   call *y;
   FILE *fl;
   y = (call*)f->curr->stack->obj;
   fval = get_call_arg(x, 0, f); /* f: file */
   fl = get_file(fval, f, y);
   fclose(fl);
   llist_overwrite(&file_list, fval->v.i, 0);
 }

/* exec_builtin_func */
static void builtin_read_byte(function_def *x, exec_info *f)
 { value_tp *fval, *xval, *errval;
   call *y;
   FILE *fl = 0;
   int c;
   y = (call*)f->curr->stack->obj;
   fval = get_call_arg(x, 0, f); /* f: file */
   xval = get_call_arg(x, 1, f); /* res x: {0..255} */
   errval = get_call_arg(x, 2, f); /* res err: file_err */
   fl = get_file(fval, f, y);
   c = getc(fl);
   xval->rep = REP_int;
   errval->rep = REP_symbol;
   if (c == EOF)
     { xval->v.i = 0;
       errval->v.s = make_str("eof");
     }
   else
     { xval->v.i = c;
       errval->v.s = make_str("ok");
     }
 }

static void read_int(FILE *f, int c, int base, value_tp *x, exec_info *ef)
 /* Read an integer in base from f, where c is the first character.
    The result is stored in *x (assumed to have no old value), which may be
    REP_z.
 */
 { int d, i = 0, overflow = 0;
   ulong n = 0, m;
   m = LONG_MAX / base;
   do { while (c == '_') c = getc(f);
        if (isdigit(c)) d = c - '0';
        else if (isalpha(c)) d = 10 + tolower(c) - 'a';
        else break;
        if (0 <= d && d < base)
          { if (n <= m)
	      { n = n * base + d;
	        if (n > LONG_MAX)
		  { overflow = 1; }
	      }
	    else
	      { overflow = 1; }
	  }
        else
          { break; }
	VAR_STR_X(&ef->scratch, i) = c; i++;
	c = getc(f);
   } while (1); /* i.e., while (c is a valid character) */
   ungetc(c, f);
   VAR_STR_X(&ef->scratch, i) = 0;
   if (overflow)
     { x->rep = REP_z;
       x->v.z = new_z_value(ef);
       mpz_init_set_str(x->v.z->z, ef->scratch.s, base);
     }
   else
     { x->rep = REP_int;
       x->v.i= (long)n;
     }
 }

/* exec_builtin_func */
static void builtin_read_int(function_def *x, exec_info *f)
 { value_tp *fval, *xval, *errval;
   call *y;
   FILE *fl = 0;
   int c, neg = 0;
   y = (call*)f->curr->stack->obj;
   fval = get_call_arg(x, 0, f); /* f: file */
   xval = get_call_arg(x, 1, f); /* res x: int */
   errval = get_call_arg(x, 2, f); /* res err: file_err */
   fl = get_file(fval, f, y);
   xval->rep = REP_int;
   xval->v.i = 0;
   errval->rep = REP_symbol;
   do { c = getc(fl);
   } while (isspace(c));
   if (c == '-')
     { neg = 1;
       c = getc(fl);
     }
   if (c == EOF)
     { errval->v.s = make_str("eof");
       return;
     }
   if (!isdigit(c))
     { ungetc(c, fl);
       errval->v.s = make_str("no_int");
       return;
     }
   errval->v.s = make_str("ok");
   if (c == '0')
     { c = getc(fl);
       if (c == 'x' || c == 'X')
         { c = getc(fl);
	   read_int(fl, c, 16, xval, f);
	   goto do_sign;
	 }
       else if (c == 'b' || c == 'B')
         { c = getc(fl);
	   read_int(fl, c, 2, xval, f);
	   goto do_sign;
	 }
     }
   read_int(fl, c, 10, xval, f);
   c = getc(fl);
   if (c == '#' && xval->rep == REP_int && 2 <= xval->v.i && xval->v.i <= 36)
     { c = getc(fl);
       read_int(fl, c, xval->v.i, xval, f);
     }
   else
     { ungetc(c, fl); }
 do_sign:
   if (!neg) return;
   if (xval->rep == REP_int)
     { xval->v.i = - xval->v.i; }
   else
     { mpz_neg(xval->v.z->z, xval->v.z->z); }
 }

/* exec_builtin_func */
static void builtin_write_byte(function_def *x, exec_info *f)
 { value_tp *fval, *xval;
   call *y;
   FILE *fl = 0;
   y = (call*)f->curr->stack->obj;
   fval = get_call_arg(x, 0, f); /* f: file */
   xval = get_call_arg(x, 1, f); /* x: {0..255} */
   int_simplify(xval, f);
   if (!xval->rep)
     { exec_error(f, y, "No value passed for x"); }
   fl = get_file(fval, f, y);
   putc(xval->v.i, fl);
   fflush(fl);
 }

/* exec_builtin_func */
static void builtin_write_string(function_def *x, exec_info *f)
 { value_tp *fval, *xval;
   call *y;
   FILE *fl = 0;
   y = (call*)f->curr->stack->obj;
   fval = get_call_arg(x, 0, f); /* f: file */
   xval = get_call_arg(x, 1, f); /* x: string */
   if (!xval->rep)
     { exec_error(f, y, "No value passed for x"); }
   fl = get_file(fval, f, y);
   print_string_value(&f->scratch, 0, xval);
   fprintf(fl, "%s", f->scratch.s);
   fflush(fl);
 }

/* exec_builtin_func */
static void builtin_write_int(function_def *x, exec_info *f)
 { value_tp *fval, *xval, *baseval;
   call *y;
   FILE *fl = 0;
   int base, pos, d;
   ulong n = NO_INIT;
   mpz_t nz;
   y = (call*)f->curr->stack->obj;
   fval = get_call_arg(x, 0, f); /* f: file */
   xval = get_call_arg(x, 1, f); /* x: int */
   baseval = get_call_arg(x, 2, f); /* base: {2..36} */
   int_simplify(baseval, f);
   if (!xval->rep)
     { exec_error(f, y, "No value passed for x"); }
   if (!baseval->rep)
     { exec_error(f, y, "No value passed for base"); }
   fl = get_file(fval, f, y);
   base = baseval->v.i;
   if (xval->rep == REP_int)
     { if (xval->v.i < 0)
         { putc('-', fl);
           n = - xval->v.i;
	 }
       else
         { n = xval->v.i; }
     }
   else if (xval->rep == REP_z)
     { mpz_init_set(nz, xval->v.z->z);
       if (mpz_sgn(nz) < 0)
         { putc('-', fl);
	   mpz_neg(nz, nz);
	 }
     }
   if (base == 16)
     { fprintf(fl, "0x"); }
   else if (base != 10)
     { fprintf(fl, "%d#", base); }
   if (xval->rep == REP_int)
     { pos = 0;
       while (n)
	 { d = n % base;
	   if (d < 10) d += '0';
	   else d = d - 10 + 'A'; 
	   VAR_STR_X(&f->scratch, pos) = d; pos++;
	   n = n / base;
	 }
       if (!pos)
	 { putc('0', fl); }
       else
	 { do { pos--;
		putc(f->scratch.s[pos], fl);
	   } while (pos > 0);
	 }
     }
   else
     { mpz_out_str(fl, base, nz);
       mpz_clear(nz);
     }
   fflush(fl);
 }

/* exec_builtin_func */
static void builtin_write(function_def *x, exec_info *f)
 { value_tp *fval;
   call *y;
   FILE *fl = 0;
   llist m;
   y = (call*)f->curr->stack->obj;
   fval = get_call_arg(x, 0, f); /* f: file */
   fl = get_file(fval, f, y);
   m = y->a;
   m = llist_alias_tail(&m); /* skip 'file' */
   bif_print_argv(&m, 0, f, 0);
   fprintf(fl, "%s", f->scratch.s);
   fflush(fl);
 }


/*****************************************************************************/

extern void init_builtin_io(FILE *user_stdout)
 /* call at startup */
 { llist_init(&file_list);
   llist_append(&file_list, stdin);
   llist_append(&file_list, user_stdout);
   set_builtin_func("fopen", builtin_fopen);
   set_builtin_func("fclose", builtin_fclose);
   set_builtin_func("read_byte", builtin_read_byte);
   set_builtin_func("read_int", builtin_read_int);
   set_builtin_func("write_byte", builtin_write_byte);
   set_builtin_func("write_string", builtin_write_string);
   set_builtin_func("write_int", builtin_write_int);
   set_builtin_func("write", builtin_write);
 }

extern FILE *builtin_io_change_std(FILE *f, int set_stdout)
 /* Pre: init_builtin_io() has already been called.
    If set_stdout, then set CHP's stdout to f, otherwise set CHP's stdin to f.
    Return is the file previously bound to stdin/stdout (which has not been
    closed). (Return is 0 if the old file was closed in CHP.)
 */
 { if (llist_size(&file_list) < 2)
     { error("Internal: builtin_io_change_std(..., %d) called before"
	     "init_builtin_io", set_stdout);
     }
   if (set_stdout) set_stdout = 1;
   return llist_overwrite(&file_list, set_stdout, f);
 }

