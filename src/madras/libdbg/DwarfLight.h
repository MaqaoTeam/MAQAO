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

#ifndef __DWARF_LIGHT_H__
#define __DWARF_LIGHT_H__

#include "libelf.h"
#include "libdwarf.h"

/* -------------------------- Types definition ----------------------------- */
typedef unsigned long Dwarf_Word;
typedef struct _DwarfAPI DwarfAPI;
typedef struct _DwarfFile DwarfFile;
typedef struct _DwarfFunction DwarfFunction;
typedef struct _DwarfInlinedFunction DwarfInlinedFunction;
typedef struct _DwarfLine DwarfLine;
typedef struct _DwarfVar DwarfVar;
typedef struct _DwarfStruct DwarfStruct;
typedef struct _DwarfMemLoc DwarfMemLoc;
typedef enum _DwarfMemLocType DwarfMemLocType;

/* ------------------------ DwarfAPI functions ----------------------------- */
/**
 * Initializes the Dwarf API
 * \param api The API object
 * \param elf_name
 * \param asmf corresponding assembly file or NULL
 * \return The API object
 */
extern DwarfAPI* dwarf_api_init_light(Elf* elf, char* elf_name, asmfile_t* asmf);

/**
 * Frees a Dwarf API structure
 * \param api a Dwarf API created with dwarf_api_init_light
 */
extern void dwarf_api_close_light(DwarfAPI* api);

/**
 * Retrieves all addresses, filenames and source lines from Dwarf
 * \param api The API object
 * \param filename used to return an array of source file name
 * \param addrs used to return an array of addresses
 * \param srcs used to return an array of source lines
 * \param size used to return the size of each array
 */
extern void dwarf_api_get_all_lines(DwarfAPI *api, char ***filename,
      maddr_t** addrs, int ** srcs, unsigned int* size);

/**
 * Retrieve all compile units in a queue of DwarfFile*
 * \param api A DwarfAPI object
 * \return A queue of DwarfFile pointer
 */
extern queue_t* dwarf_api_get_files(DwarfAPI *api);

/**
 * Retrieve all functions. The retrieved queue should be freed manually
 * \param api A DwarfAPI object
 * \return A queue of DwarfFunction pointer
 */
extern queue_t* dwarf_api_get_functions(DwarfAPI *api);

/**
 * Retrieve the ranges of addresses containing debug information, based on the ranges of dwarf functions.
 * Ranges are sorted by starting address and do not overlap
 * \param api A DwarfAPI object
 * \param starts_ranges Return parameter. Will contain the array of ranges starts
 * \param stops_ranges Return parameter. Will contain the array of ranges stops
 * \return The size of the returned arrays. If either parameter is NULL, 0
 */
extern unsigned int dwarf_api_get_debug_ranges(DwarfAPI *api, maddr_t** starts_ranges, maddr_t** stops_ranges);

/**
 * Retrieves a function corresponding to an address
 * \param api A DwarfAPI object
 * \param low_pc the first instruction address of the function
 * \return A DwarfFunction pointer, or NULL if not found
 */
extern DwarfFunction* dwarf_api_get_function_by_addr(DwarfAPI *api,
      Dwarf_Addr low_pc);

/**
 * Retrieves a function belonging to an interval
 * \param api A DwarfAPI object
 * \param low_pc the first instruction address where to start to look for the function
 * \param high_pc the last instruction address where to stop to look for the function
 * \return A DwarfFunction pointer, or NULL if not found
 */
extern DwarfFunction* dwarf_api_get_function_by_interval(DwarfAPI *api,
      Dwarf_Addr low_pc, Dwarf_Addr high_pc);

/**
 * Retrieve a function corresponding to instruction debug data
 * \param api A DwarfAPI object
 * \param name name of the source file containing the function
 * \param srcl instruction source line
 * \return A DwarfFunction pointer, or NULL if not found
 */
extern DwarfFunction* dwarf_api_get_function_by_src(DwarfAPI *api,
      const char* name, int srcl);

/**
 * Retrieve a function corresponding to a given link name.
 * \param api A DwarfAPI object
 * \param linkname The name of the function as it appears in the binary (mangled name)
 * \return A DwarfFunction pointer, or NULL if not found
 */
extern DwarfFunction* dwarf_api_get_function_by_linkname(DwarfAPI *api, char* linkname);

/**
 * Set the asmfile associated to Dwarf data
 * \param api a Dwarf API created with dwarf_api_init_light
 * \param asmf an asmfile
 */
extern void dwarf_api_set_asmfile(DwarfAPI* api, asmfile_t* asmf);

/* ----------------------- DwarfFile functions ----------------------------- */
/**
 * Retrieves the filename
 * \param file the DwarfFile object
 * \return A string containing the name
 */
extern char* dwarf_file_get_name(DwarfFile *file);

/**
 * Retrieves the directory where the file operates
 * \param file the DwarfFile object
 * \return A string containing the directory
 */
extern char* dwarf_file_get_dir(DwarfFile *file);

/**
 * Retrieves the directory where the file operates
 * \param file the DwarfFile object
 * \return A string containing the directory
 */
extern char* dwarf_file_get_vendor(DwarfFile *file);

/**
 * Retrieves the directory where the file operates
 * \param file the DwarfFile object
 * \return A string containing the directory
 */
extern char* dwarf_file_get_version(DwarfFile *file);

/**
 * Retrieves the source language of the file
 * \param file the DwarfFile object
 * \return A string containing a source language
 */
extern char* dwarf_file_get_language(DwarfFile *file);

/**
 * Retrieves the full producer string (compiler, version ...)
 * \param file the DwarfFile object
 * \return A string containing the producer
 */
extern char* dwarf_file_get_producer(DwarfFile *file);

/**
 * Retrieves the compiler code of the file. Codes are described
 * in libmasm.h header file
 * \param file the DwarfFile object
 * \return A code containing a compiler
 */
extern int dwarf_file_get_producer_code(DwarfFile *file);

/**
 * Retrieves the source language code of the file. Codes are described
 * in libmasm.h header file
 * \param file the DwarfFile object
 * \return A code containing a source language
 */
extern int dwarf_file_get_language_code(DwarfFile *file);

/**
 * Retrieves a function corresponding to an address
 * \param file A DwarfFile object
 * \param low_pc the first instruction address of the function
 * \return A DwarfFunction pointer
 */
extern DwarfFunction* dwarf_file_get_function_by_addr(DwarfFile *file,
      Dwarf_Addr low_pc);

/**
 * Retrieves a function belonging to an interval
 * \param file A DwarfFile object
 * \param low_pc the first instruction address of the interval
 * \param high_pc the last instruction address of the interval
 * \return A DwarfFunction pointer
 */
extern DwarfFunction* dwarf_file_get_function_by_interval(DwarfFile *file,
      Dwarf_Addr low_pc, Dwarf_Addr high_pc);

/**
 * Retrieves global variables of a file from Dwarf
 * \param api A DwarfAPI object
 * \param file a file
 * \return a queue of DwarfVar structures or NULL if there is a problem
 */
extern queue_t* dwarf_file_get_global_variables(DwarfFile *file);

/**
 * Retrieves a function corresponding to instruction debug data
 * \param file A DwarfFile object
 * \param srcl instruction source line
 * \return A DwarfFunction pointer, or NULL if not found
 */
extern DwarfFunction* dwarf_file_get_function_by_src(DwarfFile *file, int srcl);

/**
 * Adds a range in a function
 * \param func a function
 * \param start first instruction
 * \param stop last instruction
 */
extern void dwarf_function_add_range(DwarfFunction *func, insn_t* start,
      insn_t* stop);

/* ------------------- DwarfFunction functions ----------------------------- */
/**
 * Retrieves the file containing the function
 * \param function A DwarfFunction object
 * \return A DwarfFile pointer, should never fail
 */
extern DwarfFile* dwarf_function_get_file(DwarfFunction *function);

/**
 * Retrieves the name of the function
 * \param function A DwarfFunction object
 * \return A string containing the name of the function, or NULL if unavailable
 */
extern char* dwarf_function_get_name(DwarfFunction *function);

/**
 * Retrieves parameters of a function from Dwarf
 * \param func a function
 * \return a queue of DwarfVar structures or NULL if there is a problem
 */
extern queue_t* dwarf_function_get_parameters(DwarfFunction *func);

/**
 * Retrieves local variables of a function from Dwarf
 * \param api A DwarfAPI object
 * \param func a function
 * \return a queue of DwarfVar structures or NULL if there is a problem
 */
extern queue_t* dwarf_function_get_local_variables(DwarfFunction *func);

/**
 * Retrieves the function return variable
 * \param func a function
 * \return a DwarfVar variable or NULL if there is a problem
 */
extern DwarfVar* dwarf_function_get_returned_var(DwarfFunction *func);

/**
 * Retrieves a list of subfunctions (parallel regions / loops)
 * \param func a function
 * \return a list of sub functions (DwarfFunction* structures) or NULL if there is a problem
 */
extern queue_t* dwarf_function_get_subfunctions(DwarfFunction *func);

/**
 * Retrieves a low-pc (low Program Counter)
 * \param func a function
 * \return the low-pc
 */
extern int64_t dwarf_function_get_lowpc(DwarfFunction *func);

/**
 * Retrieves a high-pc (high Program Counter)
 * \param func a function
 * \return the high-pc
 */
extern int64_t dwarf_function_get_highpc(DwarfFunction *func);

/**
 * Retrieves the source line declaration
 * \param func a function
 * \return the source line declaration
 */
extern int dwarf_function_get_srcl(DwarfFunction *func);

/**
 * Retrieves the source file
 * \param func a function
 * \return the source file
 */
extern char* dwarf_function_get_decl_file(DwarfFunction *func);

/**
 * Retrieves functions inlined in a function
 * \param func a function
 * \return the list of inlined function in func
 */
extern queue_t* dwarf_function_get_inlined(DwarfFunction *func);

/**
 * Gets the ranges of a function
 * \param func a function
 * \return a list of ranges
 */
extern queue_t* dwarf_function_get_ranges(DwarfFunction *func);

/**
 * Set the options used on command line to compile the file
 * \param file A DwarfFile object
 * \param opts the options
 */
extern void dwarf_file_set_command_line_opts(DwarfFile* file, const char* opts);

/**
 * Get the options used on command line to compile the file
 * \param file A DwarfFile object
 * \returns the options used to compile the file
 */
extern char* dwarf_file_get_command_line_opts(DwarfFile* file);

/* ------------------- DwarfInlinedFunction functions ---------------------- */
/**
 * Retrieves the function the inlined function is extracted from
 * \param ifunc an inlined function
 * \return the function the inlined function is extracted from
 */
extern DwarfFunction* dwarf_inlinedFunction_get_origin_function(
      DwarfInlinedFunction* ifunc);

/**
 * Retrieves the source line the inline function is called
 * \param ifunc an inlined function
 * \return the source line where the inline function is called
 */
extern int dwarf_inlinedFunction_get_call_line(DwarfInlinedFunction* ifunc);

/**
 * Retrieves the source column the inline function is called
 * \param ifunc an inlined function
 * \return the source column where the inline function is called
 */
extern int dwarf_inlinedFunction_get_call_column(DwarfInlinedFunction* ifunc);

/**
 * Retrieves the address the inline function begins
 * \param ifunc an inlined function
 * \return the address where the inline function begins
 */
extern int64_t dwarf_inlinedFunction_get_low_pc(DwarfInlinedFunction* ifunc);

/**
 * Retrieves the address the inline function stops
 * \param ifunc an inlined function
 * \return the address where the inline function stops
 */
extern int64_t dwarf_inlinedFunction_get_high_pc(DwarfInlinedFunction* ifunc);

/**
 * Retrieves an array of ranges extracted from DWARF
 * \param ifunc an inlined function
 * \param size if not NULL, used to return the number of ranges
 * \return an array of ranges extracted from DWARF
 */
extern Dwarf_Ranges* dwarf_inlinedFunction_get_ranges(
      DwarfInlinedFunction* ifunc, int* size);

/**
 * Retrieve the name of the inlined function                                    
 * \param function A DwarfInlinedFunction object                                
 * \return A string containing the name of the inlined function, or NULL if unavailable
 */
extern char* dwarf_inlinedFunction_get_name(DwarfInlinedFunction *ifunc);

/* ------------------------- DwarfVar functions ---------------------------- */
/**
 * Retrieves the variable name
 * \param var a variable
 * \return the variable name or NULL if there is a problem
 */
extern char* dwarf_var_get_name(DwarfVar* var);

/**
 * Retrieves the variable type (without const, static ...)
 * \param var a variable
 * \return the variable type or NULL if there is a problem
 */
extern char* dwarf_var_get_type(DwarfVar* var);

/**
 * Retrieves the variable full type (for example const char**)
 * \param var a variable
 * \return the variable full type or NULL if there is a problem
 */
extern char* dwarf_var_get_full_type(DwarfVar* var);

/**
 * Retrieves the function a variable belongs to
 * \param var a variable
 * \return the function a variable belongs to or NULL if there is a problem
 */
extern DwarfFunction* dwarf_var_get_function(DwarfVar* var);

/**
 * Retrieves the variable structure if it is not a native type
 * \param var a variable
 * \return the variable structure or NULL if there is a problem
 */
extern DwarfStruct* dwarf_var_get_structure(DwarfVar* var);

/**
 * Retrieves the position of the member in the structure
 * \param var a variable
 * \return the position of the member in the structure
 */
extern int dwarf_var_get_position_in_structure(DwarfVar* var);

/**
 * Retrieves the location of a variable
 * \param var a variable
 * \return the location of the variable else NULL
 */
extern DwarfMemLoc* dwarf_var_get_location(DwarfVar* var);

/**
 * Retrieves the source line of a variable
 * \param var a variable
 * \return the source line of the variable else -1
 */
extern int dwarf_var_get_source_line(DwarfVar* var);

/**
 * Retrieves the source column of a variable
 * \param var a variable
 * \return the source column of the variable else -1
 */
extern int dwarf_var_get_source_column(DwarfVar* var);

/**
 * Get the number of pointers.
 * \param var a variable
 * \return 1 if the variable is static, else 0
 */
extern int dwarf_var_get_pointer_number(DwarfVar* var);

/**
 * Checks if the variable is a constant
 * \param var a variable
 * \return 1 if the variable is constant, else 0
 */
extern int dwarf_var_is_const(DwarfVar* var);

/**
 * Checks if the variable is static
 * \param var a variable
 * \return 1 if the variable is static, else 0
 */
extern int dwarf_var_is_static(DwarfVar* var);

/* ------------------------ DwarfStruct functions -------------------------- */
//Values for a DwarfStruct type
#define DS_NOTYPE		0x0  	/**< DwarfStruct has no type (error)*/
#define DS_UNION		0x1  	/**< DwarfStruct is an union*/
#define DS_STRUCT		0x2  	/**< DwarfStruct is a structure*/
#define DS_TYPEDEF      0x3     /**< DwarfStruct is a typedef*/

/*
 * Retrieves the name of a structure
 * \param struc a structure
 * \return the structure name or NULL if there is a problem
 */
extern char* dwarf_struct_get_name(DwarfStruct* struc);

/**
 * Retrieves the size of a structure
 * \param struc a structure
 * \return the structure size or 0 if there is a problem
 */
extern int dwarf_struct_get_size(DwarfStruct* struc);

/**
 * Retrieves the members of a structure. Each member is a DwarfVar.
 * \param struc a structure
 * \return the queue of structure members or NULL if there is a problem
 */
extern queue_t* dwarf_struct_get_members(DwarfStruct* struc);

/**
 * Retrieves the type of a structure
 * \param struc a structure
 * \return the structure type or DS_NOTYPE if there is a problem
 */
extern char dwarf_struct_get_type(DwarfStruct* struc);

/**
 * Checks if the structure is an union
 * \param struc a structure
 * \return TRUE if the structure is an union else FALSE
 */
extern int dwarf_struct_is_union(DwarfStruct* struc);

/**
 * Checks if the structure is an struct
 * \param struc a structure
 * \return TRUE if the structure is an struct else FALSE
 */
extern int dwarf_struct_is_struct(DwarfStruct* struc);

/* ------------------------ DwarfMemLoc functions -------------------------- */
//@const_start
/**
 *	Enumeration of different kind of registers
 */
enum _DwarfMemLocType {
   DWARF_REG, /**< The value is directly in a register*/
   DWARF_BREG, /**< The value is at the address contained in reg + offset*/
   DWARF_FBREG, /**< The value is at the address contained in reg + offset*/
   DWARF_ADDR, /**< The value is at a fixed address*/
   DWARF_BREGX, DWARF_NONE, DWARF_FBREG_TBRES
};
//@const_stop

/**
 * Sets a register in a memory location
 * \param memloc a memory location
 * \param reg a register
 */
extern void dwarf_memloc_set_reg(DwarfMemLoc* memloc, reg_t* reg);

/**
 * Retrieves the type of a memory location
 * \param memloc a memory location
 * \return the type of a memory location or 0
 */
extern int dwarf_memloc_get_type(DwarfMemLoc* memloc);

/**
 * Retrieves the register of a memory location
 * \param memloc a memory location
 * \return the register of a memory location or NULL
 */
extern reg_t* dwarf_memloc_get_register(DwarfMemLoc* memloc);

/**
 * Retrieves the Dwarf index used to represent the register
 * \param memloc a memory location
 * \return the Dwarf index used to represent the register or -1
 */
extern int dwarf_memloc_get_register_index(DwarfMemLoc* memloc);

/**
 * Retrieves the offset member of a memory location
 * \param memloc a memory location
 * \return the offset member of a memory location or 0
 */
extern int dwarf_memloc_get_offset(DwarfMemLoc* memloc);

/**
 * Retrieves the address member of a memory location
 * \param memloc a memory location
 * \return the address member of a memory location or 0
 */
extern int64_t dwarf_memloc_get_address(DwarfMemLoc* memloc);

/**
 * Uses debug data to find ranges (can be used to detect inlining)
 * \param asmf an assembly file containing debug data
 */
extern void asmfile_detect_debug_ranges(asmfile_t* asmf);

#endif
