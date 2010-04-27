/* routines.h: routines; builtin procedures

   Author: Marcel van der Goot

   $Id: routines.h,v 1.2 2002/12/10 20:45:37 marcel Exp $
*/
#ifndef ROUTINES_H
#define ROUTINES_H

#include "sem_analysis.h"
#include "exec.h"

extern void init_routines(void);
 /* call at startup */

extern void set_builtin_func(const char *nm, exec_builtin_func *f);
 /* set nm as name of builtin function f */

extern value_tp *get_call_arg(function_def *x, int i, exec_info *f);
 /* Pre: a call of x is being executed.
    Return a pointer to argument value i (starting from 0) of x. This
    can be one of the regular arguments or an extra argument for a
    DEF_vararg routine.
 */

extern int bif_print_argv(llist *m, int i, exec_info *f, int pos);
 /* Pre: llist(expr *) m; the values of these expressions are stored
	 in f->curr->argv[i...] (as with the arguments corresponding to "..."
	 when calling a builtin function).
    Prints the arguments to f->scratch[p...] in the same way as the
    builtin print() procedure.
    Return is the number of characters printed.
 */

#endif /* ROUTINES_H */
