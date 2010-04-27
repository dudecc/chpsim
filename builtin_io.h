/* builtin_io.h: support for stdio.chp

   Author: Marcel van der Goot

   $Id: builtin_io.h,v 1.2 2004/04/13 20:46:13 marcel Exp $
*/
#ifndef BUILTIN_IO_H
#define BUILTIN_IO_H

extern void init_builtin_io(FILE *user_stdout);
 /* call at startup */

extern FILE *builtin_io_change_std(FILE *f, int set_stdout);
 /* Pre: init_builtin_io() has already been called.
    If set_stdout, then set CHP's stdout to f, otherwise set CHP's stdin to f.
    Return is the file previously bound to stdin/stdout (which has not been
    closed). (Return is 0 if the old file was closed in CHP.)
 */

#endif /* BUILTIN_IO_H */
