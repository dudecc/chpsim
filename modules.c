/* modules.c: modules; resolving module dependencies

   Author: Marcel van der Goot
*/

#ifndef NEED_UINT
#define NOUINT
#endif
#include <sys/types.h> /* for stat() */
#include <sys/stat.h>  /* for stat() */
#include <errno.h>     /* for errno */

#include <standard.h>
#include "print.h"
#include "parse_obj.h"
#include "modules.h"
#include "sem_analysis.h"
#include "parse.h"
#include "interact.h"
#include "types.h"


/********** printing *********************************************************/

static void print_required_module(required_module *x, print_info *f)
 { f->pos += var_str_printf(f->s, f->pos, "requires %v;",
	   print_string_const, x->s);
 }

static void print_module_def(module_def *x, print_info *f)
 { parse_obj *d;
   llist m;
   print_obj_list(&x->rl, f, "\n");
   print_char('\n', f); print_char('\n', f);
   m = x->dl;
   while (!llist_is_empty(&m))
     { d = llist_head(&m);
       if (IS_SET(d->flags, DEF_export))
         { print_string("export ", f); }
       print_obj(d, f);
       print_char('\n', f);
       m = llist_alias_tail(&m);
     }
 }


/********** semantic analysis ************************************************/

static required_module builtin_req;
static module_def *builtin_module;

static void *sem_required_module(required_module *x, sem_info *f)
 { module_def *d, *c;
   if (!IS_SET(x->flags, DEF_forward))
     { SET_FLAG(x->flags, DEF_forward);
       d = cycle_rep(x->m);
       c = cycle_rep(f->curr);
       if (d == c) /* circular dependency between d and c */
         { return x; /* only routines visible, not needed for first pass */ }
     }
   if (x->m->importer == f->curr)
     { /* sem_warning(x, "Module \"%s\" is imported twice", x->m->src); */ }
   else /* importing twice would hide all declarations, so don't */ 
     { import(x->m, f); }
   return x;
 }

static void reset_importer(llist *l, sem_info *f)
 /* llist(required_module); clear the 'importer' fields of the modules */
 { required_module *r;
   llist m;
   m = *l;
   while (!llist_is_empty(&m))
     { r = llist_head(&m);
       r->m->importer = 0;
       m = llist_alias_tail(&m);
     }
 }

static void *sem_module_def(module_def *x, sem_info *f)
 { f->curr = x;
   enter_level(x, &x->import_cxt, f);
   sem_list(&x->rl, f);
   reset_importer(&x->rl, f);
   enter_level(x, &x->cxt, f);
   sem_list(&x->dl, f);
   SET_FLAG(x->flags, DEF_forward);
   if (x == builtin_module)
     { fix_builtin_string(x, f); }
   leave_level(f);
   leave_level(f); /* remove imported declarations */
   return x;
 }

static void import_module_def(module_def *x, sem_info *f)
 { assert(IS_SET(x->flags, DEF_forward));
   x->importer = f->curr;
   import_list(&x->dl, f, 0);
 }


/********** reading modules **************************************************/

static int file_exists(const char *fnm, int warn)
 /* Return true if file exists and is not a directory. If warn, a warning is
    given in case of problems checking file existence (other than
    non-existence) or if the file is a directory.
 */
 { struct stat info;
   if (stat(fnm, &info))
     { if (warn && errno != ENOENT)
         { warning("While accessing %s: %s", fnm, strerror(errno)); }
       return 0;       
     }
   if (S_ISDIR(info.st_mode))
     { if (warn)
         { warning("%s is a directory", fnm); }
       return 0;
     }
   return 1;
 }

static int search_for_module(user_info *f, const char *fnm, parse_obj *parent)
 /* Pre: if not 0, parent is used for the default directory.
    Search fnm, using parent and f->path. If found, return true and write
    the full path name to f->scratch. Otherwise return 0.
 */
 { llist m;
   int pos;
   m = f->path;
   if (!fnm[0])
     { warning("Empty file name");
       return 0;
     }
   if (fnm[0] == '/') /* absolute path specified */
     { var_str_copy(&f->scratch, fnm);
       llist_init(&m);
     }
   else
     { if (fnm[0] == '.' && (fnm[1] == '/' ||
			      (fnm[1] == '.' && fnm[2] == '/'))
	  ) /* relative to parent, but no search path */
         { llist_init(&m); }
       if (parent)
	 { pos = var_str_copy(&f->scratch, parent->src);
	   while (pos > 0 && f->scratch.s[pos] != '/')
	     { pos--; }
	   if (f->scratch.s[pos] == '/')
	     { var_str_slice_copy(&f->scratch, pos+1, fnm, -1); }
	   else
	     { var_str_copy(&f->scratch, fnm); }
	 }
       else
	 { var_str_copy(&f->scratch, fnm); }
     }
   /* f->scratch contains default file name, to be checked first */
   do { if (file_exists(f->scratch.s, 1))
          { return 1; }
	if (llist_is_empty(&m))
	  { break; }
	var_str_printf(&f->scratch, 0, "%s/%s", llist_head(&m), fnm);
	m = llist_alias_tail(&m);
   } while (1);
   return 0;
 }

/* llist_func */
static int is_module(module_def *x, const str *nm)
 /* true if nm is full path name of x */
 { return x->src == nm; }

/* llist_func */
static int match_module(module_def *x, const char *nm)
 /* true if nm is postfix of the path of x */
 { const char *s = x->src;
   const char *p, *q;
   p = s + strlen(s);
   q = nm + strlen(nm);
   while (*p == *q && p != s && q != nm)
     { p--; q--; }
   if (q == nm && *p == *q) /* nm is postfix of s */
     { if (p == s || *(p-1) == '/') /* prefix is directory */
	 { return 1; }
     }
   return 0;
 }

extern module_def *find_module(user_info *f, const char *fnm, int exact)
 /* If fnm correspond to a module in f->ml, return it. If exact, only
    exact file name matches are considered.
 */
 { if (exact || fnm[0] == '/')
     { return llist_find(&f->ml, (llist_func*)is_module, make_str(fnm)); }
   return llist_find(&f->ml, (llist_func*)match_module, fnm);
 }

static module_def *read_module
 (user_info *f, const char *fnm, lex_tp *L, void *parent, int builtin)
 /* Pre: L has been initialized and is not currently in use for parsing.
         if parent, it is a parse_obj (used for the default directory).
    (If !parent, can use !fnm for stdin.)
    If fnm has already been read, returns the already parsed module.
    Otherwise, reads and parses fnm, then does the same with all required
    modules; returns the module for fnm.
    If builtin is set, the builtin module is automatically imported.
    f is used for the search path.
 */
 { module_def *d;
   llist m;
   required_module *r;
   const str *full_nm = NO_INIT;
   if (fnm)
     { if (!search_for_module(f, fnm, parent))
         { if (parent)
	     { sem_error(0, parent, "Module %s not found", fnm); }
	   else
	     { error("Module %s not found", fnm); }
	 }
       full_nm = make_str(f->scratch.s);
       d = llist_find(&L->modules, (llist_func*)is_module, full_nm);
       if (d) return d;
     }
   if (L->fin)
     { fclose(L->fin); L->fin = 0; }
   if (fnm)
     { if (IS_SET(f->flags, USER_verbose))
         { report(f, "Reading \"%s\"", full_nm); }
       L->fin_nm = full_nm;
       FOPEN(L->fin, L->fin_nm, "r");
     }
   else
     { L->fin_nm = "-";
       L->fin = stdin;
     }
   lex_start(L);
   d = parse_source_file(L);
   fclose(L->fin); L->fin = 0;
   if (builtin)
     { llist_prepend(&d->rl, &builtin_req); }
   llist_prepend(&L->modules, d);
   m = d->rl;
   while (!llist_is_empty(&m))
     { r = llist_head(&m);
       if (!r->s[0])
         { sem_error(0, r, "Empty module name"); }
       r->m = read_module(f, r->s, L, r, 1);
       if (r->m == d)
         { sem_error(0, r, "A module cannot depend on itself"); }
       m = llist_alias_tail(&m);
     }
   return d;
 }

extern module_def *read_main_module(user_info *f, const char *fnm, lex_tp *L)
 /* Pre: L has been initialized and is not currently in use for parsing.
    (Use !fnm for stdin.)
    Reads and parses the main module, fnm, then does the same with all
    required modules; returns the module for fnm.
    f is used for the search path.
 */
 { if (fnm && !file_exists(fnm, 0))
     { error("No such file: %s", fnm); }
   return read_module(f, fnm, L, 0, 1);
 }

extern void read_builtin(user_info *f, lex_tp *L)
 /* Pre: L has been initialized and is not currently in use for parsing.
    Read the "builtin.chp" module.
    Call this before reading other modules.
 */
 { builtin_module = read_module(f, "builtin.chp", L, 0, 0);
 }


/********** module dependency cycles *****************************************/

/* The cycle field of a module_def forms a list; the last element of the
   list is the representative of the module's equivalence class, where
   x == y means there is a path (dependency) from x to y and vice versa.
   The representative is always the earliest possible ancestor, as
   defined by the dfs number. A node w is an ancestor of the current node
   v iff w->visited = 1.
*/

extern module_def *cycle_rep(module_def *d)
 /* Return the representative of d's cycle (actually, of d's equivalence
    class).
 */
 { while (d->cycle) d = d->cycle;
   return d;
 }

static void merge_cycles(module_def *x, module_def *y)
 /* merge x and y to belong to the same cycle */
 { x = cycle_rep(x);
   y = cycle_rep(y);
   if (x == y) return;
   if (x->module_nr < y->module_nr)
     { y->cycle = x; }
   else
     { x->cycle = y; }
 }

extern module_def *module_cycles(module_def *d, sem_info *f)
 /* depth-first-search to find cycles in the dependency graph.
    Return is the earliest (according to dfs) ancestor reachable from
    d, or 0 if none.
    After module_cycles(root), all nodes v that are part of the same
    cycle have the same representative, cycle_rep(v).
    Assuming module_cycles(root) was called, the modules are also
    put in reverse topological order in f->ml.
 */
 { module_def *a;
   required_module *r;
   llist m;
   if (d->visited)
     { a = cycle_rep(d);
       if (a->visited == 1) /* ancestor of d is still on stack */
         { return a; }
       return 0;
     }
   d->visited = 1;
   d->module_nr = f->module_nr++;
   m = d->rl;
   while (!llist_is_empty(&m))
     { r = llist_head(&m);
       a = module_cycles(r->m, f);
       if (a)
         { merge_cycles(a, d); }
       m = llist_alias_tail(&m);
     }
   d->visited = 2;
   llist_prepend(&f->ml, d);
   if (d->cycle)
     { return cycle_rep(d); }
   return 0;
 }

/********** breakpoints ******************************************************/

static int brk_module_def(module_def *x, user_info *f)
 { return brk_list(&x->dl, f); }

/*****************************************************************************/

extern void init_modules(void)
 /* call at startup */
 { NEW_STATIC_OBJ(&builtin_req, CLASS_required_module);
   builtin_req.s = strdup("builtin.chp");
   SET_FLAG(builtin_req.flags, DEF_builtin);

   set_print(required_module);
   set_print(module_def);
   set_sem(required_module);
   set_sem(module_def);
   set_import(module_def);
   set_brk(module_def);
 }
