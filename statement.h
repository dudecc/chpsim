/* statement.h: statements

   Author: Marcel van der Goot
*/
#ifndef STATEMENT_H
#define STATEMENT_H

#include "exec.h"

extern void init_statement(void);
 /* call at startup */

/* Sets bit one of return if not all ports connected to pval have probe zero
   Sets bit zero of return if not all ports of pval have probe one
   Error if we run into REP_none.
 */
extern int get_probe(value_tp *pval, exec_info *f);
 
/* f->curr->i should be false if this is a peek */
extern value_tp receive_value(value_tp *pval, type *tp, exec_info *f);

extern void sem_meta_binding_aux(process_def *d, meta_binding *x, sem_info *f);
/* If x->x is not valid and the process for binding (d) can be found
 * elsewhere, then use this instead of sem(x, f)
 */

extern void meta_init(process_state *ps, exec_info *f);
/* Everything that should be executed as soon as all meta parameters are set.
 * Note that exec_meta_binding_aux calls this, so this only needs to be
 * used when there are no meta parameters to set.
 */

extern void exec_meta_binding_aux(meta_binding *x, value_tp *v, exec_info *f);
/* If x->x is not valid and the process_state for binding (v->v.ps) can be
 * found elsewhere, then use this instead of exec(x, f).  v can also be an
 * array of processes.
 */

#endif /* STATEMENT_H */
