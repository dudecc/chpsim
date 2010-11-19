/* chp.c: wrapper code for chp frontend

   Author: Marcel van der Goot
*/

#include "lex.h"
#include "parse.h"
#include "value.h"
#include "exec.h"
#include "chp.h"
#include "builtin_io.h"
#include "expr.h"
#include "modules.h"
#include "routines.h"
#include "statement.h"
#include "variables.h"
#include "types.h"

/********** reading source ***************************************************/

extern void read_source(user_info *f, const char *fin_nm, module_def **src_md)
 /* parsing and semantic analysis.
    Creates a llist(module_def) of all modules (in topological order), which
    is appended to f->ml.
    *src_md is assigned the module corresponding to fin_nm.
    f is used for the search path and to store the module list.
 */
 { lex_tp f_lex, *L = &f_lex;
   module_def *x;
   sem_info f_sem, *S = &f_sem;
   lex_tp_init(L);
   read_builtin(f, L);
   x = read_main_module(f, fin_nm, L);
   sem_info_init(S);
   module_cycles(x, S);
   llist_reverse(&S->ml);
   sem_list(&S->ml, S); /* first pass */
   sem_list(&S->ml, S); /* second pass */
   sem_info_term(S);
   lex_tp_term(L);
   *src_md = x;
   llist_concat(&f->ml, &S->ml);
 }


/********** execution ********************************************************/

extern process_def *find_main
 (module_def *src_md, const char *main_id, user_info *f, int *err)
 /* Find the main routine ("main" if !main_id), looking first in src_md, then
    in f->ml.
    Return is 0 if not found. If found but not valid, return is 0 and *err
    is set to 1. Warning messages are printed using report(f, ...).
    Warnings are printed if main_id is specified but not found, or routine
    is found but invalid.
 */
 { process_def *dp;
   int warn = 1;
   if (main_id)
     { main_id = make_str(main_id); }
   else
     { main_id = make_str("main"); warn = 0; }
   dp = find_routine(src_md, main_id, f, 0);
   if (!dp)
     { if (warn)
         { report(f, "Warning: main process '%s' not found\n", main_id); }
       *err = 0;
       return 0;
     }
   *err = 1;
   if (dp->class == CLASS_function_def)
     { report(f, "%s[%d:%d] '%s' is not a process\n",
	        dp->src, dp->lnr, dp->lpos, main_id);
       return 0;
     }
   if (llist_size(&dp->pl))
     { report(f, "%s[%d:%d] main process '%s' has port parameters\n",
	      dp->src, dp->lnr, dp->lpos, main_id);
       return 0;
     }
   *err = 0;
   return dp;
 }

extern exec_info *prepare_exec(user_info *U, process_def *dp)
 /* Prepare data structures to execute the program, starting with
    an instance ("/") of dp.
 */
 { exec_info *f;
   process_state *ps;
   ctrl_state *cs;
   value_tp *var;
   int i;
   NEW(f);
   exec_info_init(f, 0);
   SET_FLAG(f->flags, EXEC_instantiation);
   f->user = U;
   U->global = f;
   if (IS_SET(U->flags, USER_critical))
     { NEW(f->crit_map);
       hash_table_init(f->crit_map, 1, HASH_ptr_is_key, 0);
     }
   ps = new_process_state(f, make_str("/"));
   ps->p = dp;
   ps->nr_meta = dp->nr_meta;
   if (ps->nr_meta)
     { NEW_ARRAY(ps->meta, ps->nr_meta); }
   var = ps->meta;
   for (i = 0; i < ps->nr_meta; i++)
     { var[i].rep = REP_none; }
   cs = ps->cs;
   cs->ps = ps;
   cs->obj = (parse_obj*)dp;
   cs->nr_var = dp->nr_var;
   if (cs->nr_var)
     { NEW_ARRAY(cs->var, cs->nr_var); }
   var = cs->var;
   for (i = 0; i < cs->nr_var; i++)
     { var[i].rep = REP_none; }
   ps->var = cs->var;
   ps->nr_var = cs->nr_var;
   ps->nr_thread = -1;
   U->main = ps;
   f->curr = cs;
   f->meta_ps = ps;
   return f;
 }

extern exec_info *term_exec(exec_info *f)
 /* Pre: f is return value of prepare_exec().
    Free the memory.
 */
 { exec_info_term(f);
   return 0;
 }


/********** startup **********************************************************/

extern void init_chp(int app0, FILE *user_stdout)
 /* Call at startup.
    Uses App_nr_chp app indices (see obj.h), app0 .. app0 + App_nr_chp - 1.
    user_stdout is used for CHP's stdout (you can change this afterwards
    with builtin_io.h:builtin_io_change_std().)
    Note: must set app_nr before calling this function.
 */
 { if (app_nr < app0 + App_nr_chp)
     { error("app_nr not initialized"); }

   init_lex();
   init_print(app0 + App_print);
   init_sem_analysis(app0 + App_sem, app0 + App_import);
   init_value(app0 + App_eval, app0 + App_range, app0 + App_assign);
   init_exec(app0 + App_exec, app0 + App_pop);
   init_interact(app0 + App_brk);

   init_expr();
   init_statement();
   init_types();
   init_variables();
   init_routines();
   init_modules();

   init_builtin_io(user_stdout);
 }

