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
 * \todo TODO (2014-11-13) Update documentation to reflect split between asmb_archinterface and patch_archinterface
 *
 * \file asmb_archinterface.h
 * \brief This file contains the functions used to register architecture-specific functions
 *
 * \date 20 sept. 2011
 */

#ifndef ASMB_ARCHINTERFACE_H_
#define ASMB_ARCHINTERFACE_H_

#include "libmcommon.h"
#include "libmasm.h"
#include "arch.h"

/**
 * \brief Architecture assembler driver
 * Holds the pointer to all the functions needed by the assembler and
 * specific to the architecture
 * */
typedef struct asmbldriver_s {
   bitvector_t* (*insn_gencoding)(insn_t*); /**< Function used to generate the coding of an instruction*/
   arch_t* (*getarch)(void); /**<Retrieves the architecture.*/
} asmbldriver_t;
/**\todo TODO (2014-07-08) We may get rid of the need of getarch by creating a function that return the arch_t structure from its name
 * Alternatively, we may simply consider that the encoding function will fill the architecture into the instruction and the we will be
 * able to access it that way. (the problem with the code -> name function is that we would need to know if the architecture exists)*/

/**
 * Creates a new driver structure and initializes its function pointers from the library
 * relevant to the given architecture
 * \param arch The architecture for which we have to load a driver
 * \return The new driver for the given architecture
 * */
extern asmbldriver_t *asmbldriver_load(arch_t* arch);

/**
 * Creates a new driver structure and initializes its function pointers from the library
 * relevant to the given architecture
 * \param archname The name of the architecture we have to load (as present in an arch_t structure)
 * \return The new driver for the given architecture
 * */
extern asmbldriver_t *asmbldriver_load_byarchname(char* archname);

/**
 * Creates a new assembly driver structure and initializes its function pointers from the library
 * relevant to the given architecture
 * \param arch The structure representing the architecture the assembler driver of which we have to load (as defined in arch.h)
 * \return The new driver for the given architecture
 * */
extern asmbldriver_t *asmbldriver_load_byarchcode(arch_code_t archcode);

/**
 * Frees a driver and closes its link to the relevant library file
 * \param d The driver to free
 * */
extern void asmbldriver_free(asmbldriver_t *d);

#endif /* ASMB_ARCHINTERFACE_H_ */
