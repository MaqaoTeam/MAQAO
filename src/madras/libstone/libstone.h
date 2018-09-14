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

#ifndef LIBSTONE_H__
#define LIBSTONE_H__

#include "libmcommon.h"
#include "libmasm.h"

/**
 * Suffix of external labels
 * */
#define EXT_LBL_CHAR "@"

/**
 * Default function name
 */
#define FCT_LBL "Function n°"

typedef struct section_s section_t;

///////////////////////////////////////////////////////////////////////////////
//                   DOS HEADER/STUB STRUCTURES AND DEFINES                  //
///////////////////////////////////////////////////////////////////////////////

typedef struct DOS_HEADER {
   uint16_t Signature;
   unsigned char Unknown[58];
   uint32_t PEAddress;
} dos_header_t;

typedef struct DOS_STUB {
   void* Stub;
   uint32_t SizeOfStub;
} dos_stub_t;

#define DOS_MAGIC       0x5a4d

///////////////////////////////////////////////////////////////////////////////
//                     COFF HEADER STRUCTURES AND DEFINES                    //
///////////////////////////////////////////////////////////////////////////////

typedef struct COFF_HEADER {
   uint32_t Signature;
   uint16_t Machine;
   uint16_t NumberOfSections;
   uint32_t TimeDateStamp;
   uint32_t PointerToSymbolTable;   // DEPRECATED.
   uint32_t NumberOfSymbolTable;    // DEPRECATED.
   uint16_t SizeOfOptionalHeader;
   uint16_t Characteristics;
} coff_header_t;

#define COFF_SIGNATURE                       0x00004550

/* These defines describe the meaning of the the bits in the Machine field */
#define IMAGE_FILE_MACHINE_UNKNOWN           0
#define IMAGE_FILE_MACHINE_I386              0x014c  // Intel 386 or later processors and compatible processors
#define IMAGE_FILE_MACHINE_R3000             0x0162  // MIPS little-endian, 0x160 big-endian
#define IMAGE_FILE_MACHINE_R4000             0x0166  // MIPS little-endian
#define IMAGE_FILE_MACHINE_R10000            0x0168  // MIPS little-endian
#define IMAGE_FILE_MACHINE_WCEMIPSV2         0x0169  // MIPS little-endian WCE v2
#define IMAGE_FILE_MACHINE_ALPHA             0x0184  // Alpha_AXP
#define IMAGE_FILE_MACHINE_SH3               0x01a2  // Hitashi SH3 little-endian
#define IMAGE_FILE_MACHINE_SH3DSP            0x01a3  // Hitashi SH3 DSP
#define IMAGE_FILE_MACHINE_SH3E              0x01a4  // Hitashi SH3E
#define IMAGE_FILE_MACHINE_SH4               0x01a6  // Hitashi SH4
#define IMAGE_FILE_MACHINE_SH5               0x01a8  // Hitashi SH5
#define IMAGE_FILE_MACHINE_ARM               0x01c0  // ARM Little-Endian
#define IMAGE_FILE_MACHINE_THUMB             0x01c2  // ARM or Thumb/Thumb-2 (interworking)
#define IMAGE_FILE_MACHINE_ARMNT             0x01c4  // ARMv7 Thumb mode only
#define IMAGE_FILE_MACHINE_ARM64             0xaa64  // ARMv8 in 64-bit mode
#define IMAGE_FILE_MACHINE_AM33              0x01d3  // Matsushita AM33
#define IMAGE_FILE_MACHINE_POWERPC           0x01F0  // IBM PowerPC Little-Endian
#define IMAGE_FILE_MACHINE_POWERPCFP         0x01f1  // IBM PowerPC with floating point support
#define IMAGE_FILE_MACHINE_IA64              0x0200  // Intel Itanium processor family
#define IMAGE_FILE_MACHINE_MIPS16            0x0266  // MIPS16
#define IMAGE_FILE_MACHINE_ALPHA64           0x0284  // ALPHA64
#define IMAGE_FILE_MACHINE_MIPSFPU           0x0366  // MIPS with FPU
#define IMAGE_FILE_MACHINE_MIPSFPU16         0x0466  // MIPS16 with FPU
#define IMAGE_FILE_MACHINE_AXP64             IMAGE_FILE_MACHINE_ALPHA64
#define IMAGE_FILE_MACHINE_TRICORE           0x0520  // Infineon
#define IMAGE_FILE_MACHINE_CEF               0x0CEF
#define IMAGE_FILE_MACHINE_EBC               0x0EBC  // EFI Byte Code
#define IMAGE_FILE_MACHINE_AMD64             0x8664  // x86_64
#define IMAGE_FILE_MACHINE_M32R              0x9041  // Mitsubishi M32R little-endian
#define IMAGE_FILE_MACHINE_CEE               0xC0EE

/* These defines describe the meanings of the bits in the Characteristics field */
#define IMAGE_FILE_RELOCS_STRIPPED           0x0001 // Image only: no relocation info
#define IMAGE_FILE_EXECUTABLE_IMAGE          0x0002 // Image only: valid image that can be executed
#define IMAGE_FILE_LINE_NUMS_STRIPPED        0x0004 // DEPRECATED. COFF line numbers removed.
#define IMAGE_FILE_LOCAL_SYMS_STRIPPED       0x0008 // DEPRECATED. COFF symbol table entries removed.
#define IMAGE_FILE_AGGRESIVE_WS_TRIM         0x0010 // OBSOLETE. Aggressively trim working set.
#define IMAGE_FILE_LARGE_ADDRESS_AWARE       0x0020 // Application can handle > 2GB addresses.
#define IMAGE_FILE_16BIT_MACHINE             0x0040 // This flag is reserved for future use.
#define IMAGE_FILE_BYTES_REVERSED_LO         0x0080 // DEPRECATED. Little endian.
#define IMAGE_FILE_32BIT_MACHINE             0x0100 // Machine is based on a 32-bit-word architecture.
#define IMAGE_FILE_DEBUG_STRIPPED            0x0200 // Debugging information removed from the image file.
#define IMAGE_FILE_REMOVABLE_RUN_FROM_SWAP   0x0400 // If the image is on removable media, copy it to the swap file.
#define IMAGE_FILE_NET_RUN_FROM_SWAP         0x0800 // If the image is on network, copy it to the swap file
#define IMAGE_FILE_SYSTEM                    0x1000 // The image file is a system file, not a user program.
#define IMAGE_FILE_DLL                       0x2000 // The image file is a dynamic-link library.
#define IMAGE_FILE_UP_SYSTEM_ONLY            0x4000 // The file should be run only on a uniprocessor machine.
#define IMAGE_FILE_BYTES_REVERSED_HI         0x8000 // DEPRECATED. Big endian.

///////////////////////////////////////////////////////////////////////////////
//                COFF OPTIONAL HEADER STRUCTURES AND DEFINES                //
///////////////////////////////////////////////////////////////////////////////

typedef struct COFF_DATA_DIRECTORY {
    uint32_t VirtualAddress;
    uint32_t Size;
} coff_data_directory_t;

typedef struct COFF_OPTIONAL_HEADER32 {
    uint16_t Magic;
    unsigned char MajorLinkerVersion;
    unsigned char MinorLinkerVersion;
    uint32_t SizeOfCode;
    uint32_t SizeOfInitializedData;
    uint32_t SizeOfUnitializedData;
    uint32_t AddressOfEntryPoint;
    uint32_t BaseOfCode;
    uint32_t BaseOfData;
    uint32_t ImageBase;
    uint32_t SectionAlignment;
    uint32_t FileAlignment;
    uint16_t MajorOperatingSystemVersion;
    uint16_t MinorOperatingSystemVersion;
    uint16_t MajorImageVersion;
    uint16_t MinorImageVersion;
    uint16_t MajorSubsystemVersion;
    uint16_t MinorSubsystemVersion;
    uint32_t Win32VersionValue;
    uint32_t SizeOfVersion;
    uint32_t SizeOfHeaders;
    uint32_t CheckSum;
    uint16_t Subsystem;
    uint16_t DllCharacteristics;
    uint32_t SizeOfStackReserve;
    uint32_t SizeOfStackCommit;
    uint32_t SizeOfHeapReserve;
    uint32_t SizeOfHeapCommit;
    uint32_t LoaderFlags;
    uint32_t NumberOfRvaAndSizes;
    coff_data_directory_t ExportTable;
    coff_data_directory_t ImportTable;
    coff_data_directory_t ResourceTable;
    coff_data_directory_t ExceptionTable;
    coff_data_directory_t CertificateTable;
    coff_data_directory_t BaseRelocationTable;
    coff_data_directory_t Debug;
    coff_data_directory_t Architecture;            // RESERVED. Must be 0
    coff_data_directory_t GlobalPointer;
    coff_data_directory_t TLSTable;
    coff_data_directory_t LoadConfigTable;
    coff_data_directory_t BoundImportTable;
    coff_data_directory_t ImportAddressTable;
    coff_data_directory_t DelayImportDescriptor;
    coff_data_directory_t CLRRuntimeHeader;
    coff_data_directory_t EmptyTable;              // RESERVED. Must be 0
} coff_optional_header32_t;

typedef struct COFF_OPTIONAL_HEADER64 {
    uint16_t Magic;
    unsigned char MajorLinkerVersion;
    unsigned char MinorLinkerVersion;
    uint32_t SizeOfCode;
    uint32_t SizeOfInitializedData;
    uint32_t SizeOfUnitializedData;
    uint32_t AddressOfEntryPoint;
    uint32_t BaseOfCode;
    uint64_t ImageBase;
    uint32_t SectionAlignment;
    uint32_t FileAlignment;
    uint16_t MajorOperatingSystemVersion;
    uint16_t MinorOperatingSystemVersion;
    uint16_t MajorImageVersion;
    uint16_t MinorImageVersion;
    uint16_t MajorSubsystemVersion;
    uint16_t MinorSubsystemVersion;
    uint32_t Win32VersionValue;
    uint32_t SizeOfImage;
    uint32_t SizeOfHeaders;
    uint32_t CheckSum;
    uint16_t Subsystem;
    uint16_t DllCharacteristics;
    uint64_t SizeOfStackReserve;
    uint64_t SizeOfStackCommit;
    uint64_t SizeOfHeapReserve;
    uint64_t SizeOfHeapCommit;
    uint32_t LoaderFlags;
    uint32_t NumberOfRvaAndSizes;
    coff_data_directory_t ExportTable;
    coff_data_directory_t ImportTable;
    coff_data_directory_t ResourceTable;
    coff_data_directory_t ExceptionTable;
    coff_data_directory_t CertificateTable;
    coff_data_directory_t BaseRelocationTable;
    coff_data_directory_t Debug;
    coff_data_directory_t Architecture;            // RESERVED. Must be 0
    coff_data_directory_t GlobalPointer;
    coff_data_directory_t TLSTable;
    coff_data_directory_t LoadConfigTable;
    coff_data_directory_t BoundImportTable;
    coff_data_directory_t ImportAddressTable;
    coff_data_directory_t DelayImportDescriptor;
    coff_data_directory_t CLRRuntimeHeader;
    coff_data_directory_t EmptyTable;              // RESERVED. Must be 0
} coff_optional_header64_t;

#define COFF_OPTIONAL_HEADER_MAGIC_32B       0x10B
#define COFF_OPTIONAL_HEADER_MAGIC_64B       0x20B
#define COFF_OPTIONAL_HEADER_MAGIC_ROM_IMAGE 0x107
#define COFF_OPTIONAL_HEADER_NUMBEROF_MANDATORY_DIRECTORY_ENTRIES 16

// Imports

typedef struct COFF_IMPORT_ENTRY {
    uint32_t   ImportLookupTable;   // RVA to the Import Lookup Table.
    uint32_t   TimeDateStamp;       // 0 until the image is bound.
    uint32_t   ForwarderChain;      // -1 if no forwarders
    uint32_t   Name;                // Address of an ASCII string that contains the name of the DLL.
    uint32_t   FirstThunk;          // RVA to the Import Address Table.
} coff_import_entry_t;

#define COFF_IMPORT_LOOKUP_BY_ORDINAL_32B    0x80000000
#define COFF_IMPORT_LOOKUP_ORDINAL_32B       0x0000FFFF
#define COFF_IMPORT_LOOKUP_NAME_32B          0x7FFFFFFF
#define COFF_IMPORT_LOOKUP_BY_ORDINAL_64B    0x8000000000000000
#define COFF_IMPORT_LOOKUP_ORDINAL_64B       0x000000000000FFFF
#define COFF_IMPORT_LOOKUP_NAME_64B          0x000000007FFFFFFF

// Exceptions (functions)

typedef struct COFF_FUNCTION_X64_ENTRY {
   uint32_t BeginAddress;        // RVA of the corresponding function
   uint32_t EndAddress;          // RVA of the end of the function
   uint32_t UnwindInformation;   // RVA of the unwind information
} coff_function_x64_entry_t;


///////////////////////////////////////////////////////////////////////////////
//                                   DEBUG                                   //
///////////////////////////////////////////////////////////////////////////////

typedef struct DEBUG_DIRECTORY {
    uint32_t    Characteristics;
    uint32_t    TimeDateStamp;
    uint16_t    MajorVersion;
    uint16_t    MinorVersion;
    uint32_t    Type;
    uint32_t    SizeOfData;
    uint32_t    AddressOfRawData;
    uint32_t    PointerToRawData;
} debug_directory_t;

///////////////////////////////////////////////////////////////////////////////
//                               SECTION STRUCTURES                          //
///////////////////////////////////////////////////////////////////////////////

typedef struct SECTION_HEADER {
   unsigned char Name[8];
   uint32_t VirtualSize;
   uint32_t VirtualAddress;
   uint32_t SizeOfRawData;
   uint32_t PointerToRawData;
   uint32_t PointerToRelocations;
   uint32_t PointerToLinenumbers;
   uint16_t NumberOfRelocations;
   uint16_t NumberOfLinenumbers;
   uint32_t Characteristics;
} section_header_t;

#define SECTION_CNT_CODE                  0x00000020     // The section contains executable code.
#define SECTION_CNT_INITIALIZED_DATA      0x00000040     // The section contains initialized data.
#define SECTION_CNT_UNINITIALIZED_DATA    0x00000080     // The section contains uninitialized data.
#define SECTION_SCN_MEM_EXECUTE           0x20000000     // The section can be executed as code.
#define SECTION_SCN_MEM_READ              0x40000000     // The section can be read.
#define SECTION_SCN_MEM_WRITE             0x80000000     // The section can be written to.

///////////////////////////////////////////////////////////////////////////////
//                                  Binfiles                                 //
///////////////////////////////////////////////////////////////////////////////

/**
 * Loads a binfile_t structure with the result of the parsing of an PE file or directory
 * THIS IS THE ENTRY POINT
 * \param bf A binary file structure containing an opened stream to the file to parse
 * \return Error code if the file could not be successfully parsed as an PE file, EXIT_SUCCESS otherwise. In this
 * case, the structure representing the binary file will have been updated with the result of the parsing
 * */
extern int pe_binfile_load(binfile_t* bf);

/**
 * Returns the first loaded address
 * \param bf A binary file structure containing an opened stream to the file to parse
 * \return The first loaded address of this binary file
 */
extern int64_t binfile_get_firstloadaddr(binfile_t* bf);

/**
 * Returns the last loaded address
 * \param bf A binary file structure containing an opened stream to the file to parse
 * \return The last loaded address of this binary file
 */
extern int64_t binfile_get_lastloadaddr(binfile_t* bf);

///////////////////////////////////////////////////////////////////////////////
//                                 COFF-file                                 //
///////////////////////////////////////////////////////////////////////////////

typedef struct coff_file_s coff_file_t;

struct coff_file_s {
   binfile_t*                    binfile;          // Link to the common representation
   dos_header_t*                 dos_header;       // DOS header, mandatory in WinPE binaries
   dos_stub_t*                   dos_stub;         // DOS stub, it is not always the same
   coff_header_t*                coff_header;      // COFF header
   union {
      coff_optional_header32_t*  header32;
      coff_optional_header64_t*  header64;
   } coff_optional_header;                         // Optional header
   section_t**                   sections;         // Sections (include their headers)
   uint32_t                      nb_sections;      // Number of sections
   coff_import_entry_t**         import_entries;   // Import tables
   uint32_t                      nb_imports;       // Number of import tables
};

/**
 * Creates a new COFF structure representing the binary file
 * \param bf The common binary file structure
 * \return A new architecture-specific representation
 */
extern coff_file_t* coff_file_new(binfile_t* bf);

/**
 * Frees a COFF structure representing the binary file
 * \param coff_file_ptr A pointer to the format specific structure
 */
extern void coff_file_free(void* coff_file_ptr);

/**
 * Sets the coff header of the coff file representation
 * \param coff_file The COFF structure representing the binary file
 * \param coff_header The coff header to set
 */
extern void coff_file_set_coffheader(coff_file_t* coff_file, coff_header_t* coff_header);

/**
 * Gets the binfile representation corresponding to a COFF file
 * \param coff_file The COFF structure representing the binary file
 * \return The binfile representation
 */
extern binfile_t* coff_file_get_binfile(coff_file_t* coff_file);

/**
 * Gets the number of sections
 * \param coff_file The COFF structure representing the binary file
 * \return the number of sections
 */
extern uint32_t coff_file_get_nb_sections(coff_file_t* coff_file);

/**
 * Gets a section
 * \param coff_file The COFF structure representing the binary file
 * \param index The index of the section in the COFF structures
 * \return The section at the required index or NULL if there is none
 */
extern section_t* coff_file_get_section(coff_file_t* coff_file, int index);

/**
 * Returns a suffixed label corresponding to an external function
 * \param common_name The label to suffix
 * \return The suffixed string
 */
extern char* coff_file_gen_extlabelname(char* common_name);

/**
 * Returns the first loaded address
 * \param coff_file The COFF structure representing the binary file
 * \return The first loaded address of this binary file
 */
extern int64_t coff_file_get_firstloadaddr(coff_file_t* coff_file);

/**
 * Returns the last loaded address
 * \param coff_file The COFF structure representing the binary file
 * \return The last loaded address of this binary file
 */
extern int64_t coff_file_get_lastloadaddr(coff_file_t* coff_file);

/**
 * Parses a section and create it's binfile representation
 * \param filedesc The file descriptor, it should be set at the section header address
 * \param bf A binary file structure
 * \param coff_file The COFF structure representing the binary file
 * \param section A pointer to an allocated section structure
 */
extern void coff_file_parse_section(int filedesc, binfile_t* bf, coff_file_t* coff_file, section_t* section);

/**
 * Parses the function table of a COFF file
 * \param filedesc The file descriptor, it's position will be restored at the end of the parsing
 * \param coff_file The COFF structure representing the binary file
 */
extern void coff_file_parse_functions(int filedesc, coff_file_t* coff_file);

/**
 * Parses the import table of a COFF file
 * \param filedesc The file descriptor, it's position will be restored at the end of the parsing
 * \param coff_file The COFF structure representing the binary file
 */
extern void coff_file_parse_imports(int filedesc, coff_file_t* coff_file);

/**
 * Loads a coff_file_t structure with the result of the parsing of a PE executable file.
 * It assumes the executable file is opened.
 * \param filedesc A file descriptor
 * \param coff_file The COFF structure representing the binary file
 * \return Error code if the file could not be successfully parsed as an PE file, EXIT_SUCCESS otherwise. In this
 * case, the structure representing the binary file will have been updated with the result of the parsing
 */
extern int coff_file_load(int filedesc, coff_file_t* coff_file);

///////////////////////////////////////////////////////////////////////////////
//                                  Sections                                 //
///////////////////////////////////////////////////////////////////////////////

struct section_s {
   section_header_t* header;
   binscn_t* binscn;
};

/**
 * Gets the virtual address of a COFF section
 * \param section The section to get the virtual address of
 * \return The virtual address of the section.
 */
extern uint32_t section_get_virtual_address(section_t* section);

/**
 * Gets the virtual size of a COFF section
 * \param section The section to get the virtual size of
 * \return The virtual size of the section.
 */
extern uint32_t section_get_virtual_size(section_t* section);

/**
 * Gets the offset (in the file) of a COFF section
 * \param section The section to get the offset of
 * \return The offset of the section
 */
extern uint32_t section_get_offset(section_t* section);

///////////////////////////////////////////////////////////////////////////////
//                                    DOS                                    //
///////////////////////////////////////////////////////////////////////////////

/**
 * Loads the DOS header of a PE executable file
 * This header being mandatory, if we cannot match the magic number (signature) we abort the parsing
 * \param filedesc The file descriptor corresponding to the PE file
 * \param coff_file The COFF structure representing the binary file
 * \return Error code if the file could not be successfully parsed as a PE file, EXIT_SUCCESS otherwise. In this
 * case, the structure representing the binary file will have been updated with the result of the parsing
 */
extern uint32_t dos_load(int filedesc, coff_file_t* coff_file);

///////////////////////////////////////////////////////////////////////////////
//                                 Dbgfiles                                  //
///////////////////////////////////////////////////////////////////////////////

extern dbg_file_t* coff_file_parsedbg(binfile_t* bf);

///////////////////////////////////////////////////////////////////////////////
//                                  Asmfile                                  //
///////////////////////////////////////////////////////////////////////////////

extern int coff_file_asmfile_addlabels(asmfile_t* asmf);

extern void coff_file_asmfile_print_binfile(asmfile_t* asmf);

#endif /* LIBSTONE_H__ */
