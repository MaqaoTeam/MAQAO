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

/**
 * \file This file defines functions related to hash tables.
 * Present (2018/08/29) implementation is not optimized for space. If need so, consider open addressing
 * or dedicated allocator. With dedicated allocator, key-value pairs are no more allocated using lc_malloc
 * but using another function returning an offset in a big buffer allocated by a single lc_malloc.
 * */
#include "libmcommon.h"

///////////////////////////////////////////////////////////////////////////////
//                         hashtable functions                               //
///////////////////////////////////////////////////////////////////////////////

int direct_equal (const void *v1, const void *v2)
{
   return (v1 == v2);
}

hashtable_size_t direct_hash (const void *v, hashtable_size_t size)
{
   /* x86 integer divide:
    *  - unsigned (DIV instruction) faster than signed (IDIV)
    *  - 32-bit much faster than 64-bit
    *  - => use uint32_t for both operands
    *  - probably same properties for many other architectures */
#ifdef UINTPTR_MAX
   return ((hashtable_size_t)(uintptr_t) v) % size;
#else
   return ((hashtable_size_t)(size_t) v) % size;
#endif
}

int str_equal (const void *v1, const void *v2)
{
   if (!v1 || !v2)
      return (v1 == v2);

   return !strcmp (v1, v2);
}

hashtable_size_t str_hash (const void *v, hashtable_size_t size)
{
   /* TODO: what if uint64_t h ? overflow will come later (and hash results will change)
    * ... but really useful ? */
   unsigned int h = 1;
   const char *c = v;
   assert (c != NULL);
   while (c != NULL && *c != '\0') {
      h = h * 263 + *c;
      c++;
   }

   return (h % size);
}

/*
 * An equalfunc_t function comparing keys as pointers to 64 bits integers
 * \param v1 First key, pointer to an int64_t
 * \param v2 Second key, pointer to an int64_t
 * \return 1 if the keys are equal, 0 otherwise
 * */
int int64p_equal (const void *v1, const void *v2)
{
   if (!v1 || !v2)
      return (v1 == v2); //Fallback in case either one is NULL: comparing the pointers

   return (*(int64_t*) v1 == *(int64_t*) v2);
}

/*
 * A hashfunc_t function hashing a pointer to a 64 bits integer
 * \param v Key of the element, interpreted as a pointer to an int64_t
 * \param size Number of slots in the targeted hashtable
 * \return Hash of the key (a value between 0 and size-1)
 * */
hashtable_size_t int64p_hash (const void *v, hashtable_size_t size)
{
   if (!v) return 0;

   return (*(int64_t*) v) % size;
}

/*
 * An equalfunc_t function comparing keys as pointers to 32 bits integers
 * \param v1 First key, pointer to an int32_t
 * \param v2 Second key, pointer to an int32_t
 * \return 1 if the keys are equal, 0 otherwise
 * */
int int32p_equal (const void *v1, const void *v2)
{
   if (!v1 || !v2)
      return (v1 == v2); //Fallback in case either one is NULL: comparing the pointers

   return (*(int32_t*) v1 == *(int32_t*) v2);
}

/*
 * A hashfunc_t function hashing a pointer to a 32 bits integer
 * \param v Key of the element, interpreted as a pointer to an int32_t
 * \param size Number of slots in the targeted hashtable
 * \return Hash of the key (a value between 0 and size-1)
 * */
hashtable_size_t int32p_hash (const void *v, hashtable_size_t size)
{
   if (!v) return 0;

   return (*(int32_t*) v) % size;
}

/*
 * creates a new hashtable and initializes it with the values given in parameters
 * */
hashtable_t *hashtable_new_with_custom_size (hashfunc_t hf, equalfunc_t ef,
                                             hashtable_size_t size, boolean_t fixed_size)
{
   hashtable_t *t = lc_malloc (sizeof *t);

   if (size == 0) size = HASH_INIT_SIZE;

   t->size       = size;
   t->fixed_size = fixed_size;
   t->nodes      = lc_malloc0 (size * sizeof t->nodes[0]);
   t->nnodes     = 0;
   t->hash_func  = hf;
   t->key_equal_func = ef;

   return t;
}

/*
 * creates a new hashtable and initializes it with the values given in parameters
 * */
hashtable_t *hashtable_new (hashfunc_t hf, equalfunc_t ef)
{
   return hashtable_new_with_custom_size (hf, ef, HASH_INIT_SIZE, FALSE);
}

/*
 * Remove the first element found for the given key (but does not free it)
 * \param t a hashtable
 * \param key key of the element to return
 * \return an element if found, else NULL
 */
void *hashtable_remove (hashtable_t *t, const void *key)
{
   if (t == NULL) return NULL;

   hashnode_t *n, *prev = NULL;
   const hashtable_size_t slot = t->hash_func (key, t->size);
   for (n = t->nodes[slot]; n != NULL; prev = n, n = n->next) {
      if (t->key_equal_func (key, n->key)) {
         void *ret = n->data;
         if (prev == NULL)
            t->nodes[slot] = n->next;
         else
            prev->next = n->next;
         t->nnodes--;
         lc_free(n);
         return ret;
      }
   }

   return NULL;
}

/*
 * Removes one given element from a hashtable
 * \param t The hashtable
 * \param key The key of the element to remove
 * \param data The element to remove
 * \return 0 if no element was found, 1 if one was found and removed
 * \warning The element is not freed, this is up the the caller function
 * */
int hashtable_remove_elt (hashtable_t *t, const void *key, const void *data)
{
   if (t == NULL) return 0;

   hashnode_t *n, *prev = NULL;
   const hashtable_size_t slot = t->hash_func (key, t->size);
   for (n = t->nodes[slot]; n != NULL; prev = n, n = n->next) {
      if (t->key_equal_func (key, n->key) && (n->data == data)) {
         if (prev == NULL)
            t->nodes[slot] = n->next;
         else
            prev->next = n->next;
         t->nnodes--;
         lc_free(n);
         return 1;
      }
   }

   return 0;
}

/*
 * Tries to find a given element in a hashtable
 * \param t The hashtable
 * \param key The key of the element to look up
 * \param data The element to look up
 * \return TRUE if the element was found, FALSE otherwise
 * */
boolean_t hashtable_lookup_elt (const hashtable_t *t, const void *key, const void *data)
{
   if (t == NULL) return FALSE;

   FOREACH_AT_HASHTABLE_KEY (t, key, n) {
      if (n->data == data) return TRUE;
   }

   return FALSE;
}

/*
 * Empties the hashtable, but does not free it
 * \param t The hashtable
 * \param f Function for freeing a node's data
 * \param fk Function for freeing a node's key
 * */
void hashtable_flush (hashtable_t *t, void (*f)(void*), void (*fk)(void*))
{
   if (t == NULL) return;

   hashtable_size_t i;
   for (i = 0; i < t->size; i++) {
      hashnode_t *n = t->nodes[i];

      while (n != NULL) {
         hashnode_t *next = n->next;

         if (f != NULL)
            f(n->data);
         if (fk != NULL)
            fk(n->key);
         lc_free(n);

         n = next;
      }

      t->nodes[i] = NULL;
   }

   t->nnodes = 0;
}

/*
 * Empties the hashtable and frees it
 * \param t The hashtable
 * \param f Function for freeing a node's data
 * \param fk Function for freeing a node's key
 * */
void hashtable_free (hashtable_t *t, void (*f)(void*), void (*fk)(void*))
{
   if (t == NULL) return;

   hashtable_flush (t, f, fk);
   lc_free(t->nodes);
   lc_free(t);
}

static hashnode_t *hashnode_new()
{
   hashnode_t *n = lc_malloc (sizeof *n);
   n->next = NULL;
   n->key  = NULL;
   n->data = NULL;

   return n;
}

/* Useful for crash or performance debugging */
void hashtable_print (const hashtable_t *t, int verbose_lvl)
{
   if (t == NULL) {
      printf ("Cannot print hashtable at NULL pointer\n");
      return;
   }

   if (verbose_lvl < 1 || verbose_lvl > 4) {
      fprintf (stderr,
               "hashtable_print: invalid verbose level (got %d, expected 1-4)\n",
               verbose_lvl);
      return;
   }

   printf ("Printing the hashtable %p\n", t);
   printf ("nnodes: %"PRIu32"\n", t->nnodes);
   printf ("size  : %"PRIu32"\n", t->size);
   printf ("load factor: %.2f\n", (float) t->nnodes / t->size);

   if (verbose_lvl == 1)
      return;

   hashtable_size_t i;
   hashtable_nnodes_t nnodes_min, nnodes_max = 0; // Irrelevant compiler warning if nnodes_max not set...
   hashtable_size_t slot_idx_min, slot_idx_max;

   for (i = 0; i < t->size; i++) {
      hashnode_t *hn;
      hashtable_nnodes_t nnodes = 0;
      for (hn = t->nodes[i]; hn != NULL; hn = hn->next)
         nnodes++;

      if (i == 0) {
         nnodes_min = nnodes;
         slot_idx_min = 0;
         nnodes_max = nnodes;
         slot_idx_max = 0;
      } else {
         if (nnodes > nnodes_max) {
            nnodes_max = nnodes;
            slot_idx_max = i;
         }
         if (nnodes < nnodes_min) {
            nnodes_min = nnodes;
            slot_idx_min = i;
         }
      }
   }

   printf ("min nnodes: %"PRIu32" in slot %"PRIu32"\n", nnodes_min, slot_idx_min);
   printf ("max nnodes: %"PRIu32" in slot %"PRIu32"\n", nnodes_max, slot_idx_max);

   if (verbose_lvl == 2)
      return;

   for (i = 0; i < t->size; i++) {
      printf ("%"PRIu32": ", i);
      hashnode_t *hn;

      if (verbose_lvl == 3) {
         for (hn = t->nodes[i]; hn != NULL; hn = hn->next)
            putchar ('.');
      } else
         for (hn = t->nodes[i]; hn != NULL; hn = hn->next)
            printf ("(%p,%p) ", hn->data, hn->key);

      putchar ('\n');
   }
}

/** Primality test */
static boolean_t is_prime (uint64_t n)
{
   /* An even number is not prime */
   if (n % 2 == 0)
      return FALSE;

   /* A number dividable by another number other than 1 and itself is not prime */
   /* Can be made faster by using a sieve of Eratosthenes */
   unsigned i;
   for (i = 3; i <= sqrt(n); i += 2)
      if (n % i == 0)
         return FALSE;

   return TRUE;
}

/** Returns the prime number which is the closest from a given integer */
static uint64_t get_nearest_prime (uint64_t n)
{
   if (n % 2 == 0)
      n++;

   if (is_prime(n))
      return n;

   unsigned i;
   for (i = 2; i < 50; i += 2) {
      if (is_prime(n + i))
         return n + i;
      if (is_prime(n - i))
         return n - i;
   }

   /* If found no close prime number, return n */
   return n;
}

static void hashtable_resize (hashtable_t *t)
{
   if (t == NULL) return;

   /* Find the nearest prime number */
   uint64_t new_size = get_nearest_prime (t->nnodes); /* load factor will go back to 1 */
   if (new_size > HASHTABLE_MAX_SIZE) {
      DBGMSG ("Cannot increase hashtable size beyond %"PRIu32"\n", HASHTABLE_MAX_SIZE);
      return;
   }

   /* Allocate a new 'nodes' array with the new size */
   hashnode_t **new_nodes = lc_malloc0 (new_size * sizeof t->nodes[0]);

   /* Save nodes */
   hashnode_t **saved_nodes = lc_malloc (t->nnodes * sizeof saved_nodes[0]);

   hashnode_t **p = saved_nodes;
   FOREACH_INHASHTABLE(t, iter) {
      *p++ = iter;
   }

   /* From here, we are sure to be able to complete: we got enough memory */

   lc_free (t->nodes);
   t->nodes = new_nodes;
   t->size = new_size;

   /* Copy nodes into the new 'node' array */
   for (p = saved_nodes; p < &saved_nodes[t->nnodes]; p++) {
      const hashtable_size_t slot = t->hash_func ((*p)->key, t->size);
      hashnode_t **hn = &(t->nodes [slot]);

      (*p)->next = *hn;
      *hn = *p;
   }

   /* Now nodes are inserted into the new hashtable, we can free copies */
   lc_free (saved_nodes);
}

/* allocate a new token */
void hashtable_insert (hashtable_t *t, void *key, void *data)
{
   if (t == NULL) return;

   if (t->nnodes == HASHTABLE_MAX_NNODES) {
      HLTMSG ("Cannot insert in already full hashtable (max nodes nb: %"PRIu32")\n", HASHTABLE_MAX_NNODES);
   }

   if ((t->fixed_size == FALSE)
       && ((float) t->nnodes / t->size >= HASH_MAX_LOAD_FACTOR)) {
      hashtable_resize (t);
   }

   const hashtable_size_t slot = t->hash_func (key, t->size);
   hashnode_t **head_node = &(t->nodes [slot]);
   hashnode_t *new_node = hashnode_new();
   if (*head_node)
      new_node->next = *head_node;
   *head_node = new_node;
   new_node->data = data;
   new_node->key  = key;
   t->nnodes++;
}

/*
 * Executes a given function accepting 3 parameters for every element of the hashtable
 * \param f The function to execute. Its first argument must be the key of the hashtable element,
 * its second must be the element itself, and the last one is user defined
 * \param user Will be used for the 3rd parameter of f when calling it
 * */
void hashtable_foreach (const hashtable_t *t, void (*f)(void *, void *, void *),
                        void *user)
{
   if (t == NULL || f == NULL) return;

   hashtable_size_t i;
   for (i = 0; i < t->size; i++) {
      hashnode_t *n;
      for (n = t->nodes[i]; n != NULL; n = n->next) {
         f (n->key, n->data, user);
      }
   }
}

/*
 * Returns the number of elements present in the hashtable
 * \param t hashtable
 * \return number of elements
 * */
hashtable_nnodes_t hashtable_size (const hashtable_t *t)
{
   return t ? t->nnodes : 0;
}

/*
 * Returns the number of slots in the hashtable array
 * \param h hashtable
 * \return number of slots
 * */
hashtable_size_t hashtable_t_size (const hashtable_t *t)
{
   return t ? t->size : 0;
}

/*
 * Finds a token in the hastable
 * \param key Key associated to the token to look for
 * \return The value associated to the token
 * */
void *hashtable_lookup (const hashtable_t *t, const void *key)
{
   if (t == NULL) return NULL;

   FOREACH_AT_HASHTABLE_KEY (t, key, n) {
      return n->data;
   }

   return NULL;
}

/*
 * Retrieves all the values in an hashtable associated to a key
 * \return A queue containing all the values associated to this key or NULL if no match
 * */
queue_t *hashtable_lookup_all (const hashtable_t *t, const void *key)
{
   if (t == NULL) return NULL;

   queue_t* q = NULL;
   FOREACH_AT_HASHTABLE_KEY (t, key, n) {
      if (q == NULL) q = queue_new();
      queue_add_tail (q, n->data);
   }

   return q;
}

/*
 * Retrieves all the values in an hashtable associated to a key
 * \return A dynamic array containing all the values associated to this key or NULL if no match
 * */
array_t* hashtable_lookup_all_array(const hashtable_t* t, const void *key)
{
   if (t == NULL) return NULL;

   array_t* a = NULL;
   FOREACH_AT_HASHTABLE_KEY (t, key, n) {
      if (a == NULL) a = array_new();
      array_add (a, n->data);
   }

   return a;
}

/*
 * Copies (key,data) pairs from a source table into a destination table
 * \param dst destination table
 * \param src source table
 * */
void hashtable_copy (hashtable_t *dst, const hashtable_t *src)
{
   if (!dst || !src) return;

   FOREACH_INHASHTABLE (src, node_iter) {
      void *key  = GET_KEY (key, node_iter);
      void *data = GET_DATA (node_iter);
      hashtable_insert (dst, key, data);
   }
}
