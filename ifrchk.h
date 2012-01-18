/* ifrchk.h: procedures for detecting variable interference
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
 * Authors: Chris Moore
 */

#ifndef IFRCHK_H
#define IFRCHK_H

#include <standard.h>
#include <llist.h>
#include <hash.h>
#include <var_string.h>
#include "parse_obj.h"
#include "print.h"
#include "interact.h"
#include "exec.h"

FLAGS(ifrchk_var_flags)
   { FIRST_FLAG(IFRVAR_read), /* set if var is read in current thread */
     NEXT_FLAG(IFRVAR_write), /* set if var is written in current thread */
     NEXT_FLAG(IFRVAR_pread), /* set if var was read in previous thread */
     NEXT_FLAG(IFRVAR_pwrite), /* set if var was written in previous thread */
     NEXT_FLAG(IFRVAR_rw), /* set if var needs read-write checking */
     NEXT_FLAG(IFRVAR_ww) /* set if var needs write-write checking */
   };

typedef struct ifrchk_var
   { ifrchk_var_flags flags;
     int write_idx;
   } ifrchk_var;

FLAGS(ifrchk_flags)
   { FIRST_FLAG(IFRCHK_set), /* indicates a second pass to set EXPR_ifrchk */
     NEXT_FLAG(IFRCHK_assign), /* set if current expr is being written to */
     NEXT_FLAG(IFRCHK_haschk), /* set if current chp body needs checking */
     NEXT_FLAG(IFRCHK_volstmt) /* set if current stmt needs volatile checking */
   };

typedef struct ifrchk_info
   { ifrchk_flags flags;
     int nr_var, thread_idx;
     ifrchk_var *var;
   } ifrchk_info;

extern void ifrchk_setup(user_info *f);
 /* Compile time check to see which statements need
  * runtime interference checks
  */

extern int app_ifrchk;
#define set_ifrchk(C) set_app(CLASS_ ## C, app_ifrchk, (obj_func*)ifrchk_ ## C)
 /* To set ifrchk_abc as ifrchk function for class abc */
#define set_ifrchk_cp(C,D) \
        set_app(CLASS_ ## C, app_ifrchk, (obj_func*)ifrchk_ ## D)
 /* Used if C is a copy of D, and uses the same ifrchk function */

/********** strict checks ***************************************************/

#define IS_PARALLEL_STMT(S) \
    ((S)->class == CLASS_parallel_stmt \
    || ((S)->class == CLASS_rep_stmt && ((rep_stmt*)(S))->rep_sym == ','))

extern void strict_check_init(process_state *ps, exec_info *f);

extern void strict_check_term(process_state *ps, exec_info *f);

extern void strict_check_read(value_tp *v, exec_info *f);

extern void strict_check_read_elem(value_tp *v, exec_info *f);
/* To be called when one or more elements of v will be read */

extern void strict_check_read_bits(value_tp *v, int l, int h, exec_info *f);
/* To be called when reading bits l through h of integer v */

extern void strict_check_write(value_tp *v, exec_info *f);

extern void strict_check_write_elem(value_tp *v, exec_info *f);
/* To be called when one or more elements of v will be written */

extern void strict_check_write_bits(value_tp *v, int l, int h, exec_info *f);
/* To be called when writing bits l through h of integer v */

extern void strict_check_delete(value_tp *v, exec_info *f);
/* Upon ending a function/routine call, use this to remove
 * all f->prev->var[i] for all i
 * OBSOLETE because stric checking is no longer performed inside of
 * function/routine calls.
 */

extern void strict_check_frame_end(exec_info *f);
/* Pre: f->prev has ended sequential operations and popped the
 * parallel statement f->curr
 */

extern void strict_check_update(ctrl_state *cs, exec_info *f);
/* This should be called after cs has completed a parallel statement
 * (i.e. all children of cs have terminated)
 */

extern void init_ifrchk(int app1);
 /* call at startup; pass unused object app indices */

#endif /* IFRCHK_H */
