/* interact.h: user interaction

   Author: Marcel van der Goot

   $Id: interact.h,v 1.8 2004/04/13 20:51:40 marcel Exp $
*/
#ifndef INTERACT_H
#define INTERACT_H

#include "exec.h"

typedef struct brk_cond brk_cond;
struct brk_cond
   { process_state *ps; /* if not zero, break only in this process */
     expr *e; /* if not zero, break when this evaluates true */
     llist e_alloc; /* list to be freed when e is freed */
   };

FLAGS(user_flags)
   { FIRST_FLAG(USER_batch), /* non-interactive operation */
     NEXT_FLAG(USER_quit), /* quit after command line commands */
     NEXT_FLAG(USER_verbose), /* be more verbose */
     NEXT_FLAG(USER_traceall), /* trace all processes */
     NEXT_FLAG(USER_watchall), /* watch all wires */
     NEXT_FLAG(USER_debug), /* Evaluating from the debug prompt */
     NEXT_FLAG(USER_strict), /* do strict checks */
     NEXT_FLAG(USER_random), /* use random timing */
     NEXT_FLAG(USER_nohide), /* do not hide wired decomposition processes */
     NEXT_FLAG(USER_critical), /* track critical cycles */
     NEXT_FLAG(USER_global), /* use global context for debug prompt exprs */
     NEXT_FLAG(USER_clear) /* brkp() clears breakpoints instead of setting them */
   };

typedef struct user_info user_info;
struct user_info
   { user_flags flags;
     FILE *log; /* output file for reporting */
     FILE *user_stdout; /* output for print() function and stdio.chp:stdout */
     lex_tp *L; /* for reading user commands */
     llist old_L; /* llist(lex_tp*) stack of command files */
     llist cmds; /* commands to pre-execute */
     llist procs; /* llist(process_state*); current, past, future processes */
     int brk_lnr, brk_lpos; /* requested breakpoint */
     const char *brk_src; /* requested breakpoint module name */
     hash_table brk_condition; /* conditions on breakpoints */
     llist ml; /* llist(module_def) */
     llist path; /* llist(char*); search path for modules */
     exec_info *global; /* currently used by exec_run() */
     exec_info *focus; /* top-level of focus process */
     ctrl_state *focus_top; /* top-level of focus process */
     ctrl_state *curr; /* currently viewed stmt */
     sem_context *cxt; /* context for parsed expressions */
     llist wait; /* processes waiting to be added */
     process_state *main; /* The root instance */
     process_state *ps; /* State we are seaching for */
     int view_pos; /* position of view on stack */
     var_string scratch, rep;
     int limit; /* if >0, limits the length of report()'s output */
   };

#define REPORT_LIMIT 1024
/* This limit is only set when there is a possiblity of an oversized
   value obscuring information in a way that cannot be easily worked
   around with different debug commands
*/

extern void init_interact(int app);
 /* call at startup; pass unused object app index */

extern void user_info_init(user_info *f);
 /* initialize *f */

/********** path *************************************************************/

extern void set_default_path(user_info *f);
 /* Append default search path for required modules. */

extern void add_path(user_info *f, const char *d);
 /* add (copy of) d to the search path */

extern void reset_path(user_info *f);
 /* Reset the module search path (to empty) */

extern void show_path(user_info *f);
 /* print search path */

/*****************************************************************************/

extern void report(user_info *f, const char *fmt, ...);
 /* report information to user */

extern token_tp prompt_user(user_info *f, const char *prompt);
 /* prompt information from user */

extern void interact(exec_info *f, const str *deflt);
 /* accept and interpret user commands, until execution should continue.
    deflt is default command.
 */

extern void interact_report(exec_info *f);
 /* Report current position, then interact. */

extern void interact_instantiate(exec_info *f);
 /* Pre: f->curr is state for initial process (not yet scheduled).
    Run instantiation phase (including global constants)
 */

extern void prepare_chp(exec_info *f);
 /* Prepare chp execution phase */

extern void show_perm_threads(exec_info *g, user_info *f);
 /* Print all threads */

extern void show_all_threads(user_info *f);
 /* Print all threads */

extern void deadlock_find(exec_info *f);
 /* Find a deadlock cycle in f, set f->curr to some element of the cycle */

extern int cmnd_where(user_info *f);
 /* Print threads of current process */

extern int cmnd_quit(user_info *f);
 /* Pre: f->global is set.
    terminate the program
 */

extern process_state *find_instance(user_info *f, const str *nm, int must);
 /* Find the process instance for nm. If 'must' and not found, then a new
    instance is created.
 */

extern int is_visible(process_state *ps);
 /* false if ps should be invisible (cannot be looked up by name) */

/* llist_func */
extern int instance_eq(process_state *ps, const str *nm);
 /* true if ps has name nm */

extern process_def *find_routine
 (module_def *md, const str *id, user_info *f, int only);
 /* Find the routine with the specified id. If 'only', only search in md.
    Otherwise, prefer md, but also search other modules.
    Note: return may be function_def.
 */

extern rep_common *find_rep_frame(ctrl_state **cs, llist *rl, user_info *f);
 /* Find and return the innermost replicator frame containing cs.  Replaces cs
    with the innermost control state which contains this frame.  If there are
    multiple rep frames between control states, rl is set with the unreturned
    frames (Conversely, if rl is not zero, this fn just pops off rl's top)
 */

/********** breakpoints ******************************************************/

typedef enum brk_return
   { BRK_no_stmt = 0, /* no statement found */
     BRK_stmt, /* statement found, no problems */
     BRK_stmt_err, /* statement found, but there is already a breakpoint
                    * (or we are clearing a breakpoint that does not exist)
                    */
     BRK_usage, /* command usage error */
     BRK_error /* some other error */
   } brk_return;

extern brk_return brkp(void *obj, user_info *f);
 /* Pre: The object immediately preceding obj, if any, has line nr less than
  *      f->brk_lnr, -or- f->brk_lnr:f->brk_lpos occurs after the start of obj.
  *      The start of the object immeditely following obj, if any, occurs
  *      after f->brk_lnr:f->brk_lpos.
  * If any part of obj starts on f->brk_lnr, set a breakpoint. (If multiple
  * objects start there, choose the one closest to f->brk_lpos.)
  */

extern brk_return brk_list(llist *l, user_info *f);
 /* Pre: objects in l are in order. Also see precondition for brkp above.
    Apply brk to each element of l (except more efficient). Return 1 if
    breakpoint was set, 0 otherwise.
 */

extern brk_return set_breakpoint(void *obj, user_info *f);
 /* Set breakpoint at obj, or if USER_clear is set,
  * clear existing breakpoint instead.  Return BRK_stmt_error
  * if there is already a break point, or if there is already
  * no breakpoint in these respective situations.
  */

extern int app_brk;
#define set_brk(C) set_app(CLASS_ ## C, app_brk, (obj_func*)brk_ ## C)
 /* To set brk_abc as brk function of class abc */
#define set_brk_cp(C,D) set_app(CLASS_ ## C, app_brk, (obj_func*)brk_ ## D)
 /* Used if C is a copy of D, and uses the same brk function */


#endif /* INTERACT_H */
