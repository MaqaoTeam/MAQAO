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

/*
 * \file dsmb_archinterface.h
 * \brief This file contains the functions used to register architecture-specific functions
 *
 * \date 6 sept. 2011
 */

#ifndef DSMB_ARCHINTERFACE_H_
#define DSMB_ARCHINTERFACE_H_

#include "fsmutils.h"
#include "arch.h"

/**
 * \brief Architecture disassembler driver
 * Holds the pointer to all the functions needed by the disassembler and
 * specific to the architecture
 * */
typedef struct dsmbldriver_s {
   void (*fsmloader)(fsmload_t*); /**<Initialises the fsm.*/
   arch_t* (*getarch)(void); /**<Retrieves the architecture.*/
   int (*switchfsm)(asmfile_t*, int64_t, int64_t*, list_t**); /**<Switch to an fsm for another architecture*/
} dsmbldriver_t;

/**
 * Frees a driver and closes its link to the relevant library file
 * \param d The driver to free
 * */
extern void dsmbldriver_free(dsmbldriver_t *d);

/**
 * Creates a new disassembly driver structure and initializes its function pointers from the library
 * relevant to the given architecture
 * \param arch The structure representing the architecture the disassembler driver of which we have to load
 * \return The new driver for the given architecture
 * */
extern dsmbldriver_t *dsmbldriver_load(arch_t* arch);

/**
 * Creates a new disassembly driver structure and initializes its function pointers from the library
 * relevant to the given architecture
 * \param arch The code of the architecture we have to load (as defined in arch.h)
 * \return The new driver for the given architecture
 * */
extern dsmbldriver_t *dsmbldriver_load_byarchcode(arch_code_t archcode);

/**
 * Creates a new driver structure and initializes its function pointers from the library
 * relevant to the given architecture
 * \param archname The name of the architecture we have to load
 * \return The new driver for the given architecture
 * */
extern dsmbldriver_t *dsmbldriver_load_byarchname(char* archname);

#endif /*DSMB_ARCHINTERFACE_H_*/
