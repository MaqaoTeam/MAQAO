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
#include <inttypes.h>
#include "libmasm.h"
#include "arm64_arch.h"
#include "arm64_ext.h"
#include "arch.h"
#include "fsmutils.h"

/*! \file arm64_ext.c
 *  \brief This file handles operands' extensions and architecture specific functions.
 *
 *  It should contains the functions processing the operands' extensions.
 *  These extensions are architecture specific. They correspond to a specificity or a feature that cannot be
 *  handled by the common structures (see the libmasm.h file for structures describing an instruction and its operands).
 *
 *  It should also contains all the mandatory architecture specific functions:
 *    -  void insn_free(void*);
 *    -  insn_t* insn_parse(char*, arch_t*);
 *    -  void insn_print(insn_t*, char*, arch_t*);
 *    -  void insn_fprint(insn_t*, FILE*, arch_t*);
 *    -  int64_t oprnd_get_ptraddr(insn_t*, oprnd_t*);
 *    -  void oprnd_setptraddr(insn_t*, oprnd_t*, int64_t);
 *
 *  These functions are called through the arch_t variable which stores a pointer on them.
 *  They can call the common version of the function if there is no specificity to take care of.
 *
 *  In the case of the Arm architecture there are also functions that wrap the handling of the instruction suffixes.
 *  Even if the suffix is stored in a non architecture specific structure, the value stored in it is.\n
 *  In fact, this variable contains only the condition suffix and two flags for input and output vector suffixes.
 *  The last two suffixes information are stored elsewhere, we only need a flag specifying their existence.\n
 *  See \ref instruction_suffixes for additional information about Arm mnemonics extensions.
 */

/** \var ConditionSuffixes
 * Assembly suffixes for conditional instructions.\n
 * \verbatim
 Int:  Integer
 Fp :  Floating-point

 "EQ",   0b0000  Int:  Equal                            Fp:   Equal                               Flags:   Z == 1
 "NE",   0b0001  Int:  Not Equal                        Fp:   Not Equal or unordered              Flags:   Z == 0
 "CS",   0b0010  Int:  Carry Set                        Fp:   Greater than, equal or unordered    Flags:   C == 1
 "CC",   0b0011  Int:  Carry Clear                      Fp:   Less than                           Flags:   C == 0
 "MI",   0b0100  Int:  Minus, negative                  Fp:   Less than                           Flags:   N == 1
 "PL",   0b0101  Int:  Plus, positive or zero           Fp:   Greater than, equal or unordered    Flags:   N == 0
 "VS",   0b0110  Int:  Overflow                         Fp:   Unordered                           Flags:   V == 1
 "VC",   0b0111  Int:  No overflow                      Fp:   Ordered                             Flags:   V == 0
 "HI",   0b1000  Int:  Unsigned Higher                  Fp:   Greater than or unordered           Flags:   C == 1 and Z == 0
 "LS",   0b1001  Int:  Unsigned Lower or Same           Fp:   Less than or equal                  Flags:   C == 0 or Z == 1
 "GE",   0b1010  Int:  Signed Greater than or Equal     Fp:   Greater than or Equal               Flags:   N == V
 "LT",   0b1011  Int:  Signed Less Than                 Fp:   Less Than or unordered              Flags:   N != V
 "GT",   0b1100  Int:  Signed Greater Than              Fp:   Greater Than                        Flags:   Z == 0 and N == V
 "LE",   0b1101  Int:  Signed Less than or Equal        Fp:   Less than, Equal or unordered       Flags:   Z == 1 or N != V
 "",     0b1110  Int:  ALWAYS                           Fp:   ALWAYS                              Flags:   Any
 "",     0b1111  RESERVED                               RESERVED                                  \endverbatim
 */
static char* ConditionSuffixes[] =
{
   "EQ",
   "NE",
   "CS",
   "CC",
   "MI",
   "PL",
   "VS",
   "VC",
   "HI",
   "LS",
   "GE",
   "LT",
   "GT",
   "LE",
   "AL",
   "NV",
};

static char* NumberOfElements[] =
{
   "",    // None
   "",    // B
   "8",   // 8B
   "16",  // 16B
   "",    // H
   "4",   // 4H
   "8",   // 8H
   "",    // S
   "2",   // 2S
   "4",   // 4S
   "",    // D
   "1",   // 1D
   "2",   // 2D
};

static char* ElementSize[] =
{
   "",    // None
   "B",    // B
   "B",   // 8B
   "B",  // 16B
   "H",    // H
   "H",   // 4H
   "H",   // 8H
   "S",    // S
   "S",   // 2S
   "S",   // 4S
   "D",    // D
   "D",   // 1D
   "D",   // 2D
};

///////////////////////////////////////////////////////////////////////////////
//                                  Extend                                   //
///////////////////////////////////////////////////////////////////////////////

static char* extends[] =
{
  "UXTB",
  "UXTH",
  "UXTW",
  "UXTX",
  "SXTB",
  "SXTH",
  "SXTW",
  "SXTX"
};

/**
 * Create a new extend and sets its type
 * \param type Type of the extend
 * \param value The extend value
 * \return A pointer to the new extend
 */
extend_t* arm64_extend_new (uint8_t type, uint8_t value)
{

   extend_t* extend  = (extend_t*)lc_malloc0(sizeof(extend_t));
   extend->type      = type;
   extend->value     = value;

   DBGMSG("NEW EXTEND (%p): %s(%d), %d\n", extend, type <= SXTX ? extends[type] : NULL, type, value);

   return extend;
}

/**
 * Gets the extend type
 * \param extend A pointer to the extend
 * \return A uint8_t containing the extend type
 */
uint8_t arm64_extend_get_type (extend_t* extend)
{
   if (extend == NULL)
     return 0;

   return(extend->type);
}

/**
 * Gets the extend value
 * \param extend A pointer to the extend
 * \return A uint8_t containing the extend value
 */
uint8_t arm64_extend_get_value (extend_t* extend)
{
   if (extend == NULL)
     return 0;

   return(extend->value);
}

/**
 * Sets the extend type
 * \param extend A pointer to the extend
 * \param type The type to set to the extend
 */
void arm64_extend_set_type (extend_t* extend, uint8_t type)
{
   if (extend == NULL)
     return;

   extend->type = type;
}

/**
 * Sets the extend value
 * \param extend A pointer to the extend
 * \param value The value to set to the extend
 */
void arm64_extend_set_value (extend_t* extend, uint8_t value)
{
   if (extend == NULL)
     return;

   extend->value = value;
}

/**
 * Frees an extend
 * \param extend A pointer to the extend
 */
void arm64_extend_free (extend_t* extend)
{
   if (extend == NULL)
     return;

   lc_free(extend);
   extend = NULL;
}

/**
 * Prints a extend
 * \param extend A pointer to the extend
 * \param c A char array
 */
static void arm64_extend_print (extend_t* extend, char* c)
{
   if (extend == NULL || c == NULL)
     return;

   c += sprintf (c, ",%s", extends[extend->type]);
   if (extend->value > 0)
      c += sprintf (c, " #%d", extend->value);

   return;
}

/**
 * Prints a extend to a file stream
 * \param extend A pointer to the extend
 * \param f A file stream
 */
static void arm64_extend_fprint (extend_t* extend, FILE* f)
{

   if (extend == NULL || f == NULL)
      return;

   fprintf (f, ",%s", extends[extend->type]);
   if (extend->value > 0)
      fprintf (f, " #%d", extend->value);

   return;
}

///////////////////////////////////////////////////////////////////////////////
//                                  Shift                                    //
///////////////////////////////////////////////////////////////////////////////

static char* shifts[] =
{
  "LSL",
  "LSR",
  "ASR",
  "ROR"
};

/**
 * Create a new shift extension and sets its type and immediate value
 * \param type Type of the shift
 * \param value The shift value
 * \return A pointer to the new shift
 */
shift_t* arm64_shift_new (uint8_t type, uint8_t value)
{

   shift_t* shift = (shift_t*)lc_malloc0(sizeof(shift_t));

   shift->type    = type;
   shift->value   = value;

   DBGMSG("NEW SHIFT (%p): %s(%d), %d\n", shift, type <= ROR ? shifts[type] : NULL, type, value);

   return shift;
}

/**
 * Gets the shift type
 * \param shift A pointer to the shift
 * \return A uint8_t containing the shift type
 */
uint8_t arm64_shift_get_type (shift_t* shift)
{
   if (shift == NULL)
     return 0;

   return(shift->type);
}

/**
 * Gets the shift value
 * \param shift A pointer to the shift
 * \return A uint8_t containing the shift value
 */
uint8_t arm64_shift_get_value (shift_t* shift)
{
   if (shift == NULL)
     return 0;

   return (shift->value);
}

/**
 * Sets the shift type
 * \param shift A pointer to the shift
 * \param type The type to set to the shift
 */
void arm64_shift_set_type (shift_t* shift, uint8_t type)
{
   if (shift == NULL)
     return;

   shift->type = type;
}

/**
 * Sets the shift value
 * \param shift A pointer to the shift
 * \param value The value to set to the shift
 */
void arm64_shift_set_value (shift_t* shift, uint8_t value)
{
   if (shift == NULL)
     return;

   shift->value   = value;
}

/**
 * Frees a shift
 * \param shift A pointer to the shift
 */
void arm64_shift_free (shift_t* shift)
{
   if (shift == NULL)
     return;

   lc_free(shift);
   shift = NULL;
}

/**
 * Prints a shift
 * \param shift A pointer to the shift
 * \param c A char array
 */
static void arm64_shift_print (shift_t* shift, char* c)
{
   if (shift == NULL || c == NULL)
     return;

   switch (shift->type) {
      case LSR:
         c += sprintf (c, ",LSR #%d", shift->value);
         break;
      case ASR:
         c += sprintf (c, ",ASR #%d", shift->value);
         break;
      case ROR:
         c += sprintf (c, ",ROR #%d", shift->value);
         break;
      case LSL:
      default :
         if (shift->value > 0)
            c += sprintf (c, ",LSL #%d", shift->value);
   }

   return;
}

/**
 * Prints a shift to a file stream
 * \param shift A pointer to the shift
 * \param f A file stream
 */
static void arm64_shift_fprint (shift_t* shift, FILE* f)
{

   if (shift == NULL || f == NULL)
      return;

   switch (shift->type) {
      case LSR:
         fprintf (f, ",LSR #%d", shift->value);
         break;
      case ASR:
         fprintf (f, ",ASR #%d", shift->value);
         break;
      case ROR:
         fprintf (f, ",ROR #%d", shift->value);
         break;
      case LSL:
      default :
         if (shift->value > 0)
            fprintf (f, ",LSL #%d", shift->value);
   }

   return;
}


///////////////////////////////////////////////////////////////////////////////
//                                Extension                                  //
///////////////////////////////////////////////////////////////////////////////

/**
 * Create a new operand extension
 * \return A pointer to the new operand extension
 */
arm64_oprnd_ext_t* arm64_oprnd_ext_new()
{
   arm64_oprnd_ext_t* arm64_oprnd_ext = (arm64_oprnd_ext_t *)lc_malloc0(sizeof(arm64_oprnd_ext_t));

   return arm64_oprnd_ext;
}

/**
 * Create a new operand extension and sets a extend
 * \param extend A pointer to an extend
 * \return A pointer to the new operand extension
 */
arm64_oprnd_ext_t* arm64_oprnd_ext_new_extend (extend_t* extend)
{
   if (extend == NULL)
      return NULL;

   arm64_oprnd_ext_t* arm64_oprnd_ext = (arm64_oprnd_ext_t *)lc_malloc0(sizeof(arm64_oprnd_ext_t));
   arm64_oprnd_ext->ext_type     = EXTEND;
   arm64_oprnd_ext->ext.extend   = extend;

   return arm64_oprnd_ext;
}

/**
 * Create a new operand extension and sets a shift
 * \param shift A pointer to the shift
 * \return A pointer to the new operand extension
 */
arm64_oprnd_ext_t* arm64_oprnd_ext_new_shift(shift_t* shift)
{
   if (shift == NULL)
      return NULL;

   arm64_oprnd_ext_t* arm64_oprnd_ext = (arm64_oprnd_ext_t *)lc_malloc0(sizeof(arm64_oprnd_ext_t));
   arm64_oprnd_ext->ext_type  = SHIFT;
   arm64_oprnd_ext->ext.shift = shift;

   return arm64_oprnd_ext;

}

/**
 * Create a new operand extension and sets an arrangement
 * \param arrangement The arrangement value
 * \return A pointer to the new operand extension
 */
arm64_oprnd_ext_t* arm64_oprnd_ext_new_arrangement(arrangement_t arrangement)
{

   arm64_oprnd_ext_t* arm64_oprnd_ext = (arm64_oprnd_ext_t *)lc_malloc0(sizeof(arm64_oprnd_ext_t));
   arm64_oprnd_ext->ext_type    = EMPTY;
   arm64_oprnd_ext->arrangement = arrangement;

   return arm64_oprnd_ext;
}

/**
 * Gets the extend of an extension
 * \param ext A pointer to the extension
 * \return A pointer to the extend extension
 */
extend_t* arm64_oprnd_ext_get_extend(arm64_oprnd_ext_t* ext)
{
   if (ext == NULL || ext->ext_type != EXTEND)
      return NULL;

   return (ext->ext.extend);
}

/**
 * Gets the shift of an extension
 * \param ext A pointer to the extension
 * \return A pointer to the shift
 */
shift_t* arm64_oprnd_ext_get_shift(arm64_oprnd_ext_t* ext)
{
   if (ext == NULL || ext->ext_type != SHIFT)
      return NULL;

   return (ext->ext.shift);

}

/**
 * Gets the arrangement of an extension
 * \param ext A pointer to the extension
 * \return The operand arrangement
 */
arrangement_t arm64_oprnd_ext_get_arrangement(arm64_oprnd_ext_t* ext)
{
   if (ext == NULL)
      return UNSIGNED_ERROR;

   return (ext->arrangement);
}

/**
 * Sets the extend of an extension
 * \param ext A pointer to the extension
 * \param extend A pointer to the extend
 */
void arm64_oprnd_ext_set_extend(arm64_oprnd_ext_t* ext, extend_t* extend)
{
   if (ext == NULL || extend == NULL)
      return;

   if (ext->ext_type == SHIFT)
      arm64_shift_free(ext->ext.shift);
   else if (ext->ext_type == EXTEND)
      arm64_extend_free(ext->ext.extend);

   ext->ext_type     = EXTEND;
   ext->ext.extend   = extend;
}

/**
 * Sets the shift of an extension
 * \param ext A pointer to the extension
 * \param shift A pointer to the shift
 */
void arm64_oprnd_ext_set_shift(arm64_oprnd_ext_t* ext, shift_t* shift)
{
   if (ext == NULL || shift == NULL)
      return;

   if (ext->ext_type == SHIFT)
      arm64_shift_free(ext->ext.shift);
   else if (ext->ext_type == EXTEND)
      arm64_extend_free(ext->ext.extend);

   ext->ext_type  = SHIFT;
   ext->ext.shift = shift;
}

/**
 * Sets the arrangement of an extension
 * \param ext A pointer to the extension
 * \param arrangement The arrangement value to set
 */
void arm64_oprnd_ext_set_arrangement(arm64_oprnd_ext_t* ext, arrangement_t arrangement)
{
   if (ext == NULL)
      return;

   DBGMSG("GLOU: ext: %p\n", ext);

   ext->arrangement = arrangement;
}

/**
 * Frees the arm operand extension in an operand structure
 * \param oprnd The operand in which to free the extension
 */
void arm64_oprnd_ext_free (oprnd_t* oprnd)
{
   if (oprnd == NULL || oprnd->ext == NULL)
      return;

   if (((arm64_oprnd_ext_t*)oprnd->ext)->ext_type == SHIFT)
      arm64_shift_free(((arm64_oprnd_ext_t*)oprnd->ext)->ext.shift);
   else if (((arm64_oprnd_ext_t*)oprnd->ext)->ext_type == EXTEND)
      arm64_extend_free(((arm64_oprnd_ext_t*)oprnd->ext)->ext.extend);

   lc_free(oprnd->ext);

   oprnd->ext = NULL;
}

/**
 * Prints the operand extension
 * \param ioprnd The operand to print the extension
 * \param c A char array
 * \param archi The architecture the operand belongs to
 */
static void arm64_oprnd_ext_print (oprnd_t* oprnd, char* c, arch_t* arch)
{
   if (oprnd == NULL || oprnd->ext == NULL || c == NULL || arch == NULL)
      return;

   if (((arm64_oprnd_ext_t*)oprnd->ext)->arrangement != NONE)
      c += sprintf(c, ".%s%s", NumberOfElements[((arm64_oprnd_ext_t*)oprnd->ext)->arrangement],
        ElementSize[((arm64_oprnd_ext_t*)oprnd->ext)->arrangement]);

   if (((arm64_oprnd_ext_t*)oprnd->ext)->ext_type == SHIFT)
      arm64_shift_print(((arm64_oprnd_ext_t*)oprnd->ext)->ext.shift, c);
   else if (((arm64_oprnd_ext_t*)oprnd->ext)->ext_type == EXTEND)
      arm64_extend_print(((arm64_oprnd_ext_t*)oprnd->ext)->ext.extend, c);
}

/**
 * Prints the operand extension to a file stream
 * \param ioprnd The operand to print the extension
 * \param f A file stream
 * \param archi The architecture the operand belongs to
 */
static void arm64_oprnd_ext_fprint (oprnd_t* oprnd, FILE* f, arch_t* arch)
{
   if (oprnd == NULL || oprnd->ext == NULL || f == NULL || arch == NULL)
      return;

   if (((arm64_oprnd_ext_t*)oprnd->ext)->arrangement != NONE)
      fprintf(f, ".%s%s", NumberOfElements[((arm64_oprnd_ext_t*)oprnd->ext)->arrangement],
        ElementSize[((arm64_oprnd_ext_t*)oprnd->ext)->arrangement]);

   if (((arm64_oprnd_ext_t*)oprnd->ext)->ext_type == SHIFT)
      arm64_shift_fprint(((arm64_oprnd_ext_t*)oprnd->ext)->ext.shift, f);
   else if (((arm64_oprnd_ext_t*)oprnd->ext)->ext_type == EXTEND) 
      arm64_extend_fprint(((arm64_oprnd_ext_t*)oprnd->ext)->ext.extend, f);
}

/**
 * Parses an Arm extension from a string representation
 * \param strinsn The string containing the representation of the instruction.
 * \param pos Pointer to the index into the string at which the parsing starts. Will be updated to contain the
 * index at which the extension ends (operand separator or end of string)
 * \param arch Pointer to the architecture structure for which the operand must be parsed
 * \return The parsed extension, or NULL if the parsing failed
 */
static arm64_oprnd_ext_t * arm64_oprnd_ext_parsenew (char* strinsn, int *pos, arch_t* arch)
{
   int c = *pos;

   int64_t imm_shift       = 0;
   reg_t *reg_shift        = NULL;
   arm64_oprnd_ext_t *ext  = NULL;
   extend_t *extend        = NULL;
   shift_t *shift          = NULL;

   // Filtering on the start character to avoid multiple tests
   if ((strinsn[c] == 'U') || (strinsn[c] == 'S') || (strinsn[c] == 'L') || (strinsn[c] == 'A')){

      // Logical Shift Left
      if ((strinsn[c] == 'L') && (strinsn[c+1] == 'S') && (strinsn[c+2] == 'L')) {
         c += 3;

         // Constant shifts
         if ((strinsn[c] == ' ') && (strinsn[c+1] == '#')) {
            c += 2;

            /*Retrieving the offset*/
            parse_number__(strinsn, &c, &imm_shift);
            shift = arm64_shift_new(LSL, imm_shift);

         } else {
            return NULL;
         }

         ext = arm64_oprnd_ext_new_shift(shift);

      } else if ((strinsn[c] == 'L') && (strinsn[c+1] == 'S') && (strinsn[c+2] == 'R')) {
      // Logical Shift Right
         c += 3;

         // Constant shifts
         if ((strinsn[c] == ' ') && (strinsn[c+1] == '#')) {
            c += 2;

            /*Retrieving the offset*/
            parse_number__(strinsn, &c, &imm_shift);
            shift = arm64_shift_new(LSR, imm_shift);

         } else {
            return NULL;
         }

         ext = arm64_oprnd_ext_new_shift(shift);

      } else if ((strinsn[c] == 'A') && (strinsn[c+1] == 'S') && (strinsn[c+2] == 'R')) {
      // Arithmetic Shift Right
         c += 3;

         // Constant shifts
         if ((strinsn[c] == ' ') && (strinsn[c+1] == '#')) {
            c += 2;

            /*Retrieving the offset*/
            parse_number__(strinsn, &c, &imm_shift);
            shift = arm64_shift_new(ASR, imm_shift);

         } else {
            return NULL;
         }

         ext = arm64_oprnd_ext_new_shift(shift);

      } else if ((strinsn[c] == 'U') && (strinsn[c+1] == 'X') && (strinsn[c+2] == 'T')) {
      // Unsigned extends
         c += 3;

         enum extend_type_e extend_type;
         switch(strinsn[c]) {
            case 'B':
               extend_type = UXTB;
               break;
            case 'H':
               extend_type = UXTH;
               break;
            case 'W':
               extend_type = UXTW;
               break;
            case 'X':
               extend_type = UXTX;
               break;
            default:
               return NULL;
         }
         c++;

         // Constant shifts, this has no meaning and will be ignored by the processor but the assembly allow it...
         if ((strinsn[c] == ' ') && (strinsn[c+1] == '#')) {
            c += 2;

            /*Retrieving the offset*/
            parse_number__(strinsn, &c, &imm_shift);

         }

         extend = arm64_extend_new(extend_type, imm_shift);
         ext = arm64_oprnd_ext_new_extend(extend);

      } else if ((strinsn[c] == 'S') && (strinsn[c+1] == 'X') && (strinsn[c+2] == 'T')) {
      // Signed extends
         c += 3;

         enum extend_type_e extend_type;
         switch(strinsn[c]) {
            case 'B':
               extend_type = SXTB;
               break;
            case 'H':
               extend_type = SXTH;
               break;
            case 'W':
               extend_type = SXTW;
               break;
            case 'X':
               extend_type = SXTX;
               break;
            default:
               return NULL;
         }

                  // Constant shifts, this has no meaning and will be ignored by the processor but the assembly allow it...
         if ((strinsn[c] == ' ') && (strinsn[c+1] == '#')) {
            c += 2;

            /*Retrieving the offset*/
            parse_number__(strinsn, &c, &imm_shift);

         }

         extend = arm64_extend_new(extend_type, imm_shift);
         ext = arm64_oprnd_ext_new_extend(extend);

      }

   }

   *pos = c;

   return ext;
}

///////////////////////////////////////////////////////////////////////////////
//                                 Operands                                  //
///////////////////////////////////////////////////////////////////////////////

/**
 * Get a pointer address (address to which the operand points)
 * \param in The instruction the pointer belongs to
 * \param p a pointer operand
 * \return an address or SIGNED_ERROR if p is null
 */
static int64_t arm64_pointer_get_addr (insn_t* in, pointer_t* p)
{
   if (p == NULL)
      return SIGNED_ERROR;

   switch (p->type)
   {
      case POINTER_ABSOLUTE:
         return p->addr;
         break;
      case POINTER_RELATIVE:
         if (in == NULL)
            return SIGNED_ERROR;
         return (p->offset + in->address);
         break;
      default:
         return p->addr;
   }
}

/**
 * Sets the address pointed to by a pointer
 * \param in The instruction the pointer belongs to
 * \param ptr The pointer to update
 * \param address The new address pointed to
 */
static void arm64_pointer_set_addr (insn_t* in, pointer_t* ptr, int64_t address)
{

   if (!ptr)
      return;

   switch (ptr->type)
   {
      case POINTER_ABSOLUTE:
         ptr->addr = address;
         break;
      case POINTER_RELATIVE:
         if (in == NULL) return;
         ptr->offset = address - (in->address);
         DBGMSG("Branch instruction with code %d at address %#"PRIx64" now has offset %#"PRIx64" (destination: %#"PRIx64")\n",
                in->opcode, in->address, ptr->offset, arm64_pointer_get_addr(in,ptr));
         break;
      default:
         ptr->addr = address;
         break;
   }
}

/*
 * Updates the address and offset in a pointer operand
 * This function performs the following operations for a pointer_t inside the oprnd_t:
 * - If it is has a target, its address will be updated to that of the target
 * - If it is of type POINTER_RELATIVE and does not have a target, its address will be updated based on the address of insn_t and the offset,
 *   otherwise its offset will be updated based on its address and the address of insn_t
 * \param in The instruction
 * \param oprnd The operand
 * */
void arm64_oprnd_updptr(insn_t* in, pointer_t* ptr)
{
   if (!ptr)
      return;

   int hastarget = pointer_has_target(ptr);
   int64_t address = pointer_get_target_addr(ptr) + pointer_get_offset_in_target(ptr);

   //Update the address of the pointer with the address of the target
   if (hastarget)
      arm64_pointer_set_addr(in, ptr, address);

   //Now handle relative pointers
   if (pointer_get_type(ptr) == POINTER_RELATIVE && in) {
//      int64_t addrbase = 0;

//      if (!pointer_get_offset_origin(ptr)) {
//         addrbase = insn_get_addr(in) + insn_get_bytesize(in);
//      } else {
//         if (pointer_getorigin_type(ptr) == TARGET_INSN)
//            arm_oprnd_setptraddr(in, ptr, insn_get_addr(pointer_get_offset_origin(ptr)));
//         else if (pointer_getorigin_type(ptr) == TARGET_DATA)
//            arm_oprnd_setptraddr(in, ptr, data_get_addr(pointer_get_offset_origin(ptr)));
//         else
//            assert(0);
//      }
      if (hastarget) {
         //Pointer has a target: we consider its address is up to date, so it's the offset we need to update
         arm64_pointer_set_addr(in, ptr, address);
      } else {
         //No target: we will update the address of the pointer based on its offset
         pointer_set_addr(ptr, arm64_pointer_get_addr(in, ptr));
      }
   }
}

/**
 * Prints a memory operand in a format similar to objdump to a file stream
 * \param m The memory operand to print
 * \param f A file stream.
 * \param arch The architecture for which the operand is defined
 * \param role The operand's role
 */
static void arm64_oprnd_memory_fprint(void* o, FILE* f, arch_t* arch)
{
   if ((o == NULL) || (f == NULL))
      return;

   oprnd_t *oprnd = (oprnd_t*)o;
   memory_t *mem  = oprnd->data.mem;

   // Arm memory operands require a base
   if (mem->base != NULL) {
      // Prints the base register
      fprintf(f,"[%s", arch_get_reg_name (arch, mem->base->type, mem->base->name));

      // The memory operand is not post indexed
      if (!oprnd_mem_is_postindexed(oprnd)) {

         // Prints the index
         if (mem->index != NULL) 
            fprintf(f,",%s", arch_get_reg_name (arch, mem->index->type, mem->index->name));
         else if (mem->offset != 0) 
            // Immediate indexes act as offset and are stored this way in maqao structures
            fprintf(f, ",#%"PRId64, mem->offset);

         // Indexes can be shifted (operand extension)
         arm64_oprnd_ext_fprint(oprnd, f, arch);\
         fprintf(f, "]");

         // The base register can be write-back with the calculated address
         if (oprnd_mem_base_reg_is_dst(oprnd))
            fprintf(f,"!");

      // The memory operand is post indexed
      } else {
         fprintf(f, "]");

         // Prints the index
         if (mem->index != NULL)
            fprintf(f, ",%s", arch_get_reg_name (arch, mem->index->type, mem->index->name));
         else if (mem->offset != 0)
            // Immediate indexes act as offset and are stored this way in maqao structures
            fprintf(f, ",#%"PRId64, mem->offset);

         // Indexes can be shifted (operand extension)
         arm64_oprnd_ext_fprint(oprnd, f, arch);
      }
   }
}

/**
 * Prints a memory operand in a format similar to objdump to a file stream
 * \param m The memory operand to print
 * \param f A file stream.
 * \param arch The architecture for which the operand is defined
 * \param role The operand's role
 */
static void arm64_oprnd_memory_print(void* o, char* c, arch_t* arch)
{
   if ((o == NULL) || (c == NULL))
      return;

   oprnd_t* oprnd = (oprnd_t*)o;
   memory_t* mem = oprnd->data.mem;

   // Arm memory operands require a base
   if (mem->base != NULL)
   {
      // Prints the base register and the alignment if there is one
      c += sprintf (c, "[%s", arch_get_reg_name(arch, mem->base->type, mem->base->name));

      // The memory operand is not post indexed
      if (!oprnd_mem_is_postindexed(oprnd)) {

         // Prints the index
         if (mem->index != NULL)
               c += sprintf(c, ",%s", arch_get_reg_name(arch, mem->index->type, mem->index->name));
         else if (mem->offset != 0)
            // Immediate indexes act as offset and are stored this way in maqao structures
            c += sprintf(c, ",#%"PRId64, mem->offset); 

         // Indexes can be shifted (operand extension)
         arm64_oprnd_ext_print(oprnd,c,arch);\

         while (*c!='\0')
            c++;

         c += sprintf(c, "]");

         // The base register can be write-back with the calculated address
         if (oprnd_mem_base_reg_is_dst(oprnd))
            c += sprintf(c,"!");

      // The memory operand is post indexed
      } else {
         c += sprintf(c, "]");

         // Prints the index
         if (mem->index != NULL)
            c += sprintf(c,",%s", arch_get_reg_name (arch, mem->index->type, mem->index->name));
         else if(mem->offset != 0)
            // Immediate indexes act as offset and are stored this way in maqao structures   
            c += sprintf(c, ",#%"PRId64, mem->offset);

         // Indexes can be shifted (operand extension)
         arm64_oprnd_ext_print(oprnd,c,arch);

         while (*c!='\0')
            c++;
      }
   }
}

/**
 * Prints a pointer operand in a format similar to objdump in a file stream
 * \param in The instruction the operand belongs to. If not NULL, the destination
 * address of the operand will be printed, otherwise its value will be
 * \param p The pointer operand to print
 * \param f A file stream.
 */
static void arm64_pointer_fprint(insn_t* in, void* p, FILE* f)
{
   pointer_t* ptr = (pointer_t*)p;
   assert (p != NULL || f != NULL);

   if (in != NULL) {
      //Print branch label (if exists)
      int64_t branch       = arm64_pointer_get_addr(in, ptr);
      insn_t* branchdest   = ptr->target.insn;
      label_t* destlbl     = insn_get_fctlbl(branchdest);     //Retrieves the label of the destination
      int16_t opcode       = insn_get_opcode_code(in);

      if (destlbl != NULL) {
         int64_t lbloffs = branch - label_get_addr(destlbl);   //Calculates the offset to the label
         if (lbloffs > 0)
            fprintf(f, "%"PRIx64" <%s+%#"PRIx64">", branch, label_get_name(destlbl), lbloffs);
         else 
            fprintf(f, "%"PRIx64" <%s>", branch, label_get_name(destlbl));
      } else {
         fprintf(f, "<%"PRIx64">", branch);
      }
   } 
}

/**
 * Prints a pointer operand in a format similar to objdump in a file stream
 * \param in The instruction the operand belongs to. If not NULL, the destination
 * address of the operand will be printed, otherwise its value will be
 * \param p The pointer operand to print
 * \param f A file stream.
 */
static void arm64_pointer_print(insn_t* in, void* p, char* c)
{
   pointer_t* ptr = (pointer_t*)p;
   assert (p != NULL || c != NULL);

   //Print branch label (if exists)
   if (in != NULL) {
      int64_t branch       = arm64_pointer_get_addr(in,ptr);
      insn_t* branchdest   = ptr->target.insn;
      label_t* destlbl     = insn_get_fctlbl(branchdest);          //Retrieves the label of the destination
      int16_t opcode       = insn_get_opcode_code(in);

      if (destlbl != NULL) {
         int64_t lbloffs = branch - label_get_addr(destlbl);    //Calculates the offset to the label
         if (lbloffs > 0)
            sprintf(c, "%"PRIx64" <%s+%#"PRIx64">", branch, label_get_name(destlbl), lbloffs);
         else
            sprintf(c, "%"PRIx64" <%s>", branch, label_get_name(destlbl));
      } else {
         sprintf(c, "<%"PRIx64">", branch);
      }
   }
}

/**
 * Parses an memory operand (starting at the offset) from a string representation of an instruction
 * \param strinsn The string containing the representation of the instruction.
 * \param pos Pointer to the index into the string at which the parsing starts. Will be updated to contain the
 * index at which the memory operand ends (operand separator or end of string)
 * \param arch Pointer to the architecture structure for which the operand must be parsed
 * \return The parsed operand, or NULL if the parsing failed
 */
static oprnd_t* arm64_oprnd_parsenewmemory(char* strinsn, int *pos, arch_t* arch)
{
   int c = *pos;
   oprnd_t* out   = NULL;
   memory_t* mem  = NULL;
   reg_t* base    = NULL;
   reg_t* index   = NULL;
   int64_t offset = 0;
   arm64_oprnd_ext_t * ext;

   // Arm memory operands start with a '['
   if (strinsn[c] == '[') {
      c++;
      mem = memory_new();
      out = oprnd_new_memory(mem);

      // There is ALWAYS a base register
      base = reg_parsenew(strinsn, &c, arch);

      if (base != NULL)
         memory_set_base(mem, base);
      else
         return NULL;

      if ((strinsn[c] == ']') && (base != NULL)) {
         /*This is a post indexed memory or a base only memory */
         c++;

         if ((strinsn[c] == ',') && (strinsn[c+1] != '#')) {
            /* This is a post indexed memory */
            oprnd_mem_set_piflag(out);
            oprnd_mem_set_wbflag(out);
            c++;

            index = reg_parsenew(strinsn, &c, arch);

            if (index != NULL)
               memory_set_index(mem, index);

         } else if ((strinsn[c] == ',') && (strinsn[c+1] == '#')) {
            /* This is a post indexed memory : the index is an offset*/
            oprnd_mem_set_piflag(out);
            oprnd_mem_set_wbflag(out);
            c += 2;

            /*Retrieving the offset*/
            parse_number__(strinsn, &c, &offset);

            memory_set_offset(mem,offset);
         }

      } else {
         if ((strinsn[c] == ',') && (strinsn[c+1] != '#')) {
            /*There is also an index*/
            c++;

            index = reg_parsenew(strinsn, &c, arch);

            if (index != NULL) {
               memory_set_index(mem,index);

               /* Is there an extend ? */
               if (strinsn[c] == ',') {
                  c++;
                  ext = arm64_oprnd_ext_parsenew(strinsn, &c, arch);
                  oprnd_set_ext(out, (void*)ext);
               }
            }

         } else if ((strinsn[c] == ',') && (strinsn[c+1] == '#')) {
            /*There is also an offset*/
            c += 2;

            /*Retrieving the offset*/
            parse_number__(strinsn, &c, &offset);

            memory_set_offset(mem, offset);
         }

         if (strinsn[c] == ']')
            c++;

         // Write-back memories are identified by the '!'
         if (strinsn[c] == '!') {
            oprnd_mem_set_wbflag(out);
            c++;
         }

      }

      *pos = c;

   }

   return out;
}

/*
 * Parses an operand from a string representation of an instruction, based on a given architecture
 * \param strinsn The string containing the representation of the instruction.
 * \param pos Pointer to the index into the string at which the parsing starts. Will be updated to contain the
 * index at which the operand ends (operand separator or end of string)
 * \return The parsed operand, or NULL if the parsing failed
 */
oprnd_t* arm64_oprnd_parse(char* strinsn, int *pos)
{
   int c;
   oprnd_t* out   = NULL;
   arch_t* arch   = &arm64_arch;

   if (pos)
      c = *pos;
   else
      c = 0;

   switch (strinsn[c]) {
      reg_t* reg;
      arm64_oprnd_ext_t * ext;
      int64_t val;
      int parseres;

      case '#':
         c++;
         /*Beginning of an immediate operand*/
         parseres = parse_number__(strinsn, &c, &val);
         if (!ISERROR(parseres))
            out = oprnd_new_imm(val);
         break;
      case '[':
         /*Beginning of a memory operand*/
         out = arm64_oprnd_parsenewmemory(strinsn, &c, arch);
         break;
      default:
         reg = reg_parsenew(strinsn, &c, arch);
         if (reg != NULL){
            /*We found a register operand*/
            out = oprnd_new_reg(reg);
            /* Is there an operand extension ? */
            if (strinsn[c] == ',') {
               c++;
               ext = arm64_oprnd_ext_parsenew(strinsn, &c, arch);
               if (ext != NULL)
                  oprnd_set_ext(out, (void*)ext);
               else
                  c--;
            }
         } else {
            out = oprnd_new_ptr(c, 0, POINTER_RELATIVE);
            while (strinsn[c] != ',' && strinsn[c] != '\0' && strinsn[c] != ' ')
               c++; //Reaching the end of the label
         }
         break;
   }
   /* Updating pointer in the string */
   if ((out != NULL) && (pos))
      *pos = c;

   return out;
}

/**
 * Prints a oprnd in a format similar to objdump to a file stream
 * \param in The instruction the operand belongs to. If not NULL, the destination of
 * relative operands will be printed, and only their value otherwise
 * \param p The operand to print
 * \param f A file stream.
 * \param archi The architecture the operand belongs to
 */
static void arm64_oprnd_fprint(insn_t* in,void* p,FILE* f, arch_t* archi)
{
   oprnd_t* oprnd = (oprnd_t*)p;
   if (f == NULL)
      return;

   if (p == NULL) {
      fprintf(f, "(NULL)");
      return;
   }

   switch(oprnd_get_type(oprnd)) {
      case (OT_REGISTER):
         if (oprnd->data.reg != NULL) {
            fprintf(f, "%s", arch_get_reg_name(archi, oprnd->data.reg->type, oprnd->data.reg->name));

            if (oprnd_reg_is_indexed(oprnd))
               fprintf(f, "[%d]", oprnd_reg_get_index(oprnd));
            arm64_oprnd_ext_fprint(oprnd, f, archi);

         } else {
            fprintf(f, "(null)");
         }
         break;
      case (OT_MEMORY):
      case (OT_MEMORY_RELATIVE):
         arm64_oprnd_memory_fprint(oprnd, f, archi);
         break;
      case (OT_IMMEDIATE):
         fprintf(f, "#%"PRId64, oprnd->data.imm);
         break;
      case (OT_POINTER):
         arm64_pointer_fprint(in, oprnd->data.ptr, f);
         break;
      default:
         if (oprnd->data.reg != NULL)
            fprintf(f, "\"%s\"", arch_get_reg_name (archi, oprnd->data.reg->type, oprnd->data.reg->name));
         else
            fprintf(f, "\"(null)\"");
         break;
    }
}

/**
 * Prints a oprnd in a format similar to objdump to a file stream
 * \param in The instruction the operand belongs to. If not NULL, the destination of
 * relative operands will be printed, and only their value otherwise
 * \param p The operand to print
 * \param f A file stream.
 * \param archi The architecture the operand belongs to
 */
static void arm64_oprnd_print(insn_t* in,void* p,char* c, arch_t* archi)
{
   oprnd_t* oprnd = (oprnd_t*)p;
   if (c == NULL)
      return;

   if (p == NULL) {
      sprintf(c,"(NULL)");
      return;
   }

   switch(oprnd_get_type(oprnd)) {
      case (OT_REGISTER):
         if (oprnd->data.reg != NULL){
            c += sprintf(c, "%s", arch_get_reg_name(archi, oprnd->data.reg->type, oprnd->data.reg->name));

            if (oprnd_reg_is_indexed(oprnd)) {
               c += sprintf(c, "[%d]", oprnd_reg_get_index(oprnd));
            }
            arm64_oprnd_ext_print(oprnd, c, archi);

         } else {
            sprintf(c, "(null)");
         }
         break;
      case (OT_MEMORY):
      case (OT_MEMORY_RELATIVE):
         arm64_oprnd_memory_print(oprnd, c, archi);
         break;
      case (OT_IMMEDIATE):
         sprintf(c, "#%"PRId64, oprnd->data.imm);
         break;
      case (OT_POINTER):
         arm64_pointer_print(in, oprnd->data.ptr, c);
         break;
      default:
         if (oprnd->data.reg != NULL)
            sprintf(c, "\"%s\"", arch_get_reg_name (archi, oprnd->data.reg->type, oprnd->data.reg->name));
         else
            sprintf(c, "\"(null)\"");
         break;
   }
}

/*
 * Creates a new operand from a model.
 * \param src The operand to copy from.
 * \return A new operand with the same characteristics as the source or NULL if the source is NULL
 * */
oprnd_t* arm64_oprnd_copy(oprnd_t* oprnd)
{
   //Performs the generic part of the copy
   oprnd_t* copy = oprnd_copy_generic(oprnd);
   //If the generic copy failed, there is no need to go further
   if (copy == NULL)
      return NULL;

   arm64_oprnd_ext_t* ext = oprnd_get_ext(oprnd);
   //If the original has no archi-specific extension, no need to copy it
   if (ext == NULL)
      return copy;

   // Copying the extend
   extend_t* extend = arm64_oprnd_ext_get_extend(ext);
   extend_t* extend_cpy = NULL;
   if (extend != NULL) {
      extend_cpy = arm64_extend_new(arm64_extend_get_type(extend), arm64_extend_get_value(extend));
   }

   // Copying the shift
   shift_t* shift = arm64_oprnd_ext_get_shift(ext);
   shift_t* shift_cpy = NULL;
   if (shift != NULL) {
      shift_cpy = arm64_shift_new(arm64_shift_get_type(shift),arm64_shift_get_value(shift));
   }

   //Creating the copy extensions
   arm64_oprnd_ext_t* ext_cpy = arm64_oprnd_ext_new();
   arm64_oprnd_ext_set_extend(ext_cpy, extend_cpy);
   arm64_oprnd_ext_set_shift(ext_cpy, shift_cpy);

   oprnd_set_ext(copy, ext_cpy);

   return copy;
}

///////////////////////////////////////////////////////////////////////////////
//                               instruction                                 //
///////////////////////////////////////////////////////////////////////////////

/**
 * Frees an instruction structure
 * \param in The instruction to be freed
 */
void arm64_insn_free(void* p)
{
   if (p == NULL)
      return;
   insn_t* in = (insn_t*)p;
   int i = 0;

   for (i = 0; i < in->nb_oprnd; i++)
      arm64_oprnd_ext_free(in->oprndtab[i]);

   insn_free_common(in);
}

/**
 * Function for printing an Arm instruction
 * \param in A pointer to the instruction
 * \param c A char pointer to an allocated string
 * \param archi The architecture the instruction belongs to
 */
void arm64_insn_print(insn_t* in, char* c)
{
   if (in == NULL)
       return;

   arch_t* arch = &arm64_arch;

   /*Prints the opcode*/
   sprintf(c, "%s", arch_get_opcode_name (arch, in->opcode));
   while (*c != '\0')
      c++;

   /*Prints the condition suffix if there is one*/
   // if (arm64_insn_get_suffix(in) < NO_CND) {
   //    sprintf(c, "%s", ConditionSuffixes[arm64_insn_get_suffix(in)]);
   //    while (*c != '\0')
   //       c++;
   // }

   /*Prints the vector suffixes */
   // if (arm64_insn_has_outputsuffix(in) || arm64_insn_has_inputsuffix(in)){
   //    if (arm64_insn_has_outputsuffix(in)) {
   //       sprintf(c, ".%s%s", VectorTypeSuffix[insn_get_output_element_type(in)], VectorSizeSuffix[insn_get_output_element_size_raw(in)]);
   //       while (*c != '\0')
   //          c++;
   //    }
   //    if (arm64_insn_has_inputsuffix(in)) {
   //       sprintf(c, ".%s%s", VectorTypeSuffix[insn_get_input_element_type(in)], VectorSizeSuffix[insn_get_input_element_size_raw(in)]);
   //       while (*c != '\0')
   //          c++;
   //    }
   // }

   if (in->nb_oprnd > 0) {
      int i = 0;

      /*Prints the space between the instruction and the operands*/
      sprintf(c,"\t");
      c++;

      /*Prints the oprnds*/
      for (; i<in->nb_oprnd; i++)
      {
         arm64_oprnd_print(in, in->oprndtab[i], c, arch);
         while (*c != '\0')
            c++;

         if((i+1) < in->nb_oprnd){
            sprintf(c, ",");
            while (*c != '\0')
               c++;
         }
      }
   }
}

/**
 * Function for printing an arm instruction to a file stream
 * \param in A pointer to the instruction
 * \param f A file stream
 * \param archi The architecture the instruction belongs to
 */
void arm64_insn_fprint(insn_t* in, FILE* f)
{

   if (in == NULL)
      return;

   arch_t* arch = &arm64_arch;

   /*Prints the opcode*/
   fprintf(f, "%s", arch_get_opcode_name (arch, in->opcode));

   /*Prints the condition suffix if there is one*/
   // if (arm64_insn_get_suffix(in) < NO_CND)
   //    fprintf(f, "%s", ConditionSuffixes[arm_insn_get_suffix(in)]);

   /*Prints the vector suffixes */
   // if (arm64_insn_has_outputsuffix(in) || arm64_insn_has_inputsuffix(in)) {
   //    if (arm64_insn_has_outputsuffix(in))
   //       fprintf(f, ".%s%s", VectorTypeSuffix[insn_get_output_element_type(in)], VectorSizeSuffix[insn_get_output_element_size_raw(in)]);
   //    if (arm64_insn_has_inputsuffix(in))
   //       fprintf(f, ".%s%s", VectorTypeSuffix[insn_get_input_element_type(in)], VectorSizeSuffix[insn_get_input_element_size_raw(in)]);
   // }

   if (in->nb_oprnd > 0) {
      int i = 0;

      /*Prints the space between the instruction and the operands*/
      fprintf(f, "\t");

      /*Prints the oprnds*/
      for (; i < in->nb_oprnd; i++) {
         arm64_oprnd_fprint(in, in->oprndtab[i], f, arch);
         if((i+1) < in->nb_oprnd)
            fprintf(f, ",");
      }
   }
}

/**
 * Creates a new instruction from its string representation
 * \param strinsn The instruction in string form. It must have the same format as an instruction printed using insn_print
 * \param arch The architecture this instruction belongs to
 * \return The structure containing the instruction (without its coding), or PTR_ERROR if the instruction could not be parsed
 * \warn The current algorithm does not allow spaces in instruction names (ie suffixes or prefixes)
 */
insn_t* arm64_insn_parse(char* strinsn)
{
   if (strinsn == NULL || strlen(strinsn) <= 0)
       return NULL;

   arch_t* arch = &arm64_arch;
   insn_t* out = NULL;
   int c = 0, ia = 0, cond = 14, in_elt_type = 0, in_elt_size = 0, out_elt_type = 0, out_elt_size = 0, out_suffix = 0, in_suffix = 0;

   // Handling ".word 0xe320f000" as the NOP instruction
   // if (str_equal_nocase(strinsn, ".word 0xe320f000 ")) {
   //    out = insn_new(&arm_arch);
   //    insn_set_opcode(out, I_NOP);
   //    DBGMSG("Match made with opcode %s (%d)\n", arch->opcodes[I_NOP], I_NOP);
   //    arm_insn_set_suffix(out, cond);
   // }

   // Trying to find an opcode name matching with the instruction's name. Since opcodes
   // are ordered in lexicographical order, we can scan the list and stop once the current
   // character is above the string's current character
   while ((ia < arch->size_opcodes) && (strinsn[c] != '\0') && (strinsn[c] != ' ') && (strinsn[c] != '.') && (strinsn[c] >= arch->opcodes[ia][c])) {

      // Scans the architecture's opcode names to find one with the same beginning
      while ((ia < arch->size_opcodes) && (strinsn[c] > arch->opcodes[ia][c]))
         ia++;

      // Checks that the opcode found matches
      while ((arch->opcodes[ia][c] != '\0') && (strinsn[c] == arch->opcodes[ia][c]))
         c++;

      // Checks if there is a condition suffix
      if ((arch->opcodes[ia][c] == '\0') && (strinsn[c] != '\0') && (strinsn[c] != '.')) {
         int i;
         for (i = 0; i < 14; i++) {
            if ((strinsn[c] == ConditionSuffixes[i][0]) && (strinsn[c+1] == ConditionSuffixes[i][1])) {
               cond = i;
               c += 2;
               break;
            }
         }
      }
   }

   if ((ia < arch->size_opcodes) && ((arch->opcodes[ia][c] == '\0') || ((cond != 14) && (arch->opcodes[ia][c-2] == '\0')))) {

      // Now checking if the opcode name is actually completed in the input string
      if ((strinsn[c] == ' ') || (strinsn[c] == '\0')) {

         while ((strinsn[c] != '\0') && (strinsn[c] == ' '))
            c++; //Skips the spaces between the opcode name and the operands

         out = insn_new(&arm64_arch);
         insn_set_opcode(out, ia);
         DBGMSG("Match made with opcode %s (%d)\n", arch->opcodes[ia], ia);

         // Set the suffixes information
         //arm64_insn_set_suffix(out, cond);
         insn_set_output_element_type(out, out_elt_type);
         insn_set_output_element_size_raw(out, out_elt_size);
         insn_set_input_element_type(out, in_elt_type);
         insn_set_input_element_size_raw(out, in_elt_size);

         // Scanning the operands
         while (strinsn[c] != '\0') {

            // Skips spaces and brackets
            while ((strinsn[c] == '{') || (strinsn[c] == '}'))
               c++;

            oprnd_t* op = NULL;
            // Parsing the operand
            op = arm64_oprnd_parse(strinsn, &c);

            // Parsing ok: we add the operand to the instruction
            if (op != NULL) {

               if (oprnd_is_reg(op) || oprnd_is_mem(op))
                  oprnd_set_bitsize(op, DATASZ_UNDEF);

               // Parsing the element index in a register
               if (oprnd_is_reg(op) && (strinsn[c] == '[')) {

                  int64_t index;
                  c++;

                  int parse_ok = parse_number__(strinsn, &c, &index);

                  if ((parse_ok == TRUE) && (strinsn[c] == ']')) {
                     c++;
                     oprnd_reg_set_index(op, index);
                     oprnd_reg_set_irflag(op);
                  }
               }
               insn_add_oprnd (out, op);
            } else {
            // An error occurred: we quit and free the instruction
               DBGMSG("Unable to parse operands: exiting (%d: %c)\n", c, strinsn[c]);
               arch->insn_free(out);
               out = NULL;
               break;
            }

            // Skips spaces and brackets
            while ((strinsn[c] == '{') || (strinsn[c] == '}'))
               c++;

            // There is another operand
            if (strinsn[c] == ',') {
               c++;
               while (strinsn[c] != '\0' && ((strinsn[c] == ' ') || (strinsn[c] == '{') || (strinsn[c] == '}')))
                  c++;
            }
            // Wrong character: we quit and free the instruction
            else if (strinsn[c] != '\0') {

               DBGMSG("Wrong character (%c)%d found after operand %d: exiting\n", strinsn[c], c, insn_get_nb_oprnds(out));
               arch->insn_free(out);
               out = NULL;
               break;

            } //Else we reached the end of the string
         }
      }
      // else this means the opcode name found does not completely match the
      // instruction in the input string, so it is undefined
   }

   if (out)
      insn_add_annotate(out, arch->dflt_anno[insn_get_opcode_code(out)]);

   return out;
}

/**
 * Parses a list of instructions. The instructions must be separated by '\n' characters and use a syntax compatible with the architecture
 * Labels must be a chain of characters beginning with a letter, dot ('.'), or underscore ('_'), followed by a colon (':').
 * \param insn_list List of instructions separated by '\n' characters
 * \param arch The architecture the instructions to parse belong to
 * \return Queue of pointers to insn_t structures
 */
queue_t* arm64_insnlist_parse(char* insn_list)
{
   (void)insn_list;
   return NULL;
}

///////////////////////////////////////////////////////////////////////////////
//                               interworking                                //
///////////////////////////////////////////////////////////////////////////////

/**
 * Checks if a FSM switch is required.
 * It happens when there is an architecture change inside a binary.
 * \param af The asmfile of the binary file
 * \param address Address of the next instruction
 * \param address Address of the required reset (will be updated by the function)
 * \param container If not pointing to a NULL pointer, will be used as the starting
 * point for the search in the list of ordered labels, and will be updated to contain the container
 * of the label found
 * \return An integer corresponding of the architecture code for the next instruction
 */
int arm64_switchfsm(asmfile_t* af, int64_t address, int64_t* reset_address, list_t** container)
{
   return ARCH_arm64;
}
