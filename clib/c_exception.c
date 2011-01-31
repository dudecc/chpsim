/*
c_exception.c: Portable method of handling exceptions without special OS
	       or compiler support.

Copyright (c) 2003 Marcel R. van der Goot
All rights reserved.

This file is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This file is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this file.  If not, see <http://www.gnu.org/licenses/>.

*/

#include <stdarg.h>
#include "c_exception.h"
#include <string.h> /* for memcpy() */
#include <stdio.h> /* fprintf(), stderr */
#include <stdlib.h> /* for abort(), exit() */
#include <stddef.h> /* for size_t, in case the above did not define it */

/* You could use vsprintf() instead of vsnprintf(), but that could be
   a source of errors.
*/
extern int vsnprintf(char *s, size_t size, const char *fmt, va_list ap);

/*****************************************************************************/
/* Note: Most of the complexity of the implementation is due to the
   possibility that an exception is raised while handling an exception.
   If the nested exception is caught and handled, the handling code of
   the original exception must continue as if nothing has happened.
   However, the nested exception will have overwritten except_any and
   possibly the exception_class's special fields. Hence, whenever this
   situation may arise (i.e., when an exception is active and a new
   handler is pushed), we must save the original values of except_any
   and possibly the exception_class. For this we use the various
   'active' flags and the 'save' variables. Although this situation rarely
   occurs, we always allocate space for the 'save' variables, because we
   don't want to be caught without space when an exception actually occurs.
   None of the exception handling relies on dynamic memory allocation.
*/

def_exception_class(except_any);

static c_exception *curr_except = 0; /* top-most exception handler */

/* Used when exception mechanism is incorrectly used: */
static exception_class(exception_implementation)
  { EXCEPTION_CLASS;
  };
static def_exception_class(exception_implementation);
enum { EXCEPT_missing_pop, EXCEPT_reraise };

/* To be used for malloc failures: */
def_exception_class(except_malloc);


extern void except_push(const char *src, int lnr,
       			c_exception *h, void *ev, void *sv)
 /* Push h as handler for ev on the handler stack. sv points to memory that
    can be used to save the old value of *ev.
 */
 { exception_class_var e = ev;
   h->class = e;
   if (h != curr_except)
     { h->prev = curr_except;
       curr_except = h;
       h->src = src; h->lnr = lnr;
       if (except_any->_active)
	 { h->save_any = *except_any;
	   h->any_saved = 1;
	 }
       else
	 { h->any_saved = 0; }
       if (e != (void*)except_any && e->_active)
	 { h->save = sv;
	   memcpy(sv, e, e->_size);
	 }
       else
         { h->save = 0; }
     }
   /* else { happens with 'continue' } */
   except_any->_active = 0;
   e->_active = 0;
   h->active = 0;
 }

static void pop_except(exception_class_var raising)
 /* Pop an entry of the exception handler stack. If this happens as part
    of raising an exception then 'raising' indicates that exception.
 */
 { /* Saved values need to be restored, unless that overwrites the
      exception that we are trying to raise.
   */
   if (curr_except->save && curr_except->class != raising)
     { memcpy(curr_except->class, curr_except->save, curr_except->save->_size);
     }
   else
     { curr_except->class->_active = 0; }
   if (curr_except->any_saved && !raising)
     { *except_any = curr_except->save_any; }
   else
     { except_any->_active = 0; }
   curr_except = curr_except->prev;
 }

static void verify_stack(c_exception *h);

extern void _except_pop(c_exception *h)
 /* Pre: h is the top of the exception handler stack.
    Pop h off the stack (called by user code).
 */
 { if (h != curr_except)
     { verify_stack(h); }
   pop_except(0);
 }

static void except_not_found(exception_class_var cp)
 /* Called when there is no handler for exception cp */
 { if (except_any->action != EXCEPT_ACTION_SILENT)
     { if (except_any->msg[0])
         { fprintf(stderr, "\n\nException at %s[%d]:\n%s\n",
		   except_any->file_nm, except_any->line_nr, except_any->msg);
	 }
       else
         { fprintf(stderr, "\n\nException %s(%d) at %s[%d]\n",
		   cp->class_nm, cp->code,
		   except_any->file_nm, except_any->line_nr);
	 }
     }
   if (except_any->action == EXCEPT_ACTION_ABORT)
     { abort(); }
   exit(-1);
 }

static void raise_exception(exception_class_var cp)
 /* raise exception cp, assuming the cp and except_any fields have already
    been filled in.
 */
 { while (curr_except &&
 	  ((curr_except->class != cp && curr_except->class != (void*)except_any
	   ) || curr_except->active
	 ))
     { pop_except(cp); }
   if (curr_except)
     { if (except_any->action == EXCEPT_ACTION_ABORT)
         { except_any->action = EXCEPT_ACTION_MSG; }
       cp->_active = 1;
       except_any->_active = 1;
       curr_except->active = 1;
       longjmp(curr_except->dest, 1);
     }
   else
     { except_not_found(cp); }
 }

static void set_exception(const char *src, int lnr, void *ev, int code)
 /* fill in ev and except_any fields prior to raising an exception */
 { exception_class_var e = ev;
   except_any->file_nm = src;
   except_any->line_nr = lnr;
   except_any->class = ev;
   except_any->code = code;
   e->code = code;
   except_any->msg[0] = 0;
   except_any->action = EXCEPT_ACTION_ABORT;
 }

extern void _except_raise(const char *src, int lnr, void *ev, int code)
 /* raise exception ev */
 { set_exception(src, lnr, ev, code);
   raise_exception(ev);
 }

static const char *except_raise_src = 0; /* arg of _except_Raise() */
static int except_raise_lnr = 0; /* arg of _except_Raise() */

static void except_raise_str(void *ev, int code, const char *fmt, ...)
 /* Raise exception ev, with msg, and src and lnr from except_raise_ variables.
    This is the actual function called by the user's except_Raise().
 */
 { va_list a;
   set_exception(except_raise_src, except_raise_lnr, ev, code);
   va_start(a, fmt);
   vsnprintf(except_any->msg, sizeof(except_any->msg), fmt, a);
   except_any->msg[199] = 0; /* just in case */
   va_end(a);
   raise_exception(ev);
 }

static void except_vraise_str(void *ev, int code, const char *fmt, va_list a)
 /* Raise exception ev, with msg, and src and lnr from except_raise_ variables.
    This is the actual function called by the user's except_vRaise().
 */
 { set_exception(except_raise_src, except_raise_lnr, ev, code);
   vsnprintf(except_any->msg, sizeof(except_any->msg), fmt, a);
   except_any->msg[199] = 0; /* just in case */
   raise_exception(ev);
 }

static void verify_stack(c_exception *h)
 /* Verify that h is the top of the exception handler stack. */
 { if (curr_except == h) return;
   /* presumably a pop was missed */
   except_raise_src = curr_except->src;
   except_raise_lnr = curr_except->lnr;
   curr_except = h;
   except_raise_str(exception_implementation, EXCEPT_missing_pop,
		    "missing except_pop() call for this exception handler");
 }

extern void (*_except_Raise(const char *src, int lnr))
	     (void *ev, int code, const char *fmt, ...)
 /* The user's except_Raise() needs to be a macro because we need to pass
    filename and linenumber. However, there is no standard way of creating
    variable-argument macros (although, e.g., gcc has a method). So the
    macro calls this function, which returns a variable-argument
    function, which then performs the actual work.
 */
 { except_raise_src = src;
   except_raise_lnr = lnr;
   return except_raise_str;
 }

extern void (*_except_vRaise(const char *src, int lnr))
	     (void *ev, int code, const char *fmt, va_list a)
 /* Like _except_Raise() */
 { except_raise_src = src;
   except_raise_lnr = lnr;
   return except_vraise_str;
 }

extern void _except_reraise(const char *src, int lnr, c_exception *h)
 /* Pre: Currently executing the handling code of h.
    Reraise the current exception.
 */
 { if (!h->active)
     { /* except_reraise() used in try{...} instead of except{...} */
       except_raise_src = src;
       except_raise_lnr = lnr;
       except_raise_str(exception_implementation, EXCEPT_reraise,
			"except_reraise() was used outside except{...}");
     }
   if (h != curr_except)
     { verify_stack(h); }
   raise_exception(except_any->class);
 }

/* TODO:
   - signals
   - have we done enough testing?
   - should except_not_found be in a different file?
   - rewrite clib
*/
