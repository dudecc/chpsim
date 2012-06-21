/* sem_analysis.h: semantic analysis
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

#ifndef SEM_ANALYSIS_H
#define SEM_ANALYSIS_H

#include <hash.h>
#include "parse_obj.h"
#include "lex.h"

extern void init_sem_analysis(int app1, int app2);
 /* call at startup; pass unused object app indices */

FLAGS(sem_id_flags)
   { FIRST_FLAG(SEM_error), /* something wrong with the def of this name */
     NEXT_FLAG(SEM_conflict) /* name is not accessible */
   };

FLAGS(sem_flags)
   { FIRST_FLAG(SEM_need_lvalue), /* expr should be lvalue */
     NEXT_FLAG(SEM_forward_clean), /* cleanup after 1st pass */
     NEXT_FLAG(SEM_value_probe), /* bool expr in value probe */
     NEXT_FLAG(SEM_func_def), /* in function/procedure def */
     NEXT_FLAG(SEM_instance_decl), /* in instance decl */
     NEXT_FLAG(SEM_prs), /* in prs body _or_ delay body  */
     NEXT_FLAG(SEM_delay), /* in delay body */
     NEXT_FLAG(SEM_meta), /* in meta body */
     NEXT_FLAG(SEM_meta_binding), /* in meta binding */
     NEXT_FLAG(SEM_connect), /* in connect stmt */
     NEXT_FLAG(SEM_prop), /* set when property references are allowed */
     NEXT_FLAG(SEM_debug), /* parsing from the debug prompt */
     SEM_body = SEM_prs | SEM_delay | SEM_meta /* all body types */
   };

typedef struct id_info
   { sem_id_flags flags;
     union { parse_obj *d; /* definition or declaration */
             llist l; /* multiple defs if SEM_conflict is set */
           } u;
   } id_info;

typedef struct sem_context sem_context;
struct sem_context
   { sem_context *parent;
     void *owner;
     hash_table *H;
     const str *id;
   };

typedef struct sem_info
   { sem_context *cxt;
     sem_flags flags;
     int meta_idx; /* counts meta 'variables' in process */
     int var_idx; /* counts variables in scope level */
     int rv_ref_depth; /* tracks references to repliaction variables */
     void *curr_routine; /* function_def, process_def */
     struct sem_info *prev; /* for nesting */
     int module_nr; /* for modules.h:module_cycles() */
     llist ml; /* llist(module_def); topological order */
     module_def *curr; /* current module being processed */
     lex_tp *L; /* init 0, set during debug prompt parsing */
     struct user_info *user; /* Also used during debug prompt parsing */
     /* Objects created during sem_analysis should use new_parse(f->L, ...) */
   } sem_info;

extern int app_sem;
#define set_sem(C) set_app(CLASS_ ## C, app_sem, (obj_func*)sem_ ## C)
 /* To set sem_abc as sem function for class abc */
#define set_sem_cp(C,D) set_app(CLASS_ ## C, app_sem, (obj_func*)sem_ ## D)
 /* Used if C is a copy of D, and uses the same sem function */

extern void sem_info_init(sem_info *f);
 /* Intitialize *f */

extern void sem_info_term(sem_info *f);
 /* Pre: f is at level 0.
    Terminate *f (deletes remaining elements from hash table).
    You must call sem_info_init() to use f again.
 */

extern void sem_free_obj(sem_info *f, void *x);
 /* Use this instead of free_obj when f is available */

/********** semantic analysis ************************************************/

extern void sem_error(sem_info *f, void *obj, const char *fmt, ...);
 /* print error msg for parse_obj and exit */

extern void sem_warning(void *obj, const char *fmt, ...);
 /* print warning msg for parse_obj */

extern void *sem(void *x, sem_info *f);
 /* Apply semantic analysis to parse_obj *x. Return is resulting object.
    In practice, return is x unless semantic info was necessary to resolve
    ambiguities in the grammar.
 */

extern void sem_list(llist *l, sem_info *f);
 /* Apply sem() to each element of l */

extern void sem_stmt_list(llist *l, sem_info *f);
 /* Use this instead of sem_list(l, f) when l should be a llist(statement).
    Same as sem_list(), but verify that there are only statements.
 */

extern void sem_rep_common(rep_common *x, sem_info *f);
 /* Apply semantic analysis to the common elements of a rep_expr/rep_stmt */

/********** scope ************************************************************/

extern void enter_level(void *owner, sem_context **cxt, sem_info *f);
 /* Start a nested scope level */

extern void enter_body(void *owner, sem_context **cxt, sem_info *f);
 /* Start a nested scope level that is a process body */

extern void enter_sublevel
(void *owner, const str *id, sem_context **cxt, sem_info *f);
 /* Start a nested scope level within a routine
    (Doesn't invalidate port/variable references)
  */

extern void leave_level(sem_info *f);
 /* Leave the current scope level */

/********** declarations *****************************************************/

extern void declare_id(sem_info *f, const str *id, void *x);
 /* Associate id with parse_obj *x */

extern void *find_id(sem_info *f, const str *id, void *obj);
 /* Return object associated with id.
    parse_obj *obj is used for the location in error messages.
 */

extern int find_level(sem_info *f, const str *id);
 /* Pre: find_id has already suceeded, returned rep_stmt/expr
    return relative scope level of the object assciated with id.
    (object's relative level = current level - object's absolute level)
 */

/********** importing modules ************************************************/

extern int app_import;
#define set_import(C) set_app(CLASS_ ## C, app_import, (obj_func*)import_ ## C)
 /* To set import_abc as import function for class abc */
#define set_import_cp(C,D) set_app(CLASS_ ## C, app_import, (obj_func*)import_ ## D)
 /* Used if C is a copy of D, and uses the same import function */

extern void import(void *x, sem_info *f);
 /* Import x. */

extern void import_list(llist *l, sem_info *f, int all);
 /* If all, import all elements of l; otherwise import only exported elements.
 */


/*****************************************************************************/

#endif /* SEM_ANALYSIS_H */
