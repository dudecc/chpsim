/* modules.h: modules; resolving module dependencies
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

