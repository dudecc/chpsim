/*
llist.h: singly-linked list.

Copyright (c) 1999 Marcel R. van der Goot
All rights reserved.

This file is only for use as part of software written by
Marcel van der Goot. Any other use requires a separate license.
This file contains confidential information, and should not be
redistributed to third parties.

*/

#ifndef LLIST_H
#define LLIST_H

/********** types ************************************************************/

typedef struct llist_elem llist_elem;
typedef llist_elem *llist; /* empty if 0 */
struct llist_elem
   { void *head;
     llist tail;
   };

typedef int llist_func(void *x, void *info);
typedef void *llist_func_p(void *x, void *info);
 /* Used by some other functions below. x is element in a list,
    info an additional object to keep "global" information.
 */

/* llist_func: */
extern int llist_free_elem(void *x, void *dummy);
 /* Equivalent to free(x).
    Note: be sure that this free() matches the malloc() used to allocate
    the elements; see mallocdbg.h.
 */

/********** general operations ***********************************************/

 /*
extern void llist_init(llist *l);
    Initialize l to the empty list. Does not allocate memory.
 */

extern void llist_free(llist *l, llist_func *f, void *info);
 /* Free the list (this makes *l empty).
    If f, call f(x, info) for every x in the list. If you just want to
    free every x, pass llist_free_elem as f.
 */

extern int llist_size(const llist *l);
 /* nr of elements */

 /*
extern int llist_is_empty(const llist *l);
    true if *l is empty
 */


/********** indexing *********************************************************/
/* Valid positions are 0 .. llist_size(l)-1.
   In most cases, -1 indicates the end of the list.
*/

extern void *llist_idx(const llist *l, int i);
 /* Return element at position i.
    If i < 0 ==> return last element (if it exists).
    Otherwise, if position i does not exist, return 0.
 */

extern void *llist_idx_extract(llist *l, int i);
 /* Same as llist_idx, but also remove the element from l. */


/********** insertions *******************************************************/

extern void llist_insert(llist *l, int i, const void *x);
 /* Insert x at position i (starting at 0).
    If i < 0 or i = llist_size(l) ==> at end.
    Otherwise, if position i does not exist, nothing happens.
 */

extern void *llist_overwrite(llist *l, int i, const void *x);
 /* Replace element at position i by x, return the old element l[i].
    If i < 0 ==> replace last element (if it exists).
    Otherwise, if position i does not exist, nothing happens (return 0).
 */

 /*
extern void llist_prepend(llist *l, const void *x);
    Insert x at the head of l.


extern void llist_append(llist *l, const void *x);
    Insert x at the end of l.
 */

extern void llist_expand(llist *l, int i, llist *k);
 /* insert k in l at position i. I.e., l will get the value
    l[0..i) ++ k ++ l[i..). This will make k empty.
    If i < 0 or i = llist_size(l) ==> at end.
    Otherwise, if position i does not exist, nothing happens.
 */

/********** searching ********************************************************/

extern void *llist_find(const llist *l, llist_func *eq, const void *x);
 /* Look for first element y in the list such that eq(y, x) is not 0;
    return y if found, 0 otherwise.
    If !eq, then look for y == x.
 */

extern int llist_find_idx(const llist *l, llist_func *eq, const void *x);
 /* Look for first element y in the list such that eq(y, x) is not 0;
    return position of y if found, -1 (not 0!) otherwise.
    If !eq, then look for y == x.
 */

extern int llist_find_idx_next
 (const llist *l, int j, llist_func *eq, const void *x, void **y);
 /* Look for first element y in l[j..) such that eq(y, x) is not 0;
    If found, assign to *y and return position; if not found, return -1.
    If !eq, then look for y == x.
 */

extern void *llist_find_extract(llist *l, llist_func *eq, const void *x);
 /* Same as llist_find, but also remove the element from l. */

extern int llist_all_extract(llist *l, llist_func *eq, const void *x);
 /* Go through all elements y, and whenever eq(y, x) is not 0, remove
    the y from the list. (If !eq, use y == x instead.)
    Return is nr of elements removed.
 */


/********** whole list operations ********************************************/

 /*
extern void llist_transfer(llist *l, llist *m);
    Pre: l is empty or uninitialized.
    assign m to l. This makes m empty.
 */

extern void llist_concat(llist *l, llist *m);
 /* concatenate m to l. This makes m empty. */

extern llist llist_copy(const llist *l, llist_func_p *cp, void *info);
 /* return a copy of l.
    If cp, copy each element x by cp(x, info); otherwise, simply use y = x.
 */

extern void llist_reverse(llist *l);
 /* reverse the order of elements */

extern llist llist_cut(llist *l, int i, int len);
 /* remove l[i..i+len) from l, and return it.
    If i outside l, or len = 0, nothing happens and return is an
    empty list. If len < 0, or len is too large, the end of l is used.
 */

/********** applying element operations **************************************/

extern int llist_apply(const llist *l, llist_func *f, void *info);
 /* In order, apply f(x, info) to each element x in l, as long as
    f returns zero.
    Return value is last value returned by f.
 */

extern int llist_apply_overwrite(llist *l, llist_func_p *f, void *info);
 /* In order, apply f(x, info) to each element x in l, and replace
    the element by the return value (always). If !f, then any occurrence of
    info in l is replaced by 0.
    Return value is number of elements that were actually changed (i.e.,
    that were overwritten with a new value).
 */

 /*
extern void *llist_head(llist *l);
    Pre: *l is not empty.
    Returns the first element of the list


extern llist llist_alias_tail(llist *l);
    Pre: *l is not empty.
    Returns, as an alias, the tail of *l.
 */

/* To apply an operation to each element of a list *l, you can use
   llist_apply(l, ...). Alternatively, you can walk through the list
   with a loop as follows:

	llist m;
	m = *l;
	while (!llist_is_empty(&m))
	  { apply operation on llist_head(&m);
	    m = llist_alias_tail(&m);
	  }

   Note that you should not change list l itself while doing this,
   as m is pointing inside the list data structure. However, you may use
   llist_overwrite() on m or l. Adding or removing elements to/from m is
   risky; prepending to m or removing the head of m is an error.
*/

extern llist llist_alias_idx(llist *l, int i);
 /* returns, as an alias, l[i..). Return is empty list if i is outside l */

/********** sorting **********************************************************/

extern void llist_sort(llist *l, llist_func *cmp);
 /* Sort l according to cmp(), smallest first, where cmp(x, y) returns
    <0 (x < y), 0 (x == y), or >0 (x > y). If !cmp, the pointer values
    themselves are used (the latter is useful if you need to check that
    each element is unique). The algorithm is merge sort.
 */


/*****************************************************************************/

#define llist_init(L) (*(L) = 0)
#define llist_is_empty(L) (*(L) == 0)
#define llist_prepend(L, X) llist_insert((L), 0, (X))
#define llist_append(L, X) llist_insert((L), -1, (X))
#define llist_transfer(L, M) ((*(L) = *(M)), (*(M) = 0))
#define llist_head(L) ((*L)->head)
#define llist_alias_tail(L) ((*L)->tail)

#endif /* LLIST_H */
