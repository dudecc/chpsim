/* chpsim.c: command line arguments, main routine
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

/********** initialization ***************************************************/

static void init_chpsim(void)
 {
#ifdef MALLOCDBG
   FOPEN(mallocdbg_log, "malloc.log", "w");
#endif
   app_nr = App_nr_chp;
   init_chp(0, stdout);
 }

/********** input and simulation *********************************************/


static void set_traces(exec_info *f, llist *trl)
 { llist m;
   process_state *ps;
   m = *trl;
   while (!llist_is_empty(&m))
     { ps = find_instance(f->user, make_str(llist_head(&m)), 1);
       SET_FLAG(ps->flags, DBG_trace);
       m = llist_alias_tail(&m);
     }
 }

static void exec_all(user_info *U, process_def *dp, llist *trl)
 /* Execute the program, starting with an instance of dp.
    llist(char*) trl is list of instances to be traced.
 */
 { exec_info *f;
   f = prepare_exec(U, dp);
   set_traces(f, trl);
   interact_instantiate(f);
   prepare_chp(f);
   interact_chp(f);
   report(f->user, "--- done -------------------------------\n");
   term_exec(f);
 }

/********** command line *****************************************************/

static void usage(const char *fmt, ...)
 /* print usage info, with optional error message; exit */
 { va_list a;
   fprintf(stderr, "chpsim - CHP simulator\n"
	"version: %s\n"
	"\n"
	"Usage: chpsim [options] [sourcefile]\n"
	"\n"
	"\t-main id       - specify initial process [main]\n"
	"\t-I dir         - add directory to module search path\n"
	"\t-I-            - clear module search path\n"
	"\t-C command ... - execute a command before interaction\n"
	"\t-batch         - non-interactive execution\n"
	"\t-q             - quit after executing all -C commands\n"
	"\t-log file      - keep log of interaction\n"
	"\t-o file        - use this file for print() and stdout\n",
        version
   );
   fprintf(stderr,
	"\t-v             - report which files are read etc.\n"
	"\t-trace /id     - trace this instance\n"
	"\t-traceall      - trace all processes\n"
	"\t-watchall      - watch all wires\n"
	"\t-timed         - use estimated delays instead of random timing\n"
	"\t-seed N        - use N as seed for PRNG (defult 0)\n"
	"\t-timeseed      - use system clock as seed for PRNG\n"
	"\t-critical      - track critical timing paths\n"
	"\t-nohide        - name and track value-union processes\n"
	"\t-strict        - run with strict checking for variable sharing\n"
	"\n"
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
 { int i = 1, err = 0, showseed = 0;
   long seed = 0;
   const char *fin_nm = 0;
   const char *main_id = 0;
   process_def *dp;
   user_info f_user, *U = &f_user;
   module_def *src_md;
   char *a;
   llist trl; /* llist(char *) trl; instances to be traced */
   llist curr_cmd; /* llist(char *); commands to execute */
   init_chpsim();
   user_info_init(U);
   SET_FLAG(U->flags, USER_random);
   set_default_path(U);
   llist_init(&trl);
   while (i < argc)
     { if (!strcmp(argv[i], "-main"))
         { i = need_arg(argc, argv, i);
	   if (main_id)
	     { usage("More than one -main: %s and %s", main_id, argv[i]); }
	   main_id = argv[i];
	 }
       else if (!strcmp(argv[i], "-o") || !strcmp(argv[i], "-stdout"))
         { i = need_arg(argc, argv, i);
	   FOPEN(U->user_stdout, argv[i], "w");
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
       else if (!strcmp(argv[i], "-q"))
         { SET_FLAG(U->flags, USER_quit); }
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
       else if (!strcmp(argv[i], "-trace"))
         { i = need_arg(argc, argv, i);
	   if (argv[i][0] != '/')
	     { usage("Instance names must start with a '/': -trace %s",
	             argv[i]);
	     }
	   llist_prepend(&trl, argv[i]);
	 }
       else if (!strcmp(argv[i], "-seed"))
         { i = need_arg(argc, argv, i);
           if (sscanf(argv[i], "%ld", &seed) != 1)
             { usage("Integer argument required: -seed %s", argv[i]); }
           showseed = 1;
         }
       else if (!strcmp(argv[i], "-timeseed"))
         { seed = time(0) & 0xffff; showseed = 1; }
       else if (!strcmp(argv[i], "-traceall"))
         { SET_FLAG(U->flags, USER_traceall); }
       else if (!strcmp(argv[i], "-watchall"))
         { SET_FLAG(U->flags, USER_watchall); }
       else if (!strcmp(argv[i], "-nohide"))
         { SET_FLAG(U->flags, USER_nohide); }
       else if (!strcmp(argv[i], "-critical"))
         { SET_FLAG(U->flags, USER_critical);
           RESET_FLAG(U->flags, USER_random);
         }
       else if (!strcmp(argv[i], "-strict"))
         { SET_FLAG(U->flags, USER_strict); }
       else if (!strcmp(argv[i], "-timed"))
         { RESET_FLAG(U->flags, USER_random); }
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
   dp = find_main(src_md, main_id, U, &err);
   srand48(seed);
   if (showseed)
     { report(U, "PRNG seed is %ld\n", seed); }
   if (!dp)
     { report(U, "----------------------------------------\n"
		 "Parsing and typechecking successful.\n"
	         "No %smain process: nothing to execute.\n",
		 err? "valid " : ""
	     );
       exit(err);
     }
   exec_all(U, dp, &trl);
   exit(0);
   return 0;
 }



