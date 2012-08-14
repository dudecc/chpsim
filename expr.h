/* expr.h: expressions
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

extern int equal_value(value_tp *v, value_tp *w, exec_info *f, void *obj);
 /* Pre: v and w have compatible types.
    1 if equal values, 0 if not.
    obj is used for warnings. (v and w are not cleared)
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
