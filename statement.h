/* statement.h: statements
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
