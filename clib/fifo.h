/***********************************************************************
 *    Copyright 1991                                                   *
 *    Department of Computer Science                                   *
 *    California Institute of Technology, Pasadena, CA 91125 USA       *
 *    All rights reserved                                              *
 ***********************************************************************/

#ifndef FIFO_H
#define FIFO_H
#include "llist.h"

typedef struct fifo fifo;
struct fifo
   { int size; /* TODO: remove? */
     llist head, tail;
   };

extern void fifo_init(fifo *x);
 /* initialize x (does not allocate) */

extern void fifo_clear(fifo *x);
 /* remove all elements in x.  x can be deallocated after this. */

extern void fifo_insert(fifo *x, void *d);
 /* insert an element on the bottom of the fifo */

extern void *fifo_extract(fifo *x);
 /* pull an element off the top of the fifo */

extern void *fifo_top(fifo *x);
 /* peek at the top of the fifo */

/* Make old function syntax errors */

#define create_Fifo (}
#define put_Fifo (}
#define get_Fifo (}
#define head_Fifo (}
#define reset_ptr_Fifo (}
#define next_Fifo (}
#define fprint_Fifo (}
#define empty_Fifo (}
#define free_Fifo (}

#endif /* FIFO_H */
