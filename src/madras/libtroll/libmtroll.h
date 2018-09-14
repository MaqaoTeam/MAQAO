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
 \file libmtroll.h
 \brief Libmtroll provides several basic data structures and functions to manage ELF files.
 \todo This page needs to be refactored
 \page page4 Libmtroll documentation page

 \section intro Introduction
 Libmtroll is a library handling ELF file support. ELF stands for Executable and Linking Format.
 It is the binary format used for executable files on UNIX based OS. 
 
 In the current version, libmtroll used the libelf library to load / change the ELF structure. It 
 implements its own data structures to overload libelf structures.
 
 \section parself ELF parsing
 \subsection functions Main functions
 - \ref parse_elf_file
 \subsection description Description
 The parsing of an ELF file uses the functions defined in libelf and gelf.
 It retrieves an Elf object from the file, which is an opaque structure, from which
 can be extracted structures (Elf_xxx and GElf_xxx) whose contents can be accessed.
 The contents of some of these structures are used to fill custom defined objects
 for easiness of access.
 An ELF file always contains an ELF header, which specifies the type of architecture,
 the number of sections, and the offsets at which the various headers can be found.\n
 These headers are:
 - the section header. This header describes sections. Sections represent different kind of structures 
 inside the binary, such as the .text section which contain the binary assembly code, sections containing
 debug data generated using the -g option during the compilation, label tables ... .
 - the program header. This header describes segments. Segments are used by the loader to 
 now how to load the binary in memory. Basically, a segment contains one ore several sections. During the loading, 
 these sections are mapped in memory at specified addresses, and using specified permissions.

 The full ELF specifications can be found 	<a href="http://www.google.fr/url?sa=t&rct=j&q=elf%20format&source=web&cd=3&sqi=2&ved=0CEAQFjAC&url=http%3A%2F%2Fflint.cs.yale.edu%2Fcs422%2Fdoc%2FELF_Format.pdf&ei=gvZNT7fgA9CLhQfRs5H4Dw&usg=AFQjCNHRIY6kwZ3mHqiQySu1QBLGIcG7Qg&sig2=DK-CTEThpJvxe65eXGGNSA&cad=rja"> here </a>.


 \section patchelf ELF patching
 
 During the ELF patching, some sections are modified. The .text section is obviously modified, because it 
 contains the program assembly code, but other sections can be modified (especially if fonctions are added) 
 or created (if assembly code is added). 
 
 Those ELF structures (relocation table, which contains the data used to call an external function,
 * 	dynamic table, which contains the names of the needed external libraries, and procedure label et global offset
 * 	tables, which are used to retrieve the pointers to functions located in an external libraries) can not be
 * 	directly increased in size without risking to cause a shift of the virtual addresses in the file.
 * 	Another problem is that the virtual addresses of the segments in an ELF file must be congruent to their offsets
 * 	in the file by a certain modulo.

 The solution implemented is the following:
 - Creates a new space at the beginning of the file, which is as large as the biggest between the highest modulo
 value and the size of the modified data (in x86_64, the modulo between offset and addresses is 200 000 bytes for
 executable code, so this is usually the highest between the two).
 - Adds in this space the inserted code. The beginning of this new space has a virtual address equal to the original
 first address of the executable, minus the size of the new space. The congruencies are then obeyed.
 - Move in this space all other ELF sections whose size has changed
 - Creates empty filler sections to keep a correct space between the section, in particular to replace the sections
 which have been moved
 - Updates all internal pointers accordingly

 \subsection functions Main functions
 - \ref elfexe_scnreorder. During the patch process, some sections become bigger. These sections must be moved because unmodified
 sections can not been moved (indirect references are not detected, so if the target is moved, the source can not been updated).
 This function computes the new section order. Two strategies can be used, depending of some properties of the binary:
 - patch after: it is the default strategy. All moved sections are located at the end of the last loaded segment. 
 This strategy allows to add a lot of code / data because there is no size limitation. It also allows to patch dynamic libraries.
 - patch before: some times, it is not possible to patch at the end of the binary (for example, too large BSS section).
 In these cases, code is added before the code. This strategy can not be done if the code to add is to big (the available size 
 is limited) or if it is a library (no empty space before the first section). When the added code is located at the begining, 
 the first loaded segment is increased.
 - \ref copy_elf_file_reorder. When the new section order is computed, the patched binary must be re-written. This function
 uses the reordering table to generate the patched binary.

 

 */

#ifndef LIBMTROLL_H__
#define LIBMTROLL_H__

#include <stdint.h>
#include <elf.h>

#include "ar.h"
#include "libmcommon.h"
#include "libmasm.h"
#include "libelf.h"
#include "DwarfLight.h"

/** AR header defined in ar.h file*/
typedef struct ar_hdr ar_hdr_t;

/**Structure corresponding to an entry in a table of the ELF file*/
typedef struct tableentry_s tableentry_t;

/**Structure corresponding to a section in an ELF file*/
typedef struct elfsection_s elfsection_t;

/**Structure corresponding to an ELF file*/
typedef struct elffile_s elffile_t;

/** Alias for struct targetaddr_s structure*/
typedef struct targetaddr_s targetaddr_t;

/** Special definition for use by the function \ref elffile_label_isfunc to identify a label that is definitely not a function*/
#define LABEL_ISNOFCT -2

/** Value signaling an erroneous value for a machine code (should not intersect with valid EM_*** values)*/
#define ELF_MACHINE_CODE_ERR -1

/**
 * Size of ar_hdr_t members
 */
#define SIZE_AR_NAME     16      /**<Size of ar_name member*/
#define SIZE_AR_DATE     12      /**<Size of ar_date member*/
#define SIZE_AR_UID      6       /**<Size of ar_uid member*/
#define SIZE_AR_GID      6       /**<Size of ar_gid member*/
#define SIZE_AR_MODE     8       /**<Size of ar_mode member*/
#define SIZE_AR_SIZE     10      /**<Size of ar_size member*/
#define SIZE_AR_FMAG     2       /**<Size of ar_fmag member*/

#define O_FILES_NUMBER   200

/**
 * Suffix of external labels
 * */
#define EXT_LBL_SUF "@plt"

///**
// * \struct ar_s
// * \brief Represents a .a file
// */
//struct ar_s{
//   char* fileName;            /**<Name of the .a file*/
//   ar_hdr_t* globalHeader;    /**<Header of the .a file*/
//   char* fcts;                /**<Array of function names*/
//   unsigned nbObjectFiles;    /**<Number of ELF file in the .a file*/
//   ar_hdr_t* headerFiles;     /**<Array of headers (one per ELF file)*/
//   long* ELFpositionInFile;   /**<offset of ELF files in the .a file (one per ELF file (object file))*/
//};
/**
 * \enum indexes
 * Positions in the table of section indexes for the various section types
 * */
enum indexes {
   RELO_IDX = 0, /**< Index of a section holding the relocation table
    (kept for compatibility with some functions used for relocatable files)*/
   RELA_IDX, /**< Index of a section holding the relocation table
    (kept for compatibility with some functions used for relocatable files)*/
   SYMTAB_IDX, /**< Index of the section holding the symbol table (-1 if no symbol table is present)*/
   STRTAB_IDX, /**< Index of the string section associated to the symbol table (-1 if no string symbol table is present)*/
   DYNSYM_IDX, /**< Index of the section holding the dynamic symbol table (-1 if no dynamic symbol table is present)*/
   DYNSTR_IDX, /**< Index of the section holding the dynamic string table (-1 if no dynamic string table is present)*/
   DYNAMIC_IDX, /**< Index of the section holding the dynamic table (-1 if no dynamic table is present)*/
   PLT_IDX, /**< Index of the section holding the procedure linkage table (-1 if no PLT_IDX section is present)*/
   GOTPLT_IDX, /**< Index of the section holding the global offset table linked to the procedure linkage table (-1 if no such section is present)*/
   GOT_IDX, /**<Index of the section holding the global offset table*/
   BSS_IDX, /**<Index of the section holding the Block Started by Symbol (-1 if not)*/
   TBSS_IDX, /**<Index of the section holding the Block Started by Symbol for Thread Local Storage (-1 if not)*/
   TDATA_IDX, /**<Index of the section holding the data for Thread Local Storage (-1 if not)*/
   RELAPLT_IDX, /**< Index of the section holding the relocation entries associated with the procedure linkage table (-1 if no such section is present)*/
   VERSYM_IDX, /**< Index of the section holding version information*/
   SHSTRTAB_IDX, /**< Index of the section containing strings for section names*/
   MADRASPLT_IDX, MADRASTEXT_IDX, MADRASDATA_IDX, MAX_NIDX /**< Number of different section indexes stored in the structure (must always be last)*/
};

/**
 * \enum ELF_kind
 * Type of supported binary formats
 */
typedef enum {
   ELFkind_NONE, /**<Unknown*/
   ELFkind_AR, /**<Archive*/
   ELFkind_ELF, /**<ELF file*/
   ELFkind_NUM /**<Number of elements (must be the last)*/
} ELF_kind;

/**These indexes will be used in the target hashtable, for referencing new sections that have not yet been created.
 * Since the number of sections is stored on an uint16, no file should contain a section with such an ID*/
/**\todo This may change when the patcher/ELF parser are refactored and not be useful*/
#define NEWCODESCNID 	0x10001	/**<Id of a new code section before it is created*/
#define NEWDATASCNID	   0x10002	/**<Id of a new data section before it is created*/
#define NEWPLTSCNID 	   0x10003	/**<Id of a new plt section before it is created*/
#define NEWTDATASCNID   0x10004
#define NEWTBSSSCNID    0x10005

#if 0
typedef struct tableentry_32_s {
   int type; /**< Type of the entry (SYMTABLE, RELATABLE, etc)*/
   union {
      Elf32_Sym *sym; /**< Symbolic table entry */
      Elf32_Rela *rela; /**< Relocation table entry */
      Elf32_Rel *rel; /**< Relocation table entry */
      Elf32_Dyn *dyn; /**< Dynamic table entry */
   }elfentry; /**< Contains the actual ELF entry object */
   Elf64_Addr addr; /**< Address pointed to by the entry*/
   char* name; /**< Name of the entry*/
   insn_t* instr; /**< Instruction pointed to by the symbol table*/
   int offset; /**< Offset in the instruction coding (in bits) to which the entry points*/
   char updated; /**< Flag notifying whether the entry has been updated or is the same one as retrieved from the ELF*/
}tableentry_32_t;
typedef struct tableentry_64_s {
   int type; /**< Type of the entry (SYMTABLE, RELATABLE, etc)*/
   union {
      Elf64_Sym *sym; /**< Symbolic table entry */
      Elf64_Rela *rela; /**< Relocation table entry */
      Elf64_Rel *rel; /**< Relocation table entry */
      Elf64_Dyn *dyn; /**< Dynamic table entry */
   } elfentry; /**< Contains the actual ELF entry object */
   int scnidx; /**< Contains the index of the section the entry belongs to */
   Elf64_Addr addr; /**< Address pointed to by the entry*/
   char* name; /**< Name of the entry*/
   insn_t* instr; /**< Instruction pointed to by the symbol table*/
   int offset; /**< Offset in the instruction coding (in bits) to which the entry points*/
   char updated; /**< Flag notifying whether the entry has been updated or is the same one as retrieved from the ELF*/
}tableentry_64_t;

/**
 * \struct tableentry_s
 * \brief This structure stores an entry in a ELF table (symbol, relocation or dynamic)
 * */
struct tableentry_s {
   tableentry_32_t* entry_32;
   tableentry_64_t* entry_64;
};

typedef struct Elf_Ehdr_s {
   Elf64_Ehdr *ehdr64;
   Elf32_Ehdr *ehdr32;
}Elf_Ehdr;

typedef struct Elf_Shdr_s {
   Elf64_Shdr *scnhdr64;
   Elf32_Shdr *scnhdr32;
}Elf_Shdr;

typedef struct Elf_Phdr_s {
   Elf64_Phdr *phdr64;
   Elf32_Phdr *phdr32;
}Elf_Phdr;

/**
 * \struct elfsection_s
 * \brief This structure stores a parsed ELF
 * */
struct elfsection_s {
   Elf_Shdr* scnhdr;
//	tableentry_t **table_entries; /**< Array of entries (can be NULL if the section does not contain a table)*/
//	hashtable_t* hentries;        /**< Hashtable containing the structure's entries, indexed on the addresses they point to*/
//	int nentries;                 /**< Number of entries in the table_entries array*/
//	unsigned char* bits;          /**< Binary code of the section*/
};
#endif

#define ELFFILE_NONE       0x00000000  /**<Default flag associated to an elffile_t structure*/
#define ELFFILE_PATCHCOPY  0x00000001  /**<Indicates that the file has been copied from another during a patching operation*/

/**
 * \struct elffile_s
 * \brief This structure stores a parsed ELF file
 * \note An undefined index might be stored as 0 instead of -1, as the section at index 0
 *  is reserved and always empty */
struct elffile_s {
//	FILE* filedes;             /**< File descriptor from which the ELF file was parsed*/
//	DwarfAPI* dwarf;           /**< Structure used to store debug data*/
//	char* filename;            /**< Name of the file*/
   Elf* elf; /**< ELF structure containing the parsed file*/
//	Elf_Ehdr *elfheader;       /**< ELF header of the file*/
//	Elf_Phdr **progheader;     /**< Program header table of the file (can be NULL)*/
//	elfsection_t** sections;   /**< Array containing the sections in the file*/
//	int nsections;             /**< Number of sections in the file*/
   uint32_t flags; /**< Flags for specifying additional characteristics to the file*/
   short indexes[MAX_NIDX]; /**< Array containing the indexes of various often-needed sections*/
//	hashtable_t* labels;       /**< Table of the tableentry_t structures for all labels inside the file, indexed by their name*/
//	hashtable_t* targets;      /**< Table of all address links inside the file, indexed by the object pointing to an address*/
//   ar_t* archive;             /**< If not null, contains the archive the file belongs to*/

//	int64_t address_newseg;		/**< Address used to save where the new segment must begin*/
//	int64_t size_newseg;	      /**< Size of the new segment*/
//	int64_t offset_newseg;     /**< Offset of the new segment*/
//	int64_t address_newseg_TLS;/**< Address used to save where the new TLS_madras segment must begin*/
//	int64_t size_newseg_TLS;	/**< Size of the new TLS_madras segment*/
//	int64_t offset_newseg_TLS; /**< Offset of the new TLS_madras segment*/

   hashtable_t* symnames; /**< Hashtable linking data entries representing symbols to a pointer to the entries representing their name
    (index is a data entry representing the symbol, value is a pointer structure referencing the data entry representing the name).*/
   /**\todo TODO (2014-10-02) The symnames hashtable may be needed for the whole file after all. To be settled when I'm there
    * => (2014-10-10) I'm now using it for the whole file*/
   int last_error_code; /**<Code of the last error encountered*/
   hashtable_t* scnnames; /**<Hashtable linking sections to the entries representing their name in a string table
    (index is a pointer to the section, value is a pointer structure referencing the data entry representing the name)*/
   uint16_t* oldscnid; /**<Array of new section indexes, indexed by the old index. Used only in patched files*/
};

/**Structure address updater function prototype*/
typedef void (*addrupdfunc_t)(void* elfstr, int64_t addr, void* ef, int scnidx,
      int tblidx);
#if 0
/**
 * \struct targetaddr_s
 * \brief Holds a link to an offset in a section, and the function used to update the structure
 * pointing to this address. The nodes data in the elffile_t:targets hashtable are pointers
 * to such structures
 * This structure is used when a section's starting address is changed and when we need to
 * update a reference to an address in this section
 * */
struct targetaddr_s {
   int srcscnidx; /**< Index of the section containing the object
    pointing to this address*/
   int srctblidx; /**< Index of the object pointing to this address
    in the table containing it (if it belongs to a table)*/
   int scnindx; /**< Index of the section containing the target*/
   int64_t offs; /**< Offset from the beginning of the section of the target*/
   addrupdfunc_t updaddr; /**< Function to use to update the source pointing to this address*/
};
#endif
/**
 * Loads a .a file and parses it
 * \param file a file to load
 * \param filename name of the archive to parse (used if file is NULL)
 * \return a loaded ar_t structure or NULL if there is a problem
 */
extern ar_t* AR_load(FILE* file, char* filename);

/**
 * Frees an ar_t structure
 * \param ar a structure to free
 */
extern void AR_free(ar_t* ar);

///**
// * Returns the number of sections in an ELF file
// * \param efile The structure holding the parsed ELF file
// * \return Number of sections or -1 if \e efile is NULL
// */
//extern int elffile_getnscn(elffile_t* efile);

///**
// * Checks if an ELF section contains prog bits
// * \param efile Structure holding the parsed ELF file
// * \param scnidx The identifier of the section to check
// * \return TRUE if the section contains prog bits, FALSE otherwise
// */
//extern int elffile_scnisprog(elffile_t* efile,int scnidx);
#if 0
/**
 * Holds the details about a relocation. This will be used to apply the relocation
 * \todo The members of this structure have not yet been fixed, some may not be necessary
 * */
typedef struct elfreloc_s {
   elffile_t* efile; /**< Pointer to the file containing the section where the relocation must be performed*/
//	int scnid;					/**< Index of the section in \e efile where the relocation must be performed*/
   elfsection_t* scn; /**< Section in \e efile where the relocation must be performed*/
   int64_t offset; /**< Offset in the section where the relocation must be applied*/
//	elffile_t* relofile;		/**< Pointer to the file containing the section containing the relocation target*/
//	int reloscnid;				/**< Index of the section in \e relofile containing the relocation target*/
   elfsection_t* reloscn; /**< Section in \e relofile containing the relocation target*/
   int64_t relooffset; /**< Offset in the section containing the relocation target of the relocation target*/
   int64_t reloaddend; /**< Addend of the relocation (if existing)*/
   int reloalign; /**< Alignment of the relocation*/
   int relosize; /**< Size of the relocation data*/
   int relotype; /**< Type of the relocation (ELF code)*/
   int symbolIsExtracted; /**< To know if a symbol is already defined in the original file (0) or have been extracted from a object file (1)*/
   tableentry_t* symentry; /**< Entry containing the symbol associated to the relocation*/
   int reloGotIdx; /**< Index in the .GOT section where the address is stored (for COM and TLS  variables)*/
   int reloPltIdx; /**< Index in the .PLT section where the address is stored (for IFUNC func)*/
   uint64_t immediateValue; /**< Store the offset of the variable in the TLS Segment (for TPOFF32 relocations)*/
   int reloPltOffset; /**< Offset in the .PLT section where the address is stored : GOTPLT[offset] (for IFUNC func)*/
   short idxPlt; /**< Index of the PLT section in the file. Could be .madras.plt OR .plt (the original plt)*/
}elfreloc_t;
#endif

///**
// * Returns the ELF machine code
// * \param efile an ELF structure
// * \return a code defined in elf.h or -1 if \e efile is NULL or not parsed
// */
//extern int elffile_getmachine(elffile_t* efile);
///**
// * Adds a target address link to an elf file
// * \param efile The parsed ELF file
// * \param elfobj A pointer to an object referencing an address
// * \param addr The address reference by the object
// * \param updfunc A pointer to the function to be used to update the object (if the address changes)
// * \param srcscnidx The index of the ELF section containing the object (if applicable)
// * \param srctblidx The index in the ELF table where the object is at (if applicable)
// */
//extern void elffile_add_targetaddr(elffile_t* efile,void* elfobj,int64_t addr,addrupdfunc_t updfunc,int srcscnidx,int srctblidx);
///**
// * Checks if a label is of type function
// * \param efile Pointer to the ELF file
// * \param label the label to look for
// * \return 1 if the label is of type function in the ELF file, 0 otherwise. If the label was
// * not found or if no symbol table is present in the file, returns -1.
// */
//extern int elffile_label_isfunc(elffile_t* efile,char* label);
///**
// * Returns the name of a section
// * \param efile Structure containing the parsed ELF file
// * \param scnidx Index of the section
// * \return A pointer to a string in the ELF file containing the name of the section or
// *       NULL if the index was out of range
// */
//extern char* elfsection_name (elffile_t* efile, int scnidx);

///**
// * Returns the bits from the specified section as a stream of unsigned char*
// * \param efile Structure holding the parsed ELF file
// * \param secindx The index of the section
// * \param len Pointer to a variable holding the size in bytes of the char array returned
// * \param startaddr Pointer to an variable holding the address of the starting address of the bytestream
// * \return The bytestream (char array) of the section, or NULL if it is empty
// */
//extern unsigned char* elffile_get_scnbits (elffile_t* efile,int secindx,int* len,int64_t* startaddr);
///**
// * Returns the bits from the specified section as a stream of unsigned char*
// * Returns the bits as Big Endian if the architecture is ARM32.
// * WARNING : temporary patch, should be placed in an architecture specific part.
// * \param efile Structure holding the parsed ELF file
// * \param secindx The index of the section
// * \param len Pointer to a variable holding the size in bytes of the char array returned
// * \param startaddr Pointer to an variable holding the address of the starting address of the bytestream
// * \return The bytestream (char array) of the section, or NULL if it is empty
// */
//extern unsigned char* elffile_get_scncode (elffile_t* efile,int secindx,int* len,int64_t* startaddr);

///**
// * Links an instruction with the tables of a Elf file
// * \param efile Structure holding the parsed ELF file
// * \param inst Instruction to tentatively link
// * \param secindx Index of the section holding the instruction
// */
//extern void elffile_insnlink (elffile_t* efile, insn_t* inst, int secindx);
///**
// * Returns the starting address of an elf section
// * \param efile Structure containing the parsed ELF file
// * \param scnidx Index of the section in the ELF file
// * \return Starting address of the section or -1 if the section is not found
// * (section index out of bounds)
// * */
//extern int64_t elffile_getscnstartaddr(elffile_t *efile, int scnidx);
///**
// * Opens an ELF file and parses its content
// * \param filename The name or path to the ELF file to parse
// * \param filedes The file identifier of the ELF file to parse (used if filename is NULL)
// * \param armbrs Return parameter. If not NULL and if the file is an archive, will point to an array
// *        containing the parsed ELF members. If the file is not an archive, will point to a NULL parameter.
// * \param n_armbrs Return parameter. If not NULL and if the file is an archive, will point to the number
// *        of members, otherwise will point to 0.
// * \param asmf corresponding assembly file
// * \return A pointer to the structure containing the parsed ELF file, or NULL if an error
// *        occurred (file not found, invalid format, unable to open file). If the file is an archive and
// *        \e armbrs is NULL, this will be a pointer to the first member, otherwise, it will be an empty
// *        elffile_t containing only the Elf structure for the whole file
// */
//extern elffile_t* parse_elf_file (char* filename, FILE* filedes, elffile_t*** armbrs, int *n_armbrs, asmfile_t* asmf);
///**
// * Update labels of .plt section using .rela.plt table and .got section
// * \param asmf An asmfile to update
// * \return TRUE if successful, FALSE otherwise
// */
//extern int asmfile_add_plt_labels (asmfile_t* asmf);
///**
// * Returns the name of the file the structure contains the parsing of
// * \param efile Structure containing the parsed ELF file
// * \return The name of the original ELF file or NULL if \e efile is NULL
// */
//extern char* elffile_getfilename(elffile_t* efile);

///**
// * Returns the type of the ELF file (shared, executable, relocatable)
// * \param efile The structure holding the parsed ELF file
// * \return Coding (as used by libelf) corresponding to the type of file or -1 if \e efile is NULL or not parsed
// */
//extern int elffile_gettype(elffile_t* efile);

///**
// * Adds a function symbol to a file at a given address
// * \param efile The structure holding the parsed ELF file
// * \param lbl The symbol to add
// * \param addr The address linked to the symbol
// * \param scnidx The index of the section the symbol entry points to
// * \param dynamic If set to TRUE, the symbol will be added to the dynamic table, otherwise it will be added to the symbol table
// * \return The index to the newly added symbol entry in the dynamic table, or -1 if an error occurred
// */
//extern int elffile_add_funcsymbol(elffile_t* efile, char* lbl, int64_t addr, int scnidx, int dynamic);

///**
// * Adds a "dummy" symbol (type not recognised as function) to a file at a given address
// * \param efile The structure holding the parsed ELF file
// * \param lbl The symbol to add
// * \param addr The address linked to the symbol
// * \param scnidx The index of the section the symbol entry points to
// * \param dynamic If set to TRUE, the symbol will be added to the dynamic table, otherwise it will be added to the symbol table
// * \return The index to the newly added symbol entry in the dynamic table, or -1 if an error occurred
// */
//extern int elffile_add_dummysymbol(elffile_t* efile, char* lbl, int64_t addr, int scnidx, int dynamic);

///**
// * Checks if an ELF file is an executable
// * \param efile The structure containing the parsed ELF file
// * \return TRUE if the file is an executable, FALSE otherwise
// */
//extern int elffile_isexe(elffile_t* efile);

///**
// * Checks if an ELF file is an shared (dynamic) library
// * \param efile The structure containing the parsed ELF file
// * \return TRUE if the file is an shared library, FALSE otherwise
// */
//extern int elffile_isdyn(elffile_t* efile);

///**
// * Checks if an ELF file is a relocatable (object) file
// * \param efile The structure containing the parsed ELF file
// * \return TRUE if the file is a relocatable, FALSE otherwise
// */
//extern int elffile_isobj(elffile_t* efile);

///**
// * Retrieves the ELF section with a given name
// * \param efile The structure holding the parsed ELF file
// * \param name The name of the section to be looked for
// * \return Index of the section in the file or -1 if no section with such name was found
// */
//extern int elffile_get_scnbyname (elffile_t* efile,char *name) ;

///**
// * Returns the size of an elf section
// * \param efile Structure containing the parsed ELF file
// * \param scnidx Index of the section in the ELF file
// * \return Size of the section or -1 if the section is not found
// * (section index out of bounds)
// */
//extern int64_t elffile_getscnsize(elffile_t* efile, int scnidx);

///**
// * Get a list of all dynamic libraries
// * \param efile An ELF file
// * \return A list of strings containing the names of all mandatory dynamic libraries
// */
//extern list_t* elffile_get_dynamic_libs (elffile_t* efile);

/**
 * Gets the kind (an element in the enumeration ELF_kind) corresponding to the given file.
 * \param file a file to analyze
 * \return  an elment in the enumeration ELF_kind
 */
extern ELF_kind ELF_get_kind(FILE* file);

///**
// * Returns the index of the section starting at the given address
// * \param efile Structure containing the parsed ELF file
// * \param addr Address
// * \return Index of the section in the file or -1 if no section found
// */
//extern int elffile_get_scnataddr(elffile_t* efile, int64_t addr);

/**
 * Closes an ELF file associated to an \ref elffile_t structure and releases the structure
 * \param efile Pointer to the structure containing the parsed ELF file
 * \bug This function does not work properly if the ELF file has been used to create a patched file
 * (even if it the changes have not been saved). As of now it does not attempt to release
 * some structures as this can cause a crash.
 */
extern void elffile_free(void* e);

///**
// * Adds a symbol with undefined type to a file at a given address
// * \param efile The structure holding the parsed ELF file
// * \param lbl The symbol to add
// * \param addr The address linked to the symbol
// * \param scnidx The index of the section the symbol entry points to
// * \param dynamic If set to TRUE, the symbol will be added to the dynamic table, otherwise it will be added to the symbol table
// * \return The index to the newly added symbol entry in the dynamic table, or -1 if an error occurred
// */
//extern int elffile_add_undefsymbol(elffile_t* efile, char* lbl, int64_t addr, int scnidx, int dynamic);

///**
// * Creates a new ELF file from an existing one
// * \param efile Parsed ELF file structure which will be copied to the new file
// * \param filename Name of the ELF file to create
// * \param filedes File descriptor of the new file to create
// * \note This function must not be used with a file that has been modified
// */
//extern void create_elf_file(elffile_t* efile, char* filename, int filedes);

///**
// * Update the byte stream in an elffile in the given section
// * \param efile Structure holding the parsed ELF file
// * \param progbits New byte array
// * \param len Size of the byte array
// * \param scnidx Index of the section whose byte stream we want to update
// */
//extern void elffile_upd_progbits (elffile_t* efile, unsigned char* progbits, int len, int scnidx);

///**
// * Returns the label associated to an address (if such a label exists)
// * \param efile The structure holding the parsed ELF file
// * \param addr The address at which we are looking for a label
// * \param secnum The section spanning over the address
// * \return The label or NULL if no label is associated to this address
// */
//extern char* elffile_getlabel (elffile_t* efile, int64_t addr, int secnum);

///**
// * Adds a dynamic library to the elf file
// * \param ef The structure holding the parsed ELF file
// * \param libname The name of the external library to add
// * \param priority 1 if the library should be added at the begining, else 0
// */
//extern void elffile_add_dynlib (elffile_t* ef, char* libname, int priority);

///**
// * Adds a symbol to the symbol table in an ELF file (dynamic or symbol table)
// * \param efile The structure holding the parsed ELF file
// * \param lbl The symbol to add
// * \param addr The address linked to the symbol
// * \param size The size of the symbol entry
// * \param info The auxiliary information of the symbol entry
// * \param scnidx The index of the section the symbol entry points to
// * \param dynamic If set to TRUE, the symbol will be added to the dynamic table, otherwise it will be added to the symbol table
// * \return The index to the newly added symbol entry in the dynamic table, or -1 if an error occurred
// */
//extern int elffile_add_symlbl(elffile_t* efile, char* lbl, int64_t addr, int size, int info, int scnidx, int dynamic);

/**
 * Modify a label in the dynamic symbol table.
 * It will modifies the external function called.
 * \param ef The structure holding the parsed ELF file
 * \param new_lbl The new name of the label
 * \param old_lbl The old name of the label
 * \return EXIT_SUCCESS if the update is successful, error code otherwise
 */
extern int elffile_modify_dinsymlbl(elffile_t* ef, char* new_lbl, char* old_lbl);

///**
// * Returns the index of the last entry in the RELAPLT table
// * \param efile The structure holding the parsed ELF file
// * \return Index of the last entry in the RELAPLT table, or -1 if an error occurred
// */
//extern int elfexe_get_jmprel_lastidx(elffile_t* efile);

///**
// * Returns the index of the .plt section (if present)
// * \param efile The structure holding the parsed ELF file
// * \return Index in the elf file of the section holding the procedure linkage table or -1 if \e efile is NULL
// */
//extern int elffile_getpltidx(elffile_t* efile);

/*
 * Returns the index of the first .tls section
 * \param efile The structure holding the parsed ELF file
 * \return Index of the first .tls section
 * WARNING This is SHIT as the first tls section is not necessarily the first in memory
 */
extern int64_t elffile_gettlsaddress(elffile_t* efile);
///**
// * Returns an array of the indexes of the ELF section containing executable code (type=PROGBITS and executable flag is set)
// * \param efile The structure holding the parsed ELF file
// * \param progscnsz Return parameter. If not NULL, will contain the size of the array
// * \return An allocated array containing the indexes of ELF sections containing executable code
// * */
//extern int* elffile_getprogscns(elffile_t* efile, int* progscnsz);

/*
 * Returns the size of the tls sections
 * \param efile The structure holding the parsed ELF file
 * \return The total size of the tls sections
 */
extern int64_t elffile_gettlssize(elffile_t* efile);

///**
// * Adds a new address to the global offset table linked to the procedure linkage
// * table, and updates the target addresses hash table to include an entry for
// * the pointing object given in parameters to this new address, and an entry for this
// * new address to the given offset in the procedure linkage table section
// * \param efile The structure holding the executable ELF file
// * \param addr The address to add to the global offset table (it's an offset from the beginning of the new plt section)
// * \param pointerobj The pointer to an object pointing to this address
// * \param updater A pointer to the function to use to update \e pointerobj
// * \return The address of the new entry in the global offset table
// */
//extern int64_t elfexe_add_pltgotaddr(elffile_t* efile,int64_t addr,void* pointerobj,addrupdfunc_t updater);

///**
// * Adds a relocation entry to the RELAPLT relocation table (its address is incremented by 8
// * from the last) and updates the size of RELAPLT in the DYNAMIC section
// * \param efile The structure holding the parsed ELF file
// * \param symname The name of the relocation entry
// * \param relidx Pointer which will contain upon return the index of the added relocation in the
// * RELAPLT table
// * \return The address of the added relocation, or -1 if an error occurred
// */
//extern int64_t elfexe_add_jmprel(elffile_t* efile,char* symname,int* relidx);

///**
// * Retrieves the address in an executable ELf file to which a call to an external function
// * must be made (in the procedure label table).
// * \param efile The structure holding the parsed ELF file (no test is made to check whether it is
// * actually an executable)
// * \param label The name of the external function
// * \return The address to call to to invoke the function, or -1 if an error occurred (no dynamic
// * section found, no relocation table for external functions found, label not found, or architecture
// * not implemented)
// * \note This function finds the address by looking for the external function's name in the
// * appropriate relocation table, then looking into the procedure label table at the index at
// * which it was found in the relocation table, multiplied by an architecture specific step. This
// * seems to be how objdump actually proceeds.
// * */
//extern int64_t elfexe_get_extlbl_addr(elffile_t* efile,char* label);

///**
// * Checks if an ELF file is a dynamic executable
// * \param efile The structure containing the parsed ELF file
// * \return TRUE if the file is a dynamic executable (contains a .dynamic section), FALSE otherwise
// */
//extern int elffile_isexedyn(elffile_t* efile);

///**
// * Adds a target address link to an ELF file from its section and offset
// * \param efile The parsed ELF file
// * \param elfobj A pointer to an object referencing an address
// * \param tscnidx The index of the section containing this address
// * \param toffs The offset of the address from the beginning of the section
// * \param updfunc A pointer to the function to be used to update the object (if the address changes)
// * \param srcscnidx The index of the ELF section containing the object (if applicable)
// * \param srctblidx The index in the ELF table where the object is at (if applicable)
// */
//extern void elffile_add_targetscnoffset(elffile_t* efile, void* elfobj, int tscnidx, int64_t toffs,
//		addrupdfunc_t updfunc, int srcscnidx, int srctblidx);

///**
// * Updates entries in the dynamic table relative to sections sizes to reflect the section's sizes'
// * changes
// * \param efile The structure holding the parsed ELF file
// */
//extern void elfexe_dyntbl_updsz(elffile_t* efile) ;

///**
// * Reorder sections and update them in order to re-write the ELF file
// * Segments are added if needed, offsets and addresses of sections / segments
// * are updated and sections are reordered to minimize changes in the final ELF file
// * \param efile The structure holding the parsed ELF file
// * \param codelen size of the code section to add. No section added if the value is 0
// * \param datalen size of the data section to add. No section added if the value is 0
// * \param pltlen size of the plt section to add. No section added if the value is 0
// * \param codeidx if codelen > 0, index of the added code section
// * \param dataidx if datalen > 0, index of the added data section
// * \param pltidx if pltlen > 0, index of the added plt section
// * \param lblname name of a label to add
// * \return an array containing index of sections. The value is the current position of
// *         the section and the index is the new position of the section
// */
extern int* elfexe_scnreorder(elffile_t* efile, asmfile_t* afile,
      int64_t codelen, int64_t datalen, uint64_t tdataSize, uint64_t tbssSize,
      int64_t pltlen, int* codeidx, int* dataidx, int* tdataMadrasIdx,
      int tdataVarsSize, int* tbssMadrasIdx, int tbssVarsSize, int* pltidx,
      char* lblname, elffile_t** addedfiles, int64_t* addedfiles_newaddrs,
      int n_addedfiles, queue_t** pltList);

///**
// * Updates all targets in an elf file (using the target hashtable)
// * \param efile The structure holding the parsed ELF file
// */
//extern void elffile_updtargets (elffile_t* efile);

///**
// * Creates a new ELF file based on the contents of the elffile_t structure and reordering
// * the sections based on the reorder table
// * \param efile Structure holding a parsed ELF file, which has been modified
// * \param filename Name of the file to save the modified file under
// * \param permtbl Array containing the indexes of the old sections (i.e. permtbl[i] contains the
// * former index of the section moved at index i)
// */
//extern void copy_elf_file_reorder(elffile_t* efile, char* filename, int* permtbl);

///**
// * Changes a dynamic library in a file (used to replace a dynamic library by its patched version)
// * \param efile An ELF file
// * \param oldname Name of the dynamic library to replace
// * \param newname Name of the new dynamic library
// * \return 1 if the library has been found and replaced, 0 otherwise
// */
//extern int elffile_rename_dynamic_library (elffile_t* efile, char* oldname, char* newname);

///**
// * Returns the index of the section containing prog bits starting at the given address
// * \param efile Structure containing the parsed ELF file
// * \param addr Address
// * \return Index of the section in the file or -1 if no section found
// */
//extern int elffile_get_progscnataddr(elffile_t* efile, int64_t addr);

///**
// * Returns the index of the section spanning over the given address
// * \param efile Parsed ELF file
// * \param addr Address over which we are looking for a spanning section
// * \return The index of the section in the ELF file, or -1 if no section was found
// */
//extern int elffile_get_scnspanaddr(elffile_t* efile, int64_t addr);

///**
// * Returns the section a label belongs to
// * \param efile Pointer to the ELF file
// * \param label the label to look for
// * \return The number of the section, or -1 if the label is not present in the file
// * */
//extern int elffile_label_get_scn(elffile_t* efile, char* label);

///**
// * Checks if a label is of type "dummy" (see \ref elffile_add_dummysymbol)
// * \param efile Pointer to the ELF file
// * \param label the label to look for
// * \return 1 if the label is of type "dummy" (STT_NUM) in the ELF file, 0 otherwise. If the label was
// * not found or if no symbol table is present in the file, returns -1.
// */
//extern int elffile_label_isdummy(elffile_t* efile,char* label);

///**
// * Creates a label_t structure from a tableeentry_t structure
// * \param ef The ELF file
// * \param symbol The table entry containing the label
// * \return A label structure containing the informations about the label
// * */
//extern label_t* elffile_label_getaslabel(elffile_t* efile, tableentry_t* symbol);

///**
// * Returns debug data associated to a function
// * \param asmf A disassembled assembly file
// * \param low_pc address of the function label
// * \return debug data associated to the function or NULL
// */
//extern fct_dbg_t* asmfile_get_fct_debug (asmfile_t* asmf, int64_t low_pc);

/**
 * Prints external functions (sorted) using ELF file data
 * \param asmf An assembly file
 * \param outfile Name of the file to print the list of functions to
 * \return EXIT_SUCCESS if successful, error code otherwise
 */
extern int elf_asmfile_print_external_fcts(asmfile_t* asmf, char* outfile);

/**
 * Prints the formatted contents of a parsed ELF file
 * \param asmf Structure containing a parsed ELF file
 */
extern void elf_asmfile_print_binfile(asmfile_t* asmf);

///**
// * Prints the ELF header
// * \param efile a structure representing an ELF file
// */
//extern void print_elf_header (elffile_t* efile);

///**
// * Prints the array of ELF section headers
// * \param efile a structure representing an ELF file
// */
//extern void print_section_header (binfile_t* bf);

///**
// * Prints the array of ELF program header
// * \param efile a structure representing an ELF file
// */
//extern void print_segment_header (binfile_t* bf);

///**
// * Prints the dynamic section content
// * \param efile a structure representing an ELF file
// */
//extern void print_dynamic_section (binfile_t* bf);

///**
// * Prints the mapping between sections and segments
// * \param efile a structure representing an ELF file
// */
//extern void print_mapping_segment_sections (binfile_t* bf);

///**
// * Prints DYNSYM and SYMTAB sections
// * \param efile a structure representing an ELF file
// * \param dynstr_idx index of section containing names
// */
//extern void print_sym_tables (binfile_t* bf);

///**
// * Prints RELA section
// * \param efile a structure representing an ELF file
// * \param dynstr_idx index of section containing names
// * \param symtab_idx index of the symbols table
// */
//extern void print_rela_section (binfile_t* bf);

///**
// * Prints VERSYM section
// * \param efile a structure representing an ELF file
// * \param indx index of the versym section to print
// */
//extern void print_versym(binfile_t *bf, Elf* elf, int indx);

///**
// * Prints VERNEED section
// * \param efile a structure representing an ELF file
// * \param indx index of the verneed section to print
// */
//extern void print_verneed(binfile_t *bf, Elf* elf, int indx);

///**
// * Prints a formatted representation of the code areas. A code area contains a number of consecutive
// * sections containing executable code
// * \param efile A structure representing an ELF file
// * */
//extern void print_codeareas(binfile_t *bf);

///**
// * Looks for several sections index
// * \param efile a structure representing an ELF file
// * \param dynstr_idx used to return index of section containing names
// * \param symtab_idx used to return index of the symbols table
// */
//extern void lookfor_sections (elffile_t* efile, int* dynstr_idx, int* symtab_idx);

///**
// * Returns bits for the given section
// * \param efile The structure containing the parsed ELF file
// * \param indx index of the section
// * \return an array of bytes or NULL if \c efile is NULL or \c indx is outside of its range of section indexes
// */
//extern unsigned char* ELF_get_scn_bits (elffile_t* efile, int indx);

///**
// * Returns a string located in section indx at offset pos
// * \param efile a structure representing an ELF file
// * \param indx index of the section containing the string
// * \param pos offset of the string in the section
// */
//extern char* ELF_strptr(elffile_t* efile, int indx, int pos);

///**
// * Returns a queue of all undefined symbols in a parsed ELF file.
// * This function is intended to be used with relocatable files
// * \param efile Pointer to a structure holding the parsed ELF file
// * \return Queue of the names of symbols whose index is undefined, or NULL if \e efile is NULL
// */
//extern queue_t* elffile_getsymbols_undefined(elffile_t* efile);

///**
// * Checks if a given symbol is defined in an ELF file
// * This function is mainly intended to be used on relocatable files (otherwise lookup into efile->labels is enough)
// * \param efile Pointer to a structure holding the parsed ELF file
// * \param symname Name of the symbol to look up to
// * \return TRUE if the symbol is defined in the file, FALSE otherwise
// */
//extern int elffile_checksymbol_defined(elffile_t* efile, char* symname);

/***********************************************************************/
////////////NEW INTERFACE FOR USE WITH LIBMBINFILE///////////////////////
// Interface functions for the driver//
///////////////////////////////////////
/**
 * Adds an external library to a file being patched
 * \param bf The binary file (must be in the process of being patched)
 * \param extlibname Name of the library to add
 * \return EXIT_SUCCESS if successful, error code otherwise
 * */
extern int elf_binfile_patch_add_extlib(binfile_t* bf, char* extlibname,
      boolean_t priority);

/**
 * Rename an existing external library
 * \param bf The binary file (must be in the process of being patched)
 * \param oldname Name of the library to rename
 * \param newname New name of the library
 * \return EXIT_SUCCESS if successful, error code otherwise
 * */
extern int elf_binfile_patch_rename_extlib(binfile_t* bf, char* oldname,
      char* newname);

/**
 * Adds a label to the file
 * \param bf The binary file (must be in the process of being patched)
 * \param label Label to add. Its type will must be set to allow identifying how and where to insert it
 * \return EXIT_SUCCESS if successful, error code otherwise
 * */
extern int elf_binfile_patch_add_label(binfile_t* bf, label_t* label);

/**
 * Adds a new section to a file. If the address of the section is set it must use it
 * \param bf The binary file (must be in the process of being patched)
 * \param scn The section to add
 * \return EXIT_SUCCESS if successful, error code otherwise
 * */
extern int elf_binfile_patch_add_scn(binfile_t* bf, binscn_t* scn);

/**
 * Adds a new segment to a file. If the address of the segment is set it must use it
 * \param bf The binary file (must be in the process of being patched)
 * \param seg The segment to add
 * \return EXIT_SUCCESS if successful, error code otherwise
 * */
extern int elf_binfile_patch_add_seg(binfile_t* bf, binseg_t* seg);

/**
 * Adds a call to a function from an external library to a file, with pre-loaded or dynamic loading
 * \param bf The binary file (must be in the process of being patched)
 * \param fctname Name of the function to add
 * \param libname Name of the external library where the function is defined (may be optional depending on the format)
 * \param preload Set to TRUE if the function address must be loaded when loading the executable, FALSE if the function
 * address is to be loaded only when first invoked
 * \return A pointer containing a reference to the object to reference for invoking this function
 * */
extern pointer_t* elf_binfile_patch_add_extfct(binfile_t* bf, char* fctname,
      char* libname, int preload);

/**
 * Finalises a binary file being patched by building its format specific structure (elffile_t and Elf)
 * \param bf The binary file (must be in the process of being patched)
 * \param emptyspaces Queue of interval_t structures representing the available empty spaces in the file
 * \return EXIT_SUCCSS if successful, error code otherwise
 * */
extern int elf_binfile_patch_finalise(binfile_t* bf, queue_t* emptyspaces);

// Loading functions //
///////////////////////
/**
 * Loads a binfile_t structure with the result of the parsing of an ELF file
 * \param bf A binary file structure containing the name of the file to parse
 * \return EXIT_SUCCESS if the file could be successfully parsed as an ELF file, error code otherwise.
 * If successful, the structure representing the binary file will have been updated with the result of the parsing
 * */
extern int elf_binfile_load(binfile_t* bf);

/**
 * Loads a filled binfile_t structure with the result of parsing its debug format
 * \param bf A binary file structure containing a successfully parsed ELF file
 * \return TRUE if the debug data could be successfully parsed as DWARF, FALSE otherwise
 * */
extern int binfile_load_dwarf(binfile_t* bf);

/**
 * Attempts to parse the debug informaiton of a filled binfile_t structure.
 * \param bf A binary file structure containing a successfully parsed ELF file
 * \return TRUE if the debug data could be successfully parsed as DWARF, FALSE otherwise
 * */
extern dbg_file_t* elf_binfile_parse_dbg(binfile_t* bf);

// Patching functions //
////////////////////////
/**
 * Used for initialising the format specific internal structure of a file
 * \param bf A binary file in the process of being patched. Its \c creator member must be filled
 * \return EXIT_SUCCESS if successful, error cod eotherwise
 * */
extern int elf_binfile_patch_init_copy(binfile_t* bf);
/**
 * Adds a string to a string table (section at the specified index)
 * \param efile The parsed ELF file
 * \param stridx Index of the section containing the string table
 * \param str String to add
 * \return Index of the added string in the table, or its index if it already exists (does not work correctly)
 * */
extern int elffile_upd_strtab(elffile_t* efile, int stridx, char* str);

/**
 * Returns the code of the last error encountered and resets it
 * \param efile The parsed ELF file
 * \return The last error code encountered. It will be reset to EXIT_SUCCESS afterward.
 * If \c efile is NULL, ERR_BINPARSE_MISSING_BINFILE will be returned
 * */
extern int elffile_get_last_error_code(elffile_t* efile);

/**
 * Retrieves the machine code of an ELF file, performing minimal parsing on the file
 * \param file_name Name of the file
 * \return ELF-specific identifier of the machine for which this ELF file was compiled, ELF_MACHINE_CODE_ERR if not an ELF file
 * or if the file does not exist
 * */
extern int elf_get_machine_code(char* file_name);


extern int elffile_upd_symlbl(elffile_t* ef, char* lbl, int64_t addr, int size,
      int info, int scnidx, int dynamic);
extern int elffile_allocate_symlbl(elffile_t* ef, int nb_symbols);

#endif /* LIBMTROLL_H_ */
