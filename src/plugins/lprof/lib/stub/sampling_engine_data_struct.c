/*
   Copyright (C) 2004 - 2018 Université de Versailles Saint-Quentin-en-Yvelines (UVSQ)

   This file is part of MAQAO.

  MAQAO is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License
   as published by the Free Software Foundation; either version 3
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <stdlib.h>
#include <libmcommon.h> // lc_malloc
#include "sampling_engine_data_struct.h"

/**************************************************************************************************
 *                                     buf_t: simple buffer                                       *
 **************************************************************************************************/
/* This buffer is composed of 1 chunk with fixed size (no realloc).
 * It will be used by lprof_queue_t and lprof_hashtable_t objects
 * TODO: improve this to allow extension, with multiple chunks (no need for realloc) */

// Creates a simple buffer, with given fixed size
buf_t *buf_new (size_t size)
{
   if (size == 0) return NULL;

   buf_t *buf = lc_malloc0 ((sizeof *buf) + size);
   buf->base = (void *) buf + sizeof *buf;
   buf->offset = 0;
   buf->size = size;

   return buf;
}

// Returns position in a simple buffer and post-increments it by 'size', use this as lc_malloc replacement
void *buf_add (buf_t *buf, size_t size)
{
   if (buf->offset + size > buf->size) {
      DBGMSG ("Cannot allocate %lu bytes in buffer %p: only %lu bytes left\n",
              size, buf, buf->size - buf->offset);
      return NULL;
   }

   void *p = buf->base + buf->offset;
   buf->offset += size;
   return p;
}

// Returns length (size in bytes) of a simple buffer
size_t buf_length (const buf_t *buf)
{
   return buf->size;
}

// Returns available size (in bytes) in a simple buffer
size_t buf_avail (const buf_t *buf)
{
   return buf->size - buf->offset;
}

// Resets position and data in a simple buffer, allow to reuse a buffer (instead of buf_new)
void buf_flush (buf_t *buf)
{
   buf->offset = 0;
   memset (buf->base, 0, buf->size);
}

// Frees memory related to a simple buffer
void buf_free (buf_t *buf)
{
   lc_free (buf);
}


/**************************************************************************************************
 *                                 lprof_queue_t: simple queue                                    *
 **************************************************************************************************/
/* This queue uses buf_add instead of lc_malloc to add elements.
 * It is a feature-limited adaptation of generic queue_t */

// Creates an empty simple queue, using given allocator
lprof_queue_t *lprof_queue_new (buf_t *buf)
{
   lprof_queue_t *q = buf_add (buf, sizeof *q);
   if (!q) {
      DBGMSG ("Cannot create lprof_queue: allocation failed from buffer %p\n", buf);
      return NULL;
   }

   q->buf = buf;
   q->head = NULL;
   q->tail = NULL;
   q->length = 0;

   return q;
}

// Returns length (number of elements) of a simple queue
unsigned int lprof_queue_length (const lprof_queue_t *queue)
{
   if (!queue) return 0;

   return queue->length;
}

static lprof_list_t *lprof_list_new (buf_t *buf, const void *data)
{
   lprof_list_t *list = buf_add (buf, sizeof *list);
   if (!list) {
      DBGMSG ("Cannot create lprof_list: allocation failed from buffer %p\n", buf);
      return NULL;
   }

   list->next = NULL;
   list->data = data;

   return list;
}

static lprof_list_t *lprof_list_add (buf_t *buf, lprof_list_t *list, const void *data) {
   lprof_list_t *new = lprof_list_new (buf, data);

   if (!list) return new;

   if (list->next) {
      new->next = list->next;
   }

   list->next = new;

   return list;
}

// Inserts new element in a simple queue, pointing to 'data'
void lprof_queue_add (lprof_queue_t *queue, const void *data)
{
   if (!queue) return;

   if (queue->length > 0)
      queue->tail = lprof_list_add (queue->buf, queue->tail, data)->next;
   else
      queue->head = queue->tail = lprof_list_new (queue->buf, data);

   queue->length++;
}


/**************************************************************************************************
 *                             lprof_hashtable_t: simple hashtable                                *
 **************************************************************************************************/
/* This hashtable uses buf_add instead of lc_malloc to add elements.
 * It is a feature-limited adaptation of generic hashtable_t */

// Creates an empty simple hashtable, using given allocator
lprof_hashtable_t *lprof_hashtable_new (buf_t *buf, lprof_hashtable_size_t size)
{
   lprof_hashtable_t *t = buf_add (buf, sizeof *t);
   if (!t) {
      DBGMSG ("Cannot create lprof_hashtable: allocation failed from buffer %p\n", buf);
      return NULL;
   }

   t->buf = buf;
   t->size = size;
   t->nodes = buf_add (buf, size * sizeof t->nodes[0]);
   t->nnodes = 0;

   return t;
}

static int lprof_hashtable_equal (uint64_t v1, uint64_t v2)
{
   return (v1 == v2);
}

static lprof_hashtable_size_t lprof_hashtable_hash (uint64_t key, lprof_hashtable_size_t size)
{
   // Performance critical: see hashtable_hash
   return ((lprof_hashtable_size_t) key) % size;
}

// Returns first element with a given key
void *lprof_hashtable_lookup (const lprof_hashtable_t *t, uint64_t key)
{
   if (t == NULL) return NULL;

   const lprof_hashtable_size_t slot = lprof_hashtable_hash (key, t->size);
   lprof_hashnode_t *n;
   for (n = t->nodes[slot]; n != NULL; n = n->next) {
      if (lprof_hashtable_equal (key, n->key))
         return n->data;
   }
   return NULL;
}

// Returns array of all elements with a given key
array_t *lprof_hashtable_lookup_all (const lprof_hashtable_t *t, uint64_t key)
{
   if (t == NULL) return NULL;

   array_t *a = NULL;
   const lprof_hashtable_size_t slot = lprof_hashtable_hash (key, t->size);
   lprof_hashnode_t *n;
   for (n = t->nodes[slot]; n != NULL; n = n->next) {
      if (lprof_hashtable_equal (key, n->key)) {
         if (a == NULL) a = array_new();
         array_add (a, n->data);
      }
   }
   return a;
}

// Inserts new element in a simple hashtable, pointing to 'data' and associated to 'key'
void lprof_hashtable_insert (lprof_hashtable_t *t, uint64_t key, const void *data)
{
   if (t == NULL) return;

   if (t->nnodes == LPROF_HASHTABLE_MAX_NNODES) {
      HLTMSG ("Cannot insert in already full hashtable (max nodes nb: %"PRIu32")\n", LPROF_HASHTABLE_MAX_NNODES);
   }

   const lprof_hashtable_size_t slot = lprof_hashtable_hash (key, t->size);
   lprof_hashnode_t **node = &(t->nodes [slot]);
   lprof_hashnode_t *new = buf_add (t->buf, sizeof *new);
   if (!new) {
      DBGMSG ("Cannot create lprof_hashnode: allocation failed from buffer %p\n", t->buf);
      return;
   }
   new->next = *node;
   new->data = (void *) data;
   new->key = key;
   *node = new;
   t->nnodes++;
}
