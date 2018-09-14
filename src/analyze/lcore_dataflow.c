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

/**
 * \section adfa_intro Introduction
 * Advanced Data Flow Analysis (ADFA) is a static analysis used to find
 * statically registers value, based on a SSA representation of the input
 * assembly code.
 * The analysis can be customized using a driver to perform more analyses,
 * such as what is doing for grouping analysis.
 *
 *
 * \section adfa_working How does it work ?
 *
 * \subsection adfa_working_driver The driver
 * ADFA uses a system of driver to customize the analysis. Currently, the driver
 * defined four functions called at different steps of the analysis, with
 * different parameters:
 *  - void* (*init) (fct_t*);
 *  - int   (*insn_filter) (ssa_insn_t*, void*);
 *  - void  (*insn_execute) (ssa_insn_t*, adfa_val_t*, hashtable_t*, void*);
 *  - void* (*propagate) (void*, ssa_block_t*);
 * More details on the driver are available for structure \ref adfa_driver_s
 *
 * \subsection adfa_working_algorithm Algorithm
 \code
 Input: fct_t F;   // F is the function to analyze

 // USER is a structure defined by the user and initialized trough the
 // init function of the driver
 USER = driver_init (F)

 // find_block_to_compute is a function looking for blocks to compute.
 // It returns NULL if there is no block to compute.
 // Block ordering will be described in subsection \ref adfa_working_block_ordering.
 while ((block_t B = find_block_to_compute (F)) != NULL)
 {
 mark_block_as_computed (B)
 ssa_block_t SSAB = get_ssa_block (B)

 // Iterate over all instructions of the current basic block
 for each ssa instruction SSAIN in SSAB
 {
 insn_t IN = get_instruction_from_ssa_instruction (SSAIN)

 // IN can be NULL if current instruction is a phi-function (SSA concept)
 // Use also the driver insn_filter function
 if (IN != NULL
 && driver_insn_filter(SSAIN, USER))
 {
 // Analyze the instruction
 adfa_val_t RESULT = analyze_insn(SSAIN);

 // Call the driver insn_execute function
 driver_insn_execute (SSAIN, RESULTS, USER)
 }
 }
 // Call the driver propagate function
 driver_propagate (SSAB, USER)
 }
 \endcode
 *
 * \subsection adfa_working_block_ordering Block Ordering
 * A block can be analyzed when all its predecessors have been computed.
 * As a function can contains loops, loop backedges are ignored when predecessors
 * are checked.
 *
 * \subsections adfa_working_data_structure Data Structures
 * The main goal of ADFA is to compute the value of registers using the SSA form.
 * To represent values, the structure \ref adfa_struct_s has been defined.
 * Each value is an implicit binary tree, whose type (leaf or intern node) is
 * defined by the value of member type. When the element is an internal node,
 * the member op is set to a value representing the operation between the two sons.
 * When a value is computed for a SSA register, it is saved in a hashtable,
 * indexed by the SSA variable structure.
 *
 * \subsection adfa_working_instruction_analysis Instruction Analysis
 * To analyze an instruction, each input operand is analyzed, then a new adfa_val_t
 * structure is created to store the output value. This value is passed to the
 * driver insn_execute function.
 */

/*
 * \struct adfa_cntxt_s
 * Stores several variables used to analyze the input function
 */
struct adfa_cntxt_s {
   queue_t* Avals; /**< All computed adfa_val_t structures*/
   hashtable_t* Rvals; /**< computed adfa_val_t structures. Key is a ssa_var_t, value a adfa_val_t */
   ssa_block_t** ssa; /**< Result of SSA computation */
   arch_t* arch; /**< Architecture */
   fct_t* f; /**< Current function */
   graph_node_t* graph; /**< CFG entry point */
   adfa_driver_t* driver; /**< Input driver*/

   char* _traversed; /**< For internal use. Array of flag (one per block id) */
   queue_t* _to_compute; /**< For internal use. Queue of block_t */
};

/*
 * Prints an adfa_val_t structure
 * \param val an adfa_val_t structure to print
 * \param arch current architecture
 */
void adfa_print_val(adfa_val_t* val, arch_t* arch)
{
   if (val == NULL || arch == NULL)
      return;

   if (val->op == ADFA_OP_SQRT)
      printf("SQRT (");

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
      if (val->data.sons[0])
         adfa_print_val(val->data.sons[0], arch);

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
      if (val->data.sons[1])
         adfa_print_val(val->data.sons[1], arch);
      printf(")");
      break;

   case ADFA_TYPE_MEM_MTL:
      printf("0x%"PRIx64, val->data.imm);
      break;
   }
   if (val->is_mem)
      printf("]");
   if (val->op == ADFA_OP_SQRT)
      printf(")");
}

/**
 * Frees an existing adfa_val_t structure
 * \param pval a pointer on an existing adfa_val_t structure to free
 */
static void adfa_free_val(void* pval)
{
   adfa_val_t* val = (adfa_val_t*) pval;
   if (val == NULL)
      return;
   lc_free(val);
}

/*
 * Creates a adfa_val_t structure from a given register
 * \param ssaop register used to create the structure
 * \param ssain an instruction
 * \paran cntxt current context
 * \return an initialized adfa_val_t structure, else NULL
 */
static adfa_val_t* __register_to_val(ssa_var_t* ssaop, ssa_insn_t* ssain,
      adfa_cntxt_t* cntxt)
{
   adfa_val_t* val = NULL;
   adfa_val_t* Rval = NULL;

   if (ssaop == NULL)
      return (NULL);
   if (ssaop != NULL)
      Rval = hashtable_lookup(cntxt->Rvals, ssaop);

   if (Rval != NULL)
      val = Rval;
   else {
      // Special case if the register is RIP
      if (ssaop->reg == cntxt->arch->reg_rip) {
         insn_t* next_in = INSN_GET_NEXT(ssain->in);
         val = lc_malloc(sizeof(adfa_val_t));
         queue_add_tail(cntxt->Avals, val);
         val->is_mem = FALSE;
         val->op = ADFA_OP_NULL;
         val->type = ADFA_TYPE_IMM;
         val->data.imm = INSN_GET_ADDR(next_in);
      } else if (ssaop->insn == NULL) {
         val = lc_malloc(sizeof(adfa_val_t));
         queue_add_tail(cntxt->Avals, val);
         val->is_mem = FALSE;
         val->op = ADFA_OP_NULL;
         val->type = ADFA_TYPE_REG;
         val->data.reg = ssaop;
         if (hashtable_lookup(cntxt->Rvals, ssaop) == NULL)
            hashtable_insert(cntxt->Rvals, ssaop, val);
      } else {
         val = ADFA_analyze_insn(ssaop->insn, cntxt);
         if (hashtable_lookup(cntxt->Rvals, ssaop) == NULL)
            hashtable_insert(cntxt->Rvals, ssaop, val);
      }
   }
   return (val);
}

/**
 * Creates a adfa_val_t structure from a given operand
 * \param ssain an instruction
 * \param pos position of the source operand
 * \param cntxt current context
 * \return an initialized adfa_val_t structure, else NULL
 */
static adfa_val_t* __oprnd_to_val(ssa_insn_t* ssain, int pos,
      adfa_cntxt_t* cntxt)
{
   adfa_val_t* val = NULL;
   insn_t* in = ssain->in;
   oprnd_t* op = insn_get_oprnd(in, pos);

   switch (oprnd_get_type(op)) {
   // Operand is an immediate value => create the corresponding structure
   case OT_IMMEDIATE:
      val = lc_malloc(sizeof(adfa_val_t));
      queue_add_tail(cntxt->Avals, val);
      val->is_mem = FALSE;
      val->op = ADFA_OP_NULL;
      val->type = ADFA_TYPE_IMM;
      val->data.imm = oprnd_get_imm(op);
      break;

      // Operand is a register: Get the associated value from the hashtable
      // If value found, return it
      // Else,
      //    If the operand is linked to another instruction, analyze this instruction and
      //       return the result
      //    Else, create a structure for the register and return it
   case OT_REGISTER:
   case OT_REGISTER_INDEXED: {
      val = __register_to_val(ssain->oprnds[2 * pos], ssain, cntxt);
      break;
   }

      // Operand is a memory:
      // Generate the structure according to not NULL members in the memory operand
   case OT_MEMORY:
   case OT_MEMORY_RELATIVE: {
      if ((cntxt->driver->flags & ADFA_NO_MEMORY)
            && insn_get_family(ssain->in) != FM_LEA) {
         val = lc_malloc(sizeof(adfa_val_t));
         queue_add_tail(cntxt->Avals, val);
         val->is_mem = TRUE;
         val->op = ADFA_OP_NULL;
         val->type = ADFA_TYPE_MEM_MTL;
         val->data.imm = INSN_GET_ADDR(ssain->in);
         break;
      }

      adfa_val_t* Rvalb = __register_to_val(ssain->oprnds[2 * pos], ssain,
            cntxt);
      adfa_val_t* Rvali = __register_to_val(ssain->oprnds[2 * pos + 1], ssain,
            cntxt);
      int scale = oprnd_get_scale(insn_get_oprnd(ssain->in, pos));

      adfa_val_t* valOff = lc_malloc(sizeof(adfa_val_t));
      queue_add_tail(cntxt->Avals, valOff);
      valOff->is_mem = FALSE;
      valOff->op = ADFA_OP_NULL;
      valOff->type = ADFA_TYPE_IMM;
      valOff->data.imm = oprnd_get_offset(insn_get_oprnd(ssain->in, pos));

      adfa_val_t* valright = NULL;
      // Index register is not NULL
      if (Rvali != NULL) {
         // rigth = index * scale
         adfa_val_t* valscale = lc_malloc(sizeof(adfa_val_t));
         queue_add_tail(cntxt->Avals, valscale);
         valscale->is_mem = FALSE;
         valscale->op = ADFA_OP_NULL;
         valscale->type = ADFA_TYPE_IMM;
         valscale->data.imm = scale;

         valright = lc_malloc(sizeof(adfa_val_t));
         queue_add_tail(cntxt->Avals, valright);
         valright->is_mem = FALSE;
         valright->op = ADFA_OP_MUL;
         valright->type = ADFA_TYPE_SONS;
         valright->data.sons[0] = Rvali;
         valright->data.sons[1] = valscale;
      }

      // Base register and index register are not NULL
      if (Rvalb != NULL && valright != NULL) {  // left = off + base
         adfa_val_t* valleft = lc_malloc(sizeof(adfa_val_t));
         queue_add_tail(cntxt->Avals, valleft);
         valleft->is_mem = FALSE;
         valleft->op = ADFA_OP_ADD;
         valleft->type = ADFA_TYPE_SONS;
         valleft->data.sons[0] = valOff;
         valleft->data.sons[1] = Rvalb;

         // then val  = left + right
         val = lc_malloc(sizeof(adfa_val_t));
         queue_add_tail(cntxt->Avals, val);
         val->is_mem = TRUE;
         val->op = ADFA_OP_ADD;
         val->type = ADFA_TYPE_SONS;
         val->data.sons[0] = valleft;
         val->data.sons[1] = valright;
      }
      // else Base register is not NULL but index register is NULL
      else if (Rvalb != NULL && valright == NULL) {  // val = off + base
         val = lc_malloc(sizeof(adfa_val_t));
         queue_add_tail(cntxt->Avals, val);
         val->is_mem = TRUE;
         val->op = ADFA_OP_ADD;
         val->type = ADFA_TYPE_SONS;
         val->data.sons[0] = valOff;
         val->data.sons[1] = Rvalb;
      }
      // else Base register is NULL but index register is not NULL
      else {  // val = off + right
         val = lc_malloc(sizeof(adfa_val_t));
         queue_add_tail(cntxt->Avals, val);
         val->is_mem = TRUE;
         val->op = ADFA_OP_ADD;
         val->type = ADFA_TYPE_SONS;
         val->data.sons[0] = valOff;
         val->data.sons[1] = valright;
      }
      break;
   }

   default:
      return (NULL);
      break;
   }
   return (val);
}

/*
 * Creates a adfa_val_t structure from a given instruction
 * \param ssain an instruction
 * \param cntxt current context
 * \return an initialized adfa_val_t structure, else NULL
 */
adfa_val_t* ADFA_analyze_insn(ssa_insn_t* ssain, adfa_cntxt_t* cntxt)
{
   adfa_val_t* val = NULL;

   // Case of artificial instructions
   if (ssain->in == NULL) {
      int i = 0;
      while (ssain->oprnds[i] != NULL)
         i++;

      if (i == 1) {
         // Means it a removed phi-function : only one operand
         val = __register_to_val(ssain->oprnds[0], NULL, cntxt);
         hashtable_insert(cntxt->Rvals, ssain->output[0], val);
      } else {  // Means it is a phi-function with several operands
                // => create a adfa_val containing a register
         val = lc_malloc(sizeof(adfa_val_t));
         queue_add_tail(cntxt->Avals, val);
         val->is_mem = FALSE;
         val->op = ADFA_OP_NULL;
         val->type = ADFA_TYPE_REG;
         val->data.reg = ssain->output[0];
         hashtable_insert(cntxt->Rvals, ssain->output[0], val);
      }
   } else {
      insn_t* in = ssain->in;
      unsigned short family = insn_get_family(in);
      char* opcode = insn_get_opcode(in);

      // Instruction is a MOV
      if (family == FM_MOV) {
         val = __oprnd_to_val(ssain, 0, cntxt);
         if (ssain->output != NULL && ssain->output[0] != NULL)
            hashtable_insert(cntxt->Rvals, ssain->output[0], val);
      }
      // Instruction is an ADD
      else if (family == FM_ADD) {
         // Get op0
         // Get op1
         // val = op1 + op0
         adfa_val_t* op0 = __oprnd_to_val(ssain, 0, cntxt);
         adfa_val_t* op1 = __oprnd_to_val(ssain, 1, cntxt);

         val = lc_malloc(sizeof(adfa_val_t));
         queue_add_tail(cntxt->Avals, val);
         val->is_mem = FALSE;
         val->op = ADFA_OP_ADD;
         val->type = ADFA_TYPE_SONS;
         val->data.sons[0] = op1;
         val->data.sons[1] = op0;

         if (ssain->output != NULL && ssain->output[0] != NULL)
            hashtable_insert(cntxt->Rvals, ssain->output[0], val);
      }
      // Instruction is a SUB
      else if (family == FM_SUB) {
         // Get op0
         // Get op1
         // val = op1 - op0
         adfa_val_t* op0 = __oprnd_to_val(ssain, 0, cntxt);
         adfa_val_t* op1 = __oprnd_to_val(ssain, 1, cntxt);

         val = lc_malloc(sizeof(adfa_val_t));
         queue_add_tail(cntxt->Avals, val);
         val->is_mem = FALSE;
         val->op = ADFA_OP_SUB;
         val->type = ADFA_TYPE_SONS;
         val->data.sons[0] = op1;
         val->data.sons[1] = op0;

         if (ssain->output != NULL && ssain->output[0] != NULL)
            hashtable_insert(cntxt->Rvals, ssain->output[0], val);
      }
      // Instruction is a MUL
      else if (family == FM_MUL) {
         if (insn_get_nb_oprnds(in) >= 2) {
            // Get op0
            // Get op1
            // val = op1 * op0
            adfa_val_t* op0 = __oprnd_to_val(ssain, 0, cntxt);
            adfa_val_t* op1 = __oprnd_to_val(ssain, 1, cntxt);

            val = lc_malloc(sizeof(adfa_val_t));
            queue_add_tail(cntxt->Avals, val);
            val->is_mem = FALSE;
            val->op = ADFA_OP_MUL;
            val->type = ADFA_TYPE_SONS;
            val->data.sons[0] = op1;
            val->data.sons[1] = op0;

            if (ssain->output != NULL && ssain->output[0] != NULL)
               hashtable_insert(cntxt->Rvals, ssain->output[0], val);
         }
      }
      // Instruction is a DIV
      else if (family == FM_DIV) {
         if (insn_get_nb_oprnds(in) >= 2) {
            // Get op0
            // Get op1
            // val = op1 / op0
            adfa_val_t* op0 = __oprnd_to_val(ssain, 0, cntxt);
            adfa_val_t* op1 = __oprnd_to_val(ssain, 1, cntxt);

            val = lc_malloc(sizeof(adfa_val_t));
            queue_add_tail(cntxt->Avals, val);
            val->is_mem = FALSE;
            val->op = ADFA_OP_DIV;
            val->type = ADFA_TYPE_SONS;
            val->data.sons[0] = op1;
            val->data.sons[1] = op0;

            if (ssain->output != NULL && ssain->output[0] != NULL)
               hashtable_insert(cntxt->Rvals, ssain->output[0], val);
         }
      }
      // Instruction is Shift Left
      else if (strcmp(opcode, "SHL") == 0 || strcmp(opcode, "SAL") == 0) {
         adfa_val_t* op0 = __oprnd_to_val(ssain, 0, cntxt);
         adfa_val_t* op1 = __oprnd_to_val(ssain, 1, cntxt);

         if ((cntxt->driver->flags & ADFA_NO_UNRESOLVED_SHIFT) == 0
               || ((cntxt->driver->flags & ADFA_NO_UNRESOLVED_SHIFT) != 0
                     && op0->type == ADFA_TYPE_IMM)) {
            val = lc_malloc(sizeof(adfa_val_t));
            queue_add_tail(cntxt->Avals, val);
            val->is_mem = FALSE;
            val->op = ADFA_OP_SL;
            val->type = ADFA_TYPE_SONS;
            val->data.sons[0] = op1;
            val->data.sons[1] = op0;
            if (ssain->output != NULL && ssain->output[0] != NULL)
               hashtable_insert(cntxt->Rvals, ssain->output[0], val);
         } else {
            val = lc_malloc(sizeof(adfa_val_t));
            queue_add_tail(cntxt->Avals, val);
            val->is_mem = FALSE;
            val->op = ADFA_OP_NULL;
            val->type = ADFA_TYPE_REG;
            val->data.reg = ssain->output[0];
            if (ssain->output != NULL && ssain->output[0] != NULL)
               hashtable_insert(cntxt->Rvals, ssain->output[0], val);
         }
      }
      // Instruction is Shift Rigth
      else if (strcmp(opcode, "SHR") == 0 || strcmp(opcode, "SAR") == 0) {
         adfa_val_t* op0 = __oprnd_to_val(ssain, 0, cntxt);
         adfa_val_t* op1 = __oprnd_to_val(ssain, 1, cntxt);

         if ((cntxt->driver->flags & ADFA_NO_UNRESOLVED_SHIFT) == 0
               || ((cntxt->driver->flags & ADFA_NO_UNRESOLVED_SHIFT) != 0
                     && op0->type == ADFA_TYPE_IMM)) {
            val = lc_malloc(sizeof(adfa_val_t));
            queue_add_tail(cntxt->Avals, val);
            val->is_mem = FALSE;
            val->op = ADFA_OP_SR;
            val->type = ADFA_TYPE_SONS;
            val->data.sons[0] = op1;
            val->data.sons[1] = op0;
            if (ssain->output != NULL && ssain->output[0] != NULL)
               hashtable_insert(cntxt->Rvals, ssain->output[0], val);
         } else {
            val = lc_malloc(sizeof(adfa_val_t));
            queue_add_tail(cntxt->Avals, val);
            val->is_mem = FALSE;
            val->op = ADFA_OP_NULL;
            val->type = ADFA_TYPE_REG;
            val->data.reg = ssain->output[0];
            if (ssain->output != NULL && ssain->output[0] != NULL)
               hashtable_insert(cntxt->Rvals, ssain->output[0], val);
         }
      }
      // Instruction is XOR
      else if (family == FM_XOR) {
         //Special case : same register => 0
         if (oprnd_is_reg(insn_get_oprnd(in, 0)) == TRUE
               && oprnd_is_reg(insn_get_oprnd(in, 1)) == TRUE
               && oprnd_get_reg(insn_get_oprnd(in, 0))
                     == oprnd_get_reg(insn_get_oprnd(in, 1))) {
            val = lc_malloc(sizeof(adfa_val_t));
            queue_add_tail(cntxt->Avals, val);
            val->is_mem = FALSE;
            val->op = ADFA_OP_NULL;
            val->type = ADFA_TYPE_IMM;
            val->data.imm = 0;

            if (ssain->output != NULL && ssain->output[0] != NULL)
               hashtable_insert(cntxt->Rvals, ssain->output[0], val);
         }
      }
      // Instruction is LEA
      else if (family == FM_LEA) {
         val = __oprnd_to_val(ssain, 0, cntxt);
         val->is_mem = FALSE;
         if (ssain->output != NULL && ssain->output[0] != NULL)
            hashtable_insert(cntxt->Rvals, ssain->output[0], val);
      }
      // Instruction is INC or DEC
      else if (family == FM_INC || family == FM_DEC) {
         // Get op0
         // val = (+/-)1 + op0
         adfa_val_t* op0 = __oprnd_to_val(ssain, 0, cntxt);
         adfa_val_t* op1 = lc_malloc(sizeof(adfa_val_t));
         queue_add_tail(cntxt->Avals, op1);
         op1->is_mem = FALSE;
         op1->op = ADFA_OP_NULL;
         op1->type = ADFA_TYPE_IMM;

         if (family == FM_INC)
            op1->data.imm = 1;
         else
            op1->data.imm = -1;

         val = lc_malloc(sizeof(adfa_val_t));
         queue_add_tail(cntxt->Avals, val);
         val->is_mem = FALSE;
         val->op = ADFA_OP_ADD;
         val->type = ADFA_TYPE_SONS;
         val->data.sons[0] = op1;
         val->data.sons[1] = op0;

         if (ssain->output != NULL && ssain->output[0] != NULL)
            hashtable_insert(cntxt->Rvals, ssain->output[0], val);
      }
      // Instruction is SQRT
      else if (family == FM_SQRT) {
         // Get op0
         // val = SQRT(op0)
         adfa_val_t* op0 = __oprnd_to_val(ssain, 0, cntxt);

         val = lc_malloc(sizeof(adfa_val_t));
         queue_add_tail(cntxt->Avals, val);
         val->is_mem = FALSE;
         val->op = ADFA_OP_SQRT;
         val->type = ADFA_TYPE_SONS;
         val->data.sons[0] = op0;
         val->data.sons[1] = NULL;

         if (ssain->output != NULL && ssain->output[0] != NULL)
            hashtable_insert(cntxt->Rvals, ssain->output[0], val);
      }
      // Instruction is GATHER
      else if (family == FM_GATHER) {
         // Uses the second output in this case
         if (ssain->nb_output
               == 2 && hashtable_lookup (cntxt->Rvals, ssain->output[1]) == NULL) {
            val = lc_malloc(sizeof(adfa_val_t));
            queue_add_tail(cntxt->Avals, val);
            val->data.reg = ssain->output[1];
            val->is_mem = FALSE;
            val->op = ADFA_OP_NULL;
            val->type = ADFA_TYPE_REG;
            hashtable_insert(cntxt->Rvals, ssain->output[1], val);
         } else if (ssain->nb_output
               == 1 && hashtable_lookup (cntxt->Rvals, ssain->output[0]) == NULL) {
            val = lc_malloc(sizeof(adfa_val_t));
            queue_add_tail(cntxt->Avals, val);
            val->data.reg = ssain->output[0];
            val->is_mem = FALSE;
            val->op = ADFA_OP_NULL;
            val->type = ADFA_TYPE_REG;
            hashtable_insert(cntxt->Rvals, ssain->output[0], val);
         }
      }
      // Other instructions
      else {
         if (ssain->nb_output
               > 0 && hashtable_lookup (cntxt->Rvals, ssain->output[0]) == NULL) {
            val = lc_malloc(sizeof(adfa_val_t));
            queue_add_tail(cntxt->Avals, val);
            val->data.reg = ssain->output[0];
            val->is_mem = FALSE;
            val->op = ADFA_OP_NULL;
            val->type = ADFA_TYPE_REG;
            hashtable_insert(cntxt->Rvals, ssain->output[0], val);
         }
      }

      // Check that each register used in ssain->operands has been computed
      int i = 0;
      for (i = 0;
            i < insn_get_nb_oprnds(ssain->in) * 2 + ssain->nb_implicit_oprnds;
            i++) {
         if (ssain->oprnds[i] != NULL
               && hashtable_lookup(cntxt->Rvals, ssain->oprnds[i]) == NULL)
            __register_to_val(ssain->oprnds[i], ssain, cntxt);
      }
   }
   return (val);
}

/*
 static loop_t* _get_outer_loop (loop_t* l)
 {
 if (l == NULL)
 return (NULL);
 if (l->hierarchy_node == NULL
 ||  l->hierarchy_node->parent == NULL)
 return (l);
 loop_t* p = (loop_t*)(l->hierarchy_node->parent->data);
 while (p->hierarchy_node->parent != NULL)
 p = (loop_t*)(p->hierarchy_node->parent->data);
 return (p);
 }
 */

/**
 * Checks if an edge is a loop backedge
 * \param edge a CFG edge
 * \return TRUE is edge is a backedge, else FALSE
 */
static int __DFA_edge_isbackedge(graph_edge_t* edge)
{
   block_t* bfrom = ((block_t*) edge->from->data);
   block_t* to = ((block_t*) edge->to->data);

   if (to->loop == bfrom->loop) {
      loop_t* loop = bfrom->loop;
      list_t* entries = loop_get_entries(loop);
      FOREACH_INLIST(entries, it_entry)
      {
         block_t* entry = GET_DATA_T(block_t*, it_entry);
         if (entry->global_id == to->global_id)
            return (TRUE);
      }
   }

   return (FALSE);
}

/**
 * Checks if block corresponding to node can be computed or not
 * Add it in the context if it is possible
 * \param node a CFG node
 * \param context current context
 */
static void __DFA_BFS_is_computable(graph_node_t* node, void* context)
{
   adfa_cntxt_t* cntxt = (adfa_cntxt_t*) context;
   block_t* b = node->data;

   if (cntxt->_traversed[b->id] == 0) {
      FOREACH_INLIST(node->in, it) {
         graph_edge_t* ed = GET_DATA_T(graph_edge_t*, it);
         block_t* pred = ed->from->data;

         if (__DFA_edge_isbackedge(ed) == FALSE && pred != b
               && cntxt->_traversed[pred->id] == 0) {
            return;
         }
      }
      if (queue_lookup(cntxt->_to_compute, &direct_equal, b) == NULL)
         queue_add_head(cntxt->_to_compute, b);
   }
}

static int _check_traversed_blocks(adfa_cntxt_t* cntxt)
{
   FOREACH_INQUEUE(cntxt->f->blocks, it_b) {
      block_t* b = GET_DATA_T(block_t*, it_b);
      if (!block_is_padding(b) && cntxt->_traversed[b->id] == 0)
         return (FALSE);
   }

   return (TRUE);
}

static void _backup_strat(adfa_cntxt_t* cntxt)
{
   // Iterate over analyzed blocks to add one not analyzed successor
   // in the "todo" list
   // This should "unlock" the traverse algorithm and allow to analyze
   // all blocks.
   FOREACH_INQUEUE(cntxt->f->blocks, it_b)
   {
      int stop = FALSE;
      block_t* b = GET_DATA_T(block_t*, it_b);

      if (!block_is_padding(b) && cntxt->_traversed[b->id] == 1) {
         FOREACH_INLIST(b->cfg_node->out, it_next)
         {
            graph_edge_t* ed = GET_DATA_T(graph_edge_t*, it_next);
            block_t* next = (block_t*) (ed->to->data);
            if (cntxt->_traversed[next->id] == 0) {
               DBGMSG("Randomly add block %d\n", next->global_id);
               queue_add_head(cntxt->_to_compute, next);
               stop = TRUE;
               break;
            }
         }
      }
      if (stop)
         break;
   }
}

/**
 * Looks for a block to compute. A block can be computed if its predecessors
 * have been computed (after "removing" loop back edges)
 * \param cntxt current context
 * \return a basic block to compute or NULL
 */
static block_t* __DFA_find_computable_block(adfa_cntxt_t* cntxt)
{
   block_t* bb = NULL;

   if (queue_length(cntxt->_to_compute) == 0)
      graph_node_BFS(cntxt->graph, &__DFA_BFS_is_computable, NULL, cntxt);

   // Backup strat: if the list is empty but all blocks have not
   // been analyzed, try analyzed blocks successors.
   if (queue_length(cntxt->_to_compute) == 0
         && _check_traversed_blocks(cntxt) == FALSE) {
      _backup_strat(cntxt);
   }

   if (queue_length(cntxt->_to_compute) != 0) {
      bb = (block_t*) queue_peek_head(cntxt->_to_compute);
      queue_remove_head(cntxt->_to_compute);
   }
   return (bb);
}

/**
 * Analyzes a function using advanced data flow analyzis
 * \param f a function to analyze
 * \param driver a driver containing user functions
 * \return the context used for the analyzis
 */
adfa_cntxt_t* ADFA_analyze_function(fct_t* f, adfa_driver_t* driver)
{
   if (f == NULL || driver == NULL)
      return NULL;
   DBGMSG("Analyzing function %s\n", fct_get_name(f))

   block_t* b = NULL;
   adfa_cntxt_t* cntxt = lc_malloc(sizeof(adfa_cntxt_t));

   cntxt->Avals = queue_new();
   cntxt->Rvals = hashtable_new(&ssa_var_hash, &ssa_var_equal);
   cntxt->_to_compute = queue_new();
   queue_add_head(cntxt->_to_compute, FCT_ENTRY(f));
   cntxt->arch = f->asmfile->arch;
   cntxt->ssa = lcore_compute_ssa(f);
   cntxt->graph = FCT_ENTRY(f)->cfg_node;
   cntxt->_traversed = lc_malloc0(queue_length(f->blocks) * sizeof(char));
   cntxt->f = f;
   cntxt->driver = driver;

   // Initialize the adfa_val corresponding to the RIP register
   ssa_var_t* ssa_rip = lc_malloc(sizeof(ssa_var_t));
   ssa_rip->index = 0;
   ssa_rip->insn = NULL;
   ssa_rip->reg = cntxt->arch->reg_rip;

   adfa_val_t* val_rip = lc_malloc(sizeof(adfa_val_t));
   val_rip->data.reg = ssa_rip;
   val_rip->is_mem = FALSE;
   val_rip->op = ADFA_OP_NULL;
   val_rip->type = ADFA_TYPE_REG;
   hashtable_insert(cntxt->Rvals, ssa_rip, val_rip);

   // Run the analysis
   if (driver->init)
      driver->user_struct = driver->init(f, cntxt);

   while ((b = __DFA_find_computable_block(cntxt)) != NULL) {
      cntxt->_traversed[b->id] = 1;
      ssa_block_t* ssab = cntxt->ssa[b->id];

      FOREACH_INQUEUE(ssab->first_insn, it_in) {
         ssa_insn_t* ssain = GET_DATA_T(ssa_insn_t*, it_in);
         insn_t* in = ssain->in;
         if (in != NULL
               && ((driver->insn_filter != NULL) ?
                     driver->insn_filter(ssain, driver->user_struct) : TRUE)) {
            adfa_val_t* result = ADFA_analyze_insn(ssain, cntxt);

            if (driver->insn_execute != NULL)
               driver->insn_execute(ssain, result, cntxt->Rvals,
                     driver->user_struct);
         }
      }
      if (driver->propagate != NULL)
         driver->propagate(driver->user_struct, ssab);

   }

   // Free memory
   //lc_free (ssa_rip);
   //lc_free (val_rip);
   return (cntxt);
}

/*
 * Frees an adfa_cntxt_t structure
 * \param cntxt a structure to free
 */
void ADFA_free(adfa_cntxt_t* cntxt)
{
   if (cntxt == NULL)
      return;
   lc_free(cntxt->_traversed);
   queue_free(cntxt->_to_compute, NULL);
   queue_free(cntxt->Avals, &adfa_free_val);
   hashtable_free(cntxt->Rvals, NULL, NULL);
   lc_free(cntxt);
}
