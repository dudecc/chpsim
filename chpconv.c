/* chpconv.c: command line arguments, main routine
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

static const char version[] = "2.0";

#include <standard.h>
#include "chp.h"
#include "statement.h"

/********** initialization ***************************************************/

static hash_table proc_names;
static int list_cells;
static const char *last_declare;

static void init_chpconv(void)
 {
#ifdef MALLOCDBG
   FOPEN(mallocdbg_log, "malloc.log", "w");
#endif
   app_nr = App_nr_chp;
   init_chp(0, stdout);
   hash_table_init(&proc_names, 1, 0, 0);
   list_cells = 0;
 }

/********** input and simulation *********************************************/

static void print_meta_process(process_state *p, print_info *f);
/* Pre: p and the children of p are fully instantiated,
 *      including production rules
 * Output the meta process p as structural verilog
 */

static void open_meta_name(process_state *p, print_info *f);
/* Pre: f does not currently have a file open for writing
 * Open the process name with a '.v' extension for writing, and set
 * f to output to this file.
 */

static int declare_new(process_state *p);
/* Return 1 if this process has already been declared */

static void prepare_convert(user_info *U, process_def *dp)
 /* Instantiate the program, starting with an instance of dp. */
 { exec_info *f;
   print_info g;
   llist m;
   char *s;
   process_state *ps;
   f = prepare_exec(U, dp);
   interact_instantiate(f);
   prepare_chp(f);
 }

static void convert_recur(value_tp *v, FILE *o);

static void convert(process_state *ps, FILE *o, int recur)
 /* Convert process ps, and output to file o.  If !o, then output to the
  * file <process name>.v instead. If recur, also output all sub processes.
  */
 { print_info g;
   int i;
   if (declare_new(ps)) return; // Already declared
   if (recur)
     { for (i = 0; i < ps->nr_var; i++)
         { convert_recur(&ps->var[i], o); }
     }
   print_info_init_new(&g);
   SET_FLAG(g.flags, PR_reset);
   if (o)
     { g.f = o; }
   else
     { open_meta_name(ps, &g); }
   print_meta_process(ps, &g);
   print_info_flush(&g);
   if (!o)
     { fclose(g.f); }
   print_info_term(&g);
 }

static void convert_recur(value_tp *v, FILE *o)
 { int i;
   const char* msg;
   process_def *p;
   if (v->rep == REP_array || v->rep == REP_record)
     { for (i = 0; i < v->v.l->size; i++)
         { convert_recur(&v->v.l->vl[i], o); }
     }
   else if (v->rep == REP_process)
     { if (v->v.ps->b->class == CLASS_meta_body)
         { convert(v->v.ps, o, 1); }
       else if (!declare_new(v->v.ps) && list_cells)
         { if (v->v.ps->b->class == CLASS_prs_body)
             { msg = "Using PRS"; }
           else if (v->v.ps->b->class == CLASS_hse_body)
             { msg = "Using HSE"; }
           else
             { msg = "WARNING: Using CHP"; }
           p = v->v.ps->p;
           printf("%s process %s(%s[%d:%d]) as a cell\n", msg, last_declare,
                  p->src, p->lnr, p->lpos);
         }
     }
 }

/********** command line *****************************************************/

static void usage(const char *fmt, ...)
 /* print usage info, with optional error message; exit */
 { va_list a;
   fprintf(stderr, "chpconv - CHP converter\n"
	"version: %s\n"
	"\n"
	"Usage: chpsim [options] [sourcefile]\n"
	"\n"
	"\t-main id       - specify initial process [main]\n"
	"\t-p /id         - convert this instance\n"
	"\t-R             - recursively convert subinstances\n"
	"\t-o file        - specify the desired output filename\n"
	"\t-I dir         - add directory to module search path\n"
	"\t-I-            - clear module search path\n"
	"\t-C command ... - execute a command before interaction\n"
	"\t-batch         - non-interactive instantiation\n"
	"\t-log file      - keep log of interaction\n"
	"\t-v             - report which files are read etc.\n"
	"\t-l             - list all non meta bodies found during recursion\n"
	"\n",
        version
   );
   if (fmt)
     { va_start(a, fmt);
       vfprintf(stderr, fmt, a);
       va_end(a);
       putc('\n', stderr);
       exit(1);
     }
   exit(0);
 }


static int need_arg(int argc, char *argv[], int i)
 /* verify that there is an argument for argv[i], and return i+1 */
 { i++;
   if (i >= argc)
     { usage("%s needs an argument", argv[i-1]); }
   return i;
 }



extern int main(int argc, char *argv[])
 { int i = 1, err = 0, recur = 0;
   const char *fin_nm = 0;
   const char *main_id = 0;
   process_def *dp;
   process_state *ps;
   user_info f_user, *U = &f_user;
   module_def *src_md;
   char *a;
   llist m, pl; /* llist(char *) pl; instances to output */
   llist curr_cmd; /* llist(char *); commands to execute */
   FILE *outfile = 0;
   init_chpconv();
   user_info_init(U);
   set_default_path(U);
   llist_init(&pl);
   while (i < argc)
     { if (!strcmp(argv[i], "-main"))
         { i = need_arg(argc, argv, i);
	   if (main_id)
	     { usage("More than one -main: %s and %s", main_id, argv[i]); }
	   main_id = argv[i];
	 }
       else if (!strcmp(argv[i], "-log"))
         { i = need_arg(argc, argv, i);
	   FOPEN(U->log, argv[i], "w");
	 }
       else if (!strcmp(argv[i], "-batch"))
         { SET_FLAG(U->flags, USER_batch);
	   if (!U->log)
	     { U->log = stderr; }
	 }
       else if (!strcmp(argv[i], "-C"))
         { llist_init(&curr_cmd);
	   i = need_arg(argc, argv, i);
	   llist_append(&curr_cmd, argv[i]);
	   while (i+1 < argc && argv[i+1][0] != '-')
	     { llist_append(&curr_cmd, argv[++i]); }
	   llist_append(&U->cmds, curr_cmd);
	 }
       else if (!strncmp(argv[i], "-I", 2))
         { if (argv[i][2])
	     { a = argv[i] + 2; }
	   else
	     { i = need_arg(argc, argv, i);
	       a = argv[i];
	     }
	   if (!strcmp(a, "-"))
	     { reset_path(U); }
	   else
	     { add_path(U, a); }
	 }
       else if (!strcmp(argv[i], "-v"))
         { SET_FLAG(U->flags, USER_verbose); }
       else if (!strcmp(argv[i], "-p"))
         { i = need_arg(argc, argv, i);
	   if (argv[i][0] != '/')
	     { usage("Instance names must start with a '/': -p %s",
	             argv[i]);
	     }
	   llist_prepend(&pl, argv[i]);
	 }
       else if (!strcmp(argv[i], "-R"))
         { recur = 1; }
       else if (!strcmp(argv[i], "-l"))
         { list_cells = 1; }
       else if (!strcmp(argv[i], "-o"))
         { i = need_arg(argc, argv, i);
           FOPEN(outfile, argv[i], "w");
           if (!outfile)
             { fprintf(stderr, "Could not open file %s for writing", argv[i]);
               exit(1);
             }
         }
       else if (!strcmp(argv[i], "-help") || !strcmp(argv[i], "-h"))
         { usage(0); }
       else if (argv[i][0] == '-')
         { usage("Unknown option: %s", argv[i]); }
       else if (!fin_nm)
         { fin_nm = argv[i]; }
       else
         { usage("Too many arguments: %s", argv[i]); }
       i++;
     }
   builtin_io_change_std(U->user_stdout, 1);
   if (U->log)
     { fprintf(U->log, "Command line:");
       for (i = 0; i < argc; i++)
         { fprintf(U->log, " %s", argv[i]); }
       fprintf(U->log, "\n\n");
     }
   if (IS_SET(U->flags, USER_verbose))
     { report(U, "version: %s", version);
       show_path(U);
     }
   if (!fin_nm)
     { fprintf(stderr, "Warning: reading source from standard input; "
		       "this forces batch mode\n");
       SET_FLAG(U->flags, USER_batch);
       if (!U->log)
	 { U->log = stderr; }
     }
   read_source(U, fin_nm, &src_md);
   dp = find_main(src_md, main_id, U, &err, 1);
   srand48(0);
   if (!dp)
     { report(U, "----------------------------------------\n"
		 "Parsing and typechecking successful.\n"
	         "No %smain process: nothing to convert.\n",
		 err? "valid " : ""
	     );
       exit(err);
     }
   prepare_convert(U, dp);
   if (llist_is_empty(&pl))
     { llist_prepend(&pl, "/"); }
   for (m = pl; !llist_is_empty(&m); m = llist_alias_tail(&m))
     { a = llist_head(&m);
       ps = find_instance(U, make_str(a), 0);
       if (!ps)
         { report(U, "Instance %s does not exist, skipping...", a);
           continue;
         }
       if (ps->b->class != CLASS_meta_body)
         { report(U, "Instance %s is not a meta process, skipping...", a);
           continue;
         }
       convert(ps, outfile, recur);
     }
   report(U, "--- done -------------------------------\n");
   if (outfile) fclose(outfile);
   term_exec(U->global);
   exit(0);
   return 0;
 }

/********** print_meta_process ***********************************************/

static void print_meta_value(value_tp *v, print_info *f)
 { int i;
   switch (v->rep)
     { case REP_bool:
         print_string(v->v.i? "true" : "false", f);
       return;
       case REP_int:
         f->pos += var_str_printf(f->s, f->pos, "%ld", v->v.i);
       return;
       case REP_z:
         var_str_ensure(f->s, f->pos + mpz_sizeinbase(v->v.z->z, 10)+1);
         mpz_get_str(f->s->s + f->pos, 10, v->v.z->z);
         f->pos += strlen(f->s->s + f->pos);
       return;
       case REP_symbol:
         print_string(v->v.s, f);
       return;
       case REP_record: case REP_array:
         for (i = 0; i < v->v.l->size; i++)
           { if (i > 0) print_char('_', f);
             print_meta_value(&v->v.l->vl[i], f);
           }
       return;
       case REP_type:
          print_string("t", f); // Hack, shouldn't be using templated types
       break;
       default: assert(!"Illegal meta value representation");
       return;
     }
 }

static void _print_meta_name(process_state *p, process_def *x, print_info *f)
 { llist l;
   meta_parameter *mp;
   if (((parse_obj*)x->parent)->class == CLASS_process_def)
     { _print_meta_name(p, x->parent, f);
       print_char('_', f);
     }
   print_string(x->id, f);
   l = x->ml;
   while (!llist_is_empty(&l))
     { print_char('_', f);
       mp = llist_head(&l);
       print_meta_value(&p->meta[mp->meta_idx], f);
       l = llist_alias_tail(&l);
     }
 }

static wire_value *find_gate_output(process_state *p)
 /* If p has a single output wire that is not part of a wired channel,
  * return the corresponding wire_value.  Otherwise return 0.
  */
 { llist l;
   var_decl *d;
   value_tp *v;
   wire_value *out = 0;
   for (l = p->p->pl; !llist_is_empty(&l); l = llist_alias_tail(&l))
     { d = llist_head(&l);
       v = &p->var[d->var_idx];
       if (IS_SET(d->flags, EXPR_writable)) /* Possible output */
         { if (v->rep != REP_wire || out) return 0;
           out = v->v.w;
         }
       else /* Verify that this is not a wired channel */
         { while (v->rep == REP_array) v = &v->v.l->vl[0];
           if (v->rep != REP_wire) return 0;
         }
     }
   return out;
 }

static int _check_internal_reset(value_tp *v)
 /* Return 0 if all nodes in v have the WIRE_reset flag set */
 { int i;
   if (v->rep == REP_array)
     { for (i = 0; i < v->v.l->size; i++)
         { if (_check_internal_reset(&v->v.l->vl[i])) return 1; }
     }
   else if (v->rep == REP_wire)
     { return !IS_SET(v->v.w->flags, WIRE_reset); }
   return 0;
 }

static int check_internal_reset(process_state *p)
 /* Return 0 if all internal nodes of p have the WIRE_reset flag set */
 { llist l;
   var_decl *d;
   for (l = p->b->dl; !llist_is_empty(&l); l = llist_alias_tail(&l))
     { d = llist_head(&l);
       if (d->class != CLASS_var_decl) continue;
       if (_check_internal_reset(&p->var[d->var_idx])) return 1;
     }
   return 0;
 }

static void print_meta_name(process_state *p, print_info *f)
 { wire_value *out;
   char *end, *check;
   _print_meta_name(p, p->p, f);
   if (IS_SET(f->flags, PR_reset) && p->b->class == CLASS_prs_body)
     { out = find_gate_output(p);
       if (!out || !IS_SET(out->flags, WIRE_reset)) return;
       end = &f->s->s[f->pos - 3];
       check = IS_SET(out->flags, WIRE_value)? "_hi" : "_lo";
       if (strcmp(end, check)) return;
       if (check_internal_reset(p)) return;
       f->pos -= 3;
       *end = 0;
     }
 }

static void open_meta_name(process_state *p, print_info *f)
/* Pre: f does not currently have a file open for writing
 * Open the process name with a '.v' extension for writing, and set
 * f to output to this file.
 */
 { int pos = f->pos;
   print_meta_name(p, f);
   print_string(".v", f);
   FOPEN(f->f, &f->s->s[pos], "w");
   f->pos = pos;
   VAR_STR_X(f->s, pos) = 0;
 }

static int declare_new(process_state *p)
/* Return 1 if this process has already been declared */
 { print_info g;
   hash_entry *e;
   int ret;
   process_def *x;
   print_info_init_new(&g);
   SET_FLAG(g.flags, PR_reset);
   print_meta_name(p, &g);
   ret = hash_insert(&proc_names, g.s->s, &e);
   if (!ret)
     { e->data.p = p->p; }
   else if (p->p != e->data.p)
     { x = e->data.p;
       if (x->mb || p->p->mb)
         { fprintf(stderr, "Process name %s used in both %s[%d:%d] and "
                   "%s[%d:%d]\n", g.s->s, p->p->src, p->p->lnr, p->p->lpos,
                   x->src, x->lnr, x->lpos);
           exit(1);
         }
     }
   last_declare = e->key;
   print_info_term(&g);
   return ret;
 }

typedef void wire_func(wire_value *, void *);

typedef struct name_info
   { hash_table names; /* (wire_value*) -> (node_name*) */
     var_string scratch;
     int priority;
     print_info *f;
     wire_func *wfn;
     int nl_pos;
     int indent;
     int first;
     int io_stat;
     int wnm_pos;
     process_state *p; // For error output
   } name_info;

/* Priorities:
 * 0: Input wire of a gate
 * 1: Input to undirected port
 * 2: Output from undirected port
 * 3: Output enable from an input port
 * 4: Input data to an input port
 * 5: Input enable to output port
 * 6: Output data from output port
 * 7: Output wire of a gate
 *
 * Also, +10 if this is an external wire from the top level body
 */
#define IS_INPUT(F) (((((F)->priority % 10) % 4) / 2) == 0)

typedef struct node_name
   { char *nm;
     int priority;
     struct node_name *alias; /* only use aliases when necessary */
   } node_name;

static node_name *new_node_name(name_info *f)
 { node_name *nm;
   NEW(nm);
   nm->nm = strdup(f->scratch.s);
   nm->priority = f->priority;
   nm->alias = 0;
   return nm;
 }

static void _node_name_free(node_name *nm)
 { if (nm->alias) _node_name_free(nm->alias);
   free(nm->nm);
   free(nm);
 }

static int node_name_free(hash_entry *e, void *dummy)
 { _node_name_free(e->data.p); }

static void name_info_init(name_info *f)
 { hash_table_init(&f->names, 1, HASH_ptr_is_key, node_name_free);
   var_str_init(&f->scratch, 100);
 }

static void name_info_term(name_info *f)
 { hash_table_free(&f->names);
   var_str_free(&f->scratch);
 }

static void _named_wire_exec(value_tp *v, type *tp, name_info *f)
 { llist l, rl;
   long i;
   record_type *rtps;
   record_field *rf;
   wired_type *wtps;
   wire_decl *wd;
   wire_value *w;
   process_state *ps;
   port_value *p;
   var_decl *d;
   int pos = VAR_STR_LEN(&f->scratch);
   if (tp && tp->kind == TP_generic)
     { fprintf(stderr, "In instance %s of process %s(%s[%d:%d]),\n"
               "port %s has templated type\n", f->p->nm, f->p->p->id,
               f->p->p->src, f->p->p->lnr, f->p->p->lpos, f->scratch.s);
       exit(1);
     }
   switch(v->rep)
     { case REP_record:
         if (tp->kind == TP_wire)
           { wtps = (wired_type*)tp->tps;
             l = wtps->li;
             f->priority++;
             for (i = 0; i < v->v.l->size; i++)
               { wd = (wire_decl*)llist_head(&l);
                 var_str_printf(&f->scratch, pos, "_%s", wd->id);
                 _named_wire_exec(&v->v.l->vl[i], &wd->tps->tp, f);
                 l = llist_alias_tail(&l);
                 if (llist_is_empty(&l)) 
                   { l = wtps->lo;
                     f->priority++;
                   }
               }
             f->priority-=3;
           }
         else
           { l = tp->elem.l;
             rtps = (record_type*)tp->tps;
             rl = rtps->l;
             for (i = 0; i < v->v.l->size; i++)
               { rf = llist_head(&rl);
                 var_str_printf(&f->scratch, pos, "_%s", rf->id);
                 _named_wire_exec(&v->v.l->vl[i], llist_head(&l), f);
                 l = llist_alias_tail(&l);
                 rl = llist_alias_tail(&rl);
               }
           }
       return;
       case REP_array:
         for (i = 0; i < v->v.l->size; i++)
           { var_str_printf(&f->scratch, pos, "_%d", i);
             _named_wire_exec(&v->v.l->vl[i], tp->elem.tp, f);
           }
       return;
       case REP_union:
         _named_wire_exec(&v->v.u->v, &v->v.u->f->tp, f); // Ignore union name
       return;
       case REP_port:
         p = v->v.p->dec? v->v.p : v->v.p->p;
         if (v->v.p->p && p->dec)
           { ps = p->ps;
             assert(!is_visible(ps));
             d = llist_idx(&ps->p->pl, 0);
             if (ps->var[d->var_idx].v.p == p)
               { d = llist_idx(&ps->p->pl, 1); }
             _named_wire_exec(&ps->var[d->var_idx], &p->dec->tps->tp, f);
           }
         else if (v->v.p->p)
           { fprintf(stderr, "In instance %s of process %s(%s[%d:%d]),\n"
                     "port %s is not a wired port\n", f->p->nm, f->p->p->id,
                     f->p->p->src, f->p->p->lnr, f->p->p->lpos, f->scratch.s);
             exit(1);
           }
         else if (v->v.p->nv)
           { assert(v->v.p->nv != v);
             if (v->v.p->dec)
               { _named_wire_exec(v->v.p->nv, &v->v.p->dec->tps->tp, f); }
             else
               { _named_wire_exec(v->v.p->nv, tp, f); }
           }
         else if (v->v.p->v.rep)
           { if (v->v.p->dec)
               { _named_wire_exec(&v->v.p->v, &v->v.p->dec->tps->tp, f); }
             else
               { assert(!"How do I figure out the type here"); }
           }
         else
           { assert(!"Wait, what?"); }
       return;
       case REP_wire:
           w = v->v.w;
           while (IS_SET(w->flags, WIRE_forward)) w = w->u.w;
           (*f->wfn)(w, f);
       return;
       default:
         assert(!"Process has bad ports");
       return;
     }
 }

static void named_wire_exec(process_state *p, name_info *f)
/* Pre: p contains only wires as external ports.
 * Execute f->wfn(w, f) for every external wire w of p, while setting
 * f->scratch to the name of p, and f->priority to a certain value depending
 * on the exact type of wire.
 */
 { llist l;
   var_decl *d;
   int pos = VAR_STR_LEN(&f->scratch), priority = f->priority;
   f->p = p;
   for (l = p->p->pl; !llist_is_empty(&l); l = llist_alias_tail(&l))
     { d = llist_head(&l);
       var_str_printf(&f->scratch, pos, "%s", d->id);
       if (IS_SET(d->flags, EXPR_writable)) f->priority += 7;
       else if ((d->flags & EXPR_port) == EXPR_outport) f->priority += 4;
       else if (IS_SET(d->flags, EXPR_port)) f->priority += 2;
       _named_wire_exec(&p->var[d->var_idx], &d->tp, f);
       f->priority = priority;
     }
 }

static void _name_nodes(wire_value *w, name_info *f)
/* Pre: f->scratch contains a potential name for wire w, and f->priority
 *      is set so that we can tell whether a name is an input (IS_INPUT(f))
 *      and whether a name is essential (i.e. external).
 * Ensure that f->names maps w to either a single non-essential name or a list
 * of one or more essential names.  If one of the essential names is an input,
 * it must also come first on the list.  Since external inputs are drivers,
 * there should be at most one essential input in a given list.  Also, if the
 * wire is tied to a rail, the rail name should be added as an essential name
 * if and only if the wire has no drivers.
 */
 { hash_entry *e;
   node_name *nm, *nmx;
   if (hash_insert(&f->names, (char*)w, &e))
     { nm = e->data.p;
       if (nm->priority == 20) // nm is a rail...
         { if ((f->priority > 9)? IS_INPUT(f) : !IS_INPUT(f)) // w has a driver
             { nmx = nm->alias;
               free(nm->nm);
               free(nm);
               if (!nmx)
                 { e->data.p = new_node_name(f);
                   return;
                 }
               e->data.p = nmx;
             }
         }
       if (nm->priority > 9 && f->priority > 9)
         { nmx = new_node_name(f);
           if (IS_INPUT(f))
             { nmx->alias = nm;
               e->data.p = nmx;
             }
           else
             { nmx->alias = nm->alias;
               nm->alias = nmx;
             }
         }
       else if (f->priority > nm->priority)
         { free(nm->nm);
           nm->nm = strdup(f->scratch.s);
           nm->priority = f->priority;
         }
     }
   else
     { e->data.p = nm = new_node_name(f);
       if (w->wframe->cs == &const_frame)
         { if (nm->priority > 9)
             { if (IS_INPUT(f)) return; // External driver
               NEW(nmx);
               nmx->alias = nm;
               e->data.p = nmx;
             }
           else if (!IS_INPUT(f)) return; // Internal driver
           else
             { nmx = nm;
               free(nmx->nm);
             }
           nmx->nm = strdup(IS_SET(w->flags, WIRE_value)? "Vdd" : "GND");
           nmx->priority = 20;
         }
     }
 }

static void name_nodes(process_state *p, name_info *f)
/* Pre: ps is the top level body, f->scratch = "", and f->priority >= 10,
 *    -OR- ps is a child, f->scratch is the child's name, and f->priority = 0.
 * Ensure that all nodes in the port list of ps have entries in f->names
 */
 { f->wfn = (wire_func*)&_name_nodes;
   named_wire_exec(p, f);
 }

static void __name_meta_nodes(value_tp *v, name_info *f)
 { int i, pos = VAR_STR_LEN(&f->scratch);
   switch (v->rep)
     { case REP_array:
         for (i = 0; i < v->v.l->size; i++)
           { var_str_printf(&f->scratch, pos, "%d_", i);
             __name_meta_nodes(&v->v.l->vl[i], f);
           }
       return;
       case REP_process:
         f->priority = 0;
         name_nodes(v->v.ps, f);
       return;
       default: /* maybe a conditionally instantiated process? */
       return;
     }
 }

static void _name_meta_nodes(process_state *p, llist l, name_info *f)
 { instance_stmt *x;
   while (!llist_is_empty(&l))
     { x = llist_head(&l);
       if (x->class == CLASS_parallel_stmt)
         { _name_meta_nodes(p, ((parallel_stmt*)x)->l, f); }
       else if (x->class == CLASS_compound_stmt)
         { _name_meta_nodes(p, ((compound_stmt*)x)->l, f); }
       else if (x->class == CLASS_rep_stmt)
         { _name_meta_nodes(p, ((rep_stmt*)x)->sl, f); }
       else if (x->class == CLASS_loop_stmt)
         { _name_meta_nodes(p, ((loop_stmt*)x)->gl, f); }
       else if (x->class == CLASS_select_stmt)
         { _name_meta_nodes(p, ((select_stmt*)x)->gl, f); }
       else if (x->class == CLASS_guarded_cmnd)
         { _name_meta_nodes(p, ((guarded_cmnd*)x)->l, f); }
       else if (x->class == CLASS_instance_stmt)
         { var_str_printf(&f->scratch, 0, "%s_", x->d->id);
           __name_meta_nodes(&p->var[x->d->var_idx], f);
         }
       l = llist_alias_tail(&l);
     }
 }

static void name_meta_nodes(process_state *p, name_info *f)
 { _name_meta_nodes(p, p->b->sl, f); }

static void do_indent(name_info *f)
/* Call this after every newline */
 { int i;
   f->nl_pos = f->f->pos;
   for (i = 0; i < f->indent; i++)
      { print_char(' ', f->f); }
 }

static void _print_module(wire_value *w, name_info *f)
 { hash_entry *e;
   node_name *nm;
   if (f->first) f->first = 0;
   else if (f->f->pos - f->nl_pos + VAR_STR_LEN(&f->scratch) > 76)
     { print_string(",\n", f->f);
       do_indent(f);
     }
   else
     { print_string(", ", f->f); }
   print_string(f->scratch.s, f->f);
   e = hash_find(&f->names, (char*)w); /* Check that we generated the same name */
   assert(e);
   nm = e->data.p;
   while (nm && strcmp(nm->nm, f->scratch.s)) nm = nm->alias;
   if (!nm) assert(!"Missing name for an external wire");
 }

static void print_module(process_state *p, name_info *f)
 { print_string("module ", f->f);
   print_meta_name(p, f->f);
   if (f->f->pos - f->nl_pos > 35)
     { f->indent = 4;
       print_char('\n', f->f);
       do_indent(f);
       print_char('(', f->f);
       f->indent++;
     }
   else
     { print_string(" (", f->f);
       f->indent = f->f->pos - f->nl_pos;
     }
   f->first = 1;
   f->wfn = (wire_func*)&_print_module;
   VAR_STR_X(&f->scratch, 0) = 0;
   named_wire_exec(p, f);
   print_string(");\n", f->f);
 }

static void _print_external_nodes(wire_value *w, name_info *f)
 { int is_input = IS_INPUT(f);
   if (f->io_stat == 0)
     { print_string(is_input? "input  " : "output ", f->f); }
   else if (f->io_stat != (is_input? 1 : 2) ||
            f->f->pos - f->nl_pos + VAR_STR_LEN(&f->scratch) > 77)
     { print_string(";\n", f->f);
       do_indent(f);
       print_string(is_input? "input  " : "output ", f->f);
     }
   else
     { print_string(", ", f->f); }
   print_string(f->scratch.s, f->f);
   f->io_stat = is_input? 1 : 2;
 }

static void print_external_nodes(process_state *p, name_info *f)
 { f->indent = 2;
   do_indent(f);
   f->io_stat = 0;
   f->priority = 0;
   f->wfn = (wire_func*)&_print_external_nodes;
   VAR_STR_X(&f->scratch, 0) = 0;
   named_wire_exec(p, f);
   print_string(";\n", f->f);
 }

static int _print_internal_nodes(node_name *nm, name_info *f)
 { if (nm->priority > 9) return 0;
   if (f->first)
     { f->first = 0;
       do_indent(f);
       print_string("wire ", f->f);
     }
   else if (f->f->pos - f->nl_pos + strlen(nm->nm) > 77)
     { print_string(";\n", f->f);
       do_indent(f);
       print_string("wire ", f->f);
     }
   else
     { print_string(", ", f->f); }
   print_string(nm->nm, f->f);
   return 0;
 }

static void print_internal_nodes(name_info *f)
 { f->indent = 2;
   f->first = 1;
   hash_apply_data(&f->names, (hash_vfunc*)_print_internal_nodes, f);
   if (!f->first) print_string(";\n", f->f);
 }

static int _print_aliases(node_name *nm, name_info *f)
 { node_name *nmx = nm->alias;
   while (nmx)
     { do_indent(f);
       print_string("assign ", f->f);
       print_string(nmx->nm, f->f);
       print_string(" = ", f->f);
       print_string(nm->nm, f->f);
       print_string(";\n", f->f);
       nmx = nmx->alias;
     }
   return 0;
 }

static void print_aliases(name_info *f)
 { f->indent = 4;
   hash_apply_data(&f->names, (hash_vfunc*)_print_aliases, f);
 }

static void _print_gate(wire_value *w, name_info *f)
 { hash_entry *e;
   node_name *nm;
   e = hash_find(&f->names, (char*)w);
   assert(e);
   nm = e->data.p;
   if (f->first) f->first = 0;
   else if (f->f->pos - f->nl_pos + strlen(nm->nm)
                      + VAR_STR_LEN(&f->scratch) - f->wnm_pos > 73)
     { print_string(",\n", f->f);
       do_indent(f);
     }
   else
     { print_string(", ", f->f); }
   print_char('.', f->f);
   print_string(&f->scratch.s[f->wnm_pos], f->f);
   print_char('(', f->f);
   print_string(nm->nm, f->f);
   print_char(')', f->f);
 }

static void print_gate(process_state *p, name_info *f)
 { llist l;
   var_decl *d;
   f->indent = 4;
   do_indent(f);
   print_meta_name(p, f->f);
   print_char(' ', f->f);
   print_string(f->scratch.s, f->f);
   if (f->f->pos - f->nl_pos > 45)
     { f->indent = 8;
       print_char('\n', f->f);
       do_indent(f);
       print_char('(', f->f);
       f->indent++;
     }
   else
     { print_string(" (", f->f);
       f->indent = f->f->pos - f->nl_pos;
     }
   f->first = 1;
   f->wnm_pos = VAR_STR_LEN(&f->scratch);
   f->wfn = (wire_func*)_print_gate;
   named_wire_exec(p, f);
   print_string(");\n", f->f);
}

static void __print_gates(value_tp *v, name_info *f)
 { int i, pos = VAR_STR_LEN(&f->scratch);
   switch (v->rep)
     { case REP_array:
         for (i = 0; i < v->v.l->size; i++)
           { var_str_printf(&f->scratch, pos, "_%d", i);
             __print_gates(&v->v.l->vl[i], f);
           }
       return;
       case REP_process:
         print_gate(v->v.ps, f);
       return;
       default: /* maybe a conditionally instantiated process? */
       return;
     }
 }

static void _print_gates(process_state *p, llist l, name_info *f)
 { instance_stmt *x;
   while (!llist_is_empty(&l))
     { x = llist_head(&l);
       if (x->class == CLASS_parallel_stmt)
         { _print_gates(p, ((parallel_stmt*)x)->l, f); }
       else if (x->class == CLASS_compound_stmt)
         { _print_gates(p, ((compound_stmt*)x)->l, f); }
       else if (x->class == CLASS_rep_stmt)
         { _print_gates(p, ((rep_stmt*)x)->sl, f); }
       else if (x->class == CLASS_loop_stmt)
         { _print_gates(p, ((loop_stmt*)x)->gl, f); }
       else if (x->class == CLASS_select_stmt)
         { _print_gates(p, ((select_stmt*)x)->gl, f); }
       else if (x->class == CLASS_guarded_cmnd)
         { _print_gates(p, ((guarded_cmnd*)x)->l, f); }
       else if (x->class == CLASS_instance_stmt)
         { var_str_printf(&f->scratch, 0, "%s", x->d->id);
           __print_gates(&p->var[x->d->var_idx], f);
         }
       l = llist_alias_tail(&l);
     }
 }

static void print_gates(process_state *p, name_info *f)
 { _print_gates(p, p->b->sl, f); }

static void print_meta_process(process_state *p, print_info *f)
/* Pre: p and the children of p are fully instantiated,
 *      including production rules
 * Output the meta process p as structural verilog
 */
 { name_info g;
   name_info_init(&g);
   g.priority = 10;
   g.f = f;
   VAR_STR_X(&g.scratch, 0) = 0;
   name_nodes(p, &g);
   name_meta_nodes(p, &g);
   g.indent = 0;
   do_indent(&g);
   print_module(p, &g);
   print_external_nodes(p, &g);
   print_internal_nodes(&g);
   print_aliases(&g);
   print_gates(p, &g);
   g.indent = 0;
   do_indent(&g);
   print_string("endmodule\n\n", f);
 }



