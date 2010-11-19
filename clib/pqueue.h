/***********************************************************************
 *    Copyright 1991                                                   *
 *    Department of Computer Science                                   *
 *    California Institute of Technology, Pasadena, CA 91125 USA       *
 *    All rights reserved                                              *
 ***********************************************************************/

/* This is not part of Marcel's original library, and the code quality
 * is a bit lacking.
 */

#ifndef PQUEUE
#define PQUEUE

/********** types ************************************************************/

typedef struct pqueue_node pqueue_node;
struct pqueue_node
   { void *data;
     int left_size, right_size;
     pqueue_node *left, *right;
     pqueue_node *parent;
   };

typedef enum pqueue_flags
   { PQUEUE_use_freelist = 1, /* Limit use of malloc/free */
     PQUEUE_is_stack = PQUEUE_use_freelist << 1
       /* INTERNAL: Determines behavoir for equal priority nodes */
   } pqueue_flags;

typedef int pqueue_func(void *x, void *info);
 /* used for priority function cmp, and for pqueue_apply */

typedef struct pqueue pqueue;
struct pqueue
   { pqueue_func *cmp;
     pqueue_node *root;
     pqueue_node *free;
     int size;
     pqueue_flags flags;
   };

extern void pqueue_init(pqueue *q, pqueue_func *cmp);
 /* Pre: q has been allocated, but not initialized.  cmp takes in two
  * data elements and returns >0 if the first has higher priority
  */

extern void pqueue_term(pqueue *q);
 /* Delete all elements of q.  q itself is not freed */

/********** insertion/extraction *********************************************/

extern pqueue_node *pqueue_insert(pqueue *q, void *x);
 /* Insert element x into the queue */

extern pqueue_node *pqueue_insert_head(pqueue *q, void *x);
 /* Also inserts element x into the queue */

extern void *pqueue_extract(pqueue *q);
 /* Remove and return the highest priority element of q */

extern void pqueue_remove(pqueue *q, pqueue_node *p);
 /* Remove the specified element p from q */

extern void *pqueue_root(pqueue *q);
 /* Return the highest priority element of q */

/********** applying element operations **************************************/

extern int pqueue_apply(pqueue *q, pqueue_func *f, void *info);
 /* Call f(x->data, info) for every node x in q, as long as f returns 0.
  * If f returns non-zero, pqueue_apply returns immediately with the same
  * value. (Otherwise, pqueue_apply returns 0.)
  */

extern void *pqueue_find(pqueue *q, pqueue_func *f, void *info);
 /* f(x->data, info) should return non-zero if x->data is what we want to
  * find.  Return value is x->data, or zero if f never returned non-zero
  */

#endif /* PQUEUE_H */
