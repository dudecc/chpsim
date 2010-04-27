/* value.c: value representation and expression evaluation

   Author: Marcel van der Goot
*/
volatile static const char cvsid[] =
        "$Id: exec.c,v 1.6 2004/04/13 20:50:42 marcel Exp $";

#include <standard.h>
#include "parse_obj.h"
#include "value.h"
#include "exec.h"
#include "interact.h"
#include "routines.h"
#include "expr.h"
#include "types.h"

/*extern*/ int app_eval = -1;
/*extern*/ int app_range = -1;
/*extern*/ int app_assign = -1;

/********** expression evaluation ********************************************/

extern void push_value(value_tp *v, exec_info *f)
 /* push *v on stack (direct copy: top = *v) */
 { eval_stack *w;
   if (f->fl)
     { w = f->fl; f->fl = w->next; }
   else
     { NEW(w); }
   w->v = *v;
   w->next = f->stack; f->stack = w;
 }

extern void pop_value(value_tp *v, exec_info *f)
 /* store top of stack in *v (direct copy: *v = top) and pop stack */
 { eval_stack *w;
   assert(f->stack);
   w = f->stack; f->stack = w->next;
   *v = w->v;
   w->next = f->fl; f->fl = w;
 }

extern void push_repval(value_tp *v, ctrl_state *cs, exec_info *f)
 /* push *v on repval stack (direct copy: top = *v) */
 { eval_stack *w;
   if (f->fl)
     { w = f->fl; f->fl = w->next; }
   else
     { NEW(w); }
   w->v = *v;
   w->next = cs->rep_vals; cs->rep_vals = w;
 }

extern void pop_repval(value_tp *v, ctrl_state *cs, exec_info *f)
 /* store top of stack in *v (direct copy: *v = top) and pop stack */
 { eval_stack *w;
   assert(cs->rep_vals);
   w = cs->rep_vals; cs->rep_vals = w->next;
   *v = w->v;
   w->next = f->fl; f->fl = w;
 }

static void no_eval(expr *x, exec_info *f)
 /* called when x has no eval function */
 { error("Internal error: "
         "Object class %s has no eval method", x->class->nm);
 }

extern void eval_expr(void *obj, exec_info *f)
 /* Evaluate expr *obj */
 { APP_OBJ_VFZ(app_eval, obj, f, no_eval);
 }

/* var_str_func_tp: use as first arg for %v */
extern int vstr_mpz(var_string *s, int pos, void *z)
 /* print mpz_t z to s[pos...] */
 { var_str_ensure(s, pos + mpz_sizeinbase(z, 10) + 2);
   mpz_get_str(s->s + pos, 10, z);
   return 0;
 }

/* var_str_func_tp: use as first arg for %v */
extern int vstr_val(var_string *s, int pos, void *val)
 /* print value_tp *val to s[pos...] */
 { print_info f;
   print_info_init(&f);
   f.s = s; f.pos = pos;
   print_value_tp(val, &f);
   VAR_STR_X(f.s, f.pos) = 0;
   return 0;
 }

extern void print_value_tp(value_tp *v, print_info *f)
 /* Print v to f */
 { char bo = '{', bc = '}';
   wire_value *w;
   int i, n;
   switch (v->rep)
     { case REP_none: print_char('?', f);
       break;
       case REP_bool: if (v->v.i == 1) print_string("true", f);
                      else if (v->v.i == 0) print_string("false", f);
                      else assert(!"Illegal bool value");
       break;
       case REP_int: f->pos += var_str_printf(f->s, f->pos, "%ld", v->v.i);
       break;
       case REP_z:
                var_str_ensure(f->s, f->pos + mpz_sizeinbase(v->v.z->z, 10)+1);
                mpz_get_str(f->s->s + f->pos, 10, v->v.z->z);
                f->pos += strlen(f->s->s + f->pos);
       break;
       case REP_symbol: print_string(v->v.s, f);
       break;
       case REP_array: bo = '['; bc = ']';
       /* fall-through */
       case REP_record:
                print_char(bo, f);
                i = 0; n = v->v.l->size;
                while (n)
                  { print_value_tp(&v->v.l->vl[i], f);
                    i++; n--;
                    if (n)
                      { print_char(',', f); print_char(' ', f); }
                  }
                print_char(bc, f);
       break;
       case REP_union:
                print_value_tp(&v->v.u->v, f);
       break;
       case REP_port:
                print_string("port --> ", f);
                if (!v->v.p->p)
                  { print_string("(disconnected)", f); }
                else if (!is_visible(v->v.p->p->ps))
                  { print_string("(wired decomposition)", f); }
                else
                  { print_string(v->v.p->p->ps->nm, f);
                    print_char(':', f);
                    print_port_value(v->v.p->p, f);
                  }
                if (!IS_SET(v->v.p->wprobe.flags, WIRE_value))
                  { print_string(", # = false", f); }
                else if (v->v.p->v.rep)
                  { print_string(", # = true, data = ", f);
                    print_value_tp(&v->v.p->v, f);
                  }
                else
                  { print_string(", # = true", f); }
       break;
       case REP_process:
                print_string("process ", f);
                print_string(v->v.ps->nm, f);
       break;
       case REP_wire:
                w = v->v.w;
                while (IS_SET(w->flags, WIRE_forward)) w = w->u.w;
                f->pos += var_str_printf(f->s, f->pos, "(%c)",
                              IS_SET(w->flags, WIRE_undef)?
                                'X' : '0' + IS_SET(w->flags, WIRE_value));
       break;
       case REP_cnt:
                f->pos += var_str_printf(f->s, f->pos, "%d", v->v.c->cnt);
       break;
       case REP_type:
                print_string("<type>", f);
       break;
       default: assert(!"Illegal representation");
       break;
     }
   VAR_STR_X(f->s, f->pos) = 0;
 }

/* var_str_func_tp: use as first arg for %v */
extern int print_string_value(var_string *s, int pos, value_tp *val)
 /* Pre: val is an array of {0..255}.
    Print val as a (0-terminated) string to s[pos...].
    Return is nr chars printed.
 */
 { int4 i, n;
   int r = 0, c;
   value_list *l;
   value_tp *ev;
   assert(val->rep == REP_array);
   l = val->v.l;
   n = l->size;
   for (i = 0; i < n; i++)
     { ev = &l->vl[i];
       if (!ev->rep)
         { VAR_STR_X(s, pos) = '?'; pos++; r++; }
       else
         { assert(ev->rep == REP_int || ev->rep == REP_z);
           if (ev->rep == REP_int)
             { c = ev->v.i; }
           else
             { c = mpz_get_si(ev->v.z->z); }
           if (!c)
             { break; }
           else
             { VAR_STR_X(s, pos) = c; pos++; r++; }
         }
     }
   VAR_STR_X(s, pos) = 0;
   return r;
 }

static int _print_wire_value
(value_tp *v, type *tp, wire_value *w, exec_info *f)
 /* Also used for port values pv, where w is &pv->wprobe */
 { llist l, rl;
   long i, res;
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
       res = _print_wire_value(v, &tpv->tp, w, f);
       f->meta_ps = meta_ps;
       return res;
     }
   switch(v->rep)
     { case REP_record:
         if (tp->kind == TP_wire)
           { wtps = (wired_type*)tp->tps;
             l = wtps->li;
             for (i = 0; i < v->v.l->size; i++)
               { wd = (wire_decl*)llist_head(&l);
                 var_str_printf(&f->scratch, pos, ".%s", wd->id);
                 if (_print_wire_value(&v->v.l->vl[i], &wd->tps->tp, w, f))
                   { return 1; }
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
                 if (_print_wire_value(&v->v.l->vl[i], llist_head(&l), w, f))
                   { return 1; }
                 l = llist_alias_tail(&l);
                 rl = llist_alias_tail(&rl);
               }
           }
       return 0;
       case REP_union:
       return _print_wire_value(&v->v.u->v, &v->v.u->f->tp, w, f);
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
             if (_print_wire_value(&v->v.l->vl[i], tp->elem.tp, w, f))
               { clear_value_tp(&idxv, f);
                 return 1;
               }
             int_inc(&idxv, f);
           }
         clear_value_tp(&idxv, f);
       return 0;
       case REP_port:
       return w == &v->v.p->wprobe || w == &v->v.p->p->wprobe;
       case REP_wire:
       return w == v->v.w;
       default:
       return 0;
     }
 }

extern void print_wire_exec(wire_value *w, exec_info *f)
 /* Print a name for w in the reference frame of f->meta_ps to f->scratch */
 { llist l;
   var_decl *d;
   value_tp *var = f->meta_ps->var;
   l = f->meta_ps->p->pl;
   while (!llist_is_empty(&l))
     { d = llist_head(&l);
       l = llist_alias_tail(&l);
       var_str_printf(&f->scratch, 0, "%s", d->id);
       if (_print_wire_value(&var[d->var_idx], &d->tp, w, f)) return;
     }
   l = f->meta_ps->b->dl;
   while (!llist_is_empty(&l))
     { d = llist_head(&l);
       l = llist_alias_tail(&l);
       if (d->class != CLASS_var_decl) continue;
       var_str_printf(&f->scratch, 0, "%s", d->id);
       if (_print_wire_value(&var[d->var_idx], &d->tp, w, f)) return;
     }
   assert(!"Wire not found");
 }

extern void print_wire_value(wire_value *w, process_state *ps, print_info *f)
 /* Print a name for w in the reference frame of ps to f */
 { exec_info g;
   exec_info_init(&g, 0);
   g.meta_ps = ps;
   print_wire_exec(w, &g);
   print_string(g.scratch.s, f);
   exec_info_term(&g);
 }

/* var_str_func2_tp: use as first arg for %V */
extern int vstr_wire(var_string *s, int pos, void *w, void *ps)
 /* Print a name for w in the reference frame of ps to s[pos...] */
 { exec_info g;
   exec_info_init(&g, 0);
   g.meta_ps = ps;
   print_wire_exec(w, &g);
   var_str_printf(s, pos, "%s", g.scratch.s);
   exec_info_term(&g);
   return 0;
 }

static void print_port_exec(port_value *p, exec_info *f)
 /* Pre: f->meta_ps == p->ps: print the name of port p to f->scratch */
 { llist l = p->ps->p->pl;
   var_decl *d;
   value_tp *var = p->ps->var;
   while (!llist_is_empty(&l))
     { d = llist_head(&l);
       l = llist_alias_tail(&l);
       var_str_printf(&f->scratch, 0, "%s", d->id);
       if (_print_wire_value(&var[d->var_idx], &d->tp, &p->wprobe, f))
         { return; }
     }
   assert(!"Port not found");
 }

extern void print_port_value(port_value *p, print_info *f)
 /* print the name of port p */
 { exec_info g;
   exec_info_init(&g, 0);
   g.meta_ps = p->ps;
   print_port_exec(p, &g);
   print_string(g.scratch.s, f);
   exec_info_term(&g);
 }

/* var_str_func_tp: use as first arg for %v */
extern int vstr_port(var_string *s, int pos, void *p)
 /* print the name of port p to s[pos...] */
 { exec_info g;
   exec_info_init(&g, 0);
   g.meta_ps = ((port_value*)p)->ps;
   print_port_exec(p, &g);
   var_str_printf(s, pos, "%s", g.scratch.s);
   exec_info_term(&g);
   return 0;
 }

extern value_list *new_value_list(int size, exec_info *f)
 /* allocate new list with given size */
 { value_list *vl;
   int i;
   MALLOC(vl, sizeof(*vl) + (size - 1) * sizeof(vl->vl[0]));
   vl->size = size;
   vl->refcnt = 1;
   for (i = 0; i < size; i++)
     { vl->vl[i].rep = REP_none; }
   return vl;
 }

extern value_union *new_value_union(exec_info *f)
 /* create new union value of the given type */
 { value_union *vu;
   NEW(vu);
   vu->refcnt = 1;
   vu->v.rep = REP_none;
   return vu;
 }

extern port_value *new_port_value(process_state *ps, exec_info *f)
 /* allocate new port_value */
 { port_value *p;
   NEW(p);
   p->wprobe.refcnt = 1;
   p->wprobe.flags = WIRE_is_probe;
   p->p = 0;
   p->v.rep = REP_none;
   p->ps = ps;
   return p;
 }

extern type_value *new_type_value(exec_info *f)
 /* allocate new type_value */
 { type_value *tp;
   NEW(tp);
   tp->refcnt = 1;
   tp->meta_ps = f->curr->ps;
   f->curr->ps->refcnt++;
   return tp;
 }

extern wire_value *new_wire_value(token_tp x, exec_info *f)
 /* allocate new wire_value */
 { wire_value *w;
   NEW(w);
   w->refcnt = 1;
   w->flags = (x? 0 : WIRE_undef) | (x=='+'? WIRE_value : 0);
   w->wframe = &f->curr->act;
   return w;
 }

extern counter_value *new_counter_value(long x, exec_info *f)
 /* allocate new counter_value */
 { counter_value *c;
   NEW(c);
   c->refcnt = 1;
   c->cnt = x;
   llist_init(&c->dep);
   return c;
 }

extern void wire_fix(wire_value **w, exec_info *f)
/* Remove all wire forwarding from w
 * Returned wire value is not reference counted
 */
 { wire_value *ww = *w;
   if (!IS_SET(ww->flags, WIRE_forward)) return;
   wire_fix(&ww->u.w, f);
   *w = ww->u.w;
   ww->refcnt--;
   if (!ww->refcnt) free(ww);
   else ww->u.w->refcnt++;
 }

static void free_port_value(port_value *p, exec_info *f)
 /* disconnect and deallocate p */
 { assert(!p->p);
   free(p);
 }

extern z_value *new_z_value(exec_info *f)
 /* allocate a new z_value, with initial value 0 */
 { z_value *z;
   NEW(z);
   z->refcnt = 1;
   mpz_init(z->z);
   return z;
 }

extern void clear_value_tp(value_tp *v, exec_info *f)
 /* Reset v to have no value. If v has allocated memory (e.g., for array),
    free that if there are no longer any references to it.
 */
 { int i;
   value_list *vl;
   value_union *vu;
   process_state *ps;
   port_value *p;
   type_value *tp;
   wire_value *w, *ww;
   counter_value *c;
   z_value *vz;
   if (v->rep == REP_z)
     { vz = v->v.z;
       vz->refcnt--;
       if (!vz->refcnt)
         {
           mpz_clear(vz->z);
           free(vz);
         }
     }
   else if (v->rep == REP_array || v->rep == REP_record)
     { vl = v->v.l;
       vl->refcnt--;
       if (!vl->refcnt)
         { for (i = 0; i < vl->size; i++)
             { clear_value_tp(&vl->vl[i], f); }
           free(vl);
         }
     }
   else if (v->rep == REP_union)
     { vu = v->v.u;
       vu->refcnt--;
       if (!vu->refcnt)
         { clear_value_tp(&vu->v, f);
           free(vu);
         }
     }
   else if (v->rep == REP_process)
     { ps = v->v.ps;
       ps->refcnt--;
       if (!ps->refcnt)
         { free_process_state(ps, f); }
     }
   else if (v->rep == REP_port)
     { p = v->v.p;
       p->wprobe.refcnt--;
       if (!p->wprobe.refcnt)
         { free_port_value(p, f); }
     }
   else if (v->rep == REP_type)
     { tp = v->v.tp;
       tp->refcnt--;
       if (!tp->refcnt)
         { ps = tp->meta_ps;
           ps->refcnt--;
           if (!ps->refcnt)
             { free_process_state(ps, f); }
           free(tp);
         }
     }
   else if (v->rep == REP_wire)
     { w = v->v.w;
       w->refcnt--;
       while (!w->refcnt)
         { if (!IS_SET(w->flags, WIRE_forward))
             { free(w); break; }
           ww = w->u.w;
           free(w);
           w = ww;
           w->refcnt--;
         }
     }
   else if (v->rep == REP_cnt)
     { c = v->v.c;
       c->refcnt--;
       if (!c->refcnt)
         { free(c); }
     }
   v->rep = REP_none;
 }

extern void copy_value_tp(value_tp *w, value_tp *v, exec_info *f)
 /* Copy v to w, allocating new memory */
 { int i;
   value_list *vl, *wl;
   value_union *vu, *wu;
   if (v->rep == REP_z)
     { w->rep = REP_z;
       w->v.z = new_z_value(f);
       mpz_set(w->v.z->z, v->v.z->z);
     }
   else if (v->rep == REP_array || v->rep == REP_record)
     { w->rep = v->rep;
       vl = v->v.l;
       w->v.l = wl = new_value_list(vl->size, f);
       for (i = 0; i < vl->size; i++)
         { copy_value_tp(&wl->vl[i], &vl->vl[i], f); }
     }
   else if (v->rep == REP_union)
     { w->rep = REP_union;
       vu = v->v.u;
       w->v.u = wu = new_value_union(f);
       wu->d = vu->d;
       copy_value_tp(&wu->v, &vu->v, f);
     }
   else if (v->rep == REP_process) /* no new memory */
     { *w = *v;
       w->v.ps->refcnt++;
     }
   else if (v->rep == REP_port) /* no new memory */
     { *w = *v;
       w->v.p->wprobe.refcnt++;
     }
   else if (v->rep == REP_type) /* no new memory */
     { *w = *v;
       w->v.tp->refcnt++;
     }
   else if (v->rep == REP_wire) /* no new memory */
     { *w = *v;
       w->v.w->refcnt++;
     }
   else if (v->rep == REP_cnt) /* no new memory */
     { *w = *v;
       w->v.c->refcnt++;
     }
   else
     { *w = *v; }
 }

extern void alias_value_tp(value_tp *w, value_tp *v, exec_info *f)
 /* Copy v to w, sharing the memory */
 { *w = *v;
   if (w->rep == REP_z)
     { w->v.z->refcnt++; }
   else if (w->rep == REP_array || w->rep == REP_record)
     { w->v.l->refcnt++; }
   else if (w->rep == REP_union)
     { w->v.u->refcnt++; }
   else if (w->rep == REP_process)
     { w->v.ps->refcnt++; }
   else if (v->rep == REP_port)
     { w->v.p->wprobe.refcnt++; }
   else if (v->rep == REP_type)
     { w->v.tp->refcnt++; }
   else if (v->rep == REP_wire)
     { w->v.w->refcnt++; }
   else if (v->rep == REP_cnt)
     { w->v.c->refcnt++; }
 }

extern void copy_and_clear(value_tp *w, value_tp *v, exec_info *f)
 /* Same as: copy_value_tp(w, v, f); clear_value_tp(v, f);
    but more efficient.
 */
 { int i;
   value_list *vl, *wl;
   value_union *vu, *wu;
   z_value *vz;
   if (v->rep == REP_z)
     { vz = v->v.z;
       if (vz->refcnt == 1)
         { *w = *v; }
       else
         { vz->refcnt--;
           copy_value_tp(w, v, f);
         }
     }
   else if (v->rep == REP_array || v->rep == REP_record)
     { w->rep = v->rep;
       vl = v->v.l;
       if (vl->refcnt == 1)
         { w->v.l = wl = vl;
           for (i = 0; i < vl->size; i++)
             { copy_and_clear(&wl->vl[i], &vl->vl[i], f); }
         }
       else
         { vl->refcnt--; 
           w->v.l = wl = new_value_list(vl->size, f);
           for (i = 0; i < vl->size; i++)
             { copy_value_tp(&wl->vl[i], &vl->vl[i], f); }
         }
     }
   else if (v->rep == REP_union)
     { w->rep = REP_union;
       vu = v->v.u;
       if (vu->refcnt == 1)
         { w->v.u = wu = vu;
           copy_and_clear(&wu->v, &vu->v, f);
         }
       else
         { vu->refcnt--; 
           w->v.u = wu = new_value_union(f);
           wu->d = vu->d;
           copy_value_tp(&wu->v, &vu->v, f);
         }
     }
   else
     { *w = *v; }
   if (w != v)
     { v->rep = REP_none; }
 }


/********** range checks *****************************************************/

static void no_range(expr *x, exec_info *f)
 /* called when x has no range function */
 { error("Internal error: "
         "Object class %s has no range method", x->class->nm);
 }

extern void range_check(type_spec *tps, value_tp *val, exec_info *f, void *obj)
 /* Verify that val is a valid value for tps. obj is used for error msgs */
 { value_tp *w = f->val;
   void *err_obj = f->err_obj;
   if (!val->rep)
     { /* at the top-level this is usually wrong, but when you assign an
          array or record it is OK if some fields are unknown. */
       return;
     }
   if (!tps) return; /* happens with some generic types */
   f->val = val;
   f->err_obj = obj;
   APP_OBJ_VFZ(app_range, tps, f, no_range);
   f->val = w;
   f->err_obj = err_obj;
 }


/********** assignment *******************************************************/

static void update_rule(wire_expr_flags val, action *a, exec_info *f)
 { action_flags af;
   wire_value *hold = (wire_value*)a;
   switch(val & WIRE_action)
     { case WIRE_pu: case WIRE_pd:
         /* Just mark the changes for now, and add the action to the queue */
         af = IS_SET(val, WIRE_pu)? ACTION_up_nxt : ACTION_dn_nxt;
         if (IS_SET(val, WIRE_value)) SET_FLAG(a->flags, af);
         else RESET_FLAG(a->flags, af);
         if (!IS_SET(a->flags, ACTION_check))
           { llist_prepend(&f->check, a);
             SET_FLAG(a->flags, ACTION_check);
           }
       return;
       case WIRE_susp: /* suspended thread, schedule immediately */
         if (!IS_SET(val, WIRE_value))
           { assert(!"Action has become unscheduled"); }
         if (!IS_SET(a->flags, ACTION_sched))
           { SET_FLAG(a->flags, ACTION_atomic);
             action_sched(a, f);
             RESET_FLAG(a->flags, ACTION_atomic);
           }
       case WIRE_hu:
         if (IS_SET(val, WIRE_value))
           { RESET_FLAG(hold->flags, WIRE_held_up);
             if (IS_SET(hold->flags, WIRE_wait))
               { write_wire(1, hold, f); }
             RESET_FLAG(hold->flags, WIRE_wait);
           }
         else
           { SET_FLAG(hold->flags, WIRE_held_up); }
       return;
       case WIRE_hd:
         if (IS_SET(val, WIRE_value))
           { RESET_FLAG(hold->flags, WIRE_held_dn);
             if (IS_SET(hold->flags, WIRE_wait))
               { write_wire(0, hold, f); }
             RESET_FLAG(hold->flags, WIRE_wait);
           }
         else
           { SET_FLAG(hold->flags, WIRE_held_dn); }
       return;
       default:
       return;
     }
 }

static void update_wire_expr(wire_flags val, wire_expr *e, exec_info *f)
 /* Flag WIRE_value of val has the new value of the wire, flag WIRE_undef
  * of val tells us whether the wire was previously undefined.
  */
 { int old;
   if (!e->undefcnt) /* Nothing undefined, make this fast */
     { if (IS_SET(e->flags, WIRE_xor))
         { e->flags = e->flags ^ WIRE_value; }
       else
         { old = e->valcnt;
           if ((val ^ (e->flags >> WIRE_vd_shft)) & WIRE_value)
             { e->valcnt++; }
           else
             { e->valcnt--; }
           if ((old > 0) ^ (e->valcnt <= 0)) return; /* no update */
           e->flags = e->flags ^ WIRE_value;
         }
       if (IS_SET(e->flags, WIRE_action))
         { update_rule(e->flags, e->u.act, f); }
       else
         { update_wire_expr(e->flags, e->u.dep, f); }
       return;
     }
   if (IS_SET(e->flags, WIRE_xor))
     { if (IS_SET(val, WIRE_undef))
         { e->undefcnt--;
           e->flags = e->flags ^ IS_SET(val, WIRE_value);
         }
       else
         { e->flags = e->flags ^ WIRE_value; }
       if (e->undefcnt) return; /* still undefined, no update */
     }
   else
     { old = e->valcnt;
       if (IS_SET(val, WIRE_undef))
         { e->undefcnt--;
           if ((val ^ (e->flags >> WIRE_vd_shft)) & WIRE_value)
             { e->valcnt++; }
         }
       else if ((val ^ (e->flags >> WIRE_vd_shft)) & WIRE_value)
         { e->valcnt++; }
       else
         { e->valcnt--; }
       if (old && !e->valcnt)
         { exec_error(f, f->err_obj, "Expression has become undefined"); }
       else if (old || (!e->valcnt && e->undefcnt)) return;
       /* no change (old), or still undefined, no update */
       e->flags = e->flags ^ WIRE_value;
     }
   if (IS_SET(e->flags, WIRE_action))
     { update_rule(e->flags, e->u.act, f); }
   else
     { update_wire_expr(e->flags, e->u.dep, f); }
   RESET_FLAG(e->flags, WIRE_undef);
 }

extern void write_wire(int val, wire_value *w, exec_info *f)
 /* Pre: w has been run through wire_fix */
 { llist m;
   wire_expr *e;
   mpz_t time;
   wire_flags old = w->flags;
   ASSIGN_FLAG(w->flags, val? WIRE_value : 0, WIRE_val_mask);
   if (w->flags == old) return;
   if (IS_SET(w->flags, val? WIRE_held_up : WIRE_held_dn))
     { w->flags = old;
       SET_FLAG(w->flags, WIRE_wait);
       /* TODO: don't reschedule production rules */
       f->e = 0;
       add_wire_dep(w, f);
       SET_FLAG(f->curr->act.flags, ACTION_delay_susp | ACTION_susp);
       return;
     }
   if (IS_SET(w->flags, WIRE_watch) || (IS_SET(f->user->flags, USER_watchall)
                                        && !IS_SET(w->flags, WIRE_is_probe)))
     { print_wire_exec(w, f);
       mpz_init(time);
       mpz_fdiv_q_2exp(time, f->time, 1);
       report(f->user, "(watch) %s:%s %s at time %v",
              f->meta_ps->nm, f->scratch.s,
              IS_SET(w->flags, WIRE_value)? "up" : "down", vstr_mpz, time);
       mpz_clear(time);
     }
   if (IS_SET(w->flags, WIRE_has_dep))
     { ASSIGN_FLAG(old, w->flags, WIRE_value);
       /* old is now formatted for update_wire_expr */
       m = w->u.dep;
       while (!llist_is_empty(&m))
         { e = llist_head(&m);
           update_wire_expr(old, e, f);
           m = llist_alias_tail(&m);
         }
       run_checks(w, f);
     }
 }

extern void update_counter(int dir, counter_value *c, exec_info *f)
 { wire_flags val;
   llist m;
   if (!dir && !(c->cnt--))
     { exec_error(f, c->err_obj, "Counter value has become negative"); }
   if (dir && (++c->cnt) > MAX_COUNT)
     { exec_error(f, c->err_obj, "Counter value has become too large "
                                 "(missing decrement?)");
     }
   val = dir? WIRE_value : 0;
   m = c->dep;
   while (!llist_is_empty(&m))
     { update_wire_expr(val, llist_head(&m), f);
       m = llist_alias_tail(&m);
     }
 }
   
extern void force_value(value_tp *xval, expr *x, exec_info *f)
 /* Assign an empty value to xval according to the type of x,
    i.e., an array/record value with a value_list without values.
 */
 { record_type *rtps;
   integer_type *itps;
   wired_type *wtps;
   value_tp v;
   if (x->tp.kind == TP_array)
     { force_value_array(xval, &x->tp, f); }
   else if (x->tp.kind == TP_record)
     { xval->rep = REP_record;
       rtps = (record_type*)x->tp.tps;
       assert(rtps->class == CLASS_record_type);
       xval->v.l = new_value_list(llist_size(&rtps->l), f);
     }
   else if (x->tp.kind == TP_wire)
     { xval->rep = REP_record;
       wtps = (wired_type*)x->tp.tps;
       assert(wtps->class == CLASS_wired_type);
       xval->v.l = new_value_list(wtps->nr_var, f);
     }
   else if (x->tp.kind == TP_int)
     { if (IS_SET(x->flags, EXPR_port))
         { if (x->tp.tps->class != CLASS_integer_type)
             { exec_error(f, x, "Cannot split a port of generic int type"); }
           xval->rep = REP_array;
           xval->v.l = new_value_list(integer_nr_bits(x->tp.tps, f), f);
         }
       else /* needed in case of bit selection */
         { if (x->tp.tps->class != CLASS_integer_type)
             { xval->rep = REP_int;
               xval->v.i = 0;
             }
           else
             { itps = (integer_type*)x->tp.tps;
               eval_expr(itps->l, f);
               pop_value(&v, f);
               copy_and_clear(xval, &v, f);
             }
         }
     }
 }

extern void force_value_array(value_tp *xval, type *tp, exec_info *f)
 /* Like the above, but assumes tp is an array type */
 { array_type *atps;
   value_tp lval, hval, dval;
   xval->rep = REP_array;
   atps = (array_type*)tp->tps;
   assert(atps->class == CLASS_array_type);
   eval_expr(atps->h, f);
   eval_expr(atps->l, f);
   pop_value(&lval, f);
   pop_value(&hval, f);
   if (int_cmp(&hval, &lval, f) < 0)
     { exec_error(f, tp, "Array bounds [%v..%#v] in the wrong order",
                  vstr_val, &lval, &hval);
     }
   dval = int_sub(&hval, &lval, f);
   int_simplify(&dval, f);
   if ((dval.rep == REP_int && hval.v.i > ARRAY_REP_MAXSIZE) ||
        dval.rep == REP_z)
     { int_inc(&dval, f);
       exec_error(f, tp, "Implementation limit exceeded: Array size "
                     "%v > %ld", vstr_val, &dval, (long)ARRAY_REP_MAXSIZE);
     }
   xval->v.l = new_value_list(dval.v.i + 1, f);
 }

static void no_assign(expr *x, exec_info *f)
 /* called when x has no assign function */
 { error("Internal error: "
         "Object class %s has no assign method", x->class->nm);
 }

extern void assign(expr *x, value_tp *val, exec_info *f)
 /* Assign x := val where x is an lvalue.
    Note: you should do a range_check() first.
 */
 { value_tp *w = f->val;
   if (!val->rep)
     { exec_error(f, x, "Unknown value in assignment to %v", vstr_obj, x); }
   f->val = val;
   APP_OBJ_VFZ(app_assign, x, f, no_assign);
   f->val = w;
 }

/********** creating wire expressions ***************************************/

extern wire_expr *new_wire_expr(exec_info *f)
 /* Note that refcnt is initially zero */
 { wire_expr *e;
   NEW(e);
   e->refcnt = 0;
   e->valcnt = 0;
   e->undefcnt = 0;
   e->flags = 0;
   e->u.dep = 0;
   return e;
 }

extern void we_add_dep(wire_expr *p, wire_expr *e, exec_info *f)
 /* add e as a child of p, update the value of p */
 { e->u.dep = p;
   p->refcnt++;
   if (IS_SET(e->flags, WIRE_undef))
     { if (!p->valcnt)
         { SET_FLAG(p->flags, WIRE_undef); }
       p->undefcnt++;
     }
   else if (IS_SET(p->flags, WIRE_xor))
     { p->flags = p->flags ^ (e->flags & WIRE_value);
       return;
     }
   if ((e->flags ^ (p->flags >> WIRE_vd_shft)) & WIRE_value)
     { if (!p->valcnt)
         { RESET_FLAG(p->flags, WIRE_undef);
           p->flags = p->flags ^ WIRE_value;
         }
       p->valcnt++;
     }
 }

/*extern*/ wire_expr temp_wire_expr;

static wire_expr *_make_wire_expr(expr *x, wire_expr *p, exec_info *f)
 /* If p, created wire_expr has p as parent and returns zero. Otherwise
  * returns created expression.
  */
 { binary_expr *be = (binary_expr*)x;
   prefix_expr *pe = (prefix_expr*)x;
   rep_expr *re = (rep_expr*)x;
   wire_ref *wr = (wire_ref*)x;
   wire_expr *e, *tmp;
   token_tp sym;
   value_tp v;
   long i, n;
   int compat;
   wire_expr_flags flags;
   if (!IS_SET(x->flags, EXPR_unconst))
     { eval_expr(x, f);
       pop_value(&v, f);
       e = &temp_wire_expr;
       assert(v.rep == REP_bool);
       ASSIGN_FLAG(e->flags, v.v.i? WIRE_value : 0, WIRE_val_mask);
     }
   else if (IS_SET(x->flags, EXPR_wire))
     { eval_expr(x, f);
       pop_value(&v, f);
       assert(v.rep == REP_wire);
       if (!IS_SET(v.v.w->flags, WIRE_has_dep))
         { llist_init(&v.v.w->u.dep);
           SET_FLAG(v.v.w->flags, WIRE_has_dep);
         }
       if (!p)
         { e = new_wire_expr(f);
           e->flags = WIRE_xor | (v.v.w->flags & WIRE_val_mask);
           llist_prepend(&v.v.w->u.dep, e);
           e->refcnt++;
         }
       else
         { e = &temp_wire_expr;
           e->flags = v.v.w->flags;
           llist_prepend(&v.v.w->u.dep, p);
         }
     }
   else if (pe->class == CLASS_prefix_expr)
     { assert(pe->op_sym == '~');
       e = _make_wire_expr(pe->r, 0, f);
       e->flags = e->flags ^ WIRE_value;
     }
   else if (be->class == CLASS_binary_expr || re->class == CLASS_rep_expr)
     { sym = (be->class == CLASS_binary_expr)? be->op_sym : re->rep_sym;
       switch (sym)
         { case '|': flags = 0; break;
           case '&': flags = WIRE_val_dir | WIRE_value; break;
           case KW_xor: case SYM_neq: case '=':
             flags = WIRE_val_dir;
           break;
           default:
             assert(!"bad binary operator in PR");
           break;
         }
       compat = p && IS_SET(p->flags, WIRE_xor | WIRE_val_dir) == flags;
       if (compat) e = p;
       else
         { e = new_wire_expr(f);
           e->flags = flags;
         } 
       if (sym == '=')
         { e->flags = e->flags ^ WIRE_value; }
       if (be->class == CLASS_binary_expr)
         { _make_wire_expr(be->l, e, f);
           _make_wire_expr(be->r, e, f);
         }
       else
         { n = eval_rep_common(&re->r, &v, f);
           push_repval(&v, f->curr, f);
           for (i = 0; i < n; i++)
             { _make_wire_expr(re->v, e, f);
               int_inc(&f->curr->rep_vals->v, f);
             }
           pop_repval(&v, f->curr, f);
           clear_value_tp(&v, f);
         }
       if (compat) return 0;
     }
   else
     { assert(!"Unhandled expr type"); }
   if (!p) return e;
   we_add_dep(p, e, f);
   return 0;
 }

extern wire_expr *make_wire_expr(expr *e, exec_info *f)
 { return _make_wire_expr(e, 0, f); }

extern void clear_wire_expr(wire_expr *e, exec_info *f)
 { e->refcnt--;
   if (!e->refcnt)
     { if (!IS_SET(e->flags, WIRE_action))
         { clear_wire_expr(e->u.dep, f); }
       free(e);
     }
 }

extern void add_wire_dep(wire_value *w, exec_info *f)
 { if (!IS_SET(w->flags, WIRE_has_dep))
     { llist_init(&w->u.dep);
       SET_FLAG(w->flags, WIRE_has_dep);
     }
   if (!f->e)
     { f->e = new_wire_expr(f);
       f->e->u.act = &f->curr->act;
     }
   if (IS_SET(w->flags, WIRE_value))
     { f->e->flags = WIRE_susp; }
   else
     { f->e->flags = WIRE_susp | WIRE_val_dir; }
   f->e->refcnt++;
   f->e->valcnt++;
   llist_prepend(&w->u.dep, f->e);
   llist_prepend(&f->curr->dep, w); /* not refcnt'ed */
 }

/********** strict checks ***************************************************/

FLAGS(scr_flags)
   { FIRST_FLAG(SCR_read_ok), /* set if this variable can possibly be read */
     NEXT_FLAG(SCR_write_ok), /* set if this variable can possibly be written */
     NEXT_FLAG(SCR_sub_elem)
     /* When set, variable has elements that must also be checked */
   };

typedef struct strict_check_record
   { ctrl_state *read_frame, *write_frame;
     scr_flags flags;
   } strict_check_record;

static int scr_delete(hash_entry *e, void *dummy)
 { free(e->data.p);
   return 0;
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
   if (!is_parent_frame(r->write_frame, frame, f))
     { exec_error(f, f->err_obj,
              "Reading from a variable that was written in parallel");
     }
   SET_FLAG(r->flags, SCR_sub_elem);
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
 { if (r->read_frame == f->prev)
     { r->read_frame = f->curr;
       RESET_FLAG(r->flags, SCR_write_ok);
     }
   if (r->write_frame == f->prev)
     { r->write_frame = f->curr;
       RESET_FLAG(r->flags, SCR_read_ok);
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
 { if (r->read_frame == cs)
     { SET_FLAG(r->flags, SCR_write_ok); }
   if (r->write_frame == cs)
     { SET_FLAG(r->flags, SCR_read_ok); }
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

/********** critical cycles *************************************************/

static crit_node *new_crit_node(crit_node *parent, void *x)
 /* if parent, then x must be action, else x must be wire_value */
 { crit_node *c;
   action *a = x;
   NEW(c);
   c->refcnt = 1;
   c->parent = parent;
   if (parent)
     { c->a = ACTION_WITH_DIR(a);
       parent->refcnt++;
     }
   else
     { c->a = x; }
   return c;
 }

static void crit_node_free(crit_node *c)
 /* Pre: c->refcnt == 0 */
 { if (c->parent && !(--c->parent->refcnt))
     { crit_node_free(c->parent); }
   free(c);
 }

extern void crit_node_step(action *a, wire_value *w, exec_info *f)
 /* Call this when a change on w has scheduled the production rule a. */
 { crit_node *wc, *ac;
   hash_entry *q;
   q = hash_find(f->crit_map, (char*)w);
   if (q)
     { wc = q->data.p; }
   else
     { wc = new_crit_node(0, w);
       wc->refcnt = 0;
     }
   if (hash_insert(f->crit_map, (char*)a->target.w, &q))
     { ac = q->data.p;
       if (!(--ac->refcnt)) crit_node_free(ac);
     }
   q->data.p = ac = new_crit_node(wc, a);
   if ((q = hash_find(&f->delays, ACTION_WITH_DIR(a))))
     { ac->delay = q->data.i; }
   else
     { ac->delay = 100; }
 }
   
   

/*****************************************************************************/

extern void init_value(int app1, int app2, int app3)
 /* call at startup; pass unused object app indices */
 { app_eval = app1;
   app_range = app2;
   app_assign = app3;
 }
