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
 * \file libmworms.c
 * \brief This file contains all functions needed to parse, modify and create Mach-O files
 * */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>

#include "libmworm.h"
#include "archinterface.h"
#include "libmasm.h"


//TODO Move this function elsewhere
static int64_t reverse_bytes (int64_t value, int64_t size) {

   switch(size) {
   case(2):
      return (((value&0xFF00) >> 8) | ((value&0xFF) << 8));
      break;
   case(4):
      return (((value&0xFF00) << 8) | ((value&0xFF) << 24) | ((value&0xFF0000) >> 8) | ((value&0xFF000000) >> 24));
      break;
   case(8):
      return value;
      break;
   default:
      return value;
      break;
   }
   return value;
}

///////////////////////////////////////////////////////////////////////////////
//                                  Macros                                   //
///////////////////////////////////////////////////////////////////////////////

/**
 * This macro wraps a function call and check that the return is not negative
 * It is intended to wrap file manipulations (lseek, read, etc.)
 */
#define SAFE(FUNC) if(FUNC < 0) { HLTMSG("Error when reading binary header !\n"); }

/**
 * This macro wraps accesses to data with a possible reversed byte order scheme
 * WARNING: You need to have access to the binfile_t named bf in order to use this macro
 */
#define REV_BYT(VALUE) ((binfile_get_byte_order(bf) == BFO_REVERSED)?reverse_bytes((int64_t)VALUE, sizeof(VALUE)):(int64_t)VALUE)

/**
 * This macro read a command from the current position in the file.
 */
#define READ_COMMAND(FILE, COMMAND, COMMAND_NAME)\
   COMMAND* COMMAND_NAME = (COMMAND*)lc_malloc0(sizeof(COMMAND));\
   SAFE(read(FILE, COMMAND_NAME, sizeof(COMMAND)));\
   DBGMSG(" - %s of %"PRId64" bytes.\n", #COMMAND, REV_BYT(COMMAND_NAME->cmdsize))

/**
 * This macro add a data chunk into a queue
 */
#define ORDER(QUEUE, CMD, START, END, LAST)\
   if ( START >= LAST){\
      LAST = END;\
      queue_add_tail(QUEUE, data_chunk_new(CMD, START, END));\
   } else {\
      FOREACH_INQUEUE(QUEUE, elt){\
         if ( GET_DATA_T(data_chunk_t*, elt)->start_address > START ) {\
            queue_insertbefore(QUEUE, elt, data_chunk_new(CMD, START, END));\
            break;\
         }\
      }\
   }

///////////////////////////////////////////////////////////////////////////////
//                                 Utilities                                 //
///////////////////////////////////////////////////////////////////////////////

/**
 * Reads an uleb128 value from a stream and returns the number of bytes read
 * \param stream The stream where to read the uleb128 value
 * \param value A pointer to an integer. The functions update this variable
 * with the read value.
 * \return The number of bytes read
 */
static int read_uleb128 (unsigned char* stream, uint64_t* value){
   if (!stream)
      return SIGNED_ERROR;

   int shift = 0, bytes_read = 0;
   *value = 0;

   //See ULEB128 for the parsing
   do {
      *value |= ((uint64_t)(stream[bytes_read] & 0x7F) << shift);
      shift += 7;
   } while (stream[bytes_read++] >= 0x80);

   /* If the value is negative it is probably normal..
    * They do overflow the uint64_t value on purpose as they don't have a way
    * to encode a negative value.
    */
   DBGMSGLVL(2, "Value: %"PRId64"\n", *value);
   DBGMSGLVL(2, "Bytes read: %d\n", bytes_read);

   return bytes_read;
}

/**
 * Reads a 'stab'
 * \param symbol A pointer to the symbol structure
 * \param name A pointer to the string containing the name
 */
static void read_stab64 (nlist32_t* symbol, char* name) {
   return;
}

/**
 * Reads symbol information
 * \param symbol_table A pointer to the start of the symbol table
 * \param symbol_left The number of symbol left to parse
 */
static void read_symbol32 (unsigned char* symbol_table, int symbol_left, unsigned char* string_table) {

   if (symbol_left > 0) {
      nlist32_t* symbol = (nlist32_t*)lc_malloc0(sizeof(nlist32_t));
      memcpy(symbol, symbol_table, sizeof(nlist32_t));
      symbol_left--;
      DBGMSGLVL(2, "Symbol: %d, %d, %d, %d\n", symbol->n_type, symbol->n_sect, symbol->n_desc, symbol->n_value);
      read_symbol32(symbol_table, symbol_left, string_table);
   }
}

/*
 * Reads symbol information
 * \param symbol_table A pointer to the start of the symbol table
 * \param symbol_left The number of symbol left to parse
 */
static void read_symbol64 (unsigned char* symbol_table, int symbol_left, unsigned char* string_table) {

   if (symbol_left > 0) {
      nlist64_t* symbol = (nlist64_t*)lc_malloc0(sizeof(nlist64_t));
      memcpy(symbol, symbol_table, sizeof(nlist64_t));
      symbol_table += sizeof(nlist64_t);
      symbol_left--;
      DBGMSGLVL(2, "Symbol: %x, %x, %x, %"PRIx64"\n", symbol->n_type, symbol->n_sect, symbol->n_desc, symbol->n_value);
      read_symbol64(symbol_table, symbol_left, string_table);
   }
}

/*
 * Reads binding information
 * This function is recursive so it keeps updated the pointer to the stream and the size_left.
 * By default when a binding ends (with BIND_OPCODE_BIND*) the following binding has the exact
 * same information (plus a delta in offset) so we make a copy of each binding when it ends
 * and it is used as basis for the the new following binding.
 * \param bindings Queue of bindings. The last element is filled with parsed information
 * \param binding_area A pointer in the stream where to read
 * \param size_left Number of bytes left before the end of the stream
 * \param offset Offset from the start of the file
 */
static void read_binding (queue_t* bindings, unsigned char* binding_area, int size_left) {
   int bytes_read = 0;
   uint64_t uvalue = 0;
   bind_t* bind = (bind_t*)queue_peek_tail(bindings);

   size_left--;

   //Opcode is on the 4 higher bits
   switch (binding_area[0]&BIND_OPCODE_MASK) {
   case(BIND_OPCODE_DONE):

      //Sets the end of a binding. Next there is padding with 0x00 to 8-byte boundary
      DBGMSG0LVL(1, "BIND_OPCODE_DONE\n");
      if (size_left > 0)
         read_binding(bindings, ++binding_area, size_left);

      break;

   case(BIND_OPCODE_SET_DYLIB_ORDINAL_IMM):

      /* Sets the index of the library the symbol must be bind to.
       * The libraries are in the dylib commands */
      bind_setlibrary(bind, binding_area[0]&BIND_IMMEDIATE_MASK);
      DBGMSGLVL(1, "BIND_OPCODE_SET_DYLIB_ORDINAL_IMM: %d\n", binding_area[0]&BIND_IMMEDIATE_MASK);

      if (size_left > 0)
         read_binding(bindings, ++binding_area, size_left);

      break;

   case(BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB):

      /* Sets the index of the library the symbol must be bind to.
       * The libraries are in the dylib commands */
      bytes_read = read_uleb128(++binding_area, &uvalue);
      size_left -= bytes_read; binding_area += (bytes_read - 1);

      bind_setlibrary(bind, uvalue);
      DBGMSGLVL(1, "BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB %"PRId64"\n", uvalue);

      if (size_left > 0)
         read_binding(bindings, ++binding_area, size_left);

      break;

   case(BIND_OPCODE_SET_DYLIB_SPECIAL_IMM):

      //Dunno what it indicates but the "special" is set as a negative value
      DBGMSGLVL(1, "BIND_OPCODE_SET_DYLIB_SPECIAL_IMM: %d\n", - (binding_area[0]&BIND_IMMEDIATE_MASK));
      if (size_left > 0)
         read_binding(bindings, ++binding_area, size_left);

      break;

   case(BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM):
   {
      //This is the symbol we are trying to bind
      DBGMSGLVL(1, "BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM: %d\n", binding_area[0]&BIND_IMMEDIATE_MASK);
      binding_area++; size_left--;

      //The strdup works only with the '0' trailing symbol
      char* symbol_name = strdup((char*)binding_area);
      bind_setsymbol_name(bind, symbol_name);
      DBGMSGLVL(1, "Symbol name: %s\n", symbol_name);

      size_left -= strlen(symbol_name);
      binding_area += strlen(symbol_name);
      if (size_left > 0)
         read_binding(bindings, ++binding_area, size_left);

      break;
   }
   case(BIND_OPCODE_SET_TYPE_IMM):

      //Sets the "type" of a symbol, 1 is pointer
      if (((binding_area[0]&BIND_IMMEDIATE_MASK) >= BIND_TYPE_POINTER)
            && ((binding_area[0]&BIND_IMMEDIATE_MASK) <= BIND_TYPE_TEXT_PCREL32))
         bind_setsymbol_type(bind, binding_area[0]&BIND_IMMEDIATE_MASK);

      DBGMSGLVL(1, "BIND_OPCODE_SET_TYPE_IMM: %d\n", binding_area[0]&BIND_IMMEDIATE_MASK);
      if (size_left > 0)
         read_binding(bindings, ++binding_area, size_left);

      break;

   case(BIND_OPCODE_SET_ADDEND_SLEB):

      //Usage unknown
      bytes_read = read_uleb128(++binding_area, &uvalue);
      size_left -= bytes_read; binding_area += (bytes_read - 1);
      DBGMSGLVL(1, "BIND_OPCODE_SET_ADDEND_SLEB %"PRId64"\n", uvalue);
      if (size_left > 0)
         read_binding(bindings, ++binding_area, size_left);

      break;

   case(BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB):

      //Indicates where the symbol is. Segment index and offset from the start of the segment
      bind_setsegment(bind, binding_area[0]&BIND_IMMEDIATE_MASK);
      DBGMSGLVL(1, "BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB %d\n", binding_area[0]&BIND_IMMEDIATE_MASK);

      bytes_read = read_uleb128(++binding_area, &uvalue);
      bind_setoffset(bind, uvalue);

      size_left -= bytes_read;  binding_area += (bytes_read - 1);
      DBGMSGLVL(1, "Offset: %#"PRIx64"\n", uvalue);

      if (size_left > 0)
         read_binding(bindings, ++binding_area, size_left);

      break;

   case(BIND_OPCODE_ADD_ADDR_ULEB):

      //Increases the offset (as above)
      bytes_read = read_uleb128(++binding_area, &uvalue);
      bind_addadjust(bind, uvalue);

      size_left -= bytes_read; binding_area += (bytes_read - 1);
      DBGMSGLVL(1, "BIND_OPCODE_ADD_ADDR_ULEB %"PRId64"\n", uvalue);
      if (size_left > 0)
         read_binding(bindings, ++binding_area, size_left);

      break;

   case(BIND_OPCODE_DO_BIND):
      //Binds with all gathered information and add 4 or 8 to the offset
      DBGMSG0LVL(1, "BIND_OPCODE_DO_BIND\n");
      if (size_left > 0) {
         //Binding done, creating a copy for the next one
         //TODO 32 bits: +4, 64 bits: +8
         bind_t* new_bind = bind_copy(bind);
         bind_setoffset(new_bind, bind_getoffset(bind) + 8);

         queue_add_tail(bindings, new_bind);
         read_binding(bindings, ++binding_area, size_left);
      }

      break;
   case(BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB):

      //Binds with all gathered information and increases the offset by a value in the extra data
      bytes_read = read_uleb128(++binding_area, &uvalue);
      size_left -= bytes_read; binding_area += (bytes_read - 1);
      DBGMSGLVL(1, "BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB %"PRId64"\n", uvalue);
      if (size_left > 0) {
         //Binding done, creating a copy for the next one
         //TODO 32 bits: +4, 64 bits: +8
         bind_t* new_bind = bind_copy(bind);
         bind_setoffset(new_bind, bind_getoffset(bind) + 8);
         bind_addadjust(new_bind, uvalue);

         queue_add_tail(bindings, new_bind);
         read_binding(bindings, ++binding_area, size_left);
      }

      break;

   case(BIND_OPCODE_DO_BIND_ADD_ADDR_IMM_SCALED):

      //Same as DO_BIND, but an extra imm*4 bytes is also added.
      DBGMSGLVL(1, "BIND_OPCODE_DO_BIND_ADD_ADDR_IMM_SCALED %d\n", (binding_area[0]&BIND_IMMEDIATE_MASK) * 4);
      if (size_left > 0) {
         //Binding done, creating a copy for the next one
         //TODO 32 bits: +4, 64 bits: +8
         bind_t* new_bind = bind_copy(bind);
         bind_setoffset(new_bind, bind_getoffset(bind) + 8);
         bind_addadjust(new_bind, (binding_area[0]&BIND_IMMEDIATE_MASK) * 4);

         queue_add_tail(bindings, new_bind);
         read_binding(bindings, ++binding_area, size_left);
      }

      break;

   case(BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB):
   {
      uint64_t skip = 0; uint i = 0;
      /* Two unsigned LEB128-encoded numbers are read off from the extra data.
       * The first is the count of symbols to be added, and the second is the bytes to skip after a symbol is added.
       */
      bytes_read = read_uleb128(++binding_area, &uvalue);
      size_left -= bytes_read; binding_area += (bytes_read - 1);
      bytes_read = read_uleb128(++binding_area, &skip);
      size_left -= bytes_read; binding_area += (bytes_read - 1);

      DBGMSGLVL(1, "BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB %"PRId64", %"PRId64"\n", uvalue, skip);

      //Binding done, creating a copy for the next ones
      bind_t* new_bind = bind_copy(bind);
      bind_setoffset(new_bind, bind_getoffset(bind) + 8);
      bind_addadjust(new_bind, skip);

      queue_add_tail(bindings, new_bind);
      bind = new_bind;

      for (i = 0; i < uvalue; i++) {
         bind_t* new_bind = bind_copy(bind);
         bind_setoffset(new_bind, bind_getoffset(bind) + 8);
         bind_addadjust(new_bind, skip);

         queue_add_tail(bindings, new_bind);
         bind = new_bind;
      }

      if (size_left > 0)
         read_binding(bindings, ++binding_area, size_left);

      break;
   }
   default:
      HLTMSG("UNKNOWN OPCODE: %#"PRIx32"\n", binding_area[0]&BIND_OPCODE_MASK);
   }

   //Free the binding allocated for further parsing
   if (size_left <= 0)
      bind_free(queue_remove_tail(bindings));

   return;
}

///////////////////////////////////////////////////////////////////////////////
//                                Data_chunks                                //
///////////////////////////////////////////////////////////////////////////////

data_chunk_t* data_chunk_new(void* command, int64_t start_address, int64_t end_address) {
   data_chunk_t* data_chunk   = (data_chunk_t*)lc_malloc0(sizeof(data_chunk_t));
   data_chunk->start_address  = start_address;
   data_chunk->end_address    = end_address;
   data_chunk->command        = command;

   DBGMSG("Created data_chunk_t: %p starting at %#"PRIx64", ending at %#"PRIx64"\n", data_chunk,
         start_address, end_address);

   return data_chunk;
}

///////////////////////////////////////////////////////////////////////////////
//                                 Bindings                                  //
///////////////////////////////////////////////////////////////////////////////

/**
 * Creates a new binding
 * \param type The type of the symbol binding. Use values from \enum bindtype_e only.
 * (ISAAC_BINDING is NOT a valid binding !)
 * \return A pointer to the created binding
 */
bind_t* bind_new(uint8_t type) {
   bind_t* bind   = (bind_t*)lc_malloc0(sizeof(bind_t));
   bind->type     = type;

   return bind;
}

/**
 * Frees a binding
 * \param bind The binding to free
 */
void bind_free(bind_t* bind) {
   if (bind) {
      lc_free(bind->symbol_name);
      lc_free(bind);
   }
}

/**
 * Creates a new binding as a copy from another binding
 * \param bind A pointer to the binding to copy
 * \return A pointer to the created binding
 */
bind_t* bind_copy(bind_t* bind) {
   bind_t* copy      = (bind_t*)lc_malloc0(sizeof(bind_t));
   copy->type        = bind->type;
   copy->library     = bind->library;
   copy->offset      = bind->offset;
   copy->segment     = bind->segment;
   copy->symbol_type = bind->symbol_type;
   copy->adjust      = bind->adjust;
   if (bind->symbol_name)
      copy->symbol_name = lc_strdup(bind->symbol_name);

   return copy;
}

/**
 * Sets the segment of a symbol binding
 * \param bind The symbol binding to update
 * \param seg The index of the segment
 */
void bind_setsegment(bind_t* bind, uint16_t seg) {
   if (bind)
      bind->segment = seg;
}

/**
 * Sets the offset from the segment in a symbol binding
 * \param bind The symbol binding to update
 * \param offset The offset from the segment where the symbol is located
 */
void bind_setoffset(bind_t* bind, uint64_t offset) {
   if (bind)
      bind->offset = offset;
}

/**
 * Sets the symbol value of a binding
 * \param bind The symbol binding to update
 * \param symbol_name A string containing the value of the symbol
 */
void bind_setsymbol_name(bind_t* bind, char* symbol_name) {
   if (bind && symbol_name)
      bind->symbol_name = lc_strdup(symbol_name);
}

/**
 * Sets the symbol type of a binding
 * \param bind The symbol binding to update
 * \param symbol_type The type of the symbol (see BIND_TYPE_* in loader.h)
 */
void bind_setsymbol_type(bind_t* bind, uint8_t symbol_type) {
   if (bind)
      bind->symbol_type = symbol_type;
}

/**
 * Sets the library ordinal of a binding
 * \param bind The symbol binding to update
 * \param library The index of the external library
 */
void bind_setlibrary(bind_t* bind, uint8_t library) {
   if (bind)
      bind->library = library;
}

/**
 * Gets the type of a symbol binding
 * \param bind A symbol binding
 * \return The type of binding
 */
uint8_t bind_gettype(bind_t* bind) {
   return (bind) ? bind->type : UNSIGNED_ERROR;
}

/**
 * Gets the segment's index of a symbol binding
 * \param bind A symbol binding
 * \return The index of the segment where to bind the symbol
 */
uint16_t bind_getsegment(bind_t* bind) {
   return (bind) ? bind->segment : UNSIGNED_ERROR;
}

/**
 * Gets the offset from the segment in a symbol binding
 * \param bind A symbol binding
 * \return The offset from the segment where to bind the symbol
 */
uint64_t bind_getoffset(bind_t* bind) {
   return (bind) ? bind->offset : UNSIGNED_ERROR;
}

/**
 * Gets the symbol value of a binding
 * \param bind A symbol binding
 * \return A string containing the name of the symbol
 */
char* bind_getsymbol_name(bind_t* bind) {
   return (bind) ? bind->symbol_name : NULL;
}

/**
 * Gets the symbol type of a binding
 * \param bind A symbol binding
 * \return The type of the symbol (see BIND_TYPE_* in loader.h)
 */
uint8_t bind_getsymbol_type(bind_t* bind) {
   return (bind) ? bind->symbol_type : UNSIGNED_ERROR;
}

/**
 * Gets the library of a binding
 * \param bind A symbol binding
 * \return The index of the external library
 */
uint8_t bind_getlibrary(bind_t* bind) {
   return (bind) ? bind->library : UNSIGNED_ERROR;
}

/**
 * Gets the adjustment made to the calculated address
 * \param bind A symbol binding
 * \return The adjustment
 */
int64_t bind_getadjust(bind_t* bind) {
   return (bind) ? bind->adjust : SIGNED_ERROR;
}

/**
 * Add an adjustment to the calculated address
 * \param bind A symbol binding
 * \param adjust The additional adjustment (can be negative)
 */
void bind_addadjust(bind_t* bind, int64_t adjust) {
   if (bind)
      bind->adjust += adjust;
}

///////////////////////////////////////////////////////////////////////////////
//                                Macho-file                                 //
///////////////////////////////////////////////////////////////////////////////

/**
 * Creates a new macho structure representing the binary file
 * \param bf The common binary file structure
 * \return A new architecture-specific representation
 */
macho_file_t* macho_file_new(binfile_t* bf){

   if (!bf)
      return NULL;

   macho_file_t* macho_file      = (macho_file_t*)lc_malloc0(sizeof(macho_file_t));
   macho_file->binfile           = bf;
   macho_file->data_chunks       = queue_new();
   macho_file->bindings          = queue_new();
   macho_file->lazy_bindings     = queue_new();
   macho_file->weak_bindings     = queue_new();

   //Setting format-specific information in the driver
   bf_driver_t* driver                    = binfile_get_driver(bf);
   driver->parsedbin                      = macho_file;
   driver->binfile_parse_dbg               = macho_parsedbg;
   driver->parsedbin_free                 = macho_file_free;
   driver->asmfile_add_ext_labels             = macho_asmfile_addlabels;
//   driver->binfile_getscnannotate         = macho_section_getannotate;
   driver->binfile_patch_get_first_load_addr = macho_binfile_getfirstloadaddr;
   driver->binfile_patch_get_last_load_addr  = macho_binfile_getlastloadaddr;
   driver->generate_ext_label_name                = macho_binfile_generate_ext_label_name;
   driver->asmfile_print_binfile           = macho_asmfile_print_binfile;

   return macho_file;
}

/**
 * Frees a macho structure representing the binary file
 * \param macho_file A format-specific representation of a binary file
 */
void macho_file_free(void* macho_file_ptr){

   if (!macho_file_ptr)
      return;

   macho_file_t* macho_file = (macho_file_t*)macho_file_ptr;
   int i;

   //Frees data chunks
   queue_free(macho_file->data_chunks, &lc_free);

   //Frees commands
   for(i = 0; i < macho_file->n_commands; i++){
      if (((load_command_t*)macho_file->commands[i])->cmd == LC_PREPAGE) {
         lc_free(((unknown_command_t*)macho_file->commands[i])->data);
      }
      lc_free(macho_file->commands[i]);
   }

   //Frees segments
   for(i = 0; i < macho_file->n_segments; i++){
      macho_segment_free(macho_file->segments[i]);
   }

   lc_free(macho_file);
}

/**
 * Sets the size of the command in a macho representation
 * \param macho_file A format-specific representation of a binary file
 * \param commands_size The size of the commands in bytes
 */
void macho_file_setcommands_size(macho_file_t* macho_file, uint32_t commands_size) {
   if(macho_file)
      macho_file->commands_size = commands_size;
}

/**
 * Sets the number of commands in a macho representation
 * WARNING It reallocates space for the array of commands,
 * make sure the last elements are free before lowering the number of commands
 * \param macho_file A format-specific representation of a binary file
 * \param n_commands The number of commands in the segment
 */
void macho_file_setn_commands(macho_file_t* macho_file, uint32_t n_commands) {
   if(macho_file) {
      macho_file->n_commands = n_commands;
      macho_file->commands = (void**)lc_realloc(macho_file->commands, sizeof(void*) * macho_file->n_commands);
   }
}

/**
 * Sets a command in a macho representation
 * \param macho_file A format-specific representation of a binary file
 * \param cmd_idx The index of the command
 * \param command The command to set
 */
void macho_file_setcommand(macho_file_t* macho_file, uint32_t cmd_idx, void* command) {
   if(macho_file && (cmd_idx < macho_file->n_commands) && command)
      macho_file->commands[cmd_idx] = command;
}

/**
 * Sets the number of segments in a macho representation
 * WARNING It reallocates space for the array of segments,
 * make sure the last elements are free before lowering the number of segments
 * \param macho_file A format-specific representation of a binary file
 * \param n_segments The number of segments in the segment
 */
void macho_file_setn_segments(macho_file_t* macho_file, uint32_t n_segments) {
   if(macho_file) {
      macho_file->n_segments = n_segments;
      macho_file->segments = (macho_segment_t**)lc_realloc(macho_file->segments, sizeof(macho_segment_t*) * macho_file->n_segments);
   }
}

/**
 * Sets a segment in a macho representation
 * \param macho_file A format-specific representation of a binary file
 * \param seg_idx The index of the segment
 * \param segment The segment to set
 */
void macho_file_setsegment(macho_file_t* macho_file, uint32_t seg_idx, macho_segment_t* segment) {
   if(macho_file && (seg_idx < macho_file->n_segments) && segment)
      macho_file->segments[seg_idx] = segment;
}

/**
 * Sets the index of the __TEXT segment
 * \param macho_file A format-specific representation of a binary file
 * \param index The index of the __TEXT segment
 */
void macho_file_settextsegment_id(macho_file_t* macho_file, int8_t index) {
   if (macho_file)
      macho_file->textseg_id = index;
}

/**
 * Sets the index of the __DATA segment
 * \param macho_file A format-specific representation of a binary file
 * \param index The index of the __DATA segment
 */
void macho_file_setdatasegment_id(macho_file_t* macho_file, int8_t index) {
   if (macho_file)
      macho_file->dataseg_id = index;
}

/**
 * Sets the binfile corresponding to a macho representation
 * \param macho_file A format-specific representation of a binary file
 * \param bf The binfile representation
 */
void macho_file_setbinfile(macho_file_t* macho_file, binfile_t* bf) {
   if(macho_file && bf)
      macho_file->binfile = bf;
}

/**
 * Sets the offset corresponding to a macho representation
 * \param macho_file A format-specific representation of a binary file
 * \param offset The offset in the file
 */
void macho_file_setoffset(macho_file_t* macho_file, uint32_t offset) {
   if(macho_file)
      macho_file->offset = offset;
}

/**
 * Sets the CPU type targeted by a binary file
 * \param macho_file A format-specific representation of a binary file
 * \param cpu_type The targeted CPU type
 */
void macho_file_setcpu_type(macho_file_t* macho_file, cpu_type_t cpu_type) {
   if (macho_file)
      macho_file->cpu_type = cpu_type;
}

/**
 * Sets the CPU sub-type targeted by a binary file
 * \param macho_file A format-specific representation of a binary file
 * \param cpu_subtype The targeted CPU sub-type
 */
void macho_file_setcpu_subtype(macho_file_t* macho_file, cpu_type_t cpu_subtype) {
   if (macho_file)
      macho_file->cpu_subtype = cpu_subtype;
}

/**
 * Gets the size of the commands in bytes
 * \param macho_file A format-specific representation of a binary file
 */
uint32_t macho_file_getcommands_size(macho_file_t* macho_file) {
   return (macho_file) ? macho_file->commands_size : UNSIGNED_ERROR;
}

/**
 * Gets the number of commands in a macho representation
 * \param macho_file A format-specific representation of a binary file
 */
uint32_t macho_file_getn_commands(macho_file_t* macho_file) {
   return (macho_file) ? macho_file->n_commands : UNSIGNED_ERROR;
}

/**
 * Gets a command from a macho representation
 * \param macho_file A format-specific representation of a binary file
 * \param cmd_idx The index of the command to get (starting at 0)
 */
void* macho_file_getcommand(macho_file_t* macho_file, uint32_t cmd_idx) {
   return (macho_file && (cmd_idx < macho_file->n_commands)) ? macho_file->commands[cmd_idx] : NULL;
}

/**
 * Gets the number of segments in a macho representation
 * \param macho_file A format-specific representation of a binary file
 */
uint32_t macho_file_getn_segments(macho_file_t* macho_file) {
   return (macho_file) ? macho_file->n_segments : UNSIGNED_ERROR;
}

/**
 * Gets the queue of data chunks
 * \param macho_file A format-specific representation of a binary file
 */
queue_t* macho_file_getdata_chunks(macho_file_t* macho_file) {
   return (macho_file) ? macho_file->data_chunks : NULL;
}

/**
 * Gets a segment from a macho representation
 * \param macho_file A format-specific representation of a binary file
 * \param seg_idx The index of the segment to get (starting at 0)
 */
macho_segment_t* macho_file_getsegment(macho_file_t* macho_file, uint32_t seg_idx) {
   return (macho_file && (seg_idx < macho_file->n_segments)) ? macho_file->segments[seg_idx] : NULL;
}

/**
 * Gets the binfile corresponding to a macho representation
 * \param macho_file A format-specific representation of a binary file
 */
binfile_t* macho_file_getbinfile(macho_file_t* macho_file) {
   return (macho_file) ? macho_file->binfile : NULL;
}

/**
 * Gets the offset corresponding to a macho representation
 * \param macho_file A format-specific representation of a binary file
 */
uint32_t macho_file_getoffset(macho_file_t* macho_file) {
   return (macho_file) ? macho_file->offset : UNSIGNED_ERROR;
}

/**
 * Gets the CPU type targeted by a binary file
 * \param macho_file A format-specific representation of a binary file
 */
cpu_type_t macho_file_getcpu_type(macho_file_t* macho_file) {
   return (macho_file) ? macho_file->cpu_type : SIGNED_ERROR;
}

/**
 * Gets the CPU sub-type targeted by a binary file
 * \param macho_file A format-specific representation of a binary file
 */
cpu_subtype_t macho_file_getcpu_subtype(macho_file_t* macho_file) {
   return (macho_file) ? macho_file->cpu_subtype : SIGNED_ERROR;
}

/**
 * Gets the queue of bindings of a Mach-O binary file
 * \param macho_file A format-specific representation of a binary file
 * \param The queue of bindings
 */
queue_t* macho_file_getbindings(macho_file_t* macho_file) {
   return (macho_file) ? macho_file->bindings : NULL;
}

/**
 * Gets the queue of lazy bindings of a Mach-O binary file
 * \param macho_file A format-specific representation of a binary file
 * \param The queue of lazy bindings
 */
queue_t* macho_file_getlazy_bindings(macho_file_t* macho_file) {
   return (macho_file) ? macho_file->lazy_bindings : NULL;
}

/**
 * Gets the queue of weak bindings of a Mach-O binary file
 * \param macho_file A format-specific representation of a binary file
 * \return The queue of weak bindings
 */
queue_t* macho_file_getweak_bindings(macho_file_t* macho_file) {
   return (macho_file) ? macho_file->weak_bindings : NULL;
}

/**
 * Prints the macho-specific representation of a binary file
 * \param macho_file A format-specific representation of a binary file
 */
void macho_file_print(macho_file_t* macho_file) {
   if (!macho_file)
      return;

   int i;

   switch (macho_file->cpu_type) {
   case(CPU_TYPE_ANY):
      printf("%-45s %s\n", "CPU type:", "ANY");
      break;

   case(CPU_TYPE_VAX):
      printf("%-45s %s\n", "CPU type:", "VAX");
      switch(macho_file->cpu_subtype) {
      case(CPU_SUBTYPE_VAX_ALL):          printf("%-45s %s\n", "CPU sub-type:", "VAX_ALL");           break;
      case(CPU_SUBTYPE_VAX780):           printf("%-45s %s\n", "CPU sub-type:", "VAX780");            break;
      case(CPU_SUBTYPE_VAX785):           printf("%-45s %s\n", "CPU sub-type:", "VAX785");            break;
      case(CPU_SUBTYPE_VAX750):           printf("%-45s %s\n", "CPU sub-type:", "VAX750");            break;
      case(CPU_SUBTYPE_VAX730):           printf("%-45s %s\n", "CPU sub-type:", "VAX730");            break;
      case(CPU_SUBTYPE_UVAXI):            printf("%-45s %s\n", "CPU sub-type:", "UVAXI");             break;
      case(CPU_SUBTYPE_UVAXII):           printf("%-45s %s\n", "CPU sub-type:", "UVAXII");            break;
      case(CPU_SUBTYPE_VAX8200):          printf("%-45s %s\n", "CPU sub-type:", "VAX8200");           break;
      case(CPU_SUBTYPE_VAX8500):          printf("%-45s %s\n", "CPU sub-type:", "VAX8500");           break;
      case(CPU_SUBTYPE_VAX8600):          printf("%-45s %s\n", "CPU sub-type:", "VAX8600");           break;
      case(CPU_SUBTYPE_VAX8650):          printf("%-45s %s\n", "CPU sub-type:", "VAX8650");           break;
      case(CPU_SUBTYPE_VAX8800):          printf("%-45s %s\n", "CPU sub-type:", "VAX8800");           break;
      case(CPU_SUBTYPE_UVAXIII):          printf("%-45s %s\n", "CPU sub-type:", "UVAXIII");           break;
      default:                            printf("%-45s %s\n", "CPU sub-type:", "UNKNOWN");           break;
      }
      break;

   case(CPU_TYPE_MC680x0):
      printf("%-45s %s\n", "CPU type:", "MC680x0");
      switch(macho_file->cpu_subtype) {
      case(CPU_SUBTYPE_MC680x0_ALL):      printf("%-45s %s\n", "CPU sub-type:", "MC680x0_ALL");       break;
      case(CPU_SUBTYPE_MC68040):          printf("%-45s %s\n", "CPU sub-type:", "MC68040");           break;
      case(CPU_SUBTYPE_MC68030_ONLY):     printf("%-45s %s\n", "CPU sub-type:", "MC68030_ONLY");      break;
      default:                            printf("%-45s %s\n", "CPU sub-type:", "UNKNOWN");           break;
      }
      break;

   case(CPU_TYPE_I386):
      printf("%-45s %s\n", "CPU type:", "I386");
      switch(macho_file->cpu_subtype) {
      case(CPU_SUBTYPE_I386_ALL):         printf("%-45s %s\n", "CPU sub-type:", "I386_ALL");          break;
      case(CPU_SUBTYPE_486):              printf("%-45s %s\n", "CPU sub-type:", "486");               break;
      case(CPU_SUBTYPE_486SX):            printf("%-45s %s\n", "CPU sub-type:", "486SX");             break;
      case(CPU_SUBTYPE_586):              printf("%-45s %s\n", "CPU sub-type:", "586");               break;
      case(CPU_SUBTYPE_PENTPRO):          printf("%-45s %s\n", "CPU sub-type:", "PENTPRO");           break;
      case(CPU_SUBTYPE_PENTII_M3):        printf("%-45s %s\n", "CPU sub-type:", "PENTII_M3");         break;
      case(CPU_SUBTYPE_PENTII_M5):        printf("%-45s %s\n", "CPU sub-type:", "PENTII_M5");         break;
      case(CPU_SUBTYPE_CELERON):          printf("%-45s %s\n", "CPU sub-type:", "CELERON");           break;
      case(CPU_SUBTYPE_CELERON_MOBILE):   printf("%-45s %s\n", "CPU sub-type:", "CELERON_MOBILE");    break;
      case(CPU_SUBTYPE_PENTIUM_3):        printf("%-45s %s\n", "CPU sub-type:", "PENTIUM_3");         break;
      case(CPU_SUBTYPE_PENTIUM_3_M):      printf("%-45s %s\n", "CPU sub-type:", "PENTIUM_3_M");       break;
      case(CPU_SUBTYPE_PENTIUM_3_XEON):   printf("%-45s %s\n", "CPU sub-type:", "PENTIUM_3_XEON");    break;
      case(CPU_SUBTYPE_PENTIUM_M):        printf("%-45s %s\n", "CPU sub-type:", "PENTIUM_M");         break;
      case(CPU_SUBTYPE_PENTIUM_4):        printf("%-45s %s\n", "CPU sub-type:", "PENTIUM_4");         break;
      case(CPU_SUBTYPE_PENTIUM_4_M):      printf("%-45s %s\n", "CPU sub-type:", "PENTIUM_4_M");       break;
      case(CPU_SUBTYPE_ITANIUM):          printf("%-45s %s\n", "CPU sub-type:", "ITANIUM");           break;
      case(CPU_SUBTYPE_ITANIUM_2):        printf("%-45s %s\n", "CPU sub-type:", "ITANIUM_2");         break;
      case(CPU_SUBTYPE_XEON):             printf("%-45s %s\n", "CPU sub-type:", "XEON");              break;
      case(CPU_SUBTYPE_XEON_MP):          printf("%-45s %s\n", "CPU sub-type:", "XEON_MP");           break;
      default:                            printf("%-45s %s\n", "CPU sub-type:", "UNKNOWN");           break;
      }
      break;

   case(CPU_TYPE_X86_64):
      printf("%-45s %s\n", "CPU type:", "X86_64");
      switch(macho_file->cpu_subtype) {
      case(CPU_SUBTYPE_I386_ALL):         printf("%-45s %s\n", "CPU sub-type:", "I386_ALL");          break;
      case(CPU_SUBTYPE_486):              printf("%-45s %s\n", "CPU sub-type:", "486");               break;
      case(CPU_SUBTYPE_486SX):            printf("%-45s %s\n", "CPU sub-type:", "486SX");             break;
      case(CPU_SUBTYPE_586):              printf("%-45s %s\n", "CPU sub-type:", "586");               break;
      case(CPU_SUBTYPE_PENTPRO):          printf("%-45s %s\n", "CPU sub-type:", "PENTPRO");           break;
      case(CPU_SUBTYPE_PENTII_M3):        printf("%-45s %s\n", "CPU sub-type:", "PENTII_M3");         break;
      case(CPU_SUBTYPE_PENTII_M5):        printf("%-45s %s\n", "CPU sub-type:", "PENTII_M5");         break;
      case(CPU_SUBTYPE_CELERON):          printf("%-45s %s\n", "CPU sub-type:", "CELERON");           break;
      case(CPU_SUBTYPE_CELERON_MOBILE):   printf("%-45s %s\n", "CPU sub-type:", "CELERON_MOBILE");    break;
      case(CPU_SUBTYPE_PENTIUM_3):        printf("%-45s %s\n", "CPU sub-type:", "PENTIUM_3");         break;
      case(CPU_SUBTYPE_PENTIUM_3_M):      printf("%-45s %s\n", "CPU sub-type:", "PENTIUM_3_M");       break;
      case(CPU_SUBTYPE_PENTIUM_3_XEON):   printf("%-45s %s\n", "CPU sub-type:", "PENTIUM_3_XEON");    break;
      case(CPU_SUBTYPE_PENTIUM_M):        printf("%-45s %s\n", "CPU sub-type:", "PENTIUM_M");         break;
      case(CPU_SUBTYPE_PENTIUM_4):        printf("%-45s %s\n", "CPU sub-type:", "PENTIUM_4");         break;
      case(CPU_SUBTYPE_PENTIUM_4_M):      printf("%-45s %s\n", "CPU sub-type:", "PENTIUM_4_M");       break;
      case(CPU_SUBTYPE_ITANIUM):          printf("%-45s %s\n", "CPU sub-type:", "ITANIUM");           break;
      case(CPU_SUBTYPE_ITANIUM_2):        printf("%-45s %s\n", "CPU sub-type:", "ITANIUM_2");         break;
      case(CPU_SUBTYPE_XEON):             printf("%-45s %s\n", "CPU sub-type:", "XEON");              break;
      case(CPU_SUBTYPE_XEON_MP):          printf("%-45s %s\n", "CPU sub-type:", "XEON_MP");           break;
      default:                            printf("%-45s %s\n", "CPU sub-type:", "UNKNOWN");           break;
      }
      break;

   case(CPU_TYPE_MC98000):
      printf("%-45s %s\n", "CPU type:", "MC98000");
      switch(macho_file->cpu_subtype) {
      case(CPU_SUBTYPE_MC98000_ALL):      printf("%-45s %s\n", "CPU sub-type:", "MC98000_ALL");       break;
      case(CPU_SUBTYPE_MC98601):          printf("%-45s %s\n", "CPU sub-type:", "MC98601");           break;
      default:                            printf("%-45s %s\n", "CPU sub-type:", "UNKNOWN");           break;
      }
      break;

   case(CPU_TYPE_HPPA):
      printf("%-45s %s\n", "CPU type:", "HPPA");
      switch(macho_file->cpu_subtype) {
      case(CPU_SUBTYPE_HPPA_ALL):         printf("%-45s %s\n", "CPU sub-type:", "HPPA_ALL");          break;
      case(CPU_SUBTYPE_HPPA_7100LC):      printf("%-45s %s\n", "CPU sub-type:", "HPPA_7100LC");       break;
      default:                            printf("%-45s %s\n", "CPU sub-type:", "UNKNOWN");           break;
      }
      break;

   case(CPU_TYPE_ARM):
      printf("%-45s %s\n", "CPU type:", "ARM");
      switch(macho_file->cpu_subtype) {
      case(CPU_SUBTYPE_ARM_ALL):          printf("%-45s %s\n", "CPU sub-type:", "ARM_ALL");           break;
      case(CPU_SUBTYPE_ARM_V4T):          printf("%-45s %s\n", "CPU sub-type:", "ARM_V4T");           break;
      case(CPU_SUBTYPE_ARM_V6):           printf("%-45s %s\n", "CPU sub-type:", "ARM_V6");            break;
      case(CPU_SUBTYPE_ARM_V5TEJ):        printf("%-45s %s\n", "CPU sub-type:", "ARM_V5TEJ");         break;
      case(CPU_SUBTYPE_ARM_XSCALE):       printf("%-45s %s\n", "CPU sub-type:", "ARM_XSCALE");        break;
      case(CPU_SUBTYPE_ARM_V7):           printf("%-45s %s\n", "CPU sub-type:", "ARM_V7");            break;
      case(CPU_SUBTYPE_ARM_V7F):          printf("%-45s %s\n", "CPU sub-type:", "ARM_V7F");           break;
      case(CPU_SUBTYPE_ARM_V7K):          printf("%-45s %s\n", "CPU sub-type:", "ARM_V7K");           break;
      default:                            printf("%-45s %s\n", "CPU sub-type:", "UNKNOWN");           break;
      }
      break;

   case(CPU_TYPE_MC88000):
      printf("%-45s %s\n", "CPU type:", "MC88000");
      switch(macho_file->cpu_subtype) {
      case(CPU_SUBTYPE_MC88000_ALL):      printf("%-45s %s\n", "CPU sub-type:", "MC88000_ALL");       break;
      case(CPU_SUBTYPE_MC88100):          printf("%-45s %s\n", "CPU sub-type:", "MC88100");           break;
      case(CPU_SUBTYPE_MC88110):          printf("%-45s %s\n", "CPU sub-type:", "MC88110");           break;
      default:                            printf("%-45s %s\n", "CPU sub-type:", "UNKNOWN");           break;
      }
      break;

   case(CPU_TYPE_SPARC):
      printf("%-45s %s\n", "CPU type:", "SPARC");
      switch(macho_file->cpu_subtype) {
      case(CPU_SUBTYPE_SPARC_ALL):        printf("%-45s %s\n", "CPU sub-type:", "SPARC_ALL");         break;
      default:                            printf("%-45s %s\n", "CPU sub-type:", "UNKNOWN");           break;
      }
      break;

   case(CPU_TYPE_I860):
      printf("%-45s %s\n", "CPU type:", "I860");
      switch(macho_file->cpu_subtype) {
      case(CPU_SUBTYPE_I860_ALL):         printf("%-45s %s\n", "CPU sub-type:", "I860_ALL");          break;
      case(CPU_SUBTYPE_I860_860):         printf("%-45s %s\n", "CPU sub-type:", "I860_860");          break;
      default:                            printf("%-45s %s\n", "CPU sub-type:", "UNKNOWN");           break;
      }
      break;

   case(CPU_TYPE_POWERPC):
      printf("%-45s %s\n", "CPU type:", "POWERPC");
      switch(macho_file->cpu_subtype){
      case(CPU_SUBTYPE_POWERPC_ALL):      printf("%-45s %s\n", "CPU sub-type:", "POWERPC_ALL");       break;
      case(CPU_SUBTYPE_POWERPC_601):      printf("%-45s %s\n", "CPU sub-type:", "POWERPC_601");       break;
      case(CPU_SUBTYPE_POWERPC_602):      printf("%-45s %s\n", "CPU sub-type:", "POWERPC_602");       break;
      case(CPU_SUBTYPE_POWERPC_603):      printf("%-45s %s\n", "CPU sub-type:", "POWERPC_603");       break;
      case(CPU_SUBTYPE_POWERPC_603e):     printf("%-45s %s\n", "CPU sub-type:", "POWERPC_603e");      break;
      case(CPU_SUBTYPE_POWERPC_603ev):    printf("%-45s %s\n", "CPU sub-type:", "POWERPC_603ev");     break;
      case(CPU_SUBTYPE_POWERPC_604):      printf("%-45s %s\n", "CPU sub-type:", "POWERPC_604");       break;
      case(CPU_SUBTYPE_POWERPC_604e):     printf("%-45s %s\n", "CPU sub-type:", "POWERPC_604e");      break;
      case(CPU_SUBTYPE_POWERPC_620):      printf("%-45s %s\n", "CPU sub-type:", "POWERPC_620");       break;
      case(CPU_SUBTYPE_POWERPC_750):      printf("%-45s %s\n", "CPU sub-type:", "POWERPC_750");       break;
      case(CPU_SUBTYPE_POWERPC_7400):     printf("%-45s %s\n", "CPU sub-type:", "POWERPC_7400");      break;
      case(CPU_SUBTYPE_POWERPC_7450):     printf("%-45s %s\n", "CPU sub-type:", "POWERPC_7450");      break;
      case(CPU_SUBTYPE_POWERPC_970):      printf("%-45s %s\n", "CPU sub-type:", "POWERPC_970");       break;
      default:                            printf("%-45s %s\n", "CPU sub-type:", "UNKNOWN");           break;
      }
      break;

   case(CPU_TYPE_POWERPC64):
      printf("%-45s %s\n", "CPU type:", "POWERPC64");
      switch(macho_file->cpu_subtype){
      case(CPU_SUBTYPE_POWERPC_ALL):      printf("%-45s %s\n", "CPU sub-type:", "POWERPC_ALL");       break;
      case(CPU_SUBTYPE_POWERPC_601):      printf("%-45s %s\n", "CPU sub-type:", "POWERPC_601");       break;
      case(CPU_SUBTYPE_POWERPC_602):      printf("%-45s %s\n", "CPU sub-type:", "POWERPC_602");       break;
      case(CPU_SUBTYPE_POWERPC_603):      printf("%-45s %s\n", "CPU sub-type:", "POWERPC_603");       break;
      case(CPU_SUBTYPE_POWERPC_603e):     printf("%-45s %s\n", "CPU sub-type:", "POWERPC_603e");      break;
      case(CPU_SUBTYPE_POWERPC_603ev):    printf("%-45s %s\n", "CPU sub-type:", "POWERPC_603ev");     break;
      case(CPU_SUBTYPE_POWERPC_604):      printf("%-45s %s\n", "CPU sub-type:", "POWERPC_604");       break;
      case(CPU_SUBTYPE_POWERPC_604e):     printf("%-45s %s\n", "CPU sub-type:", "POWERPC_604e");      break;
      case(CPU_SUBTYPE_POWERPC_620):      printf("%-45s %s\n", "CPU sub-type:", "POWERPC_620");       break;
      case(CPU_SUBTYPE_POWERPC_750):      printf("%-45s %s\n", "CPU sub-type:", "POWERPC_750");       break;
      case(CPU_SUBTYPE_POWERPC_7400):     printf("%-45s %s\n", "CPU sub-type:", "POWERPC_7400");      break;
      case(CPU_SUBTYPE_POWERPC_7450):     printf("%-45s %s\n", "CPU sub-type:", "POWERPC_7450");      break;
      case(CPU_SUBTYPE_POWERPC_970):      printf("%-45s %s\n", "CPU sub-type:", "POWERPC_970");       break;
      default:                            printf("%-45s %s\n", "CPU sub-type:", "UNKNOWN");           break;
      }
      break;
   default: printf("%-45s %s\n", "CPU type:", "UNKNOWN");  break;
   }

   printf("%-45s %d\n", "Number of commands:", macho_file_getn_commands(macho_file));
   printf("%-45s %#"PRIx32"\n\n", "Size of commands:", macho_file_getcommands_size(macho_file));

   printf("Commands -------------------------------------------------\n");
   printf("%-45s %s\n", "   Command", "Size");
   printf("----------------------------------------------------------\n");

   for (i = 0; i < (int)macho_file_getn_commands(macho_file); i++) {
      load_command_t* cmd = (load_command_t*)macho_file_getcommand(macho_file, i);
      switch(cmd->cmd) {
      case(LC_SEGMENT):                   printf("%-45s %d bytes\n", " - LC_SEGMENT", cmd->cmdsize);                break;
      case(LC_SYMTAB):                    printf("%-45s %d bytes\n", " - LC_SYMTAB", cmd->cmdsize);                 break;
      case(LC_SYMSEG):                    printf("%-45s %d bytes\n", " - LC_SYMSEG", cmd->cmdsize);                 break;
      case(LC_THREAD):                    printf("%-45s %d bytes\n", " - LC_THREAD", cmd->cmdsize);                 break;
      case(LC_UNIXTHREAD):                printf("%-45s %d bytes\n", " - LC_UNIXTHREAD", cmd->cmdsize);             break;
      case(LC_LOADFVMLIB):                printf("%-45s %d bytes\n", " - LC_LOADFVMLIB", cmd->cmdsize);             break;
      case(LC_IDFVMLIB):                  printf("%-45s %d bytes\n", " - LC_IDFVMLIB", cmd->cmdsize);               break;
      case(LC_IDENT):                     printf("%-45s %d bytes\n", " - LC_IDENT", cmd->cmdsize);                  break;
      case(LC_FVMFILE):                   printf("%-45s %d bytes\n", " - LC_FVMFILE", cmd->cmdsize);                break;
      case(LC_PREPAGE):                   printf("%-45s %d bytes\n", " - LC_PREPAGE", cmd->cmdsize);                break;
      case(LC_DYSYMTAB):                  printf("%-45s %d bytes\n", " - LC_DYSYMTAB", cmd->cmdsize);               break;
      case(LC_LOAD_DYLIB):                printf("%-45s %d bytes\n", " - LC_LOAD_DYLIB", cmd->cmdsize);             break;
      case(LC_ID_DYLIB):                  printf("%-45s %d bytes\n", " - LC_ID_DYLIB", cmd->cmdsize);               break;
      case(LC_LOAD_DYLINKER):             printf("%-45s %d bytes\n", " - LC_LOAD_DYLINKER", cmd->cmdsize);          break;
      case(LC_ID_DYLINKER):               printf("%-45s %d bytes\n", " - LC_ID_DYLINKER", cmd->cmdsize);            break;
      case(LC_PREBOUND_DYLIB):            printf("%-45s %d bytes\n", " - LC_PREBOUND_DYLIB", cmd->cmdsize);         break;
      case(LC_ROUTINES):                  printf("%-45s %d bytes\n", " - LC_ROUTINES", cmd->cmdsize);               break;
      case(LC_SUB_FRAMEWORK):             printf("%-45s %d bytes\n", " - LC_SUB_FRAMEWORK", cmd->cmdsize);          break;
      case(LC_SUB_UMBRELLA):              printf("%-45s %d bytes\n", " - LC_SUB_UMBRELLA", cmd->cmdsize);           break;
      case(LC_SUB_CLIENT):                printf("%-45s %d bytes\n", " - LC_SUB_CLIENT", cmd->cmdsize);             break;
      case(LC_SUB_LIBRARY):               printf("%-45s %d bytes\n", " - LC_SUB_LIBRARY", cmd->cmdsize);            break;
      case(LC_TWOLEVEL_HINTS):            printf("%-45s %d bytes\n", " - LC_TWOLEVEL_HINTS", cmd->cmdsize);         break;
      case(LC_PREBIND_CKSUM):             printf("%-45s %d bytes\n", " - LC_PREBIND_CKSUM", cmd->cmdsize);          break;
      case(LC_LOAD_WEAK_DYLIB):           printf("%-45s %d bytes\n", " - LC_LOAD_WEAK_DYLIB", cmd->cmdsize);        break;
      case(LC_SEGMENT_64):                printf("%-45s %d bytes\n", " - LC_SEGMENT_64", cmd->cmdsize);             break;
      case(LC_ROUTINES_64):               printf("%-45s %d bytes\n", " - LC_ROUTINES_64", cmd->cmdsize);            break;
      case(LC_UUID):                      printf("%-45s %d bytes\n", " - LC_UUID", cmd->cmdsize);                   break;
      case(LC_RPATH):                     printf("%-45s %d bytes\n", " - LC_RPATH", cmd->cmdsize);                  break;
      case(LC_CODE_SIGNATURE):            printf("%-45s %d bytes\n", " - LC_CODE_SIGNATURE", cmd->cmdsize);         break;
      case(LC_SEGMENT_SPLIT_INFO):        printf("%-45s %d bytes\n", " - LC_SEGMENT_SPLIT_INFO", cmd->cmdsize);     break;
      case(LC_REEXPORT_DYLIB):            printf("%-45s %d bytes\n", " - LC_REEXPORT_DYLIB", cmd->cmdsize);         break;
      case(LC_LAZY_LOAD_DYLIB):           printf("%-45s %d bytes\n", " - LC_LAZY_LOAD_DYLIB", cmd->cmdsize);        break;
      case(LC_ENCRYPTION_INFO):           printf("%-45s %d bytes\n", " - LC_ENCRYPTION_INFO", cmd->cmdsize);        break;
      case(LC_DYLD_INFO):                 printf("%-45s %d bytes\n", " - LC_DYLD_INFO", cmd->cmdsize);              break;
      case(LC_DYLD_INFO_ONLY):            printf("%-45s %d bytes\n", " - LC_DYLD_INFO_ONLY", cmd->cmdsize);         break;
      case(LC_LOAD_UPWARD_DYLIB):         printf("%-45s %d bytes\n", " - LC_LOAD_UPWARD_DYLIB", cmd->cmdsize);      break;
      case(LC_VERSION_MIN_MACOSX):        printf("%-45s %d bytes\n", " - LC_VERSION_MIN_MACOSX", cmd->cmdsize);     break;
      case(LC_VERSION_MIN_IPHONEOS):      printf("%-45s %d bytes\n", " - LC_VERSION_MIN_IPHONEOS", cmd->cmdsize);   break;
      case(LC_FUNCTION_STARTS):           printf("%-45s %d bytes\n", " - LC_FUNCTION_STARTS", cmd->cmdsize);        break;
      case(LC_DYLD_ENVIRONMENT):          printf("%-45s %d bytes\n", " - LC_DYLD_ENVIRONMENT", cmd->cmdsize);       break;
      case(LC_MAIN):                      printf("%-45s %d bytes\n", " - LC_MAIN", cmd->cmdsize);                   break;
      case(LC_DATA_IN_CODE):              printf("%-45s %d bytes\n", " - LC_DATA_IN_CODE", cmd->cmdsize);           break;
      case(LC_SOURCE_VERSION):            printf("%-45s %d bytes\n", " - LC_SOURCE_VERSION", cmd->cmdsize);         break;
      case(LC_DYLIB_CODE_SIGN_DRS):       printf("%-45s %d bytes\n", " - LC_DYLIB_CODE_SIGN_DRS", cmd->cmdsize);    break;
      default:                            printf("%-45s\n", "UNKNOWN");                                             break;
      }
   }
   printf("----------------------------------------------------------\n\n");

   printf("Segments -------------------------------------------------\n");
   printf("%-20s %-10s %-10s %-10s %-10s %-10s %-8s %-8s %-8s %-5s\n",
         "Name", "Offset", "Size", "VirtStart", "VirtEnd", "VirtSize", "InitProt", "MaxProt", "Flags", "NSections");
   printf("----------------------------------------------------------------------------------------------------------------\n");

   for (i = 0; i < (int)macho_file_getn_segments(macho_file); i++) {
      macho_segment_t* seg = macho_file_getsegment(macho_file, i);
      vm_prot_t protection;
      char prot[8];

      printf("%-20s ", macho_segment_getname(seg));
      printf("%#-10"PRIx64" ", macho_segment_getoffset(seg));
      printf("%#-10"PRIx64" ", macho_segment_getsize(seg));
      printf("%#-10"PRIx64" ", macho_segment_getvmaddress(seg));
      printf("%#-10"PRIx64" ", macho_segment_getvmaddress(seg) + macho_segment_getvmsize(seg));
      printf("%#-10"PRIx64" ", macho_segment_getvmsize(seg));

      protection = macho_segment_getinitprot(seg);
      sprintf(prot, "%c%c%c", (protection&VM_PROT_READ)?'R':'-', (protection&VM_PROT_WRITE)?'W':'-',
            (protection&VM_PROT_EXECUTE)?'X':'-');
      printf("%-8s ", prot);

      protection = macho_segment_getmaxprot(seg);
      sprintf(prot, "%c%c%c", (protection&VM_PROT_READ)?'R':'-', (protection&VM_PROT_WRITE)?'W':'-',
            (protection&VM_PROT_EXECUTE)?'X':'-');
      printf("%-8s ", prot);

      printf("%#-8"PRIx32" ", macho_segment_getflags(seg));
      printf("%-5d\n", macho_segment_getn_sections(seg));
   }
   printf("----------------------------------------------------------------------------------------------------------------\n");
   printf("Key to Protection:\n"
         " R (read), W (write), X (execute)\n\n");

   printf("Sections -------------------------------------------------\n");
   printf("%-20s %-15s %-10s %-10s %-10s %-10s %-10s %-6s %-6s %-6s\n",
         "Name", "Segment", "Type", "Offset", "VirtStart", "VirtEnd", "VirtSize", "EntSz", "Align", "Flags");
   printf("----------------------------------------------------------------------------------------------------------------\n");
   for (i = 0; i < (int)macho_file_getn_segments(macho_file); i++) {
      macho_segment_t* seg = macho_file_getsegment(macho_file, i);
      int j;

      for (j = 0; j < (int)macho_segment_getn_sections(seg); j++) {
         binscn_t* scn = macho_segment_getbinsection(seg, j);

         printf("%-20s ", binscn_get_name(scn));
         printf("%-15s ", macho_segment_getname(seg));

         switch(binscn_get_type(scn)) {
         case(SCNT_CODE):     printf("%-10s ", "PROGBITS");       break;
         case(SCNT_ZERODATA):
         case(SCNT_DATA):     printf("%-10s ", "DATA");           break;
         case(SCNT_STRING):   printf("%-10s ", "STRINGS");        break;
         case(SCNT_DEBUG):    printf("%-10s ", "DEBUG");          break;
         default:             printf("%-10s ", "NULL");
         }

         printf("%#-10"PRIx64" ", binscn_get_offset(scn));
         printf("%#-10"PRIx64" ", binscn_get_addr(scn));
         printf("%#-10"PRIx64" ", binscn_get_addr(scn) + binscn_get_size(scn));
         printf("%#-10"PRIx64" ", binscn_get_size(scn));
         printf("%#-6"PRIx64" ", binscn_get_entry_size(scn));
         printf("%#-6"PRIx64" ", binscn_get_align(scn));
         printf("%-6s\n", "TODO");
      }
   }
   printf("----------------------------------------------------------------------------------------------------------------\n");

}

/**
 * Returns the first loaded address
 * \param macho_file A format-specific representation of a binary file
 * \return The first loaded address of this binary file
 */
int64_t macho_file_getfirstloadaddr(macho_file_t* macho_file) {

   if (!macho_file)
      return SIGNED_ERROR;

   int memory_startaddress = SIGNED_ERROR, i;

   for (i = 0; i < (int)macho_file_getn_segments(macho_file); i++) {
      macho_segment_t* seg = macho_file_getsegment(macho_file, i);
      int64_t seg_address  = macho_segment_getvmaddress(seg);

      if (memory_startaddress == SIGNED_ERROR || seg_address < memory_startaddress)
         memory_startaddress = seg_address;
   }

   return memory_startaddress;
}

/**
 * Returns the last loaded address
 * \param macho_file A format-specific representation of a binary file
 * \return The last loaded address of this binary file
 */
int64_t macho_file_getlastloadaddr(macho_file_t* macho_file) {

   if (!macho_file)
      return SIGNED_ERROR;

   int memory_lastaddress = SIGNED_ERROR, i;

   for (i = 0; i < (int)macho_file_getn_segments(macho_file); i++) {
      macho_segment_t* seg = macho_file_getsegment(macho_file, i);
      int64_t seg_lastaddress  = macho_segment_getvmaddress(seg) + macho_segment_getvmsize(seg);

      if (memory_lastaddress == SIGNED_ERROR || seg_lastaddress > memory_lastaddress)
         memory_lastaddress = seg_lastaddress;
   }

   return memory_lastaddress;
}

/**
 * Add a segment to the macho-specific structure representing a binary file
 * \param macho_file A format-specific representation of a binary file
 * \param seg The segment to add
 */
void macho_file_addsegment(macho_file_t* macho_file, macho_segment_t* seg) {

   if(macho_file && seg) {

      //Gets space for the new segment
      macho_file->n_segments++;
      macho_file->segments = (macho_segment_t**)lc_realloc(macho_file->segments, macho_file->n_segments * sizeof(void*));

      //Adds the new segment
      macho_file->segments[macho_file->n_segments - 1] = seg;

   }
}

/**
 * Add a command to the macho-specific structure representing a binary file
 * \param macho_file A format-specific representation of a binary file
 * \param command The command to add
 */
void macho_file_addcommand(macho_file_t* macho_file, void* command) {

   if(macho_file && command) {

      //Gets space for the new commant
      macho_file->n_commands++;
      macho_file->commands = (void**)lc_realloc(macho_file->commands, macho_file->n_commands * sizeof(void*));

      //Adds the new segment
      macho_file->commands[macho_file->n_commands - 1] = command;

   }
}

/**
 * Loads the commands of a macho binary file.
 * It calls the sub-procedures depending on the wordsize of the binary file
 * \param filedesc A file descriptor
 * \param macho_file A format-specific representation of a binary file
 */
void macho_file_loadcommands(int filedesc, macho_file_t* macho_file) {

   if (filedesc < 0 || !macho_file)
      return;

   uint32_t i = 0;

   //Getting the binfile
   binfile_t* bf = macho_file_getbinfile(macho_file);

   //Getting the queue of commands ordered by their data
   queue_t* data_chunks       = macho_file_getdata_chunks(macho_file);
   uint32_t last_address      = 0;
   uint32_t start_address     = 0;

   DBGMSG("Binary has %d commands for a size of %d bytes:\n", macho_file_getn_commands(macho_file)
         , macho_file->commands_size);

   //Parse each command
   for (i = 0; i < macho_file_getn_commands(macho_file); i++) {

      //Parse a "load_command" to figure out what is the real command
      load_command_t* command = (load_command_t*)lc_malloc0(sizeof(load_command_t));
      SAFE(read(filedesc, command, sizeof(load_command_t)));

      //Moving back (the load command fields are common to all commands)
      SAFE(lseek(filedesc, -sizeof(load_command_t), SEEK_CUR));

      switch(REV_BYT(command->cmd)) {

         /* Segment of this file to be mapped */
         case(LC_SEGMENT):
         {
            int read_bytes = 0;

            READ_COMMAND(filedesc, segment_command32_t, sgt_cmd);

            //Creating a new segment
            macho_segment_t* seg = macho_segment_new(sgt_cmd);
            macho_segment_setname(seg, lc_strdup((char*)REV_BYT(sgt_cmd->segname)));
            macho_segment_setn_sections(seg, REV_BYT(sgt_cmd->nsects));
            macho_segment_setoffset(seg, REV_BYT(sgt_cmd->fileoff));
            macho_segment_setsize(seg, REV_BYT(sgt_cmd->filesize));
            macho_segment_setvmaddress(seg, REV_BYT(sgt_cmd->vmaddr));
            macho_segment_setvmsize(seg, REV_BYT(sgt_cmd->vmsize));
            macho_segment_setinitprot(seg, REV_BYT(sgt_cmd->initprot));
            macho_segment_setmaxprot(seg, REV_BYT(sgt_cmd->maxprot));
            macho_segment_setflags(seg, REV_BYT(sgt_cmd->flags));

            //Creates the libbin representation and keeps a link to it
            binseg_t* binseg = macho_binseg_newfromseg(bf, seg);
            macho_segment_setbinseg(seg, binseg);

            macho_file_setcommand(macho_file, i, (void*)sgt_cmd);
            macho_file_addsegment(macho_file, seg);

            //Reading sections
            if (macho_segment_getn_sections(seg) > 0)
               read_bytes = macho_segment_loadsections32(filedesc, bf, seg);

            //Verifying that we read all the data corresponding to this command
            if (REV_BYT(sgt_cmd->cmdsize) - sizeof(segment_command32_t) - read_bytes > 0) {
               SAFE(lseek(filedesc, REV_BYT(sgt_cmd->cmdsize) - sizeof(segment_command32_t) - read_bytes, SEEK_CUR));
               WRNMSG("There is still unread bytes after sections parsing. Reading %lu bytes further.\n",
                     REV_BYT(sgt_cmd->cmdsize) - sizeof(segment_command32_t) - read_bytes);
            }

            /* Ordering this command
             * We only take into account the sections !
             * WARNING, it supposes possible information at the beginning of the segment
             * but not between sections
             */
            if (macho_segment_getbinsection(seg, 0)) {

               start_address = binscn_get_offset(macho_segment_getbinsection(seg, 0));
               ORDER(data_chunks, sgt_cmd, start_address, macho_segment_getoffset(seg) + macho_segment_getsize(seg),
                     last_address);
            }
            break;
         }

         /* link-edit stab symbol table info */
         case(LC_SYMTAB):;

            READ_COMMAND(filedesc, symtab_command_t, symtab_cmd);
            macho_file_setcommand(macho_file, i, (void*)symtab_cmd);

            start_address = REV_BYT(symtab_cmd->stroff);
            ORDER(data_chunks, symtab_cmd, start_address, start_address + REV_BYT(symtab_cmd->strsize),
                  last_address);
            unsigned char* str_table = (unsigned char*)lc_malloc0(REV_BYT(symtab_cmd->strsize));
            int position = lseek(filedesc, 0, SEEK_CUR);
            SAFE(lseek(filedesc, start_address, SEEK_SET));
            SAFE(read(filedesc, str_table, REV_BYT(symtab_cmd->strsize)));
            SAFE(lseek(filedesc, position, SEEK_SET));

            start_address = REV_BYT(symtab_cmd->symoff);
            if (binfile_get_word_size(bf) == BFS_32BITS) {

               ORDER(data_chunks, symtab_cmd, start_address, start_address + (REV_BYT(symtab_cmd->nsyms) * sizeof(nlist32_t)),
                  last_address);

               //Getting symbol table
               unsigned char* sym_table = (unsigned char*)lc_malloc0(REV_BYT(symtab_cmd->nsyms) * sizeof(nlist32_t));
               int position = lseek(filedesc, 0, SEEK_CUR);
               SAFE(lseek(filedesc, start_address, SEEK_SET));
               SAFE(read(filedesc, sym_table, REV_BYT(symtab_cmd->nsyms) * sizeof(nlist32_t)));
               SAFE(lseek(filedesc, position, SEEK_SET));

               read_symbol32(sym_table, REV_BYT(symtab_cmd->nsyms), str_table);

            } else if (binfile_get_word_size(bf) == BFS_64BITS) {

               ORDER(data_chunks, symtab_cmd, start_address, start_address + (REV_BYT(symtab_cmd->nsyms) * sizeof(nlist64_t)),
                  last_address);

               //Getting symbol table
               unsigned char* sym_table = (unsigned char*)lc_malloc0(REV_BYT(symtab_cmd->nsyms) * sizeof(nlist64_t));
               int position = lseek(filedesc, 0, SEEK_CUR);
               SAFE(lseek(filedesc, start_address, SEEK_SET));
               SAFE(read(filedesc, sym_table, REV_BYT(symtab_cmd->nsyms) * sizeof(nlist64_t)));
               SAFE(lseek(filedesc, position, SEEK_SET));

               DBGMSGLVL(2, "Symtab64: %"PRId64" symbols\n", REV_BYT(symtab_cmd->nsyms));
               read_symbol64(sym_table, REV_BYT(symtab_cmd->nsyms), str_table);
            }

            break;

         /* link-edit gdb symbol table info (obsolete) */
         case(LC_SYMSEG):;

            READ_COMMAND(filedesc, symseg_command_t, symseg_cmd);
            macho_file_setcommand(macho_file, i, (void*)symseg_cmd);
            WRNMSG("Found an obsolete command (symseg) !\n");
            break;

         /* unix thread (includes a stack) */
         case(LC_UNIXTHREAD):
         /* thread */
         case(LC_THREAD):;

            READ_COMMAND(filedesc, thread_command_t, threat_cmd);
            macho_file_setcommand(macho_file, i, (void*)threat_cmd);

            //TODO replace it by a correct handling of threads :)
            SAFE(lseek(filedesc, REV_BYT(threat_cmd->cmdsize)-sizeof(thread_command_t), SEEK_CUR));
            DBGMSG("\t Reading %lu bytes further.\n", REV_BYT(threat_cmd->cmdsize)-sizeof(thread_command_t));

            break;

         /* fixed VM shared library identification (obsolete) */
         case(LC_IDFVMLIB):
         /* load a specified fixed VM shared library (obsolete) */
         case(LC_LOADFVMLIB):;

            READ_COMMAND(filedesc, fvmlib_command_t, loadfvmlib_cmd);
            macho_file_setcommand(macho_file, i, (void*)loadfvmlib_cmd);
            WRNMSG("Found an obsolete command (loadfvmlib) !\n");
            break;

         /* object identification info (obsolete) */
         case(LC_IDENT):;

            READ_COMMAND(filedesc, ident_command_t, ident_cmd);
            macho_file_setcommand(macho_file, i, (void*)ident_cmd);
            WRNMSG("Found an obsolete command (ident) !\n");
            break;

         /* fixed VM file inclusion (internal use) */
         case(LC_FVMFILE):;

            READ_COMMAND(filedesc, fvmfile_command_t, fvmfile_cmd);
            macho_file_setcommand(macho_file, i, (void*)fvmfile_cmd);
            break;

         /* dynamic link-edit symbol table info */
         case(LC_DYSYMTAB):;

            READ_COMMAND(filedesc, dysymtab_command_t, dysymtab_cmd);
            macho_file_setcommand(macho_file, i, (void*)dysymtab_cmd);

            if (REV_BYT(dysymtab_cmd->ntoc) > 0) {
               start_address = REV_BYT(dysymtab_cmd->tocoff);
               ORDER(data_chunks, dysymtab_cmd, start_address,
                     start_address + (REV_BYT(dysymtab_cmd->ntoc) * sizeof(dylib_table_of_contents_t)), last_address);
            }

            if (REV_BYT(dysymtab_cmd->nmodtab)) {
               start_address = REV_BYT(dysymtab_cmd->modtaboff);
               ORDER(data_chunks, dysymtab_cmd, start_address,
                     start_address + (REV_BYT(dysymtab_cmd->nmodtab) * sizeof(dylib_module32_t)), last_address);
            }

            if (REV_BYT(dysymtab_cmd->nextrefsyms)) {
               start_address = REV_BYT(dysymtab_cmd->extrefsymoff);
               ORDER(data_chunks, dysymtab_cmd, start_address,
                     start_address + (REV_BYT(dysymtab_cmd->nextrefsyms) * sizeof(dylib_reference_t)), last_address);
            }

            if (REV_BYT(dysymtab_cmd->nindirectsyms)) {
               start_address = REV_BYT(dysymtab_cmd->indirectsymoff);
               ORDER(data_chunks, dysymtab_cmd, start_address,
                     start_address + (REV_BYT(dysymtab_cmd->nindirectsyms) * sizeof(uint32_t)), last_address);
            }

            //TODO Handle this for libraries
//            start_address = dysymtab_cmd->extreloff;
//            ORDER(commands_bydata, dysymtab_cmd, start_address,
//                  start_address + (dysymtab_cmd->nextrel * sizeof(nlist32_t)), last_address);
//
//            start_address = dysymtab_cmd->locreloff;
//            ORDER(commands_bydata, dysymtab_cmd, start_address,
//                  start_address + (dysymtab_cmd->nlocrel * sizeof(nlist32_t)), last_address);

            break;

         /* dynamically linked shared lib ident */
         case(LC_ID_DYLIB):
         /*
          * load a dynamically linked shared library that is allowed to be missing
          * (all symbols are weak imported).
          */
         case(LC_LOAD_WEAK_DYLIB):
         /* load a dynamically linked shared library */
         case(LC_LOAD_DYLIB):
         /* delay load of dylib until first use */
         case(LC_LAZY_LOAD_DYLIB):
         /* load upward dylib */
         case(LC_LOAD_UPWARD_DYLIB):
         /* load and re-export dylib */
         case(LC_REEXPORT_DYLIB):;

            READ_COMMAND(filedesc, dylib_command_t, dylib_cmd);
            macho_file_setcommand(macho_file, i, (void*)dylib_cmd);

            char * name = (char*)lc_malloc0(REV_BYT(dylib_cmd->cmdsize)-sizeof(dylib_command_t));
            SAFE(read(filedesc, name, REV_BYT(dylib_cmd->cmdsize)-sizeof(dylib_command_t)));
            DBGMSG("Library found: %s\n", name);

            binfile_addextlib(bf, data_new(DATA_STR, name, REV_BYT(dylib_cmd->cmdsize)-sizeof(dylib_command_t)));

            break;

         /* dynamic linker identification */
         case(LC_ID_DYLINKER):
         /* string for dyld to treat like environment variable */
         case(LC_DYLD_ENVIRONMENT):
         /* load a dynamic linker */
         case(LC_LOAD_DYLINKER):;

            READ_COMMAND(filedesc, dylinker_command_t, dylinker_cmd);
            macho_file_setcommand(macho_file, i, (void*)dylinker_cmd);

            //TODO replace it by a correct handling of dylinker :)
            SAFE(lseek(filedesc, REV_BYT(dylinker_cmd->cmdsize)-sizeof(dylinker_command_t), SEEK_CUR));
            DBGMSG("\t Reading %lu bytes further.\n", REV_BYT(dylinker_cmd->cmdsize)-sizeof(dylinker_command_t));

            break;

         /* modules prebound for a dynamically linked shared library*/
         case(LC_PREBOUND_DYLIB):;

            READ_COMMAND(filedesc, prebound_dylib_command_t, prebound_dylib_cmd);
            macho_file_setcommand(macho_file, i, (void*)prebound_dylib_cmd);

            //TODO replace it by a correct handling of prebound dylib :)
            SAFE(lseek(filedesc, REV_BYT(prebound_dylib_cmd->cmdsize)-sizeof(prebound_dylib_command_t), SEEK_CUR));
            DBGMSG("\t Reading %lu bytes further.\n", REV_BYT(prebound_dylib_cmd->cmdsize)-sizeof(prebound_dylib_command_t));

            break;

         /* image routines */
         case(LC_ROUTINES):;

            READ_COMMAND(filedesc, routines_command32_t, routines_cmd);
            macho_file_setcommand(macho_file, i, (void*)routines_cmd);
            break;

         /* sub framework */
         case(LC_SUB_FRAMEWORK):;

            READ_COMMAND(filedesc, sub_framework_command_t, subframwrk_cmd);
            macho_file_setcommand(macho_file, i, (void*)subframwrk_cmd);

            //TODO replace it by a correct handling of frameworks :)
            SAFE(lseek(filedesc, REV_BYT(subframwrk_cmd->cmdsize)-sizeof(sub_framework_command_t), SEEK_CUR));
            DBGMSG("\t Reading %lu bytes further.\n", REV_BYT(subframwrk_cmd->cmdsize)-sizeof(sub_framework_command_t));

            break;

         /* sub umbrella */
         case(LC_SUB_UMBRELLA):;

            READ_COMMAND(filedesc, sub_umbrella_command_t, subumbrel_cmd);
            macho_file_setcommand(macho_file, i, (void*)subumbrel_cmd);

            //TODO replace it by a correct handling of umbrellas :)
            SAFE(lseek(filedesc, REV_BYT(subumbrel_cmd->cmdsize)-sizeof(sub_umbrella_command_t), SEEK_CUR));
            DBGMSG("\t Reading %lu bytes further.\n", REV_BYT(subumbrel_cmd->cmdsize)-sizeof(sub_umbrella_command_t));

            break;

         /* sub client */
         case(LC_SUB_CLIENT):;

            READ_COMMAND(filedesc, sub_client_command_t, subclient_cmd);
            macho_file_setcommand(macho_file, i, (void*)subclient_cmd);

            //TODO replace it by a correct handling of umbrellas :)
            SAFE(lseek(filedesc, REV_BYT(subclient_cmd->cmdsize)-sizeof(sub_client_command_t), SEEK_CUR));
            DBGMSG("\t Reading %lu bytes further.\n", REV_BYT(subclient_cmd->cmdsize)-sizeof(sub_client_command_t));

            break;

         /* sub library */
         case(LC_SUB_LIBRARY):;

            READ_COMMAND(filedesc, sub_library_command_t, sublib_cmd);
            macho_file_setcommand(macho_file, i, (void*)sublib_cmd);

            //TODO replace it by a correct handling of umbrellas :)
            SAFE(lseek(filedesc, REV_BYT(sublib_cmd->cmdsize)-sizeof(sub_library_command_t), SEEK_CUR));
            DBGMSG("\t Reading %lu bytes further.\n", REV_BYT(sublib_cmd->cmdsize)-sizeof(sub_library_command_t));

            break;

         /* two-level namespace lookup hints */
         case(LC_TWOLEVEL_HINTS):;

            READ_COMMAND(filedesc, twolevel_hints_command_t, twolvlhints_cmd);
            macho_file_setcommand(macho_file, i, (void*)twolvlhints_cmd);
            break;

         /* prebind checksum */
         case(LC_PREBIND_CKSUM):;

            READ_COMMAND(filedesc, prebind_cksum_command_t, prebind_cksum_cmd);
            macho_file_setcommand(macho_file, i, (void*)prebind_cksum_cmd);
            break;

         /* 64-bit segment of this file to be mapped */
         case(LC_SEGMENT_64):
         {
            int read_bytes = 0;

            READ_COMMAND(filedesc, segment_command64_t, sgt_cmd);

            //Creating a new segment
            macho_segment_t* seg = macho_segment_new(sgt_cmd);
            macho_segment_setname(seg, lc_strdup((char*)REV_BYT(sgt_cmd->segname)));
            macho_segment_setn_sections(seg, REV_BYT(sgt_cmd->nsects));
            macho_segment_setoffset(seg, REV_BYT(sgt_cmd->fileoff));
            macho_segment_setsize(seg, REV_BYT(sgt_cmd->filesize));
            macho_segment_setvmaddress(seg, REV_BYT(sgt_cmd->vmaddr));
            macho_segment_setvmsize(seg, REV_BYT(sgt_cmd->vmsize));
            macho_segment_setinitprot(seg, REV_BYT(sgt_cmd->initprot));
            macho_segment_setmaxprot(seg, REV_BYT(sgt_cmd->maxprot));
            macho_segment_setflags(seg, REV_BYT(sgt_cmd->flags));

            //Creates the libbin representation and keeps a link to it
            binseg_t* binseg = macho_binseg_newfromseg(bf, seg);
            macho_segment_setbinseg(seg, binseg);

            macho_file_setcommand(macho_file, i, (void*)sgt_cmd);
            macho_file_addsegment(macho_file, seg);

            //Reading sections
            if (macho_segment_getn_sections(seg) > 0)
               read_bytes = macho_segment_loadsections64(filedesc, bf, seg);

            //Verifying that we read all the data corresponding to this command
            if (REV_BYT(sgt_cmd->cmdsize) - sizeof(segment_command64_t) - read_bytes > 0) {
               SAFE(lseek(filedesc, REV_BYT(sgt_cmd->cmdsize) - sizeof(segment_command32_t) - read_bytes, SEEK_CUR));
               WRNMSG("There is still unread bytes after sections parsing. Reading %lu bytes further.\n",
                     REV_BYT(sgt_cmd->cmdsize) - sizeof(segment_command64_t) - read_bytes);
            }

            /* Ordering this command
             * We only take into account the sections !
             * WARNING, it supposes possible information at the beginning of the segment
             * but not between sections
             */
            if (macho_segment_getbinsection(seg, 0)) {

               start_address = binscn_get_offset(macho_segment_getbinsection(seg, 0));
               ORDER(data_chunks, sgt_cmd, start_address, macho_segment_getoffset(seg) + macho_segment_getsize(seg),
                     last_address);
            }
            break;
         }

         /* 64-bit image routines */
         case(LC_ROUTINES_64):
         /* the uuid */
         case(LC_UUID):;

            READ_COMMAND(filedesc, uuid_command_t, uuid_cmd);
            macho_file_setcommand(macho_file, i, (void*)uuid_cmd);
            break;

         /* runpath additions */
         case(LC_RPATH):;

            READ_COMMAND(filedesc, rpath_command_t, rpath_cmd);
            macho_file_setcommand(macho_file, i, (void*)rpath_cmd);
            break;

         /* encrypted segment information */
         case(LC_ENCRYPTION_INFO):;

            READ_COMMAND(filedesc, encryption_info_command32_t, encr_cmd);
            macho_file_setcommand(macho_file, i, (void*)encr_cmd);
            break;

         /* compressed dyld information */
         case(LC_DYLD_INFO):
         /* compressed dyld information only */
         case(LC_DYLD_INFO_ONLY):;

            READ_COMMAND(filedesc, dyld_info_command_t, dyld_cmd);
            macho_file_setcommand(macho_file, i, (void*)dyld_cmd);

            //List of rebase instructions
            if (REV_BYT(dyld_cmd->rebase_size) > 0) {
               DBGMSG0LVL(1, "Rebase data\n");

               //Reading and ordering the data chunk
               start_address = REV_BYT(dyld_cmd->rebase_off);
               ORDER(data_chunks, dyld_cmd, start_address,
                     start_address + REV_BYT(dyld_cmd->rebase_size), last_address);
            }

            //List of binding instructions (used for objective-c metaclass)
            if (REV_BYT(dyld_cmd->bind_size) > 0) {
               DBGMSG0LVL(1, "Binding data\n");

               //Reading and ordering the data chunk
               start_address = REV_BYT(dyld_cmd->bind_off);
               ORDER(data_chunks, dyld_cmd, start_address,
                     start_address + REV_BYT(dyld_cmd->bind_size), last_address);

               //Getting instructions
               unsigned char* binding = (unsigned char*)lc_malloc0(REV_BYT(dyld_cmd->bind_size));
               int position = lseek(filedesc, 0, SEEK_CUR);
               SAFE(lseek(filedesc, start_address, SEEK_SET));
               SAFE(read(filedesc, binding, REV_BYT(dyld_cmd->bind_size)));
               SAFE(lseek(filedesc, position, SEEK_SET));

               //Parsing instructions
               queue_add_tail(macho_file_getbindings(macho_file), bind_new(NORMAL_BINDING));
               read_binding(macho_file_getbindings(macho_file), binding, REV_BYT(dyld_cmd->bind_size));
               lc_free(binding);

               //TODO What to do with this ?
               DBGLVL(2, FOREACH_INQUEUE(macho_file_getbindings(macho_file), bind) {
                  fprintf(stderr, "Binding: %p\n", GET_DATA_T(bind_t*, bind));
                  fprintf(stderr, " - name: %s\n", bind_getsymbol_name(GET_DATA_T(bind_t*, bind)));
                  fprintf(stderr, " - segment, offset, adjust: %d, %#"PRIx64", %ld -> %#"PRIx64"\n",
                        bind_getsegment(GET_DATA_T(bind_t*, bind)), bind_getoffset(GET_DATA_T(bind_t*, bind)),
                        bind_getadjust(GET_DATA_T(bind_t*, bind)),
                        macho_segment_getoffset(macho_file_getsegment(macho_file, bind_getsegment(GET_DATA_T(bind_t*, bind))))
                        + bind_getoffset(GET_DATA_T(bind_t*, bind)) + bind_getadjust(GET_DATA_T(bind_t*, bind)));
                  fprintf(stderr, " - library: %d\n", bind_getlibrary(GET_DATA_T(bind_t*, bind)));

               }
               );
            }

            //List of weak binding instructions
            if (REV_BYT(dyld_cmd->weak_bind_size) > 0) {
               DBGMSG0LVL(1, "Weak binding data\n");

               //Reading and ordering the data chunk
               start_address = REV_BYT(dyld_cmd->weak_bind_off);
               ORDER(data_chunks, dyld_cmd, start_address,
                     start_address + REV_BYT(dyld_cmd->weak_bind_size), last_address);

               //Getting instructions
               unsigned char* binding = (unsigned char*)lc_malloc0(REV_BYT(dyld_cmd->weak_bind_size));
               int position = lseek(filedesc, 0, SEEK_CUR);
               SAFE(lseek(filedesc, start_address, SEEK_SET));
               SAFE(read(filedesc, binding, REV_BYT(dyld_cmd->weak_bind_size)));
               SAFE(lseek(filedesc, position, SEEK_SET));

               //Parsing instructions
               queue_add_tail(macho_file_getweak_bindings(macho_file), bind_new(WEAK_BINDING));
               read_binding(macho_file_getweak_bindings(macho_file), binding, REV_BYT(dyld_cmd->weak_bind_size));
               lc_free(binding);
            }

            //List of lazy bindings instruction
            if (REV_BYT(dyld_cmd->lazy_bind_size) > 0) {
               DBGMSG0LVL(1, "Lazy binding data\n");

               //Reading and ordering the data chunk
               start_address = REV_BYT(dyld_cmd->lazy_bind_off);
               ORDER(data_chunks, dyld_cmd, start_address,
                     start_address + REV_BYT(dyld_cmd->lazy_bind_size), last_address);

               //Getting instructions
               unsigned char* binding = (unsigned char*)lc_malloc0(REV_BYT(dyld_cmd->lazy_bind_size));
               int position = lseek(filedesc, 0, SEEK_CUR);
               SAFE(lseek(filedesc, start_address, SEEK_SET));
               SAFE(read(filedesc, binding, REV_BYT(dyld_cmd->lazy_bind_size)));
               SAFE(lseek(filedesc, position, SEEK_SET));

               //Parsing instructions
               queue_add_tail(macho_file_getlazy_bindings(macho_file), bind_new(LAZY_BINDING));
               read_binding(macho_file_getlazy_bindings(macho_file), binding, REV_BYT(dyld_cmd->lazy_bind_size));
               lc_free(binding);

               //TODO Add labels at those addresses and do the post-process part of plt handling
               DBGLVL(2, FOREACH_INQUEUE(macho_file_getlazy_bindings(macho_file), bind) {
                  fprintf(stderr, "Lazy binding: %p\n", GET_DATA_T(bind_t*, bind));
                  fprintf(stderr, " - name: %s\n", bind_getsymbol_name(GET_DATA_T(bind_t*, bind)));
                  fprintf(stderr, " - segment, offset, adjust: %d, %#"PRIx64", %ld -> %#"PRIx64"\n",
                        bind_getsegment(GET_DATA_T(bind_t*, bind)), bind_getoffset(GET_DATA_T(bind_t*, bind)),
                        bind_getadjust(GET_DATA_T(bind_t*, bind)),
                        macho_segment_getoffset(macho_file_getsegment(macho_file, bind_getsegment(GET_DATA_T(bind_t*, bind))))
                        + bind_getoffset(GET_DATA_T(bind_t*, bind)) + bind_getadjust(GET_DATA_T(bind_t*, bind)));
                  fprintf(stderr, " - library: %d\n", bind_getlibrary(GET_DATA_T(bind_t*, bind)));

               }
               );
            }

            //List of export instruction (required in libraries)
            if (REV_BYT(dyld_cmd->export_size) > 0) {
               start_address = REV_BYT(dyld_cmd->export_off);
               DBGMSG0("Export data\n");
               ORDER(data_chunks, dyld_cmd, start_address,
                    start_address + REV_BYT(dyld_cmd->export_size), last_address);
            }

            break;

         /* build for MacOSX min OS version */
         case(LC_VERSION_MIN_MACOSX):
         /* build for iPhoneOS min OS version */
         case(LC_VERSION_MIN_IPHONEOS):;
            {
               READ_COMMAND(filedesc, version_min_command_t, vermin_cmd);
               macho_file_setcommand(macho_file, i, (void*)vermin_cmd);
            }
            break;

         /* replacement for LC_UNIXTHREAD */
         case(LC_MAIN):
            {
               READ_COMMAND(filedesc, entry_point_command_t, main_cmd);
               macho_file_setcommand(macho_file, i, (void*)main_cmd);
            }
            break;

         /* source version used to build binary */
         case(LC_SOURCE_VERSION):
            {
               READ_COMMAND(filedesc, source_version_command_t, srcver_cmd);
               macho_file_setcommand(macho_file, i, (void*)srcver_cmd);
            }
            break;

         /* local of code signature */
         case(LC_CODE_SIGNATURE):
         /* local of info to split segments */
         case(LC_SEGMENT_SPLIT_INFO):
         /* Code signing DRs copied from linked dylibs */
         case(LC_DYLIB_CODE_SIGN_DRS):
         /* compressed table of function start addresses */
         case(LC_FUNCTION_STARTS):
         /* table of non-instructions in __text */
         case(LC_DATA_IN_CODE):
            {
               READ_COMMAND(filedesc, linkedit_data_command_t, linkedit_cmd);
               macho_file_setcommand(macho_file, i, (void*)linkedit_cmd);

               if ((REV_BYT(linkedit_cmd->cmd) == LC_DATA_IN_CODE) && (REV_BYT(linkedit_cmd->datasize) > 0)) {
                  ORDER(data_chunks, linkedit_cmd, REV_BYT(linkedit_cmd->dataoff),
                        REV_BYT(linkedit_cmd->dataoff) + REV_BYT(linkedit_cmd->datasize), last_address);
               }

               if ((REV_BYT(linkedit_cmd->cmd) == LC_DYLIB_CODE_SIGN_DRS) && (REV_BYT(linkedit_cmd->datasize) > 0)) {
                  ORDER(data_chunks, linkedit_cmd, REV_BYT(linkedit_cmd->dataoff),
                        REV_BYT(linkedit_cmd->dataoff) + REV_BYT(linkedit_cmd->datasize), last_address);
               }

               if ((REV_BYT(linkedit_cmd->cmd) == LC_SEGMENT_SPLIT_INFO) && (REV_BYT(linkedit_cmd->datasize) > 0)) {
                  ORDER(data_chunks, linkedit_cmd, REV_BYT(linkedit_cmd->dataoff),
                        REV_BYT(linkedit_cmd->dataoff) + REV_BYT(linkedit_cmd->datasize), last_address);
               }

               if ((REV_BYT(linkedit_cmd->cmd) == LC_CODE_SIGNATURE) && (REV_BYT(linkedit_cmd->datasize) > 0)) {
                  ORDER(data_chunks, linkedit_cmd, REV_BYT(linkedit_cmd->dataoff),
                        REV_BYT(linkedit_cmd->dataoff) + REV_BYT(linkedit_cmd->datasize), last_address);
               }

               /*
                * Reading addresses of functions' starts.
                * ULBE128 format.
                * The first value is the offset of the first function from the beginning of the __TEXT segment,
                * the following are the offset from the last function
                */
               if ((REV_BYT(linkedit_cmd->cmd) == LC_FUNCTION_STARTS) && (REV_BYT(linkedit_cmd->datasize) > 0)) {
                  int64_t address = 0, bytes_read = 0;
                  int position = -1, n_scn = binfile_get_nb_sections(bf);

                  //Initializes a section (libbin level)
                  binscn_t* section = binfile_init_scn(bf, n_scn, "MADRAS_function_starts", SCNT_LABEL, 0, 0);
                  binscn_set_offset(section, REV_BYT(linkedit_cmd->dataoff));
                  binscn_set_size(section, REV_BYT(linkedit_cmd->datasize));
                  binscn_set_data(section, (unsigned char*)lc_malloc0(binscn_get_size(section)), TRUE);

                  //Moving the descriptor position
                  position = lseek(filedesc, 0, SEEK_CUR);
                  if (position < 0)
                     HLTMSG("Error when reading binary header !\n");

                  SAFE(lseek(filedesc, macho_file_getoffset(macho_file) + binscn_get_offset(section), SEEK_SET));

                  //Parsing offsets
                  SAFE(read(filedesc, binscn_get_data(section, NULL), REV_BYT(linkedit_cmd->datasize)));
                  unsigned char* offsets = binscn_get_data(section, NULL);

                  //The first value is the offset of the __TEXT segment from the start of the file
                  int64_t __text_offset = 0;
                  int shift = 0;
                  do {
                     __text_offset |= ((offsets[bytes_read] & 0x7F) << shift);
                     shift += 7;
                  } while (offsets[bytes_read++] >= 0x80);

                  address = macho_segment_getvmaddress(macho_file_getsegment(macho_file, 1)) + __text_offset;

                  DBGMSG("Offset of the __TEXT's first section is %#"PRIx64".\n", __text_offset);

                  //Following are offset from previous calculated addresses
                  while (bytes_read < REV_BYT(linkedit_cmd->datasize) && (offsets[bytes_read] != 0x00)) {
                     int value = 0;
                     shift = 0;

                     //See ULEB128 for the parsing
                     do {
                        value |= ((offsets[bytes_read] & 0x7F) << shift);
                        shift += 7;
                     } while (offsets[bytes_read++] >= 0x80);

                     //Building the address
                     address += value;

                     //If this is an even address, we are looking at an ARM function, otherwise this is a Thumb one
                     if(address%2 == 0) {
                        label_t* lbl = label_new("$a", address, TARGET_INSN, NULL);
                        label_set_type(lbl, LBL_FUNCTION);
                        data_t* data_ptr = binfile_addlabel(bf, n_scn, UINT32_MAX, UINT32_MAX, lbl , 0, 0);
                        DBGMSG("Created a reference (Arm function) to the address %#"PRIx64" (%#"PRIx64").\n",
                              address, address - (macho_segment_getvmaddress(macho_file_getsegment(macho_file, 1)) + __text_offset) );

                        //Creating a reference
                        //TODO Add a proper handling of labels (use an unique section every time we add a label)
                        lbl = label_new("Toto", address, TARGET_INSN, NULL);
                        label_set_type(lbl, LBL_FUNCTION);
                        binfile_addlabel(bf, n_scn, UINT32_MAX, UINT32_MAX, lbl , 0, 0);

                        DBGMSG("Created a reference (function start) to the address %#"PRIx64" (%#"PRIx64").\n",
                              address, address - (macho_segment_getvmaddress(macho_file_getsegment(macho_file, 1)) + __text_offset) );
                     } else {
                        label_t* lbl = label_new("$t", address - 1, TARGET_INSN, NULL);
                        label_set_type(lbl, LBL_FUNCTION);
                        data_t* data_ptr = binfile_addlabel(bf, n_scn, UINT32_MAX, UINT32_MAX, lbl , 0, 0);
                        DBGMSG("Created a reference (Thumb function) to the address %#"PRIx64" (%#"PRIx64").\n",
                              address - 1, address - (macho_segment_getvmaddress(macho_file_getsegment(macho_file, 1)) + __text_offset - 1) );

                        //Creating a reference
                        //TODO Add a proper handling of labels (use an unique section every time we add a label)
                        lbl = label_new("Toto", address - 1, TARGET_INSN, NULL);
                        label_set_type(lbl, LBL_FUNCTION);
                        binfile_addlabel(bf, n_scn, UINT32_MAX, UINT32_MAX, lbl , 0, 0);

                        DBGMSG("Created a reference (function start) to the address %#"PRIx64" (%#"PRIx64").\n",
                              address - 1, address - (macho_segment_getvmaddress(macho_file_getsegment(macho_file, 1)) + __text_offset - 1) );
                     }
                  }

                  //Moving back
                  SAFE(lseek(filedesc, position, SEEK_SET));
               }
            }

            break;

         /* prepage command (internal use) */
         case(LC_PREPAGE):
         default:;

            unknown_command_t* unknown_cmd = lc_malloc0(sizeof(unknown_command_t));
            //We read only the common part
            SAFE(read(filedesc, unknown_cmd, sizeof(load_command_t)));
            macho_file_setcommand(macho_file, i, (void*)unknown_cmd);

            //Loading unstructured data if there is one after the command
            if (sizeof(unknown_command_t) - sizeof(void*) > 0)
               SAFE(read(filedesc, unknown_cmd->data, REV_BYT(command->cmdsize) - sizeof(load_command_t)));

            WRNMSG("Command %"PRId64" unrecognized. Loading %"PRId64" bytes.\n", REV_BYT(command->cmd), REV_BYT(command->cmdsize));
      }
   }

   FOREACH_INQUEUE(data_chunks, elt) {
      DBGMSG("Data_chunk: %p\n",GET_DATA_T(data_chunk_t*, elt)->command);
      DBGMSG("\t%#"PRIx64" -> %#"PRIx64"\n", GET_DATA_T(data_chunk_t*, elt)->start_address,
            GET_DATA_T(data_chunk_t*, elt)->end_address);
   }
}

///////////////////////////////////////////////////////////////////////////////
//                                 Segments                                  //
///////////////////////////////////////////////////////////////////////////////

/**
 * Creates a new segment
 * \param command The command corresponding to the created segment
 * \return A created segment
 */
macho_segment_t* macho_segment_new(void* command) {

   if (!command)
      return NULL;

   macho_segment_t* seg = (macho_segment_t*)lc_malloc0(sizeof(macho_segment_t));
   seg->command         = command;

   return seg;
}

/**
 * Frees a segment
 * \param seg The segment to free
 */
void macho_segment_free(macho_segment_t* seg) {

   if (!seg)
      return;

   lc_free(seg);
}

/**
 * Sets the name of a segment
 * \param seg The segment to update
 * \param name The name to set
 */
void macho_segment_setname(macho_segment_t* seg, char* name) {
   if(seg)
      seg->name = lc_strdup(name);
}

/**
 * Sets the offset of a segment
 * \param seg The segment to update
 * \param offset Offset of the segment in the file
 */
void macho_segment_setoffset(macho_segment_t* seg, int64_t offset) {
   if(seg)
      seg->offset = offset;
}

/**
 * Sets the size of a segment
 * \param seg The segment to update
 * \param size Size of the segment in the file
 */
void macho_segment_setsize(macho_segment_t* seg, uint64_t size) {
   if(seg)
      seg->size = size;
}

/**
 * Sets the virtual memory address of a segment
 * \param seg The segment to update
 * \param addr The address of the segment in memory
 */
void macho_segment_setvmaddress(macho_segment_t* seg, int64_t vmaddress) {
   if(seg)
      seg->vmaddress = vmaddress;
}

/**
 * Sets the virtual memory size of a segment
 * \param seg The segment to update
 * \param vmsize The virtual size of the segment in memory
 */
void macho_segment_setvmsize(macho_segment_t* seg, uint64_t vmsize) {
   if(seg)
      seg->vmsize = vmsize;
}

/**
 * Sets the number of section in a segment
 * WARNING It reallocates space for the array of sections,
 * make sure the last elements are free before lowering the number of sections
 * \param seg The segment to update
 * \param n_sections The number of sections in the segment
 */
void macho_segment_setn_sections(macho_segment_t* seg, uint32_t n_sections) {
   if(seg) {
      seg->n_sections   = n_sections;
      seg->sections     = (void**)lc_realloc(seg->sections, sizeof(void*) * seg->n_sections);
      seg->binscns      = (binscn_t**)lc_realloc(seg->binscns, sizeof(binscn_t*) * seg->n_sections);
   }
}

/**
 * Sets a section in the segment
 * \param seg The segment to update
 * \param sections The index of the section to set
 * \param section The section to set
 */
void macho_segment_setsection(macho_segment_t* seg, uint32_t sct_idx, void* section) {
   if(seg && (sct_idx < seg->n_sections) && section)
      seg->sections[sct_idx] = section;
}

/**
 * Sets a libbin section representation in the segment
 * \param seg The segment to update
 * \param sections The index of the section to set
 * \param section The section to set
 */
void macho_segment_setbinsection(macho_segment_t* seg, uint32_t sct_idx, binscn_t* section) {
   if(seg && (sct_idx < seg->n_sections) && section)
      seg->binscns[sct_idx] = section;
}

/**
 * Sets the initial virtual memory protection of a segment
 * \param seg The segment to update
 * \param initprot The initial protection
 */
void macho_segment_setinitprot(macho_segment_t* seg, int initprot) {
   if (seg)
      seg->initprot = (vm_prot_t)initprot;
}

/**
 * Sets the maximum virtual memory protection of a segment
 * \param seg The segment to update
 * \param maxprot The maximum protection
 */
void macho_segment_setmaxprot(macho_segment_t* seg, int maxprot) {
   if (seg)
      seg->maxprot = (vm_prot_t)maxprot;
}

/**
 * Sets the flags relative to a segment
 * \param seg The segment to update
 * \param flags The flags to set to the segment
 */
void macho_segment_setflags(macho_segment_t* seg, uint32_t flags) {
   if (seg)
      seg->flags = flags;
}

/**
 * Sets the libbin representation of the segment
 * \param seg The segment to update
 * \param binseg The libbin representation
 */
void macho_segment_setbinseg(macho_segment_t* seg, binseg_t* binseg) {
   if (seg)
      seg->binseg = binseg;
}

/**
 * Gets the name of a segment
 * \param seg A segment
 * \return The name of the segment
 */
char* macho_segment_getname(macho_segment_t* seg) {
   return (seg) ? seg->name : NULL;
}

/**
 * Gets the offset of a segment
 * \param seg A segment
 * \return The segment's offset in the file
 */
int64_t macho_segment_getoffset(macho_segment_t* seg) {
   return (seg) ? seg->offset : SIGNED_ERROR;
}

/**
 * Gets the size of a segment
 * \param seg A segment
 * \return The segment's size in the file
 */
uint64_t macho_segment_getsize(macho_segment_t* seg) {
   return (seg) ? seg->size : UNSIGNED_ERROR;
}

/**
 * Gets the virtual memory address of a segment
 * \param seg A segment
 * \return The address of the segment in memory
 */
int64_t macho_segment_getvmaddress(macho_segment_t* seg) {
   return (seg) ? seg->vmaddress : SIGNED_ERROR;
}

/**
 * Gets the virtual memory size of a segment
 * \param seg A segment
 * \return The virtual size of the segment in memory
 */
uint64_t macho_segment_getvmsize(macho_segment_t* seg) {
   return (seg) ? seg->vmsize : UNSIGNED_ERROR;
}

/**
 * Gets the number of sections in a segment
 * \param seg A segment
 * \return The number of sections in the segment
 */
uint32_t macho_segment_getn_sections(macho_segment_t* seg) {
   return (seg) ? seg->n_sections : UNSIGNED_ERROR;
}

/**
 * Gets a section from a segment
 * \param seg A segment
 * \param sct_idx The index of the section to get (starting at 0)
 * \return The section at the given index (macho_section32_t* or macho_section64_t*)
 */
void* macho_segment_getsection(macho_segment_t* seg, uint32_t sct_idx) {
   return (seg && (sct_idx < seg->n_sections)) ? seg->sections[sct_idx] : NULL;
}

/**
 * Gets a libbin section representation from a segment
 * \param seg A segment
 * \param sct_idx The index of the section to get (starting at 0)
 * \return The libbin section representation at the given index
 */
binscn_t* macho_segment_getbinsection(macho_segment_t* seg, uint32_t sct_idx) {
   return (seg && (sct_idx < seg->n_sections)) ? seg->binscns[sct_idx] : NULL;
}

/*
 * Gets the initial virtual memory protection of a segment
 * \param seg A segment
 */
vm_prot_t macho_segment_getinitprot(macho_segment_t* seg) {
   return (seg) ? seg->initprot : SIGNED_ERROR;
}

/*
 * Gets the maximum virtual memory protection of a segment
 * \param seg A segment
 */
vm_prot_t macho_segment_getmaxprot(macho_segment_t* seg) {
   return (seg) ? seg->maxprot : SIGNED_ERROR;
}

/*
 * Gets the flags relative to a given segment
 * \param seg A segment
 */
uint32_t macho_segment_getflags(macho_segment_t* seg) {
   return (seg) ? seg->flags : UNSIGNED_ERROR;
}

/*
 * Gets the libbin representation of a given segment
 * \param seg A segment
 */
binseg_t* macho_segment_getbinseg(macho_segment_t* seg) {
   return (seg) ? seg->binseg : NULL;
}

/*
 * Adds a section to a segment
 * \param seg A segment
 * \param sct A section
 */
void macho_segment_addsection(macho_segment_t* seg, void* sct) {
   if(seg && sct) {
      seg->n_sections++;
      seg->sections = (void**)lc_realloc(seg->sections, sizeof(void*) * seg->n_sections);
      seg->sections[seg->n_sections - 1] = sct;
   }
}

/*
 * Removes a section from a segment
 * \param seg A segment
 * \param sct_idx The index of the section to delete (starting at 0)
 */
void macho_segment_removesection(macho_segment_t* seg, uint32_t sct_idx) {
   if(seg && (sct_idx < seg->n_sections)) {
      uint32_t i = 0;

      for (i = 0; i < (seg->n_sections - 1); i++) {
         if (i >= sct_idx)
            seg->sections[i] = seg->sections[i+1];
      }

      seg->n_sections--;
      seg->sections = (void**)lc_realloc(seg->sections, sizeof(void*) * seg->n_sections);
   }
}

/*
 * Parses the 32-bit section headers contained in a given segment
 * \param filedesc A file descriptor
 * \param bf A binary file structure containing an opened stream to the file to parse
 * \param seg The segment the sections belong to
 * \return The number of bytes read
 */
int macho_segment_loadsections32(int filedesc, binfile_t* bf, macho_segment_t* seg) {

   if (!bf || !seg)
      return 0;

   int read_bytes = 0, i = 0;

   //We know from the segment header how many sections are following
   for(i = 0; i < (int)macho_segment_getn_sections(seg); i++) {

      int position = -1;

      macho_section32_t* sct = (macho_section32_t*)lc_malloc0(sizeof(macho_section32_t));
      SAFE(read(filedesc, sct, sizeof(macho_section32_t)));
      read_bytes += sizeof(macho_section32_t);

      //Creating a binscn_t structure from the Mach-O format
      binscn_t* binscn = macho_binscn_newfromsection32(bf, seg, sct);

      //Adding the section to the parsed segment
      macho_segment_setsection(seg, i, sct);
      macho_segment_setbinsection(seg, i, binscn);

      //Storing the current position for coming back after the section data parsing
      position = lseek(filedesc, 0, SEEK_CUR);
      if (position <= 0)
         HLTMSG("Error when reading binary header !\n");

      //Moving the section position, parsing the data, coming back to the section header
      SAFE(lseek(filedesc, binscn_get_offset(binscn), SEEK_SET));
      SAFE(read(filedesc, binscn_get_data(binscn, NULL), binscn_get_size(binscn)));
      SAFE(lseek(filedesc, position, SEEK_SET));

   }

   return read_bytes;
}

/*
 * Parses the 64-bit section headers contained in a given segment
 * \param filedesc A file descriptor
 * \param bf A binary file structure containing an opened stream to the file to parse
 * \param seg The segment the sections belong to
 * \return The number of bytes read
 */
int macho_segment_loadsections64(int filedesc, binfile_t* bf, macho_segment_t* seg) {

   if (!bf || !seg)
      return 0;

   int read_bytes = 0, i = 0;

   //We know from the segment header how many sections are following
   for(i = 0; i < (int)macho_segment_getn_sections(seg); i++) {

      int position = -1;

      macho_section64_t* sct = (macho_section64_t*)lc_malloc0(sizeof(macho_section64_t));
      SAFE(read(filedesc, sct, sizeof(macho_section64_t)));
      read_bytes += sizeof(macho_section64_t);

      //Creating a binscn_t structure from the Mach-O format
      binscn_t* binscn = macho_binscn_newfromsection64(bf, seg, sct);

      //Adding the section to the parsed segment
      macho_segment_setsection(seg, i, sct);
      macho_segment_setbinsection(seg, i, binscn);

      //Storing the current position for coming back after the section data parsing
      position = lseek(filedesc, 0, SEEK_CUR);
      if (position <= 0)
         HLTMSG("Error when reading binary header !\n");

      //Moving the section position, parsing the data, coming back to the section header
      SAFE(lseek(filedesc, binscn_get_offset(binscn), SEEK_SET));
      SAFE(read(filedesc, binscn_get_data(binscn, NULL), binscn_get_size(binscn)));
      SAFE(lseek(filedesc, position, SEEK_SET));

   }

   return read_bytes;
}

///////////////////////////////////////////////////////////////////////////////
//                                  Binfiles                                 //
///////////////////////////////////////////////////////////////////////////////

/*
 * Loads a binfile_t structure with the result of the parsing of an Mach-O file or directory
 * THIS IS THE ENTRY POINT
 * \param bf A binary file structure containing an opened stream to the file to parse
 * \return Error code if the file could not be successfully parsed as an Mach-O file, EXIT_SUCCESS otherwise. In this
 * case, the structure representing the binary file will have been updated with the result of the parsing
 * */
int macho_binfile_load(binfile_t* bf) {

   int error;
   int filedesc;

   if (!bf)
      return ERR_BINARY_MISSING_BINFILE;

   //TODO 27/01/2015 Add handling of directories in order to get the debug file(s).

   //Assuming the binary file is passed
   char* filename = binfile_get_file_name(bf);
   FILE* filestream = NULL;

   //Opening the binary file
   if (filename != NULL) {

      filestream = fopen(filename, "r");
      if (filestream == NULL)
         return ERR_COMMON_UNABLE_TO_OPEN_FILE;

      binfile_set_filestream(bf, filestream);

   } else
      return ERR_COMMON_FILE_NAME_MISSING;

   //Getting the file descriptor and setting the pointer at the beginning of the file
   filedesc = fileno(filestream);
   error = lseek(filedesc, 0, SEEK_SET);
   if (error < 0)
      return ERR_COMMON_UNABLE_TO_READ_FILE;

   //Parse the binary file
   return macho_binfile_loadmagic(filedesc, bf);

//   return TRUE;
}

/*
 * Loads a binfile_t structure with the result of the parsing of a Mach-O executable file.
 * It assumes the executable file is opened.
 * \param filedesc A file descriptor
 * \param bf A binary file structure containing an opened stream to the file to parse
 * \return Error code if the file could not be successfully parsed as an Mach-O file, EXIT_SUCCESS otherwise. In this
 * case, the structure representing the binary file will have been updated with the result of the parsing
 */
int macho_binfile_loadmagic(int filedesc, binfile_t* bf){

   if (filedesc < 0)
      return ERR_COMMON_FILE_STREAM_MISSING;
   if (!bf)
      return ERR_BINARY_MISSING_BINFILE;

   // Get the magic number
   uint32_t* magic = lc_malloc0(sizeof(uint32_t));
   SAFE(read(filedesc, magic, sizeof(uint32_t)));

   //Fat binary, a binary containing multiple versions of a binary for different architectures
   if (*magic == FAT_MAGIC || *magic == FAT_CIGAM) {

      if (binfile_get_archive(bf) != NULL)
         HLTMSG("Found an archive into another archive, something is wrong.\n");

      unsigned int i = 0;
      int position = 0;

      if (*magic == FAT_CIGAM)
         binfile_set_byte_order(bf, BFO_REVERSED);

      //Reading the fat header
      fat_header* fat = (fat_header*)lc_malloc0(sizeof(fat_header));
      SAFE(lseek(filedesc, 0, SEEK_SET));
      SAFE(read(filedesc, fat, sizeof(fat_header)));

//      //Initializing the list of archive elements
//      binfile_t** bfs = (binfile_t**)lc_malloc0(fat->nfat_arch * sizeof(binfile_t*));
      binfile_set_nb_ar_elts(bf, fat->nfat_arch);

      //We keep a pointer of where we are in the fat header
      position += sizeof(fat_header);

      DBGMSG("Archive found with %"PRId64" binaries inside.\n", fat->nfat_arch);

      //For each archive element
      for (i = 0; i < fat->nfat_arch; i++) {

         //Linking to the archive
         binfile_t* bfar = binfile_new(NULL);//(binfile_t*)lc_malloc0(sizeof(binfile_t));
         //binfile_set_archive(bfs[i], bf);

         //Reading the fat_arch header
         fat_arch* arch = (fat_arch*)lc_malloc0(sizeof(fat_arch));
         SAFE(lseek(filedesc, position, SEEK_SET));
         SAFE(read(filedesc, arch, sizeof(fat_arch)));
         position += sizeof(fat_arch);

         //Moving the file pointer to the right location
         SAFE(lseek(filedesc, REV_BYT(arch->offset), SEEK_SET));
         //And parsing the corresponding element
         macho_binfile_loadmagic(filedesc, bfar);
         //Moving back the file pointer
         SAFE(lseek(filedesc, position, SEEK_SET));

         //Storing the archive member
         binfile_set_ar_elt(bf, bfar, i);

         lc_free(arch);
         break;
      }

      binfile_set_format(bf, BFF_MACHO);
      binfile_set_type(bf, BFT_ARCHIVE);
      //binfile_set_ar_elts(bf, bfs, REV_BYT(fat->nfat_arch));

      lc_free(fat);

   }
   //Single binary, expecting a "normal" header
   else {

      binfile_set_format(bf, BFF_MACHO);

      //Moving back (the magic number is included in the header we read just after)
      SAFE(lseek(filedesc, -sizeof(uint32_t), SEEK_CUR));

      switch(*magic) {
         //32-bit binary with an inverted byte order (compared to the host byte order)
         case(MH_CIGAM):

            binfile_set_byte_order(bf, BFO_REVERSED);

         //32-bit binary
         case(MH_MAGIC):

            binfile_set_word_size(bf, BFS_32BITS);
            DBGMSG("32-bit Mach-O file%s.\n", (binfile_get_byte_order(bf) == BFO_REVERSED)?", reverted byte ordering scheme":"");
            macho_binfile_loadheader32 (filedesc, bf);

            break;

         //64-bit binary with an inverted byte order (compared to the host byte order)
         case(MH_CIGAM_64):

            binfile_set_byte_order(bf, BFO_REVERSED);

         //64-bit binary
         case(MH_MAGIC_64):

            binfile_set_word_size(bf, BFS_64BITS);
            DBGMSG("64-bit Mach-O file%s.\n", (binfile_get_byte_order(bf) == BFO_REVERSED)?", reverted byte ordering scheme":"");
            macho_binfile_loadheader64 (filedesc, bf);

            break;

         default:
            DBGMSG("Could not identify the file as a Mach-O file. %x\n", *magic);
            binfile_set_format(bf, BFF_UNKNOWN);
            return ERR_BINARY_FORMAT_NOT_RECOGNIZED;
            break;
      }

   }

   lc_free(magic);

   return EXIT_SUCCESS;
}

/*
 * Loads a binfile_t structure with the result of the parsing of a 32-bit Mach-O file.
 * It assumes the descriptor is at the start of the parsed binary (if the binary
 * is a fat binary then the descriptor is at the start of the sub-part corresponding
 * to the binfile_t bf).
 * \param filedesc A file descriptor
 * \param bf A binary file structure containing an opened stream to the file to parse
 */
void macho_binfile_loadheader32(int filedesc, binfile_t* bf) {

   int32_t offset;

   if (filedesc < 0 || !bf)
      return;

   /* In fat binaries we are not at the beginning of the file
    * And of course many offsets are based on the binary start address
    */
   offset = lseek(filedesc, 0, SEEK_CUR);
   if (offset < 0)
      return;

   //File header
   macho_header32_t* header = lc_malloc0(sizeof(macho_header32_t));
   SAFE(read(filedesc, header, sizeof(macho_header32_t)));

   DBGMSG("Header 32: %lu bytes\n", sizeof(macho_header32_t));

   //Setting file type
   switch(REV_BYT(header->filetype)) {
      case(MH_OBJECT):
         binfile_set_type(bf, BFT_RELOCATABLE);
         break;
      case(MH_EXECUTE):
         binfile_set_type(bf, BFT_EXECUTABLE);
         break;
      case(MH_DYLIB):
         binfile_set_type(bf, BFT_LIBRARY);
         break;
      case(MH_BUNDLE):
      case(MH_PRELOAD):
      case(MH_CORE):
      case(MH_DYLINKER):
      case(MH_DSYM):
      default:
         binfile_set_type(bf, BFT_UNKNOWN);
         break;
   }

   //Setting architecture
   binfile_set_arch(bf, getarch_bybincode(BFF_MACHO, REV_BYT(header->cputype)));

   //Setting commands information
   macho_file_t* macho_file   = macho_file_new(bf);
   macho_file_setn_commands(macho_file, REV_BYT(header->ncmds));
   macho_file_setcommands_size(macho_file, REV_BYT(header->sizeofcmds));

   //Setting CPU information
   macho_file_setcpu_type(macho_file, REV_BYT(header->cputype));
   macho_file_setcpu_subtype(macho_file, REV_BYT(header->cpusubtype));

   //Setting offset
   macho_file_setoffset(macho_file, offset);

   //Loading the list of commands
   macho_file_loadcommands(filedesc, macho_file);

   return;
}

/*
 * Loads a binfile_t structure with the result of the parsing of a 64-bit Mach-O file.
 * It assumes the descriptor is at the start of the parsed binary (if the binary
 * is a fat binary then the descriptor is at the start of the sub-part corresponding
 * to the binfile_t bf).
 * \param filedesc A file descriptor
 * \param bf A binary file structure containing an opened stream to the file to parse
 */
void macho_binfile_loadheader64(int filedesc, binfile_t* bf) {

   int32_t offset;

   if (filedesc < 0 || !bf)
      return;

   /* In fat binaries we are not at the beginning of the file
    * And of course many offsets are based on the binary start address
    */
   offset = lseek(filedesc, 0, SEEK_CUR);
   if (offset < 0)
      return;

   //File header
   macho_header64_t* header = lc_malloc0(sizeof(macho_header64_t));
   SAFE(read(filedesc, header, sizeof(macho_header64_t)));

   //Setting file type
   switch(REV_BYT(header->filetype)) {
      case(MH_OBJECT):
         binfile_set_type(bf, BFT_RELOCATABLE);
         break;
      case(MH_EXECUTE):
         binfile_set_type(bf, BFT_EXECUTABLE);
         break;
      case(MH_DYLIB):
         binfile_set_type(bf, BFT_LIBRARY);
         break;
      case(MH_BUNDLE):
      case(MH_PRELOAD):
      case(MH_CORE):
      case(MH_DYLINKER):
      case(MH_DSYM):
      default:
         binfile_set_type(bf, BFT_UNKNOWN);
         break;
   }

   //Setting architecture
   binfile_set_arch(bf, getarch_bybincode(BFF_MACHO, REV_BYT(header->cputype)));

   //Setting commands information
   macho_file_t* macho_file   = macho_file_new(bf);
   macho_file_setn_commands(macho_file, REV_BYT(header->ncmds));
   macho_file_setcommands_size(macho_file, REV_BYT(header->sizeofcmds));

   //Setting CPU information
   macho_file_setcpu_type(macho_file, REV_BYT(header->cputype));
   macho_file_setcpu_subtype(macho_file, REV_BYT(header->cpusubtype));

   //Setting offset
   macho_file_setoffset(macho_file, offset);

   //Loading the list of commands
   macho_file_loadcommands(filedesc, macho_file);

   return;
}

/*
 * Returns the first loaded address
 * \param bf A binary file structure containing an opened stream to the file to parse
 * \return The first loaded address of this binary file
 */
int64_t macho_binfile_getfirstloadaddr(binfile_t* bf) {
   return (bf)?macho_file_getfirstloadaddr((macho_file_t*)(binfile_get_parsed_bin(bf))):SIGNED_ERROR;
}

/*
 * Returns the last loaded address
 * \param bf A binary file structure containing an opened stream to the file to parse
 * \return The last loaded address of this binary file
 */
int64_t macho_binfile_getlastloadaddr(binfile_t* bf) {
   return (bf)?macho_file_getlastloadaddr((macho_file_t*)(binfile_get_parsed_bin(bf))):SIGNED_ERROR;
}

/*
 * Returns a suffixed label corresponding to an external function
 * \param common_name The label to suffix
 * \return The suffixed string
 */
char* macho_binfile_generate_ext_label_name(char* common_name) {
   if (!common_name)
      return NULL;

   char* full_name = lc_malloc(strlen(common_name) + strlen(EXT_LBL_SUFFIX) + 1);
   sprintf(full_name, "%s" EXT_LBL_SUFFIX, common_name);

   return full_name;
}

/*
 * Prints a macho header
 * \param bf A binary file structure containing an opened stream to the file to parse
 */
void macho_binfile_print(binfile_t* bf){
   if (!bf)
      return;

   printf("Macho Header ---------------------------------------------\n");

   switch (binfile_get_word_size(bf)) {
   case(BFS_32BITS):
      if (binfile_get_byte_order(bf) == BFO_HOST) {
         printf("%-45s %#"PRIx32"\n", "Magic:", MH_MAGIC);
         printf("%-45s %s\n", "Endianness:", "Host");
      }
      else if (binfile_get_byte_order(bf) == BFO_REVERSED) {
         printf("%-45s %#"PRIx32"\n", "Magic:", MH_CIGAM);
         printf("%-45s %s\n", "Endianness:", "Reversed (from host)");
      }
      printf("%-45s %s\n", "Word size:", "32 bits");
      break;
   case(BFS_64BITS):
      if (binfile_get_byte_order(bf) == BFO_HOST) {
         printf("%-45s %#"PRIx32"\n", "Magic:", MH_MAGIC_64);
         printf("%-45s %s\n", "Endianness:", "Host");
      }
      else if (binfile_get_byte_order(bf) == BFO_REVERSED) {
         printf("%-45s %#"PRIx32"\n", "Magic:", MH_CIGAM_64);
         printf("%-45s %s\n", "Endianness:", "Reversed (from host)");
      }
      printf("%-45s %s\n", "Word size:", "64 bits");
      break;
   default:
      printf("Unknown Header --------------------------------------------\n");
   }

   switch (binfile_get_type(bf)) {
   case(BFT_EXECUTABLE):
      printf("%-45s %s\n", "Type:", "Executable file");
      break;
   case(BFT_LIBRARY):
      printf("%-45s %s\n", "Type:", "Library file");
      break;
   case(BFT_RELOCATABLE):
      printf("%-45s %s\n", "Type:", "Relocatable file");
      break;
   default:
      printf("%-45s %s\n", "Type:", "Unknown type");
   }

   macho_file_print((macho_file_t*)(binfile_get_parsed_bin(bf)));
}

///////////////////////////////////////////////////////////////////////////////
//                                 Dbgfiles                                  //
///////////////////////////////////////////////////////////////////////////////

dbg_file_t* macho_parsedbg(binfile_t* bf) {
   (void)bf;
   return NULL;
}

///////////////////////////////////////////////////////////////////////////////
//                                  Asmfile                                  //
///////////////////////////////////////////////////////////////////////////////

int macho_asmfile_addlabels(asmfile_t* asmf) {
   (void)asmf;
   return FALSE;
}

void macho_asmfile_print_binfile(asmfile_t* asmf){
   if (asmf)
      macho_binfile_print(asmfile_get_binfile(asmf));
}

///////////////////////////////////////////////////////////////////////////////
//                                  Sections                                 //
///////////////////////////////////////////////////////////////////////////////

/*
 * Sets the type of a 32-bit Mach-O section
 * \param section The section to update
 * \param type The type to set
 */
void macho_section32_settype(macho_section32_t* section, uint8_t type){
   if (section)
      section->flags = (section->flags&(~SECTION_TYPE))|(type&SECTION_TYPE);
}

/*
 * Sets the attributes of a 32-bit Mach-O section
 * \param section The section to update
 * \param attr The attributes to set
 */
void macho_section32_setattributes(macho_section32_t* section, uint32_t attr){
   if (section)
      section->flags = (section->flags&(~SECTION_ATTRIBUTES))|(attr&SECTION_ATTRIBUTES);
}

/*
 * Sets the attributes of a 64-bit Mach-O section
 * \param section The section to update
 * \param attr The attributes to set
 */
void macho_section64_setattributes(macho_section64_t* section, uint32_t attr){
   if (section)
      section->flags = (section->flags&(~SECTION_ATTRIBUTES))|(attr&SECTION_ATTRIBUTES);
}

/*
 * Sets the type of a 64-bit Mach-O section
 * \param section The section to update
 * \param type The type to set
 */
void macho_section64_settype(macho_section64_t* section, uint8_t type){
   if (section)
      section->flags = (section->flags&(~SECTION_TYPE))|(type&SECTION_TYPE);
}

int macho_section_getannotate(binfile_t* bf, binscn_t* scn) {
   (void)bf;(void)scn;
   /**\todo (2015-06-01) That function is not needed any more, but the correct flags need to be
    * set on sections instead (SCNA_STDCODE, SCNA_EXTFCTSTUBS, SCNA_PATCHED)*/
   return FALSE;
}

///////////////////////////////////////////////////////////////////////////////
//                              Libbin interface                             //
///////////////////////////////////////////////////////////////////////////////

/**
 * Creates a Mach-O representation of a segment from its libbin representation
 * \param binseg The libbin representation
 */
void macho_segment_newfrombinseg(binseg_t* binseg) {
   if (!binseg)
      return;

   binfile_t* bf              = binseg_get_binfile(binseg);
   vm_prot_t protection       = VM_PROT_NONE;
   macho_file_t* macho_file   = binfile_get_parsed_bin(bf);

   if (!macho_file)
      HLTMSG("Interface between libbin and format-specific is not set.\n");

   if (binseg_get_id(binseg) > macho_file_getn_segments(macho_file) + 1)
      HLTMSG("Attempt to create a segment with a wrong index !\n");

   //We need a new segment command
   segment_command32_t* sgt_cmd = (segment_command32_t*)lc_malloc0(sizeof(segment_command32_t));

   macho_segment_t* seg = macho_segment_new(sgt_cmd);
   macho_segment_setname(seg, lc_strdup(binseg_get_name(binseg)));
   macho_segment_setn_sections(seg, binseg_get_nb_scns(binseg));
   macho_segment_setoffset(seg, binseg_get_offset(binseg));
   macho_segment_setsize(seg, binseg_get_fsize(binseg));
   macho_segment_setvmaddress(seg, binseg_get_addr(binseg));
   macho_segment_setvmsize(seg, binseg_get_msize(binseg));

   //Setting initial segment protections
   if (binseg_check_attrs(binseg, SCNA_READ))
      protection |= VM_PROT_READ;
   if (binseg_check_attrs(binseg, SCNA_WRITE))
      protection |= VM_PROT_WRITE;
   if (binseg_check_attrs(binseg, SCNA_EXE))
      protection |= VM_PROT_EXECUTE;
   macho_segment_setinitprot(seg, protection);

   //If there is any permission the maximum protection is all protection
   if (binseg_get_attrs(binseg) != SCNA_NONE)
      macho_segment_setmaxprot(seg, VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE);

   //Updating the command
   strcpy(sgt_cmd->segname, (char*)REV_BYT(macho_segment_getname(seg)));
   sgt_cmd->nsects   = REV_BYT(macho_segment_getn_sections(seg));
   sgt_cmd->fileoff  = REV_BYT(macho_segment_getoffset(seg));
   sgt_cmd->filesize = REV_BYT(macho_segment_getsize(seg));
   sgt_cmd->vmaddr   = REV_BYT(macho_segment_getvmaddress(seg));
   sgt_cmd->vmsize   = REV_BYT(macho_segment_getvmsize(seg));
   sgt_cmd->initprot = REV_BYT(macho_segment_getinitprot(seg));
   sgt_cmd->maxprot  = REV_BYT(macho_segment_getmaxprot(seg));
   sgt_cmd->flags    = REV_BYT(macho_segment_getflags(seg));

   macho_file_addcommand(macho_file, sgt_cmd);
   macho_file_addsegment(macho_file, seg);

}

/**
 * Creates a libbin segment from a Mach-O segment representation
 * \param bf A binary file structure
 * \param seg The Mach-O segment representation
 * \return The created libbin segment
 */
binseg_t* macho_binseg_newfromseg(binfile_t* bf, macho_segment_t* seg) {
   if (!bf || !seg)
      return NULL;

   uint8_t attributes = SCNA_NONE;
   int n_seg = binfile_get_nb_segments(bf);

   if (macho_segment_getinitprot(seg)&VM_PROT_READ)
      attributes |= SCNA_READ;
   if (macho_segment_getinitprot(seg)&VM_PROT_WRITE)
      attributes |= SCNA_WRITE;
   if (macho_segment_getinitprot(seg)&VM_PROT_EXECUTE)
      attributes |= SCNA_EXE;

   binseg_t* binseg = binfile_init_seg(bf, n_seg, macho_segment_getoffset(seg), macho_segment_getvmaddress(seg),
         macho_segment_getsize(seg), macho_segment_getvmsize(seg), attributes, 1);

   return binseg;
}

/**
 * Add a new Mach-O section from a libbin representation
 * \param scn The newly created binscn_t section
 */
void macho_section32_newfrombinscn(binscn_t* scn) {
   if (!scn)
      return;

   binfile_t* bf              = binscn_get_binfile(scn);
//   macho_file_t* macho_file   = binfile_get_parsed_bin(bf);
//   binseg_t* binseg           = scn->binsegs[0]; //TODO add a proper getter
   macho_section32_t* section = (macho_section32_t*)lc_malloc0(sizeof(macho_section32_t));

   section->offset   = REV_BYT(binscn_get_offset(scn));
   section->size     = REV_BYT(binscn_get_size(scn));

//   if (!binseg) {
//      MADRAS_SEGMENT
//
//   } else {
//      //TODO Checks that binseg and machoseg indexes evolves at the same time
//      macho_segment_t* seg = macho_file_getsegment(macho_file, binseg_get_id(binseg));
//   }
}

/**
 * Add a new Mach-O section from a libbin representation
 * \param scn The newly created binscn_t section
 */
void macho_section64_newfrombinscn(binscn_t* scn) {
   if (!scn)
      return;

   binfile_t* bf              = binscn_get_binfile(scn);
   macho_section64_t* section = (macho_section64_t*)lc_malloc0(sizeof(macho_section64_t));

   section->offset   = REV_BYT(binscn_get_offset(scn));
   section->size     = REV_BYT(binscn_get_size(scn));
}

/**
 * Creates a libbin section from a Mach-O section representation
 * \param bf A binary file structure
 * \param seg The segment the section belongs to
 * \param sct The Mach-O representation of the section
 * \return A pointer to the created libbin section
 */
binscn_t* macho_binscn_newfromsection32(binfile_t* bf, macho_segment_t* seg, macho_section32_t* sct) {

   if (!bf || !seg || !sct)
      return NULL;

   int n_scn            = binfile_get_nb_sections(bf);
   uint8_t attributes   = SCNA_LOADED;
   scntype_t type       = SCNT_UNKNOWN;

   //Setting attributes. Libbin section's attributes are set at the segment level in macho binaries
   if (macho_segment_getinitprot(seg)&VM_PROT_READ)
      attributes |= SCNA_READ;
   if (macho_segment_getinitprot(seg)&VM_PROT_WRITE)
      attributes |= SCNA_WRITE;
   if (macho_segment_getinitprot(seg)&VM_PROT_EXECUTE)
      attributes |= SCNA_EXE;

   //Setting type
   if (strcmp(macho_segment_getname(seg), SEG_DATA) == 0)
      type = SCNT_DATA;

   switch(REV_BYT(sct->flags)&SECTION_TYPE){
   case(S_CSTRING_LITERALS):           type = SCNT_STRING;     break;
   case(S_ZEROFILL):                   type = SCNT_ZERODATA;   break;
   default:                            ;
   }

   switch(REV_BYT(sct->flags)&SECTION_ATTRIBUTES_USR){
   case(S_ATTR_PURE_INSTRUCTIONS):     type = SCNT_CODE;       break;
   case(S_ATTR_DEBUG):                 type = SCNT_DEBUG;      break;
   default:                            ;
   }

   DBGMSG("TYPE = %d\n", type);

   //Initializes a section (libbin level)
   binscn_t* section = binfile_init_scn(bf, n_scn, lc_strdup((char*)REV_BYT(sct->sectname)), type, REV_BYT(sct->addr), attributes);
   binscn_set_offset(section, REV_BYT(sct->offset));
   binscn_set_size(section, REV_BYT(sct->size));
   binscn_set_data(section, (unsigned char*)lc_malloc0(binscn_get_size(section)), TRUE);

   return section;
}

/**
 * Creates a libbin section from a Mach-O section representation
 * \param bf A binary file structure
 * \param seg The segment the section belongs to
 * \param sct The Mach-O representation of the section
 * \return A pointer to the created libbin section
 */
binscn_t* macho_binscn_newfromsection64(binfile_t* bf, macho_segment_t* seg, macho_section64_t* sct) {

   if (!bf || !seg || !sct)
      return NULL;

   int n_scn = binfile_get_nb_sections(bf);
   uint8_t attributes = SCNA_LOADED;
   scntype_t type       = SCNT_UNKNOWN;

   //Setting attributes. Libbin section's attributes are set at the segment level in macho binaries
   if (macho_segment_getinitprot(seg)&VM_PROT_READ)
      attributes |= SCNA_READ;
   if (macho_segment_getinitprot(seg)&VM_PROT_WRITE)
      attributes |= SCNA_WRITE;
   if (macho_segment_getinitprot(seg)&VM_PROT_EXECUTE)
      attributes |= SCNA_EXE;

      //Setting type
   if (strcmp(macho_segment_getname(seg), SEG_DATA) == 0)
      type = SCNT_DATA;

   switch(REV_BYT(sct->flags)&SECTION_TYPE){
   case(S_CSTRING_LITERALS):           type = SCNT_STRING;     break;
   case(S_ZEROFILL):                   type = SCNT_ZERODATA;   break;
   default:                            ;
   }

   switch(REV_BYT(sct->flags)&SECTION_ATTRIBUTES_USR){
   case(S_ATTR_PURE_INSTRUCTIONS):     type = SCNT_CODE;       break;
   case(S_ATTR_DEBUG):                 type = SCNT_DEBUG;      break;
   default:                            ;
   }

   //Initializes a section (libbin level)
   binscn_t* section = binfile_init_scn(bf, n_scn, lc_strdup((char*)REV_BYT(sct->sectname)), type, REV_BYT(sct->addr), attributes);
   binscn_set_offset(section, REV_BYT(sct->offset));
   binscn_set_size(section, REV_BYT(sct->size));
   binscn_set_data(section, (unsigned char*)lc_malloc0(binscn_get_size(section)), TRUE);

   return section;
}
