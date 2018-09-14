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
#ifndef ARM64_EXT_H_
#define ARM64_EXT_H_

/*!
 * \file arm_ext64.h
 * \brief This files declare the enumerations and the functions for the handling of Arm64 operands' extensions.
 */

#include <inttypes.h>
#include "libmasm.h"

//TODO remove when updated on master
#define S_SCALAR  S_NO
#define S_SIMD    S_YES

enum arm64_conditions_e {
   CND_EQ=0,
   CND_NE,
   CND_CS,
   CND_CC,
   CND_MI,
   CND_PL,
   CND_VS,
   CND_VC,
   CND_HI,
   CND_LS,
   CND_GE,
   CND_LT,
   CND_GT,
   CND_LE,
   NO_CND,
   RESERVED
};

/**
 * Alias for struct extend_s structure
 */
typedef struct extend_s extend_t;

/**
 * Alias for struct shift_s structure
 */
typedef struct shift_s shift_t;

/**
 * Alias for struct arm64_oprnd_ext_s structure
 */
typedef struct arm64_oprnd_ext_s arm64_oprnd_ext_t;


///////////////////////////////////////////////////////////////////////////////
//                               instruction                                 //
///////////////////////////////////////////////////////////////////////////////

/*
 * Frees an instruction structure
 * \param in The instruction to be freed
 */
extern void arm64_insn_free(void* p);

/*
 * Function for printing a arm64 instruction
 * \param in A pointer to the instruction
 * \param c A char pointer to an allocated string
 * \param archi The architecture the instruction belongs to
 */
extern void arm64_insn_print(insn_t* in, char* c);

/*
 * Function for printing a arm64 instruction to a file stream
 * \param in A pointer to the instruction
 * \param f A file stream
 * \param archi The architecture the instruction belongs to
 */
extern void arm64_insn_fprint(insn_t* in, FILE* f);
/*
 * Creates a new instruction from its string representation
 * \param strinsn The instruction in string form. It must have the same format as an instruction printed using insn_print
 * \param arch The architecture this instruction belongs to
 * \return The structure containing the instruction (without its coding), or PTR_ERROR if the instruction could not be parsed
 * \warn The current algorithm does not allow spaces in instruction names (ie suffixes or prefixes)
 */
extern insn_t* arm64_insn_parse(char* strinsn);

/**
 * Parses a list of instructions. The instructions must be separated by '\n' characters and use a syntax compatible with the architecture
 * Labels must be a chain of characters beginning with a letter, dot ('.'), or underscore ('_'), followed by a colon (':').
 * \param insn_list List of instructions separated by '\n' characters
 * \param arch The architecture the instructions to parse belong to
 * \return Queue of pointers to insn_t structures
 * */
extern queue_t* arm64_insnlist_parse(char* insn_list);

/**
 * Function testing if two instructions are equal
 * \param in1 First instruction
 * \param in2 Second instruction
 * \return TRUE if both instructions are equal, FALSE otherwise
 * */
//extern boolean_t arm_insn_equal(insn_t* in1, insn_t* in2);
#define arm64_insn_equal NULL

///////////////////////////////////////////////////////////////////////////////
//                                 Operand                                   //
///////////////////////////////////////////////////////////////////////////////

/**
 * Parses an operand from a string representation of an instruction, based on a given architecture
 * \param strinsn The string containing the representation of the instruction.
 * \param pos Pointer to the index into the string at which the parsing starts. Will be updated to contain the
 * index at which the operand ends (operand separator or end of string)
 * \return The parsed operand, or NULL if the parsing failed
 */
extern oprnd_t* arm64_oprnd_parse(char* strinsn, int *pos);

/**
 * Updates the address and offset in a pointer operand
 * This function performs the following operations for a pointer_t inside the oprnd_t:
 * - If it is has a target, its address will be updated to that of the target
 * - If it is of type POINTER_RELATIVE and does not have a target, its address will be updated based on the address of insn_t and the offset,
 *   otherwise its offset will be updated based on its address and the address of insn_t
 * \param in The instruction
 * \param oprnd The operand
 * */
extern void arm64_oprnd_updptr(insn_t* in, pointer_t* ptr);

/**
 * Function testing if two operands are equal
 * \param op1 First operand
 * \param op2 Second operand
 * \return TRUE if both operands are equal, FALSE otherwise
 * */
//extern boolean_t arm_oprnd_equal(oprnd_t* op1, oprnd_t* op2);
#define arm64_oprnd_equal NULL

/**
 * Creates a new operand from a model.
 * \param src The operand to copy from.
 * \return A new operand with the same characteristics as the source or NULL if the source is NULL
 * */
extern oprnd_t* arm64_oprnd_copy(oprnd_t* oprnd);

///////////////////////////////////////////////////////////////////////////////
//                                  Extend                                   //
///////////////////////////////////////////////////////////////////////////////
/**
 * \struct extend_s
 * \brief This structure stores a extend
 */
struct extend_s
{
   uint8_t type;           /**<Type of the extend*/
   uint8_t value;          /**<Value of the extend (it is "optional")*/
};

enum extend_type_e {
   UXTB=0,
   UXTH,
   UXTW,
   UXTX,
   SXTB,
   SXTH,
   SXTW,
   SXTX
};

/**
 * Create a new extend and sets its type
 * \param type Type of the extend
 * \param value The extend value
 * \return A pointer to the new extend
 */
extern extend_t* arm64_extend_new (uint8_t type, uint8_t value);

/**
 * Gets the extend type
 * \param extend A pointer to the extend
 * \return A uint8_t containing the extend type
 */
extern uint8_t arm64_extend_get_type (extend_t* extend);

/**
 * Gets the extend value
 * \param extend A pointer to the extend
 * \return A uint8_t containing the extend value
 */
extern uint8_t arm64_extend_get_value (extend_t* extend);

/**
 * Sets the extend type
 * \param extend A pointer to the extend
 * \param type The type to set to the extend
 */
extern void arm64_extend_set_type (extend_t* extend, uint8_t type);

/**
 * Sets the extend value
 * \param extend A pointer to the extend
 * \param value The value to set to the extend
 */
extern void arm64_extend_set_value (extend_t* extend, uint8_t value);

/**
 * Frees an extend
 * \param extend A pointer to the extend
 */
extern void arm64_extend_free (extend_t* extend);


///////////////////////////////////////////////////////////////////////////////
//                                  Shift                                    //
///////////////////////////////////////////////////////////////////////////////

/**
 * \struct shift_s
 * \brief This structure stores a shift
 */
struct shift_s
{
   uint8_t type;       /**<Type of the shift (Logic Shift Left/Right, Arithmetic Shift Right)*/
   uint8_t value;      /**<The shift value*/
};

enum shift_type_e {
   LSL=0,
   LSR,
   ASR,
   ROR,
   MSL
};

/**
 * Create a new shift extension and sets its type and value
 * \param type Type of the shift
 * \param value The shift value
 * \return A pointer to the new shift
 */
extern shift_t* arm64_shift_new (uint8_t type, uint8_t value);

/**
 * Gets the shift type
 * \param shift A pointer to the shift
 * \return A uint8_t containing the shift type
 */
extern uint8_t arm64_shift_get_type (shift_t* shift);

/**
 * Gets the shift value
 * \param shift A pointer to the shift
 * \return A uint8_t containing the shift value
 */
extern uint8_t arm64_shift_get_value (shift_t* shift);

/**
 * Sets the shift type
 * \param shift A pointer to the shift
 * \param type The type to set to the shift
 */
extern void arm64_shift_set_type (shift_t* shift, uint8_t type);

/**
 * Sets the shift value
 * \param shift A pointer to the shift
 * \param value The value to set to the shift
 */
extern void arm64_shift_set_value (shift_t* shift, uint8_t value);

/**
 * Frees a shift
 * \param shift A pointer to the shift
 */
extern void arm64_shift_free (shift_t* shift);


///////////////////////////////////////////////////////////////////////////////
//                            Operand Extension                              //
///////////////////////////////////////////////////////////////////////////////
/**
 * \struct arm64_oprnd_ext_s
 * \brief This structure stores an arm operand extension
 */

struct arm64_oprnd_ext_s
{
   union {
      extend_t* extend;
      shift_t* shift;
   } ext;
   uint8_t ext_type;
   uint8_t arrangement;
};

enum ext_type_e {
   EMPTY = 0,
   EXTEND,
   SHIFT
};

typedef enum arrangement_e {
   NONE = 0,
   B,
   B8,
   B16,
   H,
   H4,
   H8,
   S,
   S2,
   S4,
   D,
   D1,
   D2
} arrangement_t;

/**
 * Create a new operand extension
 * \return A pointer to the new operand extension
 */
extern arm64_oprnd_ext_t* arm64_oprnd_ext_new();

/**
 * Create a new operand extension and sets an extend
 * \param extend A pointer to the extend
 * \return A pointer to the new operand extension
 */
extern arm64_oprnd_ext_t* arm64_oprnd_ext_new_extend(extend_t* extend);

/**
 * Create a new operand extension and sets a shift
 * \param shift A pointer to the shift
 * \return A pointer to the new operand extension
 */
extern arm64_oprnd_ext_t* arm64_oprnd_ext_new_shift(shift_t* shift);

/**
 * Create a new operand extension and sets an arrangement
 * \param arrangement The arrangement value
 * \return A pointer to the new operand extension
 */
extern arm64_oprnd_ext_t* arm64_oprnd_ext_new_arrangement(arrangement_t arrangement);

/**
 * Gets the extend of an extension
 * \param ext A pointer to the extension
 * \return A pointer to the extend
 */
extern extend_t* arm64_oprnd_ext_get_extend(arm64_oprnd_ext_t* ext);

/**
 * Gets the shift of an extension
 * \param ext A pointer to the extension
 * \return A pointer to the shift
 */
extern shift_t* arm64_oprnd_ext_get_shift(arm64_oprnd_ext_t* ext);

/**
 * Gets the arrangement of an extension
 * \param ext A pointer to the extension
 * \return The operand arrangement
 */
extern arrangement_t arm64_oprnd_ext_get_arrangement(arm64_oprnd_ext_t* ext);

/**
 * Sets the extend of an extension
 * \param ext A pointer to the extension
 * \param extend A pointer to the extend
 */
extern void arm64_oprnd_ext_set_extend(arm64_oprnd_ext_t* ext, extend_t* extend);

/**
 * Sets the shift of an extension
 * \param ext A pointer to the extension
 * \param shift A pointer to the shift
 */
extern void arm64_oprnd_ext_set_shift(arm64_oprnd_ext_t* ext, shift_t* shift);

/**
 * Sets the arrangement of an extension
 * \param ext A pointer to the extension
 * \param arrangement The arrangement value to set
 */
extern void arm64_oprnd_ext_set_arrangement(arm64_oprnd_ext_t* ext, arrangement_t arrangement);


/**
 * Frees the arm operand extension in an operand structure
 * \param oprnd The operand in which to free the extension
 */
extern void arm64_oprnd_ext_free (oprnd_t* oprnd);


///////////////////////////////////////////////////////////////////////////////
//                               interworking                                //
///////////////////////////////////////////////////////////////////////////////

/**
 * Checks if a FSM switch is required.
 * It happens when there is an architecture change inside a binary.
 * In ARM architecture there is a label set when changing the architecture.
 * The format is '$a' for arm assembly and '$t' for thumb assembly.
 * \param af The asmfile of the binary file
 * \param address Address of the next instruction
 * \param address Address of the required reset (will be updated by the function)
 * \param container If not pointing to a NULL pointer, will be used as the starting
 * point for the search in the list of ordered labels, and will be updated to contain the container
 * of the label found
 * \return An integer corresponding of the architecture code for the next instruction
 */
extern int arm64_switchfsm(asmfile_t* af, int64_t address, int64_t* reset_address, list_t** container);

///
/// Implicit registers
///
extern reg_t** arm64_get_implicit_srcs (arch_t* a, int opcode, int* nb_reg);

extern reg_t** arm64_get_implicit_dsts (arch_t* a, int opcode, int* nb_reg);

#endif
