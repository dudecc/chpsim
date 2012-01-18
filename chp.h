/* chp.h: wrapper code for chp frontend
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
      dp = find_main(src_md, main_id, U, &err, 0);
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
       App_ifrchk,
       App_import,
       App_exec,
       App_pop,
       App_eval,
       App_reval,
       App_range,
       App_assign,
       App_conn,
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
 (module_def *src_md, const char *main_id, user_info *f, int *err, int port);
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
