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

#include "pqueue.h"
#include "standard.h"

#define INIT_ALLOC 4

extern void pqueue_init(pqueue *q, pqueue_flags flags, pqueue_func *cmp)
 { q->size = 0; q->nr_alloc = INIT_ALLOC;
   if (INIT_ALLOC == 0)
     { q->tbl.data = 0; }
   else if (IS_SET(flags, PQUEUE_priority_int))
     { NEW_ARRAY(q->tbl.n_int, INIT_ALLOC); }
   else
     { NEW_ARRAY(q->tbl.data, INIT_ALLOC); }
   q->flags = flags;
   q->cmp = cmp;
 }

extern void pqueue_term(pqueue *q)
 { if (q->tbl.data)
     { free(q->tbl.data); }
 }

/********** Insert ***********************************************************/

static void pqueue_insert_cmp(pqueue *q, void *x)
 { void *y;
   int size, i;
   i = q->size;
   size = ++q->size;
   if (size > q->nr_alloc)
     { q->nr_alloc *= 2;
       REALLOC_ARRAY(q->tbl.data, q->nr_alloc);
     }
   while (i > 0)
     { y = q->tbl.data[(i-1)/2];
       if ((q->cmp)(x, y) <= 0) break;
       q->tbl.data[i] = y;
       i = (i-1)/2;
     }
   q->tbl.data[i] = x;
 }

extern void pqueue_insert_int(pqueue *q, void *x, int p)
 /* Pre: q->flags == PQUEUE_priority_int
  * Insert element x into the queue with priority p (ignore q->cmp)
  */
 { pqueue_node_int y, xx = {x, p};
   int size, i;
   i = q->size;
   size = ++q->size;
   if (size > q->nr_alloc)
     { q->nr_alloc *= 2;
       REALLOC_ARRAY(q->tbl.n_int, q->nr_alloc);
     }
   while (i > 0)
     { y = q->tbl.n_int[(i-1)/2];
       if (xx.p >= y.p) break;
       q->tbl.n_int[i] = y;
       i = (i-1)/2;
     }
   q->tbl.n_int[i] = xx;
 }

extern void pqueue_insert(pqueue *q, void *x)
 { if (IS_SET(q->flags, PQUEUE_priority_int))
     { pqueue_insert_int(q, x, (q->cmp)(x, q->info)); }
   else
     { pqueue_insert_cmp(q, x); }
 }

/********** Extract **********************************************************/

static void *pqueue_extract_cmp(pqueue *q)
 { void *r, *t, *c0, *c1;
   int size, i = 0;
   if (!q->size) return 0;
   size = --q->size;
   r = q->tbl.data[0];
   t = q->tbl.data[size];
   while (2*i+2 < size) // While node i has two children...
     { c0 = q->tbl.data[2*i+1];
       c1 = q->tbl.data[2*i+2];
       if ((q->cmp)(c0, c1) > 0)
         { if ((q->cmp)(t, c0) > 0) break;
           q->tbl.data[i] = c0;
           i = 2*i+1;
         }
       else
         { if ((q->cmp)(t, c1) > 0) break;
           q->tbl.data[i] = c1;
           i = 2*i+2;
         }
     }
   if (2*i+2 == size) // Node i has exactly one child
     { c0 = q->tbl.data[2*i+1];
       if ((q->cmp)(t, c0) <= 0)
         { q->tbl.data[i] = c0;
           i = 2*i+1;
         }
     }
   q->tbl.data[i] = t;
   return r;
 }

static void *pqueue_extract_int(pqueue *q)
 { pqueue_node_int r, t, c0, c1;
   int size, i = 0;
   if (!q->size) return 0;
   size = --q->size;
   r = q->tbl.n_int[0];
   t = q->tbl.n_int[size];
   while (2*i+2 < size) // While node i has two children...
     { c0 = q->tbl.n_int[2*i+1];
       c1 = q->tbl.n_int[2*i+2];
       if (c0.p < c1.p)
         { if (t.p < c0.p) break;
           q->tbl.n_int[i] = c0;
           i = 2*i+1;
         }
       else
         { if (t.p < c1.p) break;
           q->tbl.n_int[i] = c1;
           i = 2*i+2;
         }
     }
   if (2*i+2 == size) // Node i has exactly one child
     { c0 = q->tbl.n_int[2*i+1];
       if (t.p >= c0.p)
         { q->tbl.n_int[i] = c0;
           i = 2*i+1;
         }
     }
   q->tbl.n_int[i] = t;
   q->last_priority = r.p;
   return r.data;
 }

extern void *pqueue_extract(pqueue *q)
 { if (IS_SET(q->flags, PQUEUE_priority_int))
     { pqueue_extract_int(q); }
   else
     { pqueue_extract_cmp(q); }
 }

/********** Other ************************************************************/

extern void *pqueue_root(pqueue *q)
 { if (!q->size) return 0;
   else if (IS_SET(q->flags, PQUEUE_priority_int))
     { q->last_priority = q->tbl.n_int[0].p;
       return q->tbl.n_int[0].data;
     }
   else
     { return q->tbl.data[0]; }
 }

extern int pqueue_apply(pqueue *q, pqueue_func *f, void *info)
 { int i, ret;
   if (IS_SET(q->flags, PQUEUE_priority_int))
     { for (i = 0; i < q->size; i++)
         { ret = (*f)(q->tbl.n_int[i].data, info);
           if (ret) return ret;
         }
     }
   else
     { for (i = 0; i < q->size; i++)
         { ret = (*f)(q->tbl.data[i], info);
           if (ret) return ret;
         }
     }
   return 0;
 }

extern void *pqueue_find(pqueue *q, pqueue_func *f, void *info)
 { int i;
   void *x;
   for (i = 0; i < q->size; i++)
     { if (IS_SET(q->flags, PQUEUE_priority_int))
         { x = q->tbl.n_int[i].data; }
       else
         { x = q->tbl.data[i]; }
       if ((*f)(x, info)) return x;
     }
   return 0;
 }




