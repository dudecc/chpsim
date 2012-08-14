/* variables.c: variables, parameters, constants.
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
#include "variables.h"
#include "sem_analysis.h"
#include "types.h"
#include "expr.h"
#include "exec.h"

/********** printing *********************************************************/

static void *print_inline_array(void *tps, print_info *f)
 /* If tps is an array, print as an inline array. Return a non-array type
  * that tps is a (possibly multi-dimensional) array of.
  */
 { array_type *atps = tps;
   token_expr *l, *h;
   if (atps->class == CLASS_array_type)
     { print_char('[', f);
       while (1)
         { print_obj(atps->l, f);
           print_string("..", f);
           print_obj(atps->h, f);
           atps = (array_type*)atps->tps;
           if (atps->class != CLASS_array_type) break;
           print_char(',', f);
         }
       print_char(']', f);
     }
   return atps;
 }

static void *skip_inline_array(void *tps, print_info *f)
 /* Return the same value as print_inline_array, but do not print anything */
 { array_type *atps = tps;
   while (atps->class == CLASS_array_type)
     { atps = (array_type*)atps->tps; }
   return atps;
 }

static void print_const_def(const_def *x, print_info *f)
 { f->pos += var_str_printf(f->s, f->pos, "const %s", x->id);
   if (x->tps)
     { print_string(": ", f);
       print_obj(x->tps, f);
     }
   print_string(" = ", f);
   print_obj(x->z, f);
   print_char(';', f);
 }

static void print_var_decl(var_decl *x, print_info *f)
 { wired_type *wtps;
   if (IS_SET(x->flags, EXPR_port) == EXPR_port)
     { print_string(x->id, f);
       print_inline_array(x->tps, f);
     }
   else if (IS_SET(x->flags, EXPR_port))
     { print_string(x->id, f);
       wtps = print_inline_array(x->tps, f);
       print_char(IS_SET(x->flags, EXPR_inport)? '?' : '!', f);
       print_string(": ", f);
       print_obj(wtps, f);
     }
   else if (IS_SET(x->flags, VAR_def_wire))
     { print_string(x->id, f);
       print_inline_array(x->tps, f);
       if (x->z_sym)
         { print_char(x->z_sym, f); }
       else if (x->z)
         { print_string(" = ", f);
           print_obj(x->z, f);
         }
     }
   else if (IS_ALLSET(x->flags, EXPR_wire | EXPR_writable))
     { f->pos += var_str_printf(f->s, f->pos, "var %s", x->id);
       print_inline_array(x->tps, f);
       if (x->z_sym)
         { print_char(x->z_sym, f); }
       else if (x->z)
         { print_string(" = ", f);
           print_obj(x->z, f);
         }
       print_char(';', f);
     }
   else if (IS_SET(x->flags, EXPR_wire))
     { print_string(x->id, f);
       print_obj(x->tps, f);
     }
   else
     { f->pos += var_str_printf(f->s, f->pos, "var %s: ", x->id);
       print_obj(x->tps, f);
       if (x->z_sym)
         { print_char(x->z_sym, f); }
       else if (x->z)
         { print_string(" = ", f);
           print_obj(x->z, f);
         }
       print_char(';', f);
     }
 }

static void print_parameter(parameter *x, print_info *f)
 { f->pos += var_str_printf(f->s, f->pos, "%s %s: ",
                            token_str(0, x->par_sym), x->d->id);
   print_obj(x->d->tps, f);
 }

static void print_wire_decl(wire_decl *x, print_info *f)
 { print_string(x->id, f);
   print_inline_array(x->tps, f);
   if (x->init_sym)
     { print_char(x->init_sym, f); }
   else if (x->z)
     { print_string(" = ", f);
       print_obj(x->z, f);
     }
 }

static void print_meta_parameter(meta_parameter *x, print_info *f)
 { f->pos += var_str_printf(f->s, f->pos, "%s: ", x->id);
   print_obj(x->tps, f);
 }

static void print_property_decl(property_decl *x, print_info *f)
 { f->pos += var_str_printf(f->s, f->pos, "property %s", x->id);
   if (x->z)
     { print_string(" = ", f);
       print_obj(x->z, f);
     }
   print_char(';', f);
 }


/*****************************************************************************/

static void *sem_const_def(const_def *x, sem_info *f)
 { declare_id(f, x->id, x);
   if (IS_SET(x->flags, DEF_forward))
     { if (x->tps)
         { x->tps = sem(x->tps, f); }
       return x;
     }
   SET_FLAG(x->flags, DEF_forward);
   x->z = sem(x->z, f);
   if (IS_SET(x->z->flags, EXPR_unconst))
     { sem_error(f, x->z, "Initializer is not constant."); }
   x->z = mk_const_expr(x->z, f);
   if (x->tps)
     { x->tps = sem(x->tps, f);
       x->tp = x->tps->tp;
       if (!type_compatible(&x->tp, &x->z->tp))
         { sem_error(f, x->z, "Value of %s has wrong type", x->id); }
     }
   else
     { x->tp = x->z->tp; }
   return x;
 }

static void import_const_def(const_def *x, sem_info *f)
 { assert(IS_SET(x->flags, DEF_forward));
   declare_id(f, x->id, x);
 }

static void sem_z_check(expr **z, type *tp, sem_info *f)
 /* If *z or some array of *z's could match tp, then replace
  * *z with implicit_arrays as necessary to match the types.
  * Otherwise throw an error
  */
 { expr *zz = *z;
   type *tmp;
   int zn = 0, tn = 0;
   tmp = tp;
   while (tmp->kind == TP_array)
     { tn++; tmp = tmp->elem.tp; }
   tmp = &zz->tp;
   while (tmp->kind == TP_array)
     { zn++; tmp = tmp->elem.tp; }
   while (tn > zn)
     { *z = (expr*)new_parse(f->L, 0, zz, implicit_array);
       (*z)->tp = *tp;
       z = &((implicit_array*)*z)->x;
       tp = tp->elem.tp;
       tn--;
     }
   *z = zz;
   if (!type_compatible(tp, &zz->tp))
     { sem_error(f, zz, "Initial value has wrong type"); }
 }

static void *sem_var_decl(var_decl *x, sem_info *f)
 { wired_type *wtps;
   llist m;
   if (!IS_SET(x->flags, DEF_forward))
     { x->var_idx = f->var_idx++; }
   declare_id(f, x->id, x);
   x->tps = sem(x->tps, f);
   if (IS_SET(x->flags, DEF_forward)) return x;
   SET_FLAG(x->flags, DEF_forward);
   x->tp = x->tps->tp;
   SET_IF_SET(x->flags, x->tp.tps->flags, EXPR_generic);
   if (IS_SET(f->flags, SEM_delay))
     { SET_FLAG(x->flags, EXPR_counter | EXPR_writable); }
   else if (IS_SET(f->flags, SEM_prs))
     { SET_FLAG(x->flags, EXPR_wire | EXPR_writable); }
   if (IS_SET(x->tp.tps->flags, EXPR_wire))
     { if (!IS_SET(x->flags, EXPR_port))
         { if (!IS_SET(x->flags, EXPR_wire))
             { sem_error(f, x, "Wired type is only for ports."); }
           wtps = (wired_type*)x->tp.tps;
           if (!wtps->type)
             { m = wtps->li; wtps->li = wtps->lo; wtps->lo = m; }
         }
       else
         { SET_FLAG(x->flags, EXPR_wire); }
     }
   if (x->z)
     { x->z = sem(x->z, f);
       if (IS_SET(x->z->flags, EXPR_unconst))
         { sem_error(f, x->z, "Initializer is not constant."); }
       x->z = mk_const_expr(x->z, f);
       sem_z_check(&x->z, &x->tp, f);
     }
   return x;
 }

static void *sem_parameter(parameter *x, sem_info *f)
 { if (!IS_SET(x->flags, DEF_forward))
     { x->d->var_idx = f->var_idx++; }
   declare_id(f, x->d->id, x);
   x->d->tps = sem(x->d->tps, f);
   if (IS_SET(x->flags, DEF_forward)) return x;
   SET_FLAG(x->flags, DEF_forward);
   x->d->tp = x->d->tps->tp;
   return x;
 }

static void *sem_wire_decl(wire_decl *x, sem_info *f)
 { x->tps = sem(x->tps, f);
   if (IS_SET(x->flags, DEF_forward) && x->z)
     { x->z = sem(x->z, f);  
       if (IS_SET(x->z->flags, EXPR_unconst))
         { sem_error(f, x->z, "Initializer is not constant."); }
       x->z = mk_const_expr(x->z, f);
       if (!type_compatible(&x->tps->tp, &x->z->tp))
         { sem_error(f, x->z, "Initial value of %s has wrong type", x->id); }
     }
   SET_FLAG(x->flags, DEF_forward);
   return x;
 }

static void *sem_meta_parameter(meta_parameter *x, sem_info *f)
 { if (!IS_SET(x->flags, DEF_forward))
     { x->meta_idx = f->meta_idx++; }
   declare_id(f, x->id, x);
   x->tps = sem(x->tps, f);
   if (IS_SET(x->flags, DEF_forward)) return x;
   SET_FLAG(x->flags, DEF_forward);
   x->tp = x->tps->tp;
   if (x->tp.kind == TP_type) SET_FLAG(x->flags, EXPR_generic);
   SET_IF_SET(x->flags, x->tp.tps->flags, EXPR_generic);
   return x;
 }

static void *sem_property_decl(property_decl *x, sem_info *f)
 { declare_id(f, x->id, x);
   if (IS_SET(x->flags, DEF_forward)) return x;
   SET_FLAG(x->flags, DEF_forward);
   if (x->z)
     { x->z = sem(x->z, f);
       if (IS_SET(x->z->flags, EXPR_unconst))
         { sem_error(f, x->z, "Initializer is not constant."); }
       if (x->z->tp.kind != TP_int)
         { sem_error(f, x->z, "Initializer is not an integer."); }
       x->z = mk_const_expr(x->z, f);
     }
   return x;
 }


/********** execution ********************************************************/

/* Exec of a declaration evaluates any expressions used in the type spec,
   and sets the initial value.
   This is done atomically, without delay.
*/

static void wire_var_fix(value_tp *v, int write, exec_info *f)
 /* Pre: v is filled with boolean values to be replaced by wires */
 { int i;
   switch (v->rep)
     { case REP_array:
         for (i = 0; i < v->v.l->size; i++)
           { wire_var_fix(&v->v.l->vl[i], write, f); }
       return;
       case REP_bool:
         v->v.w = new_wire_value(v->v.i? '+' : '-', f);
         v->rep = write? REP_wwire : REP_rwire;
       return;
       default:
         assert(!"Illegal initial value");
       return;
     }
 }

static void counter_var_fix(value_tp *v, exec_info *f)
 /* Pre: v is filled with integer values to be replaced by counters */
 { int i;
   void *x = f->err_obj;
   switch (v->rep)
     { case REP_array:
         for (i = 0; i < v->v.l->size; i++)
           { counter_var_fix(&v->v.l->vl[i], f); }
       return;
       case REP_z:
         exec_error(f, x, "Counter value %v is too large", vstr_val, v);
       case REP_int:
         if (v->v.i > MAX_COUNT)
           { exec_error(f, x, "Counter value %d is too large", v->v.i); }
         else if (v->v.i < 0)
           { exec_error(f, x, "Counter value %d is negative", v->v.i); }
         v->v.c = new_counter_value(v->v.i, f);
         v->rep = REP_cnt;
         v->v.c->err_obj = x;
       return;
       default:
         assert(!"Illegal initial value");
       return;
     }
 }

static void wire_init
(value_tp *v, type *tp, token_tp sym, parse_obj_flags flags, exec_info *f)
 /* Pre: tp is wired type, boolean wire, or an array thereof */
 { int i;
   wired_type *wtps;
   llist l;
   wire_decl *w;
   value_tp zval;
   parse_obj_flags wfl;
   switch (tp->kind)
     { case TP_array:
         force_value_array(v, tp, f);
         for (i = 0; i < v->v.l->size; i++)
           { wire_init(&v->v.l->vl[i], tp->elem.tp, sym, flags, f); }
       return;
       case TP_wire:
         wtps = (wired_type*)tp->tps;
         v->v.l = new_value_list(wtps->nr_var, f);
         v->rep = REP_record;
         l = wtps->li;
         wfl = LI_IS_WRITE(flags, wtps->type)? EXPR_writable : 0;
         for (i = 0; i < v->v.l->size; i++)
           { w = llist_head(&l);
             if (w->z)
               { eval_expr(w->z, f);
                 pop_value(&zval, f);
                 range_check(w->tps, &zval, f, w->z);
                 copy_and_clear(&v->v.l->vl[i], &zval, f);
                 f->err_obj = w;
                 wire_var_fix(&v->v.l->vl[i], wfl, f);
               }
             else
               { wire_init(&v->v.l->vl[i], &w->tps->tp, w->init_sym, wfl, f); }
             l = llist_alias_tail(&l);
             if (llist_is_empty(&l))
               { l = wtps->lo;
                 wfl ^= EXPR_writable;
               }
           }
       return;
       case TP_bool:
         v->v.w = new_wire_value(sym, f);
         if (IS_SET(flags, EXPR_writable))
           { SET_FLAG(v->v.w->flags, WIRE_has_writer);
             v->rep = REP_wwire;
           }
         else
           { v->rep = REP_rwire; }
       return;
       default:
         assert(!"Illegal wired type");
       return;
     }
 }

static int exec_var_decl(var_decl *x, exec_info *f)
 { value_tp *val, zval;
   val = &f->curr->var[x->var_idx];
   if (x->z)
     { eval_expr(x->z, f);
       pop_value(&zval, f);
       range_check(x->tp.tps, &zval, f, x->z);
       copy_and_clear(val, &zval, f);
       f->err_obj = x;
       if (IS_SET(x->flags, EXPR_wire))
         { wire_var_fix(val, IS_SET(x->flags, EXPR_writable), f); }
       else if (IS_SET(x->flags, EXPR_counter))
         { counter_var_fix(val, f); }
     }
   else if (IS_SET(x->flags, EXPR_wire))
     { wire_init(val, &x->tp, x->z_sym, x->flags, f); }
   return EXEC_next;
 }

static int exec_const_def(const_def *x, exec_info *f)
 { value_tp zval;
   eval_expr(x->z, f);
   pop_value(&zval, f);
   if (x->tps)
     { range_check(x->tp.tps, &zval, f, x->z); }
   clear_value_tp(&zval, f);
   return EXEC_next;
 }

static int exec_property_decl(property_decl *x, exec_info *f)
 { value_tp zval;
   if (x->z)
     { eval_expr(x->z, f);
       pop_value(&zval, f);
       if (zval.rep == REP_z)
         { exec_error(f, x, "Initial value %v is too large", vstr_val, &zval); }
       declare_property(x->id, zval.v.i, f->prop);
     }
   else
     { declare_property(x->id, 0, f->prop); }
   /* TODO: Check for multiple declarations with different values of z */
   return EXEC_next;
 }


/*****************************************************************************/

extern void init_variables(void)
 /* call at startup */
 { set_print(const_def);
   set_print(var_decl);
   set_print(parameter);
   set_print(wire_decl);
   set_print(meta_parameter);
   set_print(property_decl);
   set_sem(const_def);
   set_import(const_def);
   set_sem(var_decl);
   set_sem(parameter);
   set_sem(wire_decl);
   set_sem(meta_parameter);
   set_sem(property_decl);
   set_exec(var_decl);
   set_exec(const_def);
   set_exec(property_decl);
 }

