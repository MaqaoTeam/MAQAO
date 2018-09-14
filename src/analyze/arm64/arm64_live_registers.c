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

#include "libmcommon.h"
#include "arm64_arch.h"
#include "libmasm.h"

/**
 * Returns an id corresponding to a register
 * Used in Live register analysis and SSA computation
 * \param X a register (reg_t*)
 * \param A an architecture (arch_t*)
 * \return an int with the register id
 */
int arm64_cs_regID(reg_t* X, arch_t* A)
{
   if (X == A->reg_rip)
      return 0;

   int family = A->reg_families[(int) X->type];
   int size = A->reg_sizes[(int) X->type];

   if (family == GENREG)
      return ((family * A->nb_names_registers) + X->name + 1);
   else
      return (((family + 2) * A->nb_names_registers) + X->name + 1);

}

/**
 * Returns a register corresponding to an id
 * \param id a register id
 * \param A an architecture (arch_t*)
 * \return the register corresponding to the id
 */
reg_t* arm64_cs_IDreg(int id, arch_t* A)
{
   if (id == 0)
      return (A->reg_rip);

   id = id - 1;
   int family = id / A->nb_names_registers;
   int name = id - (family * A->nb_names_registers);
   if (family > (SSEREG + 1))
      family = family - 2;
   else if (family > SSEREG)
      family = family - 1;

   int type = 0;
   while (A->reg_families[type] != family)
      type++;

   while (type + 1 < A->nb_type_registers
         && A->reg_families[type + 1] == A->reg_families[type])
      type++;

   return (A->regs[type][name]);
}

/**
 * Compute the number of registers found in the architecture. If two registers
 * have the same name but different families, they are considered as the same
 * register (ex: RAX and EAX in x86_64).
 * \param arch an architecture
 * \return the number of registers.
 */
int arm64_lcore_get_nb_registers(arch_t* arch)
{
   if (arch == NULL)
      return 0;
   int i = 0;
   int nb_families = 2;
   for (i = 1; i < arch->nb_type_registers; i++)
      if (arch->reg_families[i - 1] != arch->reg_families[i])
         nb_families++;

   return (nb_families * arch->nb_names_registers);
}

