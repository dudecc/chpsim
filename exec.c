/* exec.c: statement execution and expression evaluation
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
#include <hash.h>
#include <math.h>
#include <gmp.h>
#include "parse_obj.h"
#include "value.h"
#include "exec.h"
#include "interact.h"
#include "routines.h"
#include "expr.h"

/*extern*/ int app_exec = -1;
/*extern*/ int app_pop = -1;

/********** initialization ***************************************************/

static int action_cmp(action *a, action *b)
 { return mpz_cmp(b->time, a->time); }

extern void exec_info_init(exec_info *f, exec_info *orig)
 /* initialize *f.
    If orig, then the interaction-related fields of orig are copied.
 */
 { f->flags = 0;
   pqueue_init(&f->sched, (pqueue_func*)action_cmp);
   hash_table_init(&f->delays, 1, HASH_ptr_is_key, 0);
   llist_init(&f->check);
   llist_init(&f->chp);
   llist_init(&f->susp_perm);
   f->nr_susp = 0;
   f->curr = 0;
   f->meta_ps = 0;
   mpz_init_set_ui(f->time, 1);
   f->stack = 0; f->fl = 0;
   f->val = 0;
   f->err_obj = 0;
   var_str_init(&f->scratch, 0);
   var_str_init(&f->err, 0);
   if (orig)
     { f->user = orig->user;
       f->parent = orig;
       f->crit_map = orig->crit_map;
     }
   else
     { f->user = 0;
       f->parent = 0;
       f->crit_map = 0;
     }
 }

extern void exec_info_term(exec_info *f)
 /* termination actions */
 { eval_stack *w, *tmp;
   pqueue_term(&f->sched);
   hash_table_free(&f->delays);
   mpz_clear(f->time);
   assert(!f->susp_perm);
   f->curr = 0;
   assert(!f->stack);
   w = f->fl;
   while (w)
     { tmp = w; w = w->next;
       free(tmp);
     }
   f->fl = 0;
   var_str_free(&f->scratch);
 }

extern ctrl_state *new_ctrl_state(exec_info *f)
 /* Allocate a new state. time is set to the current time, meta is
    set from f->curr (if any). Other fields are initialized to 0.
    The state has not yet been scheduled.
    Note: a ctrl_state is deallocated just before a pop, but the var[]
    array is not automatically cleared.
 */
 { ctrl_state *s;
   NEW(s);
   mpz_init_set(s->act.time, f->time);
   s->act.cs = s;
   s->act.flags = 0;
   s->obj = 0;
   llist_init(&s->seq);
   s->var = 0;
   s->nr_var = 0;
   if (f->curr)
     { s->ps = f->curr->ps;
       s->rep_vals = f->curr->rep_vals;
       s->cxt = f->curr->cxt;
     }
   else
     { s->ps = 0;
       s->rep_vals = 0;
     }
   s->stack = 0;
   s->i = 0;
   s->argv = 0;
   s->argc = 0;
   llist_init(&s->dep);
   return s;
 }

static void free_ctrl_state(ctrl_state *s, exec_info *f)
 { mpz_clear(s->act.time);
   free(s);
 }

extern action *new_action(exec_info *f)
 /* Just create a new action. (For prs, etc. )
  * cs is set to f->curr, time is set to current time, target is not set.
  */
 { action *a;
   NEW(a);
   a->flags = 0;
   a->cs = f->curr;
   mpz_init_set(a->time, f->time);
   return a;
 }

extern process_state *new_process_state(exec_info *f, const str *nm)
 /* Allocate new process state, all fields 0 except cs is a new ctrl_state
    and the specified nm is used.
    If a process_state with the specified name already exists and the process is
    not invisible, that state is returned instead.
 */
 { process_state *ps;
   ps = llist_find(&f->user->procs, (llist_func*)instance_eq, nm);
   if (!ps)
     { NEW(ps);
       ps->refcnt = 1;
       ps->p = 0;
       ps->nm = (str*)nm;
       ps->meta = 0;
       ps->nr_meta = 0;
       ps->cs = new_ctrl_state(f);
       ps->cs->ps = ps;
       ps->nr_thread = 0;
       ps->nr_susp = 0;
       if (IS_SET(f->user->flags, USER_traceall))
         { ps->flags = DBG_trace; }
       else
         { ps->flags = 0; }
       llist_prepend(&f->user->procs, ps);
       if (IS_SET(f->user->flags, USER_strict))
         { strict_check_init(ps, f); }
     }
   ps->refcnt++;
   return ps;
 }

extern void free_process_state(process_state *ps, exec_info *f)
 /* Deallocate the process state. If refcnt is not 0, then currently
    nothing is deallocated to facilitate debugging.
 */
 { value_tp *v;
   int i;
   if (!ps->refcnt)
     { v = ps->var;
       for (i = 0; i < ps->nr_var; i++)
         { clear_value_tp(&v[i], f); }
       if (v)
         { free(v); ps->var = 0; ps->nr_var = 0; }
       v = ps->meta;
       for (i = 0; i < ps->nr_meta; i++)
         { clear_value_tp(&v[i], f); }
       if (v)
         { free(v); ps->meta = 0; ps->nr_meta = 0; }
       free(ps);
     }
   if (IS_SET(f->flags, EXEC_strict)) strict_check_term(ps, f);
 }

/* llist_func */
extern int routine_eq(void *obj, const str *id)
 /* true if x is a process_def or function_def with name id */
 { process_def *pd = obj;
   function_def *fd = obj;
   if (pd->class == CLASS_process_def)
     { return pd->id == id; }
   if (fd->class == CLASS_function_def)
     { return fd->id == id; }
   return 0;
 }


/********** errors ***********************************************************/

extern void exec_error(exec_info *f, void *obj, const char *fmt, ...)
 /* print error msg related to execution/eval of obj by current process */
 { parse_obj *x = obj;
   sem_context *cxt;
   int pos;
   eval_stack *rvl;
   value_tp v;
   va_list a, b;
   va_start(a, fmt);
   pos = var_str_printf(&f->err, 0, "Error: %s at %s[%d:%d]\n\t",
                f->curr? f->curr->ps->nm : "", x->src, x->lnr, x->lpos);
   pos += var_str_vprintf(&f->err, pos, fmt, a);
   if (f->err.s[pos-1] != '\n')
     { f->err.s[pos++] = '\n';
       VAR_STR_X(&f->err, pos) = 0;
     }
   rvl = f->curr? f->curr->rep_vals : 0;
   if (rvl)
     { pos += var_str_printf(&f->err, pos, "Replication values:");
       cxt = f->curr->cxt;
       while (!cxt->H)
         { pos += var_str_printf(&f->err, pos, " %s = %v,", cxt->id,
                                 vstr_val, &rvl->v);
           cxt = cxt->parent;
           rvl = rvl->next;
         }
       f->err.s[pos++] = '\n';
       VAR_STR_X(&f->err, pos) = 0;
     }
   va_end(a);
   report(f->user, "--- error ------------------------------\n");
   report(f->user, "%s", f->err.s);
   if (IS_SET(f->user->flags, USER_debug))
     { while (f->stack)
         { pop_value(&v, f);
           clear_value_tp(&v, f);
         }
       va_start(b, fmt);
       /* TODO: find vstr_val args and clear them */
       va_end(b);
       longjmp(*f->user->L->err_jmp, 1);
     }
   SET_FLAG(f->flags, EXEC_error);
   interact(f, "quit");
   report(f->user, "Error occurred: cannot continue\n");
   f->user->global = f;
   cmnd_quit(f->user);
 }

extern void exec_warning(exec_info *f, void *obj, const char *fmt, ...)
 /* print warning msg related to execution/eval of obj by current process */
 { parse_obj *x = obj;
   int pos;
   va_list a;
   if (IS_SET(f->user->flags, USER_debug)) return;
   if (IS_SET(f->flags, EXEC_print)) return;
   va_start(a, fmt);
   pos = var_str_printf(&f->err, 0, "Warning: %s at %s[%d:%d]\n\t%v\n\t",
                        f->curr->ps->nm, x->src, x->lnr, x->lpos,
                        vstr_stmt, f->curr->obj);
   pos += var_str_vprintf(&f->err, pos, fmt, a);
   if (f->err.s[pos-1] != '\n')
     { f->err.s[pos++] = '\n';
       VAR_STR_X(&f->err, pos) = 0;
     }
   va_end(a);
   report(f->user, "%s", f->err.s);
   SET_FLAG(f->flags, EXEC_warning);
 }

static void prs_error(exec_info *f, action *a, wire_value *w, const char *err)
 { int pos;
   process_state *meta_ps = f->meta_ps;
   pos = var_str_printf(&f->err, 0, "Error: %s transition on wire %V\n",
                        IS_SET(w->flags, WIRE_value)? "Upward" : "Downward",
                        vstr_wire_context, w, meta_ps);
   if (a->cs->ps == f->meta_ps)
     { print_wire_exec(a->target.w, f);
       pos += var_str_printf(&f->err, pos, "  caused %s on wire %s\n",
                             err, f->scratch.s);
     }
   else
     { f->meta_ps = a->cs->ps;
       print_wire_exec(a->target.w, f);
       pos += var_str_printf(&f->err, pos, "  caused %s on wire %s of process"
                             " %s\n", err, f->scratch.s, f->meta_ps->nm);
       f->meta_ps = meta_ps;
     }
   report(f->user, "--- error ------------------------------\n");
   report(f->user, "%s", f->err.s);
   SET_FLAG(f->flags, EXEC_error);
   interact(f, "quit");
   report(f->user, "Error occurred: cannot continue\n");
   f->user->global = f;
   cmnd_quit(f->user);
 }

/********** statement execution**********************************************/

INLINE_STATIC exec_return exec_obj(void *obj, exec_info *f)
 /* Execute obj, with state f->curr. Called when obj is first reached. */
 { return APP_OBJ_Z(app_exec, obj, f, EXEC_next); }

INLINE_STATIC exec_return pop_obj(void *obj, exec_info *f)
 /* Same as exec(), but called when obj is reached after nested stmts
    have been executed.
 */
 { return APP_OBJ_Z(app_pop, obj, f, EXEC_next); }

#define LN_MAX_DELAY 12.0

static int rand_delay()
 { double d = drand48();
   return (int)exp(d * LN_MAX_DELAY) - 1;
 }

extern void action_sched(action *a, exec_info *f)
 /* Pre: a is not scheduled. Schedule a. */
 { hash_entry *q;
   mpz_set(a->time, f->time);
   if (IS_SET(a->flags, ACTION_atomic))
     { mpz_clrbit(a->time, 0); }
   else if (IS_SET(f->user->flags, USER_random))
     { mpz_add_ui(a->time, a->time, rand_delay() << 1); }
   else if (IS_SET(a->flags, ACTION_delay))
     { q = hash_find(&f->delays, ACTION_WITH_DIR(a));
       if (q)
         { mpz_add_ui(a->time, a->time, 2 * q->data.i); }
       else if (IS_SET(a->flags, ACTION_is_pr))
         { mpz_add_ui(a->time, a->time, 200); }
     }
   else if (IS_SET(a->flags, ACTION_is_pr))
     { mpz_add_ui(a->time, a->time, 200); }
   if (IS_SET(a->flags, ACTION_susp))
     { f->nr_susp--;
       a->cs->ps->nr_susp--;
     }
   SET_FLAG(a->flags, ACTION_sched);
   pqueue_insert(&f->sched, a);
 }

#define CRIT_STEP(A,W,F) if (IS_SET((F)->user->flags, USER_critical)) \
                           crit_node_step((A), (W), (F))

extern void run_checks(wire_value *w, exec_info *f)
 /* Runs through all checks in f->check
  * w is the triggering wire (for error messages)
  */
 { hash_entry *q;
   action *a, *b;
   while (!llist_is_empty(&f->check))
     { a = llist_idx_extract(&f->check, 0);
       RESET_FLAG(a->flags, ACTION_check);
       if (IS_SET(a->flags, ACTION_is_cr))
         { if ((a->flags & (ACTION_pr_up | ACTION_up_nxt)) == ACTION_up_nxt)
             { NEW(b); *b = *a; /* Schedule a copy of a */
               mpz_init_set(b->time, f->time);
               if (IS_SET(b->flags, ACTION_delay))
                 { q = hash_find(&f->delays, (char*)a);
                   mpz_add_ui(b->time, b->time, 2 * q->data.i);
                 }
               SET_FLAG(b->flags, ACTION_resched | ACTION_sched);
               pqueue_insert(&f->sched, b);
             }
           if ((a->flags & (ACTION_pr_dn | ACTION_dn_nxt)) == ACTION_dn_nxt)
             { action_sched(a, f); }
           ASSIGN_FLAG(a->flags, a->flags >> 2, ACTION_pr_up | ACTION_pr_dn);
           return;
         }
       if (IS_ALLSET(a->flags, ACTION_up_nxt | ACTION_dn_nxt))
         { prs_error(f, a, w, "interference"); }
       if ((a->flags & (ACTION_pr_up | ACTION_up_nxt)) == ACTION_pr_up &&
           (a->target.w->flags & WIRE_val_mask) != WIRE_value)
         { prs_error(f, a, w, "instability"); }
       if ((a->flags & (ACTION_pr_dn | ACTION_dn_nxt)) == ACTION_pr_dn &&
           (a->target.w->flags & WIRE_val_mask) != 0)
         { prs_error(f, a, w, "instability"); }
       ASSIGN_FLAG(a->flags, a->flags >> 2, ACTION_pr_up | ACTION_pr_dn);
       if ((a->flags & (ACTION_pr_up | ACTION_sched)) == ACTION_pr_up &&
           (a->target.w->flags & WIRE_val_mask) != WIRE_value)
         { action_sched(a, f); CRIT_STEP(a, w, f); }
       if ((a->flags & (ACTION_pr_dn | ACTION_sched)) == ACTION_pr_dn &&
           (a->target.w->flags & WIRE_val_mask) != 0)
         { action_sched(a, f); CRIT_STEP(a, w, f); }
     }
 }

extern void sched_instance(ctrl_state *cs, exec_info *f)
/* Only use this for instances during instantiation */
 { process_def *p = cs->ps->p;
   llist_prepend(&f->user->wait, cs);
   cs->cxt = p->cxt;
 }

extern void sched_instance_real(ctrl_state *cs, exec_info *f)
/* Only use this during instantiation when nothing is scheduled */
 { process_def *p = cs->ps->p;
   void *b;
   exec_info *g;
   assert(cs->ps->nr_thread == -1);
   cs->ps->nr_thread = 1;
   if (p->mb && !IS_SET(f->flags, EXEC_sequential))
     { b = p->mb; }
   else if (p->pb) b = p->pb;
   else if (p->hb) b = p->hb;
   else if (p->cb) b = p->cb;
   else b = p->mb;
   cs->ps->b = b;
   cs->cxt = cs->ps->b->cxt;
   if (b == p->mb)
     { insert_sched(cs, f); }
   else
     { llist_prepend(&f->chp, cs); }
 }

extern ctrl_state *nested_seq(llist *l, exec_info *f)
 /* Create and return ctrl_state for seq l, as nested sequence of current
    statement (if any). Time has been set to current time (may be changed),
    and the new state has not yet been scheduled.
 */
 { ctrl_state *s;
   s = new_ctrl_state(f);
   s->obj = llist_head(l);
   s->seq = llist_alias_tail(l);
   if (f->curr)
     { s->var = f->curr->var;
       s->nr_var = f->curr->nr_var;
       s->stack = f->curr;
     }
   return s;
 }

static void next_stmt(exec_info *f)
 /* schedule statement that follows f->curr */
 { ctrl_state *s = f->curr, *t;
   exec_return r;
   value_tp v;
   if (!llist_is_empty(&s->seq))
     { s->obj = llist_head(&s->seq);
       s->seq = llist_alias_tail(&s->seq);
       s->i = 0;
       insert_sched(s, f);
     }
   else if (s->stack)
     { t = s->stack;
       f->curr = t;
       f->prev = s;
       r = pop_obj(t->obj, f);
       if (f->prev)
         { while (f->prev->rep_vals != f->curr->rep_vals)
             { pop_repval(&v, f->prev, f);
               clear_value_tp(&v, f);
             }
           free_ctrl_state(s, f);
         }
       if (r == EXEC_next)
         { next_stmt(f); }
       else if (r != EXEC_none)
         { assert(!"suspend upon pop"); }
     }
   else
     { /* thread has terminated; should this happen? */
       free_ctrl_state(f->curr, f);
       f->curr = 0;
     }
 }

static int _clear_action_dep(wire_expr *e, exec_info *f)
 { if (e->u.act == &f->curr->act)
     { clear_wire_expr(e, f);
       return 1;
     }
   return 0;
 }

static int clear_action_dep(wire_value *w, exec_info *f)
 { llist_all_extract(&w->u.dep, (llist_func*)_clear_action_dep, f);
   if (llist_is_empty(&w->u.dep))
     { RESET_FLAG(w->flags, WIRE_has_dep); }
   return 0;
 }

/*extern*/ int exec_interrupted = 0; /* if set, stop immediately */

extern void exec_run(exec_info *f)
 /* Run the program. */
 { exec_return r;
   parse_obj *x;
   process_state *ps;
   exec_info *g;
   action *a;
   while (1)
     { a = pqueue_extract(&f->sched);
       if (!a)
         { if (f->parent || !f->user->wait)
             { break; }
           if (IS_SET(f->flags, EXEC_single))
             { RESET_FLAG(f->flags, EXEC_single);
               interact(f, make_str("instantiate"));
             }
           else
             { f->curr = llist_idx_extract(&f->user->wait, 0);
               sched_instance_real(f->curr, f);
             }
           continue;
         }
       if (mpz_cmp(f->time, a->time) < 0)
         { mpz_set(f->time, a->time);
           mpz_setbit(f->time, 0);
         }
       f->curr = a->cs;
       f->meta_ps = ps = f->curr->ps;
       if (IS_SET(a->flags, ACTION_susp))
         { llist_free(&f->curr->dep, (llist_func*)clear_action_dep, f); }
       RESET_FLAG(a->flags, ACTION_sched | ACTION_susp);
       if (IS_SET(a->flags, ACTION_is_pr))
         { if (IS_SET(a->flags, ACTION_pr_up | ACTION_pr_dn))
             { write_wire(IS_SET(a->flags, ACTION_pr_up), a->target.w, f); }
         }
       else if (IS_SET(a->flags, ACTION_is_cr))
         { update_counter(IS_SET(a->flags, ACTION_up_nxt), a->target.c, f);
           if ((a->flags & (ACTION_sched | ACTION_resched)) == ACTION_resched)
              { mpz_clear(a->time); free(a); }
         }
       else if (IS_SET(a->flags, ACTION_delay_susp))
         { RESET_FLAG(a->flags, ACTION_delay_susp);
           next_stmt(f);
         }
       else
         { x = f->curr->obj;
           if (IS_SET(ps->flags, DBG_step|DBG_next) ||
               IS_SET(x->flags, DBG_break) || exec_interrupted)
             { interact_report(f); }
           else if (IS_SET(ps->flags, DBG_trace))
             { report(f->user, "(trace) %s at %s[%d:%d]\n\t%v\n",
                      f->curr->ps->nm, x->src, x->lnr, x->lpos, vstr_stmt, x);
             }
           RESET_FLAG(f->flags, EXEC_warning | EXEC_immediate);
           r = exec_obj(x, f);
           assert(!f->stack);
           if (IS_SET(f->flags, EXEC_warning))
             { RESET_FLAG(f->flags, EXEC_warning);
               interact(f, "continue");
             }
           if (IS_SET(a->flags, ACTION_delay_susp))
             { f->nr_susp++;
               a->cs->ps->nr_susp++;
             }
           else if (r == EXEC_next)
             { next_stmt(f); }
           else if (r == EXEC_suspend)
             { if (llist_is_empty(&f->curr->dep))
                 { llist_prepend(&f->susp_perm, f->curr); }
               SET_FLAG(a->flags, ACTION_susp);
               f->nr_susp++;
               a->cs->ps->nr_susp++;
             }
         }
     }
   if (f->nr_susp)
     { report(f->user, "--- error ------------------------------\n");
       if (f->parent)
         { report(f->user, "Error: deadlock in function\n"); }
       else
         { report(f->user, "Error: deadlock\n"); }
       if (!llist_is_empty(&f->susp_perm))
         { report(f->user, "  %d threads are permanently suspended:",
                  llist_size(&f->susp_perm));
           show_perm_threads(f, f->user);
           f->curr = 0;
         }
       else
         { deadlock_find(f); }
       SET_FLAG(f->flags, EXEC_error|EXEC_deadlock);
       interact(f, "quit");
       f->user->global = f;
       cmnd_quit(f->user);
     }
 }

extern void prepare_chp(exec_info *f)
 /* Prepare chp execution phase */
 { ctrl_state *s;
   mpz_set_ui(f->time, 1);
   RESET_FLAG(f->flags, EXEC_instantiation);
   if (IS_SET(f->user->flags, USER_strict))
     { SET_FLAG(f->flags, EXEC_strict); }
   while (!llist_is_empty(&f->chp))
     { s = llist_idx_extract(&f->chp, 0);
       f->curr = s;
       f->meta_ps = s->ps;
       assert(s->obj->class == CLASS_process_def);
       exec_obj(s->obj, f);
       /* TODO: breakpoints on the process def? */
     }
 }

extern void exec_immediate(llist *l, exec_info *f)
 /* Execute contents of l without delay, skipping
  * process/function definitions
  */
 { llist m;
   parse_obj *x, *curr;
   m = *l;
   curr = f->curr->obj;
   SET_FLAG(f->flags, EXEC_immediate);
   while (!llist_is_empty(&m))
     { x = llist_head(&m);
       if (x->class != CLASS_process_def && x->class != CLASS_function_def)
         { f->curr->obj = x;
           if (IS_SET(x->flags, DBG_break))
             { interact_report(f); }
           exec_obj(x, f);
           if (IS_SET(f->flags, EXEC_warning))
             { RESET_FLAG(f->flags, EXEC_warning);
               interact(f, "continue");
             }
         }
       m = llist_alias_tail(&m);
     }
   f->curr->obj = curr;
 }

extern void exec_modules(llist *l, exec_info *f)
 /* llist(module_def) *l;
    Execute the global declarations (i.e., the const_defs) of each module.
    Note: f->curr must be set, in case the declarations contain function
    calls.
 */
 { llist m;
   module_def *d;
   m = *l;
   while (!llist_is_empty(&m))
     { d = llist_head(&m);
       exec_immediate(&d->dl, f);
       m = llist_alias_tail(&m);
     }
 }


/*****************************************************************************/

extern void init_exec(int app1, int app2)
 /* call at startup; pass unused object app indices */
 { app_exec = app1;
   app_pop = app2;
 }
