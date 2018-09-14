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

#ifndef LIBMWORMS_H__
#define LIBMWORMS_H__

#include <stdint.h>

#include "libmcommon.h"
#include "libmasm.h"
#include "loader.h"
#include "fat.h"
#include "nlist.h"

/**
 * Alias for struct macho_data_s structure
 */
typedef struct macho_file_s macho_file_t;

/**
 * Alias for struct binseg_s structure
 */
typedef struct macho_segment_s macho_segment_t;

//TODO Move this in a wrapping header ?
/**
 * Aliases for files' headers
 */
typedef struct mach_header                macho_header32_t;
typedef struct mach_header_64             macho_header64_t;

/**
 * Aliases for all commands
 */
typedef struct load_command               load_command_t;
typedef struct segment_command            segment_command32_t;
typedef struct segment_command_64         segment_command64_t;
typedef struct symtab_command             symtab_command_t;
typedef struct symseg_command             symseg_command_t;
typedef struct thread_command             thread_command_t;
typedef struct fvmlib_command             fvmlib_command_t;
typedef struct ident_command              ident_command_t;
typedef struct fvmfile_command            fvmfile_command_t;
typedef struct dysymtab_command           dysymtab_command_t;
typedef struct dylib_command              dylib_command_t;
typedef struct dylinker_command           dylinker_command_t;
typedef struct prebound_dylib_command     prebound_dylib_command_t;
typedef struct routines_command           routines_command32_t;
typedef struct routines_command_64        routines_command64_t;
typedef struct sub_framework_command      sub_framework_command_t;
typedef struct sub_umbrella_command       sub_umbrella_command_t;
typedef struct sub_client_command         sub_client_command_t;
typedef struct sub_library_command        sub_library_command_t;
typedef struct twolevel_hints_command     twolevel_hints_command_t;
typedef struct prebind_cksum_command      prebind_cksum_command_t;
typedef struct uuid_command               uuid_command_t;
typedef struct rpath_command              rpath_command_t;
typedef struct dyld_info_command          dyld_info_command_t;
typedef struct version_min_command        version_min_command_t;
typedef struct entry_point_command        entry_point_command_t;
typedef struct source_version_command     source_version_command_t;
typedef struct linkedit_data_command      linkedit_data_command_t;
typedef struct encryption_info_command    encryption_info_command32_t;
typedef struct encryption_info_command_64 encryption_info_command64_t;
typedef struct unknown_command_s          unknown_command_t;

/**
 * Aliases for commands' sub elements
 */
typedef struct section                    macho_section32_t;
typedef struct section_64                 macho_section64_t;
typedef struct nlist                      nlist32_t;
typedef struct nlist_64                   nlist64_t;
typedef struct dylib_table_of_contents    dylib_table_of_contents_t;
typedef struct dylib_module32             dylib_module32_t;
typedef struct dylib_module_64            dylib_module64_t;
typedef struct dylib_reference            dylib_reference_t;

typedef struct data_chunk_s               data_chunk_t;

typedef struct bind_s                     bind_t;

/**
 * Suffix of external labels
 * */
#define EXT_LBL_SUFFIX "@ext"

/**
 * Name of the default added segment
 */
#define MADRAS_SEGMENT "__MADRAS"

/**
 * Name of fake label section used for function
 */
#define MADRAS_LABEL_SECTION "__madras_labels"

typedef enum bindtype_e {
   NORMAL_BINDING = 0,
   LAZY_BINDING,
   WEAK_BINDING
} bindtype_t;

///////////////////////////////////////////////////////////////////////////////
//                                Data_chunks                                //
///////////////////////////////////////////////////////////////////////////////

struct data_chunk_s {
   void* command;                /**<Load command*/
   int64_t start_address;        /**<Start offset of the data pointed by the command, or -1 if there is no data*/
   int64_t end_address;          /**<End offset of the data pointed by the command, or -1 if there is no data*/
   uint16_t flags;               /**<Flags used to indicates which sub-part of the command references this data*/
};

///////////////////////////////////////////////////////////////////////////////
//                                 Bindings                                  //
///////////////////////////////////////////////////////////////////////////////

struct bind_s {
   int64_t adjust;               /**<Adjustment made on the calculated address*/
   uint64_t offset;              /**<Offset from the start of the segment*/
   char* symbol_name;            /**<Name of the symbol*/
   uint16_t segment;             /**<Segment where the symbol is found*/
   uint8_t type;                 /**<Type of the binding*/
   uint8_t library;              /**<Index of the library to bind the symbol*/
   uint8_t symbol_type;          /**<Type of the symbol (for "normal" bindings only)*/
};

/**
 * Creates a new binding
 * \param type The type of the symbol binding. Use values from \enum bindtype_e only.
 * (ISAAC_BINDING is NOT a valid binding !)
 * \return A pointer to the created binding
 */
extern bind_t* bind_new(uint8_t type);

/**
 * Frees a binding
 * \param bind The binding to free
 */
extern void bind_free(bind_t* bind);

/**
 * Creates a new binding as a copy from another binding
 * \param bind A pointer to the binding to copy
 * \return A pointer to the created binding
 */
extern bind_t* bind_copy(bind_t* bind);

/**
 * Sets the segment of a symbol binding
 * \param bind The symbol binding to update
 * \param seg The index of the segment
 */
extern void bind_setsegment(bind_t* bind, uint16_t seg);

/**
 * Sets the offset from the segment in a symbol binding
 * \param bind The symbol binding to update
 * \param offset The offset from the segment where the symbol is located
 */
extern void bind_setoffset(bind_t* bind, uint64_t offset);

/**
 * Sets the symbol value of a binding
 * \param bind The symbol binding to update
 * \param symbol_name A string containing the value of the symbol
 */
extern void bind_setsymbol_name(bind_t* bind, char* symbol_name);

/**
 * Sets the symbol type of a binding
 * \param bind The symbol binding to update
 * \param symbol_type The type of the symbol (see BIND_TYPE_* in loader.h)
 */
extern void bind_setsymbol_type(bind_t* bind, uint8_t symbol_type);

/**
 * Sets the library ordinal of a binding
 * \param bind The symbol binding to update
 * \param library The index of the external library
 */
extern void bind_setlibrary(bind_t* bind, uint8_t library);

/**
 * Gets the type of a symbol binding
 * \param bind A symbol binding
 * \return The type of binding
 */
extern uint8_t bind_gettype(bind_t* bind);

/**
 * Gets the segment's index of a symbol binding
 * \param bind A symbol binding
 * \return The index of the segment where to bind the symbol
 */
extern uint16_t bind_getsegment(bind_t* bind);

/**
 * Gets the offset from the segment in a symbol binding
 * \param bind A symbol binding
 * \return The offset from the segment where to bind the symbol
 */
extern uint64_t bind_getoffset(bind_t* bind);

/**
 * Gets the symbol value of a binding
 * \param bind A symbol binding
 * \return A string containing the name of the symbol
 */
extern char* bind_getsymbol_name(bind_t* bind);

/**
 * Gets the symbol type of a binding
 * \param bind A symbol binding
 * \return The type of the symbol (see BIND_TYPE_* in loader.h)
 */
extern uint8_t bind_getsymbol_type(bind_t* bind);

/**
 * Gets the library of a binding
 * \param bind A symbol binding
 * \return The index of the external library
 */
extern uint8_t bind_getlibrary(bind_t* bind);

/**
 * Gets the adjustment made to the calculated address
 * \param bind A symbol binding
 * \return The adjustment
 */
extern int64_t bind_getadjust(bind_t* bind);

/**
 * Add an adjustment to the calculated address
 * \param bind A symbol binding
 * \param adjust The additional adjustment (can be negative)
 */
extern void bind_addadjust(bind_t* bind, int64_t adjust);

///////////////////////////////////////////////////////////////////////////////
//                                 Binfiles                                  //
///////////////////////////////////////////////////////////////////////////////

/**
 * Loads a binfile_t structure with the result of the parsing of an Mach-O file or directory
 * THIS IS THE ENTRY POINT
 * \param bf A binary file structure containing an opened stream to the file to parse
 * \return Error code if the file could not be successfully parsed as an Mach-O file, EXIT_SUCCESS otherwise. In this
 * case, the structure representing the binary file will have been updated with the result of the parsing
 * */
extern int macho_binfile_load(binfile_t* bf);

/**
 * Loads a binfile_t structure with the result of the parsing of a Mach-O executable file.
 * It assumes the executable file is opened.
 * \param filedesc A file descriptor
 * \param bf A binary file structure containing an opened stream to the file to parse
 * \return Error code if the file could not be successfully parsed as an Mach-O file, EXIT_SUCCESS otherwise. In this
 * case, the structure representing the binary file will have been updated with the result of the parsing
 */
extern int macho_binfile_loadmagic(int filedesc, binfile_t* bf);

/**
 * Loads a binfile_t structure with the result of the parsing of a 32-bit Mach-O file.
 * It assumes the descriptor is at the start of the parsed binary (if the binary
 * is a fat binary then the descriptor is at the start of the sub-part corresponding
 * to the binfile_t bf).
 * \param filedesc A file descriptor
 * \param bf A binary file structure containing an opened stream to the file to parse
 */
extern void macho_binfile_loadheader32(int filedesc, binfile_t* bf);

/**
 * Loads a binfile_t structure with the result of the parsing of a 64-bit Mach-O file.
 * It assumes the descriptor is at the start of the parsed binary (if the binary
 * is a fat binary then the descriptor is at the start of the sub-part corresponding
 * to the binfile_t bf).
 * \param filedesc A file descriptor
 * \param bf A binary file structure containing an opened stream to the file to parse
 */
extern void macho_binfile_loadheader64(int filedesc, binfile_t* bf);

/**
 * Returns the first loaded address
 * \param bf A binary file structure containing an opened stream to the file to parse
 * \return The first loaded address of this binary file
 */
extern int64_t macho_binfile_getfirstloadaddr(binfile_t* bf);

/*
 * Returns the last loaded address
 * \param bf A binary file structure containing an opened stream to the file to parse
 * \return The last loaded address of this binary file
 */
extern int64_t macho_binfile_getlastloadaddr(binfile_t* bf);

/**
 * Returns a suffixed label corresponding to an external function
 * \param common_name The label to suffix
 * \return The suffixed string
 */
extern char* macho_binfile_generate_ext_label_name(char* common_name);

/**
 * Prints a macho header
 * \param bf A binary file structure containing an opened stream to the file to parse
 */
extern void macho_binfile_print(binfile_t* bf);

///////////////////////////////////////////////////////////////////////////////
//                                Macho-file                                 //
///////////////////////////////////////////////////////////////////////////////

struct macho_file_s {
   uint32_t offset;              /**<Offset in the file (for fat binaries)*/
   uint32_t commands_size;       /**<Number of bytes occupied by the commands*/
   void**   commands;            /**<Array of commands*/
   macho_segment_t** segments;   /**<Array of segments*/
   queue_t* data_chunks;         /**<Queue of data chunks*/
   queue_t* bindings;            /**<Queue of bindings*/
   queue_t* lazy_bindings;       /**<Queue of lazy bindings*/
   queue_t* weak_bindings;       /**<Queue of weak bindings*/
   binfile_t* binfile;           /**<Binary file to which this macho representation belongs to*/
   cpu_type_t cpu_type;          /**<Type of the architecture targeted by the binary*/
   cpu_subtype_t cpu_subtype;    /**<Model of the targeted CPU*/
   uint16_t n_commands;          /**<Number of commands*/
   uint16_t n_segments;          /**<Number of segments*/
   uint8_t textseg_id;           /**<Index of the __TEXT segment*/
   uint8_t dataseg_id;           /**<Index of the __DATA segment*/
};

/**
 * Create a new macho structure representing the binary file
 * \param bf The common binary file structure
 * \return A new architecture-specific representation
 */
extern macho_file_t* macho_file_new(binfile_t* bf);

/**
 * Frees a macho structure representing the binary file
 * \param macho_file A format-specific representation of a binary file
 */
extern void macho_file_free(void* macho_file_ptr);

/**
 * Sets the size of the command in a macho representation
 * \param macho_file A format-specific representation of a binary file
 * \param commands_size The size of the commands in bytes
 */
extern void macho_file_setcommands_size(macho_file_t* macho_file, uint32_t commands_size);

/**
 * Sets the number of commands in a macho representation
 * WARNING It reallocates space for the array of commands,
 * make sure the last elements are free before lowering the number of commands
 * \param macho_file A format-specific representation of a binary file
 * \param n_commands The number of commands in the segment
 */
extern void macho_file_setn_commands(macho_file_t* macho_file, uint32_t n_commands);

/**
 * Sets a command in a macho representation
 * \param macho_file A format-specific representation of a binary file
 * \param cmd_idx The index of the command
 * \param command The command to set
 */
extern void macho_file_setcommand(macho_file_t* macho_file, uint32_t cmd_idx, void* command);

/**
 * Sets the number of segments in a macho representation
 * WARNING It reallocates space for the array of segments,
 * make sure the last elements are free before lowering the number of segments
 * \param macho_file A format-specific representation of a binary file
 * \param n_segments The number of segments in the segment
 */
extern void macho_file_setn_segments(macho_file_t* macho_file, uint32_t n_segments);

/**
 * Sets a segment in a macho representation
 * \param macho_file A format-specific representation of a binary file
 * \param seg_idx The index of the segment
 * \param segment The segment to set
 */
extern void macho_file_setsegment(macho_file_t* macho_file, uint32_t seg_idx, macho_segment_t* segment);

/**
 * Sets the index of the __TEXT segment
 * \param macho_file A format-specific representation of a binary file
 * \param index The index of the __TEXT segment
 */
extern void macho_file_settextsegment_id(macho_file_t* macho_file, int8_t index);

/**
 * Sets the index of the __DATA segment
 * \param macho_file A format-specific representation of a binary file
 * \param index The index of the __DATA segment
 */
extern void macho_file_setdatasegment_id(macho_file_t* macho_file, int8_t index);

/**
 * Sets the binfile corresponding to a macho representation
 * \param macho_file A format-specific representation of a binary file
 * \param bf The binfile representation
 */
extern void macho_file_setbinfile(macho_file_t* macho_file, binfile_t* bf);

/**
 * Sets the offset corresponding to a macho representation
 * \param macho_file A format-specific representation of a binary file
 * \param offset The offset in the file
 */
extern void macho_file_setoffset(macho_file_t* macho_file, uint32_t offset);

/**
 * Sets the CPU type targeted by a binary file
 * \param macho_file A format-specific representation of a binary file
 * \param cpu_type The targeted CPU type
 */
extern void macho_file_setcpu_type(macho_file_t* macho_file, cpu_type_t cpu_type);

/**
 * Sets the CPU sub-type targeted by a binary file
 * \param macho_file A format-specific representation of a binary file
 * \param cpu_subtype The targeted CPU sub-type
 */
extern void macho_file_setcpu_subtype(macho_file_t* macho_file, cpu_type_t cpu_subtype);

/**
 * Gets the size of the commands in bytes
 * \param macho_file A format-specific representation of a binary file
 */
extern uint32_t macho_file_getcommands_size(macho_file_t* macho_file);

/**
 * Gets the number of commands in a macho representation
 * \param macho_file A format-specific representation of a binary file
 */
extern uint32_t macho_file_getn_commands(macho_file_t* macho_file);

/**
 * Gets a command from a macho representation
 * \param macho_file A format-specific representation of a binary file
 * \param cmd_idx The index of the command to get (starting at 0)
 */
extern void* macho_file_getcommand(macho_file_t* macho_file, uint32_t cmd_idx);

/**
 * Gets the number of segments in a macho representation
 * \param macho_file A format-specific representation of a binary file
 */
extern uint32_t macho_file_getn_segments(macho_file_t* macho_file);

/**
 * Gets the queue of commands ordered by their data
 * \param macho_file A format-specific representation of a binary file
 */
extern queue_t* macho_file_getdata_chunks(macho_file_t* macho_file);

/**
 * Gets a segment from a macho representation
 * \param macho_file A format-specific representation of a binary file
 * \param seg_idx The index of the segment to get (starting at 0)
 */
extern macho_segment_t* macho_file_getsegment(macho_file_t* macho_file, uint32_t seg_idx);

/**
 * Gets the binfile corresponding to a macho representation
 * \param macho_file A format-specific representation of a binary file
 */
extern binfile_t* macho_file_getbinfile(macho_file_t* macho_file);

/**
 * Gets the offset corresponding to a macho representation
 * \param macho_file A format-specific representation of a binary file
 */
extern uint32_t macho_file_getoffset(macho_file_t* macho_file);

/**
 * Gets the CPU type targeted by a binary file
 * \param macho_file A format-specific representation of a binary file
 */
extern cpu_type_t macho_file_getcpu_type(macho_file_t* macho_file);

/**
 * Gets the CPU sub-type targeted by a binary file
 * \param macho_file A format-specific representation of a binary file
 */
extern cpu_subtype_t macho_file_getcpu_subtype(macho_file_t* macho_file);

/**
 * Gets the queue of bindings of a Mach-O binary file
 * \param macho_file A format-specific representation of a binary file
 * \param The queue of bindings
 */
extern queue_t* macho_file_getbindings(macho_file_t* macho_file);

/**
 * Gets the queue of lazy bindings of a Mach-O binary file
 * \param macho_file A format-specific representation of a binary file
 * \param The queue of lazy bindings
 */
extern queue_t* macho_file_getlazy_bindings(macho_file_t* macho_file);

/**
 * Gets the queue of weak bindings of a Mach-O binary file
 * \param macho_file A format-specific representation of a binary file
 * \return The queue of weak bindings
 */
extern queue_t* macho_file_getweak_bindings(macho_file_t* macho_file);

/**
 * Returns the first loaded address
 * \param macho_file A format-specific representation of a binary file
 * \return The first loaded address of this binary file
 */
extern int64_t macho_file_getfirstloadaddr(macho_file_t* macho_file);

/**
 * Returns the last loaded address
 * \param macho_file A format-specific representation of a binary file
 * \return The last loaded address of this binary file
 */
extern int64_t macho_file_getlastloadaddr(macho_file_t* macho_file);

/**
 * Add a segment to the macho-specific structure representing a binary file
 * \param macho_file A format-specific representation of a binary file
 * \param seg The segment to add
 */
extern void macho_file_addsegment(macho_file_t* macho_file, macho_segment_t* seg);

/**
 * Add a command to the macho-specific structure representing a binary file
 * \param macho_file A format-specific representation of a binary file
 * \param command The command to add
 */
extern void macho_file_addcommand(macho_file_t* macho_file, void* command);

/**
 * Loads the commands of a macho binary file.
 * It calls the sub-procedures depending on the wordsize of the binary file
 * \param filedesc A file descriptor
 * \param macho_file A format-specific representation of a binary file
 */
extern void macho_file_loadcommands(int filedesc, macho_file_t* macho_file);

///////////////////////////////////////////////////////////////////////////////
//                                 Commands                                  //
///////////////////////////////////////////////////////////////////////////////

struct unknown_command_s {
   uint32_t cmd;                 /**<An integer indicating the type of the command*/
   uint32_t cmdsize;             /**<An integer specifying the total size in bytes of the command*/
   void* data;                   /**<A pointer to the rest of the command fields*/
};

///////////////////////////////////////////////////////////////////////////////
//                                 Segments                                  //
///////////////////////////////////////////////////////////////////////////////

struct macho_segment_s {
   char* name;                   /**<Name of the segment*/
   int64_t offset;               /**<Offset in this file of the data to be mapped*/
   uint64_t size;                /**<Size of the segment in bytes (it includes the size of all the section data structures)*/
   int64_t vmaddress;            /**<Starting virtual memory address of this segment*/
   uint64_t vmsize;              /**<Number of bytes of virtual memory occupied*/
   vm_prot_t initprot;           /**<Initial VM protection for this segment*/
   vm_prot_t maxprot;            /**<Maximum VM protection for this segment*/
   binscn_t** binscns;           /**<Sections contained in this segment*/
   void** sections;              /**<Sections contained in this segment (section32_t* or section64_t*)*/
   uint32_t n_sections;          /**<Number of sections in this segment*/
   uint32_t flags;               /**<Flags */
   void* command;                /**<The corresponding command (segment_command32_t* or segment_command64_t*)*/
   binseg_t* binseg;             /**<The corresponding libbin representation*/
};

/**
 * Creates a new segment
 * \param command The command corresponding to the created segment
 * \return A created segment
 */
extern macho_segment_t* macho_segment_new(void* command);

/**
 * Frees a segment
 * \param seg The segment to free
 */
extern void macho_segment_free(macho_segment_t* seg);

/**
 * Sets the name of a segment
 * \param seg The segment to update
 * \param name The name to set
 */
extern void macho_segment_setname(macho_segment_t* seg, char* name);

/**
 * Sets the offset of a segment
 * \param seg The segment to update
 * \param offset Offset of the segment in the file
 */
extern void macho_segment_setoffset(macho_segment_t* seg, int64_t offset);

/**
 * Sets the size of a segment
 * \param seg The segment to update
 * \param size Size of the segment in the file
 */
extern void macho_segment_setsize(macho_segment_t* seg, uint64_t size);

/**
 * Sets the virtual memory address of a segment
 * \param seg The segment to update
 * \param addr The address of the segment in memory
 */
extern void macho_segment_setvmaddress(macho_segment_t* seg, int64_t vmaddress);

/**
 * Sets the virtual memory size of a segment
 * \param seg The segment to update
 * \param vmsize The virtual size of the segment in memory
 */
extern void macho_segment_setvmsize(macho_segment_t* seg, uint64_t vmsize);

/**
 * Sets the number of section in a segment
 * WARNING It reallocates space for the array of sections,
 * make sure the last elements are free before lowering the number of sections
 * \param seg The segment to update
 * \param n_sections The number of sections in the segment
 */
extern void macho_segment_setn_sections(macho_segment_t* seg, uint32_t n_sections);

/**
 * Sets a section in the segment
 * \param seg The segment to update
 * \param sections The index of the section to set
 * \param section The section to set
 */
extern void macho_segment_setsection(macho_segment_t* seg, uint32_t sct_idx, void* section);

/**
 * Sets a libbin section representation in the segment
 * \param seg The segment to update
 * \param sections The index of the section to set
 * \param section The section to set
 */
extern void macho_segment_setbinsection(macho_segment_t* seg, uint32_t sct_idx, binscn_t* section);

/**
 * Sets the initial virtual memory protection of a segment
 * \param seg The segment to update
 * \param initprot The initial protection
 */
extern void macho_segment_setinitprot(macho_segment_t* seg, int initprot);

/**
 * Sets the maximum virtual memory protection of a segment
 * \param seg The segment to update
 * \param maxprot The maximum protection
 */
extern void macho_segment_setmaxprot(macho_segment_t* seg, int maxprot);

/**
 * Sets the flags relative to a segment
 * \param seg The segment to update
 * \param flags The flags to set to the segment
 */
extern void macho_segment_setflags(macho_segment_t* seg, uint32_t flags);

/**
 * Sets the libbin representation of the segment
 * \param seg The segment to update
 * \param binseg The libbin representation
 */
extern void macho_segment_setbinseg(macho_segment_t* seg, binseg_t* binseg);

/**
 * Gets the name of a segment
 * \param seg A segment
 * \return The name of the segment
 */
extern char* macho_segment_getname(macho_segment_t* seg);

/**
 * Gets the offset of a segment
 * \param seg A segment
 * \return The segment's offset in the file
 */
extern int64_t macho_segment_getoffset(macho_segment_t* seg);

/**
 * Gets the size of a segment
 * \param seg A segment
 * \return The segment's size in the file
 */
extern uint64_t macho_segment_getsize(macho_segment_t* seg);

/**
 * Gets the virtual memory address of a segment
 * \param seg A segment
 * \return The address of the segment in memory
 */
extern int64_t macho_segment_getvmaddress(macho_segment_t* seg);

/**
 * Gets the virtual memory size of a segment
 * \param seg A segment
 * \return The virtual size of the segment in memory
 */
extern uint64_t macho_segment_getvmsize(macho_segment_t* seg);

/**
 * Gets the number of sections in a segment
 * \param seg A segment
 * \return The number of sections in the segment
 */
extern uint32_t macho_segment_getn_sections(macho_segment_t* seg);

/**
 * Gets a section from a segment
 * \param seg A segment
 * \param sct_idx The index of the section to get (starting at 0)
 * \return The section at the given index (macho_section32_t* or macho_section64_t*)
 */
extern void* macho_segment_getsection(macho_segment_t* seg, uint32_t sct_idx);

/**
 * Gets a libbin section representation from a segment
 * \param seg A segment
 * \param sct_idx The index of the section to get (starting at 0)
 * \return The libbin section representation at the given index
 */
extern binscn_t* macho_segment_getbinsection(macho_segment_t* seg, uint32_t sct_idx);

/**
 * Gets the initial virtual memory protection of a segment
 * \param seg A segment
 */
extern vm_prot_t macho_segment_getinitprot(macho_segment_t* seg);

/**
 * Gets the maximum virtual memory protection of a segment
 * \param seg A segment
 */
extern vm_prot_t macho_segment_getmaxprot(macho_segment_t* seg);

/**
 * Gets the flags relative to a given segment
 * \param seg A segment
 */
extern uint32_t macho_segment_getflags(macho_segment_t* seg);

/*
 * Gets the libbin representation of a given segment
 * \param seg A segment
 */
extern binseg_t* macho_segment_getbinseg(macho_segment_t* seg);

/**
 * Adds a section to a segment
 * \param seg A segment
 * \param sct A section
 */
extern void macho_segment_addsection(macho_segment_t* seg, void* sct);

/**
 * Removes a section from a segment
 * \param seg A segment
 * \param sct_idx The index of the section to delete (starting at 0)
 */
extern void macho_segment_removesection(macho_segment_t* seg, uint32_t sct_idx);

/**
 * Parses the 32-bit section headers contained in a given segment
 * \param filedesc A file descriptor
 * \param bf A binary file structure containing an opened stream to the file to parse
 * \param seg The segment the sections belong to
 * \return The number of bytes read
 */
extern int macho_segment_loadsections32(int filedesc, binfile_t* bf, macho_segment_t* seg);

/**
 * Parses the 64-bit section headers contained in a given segment
 * \param filedesc A file descriptor
 * \param bf A binary file structure containing an opened stream to the file to parse
 * \param seg The segment the sections belong to
 * \return The number of bytes read
 */
extern int macho_segment_loadsections64(int filedesc, binfile_t* bf, macho_segment_t* seg);

///////////////////////////////////////////////////////////////////////////////
//                                 Dbgfiles                                  //
///////////////////////////////////////////////////////////////////////////////

extern dbg_file_t* macho_parsedbg(binfile_t* bf);

///////////////////////////////////////////////////////////////////////////////
//                                  Asmfile                                  //
///////////////////////////////////////////////////////////////////////////////

extern int macho_asmfile_addlabels(asmfile_t* asmf);

extern void macho_asmfile_print_binfile(asmfile_t* asmf);

///////////////////////////////////////////////////////////////////////////////
//                                  Sections                                 //
///////////////////////////////////////////////////////////////////////////////

/**
 * Sets the type of a 32-bit Mach-O section
 * \param section The section to update
 * \param type The type to set
 */
extern void macho_section32_settype(macho_section32_t* section, uint8_t type);

/**
 * Sets the type of a 64-bit Mach-O section
 * \param section The section to update
 * \param type The type to set
 */
extern void macho_section64_settype(macho_section64_t* section, uint8_t type);

/**
 * Sets the attributes of a 32-bit Mach-O section
 * \param section The section to update
 * \param attr The attributes to set
 */
extern void macho_section32_setattributes(macho_section32_t* section, uint32_t attr);

/**
 * Sets the attributes of a 64-bit Mach-O section
 * \param section The section to update
 * \param attr The attributes to set
 */
extern void macho_section64_setattributes(macho_section64_t* section, uint32_t attr);

extern int macho_section_getannotate(binfile_t* bf, binscn_t* scn);

///////////////////////////////////////////////////////////////////////////////
//                              Libbin interface                             //
///////////////////////////////////////////////////////////////////////////////

/**
 * Creates a Mach-O representation of a segment from its libbin representation
 * \param binseg The libbin representation
 */
extern void macho_segment_newfrombinseg(binseg_t* binseg);

/**
 * Creates a libbin segment from a Mach-O segment representation
 * \param bf A binary file structure
 * \param seg The Mach-O segment representation
 * \return The created libbin segment
 */
extern binseg_t* macho_binseg_newfromseg(binfile_t* bf, macho_segment_t* seg);

/**
 * Add a new Mach-O section from a libbin representation
 * \param scn The newly created binscn_t section
 */
extern void macho_section32_newfrombinscn(binscn_t* scn);

/**
 * Creates a libbin section from a Mach-O section representation
 * \param bf A binary file structure
 * \param seg The segment the section belongs to
 * \param sct The Mach-O representation of the section
 * \return A pointer to the created libbin section
 */
extern binscn_t* macho_binscn_newfromsection32(binfile_t* bf, macho_segment_t* seg, macho_section32_t* sct);

/**
 * Add a new Mach-O section from a libbin representation
 * \param scn The newly created binscn_t section
 */
extern void macho_section64_newfrombinscn(binscn_t* scn);

/**
 * Creates a libbin section from a Mach-O section representation
 * \param bf A binary file structure
 * \param seg The segment the section belongs to
 * \param sct The Mach-O representation of the section
 * \return A pointer to the created libbin section
 */
extern binscn_t* macho_binscn_newfromsection64(binfile_t* bf, macho_segment_t* seg, macho_section64_t* sct);

#endif /* LIBMWORMS_H__ */
