/* value.h: value representation and expression evaluation
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

#ifndef VALUE_H
#define VALUE_H

#include <standard.h>
#include <llist.h>
#include <hash.h>
#include <var_string.h>
#include "parse_obj.h"
#include "print.h"

/********** value representation *********************************************/

typedef enum valrep_tp
   { REP_none = 0, /* no value assigned */
     REP_bool, REP_int, REP_z, REP_symbol, REP_array, REP_record,
     REP_process, REP_port, REP_union, REP_wire, REP_cnt, REP_type
   } valrep_tp; /* representation */

/* Limits for use of REP_int instead of REP_z */
#define MIN_INT_REP LONG_MIN /* -2^n, if long is n+1 bits */
#define MAX_INT_REP LONG_MAX /* 2^n - 1, if long is n+1 bits */
#define INT_REP_SIGN LONG_MIN /* sign bit */
#define INT_REP_HI_BYTE ((ulong)(- (long)(((ulong)INT_REP_SIGN) >> 7)  ))
				/* high byte mask */
#define INT_REP_NRBITS LONG_NRBITS /* nr bits */
/* limits to avoid ridiculous operations: */
#define INT_REP_MAXBITS 10000 /* highest bit position we'll consider */
#define INT_REP_MAXEXP 2000 /* largest exponent in x^y */
#define ARRAY_REP_MAXSIZE 65536 /* largest array size */
#define MAX_COUNT 65535 /* maximum counter value */

#define ALLOW_SPLIT_PRS 0 /* allow multiple pr's with the same target */

typedef struct value_list value_list;
typedef struct port_value port_value;
typedef struct value_union value_union;
typedef struct type_value type_value;
typedef struct wire_value wire_value;
typedef struct counter_value counter_value;
typedef struct z_value z_value;
typedef struct process_state process_state; /* defined in exec.h */

typedef struct value_tp value_tp;
struct value_tp
   { valrep_tp rep;
     union { long i; // REP_bool, REP_int
             z_value *z; // REP_z (arbitrary size int)
	     const str *s; // REP_symbol
	     value_list *l; // REP_array, REP_record
	     port_value *p; // REP_port
	     value_union *u; // REP_union
	     wire_value *w; // REP_wire
	     counter_value *c; // REP_cnt
	     process_state *ps; // REP_process
	     type_value *tp; // REP_type
	   } v;
   };

struct value_list
   { int refcnt; /* reference count */
     long size;
     value_tp vl[1]; /* actually: value_tp vl[size]; */
   };

/* only used for port values */
struct value_union
   { int refcnt; /* reference count */
     function_def *d;
     union_field *f;
     value_tp v;
   };

struct type_value
   { int refcnt; /* reference count */
     type tp;
     process_state *meta_ps;
   };

struct z_value
   { int refcnt;
     mpz_t z; /* gmp.h */
   };

FLAGS(wire_flags)
   { FIRST_FLAG(WIRE_value), /* boolean value of the wire */
     NEXT_FLAG(WIRE_undef), /* true if value should be REP_none */
     NEXT_FLAG(WIRE_forward), /* if set, forward reference to u.w */
     NEXT_FLAG(WIRE_has_writer), /* does a process write this wire */
     NEXT_FLAG(WIRE_has_dep), /* u.dep must be updated on change */
     NEXT_FLAG(WIRE_is_probe), /* not a real wire, but a port value probe */
     NEXT_FLAG(WIRE_held_up), /* delay_hold is stopping upward transition */
     NEXT_FLAG(WIRE_held_dn), /* delay_hold is stopping downward transition */
     NEXT_FLAG(WIRE_wait), /* a transition is pending on hold removal */
     NEXT_FLAG(WIRE_watch), /* print all transitions of this wire */
     NEXT_FLAG(WIRE_reset), /* set if wire is held at its initial value */
     WIRE_val_mask = WIRE_undef | WIRE_value
   };

struct wire_value
   { int refcnt;
     wire_flags flags;
     struct action *wframe;
     union { wire_value *w; /* Used during instantiation */
             llist dep; /* Used during execution */
           } u;
   };

FLAGS(wire_expr_flags)
   { /* WIRE_value is the same flag from wire_flags */
     /* WIRE_undef is the same flag from wire_flags */
     _WIRE_EXPR_dummy = 3, /* hack to start from the right place */
     NEXT_FLAG(WIRE_xor), /* if set, ignore valcnt (always change) */
     NEXT_FLAG(WIRE_val_dir), /* if set, value high decreases valcnt */
     NEXT_FLAG(WIRE_pd), /* expression is a pull-down action */
     NEXT_FLAG(WIRE_pu), /* expression is a pull-up action */
     NEXT_FLAG(WIRE_susp), /* expression is a suspended action */
     NEXT_FLAG(WIRE_hold), /* expression is a hold */
     WIRE_hd = WIRE_pd | WIRE_hold, /* hold on an upward transition */
     WIRE_hu = WIRE_pu | WIRE_hold, /* hold on an upward transition */
     WIRE_action = WIRE_pu | WIRE_pd | WIRE_susp | WIRE_hold /* is an action */
   };

#define WIRE_vd_shft 3 /* shift to align WIRE_val_dir with WIRE_value */

typedef struct action action; /* defined in exec.h */

typedef struct wire_expr wire_expr;
struct wire_expr
   { int refcnt;
     wire_expr_flags flags;
     int valcnt, undefcnt;
     union { wire_expr *dep;
             action *act;
             wire_value *hold;
           } u;
   };
/* A wire_expr can emulate xor, xnor, and, nand, or & nor gates of any size.
 * wire_expr make no reference to the expressions that they are a depend on,
 * or even the number of such expressions.  Instead the expressions they
 * depend on must refer to them.  A wire_expr may also refer to another
 * wire_expr that depends on it, and wire_values may refer to any number of
 * dependents.  Every time a wire_value changes, every dependent wire_expr
 * is updated.  If WIRE_xor is set, updating a wire_expr always changes its
 * value.  Otherwise, valcnt is altered according to WIRE_val_dir, and the
 * value is changed according to whether (valcnt >= 0) changes.
 *
 * xor/xnor: WIRE_xor set
 * and/nand: WIRE_xor reset, WIRE_val_dir set
 * or/nor:   WIRE_xor reset, WIRE_val_dir reset
 *
 * A wire_expr is also used for counters, which is when valcnt is allowed
 * to go negative.
 */

struct counter_value
  { int refcnt, cnt;
    parse_obj *err_obj;
    llist dep; /* llist(wire_expr) */
  };

struct port_value
   { wire_value wprobe;
     port_value *p; /* other port */
     value_tp v; /* value, only used for REP_inport */
     process_state *ps; /* process that communicates on this port */
     union_field *dec; /* is this part of a decomposition process? */
     value_tp *nv; /* used for disconnected ports of meta processes */
   };

/********** printing *********************************************************/
typedef struct ctrl_state ctrl_state; /* defined in exec.h */

extern void print_value_tp(value_tp *v, print_info *f);
 /* Print v to f */

/* var_str_func_tp: use as first arg for %v */
extern int vstr_mpz(var_string *s, int pos, void *z);
 /* print mpz_t z to s[pos...] */

/* var_str_func_tp: use as first arg for %v */
extern int vstr_val(var_string *s, int pos, void *val);
 /* print value_tp *val to s[pos...] */

/* var_str_func_tp: use as first arg for %v */
extern int print_string_value(var_string *s, int pos, value_tp *val);
 /* Pre: val is an array of {0..255}.
    Print val as a (0-terminated) string to s[pos...].
    Return is nr chars printed.
 */

extern void print_wire_exec(wire_value *w, struct exec_info *f);
 /* Print a name for w in the reference frame of f->meta_ps to f->scratch */

extern void print_wire_value(wire_value *w, process_state *ps, print_info *f);
 /* Print a name for w in the reference frame of ps to f */

/* var_str_func2_tp: use as first arg for %V */
extern int vstr_wire(var_string *s, int pos, void *w, void *ps);
 /* Print a name for w in the reference frame of ps to s[pos...] */

/* var_str_func2_tp: use as first arg for %V */
extern int vstr_wire_context(var_string *s, int pos, void *w, void *ps);
 /* Print both w and the name of the reference frame of ps to s[pos...]
  * This also handles the case when the real reference frame is hidden
  */

/* var_str_func2_tp: use as first arg for %V */
extern int vstr_wire_context_short(var_string *s, int pos, void *w, void *ps);
 /* Same as above, but use shorthand ':' notation */

extern void print_port_value(port_value *p, print_info *f);
 /* print the name of port p to f */

/* var_str_func_tp: use as first arg for %v */
extern int vstr_port(var_string *s, int pos, void *p);
 /* print the name of port p to s[pos...] */

extern int print_port_connect(port_value *p, print_info *f);
/* Print (process name):(port_name), where port name may include one or
 * more levels of decomposition.  If disconnected, print "disconnected"
 * and return 1, otherwise return 0;
 */

/* var_str_func_tp: use as first arg for %v */
extern int vstr_port_connect(var_string *s, int pos, void *p);
 /* print p to s[pos...] (see print_port_connect) */

/********** expression evaluation ********************************************/

extern void eval_expr(void *obj, struct exec_info *f);
 /* Evaluate expr *obj */

extern void *reval_expr(void *obj, struct exec_info *f);
 /* Evaluate lvslue expr *obj, return pointer to stored value */

extern void push_value(value_tp *v, struct exec_info *f);
 /* push *v on stack (direct copy: top = *v) */

extern void pop_value(value_tp *v, struct exec_info *f);
 /* store to of stack in *v (direct copy: *v = top) and pop stack */

extern void push_repval(value_tp *v, ctrl_state *cs, struct exec_info *f);
 /* push *v on repval stack of cs (direct copy: top = *v) */

extern void pop_repval(value_tp *v, ctrl_state *cs, struct exec_info *f);
 /* store to of repval stack of cs in *v (direct copy: *v = top), pop stack */

extern value_list *new_value_list(int size, struct exec_info *f);
 /* allocate new list with given size */

extern value_union *new_value_union(struct exec_info *f);
 /* allocate new list with given size */

extern port_value *new_port_value(process_state *ps, struct exec_info *f);
 /* allocate new port_value */

extern wire_value *new_wire_value(token_tp x, struct exec_info *f);
 /* allocate new wire_value */

extern counter_value *new_counter_value(long x, struct exec_info *f);
 /* allocate new counter_value */

extern void wire_fix(wire_value **w, struct exec_info *f);
 /* remove wire forwarding from w */

extern z_value *new_z_value(struct exec_info *f);
 /* allocate a new z_value, with initial value 0 */

extern type_value *new_type_value(struct exec_info *f);
 /* allocate new type_value */

extern void clear_value_tp(value_tp *v, struct exec_info *f);
 /* Reset v to have no value. If v has allocated memory (e.g., for array),
    free that if there are no longer any references to it.
 */

extern void copy_value_tp(value_tp *w, value_tp *v, struct exec_info *f);
 /* Copy v to w, allocating new memory */

extern void copy_and_clear(value_tp *w, value_tp *v, struct exec_info *f);
 /* Same as: copy_value_tp(w, v, f); clear_value_tp(v, f);
    but more efficient.
 */

extern void alias_value_tp(value_tp *w, value_tp *v, struct exec_info *f);
 /* Copy v to w, sharing the memory */

extern int app_eval;
#define set_eval(C) set_app(CLASS_ ## C, app_eval, (obj_func*)eval_ ## C)
 /* To set eval_abc as eval function for class abc */
#define set_eval_cp(C,D) set_app(CLASS_ ## C, app_eval, (obj_func*)eval_ ## D)
 /* Used if C is a copy of D, and uses the same eval function */

extern int app_reval;
#define set_reval(C) set_app(CLASS_ ## C, app_reval, (obj_func*)reval_ ## C)
 /* To set reval_abc as reval function for class abc */
#define set_reval_cp(C,D) set_app(CLASS_ ## C, app_reval, (obj_func*)reval_ ## D)
 /* Used if C is a copy of D, and uses the same reval function */


/********** range checks *****************************************************/

extern void range_check
 (type_spec *tps, value_tp *val, struct exec_info *f, void *obj);
 /* Verify that val is a valid value for tps. obj is used for error msgs */

extern int app_range;
#define set_range(C) set_app(CLASS_ ## C, app_range, (obj_func*)range_ ## C)
 /* To set range_abc as range function for class abc.
    range_abc should verify that f->val is a valid value for type abc.
    If not, then give an error messag with f->err_obj.
 */
#define set_range_cp(C,D) set_app(CLASS_##C, app_range, (obj_func*)range_##D)
 /* Used if C is a copy of D, and uses the same range function */


/********** assignment ******************************************************/

extern void write_wire(int val, wire_value *w, struct exec_info *f);
 /* Update the value stored in w according to val */

extern void update_counter(int dir, counter_value *c, struct exec_info *f);
 /* Increment/decrement the counter c according to dir */

extern void force_value(value_tp *xval, expr *x, struct exec_info *f);
 /* Assign an empty value to xval according to the type of x,
    i.e., an array/record value with a value_list without values.
 */

extern void force_value_array(value_tp *xval, type *tp, struct exec_info *f);
 /* Like the above, but assumes tp is an array type */

extern void assign(expr *x, value_tp *val, struct exec_info *f);
 /* Assign x := val where x is an lvalue.
    Note: you should do a range_check() first.
 */

extern value_tp *connect_expr(void *obj, struct exec_info *f);
 /* Evaluate expr *obj */

extern int app_assign;
#define set_assign(C) set_app(CLASS_ ## C, app_assign, (obj_func*)assign_ ## C)
 /* To set assign_abc as assign function for class abc */
#define set_assign_cp(C,D) set_app(CLASS_##C, app_assign, (obj_func*)assign_##D)
 /* Used if C is a copy of D, and uses the same assign function */

extern int app_conn;
#define set_connect(C) set_app(CLASS_ ## C, app_conn, (obj_func*)connect_ ## C)
 /* To set connect_abc as connect function for class abc */
#define set_connect_from_reval(C) set_app(CLASS_ ## C, app_conn, \
                                          (obj_func*)reval_ ## C)
 /* To set connect_abc as connect function for class abc */
#define set_connect_cp(C,D) set_app(CLASS_##C, app_conn, (obj_func*)connect_##D)
 /* Used if C is a copy of D, and uses the same connect function */

/********** creating wire expressions ***************************************/

extern wire_expr *new_wire_expr(struct exec_info *f);
 /* Note that refcnt is initially zero */

extern void we_add_dep(wire_expr *p, wire_expr *e, struct exec_info *f);
 /* add e as a child of p, update the value of p */

extern wire_expr *make_wire_expr(expr *e, struct exec_info *f);
/* create a wire_expr from an expression */

extern wire_expr temp_wire_expr;
/* TODO: remove need to export this */

extern void clear_wire_expr(wire_expr *e, struct exec_info *f);

extern void add_wire_dep(wire_value *w, struct exec_info *f);
 /* set up w to reschedule f->curr upon the changing of w's value */

/********** strict checks ***************************************************/

#define IS_PARALLEL_STMT(S) \
    ((S)->class == CLASS_parallel_stmt \
    || ((S)->class == CLASS_rep_stmt && ((rep_stmt*)(S))->rep_sym == ','))

extern void strict_check_init(process_state *ps, struct exec_info *f);

extern void strict_check_term(process_state *ps, struct exec_info *f);

extern void strict_check_read(value_tp *v, struct exec_info *f);

extern void strict_check_read_elem(value_tp *v, struct exec_info *f);
/* To be called when one or more elements of v will be read */

extern void strict_check_read_bit(value_tp *v, int n, struct exec_info *f);
/* To be called when reading bit n of integer v */

extern void strict_check_write(value_tp *v, struct exec_info *f);

extern void strict_check_write_elem(value_tp *v, struct exec_info *f);
/* To be called when one or more elements of v will be written */

extern void strict_check_write_bit(value_tp *v, int n, struct exec_info *f);
/* To be called when writing bit n of integer v */

extern void strict_check_delete(value_tp *v, struct exec_info *f);
/* Upon ending a function/routine call, use this to remove
 * all f->prev->var[i] for all i
 */

extern void strict_check_frame_end(struct exec_info *f);
/* Pre: f->prev has ended sequential operations and popped the
 * parallel statement f->curr
 */

extern void strict_check_update(ctrl_state *cs, struct exec_info *f);
/* This should be called after cs has completed a parallel statement
 * (i.e. all children of cs have terminated)
 */

/********** critical cycles *************************************************/

typedef struct crit_node crit_node;
struct crit_node
   { int refcnt, delay;
     void *a; /* if !parent, wire_value, else ACTION_WITH_DIR (see exec.h) */
     crit_node *parent;
   };

extern void crit_node_step(action *a, wire_value *w, struct exec_info *f);
 /* Call this when a change on w has scheduled the production rule a. */

/*****************************************************************************/

extern void init_value(int app1, int app2, int app3, int app4, int app5);
 /* call at startup; pass unused object app indices */

#endif /* VALUE_H */
