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
 * \file lc_array.c
 * \brief defines functions implementing dynamic (variable size) array
 * */
#include "libmcommon.h"

///////////////////////////////////////////////////////////////////////////////
//                              array functions                              //
///////////////////////////////////////////////////////////////////////////////

/*
 * Creates a new, empty array
 * \return An array of size 0
 */
array_t *array_new()
{
   return array_new_with_custom_size(ARRAY_INIT_SIZE);
}

/*
 * Creates an array with a custom initial size
 * \param size initial size. If <= 0, use ARRAY_INIT_SIZE
 * \return An array of size 'size'
 */
array_t *array_new_with_custom_size(int size)
{
   array_t *new = lc_malloc(sizeof *new);

   if (size <= 0)
      size = ARRAY_INIT_SIZE;

   new->length = 0;
   new->init_length = size;
   new->max_length = size;
   new->mem = lc_malloc (size * sizeof *(new->mem));

   return new;
}

/*
 * Adds a new element at the end of an array
 * \param array The array to update
 * \param data The data of the new element to add
 */
void array_add(array_t *array, void *data)
{
   if (!array) return;

   if (array->length == array->max_length) {
      if (array->max_length > ARRAY_MAX_INCREASE_SIZE)
         array->max_length += ARRAY_MAX_INCREASE_SIZE;
      else
         array->max_length *= 2;

      array->mem = lc_realloc(array->mem,
            array->max_length * sizeof *(array->mem));
   }

   ARRAY_ELT_AT_POS(array,array->length++) = data;
}

/*
 * Removes an element at the end of an array
 * \param array The array to update
 * \return data The removed data
 */
void *array_remove(array_t *array)
{
   if (!array || array->length == 0) return NULL;

   if (array->length <= (array->max_length / 4)) {
      array->max_length /= 2;
      array->mem = lc_realloc(array->mem,
            array->max_length * sizeof *(array->mem));
   }

   return ARRAY_ELT_AT_POS(array,array->length--);
}

/*
 * Returns the nth element in an array
 * \param array an array
 * \param pos rank of the element to get
 * \return nth element
 */
void *array_get_elt_at_pos(array_t *array, int pos)
{
   if (!array || pos >= array->length) return NULL;

   return ARRAY_ELT_AT_POS(array,pos);
}

/*
 * Returns the first element in an array
 * \param array an array
 * \return first element
 */
void *array_get_first_elt(array_t *array)
{
   if (!array || array->length == 0) return NULL;

   return ARRAY_ELT_AT_POS(array, 0);
}

/*
 * Returns the last element in an array
 * \param array an array
 * \return last element
 */
void *array_get_last_elt(array_t *array)
{
   if (!array || array->length == 0) return NULL;

   return ARRAY_ELT_AT_POS(array, array->length - 1);
}

/*
 * Sets the nth element in an array
 * \param array an array
 * \param pos rank of the element to set
 * \param data data to set
 */
void array_set_elt_at_pos(array_t *array, int pos, void *data)
{
   if (!array || pos >= array->length) return;

   ARRAY_ELT_AT_POS(array,pos) = data;
}

/*
 * Returns the length of a array
 * \param array a array
 * \return \e array length, or 0 if array is NULL
 */
int array_length(array_t *array)
{
   return ((array != NULL) ? array->length : 0);
}

/*
 * Checks if a array is empty
 * \param array a array
 * \return 1 if \e array is empty or NULL, else 0
 */
int array_is_empty(array_t *array)
{
   return ((array == NULL) || array->length == 0);
}

/*
 * Frees all elements in a array and sets its length to 0
 * \param array a array
 * \param f a function used to free an element, with the following prototype:
 *      void < my_function > (void* data); where:
 *                      - data is the element to free
 */
void array_flush(array_t *array, void (*f)(void*))
{
   if (!array) return;

   if (f) {
      unsigned int i;
      for (i = 0; i < array->length; i++)
         f(ARRAY_ELT_AT_POS(array,i));
   }

   array->length = 0;
   array->max_length = array->init_length;
   array->mem = lc_realloc (array->mem,
         array->init_length * sizeof *(array->mem));
}

/*
 * Frees a array and its elements if \e f is not null
 * \param array a array
 * \param f a function used to free an element, with the following prototype:
 *      void < my_function > (void* data); where:
 *                      - data is the element to free
 */
void array_free(array_t *array, void (*f)(void*))
{
   if (!array) return;

   if (f) {
      unsigned int i;
      for (i = 0; i < array->length; i++)
         f(ARRAY_ELT_AT_POS(array,i));
   }

   lc_free(array->mem);
   lc_free(array);
}

/*
 * Runs a function on every element in a array
 * \param array a array
 * \param f a function exectued of each element. It must have the following prototype:
 *              void < my_function > (void* elemen, void* user); where:
 *                      - elem is the element
 *                      - user a 2nd parameter given by the user
 * \param user a parameter given by the user for the function to execute
 * */
void array_foreach(array_t *array, void (*f)(void *, void *), void *user)
{
   if (!array || !f) return;

   unsigned int i;
   for (i = 0; i < array->length; i++)
      f(ARRAY_ELT_AT_POS(array,i), user);
}

/*
 * Scans an array looking for an element and return the first occurrence while using a custom
 * equal function for looking for the occurrence.
 * \param array an array
 * \param f a function to compare two elements. It must have the following prototype:
 *              int < my_function > (const void* e1, const void* e2); where:
 *                      - e1 is an element
 *                      - e2 is an element
 *                      - f return true (nonzero) if its two parameters are equal, else 0
 * \param data an element to look for
 * \return the element if found, else NULL
 */
void *array_lookup(array_t* array, int (*f)(const void *, const void *),
      void* data)
{
   if (!array || !f) return NULL;

   unsigned int i;
   for (i = 0; i < array->length; i++)
      if (f(ARRAY_ELT_AT_POS(array,i), data))
         return ARRAY_ELT_AT_POS(array,i);

   return NULL;
}

/*
 * Sorts an array using stdlib/qsort, according to a comparison function
 * \see queue_sort
 * \param array an array
 * \param compar comparison function (between two elements)
 */
void array_sort (array_t *array, int (*compar)(const void *, const void *))
{
   if (!array || !compar) return;

   /* Sorts the array using the comparison function passed as parameter */
   qsort (array->mem, array->length, sizeof array->mem[0], compar);
}

/*
 * Duplicates an array
 * \param array The array to duplicate
 * \return A clone array
 */
array_t *array_dup (array_t *array)
{
   if (!array) return NULL;

   array_t *dup = array_new_with_custom_size (array->length);
   memcpy (dup->mem, array->mem, array->length * sizeof *(array->mem));
   dup->length = array->length;

   return dup;
}

/*
 * Appends to a1 content of a2 (a1 = a1 + a2)
 * \param a1 left/destination array
 * \param a2 right/source array
 */
void array_append (array_t *a1, array_t *a2)
{
   const int l1 = array_length (a1);
   const int l2 = array_length (a2);

   if (l1 == 0 || l2 == 0) return;

   const int new_l1 = l1 + l2;
   if (a1->max_length < new_l1) {
      a1->mem = lc_realloc (a1->mem,
                            new_l1 * sizeof *(a1->mem));
      a1->max_length = new_l1;
   }

   memcpy (a1->mem + l1 * sizeof *(a1->mem),
           a2->mem, l2 * sizeof *(a2->mem));

   a1->length = new_l1;
}
