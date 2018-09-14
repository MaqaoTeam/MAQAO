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

#include <inttypes.h>

#include "libmcore.h"

///////////////////////////////////////////////////////////////////////////////
//                         Indirect branches solver                          //
///////////////////////////////////////////////////////////////////////////////
/**
 * Check if b contains an indirect branch (its last instruction)
 * \param b a basic block
 * return 1 if b contains an indirect block, else 0
 */
static int is_indirect_block(block_t* b)
{
   if (b == NULL)
      return 0;

   insn_t* in = block_get_last_insn(b);
   unsigned short f = insn_get_family(in);

   if (f == FM_JUMP && oprnd_is_ptr(insn_lookup_ref_oprnd(in)) == FALSE)
      return 1;

   return 0;
}

/**
 * Check if there is a modification on reg in instruction in
 * \param in an instruction
 * \param reg a register
 * \param add_reg address where the unknow register is
 * \return 1 if there is a modification, else 0
 */
static int is_modif_on_reg(insn_t* in, reg_t* reg, int64_t add_reg)
{
   if ((reg != NULL) && (in != NULL) && (INSN_GET_ADDR(in) != add_reg)
         && (insn_get_nb_oprnds(in) > 0)
         && (oprnd_is_reg(insn_get_oprnd(in, insn_get_nb_oprnds(in) - 1))
               == TRUE)
         && (reg_get_name(
               oprnd_get_reg(insn_get_oprnd(in, insn_get_nb_oprnds(in) - 1)))
               == reg_get_name(reg)) && (insn_get_family(in) != FM_JUMP)
         // && ((!str_contain (insn_get_opcode(in), "CMP") && !str_contain (insn_get_opcode(in), "TEST")))
         )
      return 1;
   else
      return 0;
}

/**
 * Find last definition (MOV only) of a given register (target) in a basic block (b)
 * \param target target register
 * \param b a basic block
 * \param add_reg address where the unknow register is
 * \return NULL if there is a problem or if no definition found
 *   else a pointeur on list element which data is the definition instruction
 */
static list_t* find_last_definition(reg_t* target, block_t* b, int* flag,
      int64_t add_reg)
{
   if (b == NULL || block_get_last_insn(b) == NULL)
      return NULL;

   insn_t* in;
   list_t* it = insn_get_sequence(block_get_last_insn(b));

   do {
      in = GET_DATA_T(insn_t*, it);

      if ((insn_get_family(in) == FM_MOV)
            && (oprnd_is_reg(insn_get_oprnd(in, 1)) == TRUE)
            && (reg_get_name(oprnd_get_reg(insn_get_oprnd(in, 1)))
                  == reg_get_name(target)))
         return insn_get_sequence(in);

      if (is_modif_on_reg(in, target, add_reg) == 1) {
         *flag = 1;
         return NULL;
      }

      it = list_getprev(it);
   } while (INSN_GET_ADDR(in) >= INSN_GET_ADDR(block_get_first_insn(b)));

   return NULL;
}

/**
 * Search a memory address used to defined indirect branch target.
 * This function searchs in the block and in its predecessors while there is only
 * one predecessor per blocks
 * \param b a basic block
 * \param insn_out will contain pointeur on list element which data is the searched instruction
 * \param b_out will contain the block wich conatins the searched instruction
 * \param add_reg address where the unknow register is
 * \return a memory address if found, else NULL
 */
static oprnd_t* find_memory_componants(block_t* b, list_t** insn_out,
      block_t** b_out, int64_t add_reg)
{
   if (b == NULL)
      return NULL;

   insn_t* in = block_get_last_insn(b);
   int type = oprnd_get_type(insn_get_oprnd(in, 0));
   int flag = 0;

   if (type == OT_MEMORY || type == OT_MEMORY_RELATIVE) {
      *insn_out = insn_get_sequence(block_get_last_insn(b));
      *b_out = b;
      return insn_get_oprnd(in, 0);
   }

   if (type == OT_REGISTER || type == OT_REGISTER_INDEXED) {
      reg_t* target = oprnd_get_reg(insn_get_oprnd(in, 0));
      insn_t* def = NULL;
      block_t* b_prev = b;

      while (def == NULL) {
         list_t* l_tmp = find_last_definition(target, b_prev, &flag, add_reg);
         if (l_tmp != NULL)
            def = GET_DATA_T(insn_t*, l_tmp);

         if (flag == 1)
            return NULL;

         if (def != NULL && oprnd_is_mem(insn_get_oprnd(def, 0)) == TRUE) {
            *insn_out = l_tmp;
            *b_out = b_prev;
            return insn_get_oprnd(def, 0);
         }

         if (list_length(block_get_CFG_node(b_prev)->in) != 1)
            return NULL;

         b_prev = ((graph_edge_t*) block_get_CFG_node(b_prev)->in->data)->from->data;
      }
   }

   return NULL;
}

/*
 * Search a CMP instruction beteween a register (reg) and an immediate, then return this immediate.
 * This function searchs in the block and in its predecessors while there is only
 * one predecessor per blocks
 * \param b a basic block
 * \param l_in pointeur on list element which data is the started instruction
 * \param reg register to look for
 * \param add_reg address where the unknow register is
 * \return an immediate value which is compared with reg, else 0
 */
static int64_t find_imm_cmp(block_t* b, list_t* l_in, reg_t* reg,
      int64_t add_reg)
{
   insn_t *in;

   do {
      in = GET_DATA_T(insn_t*, l_in);
      unsigned short family = insn_get_family(in);

      if ((family == FM_CMP) && (oprnd_is_imm(insn_get_oprnd(in, 0)) == TRUE)
            && (oprnd_is_reg(insn_get_oprnd(in, 1)) == TRUE)
            && (reg_get_name(oprnd_get_reg(insn_get_oprnd(in, 1)))
                  == reg_get_name(reg)))
         return oprnd_get_imm(insn_get_oprnd(in, 0));

      if ((family == FM_CMP) && (oprnd_is_imm(insn_get_oprnd(in, 0)) == FALSE)
            && (oprnd_is_reg(insn_get_oprnd(in, 1)) == TRUE)
            && (reg_get_name(oprnd_get_reg(insn_get_oprnd(in, 1)))
                  == reg_get_name(reg)))
         return 0;

      if (is_modif_on_reg(in, reg, add_reg) == 1)
         return 0;

      l_in = list_getprev(l_in);
   } while (INSN_GET_ADDR(in) >= INSN_GET_ADDR(block_get_first_insn(b)));

   if (list_length(block_get_CFG_node(b)->in) == 1) {
      block_t* bp = ((graph_edge_t*) block_get_CFG_node(b)->in->data)->from->data;
      return find_imm_cmp(bp, bp->end_sequence, reg, add_reg);
   } else {
      return 0;
   }
}

/*
 * Search for a value saved in memory
 * \param f current function
 * \param start address of the value to look for
 * \param size size (in byte) of the value
 * \return the value, else 0 if there is a problem
 */
static int64_t find_from_memory(fct_t* f, int64_t start, int size)
{
   if (fct_get_asmfile(f)->getbytes == NULL)
      return 0;
   unsigned char *mem = fct_get_asmfile(f)->getbytes(fct_get_asmfile(f), start,
         8);
   if (mem == NULL)
      return 0;

   int i;
   int64_t res = 0;

   for (i = size - 1; i >= 0; i--)
      res = (res * 256) + mem[i];

   return res;
}

/*
 * Look for a block which contains targeted instruction
 * \param f the function function wich contains the instruction
 * \param address address of targeted instruction
 * \param flag use to return the position of the instruction in the block:
 *              -1: error, 0: not found, 1: start, 2:middle/end
 * \return the tageted block if founded, else NULL
 */
static block_t* find_target_block(fct_t* f, int64_t add, int *flag)
{
   (*flag) = 0;

   FOREACH_INQUEUE(fct_get_blocks(f), it1) {
      block_t* bb = GET_DATA_T(block_t*, it1);

      if (INSN_GET_ADDR (block_get_first_insn(bb)) == add) {
         (*flag) = 1;
         return bb;
      }

      if ((INSN_GET_ADDR (block_get_last_insn(bb)) < add)
            && (add <= INSN_GET_ADDR(block_get_last_insn(bb)))) {
         (*flag) = 2;
         return bb;
      }
   }

   return NULL;
}

/*
 * Split a block and add it in the CFG
 * \param b_src a block to split
 * \param b_dst the block created by the splitting
 * \param address where to split the block
 * \return a list of CFG_node deleted by the splitting
 */
static block_t* split_block(block_t* b_src, int64_t address)
{
   int flag = 0;
   int64_t stop_save = INSN_GET_ADDR(block_get_last_insn(b_src));
   block_t* b_dst = NULL;

   // get all instructions of the new block
   // first find the instruction used as splitting point
   // then finish src_block and create dst_block
   // at last, put all instructions in the old block into dst_block
   FOREACH_INLIST(insn_get_sequence(block_get_first_insn(b_src)), it0) {
      insn_t* tmp = GET_DATA_T(insn_t*, it0);

      if (flag == 1)
         add_insn_to_block(tmp, b_dst);

      else if (INSN_GET_ADDR(tmp) == address && flag == 0) {
         b_dst = block_new(block_get_fct(b_src), tmp);
         b_src->end_sequence = list_getprev(it0);
         flag = 1;
         break;
      }

      if (INSN_GET_ADDR(tmp) == stop_save)
         break;
   }

   if (b_dst == NULL)
      return NULL;

   //removed old edges in the cfg and create new ones
   while (block_get_CFG_node(b_src)->out != NULL) {
      graph_edge_t* ed = block_get_CFG_node(b_src)->out->data;
      graph_add_edge(ed->from, block_get_CFG_node(b_dst), ed->data);
      graph_remove_edge(ed, NULL);
   }

   graph_add_uniq_edge(block_get_CFG_node(b_src), block_get_CFG_node(b_dst),
         NULL);

   DBGMSG("INFO: block %d has been splitted at %"PRIx64". New block: %d\n",
         block_get_id(b_src), address, block_get_id(b_dst));

   return b_dst;
}

/*
 * Solve an idirect branch located at the end of a given basic block
 * \param b a basic block ended by an idirect branch
 */
static void solve_bb(block_t* b)
{
   block_t* b_mem = NULL;
   list_t* insn_mem = NULL;

   int64_t add_reg = INSN_GET_ADDR(block_get_last_insn(b));
   oprnd_t *param = find_memory_componants(b, &insn_mem, &b_mem, add_reg);

   if (param == NULL) {
      DBGMSG(
            "INFO: no definition found for branch in block %d, at address 0x%"PRIx64"\n",
            block_get_id(b), INSN_GET_ADDR(block_get_last_insn(b)));
      return;
   }

   reg_t *base = oprnd_get_base(param);
   reg_t *index = oprnd_get_index(param);
   int scale = oprnd_get_scale(param);
   int64_t offset = oprnd_get_offset(param);

   if (base != NULL || offset == 0 || index == NULL) {
      DBGMSG(
            "INFO: definition has bad format for branch in block %d, at address 0x%"PRIx64"\n",
            block_get_id(b), INSN_GET_ADDR(block_get_last_insn (b)));
      return;
   }

   //find CMP <index>, imm in this block or blocks before (while there is only one path)
   add_reg = INSN_GET_ADDR((insn_t* ) insn_mem->data);
   int64_t imm_cmp = find_imm_cmp(b_mem, insn_mem, index, add_reg);

   //if not find
   if (imm_cmp == 0) {
      DBGMSG(
            "INFO: no CMP value found for branch in block %d, at address 0x%"PRIx64"\n",
            block_get_id(b), INSN_GET_ADDR(block_get_last_insn (b)));
      return;
   }

   //look into memory and add edges
   int i = 0;
   int cpt = 0;
   fct_t* f = block_get_fct(b);

   for (i = 0; i <= imm_cmp; i++) {
      int flag = 0;
      int64_t dst_add = find_from_memory(f, offset + i * scale, scale);
      block_t* dst_bb = find_target_block(f, dst_add, &flag);

      if (dst_bb != NULL && flag == 1) {
         if (graph_add_uniq_edge(block_get_CFG_node(b),
               block_get_CFG_node(dst_bb), NULL) == 1) {
            if (cpt == 0)
               cpt = 1;
            DBGMSG("attached block %d to %d\n", block_get_id(b),
                  block_get_id(dst_bb));
         }
      } else if (dst_bb != NULL && flag == 2) {
         if (cpt == 0)
            cpt = 1;
         dst_bb = split_block(b, dst_add);
      } else {
         cpt = -1;
         DBGMSG(
               "WARNING: no block found at address 0x%"PRIx64" for branch in block %d, at address 0x%"PRIx64"\n",
               dst_add, block_get_id(b), INSN_GET_ADDR(block_get_last_insn(b)));
      }
   }

   if (cpt == 1) {
      DBGMSG("INFO: indirect branch at 0x%"PRIx64" solved\n",
            INSN_GET_ADDR(block_get_last_insn(b)));
      insn_t* in = block_get_last_insn(b);
      in->annotate |= A_IBSOLVE;
   } else {
      DBGMSG("INFO: indirect branch at 0x%"PRIx64" not solved\n",
            INSN_GET_ADDR(block_get_last_insn(b)));
   }
}

/*
 * Solve indirect branches using CMP algorithm in a function
 * \param f a function
 */
void lcore_solve_using_cmp(fct_t* f)
{
   FOREACH_INQUEUE(fct_get_blocks(f), iter)
   {
      block_t* b = GET_DATA_T(block_t*, iter);

      if (is_indirect_block(b) == 1) {
         insn_t* in = block_get_last_insn(b);
         in->annotate |= A_IBNOTSOLVE;

         DBGMSG(
               "INFO: try to solve indirect branch in block %d, at address 0x%"PRIx64"\n",
               block_get_id(b), INSN_GET_ADDR(block_get_last_insn(b)));

         solve_bb(b);
      }
   }
}
