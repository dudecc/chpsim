/* lex.c: Lexical scanning.
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
#include <var_string.h>
#include <hash.h>
#include <ascii.h>
#include <string_table.h>

#include <readline/readline.h>
#include <readline/history.h>

#define INLINE_LEX_C
#include "lex.h"


static var_string scratch;

static void show_line(lex_tp *L)
 /* print line with current position */
 { int i;
   fprintf(stderr, "%s", L->line.s);
   for (i = 0; i < L->curr->start_pos; i++)
     { if (L->line.s[i] == '\t')
	 { putc('\t', stderr); }
       else
         { putc(' ', stderr); }
     }
   putc('^', stderr);
   if (!L->curr->end_pos)
     { L->curr->end_pos = L->c - L->line.s; }
   for (i++; i < L->curr->end_pos; i++)
     { putc('^', stderr); }
   putc('\n', stderr);
 }

extern void lex_error(lex_tp *L, const char *fmt, ...)
 /* print error msg and exit (or jump to L->err_jmp) */
 { va_list a;
   va_start(a, fmt);
   show_line(L);
   fprintf(stderr, "%s[%d:%d] Error: ", L->fin_nm, L->lnr, L->curr->start_pos);
   vfprintf(stderr, fmt, a);
   putc('\n', stderr);
   va_end(a);
   if (L->err_jmp)
     { longjmp(*L->err_jmp, 1); }
   exit(1);
 }

extern void lex_warning(lex_tp *L, const char *fmt, ...)
 /* print warning msg */
 { va_list a;
   va_start(a, fmt);
   show_line(L);
   fprintf(stderr, "%s[%d] Warning: ", L->fin_nm, L->lnr);
   vfprintf(stderr, fmt, a);
   putc('\n', stderr);
   va_end(a);
 }


/********** symbols **********************************************************/

static const char *symbol_nm[] =
   { "",
#define SYM(M, S) S ,
#include "symbol.def"
	/* symbol_nm[x - TOK_sym] = string for symbol x */
   };

static int scan_symbol(lex_tp *L)
 /* Pre: a single- or multi-character symbol must occur at current position.
    Scan till after the symbol. Return is 1 if successful, 0 if not (in
    case of an error).
 */
 { char *c = L->c, *d;
   const char *s;
   int i;
   if (!isgraph(*c))
     { lex_error(L, "Illegal character code 0x%02X in program", *c);
       do { c++; } while (!isgraph(*c) && !isspace(*c));
       L->c = c;
       return 0;
     }
   /* If there are many multi-character symbols, this routine should
      be replaced by something more efficient.
   */
   for (i = 1; i < CONST_ARRAY_SIZE(symbol_nm); i++)
     { d = c;
       s = symbol_nm[i];
       while (*d == *s) /* given: d ends with '\n', which is not in s */
         { d++; s++; }
       if (!*s)
         { L->curr->t.tp = i + TOK_sym;
	   L->c = d;
	   if (L->curr->t.tp == SYM_comment_close)
	     { lex_error(L, "'*/' outside a comment");
	       return 0;
	     }
	   else if (L->curr->t.tp == SYM_equiv)
	     { lex_error(L, "There is no '==' symbol; maybe you mean '='?");
	       L->curr->t.tp = '=';
	     }
	   else if (L->curr->t.tp == SYM_keep)
	     { lex_error(L, "There is no '?#' symbol; use '#?' for a peek");
	       L->curr->t.tp = SYM_peek;
	     }
	   else if (L->curr->t.tp == SYM_andand)
	     { lex_error(L, "There is no '&&' symbol; you probably mean '&'");
	       L->curr->t.tp = '&';
	     }
	   else if (L->curr->t.tp == SYM_oror)
	     { lex_error(L, "There is no '||' symbol; or = '|' and pll = ','");
	       L->curr->t.tp = '|';
	     }
	   return 1;
	 }
     }
   L->curr->t.tp = *c;
   L->c = c + 1;
   return 1;
 }


/********** keywords *********************************************************/

static hash_table keyword_tbl;

static const char *keyword_nm[] =
   { "",
#define KW(M) #M ,
#include "symbol.def"
	/* keyword_nm[x - TOK_kw] = string for keyword x */
   };

static void init_keyword_tbl(void)
 /* initialize the keyword hash table */
 { int i;
   hash_entry *q;
   hash_table_init(&keyword_tbl, CONST_ARRAY_SIZE(keyword_nm),
		   HASH_const_keys, 0);
   for (i = 1; i < CONST_ARRAY_SIZE(keyword_nm); i++)
     { hash_insert(&keyword_tbl, keyword_nm[i], &q);
       q->data.i = i + TOK_kw;
     }
 }

static void term_keyword_tbl(void)
 /* Deallocate the keyword hash table. The strings denoting keyword values
    remain valid, but the lexical scanner can no longer recognize keywords.
 */
 { hash_table_free(&keyword_tbl);
 }

static token_tp find_keyword(lex_tp *L, const char *s)
 /* If s is a keyword, return its token value. Otherwise return 0. */
 { char *c;
   hash_entry *q;
   var_str_copy(&L->scratch, s);
   c = L->scratch.s;
   while (*c)
     { *c = tolower(*c); c++; }
   q = hash_find(&keyword_tbl, L->scratch.s);
   if (!q) return 0;
   return q->data.i;
 }


/********** lex_tp data structure ********************************************/

extern void lex_tp_init(lex_tp *L)
 /* initialize *L */
 { L->fin = 0;
   L->fin_nm = 0;
   L->lnr = 0;
   var_str_init(&L->line, 100);
   L->c = L->line.s;
   *L->c = 0;
   L->flags = 0;
   var_str_init(&L->scratch, 100);
   NEW(L->curr); NEW(L->prev);
   L->pflags = 0;
   llist_init(&L->modules);
   L->err_jmp = 0;
 }

extern void lex_tp_term(lex_tp *L)
 /* terminate *L (closes file, deallocates memory).
    Must call lex_tp_init(L) to re-use L.
 */
 { if (L->fin)
     { fclose(L->fin); L->fin = 0; }
   var_str_free(&L->line);
   L->c = 0;
   var_str_free(&L->scratch);
   free(L->curr); free(L->prev);
   llist_free(&L->modules, 0, 0);
 }

extern void lex_start(lex_tp *L)
 /* Pre: L has been initialized;
	 L->fin_nm and L->fin have been set (and opened for reading).
    Resets L to start reading a new file, then reads first token.
 */
 { L->lnr = 0;
   L->c = L->line.s;
   *L->c = 0;
   L->flags = 0;
   L->pflags = 0;
   lex_next(L);
 }

static char *read_line(lex_tp *L)
 /* Read the next line. Return is 0 if eof, L->c otherwise. */
 { assert(L->fin);
   if (IS_SET(L->flags, LEX_cmnd)) return 0;
   if (!var_str_gets(&L->line, 0, L->fin))
     { L->c = L->line.s;
       return 0;
     }
   L->lnr++;
   L->c = L->line.s;
   return L->c;
 }

extern token_tp lex_prompt_cmnd(lex_tp *L, const char *prompt)
 /* Pre: L has been initialized;
	 L->fin is open for reading.
    Prompt for a new line of input and scans and returns the first token.
 */
 { char *line;
   if (IS_SET(L->flags, LEX_readline))
     { line = readline(prompt);
       if (!line)
         { VAR_STR_X(&L->line, 0) = 0;
           return TOK_eof;
         }
       if (*line) add_history(line);
       var_str_copy(&L->line, line);
       var_str_cat(&L->line, "\n");
       no_dbg_free(line);
       L->lnr++;
       L->c = L->line.s;
     }
   else
     { fprintf(stdout, "%s", prompt);
       fflush(stdout);
       RESET_FLAG(L->flags, LEX_cmnd);
       if (!read_line(L)) return TOK_eof;
     }
   SET_FLAG(L->flags, LEX_cmnd);
   lex_next(L);
   return L->curr->t.tp;
 }


/********** tokens ***********************************************************/

static const char *token_nm[] =
   { "",
     "identifier",
     "integer",
     "character_constant",
     "string",
     "symbol",
     "float",
     "end-of-file"
	/* token_nm[t - TOK_kw_max] = descriptive string for t */
   };

static char get_escape(char c);

/* var_str_func_tp */
extern int print_string_const(var_string *s, int pos, const char *t)
 /* Write t as string constant (with quotes) to s[pos...].
    Return is nr chars printed.
 */
 { int i = pos;
   VAR_STR_X(s, i) = '"'; i++;
   while (*t)
     { if (!isprint(*t))
         { VAR_STR_X(s, i) = '\\'; i++;
	   VAR_STR_X(s, i) = get_escape(*t); i++;
	 }
       else
         { VAR_STR_X(s, i) = *t; i++; }
       t++;
     }
   VAR_STR_X(s, i) = '"'; i++;
   VAR_STR_X(s, i) = 0;
   return i - pos;
 }

extern char *token_str(lex_tp *L, token_tp t)
 /* If L, then writes t to L->scratch. Return is L->scratch.s.
    If !L, a private var_string is used for the same purpose.
 */
 { var_string *s;
   if (L) s = &L->scratch;
   else s = &scratch;
   if (!t)
     { var_str_printf(s, 0, "(nothing)"); }
   else if (t < TOK_sym)
     { if (isprint(t))
         { var_str_printf(s, 0, "%c", t); }
       else
         { var_str_printf(s, 0, "character %02X", t); }
     }
   else if (TOK_sym < t && t < TOK_sym_max)
     { var_str_printf(s, 0, "%s", symbol_nm[t - TOK_sym]); }
   else if (TOK_kw < t && t < TOK_kw_max)
     { var_str_printf(s, 0, "%s", keyword_nm[t - TOK_kw]); }
   else if (TOK_kw_max < t && t < TOK_max)
     { var_str_printf(s, 0, "%s", token_nm[t - TOK_kw_max]); }
   else
     { assert(!"t is illegal token");
       var_str_printf(s, 0, "(illegal token)");
     }
   return s->s;
 }


/********** identifiers ******************************************************/

static void scan_id_kw(lex_tp *L)
 /* Pre: L is at first character of an id or keyword.
    Scans id/kw.
    Note: This routine assumes keywords consist of letters only.
 */
 { char *c = L->c, tmp;
   int letters_only = 1; /* for efficiency */
   assert(isalpha(*c) || *c == '_');
   while (isalnum(*c) || *c == '_')
     { if (!isalpha(*c)) letters_only = 0;
       c++;
     }
   tmp = *c; *c = 0;
   if (letters_only && (L->flags & (LEX_cmnd | LEX_cmnd_kw)) != LEX_cmnd)
     { L->curr->t.tp = find_keyword(L, L->c);
       if (!L->curr->t.tp)
         { L->curr->t.tp = TOK_id; }
     }
   else
     { L->curr->t.tp = TOK_id; }
   if (L->curr->t.tp == TOK_id)
     { L->curr->t.val.s = make_str(L->c); }
   *c = tmp;
   L->c = c;
 }

static void scan_tok_symbol(lex_tp *L)
 /* Pre: L is at the initial ` of a symbol literal
    Scan the symbol literal
 */
 { char *c = &L->c[1], tmp;
   if (!isalpha(*c) && *c != '_')
     { lex_error(L, "Symbol marker '`' must be followed by identifier"); }
   while (isalnum(*c) || *c == '_') c++;
   tmp = *c; *c = 0;
   L->curr->t.tp = TOK_symbol;
   L->curr->t.val.s = make_str(&L->c[1]);
   *c = tmp;
   L->c = c;
 }

static void scan_instance(lex_tp *L)
 /* Pre: L is at '/'.
    Scans process name.
 */
 { char *c = L->c, tmp;
   assert(*c == '/');
   while (*c == '/' || isalnum(*c) || *c == '_' || *c == '[' || *c == ']')
     { c++; }
   tmp = *c; *c = 0;
   L->curr->t.tp = TOK_instance;
   L->curr->t.val.s = make_str(L->c);
   *c = tmp;
   L->c = c;
 }


/********** floating point ***************************************************/

static int copy_decimal(lex_tp *L, int i)
 /* Pre: L must be at a digit.
    Copy decimal digits from L to L->scratch[i...], skipping underscores.
    Return is next pos in L->scratch. Scans till first
    non-digit/non-underscore.
 */
 { char *c = L->c;
   if (!isdigit(*c))
     { lex_error(L, "In float: there should be a digit here."); }
   do { while (*c == '_') c++;
        if (!isdigit(*c)) break;
	VAR_STR_X(&L->scratch, i) = *c; i++;
	c++;
   } while (1); /* i.e., while (*c is a valid char) */
   L->c = c;
   return i;
 }

static void scan_float(lex_tp *L)
 /* Pre: A float must start at L.
    Scans until the first character that cannot be part of the float.
 */
 { int i = 0, int_part = 0, fraction = 0, exponent = 0;
   L->curr->t.tp = TOK_float;
   if (*L->c != '.')
     { i = copy_decimal(L, i);
       int_part = 1;
     }
   if (*L->c == '.')
     { VAR_STR_X(&L->scratch, i) = '.'; i++;
       L->c++;
       i = copy_decimal(L, i);
       fraction = 1;
     }
   if (*L->c == 'e' || *L->c == 'E')
     { VAR_STR_X(&L->scratch, i) = 'e'; i++;
       L->c++;
       if (*L->c == '+') L->c++;
       else if (*L->c == '-')
         { VAR_STR_X(&L->scratch, i) = '-'; i++;
	   L->c++;
	 }
       i = copy_decimal(L, i);
       exponent = 1;
     }
   VAR_STR_X(&L->scratch, i) = 0;
   if (!int_part && !fraction)
     { lex_error(L, "A float must have an integer part and/or a fraction."); }
   else if (!fraction && !exponent)
     { lex_error(L, "A float must have a fraction and/or an exponent."); }
   else
     { L->curr->t.val.d = strtod(L->scratch.s, 0); }
 }


/********** integers *********************************************************/

static void scan_integer(lex_tp *L, int base)
 /* Pre: An integer in base must start at L. 2 <= base <= 36.
    Scans until the first character that cannot be part of the integer.
    If the integer does not fit in a normal long, the token type is changed
    to TOK_z.
 */
 { char *c = L->c;
   int d, i = 0, overflow = 0;
   ulong n = 0, m;
   m = LONG_MAX / base;
   do { while (*c == '_') c++;
        if (isdigit(*c)) d = *c - '0'; 
	else if (isalpha(*c)) d = 10 + tolower(*c) - 'a';
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
        VAR_STR_X(&L->scratch, i) = *c; i++;
	c++;
   } while (1); /* i.e., while (*c is a valid character) */
   L->c = c;
   VAR_STR_X(&L->scratch, i) = 0;
   if (!i && !isalnum(*c))
     { lex_error(L, "Integer without digits."); }
   else if (overflow)
     { L->curr->t.tp = TOK_z;
       mpz_init_set_str(L->curr->t.val.z, L->scratch.s, base);
     }
   else
     { L->curr->t.val.i = n; }
 }

static void scan_integer_float(lex_tp *L)
 /* Pre: L is at first digit, integer or float must follow.
    Scans integer/float, plus following erroneous characters.
 */
 { char *c = L->c;
   int base = 10;
   assert(isdigit(*c));
   L->curr->t.tp = TOK_int;
   if (*c == '0' && (*(c+1) == 'x' || *(c+1) == 'X'))
     { L->c = c + 2;
       base = 16;
       scan_integer(L, base);
     }
   else if (*c == '0' && (*(c+1) == 'b' || *(c+1) == 'B'))
     { L->c = c + 2;
       base = 2;
       scan_integer(L, base);
     }
   else
     { scan_integer(L, base);
       if (*L->c == '#')
         { L->c++;
	   if (L->curr->t.tp == TOK_z)
	     { lex_error(L, "Illegal base for integer"); }
	   base = L->curr->t.val.i;
	   if (base < 2 || 36 < base)
	     { lex_error(L, "Illegal base for integer: %d", base); }
	   else
	     { scan_integer(L, base); }
	 }
       else if ((*L->c == '.' && isdigit(*(L->c+1)))
	        || *L->c == 'e' || *L->c == 'E')
         { L->c = c;
	   scan_float(L);
	 }
     }
   if (isalnum(*L->c))
     { lex_error(L, "Character '%c' is illegal in a base-%d number.",
		 *L->c, base);
       c = L->c;
       do { c++;
       } while (isalnum(*c) || *c == '_');
       L->c = c;
     }
 }


/********** chars and strings ************************************************/

static char escaped_char[256] = { 0 };

static void init_escaped_char(void)
 { escaped_char['n'] = ASCII_LF;
   escaped_char['t'] = ASCII_TAB;
   escaped_char['v'] = ASCII_VT;
   escaped_char['b'] = ASCII_BS;
   escaped_char['r'] = ASCII_CR;
   escaped_char['f'] = ASCII_FF;
   escaped_char['a'] = ASCII_BEL;
   escaped_char['\\'] = '\\';
   escaped_char['\''] = '\'';
   escaped_char['\"'] = '\"';
   escaped_char['q'] = ASCII_XON;
   escaped_char['s'] = ASCII_XOFF;
 }

static char scan_escape(lex_tp *L)
 /* Pre: L is at backslash.
    Scan till after escape, return value.
 */
 { char *c = L->c, esc;
   assert(*c == '\\');
   c++;
   if (!isprint(*c))
     { lex_error(L, "Illegal character code 0x%02X in character escape", *c);
       L->c = c + 1;
       return 0;
     }
   esc = escaped_char[(uint)*c];
   if (!esc)
     { esc = ASCII(*c);
       if (isdigit(*c) || (*c == 'x' && isxdigit(*(c+1))))
	 { lex_error(L, "Unknown character escape '\\%c'\n"
			"(if you want a number write it without quotes)",
			*c);
      }
       else
	 { lex_error(L, "Unknown character escape '\\%c'", *c);
	   c++;
	 }
     }
   else
     { c++; }
   L->c = c;
   return esc;
 }

static char get_escape(char c)
 /* find escape for c */
 { int i;
   for (i = 0; i < CONST_ARRAY_SIZE(escaped_char); i++)
     { if (escaped_char[i] == c)
         { return (char)i; }
     }
   error("No escape for character 0x%02X", c);
   return c;
 }

static void scan_char(lex_tp *L)
 /* Pre: L is at opening quote.
    Scan till after closing quote.
 */
 { char *c = L->c;
   assert(*c == '\'');
   c++;
   L->curr->t.tp = TOK_char;
   L->curr->t.val.i = 0;
   if (*c == '\\')
     { L->c = c;
       L->curr->t.val.i = scan_escape(L);
       c = L->c;
     }       
   else if (isprint(*c))
     { L->curr->t.val.i = ASCII(*c); c++; }
   else
     { lex_error(L, "Illegal character code 0x%02X in character constant", *c);
       do { c++; } while (!isprint(*c));
     }
   if (*c != '\'')
     { if (L->curr->t.val.i == '\'')
         { lex_error(L, "Missing closing quote for char constant\n"
	 		"(maybe you meant ''', '\\'', or '\\\\'?)");
	 }
       else
         { lex_error(L, "Missing closing quote for char constant"); }
     }
   else
     { c++; }
   L->c = c;
 }

static void scan_string(lex_tp *L)
 /* Pre: L is at opening quote.
    Scan till after closing quote.
 */
 { char *c = L->c;
   int i = 0;
   assert(*c == '"');
   L->curr->t.tp = TOK_string;
   c++;
   while (*c != '\n' && *c != '"')
     { if (*c == '\\')
         { L->c = c;
	   VAR_STR_X(&L->scratch, i) = scan_escape(L); i++;
	   c = L->c;
	 }
       else
         { VAR_STR_X(&L->scratch, i) = *c; i++;
	   c++;
	 }
     }
   if (*c != '"')
     { lex_error(L, "Missing closing quote for string"); }
   else
     { c++; }
   VAR_STR_X(&L->scratch, i) = 0;
   L->curr->t.val.s = strdup(L->scratch.s);
   L->c = c;
 }

static void scan_filename(lex_tp *L)
 /* Pre: L is at first character of filename
    Scan till first unescaped space
 */
 { char *c = L->c;
   int i = 0;
   L->curr->t.tp = TOK_string;
   while (*c != '\n' && *c != ' ')
     { if (*c != '\\')
         { VAR_STR_X(&L->scratch, i) = *c; i++;
	   c++;
	 }
       else if (c[1] == ' ')
         { VAR_STR_X(&L->scratch, i) = ' '; i++;
	   c+=2;
	 }
       else
         { L->c = c;
	   VAR_STR_X(&L->scratch, i) = scan_escape(L); i++;
	   c = L->c;
	 }
     }
   VAR_STR_X(&L->scratch, i) = 0;
   L->curr->t.val.s = strdup(L->scratch.s);
   L->c = c;
 }



/********** comments *********************************************************/

static void scan_comment(lex_tp *L)
 /* Pre: L is at start of comment.
    Scan till just after comment.
 */
 { char *c = L->c;
   assert(*c == '/' && *(c+1) == '*');
   c += 2;
   while (!(*c == '*' && *(c+1) == '/'))
     { if (*c == '/' && *(c+1) == '*')
         { lex_warning(L, "'/*' inside a comment");
	   c++;
	 }
       else if (!*c)
         { if (! (c = read_line(L)) )
	     { return; }
	 }
       else
         { c++; }
     }
   L->c = c + 2;
 }


/*****************************************************************************/

extern void lex_next(lex_tp *L)
 /* scans next token */
 { char *c;
   token_info *tmp;
   tmp = L->prev; L->prev = L->curr; L->curr = tmp;
 restart:
   c = L->c;
   L->curr->t.tp = 0;
   do { if (!*c)
          { if (! (c = read_line(L)) )
	      { L->curr->t.tp = TOK_eof;
	        L->curr->lnr = L->lnr;
		L->curr->start_pos = L->curr->end_pos = 0;
	        return;
	      }
	  }
        while (isspace(*c) && (!IS_SET(L->flags, LEX_cmnd) || *c != '\n'))
	  { c++; }
   } while (!*c);
   L->c = c;
   L->curr->lnr = L->lnr;
   L->curr->start_pos = c - L->line.s;
   L->curr->end_pos = 0;
   if (IS_SET(L->flags, LEX_filename))
     { if (*c == '"')
         { scan_string(L); }
       else if (*c == '\n')
         { L->curr->t.tp = TOK_nl;
           *c = 0;
         }
       else
         { scan_filename(L); }
     }
   else if (isalpha(*c) || *c == '_')
     { scan_id_kw(L); }
   else if (*c == '`')
     { scan_tok_symbol(L); }
   else if (IS_SET(L->flags, LEX_cmnd) && *c == '/')
     { scan_instance(L); }
   else if (isdigit(*c))
     { scan_integer_float(L); }
   else if (*c == '.' && isdigit(*(c+1)))
     { scan_float(L); }
   else if (*c == '\'')
     { scan_char(L); }
   else if (*c == '"')
     { scan_string(L); }
   else if (!IS_SET(L->flags, LEX_cmnd) && *c == '/' && *(c+1) == '/')
     { *c = 0;
       goto restart;
     }
   else if (!IS_SET(L->flags, LEX_cmnd) && *c == '/' && *(c+1) == '*')
     { scan_comment(L);
       goto restart;
     }
   else if (IS_SET(L->flags, LEX_cmnd) && *c == '\n')
     { L->curr->t.tp = TOK_nl;
       *c = 0;
     }
   else if (!scan_symbol(L))
     { goto restart; }
   L->curr->end_pos = L->c - L->line.s;
   assert(L->curr->t.tp);
 }

extern void lex_redo(lex_tp *L)
 /* Pre: LEX_cmnd is set
  * Reparse L->curr (pointless unless flags have changed since lex_next)
  */
 { token_info *tmp;
   if (L->curr->t.tp == TOK_nl) return;
   L->c = L->line.s + L->curr->start_pos;
   tmp = L->prev; L->prev = L->curr; L->curr = tmp;
   lex_next(L);
 }

extern int lex_must_be_one_of(lex_tp *L, int n, ...)
 /* Verify that current token is in list, then advance
    Return is index in list
  */
 { va_list a;
   token_tp t;
   int i, pos = 0;
   va_start(a, n);
   for (i = 0; i < n; i++)
     { t = va_arg(a, token_tp);
       if (L->curr->t.tp == t) break;
       pos += var_str_printf(&L->scratch, pos, i? " or %s" : "%s",
		             token_str(0, t));
     }
   va_end(a);
   if (i == n)
     { lex_error(L, "Expected %s", L->scratch.s); }
   lex_next(L);
   return i;
 }

/*****************************************************************************/

extern void init_lex(void)
 /* Call before lexical scanning */
 { init_keyword_tbl();
   init_escaped_char();
   init_string_table();
   var_str_init(&scratch, 10);
   LEAK(scratch.s);
 }

extern void exit_lex(void)
 /* Call after lexical scanning to free some memory */
 { term_keyword_tbl();
 }

