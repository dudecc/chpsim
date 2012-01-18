/* statement.c: statements
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
#include "print.h"
#include "parse_obj.h"
#include "value.h"
#include "ifrchk.h"
#include "statement.h"
#include "sem_analysis.h"
#include "types.h"
#include "exec.h"
#include "expr.h"
#include "interact.h"

/********** printing *********************************************************/

static void print_skip_stmt(skip_stmt *x, print_info *f)
 { print_string("skip", f); }

static void print_end_stmt(end_stmt *x, print_info *f)
 { if (x->end_body)
     { print_char('}', f); }
   else
     { print_char(']', f); }
 }

static void print_parallel_stmt(parallel_stmt *x, print_info *f)
 { print_obj_list(&x->l, f, ", "); }

static void print_compound_stmt(compound_stmt *x, print_info *f)
 { print_string("{ ", f);
   print_obj_list(&x->l, f, "; ");
   print_string(" }", f);
 }

static void print_rep_stmt(rep_stmt *x, print_info *f)
 { if (IS_SET(f->flags, PR_cast)) print_char('<', f);
   else print_string("<<", f);
   if (x->rep_sym) print_string(token_str(0, x->rep_sym), f);
   print_char(' ', f);
   print_string(x->r.id, f);
   print_string(" : ", f);
   print_obj(x->r.l, f);
   print_string("..", f);
   print_obj(x->r.h, f);
   print_string(" : ", f);
   if (IS_SET(f->flags, PR_prs)) print_char('\n', f);
   if (x->rep_sym == SYM_bar)
     { print_obj_list(&x->sl, f, IS_SET(f->flags, PR_short)? " [] " : "\n [] "); }
   else if (x->rep_sym == SYM_arb)
     { print_obj_list(&x->sl, f, IS_SET(f->flags, PR_short)? " [:] " : "\n [:] "); }
   else if (x->rep_sym == ',')
     { print_obj_list(&x->sl, f, ", "); }
   else if (x->rep_sym == ';')
     { print_obj_list(&x->sl, f, "; "); }
   else if (IS_SET(f->flags, PR_prs))
     { print_obj_list(&x->sl, f, "\n"); }
   else
     { print_obj_list(&x->sl, f, " "); }
   if (IS_SET(f->flags, PR_prs)) print_char('\n', f);
   if (IS_SET(f->flags, PR_cast)) print_char('>', f);
   else print_string(">>", f);
 }


static void print_assignment(assignment *x, print_info *f)
 { print_obj(x->v, f);
   print_string(" := ", f);
   print_obj(x->e, f);
 }

static void print_bool_set_stmt(bool_set_stmt *x, print_info *f)
 { print_obj(x->v, f);
   print_char(x->op_sym, f);
 }

static void print_guarded_cmnd(guarded_cmnd *x, print_info *f)
 { print_obj(x->g, f);
   print_string(" -> ", f);
   print_obj_list(&x->l, f, "; ");
 }

static void print_loop_stmt(loop_stmt *x, print_info *f)
 { print_string("*[", f);
   if (!llist_is_empty(&x->gl))
     { print_char(' ', f);
       if (IS_SET(f->flags, PR_short))
         { print_obj_list(&x->gl, f, x->mutex ? " [] " : " [:] "); }
       else
         { print_obj_list(&x->gl, f, x->mutex ? "\n [] " : "\n [:] ");
           print_char('\n', f);
         }
     }
   else
     { print_obj_list(&x->sl, f, "; "); }
   print_string(" ]", f);
 }

static void print_select_stmt(select_stmt *x, print_info *f)
 { print_char('[', f);
   if (!llist_is_empty(&x->gl))
     { print_string("  ", f);
       if (IS_SET(f->flags, PR_short))
         { print_obj_list(&x->gl, f, x->mutex ? " [] " : " [:] "); }
       else
         { print_obj_list(&x->gl, f, x->mutex ? "\n[] " : "\n[:] ");
           print_char('\n', f);
         }
     }
   else
     { print_obj(x->w, f); }
   print_char(']', f);
 }

static void print_communication(communication *x, print_info *f)
 { print_obj(x->p, f);
   if (x->op_sym == '=')
     { print_char('!', f);
       print_obj(x->e, f);
       print_char('?', f);
     }
   else if (x->op_sym)
     { print_string(token_str(0, x->op_sym), f);
       print_obj(x->e, f);
     }
   else
     { assert(!x->e); }
 }

static void print_meta_binding(meta_binding *x, print_info *f)
 { if (x->x) print_obj(x->x, f);
   print_char('(', f);
   print_obj_list(&x->a, f, ", ");
   print_char(')', f);
 }

static void print_connection(connection *x, print_info *f)
 { char rsep[4] = {0, ',', ' ', 0 };
   if (IS_SET(f->flags, PR_simple_var))
     { print_string("connect ", f);
       print_char('{', f);
       f->rpos = f->pos;
       f->rsep = &rsep[1];
       print_obj(x->a, f);
       print_obj(x->a->tp.tps, f);
       print_string("}, {", f);
       f->rpos = f->pos;
       print_obj(x->b, f);
       print_obj(x->b->tp.tps, f);
       print_char('}', f);
     }
   else if (IS_SET(f->flags, PR_cast))
     { print_obj(x->a, f);
       print_string(" = ", f);
       print_obj(x->b, f);
     }
   else
     { print_string("connect ", f);
       print_obj(x->a, f);
       print_string(", ", f);
       print_obj(x->b, f);
     }
 }

static void print_process_name(type *tp, print_info *f)
 { while (tp->kind == TP_array)
     { tp = tp->elem.tp; }
   assert(tp->kind == TP_process);
   print_string(tp->elem.p->id, f);
 }

static void print_instance_stmt(instance_stmt *x, print_info *f)
 { if (IS_SET(f->flags, PR_simple_var))
     { print_string("instance ", f);
       f->rpos = f->pos;
       f->rsep = ", ";
       print_string(x->d->id, f);
       print_obj(x->d->tps, f);
       print_string(" : ", f);
       print_process_name(&x->d->tps->tp, f);
     }
   else if (IS_SET(f->flags, PR_cast))
     { print_process_name(&x->d->tps->tp, f);
       /* TODO: handle array for cast */
       print_char(' ', f);
       print_string(x->d->id, f);
     }
   else
     { f->pos += var_str_printf(f->s, f->pos, "instance %s: ", x->d->id);
       print_obj(x->d->tps, f);
       if (x->mb)
         { print_char('(', f);
           print_obj_list(&x->mb->a, f, ", ");
           print_char(')', f);
         }
     }
   if (!IS_SET(f->flags, PR_meta)) print_char(';', f);
 }

static void print_production_rule(production_rule *x, print_info *f)
 { print_obj(x->g, f);
   print_string(" -> ", f);
   print_obj(x->v, f);
   print_char(x->op_sym, f);
 }

static void print_transition(transition *x, print_info *f)
 { print_obj(x->v, f);
   print_char(x->op_sym, f);
 }

static void print_delay_hold(delay_hold *x, print_info *f)
 { print_char('{', f);
   print_obj_list(&x->l, f, ", ");
   print_string("} requires {", f);
   print_obj(x->c, f);
   if (x->n)
     { print_string(" > ", f);
       print_obj(x->n, f);
     }
   print_char('}', f);
 }

/********** semantic analysis ************************************************/

/* llist_func_p */
static void *sem_stmt(parse_obj *x, sem_info *f)
 /* Same as sem(x, f), but verify that x is a statement */
 { communication *c;
   if (!IS_SET(x->flags, DEF_forward))
     { if (BASE_CLASS(x->class) == CLASS_statement)
         { x = sem(x, f); }
       SET_FLAG(x->flags, DEF_forward);
       return x;
     }
   x = sem(x, f);
   if (BASE_CLASS(x->class) == CLASS_statement)
     { return x; }
   if (IS_SET(x->flags, EXPR_port))
     { c = new_parse(f->L, 0, x, communication);
       c->p = (expr*)x;
       c = sem(c, f);
       c = sem(c, f);
       return c;
     }
   if (x->class == CLASS_call)
     { if (!((call*)x)->d->ret) return x; /* procedure call */
       sem_error(f, x,
                 "A function call is not a statement; use a procedure instead");
     }
   else if (x->class == CLASS_binary_expr && ((binary_expr*)x)->op_sym == '=')
     { sem_error(f, x, "This is a comparison; maybe you meant ':='?"); }
   sem_error(f, x, "Expected a statement, not an expresssion");
   return x;
 }

/* declared in sem_analysis.h */
extern void sem_stmt_list(llist *l, sem_info *f)
 /* Use this instead of sem_list(l, f) when l should be a llist(statement).
    Same as sem_list(), but verify that there are only statements.
 */
 { llist_apply_overwrite(l, (llist_func_p*)sem_stmt, f);
 }

static void *sem_parallel_stmt(parallel_stmt *x, sem_info *f)
 { sem_stmt_list(&x->l, f);
   return x;
 }

static void *sem_compound_stmt(compound_stmt *x, sem_info *f)
 { sem_stmt_list(&x->l, f);
   return x;
 }

static void *sem_rep_stmt(rep_stmt *x, sem_info *f)
 { if (!IS_SET(x->flags, DEF_forward))
     { SET_FLAG(x->flags, DEF_forward);
       sem_stmt_list(&x->sl, f);
       return x;
     }
   sem_rep_common(&x->r, f);
   enter_sublevel(x, x->r.id, &x->cxt, f);
   sem_stmt_list(&x->sl, f);
   leave_level(f);
   return x;
 }

static void *sem_assignment(assignment *x, sem_info *f)
 { if (!IS_SET(x->flags, DEF_forward))
     { SET_FLAG(x->flags, DEF_forward);
       return x;
     }
   SET_FLAG(f->flags, SEM_need_lvalue);
   x->v = sem(x->v, f);
   RESET_FLAG(f->flags, SEM_need_lvalue);
   x->e = sem(x->e, f);
   if (!IS_SET(x->v->flags, EXPR_writable))
     { sem_error(f, x, "Left-hand side of assignment is not writable"); }
   if (IS_SET(x->e->flags,EXPR_port))
     { sem_error(f, x, "Assigned value is a port"); }
   if (!type_compatible(&x->v->tp, &x->e->tp))
     { sem_error(f, x, "Assignment between incompatible types"); }
   x->e = mk_const_expr(x->e, f);
   return x;
 }

static void *sem_bool_set_stmt(bool_set_stmt *x, sem_info *f)
 { if (!IS_SET(x->flags, DEF_forward))
     { SET_FLAG(x->flags, DEF_forward);
       return x;
     }
   SET_FLAG(f->flags, SEM_need_lvalue);
   x->v = sem(x->v, f);
   RESET_FLAG(f->flags, SEM_need_lvalue);
   if (!IS_SET(x->v->flags, EXPR_writable))
     { sem_error(f, x, "Boolean '%c' assignment requires a writable variable",
                 x->op_sym);
     }
   if (x->v->tp.kind != TP_bool)
     { sem_error(f, x, "Boolean '%c' requires a boolean variable", x->op_sym); }
   return x;
 }

static void *sem_guarded_cmnd(guarded_cmnd *x, sem_info *f)
 { if (IS_SET(x->flags, DEF_forward))
     { x->g = sem(x->g, f);
       if (IS_SET(x->g->flags,EXPR_port))
         { sem_error(f, x, "Guard is a port"); }
       if (x->g->tp.kind != TP_bool)
         { sem_error(f, x->g, "Guard is not a boolean"); }
       x->g = mk_const_expr(x->g, f); /* TODO: warn if const */
     }
   sem_stmt_list(&x->l, f);
   SET_FLAG(x->flags, DEF_forward);
   return x;
 }

static void *sem_loop_stmt(loop_stmt *x, sem_info *f)
 { if (!llist_is_empty(&x->gl))
     { sem_list(&x->gl, f);
       x->glr = llist_copy(&x->gl, 0, 0); /* see sem_select_stmt */
       llist_reverse(&x->glr);
     }
   else
     { sem_stmt_list(&x->sl, f); }
   return x;
 }

static void *sem_select_stmt(select_stmt *x, sem_info *f)
 { if (!x->w)
     { sem_list(&x->gl, f);
       /* we reverse the list to avoid accidental order-dependence, but
          for purpose of printing we keep the original list in order. */
       x->glr = llist_copy(&x->gl, 0, 0);
       llist_reverse(&x->glr);
     }
   else if (IS_SET(x->flags, DEF_forward))
     { x->w = sem(x->w, f);
       if (IS_SET(x->w->flags, EXPR_port))
         { sem_error(f, x, "Wait expression is a port"); }
       if (x->w->tp.kind != TP_bool)
         { sem_error(f, x->w, "A wait requires a boolean expression"); }
       x->w = mk_const_expr(x->w, f); /* TODO: warn if const */
     }
   SET_FLAG(x->flags, DEF_forward);
   return x;
 }

static void *sem_communication(communication *x, sem_info *f)
 { if (!IS_SET(x->flags, DEF_forward))
     { SET_FLAG(x->flags, DEF_forward);
       return x;
     }
   if (IS_SET(f->flags, SEM_meta))
     { sem_error(f, x, "A meta process cannot contain communications"); }
   if (x->op_sym) /* !x->op_sym means that x->p has already been sem'd */
     { x->p = sem(x->p, f); }
   if (x->op_sym == '!')
     { if (IS_SET(x->p->flags,EXPR_port) != EXPR_outport)
         { sem_error(f, x->p, "A send needs an output port"); }
       x->e = sem(x->e, f);
       if (IS_SET(x->e->flags,EXPR_port))
         { sem_error(f, x, "Sent value is a port"); }
       if (!type_compatible(&x->p->tp, &x->e->tp))
         { sem_error(f, x->e, "Sending a value of incompatible type"); }
       x->e = mk_const_expr(x->e, f);
     }
   else if (x->op_sym == '?' || x->op_sym == SYM_peek)
     { if (IS_SET(x->p->flags,EXPR_port) != EXPR_inport)
         { sem_error(f, x->p, "A receive needs an input port"); }
       SET_FLAG(f->flags, SEM_need_lvalue);
       x->e = sem(x->e, f);
       RESET_FLAG(f->flags, SEM_need_lvalue);
       if (!IS_SET(x->e->flags, EXPR_writable))
         { sem_error(f, x, "Operand of receive is not writable"); }
       if (!type_compatible(&x->p->tp, &x->e->tp))
         { sem_error(f, x->e, "Receiving a value of incompatible type"); }
     }
   else if (!x->op_sym)
     { /* This case is initially parsed as expr, but converted by
          sem_stmt(). */
       assert(!x->e);
       if (!IS_SET(x->p->flags,EXPR_inport))
         { sem_error(f, x->p, "Expected a sync port or an input port"); }
     }
   else if (x->op_sym == '=')
     { if (IS_SET(x->p->flags,EXPR_port) != EXPR_outport)
         { sem_error(f, x->p, "The send of a pass needs an output port"); }
       x->e = sem(x->e, f);
       if (IS_SET(x->e->flags,EXPR_port) != EXPR_inport)
         { sem_error(f, x->e, "The receive of a pass needs an input port"); }
       if (!type_compatible(&x->p->tp, &x->e->tp))
         { sem_error(f, x->e, "Passing between incompatible ports"); }
     }
   else
     { assert(!"unknown communication command"); }
   return x;
 }

extern void sem_meta_binding_aux(process_def *d, meta_binding *x, sem_info *f)
 { llist ma, mp;
   expr *a;
   meta_parameter *p;
   ma = x->a;
   mp = d->ml;
   if (llist_size(&ma) != llist_size(&mp))
     { sem_error(f, x, "Wrong number of arguments in meta-binding"); }
   while (!llist_is_empty(&mp))
     { p = llist_head(&mp);
       a = llist_head(&ma);
       if (IS_SET(a->flags, EXPR_port))
         { sem_error(f, a, "Argument for parameter %s is a port", p->id); }
       if (!IS_SET(p->flags, EXPR_generic) && !type_compatible(&p->tp, &a->tp))
         { sem_error(f, a, "Argument type mismatch for parameter %s", p->id); }
       mp = llist_alias_tail(&mp);
       ma = llist_alias_tail(&ma);
     }
 }

static void *sem_meta_binding(meta_binding *x, sem_info *f)
 { type *tp;
   if (!IS_SET(x->flags, DEF_forward))
     { SET_FLAG(x->flags, DEF_forward);
       return x;
     }
   x->x = sem(x->x, f);
   tp = &x->x->tp;
   while (tp && tp->kind == TP_array) tp = tp->elem.tp;
   if (!tp || tp->kind != TP_process)
     { sem_error(f, x, "Expected a process instance"); }
   SET_FLAG(f->flags, SEM_meta_binding);
   sem_list(&x->a, f);
   RESET_FLAG(f->flags, SEM_meta_binding);
   sem_meta_binding_aux(tp->elem.p, x, f);
   return x;
 }

static void *sem_connection(connection *x, sem_info *f)
 { type *atp, *btp;
   expr *tmp;
   if (!IS_SET(x->flags, DEF_forward))
     { SET_FLAG(x->flags, DEF_forward);
       return x;
     }
   SET_FLAG(f->flags, SEM_connect);
   x->a = sem(x->a, f);
   x->b = sem(x->b, f);
   RESET_FLAG(f->flags, SEM_connect);
   if (IS_SET(x->a->flags ^ x->b->flags, EXPR_wire)) /* Only one port wired? */
     { if (!IS_SET(x->a->flags, EXPR_unconst))
         { tmp = x->a; x->a = x->b; x->b = tmp; }
       else if (IS_SET(x->b->flags, EXPR_unconst))
         { sem_error(f, x, "Connecting a standard port to a wired port"); }
       if (!type_compatible(&x->a->tp, &x->b->tp))
         { sem_error(f, x, "Incompatible wired types."); }
       x->class = CLASS_const_wired_connection;
     }
   else if (IS_SET(x->a->flags, EXPR_wire))
     /* type compatible does the error messages for wire types */
     { if (!type_compatible(&x->a->tp, &x->b->tp))
         { sem_error(f, x, "Incompatible wired types."); }
       x->class = CLASS_wired_connection;
     }
   else
     { if (!IS_SET(x->a->flags, EXPR_port))
         { sem_error(f, x->a, "First argument of 'connect' is not a port"); }
       if (!IS_SET(x->b->flags, EXPR_port))
         { sem_error(f, x->b, "Second argument of 'connect' is not a port"); }
       atp = &x->a->tp;
       btp = &x->b->tp;
       if (atp->kind == TP_syncport || btp->kind == TP_syncport)
         { if (atp->kind != TP_syncport || btp->kind != TP_syncport)
             { sem_error(f, x, "Cannot connect a data port to a sync port"); }
         }
       else
         { if ((IS_SET(x->a->flags ^ x->b->flags, EXPR_port_ext)==0) ^
               (IS_SET(x->a->flags & x->b->flags, EXPR_port)==0))
             { sem_error(f, x, "The data port directions are mismatched"); }
           /* If one of the ports is external and templated (generic),
              compatibility cannot be checked until execution
            */
           if (IS_SET(~x->a->flags, EXPR_port_ext | EXPR_generic) &&
               IS_SET(~x->b->flags, EXPR_port_ext | EXPR_generic) &&
               !type_compatible(atp, btp))
             { sem_error(f, x, "The data port types are mismatched"); }
         }
     }
   return x;
 }

/* We associate a variable with each instance, which will point to the
   instance's state. Then references to the instance can be treated as
   variable references.
*/
static void *sem_instance_stmt(instance_stmt *x, sem_info *f)
 { type *etp;
   if (!IS_SET(f->flags, SEM_meta))
     { sem_error(f, x, "Only a meta process can declare instances"); }
   if (!f->cxt->H) /* Happens inside of replication */
     { sem_error(f, x, "'instance' cannot appear in a replicated statement"); }
   if (!IS_SET(x->flags, DEF_forward))
     { x->d->var_idx = f->var_idx++; }
   SET_FLAG(f->flags, SEM_instance_decl);
   x->d->tps = sem(x->d->tps, f);
   RESET_FLAG(f->flags, SEM_instance_decl);
   declare_id(f, x->d->id, x);
   x->d->tp = x->d->tps->tp;
   etp = &x->d->tp;
   while (etp->kind == TP_array)
     { etp = etp->elem.tp; }
   if (etp->kind != TP_process)
     { sem_error(f,x->d->tps,"An instance declaration needs a process name"); }
   if (IS_SET(x->flags, DEF_forward))
     { if (x->mb && !IS_SET(x->mb->flags, DEF_forward))
         /* x->mb is not really valid, so don't just sem it.  Also, x->mb may
          * be shared, so use DEF_forward in x->mb to only run checks once.
          */
         { SET_FLAG(x->mb->flags, DEF_forward);
           SET_FLAG(f->flags, SEM_meta_binding);
           sem_list(&x->mb->a, f);
           RESET_FLAG(f->flags, SEM_meta_binding);
           sem_meta_binding_aux(etp->elem.p, x->mb, f);
         }
     }
   SET_FLAG(x->flags, DEF_forward);
   return x;
 }

static void *sem_production_rule(production_rule *x, sem_info *f)
 { if (!IS_SET(x->flags, DEF_forward))
     { SET_FLAG(x->flags, DEF_forward);
       return x;
     }
   x->g = sem(x->g, f);
   if (IS_SET(x->g->flags, EXPR_port))
     { sem_error(f, x, "Production rule guard is a port"); }
   if (x->g->tp.kind != TP_bool)
     { sem_error(f, x->g, "Guard is not a boolean"); }
   x->g = mk_const_expr(x->g, f); /* TODO: warn if const */
   SET_FLAG(f->flags, SEM_need_lvalue);
   x->v = sem(x->v, f);
   RESET_FLAG(f->flags, SEM_need_lvalue);
   if (!IS_SET(x->v->flags, EXPR_writable))
     { sem_error(f, x, "Target of rule is not a writable variable"); }
   if (IS_SET(f->flags, SEM_delay))
     { if (x->v->tp.kind != TP_int)
         { sem_error(f, x, "Target of rule is not a counter variable"); }
       if (x->op_sym == '-')
         { if (x->delay)
             { sem_error(f, x, "Counter decrement may not be delayed"); }
           x->atomic = 1;
         }
     }
   else if (x->v->tp.kind != TP_bool)
     { sem_error(f, x, "Target of rule is not a boolean variable"); }
   if (x->delay)
     { x->delay = sem(x->delay, f);
       if (x->delay->tp.kind != TP_int)
         { sem_error(f, x, "Production rule delay is not an integer"); }
       if (IS_SET(x->delay->flags, EXPR_unconst))
         { sem_error(f, x, "Production rule delay is not constant"); }
     }
   return x;
 }

static void *sem_transition(transition *x, sem_info *f)
 { x->v = sem(x->v, f);
   if (!IS_SET(x->v->flags, EXPR_writable))
     { sem_error(f, x, "Held variable is not writable"); }
   if (x->v->tp.kind != TP_bool)
     { sem_error(f, x, "Held variable is not a boolean"); }
   return x;
 }

static void *sem_delay_hold(delay_hold *x, sem_info *f)
 { if (!IS_SET(x->flags, DEF_forward))
     { SET_FLAG(x->flags, DEF_forward);
       return x;
     }
   sem_list(&x->l, f);
   x->c = sem(x->c, f);
   if (!IS_SET(x->c->flags, EXPR_writable) || x->c->tp.kind != TP_int)
     { sem_error(f, x, "Delay hold requirement is not a counter"); }
   if (x->n)
     { x->n = sem(x->n, f);
       if (x->n->tp.kind != TP_int)
         { sem_error(f, x, "Delay hold threshold is not an integer"); }
       if (IS_SET(x->n->flags, EXPR_unconst))
         { sem_error(f, x, "Delay hold threshold is not constant"); }
     }
   return x;
 }


/********** execution ********************************************************/

static int exec_parallel_stmt(parallel_stmt *x, exec_info *f)
 { llist m;
   ctrl_state *s;
   if (IS_SET(f->flags, EXEC_immediate))
     { exec_immediate(&x->l, f);
       return EXEC_next;
     }
   m = x->l;
   f->curr->i = 0;
   f->curr->ps->nr_thread--; /* one child counts as current thread */
   while (!llist_is_empty(&m))
     { s = new_ctrl_state(f);
       s->obj = llist_head(&m);
       s->var = f->curr->var;
       s->nr_var = f->curr->nr_var;
       s->stack = f->curr;
       insert_sched(s, f);
       f->curr->ps->nr_thread++;
       f->curr->i++;
       m = llist_alias_tail(&m);
     }
   return EXEC_none;
 }

static int pop_parallel_stmt(parallel_stmt *x, exec_info *f)
 { f->curr->i--;
   if (IS_SET(x->flags, EXPR_ifrchk)) strict_check_frame_end(f);
   if (f->curr->i) /* not all children are finished */
     { f->curr->ps->nr_thread--;
       return EXEC_none;
     }
   if (IS_SET(x->flags, EXPR_ifrchk)) strict_check_update(f->curr, f);
   return EXEC_next;
 }

static int exec_compound_stmt(compound_stmt *x, exec_info *f)
 { ctrl_state *s;
   if (IS_SET(f->flags, EXEC_immediate))
     { exec_immediate(&x->l, f);
       return EXEC_next;
     }
   s = nested_seq(&x->l, f);
   insert_sched(s, f);
   return EXEC_none;
 }

static int exec_rep_stmt(rep_stmt *x, exec_info *f)
 { ctrl_state *s;
   value_tp ival;
   eval_stack *rv;
   long i, n;
   n = eval_rep_common(&x->r, &ival, f);
   if (IS_SET(f->flags, EXEC_immediate))
     { push_repval(&ival, f->curr, f);
       for (i = 0; i < n; i++)
         { exec_immediate(&x->sl, f);
           int_inc(&f->curr->rep_vals->v, f);
         }
       pop_repval(&ival, f->curr, f);
       clear_value_tp(&ival, f);
       return EXEC_next;
     }
   else if (x->rep_sym == ',')
     { f->curr->i = n;
       f->curr->ps->nr_thread += n - 1; /* one child counts as current thread */
       NEW_ARRAY(rv, n);
       /* rather than permanently allocate a large number of
          eval_stacks, we allocate temporarily a single block
          of them.  This block is stored on f->curr->rep_vals
          so we only free it when all children finish
        */
       for (i = 0; i < n; i++)
         { s = nested_seq(&x->sl, f);
           s->cxt = x->cxt;
           rv[i].next = s->rep_vals;
           copy_value_tp(&rv[i].v, &ival, f);
           s->rep_vals = &rv[i];
           insert_sched(s, f);
           int_inc(&ival, f);
         }
       clear_value_tp(&ival, f);
       f->curr->rep_vals = rv; /* keep track of what must be freed */
     }
   else if (x->rep_sym == ';')
     { f->curr->i = n;
       s = nested_seq(&x->sl, f);
       s->cxt = x->cxt;
       push_repval(&ival, s, f);
       insert_sched(s, f);
     }
   return EXEC_none;
 }

static int pop_rep_stmt(rep_stmt *x, exec_info *f)
 { eval_stack *w;
   if (x->rep_sym == ',')
     { f->curr->i--;
       if (IS_SET(x->flags, EXPR_ifrchk)) strict_check_frame_end(f);
       /* in this case, all the parallel rep_vals are allocated
          as a block so we override the automatic freeing in
          exec_next and free the block only when we are done
        */
       clear_value_tp(&f->prev->rep_vals->v, f);
       if (f->curr->i) /* not all children are finished */
         { f->curr->ps->nr_thread--;
           f->prev->rep_vals = f->curr->rep_vals;
           /* ^^^^ No automatic freeing */
           return EXEC_none;
         }
       w = f->curr->rep_vals->next;
       free(f->curr->rep_vals);
       f->prev->rep_vals = f->curr->rep_vals = w;
       /* ^^^^ No automatic freeing */
       if (IS_SET(x->flags, EXPR_ifrchk)) strict_check_update(f->curr, f);
     }
   else
     { assert(x->rep_sym == ';');
       f->curr->i--;
       if (f->curr->i) /* not done with replication yet */
         { int_inc(&f->prev->rep_vals->v, f);
           f->prev->obj = llist_head(&x->sl);
           f->prev->seq = llist_alias_tail(&x->sl);
           insert_sched(f->prev, f);
           f->prev = 0;
           return EXEC_none;
         }
     }
   return EXEC_next;
 }

static int exec_assignment(assignment *x, exec_info *f)
 { value_tp eval;
   eval_expr(x->e, f);
   pop_value(&eval, f);
   if (!eval.rep)
     { exec_warning(f, x->e,
                    "Unknown value %v at right-hand side of assignment",
                    vstr_obj, x->e);
     }
   else
     { if (eval.rep == REP_z)
         { int_simplify(&eval, f); }
       range_check(x->v->tp.tps, &eval, f, x);
       assign(x->v, &eval, f);
     }
   return EXEC_next;
 }

static int exec_bool_set_stmt(bool_set_stmt *x, exec_info *f)
 { value_tp eval;
   eval.rep = REP_bool;
   eval.v.i = (x->op_sym == '+')? 1 : 0;
   assign(x->v, &eval, f);
   return EXEC_next;
 }

static int find_true_guard_llist(llist *l, exec_info *f);

static int find_true_guard(void *stmt, exec_info *f)
/* Find and schedule the guarded cmnd with the true guard */
 { guarded_cmnd *gc = (guarded_cmnd*)stmt;
   rep_stmt *rs = (rep_stmt*)stmt;
   ctrl_state *s;
   value_tp ival, gval;
   long i, n;
   if (!f->curr->i && f->gc) return 0;
   if (gc->class == CLASS_guarded_cmnd)
     { eval_expr(gc->g, f);
       pop_value(&gval, f);
       if (gval.rep && gval.v.i)
         { if (f->gc)
             { exec_error(f, gc, "Guards %v and %#v are both true",
                             vstr_obj, gc->g, f->gc->g);
               /* TODO: include rep_vals in error message */
             }
           f->gc = gc;
           f->gcrv = f->curr->rep_vals;
           if (!IS_SET(f->flags, EXEC_immediate))
             { s = nested_seq(&gc->l, f);
               insert_sched(s, f);
             }
         }
     }
   else if (rs->class == CLASS_rep_stmt)
     { n = eval_rep_common(&rs->r, &ival, f);
       push_repval(&ival, f->curr, f);
       for (i = 0; i < n; i++)
         { gc = f->gc;
           find_true_guard_llist(&rs->sl, f);
           if (gc != f->gc)
             { copy_value_tp(&ival, &f->curr->rep_vals->v, f);
               f->curr->rep_vals = f->curr->rep_vals->next;
               push_repval(&ival, f->curr, f);
             }
           int_inc(&f->curr->rep_vals->v, f);
         }
       pop_repval(&ival, f->curr, f);
       clear_value_tp(&ival, f);
     }
   else
     { assert(!"Internal error: Expected a guarded command"); }
   return 0;
 }
                         
static int find_true_guard_llist(llist *l, exec_info *f)
 { return llist_apply(l, (llist_func*)find_true_guard, f); }

static void exec_imm_true_guard(exec_info *f)
 /* Pre: EXEC_immediate is set, find_true_guard has found one true guard
  * Execute the guard left in f->gc, then free the values in f->gcrv
  */
 { eval_stack *rv = f->curr->rep_vals;
   value_tp v;
   f->curr->rep_vals = f->gcrv;
   exec_immediate(&f->gc->l, f);
   while (f->curr->rep_vals != rv)
     { pop_repval(&v, f->curr, f);
       clear_value_tp(&v, f);
     }
 }

static int exec_loop_stmt(loop_stmt *x, exec_info *f)
 { ctrl_state *s;
   if (!llist_is_empty(&x->glr))
     { while (1)
         { f->gc = 0;
           f->curr->i = x->mutex;
           find_true_guard_llist(&x->glr, f);
           f->curr->i = 0;
           if (!IS_SET(f->flags, EXEC_immediate) || !f->gc) break;
           exec_imm_true_guard(f);
         }
       if (!f->gc)
         { return EXEC_next; }
     }
   else /* TODO: error for EXEC_immediate */
     { s = nested_seq(&x->sl, f);
       insert_sched(s, f);
     }
   return EXEC_none;
 }

static int exec_select_stmt(select_stmt *x, exec_info *f)
 { value_tp gval;
   if (!llist_is_empty(&x->glr))
     { f->gc = 0;
       f->curr->i = x->mutex;
       find_true_guard_llist(&x->glr, f);
       f->curr->i = 0;
       if (IS_SET(f->flags, EXEC_immediate))
         { if (!f->gc)
             { exec_error(f, x, "Select has no true guards during"
                                " non-parallel execution.");
             }
           exec_imm_true_guard(f);
           return EXEC_next;
         }
       if (f->gc)
         { return EXEC_none; }
       SET_FLAG(f->flags, EVAL_probe_wait);
       find_true_guard_llist(&x->glr, f);
       RESET_FLAG(f->flags, EVAL_probe_wait);
     }
   else
     { eval_expr(x->w, f);
       pop_value(&gval, f);
       if (gval.rep && gval.v.i)
         { return EXEC_next; }
       SET_FLAG(f->flags, EVAL_probe_wait);
       eval_expr(x->w, f);
       pop_value(&gval, f);
       RESET_FLAG(f->flags, EVAL_probe_wait);
     }
   return EXEC_suspend;
 }

/* Return true if all probes in pval are one.  If EVAL_probe_zero is set,
   we instead check if all probes on the other side of the channel are zero.
 */
extern int get_probe(value_tp *pval, exec_info *f)
 { value_list *vl;
   port_value *p;
   wire_value *wp;
   int i, x = WIRE_value;
   switch (pval->rep)
     { case REP_none:
       assert(!"Internal error: port has no value");
       case REP_union:
         x = get_probe(&pval->v.u->v, f);
       break;
       case REP_array: case REP_record:
         vl = pval->v.l;
         for (i = 0 ; i < vl->size ; i++)
           { x &= get_probe(&vl->vl[i], f); }
       break;
       case REP_port:
         p = pval->v.p;
         if (!p->p)
           { exec_error(f, f->curr->obj,
                        "Communication on disconnected port %v",
                        vstr_obj, ((communication*)f->curr->obj)->p);
           }
         if (IS_SET(f->flags, EVAL_probe_zero))
           { wp = &p->p->wprobe;
             x = IS_SET(~wp->flags, WIRE_value);
           }
         else
           { wp = &p->wprobe;
             x = IS_SET(wp->flags, WIRE_value);
           }
         if (!x && IS_SET(f->flags, EVAL_probe_wait))
           { add_wire_dep(wp, f); }
       break;
       default: /* can happen in value probe */
       break;
     }
   return x;
 }

/* Pre: no ports of pval are dangling (check with get_probe)
   Set all probes of ports connected to pval to one,
   or probes of ports of pval to zero if EVAL_probe_zero is set
 */
static void set_probe(value_tp *pval, exec_info *f)
 { value_list *vl;
   int i;
   assert(pval->rep);
   if (pval->rep == REP_union)
     { set_probe(&pval->v.u->v, f); }
   else if (pval->rep == REP_array || pval->rep == REP_record)
     { vl = pval->v.l;
       for (i = 0 ; i < vl->size ; i++)
         { set_probe(&vl->vl[i], f); }
     }
   else
     { assert(pval->rep == REP_port);
       if (IS_SET(f->flags, EVAL_probe_zero))
         { write_wire(0, &pval->v.p->wprobe, f); }
       else
         { assert(pval->v.p->p);
           write_wire(1, &pval->v.p->p->wprobe, f);
         }
     }
 }

static value_tp eval_fn(value_tp *v, function_def *x, exec_info *f)
 /* evaluate a single argument function */
 { exec_info sub;
   ctrl_state *s;
   parameter *p;
   value_tp *var, xval;
   int i;
   dbg_flags ps_flags = f->curr->ps->flags;
   exec_info_init(&sub, f);
   SET_IF_SET(sub.flags, f->flags, EXEC_instantiation);
   s = new_ctrl_state(&sub);
   s->obj = (parse_obj*)x;
   s->nr_var = x->nr_var;
   s->ps = f->curr->ps;
   s->cxt = x->cxt;
   RESET_FLAG(f->curr->ps->flags, DBG_next);
   NEW_ARRAY(s->var, s->nr_var);
   var = s->var;
   for (i = 0; i < s->nr_var; i++)
     { var[i].rep = REP_none; }
   p = llist_head(&x->pl);
   copy_and_clear(&var[p->d->var_idx], v, f);
   range_check(p->d->tp.tps, &var[p->d->var_idx], f, f->curr->obj);
   insert_sched(s, &sub);
   exec_run(&sub);
   copy_and_clear(&xval, &var[x->ret->var_idx], f);
   SET_FLAG(f->curr->ps->flags, IS_SET(ps_flags, DBG_next));
   for (i = 0; i < x->nr_var; i++)
     { clear_value_tp(&var[i], f); }
   free(var);
   exec_info_term(&sub);
   return xval;
 }

/* Pre: dval->rep != 0
   Do not use value dval after this
 */
static void send_value(value_tp *dval, value_tp *pval, exec_info *f)
 { value_list *dl, *pl;
   value_union *vu;
   value_tp v;
   long i, j;
   assert(pval->rep);
   if (pval->rep == REP_union)
     { vu = pval->v.u;
       v = eval_fn(dval, vu->d, f);
       send_value(&v, &vu->v, f);
     }
   else if (pval->rep == REP_array || pval->rep == REP_record)
     { pl = pval->v.l;
       if (dval->rep == REP_int)
         { v.rep = REP_bool;
           j = 1;
           for (i = 0; i < pl->size; i++)
             { v.v.i = (dval->v.i & j) != 0;
               send_value(&v, &pl->vl[i], f);
               if (j > 0) j = j << 1;
             }
         }
       else if (dval->rep == REP_z)
         { v.rep = REP_bool;
           for (i = 0; i < pl->size; i++)
             { v.v.i = mpz_tstbit(dval->v.z->z, i);
               send_value(&v, &pl->vl[i], f);
             }
           clear_value_tp(dval, f);
         }
       else
         { assert(pval->rep == dval->rep);
           dl = dval->v.l;
           assert(dl->size == pl->size);
           for (i = 0 ; i < dl->size ; i++)
             { alias_value_tp(&v, &dl->vl[i], f);
               send_value(&v, &pl->vl[i], f);
             }
           clear_value_tp(dval, f);
         }
     }
   else
     { assert(pval->rep == REP_port);
       assert(pval->v.p->p);
       pval->v.p->p->v = *dval;
     }
  }

/* f->curr->i should be false if this is a peek */
extern value_tp receive_value(value_tp *pval, type *tp, exec_info *f)
 { value_list *dl, *pl;
   value_union *vu;
   value_tp dv, v, *tpv, lv, hv;
   process_state *meta_ps = f->meta_ps;
   meta_parameter *mp;
   integer_type *itps;
   long i;
   mpz_t fix;
   llist l;
   assert(pval->rep);
   while (tp && tp->kind == TP_generic)
     { mp = (meta_parameter*)tp->tps;
       assert(mp->class == CLASS_meta_parameter);
       tpv = &f->meta_ps->meta[mp->meta_idx];
       tp = &tpv->v.tp->tp;
       f->meta_ps = tpv->v.tp->meta_ps;
     }
   switch (pval->rep)
     { case REP_union:
         vu = pval->v.u;
         v = receive_value(&vu->v, &vu->f->tp, f);
         dv = eval_fn(&v, vu->d, f);
       break;
       case REP_array:
         pl = pval->v.l;
         if (tp->kind == TP_int)
           { itps = (integer_type*)tp->tps;
             assert(itps && itps->class == CLASS_integer_type);
             /* Strategy: take the lower bound of our integer type
                          change the bits of this value to the received bits
                          if the result is too low, increment by 2^nr_bits
             */
             eval_expr(itps->l, f);
             pop_value(&lv, f);
             if (lv.rep == REP_int)
               { dv.rep = REP_z;
                 dv.v.z = new_z_value(f);
                 mpz_set_si(dv.v.z->z, lv.v.i);
               }
             else
               { assert(lv.rep == REP_z);
                 copy_value_tp(&dv, &lv, f);
               }
             for (i = 0; i < pl->size; i++)
               { v = receive_value(&pl->vl[i], 0, f);
                 if (v.v.i)
                   { mpz_setbit(dv.v.z->z, i); }
                 else
                   { mpz_clrbit(dv.v.z->z, i); }
               }
             if (int_cmp(&dv, &lv, f) < 0)
               { mpz_init(fix);
                 mpz_setbit(fix, i);
                 mpz_add(dv.v.z->z, dv.v.z->z, fix);
                 mpz_clear(fix);
               }
             clear_value_tp(&lv, f);
             int_simplify(&dv, f);
           }
         else
           { dv.rep = pval->rep;
             dv.v.l = dl = new_value_list(pl->size, f);
             for (i = 0 ; i < dl->size ; i++)
               { dl->vl[i] = receive_value(&pl->vl[i], tp->elem.tp, f); }
           }
       break;
       case REP_record:
         pl = pval->v.l;
         dv.rep = pval->rep;
         dv.v.l = dl = new_value_list(pl->size, f);
         l = tp->elem.l;
         for (i = 0 ; i < dl->size ; i++)
           { dl->vl[i] = receive_value(&pl->vl[i], llist_head(&l), f);
             l = llist_alias_tail(&l);
           }
       break;
       case REP_port:
         if (!pval->v.p->v.rep)
           { exec_error(f, f->curr->obj, "Receiving an unknown value"); }
         if (IS_SET(f->flags, EVAL_probe))
           { copy_value_tp(&dv, &pval->v.p->v, f); }
         else
           { copy_and_clear(&dv, &pval->v.p->v, f); }
       break;
       default:
         alias_value_tp(&dv, pval, f); /* can happen in value probe */
       break;
     }
   f->meta_ps = meta_ps;
   return dv;
 }

static int exec_comm_pass(communication *x, exec_info *f)
 /* Pre: x->op_sym = '=' */
 { value_tp pval, qval, v;
   int pprobe, qprobe, probe = 0;
   eval_expr(x->p, f);
   pop_value(&pval, f);
   eval_expr(x->e, f);
   pop_value(&qval, f);
   if (f->curr->i == 1) goto wait1;
   else if (f->curr->i == 2) goto wait2;
   else if (f->curr->i == 3) goto wait3;
   f->e = 0;
   SET_FLAG(f->flags, EVAL_probe_zero | EVAL_probe_wait);
   probe = get_probe(&pval, f);
   probe &= get_probe(&qval, f);
   RESET_FLAG(f->flags, EVAL_probe_zero | EVAL_probe_wait);
   /* wait0: [!pp->probe && !qq->probe] */
   if (!probe)
     { clear_value_tp(&pval, f);
       clear_value_tp(&qval, f);
       return EXEC_suspend;
     }
   f->curr->i = 1;
   wait1:
   SET_FLAG(f->flags, EVAL_probe_wait);
   f->e = 0;
   probe = pprobe = get_probe(&pval, f);
   f->e = 0;
   probe |= qprobe = get_probe(&qval, f);
   RESET_FLAG(f->flags, EVAL_probe_wait);
   /* wait1: [p->probe || q->probe] */
   if (!probe)
     { clear_value_tp(&pval, f);
       clear_value_tp(&qval, f);
       return EXEC_suspend;
     }
   if (qprobe)
     { set_probe(&pval, f); /* pp->probe = 1 */
       f->curr->i = 2;
       v = receive_value(&qval, &x->e->tp, f);
       send_value(&v, &pval, f);
     wait2:
       if (!probe)
         { SET_FLAG(f->flags, EVAL_probe_wait);
           f->e = 0;
           pprobe = get_probe(&pval, f);
           RESET_FLAG(f->flags, EVAL_probe_wait);
         }
       /* [p->probe]; */
       if (!pprobe)
         { clear_value_tp(&pval, f);
           clear_value_tp(&qval, f);
           return EXEC_suspend;
         }
     }
   else /* p->probe */
     { set_probe(&qval, f); /* qq->probe = 1 */
       f->curr->i = 3;
     wait3:
       if (!probe)
         { SET_FLAG(f->flags, EVAL_probe_wait);
           f->e = 0;
           qprobe = get_probe(&qval, f);
           RESET_FLAG(f->flags, EVAL_probe_wait);
         }
       /* [q->probe]; */
       if (!qprobe)
         { clear_value_tp(&pval, f);
           clear_value_tp(&qval, f);
           return EXEC_suspend;
         }
       v = receive_value(&qval, &x->e->tp, f);
       send_value(&v, &pval, f);
     }
   SET_FLAG(f->flags, EVAL_probe_zero);
   set_probe(&pval, f); set_probe(&qval, f); /* p,q->probe = 0 */
   RESET_FLAG(f->flags, EVAL_probe_zero);
   set_probe((f->curr->i == 2)? &qval : &pval, f); /* pp|qq->probe = 1 */
   clear_value_tp(&pval, f);
   clear_value_tp(&qval, f);
   f->curr->i = 0;
   return EXEC_next;
 }

static int exec_communication(communication *x, exec_info *f)
 { value_tp pval, dval;
   int probe;
   if (x->op_sym == '=')
     { return exec_comm_pass(x, f); }
   if (x->op_sym == SYM_peek) SET_FLAG(f->flags, EVAL_probe);
   eval_expr(x->p, f);
   RESET_FLAG(f->flags, EVAL_probe);
   pop_value(&pval, f);
   if (x->op_sym == SYM_peek) goto wait1;
   if (f->curr->i) goto wait1;
   SET_FLAG(f->flags, EVAL_probe_zero | EVAL_probe_wait);
   f->e = 0;
   probe = get_probe(&pval, f);
   RESET_FLAG(f->flags, EVAL_probe_zero | EVAL_probe_wait);
   /* wait0: [!pp->probe] */
   if (!probe)
     { clear_value_tp(&pval, f);
       return EXEC_suspend;
     }
   if (x->op_sym == '!')
     { eval_expr(x->e, f);
       pop_value(&dval, f);
       if (!dval.rep)
         { exec_error(f, x, "Sending an unknown value %v", vstr_obj, x->e); }
       range_check(x->p->tp.tps, &dval, f, x);
       send_value(&dval, &pval, f);
     }
   set_probe(&pval, f);
   f->curr->i = 1; /* to indicate which wait we're at */
 wait1:
   SET_FLAG(f->flags, EVAL_probe_wait);
   f->e = 0;
   probe = get_probe(&pval, f);
   RESET_FLAG(f->flags, EVAL_probe_wait);
   /* [p->probe] */
   if (!probe)
     { clear_value_tp(&pval, f);
       return EXEC_suspend;
     }
   if (x->op_sym == '?' || x->op_sym == SYM_peek)
     { if (x->op_sym == SYM_peek) SET_FLAG(f->flags, EVAL_probe);
       dval = receive_value(&pval, &x->p->tp, f);
       RESET_FLAG(f->flags, EVAL_probe);
       range_check(x->e->tp.tps, &dval, f, x->e);
       assign(x->e, &dval, f);
     }
   if (x->op_sym != SYM_peek)
     { SET_FLAG(f->flags, EVAL_probe_zero);
       set_probe(&pval, f);
       RESET_FLAG(f->flags, EVAL_probe_zero);
     }
   clear_value_tp(&pval, f);
   f->curr->i = 0;
   return EXEC_next;
 }

extern void meta_init(process_state *ps, exec_info *f)
/* Everything that should be executed as soon as all meta parameters are set */
 { ctrl_state *curr;
   process_state *meta_ps;
   curr = f->curr;
   meta_ps = f->meta_ps;
   f->meta_ps = ps;
   f->curr = ps->cs;
   exec_immediate(&ps->p->pl, f);
   f->curr = curr;
   f->meta_ps = meta_ps;
 }

extern void exec_meta_binding_aux(meta_binding *x, value_tp *v, exec_info *f)
 { llist ma, mp;
   expr *a;
   value_tp *eval, *meta;
   meta_parameter *p;
   process_state *ps, *meta_ps;
   long i;
   if (v->rep == REP_array)
     { for (i = 0; i < v->v.l->size; i++)
         { exec_meta_binding_aux(x, &v->v.l->vl[i], f); }
       return;
     }
   assert(v->rep == REP_process);
   ps = v->v.ps;
   meta = ps->meta;
   ma = x->a;
   mp = ps->p->ml;
   while (!llist_is_empty(&mp))
     { p = llist_head(&mp);
       a = llist_head(&ma);
       eval_expr(a, f);
       eval = &meta[p->meta_idx];
       pop_value(eval, f);
       if (!eval->rep)
         { exec_error(f, x, "Unknown value passed for meta parameter %s\n",
                        p->id);
         }
       /* To perform the range check, we need to use meta parameters from ps */
       meta_ps = f->meta_ps;
       f->meta_ps = ps;
       range_check(p->tp.tps, eval, f, a);
       f->meta_ps = meta_ps;
       mp = llist_alias_tail(&mp);
       ma = llist_alias_tail(&ma);
     }
   meta_init(ps, f);
 }

static int exec_meta_binding(meta_binding *x, exec_info *f)
 { value_tp xval;
   eval_expr(x->x, f);
   pop_value(&xval, f);
   exec_meta_binding_aux(x, &xval, f);
   clear_value_tp(&xval, f);
   return EXEC_next;
 }

static void connect_error(expr *x, process_state *ps, exec_info *f)
 { if (!ps) exec_error(f, x,"Port %v is already connected", vstr_obj, x);
   exec_error(f, x, "Port %v is already connected to %s", vstr_obj, x, ps->nm);
 }

static process_state *get_port_connect(expr *x, value_tp **v, exec_info *f)
 /* Set v to the value of x, which is initialized with a port value
    Return the process state which contains the port.
 */
 { value_tp *xv;
   process_state *ps;
   *v = xv = connect_expr(x, f);
   if (!xv->rep) /* not connected yet (empty), should be from a new process */
     { if (f->meta_ps == f->curr->ps)
         { assert(!"Empty port parameter slipped by check_new_ports"); }
       xv->rep = REP_port;
       xv->v.p = new_port_value(f->curr->ps, f);
       xv->v.p->ps = f->meta_ps;
     }
   else if (f->meta_ps != f->curr->ps)
     { connect_error(x, (xv->rep == REP_port)? xv->v.p->ps : 0, f); }
   ps = f->meta_ps;
   f->meta_ps = f->curr->ps;
   return ps;
 }

static void set_ps(value_tp *v, process_state *ps, exec_info *f)
 { int i;
   value_list *vl;
   switch (v->rep)
     { case REP_port:
         v->v.p->ps = ps;
       break;
       case REP_array: case REP_record:
         vl = v->v.l;
         for (i = 0; i < vl->size; i++)
           { set_ps(&vl->vl[i], ps, f); }
       break;
       case REP_union:
         set_ps(&v->v.u->v, ps, f);
       break;
       default:
       break;
     }
 }

static void connect_aux(value_tp *vala, value_tp *valb, exec_info *f)
/* Pre: we are connecting two ports of the current process */
 { int i, dir;
   value_tp *swap = 0, *nv;
   value_list *al, *bl;
   connection *x = (connection*)f->curr->obj;
   if (vala->rep != REP_port && valb->rep == REP_port)
     { swap = vala; vala = valb; valb = swap; }
   /* now vala!=port => valb!=port */
   switch (vala->rep)
     { case REP_port:
         if (!vala->v.p->p)
           { connect_error(swap? x->b : x->a, vala->v.p->ps, f); }
         /* TODO: what if valb was already connected? */
         if (vala->v.p->p->dec)
           { if (valb->rep != REP_port)
               { exec_error(f, x, "Connecting incompatible decompositions"); }
             if (!valb->v.p->p)
               { connect_error(x->b, valb->v.p->ps, f); }
             if (valb->v.p->p->dec)
               { if (valb->v.p->p->dec != vala->v.p->p->dec)
                   { exec_error(f, x, "Connecting incompatible "
                                      "decompositions");
                   }
                 dir = IS_SET(x->a->flags, EXPR_inport);
                 wu_proc_remove(vala, dir, f);
                 wu_proc_remove(valb, !dir, f);
                 connect_aux(&vala->v.p->v, &valb->v.p->v, f);
                 return;
               }
           }
         set_ps(valb, vala->v.p->p->ps, f);
         vala->v.p->ps = (valb->rep == REP_port)? valb->v.p->p->ps : 0;
         nv = find_reference(vala->v.p->p, f);
         *nv = *valb;
         valb->rep = REP_port;
         valb->v.p = vala->v.p->p;
         vala->v.p->p = 0; vala->v.p->wprobe.refcnt--;
         valb->v.p->p = 0; valb->v.p->wprobe.refcnt--;
         vala->v.p->nv = valb->v.p->nv = nv;
       return;
       case REP_array: case REP_record:
         if (vala->rep != valb->rep)
           { exec_error(f, x, "Connecting incompatible decompositions"); }
         al = vala->v.l; bl = valb->v.l;
         if (al->size != bl->size)
           { exec_error(f, x, "Connecting incompatible decompositions"); }
         for (i = 0; i < al->size; i++)
           { connect_aux(&al->vl[i], &bl->vl[i], f); }
       return;
       case REP_union:
         if (valb->rep != REP_union ||
             vala->v.u->f != valb->v.u->f)
           { exec_error(f, x, "Connecting incompatible decompositions"); }
         connect_aux(&vala->v.u->v, &valb->v.u->v, f);
       return;
       case REP_wire: // Can happen when bridging decomposition processes
         assert(valb->rep == REP_wire);
         wire_fix(&vala->v.w, f);
         wire_fix(&valb->v.w, f);
         SET_FLAG(valb->v.w->flags, WIRE_forward);
         valb->v.w->u.w = vala->v.w;
         if (vala->v.w->wframe->cs->ps == f->meta_ps)
           { vala->v.w->wframe = valb->v.w->wframe; }
         wire_fix(&valb->v.w, f); // For good measure
       return;
       default:
       return;
     }
 }

static int exec_connection(connection *x, exec_info *f)
 { expr *a, *b;
   process_state *psa, *psb;
   value_tp *vala, *valb, tmp;
   if (IS_SET(f->flags, EXEC_print))
     { print_connection(x, f->pf);
       print_string(";\n", f->pf);
     }
   psa = get_port_connect(x->a, &vala, f);
   if (psa == f->curr->ps)
     { psb = get_port_connect(x->b, &valb, f);
       a = x->a; b = x->b;
     }
   else
     { valb = vala; psb = psa;
       psa = get_port_connect(x->b, &vala, f);
       a = x->b; b = x->a;
     }
   if (!type_compatible_exec(&a->tp, psa, &b->tp, psb, f))
     { exec_error(f, x, "Connected ports must have identical specific types"); }
   /* psa == f->curr->ps implies that a is a new, unconnected port,
      and that vala->rep == REP_port.  Otherwise, a must be connected,
      and it may be an array, record, or union type as well.  The same
      is true for b.  Also (psb == f->curr->ps) => (psa == f->curr->ps).
    */
   if (psa != f->curr->ps) /* connection between two new ports */
     { vala->v.p->p = valb->v.p;
       valb->v.p->wprobe.refcnt++;
       valb->v.p->p = vala->v.p;
       vala->v.p->wprobe.refcnt++;
     }
   else if (psb != f->curr->ps) /* pass from local vala to new valb */
     { set_ps(vala, valb->v.p->ps, f);
       valb->v.p->nv = valb;
       tmp = *vala; *vala = *valb; *valb = tmp;
     }
   else /* connecting two local ports */
     { connect_aux(vala, valb, f); }
   return EXEC_next;
 }

static int get_write(expr *x, exec_info *f)
 /* Pre: x is a subelement of a wired type expression, a wired type expression,
  *      or an array of wired type expression.
  * For subelements, return whether the subelement is writable.  For full wired
  * types or arrays thereof, return whether the "li" list in the wired type
  * is writable.
  */
 { type *tp = &x->tp;
   parse_obj_flags fl;
   while (tp->kind == TP_array) tp = tp->elem.tp;
   if (tp->kind != TP_wire)
     { return IS_SET(x->flags, EXPR_writable); }
   else if (x->class == CLASS_field_of_union)
     /* Here we must deduce the decomposed wired port's "direction" */
     { fl = ((field_of_union*)x)->x->flags;
       if (!IS_SET(fl, EXPR_port_ext))
         { fl = fl ^ EXPR_port; }
       return LI_IS_WRITE(fl, ((wired_type*)tp->tps)->type);
     }
   else
     { return LI_IS_WRITE(x->flags, ((wired_type*)tp->tps)->type); }
 }

static process_state *get_wire_connect(expr *x, value_tp **v, exec_info *f)
 /* Set v to the value of x, which is initialized with a wire value
  * Return the process state which contains the port.
 */
 { process_state *ps;
   value_tp *xv;
   *v = xv = connect_expr(x, f);
   if (!xv->rep) /* not connected yet (empty), should be from a new process */
     { force_value(xv, x, f); }
   ps = f->meta_ps;
   f->meta_ps = f->curr->ps;
   return ps;
 }

struct wc_info
   { token_tp init_sym;
     int a_write, b_write;
     process_state *psa, *psb;
   };

static void wire_connect
(value_tp *va, value_tp *vb, type *tp, struct wc_info *g, exec_info *f)
/* Pre: run type_compatible_exec on a and b
 * TODO: A LOT of this code has been made redundant by the wire_init routine...
 */
 { int i, a_flag, b_flag;
   wire_value *wa, *wb;
   action *wra, *wrb; /* write frames */
   wired_type *wtps;
   llist m;
   wire_decl *w;
   token_tp tok;
   if (tp->kind == TP_array)
     { if (va->rep == REP_none) force_value_array(va, tp, f);
       if (vb->rep == REP_none) force_value_array(vb, tp, f);
       for (i = 0; i < va->v.l->size; i++)
         { wire_connect(&va->v.l->vl[i], &vb->v.l->vl[i], tp->elem.tp, g, f); }
     }
   else if (tp->kind == TP_record)
     { if (va->rep == REP_none) force_value(va, EXPR_FROM_TYPE(tp), f);
       if (vb->rep == REP_none) force_value(vb, EXPR_FROM_TYPE(tp), f);
       m = tp->elem.l;
       for (i = 0; i < va->v.l->size; i++)
         { wire_connect(&va->v.l->vl[i], &vb->v.l->vl[i], llist_head(&m), g, f);
           m = llist_alias_tail(&m);
         }
     }
   else if (tp->kind == TP_wire)
     { if (va->rep == REP_none) force_value(va, EXPR_FROM_TYPE(tp), f);
       if (vb->rep == REP_none) force_value(vb, EXPR_FROM_TYPE(tp), f);
       wtps = (wired_type*)tp->tps;
       m = wtps->li;
       for (i = 0; i < va->v.l->size; i++)
         { w = llist_head(&m);
           g->init_sym = w->init_sym;
           wire_connect(&va->v.l->vl[i], &vb->v.l->vl[i], &w->tps->tp, g, f);
           m = llist_alias_tail(&m);
           if (llist_is_empty(&m))
             { m = wtps->lo;
               g->a_write = !g->a_write;
               g->b_write = !g->b_write;
             }
         }
     }
   else /* TP_bool */
     { if (va->rep) { wire_fix(&va->v.w, f); wa = va->v.w; }
       if (vb->rep == REP_wire) { wire_fix(&vb->v.w, f); wb = vb->v.w; }
       if (va->rep && vb->rep && wa == wb) return; /* already connected, ignore */
       if (va->rep) wra = IS_SET(wa->flags, WIRE_has_writer)? wa->wframe : 0;
       if (!va->rep || !wra) wra = g->a_write? &g->psa->cs->act : 0;
       if (vb->rep == REP_wire)
         { wrb = IS_SET(wb->flags, WIRE_has_writer)? wb->wframe : 0; }
       if (vb->rep != REP_wire || !wrb) wrb = g->b_write? &g->psb->cs->act : 0;
       if (wra && wrb && wra->cs->ps!=f->curr->ps && wrb->cs->ps!=f->curr->ps)
         /* Two writers on the same wire   TODO: better error message */
         { if (g->b_write)
             { connect_error(((connection*)f->curr->obj)->a, 0, f); }
           if (g->a_write)
             { connect_error(((connection*)f->curr->obj)->b, 0, f); }
           exec_error(f, f->curr->obj, "Connection bridged two output wires");
         }
       /* Now actually do the connection */
       if (vb->rep == REP_bool) /* No connection, just set initial value */
         { if (!va->rep)
             { va->v.w = wa = new_wire_value(g->init_sym, f); }
           if (!IS_SET(wa->flags ^ (vb->v.i? 0 : WIRE_value), WIRE_val_mask))
             { exec_error(f, f->curr->obj, "Connecting initially %s wire "
                          "to rail '%s'", vb->v.i? "false" : "true",
                          vb->v.i? "true" : "false");
             }
           ASSIGN_FLAG(wa->flags, vb->v.i? WIRE_value : 0, WIRE_val_mask);
         }
       else if (!va->rep)
         { if (!vb->rep)
             { va->v.w = vb->v.w = wa = new_wire_value(g->init_sym, f); }
           else
             { va->v.w = wa = wb; }
         }
       else if (!vb->rep)
         { vb->v.w = wa; }
       else /* va and vb are both prexisting wires */
         { if (IS_SET(wa->flags, WIRE_undef))
             { ASSIGN_FLAG(wa->flags, wb->flags, WIRE_val_mask); }
           else if (((wa->flags ^ wb->flags) & WIRE_val_mask) == WIRE_value)
             { exec_error(f, f->curr->obj, "Incompatible initial values"
                          "in connect");
             }
           SET_FLAG(wb->flags, WIRE_forward);
           wb->u.w = wa;
           wb->refcnt--;
           if (!wb->refcnt) free(wb);
           else wa->refcnt++;
           vb->v.w = wa;
         }
       /* now va->w == vb->w = wa */
       wa->refcnt++;
       va->rep = REP_wire;
       if (!vb->rep) vb->rep = REP_wire;
       if (wra || wrb)
         { SET_FLAG(wa->flags, WIRE_has_writer);
           wa->wframe = (!wra || (wrb && wrb->cs->ps != f->curr->ps))? wrb:wra;
           /* make wframe one of wra or wrb, giving preference to new frames */
         }
     }
 }

static int exec_wired_connection(wired_connection *x, exec_info *f)
 { struct wc_info g;
   if (IS_SET(f->flags, EXEC_print))
     { print_connection((wired_connection*)x, f->pf);
       print_string(";\n", f->pf);
     }
   value_tp *vala, *valb;
   g.psa = get_wire_connect(x->a, &vala, f);
   g.psb = get_wire_connect(x->b, &valb, f);
   g.a_write = get_write(x->a, f);
   g.b_write = get_write(x->b, f);
   if (!type_compatible_exec(&x->a->tp, g.psa, &x->b->tp, g.psb, f))
     { exec_error(f, x, "Connected ports must have identical specific types"); }
   /* g.psa == f->curr->ps implies that a is a new port (same for b) */
   wire_connect(vala, valb, &x->a->tp, &g, f);
   return EXEC_next;
 }


static int exec_const_wired_connection(const_wired_connection *x, exec_info *f)
 { struct wc_info g;
   if (IS_SET(f->flags, EXEC_print))
     { print_connection((wired_connection*)x, f->pf);
       print_string(";\n", f->pf);
     }
   value_tp *vala, valb;
   g.psa = get_wire_connect(x->a, &vala, f);
   eval_expr(x->b, f);
   pop_value(&valb, f);
   g.psb = &const_frame_ps;
   g.a_write = get_write(x->a, f);
   g.b_write = 1;
   if (!type_compatible_exec(&x->a->tp, g.psa, &x->b->tp, f->meta_ps, f))
     { exec_error(f, x, "Connected ports must have identical specific types"); }
   /* g.psa == f->curr->ps implies that a is a new port */
   wire_connect(vala, &valb, &x->a->tp, &g, f);
   return EXEC_next;
 }


static void mk_instance(value_tp *xval, type *tp, exec_info *f)
 /* Pre: tp is the type or subtype of an instance_decl.
    Create the process states.
 */
 { process_state *ps;
   ctrl_state *cs;
   value_tp *v, lval;
   int i, pos, j;
   array_type *atps;
   pos = VAR_STR_LEN(&f->scratch);
   if (tp->kind == TP_array)
     { force_value_array(xval, tp, f);
       atps = (array_type*)tp->tps;
       eval_expr(atps->l, f);
       pop_value(&lval, f);
       for (i = 0; i < xval->v.l->size; i++, int_inc(&lval, f))
         { var_str_printf(&f->scratch, pos, "[%v]", vstr_val, &lval);
           mk_instance(&xval->v.l->vl[i], tp->elem.tp, f);
         }
       clear_value_tp(&lval, f);
     }
   else
     { assert(tp->kind == TP_process); 
       ps = new_process_state(f, make_str(f->scratch.s));
       ps->p = tp->elem.p;
       ps->nr_meta = ps->p->nr_meta;
       if (ps->nr_meta)
         { NEW_ARRAY(ps->meta, ps->nr_meta); }
       v = ps->meta;
       for (i = 0; i < ps->nr_meta; i++)
         { v[i].rep = REP_none; }
       cs = ps->cs;
       cs->nr_var = ps->p->nr_var;
       if (cs->nr_var)
         { NEW_ARRAY(cs->var, cs->nr_var); }
       v = cs->var;
       for (i = 0; i < cs->nr_var; i++)
         { v[i].rep = REP_none; }
       cs->obj = (parse_obj*)ps->p;
       ps->var = cs->var;
       ps->nr_var = cs->nr_var;
       for (i = 0; i < ps->p->meta_offset; i++)
         { alias_value_tp(&ps->meta[i], &f->meta_ps->meta[i], f); }
       ps->nr_thread = -1; /* instantiated, not started */
       xval->rep = REP_process;
       xval->v.ps = ps;
       if (llist_is_empty(&ps->p->ml)) meta_init(ps, f);
     }
   f->scratch.s[pos] = 0;
 }

static int exec_instance_stmt(instance_stmt *x, exec_info *f)
 { value_tp *v;
   if (IS_SET(f->flags, EXEC_print))
     { print_instance_stmt(x, f->pf);
       print_string(";\n", f->pf);
     }
   v = &f->curr->var[x->d->var_idx];
   if (f->curr->ps->nm == make_str("/"))
     { var_str_printf(&f->scratch, 0, "/%s", x->d->id); }
   else
     { var_str_printf(&f->scratch, 0, "%s/%s", f->curr->ps->nm, x->d->id); }
   mk_instance(v, &x->d->tp, f);
   if (x->mb)
     { exec_meta_binding_aux(x->mb, v, f); }
   return EXEC_next;
 }

/********** exec_production_rule *********************************************/

static void _lookup_pr
(value_tp *v, wire_expr **pu, wire_expr **pd, wire_value *w, exec_info *f)
/* find all production rules with w as target */
 { int i;
   wire_expr *e;
   llist l;
   switch (v->rep)
     { case REP_array: case REP_record:
         for (i = 0; i < v->v.l->size; i++)
           { _lookup_pr(&v->v.l->vl[i], pu, pd, w, f); }
       return;
       case REP_wire:
         if (f->curr->obj->class == CLASS_delay_hold) return;
         if (!IS_SET(v->v.w->flags, WIRE_has_dep)) return;
         l = v->v.w->u.dep;
       break;
       case REP_cnt:
         if (f->curr->obj->class != CLASS_delay_hold) return;
         l = v->v.c->dep;
       break;
       default:
       return;
     }
   while (!llist_is_empty(&l))
     { e = llist_head(&l);
       l = llist_alias_tail(&l);
       while (!IS_SET(e->flags, WIRE_action))
         { e = e->u.dep; }
       if (e->u.hold != w && e->u.act->target.w != w) continue;
       if (IS_SET(e->flags, WIRE_pu))
         { assert(!*pu || *pu == e); *pu = e; }
       else if (IS_SET(e->flags, WIRE_pd))
         { assert(!*pd || *pd == e); *pd = e; }
     }
 }

static void lookup_pr
(wire_expr **pu, wire_expr **pd, wire_value *w, exec_info *f)
 /* Note: only used by delay_hold now
  *
  * find the pull up and pull down production rule wire_expr's for w
  * (there should be at most one of each type)
  * Note that a PR with identically true guard does not have a wire_expr
  * If f->curr->obj is a delay_hold, find hold up/down wire_expr's instead
  */
 { int i;
   *pu = 0; *pd = 0;
   for (i = 0; i < f->curr->nr_var; i++)
     { _lookup_pr(&f->curr->var[i], pu, pd, w, f); }
 }

static int exec_production_rule(production_rule *x, exec_info *f)
 /* This does not execute the production rule so much as simply instantiate it,
  * since production rules are handled seperately from other statements in the
  * execution engine.
  */
 { wire_expr *e, *p, *pp, *pu, *pd;
   hash_entry *q;
   action *a;
   value_tp val, dval;
   long i, n;
   int ispu = (x->op_sym == '+');
   if (IS_SET(f->flags, EXEC_print))
     { print_production_rule(x, f->pf);
       print_char('\n', f->pf);
       return EXEC_next;
     }
   eval_expr(x->v, f);
   pop_value(&val, f);
   e = make_wire_expr(x->g, f);
   if (val.rep == REP_wire)
     { a = val.v.w->wframe;
       if (!IS_SET(a->flags, ACTION_is_pr))
         { a = new_action(f);
           a->target.w = val.v.w;
           val.v.w->wframe = a;
         }
       else if (IS_SET(a->flags, ispu? ACTION_has_up_pr: ACTION_has_dn_pr))
         { exec_error(f, x, "Target %V already has a pull%s production rule",
                      vstr_wire, val.v.w, f->meta_ps, ispu? "up" : "down");
         }
       SET_FLAG(a->flags, ispu? ACTION_has_up_pr : ACTION_has_dn_pr);
     }
   else
     { assert(val.rep == REP_cnt);
       a = new_action(f);
       a->flags = ACTION_is_cr;
       a->target.c = val.v.c;
     }
   e->u.act = a;
   if (x->atomic) SET_FLAG(a->flags, ACTION_atomic);
   if (x->delay)
     { eval_expr(x->delay, f);
       pop_value(&dval, f);
       if (dval.rep == REP_z)
         { exec_error(f, x, "Delay value %v is too large", vstr_val, &dval); }
       if (dval.v.i < 0)
         { exec_error(f, x, "Delay value %d is negative", dval.v.i); }
       SET_FLAG(a->flags, ACTION_delay);
       hash_insert(&f->delays, &((char*)a)[ispu? 0 : 1], &q);
       q->data.i = dval.v.i;
     }
   SET_FLAG(e->flags, ispu? WIRE_pu : WIRE_pd);
   if ((e->flags & WIRE_val_mask) == WIRE_value)
     { SET_FLAG(a->flags, ispu? ACTION_pr_up | ACTION_up_nxt :
                                ACTION_pr_dn | ACTION_dn_nxt);
       if (IS_ALLSET(a->flags, ACTION_pr_up | ACTION_pr_dn))
         { exec_error(f, x, "Production rule creates initial interference"); }
       if (val.rep == REP_wire &&
           ((val.v.w->flags & WIRE_value) == (ispu? 0 : WIRE_value)))
         { action_sched(a, f); }
       else
         { SET_FLAG(val.v.w->flags, WIRE_reset); }
     }
   return EXEC_next;
 }

static int exec_delay_hold(delay_hold *x, exec_info *f)
 /* We create a structure out of wire expressions to create the correct
  * delay hold behavior.  TODO: explain this more
  */
 { wire_expr *e, *h, *hu, *hd;
   counter_value *c;
   int n, ishu;
   llist m;
   transition *t;
   value_tp v;
   eval_expr(x->c, f);
   pop_value(&v, f);
   c = v.v.c;
   n = c->cnt;
   if (x->n)
     { eval_expr(x->n, f);
       pop_value(&v, f);
       if (v.rep != REP_int || v.v.i > MAX_COUNT)
         { exec_error(f, x, "Delay hold threshold is too large"); }
       else if (v.v.i < 0)
         { exec_error(f, x, "Delay hold threshold is negative"); }
       n -= v.v.i;
     }
   m = x->l;
   while (!llist_is_empty(&m))
     { t = llist_head(&m);
       ishu = (t->op_sym == '+');
       eval_expr(t->v, f);
       pop_value(&v, f);
       lookup_pr(&hu, &hd, v.v.w, f);
       h = ishu? hu : hd;
       if (!h)
         { h = new_wire_expr(f);
           h->flags = WIRE_value | WIRE_val_dir | (ishu? WIRE_hu : WIRE_hd);
           h->u.hold = v.v.w;
         }
       e = new_wire_expr(f);
       e->valcnt = n;
       if (n > 0) e->flags = WIRE_value;
       we_add_dep(h, e, f);
       if (!IS_SET(h->flags, WIRE_value))
         { SET_FLAG(v.v.w->flags, ishu? WIRE_held_up : WIRE_held_dn); }
       e->refcnt++;
       llist_prepend(&c->dep, e);
       m = llist_alias_tail(&m);
     }
 }

/********** breakpoints ******************************************************/

static brk_return brk_parallel_stmt(parallel_stmt *x, user_info *f)
 { return brk_list(&x->l, f); }

static brk_return brk_compound_stmt(compound_stmt *x, user_info *f)
 { brk_return res;
   if (x->lnr == f->brk_lnr && x->lpos >= f->brk_lpos)
     { return set_breakpoint(x, f); }
   if ((res = brk_list(&x->l, f)) != BRK_no_stmt) return res;
   if (x->lnr == f->brk_lnr)
     { return set_breakpoint(x, f); }
   return BRK_no_stmt;
 }

static brk_return brk_rep_stmt(rep_stmt *x, user_info *f)
 { brk_return res;
   parse_obj *head;
   sem_context *cxt = f->cxt;
   head = llist_is_empty(&x->sl)? 0 : llist_head(&x->sl);
   if (head && head->lnr <= f->brk_lnr &&
       (head->lnr < f->brk_lnr ||
        head->lpos <= f->brk_lpos || x->lnr < f->brk_lnr))
     { f->cxt = x->cxt;
       res = brk_list(&x->sl, f);
       f->cxt = cxt;
       return res;
     }
   if (x->lnr == f->brk_lnr)
     { return set_breakpoint(x, f); }
   return BRK_no_stmt;
 }

static brk_return brk_guarded_cmnd(guarded_cmnd *x, user_info *f)
 { brk_return res;
   if ((res = brk_list(&x->l, f)) != BRK_no_stmt) return res;
   if (x->lnr == f->brk_lnr)
     { return set_breakpoint(llist_head(&x->l), f); }
   return BRK_no_stmt;
 }

static brk_return brk_loop_stmt(loop_stmt *x, user_info *f)
 { brk_return res;
   if (x->lnr == f->brk_lnr && x->lpos + 1 >= f->brk_lpos)
     { return set_breakpoint(x, f); }
   if (!llist_is_empty(&x->gl))
     { if ((res = brk_list(&x->gl, f)) != BRK_no_stmt) return res; }
   else
     { if ((res = brk_list(&x->sl, f)) != BRK_no_stmt) return res; }
   if (x->lnr == f->brk_lnr)
     { return set_breakpoint(x, f); }
   return BRK_no_stmt;
 }

static brk_return brk_select_stmt(select_stmt *x, user_info *f)
 { brk_return res;
   if (x->lnr == f->brk_lnr && x->lpos >= f->brk_lpos)
     { return set_breakpoint(x, f); }
   if (!llist_is_empty(&x->gl))
     { if ((res = brk_list(&x->gl, f)) != BRK_no_stmt) return res; }
   else if  (x->w->lnr == f->brk_lnr)
     { return set_breakpoint(x, f); }
   if  (x->lnr == f->brk_lnr)
     { return set_breakpoint(x, f); }
   return BRK_no_stmt;
 }


/*****************************************************************************/

/*extern*/ process_state const_frame_ps;
/*extern*/ ctrl_state const_frame;

extern void init_statement(void)
 /* call at startup */
 { set_print(skip_stmt);
   set_print(end_stmt);
   set_print(parallel_stmt);
   set_print(compound_stmt);
   set_print(rep_stmt);
   set_print(assignment);
   set_print(bool_set_stmt);
   set_print(guarded_cmnd);
   set_print(loop_stmt);
   set_print(select_stmt);
   set_print(communication);
   set_print(meta_binding);
   set_print(connection);
   set_print_cp(wired_connection, connection);
   set_print_cp(const_wired_connection, connection);
   set_print(instance_stmt);
   set_print(production_rule);
   set_print(transition);
   set_print(delay_hold);
   set_sem(parallel_stmt);
   set_sem(compound_stmt);
   set_sem(rep_stmt);
   set_sem(assignment);
   set_sem(bool_set_stmt);
   set_sem(guarded_cmnd);
   set_sem(loop_stmt);
   set_sem(select_stmt);
   set_sem(communication);
   set_sem(meta_binding);
   set_sem(connection);
   set_sem_cp(wired_connection, connection);
   set_sem_cp(const_wired_connection, connection);
   set_sem(instance_stmt);
   set_sem(production_rule);
   set_sem(transition);
   set_sem(delay_hold);
   set_exec(parallel_stmt);
   set_pop(parallel_stmt);
   set_exec(compound_stmt);
   set_exec(rep_stmt);
   set_pop(rep_stmt);
   set_exec(assignment);
   set_exec(bool_set_stmt);
   set_exec(loop_stmt);
   /* pop_loop_stmt = exec_loop_stmt */
   set_app(CLASS_loop_stmt, app_pop, (obj_func*)exec_loop_stmt);
   set_exec(select_stmt);   
   set_exec(communication);
   set_exec(meta_binding);
   set_exec(connection);
   set_exec(wired_connection);
   set_exec(const_wired_connection);
   set_exec(instance_stmt);
   set_exec(production_rule);
   set_exec(delay_hold);
   set_brk(parallel_stmt);
   set_brk(compound_stmt);
   set_brk(rep_stmt);
   set_brk(guarded_cmnd);
   set_brk(loop_stmt);
   set_brk(select_stmt);

   /* Set up the const frame used by const_wired_connection */
   const_frame_ps.nm = make_str("//const");
   const_frame_ps.cs = &const_frame;
   const_frame.act.cs = &const_frame;
   const_frame.act.flags = 0;
   const_frame.ps = &const_frame_ps;
 }

