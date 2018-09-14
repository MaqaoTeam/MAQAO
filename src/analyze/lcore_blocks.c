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

/**\file lcore_blocks.c
 * \brief This file provides block utility functions */

#include "libmcore.h"

/**
 * Indicate if an instruction is an unconditional branch, i.e. that the next instruction won't be executed
 * @param in The instruction
 * @return 1 if it is an unconditional branch, 0 otherwise
 */
static int maqao_insn_is_unconditional_branch(insn_t *in)
{
   return (insn_check_annotate(in, A_RTRN)
         || insn_check_annotate(in, A_HANDLER_EX)
         || (insn_check_annotate(in, A_JUMP)
               && !insn_check_annotate(in, A_CONDITIONAL)));
}

/*
 * Indicate the kind of the link between two blocks (if any)
 * \param b1 First block
 * \param b2 Second block
 * \return Identifier of the type of link
 * */
enum BlockLink maqao_block_link_type(block_t *b1, block_t * b2)
{
   insn_t * last = block_get_last_insn(b1);
   insn_t * first = block_get_first_insn(b2);

   if (last == NULL || first == NULL)
      return BlockLinkNone;

   if (!maqao_insn_is_unconditional_branch(
         last) && INSN_GET_ADDR(last) + insn_get_size(last) / 8 == INSN_GET_ADDR(first)) {
      return BlockLinkDirect;
   } else if (insn_is_branch(
         last) && INSN_GET_ADDR(insn_get_branch(last)) == INSN_GET_ADDR(first)) {
      if (insn_check_annotate(last, A_CONDITIONAL)) {
         return BlockLinkConditionalJump;
      } else {
         return BlockLinkUnconditionalJump;
      }
   } else {
      return BlockLinkNone;
   }
}
