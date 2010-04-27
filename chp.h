/* chp.h: wrapper code for chp frontend

   Author: Marcel van der Goot

   $Id$
*/
#ifndef CHP_H
#define CHP_H

/* Suggested usage:

   startup:
      app_nr = ... + App_nr_chp;
      init_chp(app0, stdout);

   read command line arguments:
      user_info_init(U);
      set_default_path(U);
      read argv[], assigning fields of U
      builtin_io_change_std(U->user_stdout, 1);
      log, verbose, other special options

   parsing and semantic analysis:
      read_source(U, fin_nm, &src_md);
      dp = find_main(src_md, main_id, U, &err);
      if (!dp) { ... }

   instantiation & execution:
      f = prepate_exec(U, dp);
      interact_instantiate(f);  -- global const eval + graph instantiation
      interact_chp(f->chp);     -- chp execution
      term_exe(f);
*/


#include <standard.h>

/* parse data structure */
#include "parse_obj.h"

/* printing */
#include "print.h"

/* execution */
#include "exec.h"
#include "interact.h"

/* CHP's builtin io */
#include "builtin_io.h"

enum { App_print, /* these definitions are just to get App_nr_chp right */
       App_sem,
       App_import,
       App_exec,
       App_pop,
       App_eval,
       App_range,
       App_assign,
       App_brk,

       App_nr_chp /* nr of app indices to reserve */
};

extern void init_chp(int app0, FILE *user_stdout);
 /* Call at startup.
    Uses App_nr_chp app indices (see obj.h), app0 .. app0 + App_nr_chp - 1.
    user_stdout is used for CHP's stdout (you can change this afterwards
    with builtin_io.h:builtin_io_change_std().)
    Note: must set app_nr before calling this function.
 */

extern void read_source(user_info *f, const char *fin_nm, module_def **src_md);
 /* parsing and semantic analysis.
    Creates a llist(module_def) of all modules (in topological order), which
    is appended to f->ml.
    *src_md is assigned the module corresponding to fin_nm.
    f is used for the search path and to store the module list.
 */

extern process_def *find_main
 (module_def *src_md, const char *main_id, user_info *f, int *err);
 /* Find the main routine ("main" if !main_id), looking first in src_md, then
    in f->ml.
    Return is 0 if not found. If found but not valid, return is 0 and *err
    is set to 1. Warning messages are printed using report(f, ...).
    Warnings are printed if main_id is specified but not found, or routine
    is found but invalid.
 */

extern exec_info *prepare_exec(user_info *U, process_def *dp);
 /* Prepare data structures to execute the program, starting with
    an instance ("/") of dp.
 */

extern exec_info *term_exec(exec_info *f);
 /* Pre: f is return value of prepare_exec().
    Free the memory.
 */

#endif /* CHP_H */
