/* parse_obj.c: the parse data structure
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

/*****************************************************************************/

/* We don't actually call parse_obj_init(), because we initialize all
   parse_obj with OBJ_zero.
   Also, for efficiency we use the implementation knowledge that
     llist_init(&x->l)
   is equivalent to x->l = 0. (Generally we wouldn't make that assumption,
   but in this case that saves us from writing a lot of trivial initialization
   functions.)

static void parse_obj_init(expr *x)
 { x->flags = 0; }
*/

DEF_CLASS(parse_obj, 0, 0, OBJ_zero);

DEF_CLASS_B(name, parse_obj, 0, 0, OBJ_zero);


/********** expressions ******************************************************/

DEF_CLASS_B(expr, parse_obj, 0, 0, OBJ_zero);

DEF_CLASS_B(binary_expr, expr, 0, 0, OBJ_zero);
DEF_CLASS_B(rep_expr, expr, 0, 0, OBJ_zero);
DEF_CLASS_B(prefix_expr, expr, 0, 0, OBJ_zero);
DEF_CLASS_B(probe, expr, 0, 0, OBJ_zero);
DEF_CLASS_B(value_probe, expr, 0, 0, OBJ_zero);
DEF_CLASS_B(array_subscript, expr, 0, 0, OBJ_zero);
DEF_CLASS_B(int_subscript, expr, 0, 0, OBJ_zero);
DEF_CLASS_B(int_port_subscript, expr, 0, 0, OBJ_zero);
DEF_CLASS_B(array_subrange, expr, 0, 0, OBJ_zero);
DEF_CLASS_B(int_subrange, expr, 0, 0, OBJ_zero);
DEF_CLASS_B(field_of_record, expr, 0, 0, OBJ_zero);
DEF_CLASS_B(field_of_process, expr, 0, 0, OBJ_zero);
DEF_CLASS_B(field_of_union, expr, 0, 0, OBJ_zero);
DEF_CLASS_B(array_constructor, expr, 0, 0, OBJ_zero);
DEF_CLASS_B(record_constructor, expr, 0, 0, OBJ_zero);
DEF_CLASS_B(call, expr, 0, 0, OBJ_zero);
DEF_CLASS_B(token_expr, expr, 0, 0, OBJ_zero);
DEF_CLASS_B(var_ref, expr, 0, 0, OBJ_zero);
DEF_CLASS_B(rep_var_ref, expr, 0, 0, OBJ_zero);
DEF_CLASS_B(const_ref, expr, 0, 0, OBJ_zero);
DEF_CLASS_B(meta_ref, expr, 0, 0, OBJ_zero);
DEF_CLASS_B(implicit_array, expr, 0, 0, OBJ_zero);
DEF_CLASS_B(wire_ref, expr, 0, 0, OBJ_zero);
DEF_CLASS_B(type_expr, expr, 0, 0, OBJ_zero);
DEF_CLASS_B(property_ref, expr, 0, 0, OBJ_zero);


/********** statements *******************************************************/

DEF_CLASS_B(statement, parse_obj, 0, 0, OBJ_zero);

DEF_CLASS_B(skip_stmt, statement, 0, 0, OBJ_zero);
DEF_CLASS_B(end_stmt, statement, 0, 0, OBJ_zero);
DEF_CLASS_B(parallel_stmt, statement, 0, 0, OBJ_zero);
DEF_CLASS_B(compound_stmt, statement, 0, 0, OBJ_zero);
DEF_CLASS_B(rep_stmt, statement, 0, 0, OBJ_zero);
DEF_CLASS_B(assignment, statement, 0, 0, OBJ_zero);
DEF_CLASS_B(bool_set_stmt, statement, 0, 0, OBJ_zero);
DEF_CLASS_B(guarded_cmnd, statement, 0, 0, OBJ_zero);
DEF_CLASS_B(loop_stmt, statement, 0, 0, OBJ_zero);
DEF_CLASS_B(select_stmt, statement, 0, 0, OBJ_zero);
DEF_CLASS_B(communication, statement, 0, 0, OBJ_zero);
DEF_CLASS_B(instance_stmt, statement, 0, 0, OBJ_zero);
DEF_CLASS_B(meta_binding, statement, 0, 0, OBJ_zero);
DEF_CLASS_B(connection, statement, 0, 0, OBJ_zero);
DEF_CLASS_B(wired_connection, statement, 0, 0, OBJ_zero);
DEF_CLASS_B(const_wired_connection, statement, 0, 0, OBJ_zero);
DEF_CLASS_B(production_rule, statement, 0, 0, OBJ_zero);
DEF_CLASS_B(transition, statement, 0, 0, OBJ_zero);
DEF_CLASS_B(delay_hold, statement, 0, 0, OBJ_zero);
DEF_CLASS_B(property_stmt, statement, 0, 0, OBJ_zero);


/********** types ************************************************************/

DEF_CLASS_B(type_spec, parse_obj, 0, 0, OBJ_zero);

DEF_CLASS_B(dummy_type, type_spec, 0, 0, OBJ_zero);
DEF_CLASS_B(named_type, type_spec, 0, 0, OBJ_zero);
DEF_CLASS_B(generic_type, type_spec, 0, 0, OBJ_zero);
DEF_CLASS_B(integer_type, type_spec, 0, 0, OBJ_zero);
DEF_CLASS_B(field_def, parse_obj, 0, 0, OBJ_zero);
DEF_CLASS_B(symbol_type, type_spec, 0, 0, OBJ_zero);
DEF_CLASS_B(array_type, type_spec, 0, 0, OBJ_zero);
DEF_CLASS_B(record_type, type_spec, 0, 0, OBJ_zero);
DEF_CLASS_B(record_field, type_spec, 0, 0, OBJ_zero);
DEF_CLASS_B(union_type, type_spec, 0, 0, OBJ_zero);
DEF_CLASS_B(union_field, type_spec, 0, 0, OBJ_zero);
DEF_CLASS_B(wired_type, type_spec, 0, 0, OBJ_zero);
DEF_CLASS_B(type_def, type_spec, 0, 0, OBJ_zero);


/********** variables and constants ******************************************/

DEF_CLASS_B(const_def, parse_obj, 0, 0, OBJ_zero);
DEF_CLASS_B(var_decl, parse_obj, 0, 0, OBJ_zero);
DEF_CLASS_B(parameter, parse_obj, 0, 0, OBJ_zero);
DEF_CLASS_B(meta_parameter, parse_obj, 0, 0, OBJ_zero);
DEF_CLASS_B(wire_decl, parse_obj, 0, 0, OBJ_zero);
DEF_CLASS_B(property_decl, parse_obj, 0, 0, OBJ_zero);


/********** routines *********************************************************/

DEF_CLASS_B(chp_body, parse_obj, 0, 0, OBJ_zero);
DEF_CLASS_B(meta_body, parse_obj, 0, 0, OBJ_zero);
DEF_CLASS_B(hse_body, parse_obj, 0, 0, OBJ_zero);
DEF_CLASS_B(prs_body, parse_obj, 0, 0, OBJ_zero);
DEF_CLASS_B(delay_body, parse_obj, 0, 0, OBJ_zero);
DEF_CLASS_B(property_body, parse_obj, 0, 0, OBJ_zero);
DEF_CLASS_B(function_def, parse_obj, 0, 0, OBJ_zero);
DEF_CLASS_B(process_def, parse_obj, 0, 0, OBJ_zero);


/********** modules **********************************************************/

DEF_CLASS_B(required_module, parse_obj, 0, 0, OBJ_zero);
DEF_CLASS_B(module_def, parse_obj, 0, 0, OBJ_zero);


/*****************************************************************************/

extern void *_new_parse(lex_tp *L, token_info *t, void *yv, obj_class *class)
 /* Create a new object of the specified class (superclass of parse_obj).
    L and t are used for the position. Alternatively, if parse_obj yv is
    specified, the position of y is used.
 */
 { parse_obj *x, *y = yv;
   assert(y || (L && t));
   x = new_obj(class, y? y->src : L->fin_nm, y? y->lnr : t->lnr);
   x->lpos = y? y->lpos : t->start_pos;
   if (L && L->err_jmp) /* If a lex_error longjmp's instead of exiting */
     { llist_prepend(&L->alloc_list, x); } /* then we must know what to free */
   return x;
 }


extern void pos_from(void *xv, void *yv)
 /* parse_obj *xv, *yv;
    Set xv's position from yv's position.
 */
 { parse_obj *x = xv, *y = yv;
   x->src = y->src;
   x->lnr = y->lnr;
   x->lpos = y->lpos;
 }

