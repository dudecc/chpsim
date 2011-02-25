/* parse_obj.h: the parse data structure
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

#ifndef PARSE_OBJ_H
#define PARSE_OBJ_H

#include <standard.h>
#include <obj.h>
#include <llist.h>
#include <string_table.h>
#include "lex.h"

extern void *_new_parse(lex_tp *L, token_info *t, void *yv, obj_class *class);
 /* Create a new object of the specified class (superclass of parse_obj).
    L and t are used for the position. Alternatively, if parse_obj yv is
    specified, the position of y is used.
 */

/* cast to check for mistakes: */
#define new_parse(L, T, Y, C) (C*)_new_parse(L, T, Y, CLASS_ ## C)

extern void pos_from(void *xv, void *yv);
 /* parse_obj *xv, *yv;
    Set xv's position form yv's position.
 */

/*****************************************************************************/

FLAGS(parse_obj_flags)
   { FIRST_FLAG(DEF_export), /* true when definition is exported */
     NEXT_FLAG(DEF_forward), /* sem() already done once */
     NEXT_FLAG(EXPR_parenthesized), /* used to resolve precedence */
     NEXT_FLAG(EXPR_unconst), /* depends on a variable/wire/port */
     NEXT_FLAG(EXPR_metax), /* depends on meta parameters */
     NEXT_FLAG(EXPR_cparam), /* depends on a const parameter */
     NEXT_FLAG(EXPR_rep), /* depends on replication parameter */
     NEXT_FLAG(EXPR_writable), /* can be target of assignment */
     NEXT_FLAG(EXPR_inport), /* is an input port */
     NEXT_FLAG(EXPR_outport), /* is an output port */
     NEXT_FLAG(EXPR_port_ext), /* is a port of another process */
     NEXT_FLAG(EXPR_wire), /* references a wire/wired port */
     NEXT_FLAG(EXPR_counter), /* references a counter */
     NEXT_FLAG(EXPR_generic), /* cannot use type checking */
     NEXT_FLAG(DEF_builtin), /* builtin object */
     NEXT_FLAG(DEF_varargs), /* variable nr args for routine */
     NEXT_FLAG(DBG_break), /* breakpoint, for statements */
     NEXT_FLAG(DBG_break_cond), /* breakpoint with conditions */
     EXPR_nocexpr = EXPR_unconst | EXPR_cparam | EXPR_rep,
                                /* all const_expr disqualifiers */
     EXPR_all_constx = EXPR_nocexpr | EXPR_metax,
                                /* All flags relating to constant status */
     EXPR_port = EXPR_inport | EXPR_outport, /* is a port */
     EXPR_inherit = EXPR_wire | EXPR_port_ext | EXPR_generic |
	            EXPR_port | EXPR_writable | EXPR_counter
   };

#define IS_A_PORT(X) (IS_SET((X)->flags, EXPR_port) \
		  && !IS_SET((X)->flags, EXPR_wire))

#define PARSE_OBJ \
	OBJ_FIELDS; \
	int lpos /* position in line */; \
	parse_obj_flags flags

/* generic parse object */
CLASS(parse_obj)
   { PARSE_OBJ;
   };

CLASS(name)
   { PARSE_OBJ;
     const str *id;
   };


/********** type representation **********************************************/

CLASS_1(type_spec);
CLASS_1(process_def);

typedef enum type_kind
   { TP_bool = 1, TP_int, TP_symbol, TP_array, TP_record,
     TP_syncport, TP_process, TP_wire, TP_type, TP_generic
   } type_kind;

typedef struct type type;
struct type
   { type_kind kind;
     union { type *tp; /* kind = TP_array, TP_generic */
	     llist l;  /* kind = TP_record; llist(type) */
	     process_def *p; /* kind = TP_process */
     } elem;
     type_spec *tps; /* 0 if reduced type */
     type_spec *utps; /* 0 unless type is union */
   };

/********** value representation *********************************************/

/* now defined in value.h */

/********** expressions ******************************************************/

#define EXPR_OBJ \
	PARSE_OBJ; \
	type tp

/* generic expression */
CLASS(expr)
   { EXPR_OBJ;
   };

#define EXPR_FROM_TYPE(T) ((expr*)(((long)(T))-((long)(&((expr*)0x0)->tp))))

/* The const_expr class is now defined in expr.h (see mk_const_expr) */

/* used for rep_expr's and rep_stmt's */
typedef struct rep_common rep_common;
struct rep_common
   { const str *id;
     expr *l, *h;
   };

CLASS(rep_expr)
   { EXPR_OBJ;
     token_tp rep_sym;
	/* int -> int: +, *, &, |, xor
	   array -> array: ++
	   bool -> bool: &, |, xor
	*/
     rep_common r;
     expr *v;
     struct sem_context *cxt;
   };

CLASS(binary_expr)
   { EXPR_OBJ;
     token_tp op_sym;
	/* int, int -> int: +, -, *, /, %, mod, ^, &, |, xor
	   array, array -> array: ++
	   any, any -> bool: =, !=
	   int, int -> bool: <, >, <=, >=
	   bool, bool -> bool: <, >, <=, >=, &, |, xor
	*/
     expr *l, *r;
   };

CLASS(prefix_expr)
   { EXPR_OBJ;
     token_tp op_sym; /* +, -, ~, # */
     expr *r;
   };

CLASS_COPY(probe, prefix_expr);

CLASS(value_probe)
   { EXPR_OBJ;
     llist p; /* llist(expr) */
     expr *b;
   };

CLASS(array_subscript)
   { EXPR_OBJ;
     expr *x; /* TP_array */
     expr *idx;
   };

CLASS_COPY(int_subscript, array_subscript);

CLASS(array_subrange)
   { EXPR_OBJ;
     expr *x; /* TP_array or TP_int */
     expr *l, *h;
   };

CLASS_1(field_def);
CLASS(field_of_record)
   { EXPR_OBJ;
     expr *x;
     const str *id;
     int idx; /* assigned by sem(); index of field in record.
		 If x is a process instance, id a port, then idx is
		 the port_parameter's var_idx.
	       */
     field_def *field; /* assigned by sem() if x is an integer */
   };

CLASS_COPY(field_of_process, field_of_record);

CLASS_1(union_field);
CLASS(field_of_union)
   { EXPR_OBJ;
     expr *x;
     const str *id;
     union_field *d; /* assigned by sem() */
   };

/* Debug field of union is only used when parsing from the debug prompt */
CLASS_COPY(debug_field_of_union, field_of_union);

CLASS(array_constructor)
   { EXPR_OBJ;
     llist l; /* llist(expr) */
   };

CLASS_COPY(record_constructor, array_constructor);

CLASS_1(function_def);
CLASS(call)
   { EXPR_OBJ;
     const str *id;
     llist a; /* llist(expr) */
     function_def *d; /* assigned by sem() */
   };

/* During parsing, all identifiers in expressions are parsed as token_expr,
   but during semantic analysis those that are variable references or
   references to named constants are replaced by var_ref/const_ref; those
   that are function or procedure references are replaced by calls.
*/
CLASS(token_expr) /* id or literal */
   { EXPR_OBJ;
     token_value t; /* after sem(): TOK_int/TP_int, TOK_z/TP_int,
	TOK_char/TP_int, TOK_string/TP_array, TOK_id/TP_symbol,
	KW_true/KW_false/TP_bool */
   };

CLASS_1(var_decl);
CLASS(var_ref) /* variable reference */
   { EXPR_OBJ;
     var_decl *d; /* assigned by sem() */
     int var_idx; /* assigned by sem() */
   };

CLASS_1(rep_stmt);
CLASS(rep_var_ref) /* replicator variable reference */
   { EXPR_OBJ;
     rep_stmt *rs; /* assigned by sem() */
     rep_expr *re; /* assigned by sem() */
     int rep_idx; /* assigned by sem() */
   };

CLASS_1(const_def);
CLASS(const_ref) /* reference to named constant */
   { EXPR_OBJ;
     const_def *d; /* assigned by sem() */
     expr *z; /* assigned by sem() */
   };

CLASS_1(meta_parameter);
CLASS(meta_ref) /* reference to meta parameter */
   { EXPR_OBJ;
     meta_parameter *d; /* assigned by sem() */
     int meta_idx; /* assigned by sem() */
   };

CLASS(implicit_array) /* created by sem() */
   { EXPR_OBJ;
     expr *x;
   };

/* a wire_ref replaces any expression that would evaluate to REP_wire
   when REP_bool is needed
 */
CLASS(wire_ref) /* wire reference */
   { EXPR_OBJ;
     expr *x;
   };

CLASS(type_expr) /* type that is passed as a meta parameter */
   { EXPR_OBJ;
     type_spec *tps;
   };


/********** statements *******************************************************/

/* We need a generic class, so we can identify statements by the fact
   that their base class is statement. However, we don't need additional
   fields.
*/
CLASS_COPY(statement, parse_obj);

CLASS_COPY(skip_stmt, statement);

/* Used to mark end of loop and end of body, so we can put a breakpoint: */
CLASS(end_stmt)
   { PARSE_OBJ;
     int end_body; /* 0 => end loop/select; 1 => end body */
   };

CLASS(parallel_stmt)
   { PARSE_OBJ;
     llist l; /* llist(statement); > 1 elements */
   };

CLASS(compound_stmt)
   { PARSE_OBJ;
     llist l; /* llist(statement); > 1 elements */
   };

CLASS_2(rep_stmt)
   { PARSE_OBJ;
     token_tp rep_sym;
	/* ',', ';', SYM_bar, SYM_arb */
     rep_common r;
     llist sl; /* llist(statement) */
     struct sem_context *cxt;
   };

CLASS(assignment)
   { PARSE_OBJ;
     expr *v, *e; /* v := e */
   };

/* bool_set_stmt is for x+ or x-; it is easier to have a separate
   object class than to use assignment for this.
 */
CLASS(bool_set_stmt)
   { PARSE_OBJ;
     expr *v;
     token_tp op_sym; /* + or - */
   };

CLASS(guarded_cmnd)
   { PARSE_OBJ;
     expr *g;
     llist l; /* llist(statement) */
   };

CLASS(loop_stmt)
   { PARSE_OBJ;
     llist gl, glr; /* llist(guarded_cmnd); see select_stmt for glr */
     int mutex; /* if true, guards must be mutex */
     llist sl; /* llist(statement); if there is no guarded_cmnd */
   };

CLASS(select_stmt)
   { PARSE_OBJ;
     llist gl, glr; /* llist(guarded_cmnd)
		       glr has the same elements (as aliases) but is reversed
		       (by sem()). glr is used for execution, gl for printing.
		     */
     int mutex; /* if true, guards must be mutex */
     expr *w; /* or 0. Used if wait instead of select */
   };

CLASS(communication)
   { PARSE_OBJ;
     expr *p, *e;
     token_tp op_sym; /* '!', '?', SYM_peek, '=' for p!e?, 0 for sync */
   };

CLASS(meta_binding)
   { PARSE_OBJ;
     expr *x;
     llist a; /* llist(expr) */
   };

CLASS(instance_stmt)
   { PARSE_OBJ;
     var_decl *d;
     meta_binding *mb; /* inline meta_binding, mb->x should be 0 */
   };

CLASS(connection)
   { PARSE_OBJ;
     expr *a, *b; /* var_ref for port_parameter, or field_of_record */
   };

CLASS_COPY(wired_connection, connection);
CLASS_COPY(const_wired_connection, connection);

CLASS(production_rule)
   { PARSE_OBJ;
     expr *g;
     expr *v;
     token_tp op_sym; /* + or - */
     int atomic;
     expr *delay;
   };

CLASS(transition)
   { PARSE_OBJ;
     expr *v;
     token_tp op_sym; /* + or - */
   };

CLASS(delay_hold)
   { PARSE_OBJ;
     llist l; /* llist(transition) */
     expr *c;
     expr *n;
   };

/********** types ************************************************************/

#define TYPE_SPEC_OBJ \
	PARSE_OBJ; \
	type tp

/* generic */
CLASS_2(type_spec)
   { TYPE_SPEC_OBJ;
   };

/* Used when having additional pointer indirection for types is needed */
CLASS(dummy_type)
   { TYPE_SPEC_OBJ;
     type_spec *tps;
   };

/* used when identifier is found where type expected */
CLASS(named_type)
   { TYPE_SPEC_OBJ;
     const str *id;
     type_spec *tps; /* assigned by sem() */
   };

/* used for keywords int, bool, symbol */
CLASS(generic_type)
   { TYPE_SPEC_OBJ;
     token_tp sym;
   };

CLASS(integer_type)
   { TYPE_SPEC_OBJ;
     expr *l, *h;
   };

CLASS_2(field_def) /* not a type_spec */
   { PARSE_OBJ;
     const str *id;
     expr *l, *h;
   };

CLASS(symbol_type)
   { TYPE_SPEC_OBJ;
     llist l; /* llist(name) */
   };

CLASS(array_type)
   { TYPE_SPEC_OBJ;
     type_spec *tps; /* element type */
     expr *l, *h;
   };

CLASS(record_type)
   { TYPE_SPEC_OBJ;
     llist l; /* llist(record_field) */
   };

CLASS(record_field)
   { TYPE_SPEC_OBJ;
     const str *id;
     type_spec *tps;
   };

CLASS(union_type)
   { TYPE_SPEC_OBJ;
     type_spec *def;
     llist l; /* llist(union_field) */
   };

CLASS_2(union_field)
   { TYPE_SPEC_OBJ;
     const str *id, *upid, *dnid;
     type_spec *tps;
     union { function_def *f;
             process_def *p;
           } up, dn; /* assigned by sem() */
     meta_binding *upmb, *dnmb; /* inline meta_bindings, ->x should be 0 */
   };

/* This type is used as a standard type for directed ports,
   or specified inline for ports with no direction.
   In the former case, li contains wires written by the receiving process,
   lo contains wires written by the sending process.  In the latter case,
   the first list parsed contains inputs, while the second list is the outputs,
   so two ports need to have different types to be connected.  For these ports,
   type specifies whether li or lo contains the input wires.
 */
CLASS(wired_type)
   { TYPE_SPEC_OBJ;
     llist li, lo; /* llist(wire_decl) */
     int nr_var, type; /* assigned by sem() */
   };
#define LI_IS_WRITE(F,T) \
        ((~(F) & ((T)? EXPR_port : EXPR_outport)) == EXPR_outport)

CLASS(type_def)
   { TYPE_SPEC_OBJ;
     const str *id;
     type_spec *tps;
   };

#define MAX_TYPE_SIZE sizeof(wired_type)

/********** variables and constants ******************************************/

CLASS_2(var_decl)
   { PARSE_OBJ;
     type tp;
     const str *id;
     type_spec *tps;
     expr *z; /* optional initial value */
     token_tp z_sym; /* optional boolean initial value ('+' or '-') */
     int var_idx; /* index in scope; assigned by sem() */
   };

CLASS_2(const_def)
   { PARSE_OBJ;
     type tp;
     const str *id;
     type_spec *tps; /* optional */
     expr *z;
   };

CLASS(parameter)
   { PARSE_OBJ;
     token_tp par_sym; /* val, res, valres */
     var_decl *d;
   };

CLASS_2(meta_parameter)
   { PARSE_OBJ;
     type tp;
     const str *id;
     type_spec *tps;
     int meta_idx; /* index in process scope; assigned by sem() */
   };

CLASS(wire_decl)
   { PARSE_OBJ;
     const str *id;
     token_tp init_sym; /* '+', '-', or 0 */
     expr *z; /* if '= value' is used instead of a symbol */
     type_spec *tps;
   };


/********** routines *********************************************************/

CLASS(chp_body)
   { PARSE_OBJ;
     llist dl; /* definitions and var decls:
	   type_def, const_def, function_def, process_def, var_decl */
     llist sl; /* statements */
     struct sem_context *cxt;
   };

CLASS_COPY(meta_body, chp_body);

CLASS_COPY(hse_body, chp_body);

CLASS_COPY(prs_body, chp_body);

CLASS_COPY(delay_body, chp_body);

CLASS_2(function_def) /* also for procedure_def */
   { PARSE_OBJ;
     const str *id;
     llist pl; /* llist(parameter) */
     chp_body *b; /* if DEF_builtin, then actual function */
     var_decl *ret; /* for function, corresponding to return value */
     int nr_var; /* assigned by sem() */
     void *parent; /* function_def, process_def, or module_def */
     struct sem_context *cxt;
   };

CLASS_2(process_def)
   { PARSE_OBJ;
     const str *id;
     llist ml; /* llist(meta_parameter) */
     llist pl; /* llist(port_parameter) */
     int nr_var; /* assigned by sem() */
     int meta_offset, nr_meta; /* assigned by sem() */
     void *parent; /* function_def, process_def, or module_def; by sem() */
     struct sem_context *cxt;
     /* At least one of the following should be non-null */
     chp_body *cb;
     meta_body *mb;
     hse_body *hb;
     prs_body *pb;
     delay_body *db;
   };

/********** modules **********************************************************/

CLASS(module_def)
   { PARSE_OBJ;
     llist rl; /* llist(required_module) */
     llist dl; /* llist(definition) */
     module_def *cycle; /* assigned by modules.c:module_cycles() */
     int module_nr; /* assigned by modules.c:module_cycles() */
     int visited; /* used for dfs() in module_cycles() */
     module_def *importer; /* last import of this module */
     struct sem_context *cxt, *import_cxt;
   };

CLASS(required_module)
   { PARSE_OBJ;
     char *s;
     module_def *m; /* assigned by modules.c:read_module() */
   };



#endif /* PARSE_OBJ_H */
