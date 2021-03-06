/* value.c: value representation and expression evaluation
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
#include "parse_obj.h"
#include "value.h"
#include "ifrchk.h"
#include "exec.h"
#include "interact.h"
#include "routines.h"
#include "expr.h"
#include "types.h"

/*extern*/ int app_eval = -1;
/*extern*/ int app_reval = -1;
/*extern*/ int app_range = -1;
/*extern*/ int app_assign = -1;
/*extern*/ int app_conn = -1;

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
 { expr *x = obj;
   value_tp *xv, xval;
   exec_flags flags;
   if (IS_SET(x->flags, EXPR_lvalue))
     { flags = f->flags;
       RESET_FLAG(f->flags, EVAL_assign);
       xv = reval_expr(x, f);
       ASSIGN_FLAG(f->flags, flags, EVAL_assign);
       if (IS_SET(x->flags, EXPR_ifrchk))
         { f->err_obj = x;
           if (IS_SET(x->flags, EXPR_port) && !IS_SET(f->flags, EVAL_probe))
             { strict_check_write(xv, f); }
           else
             { strict_check_read(xv, f); }
         }
       alias_value_tp(&xval, xv, f);
       push_value(&xval, f);
     }
   else
     { APP_OBJ_VFZ(app_eval, obj, f, no_eval); }
 }

static void *no_reval(expr *x, exec_info *f)
 /* called when x has no eval function */
 { error("Internal error: "
         "Object class %s has no reval method", x->class->nm);
   return 0;
 }

extern void *reval_expr(void *obj, exec_info *f)
 /* Evaluate expr *obj */
 { APP_OBJ_PFZ(app_reval, obj, f, no_reval); }

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
       case REP_symbol:
                print_char('`', f);
                print_string(v->v.s, f);
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
                if (!v->v.p->p)
                  { print_string("disconnected port", f); }
                else if (!is_visible(v->v.p->p->wprobe.wps) && v->v.p->p->dec)
                  { print_string("port decomposed through field ", f);
                    print_string(v->v.p->p->dec->id, f);
                  }
                else
                  { print_string("port --> ", f);
                    if (!print_port_connect(v->v.p->p, f))
                      { if (!IS_SET(v->v.p->wprobe.flags, WIRE_value))
                          { print_string(", # = false", f); }
                        else if (v->v.p->v.rep)
                          { print_string(", # = true, data = ", f);
                            print_value_tp(&v->v.p->v, f);
                          }
                        else
                          { print_string(", # = true", f); }
                      }
                  }
       break;
       case REP_process:
                print_string("process ", f);
                print_string(v->v.ps->nm, f);
       break;
       case REP_wwire: case REP_rwire:
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
   var_decl *d;
   int pos = VAR_STR_LEN(&f->scratch);
   process_state *meta_ps, *ps;
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
         if (w == &v->v.p->wprobe) return 1;
         else if (w == v->v.p->wpp) return 1;
         else if (v->v.p->p && !IS_SET(v->v.p->wprobe.flags, PORT_deadwu))
           { if (w == &v->v.p->p->wprobe) return 1;
             if (!v->v.p->p->dec) return 0;
             var_str_printf(&f->scratch, pos, ".%s", v->v.p->p->dec->id);
             meta_ps = f->meta_ps;
             f->meta_ps = ps = v->v.p->p->wprobe.wps;
             d = llist_idx(&ps->p->pl, 0);
             if (ps->var[d->var_idx].v.p == v->v.p->p)
               { d = llist_idx(&ps->p->pl, 1); }
             res = _print_wire_value(&ps->var[d->var_idx], &d->tp, w, f);
             f->meta_ps = meta_ps;
             return res;
           }
         else if (v->v.p->nv)
           { assert(v->v.p->nv != v);
             if (v->v.p->dec)
               { var_str_printf(&f->scratch, pos, ".%s", v->v.p->dec->id);
                 meta_ps = f->meta_ps;
                 f->meta_ps = ps = v->v.p->wprobe.wps;
                 res = _print_wire_value(v->v.p->nv,&v->v.p->dec->tps->tp,w,f);
                 f->meta_ps = meta_ps;
                 return res;
               }
             else
               { return _print_wire_value(v->v.p->nv, tp, w, f); }
           }
       return 0;
       case REP_wwire: case REP_rwire:
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
   wire_expr *e;
   while (IS_SET(w->flags, WIRE_virtual))
     { assert(llist_size(&w->u.dep) == 1);
       e = llist_head(&w->u.dep);
       assert(IS_SET(e->flags, WIRE_x));
       w = e->u.hold;
     }
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
   exec_info_init_eval(&g, ps);
   print_wire_exec(w, &g);
   print_string(g.scratch.s, f);
   exec_info_term(&g);
 }

/* var_str_func2_tp: use as first arg for %V */
extern int vstr_wire(var_string *s, int pos, void *w, void *ps)
 /* Print a name for w in the reference frame of ps to s[pos...] */
 { exec_info g;
   exec_info_init_eval(&g, ps);
   print_wire_exec(w, &g);
   var_str_printf(s, pos, "%s", g.scratch.s);
   exec_info_term(&g);
   return 0;
 }

/* var_str_func2_tp: use as first arg for %V */
extern int vstr_wire_context(var_string *s, int pos, void *w, void *ps)
 /* Print both w and the name of the reference frame of ps to s[pos...]
  * This also handles the case when the real reference frame is hidden
  */
 { exec_info g;
   var_decl *d;
   port_value *pv;
   const char *ext;
   exec_info_init_eval(&g, ps);
   print_wire_exec(w, &g);
   if (is_visible(ps))
     { var_str_printf(s, pos, "%s of process %s", g.scratch.s, g.meta_ps->nm); }
   else
     { ext = strchr(g.scratch.s, '.'); /* "Remove" local port name */
       if (!ext) ext = ""; /* Sometimes the local port name is all there is */
       d = llist_idx(&g.meta_ps->p->pl, 0);
       if (g.meta_ps->var[d->var_idx].rep != REP_port)
         { d = llist_idx(&g.meta_ps->p->pl, 1); }
       assert(g.meta_ps->var[d->var_idx].rep == REP_port);
       pv = g.meta_ps->var[d->var_idx].v.p; /* The "real" port */
       assert(pv->dec);
       if (w == &pv->wprobe)
         { var_str_printf(s, pos, "%v%s of process %s", vstr_port, pv->p,
                          ext, pv->p->wprobe.wps->nm);
         }
       else
         { var_str_printf(s, pos, "%v.%s%s of process %s", vstr_port, pv->p,
                          pv->dec->id, ext, pv->p->wprobe.wps->nm);
         }
     }
   exec_info_term(&g);
   return 0;
 }

/* var_str_func2_tp: use as first arg for %V */
extern int vstr_wire_context_short(var_string *s, int pos, void *w, void *ps)
 /* Same as above, but use shorthand ':' notation */
 { exec_info g;
   var_decl *d;
   port_value *pv;
   char *ext;
   exec_info_init_eval(&g, ps);
   print_wire_exec(w, &g);
   if (is_visible(ps))
     { var_str_printf(s, pos, "%s:%s", g.meta_ps->nm, g.scratch.s); }
   else
     { ext = strchr(g.scratch.s, '.'); /* "Remove" local port name */
       if (!ext) ext = ""; /* Sometimes the local port name is all there is */
       d = llist_idx(&g.meta_ps->p->pl, 0);
       if (g.meta_ps->var[d->var_idx].rep != REP_port)
         { d = llist_idx(&g.meta_ps->p->pl, 1); }
       assert(g.meta_ps->var[d->var_idx].rep == REP_port);
       pv = g.meta_ps->var[d->var_idx].v.p; /* The "real" port */
       assert(pv->dec);
       if (w == &pv->wprobe)
         { var_str_printf(s, pos, "%s:%v%s", pv->p->wprobe.wps->nm,
                          vstr_port, pv->p, ext);
         }
       else
         { var_str_printf(s, pos, "%s:%v.%s%s", pv->p->wprobe.wps->nm,
                          vstr_port, pv->p, pv->dec->id, ext);
         }
     }
   exec_info_term(&g);
   return 0;
 }

static void print_port_exec(port_value *p, exec_info *f)
 /* Pre: f->meta_ps == p->wprobe.wps: print the name of port p to f->scratch */
 { llist l = p->wprobe.wps->p->pl;
   var_decl *d;
   value_tp *var = p->wprobe.wps->var;
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
   exec_info_init_eval(&g, p->wprobe.wps);
   print_port_exec(p, &g);
   print_string(g.scratch.s, f);
   exec_info_term(&g);
 }

/* var_str_func_tp: use as first arg for %v */
extern int vstr_port(var_string *s, int pos, void *p)
 /* print the name of port p to s[pos...] */
 { exec_info g;
   exec_info_init_eval(&g, ((port_value*)p)->wprobe.wps);
   print_port_exec(p, &g);
   var_str_printf(s, pos, "%s", g.scratch.s);
   exec_info_term(&g);
   return 0;
 }

extern int print_port_connect(port_value *p, print_info *f)
/* Print (process name):(port_name), where port name may include one or
 * more levels of decomposition.  If disconnected, print "disconnected"
 * and return 1, otherwise return 0;
 */
 { exec_info g;
   value_tp *pv;
   var_decl *d;
   char *rs, *is;
   process_state *ps;
   if (!p)
     { print_string("(disconnected)", f);
       return 1;
     }
   ps = p->wprobe.wps;
   if (!is_visible(ps))
     { assert(!p->dec);
       d = llist_idx(&ps->p->pl, 0);
       pv = &ps->var[d->var_idx];
       if (pv->rep != REP_port || !pv->v.p->dec)
         { d = llist_idx(&ps->p->pl, 1);
           pv = &ps->var[d->var_idx];
         }
       assert(pv->rep == REP_port && pv->v.p->dec);
       if (print_port_connect(pv->v.p->p, f)) return 1;
       print_char('.', f);
       print_string(pv->v.p->dec->id, f);
       exec_info_init_eval(&g, ps);
       print_port_exec(p, &g);
       rs = strchr(g.scratch.s, '.');
       is = strchr(g.scratch.s, '[');
       if (rs && (!is || is > rs))
         { print_string(rs, f); }
       else if (is && (!rs || rs > is))
         { print_string(is, f); }
       exec_info_term(&g);
     }
   else
     { print_string(ps->nm, f);
       print_char(':', f);
       print_port_value(p, f);
     }
   return 0;
 }

/* var_str_func_tp: use as first arg for %v */
extern int vstr_port_connect(var_string *s, int pos, void *p)
 /* print p to s[pos...] (see print_port_connect) */
 { print_info f;
   print_info_init(&f);
   f.s = s; f.pos = pos;
   print_port_connect(p, &f);
   VAR_STR_X(f.s, f.pos) = 0;
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
   p->wprobe.wps = ps;
   p->wpp = 0;
   p->p = 0;
   p->v.rep = REP_none;
   p->dec = 0;
   p->nv = 0;
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
   switch (v->rep)
     { case REP_z:
         vz = v->v.z;
         vz->refcnt--;
         if (!vz->refcnt)
           { mpz_clear(vz->z);
             free(vz);
           }
       break;
       case REP_array: case REP_record:
         vl = v->v.l;
         vl->refcnt--;
         if (!vl->refcnt)
           { for (i = 0; i < vl->size; i++)
               { clear_value_tp(&vl->vl[i], f); }
             free(vl);
           }
       break;
       case REP_union:
         vu = v->v.u;
         vu->refcnt--;
         if (!vu->refcnt)
           { clear_value_tp(&vu->v, f);
             free(vu);
           }
       break;
       case REP_process:
         ps = v->v.ps;
         ps->refcnt--;
         if (!ps->refcnt)
           { free_process_state(ps, f); }
       break;
       case REP_port:
         p = v->v.p;
         p->wprobe.refcnt--;
         if (!p->wprobe.refcnt)
           { free_port_value(p, f); }
       break;
       case REP_type:
         tp = v->v.tp;
         tp->refcnt--;
         if (!tp->refcnt)
           { ps = tp->meta_ps;
             ps->refcnt--;
             if (!ps->refcnt)
               { free_process_state(ps, f); }
             free(tp);
           }
       break;
       case REP_wwire: case REP_rwire:
         w = v->v.w;
         w->refcnt--;
         while (!w->refcnt)
           { if (!IS_SET(w->flags, WIRE_forward))
               { free(w); break; }
             ww = w->u.w;
             free(w);
             w = ww;
             w->refcnt--;
           }
       break;
       case REP_cnt:
         c = v->v.c;
         c->refcnt--;
         if (!c->refcnt)
           { free(c); }
       break;
       default:
       break;
     }
   v->rep = REP_none;
 }

extern void copy_value_tp(value_tp *w, value_tp *v, exec_info *f)
 /* Copy v to w, allocating new memory */
 { int i;
   value_list *vl, *wl;
   value_union *vu, *wu;
   switch (v->rep)
     { case REP_z:
         w->rep = REP_z;
         w->v.z = new_z_value(f);
         mpz_set(w->v.z->z, v->v.z->z);
       return;
       case REP_array: case REP_record:
         w->rep = v->rep;
         vl = v->v.l;
         w->v.l = wl = new_value_list(vl->size, f);
         for (i = 0; i < vl->size; i++)
           { copy_value_tp(&wl->vl[i], &vl->vl[i], f); }
       return;
       case REP_union:
         w->rep = REP_union;
         vu = v->v.u;
         w->v.u = wu = new_value_union(f);
         wu->d = vu->d;
         wu->f = vu->f;
         copy_value_tp(&wu->v, &vu->v, f);
       return;
       /* No new memory for the remaining cases */
       case REP_process:               *w = *v; w->v.ps->refcnt++;       return;
       case REP_port:                  *w = *v; w->v.p->wprobe.refcnt++; return;
       case REP_wwire: case REP_rwire: *w = *v; w->v.w->refcnt++;        return;
       case REP_type:                  *w = *v; w->v.tp->refcnt++;       return;
       case REP_cnt:                   *w = *v; w->v.c->refcnt++;        return;
       default:                        *w = *v;                          return;
     }
 }

extern void alias_value_tp(value_tp *w, value_tp *v, exec_info *f)
 /* Copy v to w, sharing the memory */
 { *w = *v;
   switch (v->rep)
     { case REP_z:                       w->v.z->refcnt++;         return;
       case REP_array: case REP_record:  w->v.l->refcnt++;         return;
       case REP_union:                   w->v.u->refcnt++;         return;
       case REP_process:                 w->v.ps->refcnt++;        return;
       case REP_port:                    w->v.p->wprobe.refcnt++;  return;
       case REP_wwire: case REP_rwire:   w->v.w->refcnt++;         return;
       case REP_type:                    w->v.tp->refcnt++;        return;
       case REP_cnt:                     w->v.c->refcnt++;         return;
       default:                                                    return;
     }
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
           wu->f = vu->f;
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

static void update_rule(wire_expr_flags val, wire_expr_action u, exec_info *f)
 { action_flags af;
   switch(val & WIRE_action)
     { case WIRE_pu: case WIRE_pd:
         /* Just mark the changes for now, and add the action to the queue */
         af = IS_SET(val, WIRE_pu)? ACTION_up_nxt : ACTION_dn_nxt;
         if (IS_SET(val, WIRE_value)) SET_FLAG(u.act->flags, af);
         else RESET_FLAG(u.act->flags, af);
         if (!IS_SET(u.act->flags, ACTION_check))
           { llist_prepend(&f->check, u.act);
             SET_FLAG(u.act->flags, ACTION_check);
           }
       return;
       case WIRE_susp: /* suspended thread, schedule immediately */
         if (!IS_SET(val, WIRE_value))
           { assert(!"Action has become unscheduled"); }
         if (!IS_SET(u.act->flags, ACTION_sched))
           { SET_FLAG(u.act->flags, ACTION_atomic);
             action_sched(u.act, f);
             RESET_FLAG(u.act->flags, ACTION_atomic);
           }
       return;
       case WIRE_hu:
         if (IS_SET(val, WIRE_value))
           { RESET_FLAG(u.hold->flags, WIRE_held_up);
             if (IS_SET(u.hold->flags, WIRE_wait))
               { write_wire(1, u.hold, f); }
             RESET_FLAG(u.hold->flags, WIRE_wait);
           }
         else
           { SET_FLAG(u.hold->flags, WIRE_held_up); }
       return;
       case WIRE_hd:
         if (IS_SET(val, WIRE_value))
           { RESET_FLAG(u.hold->flags, WIRE_held_dn);
             if (IS_SET(u.hold->flags, WIRE_wait))
               { write_wire(0, u.hold, f); }
             RESET_FLAG(u.hold->flags, WIRE_wait);
           }
         else
           { SET_FLAG(u.hold->flags, WIRE_held_dn); }
       return;
       case WIRE_xu:
         if (IS_SET(val, WIRE_value))
           { write_wire(1, u.hold, f); }
       return;
       case WIRE_xd:
         if (IS_SET(val, WIRE_value))
           { write_wire(0, u.hold, f); }
       return;
       case WIRE_x:
         write_wire(IS_SET(val, WIRE_value)? 1 : 0, u.hold, f);
       return;
       case WIRE_vc:
         if (IS_SET(val, WIRE_value))
           { clear_value_tp(u.val, f); }
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
   llist m;
   if (!e->undefcnt) /* Nothing undefined, make this fast */
     { if (IS_SET(e->flags, WIRE_trigger))
         { e->valcnt -= (~val ^ (e->flags >> WIRE_vd_shft)) & WIRE_value;
           /* Here we have made use of the fact that WIRE_value == 1 */
           if (e->valcnt > 0) return;
           e->valcnt = e->refcnt;
           if (IS_SET(e->flags, WIRE_xor))
             { e->flags ^= WIRE_value | WIRE_val_dir; }
         }
       else if (IS_SET(e->flags, WIRE_xor))
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
         { update_rule(e->flags, e->u, f); }
       else if (IS_SET(e->flags, WIRE_llist))
         { m = e->u.ldep;
           while (!llist_is_empty(&m))
             { update_wire_expr(e->flags, llist_head(&m), f);
               m = llist_alias_tail(&m);
             }
         }
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
     { update_rule(e->flags, e->u, f); }
   else if (IS_SET(e->flags, WIRE_llist))
     { m = e->u.ldep;
       while (!llist_is_empty(&m))
         { update_wire_expr(e->flags, llist_head(&m), f);
           m = llist_alias_tail(&m);
         }
     }
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
     { if (IS_SET(f->user->flags, USER_random))
         { report(f->user, "(watch) %V %s",
                  vstr_wire_context_short, w, f->meta_ps,
                  IS_SET(w->flags, WIRE_value)? "up" : "down");
         }
       else
         { mpz_init(time);
           mpz_fdiv_q_2exp(time, f->time, 1);
           report(f->user, "(watch) %V %s at time %v",
                  vstr_wire_context_short, w, f->meta_ps,
                  IS_SET(w->flags, WIRE_value)? "up" : "down", vstr_mpz, time);
           mpz_clear(time);
         }
     }
   if (IS_SET(f->user->flags, USER_critical))
     { new_crit_node(w, val, f); }
   if (IS_SET(w->flags, WIRE_has_dep))
     { ASSIGN_FLAG(old, w->flags, WIRE_value);
       /* old is now formatted for update_wire_expr */
       m = w->u.dep;
       while (!llist_is_empty(&m))
         { e = llist_head(&m);
           update_wire_expr(old, e, f);
           m = llist_alias_tail(&m);
           f->ecount++;
         }
       run_checks(w, f);
     }
   if (IS_SET(f->user->flags, USER_critical))
     { f->crit = f->crit->parent; }
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
 { value_tp *xv, *w = f->val;
   if (!val->rep)
     { exec_error(f, x, "Unknown value in assignment to %v", vstr_obj, x); }
   SET_FLAG(f->flags, EVAL_assign);
   if (IS_SET(x->flags, EXPR_lvalue))
     { xv = reval_expr(x, f);
       if (IS_SET(x->flags, EXPR_ifrchk))
         { f->err_obj = x;
           strict_check_write(xv, f);
         }
       clear_value_tp(xv, f);
       copy_and_clear(xv, val, f);
     }
   else
     { f->val = val;
       APP_OBJ_VFZ(app_assign, x, f, no_assign);
       f->val = w;
     }
   RESET_FLAG(f->flags, EVAL_assign);
 }

static void *no_connect(expr *x, exec_info *f)
 /* called when x has no connect function */
 { error("Internal error: "
         "Object class %s has no connect method", x->class->nm);
   return 0;
 }

extern value_tp *connect_expr(void *obj, exec_info *f)
 /* Evaluate expr *obj */
 { return APP_OBJ_PFZ(app_conn, obj, f, no_connect); }

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
 /* If p, the created wire_expr has p as a parent and returns zero.
  * Otherwise returns the created expression.
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
       assert(v.rep == REP_wwire || v.rep == REP_rwire);
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


/*****************************************************************************/

extern void init_value(int app1, int app2, int app3, int app4, int app5)
 /* call at startup; pass unused object app indices */
 { app_eval = app1;
   app_reval = app2;
   app_range = app3;
   app_assign = app4;
   app_conn = app5;
 }
