/* sem_analysis.c: semantic analysis

   Author: Marcel van der Goot
*/
volatile static const char cvsid[] =
        "$Id: sem_analysis.c,v 1.1 2002/11/20 21:55:38 marcel Exp $";

#include <standard.h>
#include "sem_analysis.h"
#include "lex.h"

/* Declaration and definition of identifiers (here we use 'definition' as
   generic term).

   - All names are in the same name space.
   - Scope is static (i.e., does not depend on program execution).
   - Names exported by a required module are imported in the current module.
       - If there is a conflict between imported names, the imported name
         becomes inaccesible in the current module.
   - Names defined in the current module always override imported names.
   - A name defined at a deeper nesting level always overrides the meaning
     of that name defined at a surrounding nesting level.
   - Except for imported names, there may be no conflict between names
     defined at the same nesting level.
     However, two symbols with the same name do not conflict with each other.
   - Routines (processes, functions, procedures) can be defined in any order
     (at the same level). All other names must be defined before their use.
   - Variables (and parameters) must always be local to the routine that
     uses them.
*/


/* Implementation:
   For each name in the hash table, there is a list of id_info, sorted
   by nesting level, highest (= innermost) first. Imported names are at
   level 0, names defined in the current module at levels >= 1 (this implements
   overriding of imported names). A name with a conflict at level 0
   stays in the table, because there could be even more conflicts for
   the same name. (If the second def removed the name, then the third def
   would find no conflict.) Such names are marked invisible.

   FIX_THIS_COMMENT

   The order of definitions of routines is irrelevant. To implement that,
   we do two passes over a list of definitions. The first pass we process
   all definitions normally, except that we do not enter the routine bodies.
   If relevant, an object marks that it has done the first pass by setting
   DEF_forward. After the first pass we remove all entries of the current
   level from the hash table, except for routines. The effect is that we
   have now forward declarations of all routines. (Note that we needed not
   just routine names, but also their parameter/return types. That in turn
   required that we did type and constant definitions on the first pass
   as well.) We follow with a second pass, this time entering routine bodies
   as well. On the second pass any processing that cannot declare a new
   name can be skipped; note that processing a type may declare a new name
   if it is an unnamed symbol type.
*/

#define LL(Q) ((llist*)&q->data.p)
 /* llist *LL(hash_entry *q); */

static int del_all_names(hash_entry *q, sem_info *f);

extern void sem_info_init(sem_info *f)
 /* Intitialize *f */
 { f->cxt = 0; 
   f->flags = 0;
   f->meta_idx = 0;
   f->var_idx = 0;
   f->curr_routine = 0;
   f->prev = 0;
   f->module_nr = 0;
   llist_init(&f->ml);
   f->curr = 0;
   f->L = 0;
 }

extern void sem_info_term(sem_info *f)
 /* Pre: f is at level 0.
    Terminate *f (deletes remaining elements from hash table).
    You must call sem_info_init() to use f again.
 */
 { assert(!f->prev); }

extern void sem_free_obj(sem_info *f, void *x)
 { if (!f->L || !f->L->err_jmp) free_obj(x); }

/*extern*/ int app_sem = -1;

extern void *sem(void *x, sem_info *f)
 /* Apply semantic analysis to parse_obj *x. Return is resulting object.
    In practice, return is x unless semantic info was necessary to resolve
    ambiguities in the grammar.
 */
 { return APP_OBJ_PZ(app_sem, x, f, x);
 }

extern void sem_list(llist *l, sem_info *f)
 /* Apply sem() to each element of l */
 { llist_apply_overwrite(l, (llist_func_p*)sem, f);
 }

extern void sem_rep_common(rep_common *x, sem_info *f)
 { x->l = sem(x->l, f);
   x->h = sem(x->h, f);
   if (x->l->tp.kind != TP_int)
     { sem_error(f, x, "Lower bound of replicator range is not an integer."); }
   if (x->h->tp.kind != TP_int)
     { sem_error(f, x, "Upper bound of replicator range is not an integer."); }
   if (IS_SET(x->l->flags, EXPR_unconst))
     { sem_error(f, x->l, "Lower bound of replicator range is not constant."); }
   if (IS_SET(x->h->flags, EXPR_unconst))
     { sem_error(f, x->h, "Upper bound of replicator range is not constant."); }
 }

extern void sem_error(sem_info *f, void *obj, const char *fmt, ...)
 /* print error msg for parse_obj and exit */
 { va_list a;
   parse_obj *x = obj;
   va_start(a, fmt);
   fprintf(stderr, "%s[%d:%d] Error: ", x->src, x->lnr, x->lpos);
   vfprintf(stderr, fmt, a);
   putc('\n', stderr);
   va_end(a);
   if (f && f->L && f->L->err_jmp)
     { longjmp(*f->L->err_jmp, 1); }
   exit(1);
 }

extern void sem_warning(void *obj, const char *fmt, ...)
 /* print warning msg for parse_obj */
 { va_list a;
   parse_obj *x = obj;
   va_start(a, fmt);
   fprintf(stderr, "\a%s[%d] Warning: ", x->src, x->lnr);
   vfprintf(stderr, fmt, a);
   putc('\n', stderr);
   va_end(a);
 }

/********** scope ************************************************************/

/* Note: Level 0 is used for imported modules; everything defined in
   the current module has level > 0. W.r.t. the hash table the difference
   is that at level > 0 each name can occur only once, whereas at level 0
   a name can occur more than once. The latter occurs if multiple modules
   export the same name; the only reason to insert both instances of the
   name in the table is to give better error messages.
   Below we use the fact that each name occurs only once at level > 0 when
   we delete items.

   FIX THIS COMMENT
*/

extern void enter_level(void *owner, sem_context **cxt, sem_info *f)
 /* Start a nested scope level */
 { sem_info *tmp;
   NEW(tmp);
   *tmp = *f;
   f->prev = tmp;
   if (!*cxt)
     { NEW(f->cxt);
       f->cxt->owner = owner;
       f->cxt->parent = tmp->cxt;
       NEW(f->cxt->H);
       hash_table_init(f->cxt->H, 1, HASH_ptr_is_key, 0);
       *cxt = f->cxt;
     }
   else
     { f->cxt = *cxt; }
   f->var_idx = 0;
   f->rv_ref_depth = -1;
 }

extern void enter_body(void *owner, sem_context **cxt, sem_info *f)
 /* Start a nested scope level that is a process body */
 { sem_info *tmp;
   NEW(tmp);
   *tmp = *f;
   f->prev = tmp;
   if (!*cxt)
     { NEW(f->cxt);
       f->cxt->owner = owner;
       f->cxt->parent = tmp->cxt;
       NEW(f->cxt->H);
       hash_table_init(f->cxt->H, 1, HASH_ptr_is_key, 0);
       *cxt = f->cxt;
     }
   else
     { f->cxt = *cxt; }
   f->rv_ref_depth = -1;
 }

extern void enter_sublevel
(void *owner, const str *id, sem_context **cxt, sem_info *f)
 /* Add a replication scope level within a routine */
 { sem_info *tmp;
   NEW(tmp);
   *tmp = *f;
   f->prev = tmp;
   if (!*cxt)
     { NEW(f->cxt);
       f->cxt->owner = owner;
       f->cxt->parent = tmp->cxt;
       f->cxt->H = 0;
       f->cxt->id = id;
       *cxt = f->cxt;
     }
   else
     { f->cxt = *cxt; }
   if (f->rv_ref_depth >= 0) f->rv_ref_depth++;
 }

extern void leave_level(sem_info *f)
 /* Leave the current scope level */
 { sem_info *tmp;
   tmp = f->prev;
   f->cxt = tmp->cxt;
   f->var_idx = tmp->var_idx;
   f->curr_routine = tmp->curr_routine;
   f->rv_ref_depth = tmp->rv_ref_depth - 1;
   f->prev = tmp->prev;
   free(tmp);
 }


/********** declarations *****************************************************/

extern void declare_id(sem_info *f, const str *id, void *x)
 /* Associate id with parse_obj *x */
 { hash_entry *q;
   id_info *d;
   sem_flags flags = 0;
   parse_obj *obj = x;
   llist l;
   if (!hash_insert(f->cxt->H, id, &q))
     { NEW(d);
       q->data.p = d;
       d->u.d = x;
       d->flags = 0;
       return;
     }
   d = q->data.p;
   if (IS_SET(d->flags, SEM_conflict)) return; /* conflict already noted */
   if (obj->class == CLASS_symbol_type && d->u.d->class == CLASS_symbol_type)
     { if (d->u.d == x && !IS_SET(obj->flags, DEF_forward))
         { sem_error(f, x, "symbol %s defined twice in the same type", id); }
       return; /* not a conflict, symbol is already defined as a symbol */
     }
   if (d->u.d == x) return;
   if (!f->cxt->parent) /* Conflict here is not an immediate error */
     { SET_FLAG(d->flags, SEM_conflict);
       llist_init(&l);
       llist_prepend(&l, d->u.d);
       llist_prepend(&l, x);
       d->u.l = l;
       return;
     }
   SET_FLAG(d->flags, SEM_error);
   sem_error(f, x, "%s was already %s at %s[%d]", id,
     (d->u.d->class == CLASS_var_decl || d->u.d->class == CLASS_parameter)?
            "declared" : "defined", d->u.d->src, d->u.d->lnr);
 }

extern void *find_id(sem_info *f, const str *id, void *obj)
 /* Return object associated with id.
    parse_obj *obj is used for the location in error messages.
 */
 { hash_entry *q;
   id_info *d;
   parse_obj *d1, *d2;
   sem_context *cxt = f->cxt;
   int no_var = 0;
   while (cxt)
     { if (cxt->H)
         { q = hash_find(cxt->H, id);
           if (q) break;
         }
       else if (cxt->id == id) return cxt->owner;
       if (cxt->H && cxt->parent && cxt->parent->owner != cxt->owner)
         { no_var = 1; }
       cxt = cxt->parent;
     }
   if (!cxt)
     { sem_error(f, obj, "Unknown name: %s", id); }
   d = q->data.p;
   if (IS_SET(d->flags, SEM_conflict))
     { d1 = llist_idx(&d->u.l, 0);
       d2 = llist_idx(&d->u.l, 1);
       sem_error(f, obj, "%s is not visible due to an import conflict:\n"
               "\t%s[%d]\n\t%s[%d]\n", id, d1->src, d2->lnr, d2->src, d2->lnr);
     }
   if ((d->u.d->class == CLASS_var_decl || d->u.d->class == CLASS_parameter)
       && no_var)
     { sem_error(f, obj, "%s %s is not in scope",
              (d->u.d->class == CLASS_var_decl)? "Variable" : "Parameter", id);
     }
   return d->u.d;
 }

extern int find_level(sem_info *f, const str *id)
 /* Pre: find_id has already suceeded, returned rep_stmt/expr
    return relative scope level of the object assciated with id.
    (object's relative level = current level - object's absolute level)
 */
 { int level = 0;
   sem_context *cxt = f->cxt;
   while (!cxt->H && cxt->id != id)
     { cxt = cxt->parent;
       level++;
     }
   assert(!cxt->H);
   if (level > f->rv_ref_depth) f->rv_ref_depth = level;
   return level;
 }


/********** importing ********************************************************/

/*extern*/ int app_import = -1;

static void no_import(parse_obj *x, void *f)
 /* called when x has no import function */
 { error("Internal error: "
         "Object class %s has no import method", x->class->nm);
 }

extern void import(void *x, sem_info *f)
 /* Import x. */
 { APP_OBJ_VFZ(app_import, x, f, no_import);
 }

extern void import_list(llist *l, sem_info *f, int all)
 /* If all, import all elements of l; otherwise import only exported elements.
 */
 { llist m;
   parse_obj *x;
   m = *l;
   while (!llist_is_empty(&m))
     { x = llist_head(&m);
       if (all || IS_SET(x->flags, DEF_export))
         { import(x, f); }
       m = llist_alias_tail(&m);
     }
 }


/*****************************************************************************/

extern void init_sem_analysis(int app1, int app2)
 /* call at startup; pass unused object app indices */
 { app_sem = app1;
   app_import = app2;
 }

