/* expr.h: expressions

   Author: Marcel van der Goot

   $Id: expr.h,v 1.2 2002/12/03 19:56:21 marcel Exp $
*/
#ifndef EXPR_H
#define EXPR_H

#include "exec.h"
#include "sem_analysis.h"

extern int precedence(token_tp op);
 /* return precedence value of binary operator t */

/* A constant expression is replaced by a const_expr, to have a place
   to store the value so it is evaluated only once.  Only used
   when EXPR_unconst or EXPR_rep are unset. (Maybe EXPR_const_param also?)
*/
CLASS(const_expr)
   { EXPR_OBJ;
     value_tp val;
     int meta_idx; /* used if only EXPR_meta is set*/
     expr *x;
   };

extern expr *mk_const_expr(expr *x, sem_info *f);
 /* if x is a const expr, insert a const_expr object; return is x or
    the const_expr
 */

extern void mk_const_expr_list(llist *l, sem_info *f);
 /* llist(expr) *l; For each constant in l, replace it by a const_expr */

extern long eval_rep_common(rep_common *r, value_tp *v, exec_info *f);
 /* set v to value of low replicator bound, returns # of values in bounds */
 
extern void int_simplify(value_tp *x, exec_info *f);
 /* if possible, replace x by REP_int rather than REP_z */

extern int int_cmp(value_tp *x, value_tp *y, exec_info *f);
 /* Pre: x->rep && y->rep, x != y.
    Return 0 if x == y, <0 if x < y, >0 if x > y.
    Clears *x and *y.
 */

extern value_tp int_add(value_tp *l, value_tp *r, exec_info *f);
 /* Pre: l->rep && r->rep, l != r.
    return l + r; clears *l and *r.
 */

extern value_tp int_sub(value_tp *l, value_tp *r, exec_info *f);
 /* Pre: l->rep && r->rep, l != r.
    return l - r; clears *l and *r.
 */

extern void int_inc(value_tp *x, exec_info *f);
 /* Pre: x->rep
    increment x in place
 */

extern void int_dec(value_tp *x, exec_info *f);
 /* Pre: x->rep
    decrement x in place
 */

extern long int_log2(value_tp *x, exec_info *f);
 /* Pre: x->rep
    returns the log of (x+1) base 2, rounded up to the nearest integer
    clears *x
 */

extern value_tp *find_reference(port_value *p, exec_info *f);
/* Checks all ports to find the reference to the given port value */
	
extern value_tp convert_up(value_tp *v, type *utp, type *dtp, exec_info *f);
 /* convert v to the default type (utp) from the decomposed type (dtp) */

extern value_tp convert_down(value_tp *v, type *utp, type *dtp, exec_info *f);
 /* convert v from the default type (utp) to the decomposed type (dtp) */

extern void port_to_array(value_tp *v, expr *x, exec_info *f);
 /* convert a channel of array type to an array of channels */

extern void default_value(value_tp *v, exec_info *f);
 /* Pre: v is union value */

extern void init_expr(void);
 /* call at startup */


#endif /* EXPR_H */
