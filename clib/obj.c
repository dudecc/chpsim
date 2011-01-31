/*
obj.c: objects

Copyright (c) 2000 Marcel R. van der Goot
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

#include "standard.h"
#include <string.h> /* for memcpy() */
#include "obj.h"

/*extern*/ int app_nr = 0; /* default */

def_exception_class(obj_err);

/********** class of all classes *********************************************/

/* obj_class objects are created statically by DEF_CLASS().
   DEF_CLASS also initializes these objects, and of course static objects
   are never freed.
*/

/* For each class abc, there are
	obj_class _CLASS_abc;
   and
        obj_class *CLASS_abc;
   The pointer variable is what should be used to refer to the class. The
   actual obj_class variable stores the data associated with the class.

   (Originally _CLASS_abc was static, but when I added base classes,
   it had to be declared extern in order to define DEF_CLASS_B.)
*/
DEF_CLASS_B(obj_class, object, 0, 0, 0);

extern void dbg_obj_class(obj_class *x, FILE *f)
 /* print description of x to f */
 { int i, j;
   const char *obj_flags_nm[] = {"zero", "mini", "ref_cnt"};
   const char *sep = "";
   obj_flags flags;
   if (!f)
     { fprintf(f, "dbg_obj_class(0, f): null pointer instead of class\n");
       return;
     }
   fprintf(f, "CLASS(%s) :\n", x->nm);
   fprintf(f, "\tdefined at %s[%d]\n", x->src, x->lnr);
   fprintf(f, "\tsize = %lu\n", (ulong)x->size);
   fprintf(f, "\tinit: %p; term: %p\n", (void*)x->init, (void*)x->term);
   if (x->flags)
     { fprintf(f, "\tflags = 0x%X (", x->flags);
       flags = x->flags;
       for (i = 0, j = 1; flags; i++, j <<= 1) /* j == 2^i */
         { if (IS_SET(flags, j))
	     { if (i < CONST_ARRAY_SIZE(obj_flags_nm))
	         { fprintf(f, "%s%s", sep, obj_flags_nm[i]); }
	       else
	         { fprintf(f, "%s?", sep); }
	       sep = " | ";
	       RESET_FLAG(flags, j);
	     }
	 }
       fprintf(f, ")\n");
     }
   else
     { fprintf(f, "\tflags = 0\n"); }
   if (BASE_CLASS(x))
     { fprintf(f, "\tbase = CLASS_%s\n", BASE_CLASS(x)->nm); }
   else
     { fprintf(f, "\tbase = 0\n"); }
   if (x->app)
     { fprintf(f, "\tapp[%d] = { %p", app_nr, (void*)x->app[0]);
       for (i = 1; i < app_nr; i++)
	 { fprintf(f, ", %p", (void*)x->app[i]); }
       fprintf(f, " }\n");
     }
   else
     { fprintf(f, "\tapp[%d] = 0\n", app_nr); }
   fprintf(f, "\tfreelist: %d out of %d\n", x->freelist_sz, x->freelist_max);
   fprintf(f, "\tinfo = %p\n\n", x->info);
 }


/********** generic object operations ****************************************/

/* When an object is freed, its class is changed to freed_object, so that
   we can catch some potential errors. This class also implements the
   freelists.
*/

DEF_CLASS_B(object, mini_object, 0, 0, 0);
DEF_CLASS(mini_object, 0, 0, OBJ_mini);
DEF_CLASS_B(ref_object, object, 0, 0, OBJ_ref_cnt);
DEF_CLASS_B(ref_mini_object, mini_object, 0, 0, OBJ_mini|OBJ_ref_cnt);
DEF_CLASS(freed_object, 0, 0, OBJ_mini);

/* We could assign the base field in DEF_CLASS, but the nested selection
   needed seemed a bit too ugly. This function is effectively just as
   fast, because BASE_CLASS will call it only once per class.
*/
extern obj_class *base_class(obj_class *c)
 /* return base class of c; if not set, c->base is assigned as well */
 { obj_class *b;
   if (c->base) return c->base;
   if (c == CLASS_mini_object) return 0;
   if (!IS_SET(c->flags, OBJ_mini|OBJ_ref_cnt))
     { b = CLASS_object; }
   else if (!IS_SET(c->flags, OBJ_ref_cnt))
     { b = CLASS_mini_object; }
   else if (!IS_SET(c->flags, OBJ_mini))
     { b = CLASS_ref_object; }
   else
     { b = CLASS_ref_mini_object; }
   c->base = b;
   return b;
 }

extern int obj_kind_is(void *obj, obj_class *c)
 /* true if obj has class c or class c occurs in its base class chain */
 { obj_class *b;
   b = ((object*)obj)->class;
   while (b && b != c)
     { b = BASE_CLASS(b); }
   return b == c;
 }

extern void *obj_cast(const char *src, int lnr, void *obj, obj_class *c)
 /* causes an error if !obj_kind_is(obj, c); otherwise returns obj */
 { if (obj && OBJ_KIND_IS(obj, c)) return obj;
   obj_err->obj = obj;
   if (obj)
     { except_Raise(obj_err, OBJ_err_cast,
	    "%s[%d] Illegal cast: %p is %s, cannot be cast to %s.",
	    src, lnr, obj, CLASS_NAME(((object*)obj)->class), CLASS_NAME(c));
     }
   else
     { except_Raise(obj_err, OBJ_err_cast,
	    "%s[%d] Illegal cast of null pointer.", src, lnr);
     }
   return obj;
 }

extern void dbg_obj(void *obj, FILE *f)
 /* for any object "obj", print generic fields */
 { object *x = obj;
   if (!x)
     { fprintf(f, "dbg_obj(0, f): null pointer instead of object\n");
       return;
     }
   if (x->class == CLASS_freed_object)
     { fprintf(f, "dbg_obj(%p): object has been deallocated\n", obj); }
   else if (CLASS_IS_MINI(x->class))
     { fprintf(f, "%p: CLASS(%s)", obj, x->class->nm);
       if (CLASS_HAS_REF_CNT(x->class))
         { fprintf(f, ", ref_cnt = %d", ((ref_mini_object*)x)->ref_cnt); }
     }
   else
     { fprintf(f, "%p: CLASS(%s), src = %s[%d]", obj,
		x->class->nm, (x->src? x->src : "?"), x->lnr);
       if (CLASS_HAS_REF_CNT(x->class))
         { fprintf(f, ", ref_cnt = %d", ((ref_object*)x)->ref_cnt); }
     }
   putc('\n', f);
 }

extern void *new_obj(obj_class *class, const char *src, int lnr)
 /* create new object of specified class, with src and lnr fields */
 { object *x;
   if (class->freelist_sz)
     { class->freelist_sz--;
       x = class->freelist;
       class->freelist = ((freed_object*)x)->tail;
     }
   else
     { MALLOC(x, class->size); }
   new_static_obj(x, class, src, lnr);
   return x;
 }

extern void new_static_obj
(void *obj, obj_class *class, const char *src, int lnr)
 /* initialize *obj, which must be a statically allocated variable of
    the type corresponding to class.
 */
 { int i, std_size, *refc;
   ubyte *b;
   object *x = obj;
   x->class = class;
   if (CLASS_IS_MINI(class))
     { std_size = sizeof(mini_object);
       refc = &((ref_mini_object*)x)->ref_cnt;
     }
   else
     { x->src = src;
       x->lnr = lnr;
       std_size = sizeof(object);
       refc = &((ref_object*)x)->ref_cnt;
     }
   if (IS_SET(class->flags, OBJ_zero))
     { b = ((ubyte*)x) + std_size;
       for (i = class->size - std_size; i; i--)
         { *b++ = 0; }
     }
   if (IS_SET(class->flags, OBJ_ref_cnt))
     { *refc = 1; }
   if (class->init)
     { class->init(x); }
 }

extern void free_obj(void *obj)
 /* deallocate obj (or decrement reference count); OK to call with 0 */
 { object *x = obj;
   obj_class *class;
   int *refc;
   if (!x) return;
   class = x->class;
   if (class == CLASS_freed_object)
     { obj_err->obj = obj;
       except_Raise(obj_err, OBJ_err_free,
			"free_obj(%p): object has already been freed.", obj);
     }
   if (IS_SET(class->flags, OBJ_ref_cnt))
     { if (CLASS_IS_MINI(class))
         { refc = &((ref_mini_object*)x)->ref_cnt; }
       else
         { refc = &((ref_object*)x)->ref_cnt; }
       (*refc)--;
       if (*refc) return;
     }
   if (class->term) class->term(x);
   x->class = CLASS_freed_object;
   if (class->freelist_sz < class->freelist_max)
     { class->freelist_sz++;
       ((freed_object*)x)->tail = class->freelist;
       class->freelist = x;
     }
   else
     { free(x); }
 }

extern void free_static_obj(void *obj)
 /* Applies obj's termination function, just like free_obj(obj), but does
    not deallocate obj. You must call new_static_obj() before using obj
    again.
 */
 { object *x = obj;
   obj_class *class;
   int *refc;
   if (!x) return;
   class = x->class;
   if (class == CLASS_freed_object)
     { obj_err->obj = obj;
       except_Raise(obj_err, OBJ_err_free,
		"free_static_obj(%p): object has already been freed.", obj);
     }
   if (IS_SET(class->flags, OBJ_ref_cnt))
     { if (CLASS_IS_MINI(class))
         { refc = &((ref_mini_object*)x)->ref_cnt; }
       else
         { refc = &((ref_object*)x)->ref_cnt; }
       (*refc)--;
       if (*refc) return;
     }
   if (x->class->term) x->class->term(x);
   x->class = CLASS_freed_object;
 }

extern int set_obj_freelist(obj_class *class, int max)
 /* Specify the maximum nr of elements in the free list for this class. If
    the actual nr is larger, the extra elements are freed. Return is the
    old maximum. DEF_CLASS() sets this number initially to OBJ_FREELIST_MAX.
    (In the rare case that the objects of this class are too small (< 2 ptrs),
    no free list is possible, and the max stays 0.)
 */
 { freed_object *t;
   int old;
   if (class->size < sizeof(freed_object))
     { return 0; }
   if (max < 0)
     { except_Raise(obj_err, OBJ_err_arg, 
	"set_obj_freelist(%s, %d): negative max size", class->nm, max);
     }
   while (class->freelist_sz > max)
     { class->freelist_sz--;
       t = class->freelist; class->freelist = t->tail;
       free(t);
     }
   old = class->freelist_max;
   class->freelist_max = max;
   return old;
 }

extern void *ref_obj(void *obj)
 /* If obj has a ref_cnt, increment it. Return obj. */
 { ref_object *x = obj;
   ref_mini_object *y;
   obj_class *class = x->class;
   if (!IS_SET(class->flags, OBJ_ref_cnt)) return obj;
   if (CLASS_IS_MINI(class))
     { y = obj;
       y->ref_cnt++;
     }
   else
     { x->ref_cnt++; }
   return obj;
 }

extern void *copy_obj(void *obj, void *info)
 /* Create a copy of obj. If obj's class has a copy function, that function
    is called. Otherwise, all fields are just copied literally (memcpy),
    except that the reference count, if there is one, is set to 1.
    (Calling with obj = 0 is OK.)
 */
 { object *x = obj, *y;
   obj_class *class;
   if (!obj) return 0;
   class = x->class;
   if (class->copy)
     { y = class->copy(obj, info); }
   else
     { y = new_obj(class, 0, 0);
       memcpy(y, x, class->size);
       if (IS_SET(class->flags, OBJ_ref_cnt))
	 { if (CLASS_IS_MINI(class))
	     { ((ref_mini_object*)y)->ref_cnt = 1; }
	   else
	     { ((ref_object*)y)->ref_cnt = 1; }
	 }
     }
   return y;
 }


/********** methods (app functions) ******************************************/

/* obj_func */
static int app_index_error(object *x, void *dummy)
 /* Associated with app index -1. Often an index is stored in a variable
    that must be assigned at run time. Such variables should be initialized
    to -1 at compile time. (Calling with index -1 is different from calling
    with a valid index for which no app_function exists. The latter simply
    results in the default behavior.) 
 */
 { obj_err->obj = x->class;
   except_Raise(obj_err, OBJ_err_app,
	       "APP_OBJ called with uninitialized index for obj of class %s\n",
	       x->class->nm);
   return 0;
 }

extern void set_app(obj_class *class, int i, obj_func *f)
 /* Pre: 0 <= i < app_nr. f is obj_func*, obj_func_p*, or obj_func_v*.
    set class->app[i] = f, allocating class->app if necessary.
    class is CLASS_...
 */
 { int j;
   obj_func **app;
   if (i < 0 || app_nr <= i)
     { obj_err->obj = class;
       except_Raise(obj_err, OBJ_err_app,
		"set_app(%s, i=%d, ...): should be 0 <= i < app_nr = %d\n",
	        class->nm, i, app_nr);
     }
   if (!class->app)
     { NEW_ARRAY(app, app_nr+1);
       LEAK(app);
       class->app = app + 1;
       for (j = 0; j < app_nr; j++)
	 { class->app[j] = 0; }
       app[0] = (obj_func*)app_index_error;
     }
   class->app[i] = f;
 }

extern int base_app_obj_b(int i, void *vx, void *f)
 /* Same as APP_OBJ_B(i, vx, f), except treat vx as if it were an object
    of its base class instead.
 */
 { obj_class *c;
   object *x = vx;
   c = BASE_CLASS(x->class);
   while (c)
     { if (c->app && c->app[i])
         { return ((obj_func*)c->app[i])(vx, f); }
       c = BASE_CLASS(c);
     }
   return 0;
 }

extern void *base_app_obj_pb(int i, void *vx, void *f)
 /* Same as APP_OBJ_PB(i, vx, f), except treat vx as if it were an object
    of its base class instead.
 */
 { obj_class *c;
   object *x = vx;
   c = BASE_CLASS(x->class);
   while (c)
     { if (c->app && c->app[i])
         { return ((obj_func_p*)c->app[i])(vx, f); }
       c = BASE_CLASS(c);
     }
   return 0;
 }

extern void base_app_obj_vb(int i, void *vx, void *f)
 /* Same as APP_OBJ_VB(i, vx, f), except treat vx as if it were an object
    of its base class instead.
 */
 { obj_class *c;
   object *x = vx;
   c = BASE_CLASS(x->class);
   while (c)
     { if (c->app && c->app[i])
         { ((obj_func_v*)c->app[i])(x, f);
	   return;
	 }
       c = BASE_CLASS(c);
     }
 }


/********** errors ***********************************************************/

extern void obj_error(void *obj, const char *fmt, ...)
 /* Prints an error message, including file name and line number (from obj,
    if not 0). A newline is added.
    Then raises an exception (class obj_err, code OBJ_error).
 */
 { va_list a;
   object *x = obj;
   va_start(a, fmt);
   if (obj && !CLASS_IS_MINI(x->class))
     { fprintf(stderr, "%s[%d] error: ", x->src, x->lnr); }
   else
     { fprintf(stderr, "error: "); }
   vfprintf(stderr, fmt, a);
   putc('\n', stderr);
   obj_err->obj = obj;   
   except_vRaise(obj_err, OBJ_error, fmt, a);
   va_end(a);
 }

extern void obj_warning(void *obj, const char *fmt, ...)
 /* Prints a warning message, including file name and line number (from obj,
    if not 0). A newline is added.
 */
 { va_list a;
   object *x = obj;
   va_start(a, fmt);
   if (obj && !CLASS_IS_MINI(x->class))
     { fprintf(stderr, "%s[%d] warning: ", x->src, x->lnr); }
   else
     { fprintf(stderr, "warning: "); }
   vfprintf(stderr, fmt, a);
   putc('\n', stderr);
   va_end(a);
 }


/********** test *************************************************************/

#ifdef OBJ_TST

CLASS(tst)
   { OBJ_FIELDS;
     int y;
   };

static void tst_init(tst *x)
 { x->y = 12345;
 }

DEF_CLASS(tst, tst_init, 0, 0);

extern void main(void)
 { tst *t;
   dbg_obj_class(CLASS_obj_class, stdout);
   t = NEW_OBJ(CLASS_tst);
   printf("dbg_obj(t, stdout):\n");
   dbg_obj(t, stdout);
   printf("dbg_obj_class(t->class, stdout):\n");
   dbg_obj_class(t->class, stdout);
   printf("t->y = %d\n", t->y);
   free_obj(t);
   printf("\nTesting error checking (now comes an error):\n");
   free_obj(t);
 }

#endif /* OBJ_TST */
