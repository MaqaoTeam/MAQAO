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
 * \file arm64_patcher.c
 * \brief This file contains functions used to generate instructions needed by the patcher for the arm64 architecture
 * */

#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include "libmcommon.h"
#include "libmasm.h"
#include "arm64_arch.h"
//#include "arm64_assembler.h"
#include "arm64_patcher.h"
#include "arm64_ext.h"

extern bitvector_t* arm64_insn_gencoding(insn_t* in);

/**
 * Retrieves an instruction coding based on the values it contains
 * \param in An instruction whose opcode name and operands have been set
 * \return The coding corresponding to this instruction
 * */
static bitvector_t* getinsncoding(insn_t* in) {
   bitvector_t* coding = arm64_insn_gencoding(in);

   if ( coding == NULL ) {
      char buf[256];
      arm64_arch.insn_print(in, buf, sizeof(buf));
      fprintf(stderr,"ERROR: Unable to assemble instruction %s at address %#"PRIx64"\n", buf, insn_get_addr(in));

   }
   DBG(char bufi[256];char bufb[256];arm64_arch.insn_print(in, bufi, sizeof(bufi));bitvector_hexprint(coding, bufb, sizeof(bufb), " ");
      DBGMSG("Generated instruction %s with coding %s (%p)\n", bufi, bufb, in);)

   return coding;
}

/**
 * Sets an instruction's coding if it could be correctly assembled and its coding was not already initialised
 * \param in Pointer to the structure describing the instruction
 * \param startaddr Pointer to the address of the instruction. If not NULL, the new instruction will be set to this address
 * and \e startaddr will be updated to be the address immediately following the new instruction
 * */
static void setinsncoding(insn_t* in, int64_t *startaddr) {
   bitvector_t* coding = getinsncoding(in);
   if ( (coding != NULL) && (BITVECTOR_GET_BITLENGTH(insn_get_coding(in)) == 0) ) {
      insn_set_coding(in, NULL, 0, coding);
   }
   /*Updates address*/
   if(startaddr!=NULL) {
      insn_set_addr(in, *startaddr);
      *startaddr+=BITVECTOR_GET_BITLENGTH(insn_get_coding(in));
   }
}

/**
 * Creates a new instruction and initialises its flag has having been created for the patching operation
 * \return Pointer to a structure containing an undefined instruction
 * */
static insn_t* newinsn(int16_t opcode) {
   insn_t* in = insn_new(&arm64_arch);
   /*Sets the opcode*/
   insn_set_opcode(in, opcode);
   /*Sets the flag*/
   insn_add_annotate(in, A_PATCHNEW|arm64_arch.dflt_anno[opcode]);

   return in;
}

/**
 * Returns an instruction list corresponding to the instructions for aligning the stack before a function call
 * \param startaddr  Pointer to the address at which the first instruction in the list must be set
 * \return The list of instructions for aligning the stack
 * */
static queue_t* generate_insnlist_alignstack(int64_t *startaddr) {
   return NULL;
}

/**
 * Generates an instruction list used to jump to an address and returns the pointer to the actual jmp instruction in the list
 * \param startaddr Pointer to the address at which the instruction must be set
 * If not NULL, contains the address at the end of the instruction after the operation
 * \param jmpinsn Pointer to the JUMP instruction
 * \return A pointer to the list of instructions
 * */
static queue_t* generate_insnlist_jmp(int64_t* startaddr, insn_t** jmpinsn) {
   return NULL;
}

/**
 * Generates an instruction list used to test if a condition is true
 * \param startaddr Pointer to the address at which the instruction must be set
 * \param condop Operand to test
 * \param condtype Type of test to perform
 * - 'e' for equal,
 * - 'n' for non equal
 * - 'l' for less or equal
 * - 'L' for less strict
 * - 'g' for greater or equal
 * - 'G' for greater strict
 * \todo Define this somewhere else
 * \param condval Value to test
 * \param condresbr Return parameter. Pointer to the branch instruction depending on the result
 * \return The instruction list
 * */
static queue_t* generate_insnlist_testcond(int64_t* startaddr, oprnd_t* condop, char condtype, int64_t condval, insn_t** condresbr) {
   return NULL;
}

/**
 * Returns an instruction list corresponding to the instructions for saving the stack (if necessary) and the comparison flags.
 * \param startaddr Pointer to the address at which the first instruction in the list must be set
 * If not NULL, contains the address at the end of the list after the operation
 * \param stack_shift Return parameter, updated to contain the current stack shift
 * \return The list of instructions
 * */
static queue_t* generate_insnlist_save_stackandflags(int64_t* startaddr, int* stack_shift) {
   return NULL;
}

/**
 * Returns an instruction list corresponding to the instructions for restoring the flag register and the stack.
 * \param startaddr Pointer to the address at which the first instruction in the list must be set
 * If not NULL, contains the address at the end of the list after the operation
 * \return The list of instructions
 * */
static queue_t *generate_insnlist_restore_stackandflags(int64_t* startaddr) {
   return NULL;
}

/**
 * Returns an instruction list corresponding to the instructions for saving all registers.
 * \param startaddr Pointer to the address at which the first instruction in the list must be set
 * If not NULL, contains the address at the end of the list after the operation
 * \param stack_shift Return parameter, update the global stack shift
 * \return The list of instructions
 * */
static queue_t *generate_insnlist_save_allregisters(int64_t *startaddr, int* stack_shift) {
   return NULL;
}

/**
 * Returns an instruction list corresponding to the instructions for restoring the system state.
 * \param startaddr Pointer to the address at which the first instruction in the list must be set
 * If not NULL, contains the address at the end of the list after the operation
 * \return The list of instructions
 * */
static queue_t *generate_insnlist_restore_allregisters(int64_t* startaddr) {
   return NULL;
}

/**
 * Generates a list of instruction for testing a series of conditions. It assumes that generate_insnlist_save_stackandflags has been invoked first
 * \param startaddr Pointer to the address at which the instruction must be set
 * If not NULL, contains the address at the end of the instruction after the operation
 * \param insconds Structure containing the details of the conditions
 * \param conds_ok Updated, queue containing the instructions that must point to the first instruction to be executed if the conditions are matched (must be initialised)
 * \param conds_nok Updated, queue containing the instructions that must point to the first instruction to be executed if the conditions are not matched (must be initialised)
 * \return The list of instruction
 * */
static queue_t* generate_insnlist_testconds(int64_t* startaddr, insertconds_t* insconds, queue_t* conds_ok, queue_t* conds_nok) {
   return NULL;
}

/**
 * Finalises the use of conditions by pointing branches from the conditions to the right instructions
 * \param first_ok First instruction to be executed if the conditions are matched
 * \param first_nok First instruction to be executed if the conditions are not matched
 * \param conds_ok Queue containing the instructions that must point to the first instruction to be executed if the conditions are matched.
 * \param conds_nok Queue containing the instructions that must point to the first instruction to be executed if the conditions are not matched.
 * */
static void finalise_testconds(insn_t* first_ok, insn_t* first_nok, queue_t* conds_ok, queue_t* conds_nok) {

}

/*
 * Surrounds an instruction list with instructions conditioning its execution (the list will only be executed if the conditions are met).
 * \param startaddr Pointer to the address at which the instruction must be set
 * If not NULL, contains the address at the end of the instruction after the operation
 * \param inslist The list to surround with conditions
 * \param insconds Structure containing the details of the conditions
 * \param stackinsns Return parameter. If not NULL, will contain a NULL-terminated array of
 * pointers to instructions accessing the stack address
 * \param newstack Is set to 1 if this insertion uses a new stack
 * \param stackshift If this insertion uses a shifted stack, this must contain the value the stack was shifted of
 * \param elsecode Code to be executed if the conditions are not met
 * \return An updated list of instructions
 * */
void arm64_add_conditions_to_insnlist(queue_t* inslist, insertconds_t* insconds, insn_t*** stackinsns, int newstack, int64_t stackshift) {

}

/*
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
queue_t* arm64_generate_insnlist_smalljmpaddr(int64_t* startaddr, insn_t** jmpinsn, uint64_t *maxdistpos, uint64_t *maxdistneg) {
   return NULL;
}

/*
 * Generates an instruction list used to jump to an address and returns the pointer to the actual jmp instruction in the list
 * \param startaddr Pointer to the address at which the instruction must be set
 * If not NULL, contains the address at the end of the instruction after the operation
 * \param jmpinsn Pointer to the JUMP instruction
 * \return A pointer to the list of instructions
 * */
queue_t* arm64_generate_insnlist_jmpaddr(int64_t* startaddr, insn_t** jmpinsn) {
   return NULL;
}

/*
 * Generates an instruction list used to perform a return
 * \param Pointer to the address at which the instruction must be set
 * If not NULL, contains the address at the end of the instruction after the operation
 * \return A pointer to the list of instructions
 * */
queue_t* arm64_generate_insnlist_return(int64_t* startaddr) {
   return NULL;
}

/*
 * Generates a nop instruction, depending on its size
 * \param blen Length in bits of the required NOP instruction (if applicable)
 * \return A pointer to a NOP instruction of the specified length
 * */
insn_t* arm64_generate_insn_nop(unsigned int blen) {
   return NULL;
}

/*
 * Checks if an instruction is a NOP instruction
 * \param in The instruction to check
 * \return TRUE if the instruction is a NOP, FALSE otherwise
 * */
int arm64_instruction_is_nop(insn_t* in) {

   int out = FALSE;

   if( insn_get_opcode_code(in) == I_HINT )
      out = TRUE;

   return out;
}

/*
 * Generates a list of instructions to be used to call a function
 * A check will be made on the parameters. If one of them uses one of the registers used for passing
 * the previous parameters, the operand's value will be pushed on the stack then popped afterward
 * into the correct register. For instance, if the third parameter of the called function is \c 16(\%RDI)
 * (in arm64, \c RDI is used to store the first parameter), the  \c 16(\%RDI) will first be pushed on the stack,
 * then the first parameter's value will be loaded to \c RDI and a POP from the stack will set the \c RDX
 * register (used to store the third parameter) to the correct value
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
 * */
queue_t* arm64_generate_insnlist_functioncall(modif_t* insfctmod, insn_t* callee,
		insn_t** call, insn_t*** stackinsns, insn_t** gvinsns, insn_t** retinsn) {

   return NULL;
}

/*
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
queue_t* arm64_generate_insnlist_pltcall(int relidx,insn_t* pltstart,insn_t** jmpgot,insn_t** jmpgotret) {
   /*
    * Adapt to arm64 purpose.
    * There is a plt in arm64 too, check that the behavior is the same and use the corresponding instruction.
    */
	return NULL;
}

/*
 * Generates an operand used to access a global variable
 * \param type Currently not used. Reserved for specifying the type of operand needed (for instance whether it must be RIP-based
 * or contain the address of the variable as an immediate)
 * \return An operand used to access a global variable
 * */
oprnd_t* arm64_generate_oprnd_globvar(int type) {
   /*
    * Not sure that this function is still called or not.
    * Anyway, easy to do, just use the PC.
    */
	return NULL;
}

oprnd_t* arm64_generate_oprnd_tlsvar(int type) {

   return NULL;
}
/*
 * Generates a branch instruction that is the opposite of a given conditional branch instruction.
 * \param in The branch instruction to reverse
 * \param op Return parameter. Will contain the pointer to the operand on which to perform a test equivalent to the opposite of the branch
 * (only needed for the J*CXZ instructions)
 * \param val Return parameter. Will contain a pointer to the value to compare with \c op if needed
 * \param condtype Return parameter. Will contain a pointer to a character representing the type of comparison to perform if needed.
 * This is the same characters as those used in \ref generate_insnlist_testcond
 * \return A new instruction that is the opposite of \c in (e.g. JE if JNE was given), with the same operand as \c in
 * If \c in is not a branch, NULL is returned. If \c in is a branch but has no opposite (unconditional JUMP, CALL, JRCXZ), a pointer
 * to \c in is returned.
 * */
insn_t* arm64_generate_opposite_branch(insn_t* in, oprnd_t** op, int64_t* val, char* condtype) {
   return NULL;
}


/*****************************************/
/*    End of code template generation    */
/*****************************************/

/****************************************************/
/*    Functions used for updating and assembling    */
/****************************************************/

void arm64_upd_dataref_coding(void* in,int64_t newaddr,void* e,int si,int ti) {
}

void arm64_upd_tlsref_coding(void* in,int64_t newaddr,void* e,int si,int ti){

}
/****End of functions used for updating and assembling****/

///////////NEW FUNCTIONS USED BY PATCHER REFACTORING
queue_t* arm64_generate_insnlist_ripjmpaddr(int64_t* addr, insn_t** jmpinsn, pointer_t** ptr) {
   /**\todo TODO (2015-04-17) TO BE IMPLEMENTED*/
   return NULL;
}
queue_t* arm64_generate_insnlist_indjmpaddr(int64_t* addr, insn_t** jmpinsn, pointer_t** ptr) {
   /**\todo TODO (2015-04-17) TO BE IMPLEMENTED*/
   return NULL;
}

/**<Checks if a small jump instruction at a given address can reach another address*/
int arm64_smalljmp_reachaddr(int64_t originaddr, int64_t addr) {
   return FALSE;
}

int64_t arm64_get_smalljmp_maxdistneg() {
   /**<Smallest signed distance reachable with the smallest (in instruction size) direct jump instruction*/
   return - 0x2000000;
}
int64_t arm64_get_smalljmp_maxdistpos() {
   /**<Largest signed distance reachable with the smallest (in instruction size) direct jump instruction*/
   return 0x1FFFFFC;
}
int64_t arm64_get_jmp_maxdistneg() {
   /**<Smallest signed distance reachable with the standard direct jump instruction*/
   return - 0x2000000;
}
int64_t arm64_get_jmp_maxdistpos() {
   /**<Largest signed distance reachable with the standard direct jump instruction*/
   return 0x1FFFFFC;
}
int64_t arm64_get_relmem_maxdistneg() {
   /**<Smallest signed distance between an instruction using a relative memory operand and the referenced address*/
   return - 2048;
}
int64_t arm64_get_relmem_maxdistpos() {
   /**<Largest signed distance between an instruction using a relative memory operand and the referenced address*/
   return 2047;
}

/**\todo TODO (2015-04-07) For these functions, we could also create the instruction(s), compute its size, and free it.
 * It's a bit cleaner that those hard-coded values, but is more costly in terms of time and memory*/
uint16_t arm64_get_smalljmpsz() {
   /**<Size in byte of the smallest direct jump instruction list*/
   return 4;
}
uint16_t arm64_get_jmpsz() {
   /**<Size in byte of the direct jump instruction list*/
   return 4;
}
uint16_t arm64_get_relmemjmpsz() {
   /**<Size in byte of the jump instruction list using a relative memory operand as destination.*/
   return 4;
}
uint16_t arm64_get_indjmpaddrsz () {
   /**<Size in bytes of the indirect jump instruction list*/
   return 4;
   /**\todo TODO (2015-04-03) Use the real value here (writing the instruction list would be interesting in that case)*/
}

/**<Returns the size in bytes of an address usable by a memory relative operand
 * from a jump instruction to store its destination based on the binary file word size*/
uint8_t arm64_get_addrsize(bfwordsz_t sz){
   switch(sz) {
   case BFS_32BITS:
      return 4;
   case BFS_64BITS:
      return 8;
   default:
      assert(0);   //Because this should never happen
      return 0;   //Because Eclipse and the non-debug standard mode
   }

}

/*
 * Retrieves the maximal size in bytes for a moved instruction with a relative or memory relative operand
 * \param insn The instruction
 * \return Maximal size in bytes for this instruction if it contains a relative operand, and the size in bytes of the instruction otherwise
 * */
uint8_t arm64_movedinsn_getmaxbytesize(insn_t* insn) {
   return 4;
}
