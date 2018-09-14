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
 * \file archinterface.h
 * \brief This file contains definitions of functions allowing to retrieve an architecture structure depending on its code
 * under the various supported binary formats
 * \date 28 avr. 2014
 */

#ifndef ARCHINTERFACE_H_
#define ARCHINTERFACE_H_

#include "libmasm.h"

/**
 * NULL-terminated array of pointers to the structures describing the architectures supported by MAQAO
 * */
extern arch_t* MAQAO_archs[];

/**
 * Returns the architecture structure corresponding to a given code for a given binary format
 * \param bincode Identifier of the binary format (as defined in libmasm.h)
 * \param archcode Code of the architecture in this format (as defined in the relevant headers and referenced in supportedarchs.h)
 * \return A pointer to the structure representing the architecture, or NULL if the code or binary format is unknown
 * */
extern arch_t* getarch_bybincode(bf_format_t bincode, uint16_t archcode);

/**
 * Returns the architecture structure corresponding to a given name
 * \param archname Name of the architecture, as defined in its MINJAG binary definition file
 * \return A pointer to the structure representing the architecture, or NULL if the name is unknown
 * */
extern arch_t* getarch_byname(char* archname);

/**
 * Returns the architecture structure corresponding to a given code
 * \param archcode Code of the architecture, as defined arch.h
 * \return A pointer to the structure representing the architecture, or NULL if the code is unknown
 * */
extern arch_t* getarch_bycode(uint16_t archcode);

/**
 * Returns the architecture associated to a file, with minimal parsing or disassembling
 * \param filename Path to a binary file
 * \return Structure representing the architecture for which this file is compiled, or NULL
 * if \c filename is an invalid path, if the file is not a binary file with a format supported by
 * MAQAO, or if the architecture for which it is compiled is not supported by MAQAO
 * */
extern arch_t* file_get_arch(char* filename);

#endif /* ARCHINTERFACE_H_ */
