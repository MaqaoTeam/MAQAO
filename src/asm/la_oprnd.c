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

#define MAX_REG_NAME_LEN 16   /**<Max length for a register name (used for parsing)*/

///////////////////////////////////////////////////////////////////////////////
//                                 register                                  //
///////////////////////////////////////////////////////////////////////////////
/*
 * Creates a new register from its codes
 * \param reg_name register name code (defined in arch_t structure)
 * \param reg_type register type code (defined in arch_t structure)
 * \param arch architecture the register belongs to
 * \return a new register
 */
reg_t* reg_new(int reg_name, int reg_type, arch_t* arch)
{
   if (arch == NULL)
      return NULL;

   if (reg_type < 0 || reg_type >= arch->nb_type_registers || reg_name < 0
         || reg_name >= arch->nb_names_registers) {
      if (reg_type == RIP_TYPE) {
         DBGMSG("Creating RIP register with name %d and type %d (pointer %p)\n",
               reg_name, reg_type, arch->reg_rip);
         return arch->reg_rip;
      }
      DBGMSG(
            "Register type %d or name %d is outside acceptable ranges. No register created\n",
            reg_type, reg_name);
      return NULL;
   }
   DBGMSG("Creating register with name %d and type %d (pointer %p)\n", reg_name,
         reg_type, arch->regs[reg_type][reg_name]);
   return arch->regs[reg_type][reg_name];
}

static int _isalpha(char c)
{
   return (((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z')));
}

static int _isdigit(char c)
{
   return (c >= '0') && (c <= '9');
}

/*
 * Parses a string to find a register name belonging to the given architecture
 * \note matches %abc123 pattern (AT&T syntax)
 * \param insn_str The string to parse
 * \param pos Pointer to the index value in the string at which to start the search.
 * Will be updated to point to the character immediately after the register name
 * \param arch The architecture against which the register must be parsed
 * \return Pointer to the register whose name is in the string, or NULL if no register was parsed
 */
reg_t* reg_parsenew(char* insn_str, int *pos, arch_t* arch)
{
   /** \todo Remove the use of pos and only use the string pointer (update also oprnd_parsenew and insn_parsenew)*/

   if (arch == NULL || arch->reg_names == NULL || pos == NULL)
      return NULL;
   int c = *pos, rtype = 0, rname = 0, p = 0;
   char regname[MAX_REG_NAME_LEN]; //Supposing here no register name will be larger than 16 bytes

   // Looking for starting % character
   if (insn_str[c] == '%') {
      c++;
      *pos = c;
   }

   // Extending match up to end of pattern (closing parenthesis or end of string)
   p = c;
   while ( insn_str[c] != '\0' && (p - (*pos)) < MAX_REG_NAME_LEN
         && (_isalpha(insn_str[c]) || _isdigit(insn_str[c])
               || insn_str[c] == '(' || insn_str[c] == '_')) {
      /**\note (2016-10-11) The test on MAX_REG_NAME_LEN is there to 
       * prevent an overflow in regname*/
      if ((insn_str[c] == '(') && _isdigit(insn_str[c + 1])
            && (insn_str[c + 2] == ')')) {
         regname[p - (*pos)] = insn_str[c + 1];
         p++;
         c += 3;
      } else {
         regname[p - (*pos)] = insn_str[c];
         p++;
         c++;
      }
   }

   if (c == *pos)
      return NULL; //The name is invalid: exiting with NULL

   regname[p - (*pos)] = '\0';

   /* Checking if the register is the RIP*/
   if (str_equal(regname, arch_get_reg_rip_name(arch))) {
      *pos = c;
      return arch->reg_rip;
   }

   rtype = 0;
   /*Searching the register name in the architecture*/
   while (rtype < arch->nb_type_registers) {
      int found = FALSE;
      rname = 0;
      /*Search the register name among names of type rtype*/
      while ((rname < arch->nb_names_registers)
            && (arch->reg_names[rtype][rname] != NULL)) {
         if (str_equal(regname, arch->reg_names[rtype][rname])) {
            /*Name found*/
            found = TRUE;
            break;
         }
         rname++;
      } // Completed search on registers of type rtype

      if (found == TRUE)
         break;

      rtype++;
   }

   /*Either we have a match, or we reached the end of the types (reg_new will return NULL in that case)*/
   reg_t* out = reg_new(rname, rtype, arch);
   if (out != NULL)
      *pos = c;

   return out;
}

/*
 * Gets a register family
 * \param reg a register
 * \param arch an architecture
 * \return The identifier of the register family (those are defined in the appropriate <archname>_arch.h header)
 * or SIGNED_ERROR if the register type is not valid (or RIP_TYPE) or if \c arch is NULL
 */
char reg_get_family(reg_t* reg, arch_t* arch)
{
   if (reg == NULL)
      return SIGNED_ERROR;

   return arch_get_reg_family(arch, reg_get_type(reg));
}

/*
 * Gets register type code
 * \param reg a register
 * \return the register type code
 */
char reg_get_type(reg_t* reg)
{
   return (reg != NULL) ? reg->type : R_NONE;
}

/*
 * Gets register name code
 * \param reg a register
 * \return the register name code
 */
char reg_get_name(reg_t* reg)
{
   return (reg != NULL) ? reg->name : R_NONE;
}

/*
 * Delete an existing register
 * \param p a pointer on a register
 */
void reg_free(void* p)
{
   (void) p;
   // nothing to free (CF reg_new)
}

///////////////////////////////////////////////////////////////////////////////
//                                 indexed register                          //
///////////////////////////////////////////////////////////////////////////////

/*
 * Creates a new structure for storing an indexed register
 * \param reg The reg_t structure representing the register
 * \param idx The index of the register
 * \return A new regidx_t structure
 * */
regidx_t* regidx_new(reg_t* reg, uint8_t idx)
{
   if (!reg)
      return NULL;

   regidx_t* out = lc_malloc(sizeof *out);
   out->reg = reg;
   out->idx = idx;
   return out;
}

/*
 * Frees a structure storing an indexed register
 * \param regidx A regidx_t structure
 * */
void regidx_free(regidx_t* regidx)
{
   lc_free(regidx);
}

///////////////////////////////////////////////////////////////////////////////
//                                      pointer                              //
///////////////////////////////////////////////////////////////////////////////

/**
 * Prints a pointer operand in a format similar to objdump
 * \param insn The instruction the operand belongs to. If not NULL, the destination
 * address of the operand will be printed, otherwise its value will be
 * \param ptr The pointer operand to print
 * \param out_str output string
 * \param size The size of the char array
 * \return The number of characters actually written
 */
static int oprnd_ptr_printdump(insn_t* insn, pointer_t* ptr, char* out_str,
      size_t size)
{
   assert(ptr != NULL && out_str != NULL);
   int written = 0;
   if (insn != NULL) {
      //Print branch label (if exists)
      insn_t* branchdest = ptr->target.insn;
      int64_t branch =
            (branchdest != NULL) ?
                  insn_get_addr(branchdest) : pointer_get_addr(ptr);
      label_t* destlbl = insn_get_fctlbl(branchdest); //Retrieves the label of the destination

      if (destlbl != NULL) {
         int64_t lbloffs = branch - label_get_addr(destlbl); //Calculates the offset to the label
         if (lbloffs > 0)
            written = lc_sprintf(out_str, size, "%"PRIx64" <%s+%#"PRIx64">", branch,
                  label_get_name(destlbl), lbloffs);
         else
            written = lc_sprintf(out_str, size, "%"PRIx64" <%s>", branch,
                  label_get_name(destlbl));
      } else {
         written = lc_sprintf(out_str, size, "%"PRIx64, branch);
      }
   } else
      written = lc_sprintf(out_str, size, "%#"PRIx64, pointer_get_addr(ptr));
   return written;
}

/**
 * Prints a pointer operand in a format similar to objdump in a file stream
 * \param insn The instruction the operand belongs to. If not NULL, the destination
 * address of the operand will be printed, otherwise its value will be
 * \param ptr The pointer operand to print
 * \param out_fp output file pointer
 */
static void oprnd_ptr_fprint(insn_t* insn, oprnd_t* op, FILE* out_fp)
{
   assert(op != NULL && out_fp != NULL);
   pointer_t* ptr = op->data.ptr;
   int64_t branch = pointer_get_addr(ptr);

   if (insn != NULL) {
      //Print branch label (if exists)
      insn_t* branchdest = ptr->target.insn;
      int64_t branch =
            (branchdest != NULL) ?
                  insn_get_addr(branchdest) : pointer_get_addr(ptr);
      label_t* destlbl = insn_get_fctlbl(branchdest); //Retrieves the label of the destination

      if (destlbl != NULL) {
         int64_t lbloffs = branch - label_get_addr(destlbl); //Calculates the offset to the label
         if (lbloffs > 0)
            fprintf(out_fp, "%"PRIx64" <%s+%#"PRIx64">", branch,
                  label_get_name(destlbl), lbloffs);
         else
            fprintf(out_fp, "%"PRIx64" <%s>", branch, label_get_name(destlbl));
      } else {
         fprintf(out_fp, "%"PRIx64, branch);
      }
   } else
      fprintf(out_fp, "%#"PRIx64, pointer_get_addr(ptr));
}

/*
 * Create a new pointer operand
 * \param addr Address of the target object
 * \param offset Offset from the pointer to the target object (for relative pointers)
 * \param next Element targeted by the pointer (NULL if unknown)
 * \param pointer_type Type of the pointer value
 * \param target_type Type of the pointer target
 * \return A new initialized pointer
 */
pointer_t* pointer_new(maddr_t addr, pointer_offset_t offset, void* next,
      pointer_type_t pointer_type, target_type_t target_type)
{
   pointer_t* new = lc_malloc0(sizeof *new);
   new->target_type = target_type;

   switch (target_type) {
   case TARGET_INSN:
      new->target.insn = next;
      if (next && (addr == 0))
         addr = insn_get_addr(next);
      break;
   case TARGET_DATA:
      new->target.data = next;
      if (next && (addr == 0))
         addr = data_get_addr(next);
      break;
   case TARGET_BSCN:
      new->target.bscn = next;
      break;
   default:
      new->target.insn = next;
      new->target_type = TARGET_UNDEF;
      break;
   }

   new->type = pointer_type;
   new->addr = addr;
   new->offset = offset;

   return new;
}

/*
 * Frees a pointer operand
 * \param p Pointer to free
 * */
void pointer_free(void* p)
{
   if (p)
      lc_free(p);
}

/*
 * Duplicates a pointer operand from an existing one. The object targetted by the copy
 * will be the same as the original
 * \param srcptr Source pointer to duplicate
 * \return A newly allocated pointer_t structure targetting the same element, or NULL if srcptr is NULL
 * */
pointer_t* pointer_copy(pointer_t* srcptr)
{
   if (!srcptr)
      return NULL;
   pointer_t* outptr = lc_malloc0(sizeof(pointer_t));
   outptr->target_type = srcptr->target_type;
   switch (srcptr->target_type) {
   case TARGET_INSN:
      outptr->target.insn = srcptr->target.insn;
      break;
   case TARGET_DATA:
      outptr->target.data = srcptr->target.data;
      break;
   case TARGET_BSCN:
      outptr->target.bscn = srcptr->target.bscn;
      break;
   }
   outptr->type = srcptr->type;
   outptr->offset = srcptr->offset;
   outptr->addr = srcptr->addr;
   return outptr;
}

/*
 * Gets the address referenced by a pointer operand
 * \param p The pointer
 * \return The address referenced by the pointer, or ADDRESS_ERROR if p is NULL
 * */
maddr_t pointer_get_addr(pointer_t* p)
{
   return (p) ? p->addr : ADDRESS_ERROR ;
}

/*
 * Gets the offset of a pointer operand (will be 0 for absolute pointers)
 * \param p A pointer operand
 * \return The offset to the address referenced by the pointer, or SIGNED_ERROR if p is NULL
 * */
pointer_offset_t pointer_get_offset(pointer_t* p)
{
   return (p) ? p->offset : SIGNED_ERROR;
}
/*
 * Updates the target of a pointer. New target is assumed to have the same type as the existing one
 * \param p The pointer
 * \param target The new target object
 * */
void pointer_upd_target(pointer_t* p, void* target)
{
   if (!p)
      return;
   switch (p->target_type) {
   case TARGET_INSN:
      p->target.insn = target;
      break;
   case TARGET_DATA:
      p->target.data = target;
      break;
   case TARGET_BSCN:
      p->target.bscn = target;
      break;
   }
}

/*
 * Updates the address of a pointer with regard to its target.
 * This function should not be used for pointers found in the operand of an instruction (\ref insn_oprnd_updptr should be used instead)
 * \param p The pointer
 * */
void pointer_upd_addr(pointer_t* p)
{
   if (!p)
      return;
   int64_t address = pointer_get_target_addr(p)
         + pointer_get_offset_in_target(p);
   if (pointer_has_target(p)) {
      p->addr = address;
   }
   if (p->type == POINTER_RELATIVE && p->relative_origin) {
      //Relative pointer with an origin
      if (p->origin_type == TARGET_INSN) {
         insn_get_arch(p->relative_origin)->oprnd_updptr(p->relative_origin, p);
      } else if (p->origin_type == TARGET_DATA) {
         int64_t addrbase = data_get_addr(p->relative_origin);
         if (pointer_has_target(p)) {
            //Pointer has a target: we consider its address is up to date, so it's the offset we need to update
            p->offset = address - addrbase;
         } else {
            //No target: we will update the address of the pointer based on its offset
            p->addr = p->offset + addrbase;
         }
      }
   } //Otherwise the pointer is either not relative, or we need to know its container to update it
}

/*
 * Sets an instruction object as the target of a pointer.
 * \param p The pointer
 * \param target The destination instruction
 * */
void pointer_set_insn_target(pointer_t* p, insn_t* target)
{
   if (!p)
      return;
   p->target_type = TARGET_INSN;
   p->target.insn = target;
   DBGMSGLVL(1, "Linking pointer %p to instruction %p\n", p, target);
}

/*
 * Sets a data object as the target of a pointer.
 * \param p The pointer
 * \param target The destination data
 * */
void pointer_set_data_target(pointer_t* p, data_t* target)
{
   if (!p)
      return;
   p->target_type = TARGET_DATA;
   p->target.data = target;
   DBGMSGLVL(1, "Linking pointer %p to data %p\n", p, target);
}

/*
 * Sets a section  object as the target of a pointer.
 * \param p The pointer
 * \param target The destination section
 * */
void pointer_set_bscn_target(pointer_t* p, binscn_t* target)
{
   if (!p)
      return;
   p->target_type = TARGET_BSCN;
   p->target.bscn = target;
   DBGMSGLVL(1, "Linking pointer %p to section %p\n", p, target);
}

/*
 * Sets the origin of a relative pointer (object whose address must be used to compute the destination from the offset)
 * \param p The pointer. Must be of type POINTER_RELATIVE
 * \param origin Object whose address must be used for computing the destination from the origin for relative pointers
 * \param origin_type Type of the origin object
 * */
void pointer_set_relative_origin(pointer_t* p, void* origin,
      target_type_t origin_type)
{
   if (!p || (p->type != POINTER_RELATIVE))
      return;
   p->relative_origin = origin;
   p->origin_type = origin_type;
}

/*
 * Gets the instruction pointed by the pointer
 * \param p a pointer operand
 * \return a pointer on the targeted instruction or PTR_ERROR if p is null or does not target an instruction
 */
insn_t* pointer_get_insn_target(pointer_t* p)
{
   return (
         ((p != NULL) && (p->target_type == TARGET_INSN)) ?
               p->target.insn : PTR_ERROR);
}

/*
 * Get the data pointed by the pointer
 * \param p a pointer operand
 * \return a pointer on the targeted data or PTR_ERROR if p is null or does not target a data element
 */
data_t* pointer_get_data_target(pointer_t* p)
{
   return (
         ((p != NULL) && (p->target_type == TARGET_DATA)) ?
               p->target.data : PTR_ERROR);
}

/*
 * Get the binary section pointed by the pointer
 * \param p a pointer operand
 * \return a pointer on the targeted binary section or PTR_ERROR if p is null or does not target a binary section
 */
binscn_t* pointer_get_bscn_target(pointer_t* p)
{
   return (
         ((p != NULL) && (p->target_type == TARGET_BSCN)) ?
               p->target.bscn : PTR_ERROR);
}

/*
 * Gets the address of the object targeted to by the pointer
 * \param p A pointer
 * \return The address of the object targeted by the pointer, or SIGNED_ERROR if p or its target is NULL
 * */
maddr_t pointer_get_target_addr(pointer_t* p)
{
   if (!p)
      return SIGNED_ERROR;
   switch (p->target_type) {
   case TARGET_INSN:
      return ((p->target.insn) ? p->target.insn->address : SIGNED_ERROR);
   case TARGET_DATA:
      return ((p->target.data) ? p->target.data->address : SIGNED_ERROR);
   case TARGET_BSCN:
      return ((p->target.bscn) ? p->target.bscn->address : SIGNED_ERROR);
   default:
      return SIGNED_ERROR;
   }
}

/*
 * Get the type of a pointer
 * \param p A pointer
 * \return The pointer type or POINTER_UNKNOWN if p is null
 */
unsigned pointer_get_type(pointer_t* p)
{
   return ((p != NULL) ? p->type : POINTER_UNKNOWN);
}

/*
 * Get the type of the target of a pointer
 * \param p A pointer
 * \return The pointer type or TARGET_UNDEF if p is null
 */
unsigned pointer_get_target_type(pointer_t* p)
{
   return ((p != NULL) ? p->target_type : TARGET_UNDEF);
}

/*
 * Get the offset of a pointer
 * \param p A pointer
 * \return The value of the pointer or UINT16_MAX if \c p is NULL
 * */
pointer_offset_intarget_t pointer_get_offset_in_target(pointer_t* p)
{
   return p ? p->offset_intarget : UINT16_MAX;
}

/*
 * Get the element whose address must be used to compute the destination address of a relative pointer if different from the element
 * containing the pointer
 * \param p A pointer operand
 * \return The element to use to compute its destination address or NULL if \c p is NULL
 * */
void* pointer_get_relative_origin(pointer_t* p)
{
   return p ? p->relative_origin : NULL;
}

/*
 * Get the type of the element whose address must be used to compute the destination address of a relative pointer if different from the element
 * containing the pointer
 * \param p A pointer
 * \return The type of the element to use to compute its destination address or TARGET_UNDEF if \c p is NULL
 * */
unsigned pointer_get_origin_type(pointer_t* p)
{
   return p ? p->origin_type : TARGET_UNDEF;
}

/*
 * Sets the address referenced by a pointer
 * \param p A pointer
 * \param addr The address referenced by the pointer
 * */
void pointer_set_addr(pointer_t* p, maddr_t addr)
{
   if (p == NULL)
      return;
   p->addr = addr;
   DBGMSG("Pointer %p now has absolute address %#"PRIx64"\n", p, addr);
}

/*
 * Sets the offset to the address referenced by a pointer
 * \param p A pointer
 * \param offset The offset to the address referenced by the pointer
 * */
void pointer_set_offset(pointer_t* p, pointer_offset_t offset)
{
   if (p == NULL)
      return;
   p->offset = offset;
   DBGMSG("Pointer %p now has relative offset %#"PRIx64"\n", p, offset);
}

/*
 * Checks if a pointer has been linked to a target
 * \param p The pointer
 * \return TRUE if the pointer has a non NULL target object, FALSE otherwise
 * */
int pointer_has_target(pointer_t* p)
{
   if (!p)
      return FALSE;
   if (p->target.insn)
      return TRUE; //We simply have to test one of the element of the union to know whether it is empty or not
   else
      return FALSE;
}

/*
 * Sets the offset of a pointer
 * \param p The pointer
 * \param offset_intarget The value of the offset from the beginning of the object representing its target target
 * */
void pointer_set_offset_in_target(pointer_t* p,
      pointer_offset_intarget_t offset_intarget)
{
   if (!p)
      return;
   assert(offset_intarget <= UINT16_MAX); //Change this if we change the size over which off is coded
   p->offset_intarget = offset_intarget;
}

/*
 * Sets the type of a pointer
 * \param p The pointer
 * \param type Type of the pointer value
 */
void pointer_set_type(pointer_t* p, pointer_type_t type)
{
   if (p)
      p->type = type;
}

/*
 * Prints a pointer. This is to be used for printing a pointer alone, for instance when it is in a data or relocation object
 * \param p The pointer
 * \param str The string to which to print
 * \param size The size of the string to which to print
 * */
void pointer_print(pointer_t* p, char* str, size_t size)
{
   if (!p || !str)
      return;
   size_t char_limit = (size_t)str + size;
   switch (p->target_type) {
   case TARGET_INSN:
      lc_sprintf(str, char_limit - (size_t)str, "[Instruction @ %#"PRIx64, INSN_GET_ADDR(p->target.insn));
      while (*str != '\0') str++;
      ;
      break;
   case TARGET_DATA:
      lc_sprintf(str, char_limit - (size_t)str, "[Data entry @ %#"PRIx64, data_get_addr(p->target.data));
      while (*str != '\0') str++;
      break;
   case TARGET_BSCN:
      lc_sprintf(str, char_limit - (size_t) str, "[Section %s @ %#"PRIx64,
            binscn_get_name(p->target.bscn), binscn_get_addr(p->target.bscn));
      while (*str != '\0') str++;
      break;
   default:
      lc_sprintf(str, char_limit - (size_t)str, "[Unknown @ %#"PRIx64, pointer_get_addr(p));
      while(*str!='\0') str++;
      break;
   }
   if (p->offset_intarget > 0) {
      lc_sprintf(str, char_limit - (size_t)str, " + %#"PRIx32, p->offset_intarget);
      while (*str != '\0') str++;
   }
   lc_sprintf(str, char_limit - (size_t)str, "]");
   while(*str!='\0') str++;
}

/*
 * Returns the value of a pointer as a string of bytes, encoded on a given length
 * \param p The pointer
 * \param len Length in bytes over which the value must be encoded
 * \return String of size len representing either the address targeted by the pointer if it is of type POINTER_ABSOLUTE
 * or its displacement if it is of type POINTER_RELATIVE, and NULL if \c p is NULL or if its value can not be encoded
 * over the given length. This string is NOT allocated.
 * */
unsigned char* pointer_tobytes(pointer_t* p, uint64_t len)
{
   if (!p)
      return NULL;
   int64_t *val = NULL;
   unsigned char* out = NULL;
   switch (p->type) {
   case POINTER_RELATIVE:
      val = &(p->offset);
      break;
   case POINTER_ABSOLUTE:
      val = &(p->addr);
      break;
   }
   /**\todo TODO (2015-04-10) BEWARE OF THE SIGN! If we sometime move the address of pointers into an unsigned
    * and keep the offset as a signed (necessary), we will have to distinguish both cases, and cast the unsigned
    * value as unsigned while comparing on the UINT*_MAX value*/
   if (val) {
      switch (len) {
      case 1:
         if ((*val >= INT8_MIN) && (*val <= INT8_MAX))
            out = (unsigned char*) ((int8_t*) val);
         break;
      case 2:
         if ((*val >= INT16_MIN) && (*val <= INT16_MAX))
            out = (unsigned char*) ((int16_t*) val);
         break;
      case 4:
         if ((*val >= INT32_MIN) && (*val <= INT32_MIN))
            out = (unsigned char*) ((int32_t*) val);
         break;
      case 8:
         if ((*val >= INT64_MIN) && (*val <= INT64_MAX))
            out = (unsigned char*) ((int64_t*) val);
         break;
      }
   }
   return out;
}

///////////////////////////////////////////////////////////////////////////////
//                               memory                                    //
///////////////////////////////////////////////////////////////////////////////
/*
 * Creates a new empty structure holding a memory oprnd
 * \return An empty memory oprnd (correspond to a displacement of 0 with no base)
 */
memory_t* memory_new()
{
   memory_t* mem = (memory_t*) lc_malloc0(sizeof(memory_t));
   return mem;
}

/*
 * Frees a memory oprnd structure
 * \param mem The memory oprnd to free
 */
void memory_free(memory_t* mem)
{
   if (mem == NULL)
      return;
   lc_free(mem);
}

/**
 * Prints a memory operand in a format similar to objdump
 * \param m The memory operand to print
 * \param c A char array.
 * \param size Size of the char array
 * \param arch The architecture for which the operand is defined
 */
static void memory_printdump(void* m, char* c, size_t size, arch_t* arch)
{
   memory_t* mem = (memory_t*) m;
   int r;
   size_t char_limit = (size_t) c + size;

   if (m == NULL || c == NULL)
      return;

   if (mem->seg != NULL) {
      r = lc_sprintf(c, char_limit - (size_t) c, "%%%s:",
            arch_get_reg_name(arch, mem->seg->type, mem->seg->name));
      c += r;
   }
   if (mem->offset > 0) {
      r = lc_sprintf(c, char_limit - (size_t) c, "%#"PRIx64, mem->offset);
      c += r;
   } else if (mem->offset < 0) {
      r = lc_sprintf(c, char_limit - (size_t) c, "-%#"PRIx64, -mem->offset);
      c += r;
   }
   if ((mem->base != NULL) || (mem->index != NULL))
      *c++ = '(';
   if (mem->base != NULL) {
      r = lc_sprintf(c, char_limit - (size_t) c, "%%%s",
            arch_get_reg_name(arch, mem->base->type, mem->base->name));
      c += r;
   }
   if (mem->index != NULL) {
      r = lc_sprintf(c, char_limit - (size_t) c, ",%%%s,%d",
            arch_get_reg_name(arch, mem->index->type, mem->index->name),
            mem->scale);
      c += r;
   }
   if ((mem->base != NULL) || (mem->index != NULL))
      *c++ = ')';
   *c = '\0';
}

/**
 * Prints a memory operand in a format similar to objdump to a file stream
 * \param m The memory operand to print
 * \param f A file stream.
 * \param arch The architecture for which the operand is defined
 */
static void memory_fprint(void* m, FILE* f, arch_t* arch)
{
   memory_t* mem = (memory_t*) m;

   assert(m != NULL || f != NULL);

   if (mem->seg != NULL) {
      fprintf(f, "%%%s:",
            arch_get_reg_name(arch, mem->seg->type, mem->seg->name));
   }
   if (mem->offset > 0) {
      fprintf(f, "%#"PRIx64, mem->offset);
   } else if (mem->offset < 0) {
      fprintf(f, "-%#"PRIx64, -mem->offset);
   }
   if ((mem->base != NULL) || (mem->index != NULL)) {
      fprintf(f, "(");
      if (mem->base != NULL) {
         fprintf(f, "%%%s",
               arch_get_reg_name(arch, mem->base->type, mem->base->name));
      }
      if (mem->index != NULL) {
         fprintf(f, ",%%%s,%d",
               arch_get_reg_name(arch, mem->index->type, mem->index->name),
               mem->scale);
      }
      fprintf(f, ")");
   }
}

/*
 * Returns the name of the segment for a memory operand
 * \param mem Pointer to the memory operand structure
 * \return Name of the segment or PTR_ERROR if \e mem is NULL
 */
reg_t* memory_get_seg(memory_t* mem)
{
   return ((mem) ? mem->seg : PTR_ERROR);
}

/*
 * Returns the name of the base register for a memory operand
 * \param mem Pointer to the memory operand structure
 * \return Name of the base register or PTR_ERROR if \e mem is NULL
 */
reg_t* memory_get_base(memory_t* mem)
{
   return ((mem) ? mem->base : PTR_ERROR);
}

/*
 * Returns the name of the index register for a memory operand
 * \param mem Pointer to the memory operand structure
 * \return Name of the index register or PTR_ERROR if \e mem is NULL
 */
reg_t* memory_get_index(memory_t* mem)
{
   return ((mem) ? mem->index : PTR_ERROR);
}

/*
 * Returns the value of the offset for a memory operand
 * \param mem Pointer to the memory operand structure
 * \return Value of the offset or 0 if \e mem is NULL
 */
memory_offset_t memory_get_offset(memory_t* mem)
{
   return ((mem) ? mem->offset : 0);
}

/*
 * Returns the name of the scale for a memory operand
 * \param mem Pointer to the memory operand structure
 * \return Name of the scale or 0 if \e mem is NULL
 */
int memory_get_scale(memory_t* mem)
{
   return ((mem) ? mem->scale : 0);
}

/*
 * Sets the value of segment in a memory address
 * Returns if the memory is subject to a broadcast
 * \param mem Pointer to the memory operand structure
 * \return TRUE if the memory is broadcast, FALSE otherwise
 */
uint8_t memory_is_broadcast(memory_t* mem)
{
   return ((mem) ? mem->bcflag : FALSE);
}

/*
 * Returns the sign to apply on the index value
 * \param mem a memory address
 */
uint8_t memory_get_sign(memory_t* mem)
{
   return ((mem) ? mem->sign : 0);
}

/*
 * Returns the memory alignment
 * \param mem a memory address
 */
uint8_t memory_get_align(memory_t* mem)
{
   return ((mem) ? mem->align : 0);
}

/*
 * Sets the value of segment in a memory address
 * \param a memory address
 * \param seg name of a segment
 */
void memory_set_seg(memory_t* mem, reg_t* seg)
{
   if (mem != NULL)
      mem->seg = seg;
}

/*
 * Sets the sign to apply on the index value
 * \param mem a memory address
 * \param sign sign to apply (0: positive)
 */
void memory_set_sign(memory_t* mem, uint8_t sign)
{
   if (mem != NULL)
      mem->sign = sign;
}

/*
 * Sets the memory alignment
 * \param mem a memory address
 * \param align the memory alignment
 */
void memory_set_align(memory_t* mem, memalign_t align)
{
   if (mem != NULL)
      mem->align = align;
}

/*
 * Sets the value of base in a memory address
 * \param a memory address
 * \param base name of a base
 */
void memory_set_base(memory_t* mem, reg_t* base)
{
   if (mem != NULL)
      mem->base = base;
}

/*
 * Sets the value of index in a memory address
 * \param a memory address
 * \param index name of a nindex
 */
void memory_set_index(memory_t* mem, reg_t* index)
{
   if (mem != NULL) {
      mem->index = index;
      if (index == NULL)
         mem->scale = 0; //Resetting the scale to 0 if the index is NULL for coherence
   }
}

/*
 * Sets the value of offset in a memory address
 * \param a memory address
 * \param offset value of the offset
 */
void memory_set_offset(memory_t* mem, memory_offset_t offset)
{
   if (mem != NULL)
      mem->offset = offset;
}

/*
 * Sets the value of scale in a memory address
 * \param a memory address
 * \param scale value of the scale
 */
void memory_set_scale(memory_t* mem, int scale)
{
   if (mem != NULL)
      mem->scale = scale;
}

/*
 * Flags the memory as being broadcasted
 * \param a memory address
 */
void memory_set_bcflag(memory_t* mem)
{
   if (mem != NULL)
      mem->bcflag = TRUE;
}

///////////////////////////////////////////////////////////////////////////////
//                                memory pointer                             //
///////////////////////////////////////////////////////////////////////////////
/*
 * Creates a new empty structure holding a memory pointer
 * \param mem A pointer to the structure describing the memory
 * \param ptr A pointer to a structure describing the reference
 * \return An empty memory pointer (correspond to a displacement of 0)
 */
memrel_t* memrel_new(memory_t* mem, pointer_t* ptr)
{
   memrel_t* mpt = lc_malloc0(sizeof(*mpt));
   mpt->mem = mem;
   mpt->ptr = ptr;
   return mpt;
}

/*
 * Frees a memory pointer structure
 * \param mem The memory pointer structure to free
 */
void memrel_free(memrel_t* mpt)
{
   if (mpt == NULL)
      return;
   memory_free(mpt->mem);
   lc_free(mpt->ptr);
   lc_free(mpt);
}

/*
 * Updates the pointer of a relative memory
 * \param mpt The memory pointer structure
 * \param ptr The new pointer to set
 * */
void memrel_set_ptr(memrel_t* mpt, pointer_t* ptr)
{
   if (!mpt)
      return;
   if (mpt->ptr)
      lc_free(mpt->ptr);
   mpt->ptr = ptr;
}

///////////////////////////////////////////////////////////////////////////////
//                               oprnd                                   //
///////////////////////////////////////////////////////////////////////////////
/*
 * Creates a new oprnd of type register
 * \param reg The register
 * \return A oprnd structure of type REGISTER
 */
oprnd_t* oprnd_new_reg(reg_t* reg)
{
   if (reg == NULL)
      return NULL;

   oprnd_t* oprnd = (oprnd_t*) lc_malloc0(sizeof(oprnd_t));
   oprnd->type = OT_REGISTER;
   oprnd->role = OP_ROLE_UNDEF;
   oprnd->data.reg = reg;
   return oprnd;
}

/*
 * Creates a new oprnd of type memory
 * \param seg The name of the base segment register for the memory address (can be NULL)
 * \param base The name of the register containing the base memory address (can be NULL)
 * \param index The name of the index register for the memory address (can be NULL)
 * \param scale The scale associated to the index (if present)
 * \param offset The value of displacement from the base for the memory address
 * \return A oprnd of type memory with the specified elements
 */
oprnd_t* oprnd_new_mem(reg_t* seg, reg_t* base, reg_t* index, int scale,
      memory_offset_t offset)
{
   if (base && base->type == RIP_TYPE) //Special case: RIP-based instruction is actually a relative memory operand
      return oprnd_new_memrel(seg, base, index, scale, 0, offset,
            POINTER_RELATIVE);

   oprnd_t* oprnd = (oprnd_t*) lc_malloc0(sizeof(oprnd_t));
   memory_t* mem = memory_new();

   oprnd->type = OT_MEMORY;
   oprnd->role = OP_ROLE_UNDEF;
   if (seg != NULL)
      mem->seg = seg;
   if (base != NULL)
      mem->base = base;
   if (index != NULL)
      mem->index = index;
   if (scale >= 0)
      mem->scale = scale;
   mem->offset = offset;
   oprnd->data.mem = mem;
   return oprnd;
}

/*
 * Creates a memory operand from a memory address structure
 * \param mem A pointer to a structure describing memory address
 * \return A memory operand corresponding to the memory address
 * */
oprnd_t* oprnd_new_memory(memory_t* mem)
{
   if (mem && mem->base && (mem->base->type == RIP_TYPE))
      return oprnd_new_memory_pointer(mem,
            pointer_new(0, mem->offset, NULL, POINTER_RELATIVE, TARGET_DATA));

   oprnd_t* oprnd = (oprnd_t*) lc_malloc0(sizeof(oprnd_t));

   oprnd->type = OT_MEMORY;

   oprnd->data.mem = mem;

   /*Special case: resetting the scale to 0 if the index is to NULL (needed by the assembler for coherence)*/
   if (memory_get_index(mem) == NULL)
      memory_set_scale(mem, 0);

   return oprnd;
}

/*
 * Creates a new immediate oprnd
 * \param imm The value of the immediate oprnd
 * \return A oprnd of type immediate with the specified value
 */
oprnd_t* oprnd_new_imm(imm_t imm)
{
   oprnd_t* oprnd = (oprnd_t*) lc_malloc0(sizeof(oprnd_t));
   oprnd->type = OT_IMMEDIATE;
   oprnd->role = OP_ROLE_UNDEF;
   oprnd->data.imm = imm;
   return oprnd;
}

/*
 * Creates a new pointer operand
 * \param addr The address pointed by the pointer
 * \param disp The offset (relative displacement) of the pointer (for relative pointers)
 * \param type Type of the pointer operand
 * \return An operand of type pointer pointing to the specified address
 */
oprnd_t* oprnd_new_ptr(maddr_t addr, pointer_offset_t offset,
      pointer_type_t type)
{
   oprnd_t* oprnd = (oprnd_t*) lc_malloc0(sizeof(oprnd_t));
   oprnd->type = OT_POINTER;
   oprnd->role = OP_ROLE_UNDEF;
   oprnd->data.ptr = pointer_new(addr, offset, NULL, type, TARGET_UNDEF);
   return oprnd;
}

/*
 * Creates a new pointer operand from an existing pointer structure
 * \param ptr A pointer to a structure describing an operand
 * \return An operand of type pointer corresponding to the given pointer, or NULL if \c ptr is NULL
 * */
oprnd_t* oprnd_new_pointer(pointer_t* ptr)
{
   if (!ptr)
      return NULL;
   oprnd_t* oprnd = (oprnd_t*) lc_malloc0(sizeof(oprnd_t));
   oprnd->type = OT_POINTER;
   oprnd->role = OP_ROLE_UNDEF;
   oprnd->data.ptr = ptr;
   return oprnd;
}

/*
 * Creates a new memory relative operand
 * \param seg The name of the base segment register for the memory address (can be NULL)
 * \param base The name of the register containing the base memory address. It must be of type RIP_TYPE.
 * \param index The name of the index register for the memory address (can be NULL)
 * \param scale The scale associated to the index (if present)
 * \param addr Address pointed by the relative memory
 * \param offset Relative displacement to reach the address
 * \param type Type of the pointer operand
 * \return A oprnd of type relative memory pointing to the specified address. Its base will be set to NULL if \c base is not of type RIP_TYPE
 * */
oprnd_t* oprnd_new_memrel(reg_t* seg, reg_t* base, reg_t* index, int scale,
      maddr_t addr, memory_offset_t offset, pointer_type_t type)
{

   oprnd_t* oprnd = (oprnd_t*) lc_malloc0(sizeof(oprnd_t));
   oprnd->type = OT_MEMORY_RELATIVE;
   oprnd->role = OP_ROLE_UNDEF;

   memory_t* mem = memory_new();
   if (base && (base->type == RIP_TYPE)) /**\todo TODO (2014-12-04) This may not have to be enforced for all architectures!*/
      mem->base = base;
   mem->seg = seg;
   mem->index = index;
   mem->scale = scale;
   mem->offset = offset;

   pointer_t* ptr = pointer_new(addr, offset, NULL, type, TARGET_DATA);

   oprnd->data.mpt = memrel_new(mem, ptr);
   return oprnd;
}

/*
 * Creates a relative memory operand from a memory and a pointer structures
 * \param mem A pointer to the structure describing the memory
 * \param ptr A pointer to a structure describing the reference
 * \return A relative memory operand corresponding to the pointer
 * */
oprnd_t* oprnd_new_memory_pointer(memory_t* mem, pointer_t* ptr)
{
   oprnd_t* oprnd = (oprnd_t*) lc_malloc0(sizeof(oprnd_t));
   oprnd->type = OT_MEMORY_RELATIVE;
   oprnd->data.mpt = memrel_new(mem, ptr);
   return oprnd;
}

/*
 * Parses an memory operand (starting at the offset) from a string representation of an instruction, based on a given architecture
 * \param strinsn The string containing the representation of the instruction.
 * \param pos Pointer to the index into the string at which the parsing starts. Will be updated to contain the
 * index at which the memory operand ends (operand separator or end of string)
 * \param arch Pointer to the architecture structure for which the operand must be parsed
 * \return The parsed operand, or NULL if the parsing failed
 * */
oprnd_t* oprnd_parsenew_memory(char* strinsn, int *pos, arch_t* arch)
{
   int c = *pos, scale = 0;
   int64_t offset = 0;
   reg_t* base, *index;
   oprnd_t* out = NULL;

   /*Retrieving the offset*/
   parse_number__(strinsn, &c, &offset);

   if (strinsn[c] == '(') {
      /*There is a base*/
      c++;
      base = reg_parsenew(strinsn, &c, arch);
      if ((strinsn[c] == ')') && (base != NULL)) {
         /*There is only a base*/
         c++;
         out = oprnd_new_mem(NULL, base, NULL, 0, offset);
      } else if (strinsn[c] == ',') {
         /*There is also an index*/
         c++;
         index = reg_parsenew(strinsn, &c, arch);
         if ((index != NULL) && (strinsn[c] == ',')) {
            /*Index register valid and followed by a scale*/
            c++;
            switch (strinsn[c]) {
            case '1':
               scale = 1;
               break;
            case '2':
               scale = 2;
               break;
            case '4':
               scale = 4;
               break;
            case '8':
               scale = 8;
               break;
            }
            c++;
            if ((scale > 0) && (strinsn[c] == ')')) {
               /*The scale is valid and it is the end of the memory expression*/
               c++;
               out = oprnd_new_mem( NULL, base, index, scale, offset);
            } // Else the scale is not valid
         } // Invalid index register
      } // else the operand is not valid
   } else if ((strinsn[c] == ' ') || (strinsn[c] == '\0')
         || (strinsn[c] == ',')) {
      /*Nothing apart a displacement*/
      out = oprnd_new_mem(NULL, NULL, NULL, 0, offset);
   }
   /*Adding the offset if the memory operand is valid*/
   if (out != NULL) {
      //memory_set_offset(oprnd_get_memory(out), offset);
      *pos = c;
   }

   return out;
}

/*
 * Parses an operand from a string representation of an instruction, based on a given architecture
 * \param strinsn The string containing the representation of the instruction.
 * \param pos Pointer to the index into the string at which the parsing starts. Will be updated to contain the
 * index at which the operand ends (operand separator or end of string)
 * \param arch Pointer to the architecture structure for which the operand must be parsed
 * \return The parsed operand, or NULL if the parsing failed
 * */
oprnd_t* oprnd_parsenew(char* strinsn, int *pos, arch_t* arch)
{
   return (arch) ? arch->oprnd_parse(strinsn, pos) : NULL;
}

/*
 * Frees a oprnd
 * \param p A pointer to the oprnd to be freed
 */
void oprnd_free(void* p)
{
   oprnd_t* oprnd = (oprnd_t*) p;
   if (oprnd == NULL)
      return;

   switch (oprnd->type) {
   case (OT_REGISTER):
      break;
   case (OT_MEMORY):
      memory_free(oprnd->data.mem);
      break;
   case (OT_POINTER):
      pointer_free(oprnd->data.ptr);
      break;
   case (OT_MEMORY_RELATIVE):
      memory_free(oprnd->data.mpt->mem);
      pointer_free(oprnd->data.mpt->ptr);
      lc_free(oprnd->data.mpt);
      break;
   case (OT_REGISTER_INDEXED):
      regidx_free(oprnd->data.rix);
      break;
   }

   lc_free(oprnd);
}

/*
 * Get the pointer associated to an oprnd if its type is POINTER
 * \param oprnd an operand
 * \return a pointer structure or PTR_ERROR if oprnd is NULL or its type is not POINTER
 */
pointer_t* oprnd_get_ptr(oprnd_t* oprnd)
{
   if (oprnd == NULL || oprnd->type != OT_POINTER)
      return (NULL);
   else
      return (oprnd->data.ptr);
}

/*
 * Retrieves the address pointed to by an operand containing a pointer
 * \param oprnd The operand to analayze
 * \return The address pointed to by \c oprnd, or SIGNED_ERROR if it is NULL or does not contain a pointer
 */
maddr_t oprnd_get_refptr_addr(oprnd_t* oprnd)
{
   return pointer_get_addr(oprnd_get_refptr(oprnd));
}

/*
 * Returns the instruction pointed by an operand containing a pointer
 * \param oprnd The operand
 * \return The instruction pointed by \c oprnd, or NULL if it is NULL, does not contain a pointer , or not
 * referencing an instruction
 * */
insn_t* oprnd_get_refptr_insn_target(oprnd_t* oprnd)
{
   return pointer_get_insn_target(oprnd_get_refptr(oprnd));
}

/*
 * Returns the data referenced by an operand of type POINTER or MEMORY_RELATIVE
 * \param oprnd The operand
 * \return The data referenced by \c oprnd, or NULL if it is NULL, not of type POINTER or MEMORY_RELATIVE, or not
 * referencing a data
 * */
data_t* oprnd_get_ptr_data_target(oprnd_t* oprnd)
{
   return pointer_get_data_target(oprnd_get_ptr(oprnd));
}

/*
 * Get the pointer associated to an oprnd if its type is MEMORY_RELATIVE
 * \param oprnd an operand
 * \return a pointer structure or PTR_ERROR if oprnd is NULL or its type is not MEMORY_RELATIVE
 */
pointer_t* oprnd_get_memrel_pointer(oprnd_t* oprnd)
{
   return
         (oprnd && (oprnd->type == OT_MEMORY_RELATIVE)) ?
               (oprnd->data.mpt->ptr) : PTR_ERROR;
}

/*
 * Get the pointer associated to an oprnd if its type is POINTER or MEMORY_RELATIVE
 * \param oprnd an operand
 * \return a pointer structure or PTR_ERROR if oprnd is NULL or its type is neither POINTER nor MEMORY_RELATIVE
 */
pointer_t* oprnd_get_refptr(oprnd_t* oprnd)
{
   if (oprnd == NULL)
      return (NULL);
   switch (oprnd->type) {
   case OT_POINTER:
      return (oprnd->data.ptr);
   case OT_MEMORY_RELATIVE:
      return (oprnd->data.mpt->ptr);
   default:
      return NULL;
   }
}

/*
 * Get the memory structure associated to an oprnd if its type is MEMORY or MEMORY_RELATIVE
 * \param oprnd an operand
 * \return a memory structure or PTR_ERROR if oprnd is NULL or its type is not MEMORY
 */
memory_t* oprnd_get_memory(oprnd_t* oprnd)
{
   if (oprnd == NULL)
      return PTR_ERROR;
   if (oprnd->type == OT_MEMORY)
      return (oprnd->data.mem);
   else if (oprnd->type == OT_MEMORY_RELATIVE)
      return (oprnd->data.mpt->mem);
   else
      return NULL;
}

/*
 * Get the memory pointer structure associated to an oprnd if its type is MEMORY_RELATIVE
 * \param oprnd an operand
 * \return a memory structure or PTR_ERROR if oprnd is NULL or its type is not MEMORY_RELATIVE
 */
memrel_t* oprnd_get_memrel(oprnd_t* oprnd)
{
   return
         (oprnd && (oprnd->type == OT_MEMORY_RELATIVE)) ?
               oprnd->data.mpt : PTR_ERROR;
}

/*
 * Returns the register base name for a memory oprnd
 * \param oprnd A pointer to the memory oprnd
 * \return The name of the base register or PTR_ERROR if the oprnd is not a memory address or memory relative
 */
reg_t* oprnd_get_base(oprnd_t* oprnd)
{
   memory_t* mem = oprnd_get_memory(oprnd);
   return ((mem != NULL) ? mem->base : PTR_ERROR);
}

/*
 * Returns the value of an immediate oprnd
 * \param oprnd A pointer to the immediate oprnd
 * \return The oprnd value or 0 if the oprnd is not an immediate
 */
imm_t oprnd_get_imm(oprnd_t* oprnd)
{
   /**\todo Add an error code handling if the oprnd is not an immediate as it could legitimately be 0*/
   if (oprnd == NULL)
      return SIGNED_ERROR;
   return ((oprnd->type == OT_IMMEDIATE) ? oprnd->data.imm : 0);
}

/*
 * Returns the register index name for a memory oprnd
 * \param oprnd A pointer to the memory oprnd
 * \return The name of the index register or PTR_ERROR if the oprnd is not a memory address
 */
reg_t* oprnd_get_index(oprnd_t* oprnd)
{
   memory_t* mem = oprnd_get_memory(oprnd);
   return ((mem != NULL) ? mem->index : PTR_ERROR);
}

/*
 * Returns the offset value for a memory operand oprnd
 * \param oprnd A pointer to the oprnd
 * \return The value of the offset or SIGNED_ERROR if the oprnd is not a memory operand
 */
memory_offset_t oprnd_get_offset(oprnd_t* oprnd)
{
   memory_t* mem = oprnd_get_memory(oprnd);
   return ((mem != NULL) ? mem->offset : SIGNED_ERROR);
}

/*
 * Returns the scale value for a memory operand oprnd
 * \param oprnd A pointer to the oprnd
 * \return The value of the scale or SIGNED_ERROR if the oprnd is not a memory operand
 */
int oprnd_get_scale(oprnd_t* oprnd)
{
   memory_t* mem = oprnd_get_memory(oprnd);
   return ((mem != NULL) ? mem->scale : SIGNED_ERROR);
}

/*
 * Returns the register segment name for a memory oprnd
 * \param oprnd A pointer to the memory oprnd
 * \return The name of the segment register or PTR_ERROR if the oprnd is not a memory address
 */
reg_t* oprnd_get_seg(oprnd_t* oprnd)
{
   memory_t* mem = oprnd_get_memory(oprnd);
   return ((mem != NULL) ? mem->seg : PTR_ERROR);
}

/*
 * Returns the register name for a register oprnd
 * \param oprnd A pointer to the register oprnd
 * \return The name of the register or PTR_ERROR if the oprnd is not a register
 */
reg_t* oprnd_get_reg(oprnd_t* oprnd)
{
   if (oprnd == NULL)
      return PTR_ERROR;
   if (oprnd->type == OT_REGISTER)
      return (oprnd->data.reg);
   else if (oprnd->type == OT_REGISTER_INDEXED)
      return (oprnd->data.rix->reg);
   else
      return NULL;
}

/*
 * Returns the type of a oprnd
 * \param p The oprnd to analyze
 * \return The type of the oprnd
 */
oprnd_type_t oprnd_get_type(oprnd_t* oprnd)
{
   if (oprnd == NULL)
      return OT_UNKNOWN;
   return oprnd->type;
}

/**
 * Returns the extension of an operand
 * \param op A pointer to the operand
 * \return A pointer to the operand extension
 */
void* oprnd_get_ext(oprnd_t* oprnd)
{
   if (oprnd == NULL)
      return PTR_ERROR;
   return oprnd->ext;
}

/**
 * Retrieves the type of a register operand
 * \param oprnd An operand
 * \return The type of the register, or -1 if \e oprnd is not a register operand
 * */
char oprnd_get_reg_type(oprnd_t* oprnd)
{
   reg_t* reg = oprnd_get_reg(oprnd);
   return (reg != NULL) ? reg->type : -1;
}

/**
 * Remove the index from an memory operand and returns the resulting register
 * \param op The operand to update
 * \return The register that was used as index
 * */
reg_t* oprnd_rm_memory_index(oprnd_t* op)
{
   memory_t* mem = oprnd_get_memory(op);
   if (mem == NULL)
      return NULL;
   reg_t* out = mem->index;
   mem->index = NULL;
   /*Special case: resetting the scale to 0 if the index is to NULL (needed by the assembler for coherence)*/
   memory_set_scale(mem, 0);

   return out;
}

/*
 * Returns the size identifier of the operand (immediate and relative) or the size
 * of the data it contains
 * \param oprnd A pointer to the structure holding the operand
 * \return Identifier of the operand's size (as defined in the \ref data_sizes enum)
 * or DATASZ_UNDEF if p is NULL
 */
data_size_t oprnd_get_bitsize(oprnd_t* oprnd)
{
   if (oprnd == NULL)
      return DATASZ_UNDEF;
   return (oprnd->bitsize);
}

/*
 * Gets the size value for an operand size identifier
 * \param datasz The operand size identifier
 * \return The associated size in bits
 * */
int datasz_getvalue(int datasz)
{

   switch (datasz) {
   case DATASZ_1b:
      return (1);
   case DATASZ_2b:
      return (2);
   case DATASZ_3b:
      return (3);
   case DATASZ_4b:
      return (4);
   case DATASZ_5b:
      return (5);
   case DATASZ_6b:
      return (6);
   case DATASZ_7b:
      return (7);
   case DATASZ_8b:
      return (8);
   case DATASZ_9b:
      return (9);
   case DATASZ_10b:
      return (10);
   case DATASZ_11b:
      return (11);
   case DATASZ_12b:
      return (12);
   case DATASZ_16b:
      return (16);
   case DATASZ_20b:
      return (20);
   case DATASZ_21b:
      return (21);
   case DATASZ_23b:
      return (23);
   case DATASZ_24b:
      return (24);
   case DATASZ_25b:
      return (25);
   case DATASZ_26b:
      return (26);
   case DATASZ_32b:
      return (32);
   case DATASZ_64b:
      return (64);
   case DATASZ_80b:
      return (80);
   case DATASZ_128b:
      return (128);
   case DATASZ_256b:
      return (256);
   case DATASZ_512b:
      return (512);

   case DATASZ_112b:
      return (112);
   case DATASZ_224b:
      return (224);
   case DATASZ_672b:
      return (752);
   case DATASZ_864b:
      return (864);
   case DATASZ_4096b:
      return (4096);

   default:
      return (0);

   }
}

/*
 * Returns the name of an operand type
 * \param optype Operand type
 * \return Name of the type of operand
 * */
char* oprnd_type_get_name(oprnd_type_t optype)
{
   switch (optype) {
   case OT_REGISTER:
      return OT_NAME_REGISTER;
   case OT_REGISTER_INDEXED:
      return OT_NAME_REGISTER_INDEXED;
   case OT_MEMORY:
      return OT_NAME_MEMORY;
   case OT_MEMORY_RELATIVE:
      return OT_NAME_MEMORY_RELATIVE;
   case OT_IMMEDIATE:
      return OT_NAME_IMMEDIATE;
   case OT_IMMEDIATE_ADDRESS:
      return OT_NAME_IMMEDIATE_ADDRESS;
   case OT_POINTER:
      return OT_NAME_POINTER;
   case OT_UNKNOWN:
   default:
      return "Unknown operand type";
   }
}

/*
 * Gets the representation of the enum value for operand size (bitsize)
 * \param oprnd an operand
 * \return the operand size (in bit)
 */
int oprnd_get_size_value(oprnd_t* oprnd)
{
   if (oprnd == NULL)
      return (0);

   return datasz_getvalue(oprnd->bitsize);
}

/*
 * Checks if a oprnd is an immediate
 * \param p The oprnd to analyze
 * \return TRUE if a oprnd is of type immediate, FALSE otherwise
 */
int oprnd_is_imm(oprnd_t* p)
{
   return (
         (p && (p->type == OT_IMMEDIATE || p->type == OT_IMMEDIATE_ADDRESS)) ?
               TRUE : FALSE);
}

/*
 * Checks if a oprnd is a memory address
 * \param p The oprnd to analyze
 * \return TRUE if a oprnd is of type address, FALSE otherwise
 */
int oprnd_is_mem(oprnd_t* p)
{
   return (
         (p && (p->type == OT_MEMORY || p->type == OT_MEMORY_RELATIVE)) ?
               TRUE : FALSE);
}

/*
 * Checks if a oprnd is a memory relative address
 * \param p The oprnd to analyze
 * \return TRUE if a oprnd is of type relative memory address, FALSE otherwise
 */
int oprnd_is_memrel(oprnd_t* p)
{
   return ((p && (p->type == OT_MEMORY_RELATIVE)) ? TRUE : FALSE);
}

/*
 * Checks if a oprnd is a register
 * \param p The oprnd to analyze
 * \return TRUE if the oprnd is of type register, FALSE otherwise
 */
int oprnd_is_reg(oprnd_t* p)
{
   return (
         (p && (p->type == OT_REGISTER || p->type == OT_REGISTER_INDEXED)) ?
               TRUE : FALSE);
}

/*
 * Checks if a oprnd is a pointer
 * \param p The oprnd to analyze
 * \return TRUE if a oprnd is of type pointer, FALSE otherwise
 */
int oprnd_is_ptr(oprnd_t* p)
{
   return ((p && (p->type == OT_POINTER)) ? TRUE : FALSE);
}

/*
 * Checks if a oprnd references another address
 * \param p The oprnd to analyze
 * \return TRUE if a oprnd is of type pointer, memory relative or immediate address, FALSE otherwise
 */
int oprnd_is_refptr(oprnd_t* p)
{
   return (
         (p
               && (p->type == OT_POINTER || p->type == OT_MEMORY_RELATIVE
                     || p->type == OT_IMMEDIATE_ADDRESS)) ? TRUE : FALSE);
}

/*
 * Sets the offset value for a memory operand oprnd.
 * If the oprnd is not a memory operand, nothing is done
 * \param oprnd A pointer to the oprnd
 * \param offset The new value of the offset
 */
void oprnd_set_offset(oprnd_t* oprnd, memory_offset_t offset)
{
   if (!oprnd)
      return;
   if (oprnd->type == OT_MEMORY)
      oprnd->data.mem->offset = offset;
   else if (oprnd->type == OT_MEMORY_RELATIVE) {
      oprnd->data.mpt->mem->offset = offset;
      pointer_set_addr(oprnd->data.mpt->ptr, offset);
   }
}

/*
 * Sets the size identifier of the operand (immediate and relative) or the size
 * of the data it contains
 * \param oprnd A pointer to the structure holding the oprnd
 * \param s Identifier of the operand's size (as defined in the \ref data_sizes enum)
 */
void oprnd_set_bitsize(oprnd_t* oprnd, data_size_t s)
{
   if (oprnd != NULL)
      oprnd->bitsize = s;
}

/*
 * Sets the address pointed to by an operand containing a pointer
 * \param oprnd The operand to update
 * \param addr The new address to be pointed by the operand
 */
void oprnd_set_ptr_addr(oprnd_t* oprnd, maddr_t addr)
{
   pointer_set_addr(oprnd_get_refptr(oprnd), addr);
}

/*
 * Prints a oprnd in a format similar to objdump
 * \param in The instruction the operand belongs to. If not NULL, the destination of
 * relative operands will be printed, and only their value otherwise
 * \param p The operand to print
 * \param c A char array.
 * \param size Size of the char array
 * \param archi The architecture the operand belongs to
 * \return The number of written characters
 */
int oprnd_print(insn_t* in, void* p, char* c, size_t size, arch_t* archi)
{
   oprnd_t* oprnd = (oprnd_t*) p;
   size_t char_limit = (size_t) c + size;

   if (c == NULL)
      return 0;

   if (p == NULL) {
      c += lc_sprintf(c, char_limit - (size_t) c, "(NULL)");
      return size - (char_limit - (size_t) c);
   }
   switch (oprnd->type) {
   case (OT_REGISTER):
   case (OT_REGISTER_INDEXED): {
      reg_t* reg = oprnd_get_reg(oprnd);
      if (reg != NULL) {
         c += lc_sprintf(c, char_limit - (size_t) c, "%%%s",
               arch_get_reg_name(archi, reg->type, reg->name));
         if (oprnd->type == OT_REGISTER_INDEXED)
            c += lc_sprintf(c, char_limit - (size_t) c, "[%u]",
                  oprnd->data.rix->idx);
      } else
         c += lc_sprintf(c, char_limit - (size_t) c, "%%(null)");
   }
      break;
   case (OT_MEMORY):
      memory_printdump(oprnd->data.mem, c, size, archi);
      break;
   case (OT_MEMORY_RELATIVE):
      memory_printdump(oprnd->data.mpt->mem, c, size, archi);
      break;
   case (OT_IMMEDIATE):
      if (oprnd->data.imm >= 0)
         c += lc_sprintf(c, char_limit - (size_t) c, "$%#"PRIx64,
               oprnd->data.imm);
      else
         c += lc_sprintf(c, char_limit - (size_t) c, "$-%#"PRIx64,
               -oprnd->data.imm);
      break;
   case (OT_POINTER):
      c += oprnd_ptr_printdump(in, oprnd->data.ptr, c, size);
      break;
   default:
      if (oprnd->data.reg != NULL)
         c += lc_sprintf(c, char_limit - (size_t) c, "\"%s\"",
               arch_get_reg_name(archi, oprnd->data.reg->type,
                     oprnd->data.reg->name));
      else
         c += lc_sprintf(c, char_limit - (size_t) c, "\"(null)\"");
      break;
   }

   return size - (char_limit - (size_t) c);
}

/*
 * Prints a oprnd in a format similar to objdump to a file stream
 * \param in The instruction the operand belongs to. If not NULL, the destination of
 * relative operands will be printed, and only their value otherwise
 * \param p The operand to print
 * \param f A file stream.
 * \param archi The architecture the operand belongs to
 */
void oprnd_fprint(insn_t* in, void* p, FILE* f, arch_t* archi)
{
   oprnd_t* oprnd = (oprnd_t*) p;
   if (f == NULL)
      return;

   if (p == NULL) {
      fprintf(f, "(NULL)");
      return;
   }
   switch (oprnd->type) {
   case (OT_REGISTER):
   case (OT_REGISTER_INDEXED): {
      reg_t* reg = oprnd_get_reg(oprnd);
      if (reg != NULL) {
         fprintf(f, "%%%s", arch_get_reg_name(archi, reg->type, reg->name));
         if (oprnd->type == OT_REGISTER_INDEXED)
            fprintf(f, "[%u]", oprnd->data.rix->idx);
      } else
         fprintf(f, "%%(null)");
   }
      break;
   case (OT_MEMORY):
      memory_fprint(oprnd->data.mem, f, archi);
      break;
   case (OT_MEMORY_RELATIVE):
      memory_fprint(oprnd->data.mpt->mem, f, archi);
      break;
   case (OT_IMMEDIATE):
      if (oprnd->data.imm >= 0)
         fprintf(f, "$%#"PRIx64, oprnd->data.imm);
      else
         fprintf(f, "$-%#"PRIx64, -oprnd->data.imm);
      break;
   case (OT_POINTER):
      oprnd_ptr_fprint(in, oprnd, f);
      break;
   default:
      if (oprnd->data.reg != NULL)
         fprintf(f, "\"%s\"",
               arch_get_reg_name(archi, oprnd->data.reg->type,
                     oprnd->data.reg->name));
      else
         fprintf(f, "\"(null)\"");
      break;
   }
}

/*
 * Dumps the contents of a oprnd structure
 * \param p A pointer to the oprnd
 */
void oprnd_dump(void* p, arch_t* archi)
{
   oprnd_t* pa = (oprnd_t*) p;

   if (p == NULL)
      return;
   fprintf(stdout, "\n(");
   switch (pa->type) {
   case (OT_REGISTER):
      printf("type=REGISTER, reg=%s",
            arch_get_reg_name(archi, pa->data.reg->type, pa->data.reg->name));
      break;
   case (OT_REGISTER_INDEXED):
      printf("type=REGISTER_INDEXED, reg=%s, index=%u",
            arch_get_reg_name(archi, pa->data.rix->reg->type,
                  pa->data.rix->reg->name), pa->data.rix->idx);
      break;
   case (OT_MEMORY):
      printf(
            "type=MEMORY, base=%s, index=%s, offset=%#"PRIx64", scale=%d, seg=%s",
            arch_get_reg_name(archi, pa->data.mem->base->type,
                  pa->data.mem->base->name),
            (pa->data.mem->index) ?
                  arch_get_reg_name(archi, pa->data.mem->index->type,
                  pa->data.mem->index->name) : "NULL",
            pa->data.mem->offset,
            pa->data.mem->scale,
            (pa->data.mem->seg) ?
                  arch_get_reg_name(archi, pa->data.mem->seg->type,
                        pa->data.mem->seg->name) :
                  "NULL");
      break;
   case (OT_MEMORY_RELATIVE):
      printf(
            "type=MEMORY_RELATIVE, base=%s, index=%s, offset=%#"PRIx64", scale=%d, seg=%s",
            arch_get_reg_name(archi, pa->data.mem->base->type,
                  pa->data.mem->base->name),
            arch_get_reg_name(archi, pa->data.mem->index->type,
                  pa->data.mem->index->name), pa->data.mem->offset,
            pa->data.mem->scale,
            (pa->data.mem->seg) ?
                  arch_get_reg_name(archi, pa->data.mem->seg->type,
                        pa->data.mem->seg->name) :
                  "NULL");
      /**\todo Also print the pointer once we handle that it can target something else than instructions*/
      break;
   case (OT_IMMEDIATE):
      printf("type=IMMEDIATE, imm=%#"PRIx64, pa->data.imm);
      break;
   case (OT_POINTER):
      /**\todo Rewrite this part to take into account the fact that the destination depends on the architecture, and that a pointer
       * can reference something else than an instruction*/
      printf(
            "type=POINTER, address=%#"PRIx64", type=%d, next_insn=(%#"PRIx64",%s)",
            pa->data.ptr->addr, pa->data.ptr->type,
            INSN_GET_ADDR(pa->data.ptr->target.insn),
            insn_get_opcode(pa->data.ptr->target.insn));
      break;
   default:
      break;
   }
}

/**
 * Copies the attributes of a memory structure from one to another
 * (this does not include the elements of the structure initialised in oprnd_new_mem*)
 * @param src Source structure
 * @param dst Destination structure
 * */
static void memory_copy_attrs(memory_t* src, memory_t* dst)
{
   assert(src && dst);
   dst->align = src->align;
   dst->bcflag = src->bcflag;
   dst->sign = src->sign;
}

/*
 * Creates a new operand from a model. The copy does not contain any arch-specific elements
 * \param src The operand to copy from.
 * \return A new operand with the same characteristics as the source or NULL if the source is NULL
 * */
oprnd_t* oprnd_copy_generic(oprnd_t* src)
{
   if (src == NULL)
      return NULL;

   oprnd_t* out = NULL;
   switch (src->type) {
   case OT_REGISTER:
      out = oprnd_new_reg(oprnd_get_reg(src));
      break;
   case OT_REGISTER_INDEXED:
      out = oprnd_new_reg(oprnd_get_reg(src));
      oprnd_reg_set_index(out, oprnd_reg_get_index(src));
      break;
   case OT_MEMORY:
      out = oprnd_new_mem(oprnd_get_seg(src), oprnd_get_base(src),
            oprnd_get_index(src), oprnd_get_scale(src), oprnd_get_offset(src));
      memory_copy_attrs(oprnd_get_memory(src), oprnd_get_memory(out));
      break;
   case OT_MEMORY_RELATIVE:
      out = oprnd_new_memrel(src->data.mpt->mem->seg, src->data.mpt->mem->base,
            src->data.mpt->mem->index, src->data.mpt->mem->scale,
            src->data.mpt->ptr->addr, src->data.mpt->mem->offset,
            POINTER_UNKNOWN);
      memrel_set_ptr(out->data.mpt, pointer_copy(src->data.mpt->ptr));
      memory_copy_attrs(oprnd_get_memory(src), oprnd_get_memory(out));
      break;
   case OT_IMMEDIATE:
      out = oprnd_new_imm(oprnd_get_imm(src));
      break;
   case OT_POINTER:
      out = oprnd_new_pointer(pointer_copy(src->data.ptr));
      break;
   default:
      assert(0);  //We are copying a type that has not yet been implemented
   }
   if (out != NULL) {
      out->bitsize = src->bitsize;
      out->role = src->role;
   }

   return out;
}

/*
 * Sets the role of an operand
 * \param oprnd The operand to update
 * \param role The value of the role
 */
void oprnd_set_role(oprnd_t* op, char role)
{
   if (op)
      op->role = role;
}

/*
 * Flags the operand as a source
 * \param oprnd The operand to update
 */
void oprnd_set_role_src(oprnd_t* op)
{
   if (op)
      op->role |= OP_ROLE_SRC;
}

/*
 * Flags the operand as a destination
 * \param oprnd The operand to update
 */
void oprnd_set_role_dst(oprnd_t* op)
{
   if (op)
      op->role |= OP_ROLE_DST;
}

/*
 * Flags the memory operand to have a base register wrote back after access
 * \param oprnd The operand to update
 */
void oprnd_mem_set_wbflag(oprnd_t* op)
{
   if (oprnd_is_mem(op))
      op->writeback = TRUE;
}

/*
 * Flags the memory operand to be post indexed
 * \param oprnd The operand to update
 */
void oprnd_mem_set_piflag(oprnd_t* op)
{
   if (oprnd_is_mem(op))
      op->postindex = TRUE;
}

/*
 * Flags the register operand as an indexed register
 * \param oprnd The operand to update
 */
void oprnd_reg_set_irflag(oprnd_t* op)
{
   if (oprnd_get_type(op) == OT_REGISTER) {
      regidx_t* regidx = regidx_new(op->data.reg, 0);
      op->data.rix = regidx;
      op->type = OT_REGISTER_INDEXED;
   }
}

/*
 * Sets the index of an indexed register operand
 * \param oprnd The operand to update
 * \param index The index of the register operand
 */
void oprnd_reg_set_index(oprnd_t* op, char index)
{
   if (!op)
      return;
   if (op->type == OT_REGISTER)
      oprnd_reg_set_irflag(op);
   if (op->type == OT_REGISTER_INDEXED)
      op->data.rix->idx = index;
}

/*
 * Sets the value of an immediate oprnd
 * \param oprnd A pointer to the immediate oprnd
 * \param The value to set
 */
void oprnd_imm_set_value(oprnd_t* op, imm_t value)
{
   if (!op)
      return;
   if (op->type == OT_IMMEDIATE)
      op->data.imm = value;
   /**\todo (2016-02-26) Don't forget to handle the OT_IMMEDIATE_ADDRESS case when it's implemented
    * It will be something on the lines of:
    * if (op->type == OT_IMMEDIATE_ADDRESS)
    *    op->data.ipt->value = value;
    * */
}

/*
 * Checks if an operand register has an index
 * \param oprnd The operand to analyse
 * \return TRUE if the regiser has an index
 */
char oprnd_reg_is_indexed(oprnd_t* op)
{
   return (oprnd_get_type(op) == OT_REGISTER_INDEXED) ? TRUE : FALSE;
}

/*
 * Gets the index of an indexed register operand
 * \param oprnd The operand to update
 * \return The index of the register operand
 */
uint8_t oprnd_reg_get_index(oprnd_t* op)
{
   if (oprnd_get_type(op) == OT_REGISTER_INDEXED)
      return op->data.rix->idx;

   return 0;
}

/*
 * Gets the role of an operand
 * \param oprnd The operand to analyse
 * \return The operand's role
 */
char oprnd_get_role(oprnd_t* op)
{
   return (op != NULL) ? op->role : 0;
}

/*
 * Checks if the base register of a memory operand is write back
 * \param oprnd The operand to analyse
 * \return TRUE if the base register is write back, FALSE otherwise
 */
char oprnd_mem_base_reg_is_dst(oprnd_t* op)
{
   return (oprnd_is_mem(op)) ? op->writeback : FALSE;
}

/*
 * Checks if a memory operand is post indexed
 * \param oprnd The operand to analyse
 * \return TRUE if the memory is post indexed, FALSE otherwise
 */
char oprnd_mem_is_postindexed(oprnd_t* op)
{
   return (oprnd_is_mem(op)) ? op->postindex : FALSE;
}

/*
 * Checks if an operand is a source
 * \param oprnd The operand to analyse
 * \return TRUE if the operand is a source, FALSE otherwise
 */
char oprnd_is_src(oprnd_t* op)
{
   return ((op != NULL) && ((op->role & OP_ROLE_SRC) != 0)) ? TRUE : FALSE;
}

/*
 * Checks if an operand is a destination
 * \param oprnd The operand to analyse
 * \return TRUE if the operand is a destination, FALSE otherwise
 */
char oprnd_is_dst(oprnd_t* op)
{
   return ((op != NULL) && ((op->role & OP_ROLE_DST) != 0)) ? TRUE : FALSE;
}

/*
 * Checks if two operands are equal
 * \param op1 First operand
 * \param op2 Second operand
 * \return TRUE if both operands are equal (same type, same register names and types, same offset for pointer operands), FALSE otherwise
 * */
boolean_t oprnd_equal(oprnd_t* op1, oprnd_t* op2)
{
   if ((!op1) || (!op2))
      return (op1 == op2); //If both are NULL, they can be considered equal
   if (op1->type != op2->type)
      return FALSE;
   switch (op1->type) {
   memory_t* mem1;
   memory_t* mem2;
   reg_t* reg1, *reg2;
case OT_REGISTER:
case OT_REGISTER_INDEXED:
   reg1 = oprnd_get_reg(op1);
   reg2 = oprnd_get_reg(op2);
   if ((reg1->name != reg2->name) || (reg1->type != reg2->type))
      return FALSE;
   if (oprnd_reg_get_index(op1) != oprnd_reg_get_index(op2))
      return FALSE;
   break;
case OT_MEMORY:
case OT_MEMORY_RELATIVE:
   mem1 = oprnd_get_memory(op1);
   mem2 = oprnd_get_memory(op2);
   //Checks if one segment register is NULL but not the other
   if (((!mem1->seg) || (!mem2->seg)) && (mem1->seg != mem2->seg))
      return FALSE;
   //Checks if both segment registers are identical
   if (((mem1->seg != NULL) && (mem2->seg != NULL))
         && ((mem1->seg->name != mem2->seg->name)
               || (mem1->seg->type != mem2->seg->type)))
      return FALSE;
   //Checks if both displacement are identical
   if (mem1->offset != mem2->offset)
      return FALSE;
   //Checks if one base register is NULL but not the other
   if (((!mem1->base) || (!mem2->base)) && (mem1->base != mem2->base))
      return FALSE;
   //Checks if both base registers are identical
   if (((mem1->base != NULL) && (mem2->base != NULL))
         && ((mem1->base->name != mem2->base->name)
               || (mem1->base->type != mem2->base->type)))
      return FALSE;
   //Checks if one index register is NULL but not the other
   if (((!mem1->index) || (!mem2->index)) && (mem1->index != mem2->index))
      return FALSE;
   //Checks if both index registers are identical
   if (((mem1->index != NULL) && (mem2->index != NULL))
         && ((mem1->index->name != mem2->index->name)
               || (mem1->index->type != mem2->index->type)))
      return FALSE;
   //Checks if both scales are identical
   if (mem1->scale != mem2->scale)
      return FALSE;
   break;
case OT_IMMEDIATE:
   if (op1->data.imm != op2->data.imm)
      return FALSE;
   break;
case OT_POINTER:
   if (op1->data.ptr->type != op2->data.ptr->type)
      return FALSE;
   if (op1->data.ptr->target_type != POINTER_UNKNOWN
         && op2->data.ptr->target_type != POINTER_UNKNOWN
         && op1->data.ptr->target_type != op2->data.ptr->target_type)
      return FALSE;
   /**\todo (2015-11-25) Adding the test on target types being different than POINTER_UNKNOWN specifically to handle the
    * case of asmfile_disassemble_existing, where we compare a newly disassembled instruction with an instruction parsed
    * from a file. The target type of the newly disassembled instruction can't be known at that point.
    * In the future, we could try changing the algorithm in asmfile_disassemble_existing to find the target, or leave
    * it as it is. See also below the question about when pointers should be identical*/
   /**
    * \todo Should pointers be equal because they have the same offset value or point to a different instruction/data ?
    * */
   /**\todo TODO (2014-12-04) Add some test on the origin as well*/
   if (((op1->data.ptr->type == POINTER_ABSOLUTE)
         && (op1->data.ptr->addr != op2->data.ptr->addr))
         || ((op1->data.ptr->type == POINTER_RELATIVE)
               && (op1->data.ptr->offset != op2->data.ptr->offset)))
      return FALSE;
   /**\todo (2015-11-25) See comment above for the exclusion of POINTER_UNKNOWN*/
   if ((op1->data.ptr->target_type != POINTER_UNKNOWN
         && op2->data.ptr->target_type != POINTER_UNKNOWN)
         && (((op1->data.ptr->target_type == TARGET_INSN)
               && (op1->data.ptr->target.insn != op2->data.ptr->target.insn))
               || ((op1->data.ptr->target_type == TARGET_DATA)
                     && (op1->data.ptr->target.data
                           != op2->data.ptr->target.data))))
      return FALSE;
   break;
default:
   return FALSE; //Something is wrong anyway
   }
   return TRUE;
}

/*
 * Changes the reg field in an operand (this operand must already be a reg operand)
 * \param op an operand
 * \param reg the new value of the register operand
 */
void oprnd_change_reg(oprnd_t* op, reg_t* reg)
{
   if (reg == NULL || oprnd_get_type(op) != OT_REGISTER)
      return;
   DBGMSG(
         "Changing register {name=%d, type=%d} (%p) to {name=%d, type=%d} (%p)\n",
         reg_get_name(op->data.reg), reg_get_type(op->data.reg), op->data.reg,
         reg_get_name(reg), reg_get_type(reg), reg);
   op->data.reg = reg;
}

/*
 * Changes the register used as base in a memory operand
 * \param op An operand
 * \param reg The new register used as base
 */
void oprnd_changebase(oprnd_t* op, reg_t* reg)
{
   if (reg == NULL || !oprnd_is_mem(op))
      return;
   if (op->type == OT_MEMORY) {
      if (reg->type != RIP_TYPE)
         op->data.mem->base = reg; //Standard case, replacing an non IP register with another one
      else {
         //Turning the memory operand to a memory relative operand
         memory_t* mem = op->data.mem;
         pointer_t* ptr = pointer_new(0, op->data.mem->offset, NULL,
               POINTER_RELATIVE, TARGET_DATA);
         memrel_t* mpt = memrel_new(mem, ptr);
         mem->base = reg;
         op->data.mpt = mpt;
         op->type = OT_MEMORY_RELATIVE;
      }
   } else if (op->type == OT_MEMORY_RELATIVE) {
      if (reg->type == RIP_TYPE)
         op->data.mpt->mem->base = reg; //This covers the remote case where an arch would have two different IP registers
      else {
         //Turning the memory relative operand into a memory operand
         memrel_t* mpt = op->data.mpt;
         memory_t* mem = mpt->mem;
         mem->base = reg;
         lc_free(mpt->ptr); //We can't use memrel_free here as we still need the memory_t structure
         lc_free(mpt);
         op->data.mem = mem;
         op->type = OT_MEMORY;
      }
   }
}

/*
 * Changes the register used as index in a memory operand
 * \param op An operand
 * \param reg The new register used as index
 */
void oprnd_changeindex(oprnd_t* op, reg_t* reg)
{
   if (op == NULL || reg == NULL)
      return;
   memory_t* mem = oprnd_get_memory(op);
   if (mem) {
      mem->index = reg;
      if (reg == NULL)
         mem->scale = 0; //Resetting the scale to 0 if the index is NULL for coherence
   }
}

/**
 * Sets the extension of an operand
 * \param op The operand to update
 * \param role The extension to link with the operand
 */
void oprnd_set_ext(oprnd_t* op, void* ext)
{
   if (op)
      op->ext = ext;
}
