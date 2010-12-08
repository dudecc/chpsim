/* interact.c: user interaction

   Author: Marcel van der Goot
*/

#ifndef NEED_UINT
#define NOUINT
#endif
#ifndef NO_SIGNALS
#include <signal.h>
#endif
#include <standard.h>
#include "parse_obj.h"
#include "exec.h"
#include "print.h"
#include "interact.h"
#include "modules.h"
#include "routines.h"
#include "expr.h"
#include "types.h"
#include "parse.h"
#include <errno.h>

/*extern*/ int app_brk = -1;

static int is_prefix(const char *p, const char *s)
 /* true if p is prefix of s */
 { while (*p && *p == *s)
     { p++; s++; }
   if (!*p) return 1;
   return 0;
 }

/* hash_func */
static int free_brk_cond(hash_entry *e, user_info *f)
 { brk_cond *bc = e->data.p;
   free(bc);
   return 0;
 }

extern void user_info_init(user_info *f)
 /* initialize *f */
 { f->flags = 0;
   f->log = 0;
   f->user_stdout = stdout;
   NEW(f->L);
   lex_tp_init(f->L);
   f->L->fin = stdin;
   f->L->fin_nm = "(interaction)";
   llist_init(&f->old_L);
   llist_init(&f->cmds);
   llist_init(&f->procs);
   llist_init(&f->ml);
   llist_init(&f->path);
   llist_init(&f->wait);
   hash_table_init(&f->brk_condition, 1, HASH_ptr_is_key,
                   (hash_func*)free_brk_cond);
   f->brk_condition.del_info = f;
   f->cxt = 0;
   /* These are initialized at start of interaction: */
   f->global = f->focus = (void*)0xBAD;
   f->focus_top = f->curr = (void*)0xBAD;
   f->view_pos = 0xBAD;
   var_str_init(&f->scratch, 100);
   var_str_init(&f->rep, 100);
   f->limit = 0;
 }

static int init_focus(user_info *f, exec_info *g)
 /* Set the focus to g->curr. Return 0 if there was no thread left,
    1 otherwise.
 */
 { action *a;
   f->focus = g;
   if (g->curr)
     { f->curr = f->focus_top = g->curr; }
   else if ((a = pqueue_root(&g->sched)))
     { f->curr = f->focus_top = a->cs; }
   else if (!llist_is_empty(&f->wait))
     { f->curr = f->focus_top = llist_head(&f->wait); }
   else if (!llist_is_empty(&g->susp_perm))
     { f->curr = f->focus_top = llist_head(&g->susp_perm); }
   else /* There may be other suspended threads, but don't bother */
     { return 0; }
   f->view_pos = 0;
   return 1;
 }

static void *parse_expr_interact(user_info *f)
 { void *e;
   SET_FLAG(f->L->flags, LEX_cmnd_kw);
   lex_redo(f->L);
   e = parse_expr(f->L);
   RESET_FLAG(f->L->flags, LEX_cmnd_kw);
   lex_redo(f->L);
   return e;
 }

static void sem_interact(expr **e, sem_flags flags, user_info *f)
 { sem_info g;
   sem_info_init(&g);
   g.L = f->L;
   g.user = f;
   g.flags = flags;
   SET_FLAG(g.flags, SEM_debug);
   if (f->cxt) g.cxt = f->cxt;
   else if (f->curr && !IS_SET(f->flags, USER_global))
     { g.cxt = f->curr->cxt; }
   else
     { g.cxt = f->main->p->cxt->parent; }
   *e = sem(*e, &g);
   sem_info_term(&g);
 }

static void eval_interact(value_tp *v, expr *e, user_info *f)
 { exec_info g;
   if (f->curr && !IS_SET(f->flags, USER_global))
     { exec_info_init(&g, f->global);
       g.curr = f->curr;
       g.meta_ps = f->curr->ps;
       SET_FLAG(f->flags, USER_debug);
       eval_expr(e, &g);
       pop_value(v, &g);
       RESET_FLAG(f->flags, USER_debug);
       exec_info_term(&g);
       /* TODO: don't leak on exec_error's */
     }
   else
     { SET_FLAG(f->flags, USER_debug);
       eval_expr(e, f->global);
       pop_value(v, f->global);
       RESET_FLAG(f->flags, USER_debug);
     }
 }


/********** instances ********************************************************/

extern int is_visible(process_state *ps)
 /* false if ps should be invisible (cannot be looked up by name) */
 { return ps->nm && ps->nm[1] != '/'; }

/* llist_func */
extern int instance_eq(process_state *ps, const str *nm)
 /* true if ps has name nm */
 { return is_visible(ps) && ps->nm == nm; }

extern process_state *find_instance(user_info *f, const str *nm, int must)
 /* Find the process instance for nm. If 'must' and not found, then a new
    instance is created.
 */
 { process_state *ps;
   ps = llist_find(&f->procs, (llist_func*)instance_eq, nm);
   if (!ps && must)
     { ps = new_process_state(f->global, nm); }
   return ps;
 }

/*****************************************************************************/

typedef int cmnd_func_tp(user_info *f);
 /* Execute current command.
    Return 1 if interaction should continue, 0 if program execution should
    continue.
 */

static int cmnd_flag(user_info *f, dbg_flags flag, int set)
 /* set/reset flag of current or specified process instance.
    Return 0: done for current process
           1: done for other process
           2: incorrect usage
           3: not done because instance has terminated or does not exist
 */
 { const str *nm;
   process_state *ps;
   if (lex_have(f->L, TOK_instance))
     { nm = f->L->curr->t.val.s;
       if (nm != f->curr->ps->nm)
         { ps = find_instance(f, nm,
                              IS_SET(f->global->flags, EXEC_instantiation));
           if (!ps)
             { report(f, "  No such process instance: %s\n", nm);
               return 3;
             }
           if (ps->nr_thread < -1)
             { report(f, "  Process instance %s has terminated\n",nm);
               return 3;
             }
           if (set) SET_FLAG(ps->flags, flag);
           else RESET_FLAG(ps->flags, flag);
           return 1;
         }
     }
   else if (!lex_have(f->L, TOK_nl))
     { return 2; }
   if (set) SET_FLAG(f->curr->ps->flags, flag);
   else RESET_FLAG(f->curr->ps->flags, flag);
   return 0;
 }

/********** stepping *********************************************************/

/* cmnd_func_tp */
static int cmnd_step(user_info *f)
 { int e;
   e = cmnd_flag(f, DBG_step, 1);
   if (e == 2)
     { report(f, "  Usage: step [instance]\n"); }
   return e;
 }

/* cmnd_func_tp */
static int cmnd_next(user_info *f)
 { int e;
   e = cmnd_flag(f, DBG_next, 1);
   if (e == 2)
     { report(f, "  Usage: next [instance]\n"); }
   return e;
 }

/* cmnd_func_tp */
static int cmnd_continue(user_info *f)
 { if (!lex_have(f->L, TOK_nl))
     { if (f->L->curr->t.val.s == make_str("seq"))
         { SET_FLAG(f->global->flags, EXEC_sequential); }
       else
         { report(f, "  Usage: continue\n");
           return 2;
         }
     }
   return 0;
 }

/* cmnd_func_tp */
static int cmnd_trace(user_info *f)
 { int e;
   e = cmnd_flag(f, DBG_trace, 1);
   if (e == 2)
     { report(f, "  Usage: trace [instance]\n"); }
   return 1;
 }


/********** process information **********************************************/

/* pqueue_func */
static int show_thread(action *a, user_info *f)
 /* show thread if a->cs->ps = f->ps (show unconditionally if !f->ps) */
 { const char *msg;
   ctrl_state *cs = a->cs;
   if (!is_visible(cs->ps)) return 0;
   if (f->ps && cs->ps != f->ps) return 0;
   if ((cs->act.flags & (ACTION_susp | ACTION_sched)) != ACTION_susp)
     { msg = "active"; }
   else if (llist_is_empty(&cs->dep))
     { msg = "susp-perm"; }
   else
     { msg = "suspended"; }
   report(f, "(%s) %s at %s[%d:%d]\n", msg, cs->ps->nm, cs->obj->src,
          cs->obj->lnr, cs->obj->lpos);
   return 0;
 }

static void show_conn_aux(value_tp *v, type *tp, user_info *f)
 /* Pre: v->rep, v is a port or array/record/union of ports */
 { int i, pos = VAR_STR_LEN(&f->scratch);
   array_type *atps;
   var_decl *d;
   value_tp idxv, *pv;
   record_type *rtps;
   record_field *rf;
   llist m;
   process_state *meta_ps;
   meta_parameter *mp;
   type_value *tpv;
   if (tp && tp->kind == TP_generic)
     { mp = (meta_parameter*)tp->tps;
       assert(mp->class == CLASS_meta_parameter);
       meta_ps = f->global->meta_ps;
       tpv = meta_ps->meta[mp->meta_idx].v.tp;
       f->global->meta_ps = tpv->meta_ps;
       show_conn_aux(v, &tpv->tp, f);
       f->global->meta_ps = meta_ps;
     }
   else if (v->rep == REP_array)
     { if (tp->kind == TP_array)
         { atps = (array_type*)tp->tps;
           eval_expr(atps->l, f->global);
           pop_value(&idxv, f->global);
         }
       else
         { idxv.rep = REP_int; idxv.v.i = 0; }
       for (i = 0; i < v->v.l->size; i++)
         { var_str_printf(&f->scratch, pos, "[%v]", vstr_val, &idxv);
           show_conn_aux(&v->v.l->vl[i], tp->elem.tp, f);
           int_inc(&idxv, f->global);
         }
       clear_value_tp(&idxv, f->global);
     }
   else if (v->rep == REP_record)
     { if (tp->kind == TP_wire)
         { report(f, "  %s (wired)\n", f->scratch.s); }
       else
         { rtps = (record_type*)tp->tps;
           assert(rtps && rtps->class == CLASS_record_type);
           m = rtps->l;
           for (i = 0; i < v->v.l->size; i++)
             { rf = llist_head(&m);
               var_str_printf(&f->scratch, pos, ".%s", rf->id);
               show_conn_aux(&v->v.l->vl[i], &rf->tp, f);
               m = llist_alias_tail(&m);
             }
         }
     }
   else if (v->rep == REP_union)
     { var_str_printf(&f->scratch, pos, ".%s", v->v.u->f->id);
       show_conn_aux(&v->v.u->v, &v->v.u->f->tp, f);
     }
   else if (v->rep != REP_port)
     { report(f, "  %s has not been connected\n", f->scratch.s); }
   else if (!v->v.p->p)
     { report(f, "  %s has been disconnected\n", f->scratch.s); }
   else if (!is_visible(v->v.p->p->ps))
     { assert(v->v.p->p->dec);
       d = llist_idx(&v->v.p->p->ps->p->pl, 0);
       pv = &v->v.p->p->ps->var[d->var_idx];
       if (pv->v.p == v->v.p->p)
         { d = llist_idx(&v->v.p->p->ps->p->pl, 1);
           pv = &v->v.p->p->ps->var[d->var_idx];
         }
       report(f, "  %s.%s = %v\n",
              f->scratch.s, v->v.p->p->dec->id, vstr_val, pv);
     }
   else
     { report(f, "  %s --> %s:%v\n", f->scratch.s,
              v->v.p->p->ps->nm, vstr_port, v->v.p->p);
     }
 }

static void show_connections(process_state *ps, user_info *f)
 /* show meta parameters and port connections of ps */
 { var_decl *d;
   meta_parameter *mp;
   value_tp *v;
   llist m;
   process_state *meta_ps = f->global->meta_ps;
   f->global->meta_ps = ps;
   m = ps->p->ml;
   while (!llist_is_empty(&m))
     { mp = llist_head(&m);
       report(f, "  %s = %v\n", mp->id, vstr_val, &ps->meta[mp->meta_idx]);
       m = llist_alias_tail(&m);
     }
   m = ps->p->pl;
   while (!llist_is_empty(&m))
     { d = llist_head(&m);
       v = &ps->var[d->var_idx];
       if (!v->rep)
         { report(f, "  %s --> (not connected)\n", d->id); }
       else
         { var_str_copy(&f->scratch, d->id);
           show_conn_aux(v, &d->tp, f);
         }
       m = llist_alias_tail(&m);
     }
   f->global->meta_ps = meta_ps;
 }

static void __get_susp_threads(wire_value *w, hash_table *h, user_info *f)
 { llist l;
   wire_expr *e;
   if (!IS_SET(w->flags, WIRE_has_dep)) return;
   l = w->u.dep;
   while (!llist_is_empty(&l))
     { e = llist_head(&l);
       l = llist_alias_tail(&l);
       while (!IS_SET(e->flags, WIRE_action))
         { e = e->u.dep; }
       if (!IS_SET(e->u.act->flags, ACTION_sched) &&
           e->u.act->cs->ps == f->ps && e->u.act != &f->global->curr->act)
         { hash_insert(h, (char*)e->u.act, 0); }
     }
 }

static void _get_susp_threads(value_tp *v, hash_table *h, user_info *f)
 { int i;
   switch (v->rep)
     { case REP_array: case REP_record:
         for (i = 0; i < v->v.l->size; i++)
           { _get_susp_threads(&v->v.l->vl[i], h, f); }
       return;
       case REP_union:
         _get_susp_threads(&v->v.u->v, h, f);
       return;
       case REP_wire:
         __get_susp_threads(v->v.w, h, f);
       return;
       case REP_port:
         __get_susp_threads(&v->v.p->wprobe, h, f);
         if (v->v.p->p) __get_susp_threads(&v->v.p->p->wprobe, h, f);
       return;
       default:
       return;
     }
 }

static void get_susp_threads(hash_table *h, user_info *f)
 /* Initialize h, then place all suspended threads of f->ps into h
  * (each entry has a thread as a key).  Use hash_table_free when finished.
  */
 { int i;
   hash_table_init(h, 1, HASH_ptr_is_key, 0);
   for (i = 0; i < f->ps->nr_var; i++)
     { _get_susp_threads(&f->ps->var[i], h, f); }
 }

static void show_ps_threads(process_state *ps, user_info *f)
 /* Pre: ps is not a production rule process
  * Print all threads of ps.
  */
 { ctrl_state *cs;
   exec_info *g = f->global; /* TODO: include function call threads */
   hash_table h;
   int i;
   f->ps = ps;
   if (g->curr) show_thread(&g->curr->act, f);
   while (g)
     { pqueue_apply(&g->sched, (pqueue_func*)show_thread, f);
       llist_apply(&g->susp_perm, (llist_func*)show_thread, f);
       g = g->parent;
     }
   get_susp_threads(&h, f);
   hash_apply_key(&h, (hash_vfunc*)show_thread, f);
   hash_table_free(&h);
 }

/********** quit *************************************************************/

/* llist_func */
static int _show_all_threads(process_state *ps, user_info *f)
 { if (ps->nr_thread <= 0) return 0;
   if (!is_visible(ps)) return 0;
   if (ps->nr_susp == 0)
     { report(f, "  %s: %d active threads", ps->nm, ps->nr_thread); }
   else if (ps->nr_susp == ps->nr_thread)
     { report(f, "  %s: %d suspended threads", ps->nm, ps->nr_susp); }
   else
     { report(f, "  %s: %d active, %d suspended threads",
              ps->nm, ps->nr_thread - ps->nr_susp, ps->nr_susp);
     }
   return 0;
 }

extern void show_all_threads(user_info *f)
 /* print all threads
  * abbreviate by listing only number of threads of each type per process
  */
 { llist_apply(&f->procs, (llist_func*)_show_all_threads, f); }

extern void show_perm_threads(exec_info *g, user_info *f)
 /* print all threads in permanent susension */
 { f->ps = 0;
   llist_apply(&g->susp_perm, (llist_func*)show_thread, f);
 }

/* cmnd_func_tp */
extern int cmnd_quit(user_info *f)
 /* Pre: f->global is set.
    terminate the program
 */
 { const str *answ;
   exec_info *g = f->global;
   int nr_active;
   if (IS_SET(f->flags, USER_batch))
     { if (init_focus(f, g))
         { report(f, "(cmnd) where\n");
           cmnd_where(f);
         }
       report(f, "(cmnd) quit\n");
     }
   if (IS_SET(g->flags, EXEC_error))
     { exit(1); }
   if (IS_SET(f->flags, USER_batch))
     { exit(0); }
   nr_active = g->sched.size;
   if (g->curr && !IS_SET(g->curr->act.flags, ACTION_susp))
     { nr_active++; }
   var_str_printf(&f->scratch, 0, "\nThere are still %d active processes;"
                                  " really quit (y/n)? ", nr_active);
   while (nr_active)
     { prompt_user(f, f->scratch.s);
       if (lex_have(f->L, TOK_id))
         { answ = f->L->curr->t.val.s;
           if (is_prefix("y", answ) && is_prefix(answ, "yes"))
             { exit(0); }
           else if (is_prefix("n", answ) && is_prefix(answ, "no"))
             { return 1; }
         }
     }
   exit(0);
 }

/********** show/print *******************************************************/

/* cmnd_func_tp */
static int cmnd_show(user_info *f)
 { process_state *ps;
   const str *nm;
   expr *e;
   sem_info g;
   value_tp val;
   if (lex_have(f->L, TOK_nl))
     { show_all_threads(f); }
   else if (lex_have(f->L, TOK_instance))
     { nm = f->L->curr->t.val.s;
       ps = find_instance(f, nm, 0);
       if (!ps || ps->nr_thread == 0)
         { report(f, "  No such process instance: %s\n", nm); }
       else if (ps->nr_thread < -1)
         { report(f, "  Process instance %s has terminated\n", nm); }
       else
         { report(f, "  %s is an instance of %s[%d:%d] %s", nm,
                  ps->p->src, ps->p->lnr, ps->p->lpos, ps->p->id);
           f->limit = REPORT_LIMIT;
           show_connections(ps, f);
           f->limit = 0;
           if (ps->nr_thread == -1)
             { report(f, "  Process instance %s has not yet started\n", nm); }
           else if (ps->b->class == CLASS_prs_body)
             { report(f, "  Process instance %s has production rules", nm); }
           else
             { show_ps_threads(ps, f); }
         }
     }
   else
     { e = parse_expr_interact(f);
       if (!lex_have(f->L, TOK_nl))
         { report(f, "  Usage: print [expression|instance]\n"); }
       else
         { sem_interact(&e, 0, f);
           eval_interact(&val, e, f);
           report(f, "  %v = %v\n", vstr_obj, e, vstr_val, &val);
           clear_value_tp(&val, f->global);
         }
       parse_cleanup(f->L);
     }
   return 1;
 }

/********** stack trace ******************************************************/

/* cmnd_func_tp */
extern int cmnd_where(user_info *f)
 { ctrl_state *cs;
   obj_class *class;
   int first = 1;
   exec_info *g = f->focus;
   /* The first cs is the current stmt. After that, a CLASS_call corresponds
      to a procedure call. If f has a parent, the first cs of the parent
      has a function call.
   */
   if (!IS_SET(f->flags, USER_batch) && !lex_have(f->L, TOK_nl))
     { report(f, "  Usage: where\n");
       return 2;
     }
   cs = f->focus_top;
   while (g)
     { while (cs)
         { class = cs->obj->class;
           if (is_visible(cs->ps) && (first || class == CLASS_call))
             { report(f, "%s at %s[%d:%d]\n\t%v\n",
                      cs->ps->nm, cs->obj->src, cs->obj->lnr, cs->obj->lpos,
                      vstr_stmt, cs->obj);
               first = 0;
             }
           cs = cs->stack;
         }
       g = g->parent;
       if (g) cs = g->curr;
       first = 1;
     }
   return 1;
 }

static void set_view_level(user_info *f, int n)
 /* Make the n-th item on the current call-stack (as printed by 'where')
    the current obj. (Deepest nesting is item 0.)
 */
 { /* essentially the same as cmnd_where() */
   ctrl_state *cs;
   obj_class *class;
   int first = 1;
   exec_info *g = f->focus;
   /* The first cs is the current stmt. After that, a CLASS_call corresponds
      to a procedure call. If f has a parent, the first cs of the parent
      has a function call.
   */
   cs = f->focus_top;
   f->curr = cs;
   f->view_pos = -1;
   while (g && f->view_pos < n)
     { while (cs && f->view_pos < n)
         { class = cs->obj->class;
           if (first || class == CLASS_call)
             { f->view_pos++;
               f->curr = cs;
               first = 0;
             }
           cs = cs->stack;
         }
       g = g->parent;
       if (g) cs = g->curr;
       first = 1;
     }
 }

/* cmnd_func_tp */
static int cmnd_up(user_info *f)
 { int n = 1;
   parse_obj *x;
   if (lex_have_next(f->L, TOK_int))
     { n = f->L->prev->t.val.i; }
   if (!lex_have(f->L, TOK_nl))
     { report(f, "  Usage: up [int]\n");
       return 1;
     }
   set_view_level(f, f->view_pos + n);
   x = f->curr->obj;
   report(f, "(view) %s at %s[%d:%d]\n\t%v\n",
             f->curr->ps->nm, x->src, x->lnr, x->lpos, vstr_stmt, x);
   return 1;
 }

/* cmnd_func_tp */
static int cmnd_down(user_info *f)
 { int n = 1;
   parse_obj *x;
   if (lex_have_next(f->L, TOK_int))
     { n = f->L->prev->t.val.i; }
   if (!lex_have(f->L, TOK_nl))
     { report(f, "  Usage: down [int]\n");
       return 1;
     }
   set_view_level(f, f->view_pos - n);
   x = f->curr->obj;
   report(f, "(view) %s at %s[%d:%d]\n\t%v\n",
             f->curr->ps->nm, x->src, x->lnr, x->lpos, vstr_stmt, x);
   return 1;
 }

/* pqueue_func */
static int _set_focus_ps(action *a, user_info *f)
 /* change the focus to a->cs if it belongs to f->ps */
 { if (a->cs->ps != f->ps) return 0;
   f->curr = f->focus_top = a->cs;
   return 1;
 }

static void set_focus_ps(process_state *ps, user_info *f)
 /* Pre: ps is currently running (has at least one thread)
  * set the focus of f to some thread of ps
  */
 { exec_info *old_focus = f->focus;
   ctrl_state *cs;
   hash_table h;
   int res;
   f->view_pos = 0;
   f->focus = f->global;
   f->ps = ps;
   cs = f->focus->curr;
   if (cs && _set_focus_ps(&cs->act, f)) return;
   while (f->focus)
     { if (pqueue_apply(&f->focus->sched, (pqueue_func*)_set_focus_ps, f))
         { return; }
       if (llist_apply(&f->focus->susp_perm, (llist_func*)_set_focus_ps, f))
         { return; }
       get_susp_threads(&h, f);
       res = hash_apply_key(&h, (hash_vfunc*)_set_focus_ps, f);
       hash_table_free(&h);
       if (res) return;
       if (!f->focus->parent)
         { if (llist_apply(&f->wait, (llist_func*)_set_focus_ps, f))
             { return; }
         }
       f->focus = f->focus->parent;
     }
   f->focus = old_focus;
 }

static int set_focus(user_info *f)
/* Parse the instance name from the input and set the focus
   accordingly.  (No instance name means no change in focus)
 */
 { const str *nm;
   process_state *ps;
   if (!lex_have(f->L, TOK_instance)) return 0;
   nm = f->L->curr->t.val.s;
   if (nm == f->curr->ps->nm) return 0;
   ps = find_instance(f, nm, 0);
   if (!ps || ps->nr_thread == 0)
     { report(f, "  No such process instance: %s\n", nm);
       return 1;
     }
   else if (ps->nr_thread < -1)
     { report(f, "  Process instance %s has terminated\n", nm);
       return 1;
     }
   else if (ps->nr_thread == -1)
     { report(f, "  Process instance %s has not yet started\n", nm);
       return 1;
     }
   set_focus_ps(ps, f);
   return 0;
 }

/* cmnd_func_tp */
static int cmnd_view(user_info *f)
 { parse_obj *x;
   if (!set_focus(f))
     { x = f->curr->obj;
       report(f, "(view) %s at %s[%d:%d]\n\t%v\n",
              f->curr->ps->nm, x->src, x->lnr, x->lpos, vstr_stmt, x);
     }
   return 1;
 }


/********** deadlock *********************************************************/

static process_state *_deadlock_find(process_state *ps, exec_info *f);

static process_state *__deadlock_find
(value_tp *v, process_state *ps, exec_info *f)
 { int i;
   llist l;
   wire_expr *e;
   process_state *r;
   switch (v->rep)
     { case REP_array: case REP_record:
         for (i = 0; i < v->v.l->size; i++)
           { r = __deadlock_find(&v->v.l->vl[i], ps, f);
             if (r) return r;
           }
       return 0;
       case REP_union:
       return __deadlock_find(&v->v.u->v, ps, f);
       case REP_port:
         if (!v->v.p->p) return 0; /* TODO: maybe return ps? */
         if (!IS_SET(v->v.p->wprobe.flags, WIRE_has_dep)) return 0;
         /* TODO: remove most of the below */
         l = v->v.p->wprobe.u.dep;
         while (!llist_is_empty(&l))
           { e = llist_head(&l);
             l = llist_alias_tail(&l);
             while (!IS_SET(e->flags, WIRE_action))
               { e = e->u.dep; }
             if (e->u.act->cs->ps == ps)
               { return _deadlock_find(v->v.p->p->ps, f); }
           }
       return 0;
       default:
       return 0;
     }
 }

static process_state *_deadlock_find(process_state *ps, exec_info *f)
 { int i;
   process_state *r;
   if (IS_SET(ps->flags, DBG_tmp1))
     { SET_FLAG(ps->flags, DBG_tmp2);
       return 0;
     }
   SET_FLAG(ps->flags, DBG_tmp1);
   for (i = 0; i < ps->nr_var; i++)
     { r = __deadlock_find(&ps->var[i], ps, f);
       if (r) return r;
     }
   return (IS_SET(ps->flags, DBG_tmp2))? ps : 0;
 }

static void deadlock_mark(process_state *ps, exec_info *f);

static void _deadlock_mark(value_tp *v, exec_info *f)
 /* Keep this consistent with __deadlock_find */
 { int i;
   switch (v->rep)
     { case REP_array: case REP_record:
         for (i = 0; i < v->v.l->size; i++)
           { _deadlock_mark(&v->v.l->vl[i], f); }
       return;
       case REP_union:
         _deadlock_mark(&v->v.u->v, f);
       return;
       case REP_port:
         if (!v->v.p->p) return;
         if (!IS_SET(v->v.p->wprobe.flags, WIRE_has_dep)) return;
         deadlock_mark(v->v.p->p->ps, f);
       return;
       default:
       return;
     }
 }

static void deadlock_mark(process_state *ps, exec_info *f)
 { int i;
   if (IS_SET(ps->flags, DBG_deadlock)) return;
   SET_FLAG(ps->flags, DBG_deadlock);
   for (i = 0; i < ps->nr_var; i++)
     { _deadlock_mark(&ps->var[i], f); }
 }

/* llist_func */
static int clear_dbg_tmp(process_state *ps, exec_info *f)
 { RESET_FLAG(ps->flags, DBG_tmp1 | DBG_tmp2);
   return 0;
 }

extern void deadlock_find(exec_info *f)
 /* Find a deadlock cycle in f, set f->curr to some element of the cycle */
 { llist l;
   process_state *ps, *dead_ps;
   l = f->user->procs;
   while (!llist_is_empty(&l))
     { ps = llist_head(&l);
       l = llist_alias_tail(&l);
       if (ps->nr_thread <= 0) continue;
       dead_ps = _deadlock_find(ps, f);
       llist_apply(&f->user->procs, (llist_func*)clear_dbg_tmp, f);
       if (dead_ps) break;
     }
   if (!dead_ps) return;
   f->user->curr = 0;
   set_focus_ps(dead_ps, f->user);
   f->curr = f->user->curr;
 }

/********** breakpoints ******************************************************/

extern brk_return set_breakpoint(void *obj, user_info *f)
 /* Set breakpoint at obj, or if USER_clear is set,
  * clear existing breakpoint instead.  Return BRK_stmt_err
  * if there is already a break point, or if there is already
  * no breakpoint in these respective situations.
  * The final portion of command line parsing is also done here, checking for
  * either a newline, of "if" condition followed by newline. f->cxt should
  * be set properly if this condition is present.
  */
 { parse_obj *x = obj;
   hash_entry *q;
   brk_cond *bc;
   expr *e = 0;
   f->brk_lnr = x->lnr;
   f->brk_lpos = x->lpos;
   if (IS_SET(f->flags, USER_clear))
     { if (!lex_have(f->L, TOK_nl)) return BRK_usage;
       if (!IS_SET(x->flags, DBG_break)) return BRK_stmt_err;
       if (IS_SET(x->flags, DBG_break_cond))
         { hash_delete(&f->brk_condition, obj); }
       RESET_FLAG(x->flags, DBG_break | DBG_break_cond);
     }
   else
     { if (lex_have_next(f->L, TOK_id))
         { if (f->L->prev->t.val.s != make_str("if")) return BRK_usage;
           e = parse_expr_interact(f);
           sem_interact(&e, 0, f);
         }
       if (!lex_have(f->L, TOK_nl)) return BRK_usage;
       if (IS_SET(x->flags, DBG_break)) return BRK_stmt_err;
       SET_FLAG(x->flags, DBG_break);
       if (f->ps || e)
         { SET_FLAG(x->flags, DBG_break_cond);
           NEW(bc);
           bc->e = e;
           bc->ps = f->ps;
           bc->e_alloc = parse_delay_cleanup(f->L);
           hash_insert(&f->brk_condition, obj, &q);
           q->data.p = bc;
         }
     }
   return BRK_stmt;
 }

static brk_return no_brk(parse_obj *x, user_info *f)
 /* default brk function */
 { if (x->lnr == f->brk_lnr)
     { return set_breakpoint(x, f); }
   return BRK_no_stmt;
 }

extern brk_return brkp(void *obj, user_info *f)
 /* If f->brk_lnr:f->brk_lpos is part of obj, set a breakpoint.
  * If USER_clear is set, clear existing breakpoint instead,
  */
 { return APP_OBJ_FZ(app_brk, obj, f, no_brk); }

/*
   lnr < brk_lnr => candidate, because can be multiline obj; overrides
                    previous candidate.
   lnr = brk_lnr => candidate; overrides '<' candidate, but multiple '='
                    candidates are possible.
   lnr > brk_lnr => try last candidate
*/
extern brk_return brk_list(llist *l, user_info *f)
 /* Pre: objects in l are in order.
  * Apply brk to each element of l (except more efficient).
  */
 { llist m;
   brk_return res;
   parse_obj *x, *prev = 0;
   m = *l;
   while (!llist_is_empty(&m))
     { x = llist_head(&m);
       if (x->lnr > f->brk_lnr)
         { if (prev)
             { return brkp(prev, f); }
           else
             { return BRK_no_stmt; }
         }
       else if (x->lnr == f->brk_lnr)
         { if (x->lpos > f->brk_lpos)
             /* prev might still contain an element that starts on this line */
             { if (prev && (res = brkp(prev, f)) != BRK_no_stmt) return res;
               return brkp(x, f);
             }
           prev = x;
           while (!llist_is_empty(&m))
             { x = llist_head(&m);
               if (x->lnr > f->brk_lnr || x->lpos > f->brk_lpos)
                 { return brkp(prev, f); }
               prev = x;
               m = llist_alias_tail(&m);
             }
           return brkp(prev, f);             
         }
       prev = x;
       m = llist_alias_tail(&m);
     }
   if (prev)
     { return brkp(prev, f); }
   return BRK_no_stmt;
 }

static brk_return break_nested_routine(process_def *p, user_info *f)
 /* Pre: f->L->prev is '.'; pd corresponds to f->scratch.s.
    p may also be function_def.
    Set breakpoint in routine declared inside p.
 */
 { process_def *pd;
   const str *id;
   chp_body *b;
   if (!lex_have_next(f->L, TOK_id)) return BRK_usage;
   id = f->L->prev->t.val.s;
   if (p->class == CLASS_process_def)
     { b = p->mb; }
   /* TODO: which body to search in? */
   else
     { b = ((function_def*)p)->b; }
   pd = b? llist_find(&b->dl, (llist_func*)routine_eq, id) : 0;
   if (!pd)
     { report(f, "  No such process, function, or procedure: %s%s\n",
                f->scratch.s, id);
       return BRK_error;
     }
   if (lex_have_next(f->L, '.'))
     { var_str_printf(&f->scratch, -1, "%s.", id);
       return break_nested_routine(pd, f);
     }
   if (!lex_have(f->L, TOK_nl)) return BRK_usage;
   return set_breakpoint(pd, f);
 }

extern process_def *find_routine
 (module_def *md, const str *id, user_info *f, int only)
 /* Find the routine with the specified id. If 'only', only search in md.
    Otherwise, prefer md, but also search other modules.
    Note: return may be function_def.
 */
 { process_def *pd;
   required_module *r;
   llist m;
   pd = llist_find(&md->dl, (llist_func*)routine_eq, id);
   if (!pd && !only)
     { m = md->rl;
       while (!llist_is_empty(&m))
         { r = llist_head(&m);
           pd = llist_find(&r->m->dl, (llist_func*)routine_eq, id);
           /* We could check for visibility as we did with find_const_module(),
              but it doesn't seem worth the effort here. If the user doesn't
              like the breakpoint he can always use a file argument. For
              constants, on the other hand, the user may not realize it if
              we report the wrong constant. (Note: gdb is no better.) */
           if (pd) break;
           m = llist_alias_tail(&m);
         }
       if (!pd)
         { m = f->ml;
           while (!llist_is_empty(&m))
             { md = llist_head(&m);
               pd = llist_find(&md->dl, (llist_func*)routine_eq, id);
               if (pd) break;
               m = llist_alias_tail(&m);
             }
         }
     }
   return pd;
 }

static brk_return break_routine(module_def *md, int only, user_info *f)
 /* Pre: f->L->curr is TOK_id;
    Set breakpoint in specified routine. If 'only', then only look in md;
    otherwise prefer md, but consider other modules too.
    Return 1 if set, 0 otherwise.
 */
 { process_def *pd;
   const str *id;
   lex_next(f->L);
   id = f->L->prev->t.val.s;
   pd = find_routine(md, id, f, only);
   if (!pd)
     { report(f, "  No such process, function, or procedure: %s\n", id);
       return BRK_error;
     }
   if (lex_have_next(f->L, '.'))
     { var_str_printf(&f->scratch, 0, "%s.", id);
       return break_nested_routine(pd, f);
     }
   if (!lex_have(f->L, TOK_nl)) return BRK_usage;
   return set_breakpoint(pd, f);
 }

static brk_return _cmnd_break(user_info *f)
 /* Also used by cmnd_clear, which sets USER_clear */
 { module_def *d;
   int res, only = 0;
   const str *nm;
   process_state *ps;
   f->ps = 0;
   if (lex_have(f->L, TOK_nl) ||
       (lex_have(f->L, TOK_id) && f->L->curr->t.val.s == make_str("if")))
     { f->brk_src = f->curr->obj->src;
       return set_breakpoint(f->curr->obj, f);
     }
   if (lex_have_next(f->L, TOK_instance))
     { nm = f->L->prev->t.val.s;
       f->ps = ps = find_instance(f, nm, 0);
       if (!ps || ps->nr_thread == 0)
         { report(f, "  No such process instance: %s\n", nm);
           return BRK_error;
         }
       f->brk_src = ps->p->src;
       f->cxt = ps->p->cxt;
       if (lex_have_next(f->L, TOK_int))
         { f->brk_lnr = f->L->prev->t.val.i;
           if (lex_have_next(f->L, ':'))
             { if (!lex_have_next(f->L, TOK_int)) return BRK_usage;
               f->brk_lpos = f->L->prev->t.val.i;
             }
           else
             { f->brk_lpos = -1; }
           res = brkp(ps->p, f);
           if (!res)
             { report(f, "  Process %s at %s[%d:%d] does not reach line %d",
                      ps->p->id, ps->p->src, ps->p->lnr, ps->p->lpos,
                      f->brk_lnr);
               return BRK_error;
             }
           else return res;
         }
       return set_breakpoint(ps->p, f);
     }
   if (lex_have_next(f->L, TOK_string))
     { d = find_module(f, f->L->prev->t.val.s, 0);
       if (!d)
         { report(f, "  No module \"%s\"\n", f->L->prev->t.val.s);
           free(f->L->prev->t.val.s);
           return BRK_error;
         }
       free(f->L->prev->t.val.s);
       only = 1;
     }
   else
     { d = find_module(f, f->curr->obj->src, 1);
       assert(d);
     }
   f->brk_src = d->src;
   if (lex_have(f->L, TOK_id) && f->L->curr->t.val.s != make_str("if"))
     { return break_routine(d, only, f); }
   if (!lex_have_next(f->L, TOK_int)) return BRK_usage;
   f->brk_lnr = f->L->prev->t.val.i;
   if (lex_have_next(f->L, ':'))
     { if (!lex_have_next(f->L, TOK_int)) return BRK_usage;
       f->brk_lpos = f->L->prev->t.val.i;
     }
   else
     { f->brk_lpos = -1; }
   return brkp(d, f);
 }

/* cmnd_func_tp */
static int cmnd_break(user_info *f)
 { RESET_FLAG(f->flags, USER_clear);
   switch(_cmnd_break(f))
     { case BRK_no_stmt:
         report(f, "  No statement at %s[%d]\n", f->brk_src, f->brk_lnr);
       break;
       case BRK_stmt:
         report(f, "  Breakpoint set at %s[%d:%d]\n",
                f->brk_src, f->brk_lnr, f->brk_lpos);
       break;
       case BRK_stmt_err:
         report(f, "  There is already a breakpoint at %s[%d:%d]\n",
                f->brk_src, f->brk_lnr, f->brk_lpos);
       break;
       case BRK_usage:
         report(f, "  Usage: break [if condition] -or-\n"
                   "break instance [linenr [: position]] [if condition] -or-\n"
                   "break [\"file\"] linenr [: position] [if condition] -or-\n"
                   "break [\"file\"] routine [. routine ...]\n");
       break;
       default: /* BRK_error: message already printed */
       break;
     }
   f->cxt = 0;
   return 1;
 }

static int check_brk_cond(exec_info *f)
 /* Pre: f->curr->obj is a breakpoint with a condition
  * Return whether or not the condition is true
  */
 { hash_entry *e;
   brk_cond *bc;
   value_tp cv;
   e = hash_find(&f->user->brk_condition, (char*)f->curr->obj);
   assert(e);
   bc = e->data.p;
   if (bc->ps && bc->ps != f->curr->ps) return 0;
   if (bc->e)
     { eval_expr(bc->e, f);
       pop_value(&cv, f);
       assert(cv.rep == REP_bool);
       return cv.v.i;
     }
   return 1;
 }


/********** wire commands ****************************************************/

typedef void wire_func_tp(wire_value *w, user_info *f);
 /* Execute a command with a single argument that is a wire
    No return, assume interaction should continue
 */

static int wire_cmnd(wire_func_tp *F, const str *cmd, user_info *f)
 { expr *e;
   wire_value *w;
   value_tp val;
   if (lex_have(f->L, TOK_nl))
     { report(f, "  Usage: %s expression\n", cmd);
       return 1;
     }
   e = parse_expr_interact(f);
   if (!lex_have(f->L, TOK_nl))
     { report(f, "  Usage: %s expression\n", cmd); }
   else
     { sem_interact(&e, SEM_prs, f);
       eval_interact(&val, e, f);
       if (val.rep == REP_wire)
         { w = val.v.w;
           while (IS_SET(w->flags, WIRE_forward))
             { w = w->u.w; }
           F(w, f);
         }
       else
         { report(f, "  Expression in %s must be a wire\n", cmd); }
       clear_value_tp(&val, f->global);
     }
   parse_cleanup(f->L);
   return 1;
 }

#define SET_WIRE_CMD(C) static int cmnd_ ## C(user_info *f) \
                         { return wire_cmnd(wire_ ## C, #C, f); }

/* wire_func_tp */
static void wire_watch(wire_value *w, user_info *f)
 { SET_FLAG(w->flags, WIRE_watch); }

SET_WIRE_CMD(watch)

/* hash_func */
static int emap_delete(hash_entry *q, void *dummy)
 { llist l = q->data.p;
   llist_free(&l, 0, 0);
 }

static void print_pr
(wire_expr *e, hash_table *emap, process_state *ps, print_info *f)
 /* If PR_user is set, print the inverse of the current expression. */
 { hash_entry *q;
   wire_expr *child; /* may actually be wire_value */
   llist m;
   char sep;
   int i, j, flags = f->flags;
   q = hash_find(emap, (char*)e);
   if (!q)
     { if (IS_SET(f->flags, PR_user))
         { print_char('~', f); }
       print_wire_value((wire_value*)e, ps, f);
       return;
     }
   if (!IS_SET(e->flags, WIRE_xor) &&
       ((e->valcnt != 0) ^ !IS_SET(e->flags, WIRE_value) ^
                           !IS_SET(e->flags, WIRE_val_dir)))
     { f->flags = f->flags ^ PR_user; }
   m = q->data.p;
   if (e->refcnt == 1)
     { child = llist_head(&m);
       if (IS_SET(e->flags, WIRE_xor) &&
           IS_SET(child->flags ^ e->flags, WIRE_value))
         { f->flags = f->flags ^ PR_user; }
       print_pr(child, emap, ps, f);
       f->flags = flags;
       return;
     }
   if (IS_SET(e->flags, WIRE_xor)) sep = '^';
   else if (!IS_SET(e->flags, WIRE_val_dir) ^ !IS_SET(f->flags, PR_user))
     { sep = '&'; }
   else sep = '|';
   i = 0;
   j = e->flags;
   print_char('(', f);
   while (!llist_is_empty(&m))
     { i++;
       child = llist_head(&m);
       j = j ^ child->flags;
       if (i == e->refcnt && sep == '^' && IS_SET(j, WIRE_value))
         { f->flags = f->flags ^ PR_user; }
       print_pr(child, emap, ps, f);
       if (i < e->refcnt)
         { print_char(' ', f);
           print_char(sep, f);
           print_char(' ', f);
         }
       m = llist_alias_tail(&m);
     }
   if (i < e->refcnt)
     { print_string("...", f); }
   print_char(')', f);
   f->flags = flags;
 }

/* wire_func_tp */
static void wire_fanout(wire_value *w, user_info *f)
 { hash_table emap; /* maps wire_exprs to llist of fanins */
   hash_entry *q;
   wire_expr *e, *pr;
   ctrl_state *cs;
   void *prev;
   llist m, prs; /* Unique list of pr expressions */
   print_info g;
   if (!IS_SET(w->flags, WIRE_has_dep) || llist_is_empty(&w->u.dep))
     { report(f, "  Wire has no fanouts\n");
       return;
     }
   hash_table_init(&emap, 1, HASH_ptr_is_key, (hash_func*)emap_delete);
   llist_init(&prs);
   m = w->u.dep;
   while (!llist_is_empty(&m))
     { e = llist_head(&m);
       prev = w;
       while (1)
         { if (hash_insert(&emap, (char*)e, &q))
             { llist_prepend((llist*)&q->data.p, prev);
               break;
             }
           llist_init((llist*)&q->data.p);
           llist_prepend((llist*)&q->data.p, prev);
           if (IS_SET(e->flags, WIRE_action) && ~IS_SET(e->flags, WIRE_hold))
             { llist_prepend(&prs, e);
               break;
             }
           prev = e;
           e = e->u.dep;
         }
       m = llist_alias_tail(&m);
     }
   report(f, "%d fanouts:\n", llist_size(&prs));
   g.s = &f->scratch; g.flags = 0; g.f = stdout;
   while (!llist_is_empty(&prs))
     { pr = llist_idx_extract(&prs, 0);
       if (IS_SET(pr->flags, WIRE_susp)) /* fanout to HSE */
         { cs = pr->u.act->cs;
           if (is_visible(cs->ps))
             { report(f, "fanout to %s at %s[%d:%d]:\n\t%v\n", cs->ps->nm,
                      cs->obj->src, cs->obj->lnr, cs->obj->lpos,
                      vstr_stmt, cs->obj);
             }
           else
             { report(f, "fanout to wired decomposition"); }
         }
       else /* fanout to PR */
         { g.pos = 0;
           print_pr(pr, &emap, f->curr->ps, &g);
           VAR_STR_X(g.s, g.pos) = 0;
           if (pr->u.act->cs->ps == f->curr->ps)
             { report(f, "%s -> %V%c", f->scratch.s,
                      vstr_wire, pr->u.act->target, pr->u.act->cs->ps,
                      IS_SET(pr->flags, WIRE_pu)? '+' : '-');
             }
           else
             { report(f, "%s -> %s:%V%c", f->scratch.s, pr->u.act->cs->ps->nm,
                      vstr_wire, pr->u.act->target, pr->u.act->cs->ps,
                      IS_SET(pr->flags, WIRE_pu)? '+' : '-');
             }
         }
     }
   hash_table_free(&emap);
 }

SET_WIRE_CMD(fanout)

static void _wire_fanin
(value_tp *v, hash_table *emap, wire_expr **pu, wire_expr **pd, wire_value *w)
 { int i;
   void *prev;
   hash_entry *q;
   wire_expr *e, *ee;
   llist l;
   switch (v->rep)
     { case REP_array: case REP_record:
         for (i = 0; i < v->v.l->size; i++)
           { _wire_fanin(&v->v.l->vl[i], emap, pu, pd, w); }
       return;
       case REP_wire:
         if (!IS_SET(v->v.w->flags, WIRE_has_dep)) return;
         l = v->v.w->u.dep;
       break;
       default:
       return;
     }
   while (!llist_is_empty(&l))
     { e = ee = llist_head(&l);
       l = llist_alias_tail(&l);
       while (!IS_SET(e->flags, WIRE_action))
         { e = e->u.dep; }
       if (e->u.act->target.w != w) continue;
       if (IS_SET(e->flags, WIRE_pu))
         { assert(!*pu || *pu == e); *pu = e; }
       else if (IS_SET(e->flags, WIRE_pd))
         { assert(!*pd || *pd == e); *pd = e; }
       prev = v->v.w;
       while (1)
         { if (hash_insert(emap, (char*)ee, &q))
             { llist_prepend((llist*)&q->data.p, prev);
               break;
             }
           llist_init((llist*)&q->data.p);
           llist_prepend((llist*)&q->data.p, prev);
           if (IS_SET(ee->flags, WIRE_action)) break;
           prev = ee;
           ee = ee->u.dep;
         }
     }
  }

/* wire_func_tp */
static void wire_fanin(wire_value *w, user_info *f)
 { hash_table emap; /* maps wire_exprs to llist of fanins */
   int i, nr;
   process_state *ps;
   var_decl *d;
   port_value *pv;
   wire_expr *pu = 0, *pd = 0;
   print_info g;
   char *ext;
   ps = w->wframe->cs->ps;
   if (!IS_SET(w->wframe->flags, ACTION_is_pr))
     { if (ps->b->class != CLASS_chp_body && ps->b->class != CLASS_hse_body)
         { report(f, "  Wire has no fanins\n"); }
       else if (is_visible(ps))
         { report(f, "  Wire is driven by output %V of process %s\n",
                  vstr_wire, w, ps, ps->nm);
         }
       else
         { vstr_wire(&f->scratch, 0, w, ps); /* get full wire name */
           ext = strchr(f->scratch.s, '.'); /* "Remove" local port name */
           assert(ext);
           d = llist_idx(&ps->p->pl, 0);
           if (ps->var[d->var_idx].rep != REP_port)
             { d = llist_idx(&ps->p->pl, 1); }
           assert(ps->var[d->var_idx].rep == REP_port);
           pv = ps->var[d->var_idx].v.p; /* The "real" port */
           assert(pv->dec);
           report(f, "  Wire is driven by output %v.%s%s of process %s\n",
                  vstr_port, pv->p, pv->dec->id, ext, pv->p->ps->nm);
         }
       return;
     }
   hash_table_init(&emap, 1, HASH_ptr_is_key, (hash_func*)emap_delete);
   for (i = 0; i < ps->nr_var; i++)
     { _wire_fanin(&ps->var[i], &emap, &pu, &pd, w); }
   nr = 0; if (pu) nr++; if (pd) nr++;
   if (ps == f->curr->ps)
     { report(f, "  %d fanins:", nr); }
   else
     { report(f, "  %d fanins in %s:", nr, ps->nm); }
   g.s = &f->scratch; g.flags = 0; g.f = stdout; g.pos = 0;
   if (pu)
     { print_pr(pu, &emap, ps, &g);
       VAR_STR_X(g.s, g.pos) = 0;
       report(f, "%s -> %V+", f->scratch.s, vstr_wire, w, ps);
     }
   g.pos = 0;
   if (pu)
     { print_pr(pd, &emap, ps, &g);
       VAR_STR_X(g.s, g.pos) = 0;
       report(f, "%s -> %V-", f->scratch.s, vstr_wire, w, ps);
     }
   hash_table_free(&emap);
 }

SET_WIRE_CMD(fanin)

static void wire_critical(wire_value *w, user_info *f)
 { action *a;
   crit_node *c;
   mpz_t time;
   hash_entry *q;
   if (!IS_SET(f->flags, USER_critical))
     { report(f, "  Use the -critical command line option to enable"
                 " critical cycle checking");
     }
   q = hash_find(f->global->crit_map, (char*)w);
   if (!q)
     { report(f, "  Wire has no critical cycle connection");
       return;
     }
   c = q->data.p;
   a = ACTION_NO_DIR(c->a);
   mpz_init(time);
   mpz_fdiv_q_2exp(time, f->global->time, 1);
   while (1)
     { report(f, "%s:%V %s at time %v", a->cs->ps->nm, vstr_wire, w,
              a->cs->ps, ACTION_DIR(c->a)? "down" : "up  ", vstr_mpz, time);
       mpz_sub_ui(time, time, c->delay);
       c = c->parent;
       if (!c) break;
       if (c->parent)
         { a = ACTION_NO_DIR(c->a);
           w = a->target.w;
         }
       else
         { w = c->a; }
     }
 }

SET_WIRE_CMD(critical)

/********** clear ************************************************************/

/* wire_func_tp */
static void clear_watch(wire_value *w, user_info *f)
 { RESET_FLAG(w->flags, WIRE_watch); }

/* cmnd_func_tp */
static int cmnd_clear(user_info *f)
 { int e;
   parse_obj *obj;
   if (lex_have(f->L, TOK_id))
     { if (f->L->curr->t.val.s == make_str("trace"))
         { lex_next(f->L);
           e = cmnd_flag(f, DBG_trace, 0);
           if (e == 2)
             { report(f, "  Usage: clear trace [instance]\n"); }
           return 1;
         }
       else if (f->L->curr->t.val.s == make_str("step"))
         { lex_next(f->L);
           e = cmnd_flag(f, DBG_step|DBG_next, 0);
           if (e == 2)
             { report(f, "  Usage: clear step [instance]\n"); }
           return 1;
         }
       else if (f->L->curr->t.val.s == make_str("watch"))
         { lex_next(f->L);
           return wire_cmnd(clear_watch, "clear watch", f);
         }
     }
   SET_FLAG(f->flags, USER_clear);
   switch(_cmnd_break(f))
     { case BRK_no_stmt:
         report(f, "  No statement at %s[%d]\n", f->brk_src, f->brk_lnr);
       break;
       case BRK_stmt:
         report(f, "  Breakpoint cleared at %s[%d:%d]\n",
                f->brk_src, f->brk_lnr, f->brk_lpos);
       break;
       case BRK_stmt_err:
         report(f, "  There is no breakpoint to clear at %s[%d:%d]\n",
                f->brk_src, f->brk_lnr, f->brk_lpos);
       break;
       case BRK_usage:
         report(f, "  Usage: clear -or-\n"
                   "clear instance [linenr [: position]] -or-\n"
                   "clear [\"file\"] linenr [: position] -or-\n"
                   "clear [\"file\"] routine [. routine ...] -or-\n"
                   "clear trace [instance] -or-\n"
                   "clear watch expression -or-\n"
                   "clear step [instance]\n");
       break;
       default: /* BRK_error: message already printed */
       break;
     }
   f->cxt = 0;
   return 1;
 }

/********** other commands ***************************************************/

/* llist_func */
static int _cmnd_inst(ctrl_state *cs, user_info *f)
 { if (cs != f->curr) return 0;
   sched_instance(f->curr, f->global);
   return 1;
 }

/* cmnd_func_tp */
static int cmnd_inst(user_info *f)
 { ctrl_state **cs;
   if (set_focus(f)) return 1;
   llist_find_extract(&f->wait, (llist_func*)_cmnd_inst, f);
   SET_FLAG(f->global->flags, EXEC_single);
   return 0;
 }

/* cmnd_func_tp */
static int cmnd_batch(user_info *f)
 { SET_FLAG(f->flags, USER_batch);
   if (!f->log)
     { f->log = stdout; }
   report(f, "--- continuing in batch mode -----------\n");
   return 0;
 }

/* cmnd_func_tp */
static int cmnd_source(user_info *f)
 { FILE *cmnd_file;
   const str *cmnd_fnm;
   if (!lex_have_next(f->L, TOK_string) && !lex_have_next(f->L, TOK_id))
     { report(f, "  Usage: source \"filename\"\n");
       return 2;
     }
   cmnd_fnm = f->L->prev->t.val.s;
   cmnd_file = fopen(cmnd_fnm, "r");
   if (!cmnd_file)
     { report(f, "  Could not open file %s\n", cmnd_fnm);
       return 2;
     }
   llist_prepend(&f->old_L, f->L);
   NEW(f->L);
   lex_tp_init(f->L);
   f->L->fin = cmnd_file;
   f->L->fin_nm = "cmnd_fnm";
   return 1;
 }

/* cmnd_func_tp */
static int cmnd_meta(user_info *f)
 { int i = 0;
   llist l;
   meta_parameter *mp;
   value_tp *mv, val;
   expr *e;
   if (f->main->nr_thread != -1)
     { report(f, "  Root process has already started");
       return 2;
     }
   l = f->main->p->ml;
   while (!llist_is_empty(&l))
     { if (lex_have(f->L, TOK_nl))
         { report(f, "  Expected %d parameters, found only %d",
                  i + llist_size(&l), i);
           clear_value_tp(&f->main->meta[0], f->global);
           return 2;
         }
       mp = llist_head(&l);
       mv = &f->main->meta[mp->meta_idx];
       if (mv->rep)
         { clear_value_tp(mv, f->global); }
       e = parse_expr_interact(f);
       SET_FLAG(f->flags, USER_global);
       sem_interact(&e, 0, f);
       if (!type_compatible(&mp->tp, &e->tp))
         { report(f, "  Type of meta parameter %s does not match value %v",
                  mp->id, vstr_obj, e);
           RESET_FLAG(f->flags, USER_global);
           f->cxt = 0;
           parse_cleanup(f->L);
           return 2;
         }
       eval_interact(&val, e, f);
       RESET_FLAG(f->flags, USER_global);
       copy_and_clear(mv, &val, f->global);
       parse_cleanup(f->L);
       l = llist_alias_tail(&l); i++;
       if (lex_have_next(f->L, ','))
         { if (llist_is_empty(&l) && lex_have(f->L, TOK_nl))
             { report(f, "  Warning: ignoring trailing comma"); }
         }
       else if (!lex_have(f->L, TOK_nl))
         { clear_value_tp(&f->main->meta[0], f->global);
           lex_must_be(f->L, ',');
         }
     }
   if (!lex_have(f->L, TOK_nl))
     { if (i == 0)
         { report(f, "  Root process has no meta parameters to set"); }
       else
         { report(f, "  Root process has only %d meta parameter%s",
                  i, i>1? "s" : "");
           clear_value_tp(&f->main->meta[0], f->global);
         }
       return 2;
     }
   return 1;
 }


/********** commands *********************************************************/

typedef struct cmnd_entry
   { const str *cmnd;
     const char *prefix;
     cmnd_func_tp *f;
     const char *help; /* if start with !, then not listed by default */
   } cmnd_entry;

/* cmnd_func_tp */
static int cmnd_help(user_info *f);

static cmnd_entry cmnd_list[] =
   { { "step", "s", cmnd_step, "step [instance] - take one step" },
     { "next", "n", cmnd_next,
          "next [instance] - take one step (call is one step)" },
     { "continue", "c", cmnd_continue, "- continue execution (a.k.a. 'run')" },
     { "run", "r", cmnd_continue, "!- same as continue" },
     { "instantiate", "i", cmnd_inst,
          "instantiate [instance] - finish current process" },
     { "meta", "m", cmnd_meta,
          "meta expr [, expr ...] - set meta parameters of the root process" },
     { "trace", "t", cmnd_trace,
                "trace [instance] - trace process execution" },
     { "watch", "w", cmnd_watch,
                "watch expression - show all transitions of the given wire" },
     { "break", "b", cmnd_break,
             "break [if condition] - set breakpoint at current statement\n\t\t"
             "break instance [linenr [: position]] [if condition]:\n\t\t\t"
                " set breakpoint in a single instance\n\t\t"
             "break [\"file\"] linenr [: position] [if condition]:\n\t\t\t"
                " set breakpoint across all instances\n\t\t"
             "break [\"file\"] routine [. routine ...]:\n\t\t\t"
                " set breakpoint at the beginning of a routine" },
     { "clear", "cl", cmnd_clear,
             "clear - clear breakpoint at current statement\n\t\t"
             "clear instance [linenr [: position]]:\n\t\t\t"
                " clear breakpoint by instance\n\t\t"
             "clear [\"file\"] linenr [: position]:\n\t\t\t"
                " clear breakpoint by module name\n\t\t"
             "clear [\"file\"] routine [. routine ...]:\n\t\t\t"
                " clear breakpoint at the beginning of a routine\n\t\t"
             "clear trace [instance] - stop tracing\n\t\t"
             "clear watch expression - stop watching\n\t\t"
             "clear step [instance] - clear step/next status" },
     { "print", "p", cmnd_show,
                "print - show current threads (a.k.a. 'show')\n\t\t"
                "print instance - show connections and position\n\t\t"
                "print expression - evaluate the expression" },
     { "show", "sh", cmnd_show, "!- same as print" },
     { "fanout", "fano", cmnd_fanout, "! - display fanout production rules" },
     { "fanin", "fani", cmnd_fanin, "! - display fanin production rules" },
     { "critical", "cr", cmnd_critical, "! - list critical transitions" },
     { "where", "wh", cmnd_where, "- show call stack" },
     { "up", "u", cmnd_up,
                "\tup [int] - move up the call stack (towards the caller)" },
     { "down", "d", cmnd_down, "down [int] - move down the call stack" },
     { "view", "v", cmnd_view,
                "view [instance] - change current process (a.k.a. 'focus')" },
     { "focus", "f", cmnd_view, "!- same as view" },
     { "source", "so", cmnd_source,
                 "source \"filename\" - execute commands from file" },
     { "batch", "bat", cmnd_batch, "- continue without interaction" },
     { "quit", "q", cmnd_quit, "- quit the program (a.k.a. 'exit')" },
     { "exit", "ex", cmnd_quit, "!- same as quit" },
     { "x", 0, cmnd_quit, "!\t- same as quit" },
     { "help", "h", cmnd_help, "!help [command] - command summary" }
   };

static cmnd_entry *find_cmnd(const str *cmnd)
 /* Find command */
 { int i;
   cmnd_entry *e;
   for (i = 0; i < CONST_ARRAY_SIZE(cmnd_list); i++)
     { if (cmnd_list[i].cmnd == cmnd)
         { return &cmnd_list[i]; }
     }
   for (i = 0; i < CONST_ARRAY_SIZE(cmnd_list); i++)
     { e = &cmnd_list[i];
       if (e->prefix && is_prefix(e->prefix, cmnd) && is_prefix(cmnd, e->cmnd))
         { return e; }
     }
   return 0;
 }

/* cmnd_func_tp */
static int cmnd_help(user_info *f)
 { int i;
   cmnd_entry *e;
   if (lex_have_next(f->L, TOK_id))
     { e = find_cmnd(f->L->prev->t.val.s);
       if (e)
         { report(f, "  %s: \t%s\n", e->cmnd,
                   (*e->help != '!')? e->help : e->help+1);
           return 1;
         }
     }
   for (i = 0; i < CONST_ARRAY_SIZE(cmnd_list); i++)
     { e = &cmnd_list[i];
       if (*e->help != '!')
         { report(f, "  %s: \t%s\n", e->cmnd, e->help); }
     }
   report(f, "  <ctrl-C>: \tbreak immediately\n");
   report(f, "  Commands may be abbreviated\n");
   return 1;
 }


/********** user interaction *************************************************/

extern void report(user_info *f, const char *fmt, ...)
 /* report information to user (adds newline if not present) */
 { int pos;
   va_list a;
   va_start(a, fmt);
   pos = var_str_vprintf(&f->rep, 0, fmt, a);
   if (f->limit > 0 && pos > f->limit)
     { pos = var_str_printf(&f->rep, f->limit, "...\n"); }
   if (f->rep.s[pos-1] != '\n')
     { f->rep.s[pos++] = '\n';
       VAR_STR_X(&f->rep, pos) = 0;
     }
   va_end(a);
   if (f->log)
     { fprintf(f->log, "%s", f->rep.s); }
   if (!IS_SET(f->flags, USER_batch))
     { fprintf(stdout, "%s", f->rep.s); }
 }

static token_tp fake_prompt(user_info *f, const char *prompt)
 /* Do not take input from stdin or a source file, but from f->cmds */
 { int pos = 0;
   llist curr_cmd = llist_idx_extract(&f->cmds, 0);
   while(!llist_is_empty(&curr_cmd))
     { pos += var_str_printf(&f->L->line, pos, "%s ",
                     llist_idx_extract(&curr_cmd, 0));
     }
   var_str_printf(&f->L->line, pos, "\n");
   f->L->c = f->L->line.s;
   SET_FLAG(f->L->flags, LEX_cmnd);
   lex_next(f->L);
   if (f->log)
     { fprintf(f->log, "%s%s", prompt, f->L->line.s); }
   fprintf(stdout, "%s%s", prompt, f->L->line.s);
   return f->L->curr->t.tp;
 }

extern token_tp prompt_user(user_info *f, const char *prompt)
 /* Write prompt, read an input line from user; return first token
    No parsing is done, so don't mess with f->L->alloc_list
  */
 { token_tp t;
   jmp_buf err_jmp, *orig_err_jmp = f->L->err_jmp;
   f->L->err_jmp = &err_jmp;
   setjmp(err_jmp);
   if (!llist_is_empty(&f->cmds) && llist_is_empty(&f->old_L))
     { return fake_prompt(f, prompt); }
   else if (IS_SET(f->flags, USER_quit))
     { exit(0); }
   fprintf(stdout, "%s", prompt);
   fflush(stdout);
   t = lex_start_cmnd(f->L);
   if (f->log)
     { fprintf(f->log, "%s%s", prompt, f->L->line.s); }
   if (f->old_L) /* source file, not stdin */
     { fprintf(stdout, "%s", f->L->line.s); }
   f->L->err_jmp = orig_err_jmp;
   return t;
 }

extern void interact_report(exec_info *f)
 /* Report current position, then interact. */
 { parse_obj *x = f->curr->obj;
   const char *reason = "step", *cmnd = "continue", *rep = 0;
   /* reason: most prominent reason why we stopped
      cmnd: default command
   */
   if (exec_interrupted)
     { reason = "ctrl-C"; }
   else if (IS_SET(f->curr->obj->flags, DBG_break))
     { if (IS_SET(f->curr->obj->flags, DBG_break_cond) &&
           !check_brk_cond(f)) return;
       rep = reason = "break";
     }
   else if (IS_SET(f->curr->ps->flags, DBG_next))
     { reason = "next"; }
   /* if we are stepping through a routine and happen to pass a breakpoint,
      the default cmnd should still be step, even though we report the break.
   */
   if (IS_SET(f->curr->ps->flags, DBG_next))
     { rep = cmnd = "next"; }
   else if (IS_SET(f->curr->ps->flags, DBG_step))
     { rep = cmnd = "step"; }
   report(f->user, "(%s) %s at %s[%d:%d]\n\t%v\n", reason,
          f->curr->ps->nm, x->src, x->lnr, x->lpos, vstr_stmt, x);
   RESET_FLAG(f->curr->ps->flags, DBG_step|DBG_next);
   if (!IS_SET(f->user->flags, USER_batch) || exec_interrupted)
     { interact(f, make_str(cmnd)); }
 }

extern void interact(exec_info *g, const str *deflt)
 /* Pre: g has at least one thread.
    Accept and interpret user commands, until execution should continue.
    deflt is default command. Note: this sets the focus to g->curr, so
    should not be called recursively.
 */
 { const str *cmnd;
   cmnd_entry *e;
   token_tp t;
   user_info *f = g->user;
   jmp_buf err_jmp, *orig_err_jmp = f->L->err_jmp;
   if (IS_SET(f->flags, USER_batch) && !exec_interrupted)
     { return; }
   f->global = g;
   if (!init_focus(f, g))
     { report(f, "No threads left.\n"); exit(0); }
   if (IS_SET(f->flags, USER_batch) && exec_interrupted)
     { SET_FLAG(g->flags, EXEC_error);
       cmnd_quit(f);
     }
   exec_interrupted = 0;
   do { t = prompt_user(f, "(cmnd?) ");
        e = 0;
        f->L->err_jmp = &err_jmp;
        llist_init(&f->L->alloc_list);
        if (setjmp(err_jmp))
          { parse_cleanup(f->L);
            f->L->flags = LEX_cmnd;
            RESET_FLAG(f->flags, USER_global);
            e = 0;
            continue;
          }
        if (t == TOK_eof && !llist_is_empty(&f->old_L))
          { lex_tp_term(f->L); free(f->L);
            f->L = llist_idx_extract(&f->old_L, 0);
            continue;
          }
        else if (t == TOK_eof && feof(f->L->fin))
          /* TOK_eof also happens with read errors, especially ctrl-C */
          { cmnd = make_str("batch"); }
        else if (t == TOK_nl || t == TOK_eof)
          { f->L->curr->t.tp = TOK_nl; cmnd = deflt; }
        else if (!lex_have(f->L, TOK_id))
          { report(f, "  Expected a command\n");
            continue;
          }
        else
          { lex_must_be(f->L, TOK_id);
            cmnd = f->L->prev->t.val.s;
          }
        e = find_cmnd(cmnd);
        if (!e)
          { report(f, "  Unknown command: %s\n", cmnd);
            continue;
          }
   } while (!e || e->f(f));
   f->L->err_jmp = orig_err_jmp;
 }

/********** interrupts *******************************************************/

#ifndef NO_SIGNALS

static void ctrl_c(int sig)
 /* stop at next statement */
 { signal(SIGINT, ctrl_c);
   exec_interrupted = 1;
 }

#endif /* NO_SIGNALS */


/********** start of simulation **********************************************/

/* llist_func */
static int process_not_started(process_state *ps, exec_info *f)
 /* if ps has not started, free it and return 1; otherwise return 0 */
 { if (!ps->nr_thread)
     { report(f->user, "(warning) no process instance %s\n", ps->nm);
       ps->refcnt--;
       free_process_state(ps, f);
       return 1;
     }
   return 0;
 }

extern void interact_instantiate(exec_info *f)
 /* Pre: f->curr is state for initial process (not yet scheduled).
    Run instantiation phase (including global constants)
 */
 { process_state *main = f->user->main;
   llist ml;
   meta_parameter *mp;
   int ready;
#ifndef NO_SIGNALS
   signal(SIGINT, ctrl_c);
#endif /* NO_SIGNALS */
   report(f->user, "--- global constants -------------------\n");
   exec_modules(&f->user->ml, f);
   llist_prepend(&f->user->wait, f->curr);
   f->curr = 0; /* f->curr was needed for exec_modules */
   report(f->user, "--- instantiation ----------------------\n");
   while (1)
     { interact(f, make_str("continue"));
       ml = main->p->ml;
       ready = 1;
       while (!llist_is_empty(&ml))
         { mp = llist_head(&ml);
           ml = llist_alias_tail(&ml);
           if (!main->meta[mp->meta_idx].rep)
             { report(f->user, "  Main process has unknown meta parameters"
                               " (use 'meta' command)");
               if (IS_SET(f->user->flags, USER_batch)) exit(1);
               ready = 0;
               break;
             }
         }
       if (ready) break;
     }
   exec_run(f);
 }

extern void interact_chp(exec_info *f)
 /* Run chp execution phase */
 { report(f->user, "--- CHP execution ----------------------\n");
   llist_all_extract(&f->user->procs, (llist_func*)process_not_started, f);
   if (!pqueue_root(&f->sched))
     { report(f->user, "  (nothing to execute)\n"); }
   else
     { interact(f, make_str("continue"));
       exec_run(f);
     }
 }

/********** search path ******************************************************/

extern void set_default_path(user_info *f)
 /* Append default search path for required modules. */
 { 
#define PATH(D) llist_append(&f->path, strdup(D));
#include "path.def"
 }

extern void add_path(user_info *f, const char *d)
 /* add (copy of) d to the search path */
 { int pos;
   pos = var_str_copy(&f->scratch, d) - 1;
   while (pos > 1 && f->scratch.s[pos] == '/')
     { f->scratch.s[pos] = 0;
       pos--;
     }
   llist_append(&f->path, strdup(f->scratch.s));
 }

/* llist_func */
static int free_path_entry(char *s, user_info *f)
 /* free path entry s */
 { free(s);
   return 0;
 }

extern void reset_path(user_info *f)
 /* Reset the module search path (to empty) */
 { llist_free(&f->path, (llist_func*)free_path_entry, f);
 }

extern void show_path(user_info *f)
 /* print search path */
 { llist m;
   report(f, "--- module search path -----------------\n");
   m = f->path;
   while (!llist_is_empty(&m))
     { report(f, "  %s", llist_head(&m));
       m = llist_alias_tail(&m);
     }
   report(f, "--- end module search path -------------\n");
 }


/********** startup **********************************************************/

extern void init_interact(int app)
 /* call at startup; pass unused object app index */
 { int i;
   for (i = 0; i < CONST_ARRAY_SIZE(cmnd_list); i++)
     { cmnd_list[i].cmnd = make_str(cmnd_list[i].cmnd); }
   app_brk = app;
 }
