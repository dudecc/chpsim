/* modules.h: modules; resolving module dependencies

   Author: Marcel van der Goot
*/
#ifndef MODULES_H
#define MODULES_H

#include "sem_analysis.h"
#include "interact.h"

extern void init_modules(void);
 /* call at startup */

extern void read_builtin(user_info *f, lex_tp *L);
 /* Pre: L has been initialized and is not currently in use for parsing.
    Read the "builtin.chp" module.
    Call this before reading other modules.
 */

extern module_def *read_main_module(user_info *f, const char *fnm, lex_tp *L);
 /* Pre: L has been initialized and is not currently in use for parsing.
    (Use !fnm for stdin.)
    Reads and parses the main module, fnm, then does the same with all
    required modules; returns the module for fnm.
    f is used for the search path.
 */

extern module_def *cycle_rep(module_def *d);
 /* Return the representative of d's cycle (d if there is no cycle, but
    that can also happen if there is a cycle). (Only after module_cycles().)
 */

extern module_def *module_cycles(module_def *d, sem_info *f);
 /* depth-first-search to find cycles in the dependency graph.
    Return is the earliest (according to dfs) ancestor reachable from
    d, or 0 if none.
    After module_cycles(root), all nodes v that are part of the same
    cycle have the same representative, cycle_rep(v).
    Assuming module_cycles(root) was called, the modules are also
    put in reverse topological order in f->ml.
 */

extern module_def *find_module(user_info *f, const char *fnm, int exact);
 /* If fnm correspond to a module in f->ml, return it. If exact, only
    exact file name matches are considered.
 */


#endif /* MODULES_H */

