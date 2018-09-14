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
 * \file maqaoerrs.h
 *
 *\brief This file contains the definition of the MAQAO error codes
 */

#ifndef MAQAOERRS_H_
#define MAQAOERRS_H_

/*** Positions and sizes of the elements of an error code inside the error code. Do not modify ***/
/*************************************************************************************************/
//@err_convert_read
#define MODULE_MASK        0xff
#define MODULE_SHIFT       24
#define ERRORLEVEL_MASK    0x0f
#define ERRORLEVEL_SHIFT   16
#define ERRORDESC_MASK     0xffff
#define ERRORDESC_SHIFT    0
//@err_convert_read
/*************************************************************************************************/

/*** Macros used to retrieve elements from an error code ***/
/***********************************************************/
/**
 * Macro for retrieving the module from the error code
 * \param ERRORCODE The error code
 * \return Identifier of the module associated to the error code
 * */
#define ERRORCODE_GET_MODULE(ERRORCODE) ((ERRORCODE >> MODULE_SHIFT) & MODULE_MASK)

/**
 * Macro for retrieving the error level from the error code
 * \param ERRORCODE the error code
 * \return Identifier of the error level
 * */
#define ERRORCODE_GET_LEVEL(ERRORCODE) ((ERRORCODE >> ERRORLEVEL_SHIFT) & ERRORLEVEL_MASK)

/**
 * Macro for retrieving the module-specific error description from the error code
 * \param ERRORCODE the error code
 * \return Module-specific description of the error code
 * */
#define ERRORCODE_GET_DESC(ERRORCODE) ((ERRORCODE >> ERRORDESC_SHIFT) & ERRORDESC_MASK)
/***********************************************************/

/*** Macros used in the declaration of error codes (should never be used outside this file) ***/
/**********************************************************************************************/
/**
 * Macro for setting the module inside the error code
 * \param MODULE Identifier of the module associated to the error code
 * */
#define SET_MODULE_IN_ERRORCODE(MODULE) ((MODULE & MODULE_MASK) << MODULE_SHIFT)

/**
 * Macro for setting the error level inside the error code
 * \param LEVEL Identifier of the error level
 * */
#define SET_LEVEL_IN_ERRORCODE(LEVEL) ((LEVEL & ERRORLEVEL_MASK) << ERRORLEVEL_SHIFT)

/**
 * Macro for setting the module-specific error description inside the error code
 * \param DESC Module-specific description of the error code
 * */
#define SET_DESC_IN_ERRORCODE(DESC) ((DESC & ERRORDESC_MASK) << ERRORDESC_SHIFT)

/**
 * Macro to use of declaring an error code
 * \param LEVEL Level of the error code
 * \param MODULE Module of the error code
 * \param DESCRIPTION Module-specific description of the error code
 * */
#define ERRORCODE_DECLARE(LEVEL, MODULE, DESCRIPTION) (SET_MODULE_IN_ERRORCODE(MODULE) | SET_LEVEL_IN_ERRORCODE(LEVEL) | SET_DESC_IN_ERRORCODE(DESCRIPTION))
/**********************************************************************************************/

/*** Declarations of the identifiers of the modules ***/
/******************************************************/
//@err_convert_read
// C Modules
#define MODULE_NONE     0x00  /**< RESERVED. DO NOT USE*/
#define MODULE_COMMON   0x01  /**< Common functions (declared in libmcommon.h)*/
#define MODULE_LIBASM   0x02  /**< Assembly functions (declared in libmasm.h)*/
#define MODULE_BINARY   0x03  /**< Parsing of the binary file (functions declared in libmtroll.h and others)*/
#define MODULE_DISASS   0x04  /**< Disassembly of the binary file (functions declared in libmdisass.h)*/
#define MODULE_ANALYZE  0x05  /**< Analysis of the binary file (functions declared in libmcore.h)*/
#define MODULE_PATCH    0x06  /**< Patching of the binary file (functions declared in libmpatch.h)*/
#define MODULE_MADRAS   0x07  /**< MADRAS API (functions declared in libmadras.h)*/
#define MODULE_MAQAO    0x08  /**< MAQAO API (function declared in libmaqao.h)*/
#define MODULE_DECAN    0x09  /**< DECAN*/
#define MODULE_ASMBL    0x0a  /**< Assembly of instructions*/
#define MODULE_LUAEXE   0x0b  /**< Lua execution*/
//Add further modules here

// Lua Modules
#define MODULE_CQA      0x20  /**< CQA*/
#define MODULE_UBENCH   0x21  /**< Microbench*/

#define MODULE_MAX      0xFF  /**< Maximum identifier of a module. If it is reached, the size of masks and shifts must be adapted*/
//@err_convert_read
/****************************************************/

/*** Declarations of the error levels ***/
/****************************************/
//@err_convert_read
#define ERRLVL_NONE     0x0   /**< RESERVED. DO NOT USE*/
#define ERRLVL_NFO      0x1   /**< Info:     Not an error, but information needs to be communicated to the user*/
#define ERRLVL_WRN      0x2   /**< Warning:  The function performs correctly, but with possibly incomplete results*/
#define ERRLVL_ERR      0x3   /**< Error:    The function failed, but the module execution may continue*/
#define ERRLVL_CRI      0x4   /**< Critical: The function failed and the module execution must stop*/

#define ERRLVL_MAX      0xF   /**< Maximum error level. If it is reached, the size of masks and shifts must be adapted*/
//@err_convert_read
/**************************************/

/*** Macros checking if an error code has a given level ***/
/**********************************************************/

/** Macro checking if an error code corresponds to a critical */
#define ISCRITICAL(ERRCODE) (ERRORCODE_GET_LEVEL(ERRCODE) == ERRLVL_CRI)

/** Macro checking if an error code corresponds to an error */
#define ISERROR(ERRCODE) (ERRCODE == EXIT_FAILURE || ERRORCODE_GET_LEVEL(ERRCODE) == ERRLVL_ERR)

/** Macro checking if an error code corresponds to a warning */
#define ISWARNING(ERRCODE) (ERRORCODE_GET_LEVEL(ERRCODE) == ERRLVL_WRN)
/**********************************************************/

/*** Declarations of the error codes **/
/**************************************/
/**
 * Declare the error code as follows: <ERRLVL>_<MODULE>_<DESC>
 * ERRLVL:  3 letters code of the error level (WRN / ERR / CRI /...)
 * MODULE:  Module of the error, using the same name as in the MODULE_* definitions
 * DESC:    Description of the error, using words separated with underscores (_)
 * Use the ERRORCODE_DECLARE macro to build the error code
 * */
// Every macro defined between @err_convert_translate tags should follow the current format:
//#define <macro_name> ERRORCODE_DECLARE(<error_level>, <module>, <error_code>)
// + error_level is either a valid ERRLVL_* macro or the corresponding positive integer value
// + module is either a valid MODULE_* macro or the corresponding positive integer value
// + error_code is a positive integer
// If a macro does not follow this format, it will not been converted into Lua
//@err_convert_translate
/** Error codes for the COMMON module **/
#define CRI_COMMON_UNABLE_TO_ALLOCATE_MEMORY       ERRORCODE_DECLARE(ERRLVL_CRI, MODULE_COMMON, 0x0001)   /**<Unable to allocate memory*/

#define ERR_COMMON_FILE_NOT_FOUND                  ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_COMMON, 0x0001)   /**<Unable to find requested file*/
#define ERR_COMMON_FILE_INVALID                    ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_COMMON, 0x0002)   /**<File is invalid*/
#define ERR_COMMON_FILE_NAME_MISSING               ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_COMMON, 0x0003)   /**<File name is missing*/
#define ERR_COMMON_PARAMETER_MISSING               ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_COMMON, 0x0004)   /**<A required parameter is NULL*/
#define ERR_COMMON_UNABLE_TO_OPEN_FILE             ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_COMMON, 0x0005)   /**<Unable to open file*/
#define ERR_COMMON_PARAMETER_INVALID               ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_COMMON, 0x0006)   /**<A parameter is invalid*/
#define ERR_COMMON_FILE_STREAM_MISSING             ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_COMMON, 0x0007)   /**<File stream missing*/
#define ERR_COMMON_UNABLE_TO_READ_FILE             ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_COMMON, 0x0008)   /**<Unable to read file*/

#define ERR_COMMON_NUMERICAL_BASE_NOT_SUPPORTED    ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_COMMON, 0x0010)   /**<Base not supported*/
#define ERR_COMMON_INTEGER_SIZE_INCORRECT          ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_COMMON, 0x0011)   /**<Size of integer not supported*/
#define ERR_COMMON_UNEXPECTED_CHARACTER            ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_COMMON, 0x0012)   /**<Unexpected character*/

#define ERR_COMMON_TXTFILE_COMMENT_END_NOT_FOUND           ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_COMMON, 0x0020)   /**<Comment ending tag not found*/
#define ERR_COMMON_TXTFILE_TAG_END_NOT_FOUND               ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_COMMON, 0x0021)   /**<Ending tag not found*/
#define ERR_COMMON_TXTFILE_PROPERTIES_MUTUALLY_EXCLUSIVE   ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_COMMON, 0x0022)   /**<Mutually exclusive properties*/
#define ERR_COMMON_TXTFILE_HEADER_EMPTY                    ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_COMMON, 0x0023)   /**<Header empty*/
#define ERR_COMMON_TXTFILE_HEADER_END_NOT_FOUND            ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_COMMON, 0x0024)   /**<End of header not found*/
#define ERR_COMMON_TXTFILE_SECTION_DUPLICATED              ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_COMMON, 0x0025)   /**<Section duplicated*/
#define ERR_COMMON_TXTFILE_SECTION_EMPTY                   ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_COMMON, 0x0026)   /**<Section empty*/
#define ERR_COMMON_TXTFILE_SECTION_END_NOT_FOUND           ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_COMMON, 0x0027)   /**<End section not found*/
#define ERR_COMMON_TXTFILE_SECTION_TOO_MANY_FIELDS         ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_COMMON, 0x0028)   /**<Section has too many fields*/
#define ERR_COMMON_TXTFILE_SECTION_PROPERTY_UNKNOWN        ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_COMMON, 0x0029)   /**<Unknown property for section*/
#define ERR_COMMON_TXTFILE_SECTION_TYPE_UNKNOWN            ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_COMMON, 0x0030)   /**<Type of section unknonwn*/
#define ERR_COMMON_TXTFILE_BODY_END_LINE_NOT_FOUND         ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_COMMON, 0x0031)   /**<End of body not found*/
#define ERR_COMMON_TXTFILE_BODY_DEFINITION_NOT_FOUND       ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_COMMON, 0x0032)   /**<Body definition not found*/
#define ERR_COMMON_TXTFILE_FIELD_ALIGNMENT_NOT_RESPECTED   ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_COMMON, 0x0033)   /**<Field alignment not respected*/
#define ERR_COMMON_TXTFILE_FIELD_NAME_DUPLICATED           ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_COMMON, 0x0034)   /**<Field name duplicated*/
#define ERR_COMMON_TXTFILE_FIELD_ENDING_NOT_FOUND          ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_COMMON, 0x0035)   /**<Field ending not found*/
#define ERR_COMMON_TXTFILE_FIELD_PARSING_ERROR             ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_COMMON, 0x0036)   /**<Field parsing error*/
#define ERR_COMMON_TXTFILE_FIELD_SEPARATOR_NOT_FOUND       ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_COMMON, 0x0037)   /**<Field separator not found*/
#define ERR_COMMON_TXTFILE_FIELD_PREFIX_NOT_FOUND          ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_COMMON, 0x0038)   /**<Field prefix not found*/
#define ERR_COMMON_TXTFILE_FIELD_UNAUTHORISED              ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_COMMON, 0x0039)   /**<Field not authorised*/
#define ERR_COMMON_TXTFILE_FIELD_NAME_UNKNOWN              ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_COMMON, 0x0040)   /**<Field name unknown*/
#define ERR_COMMON_TXTFILE_FIELD_IDENTIFIER_UNKNOWN        ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_COMMON, 0x0041)   /**<Field declaration identifier unknown*/
#define ERR_COMMON_TXTFILE_OPTIONAL_FIELDS_CONFUSION       ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_COMMON, 0x0042)   /**<Optional fields can't be distinguished*/
#define ERR_COMMON_TXTFILE_NOT_PARSED                      ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_COMMON, 0x0050)   /**<Text file has not been parsed*/
#define ERR_COMMON_TXTFILE_FIELD_NAME_MISSING              ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_COMMON, 0x0060)   /**<Missing field name*/
#define ERR_COMMON_TXTFILE_MISSING_MANDATORY_FIELD         ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_COMMON, 0x0061)   /**<Mandatory field missing*/

#define WRN_COMMON_TXTFILE_HEADER_COMPLETED                 ERRORCODE_DECLARE(ERRLVL_WRN, MODULE_COMMON, 0x0010)   /**<Header completed*/
#define WRN_COMMON_TXTFILE_NO_SECTIONS_REMAINING            ERRORCODE_DECLARE(ERRLVL_WRN, MODULE_COMMON, 0x0011)   /**<No section remaining*/
#define WRN_COMMON_TXTFILE_IGNORING_CHARACTERS              ERRORCODE_DECLARE(ERRLVL_WRN, MODULE_COMMON, 0x0012)   /**<Ignoring characters*/


/** Error codes for the LIBASM module **/
#define ERR_LIBASM_MISSING_ASMFILE        ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_LIBASM, 0x0001)  /**<Missing asmfile*/
#define ERR_LIBASM_ARCH_MISSING           ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_LIBASM, 0x0002)  /**<Missing architecture*/
#define ERR_LIBASM_ARCH_UNKNOWN           ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_LIBASM, 0x0003)  /**<Unknown architecture*/
#define ERR_LIBASM_ADDRESS_INVALID        ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_LIBASM, 0x0004)  /**<Invalid address*/
#define ERR_LIBASM_MISSING_PROJECT        ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_LIBASM, 0x0005)  /**<Missing project*/
#define ERR_LIBASM_UARCH_NAME_INVALID     ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_LIBASM, 0x0006)  /**<Invalid micro-architecture name*/
#define ERR_LIBASM_PROC_NAME_INVALID      ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_LIBASM, 0x0007)  /**<Invalid processor version name*/

#define ERR_LIBASM_INSTRUCTION_NOT_FOUND  ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_LIBASM, 0x0010)  /**<Instruction not found*/
#define ERR_LIBASM_INSTRUCTION_NOT_BRANCH ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_LIBASM, 0x0011)  /**<Instruction is not a branch*/
#define ERR_LIBASM_INSTRUCTION_MISSING    ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_LIBASM, 0x0012)  /**<Missing instruction*/
#define ERR_LIBASM_INSTRUCTION_NOT_PARSED ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_LIBASM, 0x0013)  /**<Instruction could not be parsed*/

#define ERR_LIBASM_OPERAND_NOT_FOUND      ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_LIBASM, 0x0020)  /**<Operand not found*/
#define ERR_LIBASM_OPERAND_MISSING        ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_LIBASM, 0x0021)  /**<Missing operand*/
#define ERR_LIBASM_OPERAND_NOT_REGISTER   ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_LIBASM, 0x0022)  /**<Operand is not a register*/
#define ERR_LIBASM_OPERAND_NOT_MEMORY     ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_LIBASM, 0x0023)  /**<Operand is not a memory address*/
#define ERR_LIBASM_OPERAND_NOT_IMMEDIATE  ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_LIBASM, 0x0024)  /**<Operand is not an immediate value*/
#define ERR_LIBASM_OPERAND_NOT_POINTER    ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_LIBASM, 0x0025)  /**<Operand is not a pointer*/
#define ERR_LIBASM_OPERAND_NOT_PARSED     ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_LIBASM, 0x0026)  /**<Operand could not be parsed*/
#define ERR_LIBASM_OPERAND_NOT_CREATED    ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_LIBASM, 0x0027)  /**<Operand could not be created*/

#define ERR_LIBASM_FUNCTION_NOT_FOUND     ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_LIBASM, 0x0030)  /**<Function not found*/

#define ERR_LIBASM_INCORRECT_DATA_TYPE          ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_LIBASM, 0x0040)  /**<Incorrect data type*/
#define ERR_LIBASM_DATA_MISSING                 ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_LIBASM, 0x0041)  /**<Missing data*/
#define ERR_LIBASM_ERROR_RETRIEVING_DATA_BYTES  ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_LIBASM, 0x0042)  /**<Unable to retrieve the bytes of a data entry*/

#define ERR_LIBASM_LABEL_MISSING          ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_LIBASM, 0x0050)  /**<Label missing*/

#define WRN_LIBASM_NO_DEBUG_DATA          ERRORCODE_DECLARE(ERRLVL_WRN, MODULE_LIBASM, 0x0001)  /**<File has no debug information*/

#define WRN_LIBASM_BRANCH_OPPOSITE_COND   ERRORCODE_DECLARE(ERRLVL_WRN, MODULE_LIBASM, 0x0010)  /**<Opposite of the branch instruction is conditional*/
#define WRN_LIBASM_BRANCH_HAS_NO_OPPOSITE ERRORCODE_DECLARE(ERRLVL_WRN, MODULE_LIBASM, 0x0011)  /**<Branch instruction has no opposite*/

/** Error codes for the BINPARSE module **/
#define ERR_BINARY_FORMAT_NOT_RECOGNIZED     ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_BINARY, 0x0001)  /**<Binary format not recognised or not supported*/
#define ERR_BINARY_MISSING_BINFILE           ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_BINARY, 0x0002)  /**<Missing structure representing the binary file*/
#define ERR_BINARY_HEADER_NOT_FOUND          ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_BINARY, 0x0003)  /**<Binary file header not found*/
#define ERR_BINARY_ARCHIVE_PARSING_ERROR     ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_BINARY, 0x0004)  /**<Error when parsing archive file*/
#define ERR_BINARY_UNKNOWN_FILE_TYPE         ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_BINARY, 0x0005)  /**<Unknown file type for this binary format*/
#define ERR_BINARY_FILE_ALREADY_PARSED       ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_BINARY, 0x0006)  /**<Binary file has already been parsed*/
#define ERR_BINARY_NO_EXTFCTS_SECTION        ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_BINARY, 0x0007)  /**<No section for external functions found*/
#define ERR_BINARY_NO_EXTLIBS                ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_BINARY, 0x0008)  /**<File has no external libraries*/
#define ERR_BINARY_NO_SECTIONS_FOUND         ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_BINARY, 0x0009)  /**<File has no sections*/
#define ERR_BINARY_SECTION_EMPTY             ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_BINARY, 0x000a)  /**<Section is empty*/
#define ERR_BINARY_SECTION_NOT_FOUND         ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_BINARY, 0x000b)  /**<Section was not found*/
#define ERR_BINARY_LIBRARY_TYPE_UNDEFINED    ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_BINARY, 0x000c)  /**<Library has an undefined type*/
#define ERR_BINARY_NO_SYMBOL_SECTION         ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_BINARY, 0x000d)  /**<No section for symbols found*/
#define ERR_BINARY_MISSING_SECTION           ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_BINARY, 0x000e)  /**<Missing structure representing a section*/
#define ERR_BINARY_BAD_SECTION_TYPE          ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_BINARY, 0x000f)  /**<Section had an incorrect type for the required operation*/
#define ERR_BINARY_BAD_SECTION_ENTRYSZ       ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_BINARY, 0x0010)  /**<Section had an incorrect entry size*/
#define ERR_BINARY_HEADER_ALREADY_PARSED     ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_BINARY, 0x0011)  /**<Header was already parsed*/
#define ERR_BINARY_SECTION_SEGMENT_NOT_FOUND ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_BINARY, 0x0012)  /**<The section could not be associated to a segment*/
#define ERR_BINARY_UNEXPECTED_FILE_FORMAT    ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_BINARY, 0x0013)  /**<Unexpected file format*/
#define ERR_BINARY_NO_STRING_SECTION         ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_BINARY, 0x0014)  /**<No section containing strings found*/
#define ERR_BINARY_MISSING_SEGMENT           ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_BINARY, 0x0015)  /**<Missing structure representing a segment*/

#define ERR_BINARY_SYMBOL_NOT_FOUND          ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_BINARY, 0x0020)  /**<Symbol was not found*/
#define ERR_BINARY_EXTFCT_NOT_FOUND          ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_BINARY, 0x0021)  /**<External function not found*/
#define ERR_BINARY_TARGET_ADDRESS_NOT_FOUND  ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_BINARY, 0x0022)  /**<Targeted address not found in the file*/
#define ERR_BINARY_EXTLIB_NOT_FOUND          ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_BINARY, 0x0023)  /**<External library not found*/

#define ERR_BINARY_SECTIONS_NOT_REORDERED          ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_BINARY, 0x0030)  /**<Unable to reorder the sections in the binary file*/
#define ERR_BINARY_FILE_NOT_BEING_PATCHED          ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_BINARY, 0x0031)  /**<Binary file is not in the process of being patched*/
#define ERR_BINARY_SECTION_DATA_NOT_LOCAL          ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_BINARY, 0x0032)  /**<Data in the section not allocated locally*/
#define ERR_BINARY_FAILED_SAVING_DATA_TO_SECTION   ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_BINARY, 0x0033)  /**<Data entry could not be saved to section*/
#define ERR_BINARY_PATCHED_FILE_NOT_FINALISED      ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_BINARY, 0x0034)  /**<Binary file is being patched but not finalised*/
#define ERR_BINARY_PATCHED_SECTION_NOT_CREATED     ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_BINARY, 0x0035)  /**<Patched copy of the section could not be created*/
#define ERR_BINARY_FAILED_INSERTING_STRING         ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_BINARY, 0x0036)  /**<String could not be inserted*/
#define ERR_BINARY_SECTION_NOT_RELOCATED           ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_BINARY, 0x0037)  /**<Section could not be relocated*/
#define ERR_BINARY_SECTION_ALREADY_EXISTING        ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_BINARY, 0x0038)  /**<Attempted to updated or create an already existing section*/

#define ERR_BINARY_UNABLE_TO_CREATE_FILE     ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_BINARY, 0x0040)  /**<Unable to initialise binary file for writing*/
#define ERR_BINARY_UNABLE_TO_WRITE_FILE      ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_BINARY, 0x0041)  /**<Unable to write binary file*/

#define ERR_BINARY_RELOCATION_NOT_SUPPORTED  ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_BINARY, 0x0050)  /**<Relocation type not supported*/
#define ERR_BINARY_RELOCATION_NOT_RECOGNISED ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_BINARY, 0x0051)  /**<Relocation type not recognised*/
#define ERR_BINARY_RELOCATION_INVALID        ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_BINARY, 0x0052)  /**<Invalid relocation type*/
#define ERR_BINARY_BAD_RELOCATION_ADDRESS    ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_BINARY, 0x0053)  /**<Invalid relocation address*/

#define ERR_BINARY_UNKNOWN_DEBUG_FORMAT      ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_BINARY, 0x0060)  /**<Unknown or unsupported debug format*/

/** Error codes for the ASMBL module **/
#define ERR_ASMBL_ARCH_NOT_SUPPORTED         ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_ASMBL, 0x0001)  /**<Architecture not supported for assembly*/

#define ERR_ASMBL_INSTRUCTION_NOT_ASSEMBLED     ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_ASMBL, 0x0010)   /**<Instruction could not be assembled*/
#define ERR_ASMBL_INSTRUCTION_HAS_CODING        ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_ASMBL, 0x0011)   /**<Instruction already has a coding*/
#define ERR_ASMBL_CODING_HAS_DIFFERENT_LENGTH   ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_ASMBL, 0x0012)   /**<New coding of instruction has a different length*/

/** Error codes for the DISASS module **/
#define ERR_DISASS_FILE_NOT_PARSED                       ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_DISASS, 0x0001)  /**<File has not been parsed*/
#define ERR_DISASS_STREAM_EMPTY                          ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_DISASS, 0x0002)  /**<Stream to disassemble is empty*/
#define ERR_DISASS_ARCH_NOT_SUPPORTED                    ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_DISASS, 0x0003)  /**<Architecture not supported for disassembly*/
#define ERR_DISASS_FILE_PARSING_FAILED                   ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_DISASS, 0x0004)  /**<File parsing failed*/
#define ERR_DISASS_FILE_DISASSEMBLY_FAILED               ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_DISASS, 0x0005)  /**<File disassembly failed*/

#define ERR_DISASS_FSM_NO_MATCH_FOUND                    ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_DISASS, 0x0010)  /**<Parser failed to find a match*/
#define ERR_DISASS_FSM_END_OF_STREAM_REACHED             ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_DISASS, 0x0011)  /**<End of stream reached during parsing*/

#define WRN_DISASS_EXT_FCTS_LBLS_NOT_RETRIEVED           ERRORCODE_DECLARE(ERRLVL_WRN, MODULE_DISASS, 0x0001)  /**<Unable to retrieve labels for external functions*/
#define WRN_DISASS_DBG_LBLS_NOT_RETRIEVED                ERRORCODE_DECLARE(ERRLVL_WRN, MODULE_DISASS, 0x0002)  /**<Unable to retrieve debug labels*/

#define WRN_DISASS_FSM_RESET_ADDRESS_OUT_OF_RANGE        ERRORCODE_DECLARE(ERRLVL_WRN, MODULE_DISASS, 0x0010)  /**<Requested parser reset to an out of range address*/
#define WRN_DISASS_FSM_RESET_ADDRESS_PARSING_IN_PROGRESS ERRORCODE_DECLARE(ERRLVL_WRN, MODULE_DISASS, 0x0011)  /**<Requested parser reset while parsing in progress*/

#define WRN_DISASS_INCOMPLETE_DISASSEMBLY                ERRORCODE_DECLARE(ERRLVL_WRN, MODULE_DISASS, 0x0020)  /**<Disassembly is incomplete*/

/** Error codes for the PATCH module **/
#define ERR_PATCH_ARCH_NOT_SUPPORTED               ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_PATCH, 0x0001)   /**<Architecture not supported for patching*/
#define ERR_PATCH_NOT_INITIALISED                  ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_PATCH, 0x0002)   /**<Patcher not initialised*/
#define ERR_PATCH_MISSING_MODIF_STRUCTURE          ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_PATCH, 0x0003)   /**<Missing modification structure*/
#define ERR_PATCH_WRONG_MODIF_TYPE                 ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_PATCH, 0x0004)   /**<Wrong type of modification request*/
#define ERR_PATCH_INSERT_LIST_EMPTY                ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_PATCH, 0x0005)   /**<List of instructions to insert is empty*/
#define ERR_PATCH_MISSING_MODIF_ADDRESS            ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_PATCH, 0x0006)   /**<Modification has no address*/
#define ERR_PATCH_FLOATING_MODIF_NO_SUCCESSOR      ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_PATCH, 0x0007)   /**<Floating modification has no successor*/
#define ERR_PATCH_FILE_NOT_FINALISED               ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_PATCH, 0x0008)   /**<Patched file is not finalised*/
#define ERR_PATCH_ADDRESS_LIST_ALREADY_CREATED     ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_PATCH, 0x0009)   /**<List of addresses has already been initialised*/
#define ERR_PATCH_MODIF_NOT_FINALISED              ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_PATCH, 0x000a)   /**<Modification is not finalised*/

#define ERR_PATCH_EXTFCT_STUB_NOT_GENERATED        ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_PATCH, 0x0010)   /**<Stub for external function could not be generated*/
#define ERR_PATCH_LABEL_INSERT_FAILURE             ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_PATCH, 0x0011)   /**<Label insertion failed*/
#define ERR_PATCH_RELOCATION_NOT_ADDED             ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_PATCH, 0x0012)   /**<Relocation not added to the binary file*/
#define ERR_PATCH_FUNCTION_NOT_INSERTED            ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_PATCH, 0x0013)   /**<Function could not be inserted to the file*/
#define ERR_PATCH_FUNCTION_CALL_NOT_GENERATED      ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_PATCH, 0x0014)   /**<Code for the function call could not be generated*/

#define ERR_PATCH_PADDING_INSN_TOO_BIG             ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_PATCH, 0x0020)   /**<Padding instruction is larger than the default one*/

#define ERR_PATCH_CONDITION_ARGUMENTS_MISMATCH     ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_PATCH, 0x0030)   /**<Mismatch between the condition arguments and type*/
#define ERR_PATCH_CONDITION_TYPE_UNKNOWN           ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_PATCH, 0x0031)   /**<Condition type unknown*/
#define ERR_PATCH_CONDITION_PARSE_ERROR            ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_PATCH, 0x0032)   /**<Error when parsing a condition*/
#define ERR_PATCH_CONDITION_MISSING                ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_PATCH, 0x0033)   /**<Expected condition was missing*/
#define ERR_PATCH_CONDITION_UNSUPPORTED_MODIF_TYPE ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_PATCH, 0x0034)   /**<Condition not supported for this type of modification*/

#define ERR_PATCH_REFERENCED_GLOBVAR_MISSING       ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_PATCH, 0x0040)   /**<Global variable to be referenced by an instruction is missing*/
#define ERR_PATCH_GLOBVAR_MISSING                  ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_PATCH, 0x0041)   /**<Missing global variable*/
#define ERR_PATCH_NO_SPACE_FOUND_FOR_GLOBVAR       ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_PATCH, 0x0042)   /**<No space found for inserting a global variable*/

#define ERR_PATCH_BASIC_BLOCK_NOT_FOUND            ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_PATCH, 0x0050)   /**<Unable to find a basic block around a given instruction*/
#define ERR_PATCH_INSERT_INSNLIST_FAILED           ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_PATCH, 0x0051)   /**<Unable to insert list of instructions*/
#define ERR_PATCH_INSUFFICIENT_SIZE_FOR_INSERT     ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_PATCH, 0x0052)   /**<Insufficient size for insertion of instructions*/
#define ERR_PATCH_UNABLE_TO_MOVE_TRAMPOLINE        ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_PATCH, 0x0053)   /**<Unable to move trampoline block*/
#define ERR_PATCH_UNABLE_TO_CREATE_TRAMPOLINE      ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_PATCH, 0x0054)   /**<Unable to insert trampoline rebound*/
#define ERR_PATCH_NO_SPACE_FOUND_FOR_BLOCK         ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_PATCH, 0x0055)   /**<No space found for moving a block*/
#define ERR_PATCH_NO_SPACE_FOUND_FOR_SECTION       ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_PATCH, 0x0056)   /**<No space found for moving a section*/

#define ERR_PATCH_UNRESOLVED_SYMBOL                ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_PATCH, 0x0060)   /**<Symbol not found in the inserted libraries*/

#define WRN_PATCH_SIZE_TOO_SMALL_FORCED_INSERT     ERRORCODE_DECLARE(ERRLVL_WRN, MODULE_PATCH, 0x0001)   /**<Insertion was forced while insertion site was too small*/
#define WRN_PATCH_FUNCTION_MOVED                   ERRORCODE_DECLARE(ERRLVL_WRN, MODULE_PATCH, 0x0002)   /**<A function was moved to perform the insertion*/
#define WRN_PATCH_MOVED_FUNCTION_HAS_INDIRECT_BRCH ERRORCODE_DECLARE(ERRLVL_WRN, MODULE_PATCH, 0x0003)   /**<A function containing an indirect branch was moved*/

#define WRN_PATCH_SYMBOL_ADDED_AS_EXTERNAL         ERRORCODE_DECLARE(ERRLVL_WRN, MODULE_PATCH, 0x0010)   /**<Undefined symbol was added as an external*/

#define WRN_PATCH_MODIF_NOT_PROCESSED              ERRORCODE_DECLARE(ERRLVL_WRN, MODULE_PATCH, 0x0020)   /**<Modification has not been processed and will not be applied*/

#define WRN_PATCH_NO_PENDING_MODIFS                 ERRORCODE_DECLARE(ERRLVL_WRN, MODULE_PATCH, 0x0030)  /**<Attempted to finalise patching session with no modification pending*/

#define WRN_PATCH_FILE_SAVED_WITH_DEFAULT_NAME      ERRORCODE_DECLARE(ERRLVL_WRN, MODULE_PATCH, 0x0040)  /**<File was saved with a default name as the given name was invalid*/

/** Error codes for the MADRAS API module **/
#define ERR_MADRAS_MISSING_MADRAS_STRUCTURE  ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_MADRAS, 0x0001)  /**<Missing MADRAS structure*/
#define ERR_MADRAS_MODIF_TYPE_NOT_SUPPORTED  ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_MADRAS, 0x0004)  /**<Modification type not supported*/

#define ERR_MADRAS_MISSING_CURSOR            ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_MADRAS, 0x0010)  /**<Missing MADRAS cursor*/
#define ERR_MADRAS_CURSOR_NOT_ALIGNED        ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_MADRAS, 0x0011)  /**<Unable to align cursor*/
#define ERR_MADRAS_MISSING_GLOBVAR           ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_MADRAS, 0x0012)  /**<Missing global variable*/
#define ERR_MADRAS_MODIF_COND_MISSING        ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_MADRAS, 0x0013)  /**<Modification has no condition*/
#define ERR_MADRAS_MODIF_ALREADY_HAS_ELSE    ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_MADRAS, 0x0014)  /**<Modification already has "else" code*/
#define ERR_MADRAS_ELSE_MODIF_IS_FIXED       ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_MADRAS, 0x0015)  /**<"Else" modification has a fixed address*/
#define ERR_MADRAS_MODIF_HAS_CUSTOM_PADDING  ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_MADRAS, 0x0016)  /**<Modification already has custom padding instruction*/
#define ERR_MADRAS_MODIF_ADD_COND_FAILED     ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_MADRAS, 0x0017)  /**<Unable to add condition to modification*/

#define ERR_MADRAS_ADD_LIBRARY_FAILED        ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_MADRAS, 0x0020)  /**<Unable to add request for library insertion*/
#define ERR_MADRAS_ADDRESSES_NOT_TRACKED     ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_MADRAS, 0x0021)  /**<Addresses were not tracked*/
#define ERR_MADRAS_MODIF_LABEL_FAILED        ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_MADRAS, 0x0022)  /**<Unable to add request for label modification*/
#define ERR_MADRAS_MODIF_CODE_FAILED         ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_MADRAS, 0x0023)  /**<Unable to add request for code modification*/
#define ERR_MADRAS_MODIF_LIBRARY_FAILED      ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_MADRAS, 0x0024)  /**<Unable to add request for library modification*/
#define ERR_MADRAS_MODIF_VARIABLE_FAILED     ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_MADRAS, 0x0025)  /**<Unable to add request for variable modification*/
#define ERR_MADRAS_RENAMING_LIBRARY_EXISTING ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_MADRAS, 0x0026)  /**<A request for renaming the library already exists*/

#define WRN_MADRAS_MODIFS_ALREADY_INIT       ERRORCODE_DECLARE(ERRLVL_WRN, MODULE_MADRAS, 0x0001)  /**<Modifications were already initiated for this file*/

#define WRN_MADRAS_STACK_SHIFT_NULL          ERRORCODE_DECLARE(ERRLVL_WRN, MODULE_MADRAS, 0x0010)  /**<Stack shift requested for patching with a null shift value*/

#define WRN_MADRAS_NEWNAME_IDENTICAL         ERRORCODE_DECLARE(ERRLVL_WRN, MODULE_MADRAS, 0x0020)  /**<Requested renaming of element to an identical name*/

/** Error codes for the Lua execution**/
#define ERR_LUAEXE_MISSING_LUA_STATE         ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_LUAEXE, 0x0001)  /**<Missing Lua state*/
#define ERR_LUAEXE_MISSING_LUA_CHUNK         ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_LUAEXE, 0x0002)  /**<Missing Lua chunk to execute*/
#define ERR_LUAEXE_PRECOMP_SYNTAX_ERROR      ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_LUAEXE, 0x0003)  /**<Syntax error during pre-compilation*/
#define ERR_LUAEXE_PRECOMP_MEMORY_ALLOCATION ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_LUAEXE, 0x0004)  /**<Memory allocation error during pre-compilation*/
#define ERR_LUAEXE_RUNTIME_ERROR             ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_LUAEXE, 0x0005)  /**<Lua runtime error*/
#define ERR_LUAEXE_UNKNOWN_RUNTIME_ERROR     ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_LUAEXE, 0x0006)  /**<Unspecified Lua runtime error*/
#define ERR_LUAEXE_MEMORY_ALLOCATION         ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_LUAEXE, 0x0007)  /**<Memory allocation error*/
#define ERR_LUAEXE_ERROR_HANDLER             ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_LUAEXE, 0x0008)  /**<Error while running the error handler*/

/** Error codes for MAQAO API*/
#define ERR_MAQAO_UNABLE_TO_DETECT_PROC_HOST ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_MAQAO, 0x0001)   /**<Unable to retrieve processor version of the host*/
#define ERR_MAQAO_MISSING_UARCH_OR_PROC      ERRORCODE_DECLARE(ERRLVL_ERR, MODULE_MAQAO, 0x0002)   /**<Missing micro architecture or processor version*/
//@err_convert_translate


/**
 * Identifiers for the levels of verbosity of MAQAO messages. The identifiers should always be ordered
 * with increasing corresponding verbosity. Each level includes the previous one
 * */
typedef enum maqao_verbose_e {
   MAQAO_VERBOSE_MUTE = 0, /**< Mute mode, no output*/
   MAQAO_VERBOSE_CRITICAL, /**< Only critical messages are printed*/
   MAQAO_VERBOSE_ERROR,    /**< Error messages are printed*/
   MAQAO_VERBOSE_WARNING,  /**< Warning messages are printed*/
   MAQAO_VERBOSE_MESSAGE,  /**< Standard messages are printed*/
   MAQAO_VERBOSE_INFO,     /**< Info messages are printed*/
   MAQAO_VERBOSE_ALL = 255 /**< Everything is printed (must always be last)*/
} maqao_verbose_t;

/** Global variable containing the level of verbosity for all of MAQAO*/
extern maqao_verbose_t __MAQAO_VERBOSE_LEVEL__;

/**
 * Builds an error code from its components
 * \param module Code of the module where the error occurs
 * \param level Level of severity of the error
 * \param code Module-specific code for the error
 * \return Error code
 * */
extern int errcode_build(const int module, const int level, const int code);

/**
 * Retrieves the module identifier from an error code
 * \param errcode The error code
 * \return The code of the module for which the error code is defined
 * */
extern int errcode_getmodule(const int errcode);

/**
 * Retrieves the level of an error code
 * \param errcode The error code
 * \return The level of severity of the error
 * */
extern int errcode_getlevel(const int errcode);

/**
 * Returns the name of the level of an error code
 * \param errlvl The level of the error code
 * */
extern char* errlevel_getname(const int errlvl);

/**
 * Returns the error message associated with an error code.
 * Update this function along with the error codes defined in maqaoerrs.h
 * \param errcode The error code
 * \return The associated error message
 * */
extern char* errcode_getmsg(const int errcode);

/**
 * Prints the full error message associated with an error code
 * \param errcode The error code
 * */
extern void errcode_printfullmsg(const int errcode);


#endif /* MAQAOERRS_H_ */
