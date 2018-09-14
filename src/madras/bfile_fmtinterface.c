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

#include "bfile_fmtinterface.h"
#include "libmasm.h"
/*** Format-specific headers ***/
#include "libmtroll.h"  //ELF
#include "libmworm.h"   //Mach-O
#include "libstone.h"   //PE
// Include headers for other formats as needed
/*******************************/

/** Supported formats **/
//Update this array according with the supported formats. Last element must always be NULL
binfile_loadfct_t binfile_loaders[] = { elf_binfile_load, /*ELF parser*/
macho_binfile_load,/*Mach-O parser*/
pe_binfile_load,/*PE parser*/
NULL };

/*
 * Loads a binary file structure with the results of the parsing of the file name contained in the structure
 * \param bf The structure representing the binary file. Its \c filename member must be filled with the name of the file
 * \return EXIT_SUCCESS if the file could be successfully parsed, error code otherwise.
 * */
int binfile_load(binfile_t* bf)
{
   int result = ERR_BINARY_FORMAT_NOT_RECOGNIZED;
   int i = 0;
   while (ISERROR(result) && (binfile_loaders[i] != NULL)) {
      result = binfile_loaders[i](bf);
      i++;
   }
   return result;
}

