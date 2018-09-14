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
 * \file bfile_fmtinterface.h
 * \brief Contains interface for accessing functions specific to a binary format
 * \date 2014-04-23
 * */

#ifndef BFILE_FMTINTERFACE_H_
#define BFILE_FMTINTERFACE_H_

#include "libmasm.h"

/**
 * Loads a binary file structure with the results of the parsing of the file name contained in the structure
 * \param bf The structure
 * \return EXIT_SUCCESS if the file could be successfully parsed, error code otherwise.
 * */
extern int binfile_load(binfile_t* bf);

#endif /* BFILE_FMTINTERFACE_H_ */
