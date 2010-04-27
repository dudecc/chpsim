/*
llist.c: singly-linked list.

Copyright (c) 1999 Marcel R. van der Goot
All rights reserved.

This file is only for use as part of software written by
Marcel van der Goot. Any other use requires a separate license.
This file contains confidential information, and should not be
redistributed to third parties.

*/

#include "standard.h"
#include "llist.h"

volatile static const char rcsid[] =
  "$Id: llist.c,v 1.1 2003/01/14 20:29:18 marcel Exp $\0$Copyright: 1999 Marcel R. van der Goot $";

/********** general **********************************************************/

/* llist_func: */
extern int llist_free_elem(void *x, void *dummy)
 /* Equivalent to free(x).
    Note: be sure that this free() matches the malloc() used to allocate
    the elements; see mallocdbg.h.
 */
 { free(x);
   return 0;
 }

extern void llist_free(llist *l, llist_func *f, void *info)
 /* Free the list (this makes *l empty).
    If f, call f(x, info) for every x in the list. If you just want to
    free every x, pass llist_free_elem as f.
 */
 { llist k, tmp;
   k = *l;
   while (k)
     { if (f)
         { f(k->head, info); }
       tmp = k; k = k->tail;
       free(tmp);
     }
   *l = 0;
 }

extern int llist_size(const llist *l)
 /* nr of elements */
 { int i = 0;
   llist k = *l;
   while (k)
     { i++;
       k = k->tail;
     }
   return i;
 }

/********** indexing *********************************************************/
/* Valid positions are 0 .. llist_size(l)-1.
   In most cases, -1 indicates the end of the list.
*/

extern void *llist_idx(const llist *l, int i)
 /* Return element at position i.
    If i < 0 ==> return last element (if it exists).
    Otherwise, if position i does not exist, return 0.
 */
 { llist k, prev = 0;
   k = *l;
   while (k && i != 0)
     { prev = k;
       k = k->tail;
       i--;
     }
   if (k)
     { return k->head; }
   else if (prev && i < 0)
     { return prev->head; }
   return 0;
 }

extern void *llist_idx_extract(llist *l, int i)
 /* Same as llist_idx, but also remove the element from l. */
 { llist *k, *prev = 0, e;
   void *x;
   k = l;
   while (*k && i != 0)
     { prev = k;
       k = &(*k)->tail;
       i--;
     }
   if (!*k && prev && i < 0)
     { k = prev; }
   if (*k)
     { x = (*k)->head;
       e = *k;
       *k = (*k)->tail;
       free(e);
       return x;
     }
   return 0;
 }

/********** insertions *******************************************************/

extern void llist_insert(llist *l, int i, const void *x)
 /* Insert x at position i (starting at 0).
    If i < 0 or i = llist_size(l) ==> at end.
    Otherwise, if position i does not exist, nothing happens.
 */
 { llist *k, e;
   k = l;
   while (*k && i != 0)
     { k = &(*k)->tail;
       i--;
     }
   if (i <= 0)
     { NEW(e);
       e->head = (void*)x;
       e->tail = *k;
       *k = e;
     }
 }

extern void *llist_overwrite(llist *l, int i, const void *x)
 /* Replace element at position i by x, return the old element l[i].
    If i < 0 ==> replace last element (if it exists).
    Otherwise, if position i does not exist, nothing happens (return 0).
 */
 { llist k, prev = 0;
   void *old = 0;
   k = *l;
   while (k && i != 0)
     { prev = k;
       k = k->tail;
       i--;
     }
   if (k)
     { old = k->head; k->head = (void*)x; }
   else if (prev && i < 0)
     { old = prev->head; prev->head = (void*)x; }
   return old;
 }

extern void llist_expand(llist *l, int i, llist *k)
 /* insert k in l at position i. I.e., l will get the value
    l[0..i) ++ k ++ l[i..). This will make k empty.
    If i < 0 or i = llist_size(l) ==> at end.
    Otherwise, if position i does not exist, nothing happens.
 */
 { llist *m, t;
   if (!*k) return;
   m = l;
   while (*m && i != 0)
     { m = &(*m)->tail;
       i--;
     }
   if (i <= 0)
     { t = *m;
       *m = *k; *k = 0;
       if (t)
	 { while (*m)
	     { m = &(*m)->tail;
	     }
	   *m = t;
	 }
     }
 }

/********** searching ********************************************************/

extern void *llist_find(const llist *l, llist_func *eq, const void *x)
 /* Look for first element y in the list such that eq(y, x) is not 0;
    return y if found, 0 otherwise.
    If !eq, then look for y == x.
 */
 { llist k;
   k = *l;
   while (k)
     { if (eq)
         { if (eq(k->head, (void*)x))
	     { break; }
	 }
       else if (k->head == (void*)x)
	 { break; }
       k = k->tail;
     }
   if (k) return k->head;
   return 0;
 }

extern int llist_find_idx(const llist *l, llist_func *eq, const void *x)
 /* Look for first element y in the list such that eq(y, x) is not 0;
    return position of y if found, -1 (not 0!) otherwise.
    If !eq, then look for y == x.
 */
 { llist k;
   int i = 0;
   k = *l;
   while (k)
     { if (eq)
         { if (eq(k->head, (void*)x))
	     { break; }
	 }
       else if (k->head == (void*)x)
	 { break; }
       k = k->tail;
       i++;
     }
   if (k) return i;
   return -1;
 }

extern int llist_find_idx_next
 (const llist *l, int j, llist_func *eq, const void *x, void **y)
 /* Look for first element y in l[j..) such that eq(y, x) is not 0;
    If found, assign to *y and return position; if not found, return -1.
    If !eq, then look for y == x.
 */
 { llist k;
   int i = 0;
   k = *l;
   while (k && i < j)
     { k = k->tail;
       i++;
     }
   while (k)
     { if (eq)
         { if (eq(k->head, (void*)x))
	     { break; }
	 }
       else if (k->head == (void*)x)
	 { break; }
       k = k->tail;
       i++;
     }
   if (k)
     { *y = k->head;
       return i;
     }
   return -1;
 }

extern void *llist_find_extract(llist *l, llist_func *eq, const void *x)
 /* Same as llist_find, but also remove the element from l. */
 { llist *k, e;
   void *y;
   k = l;
   while (*k)
     { if (eq)
         { if (eq((*k)->head, (void*)x))
	     { break; }
	 }
       else if ((*k)->head == (void*)x)
	 { break; }
       k = &(*k)->tail;
     }
   if (*k)
     { y = (*k)->head;
       e = *k;
       *k = (*k)->tail;
       free(e);
       return y;
     }
   return 0;
 }

extern int llist_all_extract(llist *l, llist_func *eq, const void *x)
 /* Go through all elements y, and whenever eq(y, x) is not 0, remove
    the y from the list. (If !eq, use y == x instead.)
    Return is nr of elements removed.
 */
 { llist *k, e;
   int n = 0;
   k = l;
   while (*k)
     { if ((eq && eq((*k)->head, (void*)x))
	   || (!eq && (*k)->head == (void*)x))
         { e = *k;
	   *k = e->tail;
	   free(e);
	   n++;
	 }
       else
	 { k = &(*k)->tail; }
     }
   return n;
 }


/********** whole list operations ********************************************/

extern void llist_concat(llist *l, llist *m)
 /* concatenate m to l. This makes m empty. */
 { llist *k;
   k = l;
   while (*k)
     { k = &(*k)->tail; }
   *k = *m;
   *m = 0;
 }

extern llist llist_copy(const llist *l, llist_func_p *cp, void *info)
 /* return a copy of l.
    If cp, copy each element x by cp(x, info); otherwise, simply use y = x.
 */
 { llist k, e, m, *t;
   k = *l;
   m = 0;
   t = &m;
   while (k)
     { NEW(e);
       if (cp)
         { e->head = cp(k->head, info); }
       else
	 { e->head = k->head; }
       e->tail = 0;
       *t = e; t = &e->tail;
       k = k->tail;
     }
   return m;
 }

extern void llist_reverse(llist *l)
 /* reverse the order of elements */
 { llist k, e, m;
   m = 0;
   k = *l;
   while (k)
     { e = k;
       k = k->tail;
       e->tail = m;
       m = e;
     }
   *l = m;
 }

extern llist llist_cut(llist *l, int i, int len)
 /* remove l[i..i+len) from l, and return it.
    If i outside l, or len = 0, nothing happens and return is an
    empty list. If len = -1, or len is too large, l[i..) is cut.
 */
 { llist *k, *start, m;
   if (i < 0 || len == 0) return 0;
   k = l;
   while (*k && i != 0)
     { k = &(*k)->tail;
       i--;
     }
   if (!*k) return 0;
   m = *k;
   start = k;
   while (*k && len != 0)
     { k = &(*k)->tail;
       len--;
     }
   *start = *k;
   *k = 0;
   return m;
 }

/********** applying element operations **************************************/

extern int llist_apply(const llist *l, llist_func *f, void *info)
 /* In order, apply f(x, info) to each element x in l, as long as
    f returns zero.
    Return value is last value returned by f.
 */
 { llist k;
   int v = 0;
   k = *l;
   while (k && !v)
     { v = f(k->head, info);
       k = k->tail;
     }
   return v;
 }

extern int llist_apply_overwrite(llist *l, llist_func_p *f, void *info)
 /* In order, apply f(x, info) to each element x in l, and replace
    the element by the return value (always). If !f, then any occurrence of
    info in l is replaced by 0.
    Return value is number of elements that were actually changed (i.e.,
    that were overwritten with a new value).
 */
 { llist k;
   void *v = 0;
   int n = 0;
   k = *l;
   while (k)
     { if (f)
         { v = f(k->head, info); }
       else if (k->head == info)
	 { v = 0; }
       else
	 { v = k->head; }
       if (v != k->head)
	 { k->head = v;
	   n++;
	 }
       k = k->tail;
     }
   return n;
 }

extern llist llist_alias_idx(llist *l, int i)
 /* returns, as an alias, l[i..). Return is empty list if i is outside l */
 { llist k;
   k = *l;
   while (k && i > 0)
     { k = k->tail;
       i--;
     }
   return k;
 }


/********** sorting **********************************************************/

#if 0
/* llist_selection_sort works fine, but llist_merge_sort is much
   more efficient.
*/
static void llist_selection_sort(llist *l, llist_func *cmp)
 { llist *k, *min, e;
   while (*l)
     { min = l;
       k = &(*l)->tail;
       while (*k)
	 { if (cmp)
	     { if (cmp((*k)->head, (*min)->head) < 0)
	         { min = k; }
	     }
	   else if ((*k)->head < (*min)->head)
	     { min = k; }
	   k = &(*k)->tail;
	 }
       e = *min;
       *min = (*min)->tail;
       e->tail = *l;
       *l = e;
       l = &(*l)->tail;
     }
 }
#endif

static void llist_merge_sort(llist *l, llist_func *cmp, int n)
 /* Pre: n = llist_size(l) > 0 */
 { llist k, m, *p;
   int i, j;
   /* if (n < 2) return; not necessary: won't be called unless n >= 2 */
   i = n/2;
   k = *l;
   j = 1;
   while (j < i) /* inv: k->tail = l[j] */
     { k = k->tail;
       j++;
     }
   m = k->tail; /* m = l[i] */
   k->tail = 0;
   k = *l;
   if (i > 1) llist_merge_sort(&k, cmp, i);
   if (n-i > 1) llist_merge_sort(&m, cmp, n-i);
   p = l;
   while (k && m)
     { if ((cmp && cmp(k->head, m->head) <= 0) ||
	   (!cmp && k->head <= m->head))
         { *p = k;
	   k = k->tail;
	 }
       else
	 { *p = m;
	   m = m->tail;
	 }
       p = &(*p)->tail;
     }
   if (k)
     { *p = k; }
   else
     { *p = m; }
 }

extern void llist_sort(llist *l, llist_func *cmp)
 /* Sort l according to cmp(), smallest first, where cmp(x, y) returns
    <0 (x < y), 0 (x == y), or >0 (x > y). If !cmp, the pointer values
    themselves are used (the latter is useful if you need to check that
    each element is unique). The algorithm is merge sort.
 */
 { int n;
   n = llist_size(l);
   if (n > 1) llist_merge_sort(l, cmp, n);
 }

