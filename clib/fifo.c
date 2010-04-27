/***********************************************************************
 *    Copyright 1991                                                   *
 *    Department of Computer Science                                   *
 *    California Institute of Technology, Pasadena, CA 91125 USA       *
 *    All rights reserved                                              *
 ***********************************************************************/

#include "fifo.h"
#include "llist.h"

extern void fifo_init(fifo *x)
 /* initialize x (does not allocate) */
 { x->head = x->tail = 0;
   x->size = 0;
 }

extern void fifo_clear(fifo *x)
 /* remove all elements in x.  x can be deallocated after this. */
 { llist_free(&x->head, 0, 0);
   x->tail = 0;
   x->size = 0;
 }

extern void fifo_insert(fifo *x, void *d)
 /* insert an element on the bottom of the fifo */
 { llist_append(&x->tail, d);
   if (!x->size) x->head = x->tail;
   else x->tail = llist_alias_tail(&x->tail);
   x->size++;
 }

extern void *fifo_extract(fifo *x)
 /* pull an element off the top of the fifo */
 { if (!x->size) return 0;
   if (x->size == 1) x->tail = 0;
   llist_idx_extract(&x->head, 0);
   x->size--;
 }

extern void *fifo_top(fifo *x)
 /* peek at the top of the fifo */
 { if (!x->size) return 0;
   return llist_head(&x->head);
 }

