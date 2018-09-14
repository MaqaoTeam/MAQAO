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
 * \file libstone.c
 * \brief This file contains all functions needed to parse, modify and create PE files
 * */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <time.h>

#include "libstone.h"
#include "archinterface.h"
#include "libmasm.h"

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

///////////////////////////////////////////////////////////////////////////////
//                                  Binfiles                                 //
///////////////////////////////////////////////////////////////////////////////

/*
 * Loads a binfile_t structure with the result of the parsing of an PE file or directory
 * THIS IS THE ENTRY POINT
 * \param bf A binary file structure containing an opened stream to the file to parse
 * \return Error code if the file could not be successfully parsed as an PE file, EXIT_SUCCESS otherwise. In this
 * case, the structure representing the binary file will have been updated with the result of the parsing
 * */
int pe_binfile_load(binfile_t* bf) {

   int error;
   int filedesc;
   coff_file_t* coff_file;

   if (!bf)
      return ERR_BINARY_MISSING_BINFILE;

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

   coff_file = coff_file_new(bf);

   //Loading the DOS header and stubs
   int res = dos_load(filedesc, coff_file);
   if (ISERROR(res))
      return res;

   binfile_set_format(bf, BFF_WINPE);

   //Parse the binary file
   return coff_file_load(filedesc, coff_file);
}

/*
 * Returns the first loaded address
 * \param bf A binary file structure containing an opened stream to the file to parse
 * \return The first loaded address of this binary file
 */
int64_t binfile_get_firstloadaddr(binfile_t* bf) {
   return (bf)?coff_file_get_firstloadaddr((coff_file_t*)binfile_get_driver(bf)):0;
}

/*
 * Returns the last loaded address
 * \param bf A binary file structure containing an opened stream to the file to parse
 * \return The last loaded address of this binary file
 */
int64_t binfile_get_lastloadaddr(binfile_t* bf) {
   return (bf)?coff_file_get_lastloadaddr((coff_file_t*)binfile_get_driver(bf)):0;
}

///////////////////////////////////////////////////////////////////////////////
//                                 COFF-file                                 //
///////////////////////////////////////////////////////////////////////////////

/*
 * Creates a new COFF structure representing the binary file
 * \param bf The common binary file structure
 * \return A new architecture-specific representation
 */
coff_file_t* coff_file_new(binfile_t* bf){

   if (!bf)
      return NULL;

   coff_file_t* coff_file     = (coff_file_t*)lc_malloc0(sizeof(coff_file_t));
   coff_file->binfile         = bf;

   //Setting format-specific information in the driver
   bf_driver_t* driver                    = binfile_get_driver(bf);
   driver->parsedbin                      = coff_file;
   driver->binfile_parse_dbg               = coff_file_parsedbg;
   driver->parsedbin_free                 = coff_file_free;
   driver->asmfile_add_ext_labels             = coff_file_asmfile_addlabels;
   driver->binfile_patch_get_first_load_addr = binfile_get_firstloadaddr;
   driver->binfile_patch_get_last_load_addr  = binfile_get_lastloadaddr;
   driver->generate_ext_label_name                = coff_file_gen_extlabelname;
   driver->asmfile_print_binfile           = coff_file_asmfile_print_binfile;

   return coff_file;
}

/*
 * Frees a COFF structure representing the binary file
 * \param coff_file_ptr A pointer to the format specific structure
 */
void coff_file_free(void* coff_file_ptr) {

   if (!coff_file_ptr)
      return;

   coff_file_t* coff_file = (coff_file_t*)coff_file_ptr;

   if (coff_file->coff_header)
      lc_free(coff_file->coff_header);
   if (coff_file->dos_header)
      lc_free(coff_file->dos_header);
   if (coff_file->dos_stub)
      lc_free(coff_file->dos_stub);

   lc_free(coff_file);
}

/*
 * Loads a coff_file_t structure with the result of the parsing of a PE executable file.
 * It assumes the executable file is opened.
 * \param filedesc A file descriptor
 * \param coff_file The COFF structure representing the binary file
 * \return Error code if the file could not be successfully parsed as an PE file, EXIT_SUCCESS otherwise. In this
 * case, the structure representing the binary file will have been updated with the result of the parsing
 */
int coff_file_load(int filedesc, coff_file_t* coff_file) {

   if (filedesc < 0)
      return ERR_COMMON_FILE_STREAM_MISSING;
   if (!coff_file)
      return ERR_BINARY_MISSING_BINFILE;

   binfile_t* bf = coff_file_get_binfile(coff_file);

   coff_header_t* coff_header = (coff_header_t*)lc_malloc0(sizeof(coff_header_t));

   // Get the COFF header
   SAFE(read(filedesc, coff_header, sizeof(coff_header_t)));

   DBGMSG("Coff signature: %#"PRIx32"\n", coff_header->Signature);
   DBGMSG("Coff machine: %#"PRIx32"\n", coff_header->Machine);
   DBGMSG("Coff number of sections: %#"PRIx32"\n", coff_header->NumberOfSections);
   DBGMSG("Coff time stamp: %s", ctime((time_t*)&(coff_header->TimeDateStamp)));
   DBGMSG("Coff size of optional header: %#"PRIx16"\n", coff_header->SizeOfOptionalHeader);
   DBGMSG("Coff characteristics: %#"PRIx16"\n", coff_header->Characteristics);
   DBGMSG("Coff symbol table: %#"PRIx32"\n", coff_header->PointerToSymbolTable);

   // Check the signature
   if (coff_header->Signature != COFF_SIGNATURE) {
      lc_free(coff_header);
      return ERR_BINARY_FORMAT_NOT_RECOGNIZED;
   }

   coff_file->coff_header = coff_header;

   // Set architecture
   binfile_set_arch(coff_file_get_binfile(coff_file), getarch_bybincode(BFF_WINPE, coff_header->Machine));

   // Parse the optional COFF header
   if (coff_header->SizeOfOptionalHeader > 0) {
      uint16_t magic;

      SAFE(read(filedesc, &magic, sizeof(magic)));
      SAFE(lseek(filedesc, - sizeof(magic), SEEK_CUR));

      if (magic == COFF_OPTIONAL_HEADER_MAGIC_32B)
      {
         binfile_set_word_size(coff_file_get_binfile(coff_file), BFS_32BITS);
         coff_optional_header32_t* coff_optional_header = (coff_optional_header32_t*)lc_malloc(sizeof(coff_optional_header32_t));

         SAFE(read(filedesc, coff_optional_header, sizeof(coff_optional_header32_t)));

         coff_file->coff_optional_header.header32 = coff_optional_header;

      } else if (magic == COFF_OPTIONAL_HEADER_MAGIC_64B)
      {
         binfile_set_word_size(coff_file_get_binfile(coff_file), BFS_64BITS);
         coff_optional_header64_t* coff_optional_header = (coff_optional_header64_t*)lc_malloc(sizeof(coff_optional_header64_t));

         SAFE(read(filedesc, coff_optional_header, sizeof(coff_optional_header64_t)));

         coff_file->coff_optional_header.header64 = coff_optional_header;

         DBGMSG("Coff entry point: %#"PRIx32"\n", coff_optional_header->AddressOfEntryPoint);
         DBGMSG("Coff base of code: %#"PRIx32"\n", coff_optional_header->BaseOfCode);
         DBGMSG("Coff image base: %#"PRIx64"\n", coff_optional_header->ImageBase);
         DBGMSG("Coff debug VA: %#"PRIx32"\n", coff_optional_header->Debug.VirtualAddress);
         DBGMSG("Coff debug size: %#"PRIx32"\n", coff_optional_header->Debug.Size);

      }
   }

   // Allocates space for the sections
   coff_file->sections     = (section_t**)lc_malloc0(sizeof(section_t*)*coff_header->NumberOfSections);
   coff_file->nb_sections  = coff_header->NumberOfSections;

   // Parses the sections
   int i;
   for (i = 0; i < coff_header->NumberOfSections; i++)
   {
      coff_file->sections[i] = (section_t*)lc_malloc0(sizeof(section_t));
      coff_file_parse_section(filedesc, bf, coff_file, coff_file->sections[i]);
   }

   return EXIT_SUCCESS;
}

/**
 * Gets the file offset corresponding to a virtual address
 * \param coff_file The COFF structure representing the binary file
 * \param virtual_address The virtual address
 * \return The offset corresponding to the virtual address or UNSIGNED_ERROR if there is no match.
 */
static uint64_t coff_file_get_offset_from_relative_virtual_address(coff_file_t* coff_file, uint64_t virtual_address)
{
   if (!coff_file)
      return UNSIGNED_ERROR;

   int i = 0, nb_sections = coff_file_get_nb_sections(coff_file);
   uint64_t offset = UNSIGNED_ERROR;

   // Finds the section mapped to this virtual address
   section_t* section = coff_file_get_section(coff_file, i);
   while( (i < nb_sections) && (virtual_address > section_get_virtual_address(section) + section_get_virtual_size(section)) )
   {
      i++;
      section = coff_file_get_section(coff_file, i);
   }

   // Subtracts the difference between physical and virtual addresses
   if (i != nb_sections)
      offset = virtual_address - (section_get_virtual_address(section) - section_get_offset(section));

   return offset;
}

static section_t* coff_file_get_section_from_virtual_address(coff_file_t* coff_file, uint64_t virtual_address)
{
   if (!coff_file)
      return NULL;

   int i = 0, nb_sections = coff_file_get_nb_sections(coff_file);
   int64_t image_base = coff_file_get_firstloadaddr(coff_file);

   // Finds the section mapped to this virtual address
   section_t* section = coff_file_get_section(coff_file, i);
   while( (i < nb_sections) && (virtual_address > section_get_virtual_address(section)
         + section_get_virtual_size(section) + image_base) )
   {
      i++;
      section = coff_file_get_section(coff_file, i);
   }

   return section;
}

/*
 * Parses a section and create it's binfile representation
 * \param filedesc The file descriptor, it should be set at the section header address
 * \param bf A binary file structure
 * \param coff_file The COFF structure representing the binary file
 * \param section A pointer to an allocated section structure
 */
void coff_file_parse_section(int filedesc, binfile_t* bf, coff_file_t* coff_file, section_t* section)
{
   // Parses the section header
   section_header_t* header = (section_header_t*)lc_malloc0(sizeof(section_header_t));
   SAFE(read(filedesc, header, sizeof(section_header_t)));

   DBGMSG("Section's name: %s\n", header->Name);
   DBGMSG("Section's virtual size: %#"PRIx32"\n", header->VirtualSize);
   DBGMSG("Section's virtual address: %#"PRIx32"\n", header->VirtualAddress);
   DBGMSG("Section's size of raw data: %#"PRIx32"\n", header->SizeOfRawData);
   DBGMSG("Section's pointer to raw data: %#"PRIx32"\n", header->PointerToRawData);
   DBGMSG("Section's characteristics: %#"PRIx32"\n", header->Characteristics);

   int n_scn = binfile_get_nb_sections(bf);
   uint8_t attributes = SCNA_LOADED;
   scntype_t type     = SCNT_UNKNOWN;

   if (header->Characteristics & SECTION_CNT_CODE)
      type = SCNT_CODE;
   if ((header->Characteristics & SECTION_CNT_INITIALIZED_DATA)
         || (header->Characteristics & SECTION_CNT_UNINITIALIZED_DATA))
      type = SCNT_DATA;
   if (header->Characteristics & SECTION_SCN_MEM_EXECUTE)
      attributes |= SCNA_EXE;
   if (header->Characteristics & SECTION_SCN_MEM_READ)
      attributes |= SCNA_READ;
   if (header->Characteristics & SECTION_SCN_MEM_WRITE)
      attributes |= SCNA_WRITE;

   // Initializes a binfile representation of a section
   binscn_t* binsct = binfile_init_scn(bf, n_scn, lc_strdup((char*)header->Name), type,
         coff_file_get_firstloadaddr(coff_file) + header->VirtualAddress, attributes);

   binscn_set_offset(binsct, header->PointerToRawData);
   binscn_set_size(binsct, header->VirtualSize);
   binscn_set_data(binsct, (unsigned char*)lc_malloc0(header->SizeOfRawData), TRUE);

   // Gets the data
   SAFE(pread(filedesc, binscn_get_data(binsct, NULL), header->SizeOfRawData, binscn_get_offset(binsct)));

   section->header = header;
   section->binscn = binsct;
}

/*
 * Parses the function table of a COFF file
 * \param filedesc The file descriptor, it's position will be restored at the end of the parsing
 * \param coff_file The COFF structure representing the binary file
 */
void coff_file_parse_functions(int filedesc, coff_file_t* coff_file)
{
   if (!coff_file)
      return;

   binfile_t* bf = coff_file_get_binfile(coff_file);
   uint32_t virtual_address = 0, size = 0;
   uint64_t offset = 0, main_address = 0;

   int i;

   // Gets the virtual address of the function table
   if (binfile_get_word_size(bf) == BFS_32BITS)
   {
      virtual_address = coff_file->coff_optional_header.header32->ExceptionTable.VirtualAddress;
      size = coff_file->coff_optional_header.header32->ExceptionTable.Size;
      main_address = coff_file_get_firstloadaddr(coff_file) + coff_file->coff_optional_header.header32->AddressOfEntryPoint;
   }
   else if (binfile_get_word_size(bf) == BFS_64BITS)
   {
      virtual_address = coff_file->coff_optional_header.header64->ExceptionTable.VirtualAddress;
      size = coff_file->coff_optional_header.header64->ExceptionTable.Size;
      main_address = coff_file_get_firstloadaddr(coff_file) + coff_file->coff_optional_header.header64->AddressOfEntryPoint;
   }

   // Adds a label for the entry point of the file
   DBGMSG("Add a new function label %s at %#"PRIx64"\n", "main", main_address);
   label_t* mainlbl = label_new("main", main_address, TARGET_INSN, NULL);
   label_set_type(mainlbl, LBL_FUNCTION);
   label_set_scn(mainlbl, coff_file_get_section_from_virtual_address(coff_file, main_address)->binscn);
   asmfile_add_label_unsorted (binfile_get_asmfile(bf), mainlbl);

   DBGMSG("Function table virtual address: %#"PRIx32"\n", virtual_address);
   DBGMSG("Function table size: %#"PRIx32"\n", size);

   // Translates it into a file offset
   offset = coff_file_get_offset_from_relative_virtual_address(coff_file, virtual_address);

   DBGMSG("Function table offset: %#"PRIx64"\n", offset);

   // Stores the current position for coming back after the import tables parsing
   int position = lseek(filedesc, 0, SEEK_CUR);
   if (position <= 0)
      HLTMSG("Error when reading binary header !\n");

   // Moves the file descriptor position
   SAFE(lseek(filedesc, offset, SEEK_SET));

   // Parses entries
   int32_t entry_size = 0, nb_entries = 0;

   switch (coff_file->coff_header->Machine)
   {
   case IMAGE_FILE_MACHINE_IA64:
   case IMAGE_FILE_MACHINE_AMD64:
   {
      entry_size = sizeof(coff_function_x64_entry_t);
      nb_entries = size/entry_size;
      coff_function_x64_entry_t entry;

      for (i = 0; i < nb_entries; i++)
      {
         SAFE(read(filedesc, &entry, entry_size));

         // Builds the new label
         char fctlabel[strlen(FCT_LBL) + 10 + 1];
         sprintf(fctlabel,FCT_LBL "%d", i+1);
         uint64_t label_address = coff_file_get_firstloadaddr(coff_file) + entry.BeginAddress;

         if (label_address != main_address)
         {
            // Adds a label to the file
            DBGMSG("Add a new function label %s at %#"PRIx64"\n", fctlabel, label_address);
            label_t* fctlbl = label_new(fctlabel, label_address, TARGET_INSN, NULL);
            label_set_type(fctlbl, LBL_FUNCTION);
            label_set_scn(fctlbl, coff_file_get_section_from_virtual_address(coff_file, label_address)->binscn);
            asmfile_add_label_unsorted (binfile_get_asmfile(bf), fctlbl);
         }
      }
      break;
   }
   default:
      return;
   }

   // Moves back
   SAFE(lseek(filedesc, position, SEEK_SET));
}

/*
 * Parses the import table of a COFF file
 * \param filedesc The file descriptor, it's position will be restored at the end of the parsing
 * \param coff_file The COFF structure representing the binary file
 */
void coff_file_parse_imports(int filedesc, coff_file_t* coff_file)
{
   if (!coff_file)
      return;

   binfile_t* bf = coff_file_get_binfile(coff_file);
   uint32_t virtual_address = 0, size = 0;
   uint64_t offset = 0;
   int i;

   // Gets the virtual address of the import table
   if (binfile_get_word_size(bf) == BFS_32BITS)
   {
      virtual_address = coff_file->coff_optional_header.header32->ImportTable.VirtualAddress;
      size = coff_file->coff_optional_header.header32->ImportTable.Size;
   } else if (binfile_get_word_size(bf) == BFS_64BITS)
   {
      virtual_address = coff_file->coff_optional_header.header64->ImportTable.VirtualAddress;
      size = coff_file->coff_optional_header.header64->ImportTable.Size;
   }

   DBGMSG("Import tables virtual address: %#"PRIx32"\n", virtual_address);
   DBGMSG("Import tables size: %#"PRIx32"\n", size);

   // Translates it into a file offset
   offset = coff_file_get_offset_from_relative_virtual_address(coff_file, virtual_address);

   DBGMSG("Import tables offset: %#"PRIx64"\n", offset);

   // Stores the current position for coming back after the import tables parsing
   int position = lseek(filedesc, 0, SEEK_CUR);
   if (position <= 0)
      HLTMSG("Error when reading binary header !\n");

   // Moves the file descriptor position
   SAFE(lseek(filedesc, offset, SEEK_SET));

   // Parses entries
   coff_file->nb_imports = size / sizeof(coff_import_entry_t);
   coff_file->import_entries = (coff_import_entry_t**)lc_malloc0(coff_file->nb_imports * sizeof(coff_import_entry_t*));

   // For each import table
   for (i = 0; i < coff_file->nb_imports; i++)
   {
      coff_file->import_entries[i] = (coff_import_entry_t*)lc_malloc0(sizeof(coff_import_entry_t));
      SAFE(read(filedesc, coff_file->import_entries[i], sizeof(coff_import_entry_t)));

      // The last entry is filled with NULL values...
      if (i < coff_file->nb_imports - 1)
      {
         uint64_t dll_name_offset = coff_file_get_offset_from_relative_virtual_address(coff_file, coff_file->import_entries[i]->Name);
         char dll_name[128];
         SAFE(pread(filedesc, &dll_name, sizeof(dll_name), dll_name_offset));
         DBGMSG("Imports from %s:\n", dll_name);

         // Gets the look up entries
         uint64_t table_lookup_offset = coff_file_get_offset_from_relative_virtual_address(coff_file, coff_file->import_entries[i]->ImportLookupTable);
         uint64_t look_up_entry = 0;
         uint32_t shift = 0;

         while (shift == 0 || look_up_entry != 0)
         {
            if (binfile_get_word_size(bf) == BFS_32BITS)
            {
               // Parses the name or ordinal flag of the import
               SAFE(pread(filedesc, &look_up_entry, sizeof(uint32_t), table_lookup_offset + shift));

               // Import by name
               if ((look_up_entry & COFF_IMPORT_LOOKUP_BY_ORDINAL_32B) == 0 && look_up_entry != 0)
               {
                  uint32_t name_offset = coff_file_get_offset_from_relative_virtual_address(coff_file, (look_up_entry & COFF_IMPORT_LOOKUP_NAME_32B));
                  char name[128];
                  SAFE(pread(filedesc, &name, sizeof(name), name_offset + 2));

                  // Builds the new label
                  char extlabel[strlen(name) + strlen(dll_name) + strlen(EXT_LBL_CHAR) + 1];
                  sprintf(extlabel,"%s" EXT_LBL_CHAR "%s", name, dll_name);
                  uint64_t label_address = coff_file_get_firstloadaddr(coff_file) +
                        coff_file->import_entries[i]->FirstThunk + shift;

                  // Adds a label to the file
                  DBGMSG("Add a new label %s at %#"PRIx64"\n", extlabel, label_address);
                  label_t* extlbl = label_new(extlabel, label_address, TARGET_DATA, NULL);
                  label_set_type(extlbl, LBL_VARIABLE);
                  label_set_scn(extlbl, coff_file_get_section_from_virtual_address(coff_file, label_address)->binscn);
                  asmfile_add_label_unsorted (binfile_get_asmfile(bf), extlbl);

                  // Adds the corresponding data
                  data_t* datalbl = binfile_adddata(bf, label_address, NULL, extlbl);
                  data_link_label(datalbl, extlbl);
               }
               shift += sizeof(uint32_t);
            }
            else if (binfile_get_word_size(bf) == BFS_64BITS)
            {
               // Parses the name or ordinal flag of the import
               SAFE(pread(filedesc, &look_up_entry, sizeof(uint64_t), table_lookup_offset + shift));

               // Import by name
               if ((look_up_entry & COFF_IMPORT_LOOKUP_BY_ORDINAL_64B) == 0 && look_up_entry != 0)
               {
                  uint64_t name_offset = coff_file_get_offset_from_relative_virtual_address(coff_file, (look_up_entry & COFF_IMPORT_LOOKUP_NAME_64B));
                  char name[128];
                  SAFE(pread(filedesc, &name, sizeof(name), name_offset + 2));

                  // Builds the new label
                  char extlabel[strlen(name) + strlen(dll_name) + strlen(EXT_LBL_CHAR) + 1];
                  sprintf(extlabel,"%s" EXT_LBL_CHAR "%s", name, dll_name);
                  uint64_t label_address = coff_file_get_firstloadaddr(coff_file) +
                        coff_file->import_entries[i]->FirstThunk + shift;

                  // Adds a label to the file
                  DBGMSG("Add a new label %s at %#"PRIx64"\n", extlabel, label_address);
                  label_t* extlbl = label_new(extlabel, label_address, TARGET_DATA, NULL);
                  label_set_type(extlbl, LBL_VARIABLE);
                  label_set_scn(extlbl, coff_file_get_section_from_virtual_address(coff_file, label_address)->binscn);
                  asmfile_add_label_unsorted (binfile_get_asmfile(bf), extlbl);

                  // Adds the corresponding data
                  data_t* datalbl = binfile_adddata(bf, label_address, NULL, extlbl);
                  data_link_label(datalbl, extlbl);
               }
               shift += sizeof(uint64_t);
            }
         }
      }
   }

   // Moves back
   SAFE(lseek(filedesc, position, SEEK_SET));
}

/*
 * Sets the coff header of the coff file representation
 * \param coff_file The COFF structure representing the binary file
 * \param coff_header The coff header to set
 */
void coff_file_set_coffheader(coff_file_t* coff_file, coff_header_t* coff_header){
   if (!coff_file || !coff_header)
      return;

   coff_file->coff_header = coff_header;
}

/*
 * Gets the binfile representation corresponding to a COFF file
 * \param coff_file The COFF structure representing the binary file
 * \return The binfile representation
 */
binfile_t* coff_file_get_binfile(coff_file_t* coff_file)
{
   return (coff_file)?coff_file->binfile:NULL;
}

/*
 * Gets the number of sections
 * \param coff_file The COFF structure representing the binary file
 * \return the number of sections
 */
uint32_t coff_file_get_nb_sections(coff_file_t* coff_file)
{
   return (coff_file)?coff_file->nb_sections:UNSIGNED_ERROR;
}

/*
 * Gets a section
 * \param coff_file The COFF structure representing the binary file
 * \param index The index of the section in the COFF structures
 * \return The section at the required index or NULL if there is none
 */
section_t* coff_file_get_section(coff_file_t* coff_file, int index)
{
   if (!coff_file || index >= coff_file->nb_sections)
      return NULL;

   return coff_file->sections[index];
}

/*
 * Returns a suffixed label corresponding to an external function
 * \param common_name The label to suffix
 * \return The suffixed string
 */
char* coff_file_gen_extlabelname(char* common_name) {
   if (!common_name)
      return NULL;

   char* full_name = lc_malloc(strlen(common_name) + strlen(EXT_LBL_CHAR) + 1);
   sprintf(full_name, "%s" EXT_LBL_CHAR, common_name);

   return full_name;
}

/*
 * Returns the first loaded address
 * \param coff_file The COFF structure representing the binary file
 * \return The first loaded address of this binary file
 */
int64_t coff_file_get_firstloadaddr(coff_file_t* coff_file) {

   if (!coff_file)
      return SIGNED_ERROR;

   int64_t first_load_addr = SIGNED_ERROR;
   binfile_t* bf = coff_file_get_binfile(coff_file);

   if (binfile_get_word_size(bf) == BFS_32BITS)
      first_load_addr = coff_file->coff_optional_header.header32->ImageBase;
   else if (binfile_get_word_size(bf) == BFS_64BITS)
      first_load_addr = coff_file->coff_optional_header.header64->ImageBase;

   return first_load_addr;
}

/*
 * Returns the last loaded address
 * \param coff_file The COFF structure representing the binary file
 * \return The last loaded address of this binary file
 */
int64_t coff_file_get_lastloadaddr(coff_file_t* coff_file) {
   return 0;
}

///////////////////////////////////////////////////////////////////////////////
//                                  Sections                                 //
///////////////////////////////////////////////////////////////////////////////

/*
 * Gets the virtual address of a COFF section
 * \param section The section to get the virtual address of
 * \return The virtual address of the section.
 */
uint32_t section_get_virtual_address(section_t* section)
{
   return (section)?section->header->VirtualAddress:UNSIGNED_ERROR;
}

/*
 * Gets the virtual size of a COFF section
 * \param section The section to get the virtual size of
 * \return The virtual size of the section.
 */
uint32_t section_get_virtual_size(section_t* section)
{
   return (section)?section->header->VirtualSize:UNSIGNED_ERROR;
}

/*
 * Gets the offset (in the file) of a COFF section
 * \param section The section to get the offset of
 * \return The offset of the section
 */
uint32_t section_get_offset(section_t* section)
{
   return (section)?section->header->PointerToRawData:UNSIGNED_ERROR;
}


///////////////////////////////////////////////////////////////////////////////
//                                 Dbgfiles                                  //
///////////////////////////////////////////////////////////////////////////////

dbg_file_t* coff_file_parsedbg(binfile_t* bf) {
   (void)bf;
   return NULL;
}

///////////////////////////////////////////////////////////////////////////////
//                                  Asmfile                                  //
///////////////////////////////////////////////////////////////////////////////

int coff_file_asmfile_addlabels(asmfile_t* asmf) {
   binfile_t* bf = asmfile_get_binfile(asmf);
   coff_file_t* coff_file = (coff_file_t*)binfile_get_parsed_bin(bf);
   int filedesc = filedesc = fileno(binfile_get_file_stream(bf));

   coff_file_parse_imports(filedesc, coff_file);
   coff_file_parse_functions(filedesc, coff_file);

   return TRUE;
}

void coff_file_asmfile_print_binfile(asmfile_t* asmf){
//   if (asmf)
//      coff_file_binfile_print(asmfile_get_binfile(asmf));
}

///////////////////////////////////////////////////////////////////////////////
//                                    DOS                                    //
///////////////////////////////////////////////////////////////////////////////

/*
 * Loads the DOS header of a PE executable file
 * This header being mandatory, if we cannot match the magic number (signature) we abort the parsing
 * \param filedesc The file descriptor corresponding to the PE file
 * \param coff_file The COFF structure representing the binary file
 * \return Error code if the file could not be successfully parsed as a PE file, EXIT_SUCCESS otherwise. In this
 * case, the structure representing the binary file will have been updated with the result of the parsing
 */
uint32_t dos_load(int filedesc, coff_file_t* coff_file)
{

   if (filedesc < 0)
      return ERR_COMMON_FILE_STREAM_MISSING;
   if (!coff_file)
      return ERR_BINARY_MISSING_BINFILE;

   dos_header_t* dos_header = (dos_header_t*)lc_malloc0(sizeof(dos_header_t));

   // Gets the DOS header
   SAFE(read(filedesc, dos_header, sizeof(dos_header_t)));

   if (dos_header->Signature != DOS_MAGIC) {
      lc_free(dos_header);
      return ERR_BINARY_FORMAT_NOT_RECOGNIZED;
   }

   coff_file->dos_header = dos_header;

   // Gets the DOS stub. The size of the DOS stub is not static
   uint32_t dos_stub_size = dos_header->PEAddress - sizeof(dos_header_t);

   DBGMSG("DOS stub size: %d\n", dos_stub_size);

   dos_stub_t* dos_stub = (dos_stub_t*)lc_malloc0(sizeof(dos_stub_t));
   dos_stub->SizeOfStub = dos_stub_size;
   dos_stub->Stub = (void*)lc_malloc(dos_stub_size);
   SAFE(read(filedesc, dos_stub->Stub, dos_stub_size));

   coff_file->dos_stub = dos_stub;

   return EXIT_SUCCESS;
}
