/* ifrchk.c: procedures for detecting variable interference
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
 * Authors: Chris Moore
 */

#include <standard.h>
#include "parse_obj.h"
#include "value.h"
#include "interact.h"
#include "ifrchk.h"

/*extern*/ int app_ifrchk = -1;

/********** compile time checks *********************************************/

static void ifrchk(void *x, ifrchk_info *f)
 /* Compile time check to see which statements need
  * runtime interference checks
  */
 { APP_OBJ_V(app_ifrchk, x, f); }

static void ifrchk_llist(llist l, ifrchk_info *f)
 { llist sl;
   for (sl = l; !llist_is_empty(&sl); sl = llist_alias_tail(&sl))
     { ifrchk(llist_head(&sl), f); }
 }

extern void ifrchk_setup(user_info *f)
 { ifrchk_info g;
   g.flags = 0;
   ifrchk_llist(f->ml, &g);
 }

static void ifrchk_module_def(module_def *x, ifrchk_info *f)
 { ifrchk_llist(x->dl, f); }

static void ifrchk_process_def(process_def *x, ifrchk_info *f)
 { int i, nr_var;
   ifrchk_var *var;
   parse_obj_flags flags = 0;
   if (x->nr_var == 0) return;
   nr_var = f->nr_var;
   var = f->var;
   f->nr_var = x->nr_var;
   NEW_ARRAY(f->var, f->nr_var);
   for (i = 0; i < f->nr_var; i++)
     { f->var[i].flags = 0; }
   if (x->cb) { ifrchk(x->cb, f); flags |= x->cb->flags; }
   if (x->hb) { ifrchk(x->hb, f); flags |= x->hb->flags; }
   if (x->mb) { ifrchk(x->mb, f); flags |= x->mb->flags; }
              /* TODO: disallow parallelism in meta_body instead */
   SET_IF_SET(x->flags, flags, EXPR_ifrchk);
   free(f->var);
   f->nr_var = nr_var;
   f->var = var;
 }

static void ifrchk_chp_body(chp_body *x, ifrchk_info *f)
 { ifrchk_llist(x->dl, f);
   ifrchk_llist(x->sl, f);
   if (IS_SET(f->flags, IFRCHK_haschk))
     { SET_FLAG(x->flags, EXPR_ifrchk); }
   RESET_FLAG(f->flags, IFRCHK_haschk);
 }

/********** statements ******************************************************/

#define VOLSTMT_CHK(X,F) if (IS_SET((F)->flags, IFRCHK_volstmt)) \
                           { SET_FLAG((X)->flags, EXPR_volatile); } \
                         RESET_FLAG((F)->flags, IFRCHK_volstmt)
                       

static void ifrchk_parallel_stmt(parallel_stmt *x, ifrchk_info *f)
 { llist pl;
   int i, need_chk = 0, thread_idx = f->thread_idx;
   ifrchk_var *v, *var = f->var;
   if (IS_SET(f->flags, IFRCHK_set))
     { ifrchk_llist(x->l, f);
       return;
     }
   f->thread_idx = 0;
   NEW_ARRAY(f->var, f->nr_var);
   for (i = 0; i < f->nr_var; i++)
     { f->var[i].flags = 0; }
   for (pl = x->l; !llist_is_empty(&pl); pl = llist_alias_tail(&pl))
     { ifrchk(llist_head(&pl), f);
       for (i = 0; i < f->nr_var; i++)
         { v = &f->var[i];
           if (IS_ALLSET(v->flags, IFRVAR_write | IFRVAR_pwrite))
             { SET_FLAG(v->flags, IFRVAR_ww);
               need_chk = 1;
             }
           if (IS_ALLSET(v->flags, IFRVAR_read  | IFRVAR_pwrite) ||
               IS_ALLSET(v->flags, IFRVAR_write | IFRVAR_pread))
             { SET_FLAG(v->flags, IFRVAR_rw);
               need_chk = 1;
             }
           if (IS_SET(v->flags, IFRVAR_write))
             { SET_FLAG(v->flags, IFRVAR_pwrite);
               v->write_idx = f->thread_idx;
             }
           if (IS_SET(v->flags, IFRVAR_read))
             { SET_FLAG(v->flags, IFRVAR_pread); }
           RESET_FLAG(v->flags, IFRVAR_read | IFRVAR_write);
         }
       f->thread_idx++;
     }
   if (need_chk)
     { SET_FLAG(f->flags, IFRCHK_set);
       f->thread_idx = 0;
       for (pl = x->l; !llist_is_empty(&pl); pl = llist_alias_tail(&pl))
         { ifrchk(llist_head(&pl), f);
           f->thread_idx++;
         }
       RESET_FLAG(f->flags, IFRCHK_set);
       SET_FLAG(x->flags, EXPR_ifrchk);
     }
   for (i = 0; i < f->nr_var; i++)
     { if (IS_SET(f->var[i].flags, IFRVAR_pwrite))
         { SET_FLAG(var[i].flags, IFRVAR_write);
           var[i].write_idx = thread_idx;
         }
       if (IS_SET(f->var[i].flags, IFRVAR_pread))
         { SET_FLAG(var[i].flags, IFRVAR_read); }
     }
   free(f->var);
   f->var = var;
   f->thread_idx = thread_idx;
 }

static void ifrchk_rep_stmt(rep_stmt *x, ifrchk_info *f)
 { int i, need_chk = 0;
   ifrchk_var *v, *var = f->var;
   if (x->rep_sym != ',' || IS_SET(f->flags, IFRCHK_set))
     { ifrchk_llist(x->sl, f);
       return;
     }
   NEW_ARRAY(f->var, f->nr_var);
   for (i = 0; i < f->nr_var; i++)
     { f->var[i].flags = 0; }
   ifrchk_llist(x->sl, f);
   for (i = 0; i < f->nr_var; i++)
     { if (IS_SET(f->var[i].flags, IFRVAR_write))
         { SET_FLAG(var[i].flags, IFRVAR_write);
           SET_FLAG(f->var[i].flags, IFRVAR_ww);
           var[i].write_idx = f->thread_idx;
           need_chk = 1;
         }
       if (IS_SET(f->var[i].flags, IFRVAR_read))
         { SET_FLAG(var[i].flags, IFRVAR_read); }
     }
   if (need_chk)
     { SET_FLAG(f->flags, IFRCHK_set);
       ifrchk_llist(x->sl, f);
       RESET_FLAG(f->flags, IFRCHK_set);
       SET_FLAG(x->flags, EXPR_ifrchk);
     }
   free(f->var);
   f->var = var;
 }

static void ifrchk_compound_stmt(compound_stmt *x, ifrchk_info *f)
 { ifrchk_llist(x->l, f); }

static void ifrchk_assignment(assignment *x, ifrchk_info *f)
 { ifrchk(x->e, f);
   SET_FLAG(f->flags, IFRCHK_assign);
   ifrchk(x->v, f);
   RESET_FLAG(f->flags, IFRCHK_assign);
   VOLSTMT_CHK(x, f);
 }

static void ifrchk_bool_set_stmt(bool_set_stmt *x, ifrchk_info *f)
 { SET_FLAG(f->flags, IFRCHK_assign);
   ifrchk(x->v, f);
   RESET_FLAG(f->flags, IFRCHK_assign);
   VOLSTMT_CHK(x, f);
 }

static void ifrchk_guarded_cmnd(guarded_cmnd *x, ifrchk_info *f)
 { ifrchk(x->g, f);
   VOLSTMT_CHK(x, f);
   ifrchk_llist(x->l, f);
 }

static void ifrchk_select_stmt(select_stmt *x, ifrchk_info *f)
 { if (!x->w)
     { ifrchk_llist(x->gl, f); }
   else
     { ifrchk(x->w, f);
       VOLSTMT_CHK(x, f);
     }
 }

static void ifrchk_loop_stmt(loop_stmt *x, ifrchk_info *f)
 { if (!llist_is_empty(&x->gl))
     { ifrchk_llist(x->gl, f); }
   else
     { ifrchk_llist(x->sl, f); }
 }

static void ifrchk_communication(communication *x, ifrchk_info *f)
 { switch (x->op_sym)
     { case '=': case '?':
         SET_FLAG(f->flags, IFRCHK_assign);
       /* Fallthrough */
       case '!':
         ifrchk(x->e, f);
       break;
       default:
       break;
     }
   SET_FLAG(f->flags, IFRCHK_assign);
   ifrchk(x->p, f);
   RESET_FLAG(f->flags, IFRCHK_assign);
   VOLSTMT_CHK(x, f);
 }

/********** expressions *****************************************************/

static void ifrchk_var_ref(var_ref *x, ifrchk_info *f)
 { ifrchk_var *v;
   v = &f->var[x->var_idx];
   if (IS_SET(f->flags, IFRCHK_set))
     { if (IS_SET(v->flags, IFRVAR_ww) ||
           (IS_SET(v->flags, IFRVAR_rw) &&
            (IS_SET(f->flags, IFRCHK_assign) || v->write_idx != f->thread_idx)))
         /* Skip reads if there is no write in parallel with them */
         { SET_FLAG(x->flags, EXPR_ifrchk);
           SET_FLAG(f->flags, IFRCHK_haschk);
           if (IS_SET(x->d->flags, EXPR_volatile))
             { SET_FLAG(f->flags, IFRCHK_volstmt);
               SET_FLAG(x->flags, EXPR_volatile);
                                                  /* TODO: Remove this once  */
               RESET_FLAG(x->flags, EXPR_ifrchk); /* volatile variables have */
                                                  /* proper checking.        */
             }
         }
     }
   else if (IS_SET(f->flags, IFRCHK_assign))
     { SET_FLAG(v->flags, IFRVAR_write); }
   else
     { SET_FLAG(v->flags, IFRVAR_read); }
 }

static void ifrchk_binary_expr(binary_expr *x, ifrchk_info *f)
 { ifrchk(x->l, f);
   ifrchk(x->r, f);
 }

static void ifrchk_prefix_expr(prefix_expr *x, ifrchk_info *f)
 { ifrchk(x->r, f); }

static void ifrchk_rep_expr(rep_expr *x, ifrchk_info *f)
 { ifrchk(x->v, f); }

static void ifrchk_value_probe(value_probe *x, ifrchk_info *f)
 { ifrchk_llist(x->p, f);
   ifrchk(x->b, f);
 }

static void ifrchk_array_subscript(array_subscript *x, ifrchk_info *f)
 { ifrchk(x->x, f);
   SET_IF_SET(x->flags, x->x->flags, EXPR_ifrchk);
   RESET_FLAG(f->flags, IFRCHK_assign);
   ifrchk(x->idx, f);
 }

static void ifrchk_array_subrange(array_subrange *x, ifrchk_info *f)
 { ifrchk(x->x, f);
   SET_IF_SET(x->flags, x->x->flags, EXPR_ifrchk);
   RESET_FLAG(f->flags, IFRCHK_assign);
   ifrchk(x->l, f);
   ifrchk(x->h, f);
 }

static void ifrchk_int_subrange(int_subrange *x, ifrchk_info *f)
 { ifrchk(x->x, f);
   SET_IF_SET(x->flags, x->x->flags, EXPR_ifrchk);
   RESET_FLAG(f->flags, IFRCHK_assign);
   ifrchk(x->l, f);
   ifrchk(x->h, f);
 }

static void ifrchk_field_of_record(field_of_record *x, ifrchk_info *f)
 { ifrchk(x->x, f);
   SET_IF_SET(x->flags, x->x->flags, EXPR_ifrchk);
 }

static void ifrchk_array_constructor(array_constructor *x, ifrchk_info *f)
 { ifrchk_llist(x->l, f); }

static void ifrchk_call(call *x, ifrchk_info *f)
 { llist a, l;
   parameter *p;
   for (a = x->a, l = x->d->pl; !llist_is_empty(&a); a = llist_alias_tail(&a))
     { p = llist_is_empty(&l)? 0 : llist_head(&l);
       if (p && (p->par_sym == KW_res || p->par_sym == KW_valres))
         { SET_FLAG(f->flags, IFRCHK_assign); }
       else
         { RESET_FLAG(f->flags, IFRCHK_assign); }
       ifrchk(llist_head(&a), f);
       if (!llist_is_empty(&l)) l = llist_alias_tail(&l);
     }
   RESET_FLAG(f->flags, IFRCHK_assign);
 }

/********** runtime checks **************************************************/

FLAGS(scr_flags)
   { FIRST_FLAG(SCR_read_ok), /* set if this variable can possibly be read */
     NEXT_FLAG(SCR_write_ok), /* set if this variable can possibly be written */
     NEXT_FLAG(SCR_sub_elem),
     /* When set, variable has elements that must also be checked */
     NEXT_FLAG(SCR_int_elem)
     /* When set, variable has bit slice elements */
   };

typedef struct strict_check_record strict_check_record;
struct strict_check_record
   { ctrl_state *read_frame, *write_frame;
     strict_check_record *bits;
     int nr_bits;
     scr_flags flags;
   };

static int scr_delete(hash_entry *e, void *dummy)
 { strict_check_record *r = e->data.p;
   if (IS_SET(r->flags, SCR_int_elem))
     { free(r->bits); }
   free(r);
   return 0;
 }

static strict_check_record *scr_access_bit(strict_check_record *r, int n)
 /* This function ensures that r has the flag SCR_int_elem set, and that
  * r->bits contains enough elements to include bit n, then returns
  * a reference to r->bits[n].  If r->bits[n] is being accessed for the
  * first time, it will not have SCR_int_elem set, and will be otherwise
  * uninitialized.
  */
 { int i, k;
   k = IS_SET(r->flags, SCR_int_elem)? r->nr_bits : 1;
   while (k <= n) k *= 2;
   if (!IS_SET(r->flags, SCR_int_elem))
     { NEW_ARRAY(r->bits, k);
       SET_FLAG(r->flags, SCR_int_elem);
       r->nr_bits = 0;
     }
   else if (k > r->nr_bits)
     { REALLOC_ARRAY(r->bits, k); }
   for (i = r->nr_bits; i < k; i++)
     { r->bits[i].flags = 0; }
   r->nr_bits = k;
   return &r->bits[n];
 }

extern void strict_check_init(process_state *ps, exec_info *f)
 { NEW(ps->accesses);
   hash_table_init(ps->accesses, 1, HASH_ptr_is_key, scr_delete);
 }

extern void strict_check_term(process_state *ps, exec_info *f)
 { if (ps->accesses)
     { hash_table_free(ps->accesses);
       free(ps->accesses);
       ps->accesses = 0;
     }
 }

static ctrl_state *get_proper_frame(ctrl_state *cs, exec_info *f)
/* A proper frame of execution is one whose direct parent has spawned
 * multiple threads (or has no parent)
 */
 { ctrl_state *parent = cs->stack;
   while (parent && !IS_PARALLEL_STMT(parent->obj))
     { cs = parent;
       parent = cs->stack;
     }
   return cs;
 }

static int is_parent_frame(ctrl_state *a, ctrl_state *b, exec_info *f)
/* Return whether a is b or a parent frame of b (frames need not be proper) */
 { if (!a) return 1;
   while (b)
     { if (a == b) return 1;
       b = b->stack;
     }
   return 0;
 }

static void strict_check_read_real
(strict_check_record *r, ctrl_state *frame, exec_info *f)
 { if (!IS_SET(r->flags, SCR_read_ok)
        || !is_parent_frame(r->write_frame, frame, f))
     { exec_error(f, f->err_obj,
              "Reading from a variable that was written in parallel");
     }
   if (is_parent_frame(r->read_frame, frame, f))
     { if (IS_SET(r->flags, SCR_write_ok)) r->read_frame = frame;
       return;
     }
   RESET_FLAG(r->flags, SCR_write_ok);
   while (r->read_frame->stack)
     { r->read_frame = get_proper_frame(r->read_frame->stack, f);
       if (is_parent_frame(r->read_frame, frame, f)) break;
     }
 }

static void strict_check_read_sub
(value_tp *v, ctrl_state *frame, hash_table *h, exec_info *f)
 { hash_entry *e;
   strict_check_record *r;
   value_list *vl = v->v.l;
   int i;
   for (i = 0; i < vl->size; i++)
     { e = hash_find(h, (char*)&vl->vl[i]);
       if (!e) continue;
       r = e->data.p;
       strict_check_read_real(r, frame, f);
       if (IS_SET(r->flags, SCR_sub_elem))
         { strict_check_read_sub(&vl->vl[i], frame, h, f); }
     }
 }

extern void strict_check_read(value_tp *v, exec_info *f)
 { hash_table *h;
   hash_entry *e;
   strict_check_record *r;
   ctrl_state *frame;
   int i;
   h = f->curr->ps->accesses;
   frame = get_proper_frame(f->curr, f);
   if (!hash_insert(h, (char*)v, &e))
     { NEW(r);
       r->read_frame = frame;
       r->write_frame = 0;
       r->flags = SCR_read_ok | SCR_write_ok;
       e->data.p = r;
       return;
     }
   r = e->data.p;
   strict_check_read_real(r, frame, f);
   if (IS_SET(r->flags, SCR_sub_elem))
     { strict_check_read_sub(v, frame, h, f); }
   if (IS_SET(r->flags, SCR_int_elem))
     { for (i = 0; i < r->nr_bits; i++)
         { if (IS_SET(r->bits[i].flags, SCR_int_elem))
             { strict_check_read_real(&r->bits[i], frame, f); }
         }
     }
 }

extern void strict_check_read_elem(value_tp *v, exec_info *f)
 { hash_table *h;
   hash_entry *e;
   strict_check_record *r;
   ctrl_state *frame;
   h = f->curr->ps->accesses;
   frame = get_proper_frame(f->curr, f);
   if (!hash_insert(h, (char*)v, &e))
     { NEW(r);
       r->read_frame = 0;
       r->write_frame = 0;
       r->flags = SCR_read_ok | SCR_write_ok | SCR_sub_elem;
       e->data.p = r;
       return;
     }
   r = e->data.p;
   if (!IS_SET(r->flags, SCR_read_ok) ||
       !is_parent_frame(r->write_frame, frame, f))
     { exec_error(f, f->err_obj,
                  "Reading from a variable that was written in parallel");
     }
   SET_FLAG(r->flags, SCR_sub_elem);
 }

extern void strict_check_read_bits(value_tp *v, int l, int h, exec_info *f)
 { hash_table *ht;
   hash_entry *e;
   strict_check_record *r, *ri;
   ctrl_state *frame;
   int i;
   ht = f->curr->ps->accesses;
   frame = get_proper_frame(f->curr, f);
   if (!hash_insert(ht, (char*)v, &e))
     { NEW(r);
       r->read_frame = 0;
       r->write_frame = 0;
       r->flags = SCR_read_ok | SCR_write_ok;
       e->data.p = r;
     }
   else
     { r = e->data.p;
       if (!IS_SET(r->flags, SCR_read_ok) ||
           !is_parent_frame(r->write_frame, frame, f))
         { exec_error(f, f->err_obj,
                      "Reading from a variable that was written in parallel");
         }
     }
   for (i = h; i >= l; i--)
     { ri = scr_access_bit(r, i);
       if (!IS_SET(ri->flags, SCR_int_elem))
         { ri->read_frame = frame;
           ri->write_frame = 0;
           ri->flags = SCR_read_ok | SCR_write_ok | SCR_int_elem;
         }
       else
         { strict_check_read_real(ri, frame, f); }
     }
 }

static void strict_check_write_real
(strict_check_record *r, ctrl_state *frame, exec_info *f)
/* Just does check, doesn't update write_frame */
 { if (!IS_SET(r->flags, SCR_read_ok)
        || !is_parent_frame(r->write_frame, frame, f))
     { exec_error(f, f->err_obj,
              "Writing to a variable that was written in parallel");
     }
   if (!IS_SET(r->flags, SCR_write_ok)
        || !is_parent_frame(r->read_frame, frame, f))
     { exec_error(f, f->err_obj,
              "Writing to a variable that was read in parallel");
     }
 }

static void strict_check_write_sub
(value_tp *v, ctrl_state *frame, hash_table *h, exec_info *f)
 { hash_entry *e;
   strict_check_record *r;
   value_list *vl = v->v.l;
   int i;
   for (i = 0; i < vl->size; i++)
     { e = hash_find(h, (char*)&vl->vl[i]);
       if (!e) continue;
       r = e->data.p;
       strict_check_write_real(r, frame, f);
       if (IS_SET(r->flags, SCR_sub_elem))
         { strict_check_write_sub(&vl->vl[i], frame, h, f); }
       hash_delete(h, (char*)&vl->vl[i]);
     }
 }

extern void strict_check_write(value_tp *v, exec_info *f)
 { hash_table *h;
   hash_entry *e;
   strict_check_record *r;
   ctrl_state *frame;
   int i;
   h = f->curr->ps->accesses;
   frame = get_proper_frame(f->curr, f);
   if (!hash_insert(h, (char*)v, &e))
     { NEW(r);
       r->read_frame = 0;
       r->write_frame = frame;
       r->flags = SCR_read_ok | SCR_write_ok;
       e->data.p = r;
       return;
     }
   r = e->data.p;
   strict_check_write_real(r, frame, f);
   r->write_frame = frame;
   if (IS_SET(r->flags, SCR_sub_elem))
     { strict_check_write_sub(v, frame, h, f);
       RESET_FLAG(r->flags, SCR_sub_elem);
     }
   if (IS_SET(r->flags, SCR_int_elem))
     { for (i = 0; i < r->nr_bits; i++)
         { if (IS_SET(r->bits[i].flags, SCR_int_elem))
             { strict_check_write_real(&r->bits[i], frame, f); }
         }
       free(r->bits);
       RESET_FLAG(r->flags, SCR_int_elem);
     }
 }

extern void strict_check_write_elem(value_tp *v, exec_info *f)
 { hash_table *h;
   hash_entry *e;
   strict_check_record *r;
   ctrl_state *frame;
   h = f->curr->ps->accesses;
   frame = get_proper_frame(f->curr, f);
   if (!hash_insert(h, (char*)v, &e))
     { NEW(r);
       r->read_frame = 0;
       r->write_frame = 0;
       r->flags = SCR_read_ok | SCR_write_ok | SCR_sub_elem;
       e->data.p = r;
       return;
     }
   r = e->data.p;
   strict_check_write_real(r, frame, f);
   SET_FLAG(r->flags, SCR_sub_elem);
 }

extern void strict_check_write_bits(value_tp *v, int l, int h, exec_info *f)
 { hash_table *ht;
   hash_entry *e;
   strict_check_record *r, *ri;
   ctrl_state *frame;
   int i;
   ht = f->curr->ps->accesses;
   frame = get_proper_frame(f->curr, f);
   if (!hash_insert(ht, (char*)v, &e))
     { NEW(r);
       r->read_frame = 0;
       r->write_frame = 0;
       r->flags = SCR_read_ok | SCR_write_ok;
       e->data.p = r;
     }
   else
     { r = e->data.p;
       strict_check_write_real(r, frame, f);
     }
   for (i = h; i >= l; i--)
     { ri = scr_access_bit(r, i);
       if (!IS_SET(ri->flags, SCR_int_elem))
         { ri->read_frame = 0;
           ri->write_frame = frame;
           ri->flags = SCR_read_ok | SCR_write_ok | SCR_int_elem;
         }
       else
         { strict_check_write_real(ri, frame, f);
           ri->write_frame = frame;
         }
     }
 }

static void strict_check_delete_sub(value_tp *v, hash_table *h, exec_info *f)
 { hash_entry *e;
   strict_check_record *r;
   value_list *vl = v->v.l;
   int i;
   for (i = 0; i < vl->size; i++)
     { e = hash_find(h, (char*)&vl->vl[i]);
       if (!e) continue;
       r = e->data.p;
       if (IS_SET(r->flags, SCR_sub_elem))
         { strict_check_delete_sub(&vl->vl[i], h, f); }
       hash_delete(h, (char*)&vl->vl[i]);
     }
 }

extern void strict_check_delete(value_tp *v, exec_info *f)
 { hash_table *h;
   hash_entry *e;
   strict_check_record *r;
   h = f->curr->ps->accesses;
   e = hash_find(h, (char*)v);
   if (!e) return;
   r = e->data.p;
   if (IS_SET(r->flags, SCR_sub_elem))
     { strict_check_delete_sub(v, h, f); }
   hash_delete(h, (char*)v);
 }

static int _sc_frame_end(strict_check_record *r, exec_info *f)
 { int i;
   strict_check_record *rr;
   if (r->read_frame == f->prev)
     { r->read_frame = f->curr;
       RESET_FLAG(r->flags, SCR_write_ok);
     }
   if (r->write_frame == f->prev)
     { r->write_frame = f->curr;
       RESET_FLAG(r->flags, SCR_read_ok);
     }
   if (IS_SET(r->flags, SCR_int_elem))
     { for (i = 0; i < r->nr_bits; i++)
         { rr = &r->bits[i];
           if (IS_SET(rr->flags, SCR_int_elem))
             { if (rr->read_frame == f->prev)
                 { rr->read_frame = f->curr;
                   RESET_FLAG(rr->flags, SCR_write_ok);
                 }
               if (rr->write_frame == f->prev)
                 { rr->write_frame = f->curr;
                   RESET_FLAG(rr->flags, SCR_read_ok);
                 }
             }
         }
     }
   return 0;
 }

extern void strict_check_frame_end(exec_info *f)
 { ctrl_state *curr = f->curr;
   f->curr = get_proper_frame(curr, f);
   hash_apply_data(f->curr->ps->accesses, (hash_vfunc*)_sc_frame_end, f);
   f->curr = curr;
 }

static int _sc_update(strict_check_record *r, ctrl_state *cs)
 { int i;
   strict_check_record *rr;
   if (r->read_frame == cs)
     { SET_FLAG(r->flags, SCR_write_ok); }
   if (r->write_frame == cs)
     { SET_FLAG(r->flags, SCR_read_ok); }
   if (IS_SET(r->flags, SCR_int_elem))
     { for (i = 0; i < r->nr_bits; i++)
         { rr = &r->bits[i];
           if (IS_SET(rr->flags, SCR_int_elem))
             { if (rr->read_frame == cs)
                 { SET_FLAG(rr->flags, SCR_write_ok); }
               if (rr->write_frame == cs)
                 { SET_FLAG(rr->flags, SCR_read_ok); }
             }
         }
     }
   return 0;
 }

extern void strict_check_update(ctrl_state *cs, exec_info *f)
/* This should be called after cs has completed a parallel statement
 * (i.e. all children of cs have terminated)
 */
 { ctrl_state *frame;
   frame = get_proper_frame(cs, f);
   hash_apply_data(cs->ps->accesses, (hash_vfunc*)_sc_update, frame);
 }

/* TODO: more efficient update and frame_end */


/*****************************************************************************/

extern void init_ifrchk(int app1)
 /* call at startup; pass unused object app indices */
 { app_ifrchk = app1;
   set_ifrchk(rep_expr);
   set_ifrchk(binary_expr);
   set_ifrchk(prefix_expr);
   set_ifrchk_cp(probe, prefix_expr);
   set_ifrchk(value_probe);
   set_ifrchk(array_subscript);
   set_ifrchk_cp(int_subscript, array_subscript);
   set_ifrchk_cp(int_port_subscript, array_subscript);
   set_ifrchk(array_subrange);
   set_ifrchk(int_subrange);
   set_ifrchk(field_of_record);
   set_ifrchk(array_constructor);
   set_ifrchk_cp(record_constructor, array_constructor);
   set_ifrchk(call);
   set_ifrchk(var_ref);
   set_ifrchk(parallel_stmt);
   set_ifrchk(rep_stmt);
   set_ifrchk(compound_stmt);
   set_ifrchk(assignment);
   set_ifrchk(bool_set_stmt);
   set_ifrchk(guarded_cmnd);
   set_ifrchk(select_stmt);
   set_ifrchk(loop_stmt);
   set_ifrchk(communication);
   set_ifrchk(chp_body);
   set_ifrchk_cp(hse_body, chp_body);
   set_ifrchk_cp(meta_body, chp_body);
   set_ifrchk(process_def);
   set_ifrchk(module_def);
 }
