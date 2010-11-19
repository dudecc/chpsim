/* types.c: types

   Author: Marcel van der Goot
*/

#include <standard.h>
#include "print.h"
#include "parse_obj.h"
#include "types.h"
#include "sem_analysis.h"
#include "expr.h"
#include "exec.h"
#include "statement.h"


/********** printing *********************************************************/

static void print_dummy_type(dummy_type *x, print_info *f)
 { print_obj(x->tps, f); }

static void print_name(name *x, print_info *f)
 { print_string(x->id, f); }

static void print_named_type(named_type *x, print_info *f)
 { if (IS_SET(f->flags, PR_simple_var))
     { if (x->tps && x->tps->class != CLASS_process_def)
         { print_obj(x->tps, f); }
     }
   else
     { print_string(x->id, f); }
 }

static void print_generic_type(generic_type *x, print_info *f)
 { if (!IS_SET(f->flags, PR_simple_var))
     { print_string(token_str(0, x->sym), f); }
 }

static void print_integer_type(integer_type *x, print_info *f)
 { print_char('{', f);
   print_obj(x->l, f);
   print_string("..", f);
   print_obj(x->h, f);
   print_char('}', f);
 }

static void print_field_def(field_def *x, print_info *f)
 { f->pos += var_str_printf(f->s, f->pos, "field %s = [", x->id);
   print_obj(x->l, f);
   print_string("..", f);
   print_obj(x->h, f);
   print_string("];", f);
 }

static void print_symbol_type(symbol_type *x, print_info *f)
 { print_char('{', f);
   print_obj_list(&x->l, f, ", ");
   print_char('}', f);
 }

static void print_array_type(array_type *x, print_info *f)
 { int i, len;
   char *s;
   value_tp vl, vh;
   if (IS_SET(f->flags, PR_simple_var))
     { assert(f->exec);
       eval_expr(x->l, f->exec);
       eval_expr(x->h, f->exec);
       pop_value(&vh, f->exec);
       pop_value(&vl, f->exec);
       len = f->pos - f->rpos;
       s = &f->s->s[f->rpos];
       while (1)
         { f->pos += var_str_printf(f->s, f->pos, "_%v", vstr_val, &vl);
           print_obj(x->tps, f);
           if (int_cmp(&vl, &vh, f->exec) == 0) break;
           int_inc(&vl, f->exec);
           print_string(f->rsep, f);
           f->rpos = f->pos;
           f->pos += var_str_slice_copy(f->s, f->pos, s, len);
         }
       clear_value_tp(&vl, f->exec);
       clear_value_tp(&vh, f->exec);
     }
   else
     { print_string("array [", f);
       print_obj(x->l, f);
       print_string("..", f);
       print_obj(x->h, f);
       print_string("] of ", f);
       print_obj(x->tps, f);
     }
 }

static void print_record_type(record_type *x, print_info *f)
 { llist m;
   int i, len;
   char *s;
   if (IS_SET(f->flags, PR_simple_var))
     { m = x->l;
       len = f->pos - f->rpos;
       s = &f->s->s[f->rpos];
       while (1)
         { print_obj(llist_head(&m), f);
           m = llist_alias_tail(&m);
           if (llist_is_empty(&m)) break;
           print_string(f->rsep, f);
           f->rpos = f->pos;
           f->pos += var_str_slice_copy(f->s, f->pos, s, len);
         }
     }
   else if (IS_SET(f->flags, PR_short))
     { print_string("record { ", f);
       print_obj_list(&x->l, f, " ");
       print_string(" }", f);
     }
   else
     { print_string("record {\n  ", f);
       print_obj_list(&x->l, f, "\n  ");
       print_string("\n}", f);
     }
 }

static void print_record_field(record_field *x, print_info *f)
 { if (IS_SET(f->flags, PR_simple_var))
     { f->pos += var_str_printf(f->s, f->pos, "_%s", x->id);
       print_obj(x->tps, f);
     }
   else
     { f->pos += var_str_printf(f->s, f->pos, "%s: ", x->id);
       print_obj(x->tps, f);
       print_char(';', f);
     }
 }

static void print_union_type(union_type *x, print_info *f)
 { if (IS_SET(f->flags, PR_short))
     { print_string("union { default: ", f);
       print_obj(x->def, f);
       print_string("; ", f);
       print_obj_list(&x->l, f, " ");
       print_string(" }", f);
       return;
     }
   print_string("union {\n  default: ", f);
   print_obj(x->def, f);
   print_string(";\n  ", f);
   print_obj_list(&x->l, f, "\n  ");
   print_string("\n}", f);
 }

static void print_wired_type(wired_type *x, print_info *f)
 { llist m;
   int i, j, len;
   char *s;
   if (IS_SET(f->flags, PR_simple_var))
     { m = x->li;
       j = 0;
       len = f->pos - f->rpos;
       s = &f->s->s[f->rpos];
       while (1)
         { print_obj(llist_head(&m), f);
           m = llist_alias_tail(&m);
           if (j == 0 && llist_is_empty(&m)) { m = x->lo; j++; }
           if (j == 1 && llist_is_empty(&m)) break;
           print_string(f->rsep, f);
           f->rpos = f->pos;
           f->pos += var_str_slice_copy(f->s, f->pos, s, len);
         }
     }
   else
     { print_char('(', f);
       print_obj_list(&x->li, f, ", ");
       print_string("; ", f);
       print_obj_list(&x->lo, f, ", ");
       print_char(')', f);
     }
 }

static void print_union_field(union_field *x, print_info *f)
 { f->pos += var_str_printf(f->s, f->pos, "%s{%s,%s}: ",
                            x->id, x->dnid, x->upid);
   print_obj(x->tps, f);
   print_char(';', f);
 }

static void print_type_def(type_def *x, print_info *f)
 { if (IS_SET(f->flags, PR_simple_var))
     { print_obj(x->tps, f); }
   else
     { f->pos += var_str_printf(f->s, f->pos, "type %s = ", x->id);
       print_obj(x->tps, f);
       print_char(';', f);
     }
 }


/********** semantic analysis ************************************************/

static void *sem_type_def(type_def *x, sem_info *f)
 { declare_id(f, x->id, x);
   x->tps = sem(x->tps, f);
   if (IS_SET(x->flags, DEF_forward)) return x;
   SET_FLAG(x->flags, DEF_forward);
   x->tp = x->tps->tp;
   return x;
 }

static void *sem_dummy_type(dummy_type *x, sem_info *f)
 { x->tps = sem(x->tps, f);
   if (IS_SET(x->flags, DEF_forward)) return x;
   SET_FLAG(x->flags, DEF_forward);
   x->tp = x->tps->tp;
   return x;
 }

static void *sem_named_type(named_type *x, sem_info *f)
 { type_spec *tps;
   meta_parameter *mp;
   if (IS_SET(x->flags, DEF_forward)) return x;
   SET_FLAG(x->flags, DEF_forward);
   tps = find_id(f, x->id, x);
   mp = (meta_parameter*)tps;
   if (tps->class == CLASS_process_def && IS_SET(f->flags, SEM_instance_decl))
     { x->tp.kind = TP_process;
       x->tp.elem.p = (process_def*)tps;
       x->tp.tps = (type_spec*)x;
       return x;
     }
   else if (tps->class == CLASS_meta_parameter && mp->tp.kind == TP_type)
     { x->tp.kind = TP_generic;
       x->tp.elem.tp = &mp->tp;
       x->tp.tps = tps;
       return x;
     }
   else if (tps->class != CLASS_type_def)
     { sem_error(f, x, "%s is not a type", x->id); }
   x->tps = tps;
   x->tp = tps->tp;
   return x;
 }

static void *sem_generic_type(generic_type *x, sem_info *f)
 { if (IS_SET(x->flags, DEF_forward)) return x;
   SET_FLAG(x->flags, DEF_forward);
   switch (x->sym)
     { case KW_bool:
          x->tp.kind = TP_bool;
       break;
       case KW_int:
          x->tp.kind = TP_int;
       break;
       case KW_symbol:
          x->tp.kind = TP_symbol;
       break;
       case KW_type:
          x->tp.kind = TP_type;
       break;
       case 0:
          x->tp.kind = TP_syncport;
       break;
       default:
          assert(!"Unknown type");
       break;
     }
   x->tp.tps = (type_spec*)x;
   return x;
 }

/*extern*/ generic_type generic_int = { 0 }; /* used for strings, etc. */
/*extern*/ generic_type generic_bool = { 0 }; /* used for prs vars */

static void *sem_integer_type(integer_type *x, sem_info *f)
 { if (IS_SET(x->flags, DEF_forward)) return x;
   SET_FLAG(x->flags, DEF_forward);
   x->l = sem(x->l, f);
   x->h = sem(x->h, f);
   if (x->l->tp.kind != TP_int)
     { sem_error(f, x, "Lower bound of integer type is not an integer."); }
   if (x->h->tp.kind != TP_int)
     { sem_error(f, x, "Upper bound of integer type is not an integer."); }
   if (IS_SET(x->l->flags, EXPR_unconst | EXPR_rep))
     { sem_error(f, x->l, "Lower bound of integer type is not constant."); }
   if (IS_SET(x->h->flags, EXPR_unconst | EXPR_rep))
     { sem_error(f, x->h, "Upper bound of integer type is not constant."); }
   x->l = mk_const_expr(x->l, f);
   x->h = mk_const_expr(x->h, f);
   x->tp.kind = TP_int;
   x->tp.tps = (type_spec*)x;
   return x;
 }

static void *sem_field_def(field_def *x, sem_info *f)
 { declare_id(f, x->id, x);
   if (IS_SET(x->flags, DEF_forward)) return x;
   SET_FLAG(x->flags, DEF_forward);
   x->l = sem(x->l, f);
   x->h = sem(x->h, f);
   if (x->l->tp.kind != TP_int)
     { sem_error(f, x, "Left-hand bound of field is not an integer."); }
   if (x->h->tp.kind != TP_int)
     { sem_error(f, x, "Right-hand bound of field is not an integer."); }
   if (IS_SET(x->l->flags, EXPR_unconst | EXPR_rep))
     { sem_error(f, x->l, "Left-hand bound of field is not constant."); }
   if (IS_SET(x->h->flags, EXPR_unconst | EXPR_rep))
     { sem_error(f, x->h, "Right-hand bound of field is not constant."); }
   x->l = mk_const_expr(x->l, f);
   x->h = mk_const_expr(x->h, f);
   return x;
 }

static void import_field_def(field_def *x, sem_info *f)
 { assert(IS_SET(x->flags, DEF_forward));
   declare_id(f, x->id, x);
 }

static int cmp_symbol_name(name *x, name *y)
 { return strcmp(x->id, y->id); }

static void *sem_symbol_type(symbol_type *x, sem_info *f)
 { name *nm;
   llist m;
   m = x->l;
   while (!llist_is_empty(&m))
     { nm = llist_head(&m);
       declare_id(f, nm->id, x);
       m = llist_alias_tail(&m);
     }
   if (IS_SET(x->flags, DEF_forward)) return x;
   SET_FLAG(x->flags, DEF_forward);
   x->tp.kind = TP_symbol;
   x->tp.tps = (type_spec*)x;
   llist_sort(&x->l, (llist_func*)cmp_symbol_name);
   return x;
 }

static void *sem_array_type(array_type *x, sem_info *f)
 { if (IS_SET(x->flags, DEF_forward))
     { /* element type might declare symbol literals: */
       x->tps = sem(x->tps, f);
       return x;
     }
   SET_FLAG(x->flags, DEF_forward);
   x->l = sem(x->l, f);
   x->h = sem(x->h, f);
   if (x->l->tp.kind != TP_int)
     { sem_error(f, x, "Lower bound of array type is not an integer."); }
   if (x->h->tp.kind != TP_int)
     { sem_error(f, x, "Upper bound of array type is not an integer."); }
   if (IS_SET(x->l->flags, EXPR_unconst | EXPR_rep))
     { sem_error(f, x->l, "Lower bound of array type is not constant."); }
   if (IS_SET(x->h->flags, EXPR_unconst | EXPR_rep))
     { sem_error(f, x->h, "Upper bound of array type is not constant."); }
   x->l = mk_const_expr(x->l, f);
   x->h = mk_const_expr(x->h, f);
   x->tps = sem(x->tps, f);
   SET_IF_SET(x->flags, x->tps->tp.tps->flags, EXPR_inherit);
   x->tp.kind = TP_array;
   x->tp.elem.tp = &x->tps->tp;
   x->tp.tps = (type_spec*)x;
   return x;
 }

static void *sem_record_type(record_type *x, sem_info *f)
 { llist m;
   record_field *r;
   /* TODO: check for duplicate field names */
   sem_list(&x->l, f);
   if (IS_SET(x->flags, DEF_forward)) return x;
   SET_FLAG(x->flags, DEF_forward);
   x->tp.kind = TP_record;
   x->tp.tps = (type_spec*)x;
   m = x->l;
   r = llist_head(&m);
   while (!llist_is_empty(&m))
     { r = llist_head(&m);
       llist_prepend(&x->tp.elem.l, &r->tp);
       SET_IF_SET(x->flags, r->tps->flags, EXPR_inherit);
       m = llist_alias_tail(&m);
     }
   llist_reverse(&x->tp.elem.l);
   return x;
 }

static void *sem_record_field(record_field *x, sem_info *f)
 { x->tps = sem(x->tps, f);
   SET_FLAG(x->flags, DEF_forward);
   x->tp = x->tps->tp;
   return x;
 }

static void *sem_union_type(union_type *x, sem_info *f)
 { llist m;
   union_field *r;
   parameter *p;
   var_decl *d;
   /* TODO: check for duplicate field names */
   sem_list(&x->l, f);
   x->def = sem(x->def, f);
   if (IS_SET(x->flags, DEF_forward)) return x;
   SET_FLAG(x->flags, DEF_forward);
   x->tp = x->def->tp;
   x->tp.utps = (type_spec*)x;
   if (x->def->tp.utps)
     { sem_error(f, x, "Default type may not be a union"); }
   else if (x->tp.kind == TP_wire)
     { sem_error(f, x, "Default type may not be wired type"); }
   m = x->l;
   while (!llist_is_empty(&m))
     { r = llist_head(&m);
       if (r->up.p->class == CLASS_process_def)
         { d = r->dn.p->pl->head;
           if (IS_SET(d->flags, EXPR_port) != EXPR_inport)
             { sem_error(f, x, "First port of first conversion process "
                            "%s must be input port", r->dnid);
             }
           if (!IS_SET(d->flags, EXPR_generic) &&
               !type_compatible(&d->tp, &x->tp))
             { sem_error(f, x, "Input type of first conversion process "
                            "%s is incorrect", r->dnid);
             }
           d = r->up.p->pl->tail->head;
           if (IS_SET(d->flags, EXPR_port) != EXPR_outport)
             { sem_error(f, x, "Second port of second conversion process "
                            "%s must be output port", r->upid);
             }
           if (!IS_SET(d->flags, EXPR_generic) &&
               !type_compatible(&d->tp, &x->tp))
             { sem_error(f, x, "Output type of second conversion process "
                            "%s is incorrect", r->upid);
             }
         }
       else
         { if (!type_compatible(&r->up.f->ret->tp, &x->tp))
             { sem_error(f, x, "Return type of second conversion function "
                            "%s is incorrect.", r->upid);
             }
           p = llist_head(&r->dn.f->pl);
           if (!type_compatible(&p->d->tp, &x->tp))
             { sem_error(f, x, "Argument type of first conversion function "
                            "%s is incorrect.", r->dnid);
             }
         }
       m = llist_alias_tail(&m);
     }
   return x;
 }

static void *sem_union_field(union_field *x, sem_info *f)
 { parameter *p;
   var_decl *d;
   x->tps = sem(x->tps, f);
   if (IS_SET(x->flags, DEF_forward))
     { if (x->up.p->class != CLASS_process_def) return x;
       /* Only meta_bindings need to be checked on the second pass */
       if (!llist_is_empty(&x->up.p->ml))
         { if (!x->upmb)
             { sem_error(f, x, "'%s' needs a meta binding.", x->upid); }
           SET_FLAG(f->flags, SEM_meta_binding);
           sem_list(&x->upmb->a, f);
           RESET_FLAG(f->flags, SEM_meta_binding);
           sem_meta_binding_aux(x->up.p, x->upmb, f);
         }
       if (!llist_is_empty(&x->dn.p->ml))
         { if (!x->dnmb)
             { sem_error(f, x, "'%s' needs a meta binding.", x->dnid); }
           SET_FLAG(f->flags, SEM_meta_binding);
           sem_list(&x->dnmb->a, f);
           RESET_FLAG(f->flags, SEM_meta_binding);
           sem_meta_binding_aux(x->dn.p, x->dnmb, f);
         }
       return x;
     }
   SET_FLAG(x->flags, DEF_forward);
   x->tp = x->tps->tp;
   x->up.f = find_id(f, x->upid, x);
   x->dn.f = find_id(f, x->dnid, x);
   if (x->tp.kind == TP_wire)
     { if (x->up.p->class != CLASS_process_def)
         { sem_error(f, x, "'%s' is not the name of a process.", x->upid); }
       if (x->dn.p->class != CLASS_process_def)
         { sem_error(f, x, "'%s' is not the name of a process.", x->dnid); }
     }
   else
     { if (x->up.p->class != CLASS_process_def &&
           (x->up.f->class != CLASS_function_def || !x->up.f->ret))
         { sem_error(f, x, "'%s' is not the name of a function or process.",
                     x->upid);
         }
       if (x->dn.p->class != CLASS_process_def &&
           (x->dn.f->class != CLASS_function_def || !x->dn.f->ret))
         { sem_error(f, x, "'%s' is not the name of a function or process.",
                     x->dnid);
         }
       if (x->up.p->class != x->dn.p->class)
         { sem_error(f, x, "Cannot mix conversion functions and processes"); }
     }
   if (x->up.p->class == CLASS_process_def)
     { if (!x->up.p->cb)
         { sem_error(f, x, "Conversion processes must have a chp body."); }
       if (!x->dn.p->cb)
         { sem_error(f, x, "Conversion processes must have a chp body."); }
       d = x->up.p->pl->head;
       if (IS_SET(d->flags, EXPR_port) != EXPR_inport)
         { sem_error(f, x, "First port of second conversion process "
                        "%s must be input port", x->upid);
         }
       if (!type_compatible(&d->tp, &x->tp))
         { sem_error(f, x, "Input type of second conversion process "
                        "%s is incorrect", x->upid);
         }
       d = x->dn.p->pl->tail->head;
       if (IS_SET(d->flags, EXPR_port) != EXPR_outport)
         { sem_error(f, x, "Second port of first conversion process "
                        "%s must be output port", x->upid);
         }
       if (!type_compatible(&d->tp, &x->tp))
         { sem_error(f, x, "Output type of first conversion process "
                        "%s is incorrect", x->upid);
         }
       if (x->up.p->pl->tail->tail)
         { sem_error(f, x, "Second conversion process has too many ports"); }
       if (x->dn.p->pl->tail->tail)
         { sem_error(f, x, "First conversion process has too many ports"); }
     }
   else
     { if (x->upmb || x->dnmb)
         { sem_error(f, x, "Meta bindings are only for processes"); }
       if (!type_compatible(&x->dn.f->ret->tp, &x->tp))
         { sem_error(f, x, "Return type of first conversion function "
                           "%s is incorrect.", x->dnid);
         }
       p = llist_head(&x->up.f->pl);
       if (!type_compatible(&p->d->tp, &x->tp))
         { sem_error(f, x, "Argument type of second conversion function "
                           "%s is incorrect.", x->upid);
         }
     }
   return x;
 }
 
static int cmp_wire_decl(wire_decl *x, wire_decl *y)
 { return strcmp(x->id, y->id); }

static void *sem_wired_type(wired_type *x, sem_info *f)
 { wire_decl *i, *o;
   /* TODO: check for duplicate wire names */
   sem_list(&x->li, f);
   sem_list(&x->lo, f);
   if (!IS_SET(x->flags, DEF_forward))
     { SET_FLAG(x->flags, DEF_forward);
       llist_sort(&x->li, (llist_func*)cmp_wire_decl);
       llist_sort(&x->lo, (llist_func*)cmp_wire_decl);
       i = llist_head(&x->li);
       o = llist_head(&x->lo);
       if (strcmp(i->id, o->id) > 0) x->type = 0;
       else x->type = 1;
       x->tp.kind = TP_wire;
       x->tp.tps = (type_spec*)x;
       x->nr_var = llist_size(&x->li) + llist_size(&x->lo);
       SET_FLAG(x->flags, EXPR_wire);
     }
   return x;
 }
   
static void import_type_def(type_def *x, sem_info *f)
 { assert(IS_SET(x->flags, DEF_forward));
   declare_id(f, x->id, x);
   if (x->tps->class == CLASS_symbol_type)
     { import(x->tps, f); }
 }

static void import_symbol_type(symbol_type *x, sem_info *f)
 { name *nm;
   llist m;
   assert(IS_SET(x->flags, DEF_forward));
   m = x->l;
   while (!llist_is_empty(&m))
     { nm = llist_head(&m);
       declare_id(f, nm->id, x);
       m = llist_alias_tail(&m);
     }
 }


/********** type checking ****************************************************/

extern int type_compatible(type *tp1, type *tp2)
 /* True iff the reduced forms of tp1 and tp2 are identical (compile-time
    check).
 */
 { llist m1, m2;
   wired_type *w1, *w2;
   wire_decl *d1, *d2, *d;
   int i, n = 0;
   if (tp1->kind != tp2->kind)
     { return 0; }
   switch (tp1->kind)
     { case TP_bool: case TP_int: case TP_symbol:
       case TP_syncport: case TP_type:
          return 1;
       break;
       case TP_array:
          return type_compatible(tp1->elem.tp, tp2->elem.tp);
       break;
       case TP_record:
          m1 = tp1->elem.l; m2 = tp2->elem.l;
          if (llist_size(&m1) != llist_size(&m2)) return 0;
          while (!llist_is_empty(&m1))
            { if (!type_compatible(llist_head(&m1), llist_head(&m2)))
                { return 0; }
              m1 = llist_alias_tail(&m1);
              m2 = llist_alias_tail(&m2);
            }
          return 1;
       break;
       case TP_wire:
          w1 = (wired_type*)tp1->tps;
          w2 = (wired_type*)tp2->tps;
          m1 = w1->li; m2 = w2->li;
          for (i = 0; i < 2; i++)
            { while (!llist_is_empty(&m1) || !llist_is_empty(&m2))
                { if (llist_is_empty(&m1))
                    { n=1; d = llist_head(&m2); }
                  else if (llist_is_empty(&m2))
                    { n=-1; d = llist_head(&m1); }
                  else
                    { d1 = llist_head(&m1); d2 = llist_head(&m2);
                      n = strcmp(d1->id, d2->id);
                      d = (n==1)? d1 : d2;
                    }
                  if (n)
                    { sem_error(0, d, "Wire name %s only present in one port",
                                    d->id);
                    }
                  if (!type_compatible(&d1->tps->tp, &d2->tps->tp))
                    { sem_error(0, d, "Wire %s has a different type "
                                      "in each port", d->id);
                    }
                  m1 = llist_alias_tail(&m1);
                  m2 = llist_alias_tail(&m2);
                }
              m1 = w1->lo; m2 = w2->lo;
            }
          return 1;
       break;
       case TP_generic:
          return tp1->elem.tp == tp2->elem.tp;
       break;
       default: /* other types cannot be compared */
       break;
     }
   return 0;
 }

extern int type_compatible_exec(type *tp1, process_state *ps1,
                                type *tp2, process_state *ps2, exec_info *f)
 /* Execution time type checking.  Verify that the specific types are
    identical ignoring union type differences.  Specifying a process_state
    for each type allows us to look up the real value for generic types.
 */
 { llist m1, m2;
   wired_type *w1, *w2;
   wire_decl *d1, *d2;
   meta_parameter *mp;
   value_tp lv1, hv1, lv2, hv2, *tpv;
   process_state *meta_ps = f->meta_ps;
   integer_type *itps;
   array_type *atps;
   int res, i;
   while (ps1 && tp1->kind == TP_generic)
     { mp = (meta_parameter*)tp1->tps;
       assert(mp->class == CLASS_meta_parameter);
       tpv = &ps1->meta[mp->meta_idx];
       tp1 = &tpv->v.tp->tp;
       ps1 = tpv->v.tp->meta_ps;
     }
   while (ps2 && tp2->kind == TP_generic)
     { mp = (meta_parameter*)tp2->tps;
       assert(mp->class == CLASS_meta_parameter);
       tpv = &ps2->meta[mp->meta_idx];
       tp2 = &tpv->v.tp->tp;
       ps2 = tpv->v.tp->meta_ps;
     }
   if (tp1->kind != tp2->kind)
     { return 0; }
   switch (tp1->kind)
     { case TP_bool: case TP_syncport: case TP_type:
          return 1;
       break;
       case TP_symbol:
          if (    !tp1->tps || tp1->tps->class != CLASS_symbol_type
               || !tp2->tps || tp2->tps->class != CLASS_symbol_type)
            { if (    (!tp1->tps || tp1->tps->class != CLASS_symbol_type)
                   && (!tp2->tps || tp2->tps->class != CLASS_symbol_type))
                { return 1; }
              return 0;
            }
          /* Relies on sem_symbol_type having sorted the list */
          m1 = ((symbol_type*)tp1->tps)->l;
          m2 = ((symbol_type*)tp2->tps)->l;
          while (!llist_is_empty(&m1) || !llist_is_empty(&m2))
            { if (llist_is_empty(&m1) || llist_is_empty(&m2)) return 0;
              if (cmp_symbol_name(llist_head(&m1), llist_head(&m2)))
                { return 0; }
              m1 = llist_alias_tail(&m1);
              m2 = llist_alias_tail(&m2);
            }
          return 1;
       break;
       case TP_int:
          if (    !tp1->tps || tp1->tps->class != CLASS_integer_type
               || !tp2->tps || tp2->tps->class != CLASS_integer_type)
            { if (    (!tp1->tps || tp1->tps->class != CLASS_integer_type)
                   && (!tp2->tps || tp2->tps->class != CLASS_integer_type))
                { return 1; }
              return 0;
            }
          itps = (integer_type*)tp1->tps;
          f->meta_ps = ps1;
          eval_expr(itps->l, f); eval_expr(itps->h, f);
          itps = (integer_type*)tp2->tps;
          f->meta_ps = ps2;
          eval_expr(itps->l, f); eval_expr(itps->h, f);
          pop_value(&hv2, f); pop_value(&lv2, f);
          pop_value(&hv1, f); pop_value(&lv1, f);
          res = !int_cmp(&lv1, &lv2, f) && !int_cmp(&hv1, &hv2, f);
          clear_value_tp(&hv2, f); clear_value_tp(&lv2, f);
          clear_value_tp(&hv1, f); clear_value_tp(&lv1, f);
          f->meta_ps = meta_ps;
          return res;
       break;
       case TP_array:
          assert(tp1->tps && tp1->tps->class == CLASS_array_type);
          assert(tp2->tps && tp2->tps->class == CLASS_array_type);
          /* Sem analysis should have already caught the above cases */
          atps = (array_type*)tp1->tps;
          f->meta_ps = ps1;
          eval_expr(atps->l, f); eval_expr(atps->h, f);
          atps = (array_type*)tp2->tps;
          f->meta_ps = ps2;
          eval_expr(atps->l, f); eval_expr(atps->h, f);
          pop_value(&hv2, f); pop_value(&lv2, f);
          pop_value(&hv1, f); pop_value(&lv1, f);
          res = !int_cmp(&lv1, &lv2, f) && !int_cmp(&hv1, &hv2, f);
          clear_value_tp(&hv2, f); clear_value_tp(&lv2, f);
          clear_value_tp(&hv1, f); clear_value_tp(&lv1, f);
          f->meta_ps = meta_ps;
          return res && type_compatible_exec(tp1->elem.tp, ps1,
                                             tp2->elem.tp, ps2, f);
       break;
       case TP_record:
          m1 = tp1->elem.l; m2 = tp2->elem.l;
          if (llist_size(&m1) != llist_size(&m2)) return 0;
          while (!llist_is_empty(&m1))
            { if (!type_compatible_exec(llist_head(&m1), ps1,
                                        llist_head(&m2), ps2, f))
                { return 0; }
              m1 = llist_alias_tail(&m1);
              m2 = llist_alias_tail(&m2);
            }
          return 1;
       break;
       case TP_wire:
          w1 = (wired_type*)tp1->tps;
          w2 = (wired_type*)tp2->tps;
          m1 = w1->li; m2 = w2->li;
          for (i = 0; i < 2; i++)
            { while (!llist_is_empty(&m1) || !llist_is_empty(&m2))
                { if (llist_is_empty(&m1) || llist_is_empty(&m2)) return 0;
                  d1 = llist_head(&m1); d2 = llist_head(&m2);
                  if (strcmp(d1->id, d2->id)) return 0;
                  if (!type_compatible_exec(&d1->tps->tp, ps1,
                                            &d2->tps->tp, ps2, f))
                    { return 0; }
                  m1 = llist_alias_tail(&m1);
                  m2 = llist_alias_tail(&m2);
                }
              m1 = w1->lo; m2 = w2->lo;
            }
          return 1;
       break;
       case TP_generic:
          return tp1->elem.tp == tp2->elem.tp;
       break;
       default: /* other types cannot be compared */
       break;
     }
   return 0;
 }

static void range_type_def(type_def *x, exec_info *f)
 /* This should only happen with builtin types */
 { value_list *l;
   int i;
   type_spec *tps;
   if (x == builtin_string)
     { if (f->val->rep != REP_array)
         { exec_error(f, f->err_obj, "Type mismatch"); }
       l = f->val->v.l;
       tps = x->tp.elem.tp->tps;
       for (i = 0; i < l->size; i++)
         { range_check(tps, &l->vl[i], f, f->err_obj); }
     }
 }

static void range_generic_type(generic_type *x, exec_info *f)
 { return; }

static void range_integer_type(integer_type *x, exec_info *f)
 { value_tp lval, hval, dval;
   int under, over;
   long n;
   if(f->val->rep != REP_int && f->val->rep != REP_z)
     { exec_error(f, f->err_obj, "Type mismatch"); }
   eval_expr(x->l, f);
   pop_value(&lval, f);
   eval_expr(x->h, f);
   pop_value(&hval, f);
   under = int_cmp(f->val, &lval, f) < 0; /* fval < lval */
   over = int_cmp(&hval, f->val, f) < 0; /* hval < fval */
   if (IS_SET(f->flags, EVAL_bit))
     { dval = int_sub(&hval, &lval, f);
       n = int_log2(&dval, f);
       if (under || over)
         { dval.rep = REP_z;
           dval.v.z = new_z_value(f);
           mpz_setbit(dval.v.z->z, n);
         }
       if (under) *f->val = int_add(f->val, &dval, f);
       if (over) *f->val = int_sub(f->val, &dval, f);
     }
   else if (under || over)
     { exec_error(f, f->err_obj, "Integer %v is out of range {%#v..%#v}",
                  vstr_val, f->val, &lval, &hval);
     }
   else
     { clear_value_tp(&lval, f);
       clear_value_tp(&hval, f);
     }
 }

extern long integer_nr_bits(void *x, exec_info *f)
 /* determine the number of bits needed to specify an integer of type x
  * (x should be an integer type)
  */
 { integer_type *itps = x;
   value_tp lval, hval, dval;
   eval_expr(itps->h, f);
   eval_expr(itps->l, f);
   pop_value(&lval, f);
   pop_value(&hval, f);
   dval = int_sub(&hval, &lval, f);
   return int_log2(&dval, f);
 }

static void range_symbol_type(symbol_type *x, exec_info *f)
 { llist m;
   name *nm;
   const str *s;
   if (f->val->rep != REP_symbol)
     { exec_error(f, f->err_obj, "Type mismatch"); }
   s = f->val->v.s;
   m = x->l;
   while (!llist_is_empty(&m))
     { nm = llist_head(&m);
       if (nm->id == s)
         { break; }
       m = llist_alias_tail(&m);
     }
   if (llist_is_empty(&m))
     { exec_error(f, f->err_obj,
                  "Value '%s' is out of range; should be one of\n%v", s,
                  vstr_obj, x);
     }
 }

static void range_array_type(array_type *x, exec_info *f)
 { value_tp lval, hval;
   value_list *l;
   int i, size;
   type_spec *tps;
   if (f->val->rep != REP_array)
     { exec_error(f, f->err_obj, "Type mismatch"); }
   l = f->val->v.l;
   eval_expr(x->l, f);
   pop_value(&lval, f);
   eval_expr(x->h, f);
   pop_value(&hval, f);
   size = hval.v.i - lval.v.i + 1;
   if (l->size != size)
     { exec_error(f, f->err_obj,
                  "Array value has %d elements instead of %d ([%d..%d])",
                  l->size, size, lval.v.i, hval.v.i);
     }
   tps = x->tps->tp.tps;
   for (i = 0; i < l->size; i++)
     { range_check(tps, &l->vl[i], f, f->err_obj); }
 }

static void range_record_type(record_type *x, exec_info *f)
 { llist m;
   record_field *r;
   value_list *l;
   int i;
   if (f->val->rep != REP_record)
     { exec_error(f, f->err_obj, "Type mismatch"); }
   l = f->val->v.l;
   m = x->l;
   i = 0;
   assert(l->size == llist_size(&m));
   while (!llist_is_empty(&m))
     { r = llist_head(&m);
       range_check(r->tp.tps, &l->vl[i], f, f->err_obj);
       i++;
       m = llist_alias_tail(&m);
     }
 }

/* This happens with templated types */
static void range_meta_parameter(meta_parameter *x, exec_info *f)
 { value_tp *v;
   process_state *meta_ps = f->meta_ps;
   v = &f->meta_ps->meta[x->meta_idx];
   assert(v->rep == REP_type);
   f->meta_ps = v->v.tp->meta_ps;
   range_check(v->v.tp->tp.tps, f->val, f, f->err_obj);
   f->meta_ps = meta_ps;
 }
   

/*****************************************************************************/

/*extern*/ type_def *builtin_string = 0;

extern void fix_builtin_string(void *obj, sem_info *f)
 /* Hack to make the builtin type 'string' work so the length of the string
    doesn't matter. Call when 'string' is visible in f. obj is used for errors.
 */
 { if (builtin_string) return;
   builtin_string = find_id(f, make_str("string"), 0);
   builtin_string->tp.tps = (type_spec*)builtin_string;
 }


/*****************************************************************************/

extern void init_types(void)
 /* call at startup */
 { NEW_STATIC_OBJ(&generic_int, CLASS_generic_type);
   generic_int.sym = KW_int;
   generic_int.tp.kind = TP_int;
   generic_int.tp.tps = (type_spec*)&generic_int;
   NEW_STATIC_OBJ(&generic_bool, CLASS_generic_type);
   generic_bool.sym = KW_bool;
   generic_bool.tp.kind = TP_bool;
   generic_bool.tp.tps = (type_spec*)&generic_bool;

   set_print(dummy_type);
   set_print(name);
   set_print(named_type);
   set_print(generic_type);
   set_print(integer_type);
   set_print(field_def);
   set_print(symbol_type);
   set_print(array_type);
   set_print(record_type);
   set_print(record_field);
   set_print(union_type);
   set_print(union_field);
   set_print(wired_type);
   set_print(type_def);
   set_sem(type_def);
   set_sem(dummy_type);
   set_sem(named_type);
   set_sem(generic_type);
   set_sem(integer_type);
   set_sem(field_def);
   set_sem(symbol_type);
   set_sem(array_type);
   set_sem(record_type);
   set_sem(record_field);
   set_sem(union_type);
   set_sem(union_field);
   set_sem(wired_type);
   set_import(type_def);
   set_import(symbol_type);
   set_import(field_def);
   set_range(type_def);
   set_range(generic_type);
   set_range(integer_type);
   set_range(symbol_type);
   set_range(array_type);
   set_range(record_type);
   set_range(meta_parameter);
 }

