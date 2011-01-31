/* lex.h: Lexical scanning.
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

#ifndef LEX_H
#define LEX_H

#include <standard.h>
#include <var_string.h>
#include <llist.h>
#include <gmp.h> /* mpz_t */
#include <setjmp.h>

/********** tokens ***********************************************************/

typedef enum token_tp
   { TOK_none = 0,	/* 0: none */
     /* 1 .. 255 : individual characters;
		   currently, only printable chars are actually returned.
     */

     TOK_sym = 255,	/* offset for symbols */
#define SYM(M, S) SYM_ ## M ,
#include "symbol.def"
	/* SYM_assign etc. */
     TOK_sym_max,	/* upper limit for symbols */

     TOK_kw,		/* offset for keywords */
#define KW(M) KW_ ## M ,
#include "symbol.def"
	/* KW_array etc. */
     TOK_kw_max,	/* upper limit for keywords */

     TOK_id,		/* identifier */
     TOK_int,		/* integer_constant */
     TOK_z,		/* integer_constant */
     TOK_char,		/* char_constant */
     TOK_string,	/* string_constant */
     TOK_symbol,	/* symbol_constant */
     TOK_float,		/* float_constant */
     TOK_eof,		/* end-of-file */
     TOK_nl,		/* end-of-line, when reading commands */
     TOK_instance,	/* instance name, when reading commands */

     TOK_max		/* upper limit for tokens */
   } token_tp;


typedef struct token_value
   { token_tp tp;
     union {
       long i;   /* TOK_int, TOK_char */
       mpz_t z; /* TOK_z */
       char *s;  /* TOK_string(malloc); TOK_id, TOK_instance, TOK_symbol(str) */
       double d; /* TOK_float */
     } val;
   } token_value;

typedef struct token_info
   { token_value t;
     int lnr;
     int start_pos, end_pos; /* position in line [start..end) */
   } token_info;


/********** lex_tp data structure ********************************************/

FLAGS(lex_flags)
   { FIRST_FLAG(LEX_cmnd), /* reading interactive commands */
     NEXT_FLAG(LEX_cmnd_kw), /* recognize keywords on debug prompt */
     NEXT_FLAG(LEX_filename), /* parse in filenames as strings */
     NEXT_FLAG(LEX_readline) /* Use readline instead of gets */
   };

typedef struct lex_tp
   { FILE *fin;
     const char *fin_nm;
     int lnr;
     var_string line; /* current line */
     char *c; /* points in line */
     lex_flags flags; 
     var_string scratch; /* to avoid repeated alloc/free */
     token_info *curr, *prev;
     int pflags; /* values from parse.c:parse_flags */
     llist modules; /* llist(module_def) */
     jmp_buf *err_jmp; /* jump here on error */
     llist alloc_list; /* avoid memory leaks when using the above longjmp */
   } lex_tp;

extern void lex_tp_init(lex_tp *L);
 /* initialize *L */

extern void lex_start(lex_tp *L);
 /* Pre: L has been initialized;
	 L->fin_nm and L->fin have been set (and opened for reading).
    Resets L to start reading a new file, then reads first token.
 */

extern token_tp lex_prompt_cmnd(lex_tp *L, const char *prompt);
 /* Pre: L has been initialized;
	 L->fin is open for reading.
    Prompt for a new line of input and scans and returns the first token.
 */

extern void lex_tp_term(lex_tp *L);
 /* terminate *L (closes file, deallocates memory).
    Must call lex_tp_init(L) to re-use L.
 */

extern char *token_str(lex_tp *L, token_tp t);
 /* If L, then writes t to L->scratch. Return is L->scratch.s.
    If !L, a private var_string is used for the same purpose.
 */

extern int print_string_const(var_string *s, int pos, const char *t);
 /* Write t as string constant (with quotes) to s[pos...].
    Return is nr chars printed.
 */

extern void lex_error(lex_tp *L, const char *fmt, ...);
 /* print error msg and exit */

extern void lex_warning(lex_tp *L, const char *fmt, ...);
 /* print warning msg */


/********** scanning *********************************************************/

extern void lex_next(lex_tp *L);
 /* scans next token */

extern void lex_redo(lex_tp *L);
 /* Pre: LEX_cmnd is set
  * Reparse L->curr (pointless unless flags have changed since lex_next)
  */

extern void init_lex(void);
 /* Call before lexical scanning */

extern void exit_lex(void);
 /* Call after lexical scanning to free some memory */

INLINE_EXTERN_PROTO int lex_have(lex_tp *L, token_tp t);
 /* True if current token is t */

INLINE_EXTERN_PROTO int lex_have_next(lex_tp *L, token_tp t);
 /* True if current token is t, in which case L is advanced to next token */

INLINE_EXTERN_PROTO void lex_must_be(lex_tp *L, token_tp t);
 /* Verify that current token is t, then advance */

extern int lex_must_be_one_of(lex_tp *L, int n, ...);
 /* Verify that current token is in list, then advance
    Return is index in list
  */


/********** inline defs ******************************************************/
#if defined(USE_INLINE) || defined(INLINE_LEX_C)
/* standard prologue for inlining */
#undef INLINE_EXTERN
#ifdef INLINE_LEX_C
#define INLINE_EXTERN INLINE_C
#else
#define INLINE_EXTERN INLINE_H
#endif

INLINE_EXTERN int lex_have(lex_tp *L, token_tp t)
 /* True if current token is t */
 { return L->curr->t.tp == t; }

INLINE_EXTERN int lex_have_next(lex_tp *L, token_tp t)
 /* True if current token is t, in which case L is advanced to next token */
 { if (L->curr->t.tp == t)
     { lex_next(L);
       return 1;
     }
   return 0;
 }

INLINE_EXTERN void lex_must_be(lex_tp *L, token_tp t)
 /* Verify that current token is t, then advance */
 { if (L->curr->t.tp != t)
     { lex_error(L, "Expected %s", token_str(L, t)); }
   lex_next(L);
 }

#endif /* USE_INLINE */

#endif /* LEX_H */

