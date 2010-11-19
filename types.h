/* types.h: types

   Author: Marcel van der Goot
*/
#ifndef TYPES_H
#define TYPES_H

#include "sem_analysis.h"
#include "exec.h"

extern void init_types(void);
 /* call at startup */

extern int type_compatible(type *tp1, type *tp2);
 /* True iff the reduced forms of tp1 and tp2 are identical (compile-time
    check).
 */

extern int type_compatible_exec(type *tp1, process_state *ps1,
                                type *tp2, process_state *ps2, exec_info *f);
 /* Execution time type checking.  Verify that the specific types are
    identical ignoring union type differences.  Specifying a process_state
    for each type allows us to look up the real value for generic types.
 */

extern generic_type generic_int; /* used when the integer type is implicit */
extern generic_type generic_bool; /* used when the boolean type is implicit */

extern void import_builtin_types(sem_info *f);
 /* import builtin types */

extern type_def *builtin_string;

extern void fix_builtin_string(void *obj, sem_info *f);
 /* Hack to make the builtin type 'string' work so the length of the string
    doesn't matter. Call when 'string' is visible in f. obj is used for errors.
 */

extern long integer_nr_bits(void *x, exec_info *f);
 /* determine the number of bits needed to specify an integer of type x
  * (x should be an integer type)
  */

#endif /* TYPES_H */
