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

extern void pqueue_init(pqueue *q, pqueue_func *cmp)
 { q->size = 0;
   q->root = 0;
   q->free = 0;
   q->cmp = cmp;
   q->flags = PQUEUE_use_freelist;
 }

static pqueue_node *new_pqueue_node(pqueue *q)
 { pqueue_node *p;
   if (!q->free || !IS_SET(q->flags, PQUEUE_use_freelist))
     { NEW(p); }
   else
     { p = q->free;
       q->free = p->left;
     }
   return p;
 }

static void free_pqueue_node(pqueue *q, pqueue_node *p)
 /* Only really frees if PQUEUE_use_freelist is not set */
 { if (IS_SET(q->flags, PQUEUE_use_freelist))
     { p->left = q->free;
       q->free = p;
     }
   else
     { free(p); }
 }

static pqueue_node *pqueue_delete_node(pqueue *q, pqueue_node *p)
/* Remove p from q and return the node that takes p's place */
 { pqueue_node *t;
   if (p->left == 0)
     { t = p->right; }
   else if (p->right == 0)
     { t = p->left; }
   else if (((*(q->cmp))(p->left->data, p->right->data)) > 0)
     { t = p->left; /* p->left will take p's place */
       t->left = pqueue_delete_node(q, t); 
       if (t->left) t->left->parent = t;
       t->left_size = p->left_size - 1;
       t->right = p->right;
       t->right->parent = t;
       t->right_size = p->right_size;
     }
   else
     { t = p->right; /* p->right will take p's place */
       t->right = pqueue_delete_node(q, t); 
       if (t->right) t->right->parent = t;
       t->right_size = p->right_size - 1;
       t->left = p->left;
       t->left->parent = t;
       t->left_size = p->left_size;
     }
   if (t) t->parent = p->parent;
   return t;
 }

extern void *pqueue_extract(pqueue *q)
 { void *r;
   pqueue_node *p;
   if (!q->root) return 0;
   r = q->root->data;
   p = q->root;
   q->root = pqueue_delete_node(q, p);
   free_pqueue_node(q, p);
   q->size--;
   return r;
 }

extern void pqueue_remove(pqueue *q, pqueue_node *p)
 { pqueue_node *parent;
   if (p == q->root)
     { pqueue_extract(q); }
   else
     { parent = p->parent;
       if (parent->left == p)
         { parent->left = pqueue_delete_node(q, p);
           parent->left_size--;
         }
       else if (parent->right == p)
         { parent->right = pqueue_delete_node(q, p);
           parent->right_size--;
         }
       else
         { assert(!"Error: removing a dangling Pqueuenode"); }
       free_pqueue_node(q, p);
     }
 }

extern void *pqueue_root(pqueue *q)
 { return q->root? q->root->data : 0; }

static pqueue_node *pqueue_insert_node
(pqueue *q, pqueue_node *new, pqueue_node *old, pqueue_node *parent)
 { int cmp;
   if (!old)
     { new->parent = parent;
       new->left = new->right = 0;
       new->left_size = new->right_size = 0;
       return new;
     }
   cmp = (*(q->cmp))(new->data, old->data);
   if (cmp > 0 || (cmp == 0 && IS_SET(q->flags, PQUEUE_is_stack)))
     { /*return new as root of sub-tree */
       if (old->left_size > old->right_size)
         { /*put old in right sub-tree */
           new->left = old->left;
           if (old->left) old->left->parent = new;
           new->left_size = old->left_size;
           new->right = old;
           old->parent = new;
           new->right_size = old->right_size + 1;
           old->left = 0;
           old->left_size = 0;
         }
       else
         { /*put old in left sub-tree */
           new->right = old->right;
           if (old->right) old->right->parent = new;
           new->right_size = old->right_size;
           new->left = old;
           old->parent = new;
           new->left_size = old->left_size + 1;
           old->right = 0;
           old->right_size = 0;
         } 
       new->parent = parent;
       return new;
     }
   if (old->left_size > old->right_size)
     { old->right = pqueue_insert_node(q, new, old->right, old);
       old->right_size++;
     }
   else
     { old->left = pqueue_insert_node(q, new, old->left, old);
       old->left_size++;
     }
   return old;
 }

pqueue_node *pqueue_insert(pqueue *q, void *x)
 { pqueue_node *p;
   q->size++;
   p = new_pqueue_node(q);
   p->data = x;
   RESET_FLAG(q->flags, PQUEUE_is_stack);
   q->root = pqueue_insert_node(q, p, q->root, 0);
   return p;
 }

pqueue_node *pqueue_insert_head(pqueue *q, void *x)
 { pqueue_node *p;
   q->size++;
   p = new_pqueue_node(q);
   p->data = x;
   SET_FLAG(q->flags, PQUEUE_is_stack);
   q->root = pqueue_insert_node(q, p, q->root, 0);
   return p;
 }

static void pqueue_free_nodes(pqueue_node *p)
 /* Really free, not just add to freelist */
 { if (!p) return;
   pqueue_free_nodes(p->left);
   pqueue_free_nodes(p->right);
   free(p);
 }

extern void pqueue_term(pqueue *q)
 { pqueue_node *p, *pnext;
   pqueue_free_nodes(q->root);
   p = q->free;
   while (p)
     { pnext = p->left;
       free(p);
       p = pnext;
     }
 }

static int _pqueue_apply(pqueue_node *p, pqueue_func *f, void *info)
 { int ret;
   if (!p) return 0;
   ret = (*f)(p->data, info);
   if (ret) return ret;
   ret = _pqueue_apply(p->left, f, info);
   if (ret) return ret;
   return _pqueue_apply(p->right, f, info);
 }

extern int pqueue_apply(pqueue *q, pqueue_func *f, void *info)
 { return _pqueue_apply(q->root, f, info); }

static void *_pqueue_find(pqueue_node *p, pqueue_func *f, void *info)
 { void *ret;
   if (!p) return 0;
   if ((*f)(p->data, info)) return p->data;
   ret = _pqueue_find(p->left, f, info);
   if (ret) return ret;
   return _pqueue_find(p->right, f, info);
 }

extern void *pqueue_find(pqueue *q, pqueue_func *f, void *info)
 { return _pqueue_find(q->root, f, info); }




