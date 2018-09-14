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
#ifndef ARM64_PATCHER_H_
#define ARM64_PATCHER_H_

/**
   Copyright (C) 2004 - 2012 Université de Versailles Saint-Quentin-en-Yvelines (UVSQ)

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
 * \file arm64_patcher.h
 * \brief This file contains the headers of functions used to generate instructions needed by the patcher for the arm64 architecture
 * */

#include "libmcommon.h"
#include "libmasm.h"
#include "libmpatch.h"

/**\todo Possible improvement: declaring here the lists of instructions returned by functions in assembler form
 * May be a bit tricky for lists with conditional content (method for saving the stack, etc)*/

/**
 * Surrounds an instruction list with instructions conditioning its execution (the list will only be executed if the conditions are met).
 * \param inslist The list to surround with conditions
 * \param insconds Structure containing the details of the conditions
 * \param stackinsns Return parameter. If not NULL, will contain a NULL-terminated array of
 * pointers to instructions accessing the stack address
 * \param newstack Is set to 1 if this insertion uses a new stack
 * \param stackshift If this insertion uses a shifted stack, this must contain the value the stack was shifted of
 * \return An updated list of instructions
 * */
extern void arm64_add_conditions_to_insnlist(queue_t* inslist, insertconds_t* insconds,insn_t*** stackinsns,int newstack,int64_t stackshift);

/**
 * Generates a nop instruction, depending on its size
 * \param len Length in bits of the required NOP instruction (if applicable)
 * \return A pointer to a NOP instruction of the specified length
 * */
extern insn_t* arm64_generate_insn_nop(unsigned int len);

/**
 * Generates a list of instructions to be used to call a function
 * A check will be made on the parameters. If one of them uses one of the registers used for passing
 * the previous parameters, the operand's value will be pushed on the stack then popped afterward
 * into the correct register. 
 * \param insfctmod Modification containing the insertion to perform
 * \param callee Pointer to the instruction at the address of the called function. If not NULL, the
 * generated \c call instruction will be linked to it
 * \param call Return parameter. If not NULL, will contain a pointer to the call instruction
 * \param stackinsns Return parameter. If not NULL, will contain a NULL-terminated array of
 * pointers to instructions accessing the stack address
 * \param gvinsns This return parameter is an array of the same size as nparams. It will contain, for each function parameter
 * at the same index in params, the pointer to the ASM instruction coding it, if and only if this parameter uses a global
 * variable (RIP-based addressing). It must already be initialised
 * \param retinsn If not NULL, means that the function will return a value. retinsn will then contain a pointer
 * to the instruction coding the loading of this return value into a global variable
 * \return The queue of instructions for calling a function
 * \note SO FAR ONLY 6 PARAMETERS ARE ACCEPTED
 * */
extern queue_t* arm64_generate_insnlist_functioncall(modif_t* insfctmod,insn_t* callee,
        insn_t** call,insn_t*** stackinsns,insn_t** gvinsns, insn_t** retinsn);

/**
 * Generates an instruction list used to jump to an address and returns the pointer to the actual jmp instruction in the list
 * \param startaddr Pointer to the address at which the instruction must be set
 * If not NULL, contains the address at the end of the instruction after the operation
 * \param jmpinsn Pointer to the JUMP instruction
 * \return A pointer to the list of instructions
 * */
extern queue_t* arm64_generate_insnlist_jmpaddr(int64_t* startaddr,insn_t** jmpinsn);

/**
 * Generates an instruction list used to perform a return
 * \param startaddr Pointer to the address at which the instruction must be set
 * If not NULL, contains the address at the end of the instruction after the operation
 * \return A pointer to the list of instructions
 * */
extern queue_t* arm64_generate_insnlist_return(int64_t* startaddr);


/**
 * Generates the list of instructions used in the procedure linkage table section
 * to call an external function.
 * \param relidx is the function index in the associated relocation table
 * \param pltstart is the first instruction in the procedure linkage table.
 * \param jmpgot Return parameter (must not be NULL), will contain the address
 * to the jump instruction to the got table
 * \param jmpgotret Return parameter (must not be NULL), will contain the return
 * instruction from the got table
 * \return Instruction list
 * */
extern queue_t* arm64_generate_insnlist_pltcall(int relidx,insn_t* pltstart,insn_t** jmpgot,insn_t** jmpgotret);

/**
 * Generates an instruction list used to jump to an address and returns the offset of
 * the function's address from the beginning of this instruction (used for relocations in .o files),
 *  and the pointer to the actual jmp instruction in the list. The jump instruction is the smallest possible available
 * \param startaddr Pointer to the address at which the instruction must be set
 * If not NULL, contains the address at the end of the instruction after the operation
 * \param jmpinsn Pointer to the JUMP instruction
 * \param maxdistpos Will contain the maximal positive offset (in bits) available for this jump
 * \param maxdistneg Will contain the maximal negative offset (in bits, absolute value) available for this jump
 * \return A pointer to the list of instructions
 * */
extern queue_t* arm64_generate_insnlist_smalljmpaddr(int64_t* startaddr,insn_t** jmpinsn,uint64_t *maxdistpos,uint64_t *maxdistneg);

/**
 * Generates an operand used to access a global variable
 * \param type Currently not used. Reserved for specifying the type of operand needed (for instance whether it must be RIP-based
 * or contain the address of the variable as an immediate)
 * \return An operand used to access a global variable
 * */
extern oprnd_t* arm64_generate_oprnd_globvar(int type);

/**
 * Generates a branch instruction that is the opposite of a given conditional branch instruction.
 * \param in The branch instruction to reverse
 * \param op Return parameter. Will contain the pointer to the operand on which to perform a test equivalent to the opposite of the branch
 * (only needed for the J*CXZ instructions)
 * \param val Return parameter. Will contain a pointer to the value to compare with \c op if needed
 * \param condtype Return parameter. Will contain a pointer to a character representing the type of comparison to perform if needed.
 * This is the same characters as those used in \ref generate_insnlist_testcond
 * \return A new instruction that is the opposite of the given (e.g. JE if JNE was given).
 * If \c in is not a branch, NULL is returned. If \c in is a branch but has no opposite (unconditional JUMP, CALL, JRCXZ), a pointer
 * to \c in is returned.
 * */
extern insn_t* arm64_generate_opposite_branch(insn_t* in, oprnd_t** op, int64_t* val, char* condtype);

/**
 * Checks if an instruction is a NOP instruction
 * \param in The instruction to check
 * \return TRUE if the instruction is a NOP, FALSE otherwise
 * \todo Update this function to take into account the multiple NOP instructions (possibly also the nop-like LEA 0(%RAX),%RAX ones)
 * */
extern int arm64_instruction_is_nop(insn_t* in);

/**
 * Updates the coding of an expression branching to a data block (case of RIP addressing)
 * \param in Pointer to the structure containing the instruction
 * \param newaddr Address to which the expression must now be pointing
 * \param e Unused (imposed by format of functions used in libmtroll structure targetaddr)
 * \param si Unused (imposed by format of functions used in libmtroll structure targetaddr)
 * \param ti Unused (imposed by format of functions used in libmtroll structure targetaddr)
 * */
extern void arm64_upd_dataref_coding(void* in,int64_t newaddr,void* e,int si,int ti);

#endif /*ARM64_PATCHER_H_*/
