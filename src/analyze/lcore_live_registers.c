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

//
// This code implements the live variable analysis algorithm presented
// in "Compilers, principles, techniques,& tools" (Aho, Lam, Sethi, Ullman)
//
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "libmcommon.h"
#include "libmasm.h"
#include "libmcore.h"
#include "arch.h"

#ifdef _ARCHDEF_arm64
extern int arm64_cs_regID(reg_t* X, arch_t* A);
extern int arm64_lcore_get_nb_registers (arch_t* arch);
#endif

/* ************************************************************************* *
 *                               Internals
 * ************************************************************************* */
#define USE_FLAG 1   /**<Flag indicating that the variable belongs to USE set*/
#define DEF_FLAG 2   /**<Flag indicating that the variable belongs to DEF set*/

/**
 * Returns an id corresponding to a register
 * Used in Live register analysis and SSA computation
 * \param X a register (reg_t*)
 * \param A an architecture (arch_t*)
 * \return an int with the register id
 */
int __regID(reg_t* X, arch_t* A)
{
   return (
         (X == A->reg_rip) ?
               0 :
               ((A->reg_families[(int) X->type] * A->nb_names_registers)
                     + X->name + 1));
}

typedef int (*regid_f)(reg_t*, arch_t*);

/*
 * Return function to get registers ID for the given arch
 */
static regid_f arch_regid(arch_t *arch, char mode)
{
   if (mode == FALSE)
      return &__regID;
   else {
      if (arch->code == ARCH_arm64) {
#ifdef _ARCHDEF_arm64
         return &arm64_cs_regID;
#endif
      } 
   }
   return NULL;
}

/*
 * Returns a register corresponding to an id
 * \param id a register id
 * \param A an architecture (arch_t*)
 * \return the register corresponding to the id
 */
reg_t* __IDreg(int id, arch_t* A)
{
   if (id == 0)
      return (A->reg_rip);

   id = id - 1;
   int family = id / A->nb_names_registers;
   int name = id - (family * A->nb_names_registers);

   int type = 0;
   while (A->reg_families[type] != family)
      type++;

   while (type + 1 < A->nb_type_registers
         && A->reg_families[type + 1] == A->reg_families[type])
      type++;
   return (A->regs[type][name]);
}

void lcore_compute_use_def_in_block(block_t* b, char** UseDef, char mode)
{
   arch_t* arch = b->function->asmfile->arch;
   int (*_regID)(reg_t*, arch_t*) = arch_regid(arch,mode);

   insn_t* in = NULL;
   int i = 0;
   oprnd_t* op = NULL;
   reg_t* V = NULL;
   reg_t** implicits = NULL;
   int nb_implicits = 0;

   FOREACH_INSN_INBLOCK(b, it_in)
   {
      in = GET_DATA_T(insn_t*, it_in);

      // Handle calls to external functions. Based on the AMD64 System V ABI.
      if (((insn_get_annotate(in) & A_CALL) != 0)) {
         int i;
         for (i = 0; i < arch->nb_arg_regs; i++) {
            reg_t* V = arch->arg_regs[i];
            if ((UseDef[b->id][_regID(V, arch)] & DEF_FLAG) == 0) {
               UseDef[b->id][_regID(V, arch)] |= USE_FLAG;
               DBGMSG("Call: Use(%d) += %s\n", b->global_id,
                     arch_get_reg_name(arch, V->type, V->name));
            }
         }
         for (i = 0; i < arch->nb_return_regs; i++) {
            reg_t* V = arch->return_regs[i];
            if ((UseDef[b->id][_regID(V, arch)] & USE_FLAG) == 0) {
               UseDef[b->id][_regID(V, arch)] |= DEF_FLAG;
               DBGMSG("Call: Def(%d) += %s\n", b->global_id,
                     arch_get_reg_name(arch, V->type, V->name));
            }
         }
      }

      // -------------------------------------------------------------------
      // Use: Iterate over operands to get registers used before to be defined
      for (i = 0; i < insn_get_nb_oprnds(in); i++) {
         op = insn_get_oprnd(in, i);
         if (oprnd_is_src(op) || oprnd_is_mem(op)) {
            switch (oprnd_get_type(op)) {
            case OT_REGISTER:
            case OT_REGISTER_INDEXED:
               V = oprnd_get_reg(op);
               if ((UseDef[b->id][_regID(V, arch)] & DEF_FLAG) == 0) {
                  UseDef[b->id][_regID(V, arch)] |= USE_FLAG;
                  DBGMSG("Use(%d) += %s\n", b->global_id,
                        arch_get_reg_name(arch, V->type, V->name));
               }
               break;

            case OT_MEMORY:
            case OT_MEMORY_RELATIVE:
               if (oprnd_get_base(op)) {
                  V = oprnd_get_base(op);
                  if ((UseDef[b->id][_regID(V, arch)] & DEF_FLAG) == 0) {
                     UseDef[b->id][_regID(V, arch)] |= USE_FLAG;
                     DBGMSG("Use(%d) += %s\n", b->global_id,
                           arch_get_reg_name(arch, V->type, V->name));
                  }
               }

               if (oprnd_get_index(op)) {
                  V = oprnd_get_index(op);
                  if ((UseDef[b->id][_regID(V, arch)] & DEF_FLAG) == 0) {
                     UseDef[b->id][_regID(V, arch)] |= USE_FLAG;
                     DBGMSG("Use(%d) += %s\n", b->global_id,
                           arch_get_reg_name(arch, V->type, V->name));
                  }
               }
               break;
            default:
               break;   //To avoid compilation warnings
            }
         }
      }
      implicits = b->function->asmfile->arch->get_implicite_src(arch,
            insn_get_opcode_code(in), &nb_implicits);
      for (i = 0; i < nb_implicits; i++) {
         V = implicits[i];
         if ((UseDef[b->id][_regID(V, arch)] & DEF_FLAG) == 0) {
            UseDef[b->id][_regID(V, arch)] |= USE_FLAG;
            DBGMSG("Use(%d) += %s\n", b->global_id,
                  arch_get_reg_name(arch, V->type, V->name));
         }
      }
      if (implicits != NULL)
         lc_free(implicits);

      // -------------------------------------------------------------------
      // Def: Iterate over operands to get registers defined before to be used
      for (i = 0; i < insn_get_nb_oprnds(in); i++) {
         op = insn_get_oprnd(in, i);
         if (oprnd_is_dst(op) && oprnd_is_reg(op)) {
            reg_t* V = oprnd_get_reg(op);
            if ((UseDef[b->id][_regID(V, arch)] & USE_FLAG) == 0) {
               UseDef[b->id][_regID(V, arch)] |= DEF_FLAG;
               DBGMSG("Def(%d) += %s\n", b->global_id,
                     arch_get_reg_name(arch, V->type, V->name));
            }
         }
      }
      implicits = b->function->asmfile->arch->get_implicite_dst(arch,
            insn_get_opcode_code(in), &nb_implicits);
      for (i = 0; i < nb_implicits; i++) {
         V = implicits[i];
         if ((UseDef[b->id][_regID(V, arch)] & USE_FLAG) == 0) {
            UseDef[b->id][_regID(V, arch)] |= DEF_FLAG;
            DBGMSG("Def(%d) += %s\n", b->global_id,
                  arch_get_reg_name(arch, V->type, V->name));
         }
      }
      if (implicits != NULL)
         lc_free(implicits);

   }
}

/*
 * Compute Use and Def sets for each basic block
 * \param f a function
 * \param UseDef an array used to return Use and Def sets. Its structure
 *               is detailed in lcore_compute_live_registers
 */
static void _compute_use_def(fct_t* f, char** UseDef, char mode)
{
   arch_t* arch = f->asmfile->arch;
   block_t* b = NULL;
   int i = 0;
   block_t* entry = fct_get_main_entry(f);
   int (*_regID)(reg_t*, arch_t*) = arch_regid(arch,mode);

   for (i = 0; i < arch->nb_arg_regs; i++) {
      reg_t* V = arch->arg_regs[i];
      if ((UseDef[entry->id][_regID(V, arch)] & DEF_FLAG) == 0) {
         UseDef[entry->id][_regID(V, arch)] |= USE_FLAG;
         DBGMSG("Entry: Use(%d) += %s\n", entry->global_id,
               arch_get_reg_name(arch, V->type, V->name));
      }
   }

   // Iterate over function instructions block per block
   FOREACH_INQUEUE(f->blocks, it_b)
   {
      b = GET_DATA_T(block_t*, it_b);
      lcore_compute_use_def_in_block(b, UseDef, mode);
   }
}

/*
 * Computes IN and OUT sets for a given function
 * \param f a function
 * \param UseDef USE / DEF sets. Computed using _compute_use_def
 * \param InOut Used to return IN and OUT sets.
 * \param nb_reg number of registers (second dimension of UseDef or InOut)
 */
static void _compute_in_out(fct_t* f, char** UseDef, char**InOut, int nb_reg,
      int mode)
{
   int changes = TRUE;
   int i = 0;
   arch_t* arch = f->asmfile->arch;
   int (*_regID)(reg_t*, arch_t*) = arch_regid(arch,mode);

   // Handle exits. Based on the AMD64 System V ABI.
   FOREACH_INQUEUE(f->blocks, exit_b) {
      block_t* b = GET_DATA_T(block_t*, exit_b);

      FOREACH_INSN_INBLOCK(b, it_in_exit)
      {
         insn_t* in = GET_DATA_T(insn_t*, it_in_exit);
         if (insn_get_annotate(in) & A_EX) {
            for (i = 0; i < arch->nb_return_regs; i++) {
               reg_t* V = arch->return_regs[i];
               InOut[b->id][_regID(V, arch)] |= OUT_FLAG;
               DBGMSG("Exit: OUT(%d) += %s\n", b->global_id,
                     arch_get_reg_name(arch, V->type, V->name));
            }
            break;
         }
      }
   }

   while (changes) {
      changes = FALSE;
      FOREACH_INQUEUE(f->blocks, it_b)
      {
         block_t* b = GET_DATA_T(block_t*, it_b);

         // ----------------------------------------------------------------
         // OUT(B) = U In(S), S a successor of B
         FOREACH_INLIST(b->cfg_node->out, it_ed) {
            graph_edge_t* ed = GET_DATA_T(graph_edge_t*, it_ed);
            block_t* S = ed->to->data;

            for (i = 0; i < nb_reg; i++)
               if (InOut[S->id][i] & IN_FLAG)
                  InOut[b->id][i] |= OUT_FLAG;
         }

         // ----------------------------------------------------------------
         // IN(B) = Use(B) U (Out(B) - Def(B))
         for (i = 0; i < nb_reg; i++) {
            if ((UseDef[b->id][i] & USE_FLAG)
                  || ((UseDef[b->id][i] & DEF_FLAG) == 0
                        && (InOut[b->id][i] & OUT_FLAG))) {
               if ((InOut[b->id][i] & IN_FLAG) == 0)
                  changes = TRUE;
               InOut[b->id][i] |= IN_FLAG;
            }
         }
      }
   }
}

/*
 * Compute the number of registers found in the architecture. If two registers
 * have the same name but different families, they are considered as the same
 * register.
 * \param arch an architecture
 * \return the number of registers.
 */
int lcore_get_nb_registers(arch_t* arch)
{
   if (arch == NULL)
      return 0;
   int i = 0;
   int nb_families = 1;
   for (i = 1; i < arch->nb_type_registers; i++)
      if (arch->reg_families[i - 1] != arch->reg_families[i])
         nb_families++;

   return (nb_families * arch->nb_names_registers);
}

/*
 * Computes live registers in a given function
 * \param fct a function to analyze
 * \param nb_reg used to return the number of registers
 * \return NULL if problem, else an array containing IN and OUT sets.
 *         The returned array has fct_get_nb_blocks() entries and each entry
 *         is an array of nb_reg char. Basically, ret[i] contains IN and OUT
 *         sets for block whose id is i, and ret[i][j] contains IN and OUT
 *         data for register whose id is j. The register id is available using
 *         __regID() macro. This id is defined only for this analysis.
 *         Each register data is a set of flags. Possible values are
 *         IN_FLAG and OUT_FLAGS. If ((ret[i][j] & IN_FLAG) != 0), this means
 *         the variable j belongs to b IN set. It is the same for OUT set.
 */
char** lcore_compute_live_registers(fct_t* fct, int* nb_reg, char mode)
{
   if (fct == NULL) {
      *nb_reg = 0;
      return NULL;
   }
   fct_upd_blocks_id(fct);
   fct->asmfile->free_live_registers = &lcore_free_live_registers;

   // -------------------------------------------------------------------------
   // Define and initialize variables
   char ** UseDef = NULL;        // Array of flags. UseDef[i] contains Use / Def
                                 // for the block i and UseDef[i][j] is the flag
                                 // for standardized variable whose 'id' is
                                 // (family(reg) * nb_reg_name + name(reg) + 1)
   char ** InOut = NULL;         // Array of flags. Same structure than useDef
   arch_t* arch = fct->asmfile->arch;

   // Computes the number of register families (used to allocate Use / Def structures)
   if (mode == FALSE)
      (*nb_reg) = lcore_get_nb_registers(arch);
   else {
      if (arch->code == ARCH_arm64) {
#ifdef _ARCHDEF_arm64
         (*nb_reg) = arm64_lcore_get_nb_registers(arch);
#else
         return NULL;
#endif
      }
   }

   // If live registers have already been computed, just return them
   if (fct->live_registers != NULL)
      return (fct->live_registers);

   UseDef = lc_malloc0(fct_get_nb_blocks(fct) * sizeof(char*));
   InOut = lc_malloc0(fct_get_nb_blocks(fct) * sizeof(char*));

   FOREACH_INQUEUE(fct->blocks, it_b0) {
      block_t* b = GET_DATA_T(block_t*, it_b0);
      UseDef[b->id] = lc_malloc0((*nb_reg) * sizeof(char));
      InOut[b->id] = lc_malloc0((*nb_reg) * sizeof(char));
   }

   // -------------------------------------------------------------------------
   // Compute USE and DEF for each block in the function
   _compute_use_def(fct, UseDef, mode);

   // -------------------------------------------------------------------------
   // Compute IN and OUT for each block in the function
   _compute_in_out(fct, UseDef, InOut, *nb_reg, mode);

   // -------------------------------------------------------------------------
   // Free memory
   FOREACH_INQUEUE(fct->blocks, it_b2) {
      block_t* b = GET_DATA_T(block_t*, it_b2);
      lc_free(UseDef[b->id]);
   }
   lc_free(UseDef);

   fct->live_registers = InOut;
   return (InOut);
}

/*
 * Frees live register analysis results
 * \param fct a function
 */
void lcore_free_live_registers(fct_t* fct)
{
   if (fct != NULL && fct->live_registers != NULL) {
      FOREACH_INQUEUE(fct->blocks, it_b) {
         block_t* b = GET_DATA_T(block_t*, it_b);
         lc_free(fct->live_registers[b->id]);
      }
      lc_free(fct->live_registers);
      fct->live_registers = NULL;
   }
}
