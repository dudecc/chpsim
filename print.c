/* print.c: printing the parse structure

   Author: Marcel van der Goot
*/
volatile static const char cvsid[] =
	"$Id: print.c,v 1.2 2004/04/13 20:50:42 marcel Exp $";

#include <standard.h>
#include <obj.h>
#define INLINE_PRINT_C
#include "print.h"
#include "parse_obj.h"

/*extern*/ int app_print = -1;

/*extern*/ int flush_limit = 100; /* limit for PR_flush */

extern void print_info_init(print_info *f)
 /* initialize f without var_string */
 { f->s = 0;
   f->pos = 0;
   f->flags = 0;
   f->f = stdout;
   f->nl = 0;
 }

extern void print_info_init_new(print_info *f)
 /* initialize f with new var_string */
 { NEW(f->s);
   var_str_init(f->s, 100);
   f->pos = 0;
   f->flags = 0;
   f->f = stdout;
   f->nl = 0;
 }

extern void print_info_term(print_info *f)
 /* terminate f (free var_string, but not f) (also flushes if PR_flush) */
 { if (f->s)
     { if (f->pos && IS_SET(f->flags, PR_flush))
         { print_info_flush(f); }
       var_str_free(f->s);
       free(f->s);
       f->s = 0;
     }
   f->pos = 0;
 }

extern void print_ensure_nl(print_info *f)
 /* if f does not end with a newline, add one */
 { if (!f->pos)
     { if (IS_SET(f->flags, PR_flush) && f->nl)
         { return; }
       print_char('\n', f);
     }
   else if (f->s->s[f->pos - 1] != '\n')
     { print_char('\n', f); }
 }

static void no_print(void *x, void *f)
 /* call when obj *x has no print function */
 { error("Internal error: "
	 "Object class %s has no print method", ((object*)x)->class->nm);
 }

extern void print_obj_file(void *obj, FILE *f)
 /* print obj to file (newline added if missing) */
 { print_info info;
   print_info_init_new(&info);
   info.f = f;
   SET_FLAG(info.flags, PR_flush);
   print_obj(obj, &info);
   print_ensure_nl(&info);
   print_info_term(&info);
 }

extern void print_info_flush(print_info *f)
 /* Flush f->s to f->f [default: stdout]. No newline is added. */
 { if (!f->pos) return;
   VAR_STR_X(f->s, f->pos) = 0;
   fprintf(f->f, "%s", f->s->s);
   f->nl = (f->s->s[f->pos - 1] == '\n');
   f->pos = 0;
   fflush(f->f);
 }

static void optional_flush(print_info *f)
 /* Pre: PR_flush is set.
    Optionally flush f->s or part of f->s.
 */
 { int n;
   if (!f->pos) return;
   n = f->pos - 1;
   if (n >= flush_limit || VAR_STR_X(f->s, n) == '\n')
     { VAR_STR_X(f->s, f->pos) = 0;
       fprintf(f->f, "%s", f->s->s);
       f->nl = (f->s->s[n] == '\n');
       f->pos = 0;
     }
 }

extern void print_obj(void *obj, print_info *f)
 /* print obj to f->s[f->pos...], adjust f->pos */
 { APP_OBJ_VFZ(app_print, obj, f, no_print);
   VAR_STR_X(f->s, f->pos) = 0;
   if (IS_SET(f->flags, PR_flush))
     { optional_flush(f); }
 }

/* var_str_func_tp: use as first arg for %v */
extern int vstr_obj(var_string *s, int pos, void *obj)
 /* print obj to s[pos...] */
 { print_info f;
   f.s = s; f.pos = pos; f.flags = 0; f.f = stdout;
   APP_OBJ_VFZ(app_print, obj, &f, no_print);
   VAR_STR_X(f.s, f.pos) = 0;
   return 0;
 }

/* var_str_func2_tp: use as first arg for %V */
extern int vstr_obj2(var_string *s, int pos, void *obj, void *f)
 /* print obj to s[pos...] using flags from print_info f */
 { print_info g, *parent = f;
   g.s = s; g.pos = pos; g.f = stdout;
   g.flags = parent? parent->flags : 0;
   RESET_FLAG(g.flags, PR_flush);
   APP_OBJ_VFZ(app_print, obj, &g, no_print);
   VAR_STR_X(g.s, g.pos) = 0;
   return 0;
 }

/* var_str_func_tp: use as first arg for %v */
extern int vstr_stmt(var_string *s, int pos, void *obj)
 /* same as vstr_obj, but only show beginning of complex stmts */
 { print_info f;
   f.s = s; f.pos = pos; f.flags = PR_short; f.f = stdout;
   APP_OBJ_VFZ(app_print, obj, &f, no_print);
   VAR_STR_X(f.s, f.pos) = 0;
   return 0;
 }

extern void print_obj_list(llist *l, print_info *f, const char *sep)
 /* llist(parse_obj) *l; print elements of l, separated by sep (unless !sep) */
 { llist m;
   parse_obj *x;
   m = *l;
   if (llist_is_empty(&m)) return;
   /* An end_stmt may occur at the end of a list of stmts, to create a
      potential breakpoint. By itself, the end_stmt prints a closing brace
      or bracket. However, as part of a list it should not print anything,
      because the caller will print the closing brace/bracket.
   */
   x = llist_head(&m);
   if (x->class == CLASS_end_stmt)
     { return; }
   if (IS_SET(f->flags, PR_short))
     { APP_OBJ_VFZ(app_print, x, f, no_print);
       m = llist_alias_tail(&m);
       if (sep && !llist_is_empty(&m))
         { x = llist_head(&m);
	   if (x->class != CLASS_end_stmt)
	     { f->pos += var_str_printf(f->s, f->pos, "%s...", sep); }
	 }
     }
   else
     { while (!llist_is_empty(&m))
	 { APP_OBJ_VFZ(app_print, llist_head(&m), f, no_print);
	   m = llist_alias_tail(&m);
	   if (sep && !llist_is_empty(&m))
	     { x = llist_head(&m);
	       if (x->class != CLASS_end_stmt)
	         { f->pos += var_str_printf(f->s, f->pos, "%s", sep); }
	       else
	         { break; }
             }
	 }
     }
   VAR_STR_X(f->s, f->pos) = 0;
   if (IS_SET(f->flags, PR_flush))
     { optional_flush(f); }
 }

extern void init_print(int app)
 /* call at startup; pass unused object app index */
 { app_print = app;
 }
