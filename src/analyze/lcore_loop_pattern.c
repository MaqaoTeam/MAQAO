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

/**\file
 * \brief This file provides loop pattern identification functions */

#include "libmcore.h"

/* Try to detect loop pattern. Return NULL if it isn't recognized 
 * Returned value has to be freed.
 * */
loop_pattern maqao_loop_pattern_detect(loop_t * loop)
{

   if (list_length(loop->exits) == 1 && list_length(loop->entries) == 1) {

      block_t * exit_block = GET_DATA_T(block_t*, loop->exits);
      block_t * entry_block = GET_DATA_T(block_t*, loop->entries);
      enum BlockLink lnk = maqao_block_link_type(exit_block, entry_block);

      if (block_get_id(entry_block) == block_get_id(exit_block)
            && lnk == BlockLinkNone) {
         loop_pattern pat = malloc(sizeof(struct loop_pattern_s));
         pat->type = While;
         pat->pattern_while.entry_exit = entry_block;
         return pat;
      }

      if (lnk == BlockLinkConditionalJump || lnk == BlockLinkDirect) {
         loop_pattern pat = malloc(sizeof(struct loop_pattern_s));
         pat->type = Repeat;
         pat->pattern_repeat.entry = entry_block;
         pat->pattern_repeat.exit = exit_block;
         return pat;
      }

      return NULL;
   }

   if (list_length(loop->exits) > 1 && list_length(loop->entries) == 1) {

      block_t * entry_block = GET_DATA_T(block_t*, loop->entries);

      int onlyValidLinks = 1;
      int i;
      for (i = 0; i < list_length(loop->exits); i++) {
         block_t * exit_block = GET_DATA_T(block_t*, loop->exits);
         enum BlockLink lnk = maqao_block_link_type(exit_block, entry_block);

         if (lnk != BlockLinkConditionalJump && lnk != BlockLinkDirect) {
            onlyValidLinks = 0;
            break;
         }
      }

      if (onlyValidLinks) {
         loop_pattern pat = malloc(sizeof(struct loop_pattern_s));
         pat->type = MultiRepeat;
         pat->pattern_multirepeat.entry = entry_block;
         return pat;
      }

      return NULL;
   }

   return NULL;
}
