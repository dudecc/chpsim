/* parse.h: parsing

   Author: Marcel van der Goot

   $Id: parse.h,v 1.1 2002/11/20 21:55:38 marcel Exp $
*/
#ifndef PARSE_H
#define PARSE_H

#include "lex.h"
#include "parse_obj.h"

extern module_def *parse_source_file(lex_tp *L);
 /* Pre: L has been initialized and first symbol has been scanned.
    See also modules.h:read_module().
 */

extern void *parse_expr(lex_tp *L);
 /* Pre: L has been initialized and first symbol has been scanned. */

extern void parse_cleanup(lex_tp *L);
 /* Pre: L->alloc_list has been initialized */

extern void parse_cleanup_delayed(llist *l);
 /* Pre: l is the result of a previous call to parse_delay_cleanup */

extern llist parse_delay_cleanup(lex_tp *L);
 /* Pre: L->alloc_list has been initialized */

#endif /* PARSE_H */
