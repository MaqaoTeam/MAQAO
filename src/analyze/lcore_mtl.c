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

/*
 * Gets an ssa instruction from a function using its address
 * \param f a function
 * \param addr an address
 * \return the ssa instruction corresponding to addr or NULL
 */
//TODO: remove this unused function? (Sylvain 2014-09-15)
//
//static ssa_insn_t* _get_ssain_from_addr (fct_t* f, int64_t addr)
//{
//   FOREACH_INQUEUE (f->blocks, it_b)
//   {
//      block_t* b = GET_DATA_T (block_t*, it_b);
//
//      if (addr >= INSN_GET_ADDR (block_get_first_insn (b))
//      &&  addr <= INSN_GET_ADDR (block_get_last_insn  (b)))
//      {
//         ssa_block_t* ssab = fct_get_ssa (f)[b->id]; //((ssa_block_t**)f->ssa)[b->id];
//         FOREACH_INQUEUE (ssab->first_insn, it_insn)
//         {
//            ssa_insn_t* ssain = GET_DATA_T (ssa_insn_t*, it_insn);
//            if (ssain->in != NULL && INSN_GET_ADDR (ssain->in) == addr)
//               return (ssain);
//         }
//         return (NULL);
//      }
//   }
//   return (NULL);
//}
/*
 *
 *
 *
 *
 */
//TODO: remove this unused function? (Sylvain 2014-09-15)
//
//static oprnd_t* _find_mem_oprnd (ssa_insn_t* ssain, int* pos)
//{
//   int i = 0;
//
//   for (i = 0; i < insn_get_nb_oprnds(ssain->in); i++)
//   {
//      oprnd_t* oprnd = insn_get_oprnd (ssain->in, i);
//      if (oprnd_get_type (oprnd) == MEMORY)
//      {
//         if (pos != NULL)
//            *pos = i;
//         return (oprnd);
//      }
//   }
//   return (NULL);
//}
/*
 *
 *
 *
 */
//TODO: remove this unused function? (Sylvain 2014-09-15)
//
//static void _check_component (ssa_var_t* ssav, fct_t* f, int level)
//{
//   if (ssav == NULL)
//      return ;
//   int i = 0;
//
//   if (level == 0)
//   {
//      printf ("---> Checking ");
//      print_SSA_register (ssav, f->asmfile->arch, stdout);
//      printf ("\n");
//   }
//
//   printf ("----");
//   for (i = 0; i < level; i++)
//      printf ("-");
//   printf ("> ");
//   print_SSA_insn (ssav->insn, f->asmfile->arch, stdout);
//   printf ("\n");
//
//   if (ssav->insn == NULL)
//      return ;
//
//   int family = insn_get_family (ssav->insn->in);
//   ssa_insn_t* ssain = ssav->insn;
//
//   if (ssain->in != NULL)
//   {
//      for (i = 0; i < insn_get_nb_oprnds (ssain->in); i++)
//      {
//         oprnd_t* op = insn_get_oprnd (ssain->in, i);
//
//         if (oprnd_is_src (op) == TRUE)
//         {
//            if (oprnd_is_mem (op) == TRUE
//            &&  family != FM_LEA)
//            {
//
//            }
//            else
//            {
//               _check_component (ssain->oprnds[i * 2], f, level + 1);
//               _check_component (ssain->oprnds[i * 2 + 1], f, level + 1);
//            }
//         }
//      }
//   }
//
//   /*
//   if ()
//   {
//
//   }
//   //*/
//}
/*
 *
 *
 *
 */
/*void lcore_fct_mtl (fct_t* fct, int64_t* addrs)
 {
 if (fct == NULL || addrs == NULL)
 return ;
 int i = 0;
 lcore_compute_ssa (fct);
 while (addrs[i] != 0)
 {
 printf ("analyzing instruction at address 0x%"PRIx64"\n", addrs[i]);

 // Get the ssa_insn_t* structure corresponding to the current address
 ssa_insn_t* ssain = _get_ssain_from_addr (fct, addrs[i]);
 printf ("---> ");
 print_SSA_insn (ssain, fct->asmfile->arch, stdout);
 printf ("\n");

 if (ssain == NULL)
 {
 i ++;
 continue;
 }
 // Analyze the instruction
 int mem_pos = 0;
 if (_find_mem_oprnd (ssain, &mem_pos) == NULL)
 {
 i ++;
 continue;
 }
 _check_component (ssain->oprnds[mem_pos * 2], fct, 0);
 _check_component (ssain->oprnds[mem_pos * 2 + 1], fct, 0);
 i ++;
 }
 }*/

typedef struct mtl_cntxt_s {
   // About instructions to analyze
   int* addrs;
   int size;

   // About analysis results
   queue_t* regs;
   queue_t* mem_addrs;
   queue_t* loops;

   // About SSA representation
   ssa_insn_t*** ssa_defs;
   ssa_block_t** ssa_blocks;

   hashtable_t* adfa_values;
   adfa_cntxt_t* adfa_cntxt;

   // Others
   arch_t* arch;
   fct_t* fct;
} mtl_contxt_t;

static void* init(fct_t* f, int64_t* addrs)
{
   mtl_contxt_t* mtl = lc_malloc(sizeof(mtl_contxt_t));
   int size = 0;
   int i = 0;
   while (addrs[size] != 0)
      size++;
   mtl->addrs = lc_malloc((size + 1) * sizeof(int64_t));
   mtl->size = size;
   mtl->arch = f->asmfile->arch;
   mtl->regs = queue_new();
   mtl->mem_addrs = queue_new();
   mtl->loops = queue_new();
   mtl->ssa_blocks = lcore_compute_ssa(f);
   mtl->ssa_defs = fct_get_ssa_defs(f);
   mtl->fct = f;
   mtl->adfa_values = NULL;

   memcpy(mtl->addrs, addrs, (size + 1) * sizeof(int64_t));

   for (i = 0; i < size; i++)
      mtl->addrs[i] = addrs[i];
   mtl->addrs[i] = 0;

   return (mtl);
}

static int insn_filter(ssa_insn_t* ssain, void* user)
{
   int i = 0;
   mtl_contxt_t* mtl = (mtl_contxt_t*) user;

   if (ssain->in == NULL || loop_is_innermost(ssain->in->block->loop) == FALSE)
      return (FALSE);

   while (i < mtl->size) {
      if (INSN_GET_ADDR(ssain->in) == mtl->addrs[i])
         return (TRUE);
      i++;
   }
   return (FALSE);
}

static void _list_regs(adfa_val_t* val, arch_t* arch, queue_t* regs,
      queue_t* addrs, char is_display)
{
   if (val == NULL || arch == NULL)
      return;

   if (is_display && val->op == ADFA_OP_SQRT)
      printf("SQRT (");

   if (is_display && val->is_mem)
      printf("@[");

   switch (val->type) {
   case ADFA_TYPE_IMM:
      if (is_display)
         printf("0x%"PRIx64, val->data.imm);
      break;
   case ADFA_TYPE_REG:
      if (is_display)
         printf("%s_%d",
               arch_get_reg_name(arch, val->data.reg->reg->type,
                     val->data.reg->reg->name), val->data.reg->index);
      if (regs != NULL
            && queue_lookup(regs, &ssa_var_equal, val->data.reg) == NULL)
         queue_add_tail(regs, val->data.reg);
      break;
   case ADFA_TYPE_SONS:
      if (is_display)
         printf("(");
      // print left
      if (val->data.sons[0])
         _list_regs(val->data.sons[0], arch, regs, addrs, is_display);

      // print operator
      if (is_display) {
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
      }

      // print rigth
      if (val->data.sons[1])
         _list_regs(val->data.sons[1], arch, regs, addrs, is_display);
      if (is_display)
         printf(")");
      break;

   case ADFA_TYPE_MEM_MTL:
      if (is_display)
         printf("0x%"PRIx64, val->data.imm);
      if (addrs != NULL
            && queue_lookup(addrs, &direct_equal, (void*) (val->data.imm))
                  == NULL)
         queue_add_tail(addrs, (void*) (val->data.imm));

      break;
   }
   if (is_display && val->is_mem)
      printf("]");
   if (is_display && val->op == ADFA_OP_SQRT)
      printf(")");
}

static void insn_execute(ssa_insn_t* ssain, adfa_val_t* val,
      hashtable_t* values, void* user)
{
   (void) val;
   mtl_contxt_t* mtl = (mtl_contxt_t*) user;
   if (mtl->adfa_values == NULL)
      mtl->adfa_values = values;
   int i = 0;

   if (ssain->in == NULL)
      return;

   if (queue_lookup(mtl->loops, &direct_equal,
         (void*) (ssain->in->block->loop)) == NULL)
      queue_add_tail(mtl->loops, (void*) (ssain->in->block->loop));

   for (i = 0; i < insn_get_nb_oprnds(ssain->in); i++) {
      oprnd_t* op = insn_get_oprnd(ssain->in, i);
      if (oprnd_is_mem(op) == TRUE) {
         printf("---------------------------------------\n");
         printf("\tbinary address = 0x%"PRIx64";\n", ssain->in->address);
         printf("\tmemory address = 0x%"PRIx64" (", oprnd_get_offset(op));
         print_SSA_register(ssain->oprnds[i * 2], mtl->arch, stdout);
         printf(", ");
         print_SSA_register(ssain->oprnds[i * 2 + 1], mtl->arch, stdout);
         printf(", %d);\n\trepresentation = 0x%"PRIx64" + ",
               oprnd_get_scale(op), oprnd_get_offset(op));
         _list_regs(hashtable_lookup(values, ssain->oprnds[i * 2]), mtl->arch,
               mtl->regs, mtl->mem_addrs, TRUE);
         printf(" + (");
         _list_regs(hashtable_lookup(values, ssain->oprnds[i * 2 + 1]),
               mtl->arch, mtl->regs, mtl->mem_addrs, TRUE);
         printf(") * %d;\n\n", oprnd_get_scale(op));
      }
   }
}

static void _lookfor_invariants(ssa_var_t** regs, char* is_reg_invariant,
      mtl_contxt_t* mtl)
{
   int i = 0;

   // Try to find invariants in the innermost loop.
   FOREACH_INQUEUE(mtl->loops, it_l)
   {
      loop_t* l = GET_DATA_T(loop_t*, it_l);
      FOREACH_INQUEUE(l->blocks, it_b)
      {
         block_t* b = GET_DATA_T(block_t*, it_b);
         ssa_block_t* ssab = mtl->ssa_blocks[b->id];

         FOREACH_INQUEUE(ssab->first_insn, it_ssain)
         {
            ssa_insn_t* ssain = GET_DATA_T(ssa_insn_t*, it_ssain);

            for (i = 0; i < queue_length(mtl->regs); i++) {
               ssa_var_t* ssav = regs[i];
               if (ssain->nb_output > 0 && ssain->output[0]->reg == ssav->reg
                     && ssain->output[0]->index == ssav->index) {
                  is_reg_invariant[i] = FALSE;
               }
            }
         }
      }
   }
}

static ind_triple_t** _lookfor_inductions(ssa_var_t** regs,
      char* is_reg_invariant, mtl_contxt_t* mtl)
{
   ind_triple_t** is_reg_induction = lc_malloc0(
         queue_length(mtl->regs) * sizeof(ind_triple_t*));
   ind_context_t* inductions = lcore_compute_function_induction_fromSSA(
         mtl->fct, mtl->ssa_blocks);

   int i = 0;
   for (i = 0; i < queue_length(mtl->regs); i++) {
      if (is_reg_invariant[i] == FALSE) {
         ssa_insn_t* origin =
               mtl->ssa_defs[__regID(regs[i]->reg, mtl->arch)][regs[i]->index];
         if (origin != NULL && origin->in == NULL) {
            int j = 0;
            while (origin->oprnds[j] != NULL) {
               ind_triple_t* ind = hashtable_lookup(
                     inductions->derived_induction, origin->oprnds[j]);
               if (ind != NULL)
                  is_reg_induction[i] = ind;
               j++;
            }
         }
      }
   }
   return (is_reg_induction);
}

static ssa_var_t* __get_compare_induction(ssa_insn_t* ssain, mtl_contxt_t* mtl,
      ssa_var_t** regs, ind_triple_t** is_reg_induction)
{
   insn_t* in = ssain->in;
   int i = 0;

   ADFA_analyze_insn(ssain, mtl->adfa_cntxt);

   for (i = 0; i < insn_get_nb_oprnds(in); i++) {
      oprnd_t* op = insn_get_oprnd(in, i);
      if (oprnd_is_reg(op) == TRUE) {
         char is_induction = FALSE;
         ssa_var_t* ssav = ssain->oprnds[i * 2];
         adfa_val_t* val = hashtable_lookup(mtl->adfa_values, ssav);
         queue_t* elements = queue_new();
         _list_regs(val, mtl->arch, elements, NULL, FALSE);

         // check if the register is an induction
         int j = 0;
         for (j = 0; j < queue_length(mtl->regs); j++) {
            if (is_reg_induction[j] && ssa_var_equal(regs[j], ssav))
               return (ssav);
         }

         // check if all components of the value are inductions
         FOREACH_INQUEUE(elements, it_e) {
            ssa_var_t* e = GET_DATA_T(ssa_var_t*, it_e);
            int j = 0;
            for (j = 0; j < queue_length(mtl->regs); j++) {
               if (is_reg_induction[j] && ssa_var_equal(regs[j], e))
                  is_induction = TRUE;
               else if (is_reg_induction[j] && !ssa_var_equal(regs[j], e)) {
                  is_induction = FALSE;
                  break;
               }

            }
         }
         if (is_induction == TRUE)
            return (ssav);
      }
   }
   return (NULL);
}

static void _lookfor_loopsize(mtl_contxt_t* mtl, ssa_var_t** regs,
      ind_triple_t** is_reg_induction)
{
   printf("**** Look for loops data ****\n");

   FOREACH_INQUEUE(mtl->loops, it_l) {
      loop_t* loop = GET_DATA_T(loop_t*, it_l);

      printf("---> Loop %d\n", loop->global_id);

      FOREACH_INLIST(loop->exits, it_ex)
      {
         block_t* b = GET_DATA_T(block_t*, it_ex);
         ssa_block_t* ssab = mtl->ssa_blocks[b->id];

         FOREACH_INQUEUE_REVERSE(ssab->first_insn, it_in)
         {
            ssa_insn_t* ssain = GET_DATA_T(ssa_insn_t*, it_in);
            insn_t* in = ssain->in;

            if (in == NULL)
               break;

            if (insn_get_family(in) == FM_CMP) {
               ssa_var_t* induction_var = __get_compare_induction(ssain, mtl,
                     regs, is_reg_induction);
               ssa_var_t* limite_var = NULL;

               // If an induction variable has been found, try to find the value of the other register
               if (induction_var != NULL) {

               }

               printf("  Compare instruction : ");
               print_SSA_insn(ssain, mtl->arch, stdout);
               printf("\n");
               printf("  Induction variable  : ");
               print_SSA_register(induction_var, mtl->arch, stdout);
               printf("\n");
               printf("  Limit variable      : ");
               print_SSA_register(limite_var, mtl->arch, stdout);
               printf("\n");

            }
         }
      }
   }
   printf("*****************************\n");
}

void lcore_fct_mtl(fct_t* fct, int64_t* addrs)
{
   if (fct == NULL || addrs == NULL)
      return;

   adfa_driver_t driver;
   driver.flags = (ADFA_NO_UNRESOLVED_SHIFT | ADFA_NO_MEMORY);
   driver.user_struct = init(fct, addrs);
   driver.init = NULL;
   driver.insn_filter = &insn_filter;
   driver.insn_execute = &insn_execute;
   driver.propagate = NULL;

   adfa_cntxt_t* cntxt = ADFA_analyze_function(fct, &driver);
   mtl_contxt_t* mtl = (mtl_contxt_t*) driver.user_struct;
   ssa_var_t** regs = lc_malloc0(sizeof(ssa_var_t*) * queue_length(mtl->regs));
   char* is_reg_invariant = lc_malloc0(sizeof(char) * queue_length(mtl->regs));
   ind_triple_t** is_reg_induction = NULL;
   int i = 0;
   mtl->adfa_cntxt = cntxt;

   // Convert the register queue in a register array
   FOREACH_INQUEUE(mtl->regs, it_1) {
      ssa_var_t* ssav = GET_DATA_T(ssa_var_t*, it_1);
      regs[i] = ssav;
      is_reg_invariant[i] = TRUE;
      i++;
   }

   // Look for invariants in found registers
   _lookfor_invariants(regs, is_reg_invariant, mtl);

   // Look for inductions in found registers
   is_reg_induction = _lookfor_inductions(regs, is_reg_invariant, mtl);

   // Look for loop size
   _lookfor_loopsize(mtl, regs, is_reg_induction);

   printf("\n-----------------------\n");
   printf("Registers to track:\n");
   for (i = 0; i < queue_length(mtl->regs); i++) {
      ssa_var_t* ssav = regs[i];
      printf("  -- ");
      print_SSA_register(ssav, mtl->arch, stdout);
      printf("\t===> ");
      print_SSA_insn(mtl->ssa_defs[__regID(ssav->reg, mtl->arch)][ssav->index],
            mtl->arch, stdout);
      if (is_reg_invariant[i] == TRUE)
         printf("\t[INVARIANT]");
      else if (is_reg_induction[i] != NULL) {
         printf("\t[INDUCTION :: ");
         print_induction_triple(is_reg_induction[i], mtl->arch, stdout);
         printf("]");
      }
      printf("\n");
   }

   printf("\nMemory addresses to track:\n");
   FOREACH_INQUEUE(mtl->mem_addrs, it0) {
      int64_t add = GET_DATA_T(int64_t, it0);
      printf("  -- 0x%"PRIx64"\n", add);
   }
   printf("\n");
}
