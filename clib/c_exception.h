/*
c_exception.h: Portable method of handling exceptions without special OS
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
#ifndef C_EXCEPTION_H
#define C_EXCEPTION_H

/********** documentation *****************************************************

----------- exception classes -------------------------------------------------

Exceptions are grouped into exception classes. Within an exception class,
each exception has a code. An exception class 'file_err' is declared as
follows:

        exception_class(file_err) {
            EXCEPTION_CLASS;
	    char name[FILENAME_MAX];
        };

Exception codes are just integers, normally defined with an enumeration:

	enum { FERR_notexist, FERR_readonly, FERR_noaccess };

An exception class definition always starts with EXCEPTION_CLASS; following
that you can add any "custom" fields you want. You can refer to these fields
by treating the class as a global pointer:

	file_err->name

The custom fields should be assigned just before raising an exception. Be
sure to document which fields go with which exception code. Note that fields
must be valid at the point of exception handling, not just when the exception
was raised. This usually excludes pointers to data stored on the stack.

Every exception class C has two standard fields:

        const char * C->class_nm;
        int C->code;

The class_nm is the name of the exception class ("C"). The code
is the exception code with which the exception was raised. Unlike the
custom fields, these fields should be considered read-only.

Exception class declarations usually occur in .h files. Each exception
class must also have a definition, in a .c file. The definition is
is a global definition of the form

	def_exception_class(file_err);

regardless of any custom fields. (The declaration must be visible at the
point of definition.) Although rare, you can use 'static' in front of
exception_class and def_exception_class.

There is a special predefined exception class, except_any, explained below.


---------- exception handlers -------------------------------------------------

To intercept exceptions of the file_err class that may occur in some piece
of code, use the following exception handler construct:

        except(file_err) {
           H
        } try {
           P
        }
        endexcept;

If you forget "endexcept" you will probably get an error message about
unbalanced braces, or a syntax error at the next function definition.

H and P denote arbitrary code. H is the exception handler (for class
file_err); often this is a switch statement for the different exception codes.
P, including any functions it calls, is protected against file_err exceptions
by the exception handler (note that protection is a dynamic property).
H is not protected by this handler, but may be protected by surrounding
handlers.

When the construct is reached, P is executed. If P terminates normally,
execution continues following endexcept. However, if during P a file_err
exception is raised, execution jumps to H. When H terminates, execution
again continues following endexcept (this differs from an interrupt,
which would continue at the point where it was raised).

In general, if an exception is raised, the system finds the closest
surrounding (in a dynamic sense) exception handler of the same class,
or of class except_any. Hence, a handler for except_any protects against
all exceptions. If there is no applicable exception handler, i.e., the
exception was raised in unprotected code, an error message is given and
the program aborts.

In a handler construct (but not in called functions), break and continue have
a special meaning, in addition to their normal use:

  break    - exit the handler construct, and continue after 'endexcept';
	     this can be used in H and in P.
  continue - restart P; this is intended for use in H.

P and H can also be terminated by raising an exception or calling exit().
However, if you must jump out of the handler construct in a different way,
you must call 'except_pop()' first (without arguments). This is especially
true for return statements: use

	except_pop(); return e;

rather than just "return e". It is probably clearer to use 'break' first
and put the 'return' after the handler construct.


---------- raising exceptions -------------------------------------------------

To raise an exception, use

        except_raise(class, code);
or
        except_Raise(class, code, fmt, ...);

In the latter case, fmt and ... are as for printf(), and specify an
error message. The error message is used if no handler is found, and
can also be used by a handler. vsnprintf() is used to limit the error message
to 200 bytes (about 2.5 lines); this is because we do not want to allocate
memory when an exception occurs, as lack of memory could be the exception's
cause. The file name and line number are stored separately (explained below).

There is also
	except_vRaise(class, code, fmt, arg);
which takes a va_list arg, like vprintf().

Inside H, you can use

        except_reraise();

to raise the same exception (class, code, message) again.

If an exception class has custom fields associated with it, you must
assign those fields just before raising the exception.


---------- except_any ---------------------------------------------------------

The special class except_any protects against all exceptions. An exception
handler for except_any is often used to perform some cleanup actions,
such as removing temporary files. The declaration of this class is

        extern exception_class(except_any) {
            EXCEPTION_CLASS;
            const char *file_nm;
            int line_nr;
            exception_class_var class;
            char msg[200];
            except_action action;
        };

The file_nm and line_nr fields identify where the exception was raised;
the class field identifies the particular exception; and the msg field
is the message specified with except_Raise ("" if except_raise was used).
These fields should be considered read-only, and can be accessed in any
exception handler, not just in those for except_any. (The action field
is explained below.)

If exception C was raised:

        except_any->code == C->code
        except_any->file_nm is the name of the file with the raise call
        except_any->line_nr is the line number of the raise call
        except_any->class == C
        except_any->msg is the message of except_Raise (or "")
        except_any->class->class_nm == "C"

Calling except_reraise() does not change any of these fields. Note that
C's custom fields are not available through except_any.

    ******************************************************************
    * Except_any rule:						     *
    * As a matter of principle, an except_any handler should always  *
    * reraise the exception, or raise a different exception, or exit *
    * the program. It should not continue as if nothing happened.    *
    ******************************************************************

Type exception_class_var is used to hold a reference to an exception class.
While unusual, it is allowed to declare variables or parameters of that
type:

	exception_class_var v;
	v = (exception_class_var)file_err;

(The cast is necessary to avoid warnings.) Then raising v is the same as
raising file_err. However, note that the custom fields are not available
through v.


---------- termination --------------------------------------------------------

If there is no handler for a raised exception (either because there never
was one, or because the exception was reraised repeatedly), the program
terminates. At that point, there are three possible actions, indicated by
type except_action:

        typedef enum except_action {
            EXCEPT_ACTION_ABORT,      -- message, abnormal termination
            EXCEPT_ACTION_MSG,        -- message, normal termination
            EXCEPT_ACTION_SILENT      -- no message, normal termination
        } except_action;

"message" means that the exception message is printed, together with the
file name and line number. If no message was specified, the exception class
name and exception code are printed. Abnormal termination is with
abort(): on unix systems this typically causes a core dump. Normal
termination is with exit(-1). The action to be taken is indicated by
except_any->action. This field is set as follows:

        in except_raise/except_Raise:
                except_any->action = EXCEPT_ACTION_ABORT;

        just before entering a handler:
                if (except_any->action == EXCEPT_ACTION_ABORT) {
                    except_any->action = EXCEPT_ACTION_MSG;
                }

In addition, you may assign this field inside a handler, before calling
except_reraise(). Note that there is no point in setting it before calling
except_raise(). ABORT is useful if the handler only took some emergency
actions not directly related to the exception. SILENT is useful if the
handler has already printed an error message. The difference between using
SILENT and just calling exit() is that the latter would prevent any
surrounding handlers from capturing the exception. Note that a user may
interpret an ABORT action as a bug in the program. From the above it should
be clear that an exception which was never caught always results in an ABORT.

It is recommended that you don't use atexit() or on_exit() in programs
that use exception handlers.


---------- nested exception handlers ------------------------------------------

Consider the following handler code:

        except (C) {
            f();
            H'
        }

If f() raises an exception that it does not catch, H' is never reached
(even if it was a C exception). However, if f() raises an exception,
handles it, and then returns normally, then H' is reached without indication
that an exception occurred. The implementation takes care that in that
case, when H' is reached, both except_any and C's custom fields are restored
to the value they had just before the call of f(). This is a good thing,
otherwise it would be hard to call functions from an exception handler.
(Although such nested exceptions are relatively rare, the need to support
them is in fact the major source of complexity in the implementation.)

However, there is one case that does not work: if f() raised a C exception,
caught it with an except_any handler, and then returned as if nothing
happened. In that case, C's custom fields would not be properly restored.
Note that this can only occur if the "except_any rule" mentioned earlier was
violated, which is a good reason to stick to the rule.


----------- predefined exception classes --------------------------------------

In addition to except_any, there is a predefined exception class

        exception_class(except_malloc) {
            EXCEPTION_CLASS;
            unsigned long size;
        };

This exception class has no special meaning, but is provided so that
various library functions can raise it when they fail due to a malloc
failure. Otherwise each library would have to declare its own unique
exception for something that is really not part of the library. There
are no specific exception codes for this class. The intention is that
'size' is assigned the amount of requested memory.

In some cases, improper use of except_raise() or except_pop() may be
detected at run-time. In that case, an "exception_implementation" exception
is raised. Since this class is local (static) to the exception implementation,
it cannot be caught directly; however, it can be caught with an except_any
handler (which should reraise it). There is no guarantee that all improper
uses of these functions can be detected.


----------- overhead ----------------------------------------------------------

In terms of execution time, exception handlers have very little overhead,
namely just the call to setjmp() made when entering a handler construct.

When an exception is raised, a list of all currently surrounding exception
handlers is searched to find an appropriate one. Most programs have only
few handlers, so the search is fast. In any case, presumably efficiency
is not an issue when an exception actually occurs.

There is some memory overhead, due to the need to store the contents of an
exception class in certain cases. Even though that memory is seldom used,
each handler allocates space for it. In particular, each handler allocates
space for except_any, including the msg array. This space is allocated
on the stack, because we wanted to avoid the use of malloc() in the exception
implementation. (This explains why the space is always allocated, as there
is no portable method to allocate stack space dynamically.)


----------- recommendation ----------------------------------------------------

If an existing library simply exits or aborts when an error occurs, the
library can be changed to use exceptions without affecting existing
applications.

In the standard libraries it is somewhat traditional to have the user
check the return value or even errno in order to determine whether the
function failed. This user-must-check model is not only cumbersome for
the user, but has the serious problem that the check may be omitted and
the problem go undetected (for a while). Don't use this method. Instead,
for all errors, raise an exception.

On the other hand, exceptions should not be used for cases which might
not be considered errors (meaning: can safely be ignored), as that would
force the user to add otherwise unnecessary exception handlers. Examples:
failure of fclose() can almost always be ignored; it is not an error if
read() returns fewer values than requested.


*********** (end documentation) **********************************************/

#include <stdarg.h>
#include <setjmp.h>

#define exception_class(X) \
  struct X *X; \
  struct X

#define EXCEPTION_CLASS \
   const char *class_nm; \
   int _size; \
   int code; \
   char _active

#define def_exception_class(X) \
   struct X _ ## X = { #X, sizeof(struct X), 0, 0 }, *X = & _ ## X

typedef struct exception_class_var {
    EXCEPTION_CLASS;
} *exception_class_var;

typedef enum except_action {
    EXCEPT_ACTION_ABORT,	/* message, abnormal termination */
    EXCEPT_ACTION_MSG,		/* message, normal termination */
    EXCEPT_ACTION_SILENT	/* no message, normal termination */
} except_action;

/* Note: The 200 in msg[200] is part of the spec, not up to the implementer:
   There must be some known minimum so we can give meaningful messages, and
   there is no point in providing more than the minimum, since portable code
   would never use the extra space.
*/
extern exception_class(except_any) {
    EXCEPTION_CLASS;
    const char *file_nm;
    int line_nr;
    exception_class_var class;
    char msg[200];
    except_action action;
};

/* Use this for all out-of-memory exceptions, rather than defining your own */
extern exception_class(except_malloc) {
    EXCEPTION_CLASS;
    unsigned long size;
};

typedef struct c_exception c_exception;
struct c_exception {
    jmp_buf dest;
    c_exception *prev; /* stack of handlers */
    exception_class_var class; /* class this handler is for */
    const char *src;
    int lnr;
    struct except_any save_any; /* to save old value */
    exception_class_var save; /* to save old value */
    char any_saved; /* true if save_any is in use */
    char active; /* true if handling exception */
};

#define except(C) \
	{ c_exception except; struct C _except;\
	  while (1) \
	    { except_push(__FILE__, __LINE__, &except, C, &_except); \
	      if (setjmp(except.dest))

#define try   else

#define endexcept \
	      break; \
	    } \
	  _except_pop(&except); \
	}


#define except_raise(V, N) _except_raise(__FILE__, __LINE__, V, N)
extern void _except_raise(const char *src, int lnr, void *ev, int code);

/* portable method of creating variable-argument macro: */
#define except_Raise _except_Raise(__FILE__, __LINE__)
extern void (*_except_Raise(const char *src, int lnr))
	     (void *ev, int code, const char *fmt, ...);

#define except_vRaise _except_vRaise(__FILE__, __LINE__)
extern void (*_except_vRaise(const char *src, int lnr))
	     (void *ev, int code, const char *fmt, va_list a);

#define except_reraise() _except_reraise(__FILE__, __LINE__, &except)
extern void _except_reraise(const char *src, int lnr, c_exception *h);

extern void except_push(const char *src, int lnr,
       			c_exception *h, void *ev, void *sv);

#define except_pop() _except_pop(&except)
extern void _except_pop(c_exception *h);

#endif /* C_EXCEPTION_H */
