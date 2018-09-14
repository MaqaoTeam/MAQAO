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
 * \page patcher_driver Interface with architecture specific code
 * \brief Description of the architecture specific functions to define
 *
 * The patcher handles multiple architectures through the use of a driver defining a set of architecture specific functions that will be loaded
 * from the relevant architecture (retrieved from the patched file). Those functions must be defined for each architecture implemented by the patcher
 *
 * \section driver_functions Description of the functions
 * Each function must be prefixed with the architecture name. They will be loaded into a driver identifying them with their suffixes.
 *
 * \subsection generate_insnlist_functioncall Generation of a function call
 * This function generates the instruction list for invoking a function. The generated instructions must include the following operations:
 * - Compare and branch instructions for the serialised conditions
 * - Parameters passing
 * - Saving the context (if necessary)
 * - Moving or shifting the stack (if necessary)
 * - Aligning the stack
 * - Retrieving return value
 *
 * The function also returns pointer to the actual branch instruction performing the function call.
 * It will also return pointer to all instructions referencing a memory address, to allow linking them with the \e libmtroll structures.
 *
 * \subsection generate_insnlist_jmpaddr Generation of an unconditional branch
 * This function must generate an instruction list for performing an unconditional branch to an instruction.
 *
 * \subsection generate_insnlist_smalljmpaddr Generation of a small unconditional branch
 * This function must generate an instruction list for performing an unconditional branch to an instruction, using the short version of the instruction if
 * it exists for the given architecture, otherwise return NULL if this architecture does not defines branch instructions of different sizes.
 * The function must also return the range accessed by this instruction.
 *
 * \subsection generate_insn_nop Generation of a nop instruction
 * This function must generate a \c nop instruction of a given size. If the architecture does not define \c nop instructions of the given size, the
 * variant with the closest size will be returned. Requesting a size 0 returns the smallest possible \c nop instruction.
 *
 * \subsection upd_dataref_coding Update of an instruction referencing memory
 * This function must update the binary coding of an instruction referencing a memory address based on the instruction's address.
 * It will be used chiefly by the \c libmtroll when updating targets of RIP-based memory instructions.
 *
 * \subsection generate_insnlist_pltcall Generation of a PLT stub
 * This function must generate the instruction list used in ELF files for invoking a dynamic function. It must return pointers to the instructions in
 * the list referencing the global offset table and the beginning of the stub used for invoking the dynamic loader.
 *
 * \subsection others Other functions
 * Other functions must also perform the following operations:
 * - Testing if a given instruction is a \c nop
 * - Returning an operand used to reference a memory address based from the instruction's address
 * - Returning branch instruction performing the logical opposite of a conditional branch
 * - Returning the instructions used to perform a return from an invoked function
 * - Adding the instructions representing conditions to an instruction list
 * */

/**
 * \file patch_archinterface.h
 *
 * \date 20 sept. 2011
 */

#ifndef PATCH_ARCHINTERFACE_H_
#define PATCH_ARCHINTERFACE_H_

#include "libmcommon.h"
#include "libmasm.h"
#include "patchutils.h"

/**
 * \brief Architecture patcher driver
 * Holds the pointer to all the architecture specific functions needed by the patcher
 * */
typedef struct patchdriver_s {
   queue_t* (*generate_insnlist_functioncall)(modif_t*, insn_t**, pointer_t*,
         data_t*); /**<Function used to generate the list of instructions needed to call a function*/
   queue_t* (*generate_insnlist_smalljmpaddr)(int64_t*, insn_t**, pointer_t**); /**<Function used to generate the list of instructions needed to jump to an address using a small jump*/
   queue_t* (*generate_insnlist_jmpaddr)(int64_t*, insn_t**, pointer_t**); /**<Function used to generate the list of instructions needed to jump to an address*/
   insn_t* (*generate_insn_nop)(unsigned int); /**<Function used to generate a NOP instruction*/
   void (*upd_dataref_coding)(void*, int64_t, void*, int, int); /**<Function used to update an instruction pointing to another address*/
//	queue_t* (*generate_insnlist_pltcall)(int, insn_t*, insn_t**, insn_t**); /**<Function used to generate the list of instructions needed in a PLT section to
//	 * allow the call of an external function*/
   int (*instruction_is_nop)(insn_t*); /**<Function used to check if an instruction is a nop*/
   oprnd_t* (*generate_oprnd_globvar)(int); /**<Function used to generate an operand used to access a global variable*/
   insn_t* (*generate_opposite_branch)(insn_t*, oprnd_t**, int64_t*, char*); /**<Function used to generate the opposite instruction to a branch*/
   void (*add_conditions_to_insnlist)(queue_t*, insertconds_t*, data_t*,
         int64_t); /**<Function used to add conditions around an instruction list*/
   queue_t* (*generate_insnlist_return)(int64_t*); /**<Function used to generate the list of instructions needed to perform a return from call*/
////////NEW ELEMENTS FROM PATCHER REFACTORING
   queue_t* (*generate_insnlist_ripjmpaddr)(int64_t*, insn_t**, pointer_t**);/**<Function used to generate the list of instructions representing an indirect jump using a relative memory operand as destination.*/
   queue_t* (*generate_insnlist_indjmpaddr)(int64_t*, insn_t**, pointer_t**);/**<Function used to generate the list of instructions representing an indirect jump.*/
   int (*smalljmp_reachaddr)(int64_t, int64_t); /**<Checks if a small jump instruction at a given address can reach another address*/

   int64_t (*get_smalljmp_maxdistneg)(); /**<Returns the smallest signed distance reachable with the smallest (in instruction size) direct jump instruction*/
   int64_t (*get_smalljmp_maxdistpos)(); /**<Returns the largest signed distance reachable with the smallest (in instruction size) direct jump instruction*/
   int64_t (*get_jmp_maxdistneg)(); /**<Returns the smallest signed distance reachable with the standard direct jump instruction*/
   int64_t (*get_jmp_maxdistpos)(); /**<Returns the largest signed distance reachable with the standard direct jump instruction*/
   int64_t (*get_relmem_maxdistneg)(); /**<Returns the smallest signed distance between an instruction using a relative memory operand and the referenced address*/
   int64_t (*get_relmem_maxdistpos)(); /**<Returns the largest signed distance between an instruction using a relative memory operand and the referenced address*/

   uint16_t (*get_smalljmpsz)(); /**<Returns the size in byte of the smallest direct jump instruction list*/
   uint16_t (*get_jmpsz)(); /**<Returns the size in byte of the direct jump instruction list*/
   uint16_t (*get_relmemjmpsz)(); /**<Returns the size in byte of the jump instruction list using a relative memory operand as destination.*/
   uint16_t (*get_indjmpaddrsz)(); /**<Returns the size in bytes of the indirect jump instruction list*/

   uint8_t (*get_addrsize)(bfwordsz_t); /**<Returns the size in bytes of an address usable by a memory relative operand from a jump instruction to store its destination based on the binary file word size*/
   uint8_t (*movedinsn_getmaxbytesize)(insn_t*); /**<Returns the maximal size in bytes of a moved instruction. It is expected to be different from the original if the instruction has a reference operand (relative or memory relative)*/
} patchdriver_t;

/**
 * Creates a new driver structure and initializes its function pointers from the library
 * relevant to the given architecture
 * \param arch The structure representing the architecture we have to load (as present in an arch_t structure)
 * \return The new driver for the given architecture
 * */
extern patchdriver_t *patchdriver_load(arch_t* arch);

/**
 * Frees a driver and closes its link to the relevant library file
 * \param d The driver to free
 * */
extern void patchdriver_free(patchdriver_t *d);

#endif /* PATCH_ARCHINTERFACE_H_ */
