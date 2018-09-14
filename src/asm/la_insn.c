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
 * \file
 * */
#include <inttypes.h>

#include "libmasm.h"

///////////////////////////////////////////////////////////////////////////////
//                                  insn                                     //
///////////////////////////////////////////////////////////////////////////////
/*
 * Creates a new instruction
 * \param arch The architecture for which the instruction is defined
 * \return A blank instruction with no opcode, empty coding and oprnd list, bit size
 * equal to 0 and an address equal to SIGNED_ERROR, or NULL if \c arch is NULL
 */
insn_t* insn_new(arch_t* arch)
{
   if (!arch)
      return NULL;

   insn_t* new = lc_malloc0(sizeof *new);

   new->opcode = R_NONE;
   new->address = SIGNED_ERROR;
   new->annotate = A_NA;
   new->arch = arch;

   DBGMSGLVL(3, "Created instruction %p\n", new);
   return new;
}

/*
 * Creates a new instruction from its string representation
 * \param strinsn The instruction in string form. It must have the same format
 *                as an instruction printed using insn_print
 * \param arch The architecture this instruction belongs to
 * \return The structure containing the instruction (without its coding), or
 *         PTR_ERROR if the instruction could not be parsed
 * \warn The current algorithm does not allow spaces in instruction names (ie suffixes or prefixes)
 */
insn_t* insn_parsenew(char* strinsn, arch_t* arch)
{
   if (!strinsn || !arch)
      return NULL;

   char buf[strlen(strinsn) + 1];
   strcpy_toupper(buf, strinsn);
   return arch->insn_parse(buf);
}

/*
 * Frees an instruction structure
 * \param p The instruction to be freed
 */
void insn_free_common(void* p)
{
   if (p == NULL)
      return;

   insn_t* insn = p;

   bitvector_free(insn->coding);

   int i;
   for (i = 0; i < insn->nb_oprnd; i++)
      oprnd_free(insn->oprndtab[i]);
   lc_free(insn->oprndtab);

   lc_free(insn->debug);
   lc_free(insn);
}

/*
 * Frees an instruction
 * \param p The instruction to free
 * */
void insn_free(void* p)
{
   if (p == NULL)
      return;

   insn_t* insn = p;
   DBGMSGLVL(3, "Freeing instruction %p\n", insn);
   assert(insn->arch); // The architecture should always be set for an instruction
   insn->arch->insn_free(insn);
}

/*
 * Adds an instruction into a list of instruction
 * \param insn An instruction
 * \param insnlst A list of instructions
 */
void add_insn_to_insnlst(insn_t* insn, queue_t* insn_list)
{
   if (insn == NULL)
      return;

   queue_add_tail(insn_list, insn);
   insn->sequence = queue_iterator_rev(insn_list);
}

/*
 * Add an instruction into at the end of a block (change the
 * end pointer of the block)
 * \param insn an instruction to add in a block
 * \param b a block where to add an instruction
 */
void add_insn_to_block(insn_t* insn, block_t* b)
{
   if (insn == NULL || b == NULL)
      return;

   insn->block = b;
   b->end_sequence = insn->sequence;
}

/**
 * Adds a oprnd to the table of oprnd of an instruction
 * \param insn The instruction to which a new oprnd must be added, must be !=NULL
 * \param pa The new oprnd to add
 */
static void addoprnd__(insn_t* insn, oprnd_t* pa)
{
   insn->oprndtab = lc_realloc(insn->oprndtab,
         (sizeof insn->oprndtab[0]) * (insn->nb_oprnd + 1));
   insn->oprndtab[insn->nb_oprnd] = pa;
   insn->nb_oprnd++;
}

/*
 * Adds an immediate oprnd to an instruction
 * \param insn The instruction to update
 * \param imm The value of the immediate oprnd
 */
void insn_add_imm_oprnd(insn_t* insn, int64_t imm)
{
   if (insn == NULL)
      return;

   /*Creates oprnd of type immediate*/
   oprnd_t* oprnd = oprnd_new_imm(imm);

   /*Adds oprnd to instruction*/
   addoprnd__(insn, oprnd);
}

/*
 * Adds a memory operand to an instruction
 * \param insn The instruction to update
 * \param seg The name of the base segment register for the memory address (can be NULL)
 * \param base The name of the register containing the base memory address (can be NULL)
 * \param index The name of the index register for the memory address (can be NULL)
 * \param offset The value of displacement from the base for the memory address
 * \param scale The scale associated to the index (if present)
 */
void insn_add_mem_oprnd(insn_t* insn, reg_t* seg, reg_t* base, reg_t* index,
      int64_t offset, int scale)
{
   if (insn == NULL)
      return;

   /*Creates oprnd of type memory*/
   oprnd_t* oprnd = oprnd_new_mem(seg, base, index, scale, offset);

   /*Adds oprnd to instruction*/
   addoprnd__(insn, oprnd);
}

/*
 * Adds a oprnd to an instruction
 * \param insn The instruction to update
 * \param oprnd A pointer to the structure holding the oprnd
 */
void insn_add_oprnd(insn_t* insn, oprnd_t* oprnd)
{
   if ((insn) && (oprnd))
      addoprnd__(insn, oprnd);
}

/*
 * Adds a relative address operand to an instruction
 * \param insn The instruction to update
 * \param addr The address pointed to by the pointer
 * \param offset For pointer of type POINTER_RELATIVE, the offset (relative displacement) from the current address
 * \param type Type of the pointer
 */
void insn_add_ptr_oprnd(insn_t* insn, maddr_t addr, pointer_offset_t offset,
      pointer_type_t type)
{
   if (insn == NULL)
      return;

   oprnd_t* oprnd = oprnd_new_ptr(addr, offset, type);
   /**\todo TODO (2015-03-25) I'm updating this quickly because of the recent merge with master, where the newly merged
    * decan plugin used this function. I had commented it because no one used it before, so maybe I had a good reason
    * for not wanting to update it. Check afterward if this version does not cause problems*/

   /*Adds oprnd to instruction*/
   addoprnd__(insn, oprnd);
}

/*
 * Adds a register oprnd to an instruction
 * \param insn The instruction to update
 * \param regname The name of the register
 */
void insn_add_reg_oprnd(insn_t* insn, reg_t* name)
{
   if (insn == NULL)
      return;

   oprnd_t* oprnd = oprnd_new_reg(name);

   /*Adds oprnd to instruction*/
   addoprnd__(insn, oprnd);
}

/*
 * Updates the coding of the instruction by appending the additional coding
 * \param insn The instruction to update
 * \param appendcode A bitvector holding the bits to append to the instruction's coding
 */
void insn_append_coding(insn_t* insn, bitvector_t* appendcode)
{
   // no need to check for NULL insn: getcoding will return NULL
   bitvector_append(insn_get_coding(insn), appendcode);
}

/*
 * Copies an instruction into another.
 * \param insn The instruction to copy
 * \return A pointer to a new instruction structure with identical values to \e in
 * \warning If the instruction is a branch, its destination will be identical for the copy, so additional treatments may have
 * to be performed when duplicating a list. The extension, block and label are not initialised in the copy.
 * Use this function with caution
 */
insn_t* insn_copy(insn_t* insn)
{
   if (insn == NULL)
      return NULL;

   arch_t* arch = insn_get_arch(insn);
   insn_t* cpy = insn_new(arch);
   cpy->variant_id = insn->variant_id;
   cpy->opcode = insn->opcode;
   cpy->opprefx = insn->opprefx;
   cpy->opsuffx = insn->opsuffx;
   cpy->address = insn->address;
   cpy->annotate = insn->annotate;
   insn_set_nb_oprnds(cpy, insn->nb_oprnd);

   int i;
   for (i = 0; i < insn->nb_oprnd; i++) {
      oprnd_t* opcpy = arch->oprnd_copy(insn_get_oprnd(insn, i));
      insn_set_oprnd(cpy, i, opcpy);
   }

   cpy->coding = bitvector_dup(insn->coding);
   cpy->read_size = insn->read_size;
   cpy->elt_in = insn->elt_in;
   cpy->elt_out = insn->elt_out;

   return cpy;
}

/*
 * Checks if an instruction contains a reference to another part of the code or the file
 * \param The instruction to check
 * \param Return parameter. Contains the guessed type of destination
 * (1 if destination is an instruction and 0 otherwise (data block)
 * \return the address of the destination, or ADDRESS_ERROR if the instruction does not reference any other part of the code
 * \todo Rewrite this function using annotate
 */
int64_t insn_check_refs(insn_t* in, int *isinsn)
{
   /**\bug This function may probably architecture-specific. If so, it has to be returned to the
    * architecture-specific file. What is more is that this function is not coherent with
    * \<arch\>_generate_oprnd_globvar, which is used to generate an instruction referencing a
    * global variable. Here we assume that a RIP-based instruction is such an instruction, while
    * it was left to be decided by the architecture
    * => (2015-07-30) Slowly fixing that
    */
   int64_t dest = ADDRESS_ERROR;
   oprnd_t* refop = insn_lookup_ref_oprnd(in);
   insn_oprnd_updptr(in, refop);
   pointer_t* ptr = oprnd_get_refptr(refop);

   dest = pointer_get_addr(ptr);

   if (isinsn)
      *isinsn = (oprnd_is_ptr(refop) == TRUE) ? 1 : 0;

   return dest;
}

/*
 * Retrieves an operand referencing another address, either through a pointer or a relative memory address
 * \param insn The instruction
 * \return An operand referencing another address, or NULL if the instruction has no such operand
 * */
oprnd_t* insn_lookup_ref_oprnd(insn_t* insn)
{
   if (insn == NULL)
      return NULL;

   uint8_t i = 0;
   oprnd_t* op = NULL;
   while ((op = insn_get_oprnd(insn, i++))) {
      if (oprnd_is_memrel(op) || oprnd_is_ptr(op))
         break;
   }

   return op;
}

/*
 * Retrieves the variant identifier of an instruction
 * \param insn The instruction
 * \return Its variant identifier, or 0 if the instruction is NULL
 * */
uint32_t insn_get_variant_id(insn_t* insn)
{
   return (insn != NULL) ? insn->variant_id : 0;
}

/*
 * Returns the opcode name of an instruction
 * \param insn The instruction
 * \return The instruction's opcode or PTR_ERROR if insn is NULL
 */
char* insn_get_opcode(insn_t* insn)
{
   // no need to check for NULL insn: get_architecture will return NULL
   arch_t *arch = insn_get_arch(insn);
   return (arch != NULL) ? arch_get_opcode_name(arch, insn->opcode) : PTR_ERROR;
}

/*
 * Returns the opcode code of an instruction
 * \todo suspicious UNSIGNED_ERROR (return value being signed)
 * \param insn The instruction
 * \return The instruction's opcode code or UNSIGNED_ERROR if insn is NULL
 */
int16_t insn_get_opcode_code(insn_t* insn)
{
   return (insn != NULL) ? insn->opcode : UNSIGNED_ERROR;
}

/*
 * Retrieves the instruction prefix
 * \param insn The instruction
 * \return The instruction's prefix or SIGNED_ERROR if insn is NULL
 */
int8_t insn_get_prefix(insn_t* insn)
{
   return (insn != NULL) ? insn->opprefx : SIGNED_ERROR;
}

/*
 * Retrieves the instruction suffix
 * \param insn The instruction
 * \return The instruction's suffix or SIGNED_ERROR if insn is NULL
 */
int8_t insn_get_suffix(insn_t* insn)
{
   return (insn != NULL) ? insn->opsuffx : SIGNED_ERROR;
}

/*
 * Retrieves the instruction set
 * \param insn The instruction
 * \return The instruction set or 0 if \c insn is NULL
 */
uint8_t insn_get_iset(insn_t* insn)
{
   // no need to check for NULL insn: get_architecture will return NULL
   arch_t *arch = insn_get_arch(insn);
   if (!arch)
      return 0;

   if ((arch->variants_isets && insn->variant_id < arch->nb_insnvariants))
      return arch->variants_isets[insn->variant_id];
   else
      return 0;
}

/*
 * Returns the number of oprnds of an instruction
 * \param insn an instruction
 * \return The number of operands or UNSIGNED_ERROR if insn is NULL
 */
uint8_t insn_get_nb_oprnds(insn_t* insn)
{
   return (insn != NULL) ? insn->nb_oprnd : UNSIGNED_ERROR;
}

/*
 * Returns the default annotate flags associated to an instruction. This function is mainly
 * intended to retrieve information about an instruction that was not retrieved through disassembly
 * \param insn an instruction
 * \return the flags or 0 if \c insn or \c arch is NULL
 */
uint32_t insn_get_default_annotate(insn_t* insn)
{
   if ((!insn) || (!insn->arch) || (insn->opcode < 0)
         || (insn->opcode > insn->arch->size_opcodes))
      return 0;
   return insn->arch->dflt_anno[insn->opcode];
}

/*
 * Returns the annotate flags associated to an instruction
 * \param insn an instruction
 * \return the flags or 0 if insn is NULL
 */
uint32_t insn_get_annotate(insn_t* insn)
{
   return (insn != NULL) ? insn->annotate : 0;
}

/*
 * Checks if the annotate flag of an instruction contains a given flag (or flags)
 * \param insn An instruction
 * \param annotate The flags to check in the instruction
 * \return TRUE if all the flags contained in \e annotate are present in the annotate flag
 * of the instruction, FALSE otherwise
 */
int insn_check_annotate(insn_t* insn, unsigned int annotate)
{
   if ((insn != NULL) && ((insn->annotate & annotate) == annotate))
      return TRUE;
   else
      return FALSE;
}

/*
 * Retrieves the address of an instruction
 * \param insn an instruction
 * \return Its address or 0 if insn is NULL
 */
int64_t insn_get_addr(insn_t* insn)
{
   return (insn != NULL) ? insn->address : SIGNED_ERROR;
}

/*
 * Retrieves the address at the end of an instruction
 * \param insn an instruction
 * \return The address immediately following its end or ADDRESS_ERROR if \c insn is NULL
 */
int64_t insn_get_end_addr(insn_t* insn)
{
   return
         (insn != NULL) ?
               insn->address + BITVECTOR_GET_BYTELENGTH(insn->coding) :
               ADDRESS_ERROR ;
}

/*
 * Retrieves the architecture an instruction belongs to. This is only possible if the instruction belongs to a block
 * \warning Deprecated and will be removed, use insn_get_arch instead
 * \param insn An instruction
 * \return A pointer to the arch_t structure it is defined for
 */
arch_t* insn_getarch_fromblock(insn_t* in)
{
   WRNMSG("insn_getarch_fromblock is deprecated, use insn_get_arch instead");

   if ((in != NULL) && (in->block != NULL) && (in->block->function != NULL)
         && (in->block->function->asmfile != NULL))
      return in->block->function->asmfile->arch;
   else
      return PTR_ERROR;
}

/*
 * Retrieves the asmfile an instruction belongs to. This is only possible if the instruction belongs to a block
 * \warning Deprecated and will be removed, use insn_get_asmfile instead
 * \param insn An instruction
 * \return A pointer to the asmfile_t structure it is defined for
 */
asmfile_t* insn_get_asmfile_fromblock(insn_t* in)
{
   WRNMSG(
         "insn_get_asmfile_fromblock is deprecated, use insn_get_asmfile instead");

   if ((in != NULL) && (in->block != NULL) && (in->block->function != NULL))
      return in->block->function->asmfile;
   else
      return PTR_ERROR;
}

/*
 * Retrieves the array of operands
 * \param insn an instruction
 * \return an array of pointers on oprnds (its size can be got using insn_get_nb_oprnds)
 */
oprnd_t** insn_get_oprnds(insn_t* insn)
{
   return (insn != NULL) ? insn->oprndtab : PTR_ERROR;
}

/*
 * Retrieves the binary coding of an instruction
 * \param insn an instruction
 * \return Its coding or PTR_ERROR if insn is NULL
 */
bitvector_t* insn_get_coding(insn_t* insn)
{
   return (insn != NULL) ? insn->coding : PTR_ERROR;
}

/*
 * Retrieves extensions associated to an instruction
 * \param insn an instruction
 * \return a structure (defined by the user) which contains extensions
 */
void* insn_get_ext(insn_t* insn)
{
   return (insn != NULL) ? insn->ext : PTR_ERROR;
}

/*
 * Retrieves the block which contains the instruction
 * \param insn an instruction
 * \return a block or PTR_ERROR if insn has no block or is NULL
 */
block_t* insn_get_block(insn_t* insn)
{
   return (insn != NULL) ? insn->block : PTR_ERROR;
}

/*
 * Retrieves the loop which contains the instruction
 * \warning flow and loop analysis have to be already done
 * \param insn an instruction
 * \return a loop or PTR_ERROR if insn has no loop or is NULL
 */
loop_t* insn_get_loop(insn_t* insn)
{
   // no need to check for NULL insn: getblock will return NULL
   block_t *block = insn_get_block(insn);
   return block_get_loop(block);
}

/*
 * Retrieves the function which contains the instruction
 * \warning flow analysis has to be already done
 * \param insn an instruction
 * \return a function or PTR_ERROR if insn has no function or is NULL
 */
fct_t* insn_get_fct(insn_t* insn)
{
   // no need to check for NULL insn: getblock will return NULL
   block_t *block = insn_get_block(insn);
   return block_get_fct(block);
}

/*
 * Retrieves the asmfile which contains the instruction
 * \warning flow analysis has to be already done
 * \param insn an instruction
 * \return a asmfile or PTR_ERROR if insn has no asmfile or is NULL
 */
asmfile_t* insn_get_asmfile(insn_t* insn)
{
   // no need to check for NULL insn: getfct will return NULL
   fct_t *function = insn_get_fct(insn);
   return fct_get_asmfile(function);
}

/*
 * Retrieves the project which contains the instruction
 * \warning flow analysis has to be already done
 * \param insn an instruction
 * \return a project or PTR_ERROR if insn has no project or is NULL
 */
project_t* insn_get_project(insn_t* insn)
{
   // no need to check for NULL insn: getasmfile will return NULL
   asmfile_t *asmfile = insn_get_asmfile(insn);
   return asmfile_get_project(asmfile);
}

/*
 * Retrieves the label of the function the instruction belongs to
 * \param insn an instruction
 * \return The label of the function the instruction belongs to (last label before it) or PTR_ERROR if insn is NULL
 */
label_t* insn_get_fctlbl(insn_t* insn)
{
   return (insn != NULL) ? insn->fctlbl : PTR_ERROR;
}

/*
 * Retrieves the list element which contains the instruction in the global instrucion list
 * \param insn an instruction
 * \return The list element or PTR_ERROR if insn is NULL
 */
list_t* insn_get_sequence(insn_t* insn)
{
   return (insn != NULL) ? insn->sequence : PTR_ERROR;
}

/*
 * Retrieves the previous instruction in the global instruction list
 * \note if performance critical use INSN_GET_PREV
 * \param insn an instruction
 * \return previous instruction or PTR_ERROR if anything NULL in the way
 */
insn_t* insn_get_prev(insn_t* insn)
{
   return list_getdata(list_getprev(insn_get_sequence(insn)));
}

/*
 * Retrieves the next instruction in the global instruction list
 * \note if performance critical use INSN_GET_NEXT
 * \param insn an instruction
 * \return next instruction or PTR_ERROR if anything NULL in the way
 */
insn_t* insn_get_next(insn_t* insn)
{
   return list_getdata(list_getnext(insn_get_sequence(insn)));
}

/*
 * Retrieves the address pointed by an instruction with an operand containing a pointer (ADDRESS_ERROR if instruction is not a branch)
 * \param insn A pointer to the structure holding the branch instruction
 * \return The destination's address, or ADDRESS_ERROR if the instruction has no operand containing a pointer or is NULL
 */
int64_t insn_find_pointed(insn_t* insn)
{
   // no need to check for NULL insn: lookup_refoprnd will return NULL
   return oprnd_get_refptr_addr(insn_lookup_ref_oprnd(insn));
}

/*
 * Returns a pointer to the oprnd of an instruction at the specified position
 * (0 is the first oprnd)
 * \param insn The instruction
 * \param pos The index of the oprnd to be looked for (starting at 0)
 * \return A pointer to the structure holding the oprnd, or PTR_ERROR if pos is out of range
 * (negative or greater than the number of oprnds of insn)
 */
oprnd_t* insn_get_oprnd(insn_t* insn, int pos)
{
   if ((insn == NULL) || (pos < 0) || (pos >= insn_get_nb_oprnds(insn)))
      return NULL;

   return insn->oprndtab[pos];
}

/*
 * Retrieves the size in bits of an instruction
 * \param insn The instruction to analyze
 * \return Its size in bits or 0 if insn is NULL
 */
int insn_get_size(insn_t* insn)
{
   // no need to check for NULL insn: getcoding will return NULL
   return BITVECTOR_GET_BITLENGTH(insn_get_coding(insn));
}

/*
 * Retrieves the size in bytes of an instruction
 * \param insn The instruction to analyze
 * \return Its size in bytes or 0 if insn is NULL
 */
unsigned int insn_get_bytesize(insn_t* insn)
{
   // no need to check for NULL insn: getcoding will return NULL
   return BITVECTOR_GET_BYTELENGTH(insn_get_coding(insn));
}

/*
 * Retrieves the pointer to the next instruction of a branch instruction (NULL if not a branch)
 * \param insn The instruction to analyze
 * \return A pointer to the instruction pointed to by insn, or PTR_ERROR if none was found
 * \note This function assumes that a branch instruction has one and only one operand
 * \todo Add a better way to identify branches (maybe from the grammar)
 */
insn_t* insn_get_branch(insn_t* insn)
{
   // no need to check for NULL insn: lookup_refoprnd will return NULL
   return pointer_get_insn_target(oprnd_get_ptr(insn_lookup_ref_oprnd(insn)));
}

/*
 * Gets the simd flag of a specified instruction
 * \param insn An instruction
 * \return The simd flag associated to the opcode, or UNSIGNED_ERROR if there is a problem
 */
unsigned short insn_get_simd(insn_t *insn)
{
   // no need to check for NULL insn: get_architecture will return NULL
   arch_t *arch = insn_get_arch(insn);
   if (arch == NULL)
      return UNSIGNED_ERROR;

   return arch_insnvariant_is_simd(arch, insn->variant_id);
}

/*
 * Checks whether an instruction is SIMD (uses SIMD registers and/or units)
 * \param insn An instruction
 * \return FALSE/TRUE
 */
boolean_t insn_is_SIMD(insn_t* insn)
{
   // no need to check for NULL insn: get_simd will return 0/FALSE
   return (insn_get_simd(insn) == S_YES) ? TRUE : FALSE;
}

/*
 * Checks whether an instruction processes at input integer elements
 * \param insn An instruction
 * \return FALSE/TRUE
 */
boolean_t insn_is_INT(insn_t *insn)
{
   // no need to check for NULL insn: get_input_element_type will return T_UNDEF
   int elt_type = insn_get_input_element_type(insn);
   return
         (elt_type == T_INT || elt_type == T_SINT || elt_type == T_UINT) ?
               TRUE : FALSE;
}

/*
 * Checks whether an instruction processes at input FP elements
 * \param insn An instruction
 * \return FALSE/TRUE
 */
boolean_t insn_is_FP(insn_t *insn)
{
   // no need to check for NULL insn: get_input_element_type will return T_UNDEF
   int elt_type = insn_get_input_element_type(insn);
   return (elt_type == T_FP) ? TRUE : FALSE;
}

/*
 * Checks whether an instruction processes at input a structure or a string
 * \param insn An instruction
 * \return FALSE/TRUE
 */
boolean_t insn_is_struct_or_str(insn_t *insn)
{
   // no need to check for NULL insn: get_input_element_type will return T_UNDEF
   int elt_type = insn_get_input_element_type(insn);
   return (elt_type == T_STR) ? TRUE : FALSE;
}

/*
 * Checks whether an instruction is SIMD and processes integer elements
 * \param insn An instruction
 * \return FALSE/TRUE
 */
boolean_t insn_is_SIMD_INT(insn_t *insn)
{
   // no need to check for NULL insn: is_SIMD will return 0/FALSE
   return (insn_is_SIMD(insn) && insn_is_INT(insn)) ? TRUE : FALSE;
}

/*
 * Checks whether an instruction is SIMD and processes FP (Floating-point) elements
 * \param insn An instruction
 * \return FALSE/TRUE
 */
boolean_t insn_is_SIMD_FP(insn_t *insn)
{
   // no need to check for NULL insn: is_SIMD will return 0/FALSE
   return (insn_is_SIMD(insn) && insn_is_FP(insn)) ? TRUE : FALSE;
}

/*
 * Checks whether an instruction is SIMD and processes non FP (Floating-point) elements
 * \param insn An instruction
 * \return FALSE/TRUE
 */
boolean_t insn_is_SIMD_NOT_FP(insn_t *insn)
{
   // no need to check for NULL insn: is_SIMD will return 0/FALSE
   return (insn_is_SIMD(insn) && !insn_is_FP(insn)) ? TRUE : FALSE;
}

/*
 * Returns the SIMD width for an SIMD instruction, that is the number of elements processed at input
 * \param insn An instruction
 * \return number of elements (number)
 */
unsigned short insn_get_SIMD_width(insn_t *insn)
{
   // If an instruction does not use a SIMD resource (unit or register),
   // it de factor cannot be packed

   // no need to check for NULL insn: is_SIMD will return 0/FALSE
   if (insn_is_SIMD(insn) == FALSE)
      return 1;

   int read_size = datasz_getvalue(insn_get_read_size(insn));
   int elt_size = datasz_getvalue(insn_get_input_element_size(insn));

   if (read_size > 0 && elt_size > 0) {
      // if more than one element is processed or element is a pack of at least 128 bits
      return (read_size / elt_size);
   }

   DBG(
         char buf_asm[256]; insn_print (insn, buf_asm, 256); DBGMSG("INFO: get_pack_degree: cannot guess size of elements for [%s], return 1", buf_asm););

   return 1;
}

/*
 * Checks whether an instruction is packed (a vector instruction)
 * \param insn An instruction
 * \return FALSE/TRUE
 */
boolean_t insn_is_packed(insn_t *insn)
{
   // If an instruction does not use a SIMD resource (unit or register),
   // it de factor cannot be packed

   // no need to check for NULL insn: is_SIMD will return 0/FALSE
   if (insn_is_SIMD(insn) == FALSE)
      return FALSE;

   int read_size = datasz_getvalue(insn_get_read_size(insn));
   int elt_size = datasz_getvalue(insn_get_input_element_size(insn));

   if (read_size > 0 && elt_size > 0) {
      // if more than one element is processed or element is a pack of at least 128 bits
      return (((read_size / elt_size) > 1) || read_size >= 128) ? TRUE : FALSE;
   }

   // Other instructions are not packed
   return FALSE;
}

/*
 * Checks whether an instruction processes single-precision (32 bits) FP elements
 * \param insn An instruction
 * \return FALSE/TRUE
 */
boolean_t insn_is_single_prec(insn_t *insn)
{
   // no need to check for NULL insn: get_input_element_type will return T_UNDEF
   int elt_type = insn_get_input_element_type(insn);
   if (elt_type != T_FP)
      return FALSE;

   int elt_size = insn_get_input_element_size(insn);
   return (elt_size == DATASZ_32b) ? TRUE : FALSE;
}

/*
 * Checks whether an instruction processes double-precision (64 bits) FP elements
 * \param insn An instruction
 * \return FALSE/TRUE
 */
boolean_t insn_is_double_prec(insn_t *insn)
{
   // no need to check for NULL insn: get_input_element_type will return T_UNDEF
   int elt_type = insn_get_input_element_type(insn);
   if (elt_type != T_FP)
      return FALSE;

   int elt_size = insn_get_input_element_size(insn);
   return (elt_size == DATASZ_64b) ? TRUE : FALSE;
}

/*
 * Checks whether an instruction is a prefetch
 * \param insn An instruction
 * \return FALSE/TRUE
 */
boolean_t insn_is_prefetch(insn_t *insn)
{
   // no need to check for NULL insn: get_family will return FM_UNDEF
   return (insn_get_family(insn) == FM_PREFETCH) ? TRUE : FALSE;
}

/*
 * Checks whether an instruction has a source (read) memory operand
 * \param insn An instruction
 * \return FALSE/TRUE
 */
boolean_t insn_has_src_mem_oprnd(insn_t *insn)
{
   int i;
   // no need to check for NULL insn: get_noprnd will return 0
   for (i = 0; i < insn_get_nb_oprnds(insn); i++) {
      oprnd_t *oprnd = insn_get_oprnd(insn, i);

      if (oprnd_is_mem(oprnd) == TRUE)
         return oprnd_is_src(oprnd) ? TRUE : FALSE;
   }

   return FALSE;
}

/*
 * Checks whether an instruction is a load, that is reads data from memory into a register
 * Warning: it is assumed that there are no implicit (out of operands) loads !
 * A warning is emitted for instructions with no operands
 * \param insn An instruction
 * \return FALSE/TRUE
 */
boolean_t insn_is_load(insn_t *insn)
{
   if (insn == NULL)
      return FALSE;

   const unsigned short fam = insn_get_family(insn);

   // Some instructions are not effectively loading data from the source operand
   if (fam == FM_PREFETCH || fam == FM_LEA || fam == FM_NOP)
      return FALSE;

   if (insn_get_nb_oprnds(insn) == 0) {
      DBG(
            char buf_asm[256]; insn_print (insn, buf_asm, 256); DBGMSG("INFO: Assuming [%s] does not read data from memory", buf_asm);)
      return FALSE;
   }

   return insn_has_src_mem_oprnd(insn);
}

/*
 * Checks whether an instruction has a source (read) memory operand
 * \param insn An instruction
 * \return FALSE/TRUE
 */
boolean_t insn_has_dst_mem_oprnd(insn_t *insn)
{
   int i;
   // no need to check for NULL insn: get_noprnd will return 0
   for (i = 0; i < insn_get_nb_oprnds(insn); i++) {
      oprnd_t *oprnd = insn_get_oprnd(insn, i);

      if (oprnd_is_mem(oprnd) == TRUE)
         return oprnd_is_dst(oprnd) ? TRUE : FALSE;
   }

   return FALSE;
}

/*
 * Checks whether an instruction is a store, that is writes data from a register to memory
 * Warning: it is assumed that there are no implicit (out of operands) stores !
 * A warning is emitted for instructions with no operands
 * \param insn An instruction
 * \return FALSE/TRUE
 */
boolean_t insn_is_store(insn_t *insn)
{
   if (insn == NULL)
      return FALSE;

   const unsigned short fam = insn_get_family(insn);

   // Some instructions are not effectively loading data from the source operand
   if (fam == FM_NOP)
      return FALSE;

   if (insn_get_nb_oprnds(insn) == 0) {
      DBG(
            char buf_asm[256]; insn_print (insn, buf_asm, 256); DBGMSG("INFO: Assuming [%s] does not write data to memory", buf_asm);)
      return FALSE;
   }

   return insn_has_dst_mem_oprnd(insn);
}

/*
 * Returns the first memory operand
 * \param insn An instruction
 * \return oprnd
 */
oprnd_t *insn_get_first_mem_oprnd(insn_t *insn)
{
   int i;
   // no need to check for NULL insn: get_noprnd will return 0
   for (i = 0; i < insn_get_nb_oprnds(insn); i++) {
      oprnd_t *oprnd = insn_get_oprnd(insn, i);

      if (oprnd_is_mem(oprnd) == TRUE)
         return oprnd;
   }

   return NULL;
}

/*
 * Returns position of the first memory operand
 * \param insn An instruction
 * \return index (or UNSIGNED_ERROR)
 */
int insn_get_first_mem_oprnd_pos(insn_t *insn)
{
   int i;
   // no need to check for NULL insn: get_noprnd will return 0
   for (i = 0; i < insn_get_nb_oprnds(insn); i++) {
      oprnd_t *oprnd = insn_get_oprnd(insn, i);

      if (oprnd_is_mem(oprnd) == TRUE)
         return i;
   }

   return UNSIGNED_ERROR;
}

/*
 * Returns position of the first memory operand, if read
 * NOTE : this function only considers mov sources (GAS syntax)
 * \return source operand index (index value starts at 0)
 */
int insn_get_oprnd_src_index(insn_t *insn)
{
   int i;
   // no need to check for NULL insn: get_noprnd will return 0
   for (i = 0; i < insn_get_nb_oprnds(insn); i++) {
      oprnd_t *oprnd = insn_get_oprnd(insn, i);

      if (oprnd_is_mem(oprnd) == TRUE)
         return (oprnd_is_src(oprnd) == TRUE) ? i : UNSIGNED_ERROR;
   }

   return UNSIGNED_ERROR;
}

/*
 * Returns position of the first memory operand, if written
 * NOTE : this function only considers mov sources (GAS syntax)
 * \return destination index (index value starts at 0)
 */
int insn_get_oprnd_dst_index(insn_t *insn)
{
   int i;
   // no need to check for NULL insn: get_noprnd will return 0
   for (i = 0; i < insn_get_nb_oprnds(insn); i++) {
      oprnd_t *oprnd = insn_get_oprnd(insn, i);

      if (oprnd_is_mem(oprnd) == TRUE)
         return (oprnd_is_dst(oprnd) == TRUE) ? i : UNSIGNED_ERROR;
   }

   return UNSIGNED_ERROR;
}

/*
 * Checks whether an instruction is a add or a sub
 * \param insn An instruction
 * \return FALSE/TRUE
 */
boolean_t insn_is_add_sub(insn_t *insn)
{
   // no need to check for NULL insn: get_family will return FM_UNDEF
   const unsigned short fam = insn_get_family(insn);
   return
         (fam == FM_ADD || fam == FM_INC || fam == FM_SUB || fam == FM_DEC) ?
               TRUE : FALSE;
}

/*
 * Checks whether an instruction is a mul
 * \param insn An instruction
 * \see insn:is_add_sub
 */
boolean_t insn_is_mul(insn_t *insn)
{
   // no need to check for NULL insn: get_family will return FM_UNDEF
   return (insn_get_family(insn) == FM_MUL) ? TRUE : FALSE;
}

/*
 * Checks whether an instruction is an FMA (fused multiply-add/sub)
 * \param insn An instruction
 * \return FALSE/TRUE
 */
boolean_t insn_is_fma(insn_t *insn)
{
   // no need to check for NULL insn: get_family will return FM_UNDEF
   return (insn_get_family(insn) == FM_FMA) ? TRUE : FALSE;
}

/*
 * Checks whether an instruction is a div
 * \param insn An instruction
 * \see insn:is_add_sub
 */
boolean_t insn_is_div(insn_t *insn)
{
   // no need to check for NULL insn: get_family will return FM_UNDEF
   return (insn_get_family(insn) == FM_DIV) ? TRUE : FALSE;
}

/*
 * Checks whether an instruction is a reciprocal approximate
 * \param insn An instruction
 * \see insn:is_add_sub
 */
boolean_t insn_is_rcp(insn_t *insn)
{
   // no need to check for NULL insn: get_family will return FM_UNDEF
   return (insn_get_family(insn) == FM_RCP) ? TRUE : FALSE;
}

/*
 * Checks whether an instruction is a sqrt (square root)
 * \param insn An instruction
 * \see insn:is_add_sub
 */
boolean_t insn_is_sqrt(insn_t *insn)
{
   // no need to check for NULL insn: get_family will return FM_UNDEF
   return (insn_get_family(insn) == FM_SQRT) ? TRUE : FALSE;
}

/*
 * Checks whether an instruction is a reciprocal sqrt (square root) approximate
 * \param insn An instruction
 * \see insn:is_add_sub
 */
boolean_t insn_is_rsqrt(insn_t *insn)
{
   // no need to check for NULL insn: get_family will return FM_UNDEF
   return (insn_get_family(insn) == FM_RSQRT) ? TRUE : FALSE;
}

/*
 * Checks whether an instruction is arithmetical (add, sub, mul, fma, div, rcp, sqrt or rsqrt)
 * \param insn An instruction
 * \see insn:is_add_sub
 */
boolean_t insn_is_arith(insn_t *insn)
{
   if (insn == NULL)
      return FALSE;

   return
         (insn_is_add_sub(insn) || insn_is_mul(insn) || insn_is_fma(insn)
               || insn_is_div(insn) || insn_is_rcp(insn) || insn_is_sqrt(insn)
               || insn_is_rsqrt(insn)) ? TRUE : FALSE;
}

/*
 * Gets the family associated to an instruction
 * \param insn An instruction
 * \return The family or FM_UNDEF if there is a problem
 */
unsigned short insn_get_family(insn_t* insn)
{
   // no need to check for NULL insn: get_architecture will return NULL
   arch_t *arch = insn_get_arch(insn);
   if (arch == NULL)
      return FM_UNDEF;

   return arch_get_family(arch, insn->opcode);
}

// DEPRECATED
unsigned short insn_getfamily_fromblock(insn_t* insn)
{
   WRNMSG(
         "insn_getfamily_fromblock is deprecated, use insn_get_family or insn_get_family_from_arch instead");

   return insn_get_family(insn);
}

/*
 * Faster version of insn_get_family, reusing a given architecture
 * \warning You must be sure that you can reuse the architecture (on some processors, architecture can be switched on instruction granularity)
 * \warning instruction must be not NULL
 * \param insn An instruction
 * \return The family or FM_UNDEF if there is a problem
 */
unsigned short insn_get_family_from_arch(insn_t* insn, arch_t* arch)
{
   // NO CHECK: insn must be not NULL
   return arch_get_family(arch, insn->opcode);
}

/*
 * Gets the class associated to an instruction
 * \param insn An instruction
 * \return The class or UNSIGNED_ERROR if there is a problem
 */
unsigned short insn_get_class(insn_t* insn)
{
   // no need to check for NULL insn: get_architecture will return NULL
   arch_t *arch = insn_get_arch(insn);
   if (arch == NULL)
      return UNSIGNED_ERROR;

   return arch_get_class(arch, insn->opcode);
}

/*
 * Gets the input element size the instruction work on
 * \param insn an instruction
 * \return the element size (DATASZ enum type) or UNSIGNED_ERROR if there is a problem
 */
unsigned short insn_get_input_element_size(insn_t* insn)
{
   // Conversion to a smaller enumeration in order to fit in 4 bits.
   // no need to check for NULL insn: get_input_element_size_raw will return 0
   switch (insn_get_input_element_size_raw(insn)) {
   case SZ_8:
      return DATASZ_8b;
   case SZ_16:
      return DATASZ_16b;
   case SZ_32:
      return DATASZ_32b;
   case SZ_64:
      return DATASZ_64b;
   case SZ_80:
      return DATASZ_80b;
   case SZ_128:
      return DATASZ_128b;
   case SZ_256:
      return DATASZ_256b;
   case SZ_512:
      return DATASZ_512b;
   default:
      return DATASZ_UNDEF;
   }
}

/*
 * Gets the input element size the instruction work on
 * \param insn an instruction
 * \return the element size (SZ enum type) or UNSIGNED_ERROR if there is a problem
 */
unsigned short insn_get_input_element_size_raw(insn_t* insn)
{
   return (insn != NULL) ? (insn->elt_in) & SZ_MASK : UNSIGNED_ERROR;
}

/*
 * Gets the output element size the instruction returns
 * \param insn an instruction
 * \return the element size (DATASZ enum type) or UNSIGNED_ERROR if there is a problem
 */
unsigned short insn_get_output_element_size(insn_t* insn)
{
   // Conversion to a smaller enumeration in order to fit in 4 bits.
   // no need to check for NULL insn: get_output_element_size_raw will return 0
   switch (insn_get_output_element_size_raw(insn)) {
   case SZ_8:
      return DATASZ_8b;
   case SZ_16:
      return DATASZ_16b;
   case SZ_32:
      return DATASZ_32b;
   case SZ_64:
      return DATASZ_64b;
   case SZ_80:
      return DATASZ_80b;
   case SZ_128:
      return DATASZ_128b;
   case SZ_256:
      return DATASZ_256b;
   case SZ_512:
      return DATASZ_512b;
   default:
      return DATASZ_UNDEF;
   }
}

/*
 * Gets the output element size the instruction returns
 * \param insn an instruction
 * \return the element size (SZ enum type) or UNSIGNED_ERROR if there is a problem
 */
unsigned short insn_get_output_element_size_raw(insn_t* insn)
{
   return (insn != NULL) ? (insn->elt_out) & SZ_MASK : UNSIGNED_ERROR;
}

/*
 * Gets the input element type the instruction work on
 * \param insn an instruction
 * \param arch the architecture
 * \return the element type or UNSIGNED_ERROR if there is a problem
 */
unsigned int insn_get_input_element_type(insn_t* insn)
{
   return (insn != NULL) ? ((insn->elt_in) & T_MASK) >> 4 : UNSIGNED_ERROR;
}

/*
 * Gets the output element type the instruction returns
 * \param insn an instruction
 * \param arch the architecture
 * \return the element type or UNSIGNED_ERROR if there is a problem
 */
unsigned int insn_get_output_element_type(insn_t* insn)
{
   return (insn != NULL) ? ((insn->elt_out) & T_MASK) >> 4 : UNSIGNED_ERROR;
}

/*
 * Gets the size actually read by the instruction
 * \param insn An instruction
 * \return The size read (can be DATASZ_UNDEF) or DATASZ_UNDEF if \c insn is NULL
 */
data_size_t insn_get_read_size(insn_t* insn)
{
   return (insn != NULL) ? insn->read_size : DATASZ_UNDEF;
}

/*
 * Gets groups (as a list) containing the instruction
 * \param insn An instruction
 * \return list of groups
 */
list_t *insn_get_groups(insn_t *insn)
{
   loop_t *loop = insn_get_loop(insn);
   if (loop_get_groups(loop) == NULL)
      return NULL;

   list_t* groups = NULL;

   FOREACH_INLIST(loop_get_groups(loop), it_g) {
      group_t* group = GET_DATA_T(group_t*, it_g);

      FOREACH_INQUEUE(group->gdat, it_d)
      {
         group_elem_t* gelem = GET_DATA_T(group_elem_t*, it_d);

         if (gelem->insn == insn)
            groups = list_add_before(groups, group);
      }
   }

   return groups;
}

/*
 * Gets the first group containing the instruction
 * \param insn An instruction
 * \return first group or PTR_ERROR if not found
 */
group_t *insn_get_first_group(insn_t *insn)
{
   loop_t *loop = insn_get_loop(insn);
   if (loop_get_groups(loop) == NULL)
      return PTR_ERROR;

   FOREACH_INLIST(loop_get_groups(loop), it_g) {
      group_t* group = GET_DATA_T(group_t*, it_g);

      FOREACH_INQUEUE(group->gdat, it_d)
      {
         group_elem_t* gelem = GET_DATA_T(group_elem_t*, it_d);

         if (gelem->insn == insn)
            return group;
      }
   }

   return PTR_ERROR;
}

/*
 * Gets the architecture the instruction belongs to
 * \param insn The instruction
 */
arch_t* insn_get_arch(insn_t* insn)
{
   return (insn != NULL) ? insn->arch : NULL;
}

/*
 * Dumps the raw contents of an insn_t structure
 * \param i The pointer to the instruction
 */
void insn_dump(void* i)
{
   if (i == NULL)
      return;

   insn_t* in = (insn_t*) i;
   arch_t* archi = in->arch;
   if (archi == NULL)
      return;
   int it;

   printf("\naddress=%"PRIx64" - opprefx=%s - opcode=%s - opsuffx=%s",
         in->address, arch_get_prefsuff_name(archi, in->opprefx),
         arch_get_opcode_name(archi, in->opcode),
         arch_get_prefsuff_name(archi, in->opsuffx));
   printf("\noprnd list={");
   for (it = 0; it < insn_get_nb_oprnds(in); it++)
      oprnd_dump(insn_get_oprnd(in, it), archi);
   printf("\n} (end oprnd list)");
   printf("\ncoding=");
   bitvector_dump(in->coding, stdout);
   printf("\nbitsize=%d - fctlbl=%s ", BITVECTOR_GET_BITLENGTH(in->coding),
         label_get_name(in->fctlbl));
}

/*
 * Function for printing an instruction in the objdump style
 * \param insn A pointer to the instruction
 * \param c A char pointer to an allocated string
 * \param size The size of the allocated string
 */
void insn_print(insn_t* insn, char *c, size_t size)
{
   if (insn == NULL)
      return;

   assert(insn->arch);

   insn->arch->insn_print(insn, c, size);
}

/*
 * Function for printing an instruction in the objdump style directly to a file stream
 * \param insn A pointer to the instruction
 * \param f A pointer to a file stream
 */
void insn_fprint(insn_t* insn, FILE *fp)
{
   if (insn == NULL)
      return;

   assert(insn->arch);

   insn->arch->insn_fprint(insn, fp);
}

/*
 * Checks whether an instruction is a branch
 * \param insn A pointer to the structure holding the instruction
 * \return TRUE if the instruction is a branch, FALSE if not a branch
 */
int insn_is_branch(insn_t* insn)
{
   if (insn == NULL)
      return SIGNED_ERROR;

   if (insn->annotate & (A_JUMP | A_CONDITIONAL | A_CALL | A_RTRN)) {
      DBGMSG("Instruction at address %#"PRIx64" is a branch\n",
            INSN_GET_ADDR(insn));
      return TRUE;
   } else
      return FALSE;
}

/*
 * Checks if an instruction is an indirect branch
 * \param insn A pointer to the structure holding the instruction
 * \return TRUE if the instruction is an indirect branch, FALSE otherwise
 */
int insn_is_indirect_branch(insn_t* insn)
{
   if (insn == NULL)
      return FALSE;

   if ((insn->annotate & (A_JUMP | A_CONDITIONAL | A_CALL | A_RTRN))
         && (oprnd_is_ptr(insn_lookup_ref_oprnd(insn)) == FALSE)) {
      DBGMSG("Instruction at address %#"PRIx64" is an indirect branch\n",
            INSN_GET_ADDR(insn));
      return TRUE;
   } else
      return FALSE;
}

/*
 * Checks if an instruction is an direct branch
 * \param insn A pointer to the structure holding the instruction
 * \return TRUE if the instruction is a direct branch, FALSE otherwise
 */
int insn_is_direct_branch(insn_t* insn)
{
   if (insn == NULL)
      return FALSE;

   if ((insn->annotate & (A_JUMP | A_CONDITIONAL | A_CALL | A_RTRN))
         && (oprnd_is_ptr(insn_lookup_ref_oprnd(insn)) == TRUE)) {
      DBGMSG("Instruction at address %#"PRIx64" is a direct branch\n",
            INSN_GET_ADDR(insn));
      return TRUE;
   } else
      return FALSE;
}

/*
 * Checks whether an instruction is a jump (conditional or unconditional)
 * \param insn An instruction
 * \return TRUE/FALSE
 */
boolean_t insn_is_jump(insn_t *insn)
{
   if (insn == NULL)
      return FALSE;

   return (insn_get_annotate(insn) & A_JUMP) ? TRUE : FALSE;
}

/*
 * Checks whether an instruction is a conditional jump (taken/not taken depends on a condition)
 * \param insn An instruction
 * \return TRUE/FALSE
 */
boolean_t insn_is_cond_jump(insn_t *insn)
{
   if (insn == NULL)
      return FALSE;

   return
         ((insn_get_annotate(insn) & A_JUMP)
               && (insn_get_annotate(insn) & A_CONDITIONAL)) ? TRUE : FALSE;
}

/*
 * Checks whether an instruction is an unconditional jump (always taken or not taken)
 * \param insn An instruction
 * \return TRUE/FALSE
 */
boolean_t insn_is_uncond_jump(insn_t *insn)
{
   if (insn == NULL)
      return FALSE;

   return
         ((insn_get_annotate(insn) & A_JUMP)
               && !(insn_get_annotate(insn) & A_CONDITIONAL)) ? TRUE : FALSE;
}

/**
 * Checks if two instructions are identical. They must have the same opcode, list and types of operands
 * This is a fallback function if the architecture-specific functions are not defined
 * \param in1 First instruction
 * \param in2 Second instruction
 * \return TRUE if the instructions are equal (same opcode, same number of operands, same types of operands), FALSE otherwise
 */
static boolean_t insn_equal_common(insn_t* in1, insn_t* in2)
{
   assert(in1 && in2 && in1->arch == in2->arch);
   //Checks that opcodes are identical
   if (in1->opcode != in2->opcode)
      return FALSE;
   if (in1->opprefx != in2->opprefx)
      return FALSE;
   if (in1->opsuffx != in2->opsuffx)
      return FALSE;

   //Checks that the number of operands are identical
   if (in1->nb_oprnd != in2->nb_oprnd)
      return FALSE;

   oprnd_equal_fct_t oprnd_isequal =
         (in1->arch->oprnd_equal) ? in1->arch->oprnd_equal : oprnd_equal;
   //Checks that the operands are identical
   int i;
   for (i = 0; i < in1->nb_oprnd; i++) {
      if (!oprnd_isequal(in1->oprndtab[i], in2->oprndtab[i]))
         return FALSE;
   }
   return TRUE;
}

/*
 * Checks if two instructions are identical using the architecture-specific functions if available.
 * \param in1 First instruction
 * \param in2 Second instruction
 * \return TRUE if the instructions are equal (same opcode, same number of operands, same types of operands), FALSE otherwise
 */
boolean_t insn_equal(insn_t* in1, insn_t* in2)
{
   if ((!in1) || (!in2))
      return (in1 == in2); //If both instructions are NULL, they can be considered equal

   if (in1->arch != in2->arch)
      return FALSE;  //Instructions defined for different architectures

   if (in1->arch != NULL && in1->arch->insn_equal != NULL)
      return in1->arch->insn_equal(in1, in2);
   else
      return insn_equal_common(in1, in2); //If the arch-specific function is not defined or arch is NULL (should never happen but at least we are covered)
}

/*
 * Points the function label of the instruction to the given label structure
 * Also updates the label to set it to point to the instruction if the label and the instruction
 * have the same address
 * \param insn The instruction to update
 * \param fctlbl The structure containing the details about the label
 */
void insn_link_fct_lbl(insn_t* insn, label_t* fctlbl)
{
   if (insn != NULL) {
      insn->fctlbl = fctlbl;
      if (label_get_addr(fctlbl) == insn->address)
         label_set_target_to_insn(fctlbl, insn);
   }
}

/*
 * Sets the label of an instruction
 * \param insn The instruction to update
 * \param label The new label of the instruction
 */
void insn_set_fct_lbl(insn_t* insn, label_t* label)
{
   if (insn != NULL)
      insn->fctlbl = label;
}

/*
 * Sets the address of an instruction
 * \param insn The instruction to update
 * \param addr The new address of the instruction
 */
void insn_set_addr(insn_t* insn, int64_t addr)
{
   if (insn != NULL)
      insn->address = addr;
}

/*
 * Sets the annotate flag of an instruction (replacing the existing value)
 * \param insn The instruction to update
 * \param annotate The new value of the annotate flag
 */
void insn_set_annotate(insn_t* insn, unsigned int annotate)
{
   if (insn != NULL)
      insn->annotate = annotate;
}

/*
 * Sets the input element size
 * \param insn The instruction
 * \param The element size (DATASZ enum type)
 */
void insn_set_input_element_size(insn_t* insn, uint8_t element_size)
{
   if (insn == NULL)
      return;

   unsigned short size;
   // Conversion to a smaller enumeration in order to fit in 4 bits.
   switch (element_size) {
   case DATASZ_8b:
      size = SZ_8;
      break;
   case DATASZ_16b:
      size = SZ_16;
      break;
   case DATASZ_32b:
      size = SZ_32;
      break;
   case DATASZ_64b:
      size = SZ_64;
      break;
   case DATASZ_80b:
      size = SZ_80;
      break;
   case DATASZ_128b:
      size = SZ_128;
      break;
   case DATASZ_256b:
      size = SZ_256;
      break;
   case DATASZ_512b:
      size = SZ_512;
      break;
   default:
      size = SZ_UNDEF;
   }

   insn->elt_in = size | ((insn->elt_in) & T_MASK);
}

/*
 * Sets the debug information of an instruction
 * \param insn The instruction
 * \param srcname Name of the source file associated to the instruction
 * \param srcline Line number in the source file associated to the instruction
 * */
void insn_set_debug_info(insn_t* insn, char* srcname, unsigned int srcline)
{
   if (insn == NULL)
      return;
   DBGMSGLVL(2, "[%#"PRIx64"] Set debug info to %s:%u\n", insn_get_addr(insn), srcname, srcline);
   if (insn->debug == NULL)
      insn->debug = lc_malloc(sizeof(dbg_insn_t));
   insn->debug->srcfile = srcname;
   insn->debug->srcline = srcline;
}

/*
 * Sets the input element size
 * \param insn The instruction
 * \param The element size (SZ enum type)
 */
void insn_set_input_element_size_raw(insn_t* insn, uint8_t element_size)
{
   if (insn != NULL)
      insn->elt_in = element_size | ((insn->elt_in) & T_MASK);
}

/*
 * Sets the output element size
 * \param insn The instruction
 * \param The element size (DATASZ enum type)
 */
void insn_set_output_element_size(insn_t* insn, uint8_t element_size)
{
   if (insn == NULL)
      return;

   unsigned short size;
   // Conversion to a smaller enumeration in order to fit in 4 bits.
   switch (element_size) {
   case DATASZ_8b:
      size = SZ_8;
      break;
   case DATASZ_16b:
      size = SZ_16;
      break;
   case DATASZ_32b:
      size = SZ_32;
      break;
   case DATASZ_64b:
      size = SZ_64;
      break;
   case DATASZ_80b:
      size = SZ_80;
      break;
   case DATASZ_128b:
      size = SZ_128;
      break;
   case DATASZ_256b:
      size = SZ_256;
      break;
   case DATASZ_512b:
      size = SZ_512;
      break;
   default:
      size = SZ_UNDEF;
   }

   insn->elt_out = size | ((insn->elt_out) & T_MASK);
}

/*
 * Sets the output element size
 * \param insn The instruction
 * \param The element size (SZ enum type)
 */
void insn_set_output_element_size_raw(insn_t* insn, uint8_t element_size)
{
   if (insn != NULL)
      insn->elt_out = element_size | ((insn->elt_out) & T_MASK);
}

/*
 * Sets the input element type
 * \param insn The instruction
 * \param The element type
 */
void insn_set_input_element_type(insn_t* insn, uint8_t element_type)
{
   if (insn != NULL)
      insn->elt_in = (element_type << 4) | ((insn->elt_in) & SZ_MASK);
}

/*
 * Sets the output element type
 * \param insn The instruction
 * \param The element type
 */
void insn_set_output_element_type(insn_t* insn, uint8_t element_type)
{
   if (insn != NULL)
      insn->elt_out = (element_type << 4) | ((insn->elt_out) & SZ_MASK);
}

/*
 * Sets the size actually read by the instruction
 * \param insn An instruction
 * \param size The size read
 */
void insn_set_read_size(insn_t* insn, data_size_t size)
{
   if (insn != NULL)
      insn->read_size = size;
}

/*
 * Sets the architecture
 * \param insn The instruction
 * \param arch The architecture
 */
void insn_set_arch(insn_t* insn, arch_t* arch)
{
   if (insn != NULL)
      insn->arch = arch;
}

/*
 * Updates the annotate flag of an instruction by adding a flag
 * \param insn The instruction to update
 * \param annotate The annotate flag to add to the existing annotate flag of the instruction
 */
void insn_add_annotate(insn_t* insn, unsigned int annotate)
{
   if (insn != NULL)
      insn->annotate |= annotate;
}

/*
 * Updates the annotate flag of an instruction by removing a flag
 * \param insn The instruction to update
 * \param annotate The annotate flag to remove from the existing annotate flag of the instruction
 * */
void insn_rem_annotate(insn_t* insn, unsigned int annotate)
{
   if (insn != NULL)
      insn->annotate &= ~annotate;
}

/*
 * Updates the pointer to the next instruction of a branch instruction
 * \param insn The instruction to update (must be a branch instruction)
 * \param dest The instruction to which in branches
 */
void insn_set_branch(insn_t* insn, insn_t* dest)
{
   oprnd_t* op = insn_lookup_ref_oprnd(insn);
   if (oprnd_is_ptr(op) == TRUE)
      pointer_set_insn_target(oprnd_get_ptr(op), dest);
}

/*
 * Sets the instructions coding from a character string or a bitvector
 * \param insn The instruction to update
 * \param bytecode A byte string from which the coding will be read
 * \param len The lenght of the byte string
 * \param bvcoding A bitvector holding the new coding of the instruction (will be used if
 * bytecode is NULL)
 */
void insn_set_coding(insn_t* insn, unsigned char* bytecode, int len,
      bitvector_t* bvcoding)
{
   if (insn == NULL)
      return;

   bitvector_t *newcoding = NULL;
   if (bytecode != NULL)
      newcoding = bitvector_new_from_str(bytecode, len);
   else if (bvcoding != NULL)
      newcoding = bvcoding;

   bitvector_free(insn->coding);
   insn->coding = newcoding;
}

/*
 * Sets the extensions associated to an instruction
 * \param insn an instruction
 * \param a structure (defined by the user) which contains extensions
 */
void insn_set_ext(insn_t* insn, void* ext)
{
   if (insn != NULL)
      insn->ext = ext;
}

/*
 * Sets the identifier of the variant of an instruction
 * \param insn The instruction
 * \param variant_id The identifier of its variant
 * */
void insn_set_variant_id(insn_t* insn, uint32_t variant_id)
{
   if (insn != NULL)
      insn->variant_id = variant_id;

}

/*
 * Sets the instruction opcode name
 * \param insn The instruction to update
 * \param opcode The new name of the instruction
 */
void insn_set_opcode(insn_t* insn, short opcode)
{
   if (insn != NULL)
      insn->opcode = opcode;
}

/*
 * Sets the number of operands for the instruction and updates the
 * size of the array of operands accordingly
 * If the instruction already had operands, they will be kept
 * if the new number is bigger, otherwise the exceeding operands will be deleted
 * Any new created operand will be set to NULL
 * \param insn The instruction to update
 * \param noprnd The new number of operands
 */
void insn_set_nb_oprnds(insn_t* insn, int noprnd)
{
   // if same number of operands, so we don't do anything
   if (insn == NULL || insn->nb_oprnd == noprnd)
      return;

   int i;

   if (insn->nb_oprnd > noprnd) {
      /*The new number of operands is smaller than the original: we will be cropping the array*/
      //Freeing the operands in excedent
      for (i = noprnd; i < insn->nb_oprnd; i++) {
         oprnd_free(insn->oprndtab[i]);
      }

      if (noprnd == 0) {
         //We are removing all the operands: freeing the array
         lc_free(insn->oprndtab);
         insn->oprndtab = NULL;
      } else {
         //At least one operand remains: resizing the array
         insn->oprndtab = lc_realloc(insn->oprndtab,
               sizeof(insn->oprndtab[0]) * noprnd);
      }
   } else {
      /*The new number of operands is bigger than the original: we will be enlarging the array*/
      insn->oprndtab = lc_realloc(insn->oprndtab,
            sizeof(insn->oprndtab[0]) * noprnd);
      for (i = insn->nb_oprnd; i < noprnd; i++) {
         insn->oprndtab[i] = NULL;
      }
   }

   insn->nb_oprnd = noprnd;
}

/*
 * Sets the operand at the given index with the given operand pointer
 * \param insn The instruction to update
 * \param pos The index of the uperand to set
 * \param op The pointer to the new operand to set
 */
void insn_set_oprnd(insn_t* insn, int pos, oprnd_t* op)
{
   if (insn == NULL)
      return;

   if (pos >= insn->nb_oprnd) /*Index out of range*/
      return;

   oprnd_free(insn->oprndtab[pos]);
   insn->oprndtab[pos] = op;
}

/*
 * Sets the sequence (list object containing it) of an instruction
 * \param insn The instruction to update
 * \param sequence The list object containing it
 */
void insn_set_sequence(insn_t* insn, list_t* sequence)
{
   if (insn != NULL)
      insn->sequence = sequence;
}

/*
 * Sets the instruction opcode name
 * \param insn The instruction to update
 * \param opcodestr The new name of the instruction
 */
void insn_set_opcode_str(insn_t* insn, char* opcodestr)
{
   // no need to check for NULL insn: get_architecture will return NULL
   arch_t *arch = insn_get_arch(insn);
   if (opcodestr == NULL || arch == NULL)
      return;

   int opcode = 0;
   while ((opcode < arch->size_opcodes)
         && (!str_equal(opcodestr, arch->opcodes[opcode])))
      opcode++;

   if (opcode == arch->size_opcodes)
      return;
   else
      insn->opcode = opcode;
}

/*
 * Sets the instruction suffix
 * \param insn The instruction to update
 * \param opcode The new suffix of the instruction
 */
void insn_set_suffix(insn_t* insn, uint8_t suffix)
{
   if (insn != NULL)
      insn->opsuffx = suffix;
}

/*
 * Sets the instruction prefix
 * \param insn The instruction to update
 * \param opcode The new prefix of the instruction
 */
void insn_set_prefix(insn_t* insn, uint8_t prefix)
{
   if (insn != NULL)
      insn->opprefx = prefix;
}

/*
 * Sets the instruction suffix
 * \param insn The instruction to update
 * \param suffixstr The new suffix of the instruction
 */
void insn_set_suffix_str(insn_t* insn, char* suffixstr)
{
   // no need to check for NULL insn: get_architecture will return NULL
   arch_t *arch = insn_get_arch(insn);
   if (suffixstr == NULL || arch == NULL)
      return;

   int suffix = 0;
   while ((suffix < arch->size_pref_suff)
         && (!str_equal(suffixstr, arch->pref_suff[suffix])))
      suffix++;

   if (suffix == arch->size_pref_suff)
      return;
   else
      insn->opsuffx = suffix;
}

/*
 * Updates the annotate flag of an instruction. The given value will be or-ed
 * to its existing value
 * \param insn The instruction to update
 * \param annotate The flag to add to the existing annotate flag
 * */
void insn_upd_annotate(insn_t* insn, unsigned int annotate)
{
   if (insn != NULL)
      insn->annotate |= annotate;
}

/*
 * Updates a register oprnd in an instruction, including its coding if the driver
 * oprnd is not NULL. The function displays an error message and exits if the oprnd
 * is not found or is not a register
 * \param insn The instruction to update
 * \param regname The new name of the register
 * \param oprndidx The index of the oprnd to update (starting at 0)
 * \param driver The driver of the relevant architecture        \todo update this line
 * \param updopc If set to 1, will try to update the whole coding of the instruction including
 * the opcode. Otherwise will only try to update the operands (and display an warning if an error
 * is encountered)
 */
void insn_upd_reg_oprnd(insn_t* insn, reg_t* reg, int oprndidx,
      void*(*updinsncoding)(insn_t*, int, int64_t*), int updopcd)
{
   /*Reach oprnd*/
   oprnd_t* oprnd = insn_get_oprnd(insn, oprndidx);

   /*Checks oprnd has been reached*/
   if (!oprnd) {
      ERRMSG("Instruction has %d oprnds - oprnd %d unreachable\n",
            insn_get_nb_oprnds(insn), oprndidx);
      return;
      /*Checks oprnd is of type register*/
   } else if (oprnd->type != OT_REGISTER
         && oprnd->type != OT_REGISTER_INDEXED) {
      ERRMSG("Instruction oprnd %d is not a register\n", oprndidx);
      return;
   }

   /*Updates oprnd*/
   if (oprnd->type == OT_REGISTER)
      oprnd->data.reg = reg;
   else if (oprnd->type == OT_REGISTER_INDEXED)
      oprnd->data.rix->reg = reg;

   /*Updates coding*/
   if (updinsncoding != NULL)
      updinsncoding(insn, updopcd, NULL);
}

/*
 * Updates a register oprnd in an instruction, including its coding if the driver
 * oprnd is not NULL. The function displays an error message and exits if the oprnd
 * is not found or is not a immediate
 * \param insn The instruction to update
 * \param immval The new value of the immediate
 * \param oprndidx The index of the oprnd to update (starting at 0)
 * \param driver The driver of the relevant architecture \todo update this line
 * \param updopc If set to 1, will try to update the whole coding of the instruction including
 * the opcode. Otherwise will only try to update the operands (and display an warning if an error
 * is encountered)
 */
void insn_upd_imm_oprnd(insn_t* insn, imm_t immval, int oprndidx,
      void*(*updinsncoding)(insn_t*, int, int64_t*), int updopcd)
{
   /*Reach oprnd*/
   oprnd_t* oprnd = insn_get_oprnd(insn, oprndidx);

   /*Checks oprnd has been reached*/
   if (!oprnd) {
      ERRMSG("Instruction has %d oprnds - oprnd %d unreachable\n",
            insn_get_nb_oprnds(insn), oprndidx);
      return;
      /*Checks oprnd is of type immediate*/
   } else if (oprnd->type != OT_IMMEDIATE) {
      ERRMSG("Instruction oprnd %d is not an immediate\n", oprndidx);
      return;
   }

   /*Updates oprnd*/
   oprnd->data.imm = immval;

   /*Updates coding*/
   if (updinsncoding != NULL)
      updinsncoding(insn, updopcd, NULL);
}

/*
 * Updates a memory oprnd in an instruction, including its coding if the driver
 * oprnd is not NULL. The function displays an error message and exits if the oprnd
 * is not found or is not a memory address
 * \param insn The instruction to update
 * \param newmem A pointer to the structure holding the new memory operand
 * \param oprndidx The index of the oprnd to update (starting at 0)
 * \param driver The driver of the relevant architecture \todo update this line
 * \param updopc If set to 1, will try to update the whole coding of the instruction including
 * the opcode. Otherwise will only try to update the operands (and display an warning if an error
 * is encountered)
 */
void insn_upd_mem_oprnd(insn_t* insn, oprnd_t* newmem, int oprndidx,
      void*(*updinsncoding)(insn_t*, int, int64_t*), int updopcd)
{
   //Reach oprnd//
   oprnd_t* oprnd = insn_get_oprnd(insn, oprndidx);

   //Checks oprnd has been reached//
   if (!oprnd) {
      ERRMSG("Instruction has %d oprnds - oprnd %d unreachable\n",
            insn_get_nb_oprnds(insn), oprndidx);
      return;
      //Checks oprnd is of type memory//
   } else if (oprnd_is_mem(oprnd) == FALSE) {
      ERRMSG("Instruction oprnd %d is not a memory address\n", oprndidx);
      return;
   }

   //Updates oprnd
   memory_t* odm = oprnd_get_memory(oprnd);
   memory_t* ndm = oprnd_get_memory(newmem);

   odm->base = ndm->base;
   odm->index = ndm->index;
   odm->offset = ndm->offset;
   odm->scale = ndm->scale;
   odm->seg = ndm->seg;

   //Updates coding
   if (updinsncoding != NULL)
      updinsncoding(insn, updopcd, NULL);
}

/*
 * Updates the address and offset in an operand containing a pointer.
 * This function performs the following operations for a pointer_t inside the oprnd_t:
 * - If it is has a target, its address will be updated to that of the target
 * - If it is of type POINTER_RELATIVE and does not have a target, its address will be updated based on the address of insn_t and the offset,
 *   otherwise its offset will be updated based on its address and the address of insn_t.
 * This function invokes the architecture-specific function retrieved from the instruction architecture
 * \param insn The instruction containing the operand of type pointer
 * \param oprnd The operand. It is assumed to belong to the instruction, no additional test will be performed
 * */
void insn_oprnd_updptr(insn_t* insn, oprnd_t* ptroprnd)
{
   // no need to check for NULL insn: get_architecture will return NULL
   arch_t* arch = insn_get_arch(insn);
   if (!ptroprnd || !arch)
      return;

   pointer_t* ptr = oprnd_get_refptr(ptroprnd);
   arch->oprnd_updptr(insn, ptr);

   if (oprnd_get_type(ptroprnd) == OT_MEMORY_RELATIVE) {
      //Update the displacement of the memory operand
      /**\todo TODO (2015-04-17) Beware of architectures where this is not true!*/
      memory_set_offset(oprnd_get_memory(ptroprnd), pointer_get_offset(ptr));
   }
}

/*
 * Parses a list of instructions. The instructions must be separated by '\n' characters
 * and use a syntax compatible with the architecture
 * Labels must be a chain of characters beginning with a letter, dot ('.'), or underscore ('_'), followed by a colon (':').
 * \param insn_list List of instructions separated by '\n' characters
 * \param arch The architecture the instructions to parse belong to
 * \return Queue of pointers to insn_t structures
 * */
queue_t* insnlist_parse(char* insn_list, arch_t* arch)
{
   /**\todo TODO (2015-05-20) Add as return parameters an array of labels discovered in the list (or maybe as input parameter)
    * and of variable names discovered in the list (likewise, they could be input parameters), and the operands pointing to them.
    * This will allow the caller to link these operands to the appropriate objects*/
   if (insn_list == NULL || arch == NULL || strlen(insn_list) <= 0)
      return NULL;

   int i = 1, l;
   char* insnlist;
   char* strinsn;
   insn_t* in;
   label_t** labels = NULL;           // Array of labels encountered in the list
   int n_labels = 0;                   // Number of labels
   /**\note I'm betting here that we will not be using too much labels.
    *       Otherwise, a hashtable may be in order, but it seems overkill
    */
   label_t* pendinglbl = NULL;         // Last label found
   queue_t** pendingbranches = NULL; // Array of queues of instructions pointing to labels that were not yet encountered
   queue_t* out = queue_new(); // Initialising the list of instructions to return

   // Copying list to another object for strtok to work
   insnlist = lc_strdup(insn_list);

   //Splits the instruction list along its lines
   strinsn = strtok(insnlist, "\n");

   while (strinsn) {
      if (strinsn[0] == '.' && strinsn[1] == 't' && strinsn[2] == 'y'
            && strinsn[3] == 'p' && strinsn[4] == 'e') {
         i++;
         strinsn = strtok(NULL, "\n");
         continue; //In that case we will skip to the next line
      }

      // Checking if we are not dealing with a label (it is a string followed by a colon)
      char* lbl = strchr(strinsn, ':');
      if ((strinsn[0] == '.' || strinsn[0] == '_' || strinsn[0] == '<'
            || (strinsn[0] >= 'a' && strinsn[0] <= 'z')
            || (strinsn[0] >= 'A' && strinsn[0] <= 'Z')) && lbl != NULL) {
         char* c = strinsn;
         char* beginlbl = NULL, *endlbl = NULL;
         if (lbl == strinsn) {
            WRNMSG("Empty label found at line %d in list. Label ignored\n", i);
         } else {
            // We found a colon in the string: checking if it is preceded by a uninterrupted string or enclosed by brackets
            if (strinsn[0] == '<') {
               beginlbl = strinsn + 1;
               while (*c != '\0' && *c != '>')
                  c++;
               if (*c == '\0') {
                  ERRMSG("Unclosed bracket found at line %d. Skipping line\n",
                        i);
                  i++;
                  strinsn = strtok(NULL, "\n");
                  continue;
               }
               endlbl = c;
               c = lbl;
            } else {
               beginlbl = strinsn;
               while (c != lbl && *c != ' ')
                  c++;

               if (c == lbl)
                  endlbl = c; // No space found: we consider that we are indeed dealing with a label
            }
            if (endlbl != NULL) {
               // We could find the end of the label
               // Copying the label to a string
               char label[endlbl - beginlbl + 1];
               lc_strncpy(label, endlbl - beginlbl + 1, beginlbl,
                     endlbl - beginlbl);
               label[endlbl - beginlbl] = '\0';
               DBGMSG("Declaration of label %s at line %d\n", label, i);

               // Checking if the label does not already exists
               l = 0;
               while (l < n_labels) {
                  if (str_equal(label, label_get_name(labels[l]))) {
                     if (label_get_addr(labels[l]) >= 0) {
                        WRNMSG(
                              "Label %s at line %d was already defined at line %d. Second declaration ignored\n",
                              label, i, (int )label_get_addr(labels[l]));
                        break;
                     } else {
                        DBGMSG(
                              "Label %s already encountered but not yet associated to an instruction\n",
                              label);
                        // Label already encountered (as a destination) but not yet associated to an instruction
                        label_set_addr(labels[l], i);
                        pendinglbl = labels[l];
                        break;
                     }
                  }
                  l++;
               }
               if (l == n_labels) {
                  DBGMSG("Label %s had not been encountered yet\n", label);

                  // Label was not found: adding it to the list of labels
                  labels = lc_realloc(labels, sizeof(char*) * (n_labels + 1));
                  pendinglbl = label_new(label, i, TARGET_INSN, NULL);
                  labels[n_labels] = pendinglbl;

                  // Reserves the space for branches pointing to this label
                  pendingbranches = lc_realloc(pendingbranches,
                        sizeof(queue_t*) * (n_labels + 1));
                  pendingbranches[n_labels] = NULL; //There will be no need for that as the label is now defined
                  n_labels++;
               }
               // Now checking if the label was alone on its line or followed by an instruction
               c++;
               // Skipping spaces
               while (*c != '\0' && (*c == ' ' || *c == '\t'))
                  c++;
               // Assigning the remainder of the string as to be parsed if not finished
               if (*c != '\0')
                  strinsn = c;
               else {
                  DBGMSG("Line %d contains only a label declaration\n", i);
                  i++;
                  strinsn = strtok(NULL, "\n");
                  continue; //In that case we will skip to the next line
               }
            } //Line contains a colon, but not a label (colon may belong to a segment for instance)
         } //End of label handling
      } //Either no label was detected or it has been handled
      in = insn_parsenew(strinsn, arch);
      if (in) {
         // Checks if we encountered a label previously and are waiting to associate an instruction
         if (pendinglbl != NULL) {
            DBGMSG("Instruction at line %d is associated to label %s\n", i,
                  label_get_name(pendinglbl));
            label_set_target_to_insn(pendinglbl, in);
            pendinglbl = NULL;
         }
         oprnd_t* refop = insn_lookup_ref_oprnd(in);
         // Checks if a label is present in this instruction
         if (oprnd_is_ptr(refop) == TRUE) {
            // Dirty trick: the index of the label is the value of the pointer.
            /**\todo Change this (see \ref insn_parsenew)*/
            int lblidx = pointer_get_addr(oprnd_get_ptr(refop));
            pointer_set_addr(oprnd_get_ptr(refop), 4);
            DBGMSG("Pointer value retrieved: %d\n", lblidx);
            l = 0;

            char* c = strinsn + lblidx;
            while (*c != ',' && *c != '\0' && *c != '>')
               c++;

            char label[c - (strinsn + lblidx) + 1];
            lc_strncpy(label, c - (strinsn + lblidx) + 1, (strinsn + lblidx),
                  c - (strinsn + lblidx));
            label[c - (strinsn + lblidx)] = '\0';

            // Looking for the label in the list of labels
            while (l < n_labels) {
               if (str_equal(label, label_get_name(labels[l])))
                  break;
               l++;
            }
            if (l < n_labels) {
               if (label_get_target(labels[l]) != NULL)
                  //Label already found: setting the instruction to point to it
                  pointer_set_insn_target(oprnd_get_ptr(refop),
                        label_get_target(labels[l]));
               else
                  //Label already found but not defined: adding the instruction to the pending list
                  queue_add_tail(pendingbranches[l], in);
            } else {
               DBGMSG(
                     "Instruction at line %d branches to label %s that was not yet encountered\n",
                     i, label);

               // Label was not found: adding it to the list of labels
               labels = lc_realloc(labels, sizeof(char*) * (n_labels + 1));
               labels[n_labels] = label_new(label, -1, TARGET_INSN, NULL);

               //Reserves the space for branches pointing to this label
               pendingbranches = lc_realloc(pendingbranches,
                     sizeof(queue_t*) * (n_labels + 1));
               pendingbranches[n_labels] = queue_new();
               queue_add_tail(pendingbranches[n_labels], in); //Adding the instruction to the list of branches to resolve
               n_labels++;
            }
         } //Not a pointer: we are finished with this line
           // Adding the instruction to the list
         add_insn_to_insnlst(in, out);
      }
      // Lines starting by a "." not containing a ':' and where the instruction parsing failed are compiler directives
      else if (strinsn[0] != '.' || lbl != NULL) {
         // Instruction could not be decoded correctly
         char* env_v = getenv("_MAQAO_DBG_MSG");
         // Here use an environment variable to enable or not the message.
         // If the variable does not exist, print it
         // If the variable exists and is 1, print it
         if ((env_v != NULL && strcmp(env_v, "1") == 0) || env_v == NULL) {
            ERRMSG(
                  "Instruction at line %d: \"%s\" could not be parsed for architecture %s\n",
                  i, strinsn, arch->name);
         }
      }
      // Next line
      i++;
      strinsn = strtok(NULL, "\n");
   } // End of parsing of the instruction list

   // Resolving branches
   for (l = 0; l < n_labels; l++) {
      if ((pendingbranches[l] != NULL)
            && (queue_length(pendingbranches[l]) > 0)) {
         // We found a label for which there are pending branches
         if (label_get_target(labels[l]) != NULL) {
            FOREACH_INQUEUE(pendingbranches[l], iter)
            {
               // Associate pending branch to the label
               insn_set_branch(GET_DATA_T(insn_t*, iter),
                     label_get_target(labels[l]));
            }
         } else {
            ERRMSG("Undefined branch target %s\n", label_get_name(labels[l]));
         }
      }
      //Freeing the list of pending branches
      if (pendingbranches[l] != NULL)
         queue_free(pendingbranches[l], NULL);
   }

   // Freeing structures used for labels
   if (n_labels > 0) {
      lc_free(pendingbranches);
      for (l = 0; l < n_labels; l++)
         label_free(labels[l]);
      lc_free(labels);
   }
   lc_free(insnlist);
   return out;
}

/**
 * Retrieves the debug data of an instruction
 * @param insn The instruction
 * @return Its debug data or NULL if it is NULL or has no debug data
 */
static dbg_insn_t* get_debug(insn_t* insn)
{
   return (insn != NULL) ? insn->debug : NULL;
}

/*
 * Gets instruction source line
 * \param insn an instruction
 * \return instruction source line or 0 if not available
 */
unsigned int insn_get_src_line(insn_t* insn)
{
   dbg_insn_t* dbg = get_debug(insn);
   return (dbg != NULL) ? dbg->srcline : 0;
}

/*
 * Gets instruction source column
 * \param insn an instruction
 * \return instruction source column or 0 if not available
 */
unsigned int insn_get_src_col(insn_t* insn)
{
   (void) insn;
   return 0;
}

/*
 * Gets instruction source file
 * \param insn an instruction
 * \return instruction source file or NULL if not available
 */
char* insn_get_src_file(insn_t* insn)
{
   dbg_insn_t* dbg = get_debug(insn);
   return (dbg != NULL) ? dbg->srcfile : NULL;
}

/*
 * Retrieves the register type of a given instruction's operand
 * \param insn An instruction
 * \param pos The index of the register operand whose type we want to retrieve (starting at 0)
 * \return The type of the register operand at the given index, or -1 if it is not a register or does not exist
 */
char insn_get_reg_oprnd_type(insn_t* insn, int pos)
{
   if ((insn == NULL) || (pos >= insn->nb_oprnd)
         || (insn->oprndtab[pos] == NULL))
      return -1;

   reg_t* reg = oprnd_get_reg(insn->oprndtab[pos]);
   return (reg != NULL) ? reg->type : -1;
}

/*
 * Gets the type of given instruction's operand
 * \param insn The instruction to update
 * \param pos The index of the operand whose type we want to retrieve (starting at 0)
 * \return The type of the operand at the given index
 */
int insn_get_oprnd_type(insn_t* insn, int pos)
{
   if ((insn == NULL) || (pos >= insn->nb_oprnd)
         || (insn->oprndtab[pos] == NULL))
      return -1;

   return insn->oprndtab[pos]->type;
}

/*
 * Gets the size of given instruction's operand
 * \param insn The instruction
 * \param pos The index of the operand whose size we want to retrieve (starting at 0)
 * \return The bitsize value for the operand at the given position, or -1 if the operand does not exist
 */
int insn_get_oprnd_bitsize(insn_t* insn, int pos)
{
   if ((insn == NULL) || (pos >= insn->nb_oprnd)
         || (insn->oprndtab[pos] == NULL))
      return -1;

   return insn->oprndtab[pos]->bitsize;
}

/*
 * Gets the next instruction
 * \param insn The instruction
 * \return The next instruction or NULL if there is no next instruction.
 */
insn_t* insn_get_next_insn(insn_t* insn)
{
   list_t* ptrListNextInsn = NULL;
   insn_t* nextInsn = NULL;
   if (insn != NULL && insn->sequence != NULL) {
      ptrListNextInsn = list_getnext(insn->sequence);
      if (ptrListNextInsn != NULL)
         nextInsn = ptrListNextInsn->data;
   }
   return nextInsn;
}

/*
 * Compares two insn_t structures based on the address referenced by a pointer they contain. This function is intended to be used with qsort.
 * \param i1 First instruction
 * \param i2 Second instruction
 * \return -1 if the address referenced by the pointer contained in i1 is less than the address referenced by the pointer contained in i2,
 * 0 if they are equal and 1 otherwise
 * */
int insn_cmpptraddr_qsort(const void* i1, const void* i2)
{
   /**\warning (2014-12-05) This function considers that an instruction in this hashtable can have only one operand containing a pointer (either
    * memrel_t or pointer_t). So far it is mainly intended to be used when ordering unlinked pointers.
    * If/when we handle later types of operands containing pointer_t that can be simultaneously present in an instruction (such as MEMORY_RELATIVE
    * and IMMEDIATE_ADDRESS), we have to find a way to handle them if instructions containing multiple such pointers need to be ordered by their
    * destination. So far I'm counting on the fact that they will not need to be linked at the same stage: IMMEDIATE_ADDRESS pointers, for instance,
    * would need to be filled after some analysis, while MEMORY_RELATIVE can be filled during disassembly. But who knows what the future holds a Friday
    * afternoon before a VI-HPS.... (consider using multiple hashtable/queues, one per type of operand)*/
   insn_t* insn1 = *(insn_t**) i1;
   insn_t* insn2 = *(insn_t**) i2;
   int64_t addr1 = oprnd_get_refptr_addr(insn_lookup_ref_oprnd(insn1));
   int64_t addr2 = oprnd_get_refptr_addr(insn_lookup_ref_oprnd(insn2));
   if (addr1 < addr2)
      return -1;
   else if (addr1 == addr2)
      return 0;
   else
      return 1;
}

/*
 * Indicate flags that are
 *    - read
 *    - set (to 1)
 *    - cleared (to 0)
 *    - defined: according to the result
 *    - undefined
 */
int opcode_altered_flags(int opcode, uint8_t *read, uint8_t *set,
      uint8_t *cleared, uint8_t * def, uint8_t *undef)
{
   return 0;
}

/*
 * Indicate flags that are
 *    - read
 *    - set (to 1)
 *    - cleared (to 0)
 *    - defined: according to the result
 *    - undefined
 */
int insn_altered_flags(insn_t *in, uint8_t *read, uint8_t *set,
      uint8_t *cleared, uint8_t * def, uint8_t *undef)
{

   return opcode_altered_flags(in->opcode, read, set, cleared, def, undef);
}

/* Indicate if flags modified by the instruction override given flags */
int insn_flags_override_test(int opcode, uint8_t flags)
{
   uint8_t rd, fs, set, cleared, def, undef;

   opcode_altered_flags(opcode, &rd, &set, &cleared, &def, &undef);

   fs = set | cleared | def | undef;

   return ((rd & flags) == 0 && (fs & flags) == flags);
}

/*
 * Return the opcode of the "INC" instruction
 */
int insn_inc_opcode()
{
   return 0;
}
