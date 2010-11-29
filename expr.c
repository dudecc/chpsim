/* Expr.c: expressions

   Author: Marcel van der Goot
*/

#include <standard.h>
#include "print.h"
#include "parse_obj.h"
#include "expr.h"
#include "statement.h"
#include "sem_analysis.h"
#include "types.h"
#include "exec.h"
#include "interact.h"

/********** precedence *******************************************************/

/* precedence values: higher precedence binds tighter;
   equal precedence implies evaluate from left to right (left-associative).
*/
static struct { token_tp op; int p; } precedence_tbl[] =
   { { SYM_concat, 10 },
     { '&', 20 }, { '|', 20 },
     { '=', 30 }, { SYM_neq, 30 },
     { '<', 40 }, { '>', 40 }, { SYM_lte, 40 }, { SYM_gte, 40 },
     { '+', 50 }, { '-', 50 }, { KW_xor, 50 },
     { '*', 60 }, { '/', 60 }, { '%', 60 }, { KW_mod, 60 },
     { '^', 70 }
   };

extern int precedence(token_tp op)
 /* return precedence value of binary operator t */
 { int i;
   for (i = 0; i < CONST_ARRAY_SIZE(precedence_tbl); i++)
     { if (precedence_tbl[i].op == op)
         { return precedence_tbl[i].p; }
     }
   assert(!"operator not in table");
   return 0;
 }


/********** printing *********************************************************/

static void print_binary_expr(binary_expr *x, print_info *f)
 { token_tp l_op, r_op;
   if (x->l->class == CLASS_binary_expr)
     { l_op = ((binary_expr*)x->l)->op_sym; }
   else if (x->l->class == CLASS_rep_expr && IS_SET(f->flags, PR_simple_var))
     { l_op = ((rep_expr*)x->l)->rep_sym; }
   else l_op = 0;
   if (l_op && precedence(l_op) < precedence(x->op_sym))
     { print_char('(', f);
       print_obj(x->l, f);
       print_char(')', f);
     }
   else
     { print_obj(x->l, f); }
   f->pos += var_str_printf(f->s, f->pos, " %s ", token_str(0, x->op_sym));
   if (x->r->class == CLASS_binary_expr)
     { r_op = ((binary_expr*)x->r)->op_sym; }
   else if (x->r->class == CLASS_rep_expr && IS_SET(f->flags, PR_simple_var))
     { r_op = ((rep_expr*)x->r)->rep_sym; }
   else r_op = 0;
   if (r_op && precedence(r_op) <= precedence(x->op_sym))
     { print_char('(', f);
       print_obj(x->r, f);
       print_char(')', f);
     }
   else
     { print_obj(x->r, f); }
 }

static void print_prefix_expr(prefix_expr *x, print_info *f)
 { print_string(token_str(0, x->op_sym), f);
   if (x->r->class == CLASS_binary_expr ||
       (x->r->class == CLASS_rep_expr && IS_SET(f->flags, PR_simple_var)))
     { print_char('(', f);
       print_obj(x->r, f);
       print_char(')', f);
     }
   else
     { print_obj(x->r, f); }
 }

static void print_rep_expr(rep_expr *x, print_info *f)
 { value_tp ival;
   long i, n;
   if (IS_SET(f->flags, PR_simple_var))
     { assert(f->exec);
       n = eval_rep_common(&x->r, &ival, f->exec);
       push_repval(&ival, f->exec->curr, f->exec);
       for (i = 0; i < n; i++)
         { if (i > 0)
             { print_char(' ', f);
               print_string(token_str(0, x->rep_sym), f);
               print_char(' ', f);
             }
           print_obj(x->v, f);
           int_inc(&f->exec->curr->rep_vals->v, f->exec);
         }
       pop_repval(&ival, f->exec->curr, f->exec);
       clear_value_tp(&ival, f->exec);
       return;
     }
   if (IS_SET(f->flags, PR_cast)) print_char('<', f);
   else print_string("<<", f);
   print_string(token_str(0, x->rep_sym), f);
   print_char(' ', f);
   print_string(x->r.id, f);
   print_string(" : ", f);
   print_obj(x->r.l, f);
   print_string("..", f);
   print_obj(x->r.h, f);
   print_string(" : ", f);
   print_obj(x->v, f);
   if (IS_SET(f->flags, PR_cast)) print_char('>', f);
   else print_string(">>", f);
 }

static void print_value_probe(value_probe *x, print_info *f)
 { print_char('#', f);
   print_char('{', f);
   print_obj_list(&x->p, f, ", ");
   print_string(" : ", f);
   print_obj(x->b, f);
   print_char('}', f);
 }

static void print_array_subscript(array_subscript *x, print_info *f)
 { value_tp val;
   if (x->x->class == CLASS_binary_expr || x->x->class == CLASS_prefix_expr)
     { /* can happen if x->x is an integer */
       print_char('(', f);
       print_obj(x->x, f);
       print_char(')', f);
     }
   else
     { print_obj(x->x, f); }
   if (IS_SET(f->flags, PR_simple_var))
     { print_char('_', f);
       assert(f->exec);
       eval_expr(x->idx, f->exec);
       pop_value(&val, f->exec);
       print_value_tp(&val, f);
       clear_value_tp(&val, f->exec);
     }
   else
     { print_char('[', f);
       print_obj(x->idx, f);
       print_char(']', f);
     }
 }

static void print_array_subrange(array_subrange *x, print_info *f)
 { if (x->x->class == CLASS_binary_expr || x->x->class == CLASS_prefix_expr)
     { /* can happen if x->x is an integer */
       print_char('(', f);
       print_obj(x->x, f);
       print_char(')', f);
     }
   else
     { print_obj(x->x, f); }
   print_char('[', f);
   print_obj(x->l, f);
   print_string("..", f);
   print_obj(x->h, f);
   print_char(']', f);
 }

static void print_field_of_record(field_of_record *x, print_info *f)
 { if (x->x->class == CLASS_binary_expr || x->x->class == CLASS_prefix_expr)
     { /* can happen if x->x is an integer */
       print_char('(', f);
       print_obj(x->x, f);
       print_char(')', f);
     }
   else
     { print_obj(x->x, f); }
   if (IS_SET(f->flags, PR_simple_var) && x->x->tp.kind != TP_process)
     { f->pos += var_str_printf(f->s, f->pos, "_%s", x->id); }
   else
     { f->pos += var_str_printf(f->s, f->pos, ".%s", x->id); }
 }

static void print_field_of_union(field_of_union *x, print_info *f)
 { print_obj(x->x, f);
   f->pos += var_str_printf(f->s, f->pos, ".%s", x->id);
 }

static void print_array_constructor(array_constructor *x, print_info *f)
 { print_char('[', f);
   print_obj_list(&x->l, f, ", ");
   print_char(']', f);
 }

static void print_record_constructor(record_constructor *x, print_info *f)
 { print_char('{', f);
   print_obj_list(&x->l, f, ", ");
   print_char('}', f);
 }

static void print_call(call *x, print_info *f)
 { f->pos += var_str_printf(f->s, f->pos, "%s(", x->id);
   print_obj_list(&x->a, f, ", ");
   print_char(')', f);
 }

static void print_token_expr(token_expr *x, print_info *f)
 { switch(x->t.tp)
     { case TOK_int:
         f->pos += var_str_printf(f->s, f->pos, "%ld", x->t.val.i);
       break;
       case TOK_z:
         var_str_ensure(f->s, f->pos + mpz_sizeinbase(x->t.val.z,10)+1);
         mpz_get_str(f->s->s + f->pos, 10, x->t.val.z);
         f->pos += strlen(f->s->s + f->pos);
       break;
       case TOK_char:
         if (isprint(x->t.val.i))
           { f->pos += var_str_printf(f->s, f->pos,
                                      "'%c'", (int)x->t.val.i);
           }
         else
           { f->pos += var_str_printf(f->s, f->pos,
                                      "0x%02X", (int)x->t.val.i);
           }
       break;
       case TOK_string:
         f->pos += var_str_printf(f->s, f->pos,
                             "\"%s\"", x->t.val.s); /* TODO: escapes */
       break;
       case TOK_id:
         print_string(x->t.val.s, f);
       break;
       case TOK_float:
         f->pos += var_str_printf(f->s, f->pos, "%g", x->t.val.d);
       break;
       case KW_true: case KW_false:
         print_string(token_str(0, x->t.tp), f);
       break;
       default:
         assert(!"Non-existing token type");
       break;
     }
 }

static void print_symbol(symbol *x, print_info *f)
 { print_char('`', f);
   print_string(x->s, f);
 }

static void print_var_ref(var_ref *x, print_info *f)
 { print_string(x->d->id, f); }

static void print_rep_var_ref(rep_var_ref *x, print_info *f)
 { print_string(x->rs? x->rs->r.id : x->re->r.id, f); }

static void print_const_ref(const_ref *x, print_info *f)
 { print_string(x->d->id, f); }

static void print_meta_ref(meta_ref *x, print_info *f)
 { print_string(x->d->id, f); }

static void print_wire_ref(wire_ref *x, print_info *f)
 { print_obj(x->x, f); }

static void print_const_expr(const_expr *x, print_info *f)
 { print_obj(x->x, f); }

static void print_implicit_array(implicit_array *x, print_info *f)
 { print_obj(x->x, f); }

static void print_type_expr(type_expr *x, print_info *f)
 { print_char('<', f);
   print_obj(x->tps, f);
   print_char('>', f);
 }


/********** semantic analysis ************************************************/

DEF_CLASS_B(const_expr, expr, 0, 0, OBJ_zero);

extern expr *mk_const_expr(expr *x, sem_info *f)
 /* if x is a const expr, insert a const_expr object; return is x or
    the const_expr
 */
 { const_expr *c;
   if (!IS_SET(x->flags, EXPR_nocexpr))
     { c = new_parse(f->L, 0, x, const_expr);
       c->flags = x->flags;
       c->tp = x->tp;
       c->x = x;
       if (IS_SET(x->flags, EXPR_metax))
         { c->meta_idx = f->meta_idx++; }
       x = (expr*)c;
     }
   return x;
 }

extern void mk_const_expr_list(llist *l, sem_info *f)
 /* llist(expr) *l; For each constant in l, replace it by a const_expr */
 { llist_apply_overwrite(l, (llist_func_p*)mk_const_expr, f);
 }

static void *sem_wire_fix(void *x, sem_info *f)
 { expr *e = x;
   wire_ref *w;
   if (e->tp.kind == TP_bool && IS_SET(e->flags, EXPR_wire)
                             && !IS_SET(f->flags, SEM_meta | SEM_prs))
     { w = new_parse(f->L, 0, e, wire_ref);
       w->flags = e->flags;
       w->tp = e->tp;
       w->x = e;
       RESET_FLAG(w->flags, EXPR_wire);
       return w;
     }
   return x;
 }

static void *sem_binary_expr(binary_expr *x, sem_info *f)
 { int bool_res = 0;
   type *ltp, *rtp;
   x->l = sem(x->l, f);
   x->r = sem(x->r, f);
   SET_IF_SET(x->flags, x->r->flags | x->l->flags, EXPR_all_constx);
   if (IS_SET(x->flags, EXPR_nocexpr))
     { x->l = mk_const_expr(x->l, f);
       x->r = mk_const_expr(x->r, f);
     }
   ltp = &x->l->tp;
   rtp = &x->r->tp;
   switch (x->op_sym)
     { case '<': case '>': case SYM_lte: case SYM_gte:
          bool_res = 1;
       /* fall-through */
       case '+': case '-': case '*': case '/': case '%': case '^': case KW_mod:
          if (ltp->kind != TP_int || rtp->kind != TP_int)
            { sem_error(f, x, "'%s' operator requires integer operands",
                            token_str(0, x->op_sym));
            }
          if (bool_res) x->tp.kind = TP_bool;
          else x->tp = generic_int.tp;
       break;
       case SYM_concat:
          if (ltp->kind != TP_array || rtp->kind != TP_array)
            { sem_error(f, x, "'++' operator requires array operands"); }
          if (!type_compatible(ltp->elem.tp, rtp->elem.tp))
            { sem_error(f, x, "Concatenation of incompatible arrays"); }
          x->tp.kind = TP_array;
          x->tp.elem.tp = ltp->elem.tp;
       break;
       case '=': case SYM_neq:
          if (!type_compatible(ltp, rtp))
            { sem_error(f, x, "Incompatible operand types for '%s'",
              token_str(0, x->op_sym));
            }
          x->tp.kind = TP_bool;
       break;
       case '&': case '|': case KW_xor:
          if (ltp->kind != rtp->kind)
            { sem_error(f, x, "Incompatible operand types for '%s'",
              token_str(0, x->op_sym));
            }
          if (ltp->kind != TP_bool && ltp->kind != TP_int)
            { sem_error(f, x, "Operator '%s' requires bool or int operands",
              token_str(0, x->op_sym));
            }
          if (ltp->kind == TP_bool) x->tp.kind = TP_bool;
          else x->tp = generic_int.tp;
       break;
       default:
          assert(!"An unknown binary operator");
       break;
     }
   if (IS_SET(x->l->flags | x->r->flags, EXPR_port))
     { sem_error(f, x, "Binary expressions may not contain a port"); }
   return x;
 }

static void *sem_prefix_expr(prefix_expr *x, sem_info *f)
 { sem_flags flags = f->flags;
   type *rtp;
   x->r = sem(x->r, f);
   SET_IF_SET(x->flags, x->r->flags, EXPR_all_constx);
   rtp = &x->r->tp;
   if (x->op_sym == '+' || x->op_sym == '-')
     { if (rtp->kind != TP_int)
         { sem_error(f, x, "'%c' requires an integer operand", x->op_sym); }
       x->tp = generic_int.tp;
       x->tp.kind = TP_int;
     }
   else if (x->op_sym == '~')
     { if (rtp->kind != TP_int && rtp->kind != TP_bool)
         { sem_error(f, x, "'~' requires an integer or boolean operand"); }
       if (rtp->kind == TP_bool) x->tp.kind = TP_bool;
       else x->tp = generic_int.tp;
     }
   else if (x->op_sym == '#')
     { if (IS_SET(f->flags, SEM_meta))
         { sem_error(f, x, "A '#' cannot occur in a meta process"); }
       if (!IS_SET(x->r->flags, EXPR_port))
         { sem_error(f, x->r, "'#' requires a port operand"); }
       x->tp.kind = TP_bool;
       x->class = CLASS_probe;
     }
   else
     { assert(!"An unknown prefix operator"); }
   if (x->op_sym != '#' && IS_SET(x->r->flags,EXPR_port))
     { sem_error(f, x, "Prefix expressions may not contain a port"); }
   f->flags = flags;
   return x;
 }

static void *sem_rep_expr(rep_expr *x, sem_info *f)
 { type *vtp;
   parse_obj_flags fl;
   sem_rep_common(&x->r, f);
   enter_sublevel(x, x->r.id, &x->cxt, f);
   x->v = sem(x->v, f);
   fl = x->v->flags | x->r.l->flags | x->r.h->flags;
   SET_IF_SET(x->flags, fl, EXPR_all_constx);
   if (IS_SET(fl, EXPR_rep) && f->rv_ref_depth == 0)
     { RESET_FLAG(x->flags, EXPR_rep); }
     /* if x->v references rep vars, but the outermost rep var referenced is x's
      * own var, then x itself no longer depends on rep vars.
      */
   if (IS_SET(x->flags, EXPR_nocexpr))
     { x->v = mk_const_expr(x->v, f);
       x->r.l = mk_const_expr(x->r.l, f);
       x->r.h = mk_const_expr(x->r.h, f);
     }
   vtp = &x->v->tp;
   switch (x->rep_sym)
     { case '+': case '*':
          if (vtp->kind != TP_int)
            { sem_error(f, x, "'%s' operator requires integer operands",
                            token_str(0, x->rep_sym));
            }
          x->tp = generic_int.tp;
       break;
       case SYM_concat:
          if (vtp->kind != TP_array)
            { sem_error(f, x, "'++' operator requires array operands"); }
          x->tp.kind = TP_array;
          x->tp.elem.tp = vtp->elem.tp;
       break;
       case '=': case SYM_neq:
          if (vtp->kind != TP_bool)
            { sem_error(f, x, "Incompatible operand type for replicated '%s'",
              token_str(0, x->rep_sym));
            }
          x->tp.kind = TP_bool;
       break;
       case '&': case '|': case KW_xor:
          if (vtp->kind != TP_bool && vtp->kind != TP_int)
            { sem_error(f, x, "Operator '%s' requires bool or int operands",
              token_str(0, x->rep_sym));
            }
           if (vtp->kind == TP_bool) x->tp.kind = TP_bool;
           else x->tp = generic_int.tp;
       break;
       default:
          assert(!"An unknown replicator operator");
       break;
     }
   if (IS_SET(x->v->flags,EXPR_port))
     { sem_error(f, x, "Replicated expression contains port"); }
   leave_level(f);
   return x;
 }

static void *sem_value_probe(value_probe *x, sem_info *f)
 { llist m;
   expr *p;
   sem_flags flags = f->flags;
   if (IS_SET(f->flags, SEM_meta))
     { sem_error(f, x, "A '#' cannot occur in a meta process"); }
   sem_list(&x->p, f);
   m = x->p;
   while (!llist_is_empty(&m))
     { p = llist_head(&m);
       if (IS_SET(p->flags, EXPR_port) != EXPR_inport)
         { sem_error(f, p, "'#' needs input port operands before the colon"); }
       m = llist_alias_tail(&m);
     }
   SET_FLAG(f->flags, SEM_value_probe);
   SET_FLAG(x->flags, EXPR_unconst);
   x->b = sem(x->b, f);
   f->flags = flags;
   if (IS_SET(x->b->flags, EXPR_port))
     { sem_error(f, x, "Value probe contains non input port"); }
   if (x->b->tp.kind != TP_bool)
     { sem_error(f, x->b, "'#' needs a boolean expression after the colon"); }
   x->b = mk_const_expr(x->b, f); /* TODO: maybe warn if const */
   x->tp.kind = TP_bool;
   return x;
 }

static void *sem_array_subscript(array_subscript *x, sem_info *f)
 { type *tp;
   x->x = sem(x->x, f);
   x->idx = sem(x->idx, f);
   SET_IF_SET(x->flags, x->x->flags | x->idx->flags, EXPR_all_constx);
   if (IS_SET(x->flags, EXPR_nocexpr))
     { x->x = mk_const_expr(x->x, f);
       x->idx = mk_const_expr(x->idx, f);
     }
   SET_IF_SET(x->flags, x->x->flags, EXPR_inherit);
   if (x->idx->tp.kind != TP_int)
     { sem_error(f, x, "Index is not an integer"); }
   if (IS_SET(x->idx->flags,EXPR_port))
     { sem_error(f, x, "Array index is a port"); }
   tp = &x->x->tp;
   if (tp->kind == TP_array)
     { if (!tp->tps)
         { sem_error(f, x, "Subscript applied to array with unknown bounds"); }
       else if (tp->tps == (type_spec*)builtin_string)
         { sem_error(f, x, "Subscript applied to builtin string type"); }
       x->tp = *tp->elem.tp;
     }
   else if (tp->kind == TP_int)
     { x->tp.kind = TP_bool;
       if (!IS_SET(x->flags, EXPR_port))
         { x->class = CLASS_int_subscript; }
       else if (!IS_SET(f->flags, SEM_connect))
         { sem_error(f, x, "Cannot slice integer port in chp body"); }
     }
   else
     { sem_error(f, x, "Subscript applied to something that is not an array"); }
   return sem_wire_fix(x, f);
 }

static void *sem_array_subrange(array_subrange *x, sem_info *f)
 { type *tp;
   parse_obj_flags fl;
   x->x = sem(x->x, f);
   x->l = sem(x->l, f);
   x->h = sem(x->h, f);
   fl = x->x->flags | x->l->flags | x->h->flags;
   SET_IF_SET(x->flags, fl, EXPR_all_constx);
   if (IS_SET(x->flags, EXPR_nocexpr))
     { x->l = mk_const_expr(x->l, f);
       x->h = mk_const_expr(x->h, f);
       x->x = mk_const_expr(x->x, f);
     }
   SET_IF_SET(x->flags, x->x->flags, EXPR_inherit);
   if (x->l->tp.kind != TP_int)
     { sem_error(f, x, "Lower index is not an integer"); }
   if (x->h->tp.kind != TP_int)
     { sem_error(f, x, "Upper index is not an integer"); }
   if (IS_SET(x->l->flags,EXPR_port))
     { sem_error(f, x, "Array index is a port"); }
   if (IS_SET(x->h->flags,EXPR_port))
     { sem_error(f, x, "Array index is a port"); }
   if (IS_SET(x->flags, EXPR_port) && IS_SET(f->flags, SEM_connect))
     { sem_error(f, x, "Cannot use array subrange in connection"
                       " - use 'connect all' instead");
     }
   tp = &x->x->tp;
   if (tp->kind == TP_array)
     { if (!tp->tps)
         { sem_error(f, x, "Subrange of array with unknown bounds"); }
       else if (tp->tps == (type_spec*)builtin_string)
         { sem_error(f, x, "Subrange of builtin string type"); }
       x->tp.kind = TP_array;
       x->tp.elem.tp = tp->elem.tp;
     }
   else if (tp->kind == TP_int)
     { x->tp = generic_int.tp;
       if (IS_SET(x->flags, EXPR_port))
         { sem_error(f, x, "Cannot slice integer port in chp body"); }
     }
   else
     { sem_error(f, x, "Subrange of something that is not an array"); }
   return x;
 }

/* Since a field of a union gets parsed in as a field of record, we
   call this function to check if a field of record could actually be
   a field of union.  If so, we free the field of record, and return a
   field of union.

   If not, then unless we are pretty sure that this was intended as an
   access to a union field, we return 0 and let the calling function
   output its own error message.
 */
static void *try_union(field_of_record *x, sem_info *f)
 { field_of_union *uf;
   union_type *utp;
   union_field *r = NO_INIT;
   llist m;
   if (!x->x->tp.utps)
     { return 0; }
   uf = new_parse(f->L, 0, x, field_of_union);
   uf->flags = x->flags; uf->x = x->x;
   utp = (union_type*)uf->x->tp.utps;
   m = utp->l;
   while (!llist_is_empty(&m))
     { r = llist_head(&m);
       if (r->id == x->id) break;
       m = llist_alias_tail(&m);
     }
   if (llist_is_empty(&m))
     { return 0; } /* probably not intended to be union */
   sem_free_obj(f, x);
   uf->d = r;
   uf->id = r->id;
   uf->tp = r->tp;
   if (!IS_SET(uf->flags, EXPR_port) || !IS_SET(f->flags, SEM_connect))
     { sem_error(f, x, "Cannot access a union field except with 'connect'"); }
   if (uf->tp.kind == TP_wire)
     { SET_FLAG(uf->flags, EXPR_wire); }
   return uf;
 }

static void *sem_field_of_record(field_of_record *x, sem_info *f)
 { record_type *tps;
   wired_type *wtps;
   llist m;
   record_field *r = NO_INIT;
   wire_decl *w = NO_INIT;
   var_decl *d = NO_INIT;
   type *tp;
   void *z;
   int i, j;
   parse_obj_flags fl;
   x->x = sem(x->x, f);
   tp = &x->x->tp;
   if (tp->kind == TP_process && IS_SET(f->flags, SEM_connect))
     { m = tp->elem.p->pl;
       while (!llist_is_empty(&m))
         { d = llist_head(&m);
           if (d->id == x->id) break;
           m = llist_alias_tail(&m);
         }
       if (llist_is_empty(&m))
         { sem_error(f, x, "Process %s has no port '%s'",
                     tp->elem.p->id, x->id);
         }
       x->tp = d->tp;
       x->idx = d->var_idx;
       SET_FLAG(x->flags, EXPR_port_ext | EXPR_unconst);
       SET_IF_SET(x->flags, d->flags, EXPR_port | EXPR_wire);
       SET_IF_SET(x->flags, x->tp.tps->flags, EXPR_inherit);
       if (IS_SET(d->tps->flags, EXPR_generic)); /* Wait, what goes here? */
       return x;
     }
   SET_IF_SET(x->flags, x->x->flags, EXPR_all_constx);
   SET_IF_SET(x->flags, x->x->flags, EXPR_inherit);
   if (tp->kind == TP_int)
     { if ((z = try_union(x, f))) return z;
       x->field = find_id(f, x->id, x);
       if (x->field->class != CLASS_field_def)
         { sem_error(f, x, "'%s' is not an integer field", x->id); }
       x->tp = generic_int.tp;
       if (IS_SET(x->flags, EXPR_port) && !IS_SET(f->flags, SEM_connect))
         { sem_error(f, x, "Cannot slice integer port in chp body"); }
       else if (IS_SET(x->flags, EXPR_port))
         { sem_error(f, x, "Cannot use integer field in connection"
                           " - use 'connect all' instead");
         }
       return x;
     }
   else if (tp->kind == TP_wire)
     { wtps = (wired_type*)tp->tps;
       assert(wtps && wtps->class == CLASS_wired_type);
       if (x->x->class == CLASS_field_of_union)
         /* Decomposed wired port, must determine "direction" */
         { fl = ((field_of_union*)x)->x->flags;
           if (!IS_SET(fl, EXPR_port_ext))
             { fl = fl ^ EXPR_port; }
           if (LI_IS_WRITE(fl, wtps->type))
             { SET_FLAG(x->flags, EXPR_writable); }
         }
       else if (LI_IS_WRITE(x->flags, wtps->type))
         { SET_FLAG(x->flags, EXPR_writable); }
       i = 0;
       m = wtps->li;
       for (j = 0; j < 2; j++)
         { while (!llist_is_empty(&m))
             { w = llist_head(&m);
               if (w->id == x->id)
                 { x->tp = w->tps->tp;
                   x->idx = i;
                   RESET_FLAG(x->flags, EXPR_port);
                   return sem_wire_fix(x, f);
                 }
               i++;
               m = llist_alias_tail(&m);
             }
           m = wtps->lo;
           x->flags = x->flags ^ EXPR_writable;
         }
       if ((z = try_union(x, f))) return z;
       sem_error(f, x, "Wired port has no wire '%s'", x->id);
     }
   else if (tp->kind != TP_record)
     { if ((z = try_union(x, f))) return z;
       sem_error(f, x, "Field of something that is not a record");
     }
   tps = (record_type*)tp->tps;
   if (!tps || tps->class != CLASS_record_type)
     { if ((z = try_union(x, f))) return z;
       sem_error(f, x, "Field of a record with unknown field names");
     }
   i = 0;
   m = tps->l;
   while (!llist_is_empty(&m))
     { r = llist_head(&m);
       if (r->id == x->id) break;
       i++;
       m = llist_alias_tail(&m);
     }
   if (llist_is_empty(&m))
     { if ((z = try_union(x, f))) return z;
       sem_error(f, x, "Record has no field '%s'", x->id);
     }
   x->tp = r->tps->tp;
   x->idx = i;
   return sem_wire_fix(x, f);
 }

static void *sem_array_constructor(array_constructor *x, sem_info *f)
 { llist m;
   expr *e;
   int4 i = 0;
   sem_list(&x->l, f);
   x->tp.kind = TP_array;
   e = llist_head(&x->l);
   x->tp.elem.tp = &e->tp;
   m = x->l;
   while (!llist_is_empty(&m) && i <= ARRAY_REP_MAXSIZE)
     { e = llist_head(&m);
       SET_IF_SET(x->flags, e->flags, EXPR_all_constx);
       if (IS_SET(e->flags,EXPR_port))
         { sem_error(f, x, "Array constructor contains a port"); }
       if (!type_compatible(x->tp.elem.tp, &e->tp))
         { sem_error(f, e, "Array elements have conflicting types"); }
       i++;
       m = llist_alias_tail(&m);
     }
   if (i > ARRAY_REP_MAXSIZE)
     { sem_error(f, x, "Implementation limit exceeded: Array size > %ld",
                    (long)ARRAY_REP_MAXSIZE);
     }
   if (IS_SET(x->flags, EXPR_nocexpr))
     { mk_const_expr_list(&x->l, f); }
   return x;
 }

static void *sem_record_constructor(record_constructor *x, sem_info *f)
 { llist m;
   expr *e;
   sem_list(&x->l, f);
   x->tp.kind = TP_record;
   llist_init(&x->tp.elem.l);
   m = x->l;
   while (!llist_is_empty(&m))
     { e = llist_head(&m);
       SET_IF_SET(x->flags, e->flags, EXPR_all_constx);
       if (IS_SET(e->flags,EXPR_port))
         { sem_error(f, x, "Record constructor contains port"); }
       llist_prepend(&x->tp.elem.l, &e->tp);
       m = llist_alias_tail(&m);
     }
   llist_reverse(&x->tp.elem.l);
   if (IS_SET(x->flags, EXPR_nocexpr))
     { mk_const_expr_list(&x->l, f); }
   return x;
 }

static void *sem_call(call *x, sem_info *f)
 { function_def *d;
   parameter *p;
   expr *a;
   llist ma, mp;
   meta_binding *y;
   token_expr *te;
   x->d = d = find_id(f, x->id, x);
   if (d->class == CLASS_instance_stmt)
     { y = new_parse(f->L, 0, x, meta_binding);
       te = new_parse(f->L, 0, x, token_expr);
       te->t.tp = TOK_id;
       te->t.val.s = (char *)x->id;
       y->x = (expr*)te;
       llist_transfer(&y->a, &x->a);
       sem_free_obj(f, x);
       SET_FLAG(y->flags, DEF_forward);
       return sem(y, f);
     }
   if (d->class == CLASS_process_def)
     { sem_error(f, x, "%s is a process, not a process instance", x->id); }
   else if (d->class != CLASS_function_def)
     { sem_error(f, x, "%s is not a function/procedure", x->id); }
   sem_list(&x->a, f);
   ma = x->a;
   mp = x->d->pl;
   if (IS_SET(d->flags, DEF_varargs))
     { if (llist_size(&ma) < llist_size(&mp))
         { sem_error(f, x, "Insufficient arguments for %s", x->id); }
     }
   else if (llist_size(&ma) != llist_size(&mp))
     { sem_error(f, x, "Wrong number of arguments for %s", x->id); }
   while (!llist_is_empty(&mp))
     { p = llist_head(&mp);
       a = llist_head(&ma);
       if (IS_SET(a->flags,EXPR_port))
         { sem_error(f, a, "Argument for parameter %s is a port", p->d->id); }
       if (!type_compatible(&p->d->tp, &a->tp))
         { sem_error(f, a, "Argument type mismatch for parameter %s", p->d->id); }
       if ((p->par_sym == KW_res || p->par_sym == KW_valres)
           && !IS_SET(a->flags, EXPR_writable))
         { sem_error(f, a, "Argument for result parameter %s must be writable",
                     p->d->id);
         }
       SET_IF_SET(x->flags, a->flags, EXPR_all_constx);
       mp = llist_alias_tail(&mp);
       ma = llist_alias_tail(&ma);
     }
   if (x->d->ret)
     { x->tp = x->d->ret->tp; }
   else
     { SET_FLAG(x->flags, EXPR_unconst); }
   if (IS_SET(x->d->flags, DEF_builtin)) /* For random(), etc */
     { SET_FLAG(x->flags, EXPR_unconst); }
   if (IS_SET(x->flags, EXPR_nocexpr))
     { mk_const_expr_list(&x->a, f); }
   return x;
 }

static void *sem_token_expr(token_expr *x, sem_info *f)
 { var_decl *dv;
   const_def *dc;
   parameter *dp;
   meta_parameter *mp;
   rep_var_ref *r;
   var_ref *y;
   const_ref *c;
   meta_ref *mx;
   function_def *df;
   instance_stmt *di;
   call *g;
   if (x->t.tp != TOK_id) /* literal */
     { if (x->t.tp == TOK_int || x->t.tp == TOK_z || x->t.tp == TOK_char)
         { x->tp = generic_int.tp; }
       else if (x->t.tp == KW_true || x->t.tp == KW_false)
         { x->tp.kind = TP_bool; }
       else if (x->t.tp == TOK_string)
         { x->tp.kind = TP_array;
           x->tp.elem.tp = &generic_int.tp;
         }
       else if (x->t.tp == TOK_float)
         { sem_error(f, x, "Floating point numbers are not supported."); }
       return x;
     }
   df = find_id(f, x->t.val.s, x);
   if (df->class == CLASS_symbol_type)
     { x->tp.kind = TP_symbol;
       return x;
     }
   else if (df->class == CLASS_function_def &&
           !(IS_SET(f->flags, SEM_need_lvalue) && df == f->curr_routine
              && df->ret))
     { g = new_parse(f->L, 0, x, call);
       g->id = df->id;
       g->d = df;
       sem_free_obj(f, x);
       g = sem(g, f);
       return g;
     }
   else if (df->class == CLASS_const_def)
     { dc = (const_def*)df;
       c = new_parse(f->L, 0, x, const_ref);
       c->d = dc;
       c->z = dc->z;
       c->tp = dc->tp;
       SET_IF_SET(c->flags, c->z->flags, EXPR_all_constx);
       sem_free_obj(f, x);
       return c;
     }
   else if (df->class == CLASS_meta_parameter)
     { mp = (meta_parameter*)df;
       if (mp->tp.kind == TP_type)
         { sem_error(f, x, "Expected expression, found type %s", mp->id); }
       mx = new_parse(f->L, 0, x, meta_ref);
       mx->d = mp;
       mx->meta_idx = mp->meta_idx;
       SET_FLAG(mx->flags, EXPR_metax);
       mx->tp = mp->tp;
       sem_free_obj(f, x);
       return mx;
     }
   else if (df->class == CLASS_rep_stmt)
     { r = new_parse(f->L, 0, x, rep_var_ref);
       r->rs = (rep_stmt*)df;
       r->rep_idx = find_level(f, x->t.val.s);
       SET_FLAG(r->flags, EXPR_rep);
       r->tp = generic_int.tp;
       sem_free_obj(f, x);
       return r;
     }
   else if (df->class == CLASS_rep_expr)
     { r = new_parse(f->L, 0, x, rep_var_ref);
       r->re = (rep_expr*)df;
       r->rep_idx = find_level(f, x->t.val.s);
       SET_FLAG(r->flags, EXPR_rep);
       r->tp = generic_int.tp;
       sem_free_obj(f, x);
       return r;
     }
   /* ref to var (or parameter or function return location) */
   y = new_parse(f->L, 0, x, var_ref);
   SET_FLAG(y->flags, EXPR_unconst);
   if (df->class == CLASS_var_decl)
     { dv = (var_decl*)df;
       if (!IS_SET(df->flags, EXPR_port | EXPR_wire))
         { SET_FLAG(y->flags, EXPR_writable); }
       SET_IF_SET(y->flags, dv->flags, EXPR_writable);
     }
   else if (df->class == CLASS_parameter)
     { dp = (parameter*)df;
       dv = dp->d;
       if (dp->par_sym != KW_const)
         { SET_FLAG(y->flags, EXPR_writable); }
       else
         { RESET_FLAG(y->flags, EXPR_unconst);
           SET_FLAG(y->flags, EXPR_cparam);
         }
     }
   else if (df->class == CLASS_function_def)
     { dv = df->ret;
       SET_FLAG(y->flags, EXPR_writable);
     }
   else if (df->class == CLASS_instance_stmt)
     { di = (instance_stmt*)df;
       dv = di->d;
     }
   else
     { sem_error(f, x, "Improper use of '%s'", x->t.val.s); dv = NO_INIT; }
   SET_IF_SET(y->flags, dv->tp.tps->flags, EXPR_inherit);
   SET_IF_SET(y->flags, df->flags, EXPR_port | EXPR_wire);
   y->d = dv;
   y->var_idx = dv->var_idx;
   if (IS_SET(f->flags, SEM_value_probe))
     { RESET_FLAG(y->flags, EXPR_inport); }
   y->tp = dv->tp;
   sem_free_obj(f, x);
   return sem_wire_fix(y, f);
 }

static void *sem_symbol(symbol *x, sem_info *f)
 { x->tp.kind = TP_symbol;
   return x;
 }

static void *sem_type_expr(type_expr *x, sem_info *f)
 { if (!IS_SET(f->flags, SEM_meta_binding))
     { sem_error(f, x, "Type expression may only appear in meta binding"); }
   x->tp.kind = TP_type;
   x->tps = sem(x->tps, f); /* may declare symbol literals, but we probably don't care */
   return x;
 }

/********** evaluation1 ******************************************************
 * The first evaluation section contain actual computation - functions that
 * perform arithmetic and bit logic, which are primarily used in evaluating
 * binary and prefix expressions (and replicated binary expressions)
 *****************************************************************************/

extern long eval_rep_common(rep_common *x, value_tp *v, exec_info *f)
 /* set v to value of low replicator bound, returns # of values in bounds */
 { value_tp lval, hval, dval;
   eval_expr(x->h, f);
   eval_expr(x->l, f);
   pop_value(&lval, f);
   pop_value(&hval, f);
   if (int_cmp(&hval, &lval, f) < 0)
     { exec_error(f, f->curr->obj,
           "Replication lower bound %v = %v exceeds upper bound %v = %v",
           vstr_obj, x->l, vstr_val, &lval, vstr_obj, x->h, vstr_val, &hval);
     }
   copy_value_tp(v, &lval, f);
   dval = int_sub(&hval, &lval, f);
   int_simplify(&dval, f);
   if (dval.rep != REP_int || dval.v.i > ARRAY_REP_MAXSIZE)
     { clear_value_tp(v, f);
       exec_error(f, x, "Implementation limit exceeded: Replication length "
                  "%v > %ld", vstr_val, &dval, (long)ARRAY_REP_MAXSIZE);
     }
   return dval.v.i + 1;
 } 

static int equal_value(value_tp *v, value_tp *w, exec_info *f, void *obj)
 /* Pre: v and w have compatible types.
    1 if equal values, 0 if not.
    obj is used for warnings. (v and w are not cleared)
 */
 { int r = 0;
   int4 n, i;
   if (!v->rep || !w->rep)
     { exec_warning(f, obj, "Testing expression with unknown value");
       return 0;
     }
   switch (v->rep)
     { case REP_bool: r = (v->v.i == w->v.i);
       break;
       case REP_int:
            if (w->rep == REP_int)
              { r = (v->v.i == w->v.i); }
            else
              { r = !mpz_cmp_si(w->v.z->z, v->v.i); }
       break;
       case REP_z:
            if (w->rep == REP_z)
              { r = !mpz_cmp(v->v.z->z, w->v.z->z); }
            else
              { r = !mpz_cmp_si(v->v.z->z, w->v.i); }
       break;
       case REP_symbol: r = (v->v.s == w->v.s);
       break;
       case REP_array: case REP_record:
        n = v->v.l->size;
        if (n == w->v.l->size)
          { i = 0;
            r = 1;
            while (r && n)
              { r = equal_value(&v->v.l->vl[i], &w->v.l->vl[i], f, obj);
                i++; n--;
              }
          }
       break;
       default: assert(!"Unknown representation");
       break;
     }
   return r;
 }

extern void int_simplify(value_tp *x, exec_info *f)
 /* if possible, replace x by REP_int rather than REP_z */
 { long i;
   if (x->rep == REP_z && mpz_fits_slong_p(x->v.z->z))
     { i = mpz_get_si(x->v.z->z);
       clear_value_tp(x, f);
       x->rep = REP_int;
       x->v.i = i;
     }
 }

extern value_tp int_add(value_tp *l, value_tp *r, exec_info *f)
 /* Pre: l->rep && r->rep, l != r.
    return l + r; clears *l and *r.
 */
 { value_tp x, *tmp;
   long li, ri, sign;
   if (l->rep == REP_int && r->rep == REP_int)
     { x.rep = REP_int;
       li = l->v.i; ri = r->v.i;
       x.v.i = li + ri;
       sign = x.v.i & INT_REP_SIGN;
       if ((INT_REP_SIGN & li) != sign && (INT_REP_SIGN & ri) != sign)
         { /* overflow */
           x.rep = REP_z;
           x.v.z = new_z_value(f);
           mpz_set_si(x.v.z->z, li);
           l = &x;
         }
       else
         { return x; }
     }
   if (l->rep == REP_int)
     { tmp = l; l = r; r = tmp; }
   /* l->rep == REP_z */
   copy_and_clear(&x, l, f);
   if (r->rep == REP_int)
     { ri = r->v.i;
       if (ri >= 0)
         { mpz_add_ui(x.v.z->z, x.v.z->z, (ulong)ri); }
       else
         { mpz_sub_ui(x.v.z->z, x.v.z->z, (ulong)-ri); }
     }
   else
     { mpz_add(x.v.z->z, x.v.z->z, r->v.z->z);
       clear_value_tp(r, f);
     }
   return x;
 }

extern value_tp int_sub(value_tp *l, value_tp *r, exec_info *f)
 /* Pre: l->rep && r->rep, l != r.
    return l - r; clears *l and *r.
 */
 { value_tp x, *tmp;
   long li, ri, sign;
   int neg = 0; /* if 1, then compute -(r - l) */
   if (l->rep == REP_int && r->rep == REP_int)
     { x.rep = REP_int;
       li = l->v.i; ri = r->v.i;
       x.v.i = li - ri;
       sign = x.v.i & INT_REP_SIGN;
       if ((INT_REP_SIGN & li) != sign && (INT_REP_SIGN & ri) == sign)
         { /* overflow */
           x.rep = REP_z;
           x.v.z = new_z_value(f);
           mpz_set_si(x.v.z->z, li);
           l = &x;
         }
       else
         { return x; }
     }
   if (l->rep == REP_int)
     { tmp = l; l = r; r = tmp; neg = 1; }
   /* l->rep == REP_z */
   copy_and_clear(&x, l, f);
   if (r->rep == REP_int)
     { ri = r->v.i;
       if (ri >= 0)
         { mpz_sub_ui(x.v.z->z, x.v.z->z, (ulong)ri); }
       else
         { mpz_add_ui(x.v.z->z, x.v.z->z, (ulong)-ri); }
       if (neg)
         { mpz_neg(x.v.z->z, x.v.z->z); }
     }
   else
     { mpz_sub(x.v.z->z, x.v.z->z, r->v.z->z);
       clear_value_tp(r, f);
     }
   return x;
 }

extern void int_inc(value_tp *x, exec_info *f)
 /* Pre: x->rep
    increment x in place
 */
 { if (x->rep == REP_int)
     { x->v.i++;
       if (x->v.i == INT_REP_SIGN)
         { x->rep = REP_z;
           x->v.z = new_z_value(f);
           mpz_set_si(x->v.z->z, INT_REP_SIGN);
         }
     }
   else
     { assert(x->rep == REP_z);
       mpz_add_ui(x->v.z->z, x->v.z->z, 1);
       int_simplify(x, f);
     }
 }

extern void int_dec(value_tp *x, exec_info *f)
 /* Pre: x->rep
    decrement x in place
 */
 { if (x->rep == REP_int)
     { x->v.i--;
       if (x->v.i == ~INT_REP_SIGN)
         { x->rep = REP_z;
           x->v.z = new_z_value(f);
           mpz_set_si(x->v.z->z, ~INT_REP_SIGN);
         }
     }
   else
     { assert(x->rep == REP_z);
       mpz_sub_ui(x->v.z->z, x->v.z->z, 1);
       int_simplify(x, f);
     }
 }

static value_tp int_mul(value_tp *l, value_tp *r, exec_info *f)
 /* Pre: l->rep && r->rep, l != r.
    return l * r; clears *l and *r.
 */
 { value_tp x, *tmp;
   long li, ri;
   ulong i, j, m;
   int bi, bj;
   if (l->rep == REP_int && r->rep == REP_int)
     { li = l->v.i; ri = r->v.i;
       if (l->v.i < 0)
            i = (ulong) - l->v.i;
       else i = (ulong) l->v.i;
       if (r->v.i < 0)
            j = (ulong) - r->v.i;
       else j = (ulong) r->v.i;
       if (i < j) { m = i; i = j; j = m; }
       /* If nr bits of i + nr bits of j < 31 then no overflow. To save
          time, we count in bytes instead of bits, hence we over-estimate. */
       m = INT_REP_HI_BYTE;
       bi = 4;
       while (m && !(i & m))
         { m >>= 8; bi--; }
       /* i has <= bi*8 bits */
       m = INT_REP_HI_BYTE | (INT_REP_HI_BYTE >> 1);
       bj = 4;
       while (m && !(j & m))
         { m >>= 8; bj--; }
       /* j has <= bj*8-1 bits */
       if (bi + bj <= 4)
         { x.rep = REP_int;
           x.v.i = l->v.i * r->v.i;
           return x;
         }
       else /* potential overflow */
         { x.rep = REP_z;
           x.v.z = new_z_value(f);
           mpz_set_si(x.v.z->z, l->v.i);
           l = &x;
         }
     }
   if (l->rep == REP_int)
     { tmp = l; l = r; r = tmp; }
   /* l->rep == REP_z */
   copy_and_clear(&x, l, f);
   if (r->rep == REP_int)
     { mpz_mul_si(x.v.z->z, x.v.z->z, r->v.i); }
   else
     { mpz_mul(x.v.z->z, x.v.z->z, r->v.z->z);
       clear_value_tp(r, f);
     }
   return x;
 }

static value_tp int_divmod(value_tp *l, value_tp *r, token_tp op, exec_info *f)
 /* Pre: l->rep && r->rep; r's value is not 0; op is '/', '%', KW_mod;
         l != r.
    Return l op r; clears *l and *r.
 */
 { ulong ali, ari;
   value_tp x;
   int n = 0;
   if (l->rep == REP_int && r->rep == REP_int)
     { if (l->v.i < 0)
            { ali = (ulong) - l->v.i; n = ~n; }
       else { ali = (ulong) l->v.i; }
       if (r->v.i < 0)
            { ari = (ulong) - r->v.i; n = ~n; }
       else { ari = (ulong) r->v.i; }
       x.rep = REP_int;
       if (op == '/')
         { x.v.i = (long) (ali / ari);
           if (n) x.v.i = - x.v.i;
           else if (x.v.i < 0) /* MIN_INT_REP / -1 */
             { goto use_z; }
         }
       else if (op == '%')
         { x.v.i = (long) (ali % ari);
           if (l->v.i < 0) x.v.i = - x.v.i;
         }
       else
         { x.v.i = (long) (ali % ari);
           if (l->v.i < 0 && x.v.i > 0) x.v.i = ari - x.v.i;
         }
       return x;
     }
 use_z:
   if (l->rep == REP_int)
     { x.rep = REP_z;
       x.v.z = new_z_value(f);
       mpz_set_si(x.v.z->z, l->v.i);
     }
   else
     { copy_and_clear(&x, l, f); }
   if (r->rep == REP_int)
     { if (r->v.i < 0)
            { ari = (ulong) - r->v.i; n = ~n; }
       else { ari = (ulong) r->v.i; }
       if (op == '/')
         { mpz_tdiv_q_ui(x.v.z->z, x.v.z->z, ari);
           if (n) mpz_neg(x.v.z->z, x.v.z->z);
         }
       else if (op == '%')
         { mpz_tdiv_r_ui(x.v.z->z, x.v.z->z, ari); }
       else
         { mpz_mod_ui(x.v.z->z, x.v.z->z, ari); }
     }
   else
     { if (op == '/')
         { mpz_tdiv_q(x.v.z->z, x.v.z->z, r->v.z->z); }
       else if (op == '%')
         { mpz_tdiv_r(x.v.z->z, x.v.z->z, r->v.z->z); }
       else
         { mpz_mod(x.v.z->z, x.v.z->z, r->v.z->z); }
       clear_value_tp(r, f);
     }
   return x;
 }

static value_tp int_bitwise(value_tp *l, value_tp *r, token_tp op,exec_info *f)
 /* Pre: l->rep && r->rep; l != r; op is '&', '|', or KW_xor.
    return l op r; clears *l and *r.
 */
 { value_tp x;
   long li, ri;
   li = l->v.i; ri = r->v.i;
   if (l->rep == REP_int && r->rep == REP_int)
     { x.rep = REP_int;
       if (op == '&')
         { x.v.i = li & ri; }
       else if (op == '|')
         { x.v.i = li | ri; }
       else
         { x.v.i = li ^ ri; }
       return x;
     }
   x.rep = REP_z;
   if (l->rep == REP_int)
     { x.v.z = new_z_value(f);
       mpz_set_si(x.v.z->z, li);
     }
   else if (r->rep == REP_int)
     { x.v.z = new_z_value(f);
       mpz_set_si(x.v.z->z, ri);
       r = l;
     }
   else
     { copy_and_clear(&x, l, f); }
   if (op == '&')
     { mpz_and(x.v.z->z, x.v.z->z, r->v.z->z); }
   else if (op == '|')
     { mpz_ior(x.v.z->z, x.v.z->z, r->v.z->z); }
   else
     { mpz_xor(x.v.z->z, x.v.z->z, r->v.z->z); }
   clear_value_tp(r, f);
   return x;
 }

static value_tp int_exponentiate
 (value_tp *x, value_tp *y, exec_info *f, binary_expr *e)
 /* Pre: x->rep && y->rep; x != y.
    return x^y; clears *x and *y. e is used for errors.
 */
 { value_tp z;
   ulong yi;
   int_simplify(y, f);
   int_simplify(x, f);
   if ((y->rep == REP_int && y->v.i < 0) ||
       (y->rep == REP_z && mpz_sgn(y->v.z->z) < 0))
     { clear_value_tp(x, f);
       exec_error(f, e->r, "Exponent %v = %v < 0", vstr_obj,e->r, vstr_val,y);
     }
   if ((y->rep == REP_int && y->v.i > INT_REP_MAXEXP) || y->rep == REP_z)
     { if (! (x->rep == REP_int &&
               (x->v.i == 0 || x->v.i == 1 || x->v.i == -1)) )
         { exec_error(f, e->r,
                      "Be reasonable, exponent %v = %v is ridiculously large",
                      vstr_obj, e->r, vstr_val, y);
         }
     }
   z.rep = REP_int;
   if (y->rep == REP_int && y->v.i == 0)
     { z.v.i = 1; }
   else if (x->rep == REP_int && (x->v.i == 1 || x->v.i == 0))
     { z.v.i = x->v.i; }
   else if (x->rep == REP_int && x->v.i == -1)
     { if ((y->rep == REP_int && ODD(y->v.i))
           || (y->rep == REP_z && mpz_odd_p(y->v.z->z)))
            z.v.i = -1;
       else z.v.i = 1;
     }
   else
     { z.rep = 0; }
   if (z.rep)
     { clear_value_tp(x, f); clear_value_tp(y, f);
       return z;
     }
   yi = (ulong)y->v.i;
   if (x->rep == REP_int && x->v.i == 2)
     { if (yi < 31)
         { z.rep = REP_int;
           z.v.i = 1 << yi;
         }
       else
         { z.rep = REP_z;
           z.v.z = new_z_value(f);
           mpz_set_si(z.v.z->z, 1);
           mpz_mul_2exp(z.v.z->z, z.v.z->z, yi);
         }
       return z;
     }
   if (x->rep == REP_z)
     { copy_and_clear(&z, x, f); }
   else
     { z.rep = REP_z;
       z.v.z = new_z_value(f);
       mpz_set_si(z.v.z->z, x->v.i);
     }
   mpz_pow_ui(z.v.z->z, z.v.z->z, yi);
   return z;
 }

extern long int_log2(value_tp *x, exec_info *f)
 /* Take the log of (x+1) base 2, round up to the nearest integer */
 { long ret, xv = x->v.i;
   if (x->rep == REP_int)
     { x->rep = REP_z;
       x->v.z = new_z_value(f);
       mpz_set_si(x->v.z->z,  xv);
     }
   ret = mpz_sizeinbase(x->v.z->z, 2);
   clear_value_tp(x, f);
   return ret;
 }

static void int_arithmetic(binary_expr *x, exec_info *f)
 /* Pre: x->l, x->r have been eval'd; x is int, int -> int expression */
 { value_tp xval, lval, rval;
   pop_value(&rval, f);
   pop_value(&lval, f);
   if (!lval.rep || !rval.rep)
     { xval.rep = REP_none;
       push_value(&xval, f);
       return;
     }
   switch(x->op_sym)
     { case '+': xval = int_add(&lval, &rval, f);
       break;
       case '-': xval = int_sub(&lval, &rval, f);
       break;
       case '*': xval = int_mul(&lval, &rval, f);
       break;
       case '&': case '|': case KW_xor:
            xval = int_bitwise(&lval, &rval, x->op_sym, f);
       break;
       case '/': case '%': case KW_mod:
            int_simplify(&rval, f);
            if (rval.rep == REP_int && rval.v.i == 0)
              { exec_error(f, x->r, "Division by %v = 0", vstr_obj, x->r); }
            xval = int_divmod(&lval, &rval, x->op_sym, f);
       break;
       case '^':
            xval = int_exponentiate(&lval, &rval, f, x);
       break;
       default: assert(!"Illegal op symbol");
       break;
     }
   push_value(&xval, f);
 }

extern int int_cmp(value_tp *x, value_tp *y, exec_info *f)
 /* Pre: x->rep && y->rep, x != y.
    Return 0 if x == y, <0 if x < y, >0 if x > y.
    Does not clear *x and *y.
 */
 { int c = 0;
   long xi, yi;
   if (x->rep == REP_int)
     { if (y->rep == REP_int)
         { xi = x->v.i; yi = y->v.i;
           /* return xi - yi; might have overflow */
           if (xi < yi) c = -1;
           else if (xi > yi) c = 1;
           else c = 0;
         }   
       else
         { c = mpz_cmp_si(y->v.z->z, x->v.i);
           if (c < 0) c = 1; /* in case c == minint */
           else c = -c;
         }
     }
   else
     { if (y->rep == REP_z)
         { c = mpz_cmp(x->v.z->z, y->v.z->z); }
       else
         { c = mpz_cmp_si(x->v.z->z, y->v.i); }
     }
   return c;
 }

static void int_compare(binary_expr *x, exec_info *f)
 /* Pre: x->l, x->r have been eval'd; x is comparison (not =, !=) */
 { value_tp xval, lval, rval;
   long c;
   pop_value(&rval, f);
   pop_value(&lval, f);
   if (!lval.rep || !rval.rep)
     { xval.rep = REP_none;
       push_value(&xval, f);
       return;
     }
   xval.rep = REP_bool;
   c = int_cmp(&lval, &rval, f);
   clear_value_tp(&lval, f);
   clear_value_tp(&rval, f);
   switch(x->op_sym)
     { case '<': xval.v.i = (c < 0);
       break;
       case SYM_lte: xval.v.i = (c <= 0);
       break;
       case '>': xval.v.i = (c > 0);
       break;
       case SYM_gte: xval.v.i = (c >= 0);
       break;
       default: assert(!"Illegal op symbol");
       break;
     }
   push_value(&xval, f);
 }

static void bool_arithmetic(binary_expr *x, exec_info *f)
 /* Pre: x->l, x->r have been eval'd; x is bool, bool -> bool (not =, !=) */
 { value_tp xval, lval, rval;
   int li, ri;
   pop_value(&rval, f);
   pop_value(&lval, f);
   if (!lval.rep || !rval.rep)
     { xval.rep = REP_none;
       push_value(&xval, f);
       return;
     }
   xval.rep = REP_bool;
   li = lval.v.i; ri = rval.v.i;
   switch(x->op_sym)
     { case '<': xval.v.i = (li < ri);
       break;
       case SYM_lte: xval.v.i = (li <= ri);
       break;
       case '>': xval.v.i = (li > ri);
       break;
       case SYM_gte: xval.v.i = (li >= ri);
       break;
       case '&': xval.v.i = li & ri;
       break;
       case '|': xval.v.i = li | ri;
       break;
       case KW_xor: xval.v.i = li ^ ri;
       break;
       default: assert(!"Illegal op symbol");
       break;
     }
   push_value(&xval, f);
 }

static void concat_array(binary_expr *x, exec_info *f)
 /* Pre: x->l, x->r have been eval'd; x is ++ */
 { value_tp xval, lval, rval;
   value_list *ll, *rl, *xl;
   int4 i, j;
   void (*cp)(value_tp *, value_tp *, exec_info *);
   pop_value(&rval, f);
   pop_value(&lval, f);
   if (!lval.rep || !rval.rep)
     { xval.rep = REP_none;
       push_value(&xval, f);
       return;
     }
   ll = lval.v.l;
   rl = rval.v.l;
   xval.rep = REP_array;
   i = ll->size + rl->size;
   if (i > ARRAY_REP_MAXSIZE)
     { exec_error(f, x, "Implementation limit exceeded: Array size %ld > %ld",
                  (long)i, (long)ARRAY_REP_MAXSIZE);
     }
   xval.v.l = xl = new_value_list(ll->size + rl->size, f);
   if (ll->refcnt == 1)
     { cp = copy_and_clear; }
   else
     { cp = alias_value_tp; }
   for (i = 0; i < ll->size; i++)
     { cp(&xl->vl[i], &ll->vl[i], f); }
   if (rl->refcnt == 1)
     { cp = copy_and_clear; }
   else
     { cp = alias_value_tp; }
   for (j = 0; j < rl->size; j++, i++)
     { cp(&xl->vl[i], &rl->vl[j], f); }
   clear_value_tp(&lval, f);
   clear_value_tp(&rval, f);
   push_value(&xval, f);
 }

static void eval_binary_expr(binary_expr *x, exec_info *f)
 { value_tp xval, lval, rval;
   eval_expr(x->l, f);
   eval_expr(x->r, f);
   if (x->op_sym == '=' || x->op_sym == SYM_neq)
     { pop_value(&rval, f);
       pop_value(&lval, f);
       if (!lval.rep || !rval.rep)
         { xval.rep = REP_none; }
       else
         { xval.rep = REP_bool;
           xval.v.i = equal_value(&lval, &rval, f, x);
           if (x->op_sym == SYM_neq)
             { xval.v.i = 1 - xval.v.i; }
         }
       push_value(&xval, f);
       clear_value_tp(&lval, f);
       clear_value_tp(&rval, f);
     }
   else if (x->tp.kind == TP_int)
     { int_arithmetic(x, f); }
   else if (x->l->tp.kind == TP_int)
     { int_compare(x, f); }
   else if (x->l->tp.kind == TP_bool)
     { bool_arithmetic(x, f); }
   else if (x->op_sym == SYM_concat)
     { concat_array(x, f); }
   else
     { assert(!"Illegal binary operator"); }
 }

static void eval_rep_expr(rep_expr *x, exec_info *f)
 { value_tp xval, lval, hval, nval;
   int i, j = 0, k = 0, n = 0;
   void (*cp)(value_tp *, value_tp *, exec_info *);
   eval_expr(x->r.l, f);
   eval_expr(x->r.h, f);
   pop_value(&hval, f);
   pop_value(&lval, f);
   if (int_cmp(&hval, &lval, f) < 0)
     { exec_error(f, x,
           "Replication lower bound %v = %v exceeds upper bound %v = %v",
           vstr_obj, x->r.l, vstr_val, lval, vstr_obj, x->r.h, vstr_val, hval);
     }
   if (x->rep_sym == SYM_concat)
     { push_repval(&hval, f->curr, f);
       xval.rep = REP_array;
       while (int_cmp(&lval, &f->curr->rep_vals->v, f) <= 0)
         { eval_expr(x->v, f);
           pop_value(&nval, f);
           if (!nval.rep)
             { xval.rep = REP_none; }
           else
             { assert(nval.rep == REP_array);
               n += nval.v.l->size;
               if (n > ARRAY_REP_MAXSIZE)
                 { exec_error(f, x, "Implementation limit exceeded: Array size"
                              " %ld > %ld", (long)n, (long)ARRAY_REP_MAXSIZE);
                 }
               else if (nval.v.l->size > 0)
                 { push_value(&nval, f);
                   k++;
                 }
               else
                 { clear_value_tp(&nval, f); }
             }
           int_dec(&f->curr->rep_vals->v, f);
         }
       if (!xval.rep)
         { for (i = 0; i < k; i++)
             { pop_value(&nval, f);
               clear_value_tp(&nval, f);
             }
         }
       else
         { xval.v.l = new_value_list(n, f);
           while (j < n)
             { pop_value(&nval, f);
               if (nval.v.l->refcnt == 1)
                 { cp = copy_and_clear; }
               else
                 { cp = alias_value_tp; }
               for (i = 0; i < nval.v.l->size; i++, j++)
                 { cp(&xval.v.l->vl[j], &nval.v.l->vl[i], f); }
               clear_value_tp(&nval, f);
             }
         }
       hval = lval;
     }
   else
     { push_repval(&lval, f->curr, f);
       eval_expr(x->v, f);
       pop_value(&xval, f);
       while (int_cmp(&f->curr->rep_vals->v, &hval, f) < 0)
         { int_inc(&f->curr->rep_vals->v, f);
           eval_expr(x->v, f);
           pop_value(&nval, f);
           if (x->rep_sym == '+')
             { xval = int_add(&xval, &nval, f); }
           else if (x->rep_sym == '*')
             { xval = int_mul(&xval, &nval, f); }
           else if (x->tp.kind == TP_int)
             { xval = int_bitwise(&xval, &nval, x->rep_sym, f); }
           else
             { assert (x->v->tp.kind == TP_bool);
               if (!nval.rep)
                 { xval.rep = REP_none; }
               else if (xval.rep)
                 { switch (x->rep_sym)
                     { case '&': xval.v.i = xval.v.i & nval.v.i;
                       break;
                       case '|': xval.v.i = xval.v.i | nval.v.i;
                       break;
                       case KW_xor: case SYM_neq:
                         xval.v.i = xval.v.i ^ nval.v.i;
                       break;
                       case '=': xval.v.i = 1 - (xval.v.i ^ nval.v.i);
                       break;
                       default:
                       break;
                     }
                 }
             }
         }
     }
   push_value(&xval, f);
   pop_repval(&lval, f->curr, f);
   clear_value_tp(&lval, f);
   clear_value_tp(&hval, f);
 }

static void eval_prefix_expr(prefix_expr *x, exec_info *f)
 { value_tp xval, rval;
   eval_expr(x->r, f);
   pop_value(&rval, f);
   if (!rval.rep)
     { xval.rep = REP_none;
       push_value(&xval, f);
       return;
     }
   switch (x->op_sym)
     { case '+': xval = rval;
       break;
       case '-': xval.rep = rval.rep;
         if (rval.rep == REP_int)
           { if (rval.v.i == MIN_INT_REP)
               { xval.rep = REP_z;
                 xval.v.z = new_z_value(f);
                 mpz_set_si(xval.v.z->z, rval.v.i);
                 mpz_neg(xval.v.z->z, xval.v.z->z);
                 /* you could use mpz_set_ui(), which implies the
                    negation, but that seems like a dirty trick */
               }
             else
               { xval.rep = REP_int;
                 xval.v.i = - rval.v.i;
               }
           }
         else
           { copy_and_clear(&xval, &rval, f);
             mpz_neg(xval.v.z->z, xval.v.z->z);
           }
       break;
       case '~': if (rval.rep == REP_int)
                   { xval.rep = REP_int;
                     xval.v.i = ~rval.v.i;
                   }
                 else if (rval.rep == REP_z)
                   { copy_and_clear(&xval, &rval, f);
                     mpz_com(xval.v.z->z, xval.v.z->z);
                   }
                 else
                   { xval.rep = REP_bool;
                     xval.v.i = 1 - rval.v.i;
                   }
       break;
       default: assert(!"bad prefix operator");
       break;
     }
   push_value(&xval, f);
 }

static void eval_probe(probe *x, exec_info *f)
 { value_tp xval, rval;
   SET_FLAG(f->flags, EVAL_probe);
   eval_expr(x->r, f);
   RESET_FLAG(f->flags, EVAL_probe);
   pop_value(&rval, f);
   xval.rep = REP_bool;
   f->e = 0; /* vacuous unless EVAL_probe_wait is set */
   xval.v.i = !!get_probe(&rval, f);
   clear_value_tp(&rval, f);
   push_value(&xval, f);
 }

static void eval_value_probe(value_probe *x, exec_info *f)
 { value_tp xval, pval, pvval;
   llist m;
   expr *p;
   llist pv; /* llist(expr) (reverse of x->p) */
   int curri = f->curr->i;
   m = x->p;
   xval.rep = REP_bool;
   xval.v.i = 1;
   llist_init(&pv);
   /* strategy: replace port values with their contained values,
    * using receive_value. Put them back when done.
    */
   f->curr->i = 0; /* make receive_value do a peek */
   SET_FLAG(f->flags, EVAL_probe);
   f->e = 0; /* vacuous unless EVAL_probe_wait is set */
   while (!llist_is_empty(&m))
     { p = llist_head(&m);
       m = llist_alias_tail(&m);
       eval_expr(p, f);
       pop_value(&pval, f);
       if (!get_probe(&pval, f) || !xval.v.i)
         { xval.v.i = 0;
           clear_value_tp(&pval, f);
           if (IS_SET(f->flags, EVAL_probe_wait)) continue;
           else break;
         }
       pvval = receive_value(&pval, &p->tp, f);
       assign(p, &pvval, f); /* replace port value with token value */
       llist_prepend(&pv, p);
       push_value(&pval, f);
     }
   if (xval.v.i)
     { eval_expr(x->b, f);
       pop_value(&xval, f);
     }
   while (!llist_is_empty(&pv))
     { p = llist_idx_extract(&pv, 0);
       pop_value(&pval, f);
       assign(p, &pval, f); /* replace token value with port value */
     }
   RESET_FLAG(f->flags, EVAL_probe);
   push_value(&xval, f);
   f->curr->i = curri;
 }

/********** evaluation2 ******************************************************
 * The second evaluation section is more about moving around and organizing
 * values.  Here we deal with references to variables, array subscripts and
 * subranges, and fields of records and unions.
 *****************************************************************************/

static value_tp *find_ref_aux(port_value *p, value_tp *v, exec_info *f)
/* if f->curr->i, return array containing port instead */
 { value_list *vl;
   value_tp *w;
   int i;
   switch (v->rep)
     { case REP_array: case REP_record:
         vl = v->v.l;
         for (i = 0 ; i < vl->size ; i++)
           { if ((w=find_ref_aux(p, &vl->vl[i], f)))
               { return (f->curr->i && vl->vl[i].rep == REP_port)? v : w; }
           }
         return 0;
       case REP_union:
         return find_ref_aux(p, &v->v.u->v, f);
       case REP_port:
         return (v->v.p==p)? v : 0;
       default:
         return 0;
     }
 }

/* Checks all ports to find the reference to the given port value */
extern value_tp *find_reference(port_value *p, exec_info *f)
 { value_tp *x, *var = p->ps->var;
   llist m = p->ps->p->pl; /* llist(port_parameter) */
   var_decl *d;
   while (!llist_is_empty(&m))
     { d = llist_head(&m);
       if ((x=find_ref_aux(p, &var[d->var_idx], f)))
         { return x; }
       m = llist_alias_tail(&m);
     }
   assert(!"Port not referenced by own process");
   return 0;
 }

static int _free_wprobe(wire_expr *e, wire_value *w)
 { e->refcnt--;
   if ((w->flags ^ (e->flags >> WIRE_vd_shft)) & WIRE_value)
     { e->valcnt--; }
   llist_all_extract(&e->u.act->cs->dep, 0, w);
   return 0;
 }

static void free_wprobe(wire_value *w, exec_info *f)
 { if (IS_SET(w->flags, WIRE_has_dep))
     { llist_free(&w->u.dep, (llist_func*)_free_wprobe, w); }
 }

static void *_copy_wprobe(wire_expr *e, wire_value *w)
 { e->refcnt++;
   if ((w->flags ^ (e->flags >> WIRE_vd_shft)) & WIRE_value)
     { e->valcnt++; }
   llist_prepend(&e->u.act->cs->dep, w);
   return e;
 }

static void copy_wprobe(wire_value *w1, wire_value *w2, exec_info *f)
 { w1->flags = w2->flags;
   if (IS_SET(w2->flags, WIRE_has_dep))
     { w1->u.dep = llist_copy(&w2->u.dep, (llist_func_p*)_copy_wprobe, w1); }
 }

extern void port_to_array(value_tp *v, expr *x, exec_info *f)
/* Pre: v is a (connected) port value, EVAL_connect is set
   Replaces v with an array of connected ports.  Also replaces
   the reference to v, and the reference to v's connected port value
   Also works for records (type created is inferred from x->tp)
 */
 { value_tp pv, ppv, *ve;
   value_list *pl, *ppl;
   port_value *p, *pp, *vp, *vpp;
   int i;
   vp = v->v.p; vpp = v->v.p->p;
   assert(vpp->wprobe.refcnt<=2);
   assert(vp->wprobe.refcnt<=3);
   assert(vpp->v.rep == REP_none);
   force_value(&pv, x, f);
   force_value(&ppv, x, f);
   assert(pv.rep == REP_array || pv.rep == REP_record);
   assert(ppv.rep == REP_array || ppv.rep == REP_record);
   pl = pv.v.l; ppl = ppv.v.l;
   for (i = 0 ; i < pl->size ; i++)
     { pl->vl[i].rep = ppl->vl[i].rep = REP_port;
       pl->vl[i].v.p = p = new_port_value(v->v.p->ps, f);
       ppl->vl[i].v.p = pp = new_port_value(v->v.p->p->ps, f);
       copy_wprobe(&p->wprobe, &vp->wprobe, f);
       copy_wprobe(&pp->wprobe, &vpp->wprobe, f);
       if (vp->v.rep == REP_array || vp->v.rep == REP_record)
         { alias_value_tp(&p->v, &vp->v.v.l->vl[i], f); }
       else if (vp->v.rep == REP_int)
         { p->v.rep = REP_bool;
           p->v.v.i = ((vp->v.v.i & (1 << i)) != 0);
         }
       else if (vp->v.rep == REP_z)
         { p->v.rep = REP_bool;
           p->v.v.i = mpz_tstbit(vp->v.v.z->z, i);
         }
       else
         { assert(!vp->v.rep); }
       p->p = pp; pp->p = p;
       p->wprobe.refcnt++; pp->wprobe.refcnt++;
     }
   free_wprobe(&vpp->wprobe, f);
   ve = find_reference(vpp, f);
   free(vpp);
   *ve = ppv;
   free_wprobe(&vp->wprobe, f);
   ve = find_reference(vp, f);
   free(vp);
   *ve = pv;
   alias_value_tp(v, ve, f);
 }

static void strict_check_eval(void *obj, value_tp *v, exec_info *f)
/* Pre: x has been evaluated as v, the location of x's stored value
 *      If EVAL_lvalue is set, v is an array/record that we are writing to
 *      one or more elements of.  Otherwise if EVAL_sub_elem is set, v is an
 *      array/record that we are read some elements of.  Otherwise it is
 *      assumed we are reading the entirety of x.
 */
 { parse_obj *x = obj;
   f->err_obj = x;
   if (IS_SET(x->flags, EXPR_port))
     { if (IS_SET(f->flags, EVAL_probe))
         { if (IS_SET(f->flags, EVAL_sub_elem))
             { strict_check_read_elem(v, f); }
           else if (!IS_SET(f->flags, EVAL_lvalue))
             { strict_check_read(v, f); }
         }
       else
         { if (IS_SET(f->flags, EVAL_sub_elem))
             { strict_check_write_elem(v, f); }
           else if (!IS_SET(f->flags, EVAL_lvalue))
             { strict_check_write(v, f); }
         }
     }
   else
     { if (IS_SET(f->flags, EVAL_lvalue))
         { if (v->rep == REP_array || v->rep == REP_record)
             { strict_check_write_elem(v, f); }
         }
       else if (IS_SET(f->flags, EVAL_sub_elem))
         { strict_check_read_elem(v, f); }
       else
         { strict_check_read(v, f); }
     }
 }

static void strict_check_assign(void *obj, value_tp *v, exec_info *f)
/* Pre: x has been evaluated as v, the location of x's stored value
 * This should not be called from assign int subscript/subrange, or any
 * other functions which call assign recursively
 * Also, v should not be cleared until this is called.
 */
 { parse_obj *x = obj;
   f->err_obj = x;
   if (!IS_SET(x->flags, EXPR_port))
     { strict_check_write(v, f); }
 }

static void eval_array_subscript(array_subscript *x, exec_info *f)
/* Also used for int_subscript when the integer is a port */
 { value_tp xval, xxval, idxval, lval, hval, *ve;
   array_type *atps;
   value_list *l;
   exec_flags flags = f->flags;
   eval_expr(x->idx, f); /* For connections, this has to come first */
   SET_FLAG(f->flags, EVAL_sub_elem);
   eval_expr(x->x, f);
   ASSIGN_FLAG(f->flags, flags, EVAL_sub_elem);
   pop_value(&xxval, f);
   pop_value(&idxval, f);
   if (!xxval.rep || !idxval.rep)
     { xval.rep = REP_none;
       clear_value_tp(&xxval, f);
       clear_value_tp(&idxval, f);
       push_value(&xval, f);
       return;
     }
   else if (xxval.rep == REP_port)
     { if (!xxval.v.p->p) /* double connection error */
         { clear_value_tp(&idxval, f);
           push_value(&xxval, f);
           return;
         }
       port_to_array(&xxval, x->x, f);
     }
   assert(xxval.rep == REP_array);
   l = xxval.v.l;
   atps = (array_type*)x->x->tp.tps;
   if (atps->class == CLASS_array_type)
     { eval_expr(atps->h, f);
       eval_expr(atps->l, f);
       pop_value(&lval, f);
       pop_value(&hval, f);
       if (int_cmp(&idxval, &lval, f) < 0 || int_cmp(&hval, &idxval, f) < 0)
         { clear_value_tp(&xxval, f);
           exec_error(f, x->idx, "Index [%v] = %v is outside array "
                      "%v[%v..%#v]", vstr_obj, x->idx, vstr_val, &idxval,
                      vstr_obj, x->x, vstr_val, &lval, &hval);
         }
       idxval = int_sub(&idxval, &lval, f);
       clear_value_tp(&hval, f);
     }
   else
     { assert(x->x->tp.kind == TP_int);
       hval.rep = lval.rep = REP_int;
       lval.v.i = 0; hval.v.i = l->size - 1;
       if (int_cmp(&idxval, &lval, f) < 0 || int_cmp(&hval, &idxval, f) < 0)
         { clear_value_tp(&xxval, f);
           exec_error(f, x->idx, "Integer index [%v] = %v is outside array "
                      "%v[%v..%#v]", vstr_obj, x->idx, vstr_val, &idxval,
                      vstr_obj, x->x, vstr_val, &lval, &hval);
         }
     }
   int_simplify(&idxval, f);
   assert(idxval.rep == REP_int);
   ve = &l->vl[idxval.v.i];
   if (!ve->rep && IS_SET(f->flags, EVAL_lvalue))
     { force_value(ve, (expr*)x, f); }
   if (l->refcnt == 1)
     { copy_and_clear(&xval, ve, f); }
   else
     { alias_value_tp(&xval, ve, f); }
   clear_value_tp(&xxval, f);
   if (IS_SET(f->flags, EXEC_strict)) strict_check_eval(x, ve, f);
   push_value(&xval, f);
 }

static void assign_array_subscript(array_subscript *x, exec_info *f)
 { value_tp xxval, idxval, lval, hval, *ve;
   array_type *atps;
   eval_expr(x->idx, f); /* For connections, this has to come first */
   pop_value(&idxval, f);
   SET_FLAG(f->flags, EVAL_lvalue);
   eval_expr(x->x, f);
   RESET_FLAG(f->flags, EVAL_lvalue);
   pop_value(&xxval, f);
   if (!idxval.rep)
     { exec_error(f, x->idx, "Unknown index [%v]", vstr_obj, x->idx); }
   atps = (array_type*)x->x->tp.tps;
   if (atps->class == CLASS_array_type)
     { eval_expr(atps->h, f);
       eval_expr(atps->l, f);
       pop_value(&lval, f);
       pop_value(&hval, f);
       if (int_cmp(&idxval, &lval, f) < 0 || int_cmp(&hval, &idxval, f) < 0)
         { exec_error(f, x->idx,"Index [%v] = %v is outside array "
                      "%v[%v..%#v]", vstr_obj, x->idx, vstr_val, &idxval,
                      vstr_obj, x->x, vstr_val, &lval, &hval);
         }
       idxval = int_sub(&idxval, &lval, f);
       clear_value_tp(&hval, f);
     }
   /* Skip integer type bounds checking - already done by eval */
   int_simplify(&idxval, f);
   assert(idxval.rep == REP_int);
   ve = &xxval.v.l->vl[idxval.v.i];
   if (IS_SET(f->flags, EXEC_strict)) strict_check_assign(x, ve, f);
   if (ve->rep)
     { clear_value_tp(ve, f); }
   copy_and_clear(ve, f->val, f);
   clear_value_tp(&xxval, f);
 }

static void eval_int_subscript(int_subscript *x, exec_info *f)
 /* Not used  when x is a port */
 { value_tp xval, xxval, idxval, tmpval, *mrkv;
   long idx, size, i, nr_bit;
   value_list *xxl, *tmpl;
   eval_expr(x->idx, f); /* For connections, this has to come first */
   eval_expr(x->x, f);
   pop_value(&xxval, f);
   pop_value(&idxval, f);
   if (!idxval.rep || !xxval.rep)
     { clear_value_tp(&idxval, f);
       clear_value_tp(&idxval, f);
       xval.rep = REP_none;
       push_value(&xval, f);
       return;
     }
   int_simplify(&idxval, f);
   if ((idxval.rep == REP_int && idxval.v.i < 0) ||
       (idxval.rep == REP_z && mpz_sgn(idxval.v.z->z) < 0))
     { exec_error(f, x->idx, "Selecting bit [%v] = %v < 0", vstr_obj,
                  x->idx, vstr_val, &idxval);
     }
   if ((idxval.rep == REP_int && idxval.v.i > INT_REP_MAXBITS) ||
        idxval.rep == REP_z)
     { exec_error(f, x->idx, "Index [%v] = %v is a bit too large",
                  vstr_obj, x->idx, vstr_val, &idxval);
     }
   if (x->x->tp.tps->class == CLASS_integer_type)
     { nr_bit = integer_nr_bits(x->x->tp.tps, f);
       if (idxval.v.i >= nr_bit)
         { exec_error(f, x->idx, "Index [%v] = %ld is out of range (%#v has "
                 "only %ld bits)", vstr_obj, x->idx, idxval.v.i, x->x, nr_bit);
         }
     }
   xval.rep = REP_bool;
   assert(!IS_SET(x->flags, EXPR_port));
   if (xxval.rep == REP_int)
     { if (idxval.v.i >= INT_REP_NRBITS)
         { idx = INT_REP_NRBITS - 1; }
       else
         { idx = idxval.v.i; }
       xval.v.i = ((xxval.v.i & (1UL << idx)) != 0);
     }
   else if (xxval.rep == REP_z)
     { xval.v.i = mpz_tstbit(xxval.v.z->z, idxval.v.i); }
   else
     { xval.rep = REP_none; }
   clear_value_tp(&xxval, f);
   clear_value_tp(&idxval, f);
   push_value(&xval, f);
 }

static void assign_int_subscript(int_subscript *x, exec_info *f)
 { value_tp xxval, idxval, *ve;
   int neg = 0;
   long nr_bit, tmp;
   eval_expr(x->idx, f); /* For connections, this has to come first */
   SET_FLAG(f->flags, EVAL_lvalue);
   eval_expr(x->x, f);
   RESET_FLAG(f->flags, EVAL_lvalue);
   pop_value(&xxval, f);
   pop_value(&idxval, f);
   if (!idxval.rep)
     { exec_error(f, x->idx, "Unknown index [%v]", vstr_obj, x->idx); }
   int_simplify(&idxval, f);
   if ((idxval.rep == REP_int && idxval.v.i < 0) ||
       (idxval.rep == REP_z && mpz_sgn(idxval.v.z->z) < 0))
     { exec_error(f, x->idx, "Selecting bit [%v] = %v < 0", vstr_obj,
                  x->idx, vstr_val, &idxval);
     }
   if ((idxval.rep == REP_int && idxval.v.i > INT_REP_MAXBITS) ||
        idxval.rep == REP_z)
     { exec_error(f, x->idx, "Index [%v] = %v is a bit too large",
                  vstr_obj, x->idx, vstr_val, &idxval);
     }
   if (x->x->tp.tps->class == CLASS_integer_type)
     { nr_bit = integer_nr_bits(x->x->tp.tps, f);
       if (idxval.v.i >= nr_bit)
         { exec_error(f, x->idx, "Index [%v] = %ld is out of range (%#v has "
                 "only %ld bits)", vstr_obj, x->idx, idxval.v.i, x->x, nr_bit);
         }
       SET_FLAG(f->flags, EVAL_bit);
     }
   if (xxval.rep == REP_int && idxval.v.i < INT_REP_NRBITS-1)
     { if (f->val->v.i)
         { xxval.v.i = (xxval.v.i | (1UL << idxval.v.i)); }
       else
         { xxval.v.i = (xxval.v.i & ~(1UL << idxval.v.i)); }
       range_check(x->x->tp.tps, &xxval, f, x);
       assign(x->x, &xxval, f);
     }
   else
     { if (xxval.rep == REP_int)
         { tmp = xxval.v.i;
           xxval.rep = REP_z;
           xxval.v.z = new_z_value(f);
           mpz_set_si(xxval.v.z->z, tmp);
         }
       if (f->val->v.i)
         { mpz_setbit(xxval.v.z->z, idxval.v.i); }
       else
         { mpz_clrbit(xxval.v.z->z, idxval.v.i); }
       range_check(x->x->tp.tps, &xxval, f, x);
       assign(x->x, &xxval, f);
     }
   RESET_FLAG(f->flags, EVAL_bit);
 }

static void eval_int_subrange(expr *x, expr *xl, expr *xh, exec_info *f)
 /* Evaluate x[xl..xh] for integer xxval. */
 { value_tp xxval, lidxval, hidxval, xval, tmpval;
   void *y;
   int rev = 0;
   long lidx, hidx, n, nr_bit;
   mpz_t mask;
   eval_expr(x, f);
   pop_value(&xxval, f);
   eval_expr(xl, f);
   pop_value(&lidxval, f);
   eval_expr(xh, f);
   pop_value(&hidxval, f);
   if (!xxval.rep || !lidxval.rep || !hidxval.rep)
     { xval.rep = REP_none;
       push_value(&xval, f);
       return;
     }
   int_simplify(&lidxval, f);
   int_simplify(&hidxval, f);
   if (int_cmp(&lidxval, &hidxval, f) > 0)
     { tmpval = lidxval; lidxval = hidxval; hidxval = tmpval;
       rev = 1;
     }
   if ((lidxval.rep == REP_int && lidxval.v.i < 0) ||
       (lidxval.rep == REP_z && mpz_sgn(lidxval.v.z->z) < 0))
     { y = rev? xh : xl;
       exec_error(f, y, "Selecting bit [%v] = %v < 0", vstr_obj, y,
                  vstr_val, &lidxval);
     }
   if ((hidxval.rep == REP_int && hidxval.v.i > INT_REP_MAXBITS) ||
       hidxval.rep == REP_z)
     { y = rev? xl : xh;
       exec_error(f, y, "Index [%v] = %v is a bit too large", vstr_obj, y,
                  vstr_val, &hidxval);
     }
   if (x->tp.tps->class == CLASS_integer_type)
     { nr_bit = integer_nr_bits(x->tp.tps, f);
       if (hidxval.v.i >= nr_bit)
         { y = rev? xl : xh;
           exec_error(f, y, "Index [%v] = %ld is out of range (%#v has "
                 "only %ld bits)", vstr_obj, y, hidxval.v.i, x, nr_bit);
         }
     }
   lidx = lidxval.v.i; hidx = hidxval.v.i;
   n = hidx - lidx + 1;
   if (xxval.rep == REP_int && hidx < INT_REP_NRBITS && n < INT_REP_NRBITS)
     { xval.rep = REP_int;
       xval.v.i = (long)((((ulong)xxval.v.i) >> lidx) & ((1UL << n) - 1));
     }
   else
     { if (xxval.rep == REP_z)
         { copy_and_clear(&xval, &xxval, f); }
       else
         { xval.rep = REP_z;
           xval.v.z = new_z_value(f);
           mpz_set_si(xval.v.z->z, xxval.v.i);
         }
       mpz_init_set_ui(mask, 1);
       mpz_mul_2exp(mask, mask, n);
       mpz_sub_ui(mask, mask, 1);
       mpz_fdiv_q_2exp(xval.v.z->z, xval.v.z->z, lidx);
       mpz_and(xval.v.z->z, xval.v.z->z, mask);
       mpz_clear(mask);
     }
   push_value(&xval, f);
 }

static void eval_array_subrange(array_subrange *x, exec_info *f)
 { value_tp xval, xxval, lidxval, hidxval, lval, hval, tmpval;
   int4 i, j, n;
   exec_flags flags = f->flags;
   array_type *atps;
   value_list *l, *xl;
   void (*cp)(value_tp *, value_tp *, exec_info *);
   if (x->x->tp.kind == TP_int)
     { eval_int_subrange(x->x, x->l, x->h, f);
       return;
     }
   SET_FLAG(f->flags, EVAL_sub_elem);
   eval_expr(x->x, f);
   ASSIGN_FLAG(f->flags, flags, EVAL_sub_elem);
   pop_value(&xxval, f);
   if (xxval.rep == REP_port)
     { assert(IS_SET(f->flags, EVAL_connect));
       if (!xxval.v.p->p) /* double connection error */
         { push_value(&xxval, f);
           return;
         }
       port_to_array(&xxval, x->x, f);
     } 
   eval_expr(x->l, f);
   pop_value(&lidxval, f);
   eval_expr(x->h, f);
   pop_value(&hidxval, f);
   if (!xxval.rep || !lidxval.rep || !hidxval.rep)
     { xval.rep = REP_none;
       push_value(&xval, f);
       return;
     }
   atps = (array_type*)x->x->tp.tps;
   assert(atps->class == CLASS_array_type);
   if (int_cmp(&lidxval, &hidxval, f) > 0)
     { exec_error(f, x,
             "Slice [%v..%#v] = [%v..%#v]: indices out of order",
             vstr_obj, x->l, x->h, vstr_val, &lidxval, &hidxval);
     }
   eval_expr(atps->l, f);
   pop_value(&lval, f);
   eval_expr(atps->h, f);
   pop_value(&hval, f);
   if (int_cmp(&lidxval, &lval, f) < 0 || int_cmp(&hval, &hidxval, f) < 0)
     { exec_error(f, x,
            "Slice [%v..%#v] = [%v..%#v] is outside array %v[%v..%#v]",
            vstr_obj, x->l, x->h, vstr_val, &lidxval, &hidxval,
            vstr_obj, x->x, vstr_val, &lval, &hval);
     }
   l = xxval.v.l;
   alias_value_tp(&tmpval, &lidxval, f);
   hidxval = int_sub(&hidxval, &tmpval, f);
   int_simplify(&hidxval, f);
   assert(hidxval.rep == REP_int);
   n = hidxval.v.i + 1; /* n = hidx - lidx + 1 */
   xval.rep = REP_array;
   xval.v.l = xl = new_value_list(n, f);
   if (l->refcnt == 1)
     { cp = copy_and_clear; }
   else
     { cp = alias_value_tp; }
   j = 0;
   lidxval = int_sub(&lidxval, &lval, f);
   clear_value_tp(&hval, f);
   int_simplify(&lidxval, f);
   assert(lidxval.rep == REP_int);
   i = lidxval.v.i; /* i = lidx - lval */
   while (n)
     { cp(&xl->vl[j], &l->vl[i], f);
       if (IS_SET(f->flags, EXEC_strict)) strict_check_eval(x, &l->vl[j], f);
       j++; i++;
       n--;
     }
   clear_value_tp(&xxval, f);
   push_value(&xval, f);
 }

static void assign_int_subrange(expr *x, expr *xl, expr *xh, exec_info *f)
 /* assign f->val to x[xl..xh] for integer x */
 { value_tp xxval, lidxval, hidxval, tmpval;
   long lidx, hidx, n, tmp, nr_bit;
   int rev = 0;
   mpz_t mask;
   void *y;
   SET_FLAG(f->flags, EVAL_lvalue);
   eval_expr(x, f);
   RESET_FLAG(f->flags, EVAL_lvalue);
   pop_value(&xxval, f);
   eval_expr(xl, f);
   pop_value(&lidxval, f);
   eval_expr(xh, f);
   pop_value(&hidxval, f);
   if (!lidxval.rep)
     { exec_error(f, xl, "Unknown index [%v.. ] in slice", vstr_obj, xl); }
   if (!hidxval.rep)
     { exec_error(f, xh, "Unknown index [ ..%v] in slice", vstr_obj, xh); }
   if (xxval.rep != REP_int && xxval.rep != REP_z)
     { exec_error(f, x, "Integer subrange of improper value"); }
   int_simplify(&lidxval, f);
   int_simplify(&hidxval, f);
   if (int_cmp(&lidxval, &hidxval, f) > 0)
     { tmpval = lidxval; lidxval = hidxval; hidxval = tmpval;
       rev = 1;
     }
   if ((lidxval.rep == REP_int && lidxval.v.i < 0) ||
       (lidxval.rep == REP_z && mpz_sgn(lidxval.v.z->z) < 0))
     { y = rev? xh : xl;
       exec_error(f, y, "Selecting bit [%v] = %v < 0", vstr_obj, y,
                  vstr_val, &lidxval);
     }
   if ((hidxval.rep == REP_int && hidxval.v.i > INT_REP_MAXBITS) ||
       hidxval.rep == REP_z)
     { y = rev? xl : xh;
       exec_error(f, y, "Index [%v] = %v is a bit too large", vstr_obj, y,
                  vstr_val, &hidxval);
     }
   if (x->tp.tps->class == CLASS_integer_type)
     { nr_bit = integer_nr_bits(x->tp.tps, f);
       if (hidxval.v.i >= nr_bit)
         { y = rev? xl : xh;
           exec_error(f, y, "Index [%v] = %ld is out of range (%#v has "
                 "only %ld bits)", vstr_obj, y, hidxval.v.i, x, nr_bit);
         }
       SET_FLAG(f->flags, EVAL_bit);
     }
   if ((f->val->rep == REP_int && f->val->v.i < 0) ||
       (f->val->rep == REP_z && mpz_sgn(f->val->v.z->z) < 0))
     { exec_error(f, x, "You cannot assign a negative value (%v) to an "
                 "integer slice", vstr_val, f->val);
     }
   lidx = lidxval.v.i; hidx = hidxval.v.i;
   n = hidx - lidx + 1;
   if (xxval.rep == REP_int && f->val->rep == REP_int &&
       hidx < INT_REP_NRBITS - 1)
     { if (f->val->v.i > (1UL << n) - 1)
         { exec_error(f, x,
                     "Value %v is too large for integer slice %v[%d..%d]",
                      vstr_val, f->val, vstr_obj, x, lidx, hidx);
         }
       xxval.v.i = xxval.v.i & ~(((1UL << n) - 1) << lidx);
       xxval.v.i = xxval.v.i | (f->val->v.i << lidx);
       range_check(x->tp.tps, &xxval, f, x);
       assign(x, &xxval, f);
     }
   else
     { if (xxval.rep == REP_int)
         { tmp = xxval.v.i;
           xxval.rep = REP_z;
           xxval.v.z = new_z_value(f);
           mpz_set_si(xxval.v.z->z, tmp);
         }
       mpz_init_set_ui(mask, 1);
       mpz_mul_2exp(mask, mask, n);
       mpz_sub_ui(mask, mask, 1);
       if (f->val->rep == REP_int)
         { tmp = mpz_cmp_si(mask, f->val->v.i); }
       else
         { tmp = mpz_cmp(mask, f->val->v.z->z); }
       if (tmp < 0)
         { exec_error(f, x,
                     "Value %v is too large for integer slice %v[%d..%d]",
                      vstr_val, f->val, vstr_obj, x, lidx, hidx);
         }
       mpz_mul_2exp(mask, mask, lidx);
       mpz_com(mask, mask);
       mpz_and(xxval.v.z->z, xxval.v.z->z, mask);
       if (f->val->rep == REP_int)
         { mpz_set_ui(mask, f->val->v.i); }
       else
         { mpz_set(mask, f->val->v.z->z); }
       mpz_mul_2exp(mask, mask, lidx);
       mpz_ior(xxval.v.z->z, xxval.v.z->z, mask);
       mpz_clear(mask);
       clear_value_tp(f->val, f);
       range_check(x->tp.tps, &xxval, f, x);
       assign(x, &xxval, f);
     }
   clear_value_tp(&xxval, f);
   RESET_FLAG(f->flags, EVAL_bit);
 }

static void assign_array_subrange(array_subrange *x, exec_info *f)
 { value_tp xxval, lidxval, hidxval, lval, hval, tmpval;
   int4 i, j, n;
   array_type *atps;
   value_list *l, *xl;
   void (*cp)(value_tp *, value_tp *, exec_info *);
   if (x->x->tp.kind == TP_int)
     { assign_int_subrange(x->x, x->l, x->h, f);
       return;
     }
   SET_FLAG(f->flags, EVAL_lvalue);
   eval_expr(x->x, f);
   RESET_FLAG(f->flags, EVAL_lvalue);
   pop_value(&xxval, f);
   eval_expr(x->l, f);
   pop_value(&lidxval, f);
   eval_expr(x->h, f);
   pop_value(&hidxval, f);
   if (!lidxval.rep)
     { exec_error(f, x->l, "Unknown index [%v.. ] in slice", vstr_obj, x->l); }
   if (!hidxval.rep)
     { exec_error(f, x->l, "Unknown index [ ..%v] in slice", vstr_obj, x->h); }
   atps = (array_type*)x->x->tp.tps;
   assert(atps->class == CLASS_array_type);
   if (int_cmp(&lidxval, &hidxval, f) > 0)
     { exec_error(f, x,
             "Slice [%v..%#v] = [%v..%#v]: indices out of order",
             vstr_obj, x->l, x->h, vstr_val, &lidxval, &hidxval);
     }
   eval_expr(atps->l, f);
   pop_value(&lval, f);
   eval_expr(atps->h, f);
   pop_value(&hval, f);
   if (int_cmp(&lidxval, &lval, f) < 0 || int_cmp(&hval, &hidxval, f) < 0)
     { exec_error(f, x,
            "Slice [%v..%#v] = [%v..%#v] is outside array %v[%v..%#v]",
            vstr_obj, x->l, x->h, vstr_val, &lidxval, &hidxval,
            vstr_obj, x->x, vstr_val, &lval, &hval);
     }
   l = f->val->v.l;
   alias_value_tp(&tmpval, &lidxval, f);
   hidxval = int_sub(&hidxval, &tmpval, f);
   int_simplify(&hidxval, f);
   assert(hidxval.rep == REP_int);
   n = hidxval.v.i + 1; /* n = hidx - lidx + 1 */
   if (n != l->size)
     { eval_expr(x->h, f);
       pop_value(&hidxval, f);
       exec_error(f, x, "Assignment of %ld values to slice %v[%v..%#v] "
                 "with %ld values", l->size, vstr_obj, x->x,
                 vstr_val, &lidxval, &hidxval, n);
     }
   xl = xxval.v.l;
   if (l->refcnt == 1)
     { cp = copy_and_clear; }
   else
     { cp = alias_value_tp; }
   j = 0;
   lidxval = int_sub(&lidxval, &lval, f);
   clear_value_tp(&hval, f);
   int_simplify(&lidxval, f);
   assert(lidxval.rep == REP_int);
   i = lidxval.v.i; /* i = lidx - lval */
   while (n)
     { if (IS_SET(f->flags, EXEC_strict)) strict_check_assign(x, &xl->vl[i], f);
       if (xl->vl[i].rep)
         { clear_value_tp(&xl->vl[i], f); }
       cp(&xl->vl[i], &l->vl[j], f);
       j++; i++;
       n--;
     }
   clear_value_tp(f->val, f);
   clear_value_tp(&xxval, f);
 }

static void eval_array_constructor(array_constructor *x, exec_info *f)
 /* also used for record_constructor */
 { value_tp xval;
   llist m;
   expr *e;
   value_list *vl;
   int i;
   m = x->l;
   if (x->class == CLASS_array_constructor)
     { xval.rep = REP_array; }
   else
     { xval.rep = REP_record; }
   xval.v.l = vl = new_value_list(llist_size(&m), f);
   i = 0;
   while (!llist_is_empty(&m))
     { e = llist_head(&m);
       eval_expr(e, f);
       pop_value(&vl->vl[i], f);
       i++;
       m = llist_alias_tail(&m);
     }
   push_value(&xval, f);
 }

static void eval_field_of_record(field_of_record *x, exec_info *f)
 { value_tp xval, v, *ve;
   exec_flags flags = f->flags;
   if (x->field)
     { eval_int_subrange(x->x, x->field->l, x->field->h, f);
       return;
     }
   SET_FLAG(f->flags, EVAL_sub_elem);
   eval_expr(x->x, f);
   ASSIGN_FLAG(f->flags, flags, EVAL_sub_elem);
   pop_value(&v, f);
   if (!v.rep)
     { xval.rep = REP_none;
       push_value(&xval, f);
       return;
     }
   else if (v.rep == REP_process)
     { assert(IS_SET(f->flags, EVAL_connect));
       ve = &v.v.ps->cs->var[x->idx];
       /* All references until we return to exec_connection's frame are
          in the subprocess's reference frame
        */
       f->meta_ps = v.v.ps;
     }
   else if (v.rep == REP_port)
     { assert(IS_SET(f->flags, EVAL_connect));
       if (!v.v.p->p) /* double connection error */
         { push_value(&v, f);
           return;
         }
       port_to_array(&v, x->x, f);
       ve = &v.v.l->vl[x->idx];
     } 
   else
     { assert(v.rep == REP_record);
       ve = &v.v.l->vl[x->idx];
     }
   if (!ve->rep && IS_SET(f->flags, EVAL_lvalue))
     { force_value(ve, (expr*)x, f); }
   if (v.rep == REP_record && v.v.l->refcnt == 1)
     { copy_and_clear(&xval, ve, f); }
   else
     { alias_value_tp(&xval, ve, f); }
   clear_value_tp(&v, f);
   if (IS_SET(f->flags, EXEC_strict)) strict_check_eval(x, ve, f);
   push_value(&xval, f);
 }

static void assign_field_of_record(field_of_record *x, exec_info *f)
 { value_tp v, *ve;
   if (x->field)
     { assign_int_subrange(x->x, x->field->l, x->field->h, f);
       return;
     }
   SET_FLAG(f->flags, EVAL_lvalue);
   eval_expr(x->x, f);
   RESET_FLAG(f->flags, EVAL_lvalue);
   pop_value(&v, f);
   if (v.rep == REP_process)
     { assert(IS_SET(f->flags, EVAL_connect));
       ve = &v.v.ps->cs->var[x->idx];
       /* All references until we return to exec_connection's frame are
          in the subprocess's reference frame
        */
       f->meta_ps = v.v.ps;
     }
   else
     { assert(v.rep == REP_record);
       ve = &v.v.l->vl[x->idx];
     }
   if (IS_SET(f->flags, EXEC_strict)) strict_check_assign(x, ve, f);
   if (ve->rep)
     { clear_value_tp(ve, f); }
   copy_and_clear(ve, f->val, f);
   clear_value_tp(&v, f);
 }

static process_state *new_wu_process
(process_def *d, meta_binding *mb, exec_info *f)
 /* Pre: d has two port parameters
  * Create and schedule a new decomposition process.  The process name
  * "//bad" should render the process invisible to the debugger.
  */
 { value_tp vps, *ve;
   process_state *ps;
   ctrl_state *cs;
   int i;
   vps.rep = REP_process;
   vps.v.ps = ps = new_process_state(f, "//bad");
   ps->p = d;
   ps->b = d->cb;
   ps->nr_meta = ps->p->nr_meta;
   if (ps->nr_meta)
     { NEW_ARRAY(ps->meta, ps->nr_meta); }
   ve = ps->meta;
   for (i = 0; i < ps->nr_meta; i++)
     { ve[i].rep = REP_none; }
   cs = ps->cs;
   cs->nr_var = ps->p->nr_var;
   if (cs->nr_var)
     { NEW_ARRAY(cs->var, cs->nr_var); }
   ve = cs->var;
   for (i = 0; i < cs->nr_var; i++)
     { ve[i].rep = REP_none; }
   cs->obj = (parse_obj*)ps->p;
   ps->var = cs->var;
   ps->nr_var = cs->nr_var;
   ps->nr_thread = 1;
   if (mb) exec_meta_binding_aux(mb, &vps, f);
   else meta_init(ps, f);
   llist_prepend(&f->chp, cs);
   return ps;
 }

static void wu_reload(field_of_union *x, port_value *p, int dir, exec_info *f)
/* Pre: p is an already connected channel.
 * If this channel is a decomposition matching x, then push the value of
 * the decomposed port. Otherwise, throw an appropriate error.
 */
 { var_decl *d;
   value_tp v;
   if (!p->dec)
     { exec_error(f, x, "Port %v is already connected to %s",
                  vstr_obj, x->x, p->ps->nm);
     }
   else if (p->dec != x->d)
     { exec_error(f, x, "Port %v is already decomposed using field %s",
                  vstr_obj, x->x, p->dec->id);
     }
   else if (p->v.rep)
     { alias_value_tp(&v, &p->v, f); }
   else
     { d = llist_idx(&p->ps->p->pl, dir? 1 : 0);
       alias_value_tp(&v, &p->ps->var[d->var_idx], f);
       f->meta_ps = p->ps; /* Used by get_wire_connect */
     }
   push_value(&v, f);
 }

static void wu_wire_fix(value_tp *v, process_state *ps, exec_info *f)
/* Switch ownership of ports and wires from ps to f->meta_ps */
 { int i;
   switch(v->rep)
     { case REP_record: case REP_array:
         for (i = 0; i < v->v.l->size; i++)
           { wu_wire_fix(&v->v.l->vl[i], ps, f); }
       return;
       case REP_union:
         wu_wire_fix(&v->v.u->v, ps, f);
       return;
       case REP_wire:
         wire_fix(&v->v.w, f);
         if (v->v.w->wframe->cs->ps == ps)
           { v->v.w->wframe = &f->meta_ps->cs->act; }
       return;
       case REP_port:
         assert(v->v.p->ps == ps);
         v->v.p->ps = f->meta_ps;
       return;
       default:
       return;
     }
 }

extern void wu_proc_remove(value_tp *v, int dir, exec_info *f)
/* Pre: v contains a port which is connected to the default port of a
 * decomposition process.
 * Relocate the decomposed port from the decomposition process to
 * v.v.p->v, then destroy the decomposition process.
 */
 { process_state *ps;
   var_decl *d;
   ps = v->v.p->p->ps;
   d = llist_idx(&ps->p->pl, dir? 0 : 1);
   /* d is now the decomposed port from the other side */
   v->v.p->v = ps->var[d->var_idx];
   ps->var[d->var_idx].rep = REP_none;
   v->v.p->dec = v->v.p->p->dec;
   v->v.p->p->p = 0; v->v.p->p->wprobe.refcnt--;
   v->v.p->p = 0; v->v.p->wprobe.refcnt--;
   wu_wire_fix(&v->v.p->v, ps, f);
   llist_find_extract(&f->chp, 0, ps->cs);
   // Finish deleting the process ps here...
 }

static void eval_wired_union_field(field_of_union *x, exec_info *f)
 { value_tp v;
   process_state *ps;
   port_value *p;
   var_decl *d;
   int dir;
   char *c;
   eval_expr(x->x, f);
   pop_value(&v, f);
   dir = !IS_SET(x->flags, EXPR_inport) ^ (f->meta_ps == f->curr->ps);
   if (IS_SET(x->x->flags, EXPR_port_ext))
     { if (v.rep == REP_port)
         { wu_reload(x, v.v.p->p, dir, f);
           return;
         }
       else if (v.rep)
         { exec_error(f, x, "Port %v has already been implicitly decomposed",
                      vstr_obj, x->x);
         }
       else
         { if (dir)
             { ps = new_wu_process(x->d->dn.p, x->d->dnmb, f); }
           else
             { ps = new_wu_process(x->d->up.p, x->d->upmb, f); }
           d = llist_idx(&ps->p->pl, dir? 0 : 1);
           /* d is now the port of ps of default type */
           v.rep = ps->var[d->var_idx].rep = REP_port;
           ps->var[d->var_idx].v.p = new_port_value(ps, f);
           v.v.p = p = new_port_value(f->meta_ps, f);
           p->p = ps->var[d->var_idx].v.p;
           p->p->p = p;
           p->wprobe.refcnt++; p->p->wprobe.refcnt++;
           p->p->dec = x->d;
           assign(x->x, &v, f);
         }
     }
   else
     { if (v.rep != REP_port)
         { exec_error(f, x, "Port %v has already been implicitly decomposed",
                      vstr_obj, x->x);
         }
       else if (!v.v.p->p)
         { wu_reload(x, v.v.p, dir, f);
           return;
         }
       else if (v.v.p->p->dec == x->d)  // Is there a matching decomposition
         { wu_proc_remove(&v, dir, f);  // on the other side of the channel?
           alias_value_tp(&v, &v.v.p->v, f);
           push_value(&v, f);
           return;
         }
       else
         { if (dir)
             { ps = new_wu_process(x->d->dn.p, x->d->dnmb, f); }
           else
             { ps = new_wu_process(x->d->up.p, x->d->upmb, f); }
           d = llist_idx(&ps->p->pl, dir? 0 : 1);
           /* d is now the port of ps of default type */
           v.v.p->dec = x->d;
           v.v.p->ps = ps;
           ps->var[d->var_idx] = v;
           v.rep = REP_port;
           v.v.p = p = new_port_value(ps, f);
           p->dec = x->d;
           assign(x->x, &v, f);
         }
     }
   if (IS_SET(f->user->flags, USER_nohide))
     { var_str_printf(&f->scratch, 0, "%s/%V/wire", f->meta_ps->nm,
                      vstr_wire, &p->wprobe, f->meta_ps);
       for (c = f->scratch.s; *c != 0; c++)
         { if (*c == '.') *c = '/'; }
       ps->nm = make_str(f->scratch.s);
     }
   d = llist_idx(&ps->p->pl, dir? 1 : 0);
   alias_value_tp(&v, &ps->var[d->var_idx], f);
   f->meta_ps = ps; /* Used by get_wire_connect */
   push_value(&v, f);
 }

static void eval_field_of_union(field_of_union *x, exec_info *f)
/* Only used in connect statements */
 { value_tp v, *ve;
   value_union *vu;
   if (x->d->up.p->class == CLASS_process_def)
     { eval_wired_union_field(x, f); return; }
   eval_expr(x->x, f);
   pop_value(&v, f);
   if (!v.rep && IS_SET(f->flags, EVAL_lvalue))
     { v.rep = REP_union;
       v.v.u = vu = new_value_union(f);
       vu->f = x->d;
       vu->d = IS_SET(x->flags, EXPR_inport)? x->d->up.f : x->d->dn.f;
       ve = &vu->v;
       force_value(ve, (expr*)x, f);
       assign(x->x, &v, f);
       alias_value_tp(&v, ve, f);
       push_value(&v, f);
       return; 
     }
   else if (!v.rep)
     { push_value(&v, f);
       return; 
     }
   else if (v.rep == REP_port)
     { ve = find_reference(v.v.p->p, f);
       vu = new_value_union(f);
       vu->f = x->d;
       vu->d = IS_SET(x->flags, EXPR_inport)? x->d->dn.f : x->d->up.f;
       copy_and_clear(&vu->v, ve, f);
       ve->rep = REP_union; ve->v.u = vu;
       ve = find_reference(v.v.p, f);
       vu = new_value_union(f);
       vu->f = x->d;
       vu->d = IS_SET(x->flags, EXPR_inport)? x->d->up.f : x->d->dn.f;
       copy_and_clear(&vu->v, ve, f);
       ve->rep = REP_union; ve->v.u = vu;
       clear_value_tp(&v, f);
       alias_value_tp(&v, ve, f);
     }
   else if (v.rep == REP_array && x->x->tp.kind == TP_int)
     { exec_error(f, x, "Attempting to decompose a sliced port via a union"); }
   else if (v.rep != REP_union)
     { exec_error(f, x, "No splitting of default union value"); }
   vu = v.v.u;
   clear_value_tp(&v, f);
   if (vu->d != x->d->dn.f && vu->d != x->d->up.f)
     { exec_error(f, x, "Use of conflicting decompositions"); }
   alias_value_tp(&v, &vu->v, f);
   push_value(&v, f);
 }

static void assign_field_of_union(field_of_union *x, exec_info *f)
/* Only used in connect statements */
 { value_tp v;
   int dir;
   process_state *ps;
   var_decl *d;
   if (x->d->up.p->class == CLASS_process_def)
     { eval_expr(x->x, f);
       pop_value(&v, f);
       assert(v.rep == REP_port);
       if (v.v.p->p)
         { dir = !IS_SET(x->flags, EXPR_inport);
           assert(v.v.p->p->dec = x->d);
           f->meta_ps = ps = v.v.p->p->ps;
         }
       else
         { dir = IS_SET(x->flags, EXPR_inport);
           assert(v.v.p->dec = x->d);
           if (v.v.p->v.rep) return;
           f->meta_ps = ps = v.v.p->ps;
         }
       d = llist_idx(&ps->p->pl, dir? 1 : 0);
       copy_and_clear(&ps->var[d->var_idx], f->val, f);
     }
   else
     { v.rep = REP_union;
       v.v.u = new_value_union(f);
       copy_and_clear(&v.v.u->v, f->val, f);
       v.v.u->f = x->d;
       v.v.u->d = IS_SET(x->flags, EXPR_inport)? x->d->up.f : x->d->dn.f;
       assign(x->x, &v, f);
     }
 }

static void eval_call(call *x, exec_info *f)
 /* used for function call */
 { exec_info sub;
   ctrl_state *s;
   expr *a;
   parameter *p;
   llist ma, mp;
   value_tp *var, xval, *argv = NO_INIT;
   int i, argc = 0;
   dbg_flags ps_flags = f->curr->ps->flags;
   exec_info_init(&sub, f);
   SET_IF_SET(sub.flags, f->flags, EXEC_instantiation | EXEC_strict);
   s = new_ctrl_state(&sub);
   s->obj = (parse_obj*)x->d;
   s->nr_var = x->d->nr_var;
   s->ps = f->curr->ps;
   s->cxt = x->d->cxt;
   RESET_FLAG(f->curr->ps->flags, DBG_next);
   NEW_ARRAY(s->var, s->nr_var);
   var = s->var;
   for (i = 0; i < s->nr_var; i++)
     { var[i].rep = REP_none; }
   ma = x->a;
   mp = x->d->pl;
   while (!llist_is_empty(&mp))
     { a = llist_head(&ma);
       p = llist_head(&mp);
       eval_expr(a, f);
       pop_value(&var[p->d->var_idx], f);
       if (!var[p->d->var_idx].rep && p->par_sym == KW_const)
         { exec_error(f, a, "A constant parameter needs a value"); }
       range_check(p->d->tp.tps, &var[p->d->var_idx], f, a);
       ma = llist_alias_tail(&ma);
       mp = llist_alias_tail(&mp);
     }
   if (!llist_is_empty(&ma))
     { s->argc = argc = llist_size(&ma);
       NEW_ARRAY(argv, argc);
       s->argv = argv;
       i = 0;
       while (!llist_is_empty(&ma))
         { a = llist_head(&ma);
           eval_expr(a, f);
           pop_value(&argv[i], f);
           i++;
           ma = llist_alias_tail(&ma);
         }
     }
   insert_sched(s, &sub);
   exec_run(&sub);
   if (IS_SET(f->flags, EXEC_strict))
     { for (i = 0; i < s->nr_var; i++)
         { strict_check_delete(&var[i], f); }
     }
   if (x->d->ret) copy_and_clear(&xval, &var[x->d->ret->var_idx], f);
   else xval.rep = REP_none;
   SET_FLAG(f->curr->ps->flags, IS_SET(ps_flags, DBG_next));
   if (!xval.rep && x->d->ret)
     { exec_warning(f, x, "Function %s did not return a value", x->id); }
   for (i = 0; i < x->d->nr_var; i++)
     { clear_value_tp(&var[i], f); }
   free(var);
   if (argc)
     { for (i = 0; i < argc; i++)
         { clear_value_tp(&argv[i], f); }
       free(argv);
     }
   exec_info_term(&sub);
   push_value(&xval, f);   
 }

static int exec_call(call *x, exec_info *f)
 /* used for procedure call */
 { ctrl_state *s;
   expr *a;
   parameter *p;
   llist ma, mp;
   value_tp *var;
   int i;
   s = new_ctrl_state(f);
   s->obj = (parse_obj*)x->d;
   s->cxt = x->d->cxt;
   s->nr_var = x->d->nr_var;
   if (s->nr_var)
     { NEW_ARRAY(s->var, s->nr_var); }
   var = s->var;
   for (i = 0; i < s->nr_var; i++)
     { var[i].rep = REP_none; }
   ma = x->a;
   mp = x->d->pl;
   while (!llist_is_empty(&mp))
     { a = llist_head(&ma);
       p = llist_head(&mp);
       if (p->par_sym != KW_res)
         { eval_expr(a, f);
           pop_value(&var[p->d->var_idx], f);
           if (!var[p->d->var_idx].rep && p->par_sym == KW_const)
             { exec_error(f, a, "A constant parameter needs a value"); }
           range_check(p->d->tp.tps, &var[p->d->var_idx], f, a);
         }
       ma = llist_alias_tail(&ma);
       mp = llist_alias_tail(&mp);
     }
   if (!llist_is_empty(&ma))
     { s->argc = llist_size(&ma);
       NEW_ARRAY(s->argv, s->argc);
       i = 0;
       while (!llist_is_empty(&ma))
         { a = llist_head(&ma);
           eval_expr(a, f);
           pop_value(&s->argv[i], f);
           i++;
           ma = llist_alias_tail(&ma);
         }
     }
   s->stack = f->curr;
   f->curr->call_flags = f->curr->ps->flags;
   RESET_FLAG(f->curr->ps->flags, DBG_next);
   insert_sched(s, f);
   return EXEC_none;
 }

static int pop_call(call *x, exec_info *f)
 /* used for procedure call */
 { expr *a;
   parameter *p;
   llist ma, mp;
   value_tp *var, *pval;
   int i, argc;
   var = f->prev->var;
   if (IS_SET(f->flags, EXEC_strict))
     { for (i = 0; i < x->d->nr_var; i++)
         { strict_check_delete(&var[i], f); }
     }
   ma = x->a;
   mp = x->d->pl;
   while (!llist_is_empty(&mp))
     { a = llist_head(&ma);
       p = llist_head(&mp);
       if (p->par_sym == KW_res || p->par_sym == KW_valres)
         { pval = &var[p->d->var_idx];
           if (!pval->rep)
             { exec_warning(f, x, "Procedure %s did not assign result "
                            "parameter '%s'", x->id, p->d->id);
             }
           else
             { range_check(a->tp.tps, pval, f, a);
               assign(a, pval, f);
             }
         }
       ma = llist_alias_tail(&ma);
       mp = llist_alias_tail(&mp);
     }
   for (i = 0; i < x->d->nr_var; i++)
     { clear_value_tp(&var[i], f); }
   if (var)
     { free(var); }
   if (!llist_is_empty(&ma))
     { argc = llist_size(&ma);
       var = f->prev->argv;
       for (i = 0; i < argc; i++)
         { clear_value_tp(&var[i], f); }
       free(var);
     }
   SET_FLAG(f->curr->ps->flags, IS_SET(f->curr->call_flags, DBG_next));
   return EXEC_next;
 }

static void eval_token_expr(token_expr *x, exec_info *f)
 { value_tp xval;
   const char *s;
   value_list *vl;
   int4 i;
   switch (x->tp.kind)
     { case TP_int:
                if (x->t.tp == TOK_int || x->t.tp == TOK_char)
                  { xval.rep = REP_int;
                    xval.v.i = x->t.val.i;
                  }
                else
                  { assert(x->t.tp == TOK_z);
                    xval.rep = REP_z;
                    xval.v.z = new_z_value(f);
                    mpz_set(xval.v.z->z, x->t.val.z);
                  }
       break;
       case TP_bool:
                xval.rep = REP_bool;
                xval.v.i = (x->t.tp == KW_true)? 1 : 0;
       break;
       case TP_array: /* string */
                s = x->t.val.s;
                xval.rep = REP_array;
                i = strlen(s) + 1;
                if (i > ARRAY_REP_MAXSIZE)
                  { exec_error(f, x, "Implementation limit exceeded: Array "
                                  "size %ld > %ld",
                               (long)i, (long)ARRAY_REP_MAXSIZE);
                  }
                xval.v.l = vl = new_value_list(i, f);
                i = 0;
                while (1)
                  { vl->vl[i].rep = REP_int;
                    vl->vl[i].v.i = *s;
                    if (!*s) break;
                    s++; i++;
                  }
       break;
       case TP_symbol:
                xval.rep = REP_symbol;
                xval.v.s = x->t.val.s;
       break;
       default:
                /* can end up here in debug mode? */
                if (!IS_SET(f->user->flags, USER_debug))
                  { assert(!"Illegal type in token_expr"); }
                xval.rep = REP_none;
       break;
     }
   push_value(&xval, f);
 }

static void eval_symbol(symbol *x, exec_info *f)
 { value_tp xval;
   xval.rep = REP_symbol;
   xval.v.s = x->s;
   push_value(&xval, f);
 }

static void eval_var_ref(var_ref *x, exec_info *f)
 { value_tp xval, *val;
   assert(x->var_idx < f->curr->nr_var);
   val = &f->curr->var[x->var_idx];
   alias_value_tp(&xval, val, f);
   if (!xval.rep)
     { if (IS_SET(f->flags, EVAL_lvalue))
         { force_value(val, (expr*)x, f);
           alias_value_tp(&xval, val, f);
         }
       else
         { exec_warning(f, x, "Variable %s has no value", x->d->id); }
     }
   if (IS_SET(f->flags, EXEC_strict)) strict_check_eval(x, val, f);
   push_value(&xval, f);
 }

static void eval_rep_var_ref(rep_var_ref *x, exec_info *f)
 { value_tp xval;
   int i = x->rep_idx;
   eval_stack *rv = f->curr->rep_vals;
   while (i > 0)
     { rv = rv->next; i--; }
   alias_value_tp(&xval, &rv->v, f);
   assert(xval.rep);
   push_value(&xval, f);
 }

static void assign_var_ref(var_ref *x, exec_info *f)
 { value_tp *val;
   assert(x->var_idx < f->curr->nr_var);
   val = &f->curr->var[x->var_idx];
   if (IS_SET(f->flags, EXEC_strict)) strict_check_assign(x, val, f);
   if (val->rep)
     { clear_value_tp(val, f); }
   copy_and_clear(val, f->val, f);
 }

static void eval_const_ref(const_ref *x, exec_info *f)
 { eval_expr(x->z, f); }

static void eval_meta_ref(meta_ref *x, exec_info *f)
 { value_tp xval;
   assert(x->meta_idx < f->meta_ps->nr_meta);
   alias_value_tp(&xval, &f->meta_ps->meta[x->meta_idx], f);
   if (!xval.rep)
     { exec_error(f, x, "Meta parameter %s has no value", x->d->id); }
   push_value(&xval, f);
 }

static void eval_wire_ref(wire_ref *x, exec_info *f)
 { value_tp v;
   wire_value *w;
   eval_expr(x->x, f);
   pop_value(&v, f);
   assert(v.rep == REP_wire);
   w = v.v.w;
   if (IS_SET(f->flags, EVAL_probe_wait))
     { f->e = 0;
       add_wire_dep(w, f);
     }
   clear_value_tp(&v, f);
   v.rep = IS_SET(w->flags, WIRE_undef)? REP_none : REP_bool;
   v.v.i = IS_SET(w->flags, WIRE_value);
   push_value(&v, f);
 }

static void assign_wire_ref(wire_ref *x, exec_info *f)
 { value_tp v;
   wire_value *w;
   eval_expr(x->x, f);
   pop_value(&v, f);
   assert(v.rep == REP_wire);
   w = v.v.w;
   clear_value_tp(&v, f);
   assert(f->val->rep == REP_bool);
   write_wire(f->val->v.i, w, f);
 }

static void eval_const_expr(const_expr *x, exec_info *f)
 { value_tp xval, *ve;
   if (IS_SET(x->flags, EXPR_metax))
     { assert(x->meta_idx < f->meta_ps->nr_meta);
       ve = &f->meta_ps->meta[x->meta_idx];
       if (!ve->rep)
         { eval_expr(x->x, f);
           pop_value(ve, f);
           if (ve->rep == REP_z)
             { int_simplify(ve, f); }
         }
       alias_value_tp(&xval, ve, f);
     }
   else
     { if (!x->val.rep)
         { eval_expr(x->x, f);
           pop_value(&x->val, f);
           if (x->val.rep == REP_z)
             { int_simplify(&x->val, f); }
         }
       alias_value_tp(&xval, &x->val, f);
     }
   push_value(&xval, f);
 }

static void eval_implicit_array(implicit_array *x, exec_info *f)
 { value_tp xv, v;
   long i;
   eval_expr(x->x, f);
   pop_value(&xv, f);
   force_value(&v, (expr*)x, f);
   for (i = 0; i < v.v.l->size; i++)
     { copy_value_tp(&v.v.l->vl[i], &xv, f); }
   clear_value_tp(&xv, f);
   push_value(&v, f);
 }

static void eval_type_expr(type_expr *x, exec_info *f)
 { value_tp v;
   meta_parameter *mp;
   if (x->tps->tp.kind == TP_generic)
     { mp = (meta_parameter*)x->tps->tp.tps;
       assert(mp->class == CLASS_meta_parameter);
       alias_value_tp(&v, &f->curr->ps->meta[mp->meta_idx], f);
     }
   else
     { v.rep = REP_type;
       v.v.tp = new_type_value(f);
       v.v.tp->tp = x->tps->tp;
     }
   push_value(&v, f);
 }

/*****************************************************************************/

extern void init_expr(void)
 /* call at startup */
 { set_print(binary_expr);
   set_print(rep_expr);
   set_print(prefix_expr);
   set_print_cp(probe, prefix_expr);
   set_print(value_probe);
   set_print(array_subscript);
   set_print_cp(int_subscript, array_subscript);
   set_print(array_subrange);
   set_print(field_of_record);
   set_print(field_of_union);
   set_print(array_constructor);
   set_print(record_constructor);
   set_print(call);
   set_print(token_expr);
   set_print(symbol);
   set_print(rep_var_ref);
   set_print(var_ref);
   set_print(const_ref);
   set_print(meta_ref);
   set_print(wire_ref);
   set_print(const_expr);
   set_print(implicit_array);
   set_print(type_expr);
   set_sem(rep_expr);
   set_sem(binary_expr);
   set_sem(prefix_expr);
   set_sem_cp(probe, prefix_expr);
   set_sem(value_probe);
   set_sem(array_subscript);
   set_sem_cp(int_subscript, array_subscript);
   set_sem(array_subrange);
   set_sem(field_of_record);
   set_sem(array_constructor);
   set_sem(record_constructor);
   set_sem(call);
   set_sem(token_expr);
   set_sem(symbol);
   set_sem(type_expr);
   set_eval(binary_expr);
   set_eval(rep_expr);
   set_eval(probe);
   set_eval(prefix_expr);
   set_eval(value_probe);
   set_eval(array_subscript);
   set_assign(array_subscript);
   set_eval(int_subscript);
   set_assign(int_subscript);
   set_eval(array_subrange);
   set_assign(array_subrange);
   set_eval(field_of_record);
   set_assign(field_of_record);
   set_eval(field_of_union);
   set_assign(field_of_union);
   set_eval(array_constructor);
   /* eval_record_constructor = eval_array_constructor */
   set_eval_cp(record_constructor, array_constructor);
   set_eval(call);
   set_exec(call);
   set_pop(call);
   set_eval(token_expr);
   set_eval(symbol);
   set_eval(var_ref);
   set_assign(var_ref);
   set_eval(rep_var_ref);
   set_eval(const_ref);
   set_eval(meta_ref);
   set_eval(wire_ref);
   set_assign(wire_ref);
   set_eval(const_expr);
   set_eval(implicit_array);
   set_eval(type_expr);
 }
