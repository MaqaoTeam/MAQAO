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

#ifndef __ARCH_H_
#define __ARCH_H_
//@const_start

//
// Definition of architectures
// Architectures codes should be obey the following naming convention:
// ARCH_<archname>, where <archname> is the name used for the architecture in its ISA description file
//
typedef enum arch_code_e {
   ARCH_NONE = 0, // No architecture
   ARCH_arm64,    // ARM64
   ARCH_MAXCODES  // Max number of architectures
} arch_code_t;
//@const_stop

#endif
