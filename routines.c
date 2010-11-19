/* routines.c: routines; builtin procedures

   Author: Marcel van der Goot
*/

#include <standard.h>
#include <gmp.h> /* mpz */
#include "print.h"
#include "parse_obj.h"
#include "routines.h"
#include "sem_analysis.h"
#include "exec.h"
#include "expr.h"
#include "interact.h"
#include "types.h"

/********** printing *********************************************************/

static void print_chp_body(chp_body *x, print_info *f)
 { if (IS_SET(f->flags, PR_short))
     { if (x->class == CLASS_meta_body)
         { print_string("META { ... }", f); }
       else if (x->class == CLASS_hse_body)
         { print_string("HSE { ... }", f); }
       else if (x->class == CLASS_prs_body)
         { print_string("PRS { ... }", f); }
       else
         { print_string("CHP { ... }", f); }
       return;
     }
   else if (IS_SET(f->flags, PR_prs))
     { if (x->class != CLASS_prs_body) return;
         { print_string("prs {\n", f); }
       if (!llist_is_empty(&x->dl))
         { print_obj_list(&x->dl, f, "\n");
           print_char('\n', f);
         }
       if (IS_SET(f->flags, PR_simple_var))
         { SET_FLAG(f->exec->flags, EXEC_print);
           exec_immediate(&x->sl, f->exec);
           RESET_FLAG(f->exec->flags, EXEC_print);
         }
       else
         { print_obj_list(&x->sl, f, "\n");
           print_char('\n', f);
         }
       print_string("}\n", f);
       return;
     }
   else if (IS_SET(f->flags, PR_meta))
     { if (x->class != CLASS_meta_body) return;
         { print_string("meta {\n", f); }
       if (IS_SET(f->flags, PR_simple_var))
         { SET_FLAG(f->exec->flags, EXEC_print);
           exec_immediate(&x->dl, f->exec);
           exec_immediate(&x->sl, f->exec);
           RESET_FLAG(f->exec->flags, EXEC_print);
         }
       else
         { if (!llist_is_empty(&x->dl))
             { print_obj_list(&x->dl, f, "\n");
               print_char('\n', f);
             }
           print_obj_list(&x->sl, f, ";\n");
           print_string(";\n", f);
         }
       print_string("}\n", f);
       return;
     }
   if (x->class == CLASS_meta_body)
     { print_string("META\n { ", f); }
   else if (x->class == CLASS_hse_body)
     { print_string("HSE\n { ", f); }
   else if (x->class == CLASS_prs_body)
     { print_string("PRS\n { ", f); }
   else
     { print_string("CHP\n { ", f); }
   if (!llist_is_empty(&x->dl))
     { print_obj_list(&x->dl, f, "\n   ");
       print_char('\n', f);
     }
   print_obj_list(&x->sl, f, ";\n   ");
   print_string("\n }\n", f);
 }

static void print_function_def(function_def *x, print_info *f)
 { if (x->ret)
     { f->pos += var_str_printf(f->s, f->pos, "function %s(", x->id); }
   else
     { f->pos += var_str_printf(f->s, f->pos, "procedure %s(", x->id); }
   print_obj_list(&x->pl, f, "; ");
   if (x->ret)
     { print_string("): ", f);
       print_obj(x->ret->tps, f);
     }
   else
     { print_char(')', f); }
   if (IS_SET(f->flags, PR_short)) return;
   print_char('\n', f);
   if (IS_SET(x->flags, DEF_builtin))
     { print_string("BUILTIN", f); }
   else
     { print_obj(x->b, f); }
 }

static void print_process_def(process_def *x, print_info *f)
 { if (IS_SET(f->flags, PR_cast))
     { f->pos += var_str_printf(f->s, f->pos, "define %s(", x->id); }
   else
     { f->pos += var_str_printf(f->s, f->pos, "process %s(", x->id); }
   print_obj_list(&x->ml, f, "; ");
   if (IS_SET(f->flags, PR_short))
     { print_string(")(", f);
       print_obj_list(&x->pl, f, "; ");
       print_char(')', f);
       return;
     }
   print_string(")\n\t(", f);
   print_obj_list(&x->pl, f, "; ");
   print_string(")\n", f);
   if (IS_SET(x->flags, DEF_builtin))
     { print_string("BUILTIN", f); }
   else
     { if (x->mb) print_obj(x->mb, f);
       if (x->cb) print_obj(x->cb, f);
       if (x->hb) print_obj(x->hb, f);
       if (x->pb) print_obj(x->pb, f);
     }
 }


/********** semantic analysis ************************************************/

static void get_builtin_func(function_def *x, sem_info *f);

static void *sem_function_def(function_def *x, sem_info *f)
 { sem_flags flags;
   declare_id(f, x->id, x);
   if (f->curr_routine)
     { x->parent = f->curr_routine; }
   else
     { x->parent = f->curr; }
   enter_level(x, &x->cxt, f);
   flags = f->flags;
   SET_FLAG(f->flags, SEM_func_def);
   f->curr_routine = x;
   if (x->ret)
     { /* declare x again, this time inside the function, so that it
          cannot be redeclared, which would make return value inaccessible
       */
       if (!IS_SET(x->flags, DEF_forward))
         { x->ret->var_idx = f->var_idx++; }
       declare_id(f, x->id, x);
       sem_list(&x->pl, f);
       x->ret->tps = sem(x->ret->tps, f);
       if (!IS_SET(x->flags, DEF_forward))
         { x->ret->tp = x->ret->tps->tp; }
     }
   else
     { sem_list(&x->pl, f); }
   if (IS_SET(x->flags, DEF_forward)) /* 2nd pass */
     { f->var_idx = x->nr_var; /* value at this point after 1st pass */
       if (IS_SET(x->flags, DEF_builtin))
         { get_builtin_func(x, f); }
       else
         { x->b = sem(x->b, f);
           x->b = sem(x->b, f);
         }
     }
   SET_FLAG(x->flags, DEF_forward);
   f->flags = flags;
   x->nr_var = f->var_idx;
   leave_level(f);
   return x;
 }

static void *sem_chp_body(chp_body *x, sem_info *f)
 { sem_flags bflags, flags = f->flags;
   if (x->class == CLASS_meta_body)
     { bflags = SEM_meta; }
   else if (x->class == CLASS_prs_body)
     { bflags = SEM_prs; }
   else if (x->class == CLASS_delay_body)
     { bflags = SEM_prs | SEM_delay; }
   else
     { bflags = 0; }
   ASSIGN_FLAG(f->flags, bflags, SEM_body);
   sem_list(&x->dl, f);
   sem_stmt_list(&x->sl, f);
   SET_FLAG(x->flags, DEF_forward);
   f->flags = flags;
   return x;
 }

static void *sem_process_body(process_def *x, void *b, sem_info *f)
/* Just a helper function for sem_process_def */
 { chp_body *cb = b;
   f->var_idx = x->nr_var; /* value at this point after 1st pass */
   f->meta_idx = x->nr_meta; /* value at this point after 1st pass */
   enter_body(x, &cb->cxt, f);
   b = sem(b, f);
   b = sem(b, f);
   if (f->var_idx > x->nr_var) x->nr_var = f->var_idx;
   if (f->meta_idx > x->nr_meta) x->nr_meta = f->meta_idx;
   leave_level(f);
   return b;
 }

static void *sem_process_def(process_def *x, sem_info *f)
 { int meta_idx;
   sem_flags flags;
   if (IS_SET(f->flags, SEM_func_def))
     { sem_error(f, x, "Cannot nest a process inside a function/procedure"); }
   declare_id(f, x->id, x);
   meta_idx = f->meta_idx;
   if (f->curr_routine)
     { x->parent = f->curr_routine; }
   else
     { x->parent = f->curr; }
   enter_level(x, &x->cxt, f);
   flags = f->flags;
   f->curr_routine = x;
   if (IS_SET(x->flags, DEF_forward)) /* 2nd pass */
     { if (x->mb) x->mb = sem_process_body(x, x->mb, f);
       if (x->cb) x->cb = sem_process_body(x, x->cb, f);
       if (x->hb) x->hb = sem_process_body(x, x->hb, f);
       if (x->pb) x->pb = sem_process_body(x, x->pb, f);
       if (x->db) x->db = sem_process_body(x, x->db, f);
     }
   else
     { sem_list(&x->ml, f);
       sem_list(&x->pl, f);
       x->meta_offset = meta_idx;
       x->nr_var = f->var_idx;
       x->nr_meta = f->meta_idx;
     }
   SET_FLAG(x->flags, DEF_forward);
   f->flags = flags;
   leave_level(f);
   f->meta_idx = meta_idx; /* not restored by leave_level() */
   return x;
 }

static void import_function_def(function_def *x, sem_info *f)
 { assert(IS_SET(x->flags, DEF_forward));
   declare_id(f, x->id, x);
 }

static void import_process_def(process_def *x, sem_info *f)
 { assert(IS_SET(x->flags, DEF_forward));
   declare_id(f, x->id, x);
 }


/********** execution ********************************************************/

static int exec_function_def(function_def *x, exec_info *f)
 { ctrl_state *s;
   exec_builtin_func *bif;
   if (IS_SET(x->flags, DEF_builtin))
     { bif = (exec_builtin_func*)x->b;
       bif(x, f);
       return EXEC_next;
     }
   exec_immediate(&x->b->dl, f);
   if (llist_is_empty(&x->b->sl))
     { return EXEC_next; }
   s = nested_seq(&x->b->sl, f);
   insert_sched(s, f);
   return EXEC_none;
 }

static void check_new_wires(value_tp *v, type *tp, exec_info *f)
 { array_type *atps;
   value_tp idxv;
   wire_value *w;
   long i;
   int pos = VAR_STR_LEN(&f->scratch);
   if (v->rep == REP_array)
     { assert(tp->kind == TP_array);
       atps = (array_type*)tp->tps;
       eval_expr(atps->l, f);
       pop_value(&idxv, f);
       for (i = 0; i < v->v.l->size; i++)
         { var_str_printf(&f->scratch, pos, "[%v]", vstr_val, &idxv);
           check_new_wires(&v->v.l->vl[i], tp->elem.tp, f);
           int_inc(&idxv, f);
         }
       clear_value_tp(&idxv, f);
     }
   else
     { assert(v->rep == REP_wire);
       wire_fix(&v->v.w, f);
       w = v->v.w;
       if (!IS_SET(w->flags, WIRE_has_writer))
         { exec_error(f, f->curr->obj, "%s)", f->scratch.s); }
     }
 }

static void check_new_ports(value_tp *v, type *tp, exec_info *f)
/* checks for dangling ports in newly created processes */
 { llist l, rl;
   long i, j, k;
   array_type *atps;
   value_tp idxv;
   record_type *rtps;
   record_field *rf;
   wired_type *wtps;
   wire_decl *wd;
   int pos = VAR_STR_LEN(&f->scratch);
   process_state *meta_ps;
   meta_parameter *mp;
   type_value *tpv;
   if (tp && tp->kind == TP_generic)
     { mp = (meta_parameter*)tp->tps;
       assert(mp->class == CLASS_meta_parameter);
       meta_ps = f->meta_ps;
       tpv = meta_ps->meta[mp->meta_idx].v.tp;
       f->meta_ps = tpv->meta_ps;
       check_new_ports(v, &tpv->tp, f);
       f->meta_ps = meta_ps;
       return;
     }
   switch(v->rep)
     { case REP_record:
         if (tp->kind == TP_wire)
           { wtps = (wired_type*)tp->tps;
             l = wtps->li;
             pos += var_str_printf(&f->scratch, pos, " not connected\n"
                                   "(No process writes to wire ");
             for (i = 0; i < v->v.l->size; i++)
               { wd = (wire_decl*)llist_head(&l);
                 var_str_printf(&f->scratch, pos, "%s", wd->id);
                 check_new_wires(&v->v.l->vl[i], &wd->tps->tp, f);
                 l = llist_alias_tail(&l);
                 if (llist_is_empty(&l)) l = wtps->lo;
               }
           }
         else
           { l = tp->elem.l;
             rtps = (record_type*)tp->tps;
             rl = rtps->l;
             for (i = 0; i < v->v.l->size; i++)
               { rf = (record_field*)llist_head(&rl);
                 var_str_printf(&f->scratch, pos, ".%s", rf->id);
                 check_new_ports(&v->v.l->vl[i], llist_head(&l), f);
                 l = llist_alias_tail(&l);
                 rl = llist_alias_tail(&rl);
               }
           }
       return;
       case REP_union:
         check_new_ports(&v->v.u->v, &v->v.u->f->tp, f);
       return;
       case REP_port:
       return;
       case REP_array:
         if (tp->kind == TP_array)
           { atps = (array_type*)tp->tps;
             eval_expr(atps->l, f);
             pop_value(&idxv, f);
           }
         else
           { idxv.rep = REP_int; idxv.v.i = 0; }
         for (i = 0; i < v->v.l->size; i++)
           { var_str_printf(&f->scratch, pos, "[%v]", vstr_val, &idxv);
             check_new_ports(&v->v.l->vl[i], tp->elem.tp, f);
             int_inc(&idxv, f);
           }
         clear_value_tp(&idxv, f);
       return;
       default:
         exec_error(f, f->curr->obj, "%s is not connected", f->scratch.s);
       return;
     }
 }

static void check_old_wires(value_tp *v, type *tp, exec_info *f)
 { array_type *atps;
   value_tp idxv;
   wire_value *w;
   long i;
   int pos = VAR_STR_LEN(&f->scratch);
   if (v->rep == REP_array)
     { assert(tp->kind == TP_array);
       atps = (array_type*)tp->tps;
       eval_expr(atps->l, f);
       pop_value(&idxv, f);
       for (i = 0; i < v->v.l->size; i++)
         { var_str_printf(&f->scratch, pos, "[%v]", vstr_val, &idxv);
           check_old_wires(&v->v.l->vl[i], tp->elem.tp, f);
           int_inc(&idxv, f);
         }
       clear_value_tp(&idxv, f);
     }
   else
     { assert(v->rep == REP_wire);
       wire_fix(&v->v.w, f);
       w = v->v.w;
       if (w->wframe->cs->ps == f->curr->ps)
         { exec_error(f, f->curr->obj, "%s)", f->scratch.s); }
     }
 }

static void check_old_ports(value_tp *v, type *tp, exec_info *f)
/* checks for dangling ports of ending meta processes
   Not called for terminating chp processes
 */
 { llist l, rl;
   long i, j, k;
   array_type *atps;
   value_tp idxv;
   record_type *rtps;
   record_field *rf;
   wired_type *wtps;
   wire_decl *wd;
   port_value *p;
   int pos = VAR_STR_LEN(&f->scratch);
   process_state *meta_ps;
   meta_parameter *mp;
   type_value *tpv;
   if (tp && tp->kind == TP_generic)
     { mp = (meta_parameter*)tp->tps;
       assert(mp->class == CLASS_meta_parameter);
       meta_ps = f->meta_ps;
       tpv = meta_ps->meta[mp->meta_idx].v.tp;
       f->meta_ps = tpv->meta_ps;
       check_old_ports(v, &tpv->tp, f);
       f->meta_ps = meta_ps;
       return;
     }
   switch(v->rep)
     { case REP_record:
         if (tp->kind == TP_wire)
           { wtps = (wired_type*)tp->tps;
             pos += var_str_printf(&f->scratch, pos, " not connected\n"
                                   "(No process writes to wire ");
             if (LI_IS_WRITE(f->curr->i, wtps->type))
               { l = wtps->li; /* only check output wires */
                 i = 0;
               }
             else
               { l = wtps->lo;
                 i = llist_size(&wtps->li);
               }
             while (!llist_is_empty(&l))
               { wd = (wire_decl*)llist_head(&l);
                 var_str_printf(&f->scratch, pos, "%s", wd->id);
                 check_old_wires(&v->v.l->vl[i], &wd->tps->tp, f);
                 l = llist_alias_tail(&l);
                 i++;
               }
           }
         else
           { l = tp->elem.l;
             rtps = (record_type*)tp->tps;
             rl = rtps->l;
             for (i = 0; i < v->v.l->size; i++)
               { rf = (record_field*)llist_head(&rl);
                 var_str_printf(&f->scratch, pos, ".%s", rf->id);
                 check_old_ports(&v->v.l->vl[i], llist_head(&l), f);
                 rl = llist_alias_tail(&rl);
                 l = llist_alias_tail(&l);
               }
           }
       return;
       case REP_union:
         check_old_ports(&v->v.u->v, &v->v.u->f->tp, f);
       return;
       case REP_port:
         p = v->v.p;
         if (p->p)
           { exec_error(f, f->curr->obj, "%s not connected", f->scratch.s); }
       return;
       case REP_array:
         if (tp->kind == TP_array)
           { atps = (array_type*)tp->tps;
             eval_expr(atps->l, f);
             pop_value(&idxv, f);
           }
         else
           { idxv.rep = REP_int; idxv.v.i = 0; }
         for (i = 0; i < v->v.l->size; i++)
           { var_str_printf(&f->scratch, pos, "[%v]", vstr_val, &idxv);
             check_old_ports(&v->v.l->vl[i], tp->elem.tp, f);
             int_inc(&idxv, f);
           }
         clear_value_tp(&idxv, f);
       return;
       default:
       return;
     }
 }

static void sched_instance_all(value_tp *v, exec_info *f)
 /* Schedule any process instances contained in v. */
 { int i;
   llist l;
   process_state *ps, *meta_ps = f->meta_ps;
   meta_parameter *mp;
   var_decl *d;
   if (v->rep == REP_array)
     { for (i = 0; i < v->v.l->size; i++)
         { sched_instance_all(&v->v.l->vl[i], f); }
     }
   else if (v->rep == REP_process)
     { ps = v->v.ps;
       ps->refcnt++; /* For the new process itself. The instantiator
                        cleans its ref to ps when cleaning its var[]; the new
                        process cleans ps in pop_process_def().
                      */
       for (i = 0; i < llist_size(&ps->p->ml); i++)
         { if (!ps->meta[i+ps->p->meta_offset].rep)
             { mp = llist_idx(&ps->p->ml, i);
               exec_error(f, f->curr->ps->p,
                      "Instance %s has unknown meta parameter %s",
                      ps->nm, mp->id);
             }
         }
       l = ps->p->pl;
       f->meta_ps = ps;
       while (!llist_is_empty(&l))
         { d = llist_head(&l);
           var_str_printf(&f->scratch, 0, "In process %s, port %s",
                          ps->nm, d->id);
           check_new_ports(&ps->var[d->var_idx], &d->tp, f);
           l = llist_alias_tail(&l);
         }
       f->meta_ps = meta_ps;
       sched_instance(ps->cs, f);
     }
 }

static int pop_process_def(process_def *x, exec_info *f);

static int exec_process_def(process_def *x, exec_info *f)
 { ctrl_state *s;
   llist l;
   var_decl *d;
   chp_body *b; /* or meta_body, ... */
   assert(f->curr->ps->nr_thread == 1);
   b = f->curr->ps->b;
   if (!IS_SET(f->flags, EXEC_instantiation))
     { l = x->pl; /* Run checks again to remove wire forwards */
       while (!llist_is_empty(&l))
         { d = llist_head(&l);
           VAR_STR_X(&f->scratch, 0) = 0;
           check_new_ports(&f->curr->var[d->var_idx], &d->tp, f);
           l = llist_alias_tail(&l);
         }
     }
   assert(b);
   exec_immediate(&b->dl, f);
   if (b == x->pb)
     { exec_immediate(&b->sl, f); }
   else
     { if (llist_is_empty(&b->sl))
         { return pop_process_def(x, f); /* no nested stmts */ }
       s = nested_seq(&b->sl, f);
       insert_sched(s, f);
     }
   if (x->db && !IS_SET(f->user->flags, USER_random))
     { exec_immediate(&x->db->dl, f);
       exec_immediate(&x->db->sl, f);
     }
   return EXEC_none;
 }

static int pop_process_def(process_def *x, exec_info *f)
 { llist m;
   var_decl *d;
   process_state *ps = f->curr->ps;
   int i;
   if (IS_SET(f->flags, EXEC_instantiation))
     { for (i = 0; i < f->curr->nr_var; i++)
         { sched_instance_all(&f->curr->var[i], f); }
       m = x->pl;
       while (!llist_is_empty(&m))
         { d = llist_head(&m);
           var_str_printf(&f->scratch, 0, "In process %s, port %s",
                          ps->nm, d->id);
           f->curr->i = d->flags;
           check_old_ports(&f->curr->var[d->var_idx], &d->tp, f);
           m = llist_alias_tail(&m);
         }
     }
   /* CHP processes now do nothing upon terminating (no memory freed) */
   ps->nr_thread--;
   assert(!ps->nr_thread);
   ps->nr_thread = -2;
   ps->refcnt--;
   free_process_state(ps, f);
   return EXEC_next;
 }

/********** builtin functions ************************************************/

typedef struct builtin_info
   { const str *id;
     exec_builtin_func *f;
     function_def *d;
   } builtin_info;

static llist builtin_funcs; /* llist(builtin_info) */

extern void set_builtin_func(const char *nm, exec_builtin_func *f)
 /* set nm as name of builtin function f */
 { builtin_info *b;
   NEW(b);
   b->id = make_str(nm);
   b->f = f;
   b->d = 0;
   llist_prepend(&builtin_funcs, b);
 }

/* llist_func */
static int builtin_func_eq(builtin_info *b, const str *id)
 /* true if b has name id */
 { return b->id == id; }

static void get_builtin_func(function_def *x, sem_info *f)
 /* set x to point to the builtin function identified by x->id */
 { builtin_info *b;
   b = llist_find(&builtin_funcs, (llist_func*)builtin_func_eq, x->id);
   if (!b)
     { sem_error(f, x, "There is no builtin function %s", x->id); }
   if (b->d)
     { sem_error(f, x, "Builtin function %s was already defined at %s[%d:%d]",
                    b->id, b->d->src, b->d->lnr, b->d->lpos);
     }
   x->b = (void*)b->f;
   b->d = x;
   SET_FLAG(x->flags, DEF_builtin);
 }

extern value_tp *get_call_arg(function_def *x, int i, exec_info *f)
 /* Pre: a call of x is being executed.
    Return a pointer to argument value i (starting from 0) of x. This
    can be one of the regular arguments or an extra argument for a
    DEF_vararg routine.
 */
 { parameter *p;
   int j;
   call *y;
   if (i < 0)
     { exec_error(f, x, "Request for argument %d (should be > 0)", i); }
   p = llist_idx(&x->pl, i);
   if (p)
     { return &f->curr->var[p->d->var_idx]; }
   j = i - llist_size(&x->pl);
   if (j >= f->curr->argc)
     { if (f->curr->stack) /* procedure call */
         { y = (call*)f->curr->stack->obj; }
       else if (f->parent) /* function call */
         { y = (call*)f->parent->curr->obj; }
       else /* shouldn't happen */
         { y = (call*)x; }
       exec_error(f, x,
                  "Request for argument %d out of %d+%d passed at %s[%d:%d]",
                  i, i - j, f->curr->argc, y->src, y->lnr, y->lpos);
     }
   return &f->curr->argv[j];
 }


/********** actual builtin functions *****************************************/

extern int bif_print_argv(llist *l, int i, exec_info *f, int pos)
 /* Pre: llist(expr *) m; the values of these expressions are stored
         in f->curr->argv[i...] (as with the arguments corresponding to "..."
         when calling a builtin function).
    Prints the arguments to f->scratch[p...] in the same way as the
    builtin print() procedure.
    Return is the number of characters printed.
 */
 { llist m = *l;
   const_expr *a;
   value_tp *aval;
   token_expr *s;
   int p = pos;
   while (!llist_is_empty(&m))
     { a = llist_head(&m);
       aval = &f->curr->argv[i];
       if (a->class == CLASS_const_expr && a->x->class == CLASS_token_expr
           && a->x->tp.kind == TP_array) /* string */
         { s = (token_expr*)a->x;
           pos += var_str_printf(&f->scratch, pos, "%s", s->t.val.s);
         }
       else if (a->tp.tps == (type_spec*)builtin_string)
         { pos += print_string_value(&f->scratch, pos, aval); }
       else
         { pos += var_str_printf(&f->scratch, pos, "%v", vstr_val, aval); }
       m = llist_alias_tail(&m);
       i++;
       if (!llist_is_empty(&m))
         { VAR_STR_X(&f->scratch, pos) = ' '; pos++; }
     }
   var_str_printf(&f->scratch, pos, "\n");
   return pos - p;
 }

/* exec_builtin_func */
static void exec_builtin_print(function_def *y, exec_info *f)
 /* corresponding to "print()" in user code */
 { int pos = 0;
   call *x;
   x = (call*)f->curr->stack->obj;
   assert(x->class == CLASS_call);
   pos += var_str_printf(&f->scratch, pos, "%s> ", f->curr->ps->nm);
   bif_print_argv(&x->a, 0, f, pos);
   fprintf(f->user->user_stdout, "%s", f->scratch.s);
 }

/* exec_builtin_func */
static void exec_builtin_error(function_def *y, exec_info *f)
 /* corresponding to "error()" in user code */
 { call *x;
   x = (call*)f->curr->stack->obj;
   assert(x->class == CLASS_call);
   bif_print_argv(&x->a, 0, f, 0);
   exec_error(f, x, "%s", f->scratch.s);
 }

/* exec_builtin_func */
static void exec_builtin_warning(function_def *y, exec_info *f)
 /* corresponding to "warning()" in user code */
 { call *x;
   x = (call*)f->curr->stack->obj;
   assert(x->class == CLASS_call);
   bif_print_argv(&x->a, 0, f, 0);
   exec_warning(f, x, "%s", f->scratch.s);
 }

/* exec_builtin_func */
static void exec_builtin_show(function_def *y, exec_info *f)
 /* corresponding to "show()" in user code */
 { llist m;
   const_expr *a;
   value_tp *aval;
   int pos = 0, i;
   call *x;
   x = (call*)f->curr->stack->obj;
   assert(x->class == CLASS_call);
   pos += var_str_printf(&f->scratch, pos,  "%s> %s[%d:%d] \t",
                        f->curr->ps->nm, x->src, x->lnr, x->lpos);
   m = x->a; i = 0;
   if (f->curr->argc > 1)
     { pos += var_str_printf(&f->scratch, pos, "\n\t"); }
   while (!llist_is_empty(&m))
     { a = llist_head(&m);
       aval = &f->curr->argv[i];
       pos += var_str_printf(&f->scratch, pos, "%v = %v",
                             vstr_obj, a, vstr_val, aval);
       m = llist_alias_tail(&m);
       i++;
       if (!llist_is_empty(&m))
         { pos += var_str_printf(&f->scratch, pos, "\n\t"); }
     }
   var_str_printf(&f->scratch, pos, "\n");
   fprintf(f->user->user_stdout, "%s", f->scratch.s);
 }

/* exec_builtin_func */
static void exec_builtin_step(function_def *x, exec_info *f)
 /* corresponding to "step()" in user code */
 { SET_FLAG(f->curr->ps->flags, DBG_step); }

/* exec_builtin_func */
static void exec_builtin_time(function_def *x, exec_info *f)
 /* corresponding to "time()" in user code */
 { value_tp *xval;
   exec_info *g = f;
   while (g->parent) g = g->parent;
   assert(x->ret);
   xval = &f->curr->var[x->ret->var_idx];
   xval->rep = REP_z;
   xval->v.z = new_z_value(f);
   mpz_fdiv_q_2exp(xval->v.z->z, g->time, 1);
   int_simplify(xval, f);
 }

/* exec_builtin_func */
static void exec_builtin_random(function_def *x, exec_info *f)
 /* corresponding to "random()" in user code */
 /* Note that lrand48() always returns 32 random bits */
 { value_tp *xval, *rval;
   unsigned long mask, rand, size, i;
   assert(x->ret);
   rval = &f->curr->var[x->ret->var_idx];
   xval = get_call_arg(x, 0, f);
   if (xval->rep == REP_int)
     { if (xval->v.i <= 0)
         { exec_error(f, x, "random() requires a positive integer argument"); }
       mask = 1;
       while (mask < xval->v.i) mask = mask << 1;
       mask--;
       do { rand = 0;
            for (i = 0; i < sizeof(long) / 4; i++)
              { if (i > 0) rand = rand << 32;
                rand |= lrand48();
              }
            rand &= mask;
          } while (rand >= xval->v.i);
       rval->rep = REP_int;
       rval->v.i = rand;
     }
   else if (xval->rep == REP_z)
     { if (mpz_sgn(xval->v.z->z) <= 0)
         { exec_error(f, x, "random() requires a positive integer argument"); }
       rval->rep = REP_z;
       rval->v.z = new_z_value(f);
       mpz_sub_ui(rval->v.z->z, xval->v.z->z, 1);
       size = mpz_sizeinbase(rval->v.z->z, 2);
       mask = (1 << size % 32) - 1;
       do
         { mpz_set_ui(rval->v.z->z, (int)(lrand48() & mask));
           for (i = 0; i < size / 32; i++)
              { mpz_mul_2exp(rval->v.z->z, rval->v.z->z, 32);
                mpz_add_ui(rval->v.z->z, rval->v.z->z, (int)lrand48());
              }
         } while (mpz_cmp(rval->v.z->z, xval->v.z->z) >= 0);
     }
   else /* REP_none? */
     { exec_error(f, x, "random() requires a positive integer argument"); }
 }


/********** breakpoints ******************************************************/

static brk_return brk_function_def(function_def *x, user_info *f)
 { f->cxt = x->cxt;
   if (x->lnr == f->brk_lnr && 
       (x->b->lnr > f->brk_lnr || x->b->lpos > f->brk_lpos))
     { return set_breakpoint(x, f); }
   return brkp(x->b, f);
 }

static void *obj_closer_pre(void *obj0, void *obj1, user_info *f)
/* Return the object which is closest to the brk target without passing it */
 { parse_obj *x0 = obj0, *x1 = obj1;
   int v0, v1;
   v0 = x0 && x0->lnr <= f->brk_lnr &&
        (x0->lnr < f->brk_lnr || x0->lpos <= f->brk_lpos);
   v1 = x1 && x1->lnr <= f->brk_lnr &&
        (x1->lnr < f->brk_lnr || x1->lpos <= f->brk_lpos);
   if (!v0 && !v1) return 0;
   if (!v0) return x1;
   if (!v1) return x0;
   if (x0->lnr > x1->lnr) return x0;
   if (x0->lnr < x1->lnr) return x1;
   if (x0->lpos > x1->lpos) return x0;
   if (x0->lpos < x1->lpos) return x1;
   return x0;
 }

static void *obj_closer_post(void *obj0, void *obj1, user_info *f)
/* Return the object which is closest to the brk target without preceding it,
 * and also while being on the same line as the target
 */
 { parse_obj *x0 = obj0, *x1 = obj1;
   int v0, v1;
   v0 = x0 && x0->lnr == f->brk_lnr && x0->lpos > f->brk_lpos;
   v1 = x1 && x1->lnr == f->brk_lnr && x1->lpos > f->brk_lpos;
   if (!v0 && !v1) return 0;
   if (!v0) return x1;
   if (!v1) return x0;
   if (x0->lpos < x1->lpos) return x0;
   if (x0->lpos > x1->lpos) return x1;
   return x0;
 }

static brk_return brk_process_def(process_def *x, user_info *f)
 { brk_return res;
   parse_obj *b;
   b = obj_closer_pre(x->mb, x->cb, f);
   b = obj_closer_pre(b, x->hb, f);
   /* Skip prs bodies, since a pr is not a valid target */
   if (b)
     { res = brkp(b, f);
       if (res != BRK_no_stmt) return res;
     }
   f->cxt = x->cxt;
   if (x->lnr == f->brk_lnr)
     { return set_breakpoint(x, f); }
   b = obj_closer_post(x->mb, x->cb, f);
   b = obj_closer_post(b, x->hb, f);
   if (b) return brkp(b, f);
   return BRK_no_stmt;
 }

static int brk_chp_body(chp_body *x, user_info *f)
 { brk_return res;
   parse_obj *dlhead, *slhead, *b;
   f->cxt = x->cxt;
   dlhead = llist_is_empty(&x->dl)? 0 : llist_head(&x->dl);
   slhead = llist_is_empty(&x->sl)? 0 : llist_head(&x->sl);
   b = obj_closer_pre(dlhead, slhead, f);
   if (b)
     { res = brk_list((b == dlhead)? &x->dl : &x->sl, f);
       if (res != BRK_no_stmt) return res;
     }
   if (x->lnr == f->brk_lnr)
     { if (dlhead) return set_breakpoint(dlhead, f);
       if (slhead) return set_breakpoint(slhead, f);
       return BRK_no_stmt;
     }
   b = obj_closer_post(dlhead, slhead, f);
   if (b)
     { return brk_list((b == dlhead)? &x->dl : &x->sl, f); }
   return BRK_no_stmt;
 }


/*****************************************************************************/

extern void init_routines(void)
 /* call at startup */
 { set_builtin_func("print", exec_builtin_print);
   set_builtin_func("show", exec_builtin_show);
   set_builtin_func("step", exec_builtin_step);
   set_builtin_func("error", exec_builtin_error);
   set_builtin_func("warning", exec_builtin_warning);

   set_builtin_func("time", exec_builtin_time);
   set_builtin_func("random", exec_builtin_random);

   set_print(chp_body);
   /* meta_body == hse_body == prs_body == chp_body */
   set_print_cp(meta_body, chp_body);
   set_print_cp(hse_body, chp_body);
   set_print_cp(prs_body, chp_body);
   set_print(function_def);
   set_print(process_def);
   set_sem(function_def);
   set_sem(chp_body);
   /* meta_body == hse_body == prs_body == chp_body */
   set_sem_cp(meta_body, chp_body);
   set_sem_cp(hse_body, chp_body);
   set_sem_cp(prs_body, chp_body);
   set_sem_cp(delay_body, chp_body);
   set_sem(process_def);
   set_import(function_def);
   set_import(process_def);
   set_exec(function_def);
   set_exec(process_def);
   set_pop(process_def);
   set_brk(function_def);
   set_brk(process_def);
   set_brk(chp_body);
   /* meta_body == chp_body */
   set_brk_cp(meta_body, chp_body);
   set_brk_cp(hse_body, chp_body);
 }
