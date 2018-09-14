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

#ifndef __PESTRUCT_H__ // Winnt.h
#define __PESTRUCT_H__

#include <stdint.h>

typedef uint32_t DWORD;
typedef int BOOL;
typedef unsigned char BYTE;
typedef uint16_t WORD;
typedef float FLOAT;
typedef uint32_t LONG;
typedef short SHORT;
typedef int BOOLEAN;
typedef uint64_t ULONGLONG;
typedef char CHAR;
//typedef wchar_t             WCHAR;

#define IMAGE_SIZEOF_FILE_HEADER             20

#define IMAGE_FILE_RELOCS_STRIPPED           0x0001  // Relocation info stripped from file.
#define IMAGE_FILE_EXECUTABLE_IMAGE          0x0002  // File is executable  (i.e. no unresolved externel references).
#define IMAGE_FILE_LINE_NUMS_STRIPPED        0x0004  // Line nunbers stripped from file.
#define IMAGE_FILE_LOCAL_SYMS_STRIPPED       0x0008  // Local symbols stripped from file.
#define IMAGE_FILE_AGGRESIVE_WS_TRIM         0x0010  // Agressively trim working set
#define IMAGE_FILE_LARGE_ADDRESS_AWARE       0x0020  // App can handle >2gb addresses
#define IMAGE_FILE_BYTES_REVERSED_LO         0x0080  // Bytes of machine word are reversed.
#define IMAGE_FILE_32BIT_MACHINE             0x0100  // 32 bit word machine.
#define IMAGE_FILE_DEBUG_STRIPPED            0x0200  // Debugging info stripped from file in .DBG file
#define IMAGE_FILE_REMOVABLE_RUN_FROM_SWAP   0x0400  // If Image is on removable media, copy and run from the swap file.
#define IMAGE_FILE_NET_RUN_FROM_SWAP         0x0800  // If Image is on Net, copy and run from the swap file.
#define IMAGE_FILE_SYSTEM                    0x1000  // System File.
#define IMAGE_FILE_DLL                       0x2000  // File is a DLL.
#define IMAGE_FILE_UP_SYSTEM_ONLY            0x4000  // File should only be run on a UP machine
#define IMAGE_FILE_BYTES_REVERSED_HI         0x8000  // Bytes of machine word are reversed.

#define IMAGE_FILE_MACHINE_UNKNOWN           0
#define IMAGE_FILE_MACHINE_I386              0x014c  // Intel 386.
#define IMAGE_FILE_MACHINE_R3000             0x0162  // MIPS little-endian, 0x160 big-endian
#define IMAGE_FILE_MACHINE_R4000             0x0166  // MIPS little-endian
#define IMAGE_FILE_MACHINE_R10000            0x0168  // MIPS little-endian
#define IMAGE_FILE_MACHINE_WCEMIPSV2         0x0169  // MIPS little-endian WCE v2
#define IMAGE_FILE_MACHINE_ALPHA             0x0184  // Alpha_AXP
#define IMAGE_FILE_MACHINE_SH3               0x01a2  // SH3 little-endian
#define IMAGE_FILE_MACHINE_SH3DSP            0x01a3
#define IMAGE_FILE_MACHINE_SH3E              0x01a4  // SH3E little-endian
#define IMAGE_FILE_MACHINE_SH4               0x01a6  // SH4 little-endian
#define IMAGE_FILE_MACHINE_SH5               0x01a8  // SH5
#define IMAGE_FILE_MACHINE_ARM               0x01c0  // ARM Little-Endian
#define IMAGE_FILE_MACHINE_THUMB             0x01c2  // ARM Thumb/Thumb-2 Little-Endian
#define IMAGE_FILE_MACHINE_ARMNT             0x01c4  // ARM Thumb-2 Little-Endian
#define IMAGE_FILE_MACHINE_ARM64             0xaa64  // ARMv8 in 64-bit mode
#define IMAGE_FILE_MACHINE_AM33              0x01d3
#define IMAGE_FILE_MACHINE_POWERPC           0x01F0  // IBM PowerPC Little-Endian
#define IMAGE_FILE_MACHINE_POWERPCFP         0x01f1
#define IMAGE_FILE_MACHINE_IA64              0x0200  // Intel 64
#define IMAGE_FILE_MACHINE_MIPS16            0x0266  // MIPS
#define IMAGE_FILE_MACHINE_ALPHA64           0x0284  // ALPHA64
#define IMAGE_FILE_MACHINE_MIPSFPU           0x0366  // MIPS
#define IMAGE_FILE_MACHINE_MIPSFPU16         0x0466  // MIPS
#define IMAGE_FILE_MACHINE_AXP64             IMAGE_FILE_MACHINE_ALPHA64
#define IMAGE_FILE_MACHINE_TRICORE           0x0520  // Infineon
#define IMAGE_FILE_MACHINE_CEF               0x0CEF
#define IMAGE_FILE_MACHINE_EBC               0x0EBC  // EFI Byte Code
#define IMAGE_FILE_MACHINE_AMD64             0x8664  // AMD64 (K8)
#define IMAGE_FILE_MACHINE_M32R              0x9041  // M32R little-endian
#define IMAGE_FILE_MACHINE_CEE               0xC0EE

// Directory Entries

#define IMAGE_DIRECTORY_ENTRY_EXPORT          0   // Export Directory
#define IMAGE_DIRECTORY_ENTRY_IMPORT          1   // Import Directory
#define IMAGE_DIRECTORY_ENTRY_RESOURCE        2   // Resource Directory
#define IMAGE_DIRECTORY_ENTRY_EXCEPTION       3   // Exception Directory
#define IMAGE_DIRECTORY_ENTRY_SECURITY        4   // Security Directory
#define IMAGE_DIRECTORY_ENTRY_BASERELOC       5   // Base Relocation Table
#define IMAGE_DIRECTORY_ENTRY_DEBUG           6   // Debug Directory
//      IMAGE_DIRECTORY_ENTRY_COPYRIGHT       7   // (X86 usage)
#define IMAGE_DIRECTORY_ENTRY_ARCHITECTURE    7   // Architecture Specific Data
#define IMAGE_DIRECTORY_ENTRY_GLOBALPTR       8   // RVA of GP
#define IMAGE_DIRECTORY_ENTRY_TLS             9   // TLS Directory
#define IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG    10   // Load Configuration Directory
#define IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT   11   // Bound Import Directory in headers
#define IMAGE_DIRECTORY_ENTRY_IAT            12   // Import Address Table
#define IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT   13   // Delay Load Import Descriptors
#define IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR 14   // COM Runtime descriptor //CLR Runtime Header
#define IMAGE_DIRECTORY_ENTRY_RESERVED 15   // Reserved, must be zero

//
// Section values.
//
// Symbols have a section number of the section in which they are
// defined. Otherwise, section numbers have the following meanings:
//

#define IMAGE_SYM_UNDEFINED           (SHORT)0          // Symbol is undefined or is common.
#define IMAGE_SYM_ABSOLUTE            (SHORT)-1         // Symbol is an absolute value.
#define IMAGE_SYM_DEBUG               (SHORT)-2         // Symbol is a special debug item.
#define IMAGE_SYM_SECTION_MAX         0xFEFF            // Values 0xFF00-0xFFFF are special
#define IMAGE_SYM_SECTION_MAX_EX      MAXLONG

//
// Type (fundamental) values.
//

#define IMAGE_SYM_TYPE_NULL                 0x0000  // no type.
#define IMAGE_SYM_TYPE_VOID                 0x0001  //
#define IMAGE_SYM_TYPE_CHAR                 0x0002  // type character.
#define IMAGE_SYM_TYPE_SHORT                0x0003  // type short integer.
#define IMAGE_SYM_TYPE_INT                  0x0004  //
#define IMAGE_SYM_TYPE_LONG                 0x0005  //
#define IMAGE_SYM_TYPE_FLOAT                0x0006  //
#define IMAGE_SYM_TYPE_DOUBLE               0x0007  //
#define IMAGE_SYM_TYPE_STRUCT               0x0008  //
#define IMAGE_SYM_TYPE_UNION                0x0009  //
#define IMAGE_SYM_TYPE_ENUM                 0x000A  // enumeration.
#define IMAGE_SYM_TYPE_MOE                  0x000B  // member of enumeration.
#define IMAGE_SYM_TYPE_BYTE                 0x000C  //
#define IMAGE_SYM_TYPE_WORD                 0x000D  //
#define IMAGE_SYM_TYPE_UINT                 0x000E  //
#define IMAGE_SYM_TYPE_DWORD                0x000F  //
#define IMAGE_SYM_TYPE_PCODE                0x8000  //
//
// Type (derived) values.
//

#define IMAGE_SYM_DTYPE_NULL                0       // no derived type.
#define IMAGE_SYM_DTYPE_POINTER             1       // pointer.
#define IMAGE_SYM_DTYPE_FUNCTION            2       // function.
#define IMAGE_SYM_DTYPE_ARRAY               3       // array.

//
// Storage classes.
//
#define IMAGE_SYM_CLASS_END_OF_FUNCTION     (BYTE )-1
#define IMAGE_SYM_CLASS_NULL                0x0000
#define IMAGE_SYM_CLASS_AUTOMATIC           0x0001
#define IMAGE_SYM_CLASS_EXTERNAL            0x0002
#define IMAGE_SYM_CLASS_STATIC              0x0003
#define IMAGE_SYM_CLASS_REGISTER            0x0004
#define IMAGE_SYM_CLASS_EXTERNAL_DEF        0x0005
#define IMAGE_SYM_CLASS_LABEL               0x0006
#define IMAGE_SYM_CLASS_UNDEFINED_LABEL     0x0007
#define IMAGE_SYM_CLASS_MEMBER_OF_STRUCT    0x0008
#define IMAGE_SYM_CLASS_ARGUMENT            0x0009
#define IMAGE_SYM_CLASS_STRUCT_TAG          0x000A
#define IMAGE_SYM_CLASS_MEMBER_OF_UNION     0x000B
#define IMAGE_SYM_CLASS_UNION_TAG           0x000C
#define IMAGE_SYM_CLASS_TYPE_DEFINITION     0x000D
#define IMAGE_SYM_CLASS_UNDEFINED_STATIC    0x000E
#define IMAGE_SYM_CLASS_ENUM_TAG            0x000F
#define IMAGE_SYM_CLASS_MEMBER_OF_ENUM      0x0010
#define IMAGE_SYM_CLASS_REGISTER_PARAM      0x0011
#define IMAGE_SYM_CLASS_BIT_FIELD           0x0012

#define IMAGE_SYM_CLASS_FAR_EXTERNAL        0x0044  //

#define IMAGE_SYM_CLASS_BLOCK               0x0064
#define IMAGE_SYM_CLASS_FUNCTION            0x0065
#define IMAGE_SYM_CLASS_END_OF_STRUCT       0x0066
#define IMAGE_SYM_CLASS_FILE                0x0067
// new
#define IMAGE_SYM_CLASS_SECTION             0x0068
#define IMAGE_SYM_CLASS_WEAK_EXTERNAL       0x0069

#define IMAGE_SYM_CLASS_CLR_TOKEN           0x006B

//
// Based relocation types.
//

#define IMAGE_REL_BASED_ABSOLUTE              0
#define IMAGE_REL_BASED_HIGH                  1
#define IMAGE_REL_BASED_LOW                   2
#define IMAGE_REL_BASED_HIGHLOW               3
#define IMAGE_REL_BASED_HIGHADJ               4
//#define IMAGE_REL_BASED_MACHINE_SPECIFIC_5  5
#define IMAGE_REL_BASED_MIPS_JMPADDR          5
#define IMAGE_REL_BASED_ARM_MOV32A            5
#define IMAGE_REL_BASED_RESERVED              6
//#define IMAGE_REL_BASED_MACHINE_SPECIFIC_7  7
#define IMAGE_REL_BASED_ARM_MOV32T            7
#define IMAGE_REL_BASED_MACHINE_SPECIFIC_8    8
//#define IMAGE_REL_BASED_MACHINE_SPECIFIC_9  9
#define IMAGE_REL_BASED_MIPS_JMPADDR16        9
#define IMAGE_REL_BASED_DIR64                 10

/* These defines describe the meanings of the bits in the Characteristics field */
#define IMAGE_FILE_RELOCS_STRIPPED      0x0001 /* No relocation info */
#define IMAGE_FILE_EXECUTABLE_IMAGE     0x0002
#define IMAGE_FILE_LINE_NUMS_STRIPPED   0x0004
#define IMAGE_FILE_LOCAL_SYMS_STRIPPED  0x0008
#define IMAGE_FILE_AGGRESIVE_WS_TRIM    0x0010
#define IMAGE_FILE_LARGE_ADDRESS_AWARE  0x0020
#define IMAGE_FILE_16BIT_MACHINE        0x0040
#define IMAGE_FILE_BYTES_REVERSED_LO    0x0080
#define IMAGE_FILE_32BIT_MACHINE        0x0100
#define IMAGE_FILE_DEBUG_STRIPPED       0x0200
#define IMAGE_FILE_REMOVABLE_RUN_FROM_SWAP      0x0400
#define IMAGE_FILE_NET_RUN_FROM_SWAP    0x0800
#define IMAGE_FILE_SYSTEM               0x1000
#define IMAGE_FILE_DLL                  0x2000
#define IMAGE_FILE_UP_SYSTEM_ONLY       0x4000
#define IMAGE_FILE_BYTES_REVERSED_HI    0x8000

/* These are the settings of the Machine field. */

#define IMAGE_DOS_SIGNATURE 0x5A4D /* MZ */
#define IMAGE_NT_SIGNATURE 0x00004550 /* PE00 */
#define IMAGE_NUMBEROF_DIRECTORY_ENTRIES 16

/* Max length of the section name */
#define IMAGE_SIZEOF_SHORT_NAME 8
/* These are the value of Magic from Optional header to know if it's wether 32
 * bits or 64 bits application */
#define IMAGE_NT_OPTIONAL_HDR32_MAGIC 0x10B
#define IMAGE_NT_OPTIONAL_HDR64_MAGIC 0x20B

/* PE classes */
#define PECLASSNONE 0   /* Invalid class */
#define PECLASS32 1     /* 32-bit objects */
#define PECLASS64 2     /* 64-bit objects */

#define IMAGE_SIZEOF_SYMBOL                  18

typedef struct _IMAGE_DOS_HEADER {
   uint16_t e_magic; /* 00: MZ Header signature */
   uint16_t e_cblp; /* 02: Bytes on last page of file */
   uint16_t e_cp; /* 04: Pages in file */
   uint16_t e_crlc; /* 06: Relocations */
   uint16_t e_cparhdr; /* 08: Size of header in paragraphs */
   uint16_t e_minalloc; /* 0a: Minimum extra paragraphs needed */
   uint16_t e_maxalloc; /* 0c: Maximum extra paragraphs needed */
   uint16_t e_ss; /* 0e: Initial (relative) SS value */
   uint16_t e_sp; /* 10: Initial SP value */
   uint16_t e_csum; /* 12: Checksum */
   uint16_t e_ip; /* 14: Initial IP value */
   uint16_t e_cs; /* 16: Initial (relative) CS value */
   uint16_t e_lfarlc; /* 18: File address of relocation table */
   uint16_t e_ovno; /* 1a: Overlay number */
   uint16_t e_res[4]; /* 1c: Reserved words */
   uint16_t e_oemid; /* 24: OEM identifier (for e_oeminfo) */
   uint16_t e_oeminfo; /* 26: OEM information; e_oemid specific */
   uint16_t e_res2[10]; /* 28: Reserved words */
   uint32_t e_lfanew; /* 3c: Offset to extended header */
} IMAGE_DOS_HEADER, *PIMAGE_DOS_HEADER;

typedef struct _IMAGE_DATA_DIRECTORY {
   uint32_t VirtualAddress;
   uint32_t Size;
} IMAGE_DATA_DIRECTORY, *PIMAGE_DATA_DIRECTORY;

typedef struct _IMAGE_FILE_HEADER {
   uint16_t Machine;
   uint16_t NumberOfSections;
   uint32_t TimeDateStamp;
   uint32_t PointerToSymbolTable;
   uint32_t NumberOfSymbols;
   uint16_t SizeOfOptionalHeader;
   uint16_t Characteristics;
} IMAGE_FILE_HEADER, *PIMAGE_FILE_HEADER;

typedef struct _IMAGE_OPTIONAL_HEADER32 {
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
   IMAGE_DATA_DIRECTORY DataDirectory[IMAGE_NUMBEROF_DIRECTORY_ENTRIES]; //16
} IMAGE_OPTIONAL_HEADER32, *PIMAGE_OPTIONAL_HEADER32;

typedef struct _IMAGE_OPTIONAL_HEADER64 {
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
   IMAGE_DATA_DIRECTORY DataDirectory[IMAGE_NUMBEROF_DIRECTORY_ENTRIES]; //16
} IMAGE_OPTIONAL_HEADER64, *PIMAGE_OPTIONAL_HEADER64;

typedef struct _IMAGE_NT_HEADERS32 {
   uint32_t Signature;
   IMAGE_FILE_HEADER FileHeader;
   IMAGE_OPTIONAL_HEADER32 OptionalHeader;
} IMAGE_NT_HEADERS32, *PIMAGE_NT_HEADERS32;

typedef struct _IMAGE_NT_HEADERS64 {
   uint32_t Signature;
   IMAGE_FILE_HEADER FileHeader;
   IMAGE_OPTIONAL_HEADER64 OptionalHeader;
} IMAGE_NT_HEADERS64, *PIMAGE_NT_HEADERS64;

#define IMAGE_SIZEOF_SHORT_NAME 8

typedef struct _IMAGE_SECTION_HEADER {
   unsigned char Name[IMAGE_SIZEOF_SHORT_NAME]; //8
   union {
      uint32_t PhysicalAddress;
      uint32_t VirtualSize;
   } Misc;
   uint32_t VirtualAddress;
   uint32_t SizeOfRawData;
   uint32_t PointerToRawData;
   uint32_t PointerToRelocations;
   uint32_t PointerToLinenumbers;
   uint16_t NumberOfRelocations;
   uint16_t NumberOfLinenumbers;
   uint32_t Characteristics;
} IMAGE_SECTION_HEADER, *PIMAGE_SECTION_HEADER;

//*comme c est defini dans winnt.h

//*
typedef struct _IMAGE_SYMBOL {
   union {
      unsigned char ShortName[8];  // 8 bytes

      struct {  // 8 bytes
         uint32_t Short;     // if 0, use LongName
         uint32_t Long;      // offset into string table
      } Name;

      uint32_t LongName[2];    // PBYTE [2] 8 bytes
   } N;
   uint32_t Value;
   unsigned short SectionNumber;
   uint16_t Type;
   unsigned char StorageClass;
   unsigned char NumberOfAuxSymbols;
}__attribute__((packed)) IMAGE_SYMBOL, *PIMAGE_SYMBOL;
//__attribute__((packed)) pour ne pas avoir de padding 
//*/

//
// Thread Local Storage
//

/*
 typedef VOID
 (NTAPI *PIMAGE_TLS_CALLBACK) (
 PVOID DllHandle,
 uint32_t Reason,
 PVOID Reserved
 );
 */

typedef struct _IMAGE_TLS_DIRECTORY32 {
   uint32_t StartAddressOfRawData;
   uint32_t EndAddressOfRawData;
   uint32_t AddressOfIndex;             // PDWORD
   uint32_t AddressOfCallBacks;         // PIMAGE_TLS_CALLBACK *
   uint32_t SizeOfZeroFill;
   uint32_t Characteristics;
} IMAGE_TLS_DIRECTORY32;
typedef IMAGE_TLS_DIRECTORY32 * PIMAGE_TLS_DIRECTORY32;

typedef struct _IMAGE_TLS_DIRECTORY64 {
   uint64_t StartAddressOfRawData;
   uint64_t EndAddressOfRawData;
   uint64_t AddressOfIndex;         // PDWORD
   uint64_t AddressOfCallBacks;     // PIMAGE_TLS_CALLBACK *;
   uint32_t SizeOfZeroFill;
   uint32_t Characteristics;
} IMAGE_TLS_DIRECTORY64;
typedef IMAGE_TLS_DIRECTORY64 * PIMAGE_TLS_DIRECTORY64;

typedef struct _IMAGE_IMPORT_DESCRIPTOR {
   union {
      uint32_t Characteristics;      // 0 for terminating null import descriptor
      uint32_t OriginalFirstThunk; // RVA to original unbound IAT (PIMAGE_THUNK_DATA)
   } DUMMYUNIONNAME;
   uint32_t TimeDateStamp;                  // 0 if not bound,
                                            // -1 if bound, and real date\time stamp
                                            //     in IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT (new BIND)
                                            // O.W. date/time stamp of DLL bound to (Old BIND)

   uint32_t ForwarderChain;                 // -1 if no forwarders
   uint32_t Name;
   uint32_t FirstThunk;   // RVA to IAT (if bound this IAT has actual addresses)
} IMAGE_IMPORT_DESCRIPTOR;

//
// Export Format
//

//j'ai changer le nom de ces variable pour collé avec la doc officielle de microsoft
typedef struct _IMAGE_EXPORT_DIRECTORY {
   uint32_t ExportFlags;           //Characteristics
   uint32_t TimeDateStamp;
   uint16_t MajorVersion;
   uint16_t MinorVersion;
   uint32_t NameRVA;
   uint32_t OrdinalBase;
   uint32_t NumberOfFunctions;      //addressTableEntries
   uint32_t NumberOfNames;          //numberOfNamePointers
   uint32_t AddressOfExportAddressTableRVA;     // RVA from base of image
   uint32_t AddressOfExportNamePointerRVA;         // RVA from base of image
   uint32_t AddressOfExportOrdinalTableRVA;  // RVA from base of image
} IMAGE_EXPORT_DIRECTORY, *PIMAGE_EXPORT_DIRECTORY;

//
// relocations
//
typedef struct _IMAGE_BASE_RELOCATION {
   uint32_t VirtualAddress;
   uint32_t SizeOfBlock;
//  uint32_t    TypeOffset[1];
} IMAGE_BASE_RELOCATION;

typedef struct _IMAGE_RELOCATION {
   union {
      uint32_t VirtualAddress;
      uint32_t RelocCount; // Set to the real count when IMAGE_SCN_LNK_NRELOC_OVFL is set
   } DUMMYUNIONNAME;
   uint32_t SymbolTableIndex;
   uint16_t Type;
} IMAGE_RELOCATION;

//typedef IMAGE_RELOCATION_UNALIGNED *PIMAGE_RELOCATION;

typedef struct IMAGE_AUX_SYMBOL_TOKEN_DEF {
   BYTE bAuxType;                  // IMAGE_AUX_SYMBOL_TYPE
   BYTE bReserved;                 // Must be 0
   DWORD SymbolTableIndex;
   BYTE rgbReserved[12];           // Must be 0
} IMAGE_AUX_SYMBOL_TOKEN_DEF;

typedef union _IMAGE_AUX_SYMBOL {
   struct {
      uint32_t TagIndex;                     // struct, union, or enum tag index

      union {
         struct {
            uint16_t Linenumber;             // declaration line number
            uint16_t Size;                   // size of struct, union, or enum
         } LnSz;
         uint32_t TotalSize;
      } Misc;

      union {
         struct {                            // if ISFCN, tag, or .bb
            uint32_t PointerToLinenumber;
            uint32_t PointerToNextFunction;
         } Function;
         struct {                            // if ISARY, up to 4 dimen.
            uint16_t Dimension[4];
         } Array;
      } FcnAry;

      uint16_t TvIndex;                        // tv index
   } Sym;

   struct {
      unsigned char Name[IMAGE_SIZEOF_SYMBOL];
   } File;

   struct {
      uint32_t Length;                         // section length
      uint16_t NumberOfRelocations;            // number of relocation entries
      uint16_t NumberOfLinenumbers;            // number of line numbers
      uint32_t CheckSum;                       // checksum for communal
      short Number;                         // section number to associate with
      unsigned char Selection;                      // communal selection type
      unsigned char bReserved;
      short HighNumber;                     // high bits of the section number
   } Section;

   IMAGE_AUX_SYMBOL_TOKEN_DEF TokenDef;

   struct {
      uint32_t crc;
      unsigned char rgbReserved[14];
   } CRC;
} IMAGE_AUX_SYMBOL;

//
// Debug Format
//

typedef struct _IMAGE_DEBUG_DIRECTORY {
   uint32_t Characteristics;
   uint32_t TimeDateStamp;
   uint16_t MajorVersion;
   uint16_t MinorVersion;
   uint32_t Type;
   uint32_t SizeOfData;
   uint32_t AddressOfRawData;
   uint32_t PointerToRawData;
} IMAGE_DEBUG_DIRECTORY, *PIMAGE_DEBUG_DIRECTORY;

#define IMAGE_DEBUG_TYPE_UNKNOWN          0
#define IMAGE_DEBUG_TYPE_COFF             1
#define IMAGE_DEBUG_TYPE_CODEVIEW         2
#define IMAGE_DEBUG_TYPE_FPO              3
#define IMAGE_DEBUG_TYPE_MISC             4
#define IMAGE_DEBUG_TYPE_EXCEPTION        5
#define IMAGE_DEBUG_TYPE_FIXUP            6
#define IMAGE_DEBUG_TYPE_OMAP_TO_SRC      7
#define IMAGE_DEBUG_TYPE_OMAP_FROM_SRC    8
#define IMAGE_DEBUG_TYPE_BORLAND          9
#define IMAGE_DEBUG_TYPE_RESERVED10       10
#define IMAGE_DEBUG_TYPE_CLSID            11

typedef struct _IMAGE_COFF_SYMBOLS_HEADER {
   uint32_t NumberOfSymbols;
   uint32_t LvaToFirstSymbol;
   uint32_t NumberOfLinenumbers;
   uint32_t LvaToFirstLinenumber;
   uint32_t RvaToFirstByteOfCode;
   uint32_t RvaToLastByteOfCode;
   uint32_t RvaToFirstByteOfData;
   uint32_t RvaToLastByteOfData;
} IMAGE_COFF_SYMBOLS_HEADER, *PIMAGE_COFF_SYMBOLS_HEADER;

#define FRAME_FPO       0
#define FRAME_TRAP      1
#define FRAME_TSS       2
#define FRAME_NONFPO    3

typedef struct _FPO_DATA {
   uint32_t ulOffStart;             // offset 1st byte of function code
   uint32_t cbProcSize;             // # bytes in function
   uint32_t cdwLocals;              // # bytes in locals/4
   uint16_t cdwParams;              // # bytes in params/4
   uint16_t cbProlog :8;           // # bytes in prolog
   uint16_t cbRegs :3;           // # regs saved
   uint16_t fHasSEH :1;           // TRUE if SEH in func
   uint16_t fUseBP :1;           // TRUE if EBP has been allocated
   uint16_t reserved :1;           // reserved for future use
   uint16_t cbFrame :2;           // frame type
} FPO_DATA, *PFPO_DATA;
#define SIZEOF_RFPO_DATA 16

#define IMAGE_DEBUG_MISC_EXENAME    1

typedef struct _IMAGE_DEBUG_MISC {
   uint32_t DataType;               // type of misc data, see defines
   uint32_t Length;                 // total length of record, rounded to four
   // byte multiple.
   int Unicode;                // TRUE if data is unicode string
   BYTE Reserved[3];
   BYTE Data[1];              // Actual data
} IMAGE_DEBUG_MISC, *PIMAGE_DEBUG_MISC;

//
// Function table extracted from MIPS/ALPHA/IA64 images.  Does not contain
// information needed only for runtime support.  Just those fields for
// each entry needed by a debugger.
//

typedef struct _IMAGE_FUNCTION_ENTRY {
   uint32_t StartingAddress;
   uint32_t EndingAddress;
   uint32_t EndOfPrologue;
} IMAGE_FUNCTION_ENTRY, *PIMAGE_FUNCTION_ENTRY;

typedef struct _IMAGE_FUNCTION_ENTRY64 {
   uint64_t StartingAddress;
   uint64_t EndingAddress;
   union {
      uint64_t EndOfPrologue;
      uint64_t UnwindInfoAddress;
   } DUMMYUNIONNAME;
} IMAGE_FUNCTION_ENTRY64, *PIMAGE_FUNCTION_ENTRY64;

//
// Debugging information can be stripped from an image file and placed
// in a separate .DBG file, whose file name part is the same as the
// image file name part (e.g. symbols for CMD.EXE could be stripped
// and placed in CMD.DBG).  This is indicated by the IMAGE_FILE_DEBUG_STRIPPED
// flag in the Characteristics field of the file header.  The beginning of
// the .DBG file contains the following structure which captures certain
// information from the image file.  This allows a debug to proceed even if
// the original image file is not accessable.  This header is followed by
// zero of more IMAGE_SECTION_HEADER structures, followed by zero or more
// IMAGE_DEBUG_DIRECTORY structures.  The latter structures and those in
// the image file contain file offsets relative to the beginning of the
// .DBG file.
//
// If symbols have been stripped from an image, the IMAGE_DEBUG_MISC structure
// is left in the image file, but not mapped.  This allows a debugger to
// compute the name of the .DBG file, from the name of the image in the
// IMAGE_DEBUG_MISC structure.
//

typedef struct _IMAGE_SEPARATE_DEBUG_HEADER {
   uint16_t Signature;
   uint16_t Flags;
   uint16_t Machine;
   uint16_t Characteristics;
   uint32_t TimeDateStamp;
   uint32_t CheckSum;
   uint32_t ImageBase;
   uint32_t SizeOfImage;
   uint32_t NumberOfSections;
   uint32_t ExportedNamesSize;
   uint32_t DebugDirectorySize;
   uint32_t SectionAlignment;
   uint32_t Reserved[2];
} IMAGE_SEPARATE_DEBUG_HEADER, *PIMAGE_SEPARATE_DEBUG_HEADER;

typedef struct _NON_PAGED_DEBUG_INFO {
   uint16_t Signature;
   uint16_t Flags;
   uint32_t Size;
   uint16_t Machine;
   uint16_t Characteristics;
   uint32_t TimeDateStamp;
   uint32_t CheckSum;
   uint32_t SizeOfImage;
   uint64_t ImageBase;
//DebugDirectorySize
//IMAGE_DEBUG_DIRECTORY
} NON_PAGED_DEBUG_INFO, *PNON_PAGED_DEBUG_INFO;

//
// Resource Format.
//

//
// Resource directory consists of two counts, following by a variable length
// array of directory entries.  The first count is the number of entries at
// beginning of the array that have actual names associated with each entry.
// The entries are in ascending order, case insensitive strings.  The second
// count is the number of entries that immediately follow the named entries.
// This second count identifies the number of entries that have 16-bit integer
// Ids as their name.  These entries are also sorted in ascending order.
//
// This structure allows fast lookup by either name or number, but for any
// given resource entry only one form of lookup is supported, not both.
// This is consistant with the syntax of the .RC file and the .RES file.
//

typedef struct _IMAGE_RESOURCE_DIRECTORY {
   uint32_t Characteristics;
   uint32_t TimeDateStamp;
   uint16_t MajorVersion;
   uint16_t MinorVersion;
   uint16_t NumberOfNamedEntries;
   uint16_t NumberOfIdEntries;
//  IMAGE_RESOURCE_DIRECTORY_ENTRY DirectoryEntries[];
} IMAGE_RESOURCE_DIRECTORY, *PIMAGE_RESOURCE_DIRECTORY;

#define IMAGE_RESOURCE_NAME_IS_STRING        0x80000000
#define IMAGE_RESOURCE_DATA_IS_DIRECTORY     0x80000000
//
// Each directory contains the 32-bit Name of the entry and an offset,
// relative to the beginning of the resource directory of the data associated
// with this directory entry.  If the name of the entry is an actual text
// string instead of an integer Id, then the high order bit of the name field
// is set to one and the low order 31-bits are an offset, relative to the
// beginning of the resource directory of the string, which is of type
// IMAGE_RESOURCE_DIRECTORY_STRING.  Otherwise the high bit is clear and the
// low-order 16-bits are the integer Id that identify this resource directory
// entry. If the directory entry is yet another resource directory (i.e. a
// subdirectory), then the high order bit of the offset field will be
// set to indicate this.  Otherwise the high bit is clear and the offset
// field points to a resource data entry.
//

typedef struct _IMAGE_RESOURCE_DIRECTORY_ENTRY {
   union {
      struct {
         uint32_t NameOffset :31;
         uint32_t NameIsString :1;
      } DUMMYSTRUCTNAME;
      uint32_t Name;
      uint16_t Id;
   } DUMMYUNIONNAME;
   union {
      uint32_t OffsetToData;
      struct {
         uint32_t OffsetToDirectory :31;
         uint32_t DataIsDirectory :1;
      } DUMMYSTRUCTNAME2;
   } DUMMYUNIONNAME2;
} IMAGE_RESOURCE_DIRECTORY_ENTRY, *PIMAGE_RESOURCE_DIRECTORY_ENTRY;

//
// For resource directory entries that have actual string names, the Name
// field of the directory entry points to an object of the following type.
// All of these string objects are stored together after the last resource
// directory entry and before the first resource data object.  This minimizes
// the impact of these variable length objects on the alignment of the fixed
// size directory entry objects.
//

typedef struct _IMAGE_RESOURCE_DIRECTORY_STRING {
   uint16_t Length;
   char NameString[1];
} IMAGE_RESOURCE_DIRECTORY_STRING, *PIMAGE_RESOURCE_DIRECTORY_STRING;

typedef struct _IMAGE_RESOURCE_DIR_STRING_U {
   uint16_t Length;
   char NameString[1];  // WCHAR à l'origine
} IMAGE_RESOURCE_DIR_STRING_U, *PIMAGE_RESOURCE_DIR_STRING_U;

//
// Each resource data entry describes a leaf node in the resource directory
// tree.  It contains an offset, relative to the beginning of the resource
// directory of the data for the resource, a size field that gives the number
// of bytes of data at that offset, a CodePage that should be used when
// decoding code point values within the resource data.  Typically for new
// applications the code page would be the unicode code page.
//

typedef struct _IMAGE_RESOURCE_DATA_ENTRY {
   uint32_t OffsetToData;
   uint32_t Size;
   uint32_t CodePage;
   uint32_t Reserved;
} IMAGE_RESOURCE_DATA_ENTRY, *PIMAGE_RESOURCE_DATA_ENTRY;

//
// Load Configuration Directory Entry
//

typedef struct {
   uint32_t Size;
   uint32_t TimeDateStamp;
   uint16_t MajorVersion;
   uint16_t MinorVersion;
   uint32_t GlobalFlagsClear;
   uint32_t GlobalFlagsSet;
   uint32_t CriticalSectionDefaultTimeout;
   uint32_t DeCommitFreeBlockThreshold;
   uint32_t DeCommitTotalFreeThreshold;
   uint32_t LockPrefixTable;            // VA
   uint32_t MaximumAllocationSize;
   uint32_t VirtualMemoryThreshold;
   uint32_t ProcessHeapFlags;
   uint32_t ProcessAffinityMask;
   uint16_t CSDVersion;
   uint16_t Reserved1;
   uint32_t EditList;                   // VA
   uint32_t SecurityCookie;             // VA
   uint32_t SEHandlerTable;             // VA
   uint32_t SEHandlerCount;
} IMAGE_LOAD_CONFIG_DIRECTORY32, *PIMAGE_LOAD_CONFIG_DIRECTORY32;

typedef struct {
   uint32_t Size;
   uint32_t TimeDateStamp;
   uint16_t MajorVersion;
   uint16_t MinorVersion;
   uint32_t GlobalFlagsClear;
   uint32_t GlobalFlagsSet;
   uint32_t CriticalSectionDefaultTimeout;
   uint64_t DeCommitFreeBlockThreshold;
   uint64_t DeCommitTotalFreeThreshold;
   uint64_t LockPrefixTable;         // VA
   uint64_t MaximumAllocationSize;
   uint64_t VirtualMemoryThreshold;
   uint64_t ProcessAffinityMask;
   uint32_t ProcessHeapFlags;
   uint16_t CSDVersion;
   uint16_t Reserved1;
   uint64_t EditList;                // VA
   uint64_t SecurityCookie;          // VA
   uint64_t SEHandlerTable;          // VA
   uint64_t SEHandlerCount;
} IMAGE_LOAD_CONFIG_DIRECTORY64, *PIMAGE_LOAD_CONFIG_DIRECTORY64;

//#ifdef _WIN64
//typedef IMAGE_LOAD_CONFIG_DIRECTORY64     IMAGE_LOAD_CONFIG_DIRECTORY;
//typedef PIMAGE_LOAD_CONFIG_DIRECTORY64    PIMAGE_LOAD_CONFIG_DIRECTORY;
//#else
//typedef IMAGE_LOAD_CONFIG_DIRECTORY32     IMAGE_LOAD_CONFIG_DIRECTORY;
//typedef PIMAGE_LOAD_CONFIG_DIRECTORY32    PIMAGE_LOAD_CONFIG_DIRECTORY;
//#endif

#endif /* __PESTRUCT_H__ */
