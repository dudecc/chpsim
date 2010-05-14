/* exec.h: statement execution and expression evaluation

   Author: Marcel van der Goot

   $Id: exec.h,v 1.3 2002/12/04 22:55:31 marcel Exp $
*/
#ifndef EXEC_H
#define EXEC_H

#include <standard.h>
#include <llist.h>
#include <hash.h>
#include <pqueue.h>
#include <var_string.h>
#include "parse_obj.h"
#include "sem_analysis.h"
#include "print.h"
#include "value.h"

/********** data structure ***************************************************/

FLAGS(dbg_flags)
   { FIRST_FLAG(DBG_step), /* step current process */
     NEXT_FLAG(DBG_next), /* step current ctrl_state */
     NEXT_FLAG(DBG_trace), /* trace current process */
     NEXT_FLAG(DBG_inst), /* instantiate current process */
     NEXT_FLAG(DBG_deadlock), /* process is part of a deadlock cycle */
     NEXT_FLAG(DBG_tmp1), /* used during deadlock checks */
     NEXT_FLAG(DBG_tmp2) /* used during deadlock checks */
   };

extern void init_exec(int app1, int app2);
 /* call at startup; pass unused object app indices */

typedef struct eval_stack eval_stack;
struct eval_stack
   { value_tp v;
     eval_stack *next;
   };

FLAGS(action_flags)
   { FIRST_FLAG(ACTION_sched), /* set if action is currently scheduled */
     NEXT_FLAG(ACTION_resched), /* set on copied actions (free on exec) */
     NEXT_FLAG(ACTION_check), /* set if action is waiting for checks */
     NEXT_FLAG(ACTION_susp), /* set if action is currently suspended */
     NEXT_FLAG(ACTION_delay), /* set if action has delay restrictions */
     NEXT_FLAG(ACTION_delay_susp), /* set if suspended on a delay hold */
     NEXT_FLAG(ACTION_atomic), /* action should be scheduled immediately */
     NEXT_FLAG(ACTION_has_up_pr), /* action has a pull up production rule */
     NEXT_FLAG(ACTION_has_dn_pr), /* action has a pull down production rule */
     NEXT_FLAG(ACTION_is_cr), /* action is a counter rule */
     NEXT_FLAG(ACTION_pr_up), /* up rule is enabled */
     NEXT_FLAG(ACTION_pr_dn), /* down rule is enabled */
     NEXT_FLAG(ACTION_up_nxt), /* up rule will be enabled */
     NEXT_FLAG(ACTION_dn_nxt), /* down rule will be enabled */
     ACTION_is_pr = ACTION_has_up_pr | ACTION_has_dn_pr
   };

/* typedef struct action action; in value.h */
struct action
   { mpz_t time; /* Scheduled execution time */
     action_flags flags;
     union { wire_value *w;
             counter_value *c;
           } target; /* Target of a production/counter rule */
     ctrl_state *cs;
   };
/* Currently there are only two types of actions: a production rule (PR), or a
 * statement.  Statement actions have a 1 to 1 correspondence with ctrl_state,
 * and if x is a statement action, x = &x->cs.act.  PR actions point to a
 * non-unique ctrl_state, giving a reference frame for the PR action.  These
 * ctrl_states leave their own act field unused
 */

#define ACTION_WITH_DIR(A) \
        ((void*)((long)(A) | (((A)->flags & ACTION_pr_up)?0:1)))
#define ACTION_NO_DIR(A) ((action*)((long)(A) & (-2l)))
#define ACTION_DIR(A) ((long)(A) & 1)
/* This is for storing both a pointer to pr action and a direction (up or down)
 * of that action.  ACTION_DIR returns true when the action is a pull down.
 */

/* typedef struct ctrl_state ctrl_state; in value.h */
struct ctrl_state
   { action act; /* attached action */
     parse_obj *obj; /* current stmt */
     llist seq; /* remaining seq of stmts (alias) */
     value_tp *var; /* var[nr_var]; variables of current routine */
     int nr_var;
     ctrl_state *stack; /* ptr to previous state, for nested stmts */
     process_state *ps;
     sem_context *cxt;
     eval_stack *rep_vals; /* holds values for replicators
                              shares nodes with f->stack usually, but
                              when doing a parallel replication,
                              creates and frees a single large chunk of memory
                            */
     /* data for specific stmts: */
     int i; /* init 0.
               parallel_stmt: counter; communication: position in hs */
     dbg_flags call_flags; /* for after procedure call */
     value_tp *argv; /* argv[argc]; extra args in a call */
     int argc;
     llist dep;
   };

/* typedef struct process_state process_state; in value.h */
struct process_state
   { int refcnt;
     process_def *p;
     chp_body *b;
     str *nm; /* for reporting */
     value_tp *meta; /* meta[nr_meta]; meta-constants of process */
     int nr_meta;
     value_tp *var; /* variables of current process; == cs->var  */
     int nr_var;
     ctrl_state *cs;
     int nr_thread; /* nr of threads; -1: instantiated, not started;
                       -2: terminated */
     int nr_susp; /* nr of suspended threads */
     dbg_flags flags;
     hash_table *accesses;  /* only used with strict checking */
   };

FLAGS(exec_flags)
   { FIRST_FLAG(EVAL_lvalue), /* computing an lvalue: if no value, create one */
     NEXT_FLAG(EVAL_sub_elem), /* evaluating an array/record */
     NEXT_FLAG(EVAL_probe), /* evaluating a probe */
     NEXT_FLAG(EVAL_probe_zero), /* checking for all probes equal to zero */
     NEXT_FLAG(EVAL_probe_wait), /* set if we might want to suspend */
     NEXT_FLAG(EVAL_connect), /* inside a connect stmt */
     NEXT_FLAG(EVAL_bit), /* assigning bits of an integer */
     NEXT_FLAG(EXEC_instantiation), /* instantiation phase */
     NEXT_FLAG(EXEC_immediate), /* do non-parallel execution */
     NEXT_FLAG(EXEC_sequential), /* choose chp over meta */
     NEXT_FLAG(EXEC_single), /* just run one process */
     NEXT_FLAG(EXEC_error), /* error occurred */
     NEXT_FLAG(EXEC_warning), /* warning occurred */
     NEXT_FLAG(EXEC_deadlock), /* f->curr is deadlocked */
     NEXT_FLAG(EXEC_strict), /* do strict checks */
     NEXT_FLAG(EXEC_print) /* print while executing */
   };

typedef struct exec_info exec_info;
struct exec_info
   { exec_flags flags;
     pqueue sched; /* queued actions */
     llist check; /* actions to be checked */
     llist susp_perm; /* permanently suspended statements */
     llist chp; /* non meta body actions stored here during instantiation */
     ctrl_state *curr; /* current state */
     ctrl_state *prev; /* previous state */
     process_state *meta_ps;
     mpz_t time; /* current time */
     eval_stack *stack; /* for expr eval */
     eval_stack *fl; /* free-list for stack */
     value_tp *val; /* target of range check */
     void *err_obj; /* obj for errors, in range check or strict checks */
     print_info *pf; /* used for printing when EXEC_print is set */
     wire_expr *e;
     guarded_cmnd *gc; /* for find_true_guard */
     eval_stack *gcrv; /* also for find_true_guard */
     var_string scratch, err; /* err is for error/warning msgs */
     exec_info *parent; /* parent of a function call */
     struct user_info *user; /* for user interaction */
     int nr_susp; /* number of suspended processes */
     hash_table delays; /* stores custom delay information */
     hash_table *crit_map; /* used for tracking critical cycles */
   };
     
typedef enum exec_return
   { EXEC_none = 0, /* no action necessary */
     EXEC_next, /* schedule next stmt in sequence */
     EXEC_suspend, /* suspend current stmt */
   } exec_return;
/* When a stmt is finished, exec_stmt or pop_stmt should return
   EXEC_next. This will schedule the next stmt (or cause a pop if this
   was the last stmt).

   If exec_stmt or pop_stmt has scheduled other (nested) stmts, return should
   be EXEC_none; once the nested stmts are finished, the matching pop_stmt
   will be called.

   If exec_stmt cannot complete until one or more wires have changed value, it
   should return EXEC_suspend after setting itself up as a dependency of the
   wires.  write_wire() will then schedule the statements to be re-executed
   upon the changing of the wire value(s).

   If there is no exec_stmt or pop_stmt for a stmt, EXEC_next is assumed.
*/


extern void exec_info_init(exec_info *f, exec_info *orig);
 /* initialize *f.
    If orig, then the interaction-related fields of orig are copied.
 */

extern void exec_info_term(exec_info *f);
 /* termination actions */

extern ctrl_state *new_ctrl_state(exec_info *f);
 /* Allocate a new state. time is set to the current time, meta is
    set from f->curr (if any). Other fields are initialized to 0.
    The state has not yet been scheduled.
    Note: a ctrl_state is deallocated just before a pop, but the var[]
    array is not automatically cleared.
 */

extern action *new_action(exec_info *f);
 /* Just create a new action. (For prs, etc. )
  * cs is set to f->curr, time is set to current time, target is not set.
  */

extern process_state *new_process_state(exec_info *f, const str *nm);
 /* Allocate new process state, all fields 0 except cs is a new ctrl_state
    and the specified nm is used.
    If a process_state with the specified name already exists, that state
    is returned instead.
 */

extern void free_process_state(process_state *ps, exec_info *f);
 /* Deallocate the process state. If refcnt is not 0, then variables
    etc. are deallocated but ps itself is not (this is used for user
    interaction).
 */

/* llist_func */
extern int routine_eq(void *obj, const str *id);
 /* true if x is a process_def or function_def with name id */


/********** errors ***********************************************************/

extern void exec_error(exec_info *f, void *obj, const char *fmt, ...);
 /* print error msg related to execution/eval of obj by current process */

extern void exec_warning(exec_info *f, void *obj, const char *fmt, ...);
 /* print warning msg related to execution/eval of obj by current process */


/********** statement execution **********************************************/

extern int exec_interrupted; /* if set, stop immediately */

extern void exec_run(exec_info *f);
 /* Run the program. */

extern void action_sched(action *a, exec_info *f);
 /* Pre: a is not scheduled
    Schedule a.
 */
#define insert_sched(S, F) action_sched(&(S)->act, F)

extern void run_checks(wire_value *w, exec_info *f);
 /* Runs through all checks in f->check */

extern void sched_instance(ctrl_state *cs, exec_info *f);
 /* Pre: cs is yet unscheduled process
    Determine which body to execute and dispatch to the appropriate list
 */

extern ctrl_state *nested_seq(llist *l, exec_info *f);
 /* Create and return ctrl_state for seq l, as nested sequence of current
    statement. Time has been set to current time (may be changed), and the
    new state has not yet been scheduled.
 */

extern void interact_chp(exec_info *f);
 /* Run chp execution phase */

extern void exec_immediate(llist *l, exec_info *f);
 /* Execute contents of l without delay, skipping
  * process/function definitions
  */

extern void exec_modules(llist *l, exec_info *f);
 /* llist(module_def) *l;
    Execute the global declarations (i.e., the const_defs) of each module.
    Note: f->curr must be set, in case the declarations contain function
    calls.
 */

typedef void exec_builtin_func(function_def *x, exec_info *f);
 /* Type of function for builtin functions */

extern int app_exec;
#define set_exec(C) set_app(CLASS_ ## C, app_exec, (obj_func*)exec_ ## C)
 /* To set exec_abc as exec function for class abc */
#define set_exec_cp(C,D) set_app(CLASS_ ## C, app_exec, (obj_func*)exec_ ## D)
 /* Used if C is a copy of D, and uses the same exec function */

extern int app_pop;
#define set_pop(C) set_app(CLASS_ ## C, app_pop, (obj_func*)pop_ ## C)
 /* To set pop_abc as pop function for class abc */
#define set_pop_cp(C,D) set_app(CLASS_ ## C, app_pop, (obj_func*)pop_ ## D)
 /* Used if C is a copy of D, and uses the same pop function */


/******************************************************************************

   There are two queues, one with scheduled statements and one with
   suspended statements. The sched queue is ordered by time. When
   a statement reaches the top of the sched queue, if it is not ready,
   it is put in the susp queue. The statements in the susp queue have
   all been reached and can execute whenever their guard conditions are
   satisfied.

   The suspended statements will be tried every time the simulation time
   is advanced, or when there are no ready statements left.

   Each thread has a ctrl_state to describe its current state. This ctrl_state
   is either in one of the two queues or the current state. The ctrl_state
   for a thread is actually a stack, which is used to implement nested
   statements.

   When ctrl_state->obj is one of a sequence of statements, the seq list
   is (an alias of) the list of following statements. If the seq
   list is empty, the ctrl_state is popped from the thread's stack.

   The exec function for a statement is called when the statement is
   first reached, and when a suspended statement is tried. It should return
   an exec_return value to indicate what comes next; the EXEC_none value is
   used if nested statements were scheduled. The pop function for a statement
   is called when the statement is reached through a pop of the thread's
   stack. It should return an exec_return value as well.
*/

#endif /* EXEC_H */
