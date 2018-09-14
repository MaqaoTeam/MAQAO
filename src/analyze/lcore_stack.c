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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "libmcommon.h"
#include "libmasm.h"
#include "libmcore.h"

typedef struct st_key_s st_key_t;

/*
 * To store context local to a function
 */
struct st_cntxt_s {
   arch_t* arch; /**< Architecture */
   fct_t* f; /**< Current function */
   hashtable_t** local_stacks; /**<  */
   hashtable_t* stack; /**<  */
   queue_t* _to_track; /**< For internal use. Queue of reg_t */
};

/*
 * Used to store key in stack
 */
struct st_key_s {
   int64_t offset; /**< Offset in the stack */
   ssa_var_t* reg; /**< Register used in the memory address */
};

/*
 * Equal function on ssa_var_t  for hashtable
 * \param v1 a pointer on a st_key_t
 * \param v1 a pointer on a st_key_t
 * \return TRUE if v1 == v2, else FALSE
 */
int st_key_equal(const void *v1, const void *v2)
{
   st_key_t* k1 = (st_key_t*) v1;
   st_key_t* k2 = (st_key_t*) v2;

   if (k1 == k2)
      return (TRUE);
   else if (k1 == NULL || k2 == NULL)
      return (FALSE);
   else if ((k1->reg->index == k2->reg->index) && (k1->reg->reg == k2->reg->reg)
         && (k1->offset == k2->offset))
      return (TRUE);
   else
      return (FALSE);
}

/*
 * Hash function on st_key_t  for hashtable
 * \param v a pointer on a st_key_t
 * \param size current size of the hashtable
 * \return an hash of v
 */
hashtable_size_t st_key_hash(const void *v, hashtable_size_t size)
{
   st_key_t* k1 = (st_key_t*) v;

   if (k1 == NULL)
      return (0);
   return ((((unsigned long) k1->reg->reg) * 1000 + k1->reg->index + k1->offset)
         % size);
}

/*
 * Creates a key from an operand
 * \param ssain current instruction
 * \param pos position of the operand to convert
 * \return an initialized key
 */
static st_key_t* __oprnd_to_key(ssa_insn_t* ssain, int pos)
{
   st_key_t* key = lc_malloc(sizeof(st_key_t));

   key->offset = oprnd_get_offset(insn_get_oprnd(ssain->in, pos));
   key->reg = ssain->oprnds[pos * 2];

   return (key);
}

static void __stack_print_key(st_key_t* key, arch_t* arch)
{
   printf("0x%"PRIx64" (%s_%d)", key->offset,
         arch_get_reg_name(arch, key->reg->reg->type, key->reg->reg->name),
         key->reg->index);
}

/*
 * Initializes data for stack analysis
 * \param f current function
 * \return a pointer on an initialized st_cntxt structure
 */
static void* stack_init(fct_t* f, adfa_cntxt_t* useless)
{
   (void) useless;
   int i = 0;
   st_cntxt_t* cntxt = lc_malloc(sizeof(st_cntxt_t));
   cntxt->_to_track = queue_new();
   cntxt->arch = f->asmfile->arch;
   cntxt->stack = hashtable_new(direct_hash, direct_equal);
   cntxt->local_stacks = lc_malloc0(
         queue_length(f->blocks) * sizeof(hashtable_t*));
   cntxt->f = f;

   for (i = 0; i < queue_length(f->blocks); i++)
      cntxt->local_stacks[i] = hashtable_new(&st_key_hash, &st_key_equal);

   if (0) {
   }
   else {
      printf("Current architecture (%s) is not handled for stack analysis\n",
            cntxt->arch->name);
      return (NULL);
   }
   return (cntxt);
}

/*
 * Propagates results of current block analysis to its sucessors
 * \param pcntxt pointer on a st_cntxt structure containing current context
 * \param ssab current SSA basic block
 * \return the context
 */
static void* stack_propagate(void* pcntxt, ssa_block_t* ssab)
{
   block_t* b = ssab->block;
   st_cntxt_t* cntxt = (st_cntxt_t*) pcntxt;

   FOREACH_INHASHTABLE(cntxt->local_stacks[b->id], it_ls) {
      st_key_t* key = GET_KEY(key, it_ls);
      adfa_val_t* val = GET_DATA_T(adfa_val_t*, it_ls);

      FOREACH_INLIST(b->cfg_node->out, it_succ)
      {
         graph_edge_t* ed = GET_DATA_T(graph_edge_t*, it_succ);
         block_t* succ = ed->to->data;

         if (succ
               != b && hashtable_lookup (cntxt->local_stacks[succ->id], key) == NULL) {
            st_key_t* nkey = lc_malloc(sizeof(st_key_t));
            nkey = memcpy(nkey, key, sizeof(st_key_t));
            hashtable_insert(cntxt->local_stacks[succ->id], nkey, val);
         }
      }
   }
   return (cntxt);
}

/*
 * Checks if an instruction should be analyzed or not
 * \param ssain current SSA instruction
 * \param pcntxt pointer on a st_cntxt structure containing current context
 * \return TRUE if the instruction should be analyzed, else FALSE
 */
static int stack_insn_filter(ssa_insn_t* ssain, void* pcntxt)
{
   if (ssain->in == NULL)
      return (FALSE);
   int i = 0;
   st_cntxt_t* cntxt = (st_cntxt_t*) pcntxt;

   for (i = 0; i < insn_get_nb_oprnds(ssain->in); i++) {
      oprnd_t* op = insn_get_oprnd(ssain->in, i);

      if (oprnd_is_mem(op)
            == TRUE && queue_lookup (cntxt->_to_track, direct_equal, oprnd_get_base (op)) != NULL) {
         return (TRUE);
      } else if (oprnd_is_reg(op)
            == TRUE && queue_lookup (cntxt->_to_track, direct_equal, oprnd_get_reg (op)) != NULL) {
         return (TRUE);
      }
   }
   return (FALSE);
}

static void stack_insn_execute(ssa_insn_t* ssain, adfa_val_t* result,
      hashtable_t* Rvals, void* pcntxt)
{
   (void) Rvals;
   if (ssain->in == NULL)
      return;
   int i = 0;
   st_cntxt_t* cntxt = (st_cntxt_t*) pcntxt;
   block_t* b = ssain->ssab->block;

   // Iterate over operands
   for (i = 0; i < insn_get_nb_oprnds(ssain->in); i++) {
      oprnd_t* op = insn_get_oprnd(ssain->in, i);

      // If memory operand using a tracked register
      if (oprnd_is_mem(op)
            == TRUE && queue_lookup (cntxt->_to_track, direct_equal, oprnd_get_base (op)) != NULL) {
         char to_free = TRUE;
         st_key_t* key = __oprnd_to_key(ssain, i);
         adfa_val_t* val = NULL;

         // If the memory operand is used as source operand
         if (oprnd_is_src(op)) {
            val = hashtable_lookup(cntxt->local_stacks[b->id], key);
            if (val != NULL) {
               printf("0x%"PRIx64"  Load from ", ssain->in->address);
               __stack_print_key(key, cntxt->arch);
               printf(" :: ");
               stack_print_val(val, cntxt->arch);
               printf("\n");
            } else {
               val = result;
               if (val != NULL) {
                  printf("0x%"PRIx64"  Load from ", ssain->in->address);
                  __stack_print_key(key, cntxt->arch);
                  printf(" :: ");
                  stack_print_val(val, cntxt->arch);
                  printf("\n");
               }
            }
         }

         // If the memory operand is used as destination operand
         if (oprnd_is_dst(op)) {
            val = result;
            if (val != NULL) {
               to_free = FALSE;
               hashtable_insert(cntxt->local_stacks[b->id], key, val);
               printf("0x%"PRIx64"  Store into ", ssain->in->address);
               __stack_print_key(key, cntxt->arch);
               printf(" :: ");
               stack_print_val(val, cntxt->arch);
               printf("\n");
            }
         }

         if (to_free)
            lc_free(key);
      }
      // If register operand using a tracked register
      else if (oprnd_is_reg(op)
            == TRUE && queue_lookup (cntxt->_to_track, direct_equal, oprnd_get_reg (op)) != NULL) {
         adfa_val_t* val = result;
         if (val != NULL) {
            printf("0x%"PRIx64"  %s_%d == ", ssain->in->address,
                  arch_get_reg_name(cntxt->arch,
                        ssain->oprnds[2 * i]->reg->type,
                        ssain->oprnds[2 * i]->reg->name),
                  ssain->oprnds[2 * i]->index);
            stack_print_val(val, cntxt->arch);
            printf("\n");
         }
      }
   }
}

/*
 * Analyzes stack for current function
 * \param f a function
 * \return context used for this analysis.
 */
st_cntxt_t* lcore_fct_analyze_stack_(fct_t* f)
{
   if (f == NULL) {
      ERRMSG("Stack: Input function is NULL");
      return NULL;
   }
   printf("***** Analyzing function %s\n", fct_get_name(f));

   adfa_driver_t driver;
   driver.init = &stack_init;
   driver.insn_execute = &stack_insn_execute;
   driver.insn_filter = &stack_insn_filter;
   driver.propagate = &stack_propagate;
   driver.flags = 0;

   // Compute groups
   adfa_cntxt_t* adfa = ADFA_analyze_function(f, &driver);
   ADFA_free(adfa);
   st_cntxt_t* cntxt = driver.user_struct;
   return (cntxt);
}

/*
 * Frees a context created by lcore_fct_analyze_stack_
 * \param cntxt a context to free
 */
void lcore_free_cntxt(st_cntxt_t* cntxt)
{
   if (cntxt == NULL)
      return;
   int i = 0;
   queue_free(cntxt->_to_track, NULL);
   hashtable_free(cntxt->stack, NULL, NULL);
   for (i = 0; i < queue_length(cntxt->f->blocks); i++)
      hashtable_free(cntxt->local_stacks[i], NULL, lc_free);
   lc_free(cntxt->local_stacks);
   lc_free(cntxt);
}

/*
 * Prints a adfa_val_t structure
 * \param val structure to print
 * \param arch current architecture
 */
void stack_print_val(adfa_val_t* val, arch_t* arch)
{
   if (val == NULL)
      return;

   if (val->is_mem)
      printf("@[");

   switch (val->type) {
   case ADFA_TYPE_IMM:
      printf("0x%"PRIx64, val->data.imm);
      break;
   case ADFA_TYPE_REG:
      printf("%s_%d",
            arch_get_reg_name(arch, val->data.reg->reg->type,
                  val->data.reg->reg->name), val->data.reg->index);
      break;
   case ADFA_TYPE_SONS:
      printf("(");
      // print left
      stack_print_val(val->data.sons[0], arch);

      // print operator
      switch (val->op) {
      case ADFA_OP_ADD:
         printf(" + ");
         break;
      case ADFA_OP_SUB:
         printf(" - ");
         break;
      case ADFA_OP_MUL:
         printf(" * ");
         break;
      case ADFA_OP_DIV:
         printf(" / ");
         break;
      case ADFA_OP_SL:
         printf(" << ");
         break;
      case ADFA_OP_SR:
         printf(" >> ");
         break;
      }

      // print rigth
      stack_print_val(val->data.sons[1], arch);
      printf(")");
      break;
   }
   if (val->is_mem)
      printf("]");
}

/*
 * Gets the value associated to an instruction
 * \param cntxt context retrived by lcore_function_analyse_stack
 * \param insn an instruction accessing to the stack
 * \param an adfa_val_t structure, else NULL
 */
adfa_val_t* insn_get_accessed_stack(st_cntxt_t* cntxt, insn_t* in)
{
   if (cntxt == NULL || in == NULL)
      return NULL;
   adfa_val_t* val = hashtable_lookup(cntxt->stack, in);

   return (val);
}
