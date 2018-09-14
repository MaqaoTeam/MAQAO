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

#ifndef __SAMPLING_ENGINE_DATA_STRUCT_H__
#define __SAMPLING_ENGINE_DATA_STRUCT_H__

// TODO: move struct definitions to .c (make this private)
// TODO: think to move some data structures to MAQAO core

/**************************************************************************************************
 *                                     buf_t: simple buffer                                       *
 **************************************************************************************************/

typedef struct {
   void *base;
   size_t offset;
   size_t size;
} buf_t;

// Creates a simple buffer, with given fixed size
buf_t *buf_new (size_t size);

// Returns position in a simple buffer and post-increments it by 'size', use this as lc_malloc replacement
void *buf_add (buf_t *buf, size_t size);

// Returns length (size in bytes) of a simple buffer
size_t buf_length (const buf_t *buf);

// Returns available size (in bytes) in a simple buffer
size_t buf_avail (const buf_t *buf);

// Resets position and data in a simple buffer, allow to reuse a buffer (instead of buf_new)
void buf_flush (buf_t *buf);

// Frees memory related to a simple buffer
void buf_free (buf_t *buf);


// Internally used by lprof_queue and lprof_hashtable
typedef struct lprof_list_s {
   const void *data; /**<Pointer to the actual data contained in the list*/
   struct lprof_list_s *next; /**<Next list node*/
} lprof_list_t;


/**************************************************************************************************
 *                                 lprof_queue_t: simple queue                                    *
 **************************************************************************************************/

typedef struct {
   buf_t        *buf;
   lprof_list_t *head; /**<The first list node of the queue*/
   lprof_list_t *tail; /**<The last list node of the queue*/
   unsigned int length; /**<size of the queue*/
} lprof_queue_t;

// Creates an empty simple queue, using given allocator
lprof_queue_t *lprof_queue_new (buf_t *buf);

// Returns length (number of elements) of a simple queue
unsigned int lprof_queue_length (const lprof_queue_t *queue);

// Inserts new element in a simple queue, pointing to 'data'
void lprof_queue_add (lprof_queue_t *queue, const void *data);

// Iterates elements of a simple queue
#define FOREACH_IN_LPROF_QUEUE(X,Y)                     \
   lprof_list_t *Y;for(Y=X->head; Y!=NULL; Y=Y->next)


/**************************************************************************************************
 *                             lprof_hashtable_t: simple hashtable                                *
 **************************************************************************************************/

typedef uint32_t lprof_hashtable_nnodes_t;
#define LPROF_HASHTABLE_MAX_NNODES UINT32_MAX
typedef uint32_t lprof_hashtable_size_t;

// Internally used by lprof_hashtable_t
typedef struct lprof_hashnode_s {
   uint64_t key;
   void *data;
   struct lprof_hashnode_s *next;
} lprof_hashnode_t;

typedef struct {
   buf_t *buf;
   lprof_hashtable_nnodes_t nnodes; /**<Total number of nodes*/
   lprof_hashtable_size_t size; /**<Number of slots, size of the 'nodes' array*/
   lprof_hashnode_t** nodes; /**<Array containing the queues of nodes, indexed by they key's hash*/
} lprof_hashtable_t;

// Creates an empty simple hashtable, using given allocator
lprof_hashtable_t *lprof_hashtable_new (buf_t *buf, lprof_hashtable_size_t size);

// Returns first element with a given key
void *lprof_hashtable_lookup (const lprof_hashtable_t *t, uint64_t key);

// Returns array of all elements with a given key
array_t *lprof_hashtable_lookup_all (const lprof_hashtable_t *t, uint64_t key);

// Inserts new element in a simple hashtable, pointing to 'data' and associated to 'key'
void lprof_hashtable_insert (lprof_hashtable_t *t, uint64_t key, const void *data);

// Iterates elements (key-value pairs) of a simple hashtable
#define FOREACH_IN_LPROF_HASHTABLE(X,Y)                 \
   lprof_hashnode_t *Y; lprof_hashtable_size_t __i;     \
   for (__i = 0; __i < X->size; __i++)                  \
      for (Y = X->nodes[__i]; Y != NULL; Y = Y->next)

#endif // __SAMPLING_ENGINE_DATA_STRUCT_H__
