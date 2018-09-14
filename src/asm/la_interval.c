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

#include "libmasm.h"
#include "inttypes.h"

/*
 * lc_interval.c
 *
 *  Created on: 4 juin 2014
 */

/*
 * Creates a new interval
 * \param address Starting address of the interval
 * \param size Size of the interval
 * \return A new interval structure with the specified starting address and size
 * */
interval_t* interval_new(int64_t address, uint64_t size)
{
   interval_t* interval = lc_malloc0(sizeof(*interval));
   interval->address = address;
   interval->size = size;
   DBGLVL(1,
         FCTNAMEMSG("Created interval %p ", interval); interval_fprint(interval, stderr); STDMSG("\n"));
   return interval;
}

/*
 * Frees an interval structure
 * \param interval The interval structure to free
 * */
void interval_free(interval_t* interval)
{
   if (!interval)
      return;
   DBGLVL(1,
         FCTNAMEMSG("Deleting interval %p ", interval); interval_fprint(interval, stderr); STDMSG("\n"));
   lc_free(interval);
}

/*
 * Gets the starting address of an interval
 * \param interval The interval structure
 * \return The starting address of the interval, or SIGNED_ERROR if \c interval is NULL
 * */
int64_t interval_get_addr(interval_t* interval)
{
   return (interval) ? interval->address : SIGNED_ERROR;
}

/*
 * Gets the size of an interval
 * \param interval The interval structure
 * \return The size of the interval, or UNSIGNED_ERROR if \c interval is NULL
 * */
uint64_t interval_get_size(interval_t* interval)
{
   return (interval) ? interval->size : UNSIGNED_ERROR;
}

/*
 * Gets the ending address of an interval
 * \param interval The interval structure
 * \return The ending address of the interval, or SIGNED_ERROR if \c interval is NULL
 * */
int64_t interval_get_end_addr(interval_t* interval)
{
   if (!interval)
      return SIGNED_ERROR;
   if (interval->size == UINT64_MAX)
      return INT64_MAX; //Infinite size: returning infinite address
   return interval->address + (int64_t) interval->size;
   /**\todo TODO (2015-02-26) Handle the case of address+size being superior INT64_MAX while size
    * is inferior to UINT64_MAX.*/
}

/*
 * Updates the starting address of an interval. Its size will be adjusted accordingly
 * \param interval The interval structure
 * \param newaddr New starting address of the interval. If superior to the current ending address,
 * the interval size will be reduced to 0
 * */
void interval_upd_addr(interval_t* interval, int64_t newaddr)
{
   if (!interval)
      return;
   if (newaddr < interval->address) {
      //New address inferior to the original starting address: increasing size
      interval->size += (interval->address - newaddr);
      interval->address = newaddr;
   } else if (newaddr > interval->address) {
      //New address is bigger than the original
      if (newaddr <= (interval->address + interval->size)) {
         //New address is still inside the original interval: decreasing size (may lead to size 0)
         interval->size -= (newaddr - interval->address);
         interval->address = newaddr;
      } else {
         //New address outside of the original interval: reducing size to 0
         interval->size = 0;
         interval->address = newaddr;
      }
   } // Else the new address is identical to the original one...
}

/*
 * Sets the size of an interval. Its starting address will not be modified
 * \param interval The interval
 * \param newsize The new size
 * */
void interval_set_size(interval_t* interval, uint64_t newsize)
{
   if (interval)
      interval->size = newsize;
}

/*
 * Sets the used-defined data of an interval.
 * \param interval The interval
 * \param data Pointer to the data to link to the interval
 * */
void interval_set_data(interval_t* interval, void* data)
{
   if (interval)
      interval->data = data;
}

/*
 * Retrieves the user-defined data linked to the interval
 * \param interval The interval
 * \return Pointer to the data linked to the interval or NULL if \c interval is NULL
 * */
void* interval_get_data(interval_t* interval)
{
   return (interval) ? interval->data : NULL;
}

//////////UNUSED! REMOVE OR MOVE TO TMP
/*
 * Updates the size of an interval with a given modifier
 * \param interval The interval
 * \param modifier The value to add or subtract to its size
 * \param add Set to TRUE if \c modifier must be added to the size of interval, FALSE if it must be subtracted
 * \return TRUE if the interval size could be successfully updated, FALSE otherwise
 * */
int interval_updsize(interval_t* interval, uint64_t modifier, int add)
{
   if (!interval)
      return FALSE;
   if (!add && modifier >= interval->size)
      return FALSE; //Requested to remove a larger size than the size of the interval
   if (add)
      interval->size += modifier;
   else
      interval->size -= modifier;
   return TRUE;
}

/*
 * Updates the ending address of an interval by changing its size
 * \param interval The interval structure
 * \param newend New ending address of the interval. If inferior to the current starting address,
 * the interval size will be reduced to 0
 * */
void interval_upd_end_addr(interval_t* interval, int64_t newend)
{
   if (!interval)
      return;
   if (newend >= interval->address) {
      //New ending address is superior or equal to the starting address: adjusting size
      interval->size = newend - interval->address;
   } else {
      //New ending address is inferior to the starting address: reducing size to 0
      interval->size = 0;
   }
}

/*
 * Sets a flag of an interval (not one about reachability)
 * \param interval The interval structure
 * \param flag Flag to set
 * */
void interval_set_flag(interval_t* interval, uint8_t flag)
{
   if (interval)
      interval->flags = flag;
}

/*
 * Retrieves a flag of an interval (not one about reachability)
 * \param interval The interval structure
 * \return flag Flag set on the interval, or 0 if \c interval is NULL
 * */
uint8_t interval_get_flag(interval_t* interval)
{
   return (interval) ? interval->flags : 0;
}

/*
 * Splits an existing interval around a given address. A new interval beginning at the address
 * of the interval and ending at the given address will be created, while the address of the existing
 * interval will be set to the given address
 * \param interval The interval to split. Its beginning address will be updated to \c splitaddr
 * \param splitaddr Address around which to split
 * \return The new interval, beginning at the original address of \c interval and ending at splitaddr, or NULL
 * if \c interval is NULL or \c splitaddr is outside of the range covered by \c interval
 * */
interval_t* interval_split(interval_t* interval, int64_t splitaddr)
{
   if (!interval || splitaddr < interval->address
         || splitaddr >= interval_get_end_addr(interval))
      return NULL;

   //Creating an interval running from the beginning of the found interval to the starting address
   interval_t* part = interval_new(interval->address,
         splitaddr - interval->address);
   DBGLVL(1,
         FCTNAMEMSG("Split interval %p ", interval); interval_fprint(interval, stderr); STDMSG(" at %#"PRIx64" => ", splitaddr));
   //Reduce the original interval
   interval_upd_addr(interval, splitaddr);
   //Copying the existing flags
   part->flags = interval->flags;
   DBGLVL(1,
         STDMSG("%p ", part); interval_fprint(part, stderr); STDMSG(" + %p ", interval); interval_fprint(interval, stderr); STDMSG("\n"););
   return part;
}

/*
 * Appends an interval at the end of another, only if the end address of the first is equal to the address of the second
 * \param interval Interval to which another must be appended
 * \param appended The interval to append to \c interval. It will not be freed
 * \return TRUE if \c merged had the same address as the end address of \c interval and was successfully merged, FALSE otherwise
 * */
int interval_merge(interval_t* interval, interval_t* merged)
{
   if (!interval || !merged)
      return FALSE;
   if (interval_get_end_addr(interval) != interval_get_addr(merged))
      return FALSE;
   //Updates the size of the interval
   interval_upd_end_addr(interval, interval_get_end_addr(merged));
   return TRUE;
}

/*
 * Compares two intervals based on their starting address. This function is intended to be used with qsort
 * \param i1 First interval
 * \param i2 Second interval
 * \return -1 if the starting address of i1 is less than the starting address of i2, 0 if they are equal and 1 otherwise
 *
 * */
int interval_cmp_addr_qsort(const void *i1, const void *i2)
{
   interval_t* interval1 = *(interval_t**) i1;
   interval_t* interval2 = *(interval_t**) i2;

   if (!interval1 || !interval2)
      return (interval1) ? 1 : -1;

   if (interval1->address < interval2->address)
      return -1;
   else if (interval1->address > interval2->address)
      return 1;
   else
      return 0;
}

/*
 * Prints an interval
 * \param interval An interval to print
 * */
void interval_fprint(interval_t* interval, FILE* stream)
{
   if (!interval || !stream)
      return;
   fprintf(stream, "[%#"PRIx64, interval->address);
   if (interval->size == UINT64_MAX)
      fprintf(stream, " - infinity]");
   else
      fprintf(stream, " - %#"PRIx64"] (%"PRIu64" bytes)",
            interval_get_end_addr(interval), interval->size);
}

/*
 * Checks if a given interval can contain a given size with the given alignment
 * \param interval The interval
 * \param size The size to check against the size of the interval.
 * \param align Alignment over which the beginning address of the given size must be aligned. If not null,
 * it will be considered that the given size is inserted into the interval at an address satisfying addr%align = 0
 * \return The total size needed to insert the given size into the interval, starting at the address of the interval,
 * or 0 if the size can't fit inside the interval or if \c interval is NULL
 * */
uint64_t interval_can_contain_size(interval_t* interval, uint64_t size,
      uint64_t align)
{
   if (!interval)
      return 0;
   int64_t addralgn = 0;   //Value to add to the address to align it
   if (align > 0) {
      uint32_t intalign = interval->address % align;
      if (intalign > 0)
         addralgn = align - intalign;
   }
   if (size <= (interval->size + addralgn)) {
      //The size can fit inside the given interval
      return (size + addralgn);
   } else
      return 0;
}
