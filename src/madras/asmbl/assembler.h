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
 * \file assembler.h
 * \brief This file contains all the functions needed to assemble
 * ASM instructions into an ELF file
 * */
/**
 * \page asmbl_main Assembler documentation page
 * \todo Write this section
 *
 * \subpage asmblutils
 *
 * \subpage asmbl_x86_64_macros
 *
 * \subpage asmbl_k1om_macros
 * */

#ifndef ASSEMBLER_H_
#define ASSEMBLER_H_

#include "libmcommon.h"
#include "libmasm.h"
#include "asmb_archinterface.h"

/**
 * Assemble an instruction and sets its coding
 * \param in The instruction. At least its opcode must be set, but not its coding (must use upd_assemble_insn instead)
 * \param driver The driver for the architecture the instruction belongs to
 * \return
 * - EXIT_SUCCESS if the instruction could be correctly assembled. Its coding will have been set
 * - Error code if the instruction could not be correctly assembled or if its coding was not NULL.
 * */
extern int assemble_insn(insn_t* in, asmbldriver_t* driver);

/**
 * Assemble the given instructions
 * \param driver Driver to the corresponding architecture
 * \param insnlist Queue of instructions
 * \return EXIT_SUCCESS if the assembly succeeded for all instructions, error code otherwise
 * */
extern int assemble_list(asmbldriver_t* driver, queue_t* insnlist);

/**
 * Assemble the given instructions
 * \param driver Driver to the corresponding architecture
 * \param insnlist_string Instruction list (separated by '\n' characters). See \ref insnlist_parsenew for the accepted format
 * \param asmfile The asmfile with the architecture the instruction belongs to
 * \param insnlist Queue of instructions
 * \return EXIT_SUCCESS if the assembly succeeded for all instructions, error code otherwise
 * */
extern int assemble_strlist(asmbldriver_t* driver, char* insnlist_string,
      asmfile_t* asmfile, queue_t** insnlist);

/**
 * Assemble the given instructions for an architecture
 * \param insnlist_string Instruction list (separated by '\n' characters). See \ref insnlist_parsenew for the accepted format
 * \param arch The structure representing the architecture
 * \param archname The name of the architecture the instruction list belongs to. Will be used if \c arch is NULL
 * \param insnlist Queue of instructions
 * \return EXIT_SUCCESS if the assembly succeeded for all instructions, error code otherwise
 * */
extern int assemble_strlist_forarch(char* insnlist_string, arch_t* arch,
      char* archname, queue_t** insnlist);

/**
 * Modifies an instruction according to a model
 * \param orig The original instruction to modify
 * \param newopcode The new opcode of the instruction, or NULL if it must be kept
 * \param newparams Array of new parameters. Those in the array that are NULL specify that the original parameter must be kept
 * \param n_newparams Size of the newparams array
 * \param driver Assembler driver for the architecture
 * \return The modified instruction
 * */
extern insn_t* modify_insn(insn_t* orig, char* newopcode, oprnd_t** newparams,
      int n_newparams, asmbldriver_t* driver);

/**
 * Updates an instruction coding for a given architecture.
 * \param in The instruction
 * \param d Assembler driver for the architecture
 * \param chgsz If set to TRUE, the coding of the instruction will be updated even if its size changes.
 * If set to FALSE, and if the new coding of the instruction changes size, a warning will be printed and the instruction's
 * coding will not be updated
 * \param shiftaddr Contains the value by which instructions have been shift because of coding changing sizes. It will be updated
 * if chgsz is set to TRUE and the instruction's coding changes. It will also be used to update an instruction's pointer operand
 * value
 * \return
 * - EXIT_SUCCESS if the instruction could be correctly updated. Its coding will have been set
 * - Error code if the instruction could not be correctly assembled or if its new coding had a different size while \e chgsz was set to FALSE.
 * In that case, its coding will not have been updated.
 * */
extern int upd_assemble_insn(insn_t* in, void* d, int chgsz, int64_t *shiftaddr);

/**
 * Parses and assembles an assembly file and returns the corresponding bytes
 * \param asmfile An asmfile structure. It must contain a path to a valid assembly file
 * \param archname Name of the architecture to use for assembling
 * \param len Return parameter, will contain the length of the returned byte string
 * \return The assembled code of the assembly instructions found in the file, or NULL if an error occurred
 * In that case, the last error code in \c asmfile will be updated
 * */
unsigned char* assemble_asm_file(asmfile_t* asmfile, char* archname, int* len);

/**
 * Parses and assembles a formatted text file
 * The text file must be formatted so as to be able to be handled by the txtfile_* functions from libmcommon.
 * \param asmfile An asmfile structure. It must contain a path to a valid formatted text file
 * \param archname Name of the architecture to use for assembling
 * \param txtfile Parsed text file
 * \param fieldnames Array of names of the fields characterising the assembly elements present in the text file
 * \return EXIT_SUCCESS / error code
 * */
extern int asmfile_assemble_fromtxtfile(asmfile_t* asmfile, char* archname,
      txtfile_t* txtfile, asm_txt_fields_t fieldnames);

#endif /*ASSEMBLER_H_*/
