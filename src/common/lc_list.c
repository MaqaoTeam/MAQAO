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
//                               list functions                              //
///////////////////////////////////////////////////////////////////////////////

/*
 * Creates a new list
 * \param data element to add in the list
 * \return a new list
 */
list_t *list_new(void *data)
{
   list_t *list = lc_malloc(sizeof *list);

   list->next = NULL;
   list->prev = NULL;
   list->data = data;

   return list;
}

/*
 * Adds an element before a given element
 * \param list an element before whom the new element must be added
 * \param data element to add
 * \return the modified list
 */
list_t *list_add_before(list_t *list, void *data)
{
   list_t *new = list_new(data);

   if (!list)
      return new;

   if (list->prev) {
      list->prev->next = new;
      new->prev = list->prev;
   }

   list->prev = new;
   new->next = list;

   return new;
}

/*
 * Adds an element after a given element
 * \param list an element after whom the new element must be added
 * \param data an element to add
 * \return the modified list
 */
list_t* list_add_after(list_t *list, void *data)
{
   list_t *new = list_new(data);

   DBGMSGLVL(1, "Adding data %p after list element %p in element %p\n", data,
         list, new);

   if (!list)
      return new;

   if (list->next) {
      list->next->prev = new;
      new->next = list->next;
   }

   list->next = new;
   new->prev = list;

   return list;
}

/*
 * Removes a list element and returns the corresponding data
 * Remark: if \e list is the head of a list, save the address
 * of the next element or use list_remove_head instead.
 * \param list A valid list element
 * \return corresponding data
 */
void* list_remove_elt(list_t *list)
{
   if (!list)
      return NULL;

   if (list->prev)
      list->prev->next = list->next;
   if (list->next)
      list->next->prev = list->prev;

   void* data = list->data;
   lc_free(list);

   return data;
}

/*
 * Removes and returns the first element of a list
 * \param list a list
 * \return the head element
 */
void* list_remove_head(list_t **list)
{
   if (!list || !(*list))
      return NULL;

   list_t *new_head = (*list)->next;
   void *data = list_remove_elt(*list);
   *list = new_head;

   return data;
}

/*
 * Finds, removes and frees (if \e f not NULL) the element of a list corresponding to \e data
 * \param list a list
 * \param data the element to remove
 * \param f a function used to free the element, with the following prototype:
 *      void < my_function > (void* data); where:
 *                      - data is the element to free
 * \return the list without the element
 */
list_t* list_remove(list_t* list, void * data, void (*f)(void*))
{
   list_t *found = list_lookup(list, data);

   if (!found)
      return list;

   /* If head corresponds */
   if (found == list)
      list = list->next;

   list_remove_elt(found);
   if (f)
      f(data);

   return list;
}

/*
 * Frees a list and its elements if \e f not null
 * \param list a list to free
 * \param f a function used to free an element, with the following prototype:
 *      void < my_function > (void* data); where:
 *                      - data is the element to free
 */
void list_free(list_t *list, void (*f)(void*))
{
   while (list != NULL) {
      list_t *next = list->next; /* save next element address */
      DBGMSGLVL(1, "Freeing list object %p containing data %p\n", list,
            list->data);
      if (f)
         f(list->data);
      lc_free(list);

      list = next;
   }
}

/*
 * Gets the size of (number of element in) a list
 * \param list a list
 * \return size of the list
 */
int list_length(list_t *list)
{
   int n = 0;

   while (list != NULL) {
      n++;
      list = list->next;
   }

   return n;
}

/*
 * Get the next element of a list
 * \param l a list
 * \return the next element or PTR_ERROR if there is a problem
 */
list_t* list_getnext(list_t* l)
{
   return ((l != NULL) ? l->next : PTR_ERROR);
}

/*
 * Get the previous element of a list
 * \param l a list
 * \return the previous element or PTR_ERROR if there is a problem
 */
list_t* list_getprev(list_t* l)
{
   return ((l != NULL) ? l->prev : PTR_ERROR);
}

/*
 * Get the data in an element of a list
 * \param l a list
 * \return the data or PTR_ERROR if there is a problem
 */
void* list_getdata(list_t* l)
{
   return ((l != NULL) ? l->data : PTR_ERROR);
}

/*
 * Executes a function f on every element in a list
 * \param f Must take two parameters, the first being the pointer to the element in the list,
 *  the second being user defined
 * \param user The second parameter to be fed to \e f
 */
void list_foreach(list_t *list, void (*f)(void *, void *), void *user)
{
   if (!f) return;

   while (list != NULL) {
      f(list->data, user);
      list = list->next;
   }
}

/*
 * Duplicates a list
 * \param list a list
 * \return A list containing the same elements as the original
 */
list_t *list_dup(list_t *list)
{
   if (!list)
      return NULL;

   /* Deals with the first element */
   list_t *head = list_new(list->data);
   list_t *tail = head;
   list = list->next;

   /* Next elements */
   while (list) {
      tail = list_add_after(tail, list->data)->next;
      list = list->next;
   }

   return head;
}

/*
 * Cuts after the first cell in \e orig containing \e data
 * \param orig a list
 * \param data an element
 * \return the end of \e orig, else NULL if \e data not found
 */
list_t* list_cut_after(list_t* orig, void * data)
{
   /* No need to check whether orig is NULL: handled by list_lookup */
   list_t *cut_after = list_lookup(orig, data);
   if (!cut_after || !cut_after->next)
      return NULL;

   cut_after->next->prev = NULL;
   list_t *cutted_list = cut_after->next;
   cut_after->next = NULL;

   return cutted_list;
}

/*
 * Cuts before the first cell in \e orig containing \e data
 * \param orig a list
 * \param end the end of the list (after cell in \e orig containing data) or NULL if data not found
 * \param data an element
 * \return the begining of \e orig, else NULL if \e data not found
 */
list_t* list_cut_before(list_t* orig, list_t** end, void * data)
{
   (*end) = NULL;

   /* No need to check whether orig is NULL: handled by list_lookup */
   list_t *cut_after = list_lookup(orig, data);
   if (!cut_after || !cut_after->prev)
      return orig;

   cut_after->prev->next = NULL;
   list_t *cutted_list = cut_after;
   (*end) = orig; //cut_after->prev;
   cut_after->prev = NULL;

   return cutted_list;
}

/*
 * Finds the first element in the list containing \e data
 * \param list a list
 * \param data The data to look for
 * \return The first list element containing \e data
 */
list_t *list_lookup(list_t *list, void *data)
{
   DBGMSG0("Lookup in a list\n");
   /* No need to check whether list is NULL: handled by the loop predicate */
   while (list && list->data != data)
      list = list->next;

   return list;
}
