/* parse.c: parsing
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
#include "parse.h"
#include "lex.h"
#include "expr.h" /* for precedence */
#include "types.h" /* generic int and bool */

/* type for lex.h:lex_tp->pflags */
FLAGS(parse_flags)
   { FIRST_FLAG(PARSE_stmt), /* parsing a statement */
     NEXT_FLAG(PARSE_gc), /* guarded command allowed */
     NEXT_FLAG(PARSE_meta), /* inside meta body */
     NEXT_FLAG(PARSE_pr), /* parsing a production rule */
     NEXT_FLAG(PARSE_hold) /* delay_hold is allowed in pr parse */
   };


/* Memory leak notes:
   If parsing is done when L->err_jmp is set, a lex_error will leave a lot
   of parse objects allocated.  To clean up this properly, L->alloc_list
   should be initialized when L->err_jmp is set, and parse_cleanup(L) should
   be called after L->err_jmp is taken.  parse_cleanup(L) can also be used
   when parsing returns with no error.
*/

#define OBJ_CLEAR_LLIST(X,C,F) \
  if (((parse_obj*)X)->class==CLASS_ ## C) llist_free(&((C*)X)->F,0,0)

static int parse_cleanup_aux(void *x, void *dummy)
 { OBJ_CLEAR_LLIST(x, guarded_cmnd, l);
   OBJ_CLEAR_LLIST(x, rep_stmt, sl);
   OBJ_CLEAR_LLIST(x, array_constructor, l);
   OBJ_CLEAR_LLIST(x, record_constructor, l);
   OBJ_CLEAR_LLIST(x, record_constructor, tp.elem.l); /* set by sem */
   OBJ_CLEAR_LLIST(x, call, a);
   OBJ_CLEAR_LLIST(x, value_probe, p);
   OBJ_CLEAR_LLIST(x, select_stmt, gl);
   OBJ_CLEAR_LLIST(x, select_stmt, glr); /* set by sem */
   OBJ_CLEAR_LLIST(x, loop_stmt, gl);
   OBJ_CLEAR_LLIST(x, loop_stmt, glr); /* set by sem */
   OBJ_CLEAR_LLIST(x, loop_stmt, sl);
   OBJ_CLEAR_LLIST(x, meta_binding, a);
   OBJ_CLEAR_LLIST(x, compound_stmt, l);
   OBJ_CLEAR_LLIST(x, parallel_stmt, l);
   OBJ_CLEAR_LLIST(x, delay_hold, l);
   OBJ_CLEAR_LLIST(x, chp_body, sl);
   OBJ_CLEAR_LLIST(x, chp_body, dl);
   OBJ_CLEAR_LLIST(x, meta_body, sl);
   OBJ_CLEAR_LLIST(x, meta_body, dl);
   OBJ_CLEAR_LLIST(x, hse_body, sl);
   OBJ_CLEAR_LLIST(x, hse_body, dl);
   OBJ_CLEAR_LLIST(x, prs_body, sl);
   OBJ_CLEAR_LLIST(x, prs_body, dl);
   OBJ_CLEAR_LLIST(x, delay_body, sl);
   OBJ_CLEAR_LLIST(x, delay_body, dl);
   OBJ_CLEAR_LLIST(x, wired_type, li);
   OBJ_CLEAR_LLIST(x, wired_type, lo);
   OBJ_CLEAR_LLIST(x, process_def, ml);
   OBJ_CLEAR_LLIST(x, process_def, pl);
   OBJ_CLEAR_LLIST(x, function_def, pl);
   OBJ_CLEAR_LLIST(x, record_type, l);
   OBJ_CLEAR_LLIST(x, record_type, tp.elem.l); /* set by sem */
   OBJ_CLEAR_LLIST(x, union_type, l);
   OBJ_CLEAR_LLIST(x, symbol_type, l);
   OBJ_CLEAR_LLIST(x, module_def, rl);
   OBJ_CLEAR_LLIST(x, module_def, dl);
   if (((parse_obj*)x)->class == CLASS_const_expr)
     { clear_value_tp(&((const_expr*)x)->val, 0); }
   /* Hopefully a constant value won't need to access exec_info to be freed */
   free_obj(x);
   return 0;
 }

#undef OBJ_CLEAR_LLIST

extern void parse_cleanup(lex_tp *L)
 { llist_free(&L->alloc_list, parse_cleanup_aux, 0); }

extern void parse_cleanup_delayed(llist *l)
 { llist_free(l, parse_cleanup_aux, 0); }

extern llist parse_delay_cleanup(lex_tp *L)
 { llist l = L->alloc_list;
   L->alloc_list = 0;
   return l;
 }

INLINE_STATIC void free_parse(lex_tp *L, void *x)
 { if (!L->err_jmp) free_obj(x); }

/*****************************************************************************/
/* TODO: more efficient when replaced by table */

INLINE_STATIC int starts_required_module(lex_tp *L)
 { return lex_have(L, KW_requires); }

INLINE_STATIC int starts_type_definition(lex_tp *L)
 { return lex_have(L, KW_type); }

/* also symbol_type */
INLINE_STATIC int starts_integer_type(lex_tp *L)
 { return lex_have(L, '{'); }

INLINE_STATIC int starts_array_type(lex_tp *L)
 { return lex_have(L, KW_array); }

INLINE_STATIC int starts_record_type(lex_tp *L)
 { return lex_have(L, KW_record); }

INLINE_STATIC int starts_union_type(lex_tp *L)
 { return lex_have(L, KW_union); }

INLINE_STATIC int starts_const_definition(lex_tp *L)
 { return lex_have(L, KW_const); }

INLINE_STATIC int starts_field_definition(lex_tp *L)
 { return lex_have(L, KW_field); }

INLINE_STATIC int starts_function_definition(lex_tp *L)
 { return lex_have(L, KW_function); }

INLINE_STATIC int starts_procedure_definition(lex_tp *L)
 { return lex_have(L, KW_procedure); }

INLINE_STATIC int starts_property_declaration(lex_tp *L)
 { return lex_have(L, KW_property); }

INLINE_STATIC int starts_value_parameter(lex_tp *L)
 { return lex_have(L, KW_val) || lex_have(L, KW_const) || lex_have(L, TOK_id); }

INLINE_STATIC int starts_result_parameter(lex_tp *L)
 { return lex_have(L, KW_res) || lex_have(L, KW_valres); }

INLINE_STATIC int starts_process_definition(lex_tp *L)
 { return lex_have(L, KW_process); }

INLINE_STATIC int starts_port_parameter(lex_tp *L)
 { return lex_have(L, TOK_id) || lex_have(L, '('); }

INLINE_STATIC int starts_meta_parameter(lex_tp *L)
 { return lex_have(L, TOK_id); }

INLINE_STATIC int starts_chp_body(lex_tp *L)
 { return lex_have(L, KW_chp); }

INLINE_STATIC int starts_meta_body(lex_tp *L)
 { return lex_have(L, KW_meta); }

INLINE_STATIC int starts_hse_body(lex_tp *L)
 { return lex_have(L, KW_hse); }

INLINE_STATIC int starts_prs_body(lex_tp *L)
 { return lex_have(L, KW_prs); }

INLINE_STATIC int starts_delay_body(lex_tp *L)
 { return lex_have(L, KW_delay); }

INLINE_STATIC int starts_property_body(lex_tp *L)
 { return lex_have(L, KW_property); }

INLINE_STATIC int starts_var_declaration(lex_tp *L)
 { return lex_have(L, KW_var) || lex_have(L, KW_volatile); }

INLINE_STATIC int starts_instance_stmt(lex_tp *L)
 { return lex_have(L, KW_instance); }

INLINE_STATIC int starts_property_stmt(lex_tp *L)
 { return lex_have(L, TOK_id); }

INLINE_STATIC int starts_initializer(lex_tp *L)
 { return lex_have(L, '='); }

INLINE_STATIC int starts_loop_statement(lex_tp *L)
 { return lex_have(L, SYM_loop); }

INLINE_STATIC int starts_connection(lex_tp *L)
 { return lex_have(L, KW_connect); }

INLINE_STATIC int starts_selection_statement(lex_tp *L)
 { return lex_have(L, '['); }

INLINE_STATIC int starts_array_constructor(lex_tp *L)
 { return lex_have(L, '['); }

INLINE_STATIC int starts_record_constructor(lex_tp *L)
 { return lex_have(L, '{'); }

INLINE_STATIC int starts_type_expr(lex_tp *L)
 { return lex_have(L, '<'); }

INLINE_STATIC int starts_literal(lex_tp *L)
 { return lex_have(L, TOK_int) || lex_have(L, TOK_z) || lex_have(L, TOK_string)
     || lex_have(L, TOK_char) || lex_have(L, TOK_id) || lex_have(L, TOK_symbol)
     || lex_have(L, KW_true) || lex_have(L, KW_false);
 }

INLINE_STATIC int starts_procedure_call(lex_tp *L)
 { return lex_have(L, TOK_id); }

INLINE_STATIC int starts_record_field(lex_tp *L)
 { return lex_have(L, TOK_id); }

INLINE_STATIC int starts_union_field(lex_tp *L)
 { return lex_have(L, TOK_id) || lex_have(L, KW_default); }

static int starts_prefix_expr(lex_tp *L);

INLINE_STATIC int starts_replicator(lex_tp *L)
 { return lex_have(L, SYM_rep); }

static int starts_expr(lex_tp *L)
 { return starts_prefix_expr(L) || starts_replicator(L); }

static int starts_atom(lex_tp *L)
 { return starts_array_constructor(L) || starts_record_constructor(L)
     || starts_type_expr(L) || starts_literal(L);
 }

static int starts_postfix_expr(lex_tp *L)
 { return lex_have(L, TOK_id) || lex_have(L, '(')
     || lex_have(L, SYM_marked_paren) || starts_atom(L); }

static int starts_prefix_expr(lex_tp *L)
 { return lex_have(L, '+') || lex_have(L, '-') || lex_have(L, '~')
          || lex_have(L, '#') || starts_postfix_expr(L);
 }

static int starts_statement(lex_tp *L)
 { return lex_have(L, KW_skip) || starts_expr(L)
          || lex_have(L, SYM_loop) || lex_have(L, '[')
          || lex_have(L, '{') || lex_have(L, KW_connect)
          || starts_instance_stmt(L);
 }

INLINE_STATIC int starts_parallel_statement(lex_tp *L)
 { return starts_statement(L); }

static int starts_definition(lex_tp *L)
 { return starts_type_definition(L) || starts_const_definition(L)
          || starts_function_definition(L) || starts_procedure_definition(L)
          || starts_process_definition(L) || starts_field_definition(L);
 }

static int starts_global_definition(lex_tp *L)
 { return lex_have(L, KW_export) || starts_definition(L)
          || starts_property_declaration(L);
 }

static int starts_production_rule(lex_tp *L)
 { return lex_have(L, KW_after) || lex_have(L, KW_atomic) || starts_expr(L); }

INLINE_STATIC int starts_transition(lex_tp *L)
 { return starts_expr(L); }


/*****************************************************************************/

/* SEPARATOR(lex_tp *L, token_tp T, int E);
      E is a starts_S(L) expression. Use this inside a loop, when T
      separates occurrences of S. If there is a token T, absorb it.
      If there is no T, but E is true (i.e., there is an S), assume T is
      missing and give an error message. If there is no T and E is false,
      do a break (out of the surrounding loop). Normally this should only
      be used if the loop is followed by a closing symbol, like a closing
      brace. If the closing symbol can be followed by S, then whether or
      not you use this depends on the likelihood that the closing symbol
      is missing.
*/
#define SEPARATOR(L, T, E) \
       if (!lex_have_next(L, T)) \
         { if (E) lex_error(L, "Missing %s", token_str(L, T)); \
           else break; \
         }


/*****************************************************************************/

static void *parse_type(lex_tp *L);
/* extern void *parse_expr(lex_tp *L); */
static void *parse_parallel_statement(lex_tp *L);
static void *parse_definition(lex_tp *L);
static void *parse_production_rule(lex_tp *L);

static void parse_unknown(token_tp stop_sym, lex_tp *L)
 /* This allows for a certain amount of extensibility for certain parts of
    chpsim, allowing unknown future syntax to be parsed and ignored (ideally
    with a warning.  We only check for balancing of () [] {}, and parse until
    stop_sym is encountered outside of these groupings.
  */
 { while (!lex_have(L, TOK_eof))
     { if (lex_have_next(L, stop_sym)) return;
       else if (lex_have_next(L, '(')) parse_unknown(')', L);
       else if (lex_have_next(L, '[')) parse_unknown(']', L);
       else if (lex_have_next(L, '{')) parse_unknown('}', L);
       else if (lex_have_next(L, ')')) lex_error(L, "Unbalanced ()");
       else if (lex_have_next(L, ']')) lex_error(L, "Unbalanced []");
       else if (lex_have_next(L, '}')) lex_error(L, "Unbalanced {}");
     }
   lex_must_be(L, stop_sym);
 }

static void *parse_guard_aux(token_tp sym, lex_tp *L)
 /* If this guarded command turns out to be a replicator, we must ensure that
    the separator type ([:] or []) matches the previously used type sym.
 */
 { guarded_cmnd *x;
   rep_stmt *tmp;
   SET_FLAG(L->pflags, PARSE_gc);
   tmp = parse_expr(L);
   RESET_FLAG(L->pflags, PARSE_gc);
   if (tmp->class == CLASS_rep_stmt)
     { if (tmp->rep_sym != sym)
         { lex_error(L, "You cannot combine [] and [:]"); }
       return tmp;
     }
   x = new_parse(L, 0, tmp, guarded_cmnd);
   x->g = (expr*)tmp;
   lex_must_be(L, SYM_arrow);
   do { llist_prepend(&x->l, parse_parallel_statement(L));
        SEPARATOR(L, ';', starts_parallel_statement(L));
   } while (starts_parallel_statement(L));
   llist_reverse(&x->l);
   return x;
 }

static void parse_rep_common(rep_common *r, lex_tp *L)
 { lex_must_be(L, TOK_id);
   r->id = L->prev->t.val.s;
   lex_must_be(L, ':');
   r->l = parse_expr(L);
   lex_must_be(L, SYM_dots);
   r->h = parse_expr(L);
   lex_must_be(L, ':');
 }

static const char *rep_flag_name(parse_flags flags)
 { if (IS_SET(flags, PARSE_stmt)) return "statement";
   else if (IS_SET(flags, PARSE_gc)) return "guard";
   else if (IS_SET(flags, PARSE_pr)) return "production rule";
   else return "expression";
 }

/* This is pretty complicated because the replication construct is
 * close to being unparsable while only ever looking ahead one token.
 * Anywhere that any style of replicator is allowed, a rep_expr is
 * allowed.  In chp, meta, and hse bodies, PARSE_stmt and/or PARSE_gc
 * may be set, allowing the statement (',', ';') and guard
 * (SYM_arb, SYM_bar) replicators. In prs and delay bodies, production
 * rules and delay holds may be replicated, both of which have no
 * replication symbol.
 */
static void *parse_replicator(lex_tp *L)
 /* Pre: SYM_rep has already been parsed */
 { rep_expr *re;
   rep_stmt *rs;
   void *r = NO_INIT;
   rep_common rc;
   token_tp sym;
   int is_stmt = 0, is_gc = 0, is_pr = 0;
   parse_flags flags = L->pflags;
   RESET_FLAG(L->pflags, PARSE_stmt | PARSE_gc | PARSE_pr);
   if (lex_have_next(L, ',') || lex_have_next(L, ';'))
     { if (IS_SET(flags, PARSE_stmt))
         { is_stmt = 1; }
       else
         { lex_error(L, "Expected %s, found statement replicator",
                     rep_flag_name(flags));
         }
     }
   else if (lex_have_next(L, SYM_arb) || lex_have_next(L, SYM_bar))
     { if (IS_SET(flags, PARSE_gc))
         { is_gc = 1; }
       else
         { lex_error(L, "Expected %s, found guard replicator",
                     rep_flag_name(flags));
         }
     }
   else if (lex_have(L, TOK_id))
     { if (IS_SET(flags, PARSE_pr))
         { is_pr = 1; }
       else
         { lex_error(L, "Expected %s, found production rule replicator",
                     rep_flag_name(flags));
         }
     }
   else if (!lex_have_next(L, '+') && !lex_have_next(L, '*')
       && !lex_have_next(L, SYM_concat)
       && !lex_have_next(L, '&') && !lex_have_next(L, '|')
       && !lex_have_next(L, KW_xor)
       && !lex_have_next(L, '=') && !lex_have_next(L, SYM_neq))
     { lex_error(L, "Illegal operator in replicator"); }
   sym = L->prev->t.tp;
   parse_rep_common(&rc, L);
   if (is_gc || is_stmt || is_pr)
     { r = rs = new_parse(L, L->prev, 0, rep_stmt);
       rs->rep_sym = is_pr? 0 : sym;
       rs->r = rc;
       if (is_stmt)
         { do { llist_prepend(&rs->sl, parse_parallel_statement(L));
                SEPARATOR(L, ';', starts_parallel_statement(L));
           } while (starts_parallel_statement(L));
         }
       else if (is_gc)
         { do { llist_prepend(&rs->sl, parse_guard_aux(sym, L));
                SEPARATOR(L, sym, starts_expr(L));
           } while (starts_expr(L));
         }
       else
         { do { llist_prepend(&rs->sl, parse_production_rule(L));
           } while (!lex_have(L, SYM_rep_end) && !lex_have(L, '}'));
         }
       llist_reverse(&rs->sl);
     }
   else
     { r = re = new_parse(L, L->prev, 0, rep_expr);
       re->rep_sym = sym;
       re->r = rc;
       re->v = parse_expr(L);
     }
   lex_must_be(L, SYM_rep_end);
   L->pflags = flags;
   return r;
 }

static void *parse_array_constructor(lex_tp *L)
 { array_constructor *x;
   lex_must_be(L, '[');
   x = new_parse(L, L->prev, 0, array_constructor);
   do { llist_prepend(&x->l, parse_expr(L));
        SEPARATOR(L, ',', starts_expr(L));
   } while (1);
   llist_reverse(&x->l);
   lex_must_be(L, ']');
   return x;
 }

static void *parse_record_constructor(lex_tp *L)
 { record_constructor *x;
   lex_must_be(L, '{');
   x = new_parse(L, L->prev, 0, record_constructor);
   do { llist_prepend(&x->l, parse_expr(L));
        SEPARATOR(L, ',', starts_expr(L));
   } while (1);
   llist_reverse(&x->l);
   lex_must_be(L, '}');
   return x;
 }

static void *parse_type_expr(lex_tp *L)
 { type_expr *x;
   lex_must_be(L, '<');
   x = new_parse(L, L->prev, 0, type_expr);
   x->tps = parse_type(L);
   lex_must_be(L, '>');
   return x;
 }

static void *parse_atom(lex_tp *L)
 { token_expr *x;
   call *xc;
   if (lex_have_next(L, TOK_id))
     { if (lex_have(L, '('))
         { /* we get here for procedure_call, function_call, meta_binding */
           xc = new_parse(L, L->prev, 0, call);
           xc->id = L->prev->t.val.s;
           lex_next(L);
           if (starts_expr(L))
             { do { llist_append(&xc->a, parse_expr(L));
                    SEPARATOR(L, ',', starts_expr(L));
               } while (1);
             }
           lex_must_be(L, ')');
           x = (void*)xc;
         }
       else /* identifier or symbol_literal */
         { x = new_parse(L, L->prev, 0, token_expr);
           x->t = L->prev->t;
         }
     }
   else if (lex_have_next(L, '(') || lex_have_next(L, SYM_marked_paren))
     { x = parse_expr(L);
       SET_FLAG(x->flags, EXPR_parenthesized);
       lex_must_be(L, ')');
     }
   else if (starts_array_constructor(L))
     { x = parse_array_constructor(L); }
   else if (starts_record_constructor(L))
     { x = parse_record_constructor(L); }
   else if (starts_type_expr(L) && !L->err_jmp)
     { x = parse_type_expr(L); }
   else if (starts_literal(L))
     { x = new_parse(L, L->curr, 0, token_expr);
       x->t = L->curr->t;
       lex_next(L);
     }
   else if (lex_have_next(L, SYM_rep))
     { x = parse_replicator(L); }
   else
     { lex_error(L, "Expected an expression"); x = NO_INIT; }
   return x;
 }

static void *parse_postfix_expr(lex_tp *L)
 { expr *x, *tmp;
   array_subrange *xr;
   array_subscript *xi;
   field_of_record *xf;
   x = parse_atom(L);
   if (x->class == CLASS_rep_stmt) return x;
   /* x is postfix_expr */
   while (lex_have(L, '[') || lex_have(L, '.'))
     { if (lex_have_next(L, '['))
         { tmp = parse_expr(L);
           if (lex_have_next(L, SYM_dots))
             { xr = new_parse(L, 0, x, array_subrange);
               xr->x = x; xr->l = tmp;
               xr->h = parse_expr(L);
               x = (expr*)xr;
             }
           else
             { xi = new_parse(L, 0, x, array_subscript);
               xi->x = x; xi->idx = tmp;
               x = (expr*)xi;
               /* parse x[i,j] as x[i][j] */
               if (lex_have(L, ',') || starts_expr(L))
                 /* if not ',' then separator error will follow */
                 { do { SEPARATOR(L, ',', starts_expr(L));
                        xi = new_parse(L, 0, x, array_subscript);
                        xi->x = x;
                        xi->idx = parse_expr(L);
                        x = (expr*)xi;
                   } while (1);
                 }
             }
           lex_must_be(L, ']');
         }
       else if (lex_have_next(L, '.'))
         { xf = new_parse(L, 0, x, field_of_record);
           xf->x = x;
           lex_must_be(L, TOK_id);
           xf->id = L->prev->t.val.s;
           x = (expr*)xf;
         }
     }
   return x;
 }

static void *parse_prefix_expr(lex_tp *L)
 { prefix_expr *x;
   value_probe *y;
   /* do not simplify by omitting '+', because '+' has type requirements */
   if (lex_have_next(L, '+') || lex_have_next(L, '-') || lex_have_next(L, '~'))
     { x = new_parse(L, L->prev, 0, prefix_expr);
       x->op_sym = L->prev->t.tp;
       x->r = parse_prefix_expr(L);
     }
   else if (lex_have_next(L, '#'))
     { if (lex_have(L, '{'))
         { y = new_parse(L, L->prev, 0, value_probe);
           lex_next(L);
           do { llist_append(&y->p, parse_postfix_expr(L));
                SEPARATOR(L, ',', starts_postfix_expr(L));
           } while (1);
           lex_must_be(L, ':');
           y->b = parse_expr(L);
           lex_must_be(L, '}');
           x = (void*)y;
         }
       else
         { x = new_parse(L, L->prev, 0, prefix_expr);
           x->op_sym = '#';
           x->r = parse_prefix_expr(L);
         }
     }
   else
     { x = parse_postfix_expr(L); }
   return x;
 }

static void *resolve_precedence(lex_tp *L, void *xv)
 /* Pre: x = xv is a binary_expr;
         x->l is not a binary_expr or is parenthesized;
         x->r may be a binary_expr, but must have been parsed correctly.
    If x->r is a binary_expr, use operator precedence to resolve whether
    r should be a child of x or vice versa. Return is the new root.
 */
 { binary_expr *x = xv, *r = (binary_expr*)x->r;
   assert(x->class == CLASS_binary_expr);
   assert(x->l->class != CLASS_binary_expr
             || IS_SET(x->l->flags, EXPR_parenthesized));
   if (r->class != CLASS_binary_expr || IS_SET(r->flags, EXPR_parenthesized))
     { return x; }
   if (precedence(x->op_sym) < precedence(r->op_sym))
     { return x; }
   /* x->op >= r->op, i.e., x->op binds tighter (or left-associativity) */
   r->lnr = x->lnr;
   r->lpos = x->lpos;
   x->r = r->l;
   r->l = resolve_precedence(L, x);
   return r;
 }

extern void *parse_expr(lex_tp *L)
 /* actually this is parse_binary_expr().
    This can also return a bool_set_stmt or rep_stmt if PARSE_stmt was set
    just before this call (i.e., when called from parse_statement_2()).
 */
 { binary_expr *x;
   void *tmp;
   bool_set_stmt *xb;
   int stmt_allowed = IS_SET(L->pflags, PARSE_stmt);
   if (!lex_have(L, SYM_rep))
     { RESET_FLAG(L->pflags, PARSE_stmt | PARSE_gc | PARSE_pr); }
   x = parse_prefix_expr(L);
   if (x->class == CLASS_rep_stmt) return x;
   RESET_FLAG(L->pflags, PARSE_stmt | PARSE_gc | PARSE_pr);
   if (lex_have_next(L, '+') || lex_have_next(L, '-') || lex_have_next(L, '*')
       || lex_have_next(L, '/') || lex_have_next(L, '%')
       || lex_have_next(L, KW_mod) || lex_have_next(L, '^')
       || lex_have_next(L, SYM_concat)
       || lex_have_next(L, '=') || lex_have_next(L, SYM_neq)
       || lex_have_next(L, '<') || lex_have_next(L, '>')
       || lex_have_next(L, SYM_lte) || lex_have_next(L, SYM_gte)
       || lex_have_next(L, '&') || lex_have_next(L, '|')
       || lex_have_next(L, KW_xor))
     { tmp = x;
       if (stmt_allowed &&
           (L->prev->t.tp == '+' || L->prev->t.tp == '-') && !starts_expr(L))
         { xb = new_parse(L, 0, tmp, bool_set_stmt);
           xb->v = tmp;
           xb->op_sym = L->prev->t.tp;
           x = (void*)xb;
         }
       else
         { x = new_parse(L, 0, tmp, binary_expr);
           x->l = tmp; x->op_sym = L->prev->t.tp;
           x->r = parse_expr(L);
           x = resolve_precedence(L, x);
         }
     }
   return x;
 }

static void *parse_guarded_command(lex_tp *L)
 { return parse_guard_aux(L->prev->t.tp, L); }

static void *parse_selection_statement(lex_tp *L)
/* Also parse_loop_statment */
 { select_stmt *x = 0;
   loop_stmt *l = 0;
   guarded_cmnd *g;
   rep_stmt *rs;
   end_stmt *e;
   llist *gl;
   int mutex;
   if (lex_have_next(L, '['))
     { x = new_parse(L, L->prev, 0, select_stmt);
       gl = &x->gl;
     }
   else
     { lex_must_be(L, SYM_loop);
       l = new_parse(L, L->prev, 0, loop_stmt);
       gl = &l->gl;
     }
   mutex = 0;
   SET_FLAG(L->pflags, PARSE_gc);
   g = parse_parallel_statement(L);
   rs = (rep_stmt*)g;
   RESET_FLAG(L->pflags, PARSE_gc);
   if (g->class == CLASS_guarded_cmnd)
     { llist_append(gl, g);
       if (lex_have(L, SYM_bar))
         { while (lex_have_next(L, SYM_bar))
             { llist_append(gl, parse_guarded_command(L)); }
           mutex = 1;
         }
       else
         { while (lex_have_next(L, SYM_arb))
             { llist_append(gl, parse_guarded_command(L)); }
         }
       if (lex_have(L, SYM_bar) || lex_have(L, SYM_arb))
         { lex_error(L, "You cannot combine [] and [:]"); }
     }
   else if (rs->class == CLASS_rep_stmt && rs->rep_sym > TOK_sym)
     { llist_append(gl, g);
       if (rs->rep_sym == SYM_bar)
         { while (lex_have_next(L, SYM_bar))
             { llist_append(gl, parse_guarded_command(L)); }
           mutex = 1;
         }
       else if (rs->rep_sym == SYM_arb)
         { while (lex_have_next(L, SYM_arb))
             { llist_append(gl, parse_guarded_command(L)); }
         }
       if (lex_have(L, SYM_bar) || lex_have(L, SYM_arb))
         { lex_error(L, "You cannot combine [] and [:]"); }
     }
   else if (x)
     { /* TODO: some checks needed */
       x->w = (void*)g;
     }
   else
     { llist_prepend(&l->sl, g);
       if (lex_have(L, ';') || starts_parallel_statement(L))
         { do { SEPARATOR(L, ';', starts_parallel_statement(L));
                if (!starts_parallel_statement(L))
                  { break; }
                llist_prepend(&l->sl, parse_parallel_statement(L));
           } while (1);
         }
       llist_reverse(&l->sl);
     }
   lex_must_be(L, ']');
   if (x)
     { x->mutex = mutex;
       return x;
     }
   else
     { e = new_parse(L, L->prev, 0, end_stmt);
       if (llist_is_empty(gl))
         { llist_append(&l->sl, e); }
       else
         { l->mutex = mutex;
           while (!llist_is_empty(gl))
             { g = llist_head(gl);
               /* Note: e is shared among the guarded commands. This is
                  necessary because the function that sets breakpoints will
                  only see the end_stmt as part of the last gc (because only
                  the last gc could contain that line nr).
               */
               if (g->class == CLASS_guarded_cmnd)
                 { llist_append(&g->l, e); }
               gl = &(*gl)->tail;
             }
         }
       return l;
     }
 }

static void *parse_inline_array(lex_tp *L, void *end)
/* Pre: '[' (or ',') has been parsed, end is the type to be arrayed
 * If the type to be arrayed has not yet been parsed, use a dummy type.
 */
 { array_type *x;
   x = new_parse(L, L->prev, 0, array_type);
   x->l = parse_expr(L);
   lex_must_be(L, SYM_dots);
   x->h = parse_expr(L);
   if (lex_have_next(L, ','))
     { x->tps = parse_inline_array(L, end); }
   else
     { lex_must_be(L, ']');
       x->tps = end;
     }
   return x;
 }

static void parse_instance_stmt(lex_tp *L, llist *l)
 /* Note: object list is prepended to l in reverse order */
 { instance_stmt *x;
   meta_binding *mb = 0;
   llist m, old = *l;
   dummy_type *end = new_parse(L, L->prev, 0, dummy_type);
   lex_must_be(L, KW_instance);
   if (!IS_SET(L->pflags, PARSE_meta))
     { lex_error(L,"An instance declaration can only occur in a meta process");
     }
   /* instance x, y: t; is parsed as instance x: t; instance y: t; */
   do { lex_must_be(L, TOK_id);
        x = new_parse(L, L->prev, 0, instance_stmt);
        x->d = new_parse(L, 0, x, var_decl);
        x->d->id = L->prev->t.val.s;
        llist_prepend(l, x);
        if (lex_have_next(L, '['))
          { x->d->tps = parse_inline_array(L, end); }
        else
          { x->d->tps = (type_spec*)end; }
   } while (lex_have_next(L, ','));
   lex_must_be(L, ':');
   end->tps = parse_type(L);
   m = *l;
   if (lex_have_next(L, '('))
     { mb = new_parse(L, L->prev, 0, meta_binding);
       if (starts_expr(L))
         { do { llist_append(&mb->a, parse_expr(L));
                SEPARATOR(L, ',', starts_expr(L));
              } while (1);
         }
       lex_must_be(L, ')');
     }
   while (m != old)
     { x = llist_head(&m);
       x->mb = mb;
       m = llist_alias_tail(&m);
     }
 }

static void *parse_connection(lex_tp *L)
 { connection *c;
   rep_stmt *rs;
   void *x;
   if (!IS_SET(L->pflags, PARSE_meta))
     { lex_error(L,"A connection statement can only occur in a meta process");
     }
   lex_must_be(L, KW_connect);
   x = c = new_parse(L, L->prev, 0, connection);
   while (lex_have_next(L, KW_all))
     { rs = new_parse(L, L->prev, 0, rep_stmt);
       rs->rep_sym = ';';
       parse_rep_common(&rs->r, L);
       llist_init(&rs->sl);
       llist_prepend(&rs->sl, x);
       x = rs;
     }
   c->a = parse_expr(L);
   lex_must_be(L, ',');
   c->b = parse_expr(L);
   return x;
 }

static void *parse_statement_2(lex_tp *L)
 /* This parses various statements that start with an expression, or a
    rep_stmt, which starts identically to a rep_expr.
    If PARSE_gc is set, we also recognize guarded commands (when called
    directly from parse_selection_statement())
    If PARSE_meta is set, we allow meta_binding statements.

    statement_2:    expr |:=| expr
                 [] expr { |+| [] |-| }
                 [] expr {|?| [] |#?| } expr
                 [] expr |!| expr |?|\OPT
                 [] expr |->| parallel_statement\TSEQ
                 [] expr |(| expr\LIST |)|
                 [] expr
                 [] << ...
                 ;
    We allow an initial expr rather than forcing, e.g., a postfix_expr for
    assignment, because that is what we need for the guarded command.

    The last alternative (just expr) can be a procedure_call, a sync statement,
    or the expr in a wait if we are parsing a selection_statement. Note
    that even a procedure call with parameters is parsed as expr (namely,
    as function call).
    The x+ and x- forms are actually parsed by parse_expr().
 */
 { expr *x;
   assignment *xa;
   guarded_cmnd *gc;
   communication *xc;
   meta_binding *xb;
   int gc_allowed = IS_SET(L->pflags, PARSE_gc);
   SET_FLAG(L->pflags, PARSE_stmt); /* to allow x+, x-, and rep_stmt's */
   x = parse_expr(L);
   RESET_FLAG(L->pflags, PARSE_stmt | PARSE_gc);
   if (x->class == CLASS_bool_set_stmt || x->class == CLASS_rep_stmt)
     { /* already parsed */ }
   else if (lex_have_next(L, SYM_assign))
     { xa = new_parse(L, 0, x, assignment);
       xa->v = x;
       xa->e = parse_expr(L);
       return xa;
     }
   else if (lex_have(L, '='))
     { lex_error(L, "Maybe you mean ':=' ?"); }
   else if (lex_have_next(L, '?') || lex_have_next(L, SYM_peek))
     { xc = new_parse(L, 0, x, communication);
       xc->p = x;
       xc->op_sym = L->prev->t.tp;
       xc->e = parse_expr(L);
       return xc;
     }
   else if (lex_have_next(L, '!'))
     { xc = new_parse(L, 0, x, communication);
       xc->p = x;
       xc->op_sym = L->prev->t.tp;
       xc->e = parse_expr(L);
       if (lex_have_next(L, '?'))
         { xc->op_sym = '='; }
       return xc;
     }
   else if (lex_have_next(L, SYM_arrow))
     { if (!gc_allowed)
         { lex_error(L, "You cannot have a guarded command here"
                      " (missing [] symbol?)");
         }
       RESET_FLAG(L->pflags, PARSE_gc);
       if (!starts_parallel_statement(L))
         { lex_error(L, "Guarded command without statement"); }
       gc = new_parse(L, 0, x, guarded_cmnd);
       gc->g = x;
       do { llist_prepend(&gc->l, parse_parallel_statement(L));
            SEPARATOR(L, ';', starts_parallel_statement(L));
       } while (starts_parallel_statement(L));
       llist_reverse(&gc->l);
       return gc;
     }
   else if (lex_have_next(L, '('))
     { if (!IS_SET(L->pflags, PARSE_meta))
         { lex_error(L, "Is this a meta-binding? You cannot have that here"); }
       xb = new_parse(L, 0, x, meta_binding);
       xb->x = x;
       if (starts_expr(L))
         { do { llist_append(&xb->a, parse_expr(L));
                SEPARATOR(L, ',', starts_expr(L));
           } while (1);
         }
       lex_must_be(L, ')');
       return xb;
     }
   /* else sync statement or procedure call */
   return x;
 }

static void *parse_statement(lex_tp *L)
 { void *x;
   compound_stmt *xb;
   if (lex_have_next(L, KW_skip))
     { x = new_parse(L, L->prev, 0, skip_stmt); }
   else if (starts_selection_statement(L) || starts_loop_statement(L))
     { x = parse_selection_statement(L); }
   else if (lex_have_next(L, '{'))
     { xb = new_parse(L, L->prev, 0, compound_stmt);
       do { llist_prepend(&xb->l, parse_parallel_statement(L));
            SEPARATOR(L, ';', starts_parallel_statement(L));
       } while (starts_parallel_statement(L));
       lex_must_be(L, '}');
       llist_reverse(&xb->l);
       if (llist_size(&xb->l) > 1)
         { x = xb; }
       else /* size == 1 */
         { x = llist_idx_extract(&xb->l, 0);
           free_parse(L, xb);
         }
     }
   else if (starts_instance_stmt(L))
     { xb = new_parse(L, L->prev, 0, compound_stmt);
       parse_instance_stmt(L, &xb->l);
       llist_reverse(&xb->l);
       if (llist_size(&xb->l) > 1)
         { x = xb; }
       else /* size == 1 */
         { x = llist_idx_extract(&xb->l, 0);
           free_parse(L, xb);
         }
     }
   else if (starts_connection(L))
     { x = parse_connection(L); }
   else if (starts_expr(L))
     { x = parse_statement_2(L); }
   else
     { lex_error(L, "Expected a statement"); x = NO_INIT; }
   return x;
 }

static void *parse_parallel_statement(lex_tp *L)
 { parallel_stmt *x;
   rep_stmt *y;
   y = parse_statement(L);
   if (y->class == CLASS_rep_stmt && y->rep_sym != ';' && y->rep_sym != ',')
     { return y; }
   if (!lex_have_next(L, ','))
     { return y; }
   x = new_parse(L, 0, y, parallel_stmt);
   llist_prepend(&x->l, y);
   do { llist_prepend(&x->l, parse_statement(L));
   } while (lex_have_next(L, ','));
   llist_reverse(&x->l);
   return x;
 }

static void *parse_initializer(lex_tp *L)
 { lex_must_be(L, '=');
   return parse_expr(L);
 }

static void parse_var_declaration(lex_tp *L, llist *l)
 /* Note: object list is prepended to l in reverse order */
 { var_decl *x;
   llist m, old = *l;
   void *z = 0;
   dummy_type *end = new_parse(L, L->prev, 0, dummy_type);
   int volatl = 0;
   if (lex_have_next(L, KW_volatile)) volatl = 1;
   lex_must_be(L, KW_var);
   /* var x, y: t; is parsed as var x: t; var y: t; */
   do { lex_must_be(L, TOK_id);
        x = new_parse(L, L->prev, 0, var_decl);
        x->id = L->prev->t.val.s;
        if (volatl) SET_FLAG(x->flags, EXPR_volatile);
        llist_prepend(l, x);
        if (lex_have_next(L, '['))
          { x->tps = parse_inline_array(L, end); }
        else
          { x->tps = (type_spec*)end; }
        /* don't use SEPARATOR, because more likely ':' is missing than ',' */
   } while (lex_have_next(L, ','));
   lex_must_be(L, ':');
   end->tps = parse_type(L);
   if (starts_initializer(L))
     { z = parse_initializer(L); }
   lex_must_be(L, ';');
   m = *l;
   while (m != old)
     { x = llist_head(&m);
       x->z = z;
       m = llist_alias_tail(&m);
     }
 }

static void *parse_transition(lex_tp *L)
 { transition *x;
   x = new_parse(L, L->curr, 0, transition);
   x->v = parse_prefix_expr(L);
   lex_must_be_one_of(L, 2, '+', '-');
   x->op_sym = L->prev->t.tp;
   return x;
 }

static void *parse_delay_hold(lex_tp *L)
 { delay_hold *x;
   lex_must_be(L, '{');
   x = new_parse(L, L->prev, 0, delay_hold);
   do { llist_prepend(&x->l, parse_transition(L));
        SEPARATOR(L, ',', starts_transition(L));
      } while (1);
   llist_reverse(&x->l);
   lex_must_be(L, '}');
   lex_must_be(L, KW_requires);
   lex_must_be(L, '{');
   x->c = parse_prefix_expr(L);
   if (lex_have_next(L, '>'))
     { x->n = parse_expr(L); }
   lex_must_be(L, '}');
   if (lex_have(L, ',') || lex_have(L, ';'))
     { lex_error(L, "Delay holds cannot be separated by '%c'",
                 L->curr->t.tp);
     }
   return x;
 }

static void *parse_production_rule(lex_tp *L)
 /* if PARSE_hold is set, also allow delay_holds */
 { production_rule *x;
   expr *g, *delay = 0;
   int atomic = 0;
   if (IS_SET(L->pflags, PARSE_hold) && lex_have(L, '{'))
     { return parse_delay_hold(L); }
   if (lex_have_next(L, KW_atomic))
     { atomic = 1; }
   else if (lex_have_next(L, KW_after))
     { lex_must_be(L, '(');
       delay = parse_expr(L);
       lex_must_be(L, ')');
     }
   else
     { SET_FLAG(L->pflags, PARSE_pr); }
   g = parse_expr(L);
   RESET_FLAG(L->pflags, PARSE_pr);
   if (g->class == CLASS_rep_stmt) return g;
   x = new_parse(L, 0, g, production_rule);
   x->g = g;
   x->atomic = atomic;
   x->delay = delay;
   lex_must_be(L, SYM_arrow);
   x->v = parse_prefix_expr(L);
   lex_must_be_one_of(L, 2, '+', '-');
   x->op_sym = L->prev->t.tp;
   if (lex_have(L, ',') || lex_have(L, ';'))
     { lex_error(L, "Production rules cannot be separated by '%c'",
                 L->curr->t.tp);
     }
   return x;
 }

static void *parse_body(chp_body *x, lex_tp *L)
 /* pre: KW_chp or KW_meta has been parsed */
 { end_stmt *e;
   lex_must_be(L, '{');
   while (1)
     { if (starts_definition(L))
         { llist_prepend(&x->dl, parse_definition(L)); }
       else if (starts_var_declaration(L))
         { parse_var_declaration(L, &x->dl); }
       else if (starts_instance_stmt(L))
         { parse_instance_stmt(L, &x->sl);
           lex_must_be(L, ';');
         }
       else
         { break; }
     }
   llist_reverse(&x->dl);
   while (starts_parallel_statement(L))
     { llist_prepend(&x->sl, parse_parallel_statement(L));
       SEPARATOR(L, ';', starts_parallel_statement(L));
     }
   lex_must_be(L, '}');
   if (!llist_is_empty(&x->sl))
     { e = new_parse(L, L->prev, 0, end_stmt);
       e->end_body = 1;
       llist_prepend(&x->sl, e);
     }
   llist_reverse(&x->sl);
   return x;
 }

static void *parse_chp_body(lex_tp *L)
 { chp_body *x;
   int pflags = L->pflags;
   lex_must_be(L, KW_chp);
   RESET_FLAG(L->pflags, PARSE_meta);
   x = new_parse(L, L->prev, 0, chp_body);
   parse_body(x, L);
   L->pflags = pflags;
   return x;
 }

static void *parse_meta_body(lex_tp *L)
 { meta_body *x;
   int pflags = L->pflags;
   lex_must_be(L, KW_meta);
   SET_FLAG(L->pflags, PARSE_meta);
   x = new_parse(L, L->prev, 0, meta_body);
   parse_body((chp_body*)x, L);
   L->pflags = pflags;
   return x;
 }

static void *parse_hse_body(lex_tp *L)
 { hse_body *x;
   int pflags = L->pflags;
   lex_must_be(L, KW_hse);
   RESET_FLAG(L->pflags, PARSE_meta);
   x = new_parse(L, L->prev, 0, hse_body);
   parse_body(x, L);
   L->pflags = pflags;
   return x;
 }

static void *parse_prs_var_decl(lex_tp *L)
 { var_decl *x;
   lex_must_be(L, TOK_id);
   x = new_parse(L, L->prev, 0, var_decl);
   x->id = L->prev->t.val.s;
   if (lex_have_next(L, '['))
     { x->tps = parse_inline_array(L, &generic_bool); }
   else
     { x->tps = (type_spec*)&generic_bool; }
   if (lex_have_next(L, '+') || lex_have_next(L, '-'))
     { x->z_sym = L->prev->t.tp; }
   else if (lex_have_next(L, '='))
     { x->z = parse_expr(L); }
   return x;
 }

static void *parse_prs_body(lex_tp *L)
 { prs_body *x;
   int pflags = L->pflags;
   lex_must_be(L, KW_prs);
   RESET_FLAG(L->pflags, PARSE_meta);
   x = new_parse(L, L->prev, 0, prs_body);
   lex_must_be(L, '{');
   while (1)
     { if (starts_definition(L))
         { llist_prepend(&x->dl, parse_definition(L)); }
       else if (lex_have_next(L, KW_var))
         { do { llist_prepend(&x->dl, parse_prs_var_decl(L));
              } while (lex_have_next(L, ','));
           lex_must_be(L, ';');
         }
       else
         { break; }
     }
   llist_reverse(&x->dl);
   while (starts_production_rule(L))
     { llist_prepend(&x->sl, parse_production_rule(L)); }
   lex_must_be(L, '}');
   llist_reverse(&x->sl);
   L->pflags = pflags;
   return x;
 }

static void *parse_counter_decl(lex_tp *L)
 { var_decl *x;
   token_expr *z;
   lex_must_be(L, TOK_id);
   x = new_parse(L, L->prev, 0, var_decl);
   x->id = L->prev->t.val.s;
   if (lex_have_next(L, '['))
     { x->tps = parse_inline_array(L, &generic_int); }
   else
     { x->tps = (type_spec*)&generic_int; }
   if (lex_have_next(L, '='))
     { x->z = parse_expr(L); }
   else /* hack to make initial value default to zero */
     { z = new_parse(L, 0, x, token_expr);
       z->t.tp = TOK_int;
       z->t.val.i = 0;
       x->z = (expr*)z;
     }
   return x;
 }

static void *parse_delay_body(lex_tp *L)
 { delay_body *x;
   int pflags = L->pflags;
   lex_must_be(L, KW_delay);
   RESET_FLAG(L->pflags, PARSE_meta);
   x = new_parse(L, L->prev, 0, delay_body);
   lex_must_be(L, '{');
   while (1)
     { if (starts_definition(L))
         { llist_prepend(&x->dl, parse_definition(L)); }
       else if (lex_have_next(L, KW_counter))
         { do { llist_prepend(&x->dl, parse_counter_decl(L));
              } while (lex_have_next(L, ','));
           lex_must_be(L, ';');
         }
       else if (starts_var_declaration(L))
         { parse_var_declaration(L, &x->dl); }
       else break;
     }
   llist_reverse(&x->dl);
   SET_FLAG(L->pflags, PARSE_hold);
   while (lex_have(L, '{') || starts_production_rule(L))
     { llist_prepend(&x->sl, parse_production_rule(L)); }
   RESET_FLAG(L->pflags, PARSE_hold);
   lex_must_be(L, '}');
   llist_reverse(&x->sl);
   L->pflags = pflags;
   return x;
 }

static void *parse_property_stmt(lex_tp *L)
 { property_stmt *x;
   lex_must_be(L, TOK_id);
   x = new_parse(L, L->prev, 0, property_stmt);
   x->id = L->prev->t.val.s;
   lex_must_be(L, '(');
   x->node = parse_expr(L);
   lex_must_be(L, ')');
   lex_must_be(L, SYM_assign);
   x->v = parse_expr(L);
   return x;
 }
   

static void *parse_property_body(lex_tp *L)
 { property_body *x;
   lex_must_be(L, KW_property);
   x = new_parse(L, L->prev, 0, property_body);
   lex_must_be(L, '{');
   while (starts_property_stmt(L))
     { llist_prepend(&x->sl, parse_property_stmt(L));
       SEPARATOR(L, ';', starts_property_stmt(L));
     }
   lex_must_be(L, '}');
   llist_reverse(&x->sl);
   return x;
 }

static void *parse_wire_decl(lex_tp *L)
 { wire_decl *x;
   lex_must_be(L, TOK_id);
   x = new_parse(L, L->prev, 0, wire_decl);
   x->id = L->prev->t.val.s;
   if (lex_have_next(L, '['))
     { x->tps = parse_inline_array(L, &generic_bool); }
   else
     { x->tps = (type_spec*)&generic_bool; }
   if (lex_have_next(L, '+') || lex_have_next(L, '-'))
     { x->init_sym = L->prev->t.tp; }
   else if (lex_have_next(L, '='))
     { x->z = parse_expr(L); }
   return x;
 }

static void *parse_wired_type(lex_tp *L)
/* Pre: '(' has been parsed */
 { wired_type *x = new_parse(L, L->prev, 0, wired_type);
   wire_decl *w;
   do { llist_prepend(&x->li, parse_wire_decl(L));
      } while (lex_have_next(L, ','));
   llist_reverse(&x->li);
   lex_must_be(L, ';');
   do { llist_prepend(&x->lo, parse_wire_decl(L));
      } while (lex_have_next(L, ','));
   llist_reverse(&x->lo);
   lex_must_be(L, ')');
   return x;
 }

static void *parse_default_wire_decl(lex_tp *L)
 { var_decl *x;
   lex_must_be(L, TOK_id);
   x = new_parse(L, L->prev, 0, var_decl);
   x->id = L->prev->t.val.s;
   if (lex_have_next(L, '['))
     { x->tps = parse_inline_array(L, &generic_bool); }
   else
     { x->tps = (type_spec*)&generic_bool; }
   if (lex_have_next(L, '+') || lex_have_next(L, '-'))
     { x->z_sym = L->prev->t.tp; }
   else if (lex_have_next(L, '='))
     { x->z = parse_expr(L); }
   return x;
 }

static void parse_default_wired_port(lex_tp *L, llist *l)
/* Pre: '(' has been parsed */
 { var_decl *v;
   do { v = parse_default_wire_decl(L);
        SET_FLAG(v->flags, VAR_def_wire | EXPR_wire);
        llist_prepend(l, v);
      } while (lex_have_next(L, ','));
   lex_must_be(L, ';');
   do { v = parse_default_wire_decl(L);
        SET_FLAG(v->flags, VAR_def_wire | EXPR_wire | EXPR_writable);
        llist_prepend(l, v);
      } while (lex_have_next(L, ','));
   lex_must_be(L, ')');
 }

typedef enum {PORT_none = 0, PORT_dir, PORT_sync, PORT_wire} parse_port_type;

static void parse_port_parameter(lex_tp *L, llist *l)
 /* Note: object list is prepended to l in reverse order */
 { var_decl *x;
   parse_port_type curr, old = PORT_none;
   dummy_type *end;
   if (lex_have_next(L, '('))
     { parse_default_wired_port(L, l);
       return;
     }
   end = new_parse(L, L->prev, 0, dummy_type);
   do { lex_must_be(L, TOK_id);
        x = new_parse(L, L->prev, 0, var_decl);
        x->id = L->prev->t.val.s;
        if (lex_have_next(L, '['))
          { x->tps = parse_inline_array(L, end); }
        else
          { x->tps = (type_spec*)end; }
        if (lex_have_next(L, '('))
          { SET_FLAG(x->flags, EXPR_wire); curr = PORT_wire;
            end->tps = parse_wired_type(L);
          }
        else if (lex_have_next(L, '?'))
          { SET_FLAG(x->flags, EXPR_inport); curr = PORT_dir; }
        else if (lex_have_next(L, '!'))
          { SET_FLAG(x->flags, EXPR_outport); curr = PORT_dir; }
        else
          { SET_FLAG(x->flags, EXPR_port); curr = PORT_sync; }
        if (old != PORT_none)
          { if (old == PORT_wire || curr == PORT_wire)
              { lex_error(L, "Wired ports must be seperated by ';'"); }
            if (old == PORT_sync || curr == PORT_sync)
              { lex_error(L, "Sync ports must be seperated by ';'"
                             "(may be missing ? or !)");
              }
          }
        llist_prepend(l, x);
        old = curr;
   } while (lex_have_next(L, ','));
   if (old == PORT_sync)
     { if (lex_have(L, ':'))
         { lex_error(L, "Sync ports have no type (may be missing ? or !)"); }
       end->tps = (type_spec*)new_parse(L, L->prev, 0, generic_type);
     }
   else if (old == PORT_dir)
     { if (lex_have(L, ';') || lex_have(L, ')'))
         { lex_error(L, "Directed ports must have a type"); }
       lex_must_be(L, ':');
       end->tps = parse_type(L);
     }
 }

static parse_meta_parameter(lex_tp *L, llist *l)
 /* Note: object list is prepended to l in reverse order */
 { meta_parameter *x;
   generic_type *g;
   dummy_type *end = new_parse(L, L->prev, 0, dummy_type);
   int have_array = 0;
   /* x, y: t; is parsed as x: t; y: t */
   do { lex_must_be(L, TOK_id);
        x = new_parse(L, L->prev, 0, meta_parameter);
        x->id = L->prev->t.val.s;
        llist_prepend(l, x);
        if (lex_have_next(L, '['))
          { x->tps = parse_inline_array(L, end);
            have_array = 1;
          }
        else
          { x->tps = (type_spec*)end; }
   } while (lex_have_next(L, ','));
   lex_must_be(L, ':');
   if (lex_have_next(L, KW_type)) /* don't allow array of type */
     { if (have_array)
         { lex_error(L, "Expected a type"); }
       g = new_parse(L, L->prev, 0, generic_type);
       g->sym = L->prev->t.tp;
       end->tps = (type_spec*)g;
     } 
   else
     { end->tps = parse_type(L); }
 }

static void *parse_process_definition(lex_tp *L)
 { process_def *x;
   chp_body *b;
   lex_must_be(L, KW_process);
   x = new_parse(L, L->prev, 0, process_def);
   lex_must_be(L, TOK_id);
   x->id = L->prev->t.val.s;
   lex_must_be(L, '(');
   if (starts_meta_parameter(L))
     { do { parse_meta_parameter(L, &x->ml);
            SEPARATOR(L, ';', starts_meta_parameter(L));
       } while (1);
     }
   llist_reverse(&x->ml);
   lex_must_be_one_of(L, 2, ';', ')');
   lex_must_be(L, '(');
   if (starts_port_parameter(L))
     { do { parse_port_parameter(L, &x->pl);
            SEPARATOR(L, ';', starts_port_parameter(L));
       } while (1);
     }
   llist_reverse(&x->pl);
   lex_must_be_one_of(L, 2, ';', ')');
   while (1)
     { if (starts_meta_body(L))
         { if (x->mb) lex_error(L, "Multiple meta bodies encountered");
           x->mb = parse_meta_body(L);
         }
       else if (starts_hse_body(L))
         { if (x->hb) lex_error(L, "Multiple hse bodies encountered");
           x->hb = parse_hse_body(L);
         }
       else if (starts_prs_body(L))
         { if (x->pb) lex_error(L, "Multiple prs bodies encountered");
           x->pb = parse_prs_body(L);
         }
       else if (starts_chp_body(L))
         { if (x->cb) lex_error(L, "Multiple chp bodies encountered");
           x->cb = parse_chp_body(L);
         }
       else if (starts_delay_body(L))
         { if (x->db) lex_error(L, "Multiple delay bodies encountered");
           x->db = parse_delay_body(L);
         }
       else if (starts_property_body(L))
         { if (x->ppb) lex_error(L, "Multiple property bodies encountered");
           x->ppb = parse_property_body(L);
         }
       else break;
     }
   if (!x->mb && !x->cb && !x->hb && !x->pb)
     { lex_error(L, "Expected a process body"); }
   return x;
 }

static void parse_result_parameter(lex_tp *L, llist *l)
 /* Note: object list is prepended to l in reverse order */
 { parameter *x;
   token_tp sym;
   dummy_type *end = new_parse(L, L->prev, 0, dummy_type);
   if (lex_have_next(L, KW_res) || lex_have_next(L, KW_valres))
     { sym = L->prev->t.tp;
       do { lex_must_be(L, TOK_id);
            x = new_parse(L, L->prev, 0, parameter);
            x->par_sym = sym;
            x->d = new_parse(L, 0, x, var_decl);
            x->d->id = L->prev->t.val.s;
            llist_prepend(l, x);
            if (lex_have_next(L, '['))
              { x->d->tps = parse_inline_array(L, end); }
            else
              { x->d->tps = (type_spec*)end; }
       } while (lex_have_next(L, ','));
     }
   else
     { lex_error(L, "Expected 'res' or 'valres'"); }
   lex_must_be(L, ':');
   end->tps = parse_type(L);
 }

static void parse_value_parameter(lex_tp *L, llist *l)
 /* Note: object list is prepended to l in reverse order */
 { parameter *x;
   token_tp sym;
   dummy_type *end = new_parse(L, L->prev, 0, dummy_type);
   sym = lex_have_next(L, KW_const)? KW_const : KW_val;
   lex_have_next(L, KW_val);
   /* x, y: t; is parsed as x: t; y: t */
   do { lex_must_be(L, TOK_id);
        x = new_parse(L, L->prev, 0, parameter);
        x->par_sym = sym;
        x->d = new_parse(L, 0, x, var_decl);
        x->d->id = L->prev->t.val.s;
        llist_prepend(l, x);
        if (lex_have_next(L, '['))
          { x->d->tps = parse_inline_array(L, end); }
        else
          { x->d->tps = (type_spec*)end; }
   } while (lex_have_next(L, ','));
   lex_must_be(L, ':');
   end->tps = parse_type(L);
 }

static void *parse_procedure_definition(lex_tp *L)
 { function_def *x;
   lex_must_be(L, KW_procedure);
   x = new_parse(L, L->prev, 0, function_def);
   lex_must_be(L, TOK_id);
   x->id = L->prev->t.val.s;
   lex_must_be(L, '(');
   if (starts_value_parameter(L) || starts_result_parameter(L)
       || lex_have(L, SYM_varargs))
     {  do { if (starts_value_parameter(L))
               { parse_value_parameter(L, &x->pl); }
             else if (starts_result_parameter(L))
               { parse_result_parameter(L, &x->pl); }
             else if (lex_have_next(L, SYM_varargs))
               { SET_FLAG(x->flags, DEF_varargs);
                 break;
               }
             else
               { lex_error(L, "Expected a parameter"); }
             SEPARATOR(L, ';', (starts_value_parameter(L) ||
                starts_result_parameter(L) || lex_have(L, SYM_varargs)));
        } while (1);
     }
   else if (!lex_have(L, ')'))
     { lex_error(L, "Expected a parameter list"); }
   llist_reverse(&x->pl);
   lex_must_be_one_of(L, 2, ';', ')');
   if (lex_have_next(L, KW_builtin))
     { x->b = 0; SET_FLAG(x->flags, DEF_builtin); }
   else
     { x->b = parse_chp_body(L); }
   return x;
 }

static void *parse_function_definition(lex_tp *L)
 { function_def *x;
   lex_must_be(L, KW_function);
   x = new_parse(L, L->prev, 0, function_def);
   lex_must_be(L, TOK_id);
   x->id = L->prev->t.val.s;
   lex_must_be(L, '(');
   while (!lex_have(L, ')'))
     { if (lex_have_next(L, SYM_varargs))
         { SET_FLAG(x->flags, DEF_varargs);
           break;
         }
       parse_value_parameter(L, &x->pl);
       SEPARATOR(L, ';',
                    (starts_value_parameter(L) || lex_have(L, SYM_varargs)));
     }
   llist_reverse(&x->pl);
   lex_must_be_one_of(L, 2, ';', ')');
   lex_must_be(L, ':');
   x->ret = new_parse(L, 0, x, var_decl);
   x->ret->id = x->id;
   x->ret->tps = parse_type(L);
   if (lex_have_next(L, KW_builtin))
     { x->b = 0; SET_FLAG(x->flags, DEF_builtin); }
   else
     { x->b = parse_chp_body(L); }
   return x;
 }

static void *parse_const_definition(lex_tp *L)
 { const_def *x;
   dummy_type *end;
   lex_must_be(L, KW_const);
   x = new_parse(L, L->prev, 0, const_def);
   lex_must_be(L, TOK_id);
   x->id = L->prev->t.val.s;
   if (lex_have_next(L, '['))
     { end = new_parse(L, L->prev, 0, dummy_type);
       x->tps = parse_inline_array(L, end);
       lex_must_be(L, ':');
       end->tps = parse_type(L);
     }
   else if (lex_have_next(L, ':'))
     { x->tps = parse_type(L); }
   x->z = parse_initializer(L);
   lex_must_be(L, ';');
   return x;
 }

static void *parse_field_definition(lex_tp *L)
 { field_def *x;
   lex_must_be(L, KW_field);
   x = new_parse(L, L->prev, 0, field_def);
   lex_must_be(L, TOK_id);
   x->id = L->prev->t.val.s;
   lex_must_be(L, '=');
   lex_must_be(L, '[');
   x->l = parse_expr(L);
   lex_must_be(L, SYM_dots);
   x->h = parse_expr(L);
   lex_must_be(L, ']');
   lex_must_be(L, ';');
   return x;
 }

static void parse_record_field(lex_tp *L, llist *l)
 /* Note: object list is prepended to l in reverse order */
 { record_field *x;
   dummy_type *end = new_parse(L, L->prev, 0, dummy_type);
   /* x, y: t is parsed as x: t; y: t */
   do { lex_must_be(L, TOK_id);
        x = new_parse(L, L->prev, 0, record_field);
        x->id = L->prev->t.val.s;
        llist_prepend(l, x);
        if (lex_have_next(L, '['))
          { x->tps = parse_inline_array(L, end); }
        else
          { x->tps = (type_spec*)end; }
   } while (lex_have_next(L, ','));
   lex_must_be(L, ':');
   end->tps = parse_type(L);
 }

static void *parse_record_type(lex_tp *L)
 { record_type *x;
   llist m;
   lex_must_be(L, KW_record);
   x = new_parse(L, L->prev, 0, record_type);
   lex_must_be(L, '{');
   do { parse_record_field(L, &x->l);
        SEPARATOR(L, ';', starts_record_field(L));
   } while (starts_record_field(L));
   lex_must_be(L, '}');
   llist_reverse(&x->l);
   return x;
 }

static void *parse_union_field(lex_tp *L)
 { union_field *x;
   lex_must_be(L, TOK_id);
   x = new_parse(L, L->prev, 0, union_field);
   x->id = L->prev->t.val.s;
   lex_must_be(L, '{');
   lex_must_be(L, TOK_id);
   x->dnid = L->prev->t.val.s;
   if (lex_have_next(L, '('))
     { x->dnmb = new_parse(L, L->prev, 0, meta_binding);
       if (starts_expr(L))
         { do { llist_append(&x->dnmb->a, parse_expr(L));
                SEPARATOR(L, ',', starts_expr(L));
              } while (1);
         }
       lex_must_be(L, ')');
     }
   lex_must_be(L, ',');
   lex_must_be(L, TOK_id);
   x->upid = L->prev->t.val.s;
   if (lex_have_next(L, '('))
     { x->upmb = new_parse(L, L->prev, 0, meta_binding);
       if (starts_expr(L))
         { do { llist_append(&x->upmb->a, parse_expr(L));
                SEPARATOR(L, ',', starts_expr(L));
              } while (1);
         }
       lex_must_be(L, ')');
     }
   lex_must_be(L, '}');
   lex_must_be(L, ':');
   x->tps = parse_type(L);
   return x;
 }

static void *parse_union_type(lex_tp *L)
 { union_type *x;
   lex_must_be(L, KW_union);
   x = new_parse(L, L->prev, 0, union_type);
   lex_must_be(L, '{');
   do { if (lex_have_next(L, KW_default))
          { if (x->def) lex_error(L, "Multiple default fields");
            lex_must_be(L, ':');
            x->def = parse_type(L);
          }
        else
          { llist_prepend(&x->l, parse_union_field(L)); }
        SEPARATOR(L, ';', starts_union_field(L));
   } while (starts_union_field(L));
   lex_must_be(L, '}');
   if (!x->def) lex_error(L, "No default field");
   return x;
 }

static void *parse_array_type(lex_tp *L)
 { array_type *x, *y;
   lex_must_be(L, KW_array);
   x = new_parse(L, L->prev, 0, array_type);
   y = x;
   lex_must_be(L, '[');
   do { y->l = parse_expr(L);
        lex_must_be(L, SYM_dots);
        y->h = parse_expr(L);
        /* parse array [i..j, k..l] of T as
           array [i..j] of array [k..l] of T.
        */
        if (lex_have(L, ','))
          { y->tps = (void*)new_parse(L, 0, x, array_type);
            y = (array_type*)y->tps;
          }
        SEPARATOR(L, ',', starts_expr(L));
   } while (1);
   lex_must_be(L, ']');
   lex_must_be(L, KW_of);
   y->tps = parse_type(L);
   return x;
 }

static void *parse_type(lex_tp *L)
 { void *x;
   token_expr *t;
   name *nm;
   integer_type *xi;
   symbol_type *xs;
   named_type *nmtp;
   generic_type *g;
   if (lex_have_next(L, '{')) /* integer_type, symbol_type */
     { x = new_parse(L, L->prev, 0, parse_obj); /* for position */
       t = parse_expr(L);
       if (lex_have_next(L, SYM_dots))
         { xi = new_parse(L, 0, x, integer_type);
           xi->l = (expr*)t;
           xi->h = parse_expr(L);
           free_parse(L, x);
           x = xi;
         }
       else if (t->class == CLASS_token_expr
                 && (t->t.tp == TOK_id || t->t.tp == TOK_symbol)
                 && !IS_SET(t->flags, EXPR_parenthesized))
         { xs = new_parse(L, 0, x, symbol_type);
           nm = new_parse(L, 0, t, name);
           nm->id = t->t.val.s;
           free_parse(L, t);
           llist_prepend(&xs->l, nm);
           do { SEPARATOR(L, ',', lex_have(L, TOK_id) ||
                                  lex_have(L, TOK_symbol));
                if (!lex_have_next(L, TOK_id)) /* Encourage symbol over id */
                  { lex_must_be(L, TOK_symbol); }
                nm = new_parse(L, L->prev, 0, name);
                nm->id = L->prev->t.val.s;
                llist_prepend(&xs->l, nm);
           } while (1);
           llist_reverse(&xs->l);
           free_parse(L, x);
           x = xs;
         }
       else
         { lex_error(L, "Expected '..'"); }
       lex_must_be(L, '}');
     }
   else if (lex_have_next(L, '('))
     { x = parse_wired_type(L); }
   else if (starts_array_type(L))
     { x = parse_array_type(L); }
   else if (starts_record_type(L))
     { x = parse_record_type(L); }
   else if (starts_union_type(L))
     { x = parse_union_type(L); }
   else if (lex_have_next(L, TOK_id))
     { nmtp = new_parse(L, L->prev, 0, named_type);
       nmtp->id = L->prev->t.val.s;
       x = nmtp;
     }
   else if (lex_have_next(L, KW_bool) || lex_have_next(L, KW_int) ||
            lex_have_next(L, KW_symbol))
     { g = new_parse(L, L->prev, 0, generic_type);
       g->sym = L->prev->t.tp;
       x = g;
     }
   else
     { lex_error(L, "Expected a type"); x = NO_INIT; }
   return x;
 }

static void *parse_type_definition(lex_tp *L)
 { type_def *x;
   lex_must_be(L, KW_type);
   x = new_parse(L, L->prev, 0, type_def);
   lex_must_be(L, TOK_id);
   x->id = L->prev->t.val.s;
   lex_must_be(L, '=');
   x->tps = parse_type(L);
   lex_must_be(L, ';');
   return x;
 }

static void *parse_definition(lex_tp *L)
 { void *x;
   if (starts_type_definition(L))
     { x = parse_type_definition(L); }
   else if (starts_const_definition(L))
     { x = parse_const_definition(L); }
   else if (starts_function_definition(L))
     { x = parse_function_definition(L); }
   else if (starts_procedure_definition(L))
     { x = parse_procedure_definition(L); }
   else if (starts_process_definition(L))
     { x = parse_process_definition(L); }
   else if (starts_field_definition(L))
     { x = parse_field_definition(L); }
   else
     { lex_error(L, "Expected a definition"); x = NO_INIT; }
   return x;
 }

static void *parse_property_declaration(lex_tp *L)
 { property_decl *x;
   lex_must_be(L, KW_property);
   x = new_parse(L, L->prev, 0, property_decl);
   lex_must_be(L, TOK_id);
   x->id = L->prev->t.val.s;
   if (lex_have_next(L, '='))
     { x->z = parse_expr(L); }
   lex_must_be(L, ';');
   return x;
 }

static void *parse_global_definition(lex_tp *L)
 { parse_obj *x;
   int export;
   export = lex_have_next(L, KW_export);
   if (starts_property_declaration(L))
     { x = parse_property_declaration(L);
       SET_FLAG(x->flags, DEF_export);
     }
   else
     { x = parse_definition(L);
       if (export)
         { SET_FLAG(x->flags, DEF_export); }
     }
   return x;
 }

static void parse_required_module(lex_tp *L, llist *l)
 /* Note: object list is prepended to l in reverse order */
 { required_module *x;
   lex_must_be(L, KW_requires);
   do { lex_must_be(L, TOK_string);
        x = new_parse(L, L->prev, 0, required_module);
        x->s = L->prev->t.val.s;
        llist_prepend(l, x);
        SEPARATOR(L, ',', lex_have(L, TOK_string));
   } while (1);
   lex_must_be(L, ';');
 }

extern module_def *parse_source_file(lex_tp *L)
 /* Pre: L has been initialized and first symbol has been scanned. */
 { module_def *x;
   x = new_parse(L, L->curr, 0, module_def);
   if (starts_required_module(L) || starts_global_definition(L))
     { while (starts_required_module(L))
         { parse_required_module(L, &x->rl); }
       while (starts_global_definition(L))
         { llist_prepend(&x->dl, parse_global_definition(L)); }
     }
   else if (lex_have(L, TOK_eof))
     { lex_warning(L, "Empty source file"); }
   else
     { lex_error(L, "Expected a required_module or global_definition"); }
   llist_reverse(&x->rl);
   llist_reverse(&x->dl);
   if (!lex_have(L, TOK_eof))
     { lex_error(L, "Expected a global_definition"); }
   return x;
 }
