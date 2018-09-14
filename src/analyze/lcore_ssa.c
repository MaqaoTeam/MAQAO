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

#include "libmcore.h"

/**
 * \file
 * \section ssa_howto1 How to uses SSA computation ? (part 1 / 2)
 * To compute SSA form, call \ref lcore_compute_ssa function. It returns an
 * array of ssa_block_t* structures, one structure per block in the input
 * function. The ssa_block_t* structure representing a block is at index
 * block->id in the returned array.
 * A ssa_block_t structure contains a reference to the original block_t
 * structure, and a list of ssa_insn_t* structures, representing phi functions
 * or original instructions.
 * ssa_insn_t structures contain references to the ssa_block_t containing them
 * and to the instruction they represent. If the reference to the instruction
 * is NULL, this means the ssa_insn_t structure represents a phi function. The
 * same structure is used to represent both phi functions and instructions.
 * Differences are described in following subsections.
 *
 *
 * \subsection ssa_regular_insn SSA representation of an instruction
 * An ssa_insn_t structure representing an instruction can have any number of
 * output variables specified in nb_output member. Instruction operands are
 * saved in a flat 2D array: the ith operand of the instruction is saved at
 * indexes (2 * i) and (2 * i + 1).
 * If the operand is a register, then (2 * i) represents the register and
 * (2 * i + 1) is NULL.
 * If the operand is a memory access, then (2 * i) represents the base and
 * (2 * i + 1) represent the index. Scale and offset must be get from the
 * insn_t structure.
 * The member nb_implicit_oprnds gives the number of implicite operands.
 * Implicit operands are saved in the operands member, at the end of the array.
 * Example of operands member structure:
 *    For an example taking a memory operand, a register operand and an implict
 *    register, the structure of operands array is:
 *
 *   ---------------------------------------------------------
 *   | index |   0    |    1    |   3   |   4  |     5       |
 *   |-------+--------+---------+-------+------+-------------|
 *   | value | <base> | <index> | <reg> | NULL | <implicit>  |
 *   ---------------------------------------------------------
 *
 * \subsection ssa_phif Phi functions
 * A ssa_insn_t structure representing a phi function has not any
 * implicit register and always has one output element. The operands array is a
 * NULL terminated array, containing one ssa_var_t structure for each possible
 * value of the register at this time. In some cases, there is only one operand,
 * because some simplifactions have been done.
 *
 *
 * \section ssa_howto2 How to uses SSA computation ? (part 2 / 2)
 * Once SSA is computed, there is no need to call lcore_free_ssa function
 * because results are saved in fct_t structure and freed when the function
 * is freed.
 *
 * ssa_var_t structure is used to represent registers. The member reg is used
 * to store the register represented by the variable. The member index is used
 * to store the index of the variable in the SSA representation. If the index
 * is equal to 0, this means its the variable set at the function entry.
 * The last member, insn, is used to link a variable to its declaration.
 */

/* ************************************************************************* *
 *                               Internals
 * ************************************************************************* */
/**
 * \struct context_s
 * Used to store the analysis context
 */
typedef struct ssa_context_s {
   fct_t* f; /**<Current function*/
   hashtable_t* DF; /**<Dominance Frontier. 
    key  : (block_t*)
    data : (queue_t*) of (block_t*) */
   hashtable_t* A; /**<Set of variables and blocks assigning them.
    key  : (reg_t*)
    data : (queue_t*) of (block_t*) */
   int* C; /**<Number of assignment processed for each (reg_t*).
    The index of a register is computed using 
    the macro __regID(reg, arch).*/
   queue_t** S; /**<Stack of assignment processed for each (reg_t*).
    The index of a register is computed using 
    the macro __regID(reg, arch).*/
   ssa_insn_t*** def; /**<Array of definitions for each (reg_t*)
    The index of a register is computed using
    the macro __regID(reg, arch). Each subtable
    contains pointers on (ssa_insn_t*) defining
    the (reg_t*)*/
   arch_t* arch; /**<Architecture of current function*/
   ssa_block_t** ssa_blocks; /**<Array of ssa blocks*/
   int nb_reg; /**<Number of register in current architecture*/
} ssa_context_t;

/* ************************************************************************* *
 *                       Printing / Debug functions
 * ************************************************************************* */
/*
 * Prints a SSA variable
 * \param reg a SSA variable to print
 * \param arch current architecture
 * \param outfile file where to print the register
 */
void print_SSA_register(ssa_var_t* reg, arch_t* arch, FILE* outfile)
{
   if (reg == NULL || arch == NULL || outfile == NULL)
      return;
   /*
    fprintf (outfile, "%s_%d [%p]", arch_get_reg_name(arch,
    reg->reg->type,
    reg->reg->name),
    reg->index, reg->insn);
    //*/
   //*
   fprintf(outfile, "%s_%d",
         arch_get_reg_name(arch, reg->reg->type, reg->reg->name), reg->index);
   //*/
}

/*
 * Prints a SSA instruction
 * \param in a SSA instruction to print
 * \param arch current architecture
 * \param outfile file where to print the instruction
 */
void print_SSA_insn(ssa_insn_t* in, arch_t* arch, FILE* outfile)
{
   if (in == NULL || arch == NULL || outfile == NULL)
      return;
   int i = 0;
   oprnd_t* oprnd = NULL;

   if (in->in != NULL)
      fprintf(outfile, "0x%"PRIx64"  ", in->in->address);

   if (in->nb_output != 0) {
      printf("<");

      for (i = 0; i < in->nb_output; i++) {
         if (i > 0)
            printf(", ");
         print_SSA_register(in->output[i], arch, outfile);
      }
      fprintf(outfile, "> = ");
   }

   if (in->in != NULL) {
      fprintf(outfile, "%s ", arch_get_opcode_name(arch, in->in->opcode));

      for (i = 0; i < insn_get_nb_oprnds(in->in); i++) {
         oprnd = insn_get_oprnd(in->in, i);
         if (i != 0)
            fprintf(outfile, ", ");

         switch (oprnd_get_type(oprnd)) {
         case OT_REGISTER:
         case OT_REGISTER_INDEXED:
            print_SSA_register(in->oprnds[i * 2], arch, outfile);
            break;
         case OT_IMMEDIATE:
         case OT_IMMEDIATE_ADDRESS:
            fprintf(outfile, "0x%"PRIx64"", oprnd_get_imm(oprnd));
            break;
         case OT_POINTER:
            if (pointer_get_insn_target(oprnd_get_ptr(oprnd)) != NULL)
               fprintf(outfile, "(%d)",
                     block_get_id(
                           insn_get_block(
                                 pointer_get_insn_target(
                                       oprnd_get_ptr(oprnd)))));
            else
               fprintf(outfile, "()");
            break;
         case OT_MEMORY:
         case OT_MEMORY_RELATIVE:
            fprintf(outfile, "0x%"PRIx64"(", oprnd_get_offset(oprnd));
            if (oprnd_get_base(oprnd) != NULL)
               print_SSA_register(in->oprnds[i * 2], arch, outfile);

            if (oprnd_get_index(oprnd) != NULL) {
               fprintf(outfile, ", ");
               print_SSA_register(in->oprnds[i * 2 + 1], arch, outfile);
               fprintf(outfile, ", %d", oprnd_get_scale(oprnd));
            }
            fprintf(outfile, ")");
            break;
         default:
            break;   //To avoid compilation warnings
         }
      }

      if (in->nb_implicit_oprnds > 0) {
         printf("  <<");
         for (i = 0; i < in->nb_implicit_oprnds; i++) {
            if (i != 0)
               printf(", ");
            print_SSA_register(in->oprnds[i + insn_get_nb_oprnds(in->in) * 2],
                  arch, outfile);
         }
         printf(">>");
      }
   } else {
      int nb_op = 0;
      i = 0;
      while (in->oprnds[i++] != NULL)
         nb_op++;

      if (nb_op > 1) {
         i = 0;
         fprintf(outfile, "phi(");

         while (in->oprnds[i] != NULL) {
            if (i != 0)
               fprintf(outfile, ", ");
            if (in->oprnds[i] != NULL)
               print_SSA_register(in->oprnds[i], arch, outfile);
            i++;
         }
         fprintf(outfile, ")");
      } else {
         if (in->oprnds[0] != NULL)
            print_SSA_register(in->oprnds[0], arch, outfile);
      }
   }
}

/*
 * Prints the result of SSA analysis
 * \param cntxt current context
 * \param outfile file where to print the instruction
 */
void __print_SSA_code(ssa_context_t* cntxt, FILE* outfile)
{
   int i = 0;
   for (i = 0; i < fct_get_nb_blocks(cntxt->f); i++) {
      // print ssa instructions
      FOREACH_INQUEUE(cntxt->ssa_blocks[i]->first_insn, it_in)
      {
         ssa_insn_t* ssain = GET_DATA_T(ssa_insn_t*, it_in);
         fprintf(outfile, "(%3d :: %p) ",
               cntxt->ssa_blocks[i]->block->global_id, ssain);
         print_SSA_insn(ssain, cntxt->arch, outfile);
         fprintf(outfile, "\n");
      }
   }
}

/* ************************************************************************* *
 *                            Utils functions
 * ************************************************************************* */
/*
 * Equal function on ssa_var_t  for hashtable
 * \param v1 a pointer on a ssa_var_t
 * \param v1 a pointer on a ssa_var_t
 * \return TRUE if v1 == v2, else FALSE
 */
int ssa_var_equal(const void *v1, const void *v2)
{
   ssa_var_t* ssav1 = (ssa_var_t*) v1;
   ssa_var_t* ssav2 = (ssa_var_t*) v2;

   if (ssav1 == ssav2)
      return (TRUE);
   else if (ssav1 == NULL || ssav2 == NULL)
      return (FALSE);
   else if ((ssav1->index == ssav2->index) && (ssav1->reg == ssav2->reg))
      return (TRUE);
   else
      return (FALSE);
}

/*
 * Hash function on ssa_var_t  for hashtable
 * \param v a pointer on a ssa_var_t
 * \param size current size of the hashtable
 * \return an hash of v
 */
hashtable_size_t ssa_var_hash(const void *v, hashtable_size_t size)
{
   ssa_var_t* ssav = (ssa_var_t*) v;
   if (ssav == NULL)
      return (0);
   return ((((unsigned long) ssav->reg) * 1000 + ssav->index) % size);
}

/*
 * Standardizes a register retrieving the register with:
 *   - same name
 *   - same family
 *   - biggest type
 * \param reg a register
 * \param arch current architecture
 * \return the standardized register
 */
reg_t* standardize_reg(reg_t* reg, arch_t* arch)
{
   reg_t* s_reg = NULL;
   int type = reg_get_type(reg);

   if (reg == arch->reg_rip)
      return (reg);

   while (type + 1 < arch->nb_type_registers
         && arch->reg_families[type + 1] == arch->reg_families[type])
      type++;
   s_reg = arch->regs[type][(int) reg_get_name(reg)];
   return (s_reg);
}

/*
 * Creates a new SSA block
 * \param b a basic bloc
 * \return an initialized ssa block
 */
static ssa_block_t* __new_ssa_block(block_t* b)
{
   ssa_block_t* ssab = lc_malloc(sizeof(ssa_block_t));
   ssab->block = b;
   ssab->first_insn = queue_new();

   return (ssab);
}

/*
 * Creates a new SSA variable
 * \param r a register
 * \param index index of the SSA variable (-1 if value unknown)
 * \param arch current architecture
 * \return an initialized SSA variable
 */
ssa_var_t* __new_ssa_var(reg_t* r, int index, arch_t* arch)
{
   if (r == NULL)
      return (NULL);

   ssa_var_t* ssav = lc_malloc(sizeof(ssa_var_t));
   ssav->reg = standardize_reg(r, arch);
   ssav->index = index;
   ssav->insn = NULL;

   return (ssav);
}

/*
 * Creates a new SSA instruction
 * \param in an instruction
 * \param b current basic block
 * \param cntxt current context
 * \return an initialized ssa instruction
 */
static ssa_insn_t* __new_ssa_insn(reg_t* reg, insn_t* in, block_t* b,
      ssa_context_t* cntxt)
{
   ssa_insn_t* ssain = lc_malloc(sizeof(ssa_insn_t));
   int i = 0;
   ssain->in = in;
   queue_add_tail(cntxt->ssa_blocks[b->id]->first_insn, ssain);
   ssain->ssab = cntxt->ssa_blocks[b->id];

   // ssa instruction is an instruction
   if (in != NULL) {
      ssain->output = NULL;
      ssain->oprnds = lc_malloc(
            insn_get_nb_oprnds(in) * 2 * sizeof(ssa_var_t*));
      ssain->nb_output = 0;
      ssain->nb_implicit_oprnds = 0;
      for (i = 0; i < insn_get_nb_oprnds(in); i++) {
         oprnd_t* op = insn_get_oprnd(in, i);
         switch (oprnd_get_type(op)) {
         case OT_MEMORY:
         case OT_MEMORY_RELATIVE:
            ssain->oprnds[i * 2] = __new_ssa_var(oprnd_get_base(op), -1,
                  cntxt->arch);
            ssain->oprnds[i * 2 + 1] = __new_ssa_var(oprnd_get_index(op), -1,
                  cntxt->arch);
            break;

         case OT_REGISTER:
         case OT_REGISTER_INDEXED:
            ssain->oprnds[i * 2] = __new_ssa_var(oprnd_get_reg(op), -1,
                  cntxt->arch);
            ssain->oprnds[i * 2 + 1] = NULL;
            break;

         default:
            ssain->oprnds[i * 2] = NULL;
            ssain->oprnds[i * 2 + 1] = NULL;
            break;
         }
      }
   }
   // ssa instruction is a phi-function
   else {
      int nb = list_length(b->cfg_node->in);
      if (b == queue_peek_head(b->function->entries))
         nb++;

      ssain->oprnds = lc_malloc0((nb + 1) * sizeof(ssa_var_t*));
      for (i = 0; i < nb; i++)
         ssain->oprnds[i] = __new_ssa_var(reg, -1, cntxt->arch);
      ssain->nb_output = 1;
      ssain->output = lc_malloc(sizeof(ssa_var_t*));
      ssain->output[0] = __new_ssa_var(reg, -1, cntxt->arch);
   }
   return (ssain);
}

/*
 * Frees a SSA instruction
 * \param p a pointer on a SSA instruction to free
 */
static void __free_ssa_insn(void* p)
{
   ssa_insn_t* in = (ssa_insn_t*) p;
   int i = 0;

   if (in->output != NULL) {
      for (i = 0; i < in->nb_output; i++)
         lc_free(in->output[i]);
      lc_free(in->output);
   }

   if (in->in != NULL) {
      for (i = 0; i < insn_get_nb_oprnds(in->in); i++) {
         if (in->oprnds[i * 2] != NULL)
            lc_free(in->oprnds[i * 2]);
         if (in->oprnds[i * 2 + 1] != NULL)
            lc_free(in->oprnds[i * 2 + 1]);
      }
      for (i = 0; i < in->nb_implicit_oprnds; i++)
         lc_free(in->oprnds[i + insn_get_nb_oprnds(in->in) * 2]);
   } else {
      i = 0;
      while (in->oprnds[i] != NULL) {
         lc_free(in->oprnds[i]);
         i++;
      }
   }
   lc_free(in->oprnds);
   lc_free(in);
}

/*
 * Frees a SSA basic block
 * \param b a SSA instruction to free
 */
static void __free_ssa_block(ssa_block_t* b)
{
   queue_free(b->first_insn, &__free_ssa_insn);
   lc_free(b);
}

/* ************************************************************************* *
 *                   To compute Dominance Frontier
 * ************************************************************************* */
/*
 * Gets the immediate dominator of a block
 * \param X a block
 * \return the immediate dominator of X
 */
static block_t* idom(block_t* X)
{
   if (X == NULL)
      return (NULL);
   if (X->domination_node->parent == NULL)
      return (NULL);
   return (X->domination_node->parent->data);
}

/*
 * Computes the Dominance Frontier of a block 
 * \param tX node in the dominance tree of the block to analyze
 * \param cntxt current context
 */
static void _computes_DF(tree_t* tX, ssa_context_t* cntxt)
{
   if (tX == NULL)
      return;

   tree_t *tY = tX->children, *tZ = NULL;
   block_t *bX = NULL, *bY = NULL;
   queue_t *DF_X = NULL, *DF_Z = NULL;

   // Bottom-up traversal of the dominator tree
   while (tY != NULL) {
      _computes_DF(tY, cntxt);
      tY = tY->next;
   }

   // Add DF_local
   bX = tX->data;
   DF_X = hashtable_lookup(cntxt->DF, bX);
   FOREACH_INLIST(bX->cfg_node->out, it_ed) {
      graph_edge_t* ed = GET_DATA_T(graph_edge_t*, it_ed);
      bY = ed->to->data;
      if (idom(bY) != bX)
         queue_add_tail(DF_X, bY);
   }

   // Add DF_up
   tZ = tX->children;
   while (tZ != NULL) {
      DF_Z = hashtable_lookup(cntxt->DF, tZ->data);
      FOREACH_INQUEUE(DF_Z, it_Y) {
         bY = GET_DATA_T(block_t*, it_Y);
         if (idom(bY) != bX)
            queue_add_tail(DF_X, bY);
      }
      tZ = tZ->next;
   }
}

/* ************************************************************************* *
 *                         To insert phi-functions
 * ************************************************************************* */
/*
 * Macro defined to does not duplicate some code
 * into  __compute_A function
 */
#define __compute_Av {\
   if (o_reg != NULL)\
   {\
      s_reg = standardize_reg (o_reg, cntxt->arch);\
      queue_t* Av = hashtable_lookup (cntxt->A, s_reg);\
      if (Av == NULL)\
      {\
         Av = queue_new ();\
         hashtable_insert (cntxt->A, s_reg, Av);\
      }\
      if (queue_peek_tail (Av) != b)\
         queue_add_tail (Av, b);\
   }\
}

/*
 * Computes the set of used registers and blocks defining them
 * \param cntxt current context
 */
static void __compute_A(ssa_context_t* cntxt)
{
   int i = 0;
   oprnd_t* oprnd = NULL;
   reg_t *o_reg = NULL, *s_reg = NULL;

   // Iterates over all instructions in the function
   // to analyze operands
   FOREACH_INQUEUE(cntxt->f->blocks, it_b)
   {
      block_t* b = GET_DATA_T(block_t*, it_b);

      FOREACH_INSN_INBLOCK(b, it_in)
      {
         insn_t* in = GET_DATA_T(insn_t*, it_in);
         // For each operand, get used registers, standardize them
         // then add them into used registers
         for (i = 0; i < insn_get_nb_oprnds(in); i++) {
            oprnd = insn_get_oprnd(in, i);
            if (oprnd_is_reg(oprnd) == TRUE && oprnd_is_dst(oprnd) == TRUE) {
               o_reg = oprnd_get_reg(oprnd);
               __compute_Av;
            }
         }

         int nb_implicits = 0;
         reg_t** implicits = cntxt->arch->get_implicite_dst(cntxt->arch,
               insn_get_opcode_code(in), &nb_implicits);
         for (i = 0; i < nb_implicits; i++) {
            o_reg = implicits[i];
            __compute_Av;
         }
         if (implicits != NULL)
            lc_free(implicits);

      }
   }
}

/*
 * Inserts phi functions in the current function
 * \param cntxt current context
 * \param InOut Array with live registers results
 */
static void _insert_phi_functions(ssa_context_t* cntxt, char** InOut)
{
   long int IterCount = 0;
   queue_t* W = queue_new();
   block_t *bX = NULL, *bY = NULL;
   queue_t* DF_X = NULL;
   int* HasAlready = lc_malloc(sizeof(int) * fct_get_nb_blocks(cntxt->f));
   int* Work = lc_malloc(sizeof(int) * fct_get_nb_blocks(cntxt->f));

   __compute_A(cntxt);

   // Initializes arrays
   FOREACH_INQUEUE(cntxt->f->blocks, it_b) {
      block_t* b = GET_DATA_T(block_t*, it_b);
      HasAlready[b->id] = 0;
      Work[b->id] = 0;
   }

   // Iterates over variables
   FOREACH_INHASHTABLE(cntxt->A, it_V) {
      reg_t* V = GET_KEY(V, it_V);
      queue_t* Av = GET_DATA_T(queue_t*, it_V);
      IterCount += 1;

      // List a set a blocks to analyze
      FOREACH_INQUEUE(Av, it_X) {
         bX = GET_DATA_T(block_t*, it_X);
         Work[bX->id] = IterCount;
         queue_add_tail(W, bX);
      }

      while (!queue_is_empty(W)) {
         bX = queue_remove_head(W);
         DF_X = hashtable_lookup(cntxt->DF, bX);
         FOREACH_INQUEUE(DF_X, it_Y)
         {
            bY = GET_DATA_T(block_t*, it_Y);

            if (HasAlready[bY->id] < IterCount) {
               // if IN[V] then add a new phi-function
               if (InOut[bY->id][__regID(V, cntxt->arch)] & IN_FLAG)
                  __new_ssa_insn(V, NULL, bY, cntxt);
               HasAlready[bY->id] = IterCount;

               if (Work[bY->id] < IterCount) {
                  Work[bY->id] = IterCount;
                  queue_add_tail(W, bY);
               }
            }
         }
      }
   }

   queue_free(W, NULL);
   lc_free(HasAlready);
   lc_free(Work);
}

/* ************************************************************************* *
 *                          To rename registers
 * ************************************************************************* */
/*
 * Identify the position of P in S predecessors
 * \param S a block
 * \param P a block
 * \return the position of P in S predecessors or -1 if error
 */
static int __WhichPred(block_t* S, block_t* P)
{
   int i = 0;

   FOREACH_INLIST(S->cfg_node->in, it) {
      graph_edge_t* ed = GET_DATA_T(graph_edge_t*, it);
      block_t* Pp = ed->from->data;

      if (Pp == P)
         return (i);
      i++;
   }
   return (-1);
}

static int __filter_output_LHS(insn_t* in)
{
   char* opcode = insn_get_opcode(in);

   if (strcmp(opcode, "CMP") == 0)
      return (1);
   if (strcmp(opcode, "XCHG") == 0 && insn_get_nb_oprnds(in) == 2
         && oprnd_is_reg(insn_get_oprnd(in, 0)) == TRUE
         && oprnd_is_reg(insn_get_oprnd(in, 1)) == TRUE
         && oprnd_get_reg(insn_get_oprnd(in, 0))
               == oprnd_get_reg(insn_get_oprnd(in, 1)))
      return (1);
   return (0);
}

void __handle_LHS_var(void* pcntxt, ssa_insn_t* ssa_insn, reg_t* V)
{
   ssa_context_t* cntxt = (ssa_context_t*) pcntxt;
   long int i = cntxt->C[__regID(V, cntxt->arch)];
   queue_t* Sv = cntxt->S[__regID(V, cntxt->arch)];

   if (__filter_output_LHS(ssa_insn->in))
      return;

   ssa_insn->output = lc_realloc(ssa_insn->output,
         (ssa_insn->nb_output + 1) * sizeof(ssa_var_t*));
   ssa_insn->output[ssa_insn->nb_output] = __new_ssa_var(V, i, cntxt->arch);
   ssa_insn->output[ssa_insn->nb_output]->insn = cntxt->def[__regID(V,
         cntxt->arch)][i - 1];
   ssa_insn->nb_output = ssa_insn->nb_output + 1;

   queue_add_head(Sv, (void*) i);
   cntxt->C[__regID(V, cntxt->arch)] = i + 1;
   cntxt->def[__regID(V, cntxt->arch)] = lc_realloc(
         cntxt->def[__regID(V, cntxt->arch)], (i + 2) * sizeof(ssa_insn_t*));
   cntxt->def[__regID(V, cntxt->arch)][i] = ssa_insn;
   cntxt->def[__regID(V, cntxt->arch)][i + 1] = NULL;
}

void __handle_RHS_var(void* pcntxt, ssa_insn_t* ssa_insn, reg_t* V, int index)
{
   ssa_context_t* cntxt = (ssa_context_t*) pcntxt;
   queue_t* Sv = cntxt->S[__regID(V, cntxt->arch)];
   ssa_insn->oprnds[index]->index = (long int) queue_peek_head(Sv);
   ssa_insn->oprnds[index]->insn =
         cntxt->def[__regID(V, cntxt->arch)][ssa_insn->oprnds[index]->index];
}

/*
 * Recursive function used in variable renaming
 * \param bX a block
 * \param cntxt current context
 */
static void SEARCH(block_t* bX, ssa_context_t* cntxt)
{
   block_t* bY = NULL;
   int nb_implicits = 0;
   reg_t** implicits = NULL;

   // -------------------------------------------------------------------------
   //First loop: iterates over block instructions to update
   //RHS part, then LHS part
   // =================================
   // for each statement A in X do
   //    for each variable V in RHS(A) do
   //       replace use of V by use of Vi where i = Tp(S(V))
   //
   //    for each variable V in LHS(A) do
   //       i = C(V)
   //       replace V by Vi in LHS(A)
   //       push i onto S(V)
   //       C(V) = i + 1

   //As phi-functions are not in instruction list, there LHS parts are updated
   //before iterating over instructions
   FOREACH_INQUEUE(cntxt->ssa_blocks[bX->id]->first_insn, it_phi0) {
      ssa_insn_t* ssain = GET_DATA_T(ssa_insn_t*, it_phi0);

      // ssain->in != NULL means the instruction is not a phi-function
      if (ssain->in != NULL)
         break;
      reg_t* V = standardize_reg(ssain->output[0]->reg, cntxt->arch);
      long int i = cntxt->C[__regID(V, cntxt->arch)];
      queue_t* Sv = cntxt->S[__regID(V, cntxt->arch)];

      ssain->output[0]->index = i;
      queue_add_head(Sv, (void*) i);
      cntxt->C[__regID(V, cntxt->arch)] = i + 1;
      cntxt->def[__regID(V, cntxt->arch)] = lc_realloc(
            cntxt->def[__regID(V, cntxt->arch)], (i + 2) * sizeof(ssa_insn_t*));
      cntxt->def[__regID(V, cntxt->arch)][i] = ssain;
      cntxt->def[__regID(V, cntxt->arch)][i + 1] = NULL;
   }

   FOREACH_INSN_INBLOCK(bX, it_in) {
      insn_t* in = GET_DATA_T(insn_t*, it_in);
      oprnd_t* oprnd = NULL;
      int iter = 0;
      ssa_insn_t* ssa_insn = __new_ssa_insn(NULL, in, bX, cntxt);

      //RHS
      for (iter = 0; iter < insn_get_nb_oprnds(in); iter++) {
         oprnd = insn_get_oprnd(in, iter);
         reg_t* V = NULL;

         //oprnd belongs to RHS if the operand is not a destination operand
         //or if its type is not REGISTER
         switch (oprnd_get_type(oprnd)) {
         case OT_MEMORY:
         case OT_MEMORY_RELATIVE:
            if (oprnd_get_base(oprnd)) {
               V = standardize_reg(oprnd_get_base(oprnd), cntxt->arch);
               __handle_RHS_var(cntxt, ssa_insn, V, iter * 2);
            }

            if (oprnd_get_index(oprnd)) {
               V = standardize_reg(oprnd_get_index(oprnd), cntxt->arch);
               __handle_RHS_var(cntxt, ssa_insn, V, iter * 2 + 1);
            }
            break;

         case OT_REGISTER:
         case OT_REGISTER_INDEXED:
            V = standardize_reg(oprnd_get_reg(oprnd), cntxt->arch);
            __handle_RHS_var(cntxt, ssa_insn, V, iter * 2);
            break;

         default:
            break;
         }
      }
      // Here, handle implicit registers modified by the current instruction
      // As this is arch-specific, a arch-specific driver should be used
      implicits = cntxt->arch->get_implicite_src(cntxt->arch, in->opcode,
            &nb_implicits);
      if (nb_implicits > 0) {
         ssa_insn->oprnds = lc_realloc(ssa_insn->oprnds,
               (insn_get_nb_oprnds(in) * 2 + nb_implicits)
                     * sizeof(ssa_var_t*));
         ssa_insn->nb_implicit_oprnds = nb_implicits;

         for (iter = 0; iter < nb_implicits; iter++) {
            reg_t* V = implicits[iter];
            ssa_insn->oprnds[iter + 2 * insn_get_nb_oprnds(in)] = __new_ssa_var(
                  V, -1, cntxt->arch);
            __handle_RHS_var(cntxt, ssa_insn, V,
                  iter + 2 * insn_get_nb_oprnds(in));
         }
         lc_free(implicits);
      }

      //LHS
      for (iter = 0; iter < insn_get_nb_oprnds(in); iter++) {
         oprnd = insn_get_oprnd(in, iter);
         if (oprnd_is_reg(oprnd) == TRUE && oprnd_is_dst(oprnd) == TRUE) {
            //oprnd belongs to LHS only if the operand is a destination operand
            //and its type is REGISTER
            reg_t* V = standardize_reg(oprnd_get_reg(oprnd), cntxt->arch);
            __handle_LHS_var(cntxt, ssa_insn, V);
         }
      }
      // Here, handle implicit registers modified by the current instruction
      // As this is arch-specific, a arch-specific driver should be used
      implicits = cntxt->arch->get_implicite_dst(cntxt->arch, in->opcode,
            &nb_implicits);
      if (nb_implicits > 0) {
         for (iter = 0; iter < nb_implicits; iter++) {
            reg_t* V = implicits[iter];
            __handle_LHS_var(cntxt, ssa_insn, V);
         }
         lc_free(implicits);
      }
   }

   // -------------------------------------------------------------------------
   //Second loop: iterates over block successors in CFG to update
   //their phi-functions
   // =================================
   // for each Y in Succ(X) do
   //    j = WhichPred(Y, X)
   //    for each phi-function F in Y
   //       replace the j-th operand V in RHS(F) by Vi where i = Top (S(V))

   FOREACH_INLIST(bX->cfg_node->out, it_ed) {
      graph_edge_t* ed = GET_DATA_T(graph_edge_t*, it_ed);
      bY = ed->to->data;
      int j = __WhichPred(bY, bX);

      FOREACH_INQUEUE(cntxt->ssa_blocks[bY->id]->first_insn, it_phi0)
      {
         ssa_insn_t* ssain = GET_DATA_T(ssa_insn_t*, it_phi0);
         int i = 0;

         // ssain->in != NULL means the instruction is not a phi-function
         if (ssain->in != NULL)
            break;

         for (i = 0; i < ssain->nb_output; i++) {
            reg_t* V = standardize_reg(ssain->output[i]->reg, cntxt->arch);
            queue_t* Sv = cntxt->S[__regID(V, cntxt->arch)];
            long int topSV = (long int) queue_peek_head(Sv);
            ssain->oprnds[j]->index = topSV;
            ssain->oprnds[j]->insn = cntxt->def[__regID(V, cntxt->arch)][topSV];
         }
      }
   }

   // -------------------------------------------------------------------------
   //Third loop: iterates over block children in DT to analyze them
   // =================================
   // for each Y in Children(X) do
   //    call SEARCH(Y)

   tree_t* tY = bX->domination_node->children;
   while (tY != NULL) {
      bY = tY->data;
      SEARCH(bY, cntxt);
      tY = tY->next;
   }

   // -------------------------------------------------------------------------
   //Fourth loop: iterates over instructions to pop stacks
   // =================================
   // for each assignment A in X do
   //    for each V in oldLHS(A)
   //       pop S(V)

   FOREACH_INQUEUE(cntxt->ssa_blocks[bX->id]->first_insn, it_phi1)
   {
      ssa_insn_t* ssain = GET_DATA_T(ssa_insn_t*, it_phi1);
      int i = 0;
      for (i = 0; i < ssain->nb_output; i++) {
         reg_t* V = ssain->output[i]->reg;
         queue_t* Sv = cntxt->S[__regID(V, cntxt->arch)];
         queue_remove_head(Sv);
      }
   }
}

/*
 * Entry point for variables renaming.
 * \param cntxt current context
 */
static void _rename_variables(ssa_context_t* cntxt)
{
   int i = 0;

   cntxt->C = lc_malloc0(cntxt->nb_reg * sizeof(int));
   cntxt->S = lc_malloc0(cntxt->nb_reg * sizeof(queue_t*));
   cntxt->def = lc_malloc0(cntxt->nb_reg * sizeof(queue_t*));

   // Iterates over variables
   //FOREACH_INHASHTABLE (cntxt->A, it_V)
   for (i = 0; i < cntxt->nb_reg; i++) {
      cntxt->C[i] = 1;
      cntxt->S[i] = queue_new();
      queue_add_head(cntxt->S[i], (void*) 0);

      cntxt->def[i] = lc_malloc(sizeof(ssa_insn_t*) * 2);
      cntxt->def[i][0] = NULL;
      cntxt->def[i][1] = NULL;
   }

   //Initialize phi function for first block
   block_t* b_head = queue_peek_head(cntxt->f->entries);
   ssa_block_t* ssab = cntxt->ssa_blocks[b_head->id];
   FOREACH_INQUEUE(ssab->first_insn, it_ssain) {
      ssa_insn_t* ssain = GET_DATA_T(ssa_insn_t*, it_ssain);
      if (ssain->in != NULL)
         break;
      ssain->oprnds[list_length(b_head->cfg_node->in)]->index = 0;
   }

   SEARCH(FCT_ENTRY(cntxt->f), cntxt);

   // Once SSA has been computed for each block, try to link phi-functions
   // return variables to the previous definition of the variable.
   // As phi-functions are added when there is more than one predecessor,
   // the previous definition is the definition which is not in a
   // "back edge predecessor"
   FOREACH_INQUEUE(cntxt->f->blocks, it_b)
   {
      block_t* b = GET_DATA_T(block_t*, it_b);
      int pred = 0;

      // Only loop entry blocks with two predecessors are handled.
      if (block_is_loop_entry(b) == TRUE && list_length(b->cfg_node->in) == 2) {
         // look for the predecessor with is not in the same loop (means it is
         // a backedge) in order to link it to return variable.
         graph_edge_t* ed = b->cfg_node->in->data;
         block_t* pred_block = ed->from->data;
         if (pred_block->loop == b->loop)
            pred = 1;

         // Iterates over phi-functions
         FOREACH_INQUEUE(cntxt->ssa_blocks[b->id]->first_insn, it_in)
         {
            ssa_insn_t* ssain = GET_DATA_T(ssa_insn_t*, it_in);
            if (ssain->in != NULL)
               break;
            reg_t* V = standardize_reg(ssain->output[0]->reg, cntxt->arch);
            ssain->output[0]->insn =
                  cntxt->def[__regID(V, cntxt->arch)][ssain->oprnds[pred]->index];
         }
      }
   }
}

/* ************************************************************************* *
 *       To simplify phi-functions using the same operand several times
 * ************************************************************************* */
static void _simplify_phifunctions_operands(ssa_context_t* cntxt)
{
   FOREACH_INQUEUE(cntxt->f->blocks, it_b)
   {
      block_t* b = GET_DATA_T(block_t*, it_b);
      ssa_block_t* ssab = cntxt->ssa_blocks[b->id];

      // Iterates over phi-functions ---------------------------------------
      FOREACH_INQUEUE(ssab->first_insn, it_ssain)
      {
         ssa_insn_t* ssain = GET_DATA_T(ssa_insn_t*, it_ssain);
         int i = 0, j = 0;
         ssa_var_t** oprnds = NULL;
         int nb_oprnd_orig = 1;
         int nb_oprnd_new = 0;

         if (ssain->in != NULL)
            break;
         // At this point, ssain is a phi-function
         i = 0;
         while (ssain->oprnds[i++] != NULL)
            nb_oprnd_orig++;
         oprnds = lc_malloc0(sizeof(ssa_var_t*) * nb_oprnd_orig);

         i = 0;
         while (ssain->oprnds[i] != NULL) {
            int found = FALSE;

            for (j = 0; j < nb_oprnd_new; j++)
               if (ssain->oprnds[i]->index == oprnds[j]->index)
                  found = TRUE;
            if (!found) {
               oprnds[nb_oprnd_new] = lc_malloc(sizeof(ssa_var_t));
               memcpy(oprnds[nb_oprnd_new], ssain->oprnds[i],
                     sizeof(ssa_var_t));
               nb_oprnd_new++;
            }
            lc_free(ssain->oprnds[i]);
            i++;
         }
         lc_free(ssain->oprnds);
         ssain->oprnds = oprnds;
      }
   }
}

/* ************************************************************************* *
 *                       To remove some phi-functions
 * ************************************************************************* */
/*
 * Looks for the position of the operand which is not defined in the loop
 * \param ssain current phi-function
 * \return -1 if the position is not found, else a positive value
 */
static int __lookfor_pred_id(ssa_insn_t* ssain)
{
   int i = 0;

   while (ssain->oprnds[i] != NULL) {
      if (ssain->oprnds[i]->insn == NULL)
         return (-1);

      ssa_block_t* bp = ssain->oprnds[i]->insn->ssab;
      if (block_is_dominated(bp->block, ssain->ssab->block)) { // The block defining the operand dominates current block
                                                               // means it is before the loop in the domination tree
         return (i);
         break;
      }
      i++;
   }
   return (-1);
}

/*
 * Looks for an instruction updating a register into a loop
 * \param loop a loop to analyze
 * \param reg a register to look for
 * \param cst a constant value which is the only accepted value for reg
 * \return FALSE if an instruction updates reg with a value different than cst,
 *               else TRUE
 */
static int __lookfor_updates_on_reg(loop_t* loop, reg_t* reg, int64_t cst)
{
   FOREACH_INQUEUE(loop->blocks, it_b) {
      block_t* b = GET_DATA_T(block_t*, it_b);

      FOREACH_INSN_INBLOCK(b, it_in)
      {
         insn_t* in = GET_DATA_T(insn_t*, it_in);

         if (insn_get_nb_oprnds(in) == 2) {
            oprnd_t* op = insn_get_oprnd(in, 1);
            if (oprnd_is_dst(op) && oprnd_is_reg(op) == TRUE
                  && oprnd_get_reg(op) == reg) {
               if (oprnd_is_imm(insn_get_oprnd(in, 0)) == FALSE)
                  return (FALSE);
               else if (oprnd_get_imm(insn_get_oprnd(in, 0)) != cst)
                  return (FALSE);
            }
         } else {
            // Check an instruction doesn't update the target register
            int i = 0;
            for (i = 0; i < insn_get_nb_oprnds(in); i++) {
               oprnd_t* op = insn_get_oprnd(in, i);
               if (oprnd_is_reg(op) == TRUE && oprnd_get_reg(op) == reg
                     && oprnd_is_dst(op))
                  return (FALSE);
            }
         }
      }
   }
   return (TRUE);
}

/*
 * Updates a phi-function, removing its operands to transforme it into an affectation
 * \param ssain current phi-function
 * \param index of the ssa register to keep
 */
static void __remove_phi_function(ssa_insn_t* ssain, int index,
      ssa_insn_t* prev_in)
{
   int i = 1;

   while (ssain->oprnds[i] != NULL) {
      lc_free(ssain->oprnds[i]);
      ssain->oprnds[i] = NULL;
      i++;
   }
   ssain->oprnds[0]->index = index;
   ssain->oprnds[0]->insn = prev_in;
}

/*
 * Deletes some phi-functions removing ones created because the same value is
 * set in all paths
 * \param cntxt current context
 */
static void _delete_phifunctions_loops(ssa_context_t* cntxt)
{
   // Iterates over innermost loops -------------------------------------------
   FOREACH_INQUEUE(cntxt->f->loops, it_loop)
   {
      loop_t* loop = GET_DATA_T(loop_t*, it_loop);
      if (list_length(loop->entries) == 1) {
         block_t* b = GET_DATA_T(block_t*, loop->entries);
         ssa_block_t* ssab = cntxt->ssa_blocks[b->id];

         // Iterates over phi-functions ---------------------------------------
         FOREACH_INQUEUE(ssab->first_insn, it_ssain)
         {
            ssa_insn_t* ssain = GET_DATA_T(ssa_insn_t*, it_ssain);
            int pred_id = -1;
            insn_t* in = NULL;
            if (ssain->in != NULL)
               break;

            // At this point, ssain is a phi-function
            // Find the operand which is defined before the loop
            pred_id = __lookfor_pred_id(ssain);
            if (pred_id == -1)
               continue;

            // At this point, pred_id contains the position of the operand
            // defined before the loop
            // If the operand is not set with an immediate value, exit
            in = ssain->oprnds[pred_id]->insn->in;
            if (in == NULL || insn_get_nb_oprnds(in) != 2
                  || insn_get_family(in) != FM_MOV
                  || oprnd_is_imm(insn_get_oprnd(in, 0)) == FALSE)
               continue;

            // Get the immediate value, then compare it to all instructions
            // modifing the register. If it is always the same immediate value,
            // the phi-function can be removed
            int64_t cst = oprnd_get_imm(insn_get_oprnd(in, 0));
            if (__lookfor_updates_on_reg(loop, ssain->oprnds[0]->reg, cst)) {
               int index = ssain->oprnds[pred_id]->index;
               ssa_insn_t* prev_in = ssain->oprnds[pred_id]->insn;

               FOREACH_INQUEUE(loop->blocks, it_bb)
               {
                  block_t* bb = GET_DATA_T(block_t*, it_bb);
                  ssa_block_t* ssabb = cntxt->ssa_blocks[bb->id];

                  FOREACH_INQUEUE(ssabb->first_insn, it_ssain1)
                  {
                     ssa_insn_t* ssain1 = GET_DATA_T(ssa_insn_t*, it_ssain1);

                     if (ssain1->in != NULL)
                        break;
                     if (ssain1->output[0]->reg == ssain->output[0]->reg)
                        __remove_phi_function(ssain1, index, prev_in);
                  }
               }
            }
         }
      }
   }
}

/* ************************************************************************* *
 *      Delete some phi functions loading the same value in all operands
 * ************************************************************************* */
/*
 * Deletes phi-functions created because the same memory element is used to set a register
 * \param cntxt current context
 */
static void _delete_phifunctions_same_affectation(ssa_context_t* cntxt)
{
   // Iterates over innermost loops -------------------------------------------
   FOREACH_INQUEUE(cntxt->f->loops, it_loop)
   {
      loop_t* loop = GET_DATA_T(loop_t*, it_loop);
      if (list_length(loop->entries) == 1) {
         block_t* b = GET_DATA_T(block_t*, loop->entries);
         ssa_block_t* ssab = cntxt->ssa_blocks[b->id];

         // Iterates over phi-functions ---------------------------------------
         FOREACH_INQUEUE(ssab->first_insn, it_ssain)
         {
            ssa_insn_t* ssain = GET_DATA_T(ssa_insn_t*, it_ssain);
            int nb_oprnd = 1;
            if (ssain->in != NULL)
               break;

            while (ssain->oprnds[nb_oprnd] != NULL)
               nb_oprnd++;

            // Analyze available only if there is two operands
            if (nb_oprnd == 2 && ssain->oprnds[0]->insn != NULL
            && ssain->oprnds[1]->insn != NULL
            && ssain->oprnds[0]->insn->in != NULL
            && ssain->oprnds[1]->insn->in != NULL) {
               ssa_insn_t* ssain0 = ssain->oprnds[0]->insn;
               ssa_insn_t* ssain1 = ssain->oprnds[1]->insn;
               unsigned short fam0 = insn_get_family(ssain0->in);
               unsigned short fam1 = insn_get_family(ssain1->in);

               if (((fam0 == FM_MOV && fam1 == FM_MOV)
                     || (fam0 == FM_LEA && fam1 == FM_LEA))
                     && insn_get_nb_oprnds(ssain0->in) == 2
                     && insn_get_nb_oprnds(ssain1->in) == 2) {
                  if (ssa_var_equal(ssain0->oprnds[0], ssain1->oprnds[0])
                        && ssa_var_equal(ssain0->oprnds[1], ssain1->oprnds[1])
                        && oprnd_get_offset(insn_get_oprnd(ssain0->in, 0))
                              == oprnd_get_offset(
                                    insn_get_oprnd(ssain1->in, 0))) {
                     //printf ("===> DELETE ");
                     //print_SSA_insn (ssain, cntxt->arch, stdout);
                     //printf ("\n");

                     // At this point, we know the phi function can be removed.
                     // To do this, remove the operand defined in the loop from
                     // phi function operands.
                     int pred_id = __lookfor_pred_id(ssain);
                     if (pred_id >= 0) {
                        ssain->oprnds[0]->index = ssain->oprnds[pred_id]->index;
                        ssain->oprnds[0]->insn = ssain->oprnds[pred_id]->insn;
                        lc_free(ssain->oprnds[1]);
                        ssain->oprnds[1] = NULL;
                     }
                  }
               }
            }
         }
      }
   }
}

/* ************************************************************************* *
 *                             API functions
 * ************************************************************************* */
ssa_block_t** lcore_compute_ssa(fct_t* fct)
{
   if (fct == NULL)
      return NULL;
   if (fct->ssa != NULL)
      return (((ssa_context_t*) fct->ssa)->ssa_blocks);
   if (fct->asmfile != NULL)
      fct->asmfile->free_ssa = &lcore_free_ssa;

   // Initialize variables
   // -------------------------------------------------------------------------
   DBGMSG("Computing SSA for function %s\n", fct_get_name(fct));
   block_t* fct_entry = FCT_ENTRY(fct);
   ssa_context_t* cntxt = lc_malloc(sizeof(ssa_context_t));
   char** InOut = lcore_compute_live_registers(fct, &(cntxt->nb_reg), FALSE);
   cntxt->f = fct;
   cntxt->DF = hashtable_new_with_custom_size(direct_hash, direct_equal,
         fct_get_nb_blocks(fct), TRUE);
   cntxt->A = hashtable_new(direct_hash, direct_equal);
   cntxt->arch = fct->asmfile->arch;
   cntxt->ssa_blocks = lc_malloc(sizeof(ssa_block_t*) * fct_get_nb_blocks(fct));

   FOREACH_INQUEUE(fct->blocks, it_b) {
      block_t* b = GET_DATA_T(block_t*, it_b);
      hashtable_insert(cntxt->DF, b, queue_new());
      cntxt->ssa_blocks[b->id] = __new_ssa_block(b);
   }

   // Run the analysis
   // -------------------------------------------------------------------------
   DBGMSG0("--- Computing dominance frontier ...\n");
   _computes_DF(fct_entry->domination_node, cntxt);
   DBGMSG0("--- Computing phi functions ...\n");
   _insert_phi_functions(cntxt, InOut);
   DBGMSG0("--- Renaming variables ...\n");
   _rename_variables(cntxt);
   DBGMSG0("--- Simplify phi functions operands ...\n");
   _simplify_phifunctions_operands(cntxt);
   DBGMSG0("--- Simplify phi functions loops ...\n");
   _delete_phifunctions_loops(cntxt);
   DBGMSG0("--- Simplify phi functions with same affectation ...\n");
   _delete_phifunctions_same_affectation(cntxt);

   // Free memory
   // -------------------------------------------------------------------------
   // Temporary data (not used outside this function)
   DBGMSG0("--- Free memory ...\n");
   int i = 0;
   {
      FOREACH_INHASHTABLE(cntxt->DF, it_1) {
         queue_t* q = GET_DATA_T(queue_t*, it_1);
         queue_free(q, NULL);
      }
      hashtable_free(cntxt->DF, NULL, NULL);
   }
   {
      FOREACH_INHASHTABLE(cntxt->A, it_2) {
         queue_t* q = GET_DATA_T(queue_t*, it_2);
         queue_free(q, NULL);
      }
      hashtable_free(cntxt->A, NULL, NULL);
   }
   for (i = 0; i < cntxt->nb_reg; i++) {
      if (cntxt->S[i])
         queue_free(cntxt->S[i], NULL);
      /*if (cntxt->def[i])
       lc_free (cntxt->def[i]);*/
   }
   lc_free(cntxt->S);
   lc_free(cntxt->C);
   //lc_free (cntxt->def);

   fct->ssa = cntxt;
   //__print_SSA_code (cntxt, stdout);

   DBGMSG0("       ***********\n");
   return (cntxt->ssa_blocks);
}

void lcore_free_ssa(fct_t* f)
{
   // TODO Fix memory issue during freeing
   return;
   if (f == NULL || f->ssa == NULL)
      return;
   ssa_context_t* cntxt = ((ssa_context_t*) f->ssa);
   int i = 0;
   for (i = 0; i < fct_get_nb_blocks(f); i++)
      __free_ssa_block(cntxt->ssa_blocks[i]);
   lc_free(cntxt->ssa_blocks);

   for (i = 0; i < cntxt->nb_reg; i++)
      if (cntxt->def[i])
         lc_free(cntxt->def[i]);
   lc_free(cntxt->def);
   lc_free(cntxt);
   f->ssa = NULL;
}

ssa_block_t** fct_get_ssa(fct_t* f)
{
   if (f != NULL)
      return (((ssa_context_t*) f->ssa)->ssa_blocks);
   return (NULL);
}

ssa_insn_t*** fct_get_ssa_defs(fct_t* f)
{
   if (f != NULL)
      return (((ssa_context_t*) f->ssa)->def);
   return (NULL);
}
