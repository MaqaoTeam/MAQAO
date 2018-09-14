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
 * \file arm64_uarch.c
 *
 * \date 25 oct. 2016
 */


#include "arm64_arch.h"
#include "arm64_uarch.h"

/** Handy macro for computing the number of elements in an array
 * \param A The array*/
#define ARRAY_NB_ELTS(A) sizeof(A)/sizeof(*A)

/** Definitions relative to the Cortex A57 micro-architecture   */
/*******************************************************************/
#ifndef UARCH_EXCLUDE_ARM64_CORTEX_A57
uarch_t arm64_uarch_cortex_a57;
/** List of instruction sets for the Cortex A57 micro-architecture*/
uint8_t arm64_isets_cortex_a57_proc0[] = { ISET_ARM64 };

/** Declaration of the structures containing the cpuid values*/
static arm64_cpuid_code_t id_cortex_a57_proc0 = 0x0d07;

/** Definitions of processors for the Cortex A57 micro-architecture*/
proc_t arm64_proc_cortex_a57_proc0 = {&arm64_uarch_cortex_a57, "Cortex A57 variant 0", "CORTEX_A57_PROC0", &id_cortex_a57_proc0, arm64_isets_cortex_a57_proc0, ARRAY_NB_ELTS(arm64_isets_cortex_a57_proc0), arm64_PROC_CORTEX_A57_PROC0};

/** List of processors for the Cortex A57 micro-architecture*/
proc_t *arm64_procs_cortex_a57[] = {
      &arm64_proc_cortex_a57_proc0
};

/** Definition of the Cortex A57 micro-architecture*/
uarch_t arm64_uarch_cortex_a57 = {&arm64_arch, "Cortex A57", "CORTEX_A57", "", arm64_procs_cortex_a57, ARRAY_NB_ELTS(arm64_procs_cortex_a57), arm64_UARCH_CORTEX_A57};        /**< Cortex A57 */
#endif //UARCH_EXCLUDE_ARM64_CORTEX_A57

/**Array of processor variant descriptions for arm64, indexed by their identifiers*/
/**\warning The order of the processors in this array must be the same as in the arm64_procs_e enum
 * Also, any addition of a processor definition to a micro-arch (inside the #ifndef UARCH_EXCLUDE...) must be mirrored
 * into the corresponding #else case as a NULL value*/
proc_t *arm64_procs[arm64_PROC_MAX] = {
      NULL
#ifndef UARCH_EXCLUDE_ARM64_CORTEX_A57
      ,&arm64_proc_cortex_a57_proc0
      //Add other processor version for the micro-architecture here, and a NULL in the #else case
#else
      ,NULL
#endif
};

/**Array of micro-architecture descriptions for arm64, indexed by their identifiers*/
/**\warning The order of the micro-architectures in this array must be the same as in the arm64_uarchs_e enum*/
uarch_t *arm64_uarchs[arm64_UARCH_MAX] = {
      NULL
#ifndef UARCH_EXCLUDE_ARM64_CORTEX_A57
      ,&arm64_uarch_cortex_a57
#else
      ,NULL
#endif
};

arch_specs_t arm64_arch_specs = { arm64_uarchs, arm64_procs, NULL, arm64_UARCH_MAX, arm64_PROC_MAX};
