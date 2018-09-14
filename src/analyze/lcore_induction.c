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

/* *
 * Basic induction variables:
 *  i = i + c
 *  i = i - c
 *  with c an invariant (constant or value not modified in the loop)
 *  i :: (i, c, 1)
 *  family(i) = i
 *
 *
 * Derived induction variables:
 *  j = i * b + a
 *  with a and b invariants (constants or values not modified in the loop)
 *  and j an induction variable
 *  j :: (i, a, b)
 *  family(j) = i
 *
 *  k = j * c + d
 *  k :: (i, a * c + d,b * c)
 */

/* ************************************************************************* *
 *                         Induction structures
 * ************************************************************************* */
/*
 * Create a new triple
 * \param family the triple family
 * \return an initialized triple
 */
ind_triple_t* __new_triple(ssa_var_t* family)
{
   if (family == NULL)
      return (NULL);
   ind_triple_t* triple = lc_malloc(sizeof(ind_triple_t));
   triple->family = family;
   triple->add = lc_malloc0(sizeof(ind_node_t));
   triple->mul = lc_malloc0(sizeof(ind_node_t));

   return (triple);
}

/*
 * Prints an induction node
 * \param n induction node to print
 * \param arch current architecture
 */
static void __print_induc_node(ind_node_t* n, arch_t* arch, FILE* outfile)
{
   if (n == NULL)
      return;

   switch (n->type) {
   case IND_NODE_IMM:
      fprintf(outfile, "%d", n->data.imm);
      break;

   case IND_NODE_SONS:
      __print_induc_node(n->data.sons[0], arch, outfile);
      switch (n->op) {
      case IND_OP_ADD:
         fprintf(outfile, " + ");
         break;
      case IND_OP_SUB:
         fprintf(outfile, " - ");
         break;
      case IND_OP_MUL:
         fprintf(outfile, " * ");
         break;
      case IND_OP_DIV:
         fprintf(outfile, " / ");
         break;

      case IND_OP_NULL:
      default:
         break;
      }
      __print_induc_node(n->data.sons[1], arch, outfile);
      break;

   case IND_NODE_INV:
      fprintf(outfile, "%s_%d",
            arch_get_reg_name(arch, n->data.inv->reg->type,
                  n->data.inv->reg->name), n->data.inv->index);
      break;

   case IND_NODE_NULL:
   default:
      break;
   }
}

void print_induction_triple(ind_triple_t* triple, arch_t* arch, FILE* outfile)
{
   if (arch == NULL || outfile == NULL || triple == NULL)
      return;

   fprintf(outfile, "<%s_%d, ",
         arch_get_reg_name(arch, triple->family->reg->type,
               triple->family->reg->name), triple->family->index);
   __print_induc_node(triple->add, arch, outfile);
   fprintf(outfile, ", ");
   __print_induc_node(triple->mul, arch, outfile);
   fprintf(outfile, ">");
}

/*
 * Prints a triple
 * \param reg register defined by the triple
 * \param triple triple to print
 * \param arch current architecture
 */
void __print_triple(ssa_var_t* reg, ind_triple_t* triple, arch_t* arch)
{
   if (triple == NULL || arch == NULL)
      return;

   if (reg != NULL)
      printf("%s_%d", arch_get_reg_name(arch, reg->reg->type, reg->reg->name),
            reg->index);

   printf(" : <%s_%d, ",
         arch_get_reg_name(arch, triple->family->reg->type,
               triple->family->reg->name), triple->family->index);
   __print_induc_node(triple->add, arch, stdout);
   printf(", ");
   __print_induc_node(triple->mul, arch, stdout);

   printf(">\n");
}

/* ************************************************************************* *
 *                             Induction
 * ************************************************************************* */
/*
 * Checks if in belongs to loop
 * \param in an instruction
 * \param loop a loop
 * \return TRUE if loop contains in, else FALSE
 */
static int _insn_in_loop_hierarchy(insn_t* in, loop_t* loop)
{
   if (in == NULL)
      return (FALSE);
   block_t* b = in->block;

   FOREACH_INQUEUE(loop->blocks, it_bl) {
      block_t* bl = GET_DATA_T(block_t*, it_bl);
      if (b == bl)
         return (TRUE);
   }
   return (FALSE);
}

/*
 * Checks if a variable is invariant inside a loop
 * \param var a variable
 * \param ssain instruction defining the variable
 * \param loop a loop
 * \return TRUE if var is invariant into loop, else FALSE
 */
int _is_ssa_var_invariant(ssa_var_t* var, ssa_insn_t* ssain, loop_t* loop,
      hashtable_t* invariants)
{
   if (hashtable_lookup(invariants, var) != NULL)
      return (TRUE);

   if ((var->insn == NULL && ssain->in != NULL)
         || (var->insn->in != NULL && var->insn->in->block->loop == NULL)
         || _insn_in_loop_hierarchy(var->insn->in, loop) == FALSE) {
      ind_invariant_t* inv = lc_malloc(sizeof(ind_invariant_t));
      inv->type = IND_NODE_INV;
      inv->data.inv = var;
      hashtable_insert(invariants, var, inv);
      return (TRUE);
   } else
      return (FALSE);
}

/*
 * Checks if a variable is an induction variable
 * \param var a variable
 * \param cntxt current context
 * \return TRUE if var is an induction variable, else FALSE
 */
static int _is_ssa_var_inducted(ssa_var_t* var, ind_context_t* cntxt)
{
   if (hashtable_lookup(cntxt->derived_induction, var) != NULL)
      return (TRUE);
   else
      return (FALSE);
}

/*
 * Checks if instruction operands can be used to defined an induction variable.
 * All operands should be immediate, inductions or invariants. If there is more than
 * one induction, the instruction can not been used to compute induction.
 * \param ssain current instruction to analyze
 * \param cntxt current context
 * \return TRUE if the instruction can be used to compute induction, else FALSE
 */
static int __check_opernds(ssa_insn_t* ssain, ind_context_t* cntxt)
{
   int i = 0;
   int stop = 0;
   insn_t* in = ssain->in;

   // Checks operands ------------------------------------------------
   for (i = 0; i < insn_get_nb_oprnds(in); i++) {
      int is_ind = FALSE;
      if (!(oprnd_is_imm(insn_get_oprnd(in, i)) == TRUE
            || (oprnd_is_reg(insn_get_oprnd(in, i)) == TRUE
                  && (_is_ssa_var_invariant(ssain->oprnds[i * 2], ssain,
                        cntxt->l, cntxt->invariants[cntxt->l->id]) == TRUE
                        || (_is_ssa_var_inducted(ssain->oprnds[i * 2], cntxt)
                              == TRUE && is_ind == FALSE))))) {
         if (oprnd_is_src(insn_get_oprnd(in, i))) {
            stop = 1;
            break;
         }
      } else {
         if (oprnd_is_reg(insn_get_oprnd(in, i)) == TRUE
               && oprnd_is_src(insn_get_oprnd(in, i))
               && _is_ssa_var_inducted(ssain->oprnds[i * 2], cntxt) == TRUE)
            is_ind = TRUE;
      }
   }
   if (stop)
      return (FALSE);

   // Checks implicit operands ---------------------------------------
   int nb_implicit_srcs = 0;
   reg_t** implicit_srcs = cntxt->arch->get_implicite_src(cntxt->arch,
         in->opcode, &nb_implicit_srcs);
   stop = 0;
   for (i = 0; i < nb_implicit_srcs; i++) {
      if (!(_is_ssa_var_invariant(ssain->oprnds[i * 2], ssain, cntxt->l,
            cntxt->invariants[cntxt->l->id]) == TRUE
            || _is_ssa_var_inducted(ssain->oprnds[i * 2], cntxt) == TRUE)) {
         stop = 1;
         break;
      }
   }
   if (stop) {
      if (nb_implicit_srcs > 0)
         lc_free(implicit_srcs);
      return (FALSE);
   }
   return (TRUE);
}

/*
 * Computes induction variables for a loop saved in current context
 * \param cntxt current context
 */
static void _compute_loop_derived_induction_(ind_context_t* cntxt)
{
   FOREACH_INQUEUE(cntxt->l->blocks, it_b)
   {
      block_t* b = GET_DATA_T(block_t*, it_b);
      ssa_block_t* ssab = cntxt->ssa[b->id];

      if (cntxt->blocks[b->id] == 0) {
         FOREACH_INQUEUE(ssab->first_insn, it_ssain) {
            ssa_insn_t* ssain = GET_DATA_T(ssa_insn_t*, it_ssain);
            insn_t* in = ssain->in;

            if (in != NULL && ssain->nb_output > 0) {
               // Check that operands are invariant or inducted
               if (__check_opernds(ssain, cntxt) == TRUE) {
                  cntxt->interp_insn(cntxt->derived_induction,
                        cntxt->invariants[cntxt->l->id], cntxt->l, ssain,
                        cntxt->allocs_node);
               }
            }
         }
         cntxt->blocks[b->id] = 1;
      }
   }
}

/*
 * Traverses the loop hierarchy to analyze each loop using a DFS.
 * \param loop a loop
 * \param cntxt current context
 */
static void _traverse_loop_hierachy(loop_t* loop, ind_context_t* cntxt)
{
   tree_t *tl, *child;
   tl = loop->hierarchy_node;
   child = tl->children;

   // If current loop is a leaf, analyze it
   if (child == NULL) {
      cntxt->l = loop;
      cntxt->invariants[loop->id] = hashtable_new(&ssa_var_hash,
            &ssa_var_equal);
      _compute_loop_derived_induction_(cntxt);
   } else {
      // Traverse the tree then analyze the loop
      while (child != NULL) {
         _traverse_loop_hierachy(child->data, cntxt);
         child = child->next;
      }
      cntxt->l = loop;
      cntxt->invariants[loop->id] = hashtable_new(&ssa_var_hash,
            &ssa_var_equal);
      _compute_loop_derived_induction_(cntxt);
   }
}

/*
 * Computes induction variables for a given function
 * \param fct a function to analyze
 * \return
 */
ind_context_t* lcore_compute_function_induction_fromSSA(fct_t* fct,
      ssa_block_t** ssa)
{
   if (fct == NULL || ssa == NULL)
      return NULL;

   // Initialize the context  -------------------------------------------------
   ind_context_t* context = lc_malloc(sizeof(ind_context_t));
   context->ssa = ssa;
   context->f = fct;
   context->arch = fct->asmfile->arch;
   context->interp_insn = NULL;
   context->analyze_cmp = NULL;
   context->allocs_node = queue_new();
   if (0) {
   }
   else {
      ERRMSG("Induction Analysis: architecture %s is not handled\n",
            context->arch->name);
      return NULL;
   }
   context->induction_limits = hashtable_new(&direct_hash, &direct_equal);
   context->derived_induction = hashtable_new(&ssa_var_hash, &ssa_var_equal);
   context->invariants = lc_malloc0(
         queue_length(fct->loops) * sizeof(hashtable_t*));
   context->blocks = lc_malloc0(sizeof(char) * queue_length(fct->blocks));

   // Run the analyzis --------------------------------------------------------
   //printf ("Start induction computation\n");

   FOREACH_INQUEUE(fct->loops, it_loop) {
      loop_t* loop = GET_DATA_T(loop_t*, it_loop);

      if (context->invariants[loop->id] == NULL) {
         tree_t* tl = loop->hierarchy_node;
         while (tl->parent != NULL)
            tl = tl->parent;
         _traverse_loop_hierachy(tl->data, context);
      }
   }

   // Print results here
   /*
    {
    printf ("\nINDUCTIONS ::::::::::::::::\n");
    FOREACH_INHASHTABLE (context->derived_induction, it_)
    {
    ssa_var_t* var = GET_KEY (var, it_);
    ind_triple_t* triple = GET_DATA_T (ind_triple_t*, it_);
    __print_triple (var, triple, context->arch);
    }
    }
    //*/
   /*
    {
    printf ("\nLIMITS     ::::::::::::::::\n");
    FOREACH_INHASHTABLE (context->induction_limits, it_)
    {
    ssa_var_t* var = GET_KEY (var, it_);
    ssa_insn_t* ssain = GET_DATA_T (ssa_insn_t*, it_);
    print_SSA_register (var, context->arch, stdout);
    printf (" == ");
    print_SSA_insn (ssain, context->arch, stdout);
    printf ("\n");
    }
    }
    //*/
   /*
    FOREACH_INQUEUE (fct->loops, it_loop1)
    {
    loop_t* loop = GET_DATA_T (loop_t*, it_loop1);

    printf ("\nInvariants loop %d ::::::::::\n", loop->global_id);
    FOREACH_INHASHTABLE (context->invariants[loop->id], it)
    {
    ssa_var_t* var = GET_KEY (var, it);
    ind_invariant_t* inv = GET_DATA_T (ind_invariant_t*, it);

    printf ("%s_%d : ",arch_get_reg_name(context->arch, var->reg->type, var->reg->name), var->index);
    if (inv->type == IND_NODE_IMM)
    printf ("%d", inv->data.imm);
    else if (inv->type == IND_NODE_INV)
    printf ("%s_%d",arch_get_reg_name(context->arch, inv->data.inv->reg->type, inv->data.inv->reg->name), inv->data.inv->index);
    printf ("\n");
    }
    }
    printf ("End of induction computation\n\n\n");
    //*/
   return (context);
}

/*
 * Computes induction variables for a given function
 * \param fct a function to analyze
 * \return
 */
ind_context_t* lcore_compute_function_induction(fct_t* fct)
{
   if (fct == NULL)
      return NULL;
   DBGMSG("Compute induction for %s\n", fct_get_name(fct));
   ssa_block_t** ssa = lcore_compute_ssa(fct);
   return (lcore_compute_function_induction_fromSSA(fct, ssa));
}

static void _free_triple(void* ptriple)
{
   ind_triple_t* triple = (ind_triple_t*) ptriple;
   if (triple == NULL)
      return;
   // triple->add->data.sons[x] and triple->mul->data.sons[x]
   // are saved in a list. They are freed when allocs_node member
   // is freed in lcore_free_induction.
   // This is done to avoid so double free
   lc_free(triple->add);
   lc_free(triple->mul);
   lc_free(triple);
}

/*
 * Frees induction results
 * \param cntxt induction results
 */
void lcore_free_induction(ind_context_t* cntxt)
{
   if (cntxt == NULL)
      return;
   int i = 0;

   for (i = 0; i < queue_length(cntxt->f->loops); i++)
      hashtable_free(cntxt->invariants[i], &lc_free, NULL);
   lc_free(cntxt->invariants);
   hashtable_free(cntxt->induction_limits, NULL, NULL);
   hashtable_free(cntxt->derived_induction, &_free_triple, NULL);
   lc_free(cntxt->blocks);
   queue_free(cntxt->allocs_node, &free);
   lc_free(cntxt);
}
