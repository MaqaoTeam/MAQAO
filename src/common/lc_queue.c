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
 * \file
 * */
#include "libmcommon.h"

///////////////////////////////////////////////////////////////////////////////
//                              queue functions                              //
///////////////////////////////////////////////////////////////////////////////

/*
 * Creates a new, empty queue
 * \return A queue of size 0, whose head and tail are NULL
 */
queue_t *queue_new()
{
   queue_t *new = lc_malloc(sizeof *new);

   new->head = NULL;
   new->tail = NULL;
   new->length = 0;
   DBGMSG("New queue %p created\n", new);
   return new;
}

/*
 * Adds an element at the head of a queue
 * \param queue a queue
 * \param data an element to add
 */
void queue_add_head(queue_t* queue, void* data)
{
   if (!queue)
      return;

   queue->head = list_add_before(queue->head, data);

   if (queue->tail == NULL)
      queue->tail = queue->head;

   queue->length++;
   DBGMSGLVL(1, "Added data %p to queue %p in element %p (now %d elements)\n",
         data, queue, queue->head, queue->length);
}

/*
 * Adds a new node at the end of a queue
 * \param queue The queue to update
 * \param data The data of the new element to add
 * */
void queue_add_tail(queue_t* queue, void *data)
{
   if (!queue)
      return;

   if (queue->length > 0)
      queue->tail = list_add_after(queue->tail, data)->next;
   else
      queue->head = queue->tail = list_new(data);

   queue->length++;
   DBGMSGLVL(1, "Added data %p to queue %p in element %p (now %d elements)\n",
         data, queue, queue->tail, queue->length);
}

/*
 * Removes and returns the head of a queue (does not free the element)
 * \param queue a queue
 * \return the element at the head of a queue
 */
void* queue_remove_head(queue_t *queue)
{
   if (!queue || queue->length <= 0)
      return NULL;

   void* data = list_remove_head(&(queue->head));
   queue->length--;

   if (queue->length == 0)
      queue->tail = NULL;

   return data;
}

/*
 * Removes and returns the tail of a queue (does not free the element)
 * \param queue a queue
 * \return the element at the tail of a queue
 */
void* queue_remove_tail(queue_t* queue)
{
   if (!queue || !(queue->tail) || queue->length <= 0)
      return NULL;

   list_t *node = queue->tail;
   void *data = node->data;

   queue->tail = node->prev;

   if (queue->tail)
      queue->tail->next = NULL;
   else
      queue->head = NULL;

   lc_free(node);
   queue->length--;

   return data;
}

/*
 * Finds and removes the first element in a queue whose data value equals the one looked for,
 * and frees it if \e f is not null
 * \param queue a queue
 * \param data an element to remove
 * \param f a function used to free an element, with the following prototype:
 *      void < my_function > (void* data); where:
 *                      - data is the element to free
 */
void queue_remove(queue_t* queue, void* data, void (*f)(void*))
{
   if (!queue || queue_is_empty(queue))
      return;

   if (queue->head->data == data) {
      queue_remove_head(queue);
      if (f)
         f(data);
      return;
   }

   if (queue->tail->data == data) {
      queue_remove_tail(queue);
      if (f)
         f(data);
      return;
   }

   list_t *found = list_lookup(queue->head, data);

   if (!found)
      return;

   list_remove_elt(found);
   if (f)
      f(data);

   queue->length--;

   return;
}

/*
 * Removes a given list element from a queue
 * \param queue The queue
 * \param elt A node of the queue. It must belong to the queue (no test will be performed)
 * \return The data contained by the removed element
 * */
void* queue_remove_elt(queue_t* queue, list_t* elt)
{
   if (!queue || queue->length <= 0 || !elt)
      return NULL;

   DBGMSGLVL(1,
         "Removing element %p from queue %p (%d elements before removal)\n",
         elt, queue, queue->length);

   if (elt == queue->head)
      return queue_remove_head(queue);
   if (elt == queue->tail)
      return queue_remove_tail(queue);

   queue->length--;

   return list_remove_elt(elt);
}

/*
 * Returns the length of a queue
 * \param queue a queue
 * \return \e queue length, or 0 if queue is NULL
 */
int queue_length(queue_t *queue)
{
   return ((queue != NULL) ? queue->length : 0);
}

/*
 * Checks if a queue is empty
 * \param queue a queue
 * \return 1 if \e queue is empty or NULL, else 0
 */
int queue_is_empty(queue_t *queue)
{
   return ((queue == NULL) || (!queue->length && queue->head == NULL));
}

/*
 * Returns the data contained by the first node in the queue
 * \param queue a queue
 * \return the first element of a queue, else NULL
 */
void* queue_peek_head(queue_t *queue)
{
   return ((queue && queue->length && queue->head) ? queue->head->data : NULL);
}

/*
 * Returns the data contained by the last node in the queue
 * \param queue a queue
 * \return the last element of a queue, else NULL
 */
extern void *queue_peek_tail(queue_t *queue)
{
   return ((queue && queue->length && queue->tail) ? queue->tail->data : NULL);
}

/*
 * Returns an iterator on the first element of the queue
 * \param queue a queue
 * \return an iterator (list_t pointeur)
 */
list_t *queue_iterator(queue_t *queue)
{
   return ((queue != NULL) ? queue->head : NULL);
}

/*
 * Returns an iterator for the reverse iteration of the queue (its tail)
 * \param queue a queue
 * \return an iterator (list_t pointeur)
 */
list_t* queue_iterator_rev(queue_t* queue)
{
   return ((queue != NULL) ? queue->tail : NULL);
}

/**
 * Appends queue q2 to queue q1. Neither is freed
 * \param q1 a queue
 * \param q2 a queue to add to q1
 */
static void _append_queue(queue_t* q1, queue_t* q2)
{
   assert(q1 && q2 && !queue_is_empty(q2));
   if (q1->head == NULL)
      q1->head = q2->head;

   if (q1->tail)
      q1->tail->next = q2->head;
   if (q2->head)
      q2->head->prev = q1->tail;

   q1->tail = q2->tail;
   q1->length += q2->length;
}

/*
 * Appends queue q2 to queue q1 and frees q2
 * \param q1 a queue
 * \param q2 a queue to add to q1. It is deleted at the end of the function
 */
void queue_append(queue_t* q1, queue_t* q2)
{
   if (!q1 || !q2 || queue_is_empty(q2))
      return;

   _append_queue(q1, q2);
   lc_free (q2);
}

/*
 * Appends queue q2 to queue q1 but does not free any queue
 * \param q1 a queue
 * \param q2 a queue to add to q1
 */
void queue_append_and_keep(queue_t* q1, queue_t* q2)
{
   if (!q1 || !q2 || queue_is_empty(q2))
      return;

   _append_queue(q1, q2);
}

/**
 * Prepends queue q2 to queue q1. Neither queue is freed
 * \param q1 a queue
 * \param q2 a queue to add before q1
 */
static void _prepend_queue(queue_t* q1, queue_t* q2)
{
   assert(q1 && q2 && !queue_is_empty(q2));
   if (q1->tail == NULL)
      q1->tail = q2->tail;

   if (q1->head)
      q1->head->prev = q2->tail;
   if (q2->tail)
      q2->tail->next = q1->head;

   q1->head = q2->head;
   q1->length += q2->length;
}

/*
 * Prepends queue q2 to queue q1 and frees it
 * \param q1 a queue
 * \param q2 a queue to add before q1. It is deleted at the end of the function
 */
void queue_prepend(queue_t* q1, queue_t* q2)
{
   if (!q1 || !q2 || queue_is_empty(q2))
      return;

   _prepend_queue(q1, q2);
   lc_free (q2);
}

/*
 * Prepends queue q2 to queue q1 but does not free any queue
 * \param q1 a queue
 * \param q2 a queue to add before q1
 */
void queue_prepend_and_keep(queue_t* q1, queue_t* q2)
{
   if (!q1 || !q2 || queue_is_empty(q2))
      return;

   _prepend_queue(q1, q2);
}

/*
 * Appends a node to a queue
 * \param queue The queue
 * \param n The node to append (will become the new tail of queue). Nothing will be done if n is NULL
 * */
void queue_append_node(queue_t* queue, list_t* node)
{
   if (!queue || !node)
      return;

   node->prev = queue->tail;
   node->next = NULL;

   if (queue->tail != NULL)
      queue->tail->next = node;

   if (queue->head == NULL)
      queue->head = node;

   queue->tail = node;
   queue->length++;
}

/*
 * Duplicates a queue
 * \param queue The queue to duplicate
 * \return A new queue structure holding the same data pointers in the same order
 * */
queue_t* queue_dup(queue_t* queue)
{
   if (!queue)
      return NULL;

   queue_t* dup = queue_new();

   dup->head = list_dup(queue->head);
   dup->tail = dup->head;

   if (dup->tail != NULL) {
      while (dup->tail->next != NULL)
         dup->tail = dup->tail->next;
   }

   dup->length = queue->length;

   return dup;
}

/*
 * Frees all elements in a queue and sets its length to 0
 * \param queue a queue
 * \param f a function used to free an element, with the following prototype:
 *      void < my_function > (void* data); where:
 *                      - data is the element to free
 */
void queue_flush(queue_t *queue, void (*f)(void*))
{
   if (!queue) return;

   list_free(queue->head, f);

   queue->head = NULL;
   queue->tail = NULL;
   queue->length = 0;
}

/*
 * Frees a queue and its elements if \e f is not null
 * \param queue a queue
 * \param f a function used to free an element, with the following prototype:
 *      void < my_function > (void* data); where:
 *                      - data is the element to free
 */
void queue_free(queue_t *queue, void (*f)(void*))
{
   DBGMSG("Freeing queue %p (%d elements)\n", queue, queue_length(queue));
   if (!queue) return;

   list_free(queue->head, f);
   lc_free(queue);
}

/*
 * Runs a function on every element in a queue
 * \param queue a queue
 * \param f a function exectued of each element. It must have the following prototype:
 *              void < my_function > (void* elemen, void* user); where:
 *                      - elem is the element
 *                      - user a 2nd parameter given by the user
 * \param user a parameter given by the user for the function to execute
 * */
void queue_foreach(queue_t *queue, void (*f)(void *, void *), void *user)
{
   if (!queue || queue->length <= 0)
      return;

   list_foreach(queue->head, f, user);
}

/*
 * Extract part of a queue following the given item (including or not this item)
 * \param queue The queue to extract a part from
 * \param elt The list element after which extraction must take place
 * \param include 1 if \e elt must be included to the returned queue, 0 otherwise
 * \return The extracted queue
 * */
queue_t* queue_extract_after(queue_t* queue, list_t* elt, int include)
{
   if (queue == NULL)
      return NULL;

   queue_t* output;
   list_t* iter;
   int newlength;

   output = queue_new();

   iter = queue->head;
   newlength = queue->length;

   while ((iter != elt) && (iter != NULL)) {
      iter = iter->next;
      newlength--;
   }

   if (iter == NULL)
      return output;

   if (include)
      output->head = list_dup(iter);
   else {
      if (iter->next == NULL)
         return output;
      output->head = list_dup(iter->next);
      newlength--;
   }

   iter = output->head;

   while (iter->next != NULL)
      iter = iter->next;

   output->tail = iter;
   output->length = newlength;

   return output;
}

/*
 * Extract the node containing a given data from a queue
 * \param queue The queue to extract from
 * \param data The data whose node we want to extract
 * \return The node containing the data or NULL if queue is NULL or data not present in queue
 * */
list_t* queue_extract_node(queue_t* queue, void* data)
{
   list_t *found = queue_lstlookup(queue, data);

   if (!found)
      return NULL;

   if (found == queue->head)
      queue->head = found->next;
   if (found == queue->tail)
      queue->tail = found->prev;

   if (found->next)
      found->next->prev = found->prev;
   if (found->prev)
      found->prev->next = found->next;

   queue->length--;

   return found;
}

/*
 * Adds an element data in queue queue before an element
 * \param queue The queue to update
 * \param elt The element before which we want to insert the new data
 * \param data The data for the new element
 * */
void queue_insertbefore(queue_t* queue, list_t* elt, void* data)
{
   if (!queue)
      return;

   if (elt == NULL)
      queue_add_tail(queue, data);

   else {
      list_t* newhead = list_add_before(elt, data);
      queue->length++;
      DBGMSGLVL(1,
            "Inserted data %p into element %p and before element %p in queue %p (now %d elements)\n",
            data, newhead, elt, queue, queue->length);

      if (elt == queue_iterator(queue))
         queue->head = newhead;
   }
}

/*
 * Adds an element data in queue queue after an element
 * \param queue The queue to update
 * \param elt The element after which we want to insert the new data
 * \param data The data for the new element
 */
void queue_insertafter(queue_t* queue, list_t* elt, void* data)
{
   if (!queue)
      return;

   if (elt == NULL)
      queue_add_head(queue, data);

   else {
      list_add_after(elt, data);
      queue->length++;
      DBGMSGLVL(1,
            "Inserted data %p after element %p in queue %p (now %d elements)\n",
            data, elt, queue, queue->length);

      if (elt == queue->tail)
         queue->tail = elt->next;
   }
}

/**
 * Extracts a subqueue from the queue, starting at and including element start
 * and ending at and including element end, and replaces it with the queue given
 * in parameter. The subqueue extracted is returned into the replace parameter
 * This function is used for factorisation and assumes all parameters are valid
 * \param queue The queue to update
 * \param startl The first element in the queue to be swapped
 * \param endl The last element in the queue to be swapped
 * \param replace The queue to swap with elements from \e queue. Contains the swapped elements
 * from \e queue at the end of the operation
 * \param len Number of elements between startl and endl
 * */
static void _queue_swap_elts(queue_t* queue, list_t* startl, list_t* endl,
      queue_t* replace, int len)
{
   assert(queue && startl && endl && replace);

   if (startl->prev) {
      if (replace->head)
         startl->prev->next = replace->head;
      else
         startl->prev->next = endl->next; /*If the swap queue is empty, nothing to add*/
   }

   if (endl->next) {
      if (replace->tail)
         endl->next->prev = replace->tail;
      else
         endl->next->prev = startl->prev;
   }

   if (replace->head)
      replace->head->prev = startl->prev;
   if (replace->tail)
      replace->tail->next = endl->next;

   startl->prev = NULL;
   endl->next = NULL;

   if (queue->head == startl)
      queue->head = replace->head;
   if (queue->tail == endl)
      queue->tail = replace->tail;

   replace->head = startl;
   replace->tail = endl;
   queue->length += replace->length - len;
   replace->length = len;
}

/*
 * Extracts a subqueue from the queue, starting at and including element start
 * and ending at and including element end, and replaces it with the queue given
 * in parameter. The subqueue extracted is returned into the replace parameter
 * \param queue The queue to update
 * \param startl The first element in the queue to be swapped. It is assumed to belong to \c queue and won't be checked
 * \param endl The last element in the queue to be swapped. It is assumed to belong to \c queue and won't be checked
 * \param replace The queue to swap with elements from \e queue. Contains the swapped elements
 * from \e queue at the end of the operation
 * \param len Number of elements between startl and endl
 * */
void queue_swap_elts(queue_t* queue, list_t* startl, list_t* endl,
      queue_t* replace)
{
   if (!queue || !startl || !endl)
      return;
   int len = 0;

   /*Counts the number of elements in the subqueue*/
   list_t* iter = startl;
   len++;
   while (iter) {
      if (iter == endl)
         break;
      iter = iter->next;
      len++;
   }

   _queue_swap_elts(queue, startl, endl, replace, len);
}

/*
 * Extracts a subqueue from the queue, starting at and including element start
 * and ending at and including element end, and replaces it with the queue given
 * in parameter. The subqueue extracted is returned into the replace parameter
 * \param queue The queue to update
 * \param start The first element in the queue to be swapped
 * \param end The last element in the queue to be swapped
 * \param replace The queue to swap with elements from \e queue. Contains the swapped elements
 * from \e queue at the end of the operation
 * */
void queue_swap(queue_t* queue, void* start, void* end, queue_t* replace)
{
   if (!queue)
      return;

   list_t* startl, *endl;

   int len = 0;
   /*Finds the beginning of the subqueue*/
   startl = queue->head;
   while (startl) {
      if (start == startl->data)
         break;
      startl = startl->next;
   }

   /*Finds the end of the subqueue*/
   endl = startl;
   len++;
   while (endl) {
      if (end == endl->data)
         break;
      endl = endl->next;
      len++;
   }

   /*The swap will only occur if both start and end have been found*/
   if (!startl || !endl)
      return;

   _queue_swap_elts(queue, startl, endl, replace, len);
}

/*
 * Scans a queue looking for an entry and returns the first occurrence
 * \param queue The queue to search into
 * \param data The value of the element to search for
 * \return The first encountered list_t object containing \e data
 * */
list_t* queue_lstlookup(queue_t* queue, void* data)
{
   if (!queue)
      return NULL;

   return list_lookup(queue_iterator(queue), data);
}

/*
 * Scans a queue looking for an element and return the first occurrence while using a custom
 * equal function for looking for the occurrence.
 * \param queue a queue
 * \param f a function to compare two elements. It must have the following prototype:
 *              int < my_function > (const void* e1, const void* e2); where:
 *                      - e1 is an element
 *                      - e2 is an element
 *                      - f return true (nonzero) if its two parameters are equal, else 0
 * \param data an element to look for
 * \return the element if found, else NULL
 */
void *queue_lookup(queue_t* queue, int (*f)(const void *, const void *),
      void* data)
{
   FOREACH_INQUEUE(queue, iter) {
      if (f(iter->data, data))
         return iter->data;
   }

   return NULL;
}

/*
 * Checks if two queues contains the same elements (not necessarily in the same order)
 * \param v1 a queue
 * \param v2 a queue
 * Returns 1 if the two queues contain the same elements, 0 otherwise
 * */
int queue_equal(const void* v1, const void* v2)
{
   if (!v1 || !v2)
      return (v1 == v2);

   queue_t* q1 = (queue_t*) v1;
   queue_t* q2 = (queue_t*) v2;

   if (q1->length != q2->length)
      return 0;

   /*Looks for every item in the first queue in the queue*/
   FOREACH_INQUEUE(q1, iter) {
      /*If one element is not found, the queues are not equal*/
      if (queue_lstlookup(q2, iter->data) == NULL)
         return 0;
   }

   return 1;
}

/**
 * Inserts the contents of a queue into another at a specified element
 * \param queue The queue to update
 * \param ins The queue to insert
 * \param elt The element in \e queue at which \e ins must be inserted
 * \param after Must be 1 if \e ins must be inserted after \e elt, 0 if it must be inserted before
 * */
static void _insert_queue(queue_t* queue, queue_t* ins, list_t* elt, int after)
{
   assert(queue && ins && elt);
   if ((elt == queue->head) && (!after)) { //ins must be added before the beginning of queue
      if (ins->tail) {
         ins->tail->next = queue->head;
         queue->head->prev = ins->tail;
      }
      queue->head = ins->head;
   } else if ((elt == queue->tail) && (after)) { //ins must be added after the end of queue
      if (ins->head) {
         ins->head->prev = queue->tail;
         queue->tail->next = ins->head;
      }
      queue->tail = ins->tail;
   } else if (after) {
      if (ins->head)
         ins->head->prev = elt;
      if (ins->tail)
         ins->tail->next = elt->next;
      elt->next->prev = ins->tail;
      elt->next = ins->head;
   } else if (!after) {
      if (ins->head)
         ins->head->prev = elt->prev;
      if (ins->tail)
         ins->tail->next = elt;
      elt->prev->next = ins->head;
      elt->prev = ins->tail;
   }
   queue->length += ins->length;

}

/*
 * Inserts the contents of a queue into another at a specified element
 * \param queue The queue to update
 * \param ins The queue to insert
 * \param elt The element in \e queue at which \e ins must be inserted
 * \param after Must be 1 if \e ins must be inserted after \e elt, 0 if it must be inserted before
 * \warning The inserted queue is incorporated into the insertee, so ins is freed
 * */
void queue_insert(queue_t* queue, queue_t* ins, list_t* elt, int after)
{
   if (!queue || !ins || !elt)
      return;
   _insert_queue(queue, ins, elt, after);
   lc_free (ins);
}

/*
 * Inserts the contents of a queue into another at a specified element
 * \param queue The queue to update
 * \param ins The queue to insert
 * \param elt The element in \e queue at which \e ins must be inserted
 * \param after Must be 1 if \e ins must be inserted after \e elt, 0 if it must be inserted before
 * \warning Neither queue is freed
 * */
void queue_insert_and_keep(queue_t* queue, queue_t* ins, list_t* elt, int after)
{
   if (!queue || !ins || !elt)
      return;
   _insert_queue(queue, ins, elt, after);
}

/*
 * Sorts a queue using stdlib/qsort, according to a comparison function
 * To sort a list of \c item_t*, your function must be similar to the following:
 * \code
 * int compare_items (const void* p1, const void* p2) {
 *     const item_t *i1 = *((void **) p1);
 *     const item_t *i2 = *((void **) p2);
 *     // Here normal compare...
 * }
 * /endcode
 * Please also be aware that in the case of structures keeping track of their
 * iterator, for example \c insn_t with its \c sequence attribute, you will
 * need to update them, because they will \b not be correct.
 * \param queue a queue
 * \param compar comparison function (between data fields of two elements)
 */
void queue_sort(queue_t *queue, int (*compar)(const void *, const void *))
{
   if (!queue || !compar || !queue->length)
      return;

   int len = queue->length;

   /* Allocates a temporary array to save queue data */
   void **tmp_array = lc_malloc(len * sizeof *tmp_array);

   /* Copies data from the queue to the array */
   void **array_ptr = tmp_array;
   FOREACH_INQUEUE(queue, e1) {
      *(array_ptr++) = e1->data;
   }

   /* Sorts the array using the comparison function passed as parameter */
   qsort(tmp_array, len, sizeof *tmp_array, compar);

   /* Copies back data from the array to the queue */
   array_ptr = tmp_array;
   FOREACH_INQUEUE(queue, e2) {
      e2->data = *(array_ptr++);
   }

   /* Now data were restored from the array, frees it */
   lc_free(tmp_array);
}
