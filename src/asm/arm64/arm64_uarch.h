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
 * \file arm64_uarch.h
 *
 * \date 25 oct. 2016
 */

#ifndef ARM64_UARCH_H_
#define ARM64_UARCH_H_

#include "libmasm.h"

/** Definition of the structure containing the results of the CPUINFO command. For now, using int64_t*/
/**\note (2016-11-22) If these structures must become more complex, replace the type here and update their declarations in arm64_uarch.c
 * accordingly while keeping the same names*/
typedef int64_t arm64_cpuid_code_t;

/** \note (2016-11-16) For clarity, it is advised to keep the processor identifiers grouped by the micro-architecture
  * to which they belong, and to order the micro-architectures in the same order as in the arm64_uarchs_e enumeration below
 * */
//@const_start
/* Identifiers of the processor variants for arm64 */
enum arm64_procs_e {
   arm64_PROC_NONE = 0,/**<No processor variant*/
   //Processor IDs for the Cortex A57 micro-architecture
   //No processor that I could find!
   arm64_PROC_CORTEX_A57_PROC0, /**< Cortex A57, processor variant 0*/
//   arm64_PROC_CORTEX_A57_PROCX, /**< Cortex A57, processor variant X*/
   //Processor IDs for the Cortex XXX micro-architecture
//   arm64_PROC_CORTEX_XXX_PROC0, /**< Cortex XXX, processor variant 0 */
   arm64_PROC_MAX     /**< Max number of processors variants, must always be last*/
};

//@const_start
/* Identifiers of the micro-architectures for arm64 */
enum arm64_uarchs_e {
   arm64_UARCH_NONE = 0,
   arm64_UARCH_CORTEX_A57,  /**< Cortex A57 */
//   arm64_UARCH_CORTEX_XXX,  /**< Cortex XXX */
   arm64_UARCH_MAX             /**< Max number of micro-architectures, must always be last*/
};
//@const_stop

#endif /* ARM64_UARCH_H_ */
