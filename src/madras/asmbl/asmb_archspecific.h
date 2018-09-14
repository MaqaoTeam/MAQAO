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
 * \file patcher_archspecific.h
 *
 * \date 5 sept. 2011
 */
/**
 * \note This file must not contain defines to avoid double inclusion, as it is intended to be included in
 * a file and its header. The PRINT_HEADER and PRINT_SOURCE defines take care of avoiding double declaration
 */

#ifdef CURRENT_ARCH

#ifdef PRINT_HEADER
#include "libmcommon.h"
#include "libmasm.h"
#endif //PRINT_HEADER

#define ARCHPREFIX(X) MACROVAL_CONCAT(CURRENT_ARCH,X)

/**
 * Assembling an instruction for a given architecture
 * \param in The instruction. At least its opcode code must be set
 * \return The coding of the instruction, or NULL if an error occurred or if \c in is NULL
 * */
#ifdef PRINT_HEADER
extern
#endif //PRINT_HEADER
bitvector_t* ARCHPREFIX(_insn_gencoding)(insn_t* in)
#ifdef PRINT_SOURCE
{
   bitvector_t* out = NULL;

   DBG(char buf[256];insn_print(in, buf, sizeof(buf)); DBGMSG("Assembling instruction %s\n",buf));
   if ( ( ARCHPREFIX(_asmblinsns) != NULL ) && ( in != NULL )
         && ( insn_get_opcode_code(in) < ARCHPREFIX(_arch).size_opcodes ) ) {
      out = insnsym_encode(ARCHPREFIX(_asmblinsns)[ insn_get_opcode_code(in) ], in, BDFVar__VARNUMBER, BDFVar__NUMBER - BDFVar__VARNUMBER);
   }
   DBG(char buf[256];bitvector_hexprint(out, buf, sizeof(buf), " ");DBGMSG("Instruction has coding: %s\n",buf));

   return out;
}
#endif //PRINT_SOURCE
#ifdef PRINT_HEADER
;
#endif //PRINT_HEADER

/**
 * Architecture-specific getter of the arch_t structure for a given architecture
 * \return A pointer to the arch_t structure defining the given architecture
 * */
#ifdef PRINT_HEADER
extern
#endif //PRINT_HEADER
arch_t* ARCHPREFIX(_getasmarch)()
#ifdef PRINT_SOURCE
{
   return &ARCHPREFIX(_arch);
}
#endif //PRINT_SOURCE
#ifdef PRINT_HEADER
;
#endif //PRINT_HEADER

#undef ARCHPREFIX
#endif //CURRENT_ARCH
