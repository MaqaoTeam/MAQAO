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

#include <inttypes.h>
#include <limits.h>
#include "libmasm.h"

/**
 * \file
 * */

#define POS_BASE 0
#define POS_INDEX 1

#define WHILE_INSN_INBLOCK(in,it,s,p) list_t* it=in->sequence;\
	 for (;\
	    it != NULL && ((((insn_t*)it->data)->block == in->block && p == 1) \
	                 ||(((insn_t*)it->data)->address < s && p == 2)); it = it->next)

///////////////////////////////////////////////////////////////////////////////
//                          Compute group unrolling                          //
///////////////////////////////////////////////////////////////////////////////
/*
 * Computes the Greatest common divisor
 * \param a an integer greater than 0
 * \param b an integer greater than 0
 * \return the GCD of a and b
 */
static int gcd(int a, int b)
{
   if (a <= 0 || b <= 0)
      return 0;

   if (a < b) {
      int tmp = a;
      a = b;
      b = tmp;
   }

   int r = a % b;
   if (r == 0)
      return b;
   else
      return gcd(b, r);
}

/*
 * Computes the unroll factor for a group
 * \param group a group to analyse
 * \param mode used to filter groups
 */
static void group_compute_unroll(group_t* group, void* mode)
{
   // Group successive elements into an array, according to their access type
   // (L / S) and their opcode. sub_size array contains for each subgroup, the
   // number of successive group elements.
   // Once sub_size is computed, the  gcd of all its element is computed and
   // used as unroll value.

   int size = group_get_size(group, mode);
   int i = 0;
   int nb_sub = 0;
   int current_pattern = -1;
   int current_opcode = -1;
   int current_size = 0;
   int current_diff = 0;
   int val1 = 0;
   int gcdval = 0;
   int* sub_size = NULL;
   group_elem_t** data = NULL;

   if (size <= 0)
      return;
   else if (size == 1) {
      group->unroll_factor = 1;
      return;
   }
   data = lc_malloc(sizeof(group_elem_t*) * size);
   sub_size = lc_malloc(sizeof(int) * size);

   // extracts the sub group using the mode -----------------------------------
   FOREACH_INQUEUE(group->gdat, it0) {
      group_elem_t* gdat = GET_DATA_T(group_elem_t*, it0);
      if (group->filter_fct != NULL && group->filter_fct(gdat, mode) == 1)
         data[i++] = gdat;
   }

   // computes subgroups ------------------------------------------------------
   for (i = 0; i < size; i++) {
      if (current_pattern == -1) {
         current_pattern = data[i]->code;
         current_opcode = data[0]->insn->opcode;
         current_size = 1;
         current_diff = oprnd_get_offset(
               insn_get_oprnd(data[1]->insn, data[1]->pos_param))
               - oprnd_get_offset(
                     insn_get_oprnd(data[0]->insn, data[0]->pos_param));
      } else if (current_pattern != data[i]->code
            || current_opcode != data[i]->insn->opcode
            || current_diff
                  != oprnd_get_offset(
                        insn_get_oprnd(data[i]->insn, data[i]->pos_param))
                        - oprnd_get_offset(
                              insn_get_oprnd(data[i - 1]->insn,
                                    data[i - 1]->pos_param))) {
         sub_size[nb_sub++] = current_size;
         current_size = 1;
         current_pattern = data[i]->code;
         current_opcode = data[i]->insn->opcode;
      } else
         current_size = current_size + 1;
   }
   sub_size[nb_sub++] = current_size;

   // analyses subgroups ------------------------------------------------------
   for (i = 0; i < nb_sub; i++) {
      if (val1 == 0)
         val1 = sub_size[0];
      else {
         gcdval = gcd(sub_size[i], val1);
         if (gcdval < sub_size[i])
            val1 = gcdval;
         else
            val1 = sub_size[i];
      }
   }
   group->unroll_factor = val1;
   lc_free(data);
   lc_free(sub_size);
}

///////////////////////////////////////////////////////////////////////////////
//                            Compute group stride                           //
///////////////////////////////////////////////////////////////////////////////
/*
 * Gets the memory operand of the first group element
 * \param g a group
 * \return the first memory operand of the first instruction of the group, or
 *         NULL if there is an error
 */
static oprnd_t* get_first_oprnd_grom_group(group_t* g)
{
   if (g == NULL)
      return (NULL);

   FOREACH_INQUEUE(g->gdat, it) {
      group_elem_t* gd = GET_DATA_T(group_elem_t*, it);
      return (insn_get_oprnd(gd->insn, gd->pos_param));
   }
   return (NULL);
}

/*
 * Gets the register used in a memory address
 * \param in an instruction
 * \param pos_op position of the operand in the instruction
 * \param pos_reg position of the register in the operand: base register
 *        (POS_BASE) or index register (POS_INDEX)
 * \return a register or NULL
 */
static reg_t* get_reg_from_memory(insn_t* in, int pos_op, char pos_reg)
{
   if (in == NULL)
      return (NULL);
   oprnd_t* op = insn_get_oprnd(in, pos_op);

   if (pos_reg == POS_BASE)
      return (oprnd_get_base(op));
   else if (pos_reg == POS_INDEX)
      return (oprnd_get_index(op));
   else
      return (NULL);
}

/* Functions passed to interpret_dyadic */
static int _add(int a, int64_t b)
{
   return a + b;
}
static int _sub(int a, int64_t b)
{
   return a - b;
}
static int _mul(int a, int64_t b)
{
   return a * b;
}
static int _div(int a, int64_t b)
{
   return a / b;
}

/*
 * Interprets an instruction performing a dyadic operation (using two operands)
 * \param f a pointer to the corresponding function
 * \return boolean TRUE if must break inside the while loop and FALSE otherwise
 */
static int interpret_dyadic(int (*f)(int, int64_t), insn_t* in, reg_t *reg,
      queue_t *q, int **res, int **err, char *stop)
{
   if ((insn_get_nb_oprnds(in) == 2)
         && (oprnd_is_reg(insn_get_oprnd(in, 1)) == TRUE)
         && (oprnd_get_reg(insn_get_oprnd(in, 1)) == reg)) {
      queue_add_tail(q, in);
      if (oprnd_is_imm(insn_get_oprnd(in, 0)) == TRUE) {
         **res = f(**res, oprnd_get_imm(insn_get_oprnd(in, 0)));
         **err = SS_OK;
      } else {
         **res = 0;
         **err = SS_VV;
         *stop = 1;
         return TRUE;
      }
   }
   return FALSE;
}

/*
 * Finds the stride for a group
 * \param reg a register used to select the needed element
 * \param last_in the last instruction in the group
 * \param group current group
 * \param err used to return an error flag
 * \param res used to return the stride
 * \return a queue containing instructions used to compute the stride
 */
static queue_t* find_stride(reg_t* reg, insn_t* last_in, group_t* group,
      int* err, int* res)
{
   insn_t* first_in = INSN_GET_NEXT(last_in);
   int i = 0;
   *err = SS_O;
   char stop = 0;
   int pass = 1;
   queue_t* q = queue_new();

   // This function traverses to block from the last group instruction to the end
   // to find instructions modifing the given register. 
   // If the instruction uses an immediate value to modifie the register, this 
   // immediate value is saved, else the function returns 0 and puts SS_VV (for
   // Stride Status Variable Value) in the error flag.
   while (1) {
      WHILE_INSN_INBLOCK(first_in, it,
            INSN_GET_ADDR(((group_elem_t*)queue_peek_head (group->gdat))->insn),
            pass) {
         insn_t* in = GET_DATA_T(insn_t*, it);
         unsigned short family = insn_get_family(in);

         // Interprets ADD, SUB, MUL and DIV
         if (family == FM_ADD) {
            if (interpret_dyadic(_add, in, reg, q, &res, &err, &stop) == TRUE)
               break;
         } else if (family == FM_SUB) {
            if (interpret_dyadic(_sub, in, reg, q, &res, &err, &stop) == TRUE)
               break;
         } else if (family == FM_MUL) {
            if (interpret_dyadic(_mul, in, reg, q, &res, &err, &stop) == TRUE)
               break;
         } else if (family == FM_DIV) {
            if (interpret_dyadic(_div, in, reg, q, &res, &err, &stop) == TRUE)
               break;
         }

         // Interprets INC
         else if (family == FM_INC && (insn_get_nb_oprnds(in) == 1)
               && (oprnd_is_reg(insn_get_oprnd(in, 0)) == TRUE)
               && (oprnd_get_reg(insn_get_oprnd(in, 0)) == reg)) {
            //printf ("insn @0x%"PRIx64"\n", INSN_GET_ADDR (in));
            queue_add_tail(q, in);
            *res += 1;
            *err = SS_OK;
         }
         // Interprets others
         else {
            for (i = 0; i < insn_get_nb_oprnds(in); i++) {
               if (oprnd_is_dst(insn_get_oprnd(in, i))
                     == TRUE && (oprnd_is_reg(insn_get_oprnd(in, i)) == TRUE)
                     && (oprnd_get_reg(insn_get_oprnd(in, i)) == reg)
                     && family == FM_CMP) {
                  //printf ("insn @0x%"PRIx64"\n", INSN_GET_ADDR (in));
                  queue_add_tail(q, in);
                  *res = 0;
                  *err = SS_VV;
                  stop = 1;
                  break;
               }
            }
            if (*err == SS_VV) {
               stop = 1;
               break;
            }
         }
      }

      first_in = ((block_t*) group->loop->entries->data)->begin_sequence->data;
      pass++;
      if (stop == 1)
         break;
      stop = 1;
   }
   return (q);
}

/*
 * Computes the group increment, in bytes.
 * \param group a group to analyze
 */
void lcore_group_stride_group(group_t* group)
{
   if (group == NULL)
      return;

   // check the number of blocks in the loop
   if (loop_get_nb_blocks(group->loop) == 1) {
      oprnd_t* op = get_first_oprnd_grom_group(group);

      //if (offset<RIP>)
      if ((reg_get_type(oprnd_get_base(op)) == RIP_TYPE)
            && (oprnd_get_index(op) == NULL)) {
         //return the constant value
         group->s_status = SS_RIP;
      } else {
         queue_t* qbase = NULL;
         queue_t* qindex = NULL;
         int err = SS_NA;
         int err1 = SS_NA;

         // get the instruction of the group to know where to begin the iteration
         insn_t* last_in = ((group_elem_t*) queue_peek_tail(group->gdat))->insn;

         // get the index register used in this instruction
         reg_t* reg = get_reg_from_memory(last_in,
               ((group_elem_t*) queue_peek_tail(group->gdat))->pos_param,
               POS_INDEX);
         int res = 0;

         // if the register is not null, run the analysis on it
         if (reg != NULL) {
            qbase = find_stride(reg, last_in, group, &err, &res);
            res = res * ((oprnd_get_scale(op) == 0) ? 1 : oprnd_get_scale(op));
         }
         // get the base register used in this instruction
         reg = get_reg_from_memory(last_in,
               ((group_elem_t*) queue_peek_tail(group->gdat))->pos_param,
               POS_BASE);
         // if the analysis on the index failed and if the scale is equal to one, run the analysis on
         // the base register
         if (reg != NULL && res == 0)
            qindex = find_stride(reg, last_in, group, &err1, &res);

         if (err == SS_O && (err1 == SS_O || err1 == SS_NA))
            err = SS_OK;
         if (err1 == SS_OK) {
            group->s_status = err1;
            group->stride_insns = qindex;
            if (qbase != NULL)
               queue_free(qbase, NULL);
         } else if (err != SS_O) {
            group->s_status = err;
            group->stride_insns = qbase;
            if (qindex != NULL)
               queue_free(qindex, NULL);
         } else {
            group->s_status = err1;
            group->stride_insns = qindex;
            if (qbase != NULL)
               queue_free(qbase, NULL);
         }
         group->stride = res;
      }
   }
   // the loop has more than one block, so all its groups
   // are tagged with SS_MB (for Stride Status Multiple Blocks)
   else {
      group->s_status = SS_MB;
   }
}

/*
 * Computes the group increment, in bytes, in all groups in the function.
 * \param function a function whose groups must be analyzed.
 * \warning Groups had to been computed before to call this function
 */
void lcore_group_stride(fct_t* function)
{
   if (function == NULL) {
      printf("ERROR: The given function is NULL\n");
      return;
   }

   // check if work already done once
   FOREACH_INQUEUE(fct_get_loops(function), it_l0) {
      loop_t* loop = GET_DATA_T(loop_t*, it_l0);
      list_t* groups = loop_get_groups(loop);

      FOREACH_INLIST(groups, it_g)
      {
         group_t* g = GET_DATA_T(group_t*, it_g);
         if (g->s_status != SS_NA)
            return;
      }
   }

   // This function try to compute the stride in groups for each group in innermost and mono block loops.
   // It caracterize the format of the memory access:
   //     - offset (RIP)
   //     - offset (base, index, scale)
   // In the first case, it does nothing
   // else, it try to find the immediate value that modify index. If this value is not found and
   // scale is equal to 1, then the immediate value modifing base is looked for.
   FOREACH_INQUEUE(fct_get_loops(function), it_l)
   {
      loop_t* loop = GET_DATA_T(loop_t*, it_l);
      list_t* groups = loop_get_groups(loop);

      // check if groups are present
      if (groups != NULL) {
         // for each group, caracterize it and run the corresponding analysis
         // => RIP based : nothing
         // => else, detection of value used to modified index / base register
         FOREACH_INLIST(groups, it_g)
         {
            group_t* group = GET_DATA_T(group_t*, it_g);
            lcore_group_stride_group(group);
         }
      }
   }
}

///////////////////////////////////////////////////////////////////////////////
//                            Analysis functions                             //
///////////////////////////////////////////////////////////////////////////////
/*
 *
 * \param group a group to analyze
 * \param mem
 * \param size size of the group
 * \param min
 */
void _compute_touched_sets(group_t* group, char* mem, int size, int min)
{
   int i = 0;                                   //iterator
   int n = 0;                                   //index of current set
   int start = 0;                               //start of current set
   int* sets = lc_malloc(2 * sizeof(int));    //sets

   DBGMSG("size: %d, min = %d\n", size, min);
   for (i = 0; i < size - 1; i++) {
      // means start of set
      if (mem[i] == 0 && mem[i + 1] == 1) {
         start = i + 1;
      }
      // means end of set
      else if (mem[i] == 1 && mem[i + 1] == 0) {
         sets = lc_realloc(sets, (n + 2) * 2 * sizeof(int));
         sets[2 * n] = start + min;
         sets[2 * n + 1] = i + min + 1;
         DBGMSG("%d: [%d, %d]\n", n, sets[2 * n], sets[2 * n + 1]);
         n++;
      }
   }
   sets[2 * n] = start + min;
   sets[2 * n + 1] = size + min;
   DBGMSG("%d: [%d, %d]\n", n, sets[2 * n], sets[2 * n + 1]);

   group->touched_sets = sets;
   group->nb_sets = n + 1;
}

/*
 * Computes the memory accessed by a group
 * \param group a group to analyze.
 * \param user A user value given to the filter function:
 *    - 0: no filter
 *    - 1: only SSE
 *    - 2: only AVX
 *    - 3: only SSE and AVX
 */
void lcore_group_memory_group(group_t* group, void* user)
{
   if (group == NULL)
      return;
   int l_size = 0;

   //Computes the unroll factor --------------------------------------
   group_compute_unroll(group, user);

   //Computes all loaded bytes ---------------------------------------
   FOREACH_INQUEUE(group->gdat, it_d0) {
      group_elem_t* gdat = GET_DATA_T(group_elem_t*, it_d0);
      if (group->filter_fct != NULL && group->filter_fct(gdat, user) == 1)
         group->memory_all += oprnd_get_size_value(
               insn_get_oprnd(gdat->insn, gdat->pos_param)) / 8;
   }

   //Computes min / max ----------------------------------------------
   int min = INT_MAX;
   int max = INT_MIN;
   FOREACH_INQUEUE(group->gdat, it_d) {
      group_elem_t* gdat = GET_DATA_T(group_elem_t*, it_d);
      if (group->filter_fct != NULL && group->filter_fct(gdat, user) == 1) {
         int off = oprnd_get_offset(
               insn_get_oprnd(gdat->insn, gdat->pos_param));
         int loaded = oprnd_get_size_value(
               insn_get_oprnd(gdat->insn, gdat->pos_param)) / 8;
         if (min > off)
            min = off;
         if (max < off + loaded)
            max = off + loaded;
      }
   }

   l_size = max - min;
   if (l_size < 0)
      l_size = -l_size;
   group->span = l_size;

   // if the stride is greater than the span, bits can't overlapped
   // so computation is much simpler
   if (((group->stride > 0) ? group->stride : -group->stride) > l_size) {
      group->memory_nover = group->memory_all;
      group->memory_overl = 0;
      group->m_status = MS_OK;
   } else {
      int i = 0;
      int stride = ((group->stride > 0) ? group->stride : -group->stride);

      //Compute no overlapped bytes
      char* mem = lc_malloc0((l_size + 1) * sizeof(char));
      char* mem1 = lc_malloc0((l_size + 1 + 2 * stride) * sizeof(char));

      int head = 0;
      FOREACH_INQUEUE(group->gdat, it_d1) {
         group_elem_t* gdat = GET_DATA_T(group_elem_t*, it_d1);
         if (group->filter_fct != NULL && group->filter_fct(gdat, user) == 1) {
            int off = oprnd_get_offset(
                  insn_get_oprnd(gdat->insn, gdat->pos_param));
            int loaded = oprnd_get_size_value(
                  insn_get_oprnd(gdat->insn, gdat->pos_param)) / 8;
            int i_0 = off - min;

            i_0 = ((i_0 < 0) ? -i_0 : i_0);   // compute the absolute value

            for (i = 0; i < loaded; i++)
               mem[i + i_0] = 1;

            if (group->stride != 0) {
               for (i = 0; i < loaded; i++)
                  mem1[i + i_0 + 2 * stride] = 1;
            }
         }
      }
      int cpt = 0;
      for (i = 0; i < l_size; i++)
         if (mem[i] != 0)
            cpt++;

      if (group->stride != 0) {
         for (i = 0; i < l_size + 2 * stride; i++) {
            if (i < stride && mem1[i] == 1)
               head++;
            else if (i >= l_size + stride && mem1[i] == 1)
               head++;
            else if (i - stride >= 0 && i - stride <= l_size
                  && mem[i - stride] == 0 && mem1[i] == 1)
               head++;
            else if (i - stride >= 0 && mem1[i] == 1)
               head++;
         }
         group->head = head;
      }

      // Compute touched sets ----------------------------------------
      _compute_touched_sets(group, mem, l_size, min);

      lc_free(mem);
      lc_free(mem1);
      group->memory_nover = ((cpt < 0) ? -cpt : cpt);

      //compute overlapping ------------------------------------------
      group->memory_overl = group->memory_all - group->memory_nover;
      group->m_status = MS_OK;
   }
}

/*
 * Computes the memory accessed by all groups in a function
 * \param function a function whose groups must be analyzed.
 * \param user A user value given to the filter function:
 *    - 0: no filter
 *    - 1: only SSE
 *    - 2: only AVX
 *    - 3: only SSE and AVX
 * \warning Groups had to been computed before to call this function
 */
void lcore_group_memory(fct_t* function, void* user)
{
   if (function == NULL) {
      printf("ERROR: The given function is NULL\n");
      return;
   }

   // check if work already done once
   FOREACH_INQUEUE(fct_get_loops(function), it_l0) {
      loop_t* loop = GET_DATA_T(loop_t*, it_l0);
      list_t* groups = loop_get_groups(loop);

      FOREACH_INLIST(groups, it_g)
      {
         group_t* g = GET_DATA_T(group_t*, it_g);
         if (g->m_status != MS_NA)
            return;
      }
   }

   // analyze memory
   FOREACH_INQUEUE(fct_get_loops(function), it_l)
   {
      loop_t* loop = GET_DATA_T(loop_t*, it_l);
      list_t* groups = loop_get_groups(loop);

      if (groups != NULL) {
         FOREACH_INLIST(groups, it_g)
         {
            group_t* group = GET_DATA_T(group_t*, it_g);
            lcore_group_memory_group(group, user);
         }
      }
   }
}
