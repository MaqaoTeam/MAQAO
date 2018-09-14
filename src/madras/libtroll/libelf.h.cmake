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
 *============================================================================
 * This file described the API used by the libdwarf to manipulate ELF binary format.
 * As the libdwarf uses only a subset of the libelf, all functions implemented in usual libelf
 * are not implemented in this library.
 *============================================================================
 **/

#ifndef __HEADER_LIBELF__
#define __HEADER_LIBELF__
// Get the ELF types.
#include <elf.h>
#include <sys/types.h>

#cmakedefine _HAVE_APPLE_
#cmakedefine _HAVE_UNIX_



/**
 * \struct Elf
 * Descriptor for the ELF file.
 */
typedef struct Elf Elf;

/**
 * \struct Elf_Scn
 * Descriptor for ELF file section.  
 */
typedef struct Elf_Scn Elf_Scn;

/** Alias for struct ar_s structure*/
typedef struct ar_s ar_t;

/**
 * \enum Elf_Kind
 * Supported binary format.  
 */
typedef enum
{
   ELF_K_NONE = 0,         /**< Unknown.  */
   ELF_K_AR,               /**< Archive.  */
   ELF_K_ELF,              /**< ELF file.  */
   // Keep this the last entry.
   ELF_K_NUM               /**< Number of entries in the enum*/
} Elf_Kind;

/**
 * \enum Elf_Cmd
 * Commands for `...'.  
 */
typedef enum
{
  ELF_C_NULL,              /**< Nothing, terminate, or compute only.  */
  ELF_C_READ,              /**< Read .. */
  ELF_C_RDWR,              /**< Read and write .. */
  ELF_C_WRITE,             /**< Write .. */
  ELF_C_CLR,               /**< Clear flag.  */
  ELF_C_SET,               /**< Set flag.  */
  ELF_C_FDDONE,            /**< Signal that file descriptor will not be
				                    used anymore.  */
  ELF_C_FDREAD,            /**< Read rest of data so that file descriptor
				                    is not used anymore.  */ 
  ELF_C_READ_MMAP,         /**< Read, but mmap the file if possible.  */
  ELF_C_RDWR_MMAP,         /**< Read and write, with mmap.  */
  ELF_C_WRITE_MMAP,        /**< Write, with mmap.  */
  ELF_C_READ_MMAP_PRIVATE, /**< Read, but memory is writable, results are
                                not written to the file.  */
  ELF_C_EMPTY,             /**< Copy basic file data but not the content. */
  // Keep this the last entry.
  ELF_C_NUM                /**< Number of entries in the enum*/
} Elf_Cmd;

/**
 * \enum Elf_Type
 * Known translation types.  
 */
typedef enum
{
  ELF_T_BYTE,        /**< unsigned char */
  ELF_T_ADDR,        /**< Elf32_Addr, Elf64_Addr, ... */
  ELF_T_DYN,         /**< Dynamic section record.  */
  ELF_T_EHDR,        /**< ELF header.  */
  ELF_T_HALF,        /**< Elf32_Half, Elf64_Half, ... */
  ELF_T_OFF,         /**< Elf32_Off, Elf64_Off, ... */
  ELF_T_PHDR,        /**< Program header.  */
  ELF_T_RELA,        /**< Relocation entry with addend.  */
  ELF_T_REL,         /**< Relocation entry.  */
  ELF_T_SHDR,        /**< Section header.  */
  ELF_T_SWORD,       /**< Elf32_Sword, Elf64_Sword, ... */
  ELF_T_SYM,         /**< Symbol record.  */
  ELF_T_WORD,        /**< Elf32_Word, Elf64_Word, ... */
  ELF_T_XWORD,       /**< Elf32_Xword, Elf64_Xword, ... */
  ELF_T_SXWORD,      /**< Elf32_Sxword, Elf64_Sxword, ... */
  ELF_T_VDEF,        /**< Elf32_Verdef, Elf64_Verdef, ... */
  ELF_T_VDAUX,       /**< Elf32_Verdaux, Elf64_Verdaux, ... */
  ELF_T_VNEED,       /**< Elf32_Verneed, Elf64_Verneed, ... */
  ELF_T_VNAUX,       /**< Elf32_Vernaux, Elf64_Vernaux, ... */
  ELF_T_NHDR,        /**< Elf32_Nhdr, Elf64_Nhdr, ... */
  ELF_T_SYMINFO,		/**< Elf32_Syminfo, Elf64_Syminfo, ... */
  ELF_T_MOVE,			/**< Elf32_Move, Elf64_Move, ... */
  ELF_T_LIB,			/**< Elf32_Lib, Elf64_Lib, ... */
  ELF_T_GNUHASH,		/**< GNU-style hash section.  */
  ELF_T_AUXV,			/**< Elf32_auxv_t, Elf64_auxv_t, ... */
  // Keep this the last entry.
  ELF_T_NUM          /**< Number of entries in the enum*/
} Elf_Type;

/**
 * \struct Elf_Data
 * Descriptor for data to be converted to or from memory format.  
 */
typedef struct
{
  void *d_buf;             /**< Pointer to the actual data.  */
  Elf_Type d_type;         /**< Type of this piece of data.  */
  unsigned int d_version;  /**< ELF version.  */
  size_t d_size;           /**< Size in bytes.  */
#if (defined _HAVE_APPLE_) || (defined _WIN32)    // On OSX or Windows, off_t type should be used instead of loff_t
  off_t d_off;             /**< Offset into section.  */
#else
  loff_t d_off;            /**< Offset into section.  */
#endif
  size_t d_align;          /**< Alignment in section.  */
} Elf_Data;


/**
 * Checks the Elf version
 * \param __version the version of the Elf file
 * \return EV_CURRENT if there is no problem, else EV_NONE
 */
extern unsigned int elf_version (unsigned int __version);

/**
 * Gets the kind of an Elf file (Ar file, ELF file ...)
 * \param __elf an Elf structure
 * \return an Elf_Kind containing the Elf kind
 */
extern Elf_Kind elf_kind (Elf *__elf);

/**
 * Loads an Elf file
 * \param __filedes the input file file descriptor
 * \param __cmd flags used to open the file. 
 *              Only ELF_C_READ and ELF_C_READ_MMAP are supported
 * \param __ref Reference Elf file. If __ref is an Elf file, it is
 *              returned. If __ref is an archive (AR file), then 
 *              an object file is extracted from the archive.
 *              In this case, the function works as an iterator: at
 *              each call with the same ref, it returns the next 
 *              object file or NULL if all files have been returned.
 *              The iterator can be reset using elf_reset_ar_iterator
 * \return the fulled Elf structure
 */
extern Elf *elf_begin (int __fildes, Elf_Cmd __cmd, Elf *__ref);

/**
 * Frees an Elf structure
 * \param __elf an Elf structure to free
 * \return 1 if there is no problem, else 0
 */
extern int elf_end (Elf *__elf);

/**
 * Frees an Elf structure, but not the data from the sections (this is for handling the cases where they have been allocated elsewhere)
 * \param __elf an Elf structure to free
 * \return 1 if there is no problem, else 0
 */
extern int elf_end_nodatafree (Elf *__elf);

/**
 * Returns the section at index __index
 * \param __elf an Elf structure
 * \param __index index of the section to return 
 * \return NULL if there is a problem, else the Elf_Scn structure
 */
extern Elf_Scn *elf_getscn (Elf *__elf, size_t __index);

/**
 * Returns the data corresponding to a section
 * \param __scn an initialized Elf_Scn structure
 * \param __data Not used but present in the standard API
 * \return The Elf_Data corresponding to __scn or NULL if
 *         there is an error
 */
extern Elf_Data *elf_getdata (Elf_Scn *__scn, Elf_Data *__data);

/**
 * Returns the 32b section header associated to a section.
 * \param __scn an initialized Elf_Scn structure
 * \return the 32b section header or NULL if there is a problem
 */
extern Elf32_Shdr *elf32_getshdr (Elf_Scn *__scn);

/**
 * Returns the 64b section header associated to a section.
 * \param __scn an initialized Elf_Scn structure
 * \return the 64b section header or NULL if there is a problem
 */
extern Elf64_Shdr *elf64_getshdr (Elf_Scn *__scn);

/**
 * Returns the string located in the section __index at the offset __offset
 * \param __elf an Elf structure
 * \param __index index of the section the string belongs to
 * \param __offset offset in the section of the string
 * \return NULL if there is a problem, else the string located in the section
 *         __index at the offset __offset
 */
extern char *elf_strptr (Elf *__elf, size_t __index, size_t __offset);

/**
 * Returns the ident from an ELF file.
 * \param __elf an initialized Elf structure
 * \param __nbytes Not used but present in the standard API
 * \return the elf ident string or NULL if there is a problem
 */
extern char *elf_getident (Elf *__elf, size_t *__nbytes);

/**
 * Returns the 32b elf header associated to an Elf structure.
 * \param __elf an initialized Elf structure
 * \return the 32b elf header or NULL if there is a problem (no 32b
 *         elf header for example)
 */
extern Elf32_Ehdr *elf32_getehdr (Elf *__elf);

/**
 * Returns the 64b elf header associated to an Elf structure.
 * \param __elf an initialized Elf structure
 * \return the 64b elf header or NULL if there is a problem (no 64b
 *         elf header for example)
 */
extern Elf64_Ehdr *elf64_getehdr (Elf *__elf);

/**
 * Returns the 32b program header
 * \param __elf an initialized Elf structure
 * \return the 32b program header or NULL if there is a problem
 */
extern Elf32_Phdr *elf32_getphdr (Elf *__elf);

/**
 * Returns the 64b program header
 * \param __elf an initialized Elf structure
 * \return the 64b program header or NULL if there is a problem
 */
extern Elf64_Phdr *elf64_getphdr (Elf *__elf);

/**\todo TODO (2015-04-08) The two following functions can't be named elfxx_getshdr as there are already
 * functions with that name returning the header of a specific function, and which are used by the
 * libdwarf. If we can rename those functions in the libdwarf, we could rename them and the elfxx_getshdr
 * functions to have more appropriate names.*/
 
/**
 * Returns the 32b section header
 * \param __elf an initialized Elf structure
 * \return the 32b section header or NULL if there is a problem
 */
extern Elf32_Shdr *elf32_getfullshdr (Elf *__elf);

/**
 * Returns the 64b section header
 * \param __elf an initialized Elf structure
 * \return the 64b section header or NULL if there is a problem
 */
extern Elf64_Shdr *elf64_getfullshdr (Elf *__elf);


// ============================================================================
//            FOLLOWING FUNCTIONS ARE NOT DEFINED IN LIBELF API
// ============================================================================
/**
 * Returns the name of an elf file if it has been extracted from 
 * an archive file
 * \param __elf an initialized Elf structure
 * \return the file name if possible, else a NULL value
 */
extern char* elf_getname (Elf* __elf);

/**
 * Returns the Elf machine code of the file
 * \param __elf an Elf structure
 * \return an Elf machine code (EM_ macro from elf.h)
 */
extern int elf_getmachine (Elf* __elf);

/**
 * Resets ELF object file iterator to the first element for
 * an archive file
 * \param __elf an Elf structure
 */
extern void elf_reset_ar_iterator (Elf* __elf);

/**
 * Returns the number of element in an AR file
 * \param __elf an Elf structure
 * \return the number of element in the AR file
 */
extern unsigned int elf_get_ar_size (Elf* __elf);

/**
 * Returns the machine code of a file. This function performs minimal parsing of the file
 * \param Valid file descriptor
 * \return Machine code of the file, or EM_NONE if the file is not a valid ELF file
 * */
extern unsigned int get_elf_machine_code(int filedes);

// ============================================================================
//            GETTER AND SETTERS FOR ELF OBJECT MEMBERS
// ============================================================================
/**\todo TODO COMMENT DEFINITIONS*/
extern unsigned char* Elf_Ehdr_get_e_ident   (Elf* elf);
extern Elf64_Half Elf_Ehdr_get_e_type        (Elf* elf);
extern Elf64_Half Elf_Ehdr_get_e_machine     (Elf* elf);
extern Elf64_Word Elf_Ehdr_get_e_version     (Elf* elf);
extern Elf64_Addr Elf_Ehdr_get_e_entry       (Elf* elf);
extern Elf64_Off  Elf_Ehdr_get_e_phoff       (Elf* elf);
extern Elf64_Off  Elf_Ehdr_get_e_shoff       (Elf* elf);
extern Elf64_Word Elf_Ehdr_get_e_flags       (Elf* elf);
extern Elf64_Half Elf_Ehdr_get_e_ehsize      (Elf* elf);
extern Elf64_Half Elf_Ehdr_get_e_phentsize   (Elf* elf);
extern Elf64_Half Elf_Ehdr_get_e_phnum       (Elf* elf);
extern Elf64_Half Elf_Ehdr_get_e_shentsize (Elf* elf);
extern Elf64_Half Elf_Ehdr_get_e_shnum       (Elf* elf);
extern Elf64_Half Elf_Ehdr_get_e_shstrndx    (Elf* elf);

extern void Elf_Ehdr_set_e_ident       (Elf* elf, unsigned char* e_ident);
extern void Elf_Ehdr_set_e_type        (Elf* elf, Elf64_Half    e_type);
extern void Elf_Ehdr_set_e_machine     (Elf* elf, Elf64_Half    e_machine);
extern void Elf_Ehdr_set_e_version     (Elf* elf, Elf64_Word    e_version);
extern void Elf_Ehdr_set_e_entry       (Elf* elf, Elf64_Addr    e_entry);
extern void Elf_Ehdr_set_e_phoff       (Elf* elf, Elf64_Off     e_phoff);
extern void Elf_Ehdr_set_e_shoff       (Elf* elf, Elf64_Off     e_shoff);
extern void Elf_Ehdr_set_e_flags       (Elf* elf, Elf64_Word    e_flags);
extern void Elf_Ehdr_set_e_ehsize      (Elf* elf, Elf64_Half    e_ehsize);
extern void Elf_Ehdr_set_e_phentsize   (Elf* elf, Elf64_Half    e_phentsize);
extern void Elf_Ehdr_set_e_phnum       (Elf* elf, Elf64_Half    e_phnum);
extern void Elf_Ehdr_set_e_shentsize   (Elf* elf, Elf64_Half    e_shentsize);
extern void Elf_Ehdr_set_e_shnum       (Elf* elf, Elf64_Half    e_shnum);
extern void Elf_Ehdr_set_e_shstrndx    (Elf* elf, Elf64_Half    e_shstrndx);





extern Elf64_Word  Elf_Shdr_get_sh_name      (Elf* elf, Elf64_Half indx);
extern Elf64_Word  Elf_Shdr_get_sh_type      (Elf* elf, Elf64_Half indx);
extern Elf64_Xword Elf_Shdr_get_sh_flags     (Elf* elf, Elf64_Half indx);
extern Elf64_Addr  Elf_Shdr_get_sh_addr      (Elf* elf, Elf64_Half indx);
extern Elf64_Off   Elf_Shdr_get_sh_offset    (Elf* elf, Elf64_Half indx);
extern Elf64_Xword Elf_Shdr_get_sh_size      (Elf* elf, Elf64_Half indx);
extern Elf64_Word  Elf_Shdr_get_sh_link      (Elf* elf, Elf64_Half indx);
extern Elf64_Word  Elf_Shdr_get_sh_info      (Elf* elf, Elf64_Half indx);
extern Elf64_Xword Elf_Shdr_get_sh_addralign (Elf* elf, Elf64_Half indx);
extern Elf64_Xword Elf_Shdr_get_sh_entsize   (Elf* elf, Elf64_Half indx);

extern void Elf_Shdr_set_sh_name      (Elf* elf, Elf64_Half indx, Elf64_Word  sh_name);
extern void Elf_Shdr_set_sh_type      (Elf* elf, Elf64_Half indx, Elf64_Word  sh_type);
extern void Elf_Shdr_set_sh_flags     (Elf* elf, Elf64_Half indx, Elf64_Xword sh_flags);
extern void Elf_Shdr_set_sh_addr      (Elf* elf, Elf64_Half indx, Elf64_Addr  sh_addr);
extern void Elf_Shdr_set_sh_offset    (Elf* elf, Elf64_Half indx, Elf64_Off   sh_offset);
extern void Elf_Shdr_set_sh_size      (Elf* elf, Elf64_Half indx, Elf64_Xword sh_size);
extern void Elf_Shdr_set_sh_link      (Elf* elf, Elf64_Half indx, Elf64_Word  sh_link);
extern void Elf_Shdr_set_sh_info      (Elf* elf, Elf64_Half indx, Elf64_Word  sh_info);
extern void Elf_Shdr_set_sh_addralign (Elf* elf, Elf64_Half indx, Elf64_Xword addralign);
extern void Elf_Shdr_set_sh_entsize   (Elf* elf, Elf64_Half indx, Elf64_Xword sh_entisze);





extern Elf64_Word  Elf_Phdr_get_p_type   (Elf* elf, Elf64_Half indx);
extern Elf64_Word  Elf_Phdr_get_p_flags  (Elf* elf, Elf64_Half indx);
extern Elf64_Off   Elf_Phdr_get_p_offset (Elf* elf, Elf64_Half indx);
extern Elf64_Addr  Elf_Phdr_get_p_vaddr  (Elf* elf, Elf64_Half indx);
extern Elf64_Addr  Elf_Phdr_get_p_paddr  (Elf* elf, Elf64_Half indx);
extern Elf64_Xword Elf_Phdr_get_p_filesz (Elf* elf, Elf64_Half indx);
extern Elf64_Xword Elf_Phdr_get_p_memsz  (Elf* elf, Elf64_Half indx);
extern Elf64_Xword Elf_Phdr_get_p_align  (Elf* elf, Elf64_Half indx);

extern void Elf_Phdr_set_p_type   (Elf* elf, Elf64_Half indx, Elf64_Word p_type);
extern void Elf_Phdr_set_p_flags  (Elf* elf, Elf64_Half indx, Elf64_Word p_flags);
extern void Elf_Phdr_set_p_offset (Elf* elf, Elf64_Half indx, Elf64_Off p_offset);
extern void Elf_Phdr_set_p_vaddr  (Elf* elf, Elf64_Half indx, Elf64_Addr p_vaddr);
extern void Elf_Phdr_set_p_paddr  (Elf* elf, Elf64_Half indx, Elf64_Addr p_paddr);
extern void Elf_Phdr_set_p_filesz (Elf* elf, Elf64_Half indx, Elf64_Xword filesz);
extern void Elf_Phdr_set_p_memsz  (Elf* elf, Elf64_Half indx, Elf64_Xword p_memsz);
extern void Elf_Phdr_set_p_align  (Elf* elf, Elf64_Half indx, Elf64_Xword p_align);

/**
 * Sets the data of an ELF section (updates its \c data member).
 * This function assumes that all the elements of the section have been already set.
 * \param scn The ELF section.
 * \param data Data to set into the section
 * */
extern void elf_setdata(Elf_Scn* scn, void* data);

// ============================================================================
//            ELF WRITING FUNCTIONS
// ============================================================================

/**
 * Writes an Elf structure into a file
 * \param elf an elffile to write
 * \param stream File stream, must be associated to a file opened for writing
 * \return 1 (TRUE) if success, else 0 (FALSE)
 */
extern int elf_write(Elf* elf, void* stream);

// ============================================================================
//            ELF DUPLICATION FUNCTIONS
// ============================================================================

/**
 * Copies an ELF section to another
 * \param dest Structure representing the elf section to copy to. Must have already been initialised (see \ref elf_init_sections)
 * \param origin Structure representing the elf section to copy
 * \return 1 if successful, 0 if if origin is NULL or has neither 32 nor 64 bits header
 * */
extern int elf_scn_copy(Elf_Scn* dest, Elf_Scn* origin);

/**
 * Sets the bytes in a section. No other value will be initialised
 * \param scn Structure representing an elf section
 * \param data Bytes to store in the section
 * \return 1 if successful, 0 otherwise
 * */
extern int elf_scn_setdatabytes(Elf_Scn* scn, unsigned char* data);

/**
 * Gets the bytes in a section.
 * \param scn Structure representing an elf section
 * \return Bytes stored in the section
 * */
extern unsigned char* elf_scn_getdatabytes(Elf_Scn* scn);

/**
 * Duplicates an Elf structure for writing
 * \param origin File to copy from. Only the header will be copied
 * \param stream File stream, must be associated to a file opened for writing
 * \return Empty Elf structure
 * */
extern Elf* elf_copy(Elf* origin, void* stream);

/**
 * Initialises the segments in an Elf file
 * \param elf Structure representing an ELF file.
 * \param phnum Number of segments
 * \return 1 (TRUE) if successful, 0 (FALSE) otherwise (this happens if the file segments have already been initialised)
 * */
extern int elf_init_segments(Elf* elf, Elf64_Half phnum);

/**
 * Initialises the sections in an Elf file
 * \param elf Structure representing an ELF file.
 * \param shnum Number of sections
 * \return 1 (TRUE) if successful, 0 (FALSE) otherwise (this happens if the file sections have already been initialised)
 * */
extern int elf_init_sections(Elf* elf, Elf64_Half shnum);

#endif
