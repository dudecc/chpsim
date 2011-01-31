/* pqueue.c: implementation of a generic priority queue
 * 
 * COPYRIGHT 1991-2010. California Institute of Technology
 * 
 * This file is free software: you can redistribute it and/or modify
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
 * along with this file.  If not, see <http://www.gnu.org/licenses/>.
 * 
 * This is not part of Marcel's original library, and the code quality
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
