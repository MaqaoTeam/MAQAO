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
 * \brief This file contains an exhaustive list of the supported architectures
 * The declaration of a new architecture must be done as follows:
 * \code
 * #ifdef _ARCHDEF_<architecture name>
 * ARCH_ASMBL(<architecture name>)
 * #endif
 * \endcode
 * where:
 * - \<architecture name\> is the internal name of the architecture. For coherence, it is advised that this
 * name be the same as the one in the ISA description file and the name of the directory containing the files
 * specific to the architecture. It should also be the same name as the one used for option in the configure script
 *
 * \date 20 sept. 2011
 */
