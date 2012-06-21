/* properties.h: properties
 * 
 * COPYRIGHT 2010. California Institute of Technology
 * 
 * This file is part of chpsim.
 * 
 * Chpsim is free software: you can redistribute it and/or modify
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
 * along with chpsim.  If not, see <http://www.gnu.org/licenses/>.
 * 
 * Authors: Chris Moore
 */

#include "properties.h"
#include <hash.h>
#include <llist.h>
#include <string_table.h>

typedef struct pkey
   { const str *id;
     void *node;
   } pkey;

extern void init_property_info(property_info *f)
 /* Initialize the structures in f */
 { hash_table_init(&f->h, 1, HASH_fixed_len_key, 0);
   f->h.key_len = sizeof(pkey);
   llist_init(&f->pl);
 }

extern void declare_property(const str *id, long z, property_info *f)
/* Add the property named id to the list of declared properties.  The initial
 * value for the property is z.
 */
 { pkey p;
   hash_entry *e;
   llist_prepend(&f->pl, id);
   p.id = id; p.node = 0;
   if (hash_insert(&f->h, (char*)(&p), &e) && e->data.i != z)
     { fprintf(stderr, "Property %s declared with initializers %ld and %ld\n",
               id, e->data.i, z);
     }
   e->data.i = z;
 }

extern void add_property(const str *id, void *node, long x, property_info *f)
/* Add x to the value of the property for the given node. */
 { pkey p;
   hash_entry *e, *ze;
   p.id = id; p.node = node;
   if (!hash_insert(&f->h, (char*)(&p), &e))
     { p.node = 0;
       assert(ze = hash_find(&f->h, (char*)&p));
       e->data.i = ze->data.i;
     }
   if (((x > 0) && (e->data.i > LONG_MAX - x)) ||
       ((x < 0) && (e->data.i < LONG_MIN - x)))  /* Check for integer overflow */
     { fprintf(stderr, "Property %s overflowed adding %ld to %ld\n",
               id, x, e->data.i);
     }
   e->data.i += x;
 }

extern long get_property(const str *id, void *node, property_info *f)
/* Return value of the property for the given node. */
 { pkey p;
   hash_entry *e;
   p.id = id; p.node = node;
   e = hash_find(&f->h, (char*)&p);
   if (!e)
     { p.node = 0;
       e = hash_find(&f->h, (char*)&p);
       assert(e);
     }
   return e->data.i;
 }
