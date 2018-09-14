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
 * \file maqaoerrs.c
 *
 *\date 2015-08-11
 *\brief This file contains functions for printing the message associated with an error code.
 * It must be kept up to date with maqaoerrs.h
 */

#include "libmcommon.h"

maqao_verbose_t __MAQAO_VERBOSE_LEVEL__ = MAQAO_VERBOSE_ALL;

/**
 * Returns the name associated with a module present in an error code
 * \param errcode The error code
 * \return The name of the associated module
 * */
static char* errcode_getmodulename(const int errcode) {
   int errmodule = ERRORCODE_GET_MODULE(errcode);
   switch (errmodule) {
   case MODULE_COMMON:
      return "COMMON";  /**< Common functions (declared in libmcommon.h)*/
   case MODULE_LIBASM:
      return "ASSEMBLY";   /**< Assembly functions (declared in libmasm.h)*/
   case MODULE_BINARY:
      return "BINARY";   /**< Handling of the binary file (functions declared in libmtroll.h and others)*/
   case MODULE_DISASS:
      return "DISASSEMBLY";  /**< Disassembly of the binary file (functions declared in libmdisass.h)*/
   case MODULE_ANALYZE:
      return "ANALYSIS";   /**< Analysis of the binary file (functions declared in libmcore.h)*/
   case MODULE_PATCH:
      return "PATCH"; /**< Patching of the binary file (functions declared in libmpatch.h)*/
   case MODULE_MADRAS:
      return "MADRAS API"; /**< MADRAS API (functions declared in libmadras.h)*/
   case MODULE_MAQAO:
      return "MAQAO API";   /**< MAQAO API (function declared in libmaqao.h)*/
   case MODULE_DECAN:
      return "DECAN";   /**< DECAN*/
   case MODULE_ASMBL:
      return "ASSEMBLER"; /**< Assembly of instructions*/
   default:
      return "Unknown module";
   }
}

/*
 * Builds an error code from its components
 * \param module Code of the module where the error occurs
 * \param level Level of severity of the error
 * \param code Module-specific code for the error
 * \return Error code
 * */
int errcode_build(const int module, const int level, const int code) {
   return ERRORCODE_DECLARE(level, module, code);
}

/*
 * Retrieves the module identifier from an error code
 * \param errcode The error code
 * \return The code of the module for which the error code is defined
 * */
int errcode_getmodule(const int errcode) {
   return ERRORCODE_GET_MODULE(errcode);
}

/*
 * Retrieves the level of an error code
 * \param errcode The error code
 * \return The level of severity of the error
 * */
int errcode_getlevel(const int errcode) {
   return ERRORCODE_GET_LEVEL(errcode);
}

/*
 * Returns the name of the level of an error code
 * \param errlvl The level of the error code
 * */
char* errlevel_getname(const int errlvl) {
   switch(errlvl) {
   case ERRLVL_NONE:
      return "none";
   case ERRLVL_NFO:
      return "info";
   case ERRLVL_WRN:
      return "warning";
   case ERRLVL_ERR:
      return "error";
   case ERRLVL_CRI:
      return "critical";
   default:
      return "unknown";
   }
}

/*
 * Returns the error message associated with an error code.
 * Update this function along with the error codes defined in maqaoerrs.h
 * \param errcode The error code
 * \return The associated error message
 * */
char* errcode_getmsg(const int errcode) {
   switch (errcode) {
   case EXIT_SUCCESS:
      return "Success"; /**<No error!*/
   case EXIT_FAILURE:
      return "Generic error"; /**<Generic error*/

   /** Error codes for the COMMON module **/
   case CRI_COMMON_UNABLE_TO_ALLOCATE_MEMORY:
      return "Unable to allocate memory"; /**<Unable to allocate memory*/
   case ERR_COMMON_FILE_NOT_FOUND:
      return "File not found";   /**<Unable to find requested file*/
   case ERR_COMMON_FILE_INVALID:
      return "Invalid file";  /**<File is invalid*/
   case ERR_COMMON_FILE_NAME_MISSING:
      return "Missing file name";   /**<File name is missing*/
   case ERR_COMMON_UNABLE_TO_OPEN_FILE:
      return "Unable to open file";   /**<Unable to open file*/
   case ERR_COMMON_PARAMETER_MISSING:
      return "Missing required parameter";   /**<A required parameter is NULL*/
   case ERR_COMMON_PARAMETER_INVALID:
      return "A parameter is invalid";  /**<A parameter is invalid*/
   case ERR_COMMON_FILE_STREAM_MISSING:
      return "File stream missing";  /**<File stream missing*/
   case ERR_COMMON_UNABLE_TO_READ_FILE:
      return "Unable to read file";  /**<Unable to read file*/

   case ERR_COMMON_NUMERICAL_BASE_NOT_SUPPORTED:
      return "Numerical base not supported"; /**<Base not supported*/
   case ERR_COMMON_INTEGER_SIZE_INCORRECT:
      return "Size of integer not supported";   /**<Size of integer not supported*/
   case ERR_COMMON_UNEXPECTED_CHARACTER:
      return "Unexpected character";   /**<Unexpected character*/

   case ERR_COMMON_TXTFILE_COMMENT_END_NOT_FOUND:
      return "Comment ending tag not found in formatted text file";  /**<Comment ending tag not found*/
   case ERR_COMMON_TXTFILE_TAG_END_NOT_FOUND:
      return "Ending tag not found in formatted text file";  /**<Ending tag not found*/
   case ERR_COMMON_TXTFILE_PROPERTIES_MUTUALLY_EXCLUSIVE:
      return "Mutually exclusive properties found in formatted text file";  /**<Mutually exclusive properties*/
   case ERR_COMMON_TXTFILE_HEADER_EMPTY:
      return "Header empty in formatted text file";  /**<Header empty*/
   case ERR_COMMON_TXTFILE_HEADER_END_NOT_FOUND:
      return "End of header not found in formatted text file";  /**<End of header not found*/
   case ERR_COMMON_TXTFILE_SECTION_DUPLICATED:
      return "Duplicated section in formatted text file";  /**<Section duplicated*/
   case ERR_COMMON_TXTFILE_SECTION_EMPTY:
      return "Empty section in formatted text file";  /**<Section empty*/
   case ERR_COMMON_TXTFILE_SECTION_END_NOT_FOUND:
      return "End of section not found in formatted text file";  /**<End section not found*/
   case ERR_COMMON_TXTFILE_SECTION_TOO_MANY_FIELDS:
      return "Section has too many fields in formatted text file";  /**<Section has too many fields*/
   case ERR_COMMON_TXTFILE_SECTION_PROPERTY_UNKNOWN:
      return "Section has unknown property in formatted text file";  /**<Unknown property for section*/
   case ERR_COMMON_TXTFILE_SECTION_TYPE_UNKNOWN:
      return "Section type unknown in formatted text file";  /**<Type of section unknonwn*/
   case ERR_COMMON_TXTFILE_BODY_END_LINE_NOT_FOUND:
      return "End of body not found in formatted text file";  /**<End of body not found*/
   case ERR_COMMON_TXTFILE_BODY_DEFINITION_NOT_FOUND:
      return "Body definition not found in formatted text file";  /**<Body definition not found*/
   case ERR_COMMON_TXTFILE_FIELD_ALIGNMENT_NOT_RESPECTED:
      return "Field alignment not respected in formatted text file";  /**<Field alignment not respected*/
   case ERR_COMMON_TXTFILE_FIELD_NAME_DUPLICATED:
      return "Field name duplicated in formatted text file";  /**<Field name duplicated*/
   case ERR_COMMON_TXTFILE_FIELD_ENDING_NOT_FOUND:
      return "Field ending not found in formatted text file";  /**<Field ending not found*/
   case ERR_COMMON_TXTFILE_FIELD_PARSING_ERROR:
      return "Field parsing error in formatted text file";  /**<Field parsing error*/
   case ERR_COMMON_TXTFILE_FIELD_SEPARATOR_NOT_FOUND:
      return "Field separator not found in formatted text file";  /**<Field separator not found*/
   case ERR_COMMON_TXTFILE_FIELD_PREFIX_NOT_FOUND:
      return "Field prefix not found in formatted text file";  /**<Field prefix not found*/
   case ERR_COMMON_TXTFILE_FIELD_UNAUTHORISED:
      return "Unauthorized field in formatted text file";  /**<Field not authorised*/
   case ERR_COMMON_TXTFILE_FIELD_NAME_UNKNOWN:
      return "Field name unknown in formatted text file";  /**<Field name unknown*/
   case ERR_COMMON_TXTFILE_FIELD_IDENTIFIER_UNKNOWN:
      return "Field declaration identifier unknown in formatted text file";  /**<Field declaration identifier unknown*/
   case ERR_COMMON_TXTFILE_OPTIONAL_FIELDS_CONFUSION:
      return "Optional fields confusion in formatted text file";  /**<Optional fields can't be distinguished*/
   case ERR_COMMON_TXTFILE_NOT_PARSED:
      return "Text file not parsed";   /**<Text file has not been parsed*/
   case ERR_COMMON_TXTFILE_FIELD_NAME_MISSING:
      return "Missing field name";  /**<Missing field name*/
   case ERR_COMMON_TXTFILE_MISSING_MANDATORY_FIELD:
      return "Mandatory field missing";  /**<Mandatory field missing*/

   case WRN_COMMON_TXTFILE_HEADER_COMPLETED:
      return "Header of formatted text file is completed";  /**<Header completed*/
   case WRN_COMMON_TXTFILE_NO_SECTIONS_REMAINING:
      return "No section remaining in formatted text file";  /**<No section remaining*/
   case WRN_COMMON_TXTFILE_IGNORING_CHARACTERS:
      return "Ignoring characters in formatted text file";  /**<Ignoring characters*/

      /** Error codes for the LIBASM module **/
   case ERR_LIBASM_MISSING_ASMFILE:
      return "Missing structure representing the assembly file";  /**<Missing asmfile*/
   case ERR_LIBASM_ARCH_MISSING:
      return "Missing architecture";   /**<Missing architecture*/
   case ERR_LIBASM_ARCH_UNKNOWN:
      return "Unknown architecture";   /**<Unknown architecture*/
   case ERR_LIBASM_ADDRESS_INVALID:
      return "Invalid address";  /**<Invalid address*/
   case ERR_LIBASM_MISSING_PROJECT:
      return "Missing project"; /**<Missing project*/
   case ERR_LIBASM_UARCH_NAME_INVALID:
      return "Invalid micro-architecture name"; /**<Invalid micro-architecture name*/
   case ERR_LIBASM_PROC_NAME_INVALID:
      return "Invalid processor version name"; /**<Invalid processor version name*/

   case ERR_LIBASM_INSTRUCTION_NOT_FOUND:
      return "Instruction not found";  /**<Instruction not found*/
   case ERR_LIBASM_INSTRUCTION_NOT_BRANCH:
      return "Instruction is not a branch";  /**<Instruction is not a branch*/
   case ERR_LIBASM_INSTRUCTION_MISSING:
      return "Missing instruction";  /**<Missing instruction*/
   case ERR_LIBASM_INSTRUCTION_NOT_PARSED:
      return "Instruction could not be parsed";  /**<Instruction could not be parsed*/

   case ERR_LIBASM_OPERAND_NOT_FOUND:
      return "Operand not found";  /**<Operand not found*/
   case ERR_LIBASM_OPERAND_MISSING:
      return "Missing operand";  /**<Missing operand*/
   case ERR_LIBASM_OPERAND_NOT_REGISTER:
      return "Operand is not a register";  /**<Operand is not a register*/
   case ERR_LIBASM_OPERAND_NOT_MEMORY:
      return "Operand is not a memory address";  /**<Operand is not a memory address*/
   case ERR_LIBASM_OPERAND_NOT_IMMEDIATE:
      return "Operand is not an immediate value";  /**<Operand is not an immediate value*/
   case ERR_LIBASM_OPERAND_NOT_POINTER:
      return "Operand is not a pointer";  /**<Operand is not a pointer*/
   case ERR_LIBASM_OPERAND_NOT_PARSED:
      return "Operand could not be parsed";  /**<Operand could not be parsed*/
   case ERR_LIBASM_OPERAND_NOT_CREATED:
      return "Operand could not be created";  /**<Operand could not be created*/

   case ERR_LIBASM_FUNCTION_NOT_FOUND:
      return "Function not found";  /**<Function not found*/

   case ERR_LIBASM_INCORRECT_DATA_TYPE:
      return "Incorrect data type";  /**<Incorrect data type*/
   case ERR_LIBASM_DATA_MISSING:
      return "Missing data";  /**<Missing data*/
   case ERR_LIBASM_ERROR_RETRIEVING_DATA_BYTES:
      return "Unable to retrieve the bytes of a data entry";  /**<Unable to retrieve the bytes of a data entry*/

   case ERR_LIBASM_LABEL_MISSING:
      return "Label missing";  /**<Label missing*/

   case WRN_LIBASM_NO_DEBUG_DATA:
      return "File has no debug information";  /**<File has no debug information*/

   case WRN_LIBASM_BRANCH_OPPOSITE_COND:
      return "Opposite of the branch instruction is conditional";  /**<Opposite of the branch instruction is conditional*/
   case WRN_LIBASM_BRANCH_HAS_NO_OPPOSITE:
      return  "Branch instruction has no opposite"; /**<Branch instruction has no opposite*/

      /** Error codes for the BINPARSE module **/
   case ERR_BINARY_FORMAT_NOT_RECOGNIZED:
      return "Binary format not recognized"; /**<Binary format not recognised or not supported*/
   case ERR_BINARY_MISSING_BINFILE:
      return "Missing structure representing the binary file"; /**<Missing structure representing the binary file*/
   case ERR_BINARY_HEADER_NOT_FOUND:
      return "Binary file header not found"; /**<Binary file header not found*/
   case ERR_BINARY_ARCHIVE_PARSING_ERROR:
      return "Error when parsing archive file"; /**<Error when parsing archive file*/
   case ERR_BINARY_UNKNOWN_FILE_TYPE:
      return "Unknown file type for this binary format"; /**<Unknown file type for this binary format*/
   case ERR_BINARY_FILE_ALREADY_PARSED:
      return "Binary file has already been parsed";   /**<Binary file has already been parsed*/
   case ERR_BINARY_NO_EXTFCTS_SECTION:
      return "No section for external functions found";   /**<No section for external functions found*/
   case ERR_BINARY_NO_EXTLIBS:
      return "File has no external libraries";   /**<File has no external libraries*/
   case ERR_BINARY_NO_SECTIONS_FOUND:
      return "File has no sections";   /**<File has no sections*/
   case ERR_BINARY_SECTION_EMPTY:
      return "Section is empty";  /**<Section is empty*/
   case ERR_BINARY_SECTION_NOT_FOUND:
      return "Section was not found";  /**<Section was not found*/
   case ERR_BINARY_LIBRARY_TYPE_UNDEFINED:
      return "Library has an undefined type";  /**<Library has an undefined type*/
   case ERR_BINARY_NO_SYMBOL_SECTION:
      return "No section containing symbols was found";  /**<No section for symbols found*/
   case ERR_BINARY_MISSING_SECTION:
      return "Missing structure representing a section";  /**<Missing structure representing a section*/
   case ERR_BINARY_BAD_SECTION_TYPE:
      return "Section had an incorrect type for the required operation";  /**<Section had an incorrect type for the required operation*/
   case ERR_BINARY_BAD_SECTION_ENTRYSZ:
      return "Section had an incorrect entry size";  /**<Section had an incorrect entry size*/
   case ERR_BINARY_HEADER_ALREADY_PARSED:
      return "Header was already parsed";  /**<Header was already parsed*/
   case ERR_BINARY_SECTION_SEGMENT_NOT_FOUND:
      return "The section could not be associated to a segment";  /**<The section could not be associated to a segment*/
   case ERR_BINARY_UNEXPECTED_FILE_FORMAT:
      return "Unexpected file format";  /**<Unexpected file format*/
   case ERR_BINARY_NO_STRING_SECTION:
      return "No section containing strings found";  /**<No section containing strings found*/
   case ERR_BINARY_MISSING_SEGMENT:
      return "Missing structure representing a segment";  /**<Missing structure representing a segment*/

   case ERR_BINARY_SYMBOL_NOT_FOUND:
      return "Symbol was not found";  /**<Symbol was not found*/
   case ERR_BINARY_EXTFCT_NOT_FOUND:
      return "External function not found";  /**<External function not found*/
   case ERR_BINARY_TARGET_ADDRESS_NOT_FOUND:
      return "Targeted address not found in the file";  /**<Targeted address not found in the file*/
   case ERR_BINARY_EXTLIB_NOT_FOUND:
      return "External library not found";  /**<External library not found*/

   case ERR_BINARY_SECTIONS_NOT_REORDERED:
      return "Unable to reorder the sections in the binary file";  /**<Unable to reorder the sections in the binary file*/
   case ERR_BINARY_FILE_NOT_BEING_PATCHED:
      return "Binary file is not in the process of being patched";  /**<Binary file is not in the process of being patched*/
   case ERR_BINARY_SECTION_DATA_NOT_LOCAL:
      return "Data in the section not allocated locally";  /**<Data in the section not allocated locally*/
   case ERR_BINARY_FAILED_SAVING_DATA_TO_SECTION:
      return "Data entry could not be saved to section";  /**<Data entry could not be saved to section*/
   case ERR_BINARY_PATCHED_FILE_NOT_FINALISED:
      return "Binary file is being patched but not finalised";  /**<Binary file is being patched but not finalised*/
   case ERR_BINARY_PATCHED_SECTION_NOT_CREATED:
      return "Patched copy of the section could not be created";  /**<Patched copy of the section could not be created*/
   case ERR_BINARY_FAILED_INSERTING_STRING:
      return "String could not be inserted";  /**<String could not be inserted*/
   case ERR_BINARY_SECTION_NOT_RELOCATED:
      return "Section could not be relocated";  /**<Section could not be relocated*/
   case ERR_BINARY_SECTION_ALREADY_EXISTING:
      return "Attempted to updated or create an already existing section";  /**<Attempted to updated or create an already existing section*/

   case ERR_BINARY_UNABLE_TO_CREATE_FILE:
      return "Unable to initialise binary file for writing";  /**<Unable to initialise binary file for writing*/
   case ERR_BINARY_UNABLE_TO_WRITE_FILE:
      return "Unable to write binary file";  /**<Unable to write binary file*/

   case ERR_BINARY_RELOCATION_NOT_SUPPORTED:
      return "Relocation type not supported";  /**<Relocation type not supported*/
   case ERR_BINARY_RELOCATION_NOT_RECOGNISED:
      return "Relocation type not recognised";  /**<Relocation type not recognised*/
   case ERR_BINARY_RELOCATION_INVALID:
      return "Invalid relocation type";  /**<Invalid relocation type*/
   case ERR_BINARY_BAD_RELOCATION_ADDRESS:
      return "Invalid relocation address";  /**<Invalid relocation address*/

   case ERR_BINARY_UNKNOWN_DEBUG_FORMAT:
      return "Unknown or unsupported debug format";  /**<Unknown or unsupported debug format*/

      /** Error codes for the ASMBL module **/
   case ERR_ASMBL_ARCH_NOT_SUPPORTED:
      return "Architecture not supported for assembly";  /**<Architecture not supported for assembly*/

   case ERR_ASMBL_INSTRUCTION_NOT_ASSEMBLED:
      return "Instruction could not be assembled";  /**<Instruction could not be assembled*/
   case ERR_ASMBL_INSTRUCTION_HAS_CODING:
      return "Instruction already has a coding";  /**<Instruction already has a coding*/
   case ERR_ASMBL_CODING_HAS_DIFFERENT_LENGTH:
      return "New coding of instruction has a different length";  /**<New coding of instruction has a different length*/

      /** Error codes for the DISASS module **/
   case ERR_DISASS_FILE_NOT_PARSED:
      return "File has not been parsed";  /**<File has not been parsed*/
   case ERR_DISASS_STREAM_EMPTY:
      return "Stream to disassemble is empty";  /**<Stream to disassemble is empty*/
   case ERR_DISASS_ARCH_NOT_SUPPORTED:
      return "Architecture is not supported";  /**<Architecture not supported for disassembly*/
   case ERR_DISASS_FILE_PARSING_FAILED:
      return "File parsing failed";  /**<File parsing failed*/
   case ERR_DISASS_FILE_DISASSEMBLY_FAILED:
      return "File disassembly failed";  /**<File disassembly failed*/

   case ERR_DISASS_FSM_NO_MATCH_FOUND:
      return "Parser failed to find a match";   /**<Parser failed to find a match*/
   case ERR_DISASS_FSM_END_OF_STREAM_REACHED:
      return "End of stream reached during parsing";  /**<End of stream reached during parsing*/

   case WRN_DISASS_EXT_FCTS_LBLS_NOT_RETRIEVED:
      return "Unable to retrieve labels for external functions";  /**<Unable to retrieve labels for external functions*/
   case WRN_DISASS_DBG_LBLS_NOT_RETRIEVED:
      return "Unable to retrieve debug labels";  /**<Unable to retrieve debug labels*/

   case WRN_DISASS_FSM_RESET_ADDRESS_OUT_OF_RANGE:
      return "Reset of the parser was requested to an out of range address";  /**<Requested parser reset to an out of range address*/
   case WRN_DISASS_FSM_RESET_ADDRESS_PARSING_IN_PROGRESS:
      return "Reset of the parser was requested while parsing was in progress";  /**<Requested parser reset while parsing in progress*/

   case WRN_DISASS_INCOMPLETE_DISASSEMBLY:
      return "Disassembly is incomplete";  /**<Disassembly is incomplete*/

      /** Error codes for the PATCH module **/
   case ERR_PATCH_ARCH_NOT_SUPPORTED:
      return "Architecture not supported for patching";  /**<Architecture not supported for patching*/
   case ERR_PATCH_NOT_INITIALISED:
      return "Patcher not initialised";  /**<Patcher not initialised*/
   case ERR_PATCH_MISSING_MODIF_STRUCTURE:
      return "Missing modification structure";  /**<Missing modification structure*/
   case ERR_PATCH_WRONG_MODIF_TYPE:
      return "Wrong type of modification request";  /**<Wrong type of modification request*/
   case ERR_PATCH_INSERT_LIST_EMPTY:
      return "List of instructions to insert is empty";  /**<List of instructions to insert is empty*/
   case ERR_PATCH_MISSING_MODIF_ADDRESS:
      return "Modification has no address";  /**<Modification has no address*/
   case ERR_PATCH_FLOATING_MODIF_NO_SUCCESSOR:
      return "Floating modification has no successor";  /**<Floating modification has no successor*/
   case ERR_PATCH_FILE_NOT_FINALISED:
      return "Patched file is not finalised";  /**<Patched file is not finalised*/
   case ERR_PATCH_ADDRESS_LIST_ALREADY_CREATED:
      return "List of addresses has already been initialised";  /**<List of addresses has already been initialised*/
   case ERR_PATCH_MODIF_NOT_FINALISED:
      return "Modification is not finalised";  /**<Modification is not finalised*/

   case ERR_PATCH_EXTFCT_STUB_NOT_GENERATED:
      return "Stub for external function could not be generated";  /**<Stub for external function could not be generated*/
   case ERR_PATCH_LABEL_INSERT_FAILURE:
      return "Label insertion failed";  /**<Label insertion failed*/
   case ERR_PATCH_RELOCATION_NOT_ADDED:
      return "Relocation not added to the binary file";  /**<Relocation not added to the binary file*/
   case ERR_PATCH_FUNCTION_NOT_INSERTED:
      return "Function could not be inserted to the file";  /**<Function could not be inserted to the file*/
   case ERR_PATCH_FUNCTION_CALL_NOT_GENERATED:
      return "Code for the function call could not be generated";  /**<Code for the function call could not be generated*/

   case ERR_PATCH_PADDING_INSN_TOO_BIG:
      return "Padding instruction is larger than the default one";  /**<Padding instruction is larger than the default one*/

   case ERR_PATCH_CONDITION_ARGUMENTS_MISMATCH:
      return "Mismatch between the condition arguments and type";  /**<Mismatch between the condition arguments and type*/
   case ERR_PATCH_CONDITION_TYPE_UNKNOWN:
      return "Condition type unknown";  /**<Condition type unknown*/
   case ERR_PATCH_CONDITION_PARSE_ERROR:
      return "Error when parsing a condition";  /**<Error when parsing a condition*/
   case ERR_PATCH_CONDITION_MISSING:
      return "Expected condition was missing";  /**<Expected condition was missing*/
   case ERR_PATCH_CONDITION_UNSUPPORTED_MODIF_TYPE:
      return "Condition not supported for this type of modification";  /**<Condition not supported for this type of modification*/

   case ERR_PATCH_REFERENCED_GLOBVAR_MISSING:
      return "Global variable to be referenced by an instruction is missing";  /**<Global variable to be referenced by an instruction is missing*/
   case ERR_PATCH_GLOBVAR_MISSING:
      return "Missing global variable";  /**<Missing global variable*/
   case ERR_PATCH_NO_SPACE_FOUND_FOR_GLOBVAR:
      return "No space found for inserting a global variable";  /**<No space found for inserting a global variable*/

   case ERR_PATCH_BASIC_BLOCK_NOT_FOUND:
      return "Unable to find a basic block around a given instruction";  /**<Unable to find a basic block around a given instruction*/
   case ERR_PATCH_INSERT_INSNLIST_FAILED:
      return "Unable to insert list of instructions";  /**<Unable to insert list of instructions*/
   case ERR_PATCH_INSUFFICIENT_SIZE_FOR_INSERT:
      return "Insufficient size for insertion of instructions";  /**<Insufficient size for insertion of instructions*/
   case ERR_PATCH_UNABLE_TO_MOVE_TRAMPOLINE:
      return "Unable to move trampoline block";  /**<Unable to move trampoline block*/
   case ERR_PATCH_UNABLE_TO_CREATE_TRAMPOLINE:
      return "Unable to insert trampoline rebound";  /**<Unable to insert trampoline rebound*/
   case ERR_PATCH_UNRESOLVED_SYMBOL:
      return "Symbol not found in the inserted libraries";  /**<Symbol not found in the inserted libraries*/
   case ERR_PATCH_NO_SPACE_FOUND_FOR_BLOCK:
      return "No space found for moving a block";  /**<No space found for moving a block*/
   case ERR_PATCH_NO_SPACE_FOUND_FOR_SECTION:
      return "No space found for moving a section";  /**<No space found for moving a section*/

   case WRN_PATCH_SIZE_TOO_SMALL_FORCED_INSERT:
      return "Insertion was forced while insertion site was too small";   /**<Insertion was forced while insertion site was too small*/
   case WRN_PATCH_FUNCTION_MOVED:
      return "A function was moved to perform the insertion";  /**<A function was moved to perform the insertion*/
   case WRN_PATCH_MOVED_FUNCTION_HAS_INDIRECT_BRCH:
      return "A function containing an indirect branch was moved";  /**<A function containing an indirect branch was moved*/

   case WRN_PATCH_SYMBOL_ADDED_AS_EXTERNAL:
      return "Undefined symbol was added as an external";  /**<Undefined symbol was added as an external*/

   case WRN_PATCH_MODIF_NOT_PROCESSED:
      return "Modification has not been processed and will not be applied";  /**<Modification has not been processed and will not be applied*/

   case WRN_PATCH_NO_PENDING_MODIFS:
      return "Attempted to finalise patching session with no modification pending";  /**<Attempted to finalise patching session with no modification pending*/

   case WRN_PATCH_FILE_SAVED_WITH_DEFAULT_NAME:
      return "File was saved with a default name as the given name was invalid";  /**<File was saved with a default name as the given name was invalid*/

      /** Error codes for the MADRAS module **/
   case ERR_MADRAS_MISSING_MADRAS_STRUCTURE:
      return "Missing MADRAS structure";  /**<Missing MADRAS structure*/
   case ERR_MADRAS_MODIF_TYPE_NOT_SUPPORTED:
      return "Modification type not supported";  /**<Modification type not supported*/

   case ERR_MADRAS_MISSING_CURSOR:
      return "Missing MADRAS cursor";  /**<Missing MADRAS cursor*/
   case ERR_MADRAS_CURSOR_NOT_ALIGNED:
      return "Unable to align cursor";  /**<Unable to align cursor*/
   case ERR_MADRAS_MISSING_GLOBVAR:
      return "Missing global variable";  /**<Missing global variable*/
   case ERR_MADRAS_MODIF_COND_MISSING:
      return "Modification has no condition";  /**<Modification has no condition*/
   case ERR_MADRAS_MODIF_ALREADY_HAS_ELSE:
      return "Modification already has \"else\" code";  /**<Modification already has "else" code*/
   case ERR_MADRAS_ELSE_MODIF_IS_FIXED:
      return "\"Else\" modification has a fixed address";  /**<"Else" modification has a fixed address*/
   case ERR_MADRAS_MODIF_HAS_CUSTOM_PADDING:
      return "Modification already has custom padding instruction";  /**<Modification already has custom padding instruction*/
   case ERR_MADRAS_MODIF_ADD_COND_FAILED:
      return "Unable to add condition to modification";  /**<Unable to add condition to modification*/

   case ERR_MADRAS_ADD_LIBRARY_FAILED:
      return "Unable to add request for library insertion";  /**<Unable to add request for library insertion*/
   case ERR_MADRAS_ADDRESSES_NOT_TRACKED:
      return "Addresses were not tracked";  /**<Addresses were not tracked*/
   case ERR_MADRAS_MODIF_LABEL_FAILED:
      return "Unable to add request for label modification";  /**<Unable to add request for label modification*/
   case ERR_MADRAS_MODIF_CODE_FAILED:
      return "Unable to add request for code modification";  /**<Unable to add request for code modification*/
   case ERR_MADRAS_MODIF_LIBRARY_FAILED:
      return "Unable to add request for library modification";  /**<Unable to add request for library modification*/
   case ERR_MADRAS_MODIF_VARIABLE_FAILED:
      return "Unable to add request for variable modification";  /**<Unable to add request for variable modification*/
   case ERR_MADRAS_RENAMING_LIBRARY_EXISTING:
      return "A request for renaming the library already exists";  /**<A request for renaming the library already exists*/

   case WRN_MADRAS_MODIFS_ALREADY_INIT:
      return "Modifications were already initiated for this file";  /**<Modifications were already initiated for this file*/

   case WRN_MADRAS_STACK_SHIFT_NULL:
      return "Stack shift requested for patching with a null shift value";  /**<Stack shift requested for patching with a null shift value*/

   case WRN_MADRAS_NEWNAME_IDENTICAL:
      return "Requested renaming of element to an identical name";  /**<Requested renaming of element to an identical name*/

      /** Error codes for the Lua execution**/
   case ERR_LUAEXE_MISSING_LUA_STATE:
      return "Missing Lua state";  /**<Missing Lua state*/
   case ERR_LUAEXE_MISSING_LUA_CHUNK:
      return "Missing Lua chunk to execute";   /**<Missing Lua chunk to execute*/
   case ERR_LUAEXE_PRECOMP_SYNTAX_ERROR:
      return "Syntax error during pre-compilation";   /**<Syntax error during pre-compilation*/
   case ERR_LUAEXE_PRECOMP_MEMORY_ALLOCATION:
      return "Memory allocation error during pre-compilation";   /**<Memory allocation error during pre-compilation*/
   case ERR_LUAEXE_RUNTIME_ERROR:
      return "Lua runtime error";   /**<Lua runtime error*/
   case ERR_LUAEXE_UNKNOWN_RUNTIME_ERROR:
      return "Unspecified Lua runtime error";   /**<Unspecified Lua runtime error*/
   case ERR_LUAEXE_MEMORY_ALLOCATION:
      return "Memory allocation error";   /**<Memory allocation error*/
   case ERR_LUAEXE_ERROR_HANDLER:
      return "Error while running the error handler";   /**<Error while running the error handler*/

      /** Error codes for MAQAO API*/
   case ERR_MAQAO_UNABLE_TO_DETECT_PROC_HOST:
      return "Unable to retrieve processor version of the host";   /**<Unable to retrieve processor version of the host*/
   case ERR_MAQAO_MISSING_UARCH_OR_PROC:
      return "Missing micro architecture or processor version";   /**<Missing micro architecture or processor version*/


   default:
      return "Unknown error code";
   }

}

/*
 * Prints the full error message associated with an error code. This overrides the current level of verbosity
 * \param errcode The error code
 * */
void errcode_printfullmsg(const int errcode) {
   /**\todo (2016-03-09) Handle the case where errcode == EXIT_SUCCESS*/
   int errlvl = ERRORCODE_GET_LEVEL(errcode);
   char* modulename = errcode_getmodulename(errcode);
   char* errmsg = errcode_getmsg(errcode);
   uint8_t current_verbose_level = __MAQAO_VERBOSE_LEVEL__;
   __MAQAO_VERBOSE_LEVEL__ = MAQAO_VERBOSE_ALL;
   switch(errlvl) {
   case ERRLVL_WRN:
      WRNMSG("[%s] %s (%x)\n", modulename, errmsg, errcode);
      break;
   case ERRLVL_ERR:
      ERRMSG("[%s] %s (%x)\n", modulename, errmsg, errcode);
      break;
   case ERRLVL_CRI:
      HLTMSG("[%s] %s (%x)\n", modulename, errmsg, errcode);
      break;
   default:
      STDMSG("[%s] %s (%x)\n", modulename, errmsg, errcode);
      break;
   }
   __MAQAO_VERBOSE_LEVEL__ = current_verbose_level;
}
