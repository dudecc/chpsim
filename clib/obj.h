/*
obj.h: objects

Copyright (c) 2000 Marcel R. van der Goot
All rights reserved.

This file is only for use as part of software written by
Marcel van der Goot. Any other use requires a separate license.
This file contains confidential information, and should not be
redistributed to third parties.

*/
#ifndef OBJ_H
#define OBJ_H

#include "standard.h"


/********** objects and classes ***********************************************

   Note: Below, identifiers in ALL_CAPS indicate macros, which may
   evaluate their arguments more than once.

   Each object has a class (its type), which can be checked at runtime.
   Any object has the following fields (except mini-objects, see below):

	obj_class *class; -- identifies class
	const char *src;  -- source file name
	int lnr;          -- line number in src

   Normally, all objects are created dynamically. Such objects are
   automatically initialized, and termination actions are performed when
   objects are deallocated. (For statically allocated objects, see below.)

   To declare a new object type "tst" with fields y and tail, do:

	CLASS(tst)
	   { OBJ_FIELDS;
	     int y;
	     tst *tail;
	   };

   The "OBJ_FIELDS;" part must always be there. This declaration can be
   put in a .h file.

   This declares a struct type "tst" with the standard fields, "y", and
   "tail". (As can be seen, you can refer to "tst *" within the class
   declaration.) For instance, you can do

	tst *t;
	t = NEW_OBJ(CLASS_tst);
	printf("%d\n", t->y);

   Any object of class tst will have the class field equal to "CLASS_tst",
   so you can do

	if (t->class == CLASS_tst)
	  { printf("%d\n", t->y); }

   To make this useful, the standard fields can be accessed for any object
   type, regardless of the actual type. E.g.,

        object *x;
	if (x->class == CLASS_tst)
	  { printf("%d\n", ((tst*)x)->y); }

   Useful functions are dbg_obj(t, file) and dbg_obj_class(t->class, file).

   If you need mutually recursive classes, you can split CLASS(tst) into

	CLASS_1(tst);
	... use "tst *" ...
	CLASS_2(tst) { OBJ_FIELDS; ... };


*********** defining object classes ******************************************

   Declaring the object class is not enough, the class must also be
   defined as

	DEF_CLASS(tst, tst_init, tst_term, flags);

   This is done at a global level, not inside a function. This should only
   be done once, i.e., in a .c file, not in a .h file. The functions tst_init
   and tst_term have the type

	void tst_init(tst *);
	void tst_term(tst *);

   They are called to initialize and terminate objects (e.g., tst_init
   might initialize the tail field to 0). If no fields need special
   care, you can pass 0 instead. (The standard fields are always initialized
   properly.)

   If you specify the OBJ_zero flag, all fields are set to 0 before any
   init function is called.

   Instead of using CLASS(), a class can also be declared with
	CLASS_COPY(tst, old);
   where old is a previously declared class. (I.e., this must follow
   CLASS(old) or CLASS_2(old).) This gives class tst exactly the
   same fields as class old, similar to "typedef old tst"; but, unlike
   a typedef, tst is a full class with class field CLASS_tst.
   Class tst must still be defined with DEF_CLASS; in most cases the same
   init and term functions as for "old" can be used, but that is not
   a requirement.


*********** methods ***********************************************************

   Just as each class can have its own init and term functions, so can
   other functions be associated with each class. Such functions, of type

	int obj_func(void *obj, void *info);
   or   void *obj_func_p(void *obj, void *info);
   or	void obj_func_v(void *obj, void *info);

   are associated with a global index, 0 <= i < app_nr, chosen by the
   application. At the beginning of execution, the application must set
   app_nr to the number of slots for such functions. (Each class will
   have the same slots.) Say we want to associate a print function
   with each class, with index 'App_print'. The print function for the
   tst object might be

	int print_tst(tst *obj, FILE *f);

   To associate this function with the tst class, execute

	set_app(CLASS_tst, App_print, (obj_func*)print_tst);

   For any object x, the print function can then be called by

	APP_OBJ(App_print, x, stdout);

   If x has no associated print function, APP_OBJ simply returns 0.

   There are different versions of APP_OBJ, depending on the type of
   the obj_func and on the default (the default applies when the object
   has no associated function for the specified index). Each APP_OBJ
   macro has the arguments
      (int index, void *obj, void *info)
   the _Z macros have an additional z argument


   type         |  obj_func      |  obj_func_p      |  obj_func_v     |
   return type  |    int         |     void *       |     void        |
   -------------+----------------+------------------+-----------------+
                |  APP_OBJ       |  APP_OBJ_P       |  APP_OBJ_V      |
   default:     |  return 0      |  return 0        |  (nothing)      |
   -------------+----------------+------------------+-----------------+
                |  APP_OBJ_Z     |  APP_OBJ_PZ      |                 |
   extra arg:   |    int z       |    void *z       |                 |
   default:     |  return z      |  return z        |                 |
   -------------+----------------+------------------+-----------------+
                |  APP_OBJ_FZ    |  APP_OBJ_PFZ     |  APP_OBJ_VFZ    |
   extra arg:   |   obj_func *z  |   obj_func_p *z  |   obj_func_v *z |
   default:     |  z(obj, info)  |  z(obj, info)    |  z(obj, info)   |
   -------------+----------------+------------------+-----------------+
                |  APP_OBJ_B     |  APP_OBJ_PB      |  APP_OBJ_VB     |
   default:     |  use base class|  use base class  |  use base class |
   -------------+----------------+------------------+-----------------+

   (base classes are explained below)

   Note: If the index for APP_OBJ() etc. is a variable that must be
   initialized at runtime, initialize it to -1 at compile time.

   Note: The methods are stored with the object class, not with each object.
   Hence, the overhead of unused slots is very small. Don't try to reduce
   this overhead by using the same index for different purposes for different
   objects: the savings aren't worth the problems this causes.

   CLASS_HAS_APP(class, index) identifies whether the class has a method
   for the index. However, this test is almost never needed, because APP_OBJ
   etc. have a default.


*********** allocation ********************************************************

   Allocation:
   Typically objects are allocated dynamically with one of the following:
	CLASS(tst) { ... };
	tst *x, *y, *z;
	x = NEW_OBJ(CLASS_tst);
	y = new_obj(CLASS_tst, src, lnr);
	z = NEW_OBJ(x->class);
   and deallocated with
        free_obj(x);

   Sometimes it is convenient to allocate an object statically, maybe
   because it is a dummy variable or to make an array of objects. In that
   case, you must initialize the object with one of:
       tst u, v;
       NEW_STATIC_OBJ(&u, CLASS_tst);
       new_static_obj(&v, CLASS_tst, src, lnr);
   To call the object's termination function, use
       free_static_obj(&u);
   Please note: the current implementation does not keep track of which
   objects are dynamic and which are static, so you must not use
   free_obj(&u) for a static object.

   NEW_OBJ() and NEW_STATIC_OBJ() set src and lnr to the current location
   in the code.

   Each class has the option to use a "free list". This is recommended
   if objects of the class are frequently allocated and deallocated.
   NEW_OBJ() and new_obj() take an element from the free list, if it is not
   empty, and free_obj() adds an element to the free list, if it is not full.
   The maximum number of elements in the free list can be changed with
       set_obj_freelist(CLASS_tst, 20);
   The initial maximum (set by DEF_CLASS), is OBJ_FREELIST_MAX. This is
   a macro which is normally 0, but you can #undef and #define it to some
   other value, before using DEF_CLASS. Note that each class has its own
   free list, even if other classes have the same size objects.

   An object can also be allocated with
       y = copy_obj(x, info)
   If x has a copy function, that function is called. However, if x has
   no copy function, then the copy is created by simply copying all fields
   of x (with memcpy); this may not be what you want for pointer fields.
   (In particular, if some of the fields have reference counts, as
   explained below, you should not use the memcpy method.) You may call
   copy_obj with x = 0, in which case the return is also 0.

   A class can be given a copy function with
       SET_COPY(CLASS_tst, cp)
   and you can get the function with
       CLASS_COPY_FUNCTION(CLASS_tst)
   (0 if there is no copy function). The copy function is of type
   obj_func_p. Note that the copy function must allocate the new object.


*********** mini-objects ******************************************************

   The object library was originally developed for use with parsers, which
   is why each object has src and lnr fields. Presumably these would identify
   the location in the user input corresponding to an object. Even when
   no user input is involved, it may be convenient to have these fields,
   where they identify the location in the code where they were created.
   However, you can omit these fields by using "mini-objects":

       CLASS(tst)
	  { MINI_OBJ_FIELDS;
	    int y;
	    tst *tail;
	  };

       DEF_CLASS(tst, tst_init, tst_term, OBJ_mini);

   Hence, a mini-object uses MINI_OBJ_FIELDS rather than OBJ_fields, and
   must use the OBJ_mini flag in DEF_CLASS() (don't forget the flag!).
   A mini-object has the generic "class" field, but not the "src" and "lnr"
   fields. Other than that, mini-objects are just like other objects.

   CLASS_IS_MINI(class) identifies whether a class is a mini-object class.


*********** reference counts **************************************************

   A object class can be defined to include a reference count as follows: 

       CLASS(tst)
	  { REF_OBJ_FIELDS;
	    int y;
	    tst *tail;
	  };

       DEF_CLASS(tst, tst_init, tst_term, OBJ_ref_cnt);

   For a mini-object, use REF_MINI_OBJ_FIELDS instead. Do not forget to
   specify the OBJ_ref_cnt flag. The corresponding generic object classes
   are ref_object and ref_mini_object.

   With this option, each object of the class includes a reference
   count. NEW_OBJ, NEW_STATIC_OBJ, etc. set the reference count to 1 (before
   calling any init function). When a new reference to an object is stored,
   the reference count must be incremented:
       tst *p, *q;
       p = NEW_OBJ(CLASS_tst);
       q = REF_OBJ(p);
   REF_OBJ is a macro that increments p's reference count and returns p; p
   is evaluated twice. Alternatively, use ref_obj(), which is a function that
   does the same. Caveat: REF_OBJ(p) does not cast p, so p must have a proper
   object pointer type; if you must cast p, be careful to distinguish whether
   p is a mini-object or not (see below). The ref_obj() function, however, can
   be called with any object, even those that do not have a reference count.

   When a reference to an object is removed, the reference count must be
   decremented; if there are no references left, the object must be freed.
   Both actions are performed with
       free_obj(p);
   The object's termination function is only called when the reference count
   is actually 0. free_static_obj() likewise decrements the reference count.

   Important: REF_OBJ_FIELDS includes OBJ_FIELDS and REF_MINI_OBJ_FIELDS
   includes MINI_OBJ_FIELDS. This means that the reference count is stored
   at a different offset for regular objects and mini-objects. Hence, if you
   cast an object pointer in order to access the ref_cnt field directly,
   be careful to distinguish between ref_object and ref_mini_object. This
   is particularly true for the argument of REF_OBJ().

   Note: When an object is actually freed, the references it stored to other
   objects are deleted. Hence, their reference counts must be decremented.
   This must be done explicitly by an object's termination function. Be
   careful with circular references: if p->q = q and q->p = p, then you
   cannot rely on p's termination function to remove the reference to q
   and vice versa, because neither termination function will be called in
   the first place. In this case, you may need to check that both reference
   counts are 1, then do something like
       p->q = 0;
       free_obj(q);
   (p->q must be set to 0 so that p's termination function will not try
   to free q again). There may be other solutions. This problem is standard
   for reference counts, and not unique to our implementation.

   free_obj() should not be called with an object with 0 reference count.

   As a matter of principle, any function that returns a ref_object should
   increment the object's reference count, under the assumption that the
   return value will be stored. (If your functions don't do this, specify
   that explicitly; such functions may have problems in concurrent
   environments.) An object's reference count should not be incremented
   merely because the object is passed as function argument.

   CLASS_HAS_REF_CNT(class) identifies whether the class includes a reference
   count.


*********** flags *************************************************************

   DEF_CLASS can be passed the logical OR of the following flags:

       OBJ_zero   - initialize all fields to 0 (before the init function)
       OBJ_mini   - must be specified for mini-objects
       OBJ_ref_cnt - object has a reference count


*********** obj_class *********************************************************

   The obj_class type (which is the class of all classes), has one
   user-accessible field:
       void *info;
   You can use this to store information you want to associate with the
   class (rather than with each object).

   You should not try to access other fields of obj_class directly.

       const char *CLASS_NAME(obj_class *class)
   returns the name of the class, e.g., for use in error messages.

   dbg_obj_class(class, file) prints information about the class.


*********** base classes ******************************************************

   Unlike object oriented languages, the objects defined here do not have
   true inheritance. This is partly because it is hard to do this in C 
   without a clumsy syntax, but mostly because, in my opinion, inheritance
   is of little, if not negative, value. Instead of class A inheriting
   class B, you can just give class A a member b of type B. The only
   difference is that you have to write a->b.x instead of a->x to access
   fields of B. While omitting b is sometimes convenient, I've seen a lot
   of code that had become almost impossible to read because it was so
   hard to find where a->x actually was defined.

   Nevertheless, we suggest you can get a "poor man's inheritance" by
   doing something like:

       #define EXPR_FIELDS \
	   OBJ_FIELDS; \
	   int value

       CLASS(expr)       // generic expression class
	 { EXPR_FIELDS;
	 };

       CLASS(prefix_expr)
	 { EXPR_FIELDS;
	   char operator;
	   expr *r;
	 };

   This way, all types of expressions automatically have a value field.
   Typically, no objects of class expr are created; instead, expr just serves
   to indicate the presence of some kind of expression.

   The above construction is justified because prefix_expr is indeed an
   instance (or specialization) of an expr. However, if you have a
   while_loop class it would be a very bad idea to use EXPR_FIELDS instead
   of just declaring an expr member for the guard. In practice, nested
   inheritance (A inherits B inherits C) is hardly ever justified.

   In this example, class expr is said to be the base class of prefix_expr.
   If you want, you can specify this by replacing the DEF_CLASS by

       DEF_CLASS_B(prefix_expr, expr, expr_init, expr_term, flags);

   Hence, the second argument of DEF_CLASS_B is the base class; otherwise
   DEF_CLASS_B is just like DEF_CLASS.

   You can obtain the base class of a class with
       prefix_expr *x;
       BASE_CLASS(x->class)
   which would return CLASS_expr. Each class can have only one base class.
   A class should never be its own base class. You can also use
       OBJ_BASE_IS(x, CLASS_expr)
       OBJ_KIND_IS(x, CLASS_expr)
   OBJ_BASE_IS is true if x has CLASS_expr or the base class of x is
   CLASS_expr. OBJ_KIND_IS is the same, but recurses through the base
   classes.
       OBJ_CAST(expr, x)
   is the same as (expr*)x, except that it verifies that the cast is valid,
   i.e., that OBJ_KIND_IS(x, CLASS_expr).

   If you use DEF_CLASS, BASE_CLASS() will return one of the four
   predefined generic classes:
       CLASS_object
       CLASS_mini_object
       CLASS_ref_object
       CLASS_ref_mini_object
   (It is better to rely on DEF_CLASS than to set these classes as
   base class with DEF_CLASS_B.)
   If you recurse using BASE_CLASS(), eventually you always end at
   CLASS_mini_object, which has no base class.

   Note that you should use CLASS_IS_MINI() and CLASS_HAS_REF_CNT() to
   determine whether the matching fields exist. E.g., even if x is not
   a mini object, OBJ_KIND_IS(x, CLASS_mini_object) is still true.

   APP_OBJ_B is similar to APP_OBJ, except that if the object does not have
   the requested method, the base class is checked for that method. This
   is done recursively, until there is no base class, in which case the
   default is the same as for APP_OBJ. There is also base_app_obj_b(), which
   does the same, except that it skips directly to the base class even
   if an object does have the requested method. (APP_OBJ_B and
   base_app_obj_b aren't needed very often, because typically you know
   which function is the method of the base class, so you can specify
   that function directly as default with APP_OBJ_FZ.)

   Initialization and termination do *not* automatically call the init and
   term functions of the base class.


*********** other ************************************************************/

extern void obj_error(void *obj, const char *fmt, ...);
 /* Prints an error message, including file name and line number (from obj,
    if not 0). A newline is added.
    Then raises an exception (class obj_err, code OBJ_error).
 */

extern void obj_warning(void *obj, const char *fmt, ...);
 /* Prints a warning message, including file name and line number (from obj,
    if not 0). A newline is added.
 */

extern void dbg_obj(void *obj, FILE *f);
 /* for any object "obj", print generic fields */

enum obj_err_code
   { OBJ_err_arg = 0, /* error in arguments; no obj */
     OBJ_err_free,    /* reference to freed object */
     OBJ_err_app,     /* illegal app idx */
     OBJ_err_cast,    /* illegal cast (with OBJ_CAST) */
     OBJ_error        /* raised by obj_error() */
   };

extern exception_class(obj_err)
   { EXCEPTION_CLASS;
     void *obj; /* the object that caused the error; valid if code > 0 */
   };
 /* Other than OBJ_error, it seems unlikely that an application can
    recover from this exception.
    Some operations allocate memory; they may raise except_malloc.
 */


/******************************************************************************
*********** (end documentation) **********************************************/

/********** prototypes *******************************************************/

/* standard fields for all (normal) objects */
#define OBJ_FIELDS \
	obj_class *class; \
	const char *src; /* source file name */ \
	int lnr

/* standard fields for mini_objects */
#define MINI_OBJ_FIELDS \
	obj_class *class

/* objects with reference count */
#define REF_OBJ_FIELDS \
	OBJ_FIELDS; \
	int ref_cnt

#define REF_MINI_OBJ_FIELDS \
	MINI_OBJ_FIELDS; \
	int ref_cnt

/* declaring classes */
#define CLASS(X) \
	typedef struct X X; \
	extern obj_class _CLASS_ ## X, *CLASS_ ## X; \
	struct X 

#define CLASS_1(X) \
	typedef struct X X; \
	extern obj_class _CLASS_ ## X, *CLASS_ ## X \

#define CLASS_2(X) \
	struct X 

#define CLASS_COPY(X, Y) \
	typedef Y X; \
	extern obj_class _CLASS_ ## X, *CLASS_ ## X

typedef void obj_func_1(void *obj);
typedef int obj_func(void *obj, void *info);
typedef void *obj_func_p(void *obj, void *info);
typedef void obj_func_v(void *obj, void *info);

extern int app_nr; /* nr of application-specific obj_funcs for each object */

typedef enum obj_flags
   { OBJ_zero = 1, /* initialize all fields (all bytes) to 0 */
     OBJ_mini = OBJ_zero << 1, /* no src and lnr fields */
     OBJ_ref_cnt = OBJ_mini << 1 /* obj has ref_cnt */
   } obj_flags;

CLASS(obj_class)  /* class of all classes */
   { OBJ_FIELDS;
     const char *nm; /* name of class (e.g., for debugging) */
     size_t size;
     obj_func_1 *init, *term;
     obj_func_p *copy;
     obj_flags flags;
     obj_class *base; /* base class */
     obj_func **app; /* 0 or obj_func *app[app_nr]; */
     int freelist_sz, freelist_max; /* nr and max nr elements in freelist */
     void *freelist; /* list with free elements */
     void *info; /* for arbitrary use by user */
   };

CLASS(object) /* generic object */
   { OBJ_FIELDS;
   };

CLASS(mini_object) /* generic mini-object */
   { MINI_OBJ_FIELDS;
   };

CLASS(ref_object) /* object with ref_cnt */
   { REF_OBJ_FIELDS;
   };

CLASS(ref_mini_object) /* mini-object with ref_cnt */
   { REF_MINI_OBJ_FIELDS;
   };

CLASS(freed_object) /* private to implementation */
   { MINI_OBJ_FIELDS;
     freed_object *tail;
   };

#ifndef OBJ_FREELIST_MAX
#define OBJ_FREELIST_MAX 0
#endif

extern void *new_obj(obj_class *class, const char *src, int lnr);
 /* create new object of specified class, with src and lnr fields */

extern void new_static_obj
 (void *obj, obj_class *class, const char *src, int lnr);
 /* initialize *obj, which must be a statically allocated variable of
    the type corresponding to class.
 */

extern void free_obj(void *obj);
 /* deallocate obj (or decrement reference count); OK to call with 0 */

extern void free_static_obj(void *obj);
 /* Applies obj's termination function, just like free_obj(obj), but does
    not deallocate obj. You must call new_static_obj() before using obj
    again.
 */

extern void *copy_obj(void *obj, void *info);
 /* Create a copy of obj. If obj's class has a copy function, that function
    is called. Otherwise, all fields are just copied literally (memcpy),
    except that the reference count, if there is one, is set to 1.
    (Calling with obj = 0 is OK.)
 */

extern void dbg_obj_class(obj_class *x, FILE *f);
 /* print description of x to f.
    E.g., dbg_obj_class(CLASS_object, stdout);
    or dbg_obj_class(obj->class, stdout);
 */

extern int set_obj_freelist(obj_class *class, int max);
 /* Specify the maximum nr of elements in the free list for this class. If
    the current nr is larger, the extra elements are freed. Return is the
    old maximum. DEF_CLASS() sets this number initially to OBJ_FREELIST_MAX.
    (In the rare case that the objects of this class are too small (< 2 ptrs),
    no free list is possible, and the max stays 0.)
 */

extern int base_app_obj_b(int i, void *vx, void *f);
 /* Same as APP_OBJ_B(i, vx, f), except treat vx as if it were an object
    of its base class instead.
 */

extern void *base_app_obj_pb(int i, void *vx, void *f);
 /* Same as APP_OBJ_PB(i, vx, f), except treat vx as if it were an object
    of its base class instead.
 */

extern void base_app_obj_vb(int i, void *vx, void *f);
 /* Same as APP_OBJ_VB(i, vx, f), except treat vx as if it were an object
    of its base class instead.
 */

/********** implementation ***************************************************/

/* macros that take a class intentionally do not have a cast to obj_class */
#define CLASS_IS_MINI(C) ((C)->flags & OBJ_mini)

#define CLASS_HAS_REF_CNT(C) ((C)->flags & OBJ_ref_cnt)

#define CLASS_NAME(C) ((C)->nm)

extern obj_class *base_class(obj_class *c);
 /* return base class of c; if not set, c->base is assigned as well */
extern int obj_kind_is(void *obj, obj_class *c);
 /* true if obj has class c or class c occurs in its base class chain */
extern void *obj_cast(const char *src, int lnr, void *obj, obj_class *c);
 /* causes an error if !obj_kind_is(obj, c); otherwise returns obj */

#define BASE_CLASS(C) (C->base ? C->base : base_class(C))

#define OBJ_BASE_IS(X, C) \
    (((object*)(X))->class == C || BASE_CLASS(((object*)(X))->class) == C)
#define OBJ_KIND_IS(X, C) (OBJ_BASE_IS(X, C) || obj_kind_is(X, C))

#define OBJ_CAST(T, X) (OBJ_KIND_IS(X, CLASS_ ## T) ? (T*)(X) : \
 			(T*)obj_cast(__FILE__, __LINE__, X, CLASS_ ## T))

#define REF_OBJ(X) ((X)->ref_cnt++, (X))
 /* Pre: obj X has a ref_cnt; increment it and return X. */

extern void *ref_obj(void *obj);
 /* If obj has a ref_cnt, increment it. Return obj. */

#define DEF_CLASS(X, I, T, F) \
/*extern*/ obj_class _CLASS_ ## X = { &_CLASS_obj_class, __FILE__, __LINE__, \
	   #X, sizeof(X), (obj_func_1*)I, (obj_func_1*)T, 0, F, \
	   0, 0, 0, \
	   sizeof(X) >= sizeof(freed_object)? OBJ_FREELIST_MAX : 0, \
	   0, 0 }; \
/*extern*/ obj_class *CLASS_ ## X = &_CLASS_ ## X

#define DEF_CLASS_B(X, B, I, T, F) \
/*extern*/ obj_class _CLASS_ ## X = { &_CLASS_obj_class, __FILE__, __LINE__, \
	   #X, sizeof(X), (obj_func_1*)I, (obj_func_1*)T, 0, F, \
	   &_CLASS_ ## B, 0, 0, \
	   sizeof(X) >= sizeof(freed_object)? OBJ_FREELIST_MAX : 0, \
	   0, 0 }; \
/*extern*/ obj_class *CLASS_ ## X = &_CLASS_ ## X

#define NEW_OBJ(C) new_obj(C, __FILE__, __LINE__)

#define NEW_STATIC_OBJ(X, C) new_static_obj(X, C, __FILE__, __LINE__)

#define CLASS_HAS_APP(C, I) \
     ((0 <= I && I < app_nr && (C)->app) ? \
	(int)(C)->app[I] : 0)

#define SET_COPY(C, F) (C)->copy = F

#define CLASS_COPY_FUNCTION(C) (C)->copy

#define APP_OBJ(I, X, F) \
     ((((object*)(X))->class->app && ((object*)(X))->class->app[I]) ? \
 	((object*)(X))->class->app[I]((void*)(X), (void*)(F)) : 0)

#define APP_OBJ_P(I, X, F) \
     ((((object*)(X))->class->app && ((object*)(X))->class->app[I]) ? \
 	((obj_func_p*)((object*)(X))->class->app[I])((void*)(X), (void*)(F)) \
	: 0)

#define APP_OBJ_V(I, X, F) \
   if (((object*)(X))->class->app && ((object*)(X))->class->app[I]) \
     { ((obj_func_v*)((object*)(X))->class->app[I])((void*)(X), (void*)(F)); }\
   else /* skip */

#define APP_OBJ_Z(I, X, F, Z) \
     ((((object*)(X))->class->app && ((object*)(X))->class->app[I]) ? \
 	((object*)(X))->class->app[I]((void*)(X), (void*)(F)) : (int)(Z))

#define APP_OBJ_PZ(I, X, F, Z) \
     ((((object*)(X))->class->app && ((object*)(X))->class->app[I]) ? \
     ((obj_func_p*)((object*)(X))->class->app[I])((void*)(X), (void*)(F)) : \
      (void*)(Z))

#define APP_OBJ_FZ(I, X, F, Z) \
     ((((object*)(X))->class->app && ((object*)(X))->class->app[I]) ? \
 	((object*)(X))->class->app[I]((void*)(X), (void*)(F)) : \
	(Z)((void*)(X), (void*)(F)))

#define APP_OBJ_PFZ(I, X, F, Z) \
     ((((object*)(X))->class->app && ((object*)(X))->class->app[I]) ? \
	((obj_func_p*)((object*)(X))->class->app[I])((void*)(X), (void*)(F)) \
	: (Z)((void*)(X), (void*)(F)))

#define APP_OBJ_VFZ(I, X, F, Z) \
   if (((object*)(X))->class->app && ((object*)(X))->class->app[I]) \
     { ((obj_func_v*)((object*)(X))->class->app[I])((void*)(X), (void*)(F)); }\
   else (Z)((void*)(X), (void*)(F))

#define APP_OBJ_B(I, X, F) \
     ((((object*)(X))->class->app && ((object*)(X))->class->app[I]) ? \
 	((object*)(X))->class->app[I]((void*)(X), (void*)(F)) : \
	base_app_obj_b(I, (void*)(X), (void*)(F)))

#define APP_OBJ_PB(I, X, F) \
     ((((object*)(X))->class->app && ((object*)(X))->class->app[I]) ? \
	((obj_func_p*)((object*)(X))->class->app[I])((void*)(X), (void*)(F)) \
	: base_app_obj_pb(I, (void*)(X), (void*)(F)))

#define APP_OBJ_VB(I, X, F) \
   if (((object*)(X))->class->app && ((object*)(X))->class->app[I]) \
     { ((obj_func_v*)((object*)(X))->class->app[I])((void*)(X), (void*)(F)); }\
   else base_app_obj_vb(I, (void*)(X), (void*)(F))

extern void set_app(obj_class *class, int i, obj_func *f);
 /* Pre: 0 <= i < app_nr. f is obj_func*, obj_func_p*, or obj_func_v*.
    set class->app[i] = f, allocating class->app if necessary.
    class is CLASS_...
 */

#endif /* OBJ_H */
