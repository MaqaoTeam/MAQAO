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
 * \file libmtroll.c
 * \brief This file contains all functions needed to parse, modify and create ELF files
 * */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#if !(defined HAVE_APPLE) && !(defined _WIN32)
typedef off_t off64_t;/**<\todo This typedef has to be added under an ifdefine checking the architecture & Linux distribution (at least Ubuntu64)*/
#endif
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
#include <libdwarf.h>
#include <fcntl.h>
#include <inttypes.h>
#include <assert.h>

#include "arch.h"
#include "archinterface.h"
#include "bfile_fmtinterface.h"
#include "libmtroll.h"
#include "libmdbg.h"

//FOR THE STATIC PATCHER (REMOVE DURING THE PATCHER REDESIGNING?)
#include "assembler.h"
//#include "patch_archinterface.h"
/**\todo (2015-08-14) Temporary fix of a circular dependency problem. The functions needed that should be present in the
 * patch_archinterface have been moved back into the assembler driver. To be fixed when the patcher refactoring is complet*/
//--> No, I used it for retrieving the PLT labels without having disassembled the file.
// But we can still refactor the assembler/patcher files to improve the way we use them
// =>not anymore now
/**
 * Name of the section added to an ELF file when inserting code
 * */
#define MADRAS_TEXTSCN_NAME ".madras.text"

/**
 * Name of the section added to an ELF file when inserting code at a fixed address
 * */
#define MADRAS_FIXTXTSCN_NAME ".madras.text.fix"

/**
 * Name of the section added to an ELF file when inserting data
 * */
#define MADRAS_DATASCN_NAME ".madras.data"

/**
 * Name of the section added to an ELF file when inserting an additional PLT section
 * */
#define MADRAS_PLTSCN_NAME ".madras.plt"

/**
 * Name of the section containing unallocated symbols (type NOBITS) when inserting files
 * */
#define MADRAS_UNALLOCSCN_NAME ".madras.bss"

/**
 * Name of the section containing the moved code in a file patched by DynInst
 * */
#define DYNINST_SCN_NAME ".dyninst"

/**
 * Label to add to the first instruction of the .plt section
 * \todo The goal of this define is to avoid having the two first instructions of the plt (they don't have a label) to be added to the _init function.
 * It may be moved elsewhere in the files, or replaced by another mechanism.
 * */
#define FIRST_PLT_INSN_LBL "@plt_start@"

/**
 ** Name of the section containing the  PLT.global offset table
 ** 
 ***/
#define GOTPLTNAME ".got.plt"

/**
 * Name of the section containing the procedure linkage table
 * (entry points for call of functions defined in an external library)
 * */
#define PLTNAME ".plt"

/**
 * Standard name of the section containing code
 * */
#define TXTNAME ".text"

/**
 * Standard name of the section containing initialisation code
 * */
#define ININAME ".init"

/**
 * Standard name of the section containing termination code
 * */
#define FINNAME ".fini"

/**
 * Name of the section containing the global offset table
 * 
 * */

#define GOTNAME ".got"

/**
 * Name of the section containing the Block Started by Symbol
 * 
 * */
#define BSSNAME ".bss"
/**
 * Maximal size (in bits) of a BSS section if patched sections are moved at 
 * the end of the file. 20 MB for the moment
 */
#define MAXIMAL_BSS_SIZE 20971520

/**
 * Value of the type of a "dummy" symbol added by the patcher
 * */
#define DUMMY_SYMBOL_TYPE STT_NUM

/**
 * Size of a page, as defined by the OS when loading an executable into memory
 * \todo (2014-09-09) Check if this is OS-specific and where to retrieve it otherwise (use sysconf(_SC_PAGESIZE));)
 * */
#define PAGE_SIZE 0x1000

/**
 * New congruence alignment to use for segments in the patched file
 * \todo TODO (2015-02-19) If I understand the ABI correctly, this should never be lower than PAGE_SIZE. So maybe we could use
 * PAGE_SIZE instead of NEW_SEGMENT_ALIGNMENT.
 * */
#define NEW_SEGMENT_ALIGNMENT 0x1000

/**
 * Constants defining the type of a table entry
 */
enum {
   SYMTABLE = 0, /**<Symbol table entry*/
   RELATABLE, /**<Relocation table entry (type rela)*/
   RELTABLE, /**<Relocation table entry (type rel)*/
   DYNTABLE /**<Dynamic symbol table entry*/
};

/*
 * Returns the code of the last error encountered and resets it
 * \param efile The parsed ELF file
 * \return The last error code encountered. It will be reset to EXIT_SUCCESS afterward.
 * If \c efile is NULL, ERR_BINARY_MISSING_BINFILE will be returned
 * */
int elffile_get_last_error_code(elffile_t* efile)
{
   if (efile != NULL) {
      int out = efile->last_error_code;
      efile->last_error_code = EXIT_SUCCESS;
      return out;
   } else
      return ERR_BINARY_MISSING_BINFILE;
}
/*
 * Gets the kind (an element in the enumeration ELF_kind) corresponding to the given file.
 * \param file a file to analyze
 * \return  an elment in the enumeration ELF_kind
 */
ELF_kind ELF_get_kind(FILE* file)
{
   int error = 0;
   if (file == NULL)
      return ELFkind_NONE;
   ELF_kind kind = ELFkind_NONE;

   //To place the stream at the beginning of the file
   long initial_pos = ftell(file);
   rewind(file);

   char* magicNumber = lc_malloc(SARMAG * sizeof(*magicNumber));
   error = fread(magicNumber, sizeof(*(magicNumber)), SARMAG, file);

   //check that it's an archive file! (test the magic number: "!<arch>")
   if (!strncmp(magicNumber, ARMAG, SARMAG)) {
      kind = ELFkind_AR;
   } else {
      //To place the stream at the beginning of the file
      rewind(file);

      Elf64_Ehdr* header = lc_malloc(sizeof(Elf64_Ehdr));
      error = fread(header->e_ident, sizeof(*(header->e_ident)), EI_NIDENT,
            file);

      //check that it's an ELF file! (test the magic number: ".ELF")
      if (header->e_ident[EI_MAG0] == ELFMAG0
            || header->e_ident[EI_MAG1] == ELFMAG1
            || header->e_ident[EI_MAG2] == ELFMAG2
            || header->e_ident[EI_MAG3] == ELFMAG3) {
         kind = ELFkind_ELF;
      }
      free(header);
   }
   free(magicNumber);

   //TODO: CHECK RETURN OF FREAD
   (void) error;

   fseek(file, initial_pos, SEEK_SET);
   return kind;
}
#if 0
/*
 * Duplicates the section bytes of the nth section
 * \param elf an ELF structure
 * \param n index of the section bytes to duplicate
 * \param new_size if > 0, it is the size added to the current bytes array
 As some sections are duplicated to increase them, a single malloc
 is used instead of a malloc, then a realloc with a bigger size
 * \return the new bytes or null if there is a problem
 */
static unsigned char* duplicate_section_bytes (elffile_t* elf, int n, int64_t size_added)
{
   if(!elf)
   return NULL;
   if (size_added > 0)
   {
      int64_t sh_size = Elf_Shdr_get_sh_size (elf, n, NULL);
      elf->sections[n]->bits = lc_realloc (elf->sections[n]->bits, sh_size + size_added );
   }
   return (elf->sections[n]->bits);
}

/*
 * Updates a section replacing original bytes by given ones
 * \param elf an ELF structure
 * \param idx index of the section to update
 * \param offset offset of the first byte to update
 * \param size number of bytes to update
 * \param bytes new bytes
 * \return 0 if no problem, a positive value else
 */
int ELF_update_section (elffile_t* elf, int idx, int offset, int size, unsigned char* bytes)
{
   if (elf == NULL || bytes == NULL)
   return (1);
   int64_t sh_size = Elf_Shdr_get_sh_size (elf, idx, NULL);
   unsigned char* newbytes = duplicate_section_bytes (elf, idx, size - sh_size);

   if (offset >= 0
         && size > 0
         && bytes != NULL)
   {
      memcpy (&newbytes[offset], bytes, size);
   }
   return (0);
}

/*
 * Updates bytes from a Elf64_Sym
 * \param elf an ELF structure
 * \param scnidx index of the section to update
 * \param tblidx index of the element in the table to update
 * \param sym the updated structure
 */
static void ELF_update_sym (elffile_t* ef, int scnidx, int tblidx, Elf64_Sym* sym)
{
   if ( (!ef) || (scnidx == 0) )
   return;

   unsigned char* bytes = lc_malloc (sizeof (Elf64_Sym));
   int pos = 0;
   memcpy (&bytes[pos], &(sym->st_name), sizeof (sym->st_name)); pos += sizeof (sym->st_name);
   memcpy (&bytes[pos], &(sym->st_info), sizeof (sym->st_info)); pos += sizeof (sym->st_info);
   memcpy (&bytes[pos], &(sym->st_other), sizeof (sym->st_other)); pos += sizeof (sym->st_other);
   memcpy (&bytes[pos], &(sym->st_shndx), sizeof (sym->st_shndx)); pos += sizeof (sym->st_shndx);
   memcpy (&bytes[pos], &(sym->st_value), sizeof (sym->st_value)); pos += sizeof (sym->st_value);
   memcpy (&bytes[pos], &(sym->st_size), sizeof (sym->st_size)); pos += sizeof (sym->st_size);

   unsigned char* scnbytes = ef->sections[scnidx]->bits;
   memcpy (&scnbytes[tblidx * sizeof (Elf64_Sym)], bytes, sizeof (Elf64_Sym));

   //ELF_update_section (ef, scnidx, tblidx * sizeof (Elf64_Sym), sizeof (Elf64_Sym), bytes);
   lc_free (bytes);
}

/*
 * Updates bytes from a Elf64_Rela
 * \param elf an ELF structure
 * \param scnidx index of the section to update
 * \param tblidx index of the element in the table to update
 * \param rela the updated structure
 */
static void ELF_update_rela (elffile_t* ef, int scnidx, int tblidx, Elf64_Rela* rela)
{
   if (!rela)
   return;
   unsigned char* bytes = lc_malloc (sizeof (Elf64_Rela));
   int pos = 0;
   memcpy (&bytes[pos], &(rela->r_offset), sizeof (rela->r_offset)); pos += sizeof (rela->r_offset);
   memcpy (&bytes[pos], &(rela->r_info), sizeof (rela->r_info)); pos += sizeof (rela->r_info);
   memcpy (&bytes[pos], &(rela->r_addend), sizeof (rela->r_addend)); pos += sizeof (rela->r_addend);

   ELF_update_section (ef, scnidx, tblidx * sizeof (Elf64_Rela), sizeof (Elf64_Rela), bytes);
   lc_free (bytes);
}

/*
 * Updates bytes from a Elf64_Rel
 * \param elf an ELF structure
 * \param scnidx index of the section to update
 * \param tblidx index of the element in the table to update
 * \param rel the updated structure
 */
static void ELF_update_rel (elffile_t* ef, int scnidx, int tblidx, Elf64_Rel* rel)
{
   if(!rel)
   return;
   unsigned char* bytes = lc_malloc (sizeof (Elf64_Rel));
   int pos = 0;
   memcpy (&bytes[pos], &(rel->r_offset), sizeof (rel->r_offset)); pos += sizeof (rel->r_offset);
   memcpy (&bytes[pos], &(rel->r_info), sizeof (rel->r_info)); pos += sizeof (rel->r_info);

   ELF_update_section (ef, scnidx, tblidx * sizeof (Elf64_Rel), sizeof (Elf64_Rel), bytes);
   lc_free (bytes);
}

/*
 * Updates bytes from a Elf64_Dyn
 * \param elf an ELF structure
 * \param scnidx index of the section to update
 * \param tblidx index of the element in the table to update
 * \param dyn the updated structure
 */
static void ELF_update_dyn64 (elffile_t* ef, int scnidx, int tblidx, Elf64_Dyn* dyn)
{
   if(!dyn)
   return;
   unsigned char* bytes = lc_malloc (sizeof (Elf64_Dyn));
   int pos = 0;

   memcpy (&bytes[pos], &(dyn->d_tag), sizeof (dyn->d_tag)); pos += sizeof (dyn->d_tag);
   memcpy (&bytes[pos], &(dyn->d_un.d_ptr), sizeof (dyn->d_un.d_ptr)); pos += sizeof (dyn->d_un.d_ptr);

   ELF_update_section (ef, scnidx, tblidx * sizeof (Elf64_Dyn), sizeof (Elf64_Dyn), bytes);
   lc_free (bytes);
}

/*
 * Updates bytes from a Elf32_Dyn
 * \param elf an ELF structure
 * \param scnidx index of the section to update
 * \param tblidx index of the element in the table to update
 * \param dyn the updated structure
 */
static void ELF_update_dyn32 (elffile_t* ef, int scnidx, int tblidx, Elf32_Dyn* dyn)
{
   if(!dyn)
   return;
   unsigned char* bytes = lc_malloc (sizeof (Elf64_Dyn));
   int pos = 0;

   memcpy (&bytes[pos], &(dyn->d_tag), sizeof (dyn->d_tag)); pos += sizeof (dyn->d_tag);
   memcpy (&bytes[pos], &(dyn->d_un.d_ptr), sizeof (dyn->d_un.d_ptr)); pos += sizeof (dyn->d_un.d_ptr);

   ELF_update_section (ef, scnidx, tblidx * sizeof (Elf64_Dyn), sizeof (Elf64_Dyn), bytes);
   lc_free (bytes);
}

/*
 * Updates bytes from a Elf64_Addr
 * \param elf an ELF structure
 * \param scnidx index of the section to update
 * \param tblidx index of the element in the table to update
 * \param addr the updated structure
 */
static void ELF_update_addr (elffile_t* ef, int scnidx, int tblidx, Elf64_Addr addr)
{
   unsigned char* bytes = lc_malloc (sizeof (Elf64_Addr));
   int pos = 0;
   memcpy (&bytes[pos], &(addr), sizeof (Elf64_Addr)); pos += sizeof (Elf64_Addr);

   ELF_update_section (ef, scnidx, tblidx * sizeof (Elf64_Addr), sizeof (Elf64_Addr), bytes);
   lc_free (bytes);
}

/*
 * Create a new elffile_t structure to simulate elf_begin function
 * \param fd a file descriptor
 * \param filename name of the elf file
 * \return an initialized elffile_t structure
 */
static elffile_t* ELF_begin(FILE* fd, char* filename)
{
   if (fd == NULL)
   return (NULL);

   int i = 0;
   elffile_t* elf = lc_malloc (sizeof (elffile_t));
//   elf->filedes = fd;
//   elf->filename = filename;
//   elf->elfheader = NULL;
//   elf->progheader = NULL;
//   elf->sections = NULL;
//   elf->nsections = 0;
   for (i = 0; i < MAX_NIDX; i++)
   elf->indexes[i] = -1;
//   elf->labels = NULL;
//   elf->targets = NULL;

   return(elf);
}

/*
 * Create a new elf header
 * \param elf an existing elffile_t structure
 * \param origin if not null, the new elf header is a copy of origin
 * \return A new elf header or NULL
 */
static Elf_Ehdr* ELF_newehdr(elffile_t* elf, Elf_Ehdr* origin)
{
   if (elf == NULL)
   return (NULL);

   Elf_Ehdr* ehdr = lc_malloc (sizeof (Elf_Ehdr));

   if (origin == NULL)
   return (ehdr);

   if (origin->ehdr64 != NULL)
   {
      ehdr->ehdr64 = lc_malloc (sizeof (Elf64_Ehdr));
      ehdr->ehdr32 = NULL;
      memcpy(ehdr->ehdr64->e_ident, origin->ehdr64->e_ident,EI_NIDENT);
      ehdr->ehdr64->e_type = origin->ehdr64->e_type;
      ehdr->ehdr64->e_machine = origin->ehdr64->e_machine;
      ehdr->ehdr64->e_version = origin->ehdr64->e_version;
      ehdr->ehdr64->e_entry = origin->ehdr64->e_entry;
      ehdr->ehdr64->e_phoff = origin->ehdr64->e_phoff;
      ehdr->ehdr64->e_shoff = origin->ehdr64->e_shoff;
      ehdr->ehdr64->e_flags = origin->ehdr64->e_flags;
      ehdr->ehdr64->e_ehsize = origin->ehdr64->e_ehsize;
      ehdr->ehdr64->e_phentsize = origin->ehdr64->e_phentsize;
      ehdr->ehdr64->e_phnum = origin->ehdr64->e_phnum;
      ehdr->ehdr64->e_shentsize = origin->ehdr64->e_shentsize;
      ehdr->ehdr64->e_shnum = origin->ehdr64->e_shnum;
      ehdr->ehdr64->e_shstrndx = origin->ehdr64->e_shstrndx;
   }
   else if (origin->ehdr32 != NULL)
   {
      ehdr->ehdr64 = NULL;
      ehdr->ehdr32 = lc_malloc (sizeof (Elf32_Ehdr));
      memcpy(ehdr->ehdr32->e_ident, origin->ehdr32->e_ident,EI_NIDENT);
      ehdr->ehdr32->e_type = origin->ehdr32->e_type;
      ehdr->ehdr32->e_machine = origin->ehdr32->e_machine;
      ehdr->ehdr32->e_version = origin->ehdr32->e_version;
      ehdr->ehdr32->e_entry = origin->ehdr32->e_entry;
      ehdr->ehdr32->e_phoff = origin->ehdr32->e_phoff;
      ehdr->ehdr32->e_shoff = origin->ehdr32->e_shoff;
      ehdr->ehdr32->e_flags = origin->ehdr32->e_flags;
      ehdr->ehdr32->e_ehsize = origin->ehdr32->e_ehsize;
      ehdr->ehdr32->e_phentsize = origin->ehdr32->e_phentsize;
      ehdr->ehdr32->e_phnum = origin->ehdr32->e_phnum;
      ehdr->ehdr32->e_shentsize = origin->ehdr32->e_shentsize;
      ehdr->ehdr32->e_shnum = origin->ehdr32->e_shnum;
      ehdr->ehdr32->e_shstrndx = origin->ehdr32->e_shstrndx;
   }
   elf->elfheader = ehdr;
   return (ehdr);
}

/*
 * Create a new program header
 * \param elf an existing elffile_t structure
 * \param size size of the program header.
 * \param origin an existing program header.
 * If origin is not NULL and size is equal to 0, the new program header will be a copy of origin.
 * If size is greater than origin, new empty program header entries are created
 * If size is smaller (but greater than 0), last entries are removed
 * If origin is NULL, created program header entries are empty
 */
static void ELF_newphdr (elffile_t* elf, int size, Elf_Phdr** origin)
{
   if (elf == NULL || (size <= 0 && origin == NULL))
   return;
   int real_size = Elf_Ehdr_get_e_phnum (elf, NULL);
   int i = 0;

   if (size > 0)
   real_size = size;

   Elf_Phdr** phdr = lc_malloc (real_size * sizeof (Elf_Phdr*));
   for (i = 0; i < real_size; i++)
   {
      phdr[i] = lc_malloc (sizeof(Elf_Phdr));

      if (origin[i]->phdr64 != NULL)
      {
         phdr[i]->phdr64 = lc_malloc (sizeof (Elf64_Phdr));
         phdr[i]->phdr32 = NULL;

         if (origin != NULL && i < Elf_Ehdr_get_e_phnum (elf, NULL))
         {
            phdr[i]->phdr64->p_type = origin[i]->phdr64->p_type;
            phdr[i]->phdr64->p_flags = origin[i]->phdr64->p_flags;
            phdr[i]->phdr64->p_offset = origin[i]->phdr64->p_offset;
            phdr[i]->phdr64->p_vaddr = origin[i]->phdr64->p_vaddr;
            phdr[i]->phdr64->p_paddr = origin[i]->phdr64->p_paddr;
            phdr[i]->phdr64->p_filesz = origin[i]->phdr64->p_filesz;
            phdr[i]->phdr64->p_memsz = origin[i]->phdr64->p_memsz;
            phdr[i]->phdr64->p_align = origin[i]->phdr64->p_align;
         }
         else
         memset(phdr[i]->phdr64, 0, sizeof (Elf64_Phdr));
      }
      else if (origin[i]->phdr32 != NULL)
      {
         phdr[i]->phdr64 = NULL;
         phdr[i]->phdr32 = lc_malloc (sizeof (Elf32_Phdr));

         if (origin != NULL && i < Elf_Ehdr_get_e_phnum (elf, NULL))
         {
            phdr[i]->phdr32->p_type = origin[i]->phdr32->p_type;
            phdr[i]->phdr32->p_flags = origin[i]->phdr32->p_flags;
            phdr[i]->phdr32->p_offset = origin[i]->phdr32->p_offset;
            phdr[i]->phdr32->p_vaddr = origin[i]->phdr32->p_vaddr;
            phdr[i]->phdr32->p_paddr = origin[i]->phdr32->p_paddr;
            phdr[i]->phdr32->p_filesz = origin[i]->phdr32->p_filesz;
            phdr[i]->phdr32->p_memsz = origin[i]->phdr32->p_memsz;
            phdr[i]->phdr32->p_align = origin[i]->phdr32->p_align;
         }
         else
         memset(phdr[i]->phdr32, 0, sizeof (Elf32_Phdr));
      }
   }
   elf->progheader = phdr;
}

/*
 * Create a new section array
 */
static void ELF_newscns(elffile_t* elf, int size,elffile_t* origin)
{
   if (elf == NULL || (size <= 0 && origin == NULL))
   return;

   int real_size = size;
   if (size == 0)
   real_size = origin->nsections;
   elf->sections = lc_malloc (real_size * sizeof (elfsection_t*));
}

static void ELF_update_shdr64 (elffile_t* elf, int idx, Elf64_Shdr* scnhdr)
{
   if (elf == NULL
         || idx < 0
         || scnhdr == NULL
         || idx > Elf_Ehdr_get_e_shnum (elf, NULL))
   return;
   elf->sections[idx]->scnhdr = lc_malloc (sizeof (Elf_Shdr));
   elf->sections[idx]->scnhdr->scnhdr64 = lc_malloc (sizeof (Elf64_Shdr));
   elf->sections[idx]->scnhdr->scnhdr32 = NULL;
   elf->sections[idx]->scnhdr->scnhdr64->sh_name = scnhdr->sh_name;
   elf->sections[idx]->scnhdr->scnhdr64->sh_type = scnhdr->sh_type;
   elf->sections[idx]->scnhdr->scnhdr64->sh_flags = scnhdr->sh_flags;
   elf->sections[idx]->scnhdr->scnhdr64->sh_addr = scnhdr->sh_addr;
   elf->sections[idx]->scnhdr->scnhdr64->sh_offset = scnhdr->sh_offset;
   elf->sections[idx]->scnhdr->scnhdr64->sh_size = scnhdr->sh_size;
   elf->sections[idx]->scnhdr->scnhdr64->sh_link = scnhdr->sh_link;
   elf->sections[idx]->scnhdr->scnhdr64->sh_info = scnhdr->sh_info;
   elf->sections[idx]->scnhdr->scnhdr64->sh_addralign = scnhdr->sh_addralign;
   elf->sections[idx]->scnhdr->scnhdr64->sh_entsize = scnhdr->sh_entsize;

   //memcpy(elf->sections[idx]->scnhdr, scnhdr, sizeof (Elf64_Shdr));
   elf->sections[idx]->table_entries = NULL;
   elf->sections[idx]->hentries = NULL;
   elf->sections[idx]->nentries = 0;
   elf->sections[idx]->bits = NULL;
}

static void ELF_update_shdr32 (elffile_t* elf, int idx, Elf32_Shdr* scnhdr)
{
   if (elf == NULL
         || idx < 0
         || scnhdr == NULL
         || idx > Elf_Ehdr_get_e_shnum (elf, NULL))
   return;
   elf->sections[idx]->scnhdr = lc_malloc (sizeof (Elf_Shdr));
   elf->sections[idx]->scnhdr->scnhdr32 = lc_malloc (sizeof (Elf32_Shdr));
   elf->sections[idx]->scnhdr->scnhdr64 = NULL;
   elf->sections[idx]->scnhdr->scnhdr32->sh_name = scnhdr->sh_name;
   elf->sections[idx]->scnhdr->scnhdr32->sh_type = scnhdr->sh_type;
   elf->sections[idx]->scnhdr->scnhdr32->sh_flags = scnhdr->sh_flags;
   elf->sections[idx]->scnhdr->scnhdr32->sh_addr = scnhdr->sh_addr;
   elf->sections[idx]->scnhdr->scnhdr32->sh_offset = scnhdr->sh_offset;
   elf->sections[idx]->scnhdr->scnhdr32->sh_size = scnhdr->sh_size;
   elf->sections[idx]->scnhdr->scnhdr32->sh_link = scnhdr->sh_link;
   elf->sections[idx]->scnhdr->scnhdr32->sh_info = scnhdr->sh_info;
   elf->sections[idx]->scnhdr->scnhdr32->sh_addralign = scnhdr->sh_addralign;
   elf->sections[idx]->scnhdr->scnhdr32->sh_entsize = scnhdr->sh_entsize;

   //memcpy(elf->sections[idx]->scnhdr, scnhdr, sizeof (Elf64_Shdr));   
   elf->sections[idx]->table_entries = NULL;
   elf->sections[idx]->hentries = NULL;
   elf->sections[idx]->nentries = 0;
   elf->sections[idx]->bits = NULL;
}

static void ELF_update_shdr (elffile_t* elf, int idx, Elf_Shdr* scnhdr)
{
   if (elf->elfheader->ehdr64 != NULL)
   ELF_update_shdr64 (elf, idx, scnhdr->scnhdr64);
   else if (elf->elfheader->ehdr32 != NULL)
   ELF_update_shdr32 (elf, idx, scnhdr->scnhdr32);
}

/*
 * Returns bits for the given section
 * \param efile The structure containing the parsed ELF file
 * \param indx index of the section
 * \return an array of bytes or NULL if \c efile is NULL or \c indx is outside of its range of section indexes
 */
unsigned char* ELF_get_scn_bits (elffile_t* efile, int indx)
{
   return ((efile != NULL) && (indx >= 0) && (indx < efile->nsections)) ? (efile->sections[indx]->bits) : NULL;
}

/*
 * Parses bytes to extract Elf64_Sym structures
 * \param efile The structure containing the parsed ELF file
 * \param indx index of the section to parse
 * \param size return param containing the size of the returned table
 * \return an array of Elf64_Sym* or NULL if the file has no header
 */
static Elf64_Sym** ELF_get_sym_64 (elffile_t* efile, int indx, int* size)
{
   unsigned char* bytes = ELF_get_scn_bits (efile, indx);
   Elf64_Shdr* header = efile->sections[indx]->scnhdr->scnhdr64;
   if(!header)
   return NULL;
   int lsize = header->sh_size / sizeof (Elf64_Sym);
   if(size)
   *size = lsize;
   Elf64_Sym** syms = lc_malloc (lsize * sizeof (Elf64_Sym*));
   int i = 0;
   int pos = 0;

   for (i = 0; i < lsize; i++)
   {
      syms[i] = lc_malloc (sizeof (Elf64_Sym));
      memcpy (syms[i], &bytes[pos], sizeof (Elf64_Sym));
      pos += sizeof (Elf64_Sym);
   }
   return (syms);
}

/*
 * Parses bytes to extract Elf32_Sym structures
 * \param efile The structure containing the parsed ELF file
 * \param indx index of the section to parse
 * \param size return param containing the size of the returned table
 * \return an array of Elf32_Sym* or NULL if the file has no header
 */
static Elf32_Sym** ELF_get_sym_32 (elffile_t* efile, int indx, int* size)
{
   unsigned char* bytes = ELF_get_scn_bits (efile, indx);
   Elf32_Shdr* header = efile->sections[indx]->scnhdr->scnhdr32;
   if(!header)
   return NULL;
   int lsize = header->sh_size / sizeof (Elf32_Sym);
   if(size)
   *size = lsize;
   Elf32_Sym** syms = lc_malloc (lsize * sizeof (Elf32_Sym*));
   int i = 0;
   int pos = 0;

   for (i = 0; i < lsize; i++)
   {
      syms[i] = lc_malloc (sizeof (Elf32_Sym));
      memcpy (syms[i], &bytes[pos], sizeof (Elf32_Sym));
      pos += sizeof (Elf32_Sym);
   }
   return (syms);
}

/*
 * Parses bytes to extract Elfxx_Sym structures
 * \param efile The structure containing the parsed ELF file
 * \param indx index of the section to parse
 * \param size return param containing the size of the returned table
 * \return an array of Elfxx_Sym* or NULL if the file has no header
 */
static void* ELF_get_sym (elffile_t* efile, int indx, int* size)
{
   if (efile->elfheader->ehdr64 != NULL)
   return (ELF_get_sym_64 (efile, indx, size));
   else if (efile->elfheader->ehdr32 != NULL)
   return (ELF_get_sym_32 (efile, indx, size));
   else
   return (NULL);
}
return (syms);
}

/*
 * Parses bytes to extract Elf64_Rel structures
 * \param efile The structure containing the parsed ELF file
 * \param indx index of the section to parse
 * \param size return param containing the size of the returned table
 * \return an array of Elf64_Rel*
 */
static Elf64_Rel** ELF_get_rel_64 (elffile_t* efile, int indx, int* size)
{
unsigned char* bytes = ELF_get_scn_bits (efile, indx);
Elf64_Shdr* header = efile->sections[indx]->scnhdr->scnhdr64;
if(!header)
return NULL;
int lsize = header->sh_size / sizeof (Elf64_Rel);
if(*size) *size = lsize;
Elf64_Rel** rels = lc_malloc (lsize * sizeof (Elf64_Rel*));
int i = 0;
int pos = 0;

for (i = 0; i < lsize; i++)
{
   rels[i] = lc_malloc (sizeof (Elf64_Rel));
   memcpy (rels[i], &bytes[pos], sizeof (Elf64_Rel));
   pos += sizeof (Elf64_Rel);
}
return (rels);
}

/*
 * Parses bytes to extract Elf32_Rel structures
 * \param efile The structure containing the parsed ELF file
 * \param indx index of the section to parse
 * \param size return param containing the size of the returned table
 * \return an array of Elf32_Rel*
 */
static Elf32_Rel** ELF_get_rel_32 (elffile_t* efile, int indx, int* size)
{
unsigned char* bytes = ELF_get_scn_bits (efile, indx);
Elf32_Shdr* header = efile->sections[indx]->scnhdr->scnhdr32;
if(!header)
return NULL;
int lsize = header->sh_size / sizeof (Elf32_Rel);
if(*size) *size = lsize;
Elf32_Rel** rels = lc_malloc (lsize * sizeof (Elf32_Rel*));
int i = 0;
int pos = 0;

for (i = 0; i < lsize; i++)
{
   rels[i] = lc_malloc (sizeof (Elf32_Rel));
   memcpy (rels[i], &bytes[pos], sizeof (Elf32_Rel));
   pos += sizeof (Elf32_Rel);
}
return (rels);
}

/*
 * Parses bytes to extract Elfxx_Rel structures
 * \param efile The structure containing the parsed ELF file
 * \param indx index of the section to parse
 * \param size return param containing the size of the returned table
 * \return an array of Elfxx_Rel*
 */
static void* ELF_get_rel (elffile_t* efile, int indx, int* size)
{
if (efile->elfheader->ehdr64 != NULL)
return (ELF_get_rel_64 (efile, indx, size));
else if (efile->elfheader->ehdr32 != NULL)
return (ELF_get_rel_32 (efile, indx, size));
else
return (NULL);
}

/*
 * Parses bytes to extract Elf64_Rela structures
 * \param efile The structure containing the parsed ELF file
 * \param indx index of the section to parse
 * \param size return param containing the size of the returned table
 * \return an array of Elf64_Rela*
 */
static Elf64_Rela** ELF_get_rela_64 (elffile_t* efile, int indx, int* size)
{
unsigned char* bytes = ELF_get_scn_bits (efile, indx);
Elf64_Shdr* header = efile->sections[indx]->scnhdr->scnhdr64;
int lsize = header->sh_size / sizeof (Elf64_Rela);
if (size) *size = lsize;
Elf64_Rela** rels = lc_malloc (lsize * sizeof (Elf64_Rela*));
int i = 0;
int pos = 0;

for (i = 0; i < lsize; i++)
{
   rels[i] = lc_malloc (sizeof (Elf64_Rela));
   memcpy (rels[i], &bytes[pos], sizeof (Elf64_Rela));
   pos += sizeof (Elf64_Rela);
}
return (rels);
}

/*
 * Parses bytes to extract Elf32_Rela structures
 * \param efile The structure containing the parsed ELF file
 * \param indx index of the section to parse
 * \param size return param containing the size of the returned table
 * \return an array of Elf32_Rela*
 */
static Elf32_Rela** ELF_get_rela_32 (elffile_t* efile, int indx, int* size)
{
unsigned char* bytes = ELF_get_scn_bits (efile, indx);
Elf32_Shdr* header = efile->sections[indx]->scnhdr->scnhdr32;
int lsize = header->sh_size / sizeof (Elf64_Rela);
if (size) *size = lsize;
Elf32_Rela** rels = lc_malloc (lsize * sizeof (Elf32_Rela*));
int i = 0;
int pos = 0;

for (i = 0; i < lsize; i++)
{
   rels[i] = lc_malloc (sizeof (Elf32_Rela));
   memcpy (rels[i], &bytes[pos], sizeof (Elf32_Rela));
   pos += sizeof (Elf32_Rela);
}
return (rels);
}

/*
 * Parses bytes to extract Elfxx_Rela structures
 * \param efile The structure containing the parsed ELF file
 * \param indx index of the section to parse
 * \param size return param containing the size of the returned table
 * \return an array of Elfxx_Rela*
 */
static void* ELF_get_rela (elffile_t* efile, int indx, int* size)
{
if (efile->elfheader->ehdr64 != NULL)
return (ELF_get_rela_64 (efile, indx, size));
else if (efile->elfheader->ehdr32 != NULL)
return (ELF_get_rela_32 (efile, indx, size));
else
return (NULL);
}

/*
 * Parses bytes to extract Elf64_Dyn structures
 * \param efile The structure containing the parsed ELF file
 * \param indx index of the section to parse
 * \param size return param containing the size of the returned table
 * \return an array of Elf64_Dyn*
 */
static Elf64_Dyn** ELF_get_dyn_64 (elffile_t* efile, int indx, int* size)
{
unsigned char* bytes = ELF_get_scn_bits (efile, indx);
Elf64_Shdr* header = efile->sections[indx]->scnhdr->scnhdr64;
int lsize = header->sh_size / sizeof (Elf64_Dyn);
if(size) *size = lsize;
Elf64_Dyn** dyns = lc_malloc (lsize * sizeof (Elf64_Dyn*));
int i = 0;
int pos = 0;

for (i = 0; i < lsize; i++)
{
   dyns[i] = lc_malloc (sizeof (Elf64_Dyn));
   memcpy (dyns[i], &bytes[pos], sizeof (Elf64_Dyn));
   pos += sizeof (Elf64_Dyn);
}
return (dyns);
}

/*
 * Parses bytes to extract Elf32_Dyn structures
 * \param efile The structure containing the parsed ELF file
 * \param indx index of the section to parse
 * \param size return param containing the size of the returned table
 * \return an array of Elf32_Dyn*
 */
static Elf32_Dyn** ELF_get_dyn_32 (elffile_t* efile, int indx, int* size)
{
unsigned char* bytes = ELF_get_scn_bits (efile, indx);
Elf32_Shdr* header = efile->sections[indx]->scnhdr->scnhdr32;
int lsize = header->sh_size / sizeof (Elf32_Dyn);
if(size) *size = lsize;
Elf32_Dyn** dyns = lc_malloc (lsize * sizeof (Elf32_Dyn*));
int i = 0;
int pos = 0;

for (i = 0; i < lsize; i++)
{
   dyns[i] = lc_malloc (sizeof (Elf32_Dyn));
   memcpy (dyns[i], &bytes[pos], sizeof (Elf32_Dyn));
   pos += sizeof (Elf32_Dyn);
}
return (dyns);
}

/*
 * Parses bytes to extract Elfxx_Dyn structures
 * \param efile The structure containing the parsed ELF file
 * \param indx index of the section to parse
 * \param size return param containing the size of the returned table
 * \return an array of Elfxx_Dyn*
 */
static void* ELF_get_dyn (elffile_t* efile, int indx, int* size)
{
if (efile->elfheader->ehdr64 != NULL)
return (ELF_get_dyn_64 (efile, indx, size));
else if (efile->elfheader->ehdr32 != NULL)
return (ELF_get_dyn_32 (efile, indx, size));
else
return (NULL);
}

/*
 * Parses bytes to extract Elf64_Dyn structures
 * \param efile The structure containing the parsed ELF file
 * \param indx index of the section to parse
 * \param size return param containing the size of the returned table
 * \return an array of Elf64_Dyn*
 */
static void* ELF_get_addr (elffile_t* efile, int indx, int* size)
{
unsigned char* bytes = ELF_get_scn_bits (efile, indx);
Elf_Shdr* header = efile->sections[indx]->scnhdr;
if(header && size)
{
   if (header->scnhdr64 != NULL)
   *size = Elf_Shdr_get_sh_size (efile, 0, header) / sizeof (Elf64_Addr);
   else if (header->scnhdr32 != NULL)
   *size = Elf_Shdr_get_sh_size (efile, 0, header) / sizeof (Elf32_Addr);
}
return (bytes);
}

/*
 * Returns a string located in section indx at offset pos
 * \param efile a structure representing an ELF file
 * \param indx index of the section containing the string
 * \param pos offset of the string in the section
 */
char* ELF_strptr(elffile_t* efile, int indx, int pos)
{
unsigned char* bytes = ELF_get_scn_bits (efile, indx);
return ( (bytes) && (pos >= 0) )?((char*) &bytes[pos]):NULL;
}

/**
 * Updates the address pointed to by an element in a symbol table
 * \param s Elf structure corresponding to the symbol element
 * \param addr Address pointed to by the symbol
 * \param e Structure containing the parsed ELF file the symbol belongs to
 * \param scnidx Index of the section (symbol table) containing the symbol element
 * \param tblidx Index of the symbol element in the symbol table
 */
static void GElf_Sym_updaddr (void* s, int64_t addr, void* e, int scnidx, int tblidx)
{
if (s == NULL)
return;
elffile_t* ef = (elffile_t*)e;
Elf64_Sym* sym = (Elf64_Sym*)s;
DBGMSGLVL(2,"Updating sym entry from %#"PRIx64" to %#"PRIx64" in %p=%d[%d]\n",sym->st_value,addr,sym,scnidx,tblidx);
sym->st_value = addr;
/*Updates the underlying data*/
ELF_update_sym (ef, scnidx, tblidx, sym);
}

/**
 * Updates the address pointed to by an element in a relocation table
 * \param r Elf structure corresponding to the relocation element
 * \param addr Address pointed to by the relocation
 * \param e Structure containing the parsed ELF file the relocation belongs to
 * \param scnidx Index of the section (relocation table) containing the relocation element
 * \param tblidx Index of the relocation element in the relocation table
 */
static void GElf_Rel_updaddr (void* r, int64_t addr, void* e, int scnidx, int tblidx)
{
if (r == NULL)
return;
Elf64_Rel* rel = (Elf64_Rel*)r;
rel->r_offset = addr;
/*Updates the underlying data*/
elffile_t* ef = (elffile_t*)e;
ELF_update_rel (ef, scnidx, tblidx, rel);
}

/**
 * Updates the address pointed to by an element in a relocation table
 * \param r Elf structure corresponding to the relocation element
 * \param addr Address pointed to by the relocation
 * \param e Structure containing the parsed ELF file the relocation belongs to
 * \param scnidx Index of the section (relocation table) containing the relocation element
 * \param tblidx Index of the relocation element in the relocation table
 * */
static void GElf_Rela_updaddr(void* r, int64_t addr, void* e, int scnidx, int tblidx)
{
if(r == NULL)
return;
Elf64_Rela* rela = (Elf64_Rela*)r;

rela->r_offset=addr;
/*Updates the underlying data*/
elffile_t* ef = (elffile_t*)e;
ELF_update_rela (ef, scnidx, tblidx, rela);
}

/**
 * Updates the address pointed to by an element in a dynamic table
 * \param d Elf structure corresponding to the dynamic table element
 * \param addr Address pointed to by the dynamic table element
 * \param e Structure containing the parsed ELF file the element belongs to
 * \param scnidx Index of the section (dynamic table) containing the element
 * \param tblidx Index of the dynamic table element in the dynamic table
 * */
static void GElf_Dyn_updaddr(void* d,int64_t addr,void* e,int scnidx,int tblidx) {
if (d == NULL)
return;
Elf64_Dyn* dyn=(Elf64_Dyn*)d;
dyn->d_un.d_ptr=addr;
/*Updates the underlying data*/
elffile_t* ef = (elffile_t*)e;
ELF_update_dyn64 (ef, scnidx, tblidx, dyn);
}

/**
 * Updates an address in an array of addresses
 * \param a Pointer to the address to update
 * \param addr New value of the address
 * \param e Unused
 * \param scnidx Unused
 * \param tblidx Unused
 * */
static void GElf_Addr_updaddr(void* a,int64_t addr,void* e,int scnidx,int tblidx)
{
if (a == NULL)
return;
Elf64_Addr *ad = (Elf64_Addr*)a;
*ad = addr;
elffile_t* ef = (elffile_t*)e;
ELF_update_addr (ef, scnidx, tblidx, *ad);
}

/**
 * Updates the addresses in the target hashtable (to be called with hashtable_foreach)
 * \param k Pointer to the Elf structure to update (key in the hashtable)
 * \param v Pointer to the targetaddr_t object containing the target pointed to by
 * the structure
 * \param u Pointer to the parser ELF file structure containing all of this*/
static void targets_updaddr(void* k,void* v,void *u)
{
if ((k == NULL) || (v == NULL) || (u == NULL))
return;
targetaddr_t* ta = (targetaddr_t*)v;
elffile_t* ef = (elffile_t*)u;
int64_t newaddr;
int64_t sh_addr = Elf_Shdr_get_sh_addr (ef, ta->scnindx, NULL);
newaddr = sh_addr + ta->offs;
DBGMSGLVL(2,"Object %p (in %d[%d]) will be updated to point to %#"PRIx64"\n", k, ta->srcscnidx, ta->srctblidx, newaddr);
if (ta->updaddr)
ta->updaddr(k, newaddr, ef, ta->srcscnidx, ta->srctblidx);
}

/*
 * Returns the name associated to symtable entry
 * \param efile Structure containing the parsed ELF file
 * \param scnidx Index of the section containing the symbol table
 * \param symentidx Index of the symtable entry in the symbol table
 * \return A pointer to a string in the ELF file containing the name of the entry,
 * or NULL if the index of the section containing the symbol table is 0 (no symbol table)*/
static char* symentry_name(elffile_t* efile,int scnidx,int symentidx)
{
if( (scnidx > 0) && efile )
{
   tableentry_t* te = efile->sections[scnidx]->table_entries[symentidx];
   int st_name = 0;
   if (te->entry_64 != NULL)
   st_name = te->entry_64->elfentry.sym->st_name;
   else if (te->entry_32 != NULL)
   st_name = te->entry_32->elfentry.sym->st_name;

   return ELF_strptr(efile, Elf_Shdr_get_sh_link (efile, scnidx, NULL), st_name);
}
return NULL;
}

/*
 * Returns the name of a section
 * \param efile Structure containing the parsed ELF file
 * \param scnidx Index of the section
 * \return A pointer to a string in the ELF file containing the name of the section or
 *       NULL if the index was out of range
 */
char* elfsection_name (elffile_t* efile, int scnidx)
{
char* out = NULL;
int sh_name = Elf_Shdr_get_sh_name (efile, scnidx, NULL);

if (efile && (scnidx >= 0)&&(scnidx < efile->nsections))
out = ELF_strptr(efile, Elf_Ehdr_get_e_shstrndx (efile, NULL), sh_name);
return out;
}

/*
 * Returns the index of the section starting at the given address
 * \param ef Structure containing the parsed ELF file
 * \param addr Address
 * \return Index of the section in the file or -1 if no section found
 * */
int elffile_get_scnataddr(elffile_t* ef, int64_t addr)
{
int indx = 0;
if (!ef)
return -1;
while (indx < ef->nsections)
{
   if (Elf_Shdr_get_sh_addr (ef, indx, NULL) == (uint64_t) addr)
   break;
   indx++;
}
if (indx == ef->nsections)
indx=-1;
return indx;
}

/*
 * Returns the index of the section containing prog bits starting at the given address
 * \param ef Structure containing the parsed ELF file
 * \param addr Address
 * \return Index of the section in the file or -1 if no section found
 */
int elffile_get_progscnataddr(elffile_t* ef, int64_t addr)
{
/**\todo Check if this function and elffile_get_scnataddr are both needed. Maybe we don't need elffile_get_scnataddr*/
int indx = 0;
if (!ef)
return -1;
while (indx < ef->nsections)
{
   if (Elf_Shdr_get_sh_addr (ef, indx, NULL) == (uint64_t) addr
         && elffile_scnisprog(ef, indx) == TRUE)
   break;
   indx++;
}
if (indx == ef->nsections)
indx=-1;
return indx;
}

/*
 * Returns the starting address of an elf section
 * \param efile Structure containing the parsed ELF file
 * \param scnidx Index of the section in the ELF file
 * \return Starting address of the section or -1 if the section is not found
 * (section index out of bounds)
 * */
int64_t elffile_getscnstartaddr(elffile_t *efile, int scnidx)
{
int64_t addr = -1;
if (efile && (scnidx >= 0) && (scnidx < efile->nsections))
addr = Elf_Shdr_get_sh_addr (efile, scnidx, NULL);
return addr;
}

/*
 * Returns the size of an elf section
 * \param efile Structure containing the parsed ELF file
 * \param scnidx Index of the section in the ELF file
 * \return Size of the section or -1 if the section is not found
 * (section index out of bounds)
 * */
int64_t elffile_getscnsize(elffile_t* efile, int scnidx)
{
int64_t size = -1;
if (efile && (scnidx >= 0) && (scnidx < efile->nsections))
size = Elf_Shdr_get_sh_size (efile, scnidx, NULL);
return size;
}

/**
 * Frees the elfentry content of the tableentry
 * \param entry Pointer to the tableentry to free
 * \bug That function can cause memory errors when used if entries were added
 */
static void tableentry_free_elfentry_64(tableentry_64_t* entry)
{
if(!entry)
return;
if (entry->updated == FALSE)
{
   switch(entry->type)
   {
      case SYMTABLE:
      lc_free(entry->elfentry.sym);
      break;
      case RELTABLE:
      lc_free(entry->elfentry.rel);
      break;
      case RELATABLE:
      lc_free(entry->elfentry.rela);
      break;
      case DYNTABLE:
      lc_free(entry->elfentry.dyn);
      break;
   }
}
if (entry->name != NULL)
{
   lc_free (entry->name);
   entry->name = NULL;
}
lc_free (entry);
}

/**
 * Frees the elfentry content of the tableentry
 * \param entry Pointer to the tableentry to free
 * \bug That function can cause memory errors when used if entries were added
 */
static void tableentry_free_elfentry_32(tableentry_32_t* entry)
{
if(!entry)
return;
if (entry->updated == FALSE)
{
   switch(entry->type)
   {
      case SYMTABLE:
      lc_free(entry->elfentry.sym);
      break;
      case RELTABLE:
      lc_free(entry->elfentry.rel);
      break;
      case RELATABLE:
      lc_free(entry->elfentry.rela);
      break;
      case DYNTABLE:
      lc_free(entry->elfentry.dyn);
      break;
   }
}
if (entry->name != NULL)
{
   lc_free (entry->name);
   entry->name = NULL;
}
lc_free (entry);
}

/**
 * Frees the elfentry content of the tableentry
 * \param entry Pointer to the tableentry to free
 * \bug That function can cause memory errors when used if entries were added
 */
static void tableentry_free_elfentry(tableentry_t* entry)
{
if(!entry)
return;
if (entry->entry_64 != NULL)
tableentry_free_elfentry_64(entry->entry_64);
else if (entry->entry_32 != NULL)
tableentry_free_elfentry_32(entry->entry_32);
}

/**
 * Applies a relocation for the x86_64 architecture
 * \param relo The structure containing the details of the relocation to apply. Must not be NULL and must not contain NULL pointers to files
 * \return EXIT_SUCCESS if the relocation could be applied, error code otherwise
 * \note This could be later on exported to a different, architecture-specific, file
 * */
static int x86_64_elfreloc_apply(elfreloc_t* relo)
if ( relo->efile == NULL )
return ERR_BINARY_MISSING_BINFILE;
if ( relo->reloscn == NULL || relo->scn == NULL )
return ERR_BINARY_MISSING_SECTION;

int out = EXIT_SUCCESS;

/*&& ( relo->scnid < relo->efile->nsections ) && ( relo->reloscnid < relo->relofile->nsections )
 && ( relo->scnid >= 0 ) && ( relo->reloscnid >= 0 ) )*/

 // HANDLE ABS CASE && WEAK SYMBOL : No need to apply relocations.
if (relo->scn == NULL && relo->relooffset == -1)
{
return EXIT_SUCCESS;
}
elfsection_t* scn = relo->scn; //relo->efile->sections[relo->scnid];
elfsection_t* reloscn = relo->reloscn;//relo->relofile->sections[relo->reloscnid];

void* relodst = scn->bits + relo->offset;
Elf64_Addr S = Elf_Shdr_get_sh_addr (NULL, 0, reloscn->scnhdr) + relo->relooffset;//Where the relocation points (Addr to reach)
Elf64_Addr P = Elf_Shdr_get_sh_addr (NULL, 0, scn->scnhdr) + relo->offset;//Place of the relocation (Addr of bits to modify)
int64_t A = relo->reloaddend;//The Addend : Relocation's place depends of the operand's size
short indexPLT = relo->idxPlt;//Index of the .PLT section in the file
Elf64_Addr PLT = 0;// It could be .madras.plt OR the original .plt section
if (relo->idxPlt != -1)
PLT = Elf_Shdr_get_sh_addr (relo->efile, indexPLT, NULL);//Address of the .PLT section
Elf64_Addr L = PLT + relo->reloPltOffset*0x10;//Address to call in the .PLT (WARNING: 0x10 == one entry size in PLT(x86)

short indexGOT = relo->efile->indexes[GOT_IDX];//Index of the .GOT section in the file.
Elf64_Addr GOT = Elf_Shdr_get_sh_addr (relo->efile, indexGOT, NULL);//Address of the .GOT section
int G = relo->reloGotIdx * sizeof(Elf64_Addr);//Offset of the variable in the .GOT section(One entry = 64-bit)

DBGMSG("S= %#"PRIx64"\n"
   "(sh_addr = %#"PRIx64")\n"
   "(relooffset = %#"PRIx64")\n"
   "P= %#"PRIx64"\n"
   "A= %"PRId64"\n"
   "L= %#"PRIx64"\n"
   "GOT= %#"PRIx64"\n"
   "G= %d\n",S,Elf_Shdr_get_sh_addr (NULL, 0, reloscn->scnhdr),(relo->relooffset),P,A,L,GOT,G);
//ERRMSG("Unsupported relocation type in file %s at address %#"PRIx64" (type %d not implemented yet)\n",
//relo->efile->filename, scn->scnhdr->sh_addr + relo->offset, relo->relotype );

// Il faut calculer l'adresse de destination en prenant l'adresse de début de la reloscn + relooffset,
// plus l'addend (voir les formules dans la spec ELF pour les variantes) puis copier ça à la longueur
// indiquée dans le data de la scn à l'offset offset
switch(relo->relotype)
{
Elf64_Addr *addr64;
Elf32_Addr *addr32;
case R_X86_64_NONE: /* No reloc */
break;
case R_X86_64_64: /* Direct 64 bit  */
//S+A
addr64 = (Elf64_Addr*)relodst;
*addr64 = S + A;
DBGMSG("RELOCATION <R_X86_64_64> (S+A): RELOC @%#"PRIx64" NOW CONTAINS %#"PRIx64" [%#"PRIx64"]\n", P,*addr64,relo->offset);
break;
case R_X86_64_PC32: /* PC relative 32 bit signed */
//S+A-P
addr32 = (Elf32_Addr*)relodst;
*addr32 = S + A - P;
DBGMSG("RELOCATION <R_X86_64_PC32> (S+A-P): RELOC @%#"PRIx64" NOW CONTAINS %#"PRIx32" [%#"PRIx64"]\n", P,*addr32,relo->offset);
break;
case R_X86_64_GOT32: /* 32 bit GOT entry */
//G+A
addr32 = (Elf32_Addr*)relodst;
*addr32 = G + A;
DBGMSG("RELOCATION <GOT32> (G+A): RELOC @%#"PRIx64" NOW CONTAINS %#"PRIx32" [%#"PRIx64"]\n", P,*addr32,relo->offset);
break;
case R_X86_64_PLT32: /* 32 bit PLT address */
//L+A-P FOR IFUNC_GNU
//TEST S+A-P ON PLT32 RELOCATION (--> SEEMS TO BE THE REAL RELOCATION USED WITH -static)
addr32 = (Elf32_Addr*)relodst;
//if (relo->reloPltIdx == -1)
if (relo->reloPltOffset == -1)
{
   *addr32 = S+A-P;
   DBGMSG("RELOCATION <PLT32> /!!IN TEST: S+A-P!!/: RELOC @%#"PRIx64" NOW CONTAINS %#"PRIx32" [%#"PRIx64"]\n", P,*addr32,relo->offset);
}
else
{
   *addr32 = L+A-P;
   DBGMSG("RELOCATION <PLT32> (L+A-P: ONLY FOR IFUNC_GNU): RELOC @%#"PRIx64" NOW CONTAINS %#"PRIx32" [%#"PRIx64"]\n", P,*addr32,relo->offset);
}
break;
case R_X86_64_32: /* Direct 32 bit zero extended */
//S+A
addr32 = (Elf32_Addr*)relodst;
*addr32 = S + A;
DBGMSG("RELOCATION <R_X86_64_32> (S+A): RELOC @%#"PRIx64" NOW CONTAINS %#"PRIx32" [%#"PRIx64"]\n", P,*addr32,relo->offset);
DBGMSGLVL(2,"Applying R_X86_64_32 relocation: address %#"PRIx64" now contains value %#"PRIx32"\n",
      Elf_Shdr_get_sh_addr (NULL, 0, scn->scnhdr) + relo->offset,
      (Elf32_Addr)(Elf_Shdr_get_sh_addr (NULL, 0, reloscn->scnhdr) + relo->relooffset + relo->reloaddend));
out = EXIT_SUCCESS;
break;
case R_X86_64_32S: /* Direct 32 bit sign extended */
//S+A
addr32 = (Elf32_Addr*)relodst;
*addr32 = S + A;
DBGMSG("RELOCATION <R_X86_64_32S> (S+A): RELOC @%#"PRIx64" NOW CONTAINS %#"PRIx32" [%#"PRIx64"]\n", P,*addr32,relo->offset);
out = EXIT_SUCCESS;
break;
case R_X86_64_PC64: /* PC relative 64 bit */
//S+A-P
addr64 = (Elf64_Addr*)relodst;
*addr64 = S + A - P;
DBGMSG("RELOCATION <PC64> (S+A-P): RELOC @%#"PRIx64" NOW CONTAINS %#"PRIx64" [%#"PRIx64"]\n", P,*addr64,relo->offset);
break;
/*I have not yet figured out what those relocations are supposed to do exactly and I'm a lazy guy*/
/**\todo Implement the types among those that are most frequently used*/
case R_X86_64_COPY: /* Copy symbol at runtime */
DBGMSG("RELOCATION :COPY: RELOC @%#"PRIx64"\n", P);
ERRMSG("Unsupported relocation type in file %s at address %#"PRIx64" (type %d not implemented yet)\n",
      relo->efile->filename, Elf_Shdr_get_sh_addr (NULL, 0, scn->scnhdr) + relo->offset, relo->relotype );
out = ERR_BINARY_RELOCATION_NOT_SUPPORTED;
break;
case R_X86_64_GLOB_DAT: /* Create GOT entry */
//S
DBGMSG("RELOCATION :GLOB_DAT: RELOC @%#"PRIx64"\n", P);
addr64 = (Elf64_Addr*)relodst;
*addr64 = S;
DBGMSG("RELOCATION <GLOB_DAT> (S): RELOC @%#"PRIx64" NOW CONTAINS %#"PRIx64" [%#"PRIx64"]\n", P,*addr64,relo->offset);
break;
case R_X86_64_JUMP_SLOT: /* Create PLT entry */
//S
DBGMSG("RELOCATION :JUMP_SLOT: RELOC @%#"PRIx64"\n", P);
addr64 = (Elf64_Addr*)relodst;
*addr64 = S;
DBGMSG("RELOCATION <JUMP_SLOT> (S): RELOC @%#"PRIx64" NOW CONTAINS %#"PRIx64" [%#"PRIx64"]\n", P,*addr64,relo->offset);
break;
case R_X86_64_RELATIVE: /* Adjust by program base */
//B+A
DBGMSG("RELOCATION :RELATIVE: RELOC @%#"PRIx64"\n", P);
ERRMSG("Unsupported relocation type in file %s at address %#"PRIx64" (type %d not implemented yet)\n",
      relo->efile->filename, Elf_Shdr_get_sh_addr (NULL, 0, scn->scnhdr) + relo->offset, relo->relotype );
out = ERR_BINARY_RELOCATION_NOT_SUPPORTED;
break;
case R_X86_64_GOTPCREL: /* 32 bit signed PC relative*/
         /**\todo (2016-09-07) Handle the two following relocation correctly (see latest version of ABI). They are used for
          * special relocation handling low or high memory, if I understood correctly, and specify that the instruction must
          * be modified depending on the value of the relocated address.*/
      case R_X86_64_GOTPCRELX: /* 32 bit signed PC relative offset to GOT without REX prefix, relaxable*/
      case R_X86_64_REX_GOTPCRELX:  /* 32 bit signed PC relative offset to GOT with REX prefix, relaxable*/
//G + GOT + A - P
addr32 = (Elf32_Addr*)relodst;
*addr32= G+GOT+A-P;
DBGMSG("RELOCATION <GOTPCREL> (G+GOT+A-P): RELOC @%#"PRIx64" NOW CONTAINS %#"PRIx32" [%#"PRIx64"]\n", P,*addr32,relo->offset);
break;
case R_X86_64_GOTOFF64: /* 64 bit offset to GOT */
//S + A - GOT
addr64 = (Elf64_Addr*)relodst;
*addr64 = S+A-GOT;
DBGMSG("RELOCATION <GOTOFF64> (S+A-GOT): RELOC @%#"PRIx64" NOW CONTAINS %#"PRIx64" [%#"PRIx64"]\n", P,*addr64,relo->offset);
break;
case R_X86_64_GOTPC32: /* 32 bit signed pc relative*/
//GOT + A - P
STDMSG ("RELOCATION :GOTPC32: RELOC @%#"PRIx64"\n", P);
addr32 = (Elf32_Addr*)relodst;
*addr32= GOT+A-P;
DBGMSG("RELOCATION <GOTPC32> (GOT+A-P): RELOC @%#"PRIx64" NOW CONTAINS %#"PRIx32" [%#"PRIx64"]\n", P,*addr32,relo->offset);
break;
case R_X86_64_GOT64: /* 64-bit GOT entry offset */
//G+A
addr64 = (Elf64_Addr*)relodst;
*addr64 = G+A;
DBGMSG("RELOCATION <GOT64> (G+A): RELOC @%#"PRIx64" NOW CONTAINS %#"PRIx64" [%#"PRIx64"]\n", P,*addr64,relo->offset);
break;
case R_X86_64_GOTPCREL64: /* 64-bit PC relative offset to GOT entry */
//G+GOT-P+A
addr64 = (Elf64_Addr*)relodst;
*addr64 = G+GOT-P+A;
DBGMSG("RELOCATION <GOTPCREL64> (G+GOT-P+A): RELOC @%#"PRIx64" NOW CONTAINS %#"PRIx64" [%#"PRIx64"]\n", P,*addr64,relo->offset);
break;
case R_X86_64_GOTPC64: /* 64-bit PC relative offset to GOT */
//GOT-P+A
addr64 = (Elf64_Addr*)relodst;
*addr64 = GOT-P+A;
DBGMSG("RELOCATION <GOTPC64> (GOT-P+A): RELOC @%#"PRIx64" NOW CONTAINS %#"PRIx64" [%#"PRIx64"]\n", P,*addr64,relo->offset);
break;
case R_X86_64_GOTPLT64: /* like GOT64, says PLT entry needed */
//G+A
addr64 = (Elf64_Addr*)relodst;
*addr64 = G+A;
DBGMSG("RELOCATION <GOTPLT64> (G+A): RELOC @%#"PRIx64" NOW CONTAINS %#"PRIx64" [%#"PRIx64"]\n", P,*addr64,relo->offset);
break;
case R_X86_64_PLTOFF64: /* 64-bit GOT relative offset to PLT entry */
//L-GOT+A
addr64 = (Elf64_Addr*)relodst;
*addr64 = L-GOT+A;
DBGMSG("RELOCATION <PLTOFF64> (L-GOT-A): RELOC @%#"PRIx64" NOW CONTAINS %#"PRIx64" [%#"PRIx64"]\n", P,*addr64,relo->offset);
break;
case R_X86_64_SIZE32: /* Size of symbol plus 32-bit addend */
DBGMSG("RELOCATION :SIZE32: RELOC @%#"PRIx64"\n", P);
ERRMSG("Unsupported relocation type in file %s at address %#"PRIx64" (type %d not implemented yet)\n",
      relo->efile->filename, Elf_Shdr_get_sh_addr (NULL, 0, scn->scnhdr) + relo->offset, relo->relotype );
out = ERR_BINARY_RELOCATION_NOT_SUPPORTED;
break;
case R_X86_64_SIZE64: /* Size of symbol plus 64-bit addend */
DBGMSG("RELOCATION :SIZE64: RELOC @%#"PRIx64"\n", P);
ERRMSG("Unsupported relocation type in file %s at address %#"PRIx64" (type %d not implemented yet)\n",
      relo->efile->filename, Elf_Shdr_get_sh_addr (NULL, 0, scn->scnhdr) + relo->offset, relo->relotype );
out = ERR_BINARY_RELOCATION_NOT_SUPPORTED;
break;
case R_X86_64_IRELATIVE: /* Adjust indirectly by program base */
DBGMSGLVL(1,"RELOCATION :IRELATIVE: RELOC @%#"PRIx64"\n", P);
ERRMSG("Unsupported relocation type in file %s at address %#"PRIx64" (type %d not implemented yet)\n",
      relo->efile->filename, Elf_Shdr_get_sh_addr (NULL, 0, scn->scnhdr) + relo->offset, relo->relotype );
out = ERR_BINARY_RELOCATION_NOT_SUPPORTED;
break;

/*According to the ABI those types don't exist in x86_64*/
case R_X86_64_8: /* Direct 8 bit sign extended  */
case R_X86_64_16: /* Direct 16 bit zero extended */
case R_X86_64_PC8: /* 8 bit sign extended pc relative */
case R_X86_64_PC16: /* 16 bit sign extended pc relative */
ERRMSG("Unsupported relocation type in file %s at address %#"PRIx64" (relocation type %d should not exist)\n",
      relo->efile->filename, Elf_Shdr_get_sh_addr (NULL, 0, scn->scnhdr) + relo->offset, relo->relotype );
out = ERR_BINARY_RELOCATION_INVALID;
break;
/*TLS specific relocations*/
/**\todo Implement the TLS specific relocations for x86_64*/
case R_X86_64_GOTPC32_TLSDESC: /* GOT offset for TLS descriptor.  */
case R_X86_64_TLSDESC_CALL: /* Marker for call through TLS
 descriptor.  */
case R_X86_64_TLSDESC: /* TLS descriptor.  */
case R_X86_64_DTPMOD64: /* ID of module containing symbol */
case R_X86_64_DTPOFF64: /* Offset in module's TLS block */
case R_X86_64_TPOFF64: /* Offset in initial TLS block */
case R_X86_64_TLSGD: /* 32 bit signed PC relative offset
 to two GOT entries for GD symbol */
case R_X86_64_TLSLD: /* 32 bit signed PC relative offset
 to two GOT entries for LD symbol */
case R_X86_64_DTPOFF32: /* Offset in TLS block */
ERRMSG("Unsupported relocation type in file %s at address %#"PRIx64" (type %d is relative to TLS extension)\n",
      relo->efile->filename, Elf_Shdr_get_sh_addr (NULL, 0, scn->scnhdr) + relo->offset, relo->relotype );
out = ERR_BINARY_RELOCATION_NOT_SUPPORTED;
case R_X86_64_GOTTPOFF: /* 32 bit signed PC relative offset
 to GOT entry for IE symbol */
addr32 = (Elf32_Addr*)relodst;
*addr32= G+GOT+A-P;
DBGMSG("RELOCATION <GOTTPOFF> (G+GOT+A-P): RELOC @%#"PRIx64" NOW CONTAINS %#"PRIx32" [%#"PRIx64"]\n", P,*addr32,relo->offset);
//STDMSG("GOTTPOFF RELOCATION\n");
break;
case R_X86_64_TPOFF32: /* Offset in initial TLS block */
addr32 =(Elf32_Addr*)relodst;
*addr32 = relo->immediateValue;
break;
default:
ERRMSG("Unrecognized relocation type %d in file %s at address %#"PRIx64"\n",
      relo->relotype, relo->efile->filename, Elf_Shdr_get_sh_addr (NULL, 0, scn->scnhdr) + relo->offset);
out = ERR_BINARY_RELOCATION_NOT_RECOGNISED;
}
}
return out;
}

/**
 * Applies a relocation for the arm architecture
 * \param relo The structure containing the details of the relocation to apply. Must not be NULL and must not contain NULL pointers to files
 * \return EXIT_SUCCESS if the relocation could be applied, EXIT_FAILURE otherwise
 * \note This could be later on exported to a different, architecture-specific, file
 * */
static int arm_elfreloc_apply(elfreloc_t* relo)
{
int out = EXIT_FAILURE;
if ( ( relo->efile != NULL ) && ( relo->reloscn != NULL ) && ( relo->scn != NULL ) ) {
/*&& ( relo->scnid < relo->efile->nsections ) && ( relo->reloscnid < relo->relofile->nsections )
 && ( relo->scnid >= 0 ) && ( relo->reloscnid >= 0 ) )*/

         // HANDLE ABS CASE && WEAK SYMBOL : No need to apply relocations.
if (relo->scn == NULL && relo->relooffset == -1)
{
return EXIT_SUCCESS;
}
elfsection_t* scn = relo->scn;         //relo->efile->sections[relo->scnid];
elfsection_t* reloscn = relo->reloscn;//relo->relofile->sections[relo->reloscnid];

void* relodst = scn->bits + relo->offset;
Elf64_Addr S = Elf_Shdr_get_sh_addr (NULL, 0, reloscn->scnhdr) + relo->relooffset;//Where the relocation points (Addr to reach)
Elf64_Addr P = Elf_Shdr_get_sh_addr (NULL, 0, scn->scnhdr) + relo->offset;//Place of the relocation (Addr of bits to modify)
int64_t A = relo->reloaddend;//The Addend : Relocation's place depends of the operand's size
short indexPLT = relo->idxPlt;//Index of the .PLT section in the file
Elf64_Addr PLT = 0;// It could be .madras.plt OR the original .plt section
if (relo->idxPlt != -1)
PLT = Elf_Shdr_get_sh_addr (relo->efile, indexPLT, NULL);//Address of the .PLT section
Elf64_Addr L = PLT + relo->reloPltOffset*0x10;//Address to call in the .PLT (WARNING: 0x10 == one entry size in PLT(x86)

short indexGOT = relo->efile->indexes[GOT];//Index of the .GOT section in the file.
Elf64_Addr GOT = Elf_Shdr_get_sh_addr (relo->efile, indexGOT, NULL);//Address of the .GOT section
int G = relo->reloGotIdx * sizeof(Elf64_Addr);//Offset of the variable in the .GOT section(One entry = 64-bit)

DBGMSG("S= %#"PRIx64"\n"
   "(sh_addr = %#"PRIx64")\n"
   "(relooffset = %#"PRIx64")\n"
   "P= %#"PRIx64"\n"
   "A= %"PRId64"\n"
   "L= %#"PRIx64"\n"
   "GOT= %#"PRIx64"\n"
   "G= %d\n",S,Elf_Shdr_get_sh_addr (NULL, 0, reloscn->scnhdr),(relo->relooffset),P,A,L,GOT,G);
//STDMSG("ERRMSG: Unsupported relocation type in file %s at address %#"PRIx64" (type %d not implemented yet)\n",
//relo->efile->filename, scn->scnhdr->sh_addr + relo->offset, relo->relotype );

// Il faut calculer l'adresse de destination en prenant l'adresse de début de la reloscn + relooffset,
// plus l'addend (voir les formules dans la spec ELF pour les variantes) puis copier ça à la longueur
// indiquée dans le data de la scn à l'offset offset
switch(relo->relotype)
{
Elf64_Addr *addr64;
Elf32_Addr *addr32;
case R_ARM_NONE: /* No reloc */
break;
case R_ARM_ABS32: /* Direct 32 bit */
case R_ARM_ABS16: /* Direct 16 bit */
case R_ARM_ABS12: /* Direct 12 bit */
//S+A
addr32 = (Elf32_Addr*)relodst;
*addr32 = S + A;
DBGMSG("RELOCATION <R_ARM_ABS32> (S+A): RELOC @%#"PRIx32" NOW CONTAINS %#"PRIx32" [%#"PRIx32"]\n", P,*addr32,relo->offset);
out = EXIT_SUCCESS;
break;
case R_ARM_REL32: /* PC relative 32 bits signed */
case R_ARM_PC24: /* PC relative 24 bits signed */
case R_ARM_PC13: /* PC relative 13 bits signed */
//S+A-P
addr32 = (Elf32_Addr*)relodst;
*addr32 = S + A - P;
DBGMSG("RELOCATION <R_ARM_REL32> (S+A-P): RELOC @%#"PRIx32" NOW CONTAINS %#"PRIx32" [%#"PRIx32"]\n", P,*addr32,relo->offset);
out = EXIT_SUCCESS;
break;
case R_ARM_GOT32: /* 32 bit GOT entry */
//G+A
addr32 = (Elf32_Addr*)relodst;
*addr32 = G + A;
DBGMSG("RELOCATION <GOT32> (G+A): RELOC @%#"PRIx32" NOW CONTAINS %#"PRIx32" [%#"PRIx32"]\n", P,*addr32,relo->offset);
out = EXIT_SUCCESS;
break;
case R_ARM_PLT32: /* 32 bit PLT address */
//L+A-P FOR IFUNC_GNU
//TEST S+A-P ON PLT32 RELOCATION (--> SEEMS TO BE THE REAL RELOCATION USED WITH -static)
addr32 = (Elf32_Addr*)relodst;
//if (relo->reloPltIdx == -1)
if (relo->reloPltOffset == -1)
{
   *addr32 = S+A-P;
   DBGMSG("RELOCATION <PLT32> /!!IN TEST: S+A-P!!/: RELOC @%#"PRIx32" NOW CONTAINS %#"PRIx32" [%#"PRIx64"]\n", P,*addr32,relo->offset);
}
else
{
   *addr32 = L+A-P;
   DBGMSG("RELOCATION <PLT32> (L+A-P: ONLY FOR IFUNC_GNU): RELOC @%#"PRIx32" NOW CONTAINS %#"PRIx32" [%#"PRIx64"]\n", P,*addr32,relo->offset);
}
out = EXIT_SUCCESS;
break;
case R_ARM_COPY: /* Copy symbol at runtime */
DBGMSG("RELOCATION :COPY: RELOC @%#"PRIx64"\n", P);
STDMSG("ERRMSG: Unsupported relocation type in file %s at address %#"PRIx64" (type %d not implemented yet)\n",
      relo->efile->filename, Elf_Shdr_get_sh_addr (NULL, 0, scn->scnhdr) + relo->offset, relo->relotype );
out = EXIT_FAILURE;
break;
case R_ARM_GLOB_DAT: /* Create GOT entry */
//S
DBGMSG("RELOCATION :GLOB_DAT: RELOC @%#"PRIx64"\n", P);
addr32 = (Elf32_Addr*)relodst;
*addr32 = S;
DBGMSG("RELOCATION <GLOB_DAT> (S): RELOC @%#"PRIx32" NOW CONTAINS %#"PRIx32" [%#"PRIx32"]\n", P,*addr32,relo->offset);
out = EXIT_SUCCESS;
break;
case R_ARM_JUMP_SLOT: /* Create PLT entry */
//S
DBGMSG("RELOCATION :JUMP_SLOT: RELOC @%#"PRIx64"\n", P);
addr32 = (Elf32_Addr*)relodst;
*addr32 = S;
DBGMSG("RELOCATION <JUMP_SLOT> (S): RELOC @%#"PRIx32" NOW CONTAINS %#"PRIx32" [%#"PRIx32"]\n", P,*addr32,relo->offset);
out = EXIT_SUCCESS;
break;
case R_ARM_RELATIVE: /* Adjust by program base */
//B+A
DBGMSG("RELOCATION :RELATIVE: RELOC @%#"PRIx64"\n", P);
STDMSG("ERRMSG: Unsupported relocation type in file %s at address %#"PRIx64" (type %d not implemented yet)\n",
      relo->efile->filename, Elf_Shdr_get_sh_addr (NULL, 0, scn->scnhdr) + relo->offset, relo->relotype );
out = EXIT_FAILURE;
break;
case R_ARM_GOTPC: /* 32 bit signed PC relative*/
//G + GOT + A - P
addr32 = (Elf32_Addr*)relodst;
*addr32= G+GOT+A-P;
DBGMSG("RELOCATION <GOTPCREL> (G+GOT+A-P): RELOC @%#"PRIx32" NOW CONTAINS %#"PRIx32" [%#"PRIx64"]\n", P,*addr32,relo->offset);
out = EXIT_SUCCESS;
break;
case R_ARM_GOTOFF: /* 32 bit offset to GOT */
//S + A - GOT
addr32 = (Elf32_Addr*)relodst;
*addr32 = S+A-GOT;
DBGMSG("RELOCATION <GOTOFF32> (S+A-GOT): RELOC @%#"PRIx32" NOW CONTAINS %#"PRIx32" [%#"PRIx64"]\n", P,*addr32,relo->offset);
out = EXIT_SUCCESS;
break;
default:
STDMSG("ERRMSG: Unrecognized relocation type %d in file %s at address %#"PRIx64"\n",
      relo->relotype, relo->efile->filename, Elf_Shdr_get_sh_addr (NULL, 0, scn->scnhdr) + relo->offset);
}
return out;
}

static int x86_32_elfreloc_apply(elfreloc_t* relo) {
return (0);
}

/**
 * Applies a relocation
 * \param relo The structure containing the details of the relocation to apply
 * \return EXIT_SUCCESS if the relocation could be applied, error code otherwise
 * */
static int elfreloc_apply(elfreloc_t* relo) {
if (relo == NULL)
return ERR_COMMON_PARAMETER_MISSING;
if (relo->efile == NULL)
return ERR_BINARY_MISSING_BINFILE;

int out = EXIT_SUCCESS;
DBGMSGLVL(1, "Applying relocation at address %#"PRIx64"\n",relo->scn->scnhdr->sh_addr + relo->offset);
switch(elffile_getmachine(relo->efile)) {
case EM_X86_64:
DBGMSGLVL(1, "Applying relocation at address %#"PRIx64"\n", Elf_Shdr_get_sh_addr (NULL, 0, relo->scn->scnhdr) + relo->offset);
out = x86_64_elfreloc_apply(relo);
break;
case EM_386:
DBGMSGLVL(1, "Applying relocation at address %#"PRIx64"\n", Elf_Shdr_get_sh_addr (NULL, 0, relo->scn->scnhdr) + relo->offset);
out = x86_32_elfreloc_apply(relo);
break;
case EM_ARM:
DBGMSGLVL(1, "Applying relocation at address %#"PRIx64"\n", Elf_Shdr_get_sh_addr (NULL, 0, relo->scn->scnhdr) + relo->offset);
out = arm_elfreloc_apply(relo);
break;
default:
ERRMSG("Relocation requested for unimplemented architecture\n");
out = ERR_LIBASM_ARCH_UNKNOWN;
}
return out;
}

/**
 * Checks if a section is a TLS section. The test is performed on the section's flags (as opposed to, for instance,
 * comparing with the program segments) to support object files.
 * \param scn The section to test (must not be NULL)
 * \return TRUE if the section has flags Allocate, Write and TLS sets, and is of type PROGBITS(TDATA) OR NOBITS(TBSS), FALSE otherwise
 * */
static int elfsection_istls(elfsection_t* scn) {
int sh_type = Elf_Shdr_get_sh_type (NULL, 0, scn->scnhdr);
int sh_flags = Elf_Shdr_get_sh_flags (NULL, 0, scn->scnhdr);

if ((sh_flags & SHF_ALLOC) != 0
   && (sh_flags & SHF_WRITE) != 0
   && (sh_flags & SHF_TLS) != 0
   && (sh_type == SHT_PROGBITS || sh_type == SHT_NOBITS))
return TRUE;
else
return FALSE;
}

/**
 * Creates a new relocation for each entry in a relocation table and adds them to an existing array
 * \param scn The relocation section containing the relocation table. It must apply to a section loadable section
 * \param relocs Pointer to an array of relocation structures. Must not be NULL
 * \param n_relocs Pointer to the size of \e relocs. Will be updated
 * \param scnfile The file containing the section
 * \param efile The file we are updating
 * \param addfiles Array of additional files where to look up for undefined symbols
 * \param n_addfiles Size of the \e addfiles
 * \param comalign_max Pointer to the maximum alignment for unallocated symbols (must not be NULL)
 * \param comscnsz Pointer to the minimal size of the section containing unallocated symbols (must not be NULL)
 * \param comsyms Queue of unallocated symbols (tableentry_t structures)
 * \note This function is used only by elfexe_scnreorder, and exists mainly for code clarity reasons
 * \todo Reduce the number of members in elfreloc_t to only have the minimum required, and update this function accordingly
 * */
static void elfrelocs_addscn(elfsection_t* scn, elfreloc_t** relocs, int* n_relocs, elffile_t* scnfile, elffile_t* efile,
elffile_t** addfiles, int n_addfiles, unsigned int *comalign_max, int64_t* comscnsz,
queue_t* comsyms,hashtable_t* offsetInTLSsection, int tdataIdx)
{
int i;
assert(relocs != NULL
   && scn != NULL
   && n_relocs != NULL
   && (Elf_Shdr_get_sh_type (NULL, 0, scn->scnhdr) == SHT_RELA || Elf_Shdr_get_sh_type (NULL, 0, scn->scnhdr) == SHT_REL)
   && scn->nentries > 0
   && comalign_max != NULL
   && comscnsz != NULL
   && comsyms != NULL);

*relocs = lc_realloc(*relocs,sizeof(elfreloc_t)*((*n_relocs) + scn->nentries));
int symid; //Index of the Symbol table (.symtab section) corresponding to the relocation
int idxRelo;//Relocation Index for the table relocs
for (i = 0; i < scn->nentries; i++)
{
int type_i = 0;
idxRelo = i + *n_relocs;
tableentry_t *entry = scn->table_entries[i];
tableentry_t *symbol = NULL;
//Generic informations (destination of the relocation)
(*relocs)[idxRelo].efile = efile;//Pointer to the file containing the section where the relocation must be performed
(*relocs)[idxRelo].scn = scnfile->sections[Elf_Shdr_get_sh_info (NULL, 0, scn->scnhdr)];//Section in efile where the relocation must be performed

if (scn->table_entries[i]->entry_64 != NULL)
type_i = scn->table_entries[i]->entry_64->type;
else if (scn->table_entries[i]->entry_32 != NULL)
type_i = scn->table_entries[i]->entry_32->type;

//Relocation-type specific informations (offset and type of the relocation)
if (type_i == RELTABLE )
{
   if (entry->entry_64 != NULL)
   {
      (*relocs)[idxRelo].offset = entry->entry_64->elfentry.rel->r_offset; //Offset in the section where the relocation must be applied
      symid = ELF64_R_SYM(entry->entry_64->elfentry.rel->r_info);//Idx of the .symtab section in the file
      (*relocs)[idxRelo].reloaddend = 0;//Addend of the relocation (if existing)
      (*relocs)[idxRelo].relotype = ELF64_R_TYPE(entry->entry_64->elfentry.rel->r_info);//Type of the relocation (ELF code)
   }
   else if (entry->entry_32 != NULL)
   {
      (*relocs)[idxRelo].offset = entry->entry_32->elfentry.rel->r_offset; //Offset in the section where the relocation must be applied
      symid = ELF32_R_SYM(entry->entry_32->elfentry.rel->r_info);//Idx of the .symtab section in the file
      (*relocs)[idxRelo].reloaddend = 0;//Addend of the relocation (if existing)
      (*relocs)[idxRelo].relotype = ELF32_R_TYPE(entry->entry_32->elfentry.rel->r_info);//Type of the relocation (ELF code)
   }
}
else if (type_i == RELATABLE)
{
   if (entry->entry_64 != NULL)
   {
      (*relocs)[idxRelo].offset = entry->entry_64->elfentry.rela->r_offset; //Offset in the section where the relocation must be applied
      symid = ELF64_R_SYM(entry->entry_64->elfentry.rela->r_info);//Idx of the .symtab section in the file
      (*relocs)[idxRelo].reloaddend = entry->entry_64->elfentry.rela->r_addend;// Addend of the relocation (if existing)
      (*relocs)[idxRelo].relotype = ELF64_R_TYPE(entry->entry_64->elfentry.rela->r_info);// Type of the relocation (ELF code)
   }
   else if (entry->entry_32 != NULL)
   {
      (*relocs)[idxRelo].offset = entry->entry_32->elfentry.rela->r_offset; //Offset in the section where the relocation must be applied
      symid = ELF32_R_SYM(entry->entry_32->elfentry.rela->r_info);//Idx of the .symtab section in the file
      (*relocs)[idxRelo].reloaddend = entry->entry_32->elfentry.rela->r_addend;// Addend of the relocation (if existing)
      (*relocs)[idxRelo].relotype = ELF32_R_TYPE(entry->entry_32->elfentry.rela->r_info);// Type of the relocation (ELF code)
   }
}

//Now resolving the actual relocation
if (entry->entry_64 != NULL)
{
   symbol = scnfile->sections[Elf_Shdr_get_sh_link (NULL, 0, scn->scnhdr)]->table_entries[ELF64_R_SYM(entry->entry_64->elfentry.rela->r_info)]; //Associated symbol entry
   assert(symbol->entry_64->type == SYMTABLE);
   DBGMSGLVL(1,"Relocation %d (offset %#"PRIx64" in section %s of file %s) points to symbol \"%s\" at address %#"PRIx64"\n",
         i, (*relocs)[idxRelo].offset, elfsection_name(scnfile, Elf_Shdr_get_sh_info (NULL, 0, scn->scnhdr)), scnfile->filename,
         symbol->entry_64->name, symbol->entry_64->addr);
}
else if (entry->entry_32 != NULL)
{
   symbol = scnfile->sections[Elf_Shdr_get_sh_link (NULL, 0, scn->scnhdr)]->table_entries[ELF32_R_SYM(entry->entry_32->elfentry.rela->r_info)]; //Associated symbol entry
   assert(symbol->entry_32->type == SYMTABLE);
   DBGMSGLVL(1,"Relocation %d (offset %#"PRIx64" in section %s of file %s) points to symbol \"%s\" at address %#"PRIx64"\n",
         i, (*relocs)[idxRelo].offset, elfsection_name(scnfile, Elf_Shdr_get_sh_info (NULL, 0, scn->scnhdr)), scnfile->filename,
         symbol->entry_32->name, symbol->entry_32->addr);
}

(*relocs)[idxRelo].symentry = symbol; //Symbol associated to the relocation
(*relocs)[idxRelo].relooffset = 0;//initialize relooffset (by default 0)
(*relocs)[idxRelo].symbolIsExtracted = 1;//Initialize (by default 1 means that the symbol is not defined in the original file)

if((symbol->entry_64 != NULL && symbol->entry_64->elfentry.sym->st_shndx > 0 && symbol->entry_64->elfentry.sym->st_shndx < scnfile->nsections)
      || (symbol->entry_32 != NULL && symbol->entry_32->elfentry.sym->st_shndx > 0 && symbol->entry_32->elfentry.sym->st_shndx < scnfile->nsections))
{
   //Symbol refers to a section inside the file (EXECEPT FOR .bss section -> SPECIFIC TREATMENT (see below))
   if (symbol->entry_64 != NULL)
   {
      (*relocs)[idxRelo].reloscn = scnfile->sections[symbol->entry_64->elfentry.sym->st_shndx];
      (*relocs)[idxRelo].relooffset = symbol->entry_64->elfentry.sym->st_value;
      (*relocs)[idxRelo].relosize = symbol->entry_64->elfentry.sym->st_size;
   }
   else if (symbol->entry_32 != NULL)
   {
      (*relocs)[idxRelo].reloscn = scnfile->sections[symbol->entry_32->elfentry.sym->st_shndx];
      (*relocs)[idxRelo].relooffset = symbol->entry_32->elfentry.sym->st_value;
      (*relocs)[idxRelo].relosize = symbol->entry_32->elfentry.sym->st_size;
   }
   //The relocations deals with TLS (relo.tdata section)
   if (elfsection_istls(scnfile->sections[Elf_Shdr_get_sh_info (NULL, 0, scn->scnhdr)]) == TRUE && tdataIdx != -1)
   {
      //Points the relocation to the .tdata_madras section
      (*relocs)[idxRelo].scn = efile->sections[tdataIdx];
      //Search the adresse of the .tdata section that contains the variable (or the adress of the variable)
      uint64_t sectionOffset = (uint64_t) hashtable_lookup(offsetInTLSsection,(void*)scnfile->sections[Elf_Shdr_get_sh_info (NULL, 0, scn->scnhdr)]->scnhdr);
      //assert(sectionOffset != 0);
      //The position (offset) of the variable in the .tdata_madras section
      //(*relocs)[idxRelo].offset += (sectionOffset - efile->sections[tdataIdx]->scnhdr->sh_addr);
      (*relocs)[idxRelo].offset += sectionOffset;
      (*relocs)[idxRelo].reloaddend = 0;
   }

   /**< Offset in the section containing the relocation target of the relocation target*/
   if (symbol->entry_64 != NULL)
   {
      DBGMSG( "Symbol was found in the same file %s at address %#"PRIx64" in section %s\n",
            scnfile->filename,(*relocs)[idxRelo].relooffset,
            elfsection_name(scnfile,symbol->entry_64->elfentry.sym->st_shndx));
   }
   else if (symbol->entry_32 != NULL)
   {
      DBGMSG( "Symbol was found in the same file %s at address %#"PRIx64" in section %s\n",
            scnfile->filename,(*relocs)[idxRelo].relooffset,
            elfsection_name(scnfile,symbol->entry_32->elfentry.sym->st_shndx));
   }
}
else
{
   if (symbol->entry_64 != NULL && symbol->entry_64->elfentry.sym->st_shndx == SHN_COMMON)
   {
      //Unallocated symbol: it will be resolved once all sections have been added
      (*relocs)[idxRelo].reloscn = NULL;
      (*relocs)[idxRelo].relooffset = 0;
      (*relocs)[idxRelo].relosize = symbol->entry_64->elfentry.sym->st_size;
      (*relocs)[idxRelo].reloalign = symbol->entry_64->elfentry.sym->st_value;
      if(symbol->entry_64->elfentry.sym->st_value > *comalign_max)
      *comalign_max = symbol->entry_64->elfentry.sym->st_value;

      //TODO: SPECIFIC CASE LIKE SYMBOL'S SIZE == 0 EXIST... (INSERTION OF .BSS SECTION WITH SIZE NULL...) : TO HANDLE
      *comscnsz = (*comscnsz) + symbol->entry_64->elfentry.sym->st_size;
      //Adds the symbol entry to the list of symbols
      if( queue_lookup(comsyms, direct_equal, &symbol) == NULL )
      queue_add_tail(comsyms, &symbol);
   }
   else if (symbol->entry_32 != NULL && symbol->entry_32->elfentry.sym->st_shndx == SHN_COMMON)
   {
      //Unallocated symbol: it will be resolved once all sections have been added
      (*relocs)[idxRelo].reloscn = NULL;
      (*relocs)[idxRelo].relooffset = 0;
      (*relocs)[idxRelo].relosize = symbol->entry_32->elfentry.sym->st_size;
      (*relocs)[idxRelo].reloalign = symbol->entry_32->elfentry.sym->st_value;
      if(symbol->entry_32->elfentry.sym->st_value > *comalign_max)
      *comalign_max = symbol->entry_32->elfentry.sym->st_value;

      //TODO: SPECIFIC CASE LIKE SYMBOL'S SIZE == 0 EXIST... (INSERTION OF .BSS SECTION WITH SIZE NULL...) : TO HANDLE
      *comscnsz = (*comscnsz) + symbol->entry_32->elfentry.sym->st_size;
      //Adds the symbol entry to the list of symbols
      if( queue_lookup(comsyms, direct_equal, &symbol) == NULL )
      queue_add_tail(comsyms, &symbol);
   }

   if ((symbol->entry_64 != NULL && symbol->entry_64->elfentry.sym->st_shndx == SHN_UNDEF && symbol->entry_64->name != NULL && strlen(symbol->entry_64->name) > 0)
         || (symbol->entry_32 != NULL && symbol->entry_32->elfentry.sym->st_shndx == SHN_UNDEF && symbol->entry_32->name != NULL && strlen(symbol->entry_32->name) > 0))
   {  //External symbol: we need to look it up in the list of inserted files
      int j;
      tableentry_t* extsym = NULL;
      elffile_t* extsymsrc = NULL;
      //First looking up in the file we are inserting into
      if (symbol->entry_64 != NULL)
      extsym = hashtable_lookup(efile->labels, symbol->entry_64->name);
      else if (symbol->entry_32 != NULL)
      extsym = hashtable_lookup(efile->labels, symbol->entry_32->name);

      /**\todo Factorise the following code*/
      if (extsym == NULL
            || (extsym->entry_64 != NULL && ELF64_ST_BIND(extsym->entry_64->elfentry.sym->st_info) == STB_WEAK && extsym->entry_64->elfentry.sym->st_shndx == SHN_UNDEF)
            || (extsym->entry_32 != NULL && ELF32_ST_BIND(extsym->entry_32->elfentry.sym->st_info) == STB_WEAK && extsym->entry_32->elfentry.sym->st_shndx == SHN_UNDEF))
      {
         extsym=NULL;
         //Not found in the file: looking up the list of inserted files
         for (j = 0; j < n_addfiles; j++ )
         {
            if (addfiles[j] != scnfile)
            {  //No need to look up the file we are in
               if ((symbol->entry_64 != NULL && (extsym = hashtable_lookup(addfiles[j]->labels, symbol->entry_64->name)) != NULL && extsym->entry_64->elfentry.sym->st_shndx != SHN_UNDEF)
                     || (symbol->entry_32 != NULL && (extsym = hashtable_lookup(addfiles[j]->labels, symbol->entry_32->name)) != NULL && extsym->entry_32->elfentry.sym->st_shndx != SHN_UNDEF))
               {
                  //Found the file where the symbol is defined
                  extsymsrc = addfiles[j];
                  break;
               }
            }
         }
         if ( (extsym != NULL) && (extsymsrc != NULL) )
         {
            //SYMBOL IS DEFINE IN ONE OF THE INSERTED FILE
            //HANDLE ABS CASE --> NO RELOCATION NEEDED
            if ((extsym->entry_64 != NULL && extsym->entry_64->elfentry.sym->st_shndx == SHN_ABS)
                  || (extsym->entry_32 != NULL && extsym->entry_32->elfentry.sym->st_shndx == SHN_ABS))
            {
               // TO CHECK DURING THE RELOCATION
               // if (relooffset == -1 && reloscn == NULL ) in x86_64_elfreloc_apply
               // then  break;
               (*relocs)[idxRelo].reloscn = NULL;
               (*relocs)[idxRelo].relooffset = -1;
               continue;
            }
            else
            {
               //HANDLE COMMON VAR
               if ((extsym->entry_64 != NULL && extsym->entry_64->elfentry.sym->st_shndx == SHN_COMMON)
                     || (extsym->entry_32 != NULL && extsym->entry_32->elfentry.sym->st_shndx == SHN_COMMON))
               {
                  //Unallocated symbol: it will be resolved once all sections have been added
                  (*relocs)[idxRelo].reloscn = NULL;
                  (*relocs)[idxRelo].relooffset = 0;

                  if (extsym->entry_64 != NULL)
                  {
                     (*relocs)[idxRelo].relosize = extsym->entry_64->elfentry.sym->st_size;
                     (*relocs)[idxRelo].reloalign = extsym->entry_64->elfentry.sym->st_value;
                     if(extsym->entry_64->elfentry.sym->st_value > *comalign_max)
                     *comalign_max = extsym->entry_64->elfentry.sym->st_value;
                     *comscnsz = (*comscnsz) + extsym->entry_64->elfentry.sym->st_size;
                  }
                  else if (extsym->entry_32 != NULL)
                  {
                     (*relocs)[idxRelo].relosize = extsym->entry_32->elfentry.sym->st_size;
                     (*relocs)[idxRelo].reloalign = extsym->entry_32->elfentry.sym->st_value;
                     if(extsym->entry_32->elfentry.sym->st_value > *comalign_max)
                     *comalign_max = extsym->entry_32->elfentry.sym->st_value;
                     *comscnsz = (*comscnsz) + extsym->entry_32->elfentry.sym->st_size;
                  }
                  //Adds the symbol entry to the list of symbols
                  if( queue_lookup(comsyms, direct_equal, &extsym) == NULL )
                  {
                     queue_add_tail(comsyms, &extsym);
                  }
               }
               else
               {
                  (*relocs)[idxRelo].symentry = extsym;
                  //Get the target section of the relocation
                  if (extsym->entry_64 != NULL)
                  {
                     (*relocs)[idxRelo].reloscn = extsymsrc->sections[extsym->entry_64->elfentry.sym->st_shndx];
                     (*relocs)[idxRelo].relooffset = extsym->entry_64->elfentry.sym->st_value;
                  }
                  else if (extsym->entry_32 != NULL)
                  {
                     (*relocs)[idxRelo].reloscn = extsymsrc->sections[extsym->entry_32->elfentry.sym->st_shndx];
                     (*relocs)[idxRelo].relooffset = extsym->entry_32->elfentry.sym->st_value;
                  }

                  //The relocation deals with TLS (relo.tdata section)
                  if (elfsection_istls(scnfile->sections[Elf_Shdr_get_sh_info (NULL, 0, scn->scnhdr)]) == TRUE && tdataIdx != -1)
                  {
                     //Points the relocation to the .tdata_madras section
                     (*relocs)[idxRelo].scn = efile->sections[tdataIdx];
                     //search the adresse of the .tdata section that contains the variable (or the adress of the variable)
                     uint64_t sectionOffset = sectionOffset = (uint64_t) hashtable_lookup(offsetInTLSsection,
                           (void*)scnfile->sections[Elf_Shdr_get_sh_info (NULL, 0, scn->scnhdr)]->scnhdr);

                     // The position (offset) of the variable in the .tdata_madras section
                     (*relocs)[idxRelo].offset += sectionOffset;
                     (*relocs)[idxRelo].reloaddend = 0;
                  }

                  if (extsym->entry_64 != NULL)
                  {
                     DBGMSG( "Symbol was found in file %s at address %#"PRIx64" in section %s\n",
                           extsymsrc->filename,(*relocs)[idxRelo].relooffset,
                           elfsection_name(extsymsrc,extsym->entry_64->elfentry.sym->st_shndx));
                  }
                  else if (extsym->entry_32 != NULL)
                  {
                     DBGMSG( "Symbol was found in file %s at address %#"PRIx64" in section %s\n",
                           extsymsrc->filename,(*relocs)[idxRelo].relooffset,
                           elfsection_name(extsymsrc,extsym->entry_32->elfentry.sym->st_shndx));
                  }
                  //bug For some reason, elfsection_name does not returns the right value
                  // =>  probably because of the libelf trying to use the data present in the elf structure,
                  //    and it has been replaced when creating new section names
               }
            }
         }
         else
         {
            //TEMPORARY FIX TO HANDLE WEAK FUNCTION THAT DEALS WITH GOTPCREL RELO (STILL NEED TO HAVE ONE ENTRY IN GOT SET TO 0)!!!
            if (symbol->entry_64 != NULL
                  && ELF64_ST_TYPE(symbol->entry_64->elfentry.sym->st_info) == STT_FUNC
                  && ELF64_ST_BIND(symbol->entry_64->elfentry.sym->st_info) == STB_WEAK)
            {
               (*relocs)[idxRelo].symbolIsExtracted = 0; //Initialize (HERE SPECIAL CASE -> WEAK FUNCTION set to 0)
               (*relocs)[idxRelo].symentry = symbol;
               (*relocs)[idxRelo].reloscn = efile->sections[efile->indexes[GOT_IDX]];//IN TEST
            }
            else if (symbol->entry_32 != NULL
                  && ELF32_ST_TYPE(symbol->entry_32->elfentry.sym->st_info) == STT_FUNC
                  && ELF32_ST_BIND(symbol->entry_32->elfentry.sym->st_info) == STB_WEAK)
            {
               (*relocs)[idxRelo].symbolIsExtracted = 0; //Initialize (HERE SPECIAL CASE -> WEAK FUNCTION set to 0)
               (*relocs)[idxRelo].symentry = symbol;
               (*relocs)[idxRelo].reloscn = efile->sections[efile->indexes[GOT_IDX]];//IN TEST
            }
            else
            {
               // We can't find the relocation anywhere: marking it as erroneous
               (*relocs)[idxRelo].scn = NULL;
            }
            if (symbol->entry_64 != NULL)
            fprintf(stderr,"We can't find this symbol %s\n",symbol->entry_64->name);
            else if (symbol->entry_32 != NULL)
            fprintf(stderr,"We can't find this symbol %s\n",symbol->entry_32->name);

            /**\todo Add some error handling here, but this should have been detected above*/
         }
      }
      else
      {
         // SYMBOL IS DEFINE IN THE ORIGINAL FILE ITSELF (IN THE BINARY TO PATCH)
         (*relocs)[idxRelo].symbolIsExtracted = 0;//Initialize (0 means that the symbol is defined in the original file)
         extsymsrc = efile;
         if ((extsym->entry_64 != NULL && extsym->entry_64->elfentry.sym->st_shndx == SHN_UNDEF)//UNDEFINED SYMBOL
               || (extsym->entry_32 != NULL && extsym->entry_32->elfentry.sym->st_shndx == SHN_UNDEF))
         {
            //WEAK SYMBOL
            //TODO:We can override weak symbols if we've found the definition of the symbol in a .o file (see GOLD linker)
            if ((extsym->entry_64 != NULL && ELF64_ST_BIND(extsym->entry_64->elfentry.sym->st_info) == STB_WEAK)
                  || (extsym->entry_32 != NULL && ELF32_ST_BIND(extsym->entry_32->elfentry.sym->st_info) == STB_WEAK))
            {
               //fprintf(stderr,"WEAK SYMBOL\n");
               (*relocs)[idxRelo].scn = NULL;
               (*relocs)[idxRelo].relooffset = -1;
            }
            else
            {
               //SCAN RELA SECTION OF THE ORIGINAL FILE TO PATCH.
               //SEARCH symbol->name
               //EXTRACT INFORMATION
               int idx=0,entry=0;
               for (idx=0;idx<efile->nsections;idx++)
               {
                  if (Elf_Shdr_get_sh_type (efile, idx, NULL) == SHT_RELA
                        || Elf_Shdr_get_sh_type (efile, idx, NULL) == SHT_REL)
                  {
                     elfsection_t* relScn = efile->sections[idx];
                     for(entry = 0;entry < relScn->nentries; entry++)
                     {
                        tableentry_t* te = relScn->table_entries[entry];
                        if ((te->entry_64 != NULL && te->entry_64->name != NULL && strcmp(te->entry_64->name, symbol->entry_64->name) == 0)
                              || (te->entry_32 != NULL && te->entry_32->name != NULL && strcmp(te->entry_32->name, symbol->entry_32->name) == 0))
                        {
                           //TODO: TO MODIFY : SPECIFIC CASE (I.E: RELA.DYNSYM) --> DO A GENERIC FUNCTION
                           if (Elf_Shdr_get_sh_info (NULL, 0, relScn->scnhdr) != (unsigned int)efile->indexes[PLT_IDX])
                           {
                              if (symbol->entry_64 != NULL)
                              {
                                 DBGMSG("Symbol %s @ %#"PRIx64" IN ORIGINAL FILE\n",symbol->entry_64->name,
                                       relScn->table_entries[entry]->entry_64->elfentry.rela->r_offset);
                                 (*relocs)[idxRelo].symbolIsExtracted=2; //Symbol is in GOT of the original file
                                 (*relocs)[idxRelo].reloscn = efile->sections[Elf_Shdr_get_sh_link (NULL, 0, relScn->scnhdr)];
                                 (*relocs)[idxRelo].relooffset=relScn->table_entries[entry]->entry_64->elfentry.rela->r_offset;
                              }
                              else if (symbol->entry_32 != NULL)
                              {
                                 DBGMSG("Symbol %s @ %#"PRIx32" IN ORIGINAL FILE\n",symbol->entry_32->name,
                                       relScn->table_entries[entry]->entry_32->elfentry.rela->r_offset);
                                 (*relocs)[idxRelo].symbolIsExtracted=2; //Symbol is in GOT of the original file
                                 (*relocs)[idxRelo].reloscn = efile->sections[Elf_Shdr_get_sh_link (NULL, 0, relScn->scnhdr)];
                                 (*relocs)[idxRelo].relooffset=relScn->table_entries[entry]->entry_32->elfentry.rela->r_offset;
                              }
                           }
                           break;
                        }
                     }
                  }
               }
               //TO HANDLE RELA PLT... TODO:TO MERGE WITH THE LOOP JUST UPSTAIR
               if ((*relocs)[idxRelo].symbolIsExtracted!=2)
               {
                  char* name = NULL;
                  if (symbol->entry_64 != NULL)
                  name = symbol->entry_64->name;
                  else if (symbol->entry_32 != NULL)
                  name = symbol->entry_32->name;
                  DBGMSG("symbol : %s found in file %s \n",name,extsymsrc->filename);

                  //Symbol is a dynamic symbol
                  /**\todo Check if the PLT is here!!*/
                  (*relocs)[idxRelo].reloscn = extsymsrc->sections[efile->indexes[PLT_IDX]];
                  (*relocs)[idxRelo].relooffset = elfexe_get_extlbl_addr(efile, name) - Elf_Shdr_get_sh_addr (NULL, 0, (*relocs)[idxRelo].reloscn->scnhdr);
                  //STDMSG( "Symbol was found as dynamic in file %s at address %#"PRIx64" in section %s\n",
                  DBGMSGLVL(1, "Symbol was found as dynamic in file %s at address %#"PRIx64" in section %s\n",
                        extsymsrc->filename,(*relocs)[idxRelo].relooffset,
                        elfsection_name(extsymsrc,efile->indexes[PLT_IDX]));/**\bug For some reason, elfsection_name does not returns the right value*/
                  /**\todo WARNING! Lots of assumptions here (namely, that a dynamic symbol always points to an address in the PLT)*/
               }
            }
         }
         else //SYMBOL IS DEFINED
         {
            //ABSOLUTE SYMBOL
            if ((extsym->entry_64 != NULL && extsym->entry_64->elfentry.sym->st_shndx == SHN_ABS)
                  || (extsym->entry_32 != NULL && extsym->entry_32->elfentry.sym->st_shndx == SHN_ABS))
            {
               //fprintf(stderr,"ABSOLUTE SYMBOL\n");
               (*relocs)[idxRelo].scn = NULL;
               (*relocs)[idxRelo].relooffset = -1;
            }
            else
            {
               (*relocs)[idxRelo].symentry = extsym;
               // Section in the relofile containing the relocation target
               if (extsym->entry_64 != NULL)
               (*relocs)[idxRelo].reloscn = extsymsrc->sections[extsym->entry_64->elfentry.sym->st_shndx];
               else if (extsym->entry_32 != NULL)
               (*relocs)[idxRelo].reloscn = extsymsrc->sections[extsym->entry_32->elfentry.sym->st_shndx];
               // Offset in the section containing the relocation target of the relocation target

               //Specific case for TLS symbols (The offset in the TLS section is directly stored in st_value)
               if (extsym->entry_64 != NULL)
               {
                  if (ELF64_ST_TYPE(extsym->entry_64->elfentry.sym->st_info) == STT_TLS)
                  (*relocs)[idxRelo].relooffset = extsym->entry_64->elfentry.sym->st_value;
                  else
                  (*relocs)[idxRelo].relooffset = extsym->entry_64->elfentry.sym->st_value
                  - Elf_Shdr_get_sh_addr (NULL, 0, (*relocs)[idxRelo].reloscn->scnhdr);
               }
               else if (extsym->entry_32 != NULL)
               {
                  if (ELF32_ST_TYPE(extsym->entry_32->elfentry.sym->st_info) == STT_TLS)
                  (*relocs)[idxRelo].relooffset = extsym->entry_32->elfentry.sym->st_value;
                  else
                  (*relocs)[idxRelo].relooffset = extsym->entry_32->elfentry.sym->st_value
                  - Elf_Shdr_get_sh_addr (NULL, 0, (*relocs)[idxRelo].reloscn->scnhdr);
               }

               if (extsym->entry_64 != NULL)
               {
                  if ( extsym->entry_64->elfentry.sym->st_shndx == SHN_COMMON )
                  {
                     //Unallocated symbol: it will be resolved once all sections have been added
                     (*relocs)[idxRelo].reloscn = NULL;//The section will be affected later
                     (*relocs)[idxRelo].relooffset = 0;
                     (*relocs)[idxRelo].relosize = extsym->entry_64->elfentry.sym->st_size;
                     (*relocs)[idxRelo].reloalign = extsym->entry_64->elfentry.sym->st_value;
                     if(extsym->entry_64->elfentry.sym->st_value > *comalign_max)
                     *comalign_max = extsym->entry_64->elfentry.sym->st_value;
                     *comscnsz = (*comscnsz) + extsym->entry_64->elfentry.sym->st_size;
                     //Adds the symbol entry to the list of symbols
                     if( queue_lookup(comsyms, direct_equal, &extsym) == NULL )
                     queue_add_tail(comsyms, &extsym);
                  }
                  DBGMSGLVL(1, "Symbol was found in file %s at address %#"PRIx64" in section %s\n",
                        extsymsrc->filename,(*relocs)[idxRelo].relooffset,
                        elfsection_name(extsymsrc,extsym->entry_64->elfentry.sym->st_shndx));
               }
               else if (extsym->entry_32 != NULL)
               {
                  if ( extsym->entry_32->elfentry.sym->st_shndx == SHN_COMMON )
                  {
                     //Unallocated symbol: it will be resolved once all sections have been added
                     (*relocs)[idxRelo].reloscn = NULL;//The section will be affected later
                     (*relocs)[idxRelo].relooffset = 0;
                     (*relocs)[idxRelo].relosize = extsym->entry_32->elfentry.sym->st_size;
                     (*relocs)[idxRelo].reloalign = extsym->entry_32->elfentry.sym->st_value;
                     if(extsym->entry_32->elfentry.sym->st_value > *comalign_max)
                     *comalign_max = extsym->entry_32->elfentry.sym->st_value;
                     *comscnsz = (*comscnsz) + extsym->entry_32->elfentry.sym->st_size;
                     //Adds the symbol entry to the list of symbols
                     if( queue_lookup(comsyms, direct_equal, &extsym) == NULL )
                     queue_add_tail(comsyms, &extsym);
                  }
                  DBGMSGLVL(1, "Symbol was found in file %s at address %#"PRIx64" in section %s\n",
                        extsymsrc->filename,(*relocs)[idxRelo].relooffset,
                        elfsection_name(extsymsrc,extsym->entry_32->elfentry.sym->st_shndx));
               }
               /**\bug For some reason, elfsection_name does not returns the right value*/
            }
         }
      }
   }
}
}
*n_relocs = (*n_relocs) + scn->nentries;
}

/**
 * Frees an elf section
 * \param escn Pointer to the structure to free
 * */
void elfsection_free(elfsection_t* escn) {
if(!escn)
return;
if (escn->scnhdr != NULL)
{
if (escn->scnhdr->scnhdr64 != NULL)
lc_free(escn->scnhdr->scnhdr64);
else if (escn->scnhdr->scnhdr32 != NULL)
lc_free (escn->scnhdr->scnhdr32);
lc_free (escn->scnhdr);
}
lc_free(escn->table_entries);
lc_free(escn);
}

/**
 * Checks if a symbol entry is a function
 * \param sym A pointer to a structure holding a symbol entry
 * \return 1 if this symbol has type STT_FUNC, 0 otherwise
 * */
static int GElf_Sym64_isfunc(void* sym)
{
Elf64_Sym *s = (Elf64_Sym*)sym;
if(s && ELF64_ST_TYPE(s->st_info) == STT_FUNC)
return 1;
else
return 0;
}

/*
 * Checks if a symbol entry is a function
 * \param sym A pointer to a structure holding a symbol entry
 * \return 1 if this symbol has type STT_FUNC, 0 otherwise
 * */
static int GElf_Sym32_isfunc(void* sym)
{
Elf32_Sym *s = (Elf32_Sym*)sym;
if(s && ELF32_ST_TYPE(s->st_info) == STT_FUNC)
return 1;
else
return 0;
}

/**
 * Checks if a symbol entry is marked as local
 * \param sym A pointer to a structure holding a symbol entry
 * \return TRUE if this symbol has binding STB_LOCAL, FALSE otherwise
 * */
static int GElf_Sym64_islocal(void* sym) {
Elf64_Sym *s = (Elf64_Sym*)sym;
if( s && (ELF64_ST_BIND(s->st_info) == STB_LOCAL) )
return TRUE;
else
return FALSE;
}

/**
 * Checks if a symbol entry is marked as local
 * \param sym A pointer to a structure holding a symbol entry
 * \return TRUE if this symbol has binding STB_LOCAL, FALSE otherwise
 * */
static int GElf_Sym32_islocal(void* sym) {
Elf32_Sym *s = (Elf32_Sym*)sym;
if( s && (ELF32_ST_BIND(s->st_info) == STB_LOCAL) )
return TRUE;
else
return FALSE;
}

/**
 * Checks if a symbol entry is a dummy
 * \param sym A pointer to a structure holding a symbol entry
 * \return 1 if this symbol has type STT_NUM, 0 otherwise
 */
static int GElf_Sym64_isdummy(void* sym)
{
Elf64_Sym *s = (Elf64_Sym*)sym;
if (s && ELF64_ST_TYPE(s->st_info) == STT_NUM)
return 1;
else
return 0;
}

/**
 * Checks if a symbol entry is a dummy
 * \param sym A pointer to a structure holding a symbol entry
 * \return 1 if this symbol has type STT_NUM, 0 otherwise
 */
static int GElf_Sym32_isdummy(void* sym)
{
Elf32_Sym *s = (Elf32_Sym*)sym;
if (s && ELF32_ST_TYPE(s->st_info) == STT_NUM)
return 1;
else
return 0;
}

/*
 * Checks if an ELF file is an executable
 * \param The structure containing the parsed ELF file
 * \return TRUE if the file is an executable, FALSE otherwise
 * */
int elffile_isexe(elffile_t* efile) {
return ( (elffile_gettype(efile) == ET_EXEC)? TRUE: FALSE);
}

/*
 * Checks if an ELF file is a dynamic executable
 * \param efile The structure containing the parsed ELF file
 * \return TRUE if the file is a dynamic executable (contains a .dynamic section), FALSE otherwise
 * */
int elffile_isexedyn(elffile_t* efile) {
return (((efile != NULL) && (elffile_gettype(efile) == ET_EXEC) && (efile->indexes[DYN] != -1))? TRUE: FALSE);
}

/*
 * Checks if an ELF file is an shared (dynamic) library
 * \param efile The structure containing the parsed ELF file
 * \return TRUE if the file is an shared library, FALSE otherwise
 * */
int elffile_isdyn(elffile_t* efile) {
return ( (elffile_gettype(efile) == ET_DYN)? TRUE: FALSE);
}

/*
 * Checks if an ELF file is a relocatable (object) file
 * \param efile The structure containing the parsed ELF file
 * \return TRUE if the file is a relocatable, FALSE otherwise
 * */
int elffile_isobj(elffile_t* efile) {
return ((elffile_gettype(efile) == ET_REL)? TRUE: FALSE);
}

/*
 * Checks if a label is of type function
 * \param efile Pointer to the ELF file
 * \param label the label to look for
 * \return TRUE if the label is of type function in the ELF file or in the debug informations if present, FALSE otherwise. If the label was
 * not found or if no symbol table is present in the file, returns SIGNED_ERROR. If the debug informations are present and don't mark the label as
 * being a function, returns LABEL_ISNOFCT
 */
int elffile_label_isfunc(elffile_t* efile,char* label)
{
int out = SIGNED_ERROR;
tableentry_t* symbol = NULL;
if ((efile)&&(efile->labels))
{
symbol = hashtable_lookup(efile->labels,label);
if (symbol) {
   int iselffunc = 0;
   if (symbol->entry_64 != NULL)
   iselffunc = GElf_Sym64_isfunc(symbol->entry_64->elfentry.sym);
   else if (symbol->entry_32 != NULL)
   iselffunc = GElf_Sym32_isfunc(symbol->entry_32->elfentry.sym);

   if (strcmp (label, LABEL_PATCHMOV) == 0)
   out = TRUE;

   //Also checks in the debug information, if they are present, whether the label is marked as a function
   else if (efile->dwarf) {
      DwarfFunction* fctindbg = NULL;
      if (symbol->entry_64 != NULL)
      fctindbg = dwarf_api_get_function_by_addr(efile->dwarf, symbol->entry_64->addr);
      else if (symbol->entry_32 != NULL)
      fctindbg = dwarf_api_get_function_by_addr(efile->dwarf, symbol->entry_32->addr);

      if ( (!iselffunc) && ( (!fctindbg) || (!str_equal(dwarf_function_get_name(fctindbg), label)) ) )
      out = LABEL_ISNOFCT;
      else
      out = TRUE;
   } else
   out = iselffunc;
}
}
return out;
}

/*
 * Checks if a label is of type "dummy" (see \ref elffile_add_dummysymbol)
 * \param efile Pointer to the ELF file
 * \param label the label to look for
 * \return 1 if the label is of type "dummy" (STT_NUM) in the ELF file, 0 otherwise. If the label was
 * not found or if no symbol table is present in the file, returns -1.
 */
int elffile_label_isdummy(elffile_t* efile,char* label)
{
int out = -1;
tableentry_t* symbol = NULL;
if (efile && (efile->labels) && (label))
{
symbol = hashtable_lookup(efile->labels,label);
if (symbol && symbol->entry_64 != NULL)
out = GElf_Sym64_isdummy(symbol->entry_64->elfentry.sym);
else if (symbol && symbol->entry_32 != NULL)
out = GElf_Sym32_isdummy(symbol->entry_32->elfentry.sym);
}
return out;
}

/*
 * Returns the section a label belongs to
 * \param efile Pointer to the ELF file
 * \param label the label to look for
 * \return The number of the section, or -1 if the label is not present in the file
 * */
int elffile_label_get_scn(elffile_t* efile, char* label)
{
int out = -1;
tableentry_t* symbol = NULL;
if (efile && (efile->labels) && (label))
{
symbol = hashtable_lookup(efile->labels,label);
if (symbol && symbol->entry_64 != NULL && symbol->entry_64->type == SYMTABLE)
out = symbol->entry_64->elfentry.sym->st_shndx;
else if (symbol && symbol->entry_32 != NULL && symbol->entry_32->type == SYMTABLE)
out = symbol->entry_32->elfentry.sym->st_shndx;
}
return out;
}
/*
 * Creates a label_t structure from a tableeentry_t structure
 * \param ef The ELF file
 * \param symbol The table entry containing the label
 * \return A label structure containing the informations about the label
 * */
label_t* elffile_label_getaslabel(elffile_t* efile, tableentry_t* symbol) {
label_t* lbl = NULL;
if (!symbol)
return NULL;
char* sym_name = NULL;
int64_t sym_addr = 0;

if (symbol->entry_64 != NULL)
{
sym_name = symbol->entry_64->name;
sym_addr = symbol->entry_64->addr;
}
else if (symbol->entry_32 != NULL)
{
sym_name = symbol->entry_32->name;
sym_addr = symbol->entry_32->addr;
}

lbl = label_new(sym_name, sym_addr, TARGET_UNDEF, NULL);

if ((symbol->entry_64 != NULL && !elffile_scnisprog(efile, symbol->entry_64->elfentry.sym->st_shndx))
   || (symbol->entry_32 != NULL && !elffile_scnisprog(efile, symbol->entry_32->elfentry.sym->st_shndx))
   || (sym_name[0] == '$'))
label_set_type(lbl, LBL_NOFUNCTION); //Label is not associated to a section containing code, it can't identify a function
else if (strcmp (sym_name, LABEL_PATCHMOV) == 0)
label_set_type(lbl, LBL_PATCHSCN);//Label marks the beginning of the section added by the patcher
else if ((symbol->entry_64 != NULL && GElf_Sym64_isdummy(symbol->entry_64->elfentry.sym))
   || (symbol->entry_32 != NULL && GElf_Sym32_isdummy(symbol->entry_32->elfentry.sym)))
label_set_type(lbl, LBL_DUMMY);//Label is a "dummy" label added by the patcher
else
{
int iselffunc = 0;
int iselflocal = 0;

if (symbol->entry_64 != NULL) {
   iselffunc = GElf_Sym64_isfunc(symbol->entry_64->elfentry.sym);
   iselflocal = GElf_Sym64_islocal(symbol->entry_64->elfentry.sym);
} else if (symbol->entry_32 != NULL) {
   iselffunc = GElf_Sym32_isfunc(symbol->entry_32->elfentry.sym);
   iselflocal = GElf_Sym32_islocal(symbol->entry_32->elfentry.sym);
}
//Also checks in the debug information, if they are present, whether the label is marked as a function
if (efile->dwarf) {
   DwarfFunction* fctindbg = dwarf_api_get_function_by_addr(efile->dwarf, sym_addr);
   if (iselffunc || (fctindbg && str_equal(dwarf_function_get_name(fctindbg), sym_name)))
            label_set_type(lbl, LBL_FUNCTION); //Label is marked as being a function in the DWARF info or in an ELF flag
   else
            label_set_type(lbl, LBL_NOFUNCTION);    //Dwarf information is present and does not mark the label as a function
} else if (iselffunc) {
         label_set_type(lbl, LBL_FUNCTION);      //No dwarf present and label marked as function in the ELF
} else if (iselflocal) {
         label_set_type(lbl, LBL_NOFUNCTION); //Label is not marked as a function and is local
} else if ( strstr(sym_name,".") != NULL )
         label_set_type(lbl, LBL_NOFUNCTION); //Label contains a dot and is not marked as function
}

switch(elffile_getmachine(efile))
{
case EM_ARM:
if ((label_get_addr(lbl)&1) && (label_get_type(lbl) == LBL_FUNCTION)) {
   label_setaddress(lbl,label_get_addr(lbl)-1);
}
break;
default:
break;
}

 //DIRTY FIX FOR _REAL_FINI //EUGENE
 //fprintf(stderr,"<%s>\n",symbol->name);
if(strcmp(sym_name,"_real_fini") == 0)
{
      label_set_type(lbl, LBL_FUNCTION);
}
return lbl;
}

/*
 * Returns the index of the section spanning over the given address
 * \param ef Parsed ELF file
 * \param addr Address over which we are looking for a spanning section
 * \return The index of the section in the ELF file, or -1 if no section was found
 */
int elffile_get_scnspanaddr(elffile_t* ef, int64_t addr) {
/** \todo Improve the efficiency of this function (maybe with an hashtable ?) and avoid calling it too often */
int indx = 0;
if(!ef)
return -1;

while (indx < Elf_Ehdr_get_e_shnum (ef, NULL))
{ //TLS data sections don't behave like the others. No address should reference them directly
if ((Elf_Shdr_get_sh_flags (ef, indx, NULL) & SHF_TLS) == 0)
{
   if (addr >= (int64_t) Elf_Shdr_get_sh_addr (ef, indx, NULL)
         && addr < (int64_t)(Elf_Shdr_get_sh_addr (ef, indx, NULL)
               + Elf_Shdr_get_sh_size (ef, indx, NULL)))
   break;
}
indx++;
}
if (indx == Elf_Ehdr_get_e_shnum (ef, NULL))
indx = -1;
return indx;
}

/*
 * Adds a target address link to an ELF file from its section and offset
 * \param ef The parsed ELF file
 * \param elfobj A pointer to an object referencing an address
 * \param tscnidx The index of the section containing this address
 * \param toffs The offset of the address from the beginning of the section
 * \param updfunc A pointer to the function to be used to update the object (if the address changes)
 * \param srcscnidx The index of the ELF section containing the object (if applicable)
 * \param srctblidx The index in the ELF table where the object is at (if applicable)
 * \return EXIT_SUCCESS / error code
 * */
int elffile_add_targetscnoffset(elffile_t* ef, void* elfobj, int tscnidx, int64_t toffs,
addrupdfunc_t updfunc, int srcscnidx, int srctblidx)
{
if(!ef)
return ERR_BINARY_MISSING_BINFILE;
targetaddr_t* target = (targetaddr_t*)lc_malloc(sizeof(targetaddr_t));
target->scnindx = tscnidx;
target->offs = toffs;
target->updaddr = updfunc;
target->srcscnidx = srcscnidx;
target->srctblidx = srctblidx;
hashtable_insert(ef->targets,elfobj,target);
return EXIT_SUCCESS;
}

/*
 * Adds a target address link to an elf file
 * \param ef The parsed ELF file
 * \param elfobj A pointer to an object referencing an address
 * \param addr The address reference by the object
 * \param updfunc A pointer to the function to be used to update the object (if the address changes)
 * \param srcscnidx The index of the ELF section containing the object (if applicable)
 * \param srctblidx The index in the ELF table where the object is at (if applicable)
 * \return EXIT_SUCCESS / error code
 */
int elffile_add_targetaddr(elffile_t* ef,void* elfobj,int64_t addr,addrupdfunc_t updfunc,int srcscnidx,int srctblidx)
{
if(!ef)
return ERR_BINARY_MISSING_BINFILE;
  //Looking for the section containing this address
int scnidx = elffile_get_scnspanaddr(ef,addr);
int out = EXIT_SUCCESS;
if(scnidx>-1)
{  //Nothing is done if no section has been found
out = elffile_add_targetscnoffset(ef,elfobj, scnidx,addr - ef->sections[scnidx]->scnhdr->sh_addr,
      updfunc, srcscnidx, srctblidx);
DBGMSGLVL(2,"Object %p in %d[%d] points to %#"PRIx64"(in %d)\n",elfobj,srcscnidx,srctblidx,addr,scnidx);
}
else
{
//!\todo Handle the case when the target is not found
out = ERR_BINARY_TARGET_ADDRESS_NOT_FOUND;
}
return out;
}

/**
 * Updates an existing target address link in an elf file. If the existing target is not found, NULL will be returned
 * \param ef The parsed ELF file
 * \param oldelfobj A pointer to the object referencing an address and that we want to update
 * \param newelfobj A pointer to the new object referencing this address
 * \param addr The address referenced by the object
 * \return The targetaddr_t object that was updated or NULL if the object was not referenced in the table
 * */
static targetaddr_t* elffile_upd_targetaddr(elffile_t* ef,void* oldelfobj,void* newelfobj,int64_t addr)
{
if(!ef)
return NULL;
  //Retrieves the target object associated to the entry
targetaddr_t* oldtarget = hashtable_lookup(ef->targets,oldelfobj);
if(oldtarget)
{
//Former link was existing
//Removes the former target from the table
hashtable_remove(ef->targets,oldelfobj);
elffile_add_targetscnoffset(ef,newelfobj,oldtarget->scnindx,addr - Elf_Shdr_get_sh_addr (ef, oldtarget->scnindx, NULL),
      oldtarget->updaddr,oldtarget->srcscnidx,oldtarget->srctblidx);

DBGMSGLVL(2,"%p in %d[%d]=>%#"PRIx64"(in %d)\n", newelfobj, oldtarget->srcscnidx, oldtarget->srctblidx,addr,oldtarget->scnindx);
}
return oldtarget;
}

/**
 * Finds the symbol names associated to the relocation entries of all relocation tables
 * in an elf file (the sections must have been parsed before calling this function) and
 * updates the tableentry objects with those names
 * \param ef The parsed ELF file
 */
static void elffile_reset_relnames(elffile_t* ef)
{
int i, j, entsym;
if(!ef)
return;
for (i = 0; i < ef->nsections; i++)
{
for (j = 0; j < ef->sections[i]->nentries; j++)
{
   int type = 0;
   if (ef->sections[i]->table_entries[j]->entry_64 != NULL)
   type = ef->sections[i]->table_entries[j]->entry_64->type;
   else if (ef->sections[i]->table_entries[j]->entry_32 != NULL)
   type = ef->sections[i]->table_entries[j]->entry_32->type;

   if (type == SYMTABLE || type == DYNTABLE)
   break;
   else if (type == RELTABLE)
   {
      if (ef->sections[i]->table_entries[j]->entry_64 != NULL)
      entsym = ELF64_R_SYM(ef->sections[i]->table_entries[j]->entry_64->elfentry.rel->r_info);
      else if (ef->sections[i]->table_entries[j]->entry_32 != NULL)
      entsym = ELF32_R_SYM(ef->sections[i]->table_entries[j]->entry_32->elfentry.rel->r_info);
   }
   else if (type == RELATABLE)
   {
      if (ef->sections[i]->table_entries[j]->entry_64 != NULL)
      entsym = ELF64_R_SYM(ef->sections[i]->table_entries[j]->entry_64->elfentry.rela->r_info);
      else if (ef->sections[i]->table_entries[j]->entry_32 != NULL)
      entsym = ELF32_R_SYM(ef->sections[i]->table_entries[j]->entry_32->elfentry.rela->r_info);}
   else
   continue; //This should never happen but without this case the compiler returns a warning and it's cleaner anyway
   char* name = symentry_name(ef, Elf_Shdr_get_sh_link (ef, i, NULL), entsym);

   if (name && ef->sections[i]->table_entries[j]->entry_64 != NULL)
   ef->sections[i]->table_entries[j]->entry_64->name = lc_strdup(name);
   else if (name && ef->sections[i]->table_entries[j]->entry_32 != NULL)
   ef->sections[i]->table_entries[j]->entry_32->name = lc_strdup(name);
   else if (ef->sections[i]->table_entries[j]->entry_64 != NULL)
   ef->sections[i]->table_entries[j]->entry_64->name = NULL;
   else if (ef->sections[i]->table_entries[j]->entry_32 != NULL)
   ef->sections[i]->table_entries[j]->entry_32->name = NULL;
}
}
}

/*
 * Parses the section at the specified index in an Elf file into an elfsection_t structure,
 * which will be copied into the sections array from the elffile_t object at the corresponding
 * index
 * Before calling this function:
 * - The array containing the sections in the parsed ELF file structure must have been initialized
 * - The section's object and header must already have been initialised
 * \param efile The structure containing the parsed ELF file
 * \param indx The index of the section
 * \todo This function should be renamed and updated for clarity as it now mostly deals with
 * the data in the section, especially the tables
 * \bug This function is supposed to be able to reload the content of a section from its original
 * file (for instance after an executable has been patched), but that does not work well
 */
static int parse_elf_section(elffile_t* efile, int indx)
{
elfsection_t* section = efile->sections[indx];
tableentry_t* entry = NULL;
int i;
int size = 0;
if(!efile)
return ERR_BINARY_MISSING_BINFILE;
int sh_type = Elf_Shdr_get_sh_type (efile, indx, NULL);
DBGMSG("Parsing section at index %d\n",indx);
 //Checks if the section contains a table
if ( sh_type == SHT_SYMTAB
   || sh_type == SHT_DYNSYM
   || sh_type == SHT_REL
   || sh_type == SHT_RELA
   || sh_type == SHT_DYNAMIC)
{
assert (Elf_Shdr_get_sh_entsize (efile, indx, NULL));
//Gets the number of entries in the table, based on the total size of the section
// and the size of an entry
section->nentries = Elf_Shdr_get_sh_size (efile, indx, NULL) / Elf_Shdr_get_sh_entsize (efile, indx, NULL);

//Creates the table entry section
section->table_entries = (tableentry_t**)lc_malloc(sizeof(tableentry_t*) * section->nentries);
for (i = 0; i < section->nentries; i++)
section->table_entries[i] = (tableentry_t*)lc_malloc0(sizeof(tableentry_t));
section->hentries = hashtable_new(direct_hash,direct_equal);

switch (Elf_Shdr_get_sh_type (efile, indx, NULL))
{
   case SHT_SYMTAB:
   case SHT_DYNSYM:
   {
      if (Elf_Ehdr_get_e_ident (efile, NULL)[EI_CLASS] == ELFCLASS64)
      {
         Elf64_Sym** syms = ELF_get_sym (efile, indx, &size);
         //Retrieves the symbol table
         //Retrieves the entries in the symbol table
         for (i = 0; i < section->nentries; i++)
         {
            //Creates an entry corresponding to the one in the table
            entry = section->table_entries[i];
            entry->entry_64 = lc_malloc (sizeof (tableentry_64_t));
            entry->entry_32 = NULL;
            entry->entry_64->elfentry.sym = syms[i];
            entry->entry_64->name = lc_strdup(ELF_strptr(efile,
                        Elf_Shdr_get_sh_link (efile, indx, NULL), entry->entry_64->elfentry.sym->st_name));
            entry->entry_64->type = SYMTABLE;
            entry->entry_64->addr = entry->entry_64->elfentry.sym->st_value;
            entry->entry_64->instr = NULL;
            //Links this symbol entry to the address it points to
            if ((entry->entry_64->addr != 0) && entry->entry_64->name && (strlen(entry->entry_64->name)))
            {
               elffile_add_targetaddr(efile,entry->entry_64->elfentry.sym,entry->entry_64->addr,*GElf_Sym_updaddr,indx,i);
               hashtable_insert(section->hentries,(void *)entry->entry_64->addr,entry);
            }
            //Stores the entry in the hashtable containing the labels
            if((entry->entry_64->name)&&(strlen(entry->entry_64->name)))
            hashtable_insert(efile->labels,entry->entry_64->name,entry);
            entry->entry_64->updated = FALSE;
         }
         lc_free (syms);
      }
      else if (Elf_Ehdr_get_e_ident (efile, NULL)[EI_CLASS] == ELFCLASS32)
      {
         Elf32_Sym** syms = ELF_get_sym (efile, indx, &size);

         //Retrieves the symbol table
         //Retrieves the entries in the symbol table
         for (i = 0; i < section->nentries; i++)
         {
            //Creates an entry corresponding to the one in the table
            entry = section->table_entries[i];
            entry->entry_64 = NULL;
            entry->entry_32 = lc_malloc (sizeof (tableentry_32_t));
            entry->entry_32->elfentry.sym = syms[i];
            entry->scnidx = indx;
            entry->entry_32->name = lc_strdup(ELF_strptr(efile,
                        Elf_Shdr_get_sh_link (efile, indx, NULL), entry->entry_32->elfentry.sym->st_name));
            entry->entry_32->type = SYMTABLE;
            entry->entry_32->addr = entry->entry_32->elfentry.sym->st_value;
            entry->entry_32->instr = NULL;
            //Links this symbol entry to the address it points to
            if ((entry->entry_32->addr != 0) && entry->entry_32->name && (strlen(entry->entry_32->name)))
            {
               elffile_add_targetaddr(efile,entry->entry_32->elfentry.sym,entry->entry_32->addr,*GElf_Sym_updaddr,indx,i);
               hashtable_insert(section->hentries,(void *)entry->entry_32->addr,entry);
            }
            //Stores the entry in the hashtable containing the labels
            if((entry->entry_32->name)&&(strlen(entry->entry_32->name)))
            hashtable_insert(efile->labels,entry->entry_32->name,entry);
            entry->entry_32->updated = FALSE;
         }
         lc_free (syms);
      }

      //Stores the index of current section in the elf file descriptor structure
      if (Elf_Shdr_get_sh_type (efile, indx, NULL) == SHT_SYMTAB)
      efile->indexes[SYM] = indx;
      else if (Elf_Shdr_get_sh_type (efile, indx, NULL) == SHT_DYNSYM)
      efile->indexes[DYNSYM_IDX] = indx;
      break;
   }
   case SHT_RELA:
   {
      if (Elf_Ehdr_get_e_ident (efile, NULL)[EI_CLASS] == ELFCLASS64)
      {
         Elf64_Rela** relas = ELF_get_rela (efile, indx, &size);

         //Retrieves a relocation table
         //Retrieves the entries in the relocation table
         for (i = 0; i < size; i++)
         {
            //Creates an entry corresponding to the one in the table
            entry = section->table_entries[i];
            entry->entry_32 = NULL;
            entry->entry_64 = lc_malloc (sizeof (tableentry_64_t));
            entry->entry_64->elfentry.rela = relas[i];
            /**\todo The name was not retrieved, and this indeed fails, but I don't remember why*/
            entry->entry_64->name = NULL;
            entry->entry_64->type = RELATABLE;
            entry->scnidx = indx;
            entry->entry_64->addr = entry->entry_64->elfentry.rela->r_offset;
            entry->entry_64->instr = NULL;
            //Links this relocation entry to the address it points to
            if (entry->entry_64->addr != 0)
            {
               elffile_add_targetaddr (efile, entry->entry_64->elfentry.sym, entry->entry_64->addr, *GElf_Rela_updaddr, indx, i);
               hashtable_insert (section->hentries, (void *)entry->entry_64->addr, entry);
            }
            entry->entry_64->updated = FALSE;
         }
         //Stores the index of current section in the elf file descriptor structure, if this
         // relocation table is associated to the ".text" section
         //In case the program data section has not yet been parsed
         if(str_equal(elfsection_name(efile, Elf_Shdr_get_sh_info (NULL, 0, section->scnhdr)),".text"))
         efile->indexes[RELA_IDX] = indx;
         lc_free (relas);
      }
      else if (Elf_Ehdr_get_e_ident (efile, NULL)[EI_CLASS] == ELFCLASS32)
      {
         Elf32_Rela** relas = ELF_get_rela (efile, indx, &size);

         //Retrieves a relocation table
         //Retrieves the entries in the relocation table
         for (i = 0; i < size; i++)
         {
            //Creates an entry corresponding to the one in the table
            entry = section->table_entries[i];
            entry->entry_64 = NULL;
            entry->entry_32 = lc_malloc (sizeof (tableentry_32_t));
            entry->entry_32->elfentry.rela = relas[i];
            /**\todo The name was not retrieved, and this indeed fails, but I don't remember why*/
            entry->entry_32->name = NULL;
            entry->entry_32->type = RELATABLE;
            entry->entry_32->addr = entry->entry_32->elfentry.rela->r_offset;
            entry->entry_32->instr = NULL;
            //Links this relocation entry to the address it points to
            if (entry->entry_32->addr != 0)
            {
               elffile_add_targetaddr (efile, entry->entry_32->elfentry.sym, entry->entry_32->addr, *GElf_Rela_updaddr, indx, i);
               hashtable_insert (section->hentries, (void *)entry->entry_32->addr, entry);
            }
            entry->entry_32->updated = FALSE;
         }
         //Stores the index of current section in the elf file descriptor structure, if this
         // relocation table is associated to the ".text" section
         //In case the program data section has not yet been parsed
         if(str_equal(elfsection_name(efile, Elf_Shdr_get_sh_info (NULL, 0, section->scnhdr)),".text"))
         efile->indexes[RELA_IDX] = indx;
         lc_free (relas);
      }
      //!\todo Check if relaidx can be safely removed, as it is not appropriate
      break;
   }
   case SHT_REL:
   {
      if (Elf_Ehdr_get_e_ident (efile, NULL)[EI_CLASS] == ELFCLASS64)
      {
         Elf64_Rel** rels = ELF_get_rel (efile, indx, &size);

         //Retrieves a relocation table
         //Retrieves the entries in the relocation table
         for (i = 0; i < size; i++)
         {
            int sh_link = Elf_Shdr_get_sh_link (efile, indx, NULL);
            //Creates an entry corresponding to the one in the table
            entry=section->table_entries[i];
            entry->entry_32 = NULL;
            entry->entry_64 = lc_malloc (sizeof (tableentry_64_t));
            entry->entry_64->elfentry.rel = rels[i];
            entry->entry_64->name = lc_strdup(ELF_strptr(efile, Elf_Shdr_get_sh_link (efile, sh_link, NULL),
                        entry->scnidx = indx;
                        ELF64_R_SYM(entry->entry_64->elfentry.rela->r_info)));
            entry->entry_64->type = RELTABLE;
            entry->entry_64->addr = entry->entry_64->elfentry.rel->r_offset;
            entry->entry_64->instr=NULL;
            //Links this relocation entry to the address it points to
            if(entry->entry_64->addr!=0) {
               elffile_add_targetaddr(efile,entry->entry_64->elfentry.sym,entry->entry_64->addr,*GElf_Rel_updaddr,indx,i);
               hashtable_insert(section->hentries,(void *)entry->entry_64->addr,entry);
            }
            entry->entry_64->updated = FALSE;
         }
         //Stores the index of current section in the elf file descriptor structure, if this
         // relocation table is associated to the program data section
         //In case the program data section has not yet been parsed
         if(str_equal(elfsection_name(efile, Elf_Shdr_get_sh_link (NULL, 0, section->scnhdr)),".text"))
         efile->indexes[RELO_IDX] = indx;
         lc_free (rels);
      }
      else if (Elf_Ehdr_get_e_ident (efile, NULL)[EI_CLASS] == ELFCLASS32)
      {
         Elf32_Rel** rels = ELF_get_rel (efile, indx, &size);

         //Retrieves a relocation table
         //Retrieves the entries in the relocation table
         for (i = 0; i < size; i++)
         {
            int sh_link = Elf_Shdr_get_sh_link (efile, indx, NULL);

            //Creates an entry corresponding to the one in the table
            entry=section->table_entries[i];
            entry->entry_64 = NULL;
            entry->entry_32 = lc_malloc (sizeof (tableentry_32_t));
            entry->entry_32->elfentry.rel = rels[i];
            entry->entry_32->name = lc_strdup(ELF_strptr(efile, Elf_Shdr_get_sh_link (efile, sh_link, NULL),
                        ELF32_R_SYM(entry->entry_32->elfentry.rela->r_info)));
            entry->entry_32->type = RELTABLE;
            entry->entry_32->addr = entry->entry_32->elfentry.rel->r_offset;
            entry->entry_32->instr=NULL;
            //Links this relocation entry to the address it points to
            if(entry->entry_32->addr!=0) {
               elffile_add_targetaddr(efile,entry->entry_32->elfentry.sym,entry->entry_32->addr,*GElf_Rel_updaddr,indx,i);
               hashtable_insert(section->hentries,(void *)entry->entry_32->addr,entry);
            }
            entry->entry_32->updated = FALSE;
         }
         //Stores the index of current section in the elf file descriptor structure, if this
         // relocation table is associated to the program data section
         //In case the program data section has not yet been parsed
         if(str_equal(elfsection_name(efile, Elf_Shdr_get_sh_link (NULL, 0, section->scnhdr)),".text"))
         efile->indexes[RELO_IDX] = indx;
         lc_free (rels);
      }
      //!\todo Check if relaidx can be safely removed, as it is not appropriate
      break;
   }
   case SHT_DYNAMIC:
   {
      if (Elf_Ehdr_get_e_ident (efile, NULL)[EI_CLASS] == ELFCLASS64)
      {
         Elf64_Dyn** dyns = ELF_get_dyn (efile, indx, &size);
         efile->indexes[DYN] = indx;
         //Retrieves the entries in the dynamic table
         for (i = 0; i < size; i++)
         {
            entry=section->table_entries[i];
            entry->entry_32 = NULL;
            entry->entry_64 = lc_malloc (sizeof (tableentry_64_t));
            entry->scnidx = indx;
            entry->entry_64->elfentry.dyn = dyns[i];
            entry->entry_64->type = DYNTABLE;
            entry->entry_64->name = NULL;
            switch(entry->entry_64->elfentry.dyn->d_tag) {
               //Dynamic entries containing a string
               case DT_NEEDED:
               case DT_SONAME:
               case DT_RPATH:
               /**\todo Find a way to retrieve the associated string for these dynamic entries
                * (the dynamic entry of type STRTAB contains the link to the section storing it,
                * but it may not have been encountered yet) */
               break;
               //Dynamic entries containing an address
               case DT_PLTGOT:
               efile->indexes[PLTGOT] = elffile_get_scnataddr(efile,entry->entry_64->elfentry.dyn->d_un.d_val);
               case DT_JMPREL:
               if(entry->entry_64->elfentry.dyn->d_tag==DT_JMPREL)
               efile->indexes[JMPREL] = elffile_get_scnataddr(efile,entry->entry_64->elfentry.dyn->d_un.d_val);
               case DT_HASH:
               case DT_STRTAB:
               case DT_SYMTAB:
               case DT_RELA:
               case DT_INIT:
               case DT_FINI:
               case DT_REL:
               case DT_VERSYM:
               entry->entry_64->addr = entry->entry_64->elfentry.dyn->d_un.d_ptr;
               //Linking the dynamic entry to the pointed address
               if(entry->entry_64->addr!=0) {
                  elffile_add_targetaddr (efile, entry->entry_64->elfentry.dyn,entry->entry_64->addr, *GElf_Dyn_updaddr, indx, i);
               }
               break;
            }
            entry->entry_64->instr=NULL;
            entry->entry_64->updated = FALSE;
         }
         lc_free (dyns);
      }
      else if (Elf_Ehdr_get_e_ident (efile, NULL)[EI_CLASS] == ELFCLASS32)
      {
         Elf32_Dyn** dyns = ELF_get_dyn (efile, indx, &size);
         efile->indexes[DYN] = indx;
         //Retrieves the entries in the dynamic table
         for (i = 0; i < size; i++)
         {
            entry=section->table_entries[i];
            entry->entry_64 = NULL;
            entry->entry_32 = lc_malloc (sizeof (tableentry_32_t));
            entry->entry_32->elfentry.dyn = dyns[i];
            entry->entry_32->type = DYNTABLE;
            entry->entry_32->name = NULL;
            switch(entry->entry_32->elfentry.dyn->d_tag) {
               //Dynamic entries containing a string
               case DT_NEEDED:
               case DT_SONAME:
               case DT_RPATH:
               /**\todo Find a way to retrieve the associated string for these dynamic entries
                * (the dynamic entry of type STRTAB contains the link to the section storing it,
                * but it may not have been encountered yet) */
               break;
               //Dynamic entries containing an address
               case DT_PLTGOT:
               efile->indexes[PLTGOT] = elffile_get_scnataddr(efile,entry->entry_32->elfentry.dyn->d_un.d_val);
               case DT_JMPREL:
               if(entry->entry_32->elfentry.dyn->d_tag==DT_JMPREL)
               efile->indexes[JMPREL] = elffile_get_scnataddr(efile,entry->entry_32->elfentry.dyn->d_un.d_val);
               case DT_HASH:
               case DT_STRTAB:
               case DT_SYMTAB:
               case DT_RELA:
               case DT_INIT:
               case DT_FINI:
               case DT_REL:
               case DT_VERSYM:
               entry->entry_32->addr = entry->entry_32->elfentry.dyn->d_un.d_ptr;
               //Linking the dynamic entry to the pointed address
               if(entry->entry_32->addr!=0) {
                  elffile_add_targetaddr (efile, entry->entry_32->elfentry.dyn,entry->entry_32->addr, *GElf_Dyn_updaddr, indx, i);
               }
               break;
            }
            entry->entry_32->instr = NULL;
            entry->entry_32->updated = FALSE;
         }
         lc_free (dyns);
      }

   }
}
//The section holds some program bytecode
}
else if (Elf_Shdr_get_sh_type (efile, indx, NULL) == SHT_PROGBITS
   && ELF_get_scn_bits (efile, indx) != NULL)
{
//Special case : handling of the .got section, which is an array of addresses
//!\todo Check if the handling of the .got section is not too specific to x86
if(strstr(elfsection_name(efile,indx),".got")!=NULL)
{
   Elf64_Addr* gotaddr = ELF_get_addr (efile, indx, &size);
   //Links the array elements to the actual addresses they represent
   for (i = 0; i < size; i++)
   {
      if(gotaddr[i]!=0)
      elffile_add_targetaddr(efile,&gotaddr[i],gotaddr[i],*GElf_Addr_updaddr,indx,i);
   }
   if(str_equal(elfsection_name(efile,indx),GOTNAME))
   efile->indexes[GOT_IDX] = indx;

}

//Storing the .got.plt section index (with static binary : no .dynamic section)
if(str_equal(elfsection_name(efile,indx),GOTPLTNAME)
      && efile->indexes[PLTGOT] == -1)
efile->indexes[PLTGOT] = indx;
//Storing the .plt section index
else if(str_equal(elfsection_name(efile,indx),PLTNAME))
efile->indexes[PLT_IDX] = indx;
//Storing the .tdata section index
else if (Elf_Shdr_get_sh_type (efile, indx, NULL) == SHT_PROGBITS
      && (Elf_Shdr_get_sh_flags (efile, indx, NULL) & SHF_TLS))
efile->indexes[TDATA_IDX] = indx;
//Storing the .madras.code
else if(str_equal(elfsection_name(efile,indx),MADRAS_TEXTSCN_NAME))
efile->indexes[MADRASTEXT_IDX] = indx;
//Storing the .madras.plt
else if(str_equal(elfsection_name(efile,indx),MADRAS_PLTSCN_NAME))
efile->indexes[MADRASPLT_IDX] = indx;
//Storing the .madras.data
else if(str_equal(elfsection_name(efile,indx),MADRAS_DATASCN_NAME))
efile->indexes[MADRASDATA_IDX] = indx;
}
      //The section contains version symbols informations
else if (Elf_Shdr_get_sh_type (efile, indx, NULL) == SHT_GNU_versym)
efile->indexes[VERSYM_IDX] = indx;
else if (Elf_Shdr_get_sh_type (efile, indx, NULL) == SHT_NOBITS
   && str_equal(elfsection_name(efile,indx),BSSNAME))
efile->indexes[BSS_IDX] = indx;
else if ( Elf_Shdr_get_sh_type (efile, indx, NULL) == SHT_NOBITS
   && (Elf_Shdr_get_sh_flags (efile, indx, NULL) & SHF_TLS))
      //    DBGLVL(1, if( ( scnhdr->sh_type == SHT_GNU_verneed ) || ( scnhdr->sh_type == SHT_GNU_versym ) ) {
      //       DBGMSG("Version section %s contains:\n",elfsection_name(efile,indx));
      //       elffile_print_verscn(efile,indx);
      //     })
return EXIT_SUCCESS;
}
#endif

/**
 * Creates a new, empty, structure for storing parsed ELF files
 * \param elf Structure containing the result of the parsing of an ELF file
 * \return An empty structure, with all pointers set to NULL and indexes to -1
 */
static elffile_t* elffile_new(Elf* elf)
{
   elffile_t* efile;
   int i;
   efile = (elffile_t*) lc_malloc0(sizeof(elffile_t));
   efile->elf = elf;
   //Initialises pointers
//   efile->elfheader = NULL;
//   efile->progheader = NULL;
//   efile->sections = NULL;
//   efile->nsections = 0;
//   efile->targets = NULL;
//   efile->labels = NULL;
//   efile->filename = NULL;
//   efile->targets = hashtable_new(direct_hash,direct_equal);
//   efile->labels  = hashtable_new(str_hash,str_equal);
//   efile->filedes = 0;

   /*Initializes the indexes of the important sections */
   for (i = 0; i < MAX_NIDX; i++)
      efile->indexes[i] = -1;
   /**\todo TODO (2014-09-29) Make the indexes array unsigned, and use 0 as the uninitialised value, since, you know,
    * the ELF standard states that section 0 is always empty and so on.*/
   //Initialises the hashtable ensuring the correspondence between entries representing labels and entries representing their names
   efile->symnames = hashtable_new(direct_hash, direct_equal);
   //Initialises the hashtable ensuring the correspondence between section and entries representing their names
   efile->scnnames = hashtable_new(direct_hash, direct_equal);

   return efile;
}

/*
 * Closes an ELF file associated to an \ref elffile_t structure and releases the structure
 * \param efile Pointer to the structure containing the parsed ELF file
 * \bug This function does not work properly if the ELF file has been used to create a patched file
 * (even if it the changes have not been saved). As of now it does not attempt to release
 * some structures as this can cause a crash.
 */
void elffile_free(void* e)
{
   elffile_t* efile = (elffile_t*) e;
//   unsigned int i, j;
   if (efile == NULL)
      return;
//   if (efile->lblscns) {
//      //Free the array of labels by section
//      for (i=0; i < Elf_Ehdr_get_e_shnum(efile->elf); i++) {
//         if (efile->lblscns[i])
//            lc_free(efile->lblscns[i]);
//      }
//      lc_free(efile->lblscns);
//      lc_free(efile->n_lbl_byscns);
//   }
   if (efile->flags & ELFFILE_PATCHCOPY)
      elf_end_nodatafree(efile->elf);
   else
      elf_end(efile->elf);
//   if (efile->labels)
//      hashtable_free(efile->labels, NULL, NULL);
//
//   dwarf_api_close_light (efile->dwarf);
//
//   //Free segment header
//   if (efile->progheader)
//   {
//      for (i = 0; i < Elf_Ehdr_get_e_phnum (efile, NULL); i++)
//      {
//         if (efile->progheader[i]->phdr64)
//            lc_free (efile->progheader[i]->phdr64);
//         else if (efile->progheader[i]->phdr32)
//            lc_free (efile->progheader[i]->phdr32);
//         lc_free (efile->progheader[i]);
//      }
//      lc_free (efile->progheader);
//   }
//
//
//   //Releases the section's array
//   if (efile->sections)
//   {
//      //Loops on the sections to release them
//      for (i = 0; i < efile->nsections; i++)
//      {
//         if (efile->sections[i]->table_entries != NULL)
//         {
//            //Frees all symbol table entries
//            for (j = 0; j < efile->sections[i]->nentries; j++)
//            {
//               tableentry_free_elfentry(efile->sections[i]->table_entries[j]);
//            }
//            lc_free (efile->sections[i]->table_entries);
//
//            //Frees the hashtable containing the entries
//            if(efile->sections[i]->hentries)
//               hashtable_free(efile->sections[i]->hentries,NULL,NULL);/**\todo Update the free functions if needed*/
//         }
//         if (efile->sections[i]->bits != NULL)
//            lc_free (efile->sections[i]->bits);
//         if (efile->sections[i]->scnhdr->scnhdr64 != NULL)
//            lc_free (efile->sections[i]->scnhdr->scnhdr64);
//         else if (efile->sections[i]->scnhdr->scnhdr32 != NULL)
//            lc_free (efile->sections[i]->scnhdr->scnhdr32);
//         lc_free (efile->sections[i]->scnhdr);
//         lc_free (efile->sections[i]);
//      }
//      lc_free(efile->sections);
//   }
//   //Releases the hashtable containing address links
//   if(efile->targets)
//      hashtable_free(efile->targets, lc_free,NULL);
//
//   //Releases the name of the file
//   lc_free (efile->filename);
//   lc_free (efile->elfheader);
//
//   if (efile->filedes != NULL)
//      fclose(efile->filedes);
   hashtable_free(efile->symnames, pointer_free, NULL);
   hashtable_free(efile->scnnames, pointer_free, NULL);
   if (efile->oldscnid)
      lc_free(efile->oldscnid);

   lc_free(efile);
}

#if 0
/*
 * Creates a new ELF file from an existing one
 * \param efile Parsed ELF file structure which will be copied to the new file
 * \param filename Name of the ELF file to create
 * \param filedes File descriptor of the new file to create
 * \note This function must not be used with a file that has been modified
 * */
void create_elf_file(elffile_t* efile, char* filename, int filedes) {
   (void) efile;
   (void) filename;
   (void) filedes;
#if 0
   /**\todo This function is not very useful and used only in a test function from the libmpatch that is itself not used*/
   int fd,i;
   Elf* elf;
   GElf_Ehdr* elfhdr;
   Elf_Scn *scn;
   Elf_Data *data;

   /*Checking ELF version*/
   if (elf_version(EV_CURRENT) == EV_NONE) { /* library out of date */
      ERRMSG("ELF library is out of date\n");
      return; //!\todo Error handling
   }
   CHECK_ERROR("Unable to retrieve ELF version",); //!\todo Error handling

   /*Checking file parameters*/
   if ((filename==NULL)&&(filedes<0)) {
      ERRMSG("No valid file name or descriptor given\n");
      return; //!\todo Error handling
   }

   /*Opening file*/
   if(filename!=NULL) {
      fd = open(filename,O_CREAT | O_WRONLY | O_TRUNC,0644); /*Modifier les flags en fonction du type de fichier (exe ou o)*/
      if (fd < 0) {
         ERRMSG("Unable to open file %s\n", filename);
         return; //!\todo Error handling
      }
   } else
   fd = filedes;

   /*Initializing elf file for writing*/
   elf=elf_begin(fd,ELF_C_WRITE,NULL);
   if(!elf) {
      ERRMSG("Unable to open ELF file %s for writing\n", filename);
      return; //!\todo Error handling
   }
   CHECK_ERROR("Unable to open ELF file for writing",); //!\todo Error handling

   /*Updates the header of the new file with the value from the old*/
   elfhdr = (GElf_Ehdr*)gelf_newehdr(elf,efile->elfheader->e_ident[EI_CLASS]);
   CHECK_ERROR("Unable to create new ELF header",); //!\todo Error handling

   *elfhdr = *(efile->elfheader);

   memcpy(elfhdr->e_ident,efile->elfheader->e_ident,EI_NIDENT);

   /*Updates the program header table if one existed in the old file*/
   if(efile->progheader!=NULL) {
      gelf_newphdr(elf,efile->elfheader->e_phnum);
      for(i=0;i<efile->elfheader->e_phnum;i++) {
         gelf_update_phdr(elf,i,&efile->progheader[i]);
         CHECK_ERROR("Unable to update program header",); //!\todo Error handling
      }
   }

   /*Update the sections of the new file*/
   for(i=1;i<efile->nsections;i++) {
      scn = elf_newscn(elf);
      CHECK_ERROR("Unable to create new ELF section",); //!\todo Error handling
      data = elf_newdata(scn);
      CHECK_ERROR("Unable to create new ELF section data",);//!\todo Error handling

      /*Copies the section header to the new file*/
      gelf_update_shdr(scn,efile->sections[i].scnhdr);
      CHECK_ERROR("Unable to update ELF section header",); //!\todo Error handling

      /*Copies the section data to the new file*/
      data->d_type = efile->sections[i].scndata->d_type;
      data->d_size= efile->sections[i].scndata->d_size;
      data->d_version = efile->sections[i].scndata->d_version;
      if(efile->sections[i].scndata->d_buf!=NULL) {
         data->d_buf = lc_malloc(sizeof(efile->sections[i].scndata->d_buf)*efile->sections[i].scndata->d_size);
         /**\todo Check the frees!*/
         memcpy(data->d_buf,efile->sections[i].scndata->d_buf,efile->sections[i].scndata->d_size);
      } else
      data->d_buf = NULL;
   }

   /*Writing ELF data to file*/
   if(!elf_update(elf,ELF_C_WRITE)) {
      ERRMSG("Unable to write ELF file %s\n", filename);
      return; //!\todo Error handling
   }
   CHECK_ERROR("Unable to update ELF file",); //!\todo Error handling
   /*Finalizing : closing ELF object and file*/
   elf_end(elf);
   CHECK_ERROR("Unable to close ELF file",); //!\todo Error handling
   close(fd);
#endif
}

/**
 * Updates the indexes of sections in a section's data containing a table according
 * to a reordering array of the indexes of sections
 * \param data Structure holding the section's data
 * \param type Type of the data in the section
 * \param revpermtbl Array of the new indexes of sections (i.e. revpermtbl[i] is the new index of
 * section i in the original file)
 * \return The updated data buffer (contained in data)
 */
static void* data_reorder_index(void* data, int d_size, int type,int* revpermtbl)
{
   int size, i;
   void* updbuf;
   Elf64_Sym* symtbl;

   switch(type)
   {
      case SHT_SYMTAB:
      case SHT_DYNSYM:
      if((!revpermtbl) || (!data))
      return NULL;
      symtbl = (Elf64_Sym*)data;
      size=d_size / sizeof(Elf64_Sym);
      for (i = 0; i < size; i++)
      {
         if ((symtbl[i].st_shndx < SHN_ABS)
               &&(symtbl[i].st_shndx != SHN_COMMON)
               &&(symtbl[i].st_shndx != SHN_UNDEF))
         symtbl[i].st_shndx = revpermtbl[symtbl[i].st_shndx];
      }
      updbuf = (void*)symtbl;
      break;
      default:
      updbuf = data;
   }
   return updbuf;
}

/*
 * Creates a new ELF file based on the contents of the elffile_t structure and reordering
 * the sections based on the reorder table
 * \param efile Structure holding a parsed ELF file, which has been modified
 * \param filename Name of the file to save the modified file under
 * \param permtbl Array containing the indexes of the old sections (i.e. permtbl[i] contains the
 * former index of the section moved at index i)
 * \return EXIT_SUCCESS if successful, error code otherwise
 * */
int copy_elf_file_reorder(elffile_t* efile, char* filename, int* permtbl) {
   int i, fd;
   FILE* f;
   if (!efile)
   return ERR_BINARY_MISSING_BINFILE;
   if (!permtbl)
   return ERR_COMMON_PARAMETER_MISSING;
   int revpermtbl[efile->nsections];
   Elf_Ehdr* elfhdr;
   void *databuf;

   //Creates the reverse reordering table (needed to update all section indexes)
   for (i = 0; i < efile->nsections; i++)
   revpermtbl[permtbl[i]] = i;

   //Checking file parameters
   if (filename==NULL)
   return ERR_COMMON_FILE_NAME_MISSING;

   //Opening file
   fd = open(filename,O_CREAT | O_WRONLY | O_TRUNC,0777);
   if (fd < 0) {
      ERRMSG("Unable to create ELF file %s\n", filename);
      return ERR_COMMON_UNABLE_TO_OPEN_FILE;
   }
   f = fdopen (fd, "w");

   //Initializing elf file for writing
   elffile_t* elf = ELF_begin(f, filename);
   if(!elf) {
      ERRMSG("Unable to initialise ELF file %s\n", filename);
      return ERR_BINARY_UNABLE_TO_CREATE_FILE;
   }

   //Updates the header of the new file with the value from the old
   elfhdr = ELF_newehdr(elf, efile->elfheader);

   //TODO: elfhdr is never actually used
   (void)elfhdr;

   //Updates the program header table if one existed in the old file
   if(efile->progheader != NULL)
   ELF_newphdr(elf, 0, efile->progheader);

   //Create the array of sections
   if(efile->nsections > 0)
   ELF_newscns(elf, efile->nsections, efile);

   //Update the sections of the new file
   for (i = 0; i < efile->nsections; i++)
   {
      elf->sections[i] = lc_malloc (sizeof (elfsection_t));

      //Copies the section header to the new file
      Elf_Shdr* scnhdr = (efile->sections[permtbl[i]]->scnhdr);

      //Updates the indexes in the section header
      if (Elf_Shdr_get_sh_link (NULL, 0, scnhdr) != 0
            && Elf_Shdr_get_sh_link (NULL, 0, scnhdr) != SHN_UNDEF)
      Elf_Shdr_set_sh_link (NULL, 0, scnhdr, revpermtbl[Elf_Shdr_get_sh_link (NULL, 0, scnhdr)]);

      //sh_info does not hold an section index if it is a symbol table or a VERNEED section (apparently)
      if (Elf_Shdr_get_sh_info (NULL, 0, scnhdr) != 0
            && Elf_Shdr_get_sh_type (NULL, 0, scnhdr) != SHT_SYMTAB
            && Elf_Shdr_get_sh_type (NULL, 0, scnhdr) != SHT_DYNSYM
            && Elf_Shdr_get_sh_type (NULL, 0, scnhdr) != SHT_GNU_verneed)
      Elf_Shdr_set_sh_info (NULL, 0, scnhdr, revpermtbl[Elf_Shdr_get_sh_info (NULL, 0, scnhdr)]);
      ELF_update_shdr (elf, i, scnhdr);

      //Copies the section data to the new file
      if(efile->sections[permtbl[i]]->bits != NULL)
      {
         elf->sections[i]->bits = lc_malloc(Elf_Shdr_get_sh_size (elf, i, NULL));
         /**\bug It is never freed*/
         databuf = data_reorder_index(efile->sections[permtbl[i]]->bits,
               Elf_Shdr_get_sh_size (NULL, 0, scnhdr),
               Elf_Shdr_get_sh_type (NULL, 0, scnhdr),
               revpermtbl);
         memcpy (elf->sections[i]->bits, databuf, Elf_Shdr_get_sh_size (efile, permtbl[i], NULL));
      }
      else
      elf->sections[i]->bits = NULL;
   }

   //Writing ELF data to file
   if(!ELF_update(elf))
   {
      ERRMSG("Unable to write to ELF file %s\n", filename);
      elffile_free(elf);
      close(fd);
      return ERR_BINARY_UNABLE_TO_WRITE_FILE; //!\todo Error handling
   }

   //Free elf
   /**\todo I don't know why, but call to elffile_free doesn't work so do it by hand*/
   for (i = 0; i < Elf_Ehdr_get_e_phnum (elf, NULL); i++)
   {
//      if (elf->progheader[i]->phdr64)
//         lc_free (elf->progheader[i]->phdr64);
//      else if (elf->progheader[i]->phdr32)
//         lc_free (elf->progheader[i]->phdr32);
//      lc_free (elf->progheader[i]);
   }
   for (i = 0; i < Elf_Ehdr_get_e_shnum (elf, NULL); i++)
   {
      if (elf->sections[i]->scnhdr->scnhdr64 != NULL)
      lc_free (elf->sections[i]->scnhdr->scnhdr64);
      else if (elf->sections[i]->scnhdr->scnhdr32 != NULL)
      lc_free (elf->sections[i]->scnhdr->scnhdr32);
      lc_free (elf->sections[i]->scnhdr);
      if (elf->sections[i]->bits)
      lc_free (elf->sections[i]->bits);
      lc_free (elf->sections[i]);
   }
   lc_free (elf->sections);
   lc_free (elf->progheader);
   lc_free (elf->elfheader);
   lc_free (elf);
   close(fd);

   return EXIT_SUCCESS;
}
/*
 * Returns the bits from the specified section as a stream of unsigned char*
 * \param efile Structure holding the parsed ELF file
 * \param secindx The index of the section
 * \param len Pointer to a variable holding the size in bytes of the char array returned
 * \param startaddr Pointer to an variable holding the address of the starting address of the bytestream
 * \return The bytestream (char array) of the section, or NULL if it is empty
 */
unsigned char* elffile_get_scnbits (elffile_t* efile,int secindx,int* len,int64_t* startaddr)
{
   unsigned char* out = ELF_get_scn_bits(efile, secindx);
   //Checks whether the section's bit content is not empty
   if(out != NULL)
   {
      //Stores the executable code of the section
      *len = Elf_Shdr_get_sh_size (efile, secindx, NULL);
      *startaddr = Elf_Shdr_get_sh_addr (efile, secindx, NULL);
   }
   else
   {  //The section is empty
      *len = -1;
      *startaddr = -1;
   }
   return out;
}

/**
 * Returns the bits from the specified section as a stream of unsigned char*
 * Returns the bits as Big Endian if the architecture is ARM32.
 * WARNING : temporary patch, should be placed in an architecture specific part.
 * \param efile Structure holding the parsed ELF file
 * \param secindx The index of the section
 * \param len Pointer to a variable holding the size in bytes of the char array returned
 * \param startaddr Pointer to an variable holding the address of the starting address of the bytestream
 * \return The bytestream (char array) of the section, or NULL if it is empty
 */
unsigned char* elffile_get_scncode (elffile_t* efile,int secindx,int* len,int64_t* startaddr)
{
   unsigned char* out = elffile_get_scnbits(efile, secindx, len, startaddr);
   //Checks whether the section's bit content is not empty
   if(out != NULL) {
      return out;
   }
   else
   return NULL;
}

/*
 * Checks if an ELF section contains prog bits
 * \param efile Structure holding the parsed ELF file
 * \param scnidx The identifier of the section to check
 * \return TRUE if the section contains prog bits, FALSE otherwise
 */
int elffile_scnisprog(elffile_t* efile,int scnidx)
{
   int out = FALSE;
   if (efile
         && scnidx > 0
         && scnidx < efile->nsections
         && Elf_Shdr_get_sh_type (efile, scnidx, NULL) == SHT_PROGBITS
         && (Elf_Shdr_get_sh_flags (efile, scnidx, NULL) & SHF_EXECINSTR))
   out = TRUE;
   return out;
}

/**
 * Updates the ELF data associated to a table entry based the value of the entry
 * \param efile Structure holding the disassembled ELF file
 * \param tableidx Index of the section holding the table to be updated
 * \param tabletype Type of the table (SYMTABLE, RELATABLE or RELTABLE)
 * \todo Factorise the code of this function, possibly using macros to handle the rel/rela types
 */
static void elffile_upd_tbldata(elffile_t* efile,int tableidx,int tabletype)
{
   int i;
   if(!efile)
   return;
   elfsection_t* scn = efile->sections[tableidx];
   targetaddr_t* lasttarget = NULL, *currenttarget = NULL;
   switch(tabletype) {
      case(SYMTABLE):
      //Updating ELF symbol table structure
      //Creates the new ELF table
      if (Elf_Ehdr_get_e_ident (efile, NULL)[EI_CLASS] == ELFCLASS64)
      {
         Elf64_Sym *newsym = (Elf64_Sym*)lc_malloc(sizeof(Elf64_Sym)*scn->nentries);
         for (i = 0; i < scn->nentries; i++)
         {
            newsym[i] = *(scn->table_entries[i]->entry_64->elfentry.sym);
            //Updates hashtable with new elf structure
            //We don't want to add LABEL_PATCHMOV to the table as we update it manually
            if (newsym[i].st_value != 0 && scn->table_entries[i]->entry_64->name
                  && strlen (scn->table_entries[i]->entry_64->name)
                  && strcmp (scn->table_entries[i]->entry_64->name, LABEL_PATCHMOV))
            {
               currenttarget = elffile_upd_targetaddr(efile, scn->table_entries[i]->entry_64->elfentry.sym,
                     &newsym[i], newsym[i].st_value);
               if (currenttarget)
               {
                  if (lasttarget)
                  lc_free(lasttarget);
                  lasttarget = currenttarget;
               }
               else
               elffile_add_targetscnoffset(efile, &newsym[i], lasttarget->scnindx, newsym[i].st_value
                     - Elf_Shdr_get_sh_addr (efile, lasttarget->scnindx, NULL), GElf_Sym_updaddr, tableidx, i);
            }

            //Updates the address of the elf structure
            scn->table_entries[i]->entry_64->elfentry.sym = &newsym[i];
            scn->table_entries[i]->entry_64->updated = TRUE;
         }
         if (lasttarget)
         lc_free(lasttarget);
         lc_free(scn->bits);
         scn->bits = (void*)newsym;
         Elf_Shdr_set_sh_size (NULL, 0, scn->scnhdr, sizeof(Elf64_Sym)*scn->nentries);
      }
      else if (Elf_Ehdr_get_e_ident (efile, NULL)[EI_CLASS] == ELFCLASS32)
      {
         Elf32_Sym* newsym = (Elf32_Sym*)lc_malloc(sizeof(Elf32_Sym)*scn->nentries);
         for (i = 0; i < scn->nentries; i++)
         {
            newsym[i] = *(scn->table_entries[i]->entry_32->elfentry.sym);
            //Updates hashtable with new elf structure
            //We don't want to add LABEL_PATCHMOV to the table as we update it manually
            if (newsym[i].st_value != 0 && scn->table_entries[i]->entry_32->name
                  && strlen (scn->table_entries[i]->entry_32->name)
                  && strcmp (scn->table_entries[i]->entry_32->name, LABEL_PATCHMOV))
            {
               currenttarget = elffile_upd_targetaddr(efile, scn->table_entries[i]->entry_32->elfentry.sym,
                     &newsym[i], newsym[i].st_value);
               if (currenttarget)
               {
                  if (lasttarget)
                  lc_free(lasttarget);
                  lasttarget = currenttarget;
               }
               else
               elffile_add_targetscnoffset(efile, &newsym[i], lasttarget->scnindx, newsym[i].st_value
                     - Elf_Shdr_get_sh_addr (efile, lasttarget->scnindx, NULL), GElf_Sym_updaddr, tableidx, i);
            }

            //Updates the address of the elf structure
            scn->table_entries[i]->entry_32->elfentry.sym = &newsym[i];
            scn->table_entries[i]->entry_32->updated = TRUE;
         }
         if (lasttarget)
         lc_free(lasttarget);
         lc_free(scn->bits);
         scn->bits = (void*)newsym;
         Elf_Shdr_set_sh_size (NULL, 0, scn->scnhdr, sizeof(Elf32_Sym)*scn->nentries);
      }

      break;
      case(RELATABLE):
      //Updating ELF relocation structure
      //Creates the new ELF table
      if (Elf_Ehdr_get_e_ident (efile, NULL)[EI_CLASS] == ELFCLASS64)
      {
         Elf64_Rela *newrela = (Elf64_Rela*)lc_malloc(sizeof(Elf64_Rela)*scn->nentries);
         for (i = 0; i < scn->nentries; i++)
         {
            newrela[i] = *(scn->table_entries[i]->entry_64->elfentry.rela);
            //Updates hashtable with new elf structure
            if(newrela[i].r_offset!=0)
            {
               currenttarget = elffile_upd_targetaddr(efile,scn->table_entries[i]->entry_64->elfentry.rela,&newrela[i],
                     newrela[i].r_offset);
               if (currenttarget)
               {
                  if (lasttarget)
                  lc_free(lasttarget);
                  lasttarget = currenttarget;
               }
               else
               elffile_add_targetscnoffset(efile,&newrela[i],lasttarget->scnindx,newrela[i].r_offset
                     - Elf_Shdr_get_sh_addr (efile, lasttarget->scnindx, NULL), GElf_Rela_updaddr,tableidx,i);
            }
            if (scn->table_entries[i]->entry_64->updated == FALSE)
            lc_free(scn->table_entries[i]->entry_64->elfentry.rela);

            //Updates the address of the elf structure
            scn->table_entries[i]->entry_64->elfentry.rela = &newrela[i];
            scn->table_entries[i]->entry_64->updated = TRUE;
         }
         if (lasttarget)
         lc_free(lasttarget);
         lc_free(scn->bits);
         scn->bits = (void*)newrela;
         Elf_Shdr_set_sh_size (NULL, 0, scn->scnhdr, sizeof(Elf64_Rela)*scn->nentries);
      }
      else if (Elf_Ehdr_get_e_ident (efile, NULL)[EI_CLASS] == ELFCLASS32)
      {
         Elf32_Rela *newrela = (Elf32_Rela*)lc_malloc(sizeof(Elf32_Rela)*scn->nentries);
         for (i = 0; i < scn->nentries; i++)
         {
            newrela[i] = *(scn->table_entries[i]->entry_32->elfentry.rela);
            //Updates hashtable with new elf structure
            if(newrela[i].r_offset!=0)
            {
               currenttarget = elffile_upd_targetaddr(efile,scn->table_entries[i]->entry_32->elfentry.rela,&newrela[i],
                     newrela[i].r_offset);
               if (currenttarget)
               {
                  if (lasttarget)
                  lc_free(lasttarget);
                  lasttarget = currenttarget;
               }
               else
               elffile_add_targetscnoffset(efile,&newrela[i],lasttarget->scnindx,newrela[i].r_offset
                     - Elf_Shdr_get_sh_addr (efile, lasttarget->scnindx, NULL), GElf_Rela_updaddr,tableidx,i);
            }
            if (scn->table_entries[i]->entry_32->updated == FALSE)
            lc_free(scn->table_entries[i]->entry_32->elfentry.rela);

            //Updates the address of the elf structure
            scn->table_entries[i]->entry_32->elfentry.rela = &newrela[i];
            scn->table_entries[i]->entry_32->updated = TRUE;
         }
         if (lasttarget)
         lc_free(lasttarget);
         lc_free(scn->bits);
         scn->bits = (void*)newrela;
         Elf_Shdr_set_sh_size (NULL, 0, scn->scnhdr, sizeof(Elf32_Rela)*scn->nentries);
      }
      break;
      case(RELTABLE):
      if (Elf_Ehdr_get_e_ident (efile, NULL)[EI_CLASS] == ELFCLASS64)
      {
         //Updating ELF relocation structure
         //Creates the new ELF table
         Elf64_Rel* newrel = (Elf64_Rel*)lc_malloc(sizeof(Elf64_Rel)*scn->nentries);
         for(i=0;i<scn->nentries;i++)
         {
            newrel[i] = *(scn->table_entries[i]->entry_64->elfentry.rel);
            //Updates hashtable with new elf structure
            if(newrel[i].r_offset!=0)
            {
               elffile_upd_targetaddr(efile,scn->table_entries[i]->entry_64->elfentry.rel,&newrel[i],newrel[i].r_offset);
               if (currenttarget)
               {
                  if (lasttarget)
                  lc_free(lasttarget);
                  lasttarget = currenttarget;
               }
               else
               elffile_add_targetscnoffset(efile,&newrel[i],lasttarget->scnindx,newrel[i].r_offset
                     - Elf_Shdr_get_sh_addr (efile, lasttarget->scnindx, NULL), GElf_Rel_updaddr,tableidx,i);
            }
            //       if (scn->table_entries[i].updated == FALSE)     //Don't delete
            //          free(scn->table_entries[i].elfentry.rel); //First time the array of relocations is replaced
            /**\bug This free causes a seg fault if the function is invoked more than once
             * because at this time elfentry.rel contains something not created with malloc*/
            //Updates the address of the elf structure
            scn->table_entries[i]->entry_64->elfentry.rel = &newrel[i];
            scn->table_entries[i]->entry_64->updated = TRUE;
         }
         if (lasttarget)
         lc_free(lasttarget);
         lc_free(scn->bits);
         scn->bits = (void*)newrel;
         Elf_Shdr_set_sh_size (NULL, 0, scn->scnhdr, sizeof(Elf64_Rel)*scn->nentries);
      }
      else if (Elf_Ehdr_get_e_ident (efile, NULL)[EI_CLASS] == ELFCLASS32)
      {
         //Updating ELF relocation structure
         //Creates the new ELF table
         Elf32_Rel* newrel = (Elf32_Rel*)lc_malloc(sizeof(Elf32_Rel)*scn->nentries);
         for(i=0;i<scn->nentries;i++)
         {
            newrel[i] = *(scn->table_entries[i]->entry_32->elfentry.rel);
            //Updates hashtable with new elf structure
            if(newrel[i].r_offset!=0)
            {
               elffile_upd_targetaddr(efile,scn->table_entries[i]->entry_32->elfentry.rel,&newrel[i],newrel[i].r_offset);
               if (currenttarget)
               {
                  if (lasttarget)
                  lc_free(lasttarget);
                  lasttarget = currenttarget;
               }
               else
               elffile_add_targetscnoffset(efile,&newrel[i],lasttarget->scnindx,newrel[i].r_offset
                     - Elf_Shdr_get_sh_addr (efile, lasttarget->scnindx, NULL), GElf_Rel_updaddr,tableidx,i);
            }
            //       if (scn->table_entries[i].updated == FALSE)     //Don't delete
            //          free(scn->table_entries[i].elfentry.rel); //First time the array of relocations is replaced
            /**\bug This free causes a seg fault if the function is invoked more than once
             * because at this time elfentry.rel contains something not created with malloc*/
            //Updates the address of the elf structure
            scn->table_entries[i]->entry_32->elfentry.rel = &newrel[i];
            scn->table_entries[i]->entry_32->updated = TRUE;
         }
         if (lasttarget)
         lc_free(lasttarget);
         lc_free(scn->bits);
         scn->bits = (void*)newrel;
         Elf_Shdr_set_sh_size (NULL, 0, scn->scnhdr, sizeof(Elf32_Rel)*scn->nentries);
      }
      break;
   }
}

/**
 * Adds a relocation entry to a file to a specified symbol name and to the specified address
 * \param efile Structure holding the parsed ELF file
 * \param reloidx Index of the section holding the relocation table to be updated
 * \param symname Name of the symbol to be linked with the relocation entry
 * \param addr Address pointed to by the relocation entry
 * \param offset Offset inside the instruction's coding at which the relocation points (if applicable)
 * \param relotype type of relocation (as defined in the ELF format)
 * \param addend Addend value for SHT_RELA relocations)
 * \param instr Pointer to the instruction pointed to by the relocation (can be NULL)
 * \return The size of the relocation entry
 * \todo Figure out how addend actually works, and update the calls to this function accordingly
 * \todo Factorise the code of this function, possibly using macros for handling the different but similar data types
 */
static int elffile_add_reloc(elffile_t* efile, int reloidx,char* symname, int64_t addr, int offset, int relotype, int addend,insn_t* instr)
{
   int symidx,symentidx,relosize=0;
   if( (!efile) || (!symname) )
   return -1;

   if (Elf_Shdr_get_sh_type (efile, reloidx, NULL) == SHT_RELA)
   {
      symidx = Elf_Shdr_get_sh_link (efile, reloidx, NULL);

      //Looks for a symbol table entry with the same name
      for (symentidx = 0; symentidx < efile->sections[symidx]->nentries; symentidx++)
      {
         tableentry_t* te = efile->sections[symidx]->table_entries[symentidx];
         if ((te->entry_64 != NULL && str_equal(symname, te->entry_64->name))
               || (te->entry_32 != NULL && str_equal(symname, te->entry_32->name)))
         break;
         //symentry_name can not be used, as it relies on strptr, while we are here in the process
         // of inserting entries, therefore the symbol table data is not updated
      }
      if (symentidx < efile->sections[symidx]->nentries)
      {
         /*An entry with this name was found : the relocation is added*/
         efile->sections[reloidx]->table_entries = (tableentry_t**)lc_realloc(
               efile->sections[reloidx]->table_entries,
               sizeof(tableentry_t*)*(efile->sections[reloidx]->nentries+1));
         efile->sections[reloidx]->table_entries[efile->sections[reloidx]->nentries] = lc_malloc (sizeof(tableentry_t));
         if (Elf_Ehdr_get_e_ident (efile, NULL)[EI_CLASS] == ELFCLASS64)
         {
            efile->sections[reloidx]->table_entries[efile->sections[reloidx]->nentries]->entry_64 = lc_malloc (sizeof(tableentry_64_t));
            efile->sections[reloidx]->table_entries[efile->sections[reloidx]->nentries]->entry_32 = NULL;
            efile->sections[reloidx]->table_entries[efile->sections[reloidx]->nentries]->entry_64->name = lc_strdup(symname);
            efile->sections[reloidx]->table_entries[efile->sections[reloidx]->nentries]->entry_64->addr = addr;
            efile->sections[reloidx]->table_entries[efile->sections[reloidx]->nentries]->entry_64->type = RELATABLE;
            efile->sections[reloidx]->table_entries[efile->sections[reloidx]->nentries]->entry_64->offset = offset;
            efile->sections[reloidx]->table_entries[efile->sections[reloidx]->nentries]->entry_64->instr = instr;
            efile->sections[reloidx]->table_entries[efile->sections[reloidx]->nentries]->entry_64->updated = FALSE; //Will be set to TRUE in elffile_upd_tbldata
            efile->sections[reloidx]->table_entries[efile->sections[reloidx]->nentries]->entry_64->elfentry.rela = (Elf64_Rela*)lc_malloc(sizeof(Elf64_Rela));
            efile->sections[reloidx]->table_entries[efile->sections[reloidx]->nentries]->entry_64->elfentry.rela->r_offset = addr;
            efile->sections[reloidx]->table_entries[efile->sections[reloidx]->nentries]->entry_64->elfentry.rela->r_info = ELF64_R_INFO(symentidx,relotype);
            efile->sections[reloidx]->table_entries[efile->sections[reloidx]->nentries]->entry_64->elfentry.rela->r_addend = addend;
            /*Adds the new relocation entry to the hashtable containing the relocation table*/
            hashtable_insert(efile->sections[reloidx]->hentries,(void *)addr,&efile->sections[reloidx]->table_entries[efile->sections[reloidx]->nentries]);
            efile->sections[reloidx]->nentries++;
            elffile_upd_tbldata(efile,reloidx,RELATABLE);
            relosize = sizeof(Elf64_Rela);
         }
         else if (Elf_Ehdr_get_e_ident (efile, NULL)[EI_CLASS] == ELFCLASS32)
         {
            efile->sections[reloidx]->table_entries[efile->sections[reloidx]->nentries]->entry_64 = NULL;
            efile->sections[reloidx]->table_entries[efile->sections[reloidx]->nentries]->entry_32 = lc_malloc (sizeof(tableentry_32_t));
            efile->sections[reloidx]->table_entries[efile->sections[reloidx]->nentries]->entry_32->name = lc_strdup(symname);
            efile->sections[reloidx]->table_entries[efile->sections[reloidx]->nentries]->entry_32->addr = addr;
            efile->sections[reloidx]->table_entries[efile->sections[reloidx]->nentries]->entry_32->type = RELATABLE;
            efile->sections[reloidx]->table_entries[efile->sections[reloidx]->nentries]->entry_32->offset = offset;
            efile->sections[reloidx]->table_entries[efile->sections[reloidx]->nentries]->entry_32->instr = instr;
            efile->sections[reloidx]->table_entries[efile->sections[reloidx]->nentries]->entry_32->updated = FALSE; //Will be set to TRUE in elffile_upd_tbldata
            efile->sections[reloidx]->table_entries[efile->sections[reloidx]->nentries]->entry_32->elfentry.rela = (Elf32_Rela*)lc_malloc(sizeof(Elf32_Rela));
            efile->sections[reloidx]->table_entries[efile->sections[reloidx]->nentries]->entry_32->elfentry.rela->r_offset = addr;
            efile->sections[reloidx]->table_entries[efile->sections[reloidx]->nentries]->entry_32->elfentry.rela->r_info = ELF32_R_INFO(symentidx,relotype);
            efile->sections[reloidx]->table_entries[efile->sections[reloidx]->nentries]->entry_32->elfentry.rela->r_addend = addend;
            /*Adds the new relocation entry to the hashtable containing the relocation table*/
            hashtable_insert(efile->sections[reloidx]->hentries,(void *)addr,&efile->sections[reloidx]->table_entries[efile->sections[reloidx]->nentries]);
            efile->sections[reloidx]->nentries++;
            elffile_upd_tbldata(efile,reloidx,RELATABLE);
            relosize = sizeof(Elf32_Rela);
         }
      }
      else
      {
         WRNMSG("Unable to add relocation for symbol %s: Symbol not found\n",symname);
         efile->last_error_code = ERR_BINARY_SYMBOL_NOT_FOUND;
      }
   }
   else if (Elf_Shdr_get_sh_type (efile, reloidx, NULL) == SHT_REL)
   {
      symidx = Elf_Shdr_get_sh_link (efile, reloidx, NULL);

      //Looks for a symbol table entry with the same name
      for(symentidx = 0; symentidx < efile->sections[symidx]->nentries; symentidx++)
      {
         if(str_equal(symname,symentry_name(efile,symidx,symentidx)))
         break;
      }
      if(symentidx<efile->sections[symidx]->nentries)
      {
         //An entry with this name was found : the relocation is added
         efile->sections[reloidx]->table_entries = (tableentry_t**)lc_realloc(
               efile->sections[reloidx]->table_entries,
               sizeof(tableentry_t*)*(efile->sections[reloidx]->nentries+1));
         efile->sections[reloidx]->table_entries[efile->sections[reloidx]->nentries] = lc_malloc (sizeof(tableentry_t));

         if (Elf_Ehdr_get_e_ident (efile, NULL)[EI_CLASS] == ELFCLASS64)
         {
            efile->sections[reloidx]->table_entries[efile->sections[reloidx]->nentries]->entry_64 = lc_malloc (sizeof(tableentry_64_t));
            efile->sections[reloidx]->table_entries[efile->sections[reloidx]->nentries]->entry_32 = NULL;
            efile->sections[reloidx]->table_entries[efile->sections[reloidx]->nentries]->entry_64->name = lc_strdup(symname);
            efile->sections[reloidx]->table_entries[efile->sections[reloidx]->nentries]->entry_64->addr = addr;
            efile->sections[reloidx]->table_entries[efile->sections[reloidx]->nentries]->entry_64->type = RELTABLE;
            efile->sections[reloidx]->table_entries[efile->sections[reloidx]->nentries]->entry_64->offset = offset;
            efile->sections[reloidx]->table_entries[efile->sections[reloidx]->nentries]->entry_64->instr = instr;
            efile->sections[reloidx]->table_entries[efile->sections[reloidx]->nentries]->entry_64->updated = FALSE; //Will be set to TRUE in elffile_upd_tbldata
            efile->sections[reloidx]->table_entries[efile->sections[reloidx]->nentries]->entry_64->elfentry.rel = (Elf64_Rel*)lc_malloc(sizeof(Elf64_Rel));
            efile->sections[reloidx]->table_entries[efile->sections[reloidx]->nentries]->entry_64->elfentry.rel->r_offset = addr;
            efile->sections[reloidx]->table_entries[efile->sections[reloidx]->nentries]->entry_64->elfentry.rel->r_info = ELF64_R_INFO(symentidx,relotype);
            /*Adds the new relocation entry to the hashtable containing the relocation table*/
            hashtable_insert(efile->sections[reloidx]->hentries,(void *)addr,&efile->sections[reloidx]->table_entries[efile->sections[reloidx]->nentries]);
            efile->sections[reloidx]->nentries++;
            elffile_upd_tbldata(efile,efile->indexes[RELO_IDX],RELTABLE);
            relosize = sizeof(Elf64_Rel);
         }
         else if (Elf_Ehdr_get_e_ident (efile, NULL)[EI_CLASS] == ELFCLASS32)
         {
            efile->sections[reloidx]->table_entries[efile->sections[reloidx]->nentries]->entry_64 = NULL;
            efile->sections[reloidx]->table_entries[efile->sections[reloidx]->nentries]->entry_32 = lc_malloc (sizeof(tableentry_32_t));
            efile->sections[reloidx]->table_entries[efile->sections[reloidx]->nentries]->entry_32->name = lc_strdup(symname);
            efile->sections[reloidx]->table_entries[efile->sections[reloidx]->nentries]->entry_32->addr = addr;
            efile->sections[reloidx]->table_entries[efile->sections[reloidx]->nentries]->entry_32->type = RELTABLE;
            efile->sections[reloidx]->table_entries[efile->sections[reloidx]->nentries]->entry_32->offset = offset;
            efile->sections[reloidx]->table_entries[efile->sections[reloidx]->nentries]->entry_32->instr = instr;
            efile->sections[reloidx]->table_entries[efile->sections[reloidx]->nentries]->entry_32->updated = FALSE; //Will be set to TRUE in elffile_upd_tbldata
            efile->sections[reloidx]->table_entries[efile->sections[reloidx]->nentries]->entry_32->elfentry.rel = (Elf32_Rel*)lc_malloc(sizeof(Elf32_Rel));
            efile->sections[reloidx]->table_entries[efile->sections[reloidx]->nentries]->entry_32->elfentry.rel->r_offset = addr;
            efile->sections[reloidx]->table_entries[efile->sections[reloidx]->nentries]->entry_32->elfentry.rel->r_info = ELF32_R_INFO(symentidx,relotype);
            /*Adds the new relocation entry to the hashtable containing the relocation table*/
            hashtable_insert(efile->sections[reloidx]->hentries,(void *)addr,&efile->sections[reloidx]->table_entries[efile->sections[reloidx]->nentries]);
            efile->sections[reloidx]->nentries++;
            elffile_upd_tbldata(efile,efile->indexes[RELO_IDX],RELTABLE);
            relosize = sizeof(Elf32_Rel);
         }
      }
      else
      {
      }
   } else {
      WRNMSG("Relocation section is not REL or RELA type - No relocation added\n");
      efile->last_error_code = ERR_BINARY_RELOCATION_NOT_SUPPORTED;
   }
   return relosize;
}

/*
 * Returns the index of the last entry in the RELAPLT table
 * \param efile The structure holding the parsed ELF file
 * \return Index of the last entry in the RELAPLT table, or -1 if an error occurred
 */
int elfexe_get_jmprel_lastidx(elffile_t* efile) {
   int out=-1;
   if( efile && (efile->indexes[RELAPLT_IDX]>=0) ) {
      out = efile->sections[efile->indexes[RELAPLT_IDX]]->nentries-1;
   }
   return out;
}

/*
 * Adds a relocation entry to the RELAPLT relocation table (its address is incremented by 8
 * from the last) and updates the size of RELAPLT in the DYNAMIC section
 * \param efile The structure holding the parsed ELF file
 * \param symname The name of the relocation entry
 * \param relidx Pointer which will contain upon return the index of the added relocation in the
 * RELAPLT table
 * \return The address of the added relocation, or -1 if an error occurred.
 * In this case, the last error code in \c efile will be updated
 */
int64_t elfexe_add_jmprel(elffile_t* efile,char* symname,int* relidx)
{
   /**\todo Find a better way to handle the multiple architectures*/
   int64_t lastaddr,nextaddr = -1;
   int relotype,addend;
   if ( efile && (efile->indexes[RELAPLT_IDX] >= 0) )
   {
      //Retrieves the last address in the RELAPLT table
      tableentry_t * te = efile->sections[efile->indexes[RELAPLT_IDX]]->table_entries[efile->sections[efile->indexes[RELAPLT_IDX]]->nentries-1];
      if (te->entry_64 != NULL)
      lastaddr = te->entry_64->addr;
      else if (te->entry_32 != NULL)
      lastaddr = te->entry_32->addr;

      //Calculates the next address
      switch(elffile_getmachine(efile))
      {
         case EM_X86_64:
         nextaddr = lastaddr+8;
         relotype=R_X86_64_JUMP_SLOT;
         addend=0;
         break;
         case EM_K1OM:
         nextaddr = lastaddr+8;
         relotype=R_X86_64_JUMP_SLOT;
         addend=0;
         break;
         default:
         ERRMSG("Architecture with code %d not implemented\n", elffile_getmachine(efile));
         efile->last_error_code = ERR_LIBASM_ARCH_UNKNOWN;
         return -1;
      }
      elffile_add_reloc(efile,efile->indexes[RELAPLT_IDX],symname,nextaddr,0,relotype,addend,NULL);
      if (relidx)
      *relidx = efile->sections[efile->indexes[RELAPLT_IDX]]->nentries - 1;

   }
   else
   {
      ERRMSG("No RELAPLT table found in file %s\n",efile->filename);
      efile->last_error_code = ERR_BINARY_NO_EXTFCTS_SECTION;
      return -1;
   }
   return nextaddr;
}

/**
 * Looks for a string in STRTAB
 * \param efile an ELF file
 * \param stridx index of the STRTAB section
 * \param str a string to look for
 * \return -1 if there is a problem, 0 if the string is not found, else the index of the string
 */
static int is_str_exists_in_strtab (elffile_t* efile, int stridx, const char* str)
{
   if(!efile)
   return -1;
   if ((stridx < 0) || (stridx >= efile->nsections))
   {
      WRNMSG("Unable to check string table: Index %d is out of range\n", stridx);
      return -1;
   }
   if (Elf_Shdr_get_sh_type (efile, stridx, NULL) != SHT_STRTAB)
   {
      WRNMSG("Unable to check string table: Section %d is not a table\n", stridx);
      return -1;
   }

   char* strtab = NULL;
   unsigned int indx = 0;
   unsigned int sh_size = Elf_Shdr_get_sh_size (efile, stridx, NULL);

   while (indx < sh_size)
   {
      strtab = (char*)&((ELF_get_scn_bits(efile, stridx))[indx]);

      if (strcmp (str, strtab) == 0)
      return (indx);
      indx += strlen (strtab) + 1;
   }
   return (0);
}

/**
 * Adds a string to a string table (section at the specified index)
 * \param efile The parsed ELF file
 * \param stridx Index of the section containing the string table
 * \param str String to add
 * \return Index of the added string in the table, or its index if it already exists (does not work correctly)
 * */
int elffile_upd_strtab(elffile_t* efile,int stridx,char* str)
{
   if(!efile)
   return -1;
   if ((stridx < 0) || (stridx >= efile->nsections))
   {
      WRNMSG("Unable to update string table: Index %d is out of range\n",stridx);
      return -1; //Index out of range
   }
   if (Elf_Shdr_get_sh_type (efile, stridx, NULL) != SHT_STRTAB)
   {
      WRNMSG("Unable to update string table: Section %d is not a table\n",stridx);
      return -1; //Section does not contain a string table
   }
   int indx = is_str_exists_in_strtab (efile, stridx, str); // Checks if the string already exist
   if (indx > 0)
   return (indx);// Return its index if it exists, else create it

   Elf_Shdr* shdr = efile->sections[stridx]->scnhdr;
   //Adds the string to the string table
   efile->sections[stridx]->bits = lc_realloc (efile->sections[stridx]->bits,
         (Elf_Shdr_get_sh_size (NULL, 0,shdr) + strlen (str) + 1)* sizeof (char));
   memcpy(&efile->sections[stridx]->bits[Elf_Shdr_get_sh_size (NULL, 0,shdr)], str, (strlen(str) + 1 ) * sizeof (char));

   //Retrieves the index of the newly inserted string in the string table
   indx = Elf_Shdr_get_sh_size (NULL, 0,shdr);

   //Updates string table size
   Elf_Shdr_set_sh_size (NULL, 0, shdr, Elf_Shdr_get_sh_size (NULL, 0, shdr) + strlen(str) + 1);

   return indx;
}

/**
 * Updates or replaces the data in an elf section
 * \param efile The structure holding the parsed ELF file
 * \param scnidx The index of the ELF section
 * \param newdata A pointer to the data to insert into the section
 * \param newsize The size of the new data
 * \param datasize Length of the new data to add (pointed to by newdata)
 * \param replace 1 if the data must replace the existing one, or 0 if it must be appended to it
 */
static void elfsection_upd_data(elffile_t* efile,int scnidx,void* newdata,int newsize,int datasize,int replace)
{
   if(!efile)
   return;
   if (scnidx > 0 && scnidx < Elf_Ehdr_get_e_shnum (efile, NULL))
   {
      if(replace)
      {  //Replacing section data with new value
         lc_free(efile->sections[scnidx]->bits);
         efile->sections[scnidx]->bits = lc_malloc(datasize * newsize);
         //Update size of data depending
         //Data is a table
         if (Elf_Shdr_get_sh_size (efile, scnidx, NULL))
         Elf_Shdr_set_sh_size (efile, scnidx, NULL, newsize * Elf_Shdr_get_sh_size (efile, scnidx, NULL));
         else
         Elf_Shdr_set_sh_size (efile, scnidx, NULL, newsize);
         memcpy(efile->sections[scnidx]->bits, newdata, datasize*newsize);
      }
      else
      {  //Appending new value to section data
         efile->sections[scnidx]->bits = lc_realloc (efile->sections[scnidx]->bits,
               sizeof(efile->sections[scnidx]->bits)*(Elf_Shdr_get_sh_size (efile, scnidx, NULL) + newsize));
         memcpy(efile->sections[scnidx]->bits + Elf_Shdr_get_sh_size (efile, scnidx, NULL),newdata,newsize);

         if(Elf_Shdr_get_sh_entsize (efile, scnidx, NULL))
         Elf_Shdr_set_sh_size (efile, scnidx, NULL, Elf_Shdr_get_sh_size (efile, scnidx, NULL)
               + newsize * Elf_Shdr_get_sh_entsize (efile, scnidx, NULL));
         //Data is a table
         else
         Elf_Shdr_set_sh_size (efile, scnidx, NULL, Elf_Shdr_get_sh_size (efile, scnidx, NULL) + newsize);
      }
      //elf_flagdata(efile->sections[scnidx].scndata,ELF_C_SET,ELF_F_DIRTY);
   }
   else
   {
      WRNMSG("Unable to update data in section number %d: Index out of range\n",scnidx);
   }
}

/*
 * Adds a dynamic library to the elf file
 * \param ef The structure holding the parsed ELF file
 * \param libname The name of the external library to add
 * \param priority 1 if the library should be added at the beginning, else 0
 * \return EXIT_SUCCESS / error code
 */
void elffile_add_dynlib64 (elffile_t* ef, char* libname, int priority)
{
   /**\bug (2015-11-25) WARNING! This function is bugged when priority is 1: the first library is overwritten and ldd
    * reports an error. This is due to the entries in the target table overwriting the updated dynamic table.
    * (It's possible the bug is also present with priority 0, but since the original entries are not changed, we are lucky)
    * The insertion with priority set to 1 is never used, so I'm letting this bug live so far. It should be fixed in the refactoring.*/
   int i, size, strtab = 0, lastdyn = 0;

   if (!ef || (ef->indexes[DYNAMIC_IDX] <= 0))
   return ERR_BINARY_NO_EXTLIBS;

   // get the strtab index
   elfsection_t* scn = ef->sections[ef->indexes[DYNAMIC_IDX]];
   Elf64_Dyn* dyns = (Elf64_Dyn*)scn->bits;
   size = Elf_Shdr_get_sh_size (NULL, 0, scn->scnhdr) / sizeof(Elf64_Dyn);

   // if insert has no priority, it is added at the end of the table
   // else, it is added at the begining. In this case, there is no need
   // to compute the new position.
   if (priority == 0)
   {
      for (i = 0; i < size; i++)
      {
         // Exits when reaching the NULL element (signals the end of the section,
         // but there may be more than one)
         if (dyns[i].d_tag == DT_NULL)
         {
            lastdyn = i;
            break;
         }
      }
   }

   for (i = 0; i < size; i++)
   {
      //dyn[i] contains the virtual address of the string table
      if (dyns[i].d_tag == DT_STRTAB)
      {
         while ((strtab < ef->nsections) &&(Elf_Shdr_get_sh_addr (ef, strtab, 0) != dyns[i].d_un.d_ptr))
         strtab++;
      }
   }

   // create the new dyn element
   Elf64_Dyn newdyn;
   newdyn.d_tag = DT_NEEDED;
   newdyn.d_un.d_val = elffile_upd_strtab(ef, strtab, libname);

   // increase the section bits
   scn->bits = lc_realloc (scn->bits, (Elf_Shdr_get_sh_size (NULL, 0, scn->scnhdr) + sizeof (Elf64_Dyn)));

   // rewrite the last entry on the added bits
   // as memcpy does not support overlaped streams, a buffer is needed
   unsigned char* buffer = lc_malloc (sizeof (Elf64_Dyn) * (size - lastdyn));
   memcpy (buffer, &scn->bits[lastdyn * sizeof (Elf64_Dyn)],
         sizeof (Elf64_Dyn) * (size - lastdyn));

   memcpy (&scn->bits[(lastdyn + 1) * sizeof (Elf64_Dyn)],
         buffer, sizeof (Elf64_Dyn) * (size - lastdyn));
   lc_free (buffer);

   // write the new entry
   memcpy (&scn->bits[lastdyn * sizeof (Elf64_Dyn)],
         &newdyn, sizeof (Elf64_Dyn));

   // update the section header
   Elf_Shdr_set_sh_size (NULL, 0, scn->scnhdr, Elf_Shdr_get_sh_size (NULL, 0, scn->scnhdr) + sizeof (Elf64_Dyn));

   return EXIT_SUCCESS;
}

/*
 * Adds a dynamic library to the elf file
 * \param ef The structure holding the parsed ELF file
 * \param libname The name of the external library to add
 * \param priority 1 if the library should be added at the begining, else 0
 */
void elffile_add_dynlib32 (elffile_t* ef, char* libname, int priority)
{
   int i, size, strtab = 0, lastdyn = 0;
   if (!ef || (ef->indexes[DYNAMIC_IDX] <= 0))
   return;

   // get the strtab index
   elfsection_t* scn = ef->sections[ef->indexes[DYNAMIC_IDX]];
   Elf32_Dyn* dyns = (Elf32_Dyn*)scn->bits;
   size = Elf_Shdr_get_sh_size (NULL, 0, scn->scnhdr) / sizeof(Elf32_Dyn);

   // if insert has no priority, it is added at the end of the table
   // else, it is added at the begining. In this case, there is no need
   // to compute the new position.
   if (priority == 0)
   {
      for (i = 0; i < size; i++)
      {
         // Exits when reaching the NULL element (signals the end of the section,
         // but there may be more than one)
         if (dyns[i].d_tag == DT_NULL)
         {
            lastdyn = i;
            break;
         }
      }
   }

   for (i = 0; i < size; i++)
   {
      //dyn[i] contains the virtual address of the string table
      if (dyns[i].d_tag == DT_STRTAB)
      {
         while ((strtab < ef->nsections) && (Elf_Shdr_get_sh_addr (ef, strtab, 0) != dyns[i].d_un.d_ptr))
         strtab++;
      }
   }

   // create the new dyn element
   Elf64_Dyn newdyn;
   newdyn.d_tag = DT_NEEDED;
   newdyn.d_un.d_val = elffile_upd_strtab(ef, strtab, libname);

   // increase the section bits
   scn->bits = lc_realloc (scn->bits, (Elf_Shdr_get_sh_size (NULL, 0, scn->scnhdr) + sizeof (Elf32_Dyn)));

   // rewrite the last entry on the added bits
   // as memcpy does not support overlaped streams, a buffer is needed
   unsigned char* buffer = lc_malloc (sizeof (Elf32_Dyn) * (size - lastdyn));
   memcpy (buffer, &scn->bits[lastdyn * sizeof (Elf32_Dyn)],
         sizeof (Elf32_Dyn) * (size - lastdyn));

   memcpy (&scn->bits[(lastdyn + 1) * sizeof (Elf32_Dyn)],
         buffer, sizeof (Elf32_Dyn) * (size - lastdyn));
   lc_free (buffer);

   // write the new entry
   memcpy (&scn->bits[lastdyn * sizeof (Elf32_Dyn)],
         &newdyn, sizeof (Elf32_Dyn));

   // update the section header
   Elf_Shdr_set_sh_size (NULL, 0, scn->scnhdr, Elf_Shdr_get_sh_size (NULL, 0, scn->scnhdr) + sizeof (Elf32_Dyn));
}

/*
 * Adds a dynamic library to the elf file
 * \param ef The structure holding the parsed ELF file
 * \param libname The name of the external library to add
 * \param priority 1 if the library should be added at the begining, else 0
 */
void elffile_add_dynlib (elffile_t* ef, char* libname, int priority)
{
   if (Elf_Ehdr_get_e_ident (ef, NULL)[EI_CLASS] == ELFCLASS64)
   elffile_add_dynlib64 (ef, libname, priority);
   else if (Elf_Ehdr_get_e_ident (ef, NULL)[EI_CLASS] == ELFCLASS32)
   elffile_add_dynlib32 (ef, libname, priority);
}

void elffile_add_dynentrie64 (elffile_t *efile, Elf64_Dyn* pnewdyn)
{
   Elf64_Dyn newdyn = *pnewdyn;
   int i,size,lastdyn=0;

   if (efile->indexes[DYNAMIC_IDX] <= 0)
   return;

   elfsection_t* scn = efile->sections[efile->indexes[DYNAMIC_IDX]];
   Elf64_Dyn* dyns = (Elf64_Dyn*)scn->bits;
   size = Elf_Shdr_get_sh_size (NULL, 0, scn->scnhdr) / sizeof(Elf64_Dyn);

   for (i=0; i<size; i++)
   {
      // Exits when reaching the NULL element (signals the end of the section,
      // but there may be more than one)
      if (dyns[i].d_tag == DT_NULL)
      {
         lastdyn = i;
         break;
      }
   }

   // increase the section bits
   scn->bits = lc_realloc (scn->bits, (Elf_Shdr_get_sh_size (NULL, 0, scn->scnhdr) + sizeof (Elf64_Dyn)));

   // rewrite the last entry on the added bits
   // as memcpy does not support overlaped streams, a buffer is needed
   unsigned char* buffer = lc_malloc (sizeof (Elf64_Dyn) * (size - lastdyn));
   memcpy (buffer,
         &scn->bits[lastdyn * sizeof (Elf64_Dyn)],
         sizeof (Elf64_Dyn) * (size - lastdyn));

   memcpy (&scn->bits[(lastdyn + 1) * sizeof (Elf64_Dyn)],
         buffer,
         sizeof (Elf64_Dyn) * (size - lastdyn));
   lc_free (buffer);

   // write the new entry
   memcpy (&scn->bits[lastdyn * sizeof (Elf64_Dyn)],
         &newdyn,
         sizeof (Elf64_Dyn));

   // update the section header
   Elf_Shdr_set_sh_size (NULL, 0, scn->scnhdr, Elf_Shdr_get_sh_size (NULL, 0, scn->scnhdr) + sizeof (Elf64_Dyn));
}

void elffile_add_dynentrie32 (elffile_t *efile, Elf32_Dyn* pnewdyn)
{
   Elf32_Dyn newdyn = *pnewdyn;
   int i,size,lastdyn=0;

   if (efile->indexes[DYNAMIC_IDX] <= 0)
   return;

   elfsection_t* scn = efile->sections[efile->indexes[DYNAMIC_IDX]];
   Elf32_Dyn* dyns = (Elf32_Dyn*)scn->bits;
   size = Elf_Shdr_get_sh_size (NULL, 0, scn->scnhdr) / sizeof(Elf32_Dyn);

   for (i=0; i<size; i++)
   {
      // Exits when reaching the NULL element (signals the end of the section,
      // but there may be more than one)
      if (dyns[i].d_tag == DT_NULL)
      {
         lastdyn = i;
         break;
      }
   }

   // increase the section bits
   scn->bits = lc_realloc (scn->bits, (Elf_Shdr_get_sh_size (NULL, 0, scn->scnhdr) + sizeof (Elf64_Dyn)));

   // rewrite the last entry on the added bits
   // as memcpy does not support overlaped streams, a buffer is needed
   unsigned char* buffer = lc_malloc (sizeof (Elf32_Dyn) * (size - lastdyn));
   memcpy (buffer, &scn->bits[lastdyn * sizeof (Elf32_Dyn)],
         sizeof (Elf32_Dyn) * (size - lastdyn));

   memcpy (&scn->bits[(lastdyn + 1) * sizeof (Elf32_Dyn)],
         buffer, sizeof (Elf32_Dyn) * (size - lastdyn));
   lc_free (buffer);

   // write the new entry
   memcpy (&scn->bits[lastdyn * sizeof (Elf32_Dyn)],
         &newdyn, sizeof (Elf32_Dyn));

   // update the section header
   Elf_Shdr_set_sh_size (NULL, 0, scn->scnhdr, Elf_Shdr_get_sh_size (NULL, 0, scn->scnhdr) + sizeof (Elf32_Dyn));
}

void elffile_add_dynentrie (elffile_t *efile, void* newdyn)
{
   if (Elf_Ehdr_get_e_ident (efile, NULL)[EI_CLASS] == ELFCLASS64)
   elffile_add_dynentrie64 (efile, newdyn);
   else if (Elf_Ehdr_get_e_ident (efile, NULL)[EI_CLASS] == ELFCLASS32)
   elffile_add_dynentrie32 (efile, newdyn);
}

static void elffile_add_scn(elffile_t* efile,char* name,Elf64_Word type,Elf64_Word flags,
      Elf64_Addr addr, Elf64_Addr offset,Elf64_Word link,Elf64_Word info,Elf64_Word align,int64_t size,unsigned char* data)
{
   int nameidx;
   if(!efile)
   return;

   /*Adds the section name to the section names table*/
   nameidx = elffile_upd_strtab (efile, Elf_Ehdr_get_e_shstrndx (efile, NULL), name);
   if(nameidx < 0)
   WRNMSG("Unable to add name %s to section names\n",name); /*Error while adding the section name*/

   /*Updates the elffile structure*/
   efile->sections = (elfsection_t**)lc_realloc(efile->sections,sizeof(elfsection_t*)*(efile->nsections+1));
   efile->sections[efile->nsections] = lc_malloc (sizeof(elfsection_t));
   efile->sections[efile->nsections]->nentries=0;
   efile->sections[efile->nsections]->table_entries = NULL;
   efile->sections[efile->nsections]->bits = lc_malloc(size);
   if (type != SHT_NOBITS)
   {
      if (data == NULL)
      memset(efile->sections[efile->nsections]->bits, 0, size);
      else
      memcpy(efile->sections[efile->nsections]->bits, data, size);
   }

   Elf_Shdr *nshdr = lc_malloc (sizeof (Elf_Shdr));
   if (Elf_Ehdr_get_e_ident(efile, NULL)[EI_CLASS] == ELFCLASS64)
   {
      nshdr->scnhdr32 = NULL;
      nshdr->scnhdr64 = lc_malloc(sizeof(Elf64_Shdr));
   }
   else if (Elf_Ehdr_get_e_ident(efile, NULL)[EI_CLASS] == ELFCLASS32)
   {
      nshdr->scnhdr64 = NULL;
      nshdr->scnhdr32 = lc_malloc(sizeof(Elf32_Shdr));
   }

   Elf_Shdr_set_sh_name (NULL, 0, nshdr, nameidx);
   Elf_Shdr_set_sh_type (NULL, 0, nshdr, type);
   Elf_Shdr_set_sh_flags (NULL, 0, nshdr, flags);
   Elf_Shdr_set_sh_addr (NULL, 0, nshdr, addr);
   Elf_Shdr_set_sh_offset (NULL, 0, nshdr, offset);
   Elf_Shdr_set_sh_size (NULL, 0, nshdr, size);
   Elf_Shdr_set_sh_link (NULL, 0, nshdr, link);
   Elf_Shdr_set_sh_info (NULL, 0, nshdr, info);
   Elf_Shdr_set_sh_addralign (NULL, 0, nshdr, align);
   Elf_Shdr_set_sh_entsize (NULL, 0, nshdr, 0);
   efile->sections[efile->nsections]->scnhdr = nshdr;
   efile->nsections++;
}

/**
 * Update the byte stream in an elffile in the given section
 * \param efile Structure holding the parsed ELF file
 * \param progbits New byte array
 * \param len Size of the byte array
 * \param scndix Index of the section whose byte stream we want to update
 */
void elffile_upd_progbits (elffile_t* efile, unsigned char* progbits, int len, int scnidx)
{
   if (efile && (scnidx >= 0) && (scnidx < efile->nsections))
   {
      //Elf64_Shdr *scnhdr = efile->sections[scnidx]->scnhdr;
      ELF_update_section (efile, scnidx, 0, len, progbits);

      // if sizes are differents, update the header

      if ((unsigned int)len != Elf_Shdr_get_sh_size (efile, scnidx, NULL))
      Elf_Shdr_set_sh_size (efile, scnidx, NULL, len);
   }
}

/**
 * Links a table entry to an instruction if their address match
 * \param entry Structure holding the table entry
 * \param inst Instruction to try to link
 * \param progindx Index of the section holding the instruction
 * \return 1 if the match has been made, 0 otherwise
 */
static int tableentry_insnlink(tableentry_t *entry,insn_t* inst,int progindx)
{
   int result = 0, offset = 0;
   if (entry && entry->entry_64 != NULL
         && (entry->entry_64->type != SYMTABLE
               || (entry->entry_64->type == SYMTABLE
                     && entry->entry_64->elfentry.sym->st_shndx == progindx
                     && strlen (entry->entry_64->name) > 0)))
   {
      //Check if the instruction has the same address as the entry table
      if (insn_get_addr(inst) == (int64_t)entry->entry_64->addr)
      {
         entry->entry_64->instr = inst;
         entry->entry_64->offset = 0;
         result = 1;
      }
      else
      {
         //Checks if the entry table points in the middle of the instruction
         // (this is a very special case, happening only for relocations in object files)
         offset = (entry->entry_64->addr-insn_get_addr (inst)) * 8;
         if ((offset > 0) && (offset < insn_get_size(inst)))
         {
            entry->entry_64->instr = inst;
            entry->entry_64->offset = offset;
            result = 1;
         }
      }
   }
   else if (entry && entry->entry_32 != NULL
         && (entry->entry_32->type != SYMTABLE
               || (entry->entry_32->type == SYMTABLE
                     && entry->entry_32->elfentry.sym->st_shndx == progindx
                     && strlen (entry->entry_32->name) > 0)))
   {
      //Check if the instruction has the same address as the entry table
      if (insn_get_addr(inst) == (int64_t)entry->entry_32->addr)
      {
         entry->entry_32->instr = inst;
         entry->entry_32->offset = 0;
         result = 1;
      }
      else
      {
         //Checks if the entry table points in the middle of the instruction
         // (this is a very special case, happening only for relocations in object files)
         offset = (entry->entry_32->addr-insn_get_addr (inst)) * 8;
         if ((offset > 0) && (offset < insn_get_size(inst)))
         {
            entry->entry_32->instr = inst;
            entry->entry_32->offset = offset;
            result = 1;
         }
      }
   }
   return result;
}

/*
 * Links an instruction with the tables of a Elf file
 * \param efile Structure holding the parsed ELF file
 * \param inst Instruction to tentatively link
 * \param secindx Index of the section holding the instruction
 */
void elffile_insnlink (elffile_t* efile, insn_t* inst, int secindx)
{
   /**\todo Improve this function to avoid scanning all the tables for each instruction*/
   int i = 0;
   tableentry_t* entry = NULL;

   if (!efile)
   return;

   //Linking to the symtable
   if (efile->indexes[SYM] >= 0)
   {
      //Looks for links in the hashtable containing the symtable entries
      entry = hashtable_lookup (efile->sections[efile->indexes[SYM]]->hentries, (void *)INSN_GET_ADDR(inst));
      if(entry)
      {  //Entry found: linking to the instruction
         tableentry_insnlink (entry, inst, secindx);
      }
      //If no entry was found, there is no use looking for an entry spanning over the instruction's address, as
      //entries from the symbol table don't point to the middle of instructions like the relocation entries

   }
   i = 0;

   //Linking to the relocation table
   if (efile->indexes[RELO_IDX] >= 0)
   {
      //Looks for links in the hashtable containing the relocation table entries
      entry = hashtable_lookup (efile->sections[efile->indexes[RELO_IDX]]->hentries, (void *)insn_get_addr(inst));
      if(entry)
      {  //Entry found: linking to the instruction
         tableentry_insnlink (entry, inst, secindx);
      }
      else if (Elf_Ehdr_get_e_type(efile, NULL) == ET_REL)
      {
         //No entry found in a relocatable file: try finding an entry spanning
         // over the instruction's address (can happen for pointers)
         //Tries linking to the relocation table (stops after one match)
         while ((i < efile->sections[efile->indexes[RELO_IDX]]->nentries)
               &&(!tableentry_insnlink (efile->sections[efile->indexes[RELO_IDX]]->table_entries[i], inst,secindx)))
         i++;
      }
   }
   i = 0;

   //Linking to the relocation table
   if (efile->indexes[RELA_IDX] >= 0)
   {
      //Looks for links in the hashtable containing the relocation table entries
      entry = hashtable_lookup (efile->sections[efile->indexes[RELA_IDX]]->hentries, (void *)insn_get_addr(inst));
      if (entry)
      {  //Entry found: linking to the instruction
         tableentry_insnlink (entry, inst, secindx);
      }
      else if (Elf_Ehdr_get_e_type(efile, NULL) == ET_REL)
      {
         //No entry found in a relocatable file: try finding an entry spanning
         // over the instruction's address (can happen for pointers)
         //Tries linking to the relocation table (stops after one match)
         while ((i < efile->sections[efile->indexes[RELA_IDX]]->nentries)
               &&(!tableentry_insnlink (efile->sections[efile->indexes[RELA_IDX]]->table_entries[i], inst, secindx)))
         i++;
      }
   }
}

/*
 * Returns the type of machine for which this ELF file is compiled.
 * \param efile The structure holding the parsed ELF file
 * \return Coding (as used by libelf) corresponding to the type of machine or -1 if \e efile is NULL or not parsed
 */
int elffile_getmachine(elffile_t* efile) {
   return (Elf_Ehdr_get_e_machine (efile, NULL));
}

/*
 * Returns the type of the ELF file (shared, executable, relocatable)
 * \param efile The structure holding the parsed ELF file
 * \return Coding (as used by libelf) corresponding to the type of file or -1 if \e efile is NULL or not parsed
 * */
int elffile_gettype(elffile_t* efile) {
   return (Elf_Ehdr_get_e_type (efile, NULL));
}

/*
 * Returns the number of sections in an ELF file
 * \param efile The structure holding the parsed ELF file
 * \return Number of sections or -1 if \e efile is NULL
 * */
int elffile_getnscn(elffile_t* efile) {
   return(efile?efile->nsections:-1);
}

/*
 * Returns the index of the .plt section (if present)
 * \param efile The structure holding the parsed ELF file
 * \return Index in the elf file of the section holding the procedure linkage table or -1 if \e efile is NULL
 * */
int elffile_getpltidx(elffile_t* efile) {
   return(efile?efile->indexes[PLT_IDX]:-1);
}

/*
 * Returns the index of the first .tls section
 * \param efile The structure holding the parsed ELF file
 * \return Index of the first .tls section
 * WARNING This is SHIT as the first tls section is not necessarily the first in memory
 */
int64_t elffile_gettlsaddress(elffile_t* efile) {
   int indx;
   int64_t address = 0;

   if (!efile)
   return -1;

   for (indx = 0; indx < efile->nsections; indx++) {
      if (elfsection_istls(efile->sections[indx]) && elffile_getscnstartaddr(efile, indx) + elffile_getscnsize(efile, indx) > address)
      address = elffile_getscnstartaddr(efile, indx) + elffile_getscnsize(efile, indx);
   }

   return address;
}

/*
 * Returns the size of the tls sections
 * \param efile The structure holding the parsed ELF file
 * \return The total size of the tls sections
 */
int64_t elffile_gettlssize(elffile_t* efile) {
   int indx;
   int64_t tls_size = 0;

   if (!efile)
   return -1;

   for (indx = 0; indx < efile->nsections; indx++) {
      if (elfsection_istls(efile->sections[indx]))
      tls_size += elffile_getscnsize(efile, indx);
   }

   return tls_size;
}

/*
 * Retrieves the ELF section with a given name
 * \param ef The structure holding the parsed ELF file
 * \param name The name of the section to be looked for
 * \return Index of the section in the file or -1 if no section with such name was found
 */
int elffile_get_scnbyname (elffile_t* ef,char *name)
{
   /**\note A hashtable may be more efficient for such a search, but since there are at most a few dozen
    * of sections per file, the gain would not be very important*/
   int indx = 0;
   if(!ef)
   return -1;
   while(indx<ef->nsections)
   {
      if(str_equal (elfsection_name (ef,indx), name))
      break;
      indx++;
   }
   if (indx == ef->nsections)
   indx=-1;
   return indx;
}

/**
 * Retrieves a block of data from a given section from a certain offset from the beginning of the section
 * \param ef The structure holding the parsed ELF file
 * \param scnidx The index of the section whose data is to be retrieved
 * \param offs The offset from the beginning of the section at which the block is to be retrieved
 * \param len The length (in byte) of the block to retrieve
 * \return An byte array of length len containing the data, or NULL if one parameter is incorrect
 * (section index or offset out of range, or size greater than the section's)
 * */
static unsigned char* elfsection_get_databytes (elffile_t* ef, int scnidx, int64_t offs, int len)
{
   if ((!ef)||(scnidx < 0)||(scnidx >= ef->nsections))
   return NULL;
   int64_t sh_size = Elf_Shdr_get_sh_size (ef, scnidx, NULL);

   if (offs > (int64_t)sh_size || offs + len > (int64_t)sh_size)
   return NULL;
   unsigned char* datab = (unsigned char*)lc_malloc(sizeof(unsigned char)*len);
   memcpy (datab,&ELF_get_scn_bits(ef,scnidx)[offs],len);
   return datab;
}

/*
 * Returns an array of the indexes of the ELF section containing executable code (type=PROGBITS and executable flag is set)
 * \param efile The structure holding the parsed ELF file
 * \param progscnsz Return parameter. If not NULL, will contain the size of the array
 * \return An allocated array containing the indexes of ELF sections containing executable code
 * */
int* elffile_getprogscns(elffile_t* efile, int* progscnsz) {
   int* progscns = NULL;
   int i, nscn = 0;
   /*Retrieving the identifiers of sections containing code*/
   for (i = 0; i < elffile_getnscn(efile); i++) {
      if (elffile_scnisprog(efile, i) == TRUE) {
         progscns = (int*) lc_realloc(progscns, sizeof(int) * (nscn + 1));
         progscns[nscn] = i;
         DBGMSG("ELF section %d contains code\n",progscns[nscn]);
         nscn++;
      }
   }
   DBGMSG("Found %d ELF sections containing code\n",nscn);
   if (progscnsz) *progscnsz = nscn;
   return progscns;
}

/*
 * Updates all targets in an elf file (using the target hashtable)
 * \param efile The structure holding the parsed ELF file
 */
void elffile_updtargets (elffile_t* efile)
{
   if(efile)
   hashtable_foreach (efile->targets, targets_updaddr, efile);
}

/*
 * Returns the name of the file the structure contains the parsing of
 * \param efile Structure containing the parsed ELF file
 * \return The name of the original ELF file or NULL if \e efile is NULL
 */
char* elffile_getfilename(elffile_t* efile) {
   return (efile != NULL)?efile->filename:PTR_ERROR;
}

/*
 * Returns the label associated to an address (if such a label exists)
 * \param efile The structure holding the parsed ELF file
 * \param addr The address at which we are looking for a label
 * \param secnum The section spanning over the address
 * \return The label or NULL if no label is associated to this address
 */
char* elffile_getlabel (elffile_t* efile, int64_t addr, int secnum)
{
   char* answer = NULL;

   if (efile && efile->indexes[SYM] > -1)
   {
      //Look into the hashtable for an entry at that address
      tableentry_t* symlbl = hashtable_lookup (efile->sections[efile->indexes[SYM]]->hentries, (void *)addr);
      if (symlbl && symlbl->entry_64 != NULL && symlbl->entry_64->elfentry.sym->st_shndx == secnum)
      answer = symlbl->entry_64->name;
      else if (symlbl && symlbl->entry_32 != NULL && symlbl->entry_32->elfentry.sym->st_shndx == secnum)
      answer = symlbl->entry_32->name;
   }
   return answer;
}

/*
 * Returns a queue of all undefined symbols in a parsed ELF file.
 * This function is intended to be used with relocatable files
 * \param efile Pointer to a structure holding the parsed ELF file
 * \return Queue of the names of symbols whose index is undefined, or NULL if \e efile is NULL
 * */
queue_t* elffile_getsymbols_undefined(elffile_t* efile) {
   queue_t* out = NULL;
   if (efile != NULL)
   {
      out = queue_new();
      FOREACH_INHASHTABLE(efile->labels, iter)
      {
         tableentry_t* label = GET_DATA_T(tableentry_t*, iter);
         if ( ( label->type == SYMTABLE ) && ( label->elfentry.sym->st_shndx == SHN_UNDEF ) && ( label->name != NULL )
               && (ELF64_ST_BIND(label->elfentry.sym->st_info) != STB_WEAK) ) {
            /**\todo (2015-10-28) WARNING! The exclusion of weak symbols is a quick and dirty way to prevent the patcher to
             * return an error when performing static patching. The correct way to do it is to indeed flag those symbols
             * as being undefined, but not raise an error if no match was found for them either*/
            DBGMSG("File %s contains undefined symbol %s\n",efile->filename, label->entry_64->name);
            queue_add_tail(out, label->entry_64->name);
         }
         else if (label->entry_32 != NULL
               && label->entry_32->type == SYMTABLE
               && label->entry_32->elfentry.sym->st_shndx == SHN_UNDEF
               && label->entry_32->name != NULL)
         {
            DBGMSG("File %s contains undefined symbol %s\n",efile->filename, label->entry_32->name);
            queue_add_tail(out, label->entry_32->name);
         }
      }
   }
   return out;
}

/*
 * Checks if a given symbol is defined in an ELF file
 * This function is mainly intended to be used on relocatable files (otherwise lookup into efile->labels is enough)
 * \param efile Pointer to a structure holding the parsed ELF file
 * \param symname Name of the symbol to look up to
 * \return TRUE if the symbol is defined in the file, FALSE otherwise
 * */
int elffile_checksymbol_defined(elffile_t* efile, char* symname)
{
   int out = FALSE;
   if (efile != NULL && symname != NULL)
   {
      //First checking if the file is dynamic and if the symbol is defined as a dynamic symbol
      if ( efile->indexes[DYNSYM_IDX] > 0 )
      {
         int i = 0;
         elfsection_t* dynsym = efile->sections[efile->indexes[DYNSYM_IDX]];
         for (i=0; i < dynsym->nentries; i++) {
            if ((dynsym->table_entries[i]->entry_64 != NULL && str_equal(dynsym->table_entries[i]->entry_64->name, symname))
                  || (dynsym->table_entries[i]->entry_32 != NULL && str_equal(dynsym->table_entries[i]->entry_32->name, symname)))
            {
               out = TRUE;
               break;
            }
         }
      }
      //Now looking up in the symbol table
      if (out == FALSE)
      {
         tableentry_t* symentry = hashtable_lookup(efile->labels, symname);
         if (symentry != NULL
               && symentry->entry_64 != NULL
               && symentry->entry_64->type == SYMTABLE
               && symentry->entry_64->elfentry.sym->st_shndx != SHN_UNDEF)
         {
            DBGMSG("File %s contains definition of symbol %s\n", efile->filename, symentry->entry_64->name);
            out = TRUE;
         }
         else if (symentry != NULL
               && symentry->entry_32 != NULL
               && symentry->entry_32->type == SYMTABLE
               && symentry->entry_32->elfentry.sym->st_shndx != SHN_UNDEF)
         {
            DBGMSG("File %s contains definition of symbol %s\n", efile->filename, symentry->entry_32->name);
            out = TRUE;
         }
      }
   }
   return out;
}

/*
 * Returns the address associated to a label (if the label exists)
 * \param efile The structure holding the parsed ELF file
 * \param label The label to be looked for
 * \return The address associated to this label or -1 if the label was not found
 * */
int64_t elffile_getlabeladdr(elffile_t* efile,char* label) {
   int64_t answer = -1;
   tableentry_t* lblentry;

   lblentry = hashtable_lookup(efile->labels,label);
   if(lblentry)
   {
      if (lblentry->entry_64 != NULL)
      answer = lblentry->entry_64->addr;
      else if (lblentry->entry_32 != NULL)
      answer = lblentry->entry_32->addr;
   }
   return answer;
}

/*
 * Returns the instruction pointed to by a given label. The label is first looked in the
 * symbol table (used mainly by relocatable files but present also in other files),
 * then the dynamic table (used in shared or executable files)
 * \param efile The structure holding the parsed ELF file
 * \param label The label whose linked instruction we are looking for
 * \return The instruction at the address the label points to, or NULL if none was found or the
 * label does not exists
 * */
insn_t* elffile_get_insnatlbl(elffile_t* efile, char* label) {
   insn_t* out=NULL;
   tableentry_t* lblentry;

   lblentry = hashtable_lookup(efile->labels,label);
   if(lblentry)
   {
      if (lblentry->entry_64 != NULL)
      out = lblentry->entry_64->instr;
      else if (lblentry->entry_32 != NULL)
      out = lblentry->entry_32->instr;
   }
   return out;
}

/*
 * Retrieves the address in an executable ELf file to which a call to an external function
 * must be made (in the procedure label table).
 * \param efx The structure holding the parsed ELF file (no test is made to check whether it is
 * actually an executable)
 * \param label The name of the external function
 * \return The address to call to to invoke the function, or -1 if an error occurred (no dynamic
 * section found, no relocation table for external functions found, label not found, or architecture
 * not implemented)
 * \note This function finds the address by looking for the external function's name in the
 * appropriate relocation table, then looking into the procedure label table at the index at
 * which it was found in the relocation table, multiplied by an architecture specific step. This
 * seems to be how objdump actually proceeds.
 * */
int64_t elfexe_get_extlbl_addr(elffile_t* efx,char* label)
{
   /** \todo Find a better way to handle the multiple architectures
    ** \todo This function's behaviour may be partly x86 specific
    * */
   int idx=0,dynsize,jmpidx;
   int size = 0;
   int64_t addr=-1;
   Elf64_Dyn* dynentry = NULL;
   elfsection_t* dynscn,*jmprelscn;
   if(!efx)
   return -1;

   /*Retrieves dynamic section*/
   if (efx->indexes[DYN] < 0)
   {
      ERRMSG("No dynamic section found in file %s\n",efx->filename);
      return -1;
   }
   dynscn = efx->sections[efx->indexes[DYN]];
   dynsize = Elf_Shdr_get_sh_size (NULL, 0, dynscn->scnhdr) / Elf_Shdr_get_sh_entsize (NULL, 0, dynscn->scnhdr);

   /*Looks for JMPREL entry*/
   if (efx->indexes[JMPREL] == -1)
   {
      Elf64_Dyn** dyns = ELF_get_dyn (efx, efx->indexes[DYN], &size);
      while (idx<dynsize)
      {
         dynentry = dyns[idx];
         if (dynentry->d_tag == DT_JMPREL)
         break;
         idx++;
      }
      if (idx < dynsize)
      { /*JMPREL entry found*/
         /*Get the corresponding section*/
         jmpidx = elffile_get_scnataddr(efx,dynentry->d_un.d_ptr);
      } else {
         ERRMSG("No section of type JMPREL exists in file %s\n",efx->filename);
         return -1;
      }
   }
   else
   {
      jmpidx=efx->indexes[JMPREL];
   }
   if (jmpidx >= 0)
   {
      jmprelscn = efx->sections[jmpidx];
      idx=0;
      /*Looking for a table entry with the same name as the label*/
      while (idx < jmprelscn->nentries)
      {
         if ((jmprelscn->table_entries[idx]->entry_64 && str_equal (jmprelscn->table_entries[idx]->entry_64->name, label))
               || (jmprelscn->table_entries[idx]->entry_32 && str_equal (jmprelscn->table_entries[idx]->entry_32->name, label)))
         break;
         idx++;
      }
      if (idx < jmprelscn->nentries)
      { /*A relocation was found*/
         /*Retrieving the .plt section*/
         if (efx->indexes[PLT_IDX] == -1)
         efx->indexes[PLT_IDX] = elffile_get_scnbyname (efx, PLTNAME);
         /*Calculates the address to the call instruction*/
         if (efx->indexes[PLT_IDX] >= 0)
         {
            switch (elffile_getmachine(efx))
            {
               case EM_X86_64:
               //addr = ELF_get_scn_header(efx, efx->indexes[PLT_IDX])->sh_addr+(1+idx)*0x10;
               addr = Elf_Shdr_get_sh_addr (efx, efx->indexes[PLT_IDX], NULL) + (1 + idx) * 0x10;
               /**\warning The 0x10 value is specific to the x86_64 archi small model*/
               break;
               case EM_386:
               break;
               default:
               ERRMSG("Architecture with code %d not implemented\n", elffile_getmachine(efx));
               return -1;
            }
         }
      } //No relocation found: -1 (addr's default value) will be returned
   } else {
      ERRMSG("No JMPREL section found at address %#"PRIx64" in file %s\n",dynentry->d_un.d_ptr, efx->filename);
      {
         /*This error can happen if there are no sections header table*/
      }
      return addr;
   }

   /*
    * Adds a new address to the global offset table linked to the procedure linkage
    * table, and updates the target addresses hash table to include an entry for
    * the pointing object given in parameters to this new address, and an entry for this
    * new address to the given offset in the procedure linkage table section
    * \param efx The structure holding the executable ELF file
    * \param addr The address to add to the global offset table (it's an offset from the beginning of the new plt section)
    * \param pointerobj The pointer to an object pointing to this address
    * \param updater A pointer to the function to use to update \e pointerobj
    * \return The address of the new entry in the global offset table
    * */
   int64_t elfexe_add_pltgotaddr(elffile_t* efx,int64_t addr,void* pointerobj,addrupdfunc_t updater) {
      /** \todo Check if the return value is actually useful - could be changed to void*/
      Elf64_Addr *newgot,*oldgot;
      int gotsize,i;
      int64_t gotentaddr;

      if(!efx)
      return -1;

      //Retrieves the global offset table
      oldgot = (Elf64_Addr*)ELF_get_scn_bits(efx, efx->indexes[GOTPLT_IDX]);

      if (Elf_Ehdr_get_e_ident (efx, NULL)[EI_CLASS] == ELFCLASS64)
      gotsize = Elf_Shdr_get_sh_size (efx, efx->indexes[GOTPLT_IDX], NULL) / sizeof(Elf64_Addr);
      else if (Elf_Ehdr_get_e_ident (efx, NULL)[EI_CLASS] == ELFCLASS32)
      gotsize = Elf_Shdr_get_sh_size (efx, efx->indexes[GOTPLT_IDX], NULL) / sizeof(Elf32_Addr);

      //Creates the new table
      newgot = (Elf64_Addr*)lc_malloc(sizeof(Elf64_Addr)*(gotsize+1));

      //Copies the old table to the new one and replaces the old entries by the new ones in the target hashtable
      for(i=0;i<gotsize;i++)
      {
         newgot[i] = oldgot[i];

         //Updates the hashtable
         if(oldgot[i]!=0)
         {
            //Retrieves the destination of the old entry in the GOT_IDX (should be one occurrence)
            targetaddr_t* oldtarget = hashtable_lookup(efx->targets,&oldgot[i]);

            if (oldtarget) {
               //Removes the target linked to the old GOT entry from the hashtable
               hashtable_remove(efx->targets,&oldgot[i]);

               //Adds the target to the hashtable with the new GOT entry as key
               hashtable_insert(efx->targets,&newgot[i],oldtarget);
            }
         }
      }

      //Updates data in the global offset table section
      lc_free(ELF_get_scn_bits(efx, efx->indexes[GOTPLT_IDX]));
      efx->sections[efx->indexes[GOTPLT_IDX]]->bits = (unsigned char*)newgot;

      if (Elf_Ehdr_get_e_ident (efx, NULL)[EI_CLASS] == ELFCLASS64)
      Elf_Shdr_set_sh_size (efx, efx->indexes[GOTPLT_IDX], NULL, Elf_Shdr_get_sh_size (efx, efx->indexes[GOTPLT_IDX], NULL) + sizeof(Elf64_Addr));
      else if (Elf_Ehdr_get_e_ident (efx, NULL)[EI_CLASS] == ELFCLASS32)
      Elf_Shdr_set_sh_size (efx, efx->indexes[GOTPLT_IDX], NULL, Elf_Shdr_get_sh_size (efx, efx->indexes[GOTPLT_IDX], NULL) + sizeof(Elf32_Addr));

      //Sets the new entry to the required value
      newgot[gotsize] = addr;

      //This is an entry in the new plt section, which has not been created yet
      elffile_add_targetscnoffset(efx,&newgot[gotsize],NEWPLTSCNID,newgot[gotsize], *GElf_Addr_updaddr,efx->indexes[GOTPLT_IDX],gotsize);

      //Gets address of the new entry
      if (Elf_Ehdr_get_e_ident (efx, NULL)[EI_CLASS] == ELFCLASS64)
      gotentaddr = Elf_Shdr_get_sh_addr (efx, efx->indexes[GOTPLT_IDX], NULL) + gotsize * sizeof(Elf64_Addr);
      else if (Elf_Ehdr_get_e_ident (efx, NULL)[EI_CLASS] == ELFCLASS32)
      gotentaddr = Elf_Shdr_get_sh_addr (efx, efx->indexes[GOTPLT_IDX], NULL) + gotsize * sizeof(Elf32_Addr);

      //Creates a target to this new entry
      if(pointerobj!=NULL)
      {
         if (Elf_Ehdr_get_e_ident (efx, NULL)[EI_CLASS] == ELFCLASS64)
         elffile_add_targetscnoffset(efx,pointerobj,efx->indexes[GOTPLT_IDX],gotsize * sizeof(Elf64_Addr), updater,0,0);/*Should be no need to know the source section*/
         else if (Elf_Ehdr_get_e_ident (efx, NULL)[EI_CLASS] == ELFCLASS32)
         elffile_add_targetscnoffset(efx,pointerobj,efx->indexes[GOTPLT_IDX],gotsize * sizeof(Elf32_Addr), updater,0,0);/*Should be no need to know the source section*/
      }
      return gotentaddr;
   }

   /**
    * Adds an entry to the section containing version symbol informations. That function must be called before the update
    * of the structures from its linked section [or not]
    * \param efx The structure holding the parsed ELF file
    * \param value The value of the entry to add
    * \param linked_tbl The index of the section table linked to the version symbol. If this is not the index of the table
    * to which the section containing version symbol is actually linked, nothing will be done
    * \return The index of the newly added symbol entry in the dynamic table, 0 if nothing was done
    * \todo Check if this function could not make a better use of the gelf functions for updating versym entries
    * \todo This is another function that will prevent the free of the elffile structure to function properly
    * \todo Most of the code from this function was copied from elfexe_add_pltgotaddr, see if it could be factorised
    * */
   static int elfexe_add_versym (elffile_t* efx, int value, int linked_tbl)
   {
      int out = 0;
      int size_chunk = 0x50;

      if(!efx)
      return -1;

      if (efx->indexes[VERSYM_IDX] >= 0)
      {  //Check if a version symbol section actually exists
         elfsection_t* scn = efx->sections[efx->indexes[VERSYM_IDX]];
         if ((Elf_Ehdr_get_e_ident (efx, NULL)[EI_CLASS] == ELFCLASS64)
               && (int)Elf_Shdr_get_sh_link (NULL, 0, scn->scnhdr) == linked_tbl)
         {  //Check if it points to the right section
            //Retrieves the version symbol table
            int versize = Elf_Shdr_get_sh_size (NULL, 0, scn->scnhdr) / sizeof(Elf64_Versym);
            int size_realloc = ((int) Elf_Shdr_get_sh_size (NULL, 0, scn->scnhdr) / size_chunk) + 1;

            //Creates the new table
            //scn->bits = lc_realloc (scn->bits, scn->scnhdr->sh_size + sizeof(Elf64_Versym));

            // I don't know why, but with the correct formulation (above), the program
            // crashs some times. This system allocates bits by chunks of 0x50. This "solves"
            // the bug (or at least, hide it for the moment).
            /// \todo Find why and fix it
            scn->bits = lc_realloc (scn->bits, size_realloc * size_chunk);

            //Sets the new entry to the required value
            memcpy (&scn->bits[Elf_Shdr_get_sh_size (NULL, 0, scn->scnhdr)], &value, sizeof (value));

            //Updates data in the version symbol section
            Elf_Shdr_set_sh_size (NULL, 0, scn->scnhdr, Elf_Shdr_get_sh_size (NULL, 0, scn->scnhdr) + sizeof(Elf64_Versym));
            out=versize;
         }
         else if ((Elf_Ehdr_get_e_ident (efx, NULL)[EI_CLASS] == ELFCLASS64)
               && (int)Elf_Shdr_get_sh_link (NULL, 0, scn->scnhdr) == linked_tbl)
         {  //Check if it points to the right section
            //Retrieves the version symbol table
            int versize = Elf_Shdr_get_sh_size (NULL, 0, scn->scnhdr) / sizeof(Elf32_Versym);
            int size_realloc = ((int) Elf_Shdr_get_sh_size (NULL, 0, scn->scnhdr) / size_chunk) + 1;

            //Creates the new table
            //scn->bits = lc_realloc (scn->bits, scn->scnhdr->sh_size + sizeof(Elf64_Versym));

            // I don't know why, but with the correct formulation (above), the program
            // crashs some times. This system allocates bits by chunks of 0x50. This "solves"
            // the bug (or at least, hide it for the moment).
            /// \todo Find why and fix it
            scn->bits = lc_realloc (scn->bits, size_realloc * size_chunk);

            //Sets the new entry to the required value
            memcpy (&scn->bits[Elf_Shdr_get_sh_size (NULL, 0, scn->scnhdr)], &value, sizeof (value));

            //Updates data in the version symbol section
            Elf_Shdr_set_sh_size (NULL, 0, scn->scnhdr, Elf_Shdr_get_sh_size (NULL, 0, scn->scnhdr) + sizeof(Elf32_Versym));
            out=versize;
         }
      }
      return out;
   }

   /*
    * Adds a symbol to the symbol table in an ELF file (dynamic or symbol table)
    * \param ef The structure holding the parsed ELF file
    * \param lbl The symbol to add
    * \param addr The address linked to the symbol
    * \param size The size of the symbol entry
    * \param info The auxiliary information of the symbol entry
    * \param scnidx The index of the section the symbol entry points to
    * \param dynamic If set to TRUE, the symbol will be added to the dynamic table, otherwise it will be added to the symbol table
    * \return The index to the newly added symbol entry in the dynamic table, or -1 if an error occurred
    * In that case the last error code in \c ef will be updated
    */
   int elffile_add_symlbl(elffile_t* ef, char* lbl, int64_t addr, int size, int info, int scnidx, int dynamic) {
      int symidx;
      if(!ef)
      return -1;

      /**\todo Add a test to exit if the index does not exist*/
      if (dynamic == TRUE)
      {
         symidx = ef->indexes[DYNSYM_IDX];
         if (symidx == -1) {
            ef->last_error_code = ERR_BINARY_NO_SYMBOL_SECTION;
            return -1;
         }
      }
      else
      {
         symidx = ef->indexes[SYMTAB_IDX];
         if (symidx == -1) {
            ef->last_error_code = ERR_BINARY_NO_SYMBOL_SECTION;
            return -1;
         }
      }
      elfsection_t* symscn = ef->sections[symidx];

      //Increases the size of the symbol table in the existing elf file
      symscn->table_entries = (tableentry_t**)lc_realloc(symscn->table_entries, sizeof(tableentry_t*) * (symscn->nentries + 1));
      //Stores the new index of the symbol entry
      symscn->table_entries[symscn->nentries] = lc_malloc (sizeof(tableentry_t));

      if (Elf_Ehdr_get_e_ident (ef, NULL)[EI_CLASS] == ELFCLASS64)
      {
         symscn->table_entries[symscn->nentries]->entry_64 = lc_malloc (sizeof (tableentry_64_t));
         symscn->table_entries[symscn->nentries]->entry_32 = NULL;
         symscn->table_entries[symscn->nentries]->entry_64->name = lc_strdup (lbl);
         symscn->table_entries[symscn->nentries]->entry_64->type = SYMTABLE;
         symscn->table_entries[symscn->nentries]->entry_64->updated = FALSE; //Will be set to TRUE in elffile_upd_tbldata
         symscn->table_entries[symscn->nentries]->entry_64->elfentry.sym = (Elf64_Sym*)lc_malloc(sizeof(Elf64_Sym));
         symscn->table_entries[symscn->nentries]->entry_64->elfentry.sym->st_name = elffile_upd_strtab(ef, Elf_Shdr_get_sh_link (NULL, 0, symscn->scnhdr), lbl);
         symscn->table_entries[symscn->nentries]->entry_64->elfentry.sym->st_info = info;
         symscn->table_entries[symscn->nentries]->entry_64->elfentry.sym->st_other = 0;
         symscn->table_entries[symscn->nentries]->entry_64->elfentry.sym->st_shndx = scnidx;
         symscn->table_entries[symscn->nentries]->entry_64->elfentry.sym->st_size = size;
         symscn->table_entries[symscn->nentries]->entry_64->elfentry.sym->st_value = addr;
      }
      else if (Elf_Ehdr_get_e_ident (ef, NULL)[EI_CLASS] == ELFCLASS32)
      {
         symscn->table_entries[symscn->nentries]->entry_64 = NULL;
         symscn->table_entries[symscn->nentries]->entry_32 = lc_malloc (sizeof (tableentry_32_t));
         symscn->table_entries[symscn->nentries]->entry_32->name = lc_strdup (lbl);
         symscn->table_entries[symscn->nentries]->entry_32->type = SYMTABLE;
         symscn->table_entries[symscn->nentries]->entry_32->updated = FALSE; //Will be set to TRUE in elffile_upd_tbldata
         symscn->table_entries[symscn->nentries]->entry_32->elfentry.sym = (Elf32_Sym*)lc_malloc(sizeof(Elf32_Sym));
         symscn->table_entries[symscn->nentries]->entry_32->elfentry.sym->st_name = elffile_upd_strtab(ef,Elf_Shdr_get_sh_link (NULL, 0, symscn->scnhdr), lbl);
         symscn->table_entries[symscn->nentries]->entry_32->elfentry.sym->st_info = info;
         symscn->table_entries[symscn->nentries]->entry_32->elfentry.sym->st_other = 0;
         symscn->table_entries[symscn->nentries]->entry_32->elfentry.sym->st_shndx = scnidx;
         symscn->table_entries[symscn->nentries]->entry_32->elfentry.sym->st_size = size;
         symscn->table_entries[symscn->nentries]->entry_32->elfentry.sym->st_value = addr;
      }

      //Adds the new symbol entry to the hashtable containing the symbol table
      hashtable_insert(symscn->hentries,(void *)addr,&symscn->table_entries[symscn->nentries]);
      //Adds the new symbol entry to the labels hashtable
      hashtable_insert(ef->labels,lbl,&symscn->table_entries[symscn->nentries]);

      DBGMSG("Adding %s label %s at address %#"PRIx64" with size %d and info %#x in section %d at index %d of the symbol table\n",
            (dynamic==TRUE)?"dynamic":"static", lbl, addr, size, info, scnidx, symscn->nentries);
      symscn->nentries++;

      if (dynamic == TRUE)
      {
         //Updates the version informations table
         int versupd = elfexe_add_versym (ef,VER_NDX_GLOBAL,symidx);//Adding a global entry
         if(versupd!=(symscn->nentries-1))
         fprintf(stderr,"Warning: Version symbol table size mismatch (%d for %d entries)\n",versupd,symscn->nentries);
      }

      elffile_upd_tbldata(ef,symidx,SYMTABLE);
      return symscn->nentries-1;
   }

   /*
    * Modify a label in the dynamic symbol table.
    * It will modifies the external function called.
    * \param ef The structure holding the parsed ELF file
    * \param new_lbl The new name of the label
    * \param old_lbl The old name of the label
    * \return EXIT_SUCCESS if the update is successful, error code otherwise
    */
   int elffile_modify_dinsymlbl(elffile_t* ef, char* new_lbl, char* old_lbl) {
      int dynsymidx;
      if(!ef)
      return ERR_BINARY_MISSING_BINFILE;

      dynsymidx = ef->indexes[DYNSYM];
      if (dynsymidx == -1)
      return ERR_BINARY_NO_SYMBOL_SECTION;
      elfsection_t* dynsymscn = ef->sections[dynsymidx];

      //First looking up in the file we are inserting into
      tableentry_t* extsym = NULL;
      queue_t* entries = hashtable_lookup_all(ef->labels, old_lbl);

      FOREACH_INQUEUE(entries,iter) {
         if (GET_DATA_T(tableentry_t*, iter)->scnidx == dynsymidx) {
            extsym = GET_DATA_T(tableentry_t*, iter);
            break;
         }
      }

      queue_free(entries, NULL);

      if (!extsym)
      return ERR_BINARY_SYMBOL_NOT_FOUND;

      int idx = elffile_upd_strtab(ef,dynsymscn->scnhdr->sh_link,new_lbl);

      extsym->elfentry.sym->st_name = idx;
      extsym->name = lc_strdup(new_lbl);

      elffile_upd_tbldata(ef,dynsymidx,SYMTABLE);

      return EXIT_SUCCESS;
   }
   int elffile_allocate_symlbl(elffile_t* ef, int nb_symbols)
   {
      if (nb_symbols <= 0)
      return (0);
      elfsection_t* scn = ef->sections[ef->indexes[SYM]];

      if (Elf_Ehdr_get_e_ident (ef, NULL)[EI_CLASS] == ELFCLASS64)
      {
         scn->scnhdr->scnhdr64->sh_size += (sizeof(Elf64_Sym) * nb_symbols);
         Elf64_Sym* newsym = lc_malloc (scn->scnhdr->scnhdr64->sh_size);
         lc_free(scn->bits);
         scn->bits = (void*)newsym;
      }
      else if (Elf_Ehdr_get_e_ident (ef, NULL)[EI_CLASS] == ELFCLASS32)
      {
         scn->scnhdr->scnhdr32->sh_size += (sizeof(Elf32_Sym) * nb_symbols);
         Elf32_Sym* newsym = lc_malloc (scn->scnhdr->scnhdr32->sh_size);
         lc_free(scn->bits);
         scn->bits = (void*)newsym;
      }
      return (0);
   }

   int elffile_upd_symlbl(elffile_t* ef, char* lbl, int64_t addr, int size, int info, int scnidx, int dynamic) {
      int symidx;
      if(!ef)
      return -1;

      /**\todo Add a test to exit if the index does not exist*/
      if (dynamic == TRUE)
      {
         symidx = ef->indexes[DYNSYM];
         if (symidx == -1)
         return -1;
      }
      else
      {
         symidx = ef->indexes[SYM];
         if (symidx == -1)
         return -1;
      }
      elfsection_t* symscn = ef->sections[symidx];

      //Increases the size of the symbol table in the existing elf file
      symscn->table_entries = (tableentry_t**)lc_realloc(symscn->table_entries, sizeof(tableentry_t*) * (symscn->nentries + 1));
      //Stores the new index of the symbol entry
      symscn->table_entries[symscn->nentries] = lc_malloc (sizeof(tableentry_t));

      if (Elf_Ehdr_get_e_ident (ef, NULL)[EI_CLASS] == ELFCLASS64)
      {
         symscn->table_entries[symscn->nentries]->entry_64 = lc_malloc (sizeof (tableentry_64_t));
         symscn->table_entries[symscn->nentries]->entry_32 = NULL;
         symscn->table_entries[symscn->nentries]->entry_64->name = lc_strdup (lbl);
         symscn->table_entries[symscn->nentries]->entry_64->type = SYMTABLE;
         symscn->table_entries[symscn->nentries]->entry_64->updated = FALSE; //Will be set to TRUE in elffile_upd_tbldata
         symscn->table_entries[symscn->nentries]->entry_64->elfentry.sym = (Elf64_Sym*)lc_malloc(sizeof(Elf64_Sym)); /**\bug Memory leak here*/
         symscn->table_entries[symscn->nentries]->entry_64->elfentry.sym->st_name = elffile_upd_strtab(ef, Elf_Shdr_get_sh_link (NULL, 0, symscn->scnhdr), lbl);
         symscn->table_entries[symscn->nentries]->entry_64->elfentry.sym->st_info = info;
         symscn->table_entries[symscn->nentries]->entry_64->elfentry.sym->st_other = 0;
         symscn->table_entries[symscn->nentries]->entry_64->elfentry.sym->st_shndx = scnidx;
         symscn->table_entries[symscn->nentries]->entry_64->elfentry.sym->st_size = size;
         symscn->table_entries[symscn->nentries]->entry_64->elfentry.sym->st_value = addr;
         //Adds the new symbol entry to the hashtable containing the symbol table
         hashtable_insert(symscn->hentries,(void *)addr,&symscn->table_entries[symscn->nentries]);
         //Adds the new symbol entry to the labels hashtable
         hashtable_insert(ef->labels,lbl,&symscn->table_entries[symscn->nentries]);

         Elf64_Sym *newsym;
         int i;
         elfsection_t* scn = ef->sections[symidx];
         targetaddr_t* lasttarget = NULL, *currenttarget = NULL;
         newsym = (Elf64_Sym*)scn->bits;
         for (i = 0; i < scn->nentries; i++)
         {
            newsym[i] = *(scn->table_entries[i]->entry_64->elfentry.sym);
            //Updates hashtable with new elf structure
            currenttarget = elffile_upd_targetaddr(ef, scn->table_entries[i]->entry_64->elfentry.sym, &newsym[i], newsym[i].st_value);
            if (currenttarget)
            {
               if (lasttarget)
               lc_free(lasttarget);
               lasttarget = currenttarget;
            }
            else if (lasttarget)
            elffile_add_targetscnoffset(ef, &newsym[i],
                  lasttarget->scnindx, newsym[i].st_value - Elf_Shdr_get_sh_addr (ef, lasttarget->scnindx, NULL),
                  GElf_Sym_updaddr, symidx, i);

            //Updates the address of the elf structure
            scn->table_entries[i]->entry_64->elfentry.sym = &newsym[i];
            scn->table_entries[i]->entry_64->updated = TRUE;
         }
         if (lasttarget)
         lc_free(lasttarget);
         scn->bits = (void*)newsym;
      }
      else if (Elf_Ehdr_get_e_ident (ef, NULL)[EI_CLASS] == ELFCLASS32)
      {
         symscn->table_entries[symscn->nentries]->entry_32 = lc_malloc (sizeof (tableentry_32_t));
         symscn->table_entries[symscn->nentries]->entry_64 = NULL;
         symscn->table_entries[symscn->nentries]->entry_32->name = lc_strdup (lbl);
         symscn->table_entries[symscn->nentries]->entry_32->type = SYMTABLE;
         symscn->table_entries[symscn->nentries]->entry_32->updated = FALSE; //Will be set to TRUE in elffile_upd_tbldata
         symscn->table_entries[symscn->nentries]->entry_32->elfentry.sym = (Elf32_Sym*)lc_malloc(sizeof(Elf32_Sym)); /**\bug Memory leak here*/
         symscn->table_entries[symscn->nentries]->entry_32->elfentry.sym->st_name = elffile_upd_strtab(ef, Elf_Shdr_get_sh_link (NULL, 0, symscn->scnhdr), lbl);
         symscn->table_entries[symscn->nentries]->entry_32->elfentry.sym->st_info = info;
         symscn->table_entries[symscn->nentries]->entry_32->elfentry.sym->st_other = 0;
         symscn->table_entries[symscn->nentries]->entry_32->elfentry.sym->st_shndx = scnidx;
         symscn->table_entries[symscn->nentries]->entry_32->elfentry.sym->st_size = size;
         symscn->table_entries[symscn->nentries]->entry_32->elfentry.sym->st_value = addr;
         //Adds the new symbol entry to the hashtable containing the symbol table
         hashtable_insert(symscn->hentries,(void *)addr,&symscn->table_entries[symscn->nentries]);
         //Adds the new symbol entry to the labels hashtable
         hashtable_insert(ef->labels,lbl,&symscn->table_entries[symscn->nentries]);

         Elf32_Sym *newsym;
         int i;
         elfsection_t* scn = ef->sections[symidx];
         targetaddr_t* lasttarget = NULL, *currenttarget = NULL;
         newsym = (Elf32_Sym*)scn->bits;
         for (i = 0; i < scn->nentries; i++)
         {
            newsym[i] = *(scn->table_entries[i]->entry_32->elfentry.sym);
            //Updates hashtable with new elf structure
            currenttarget = elffile_upd_targetaddr(ef, scn->table_entries[i]->entry_32->elfentry.sym, &newsym[i], newsym[i].st_value);
            if (currenttarget)
            {
               if (lasttarget)
               lc_free(lasttarget);
               lasttarget = currenttarget;
            }
            else if (lasttarget)
            elffile_add_targetscnoffset(ef, &newsym[i],
                  lasttarget->scnindx, newsym[i].st_value - Elf_Shdr_get_sh_addr (ef, lasttarget->scnindx, NULL),
                  GElf_Sym_updaddr, symidx, i);

            //Updates the address of the elf structure
            scn->table_entries[i]->entry_32->elfentry.sym = &newsym[i];
            scn->table_entries[i]->entry_32->updated = TRUE;
         }
         if (lasttarget)
         lc_free(lasttarget);
         scn->bits = (void*)newsym;
      }

      DBGMSG("Adding %s label %s at address %#"PRIx64" with size %d and info %#x in section %d at index %d of the symbol table\n",
            (dynamic==TRUE)?"dynamic":"static", lbl, addr, size, info, scnidx, symscn->nentries);
      symscn->nentries++;

      return symscn->nentries-1;
   }

   /*
    * Adds a "dummy" symbol (type not recognised as function) to a file at a given address
    * \param efile The structure holding the parsed ELF file
    * \param lbl The symbol to add
    * \param addr The address linked to the symbol
    * \param scnidx The index of the section the symbol entry points to
    * \param dynamic If set to TRUE, the symbol will be added to the dynamic table, otherwise it will be added to the symbol table
    * \return The index to the newly added symbol entry in the dynamic table, or -1 if an error occurred
    * In that case the last error code in \c ef will be updated
    * */
   int elffile_add_dummysymbol(elffile_t* efile, char* lbl, int64_t addr, int scnidx, int dynamic) {
      return elffile_add_symlbl(efile,lbl,addr,0,ELF64_ST_INFO(STB_GLOBAL,STT_NUM),scnidx,dynamic);
   }

   /*
    * Adds a function symbol to a file at a given address
    * \param ef The structure holding the parsed ELF file
    * \param lbl The symbol to add
    * \param addr The address linked to the symbol
    * \param scnidx The index of the section the symbol entry points to
    * \param dynamic If set to TRUE, the symbol will be added to the dynamic table, otherwise it will be added to the symbol table
    * \return The index to the newly added symbol entry in the dynamic table, or -1 if an error occurred
    * In that case the last error code in \c ef will be updated
    * */
   int elffile_add_funcsymbol(elffile_t* ef, char* lbl, int64_t addr, int scnidx, int dynamic) {
      return elffile_add_symlbl(ef,lbl,addr,0,ELF64_ST_INFO(STB_GLOBAL,STT_FUNC),scnidx,dynamic);
   }

   /*
    * Adds a symbol with undefined type to a file at a given address
    * \param efile The structure holding the parsed ELF file
    * \param lbl The symbol to add
    * \param addr The address linked to the symbol
    * \param scnidx The index of the section the symbol entry points to
    * \param dynamic If set to TRUE, the symbol will be added to the dynamic table, otherwise it will be added to the symbol table
    * \return The index to the newly added symbol entry in the dynamic table, or -1 if an error occurred
    * In that case the last error code in \c ef will be updated
    * */
   int elffile_add_undefsymbol(elffile_t* efile, char* lbl, int64_t addr, int scnidx, int dynamic) {
      return elffile_add_symlbl(efile,lbl,addr,0,ELF64_ST_INFO(STB_GLOBAL,STT_NOTYPE),scnidx,dynamic);
   }

   /*
    * Updates entries in the dynamic table relative to sections sizes to reflect the section's sizes'
    * changes
    * \param efx The structure holding the parsed ELF file
    */
   void elfexe_dyntbl_updsz(elffile_t* efx)
   {
      int i;
      elfsection_t *dynscn;

      if(!efx)
      return;

      if(efx->indexes[DYNAMIC_IDX]>=0)
      {
         dynscn = efx->sections[efx->indexes[DYNAMIC_IDX]];
         for (i = 0; i < dynscn->nentries; i++)
         {
            int d_tag = 0;
            if (dynscn->table_entries[i]->entry_64 != NULL)
            d_tag = dynscn->table_entries[i]->entry_64->elfentry.dyn->d_tag;
            else if (dynscn->table_entries[i]->entry_64 != NULL)
            d_tag = dynscn->table_entries[i]->entry_32->elfentry.dyn->d_tag;

            switch(d_tag)
            {
               case DT_PLTRELSZ:
               {
                  //Updates dynamic entry storing the size of the RELAPLT section
                  Elf_Shdr* scnhdr = efx->sections[efx->indexes[RELAPLT_IDX]]->scnhdr;
                  if (Elf_Ehdr_get_e_ident (efx, NULL)[EI_CLASS] == ELFCLASS64)
                  {
                     dynscn->table_entries[i]->entry_64->elfentry.dyn->d_un.d_val = Elf_Shdr_get_sh_size (NULL, 0, scnhdr);
                     ELF_update_dyn64 (efx, efx->indexes[DYNAMIC_IDX], i, dynscn->table_entries[i]->entry_64->elfentry.dyn);
                  }
                  else if (Elf_Ehdr_get_e_ident (efx, NULL)[EI_CLASS] == ELFCLASS32)
                  {
                     dynscn->table_entries[i]->entry_32->elfentry.dyn->d_un.d_val = Elf_Shdr_get_sh_size (NULL, 0, scnhdr);
                     ELF_update_dyn32 (efx, efx->indexes[DYNAMIC_IDX], i, dynscn->table_entries[i]->entry_32->elfentry.dyn);
                  }
                  break;
               }
               case DT_STRSZ:
               {
                  Elf_Shdr* scnhdr = efx->sections[Elf_Shdr_get_sh_link (NULL, 0, dynscn->scnhdr)]->scnhdr;
                  //Updates dynamic entry storing the size of the STRTAB section
                  if (Elf_Ehdr_get_e_ident (efx, NULL)[EI_CLASS] == ELFCLASS64)
                  {
                     dynscn->table_entries[i]->entry_64->elfentry.dyn->d_un.d_val = Elf_Shdr_get_sh_size (NULL, 0, scnhdr);
                     ELF_update_dyn64 (efx, efx->indexes[DYNAMIC_IDX], i, dynscn->table_entries[i]->entry_64->elfentry.dyn);
                  }
                  else if (Elf_Ehdr_get_e_ident (efx, NULL)[EI_CLASS] == ELFCLASS32)
                  {
                     dynscn->table_entries[i]->entry_32->elfentry.dyn->d_un.d_val = Elf_Shdr_get_sh_size (NULL, 0, scnhdr);
                     ELF_update_dyn32 (efx, efx->indexes[DYNAMIC_IDX], i, dynscn->table_entries[i]->entry_32->elfentry.dyn);}
                  break;
               }
               case DT_REL:
               /*TODO (not used so far for adding external function calls)*/
               break;
               case DT_RELA:
               /*TODO (not used so far for adding external function calls)*/
               break;
            }
         }
      }
   }

   /*
    * Comparaison function for qsort
    * \param s1 an ELF section header
    * \param s2 an ELF section header
    * \return 1 if s2 > s1, 0 if s2 == s1, else -1
    */
   /*static*/int compare_scn (const void* s1, const void* s2)
   {
      Elf_Shdr* scn1 = (Elf_Shdr*) s1;
      Elf_Shdr* scn2 = (Elf_Shdr*) s2;

      if ( (!scn1) || (!scn2) )
      return -1;

      if (Elf_Shdr_get_sh_offset(NULL, 0, scn1) < Elf_Shdr_get_sh_offset(NULL, 0, scn2))
      return (-1);
      else if (Elf_Shdr_get_sh_offset(NULL, 0, scn1) == Elf_Shdr_get_sh_offset(NULL, 0, scn2))
      return (0);
      else
      return (1);

   }

   static void elffile_add_gotentries (elffile_t* efile,unsigned *entriesNumberToAdd)
   {
      Elf64_Addr *newgot,*oldgot;
      int gotsize,i;
      /*Retrieves the global offset table*/
      oldgot = (Elf64_Addr*)efile->sections[efile->indexes[GOT_IDX]]->bits;
      if (Elf_Ehdr_get_e_ident (efile, NULL)[EI_CLASS] == ELFCLASS64)
      gotsize = Elf_Shdr_get_sh_size (efile, efile->indexes[GOT_IDX], NULL) / sizeof(Elf64_Addr);
      else if (Elf_Ehdr_get_e_ident (efile, NULL)[EI_CLASS] == ELFCLASS32)
      gotsize = Elf_Shdr_get_sh_size (efile, efile->indexes[GOT_IDX], NULL) / sizeof(Elf32_Addr);
      /*Creates the new table*/
      /**\todo We have to use malloc instead of lc_malloc since this structure comes from the ELF and was
       * not initialised by lc_realloc. All the more reasons to avoid modifying those structures*/
      newgot = (Elf64_Addr*)malloc(sizeof(Elf64_Addr)*(gotsize+ (*entriesNumberToAdd)));
      //newgot = (GElf_Addr*)malloc(sizeof(GElf_Addr)*(5000));
      DBGMSG ("Total elements in .GOT: %d\n",gotsize+ (*entriesNumberToAdd));
      /*Copies the old table to the new one and replaces the old entries by the new ones in the target hashtable*/
      for(i=0;i<gotsize;i++)
      {
         newgot[i] = oldgot[i];
         /*Updates the hashtable*/
         if(oldgot[i]!=0)
         {
            /*Retrieves the destination of the old entry in the GOT (should be one occurrence)*/
            targetaddr_t* oldtarget = hashtable_lookup(efile->targets,&oldgot[i]);

            if (oldtarget) {
               /*Removes the target linked to the old GOT entry from the hashtable*/
               hashtable_remove(efile->targets,&oldgot[i]);
               /*Adds the target to the hashtable with the new GOT entry as key*/
               hashtable_insert(efile->targets,&newgot[i],oldtarget);
            }
         }
      }
      /*Updates data in the global offset table section*/
      /**\todo We have to use free instead of lc_free since this structure comes from the ELF and was
       * not initialised by lc_realloc. All the more reasons to avoid modifying those structures*/
      free(efile->sections[efile->indexes[GOT_IDX]]->bits);
      efile->sections[efile->indexes[GOT_IDX]]->bits =(unsigned char*)newgot;
      if (Elf_Ehdr_get_e_ident (efile, NULL)[EI_CLASS] == ELFCLASS64)
      Elf_Shdr_set_sh_size (efile, efile->indexes[GOT_IDX], NULL,
            Elf_Shdr_get_sh_size (efile, efile->indexes[GOT_IDX], NULL) + ((*entriesNumberToAdd) * sizeof(Elf64_Addr)));
      else if (Elf_Ehdr_get_e_ident (efile, NULL)[EI_CLASS] == ELFCLASS32)
      Elf_Shdr_set_sh_size (efile, efile->indexes[GOT_IDX], NULL,
            Elf_Shdr_get_sh_size (efile, efile->indexes[GOT_IDX], NULL) + ((*entriesNumberToAdd) * sizeof(Elf32_Addr)));
   }

   static void elffile_addGotPltEntries (elffile_t* efile,int *entriesNumberToAdd)
   {
      Elf64_Addr *newGotPlt,*oldGotPlt = NULL;
      int gotPltIdx,gotPltSize = 0,i;
      /*Retrieves the global offset table PLT*/
      if ((gotPltIdx = efile->indexes[GOTPLT_IDX]) != -1)
      {
         oldGotPlt = (Elf64_Addr*)efile->sections[gotPltIdx]->bits;

         if (Elf_Ehdr_get_e_ident (efile, NULL)[EI_CLASS] == ELFCLASS64)
         gotPltSize = Elf_Shdr_get_sh_size (efile, gotPltIdx, NULL) / sizeof(Elf64_Addr);
         else if (Elf_Ehdr_get_e_ident (efile, NULL)[EI_CLASS] == ELFCLASS32)
         gotPltSize = Elf_Shdr_get_sh_size (efile, gotPltIdx, NULL) / sizeof(Elf32_Addr);
      }
      else
      {
         //CREATE GOT PLT!!
         assert(0);
      }

      /*Creates the new table*/
      /**\todo We have to use malloc instead of lc_malloc since this structure comes from the ELF and was
       * not initialised by lc_realloc. All the more reasons to avoid modifying those structures*/
      newGotPlt = (Elf64_Addr*)malloc(sizeof(Elf64_Addr)*(gotPltSize+ (*entriesNumberToAdd)));
      DBGMSG ("Total elements in .GOT: %d\n",gotPltSize+ (*entriesNumberToAdd));
      /*Copies the old table to the new one and replaces the old entries by the new ones in the target hashtable*/
      for(i=0;i<gotPltSize;i++)
      {
         newGotPlt[i] = oldGotPlt[i];
         /*Updates the hashtable*/
         if(oldGotPlt[i]!=0)
         {
            /*Retrieves the destination of the old entry in the GOT (should be one occurrence)*/
            targetaddr_t* oldtarget = hashtable_lookup(efile->targets,&oldGotPlt[i]);

            if (oldtarget) {
               /*Removes the target linked to the old GOT entry from the hashtable*/
               hashtable_remove(efile->targets,&oldGotPlt[i]);
               /*Adds the target to the hashtable with the new GOT entry as key*/
               hashtable_insert(efile->targets,&newGotPlt[i],oldtarget);
            }
         }
      }
      /*Updates data in the global offset table section*/
      /**\todo We have to use free instead of lc_free since this structure comes from the ELF and was
       * not initialised by lc_realloc. All the more reasons to avoid modifying those structures*/
      free(efile->sections[efile->indexes[GOTPLT_IDX]]->bits);
      efile->sections[efile->indexes[GOTPLT_IDX]]->bits =(unsigned char*)newGotPlt;

      if (Elf_Ehdr_get_e_ident (efile, NULL)[EI_CLASS] == ELFCLASS64)
      Elf_Shdr_set_sh_size(efile, efile->indexes[GOTPLT_IDX], NULL,
            Elf_Shdr_get_sh_size (efile, efile->indexes[GOTPLT_IDX], NULL) + ((*entriesNumberToAdd) * sizeof(Elf64_Addr)));
      else if (Elf_Ehdr_get_e_ident (efile, NULL)[EI_CLASS] == ELFCLASS32)
      Elf_Shdr_set_sh_size(efile, efile->indexes[GOTPLT_IDX], NULL,
            Elf_Shdr_get_sh_size (efile, efile->indexes[GOTPLT_IDX], NULL) + ((*entriesNumberToAdd) * sizeof(Elf32_Addr)));
   }

///**
// * Checks if a section will be loaded by a program. The test is performed on the section's flags (as opposed to, for instance,
// * comparing with the program segments) to support object files.
// * \param scn The section to test (must not be NULL)
// * \return TRUE if the section has flags Allocate, Execute or Write sets, and is of type PROGBITS, FALSE otherwise
// * */
//static int elfsection_isloaded(elfsection_t* scn)
//{
//   int sh_flags = Elf_Shdr_get_sh_flags (NULL, 0, scn->scnhdr);
//   int sh_type = Elf_Shdr_get_sh_type (NULL, 0, scn->scnhdr);
//
//   if (((sh_flags & SHF_ALLOC) != 0
//     || (sh_flags & SHF_EXECINSTR) != 0
//     || (sh_flags & SHF_WRITE) != 0)
//   &&    sh_type == SHT_PROGBITS
//   /*|| (scn->scnhdr->sh_type == SHT_NOBITS)*/ )
//      return TRUE;
//   else
//      return FALSE;
//
//}

   /**
    * Adds a new section to a file that is a copy of another section
    * \param ef The file to add the section to
    * \param scn The section to copy
    * \param cpyname The name of the copied section
    * \param cpyaddr The address of the copied section
    * */
   static void elffile_add_scncopy(elffile_t* efile, elfsection_t* scn, char* cpyname, int64_t cpyaddr,int64_t cpyoffset)
   {
      DBGMSG("Adding copied section %s at address %#"PRIx64"\n",cpyname,cpyaddr);
      elffile_add_scn (efile, cpyname,
            Elf_Shdr_get_sh_type (NULL, 0, scn->scnhdr),
            Elf_Shdr_get_sh_flags (NULL, 0, scn->scnhdr),
            cpyaddr, cpyoffset,
            Elf_Shdr_get_sh_link (NULL, 0, scn->scnhdr),
            Elf_Shdr_get_sh_info (NULL, 0, scn->scnhdr),
            Elf_Shdr_get_sh_addralign (NULL, 0, scn->scnhdr),
            Elf_Shdr_get_sh_size (NULL, 0, scn->scnhdr),
            scn->bits);
   }

   /**
    * Inserts additional file into a file being patched
    * \param efile Updated file
    * \param addedfiles Array of files to add
    * \param n_addedfiles Size of \c addedfiles
    * \param permtb Permutation table of sections
    * \param addedfiles_newaddrs Array of virtual addresses at which files are inserted
    * \param permtbl_idx Number of elements in the permutation table (return parameter)
    * \param newsz Size added to the file (return parameter)
    * \param virtaddr Virtual address where to insert the first file (return parameter)
    * \return Updated permutation table
    * \todo Work in progress, may be tweaked
    * */
   static int* insert_addedfiles(elffile_t* efile, elffile_t** addedfiles, int n_addedfiles, int* permtb, int64_t* addedfiles_newaddrs,
         int* permtbl_idx, int64_t* newsz, uint64_t* virtaddr,uint64_t* virtoffset, int* oldgotsize,
         int *oldGotPltSize,uint64_t* oldGotAddr,uint64_t* sizeToAddToBss,int tdataIdx,int tdataVarsSize,int tbssVarsSize,int tlsAlign,
         int segTLSneeded,int madrasPltScnIdx,queue_t* stubPltList,tableentry_t*** madrasRelaPltEntries,
         int* nbNewGnuIfunc)
   {
      (void) tlsAlign;
      (void) segTLSneeded;
      int* permtbl = permtb;
      // Adds the sections from inserted files ------------------------------------
      if ( ( addedfiles != NULL ) && ( n_addedfiles > 0 ) )
      {
         int i,idxSection;
         elfreloc_t* relocations = NULL; //Array of all relocations
         int n_relocations = 0;//Size of the relocations array
         unsigned int max_comalign = 0;//Maximum alignment for uninitialised global variables
         int64_t comscnsz = 0;//Minimal size of the section holding unallocated symbols (uninitialised global variables)
         int64_t topcom = 0;//Offset in the section of unallocated symbols of the latest encountered symbol
         queue_t* comsyms = queue_new();//Queue of tableentry_t structures for all unallocated symbols
         int alignmod;
         uint64_t* curraddr = virtaddr;
         uint64_t* curroffset = virtoffset;
         int idx = *permtbl_idx;
         int64_t altsz = *newsz;
         int bssIdx = -1;//The index of the .GOT section in the permtbl_idx table.
         uint64_t idxInGot = (oldgotsize)?(*oldgotsize):0;//The index for the next insertion in the GOT.
         uint64_t idxInGotPlt = *oldGotPltSize;//The index for the next insertion in the GOT.
         int idxSymbolAddrFound = 0;//Store the idx in the GOT if the GLOBAL VARIABLE is already in the GOT.
         Elf64_Addr* gotAddr = (Elf64_Addr*)efile->sections[efile->indexes[GOT_IDX]]->bits;//Address of .GOT's section data.
         Elf64_Addr* gotPltAddr = NULL;//Address of .GOT's section data.
         if (efile->indexes[GOTPLT_IDX] != -1)
         gotPltAddr = (Elf64_Addr*)efile->sections[efile->indexes[GOTPLT_IDX]]->bits;
         //Hashtable containing all copied sections, indexed on the original section
         hashtable_t* copiedscns = hashtable_new(direct_hash,direct_equal);
         // Hashtable to not duplicate variables in .GOT section (to get only one reference for one variable)
         hashtable_t* variablesInGot = hashtable_new(str_hash,str_equal);;
         // Hashtable to not duplicate variables in .GOT section (to get only one reference for one variable)
         hashtable_t* varTLSinGot = hashtable_new(int_hash,int_equal);;
         //Hashtable which permits to follow the TLS variables insertion coming from the object files in .tdata_madras section.
         hashtable_t* offsetInTLSsection = hashtable_new(int_hash,int_equal);
         // Hashtable which permits to follow the insertion of symbols in the .BSS section
         hashtable_t* symbolAdded = hashtable_new(str_hash,str_equal);
         // Hashtable to not duplicate variables in .GOT.PLT section (to get only one reference for one variable)
         hashtable_t* variablesInPltGot = hashtable_new(str_hash,str_equal);;
         elfsection_t* copiedscn;
         int nbVarInBss = 0;
         uint64_t addrBss = Elf_Shdr_get_sh_addr (efile, efile->indexes[BSS_IDX], NULL);
         int lastSectionIsTdata = 0;

         hashtable_t* bssToAdd = hashtable_new(int_hash,int_equal);
         void* tdataDest = NULL;
         uint64_t tdataOffset;
         void* tbssDest = NULL;
         uint64_t tbssOffset = 0;
         uint64_t tdataOriginalSize = 0;
         uint64_t tbssOriginalSize = 0;
         uint64_t originalSegSize = 0;
         uint64_t newSegSize = 0;
         Elf_Shdr* bssHdr = NULL;
         Elf_Shdr* tbssHdr = NULL;
         Elf_Shdr* tdataHdr = NULL;

         //Get the .bss section header (if it exists)
         if (efile->indexes[BSS_IDX] != -1)
         bssHdr = efile->sections[efile->indexes[BSS_IDX]]->scnhdr;

         //TODO: bssHdr set but not used
         (void)bssHdr;

         //Get the .tdata section header and his size(if it exists)
         if (efile->indexes[TDATA_IDX] != -1)
         {
            tdataHdr = efile->sections[efile->indexes[TDATA_IDX]]->scnhdr;
            tdataOriginalSize = Elf_Shdr_get_sh_size (NULL, 0, tdataHdr);
         }

         //Get the .tbss section header and his size(if it exists)
         if (efile->indexes[TBSS_IDX] != -1)
         {
            tbssHdr =efile->sections[efile->indexes[TBSS_IDX]]->scnhdr;
            tbssOriginalSize = Elf_Shdr_get_sh_size (NULL, 0, tbssHdr);
         }

         //The patch need to create a madras TLS section -> imported symbols deals with TLS
         if (tdataIdx != -1)
         {
            tdataDest = efile->sections[tdataIdx]->bits + tdataVarsSize;
            tdataOffset = 0; //tlsAlign; //Space reserved for TLS segment alignment (if necessary)
            tbssDest = efile->sections[tdataIdx+1]->bits + tbssVarsSize;//+1: fill in .tbbs_bss section.
            tbssOffset = Elf_Shdr_get_sh_size (efile, tdataIdx, NULL);//The .tbss is adjacent to the .tdata: we need to shift the offset accordingly
            originalSegSize = tdataOriginalSize+tbssOriginalSize;
            newSegSize = Elf_Shdr_get_sh_size (efile, tdataIdx, NULL) + Elf_Shdr_get_sh_size (efile, tdataIdx + 1, NULL);
         }

         for (i = 0; i < n_addedfiles; i++)
         {
            DBGMSG("SCANNING %s FILE\n",addedfiles[i]->filename);
            /*For each added file, adding sections marked as allocated, executable or writable, and of type NOBITS or PROGBITS to the file
             * For those sections, we will consider the former index to be (added file index*256)+(section index)
             * The new sections will be named <source name file>_<original section name>*/
            int k;

            lastSectionIsTdata = 0;
            addedfiles_newaddrs[i] = *curraddr; //Storing the address of the first section of the added file
            /*We are assuming here that there is only one section in the added file containing executable code, or if there are more than one that
             * their respective starting addresses are not 0 but take into account the space between them.*/
            for (k = 0; k < addedfiles[i]->nsections; k++)
            {
               elfsection_t* addscn = addedfiles[i]->sections[k];
               if (elfsection_istls(addscn) == TRUE)
               {
                  if (Elf_Shdr_get_sh_size (NULL, 0, addscn->scnhdr) == SHT_PROGBITS)
                  {
                     DBGMSG("FILE [%s] ADDS %#"PRIx64" IN TDATA_MADRAS @%#"PRIx64"\n",addedfiles[i]->filename,Elf_Shdr_get_sh_size (NULL, 0, addscn->scnhdr),tdataOffset);
                     //COPY SECTION'S DATA INTO TDATA_MADRAS
                     void* data = addscn->bits;
                     memcpy(tdataDest, data, Elf_Shdr_get_sh_size (NULL, 0, addscn->scnhdr));
                     hashtable_insert (offsetInTLSsection,addscn->scnhdr,(void*)tdataOffset);
                     tdataOffset += Elf_Shdr_get_sh_size (NULL, 0, addscn->scnhdr);//Offset of the next insertion
                     tdataDest += Elf_Shdr_get_sh_size (NULL, 0, addscn->scnhdr);//Adress for the next memcpy
                     lastSectionIsTdata = 1;//To compute the right offset if the same file have a .tdata & a .tbss section
                  }
                  else //TBSS
                  {
                     DBGMSG("FILE [%s] ADDS %#"PRIx64" IN TBSS_MADRAS @%#"PRIx64"\n",addedfiles[i]->filename,Elf_Shdr_get_sh_size (NULL, 0, addscn->scnhdr),tbssOffset);
                     memset(tbssDest,0, Elf_Shdr_get_sh_size (NULL, 0, addscn->scnhdr));
                     hashtable_insert (offsetInTLSsection,addscn->scnhdr,(void*)tbssOffset);
                     if (lastSectionIsTdata) //To compute the right offset:
                     tbssOffset += (Elf_Shdr_get_sh_size (NULL, 0, addscn->scnhdr)
                           - Elf_Shdr_get_sh_size (NULL, 0, addedfiles[i]->sections[k-1]->scnhdr));
                     else
                     tbssOffset += Elf_Shdr_get_sh_size (NULL, 0, addscn->scnhdr);

                     tbssDest += Elf_Shdr_get_sh_size (NULL, 0, addscn->scnhdr);
                  }
               }

               else
               {
                  if ( elfsection_isloaded(addscn) == TRUE )
                  {
                     char cpyname[1024];
                     //Handles alignment
                     alignmod = *curraddr % Elf_Shdr_get_sh_addralign (NULL, 0, addscn->scnhdr);
                     if (alignmod)// Need to add a filler
                     {
                        *curraddr = *curraddr + Elf_Shdr_get_sh_addralign (efile, 0, addscn->scnhdr) - alignmod;
                        *curroffset = *curroffset + Elf_Shdr_get_sh_addralign (efile, 0, addscn->scnhdr) - alignmod;
                     }
                     //Adds the section to the file
                     sprintf(cpyname, ".%s_%s", addedfiles[i]->filename, elfsection_name(addedfiles[i], k));
                     elffile_add_scncopy (efile, addscn, cpyname, *curraddr,*curroffset);
                     hashtable_insert(copiedscns, addscn, efile->sections[efile->nsections-1]);
                     Elf_Shdr_set_sh_addr (efile, 0, addscn->scnhdr, *curraddr);
                     Elf_Shdr_set_sh_offset (efile, 0, addscn->scnhdr, *curroffset);
                     /**\todo This is not clean, as I am modifying the section from the inserted file.
                      * A better way would be to link only the section id and file index in the relocation object (relocation_addscn), then
                      * encode them in the new section id for instance as "(file id * 256) + section id". Then we would only need to find the
                * section with that original id.*/
                     //Updates the permutation array
                     permtbl = (int*)lc_realloc(permtbl,(sizeof(int)*efile->nsections));// Increase size of permutation table
                     permtbl[idx++] = efile->nsections-1;// Stores the index of the newly created section
                     *curraddr = *curraddr + Elf_Shdr_get_sh_size (NULL, 0, addscn->scnhdr);// Increases the start address
                     *curroffset = *curroffset + Elf_Shdr_get_sh_size (NULL, 0, addscn->scnhdr);// Increases the start offset
                     altsz += Elf_Shdr_get_sh_size (NULL, 0, addscn->scnhdr);
                  }
                  //Handling a relocation section in the file
                  if ((Elf_Shdr_get_sh_type (NULL, 0, addscn->scnhdr) == SHT_RELA
                              || Elf_Shdr_get_sh_type (NULL, 0, addscn->scnhdr) == SHT_REL)
                        && (elfsection_isloaded(addedfiles[i]->sections[Elf_Shdr_get_sh_info (NULL, 0, addscn->scnhdr)]) == TRUE))
                  {
                     DBGMSG("Adding relocations of section %s from file %s\n", elfsection_name(addedfiles[i], k), addedfiles[i]->filename);
                     elfrelocs_addscn(addscn, &relocations, &n_relocations, addedfiles[i], efile, addedfiles, n_addedfiles,
                           &max_comalign, &comscnsz, comsyms,offsetInTLSsection,tdataIdx);
                  }
               }
            }
         }

         int bssAlign = 32;
         //To align .madras_bss section
         alignmod = (*curraddr % bssAlign);
         if (alignmod) {
            *curraddr = *curraddr + bssAlign - alignmod;
            *curroffset = *curroffset + bssAlign - alignmod;
         }
         // Creating a section for unallocated symbols imported from the object file(madras.bss)
         if (comscnsz >= 0) {
            elffile_add_scn(efile,MADRAS_UNALLOCSCN_NAME,SHT_NOBITS,SHF_ALLOC|SHF_WRITE,*curraddr,*curroffset,SHN_UNDEF, 0,
                  bssAlign, *sizeToAddToBss, NULL);
            permtbl = (int*)lc_realloc(permtbl,(sizeof(int)*efile->nsections)); // Increase size of permutation table
            permtbl[idx++] = efile->nsections-1;
            bssIdx = efile->nsections-1;// Stores the index of the newly created section (madras.bss)
            addrBss = *curraddr;
            *curraddr = *curraddr + *sizeToAddToBss;
            //*curroffset = *curroffset + comscnsz;
         }

         //Insert the .tdata_madras and .tbss_madras at the end of our segment (if necesarry)
         // if (tdataIdx != -1 && segTLSneeded ==0)
         // {
         //   permtbl[idx++] =tdataIdx;      //Add .tdata_madras
         //   permtbl[idx++] =tdataIdx + 1; //Add .tbss_madras
         // }

         /*Now that all files have been added, we will need to perform the relocations*/
         for (i = 0; i < n_relocations; i++)
         {
            //Init .PLT index of the relocation (if needed)
            relocations[i].idxPlt = -1;
            relocations[i].reloPltIdx = -1;
            relocations[i].reloPltOffset = -1;

            if (relocations[i].scn == NULL)
            continue;//Something bad happened to this relocation and we want to avoid it (SEE ALSO WEAK SYMBOLS)

            //TODO: COMMENT : WHY WE USE COPIED SECTION.
            if ( (copiedscn = (elfsection_t*)hashtable_lookup(copiedscns, relocations[i].scn) ) != NULL )
            {
               relocations[i].scn = copiedscn;
            }
            tableentry_t* symbol = relocations[i].symentry;
            if ( ( relocations[i].reloscn == NULL )) //Handling the case of unallocated symbols (.bss)
            {
               //relocations[i].relofile = efile;
               relocations[i].reloscn = efile->sections[bssIdx];//Setting the section as the one containing unallocated symbols
               if (symbol->entry_64 != NULL)
               symbol->entry_64->addr = (uint64_t)hashtable_lookup(symbolAdded,symbol->entry_64->name);
               else if (symbol->entry_32 != NULL)
               symbol->entry_32->addr = (uint64_t)hashtable_lookup(symbolAdded,symbol->entry_32->name);

               if ((symbol->entry_64 != NULL && symbol->entry_64->addr == 0)
                     || (symbol->entry_32 != NULL && symbol->entry_32->addr == 0))
               {  //Variable has not yet been affected into the bss section
                  if (symbol->entry_64 != NULL)
                  {
                     DBGMSG ("Symbol %s IS NOT YET AFFECTED\n",symbol->entry_64->name);
                  }
                  else if (symbol->entry_32 != NULL)
                  {
                     DBGMSG ("Symbol %s IS NOT YET AFFECTED\n",symbol->entry_32->name);
                  }

                  if (relocations[i].reloalign > 0)
                  {
                     while (((addrBss + topcom) % relocations[i].reloalign) != 0)
                     topcom++; //Shifts the beginning of the variable in the bss section until the alignment is satisfied
                  }
                  //set the address of the variable
                  if (symbol->entry_64 != NULL)
                  symbol->entry_64->addr = addrBss + topcom;
                  else if (symbol->entry_32 != NULL)
                  symbol->entry_32->addr = addrBss + topcom;

                  /**\todo See the remark above on modifying the section's address. I'm modifying the entry
                   * containing the symbol to have its address in the new file in order to signal that
                   * it has been affected (otherwise the variable would be duplicated in the bss for each reference.
                   * Something should be added using maybe the comsyms queue, which I am actually not using anymore*/
                  topcom += relocations[i].relosize; //symbol->elfentry.sym->st_size;
                  // TODO : HANDLE TARGETADDR OR PUT symbol-addr DIRECTLY?
                  //elffile_add_targetaddr(efile,gotaddr+idxInGot,symbol->addr,*GElf_Addr_updaddr,bssIdx,i);

                  //The relocation deals with .GOT section?
               if (relocations[i].relotype == R_X86_64_GOT32 || relocations[i].relotype == R_X86_64_GOTPCREL
                     || relocations[i].relotype == R_X86_64_GOTPCRELX || relocations[i].relotype == R_X86_64_REX_GOTPCRELX)
                  {
                     if (symbol->entry_64 != NULL)
                     {
                        DBGMSG("ADD ONE ELEMENT IN THE GOT: %s\n", symbol->entry_64->name);
                        //Add in the .GOT section the address in the .BSS section of the variable (GOT[i]=&BSS[topcom])
                        gotAddr[idxInGot] = symbol->entry_64->addr;
                        //Insert the reference of the variable into the variablesInGot hashtable (key=address of the variable, value=index in GOT).
                        hashtable_insert (variablesInGot,(void*)symbol->entry_64->name,(void*)idxInGot);
                     }
                     else if (symbol->entry_32 != NULL)
                     {
                        DBGMSG("ADD ONE ELEMENT IN THE GOT: %s\n", symbol->entry_32->name);
                        //Add in the .GOT section the address in the .BSS section of the variable (GOT[i]=&BSS[topcom])
                        gotAddr[idxInGot] = symbol->entry_32->addr;
                        //Insert the reference of the variable into the variablesInGot hashtable (key=address of the variable, value=index in GOT).
                        hashtable_insert (variablesInGot,(void*)symbol->entry_32->name,(void*)idxInGot);
                     }
                     relocations[i].reloGotIdx = idxInGot; //Store the index in which we added the variable address.
                     idxInGot++;//The index of the next insertion.
                     nbVarInBss++;//A counter of number of variables inserted in the .GOT section.
                  }
                  //Now than we have inserted the symbol in the .bss we put the address in the corresponding hashtable(symbolAdded)
                  if (symbol->entry_64 != NULL)
                  hashtable_insert(symbolAdded,symbol->entry_64->name,(void*)symbol->entry_64->addr);
                  else if (symbol->entry_32 != NULL)
                  hashtable_insert(symbolAdded,symbol->entry_32->name,(void*)symbol->entry_32->addr);
               }
               else //The variable has been already put into the .madras.bss section.
               {
                  //The relocation deals with .GOT section?
               if (relocations[i].relotype == R_X86_64_GOT32 || relocations[i].relotype == R_X86_64_GOTPCREL
                     || relocations[i].relotype == R_X86_64_GOTPCRELX || relocations[i].relotype == R_X86_64_REX_GOTPCRELX)
                  {
                     // TODO : ADD TEST FOR THE RETURN OF HASHTABLE_LOOKUP
                     if (symbol->entry_64 != NULL)
                     idxSymbolAddrFound = (uint64_t)hashtable_lookup(variablesInGot,(void*)symbol->entry_64->name);
                     else if (symbol->entry_32 != NULL)
                     idxSymbolAddrFound = (uint64_t)hashtable_lookup(variablesInGot,(void*)symbol->entry_32->name);
                     relocations[i].reloGotIdx = idxSymbolAddrFound;
                  }
               }
               // Setting the relocation offset value from the address of the symbol in the bss;
               // Relooffset is the offset of the variable in the .BSS section
               if (symbol->entry_64 != NULL)
               relocations[i].relooffset = symbol->entry_64->addr - addrBss;
               else if (symbol->entry_32 != NULL)
               relocations[i].relooffset = symbol->entry_32->addr - addrBss;
            } else //Relocations pointing on sections already created
            {
               //We will now handle the case of copied sections: we will have to replace them with the copied sections
               if ( (copiedscn = (elfsection_t*)hashtable_lookup(copiedscns, relocations[i].reloscn) ) != NULL ) {
                  relocations[i].reloscn = copiedscn;
               }

               //MECHANISM TO HANDLE IFUNC RELOCATION
               if ((symbol->entry_64!= NULL
                           && ELF64_ST_TYPE(symbol->entry_64->elfentry.sym->st_info) == STT_GNU_IFUNC
                           && relocations[i].relotype == R_X86_64_PLT32)
                     || (symbol->entry_32!= NULL
                           && ELF32_ST_TYPE(symbol->entry_32->elfentry.sym->st_info) == STT_GNU_IFUNC
                           && relocations[i].relotype == R_X86_64_PLT32))
               {
                  if (symbol->entry_64 != NULL)
                  {
                     DBGMSG("%s IS AN GNU_IFUNC RELO\n",symbol->entry_64->name);
                  }
                  else if (symbol->entry_32 != NULL)
                  {
                     DBGMSG("%s IS AN GNU_IFUNC RELO\n",symbol->entry_32->name);
                  }

                  if (efile->indexes[PLT_IDX] == -1)
                  {
                     STDMSG("/!\\WARNING :PLT section in the original file doesn't exist!! /!\\\nWE NEED IT FOR IFUNC RELO\n");
                  }
                  else
                  {
                     //Search the rela table corresponding to the .PLT section
                     int j=0,k;
                     elfsection_t* currentScn;
                     while (relocations[i].reloPltOffset == -1 && j<efile->nsections)
                     {
                        currentScn = efile->sections[j];
                        //Search the .rela.plt section

                        if (Elf_Shdr_get_sh_type (NULL, 0, currentScn->scnhdr) == SHT_RELA
                              && Elf_Shdr_get_sh_info (NULL, 0, currentScn->scnhdr) == (unsigned int) efile->indexes[PLT_IDX])
                        {
                           tableentry_t* entry;
                           for (k = 0; k < currentScn->nentries; k++) //FOR EACH ENTRY OF THE SECTION
                           {
                              entry = currentScn->table_entries[k];
                              //For the IFUNC relos, the address is stored in the r_addend...(So obvious :)
                              if ((entry->entry_64 != NULL
                                          && ELF64_R_TYPE(entry->entry_64->elfentry.rela->r_info) == R_X86_64_IRELATIVE
                                          && entry->entry_64->elfentry.rela->r_addend == (int64_t)symbol->entry_64->elfentry.sym->st_value)
                                    || (entry->entry_32 != NULL
                                          && ELF32_R_TYPE(entry->entry_32->elfentry.rela->r_info) == R_386_IRELATIVE
                                          && entry->entry_32->elfentry.rela->r_addend == (int32_t)symbol->entry_32->elfentry.sym->st_value))
                              {
                                 //We match the address so the index 'k' is the index entry in the .PLT section
                                 DBGMSG("Symbol FOUND IN ORIGINAL .PLT : Index=%d\n",k);
                                 relocations[i].reloPltOffset = k;
                                 relocations[i].idxPlt = efile->indexes[PLT_IDX];
                                 relocations[i].reloPltIdx = k;
                                 break;
                              }
                           }

                           if (k >= currentScn->nentries)
                           {
                              if (symbol->entry_64 != NULL)
                              {
                                 DBGMSG("THE FUNCTION %s IS NOT DEFINE IN THE .PLT OF THE ORIGINAL FILE:Relo @%#"PRIx64"\n",
                                       symbol->entry_64->name,Elf_Shdr_get_sh_addr (NULL, 0, relocations[i].scn->scnhdr) + relocations[i].offset);
                              }
                              else if (symbol->entry_32 != NULL)
                              {
                                 DBGMSG("THE FUNCTION %s IS NOT DEFINE IN THE .PLT OF THE ORIGINAL FILE:Relo @%#"PRIx64"\n",
                                       symbol->entry_32->name,Elf_Shdr_get_sh_addr (NULL, 0, relocations[i].scn->scnhdr) + relocations[i].offset);
                              }
                              //ARCHI SPECIFIC!! TODO: HANDLE THIS IN A BETTER WAY DURING THE PATCHER REDESIGNING
                              int idxToAdd = 0;
                              if (symbol->entry_64 != NULL)
                              idxToAdd = ((long int) hashtable_lookup(variablesInPltGot, symbol->entry_64->name));
                              else if (symbol->entry_32 != NULL)
                              idxToAdd = ((long int) hashtable_lookup(variablesInPltGot, symbol->entry_32->name));

                              if(idxToAdd == 0)
                              {
                                 int64_t addrToAdd = -1;
                                 addrToAdd = relocations[i].relooffset + Elf_Shdr_get_sh_addr (NULL, 0, relocations[i].reloscn->scnhdr);
                                 if (symbol->entry_64 != NULL)
                                 {
                                    DBGMSG("ADD ONE ELEMENT:%s IN GOT.PLT[%"PRId64"] WITH VALUE: %#"PRIx64"\n",symbol->entry_64->name,idxInGotPlt,addrToAdd);
                                 }
                                 else if (symbol->entry_32 != NULL)
                                 {
                                    DBGMSG("ADD ONE ELEMENT:%s IN GOT.PLT[%"PRId64"] WITH VALUE: %#"PRIx64"\n",symbol->entry_32->name,idxInGotPlt,addrToAdd);
                                 }

                                 //Add in the .GOT.PLT section the address of the function (symbol->name)  (GOT.PLT[i]=ADDR_IN_FILE(symbol->name))
                                 gotPltAddr[idxInGotPlt] = addrToAdd;
                                 //Insert the reference of the variable into the variablesInGot hashtable (key=symbol name, value=index in GOT).
                                 if (symbol->entry_64 != NULL)
                                 hashtable_insert (variablesInPltGot,(void*)(symbol->entry_64->name),(void*)(idxInGotPlt+1));
                                 else if (symbol->entry_32 != NULL)
                                 hashtable_insert (variablesInPltGot,(void*)(symbol->entry_32->name),(void*)(idxInGotPlt+1));

                                 idxToAdd = idxInGotPlt+1;//+1 because the hash_table could return 0 and 0 could be an index! So we store in the hashtable the index+1
                                 idxInGotPlt++;//The index of the next insertion.
                                 *nbNewGnuIfunc = *nbNewGnuIfunc+1;
                                 //STUB IN THE PLT
                                 // JMP *0(%RIP)      -- firstIns  : points to a cell in the GOT.PLT (it contains the address of the function)
                                 // PUSHQ 0        -- secondIns : useless for static PLT
                                 // JMPQ  @NextInst   -- thirdIns  : useless for static PLT
                                 asmbldriver_t* driver = asmbldriver_load_byarchcode(ARCH_x86_64);
                                 insn_t* firstIns, *secondIns, *thirdIns;
                                 //driver->generate_insnlist_pltcall(7,,GOTRETURN,GOTCALL);
                                 driver->generate_insnlist_jmpaddr(NULL,&thirdIns);
                                 queue_t* stubPlt = driver->generate_insnlist_pltcall(idxInGotPlt,thirdIns,&firstIns,&secondIns);
                                 elffile_add_targetaddr(efile,queue_peek_head(stubPlt),Elf_Shdr_get_sh_addr (efile, efile->indexes[GOTPLT_IDX], NULL) + (idxToAdd-1) * 0x8,
                                       *(driver->upd_dataref_coding),efile->indexes[GOTPLT_IDX],0);
                                 //Append to the entrie to the final .madras.plt list
                                 queue_append(stubPltList,stubPlt);
                                 //CREATE ONE ENTRIE IN THE .MADRAS.RELA.PLT
                                 //OFFSET = ADDR_OF_GOT_PLT[I] --> idxInGotPlt+ gotPltAddr //TO TEST
                                 //INFO    = R_X86_64_IRELATIVE;
                                 //ADDEND = Adress of wrapper function (ex: strcpy is the wrapper function of strcpy_sse...)
                                 if (symbol->entry_64 != NULL)
                                 {
                                    Elf64_Rela* madrasPltEntry = lc_malloc(sizeof(*madrasPltEntry));
                                    madrasPltEntry->r_offset = Elf_Shdr_get_sh_addr (efile, efile->indexes[GOTPLT_IDX], NULL) + (idxInGotPlt-1)*0x8;
                                    madrasPltEntry->r_addend = addrToAdd;
                                    madrasPltEntry->r_info = ELF64_R_INFO(0x0,R_X86_64_IRELATIVE);
                                    (*madrasRelaPltEntries) = lc_realloc((*madrasRelaPltEntries),sizeof(tableentry_t*) * (*nbNewGnuIfunc));
                                    (*madrasRelaPltEntries)[(*nbNewGnuIfunc-1)] = lc_malloc (sizeof(tableentry_t));
                                    (*madrasRelaPltEntries)[(*nbNewGnuIfunc-1)]->entry_64 = lc_malloc (sizeof (tableentry_64_t));
                                    (*madrasRelaPltEntries)[(*nbNewGnuIfunc-1)]->entry_32 = NULL;
                                    (*madrasRelaPltEntries)[(*nbNewGnuIfunc-1)]->entry_64->name = NULL;
                                    (*madrasRelaPltEntries)[(*nbNewGnuIfunc-1)]->entry_64->elfentry.rela = madrasPltEntry;
                                 }
                                 else if (symbol->entry_32 != NULL)
                                 {
                                    Elf32_Rela* madrasPltEntry = lc_malloc(sizeof(*madrasPltEntry));
                                    madrasPltEntry->r_offset = Elf_Shdr_get_sh_addr (efile, efile->indexes[GOTPLT_IDX], NULL) + (idxInGotPlt-1)*0x8;
                                    madrasPltEntry->r_addend = addrToAdd;
                                    madrasPltEntry->r_info = ELF32_R_INFO(0x0,R_386_IRELATIVE);
                                    (*madrasRelaPltEntries) = lc_realloc((*madrasRelaPltEntries),sizeof(tableentry_t*) * (*nbNewGnuIfunc));
                                    (*madrasRelaPltEntries)[(*nbNewGnuIfunc-1)] = lc_malloc (sizeof(tableentry_t));
                                    (*madrasRelaPltEntries)[(*nbNewGnuIfunc-1)]->entry_64 = NULL;
                                    (*madrasRelaPltEntries)[(*nbNewGnuIfunc-1)]->entry_32 = lc_malloc (sizeof (tableentry_32_t));
                                    (*madrasRelaPltEntries)[(*nbNewGnuIfunc-1)]->entry_32->name = NULL;
                                    (*madrasRelaPltEntries)[(*nbNewGnuIfunc-1)]->entry_32->elfentry.rela = madrasPltEntry;
                                 }
                              }
                              relocations[i].reloPltOffset = idxToAdd-1 - *oldGotPltSize; //Store the madras.plt index.
                              relocations[i].idxPlt = madrasPltScnIdx;//Link the relocation to the .madras.plt section (and not the original)
                              break;
                           }
                        }
                        j++;
                     }
                  }
               }
               //RELOCATIONS POINTING ON NO_BITS SECTION (.BSS OR .TBSS SECTION).
               //We need to add the .bss of the inserted file in our .bss.
               //If it points to the .bss section of the patched file (-->the symbol is defined in the original file)
               //we don't need to add it ... CQFD :)
               // if ((relocations[i].reloscn->scnhdr != bssHdr && relocations[i].reloscn->scnhdr != tbssHdr)
               //    && (relocations[i].reloscn->scnhdr->sh_type == SHT_NOBITS))
               // {
               //The symbol is pointing on one of the original section?
               idxSection=0;
               while (idxSection < efile->nsections && relocations[i].reloscn->scnhdr != efile->sections[idxSection]->scnhdr)
               {
                  idxSection++;
               }
               //The symbol is not pointing on one of the original NOBITS sections (and not on .tbss section)
               if (idxSection == efile->nsections
                     && Elf_Shdr_get_sh_type (NULL, 0, relocations[i].reloscn->scnhdr) == SHT_NOBITS
                     && (Elf_Shdr_get_sh_flags (NULL, 0, relocations[i].reloscn->scnhdr) & SHF_TLS) == 0)
               {
                  uint64_t offsetInBss = (uint64_t)hashtable_lookup(bssToAdd,relocations[i].reloscn->scnhdr);
                  //.bss section is already inserted ?
                  if(offsetInBss == 0)
                  {
                     int bssAlign = 32;
                     int alignment = topcom % bssAlign;
                     DBGMSG ("Add a .BSS section @%#"PRIx64" [size=%#"PRIx64"]\n",topcom+addrBss+alignment,
                           Elf_Shdr_get_sh_size (NULL, 0, relocations[i].reloscn->scnhdr));
                     hashtable_insert (bssToAdd,(void*)relocations[i].reloscn->scnhdr,(void*)(topcom+alignment));
                     //topcom+=relocations[i].reloscn->scnhdr->sh_size;
                     //offsetInBss = addrBss+topcom;
                     offsetInBss = topcom + alignment;
                     topcom += (Elf_Shdr_get_sh_size (NULL, 0, relocations[i].reloscn->scnhdr) + alignment);
                  }
                  Elf_Shdr_set_sh_addr (NULL, 0, relocations[i].reloscn->scnhdr, addrBss + offsetInBss);
                  if (relocations[i].symentry->entry_64 != NULL)
                  relocations[i].relooffset = relocations[i].symentry->entry_64->elfentry.sym->st_value;
                  else if (relocations[i].symentry->entry_32 != NULL)
                  relocations[i].relooffset = relocations[i].symentry->entry_32->elfentry.sym->st_value;
               }

               if (//(relocations[i].reloscn->scnhdr != bssHdr) &&
                  (relocations[i].relotype == R_X86_64_GOT32 || relocations[i].relotype == R_X86_64_GOTPCREL
                        || relocations[i].relotype == R_X86_64_GOTPCRELX || relocations[i].relotype == R_X86_64_REX_GOTPCRELX)
               )
               {
                  int idxToAdd = 0;
                  //THE ENTRY IS ALREADY PRESENT IN THE ORIGINAL GOT SECTION
                  if(relocations[i].symbolIsExtracted == 2)
                  {
                     idxToAdd = (relocations[i].relooffset - *oldGotAddr)/0x8;
                     relocations[i].reloGotIdx = idxToAdd;
                  }
                  else
                  {
                     if (symbol->entry_64 != NULL)
                     idxToAdd =(long int)hashtable_lookup(variablesInGot, symbol->entry_64->name);
                     else if (symbol->entry_32 != NULL)
                     idxToAdd =(long int)hashtable_lookup(variablesInGot, symbol->entry_32->name);
                     if(idxToAdd == 0)
                     {
                        int64_t addrToAdd = -1;
                        if (relocations[i].symbolIsExtracted == 1
                              || Elf_Shdr_get_sh_type (NULL, 0, relocations[i].reloscn->scnhdr) == SHT_NOBITS)
                        addrToAdd = relocations[i].relooffset + Elf_Shdr_get_sh_addr (NULL, 0, relocations[i].reloscn->scnhdr); //symbol->addr+relocations[i].reloscn->scnhdr->sh_addr;
                        else
                        {
                           if (symbol->entry_64 != NULL)
                           addrToAdd = symbol->entry_64->addr;
                           else if (symbol->entry_32 != NULL)
                           addrToAdd = symbol->entry_32->addr;
                        }

                        if (symbol->entry_64 != NULL)
                        {
                           DBGMSG("ADD ONE ELEMENT:%s IN GOT[%"PRId64"] WITH VALUE: %#"PRIx64"\n",symbol->entry_64->name,idxInGot,addrToAdd);
                           //Add in the .GOT section the address in the .BSS section of the variable (GOT[i]=&(BSS[SymbolPosition]))
                           gotAddr[idxInGot] = addrToAdd;
                           //Insert the reference of the variable into the variablesInGot hashtable (key=symbol name, value=index in GOT).
                           hashtable_insert (variablesInGot,(void*)(symbol->entry_64->name),(void*)idxInGot);
                        }
                        else if (symbol->entry_32 != NULL)
                        {
                           DBGMSG("ADD ONE ELEMENT:%s IN GOT[%"PRId64"] WITH VALUE: %#"PRIx64"\n",symbol->entry_32->name,idxInGot,addrToAdd);
                           //Add in the .GOT section the address in the .BSS section of the variable (GOT[i]=&(BSS[SymbolPosition]))
                           gotAddr[idxInGot] = addrToAdd;
                           //Insert the reference of the variable into the variablesInGot hashtable (key=symbol name, value=index in GOT).
                           hashtable_insert (variablesInGot,(void*)(symbol->entry_32->name),(void*)idxInGot);
                        }
                        idxToAdd = idxInGot;
                        idxInGot++;    //The index of the next insertion.
                     }
                     relocations[i].reloGotIdx = idxToAdd; //Store the index in which we added the variable address.
                  }
               }
               if (//(relocations[i].reloscn->scnhdr != tbssHdr) &&
                     (relocations[i].relotype == R_X86_64_GOTTPOFF || relocations[i].relotype == R_X86_64_TPOFF32))
               {
                  int64_t varOffset = 0;
                  uint64_t sectionOffset = 0;
                  if (relocations[i].reloscn->scnhdr == tdataHdr || relocations[i].reloscn->scnhdr == tbssHdr)
                  {
                     //Relocation deals with the TLS (in the original file)
                     //Get the size of the original TLS segment
                     if (symbol->entry_64 != NULL)
                     varOffset = symbol->entry_64->elfentry.sym->st_value - originalSegSize;
                     else if (symbol->entry_32 != NULL)
                     varOffset = symbol->entry_32->elfentry.sym->st_value - originalSegSize;
                  }
                  else
                  {
                     //Relocation deal with the .madras_tbss section (symbol imported from an object file)
                     sectionOffset=(uint64_t)hashtable_lookup(offsetInTLSsection,(void*)relocations[i].reloscn->scnhdr);
                     //varOffset is the offset of the TLS variable in the MADRAS_TLS segment
                     //varOffset = sectionOffset + symbol->elfentry.sym->st_value - efile->sections[tdataIdx]->scnhdr->sh_addr;
                     if (symbol->entry_64 != NULL)
                     {
                        varOffset = sectionOffset + symbol->entry_64->elfentry.sym->st_value - originalSegSize - newSegSize;
                        DBGMSG("sectionOffset=%#"PRIx64" && originalSegSize+newSegSize= %#"PRIx64" && st_value=%#"PRIx64"\n",sectionOffset,
                              newSegSize+originalSegSize,symbol->entry_64->elfentry.sym->st_value);
                     }
                     else if (symbol->entry_32 != NULL)
                     {
                        varOffset = sectionOffset + symbol->entry_32->elfentry.sym->st_value - originalSegSize - newSegSize;
                        DBGMSG("sectionOffset=%#"PRIx64" && originalSegSize+newSegSize= %#"PRIx64" && st_value=%#"PRIx32"\n",sectionOffset,
                              newSegSize+originalSegSize,symbol->entry_32->elfentry.sym->st_value);
                     }
                  }
                  int idxToAdd=(long int)hashtable_lookup(varTLSinGot,(void*)varOffset);
                  if(idxToAdd == 0)
                  {
                     if (symbol->entry_64 != NULL)
                     {
                        DBGMSG("ADD ONE ELEMENT:%s IN GOT[%"PRId64"] WITH VALUE: %#"PRIx64" (DEALS WITH TLS)\n",symbol->entry_64->name,idxInGot,varOffset);
                     }
                     else if (symbol->entry_32 != NULL)
                     {
                        DBGMSG("ADD ONE ELEMENT:%s IN GOT[%"PRId64"] WITH VALUE: %#"PRIx64" (DEALS WITH TLS)\n",symbol->entry_32->name,idxInGot,varOffset);
                     }

                     gotAddr[idxInGot] = varOffset;
                     hashtable_insert (varTLSinGot,(void*)varOffset,(void*)idxInGot);
                     idxToAdd = idxInGot;
                     idxInGot++;    //The index of the next insertion.
                  }
                  relocations[i].reloGotIdx = idxToAdd; //Store the index in which we added the variable address.
                  relocations[i].immediateValue = varOffset;//
                  if (relocations[i].symentry->entry_64 != NULL)
                  {
                     DBGMSGLVL(1,"VARIABLE %s\t AT OFFSET %#"PRIx64" IN TLS SEGMENT  (OFFSET SAVED IN GOT[%d])\n",relocations[i].symentry->entry_64->name,
                           varOffset, idxToAdd);
                  }
                  else if (relocations[i].symentry->entry_32 != NULL)
                  {
                     DBGMSGLVL(1,"VARIABLE %s\t AT OFFSET %#"PRIx64" IN TLS SEGMENT  (OFFSET SAVED IN GOT[%d])\n",relocations[i].symentry->entry_32->name,
                           varOffset, idxToAdd);
                  }
               }
            }
            if (symbol->entry_64 != NULL)
            {
               DBGMSG ("Relocation[%d].offset = %#"PRIx64" -> \"%s\"\n",i,relocations[i].offset,symbol->entry_64->name);
            }
            else if (symbol->entry_32 != NULL)
            {
               DBGMSG ("Relocation[%d].offset = %#"PRIx64" -> \"%s\"\n",i,relocations[i].offset,symbol->entry_32->name);
            }
            int res = elfreloc_apply(&(relocations[i]));
            if (res != EXIT_SUCCESS)
            efile->last_error_code = res;
         }

         *permtbl_idx = idx;
         *newsz = altsz;
         DBGMSG("FILE ADDED = %d\n",n_addedfiles);
      }
      return permtbl;
   }

   uint64_t scanGotPcRelocations(int n_addedfiles, elffile_t** addedfiles)
   {
      int i,j,k;
      uint64_t GotPcRelosCounter=0;
      tableentry_t* entry;

      //FOR ALL ADDED FILES
      for (i = 0; i < n_addedfiles; i++)
      {
         //SCAN ALL SECTION OF THE FILE
         for (j = 0; j < addedfiles[i]->nsections; j++)
         {
            elfsection_t* addscn = addedfiles[i]->sections[j];
            int sh_type = Elf_Shdr_get_sh_type (NULL, 0, addscn->scnhdr);
            int sh_info = Elf_Shdr_get_sh_info (NULL, 0, addscn->scnhdr);

            //.GOT OR .GOT.PLT SECTION?
            if ((sh_type == SHT_RELA || sh_type == SHT_REL)
                  && (elfsection_isloaded(addedfiles[i]->sections[sh_info]) == TRUE))
            {
               for ( k = 0; k < addscn->nentries; k++) //FOR EACH ENTRY OF THE SECTION
               {
                  entry = addscn->table_entries[k];
                  if (entry->entry_64 != NULL
                        && (ELF64_R_TYPE(entry->entry_64->elfentry.rel->r_info) == R_X86_64_GOTPCREL //THESE RELOCATIONS NEED TO ADD ONE ELEMENT IN THE GOT.
                              || ELF64_R_TYPE(entry->entry_64->elfentry.rel->r_info) == R_X86_64_GOT32
                              || ELF64_R_TYPE(entry->entry_64->elfentry.rel->r_info) == R_X86_64_GOTTPOFF
                     ELF64_R_TYPE(entry->entry_64->elfentry.rel->r_info) == R_X86_64_GOTPCRELX   ||
                     ELF64_R_TYPE(entry->entry_64->elfentry.rel->r_info) == R_X86_64_REX_GOTPCRELX   ||
                              || ELF64_R_TYPE(entry->entry_64->elfentry.rel->r_info) == R_X86_64_TPOFF32))
                  GotPcRelosCounter++;
                  else if (entry->entry_32 != NULL
                        && ELF32_R_TYPE(entry->entry_32->elfentry.rel->r_info) == R_386_GOT32)
                  GotPcRelosCounter++;
               }
            }
         }
      }

      return GotPcRelosCounter;
   }

   void preliminaryScanOfSymtab(int n_addedfiles, elffile_t** addedfiles,uint64_t* originalBssSize,uint64_t* sizeToAddToBss, uint64_t* tdataSize, uint64_t* tbssSize,int* newGotPltElement)
   {

      //Hashtable which permits to not duplicate the insertion of .bss section of an object file into our final .madras.bss section
      hashtable_t* bssAdded = hashtable_new(int_hash,int_equal);
      int i,e,symtabIdx;
      int bssAlign = 32;
      tableentry_t* entry;
      for (i = 0; i < n_addedfiles; i++)
      {
         //SCAN .symtab AND SEARCH VARS OF TYPE COM (GLOBAL VARIABLES NO INITIALIZED -> IN .bss)
         symtabIdx = addedfiles[i]->indexes[SYMTAB_IDX];
         if (symtabIdx > -1)//.symtab EXISTS?
         {
            elfsection_t* symtabscn = addedfiles[i]->sections[addedfiles[i]->indexes[SYMTAB_IDX]];
            //fprintf(stderr, "File :%s en %d\n",addedfiles[i]->filename,i);
            for (e=0; e<symtabscn->nentries;e++)//CHECK ALL ENTRIES OF .symtab
            {
               entry = symtabscn->table_entries[e];
               //CHECK IF IT'S A GNU_IFUNC
               if ((entry->entry_64 != NULL && ELF64_ST_TYPE(entry->entry_64->elfentry.sym->st_info) == STT_GNU_IFUNC)
                     || (entry->entry_32 != NULL && ELF32_ST_TYPE(entry->entry_32->elfentry.sym->st_info) == STT_GNU_IFUNC))
               *newGotPltElement = *newGotPltElement +1;
               if (entry->entry_64 != NULL && entry->entry_64->elfentry.sym->st_shndx == SHN_COMMON)//UNINITIALIZED GLOBAL VARIABLES NOT POINTING ON A SECTION
               *sizeToAddToBss = *sizeToAddToBss + entry->entry_64->elfentry.sym->st_size + bssAlign;
               else if (entry->entry_32 != NULL && entry->entry_32->elfentry.sym->st_shndx == SHN_COMMON)
               *sizeToAddToBss = *sizeToAddToBss + entry->entry_32->elfentry.sym->st_size + bssAlign;
               else
               {
                  if (entry->entry_64 != NULL && ELF64_ST_TYPE(entry->entry_64->elfentry.sym->st_info) == STT_TLS)
                  {
                     if (entry->entry_64->elfentry.sym->st_shndx == addedfiles[i]->indexes[TDATA_IDX])
                     *tdataSize = *tdataSize + entry->entry_64->elfentry.sym->st_size;
                     else
                     {
                        if (entry->entry_64->elfentry.sym->st_shndx == addedfiles[i]->indexes[TBSS_IDX])
                        *tbssSize = *tbssSize + entry->entry_64->elfentry.sym->st_size;
                     }
                  }
                  else if (entry->entry_32 != NULL && ELF32_ST_TYPE(entry->entry_32->elfentry.sym->st_info) == STT_TLS)
                  {
                     if (entry->entry_32->elfentry.sym->st_shndx == addedfiles[i]->indexes[TDATA_IDX])
                     *tdataSize = *tdataSize + entry->entry_32->elfentry.sym->st_size;
                     else
                     {
                        if (entry->entry_32->elfentry.sym->st_shndx == addedfiles[i]->indexes[TBSS_IDX])
                        *tbssSize = *tbssSize + entry->entry_32->elfentry.sym->st_size;
                     }
                  }
                  else
                  {
                     int idxSection = 0;
                     if (entry->entry_64 != NULL)
                     {
                        idxSection = entry->entry_64->elfentry.sym->st_shndx;
                        if (idxSection > 0 && idxSection < addedfiles[i]->nsections)
                        {
                           //UNINITIALIZED GLOBAL VARIABLE POITING ON BSS SECTION
                           if (Elf_Shdr_get_sh_type (addedfiles[i], idxSection, NULL) == SHT_NOBITS)
                           {
                              if(hashtable_lookup(bssAdded, addedfiles[i]->sections[idxSection]->scnhdr) == NULL)
                              {
                                 hashtable_insert (bssAdded,(void*)addedfiles[i]->sections[idxSection]->scnhdr
                                       ,(void*)(*sizeToAddToBss+*originalBssSize));
                                 *sizeToAddToBss = *sizeToAddToBss + Elf_Shdr_get_sh_size (addedfiles[i], idxSection, NULL) + bssAlign;
                              }
                           }
                        }
                     }
                     else if (entry->entry_32)
                     {
                        idxSection = entry->entry_32->elfentry.sym->st_shndx;
                        if (idxSection > 0 && idxSection < addedfiles[i]->nsections)
                        {
                           //UNINITIALIZED GLOBAL VARIABLE POITING ON BSS SECTION
                           if (Elf_Shdr_get_sh_type (addedfiles[i], idxSection, NULL) == SHT_NOBITS)
                           {
                              if(hashtable_lookup(bssAdded, addedfiles[i]->sections[idxSection]->scnhdr) == NULL)
                              {
                                 hashtable_insert (bssAdded,(void*)addedfiles[i]->sections[idxSection]->scnhdr
                                       ,(void*)(*sizeToAddToBss+*originalBssSize));
                                 *sizeToAddToBss = *sizeToAddToBss + Elf_Shdr_get_sh_size (addedfiles[i], idxSection, NULL) + bssAlign;
                              }
                           }
                        }
                     }
                  }
               }
            }
         }
      }
      hashtable_free(bssAdded, NULL, NULL);
   }

   /**
    * Get the TLS segment extract from the program header.
    * \param   efile The elf file.
    * \return  tlsHdr   The TLS program header.
    * */
   Elf_Phdr* getTlsSegHdr(elffile_t* efile)
   {
      int i;
      Elf_Phdr* tlsHdr = NULL;
      for (i = 0; i < Elf_Ehdr_get_e_phnum (efile, NULL); i++)
      {
         if (Elf_Phdr_get_p_type (efile, i, NULL) == PT_TLS)
         {
            tlsHdr = efile->progheader[i];
            break;
         }
      }
      return tlsHdr;
   }

   /**
    * Increase the size of the original .bss section by the value of the variable sizeToAdd
    * \param baseSection
    * \param sectionToAdd
    * \return nothing
    * */
   void increaseBssSize (elfsection_t* originalBss ,uint64_t* sectionToAdd)
   {
      Elf_Shdr_set_sh_size (NULL, 0, originalBss->scnhdr, Elf_Shdr_get_sh_size (NULL, 0, originalBss->scnhdr) + *sectionToAdd);
   }

   /**
    ** Get the corresponding segment index of the given section
    ** \param  efile                      The elf file
    ** \param  sectionIdxToMap            The section that we need to map (section in which segment?)
    ** \param  mappingSectionSegmentArray       The array than stock for each segment which section they have.
    ** \return mappedSegmentIdx           The segment index for the sectionIdxToMap or -1 if not found
    **/
   static int getMappedSegmentIdx (elffile_t* efile, int sectionIdxToMap,int** mappingSectionSegmentArray)
   {
      int i,idx,mappedSegmentIdx=-1;

      //SANITY CHECK
      if (sectionIdxToMap < 0)
      return -1;

      for (i=0; i < Elf_Ehdr_get_e_phnum (efile, NULL); i++)
      {
         idx = 0;
         while (mappingSectionSegmentArray[i][idx] != -1) //-1 : We reach the end of the array
         {
            if (sectionIdxToMap == mappingSectionSegmentArray[i][idx])
            {
               mappedSegmentIdx = i;
               break;
            }
            idx ++;
         }
      }
      return mappedSegmentIdx;
   }

   /**
    ** Get the corresponding index of the rela.plt section
    ** \param  efile    The elf file
    ** \return idx      The rela.plt index
    **/
   static int searchRelaPlt (elffile_t* efile)
   {
   //If the index was found thanks to the dynamic table
   if (efile->indexes[JMPREL] > 0)
      return efile->indexes[JMPREL];
   /**\todo (2016-09-13) The case above was added because in files compiled with recent versions of gcc
    * the .rela.plt secion info field contains the index of the plt.got section, not the .plt, so the algorithm
    * below does not work. In dynamic files though, it seems as if the info field is correctly filled.
    * Check that this is correct */

   //Otherwise it's a static file and we have to use the plt
      if (efile->indexes[PLT_IDX] == -1)
      {
         STDMSG("SECTION .PLT DOESN'T EXIST!");
         return -1;
      }
      int idx = 0;
      for (idx=0; idx < efile->nsections;idx++)
      {
         if (Elf_Shdr_get_sh_info (efile, idx, NULL) == (unsigned int)efile->indexes[PLT_IDX])
         return idx;
      }

      return -2;
   }

   /**
    ** Modify the function __libc_csu_irel in static binary
    **      - Replace rela_iplt_start and rela_iplt_stop in hard in the assembly code
    **   - rela_iplt_start = rela.plt start addr , rela_iplt_stop = rela.plt end addr
    ** \param  efile                      The elf file
    ** \param  afile                      The asm file
    ** \param  originalRelaPltAddr           The original rela.plt address
    ** \param  originalRelaPltSize           The original rela.plt size
    **/
   void modifyLibcCsuIrel(elffile_t* efile,asmfile_t* afile,int64_t originalRelaPltAddr,int64_t originalRelaPltSize)
   {
      //SANITY CHECK
      int idxRelaPlt = searchRelaPlt(efile);
      if (idxRelaPlt == -1)
      return;

      int64_t oldRelaIpltStart = originalRelaPltAddr;
      int64_t oldRelaIpltEnd = oldRelaIpltStart + originalRelaPltSize;
      int64_t newRelaIpltStart = Elf_Shdr_get_sh_addr (efile, idxRelaPlt, NULL);
      int64_t newRelaIpltEnd = newRelaIpltStart + Elf_Shdr_get_sh_size (efile, idxRelaPlt, NULL);
   insn_t* firstInsnLibcCsuIrel = asmfile_get_insn_by_label(afile,"__libc_csu_irel");

      //SANITY CHECK: We patch a dynamic binary?
      if (firstInsnLibcCsuIrel == NULL)
      return;

   list_t* insnsLibcCsuIrel = insn_get_sequence(firstInsnLibcCsuIrel);
      insn_t* currentInsn = insnsLibcCsuIrel->data;
      oprnd_t** insnOperands = NULL;
      oprnd_t* newOprnd=NULL;
      int oprndId = 0;
      int* destType = NULL;
      int64_t newOffset,oldDestinationAddr,newDestinationAddr,oldImmediateAddr,newImmediateAddr;
   asmbldriver_t* driver = asmbldriver_load(asmfile_get_arch(afile));
   while (insnsLibcCsuIrel !=  NULL && strcmp(label_get_name(currentInsn->fctlbl),"__libc_csu_irel") == 0)
      {
         currentInsn = insnsLibcCsuIrel->data;
      insnOperands = insn_get_oprnds(currentInsn);
         if(insnOperands != NULL)
         {
            while (oprndId < insn_get_nb_oprnds(currentInsn))
            {
            switch (oprnd_get_type(insnOperands[oprndId]))
               {
                  case OT_MEMORY:
               DBGMSGLVL(1,"MEMORY: %#"PRIx64"\n",insn_check_refs(currentInsn,destType));
               oldDestinationAddr = insn_check_refs(currentInsn,destType);
                  if (oldDestinationAddr >= oldRelaIpltStart && oldDestinationAddr <= oldRelaIpltEnd)
                  {
                     if (oldDestinationAddr==oldRelaIpltEnd)
                     newDestinationAddr = newRelaIpltEnd;
                     else
                     newDestinationAddr = newRelaIpltStart + (oldDestinationAddr-oldRelaIpltStart);

                  newOffset = newDestinationAddr - (INSN_GET_ADDR(currentInsn)+(insn_get_size(currentInsn)/8));
                  oprnd_set_offset(insnOperands[oprndId],newOffset);
                     upd_assemble_insn(currentInsn,driver,FALSE,NULL);
                  DBGMSGLVL(1,"MODIFICATED MEMORY: %#"PRIx64"\n",insn_check_refs(currentInsn,destType));
                  }
                  break;

                  case OT_IMMEDIATE:
                  DBGMSGLVL(1,"IMMEDIATE: %#"PRIx64"\n",insnOperands[oprndId]->data.imm);
                  oldImmediateAddr = insnOperands[oprndId]->data.imm;
                  if (oldImmediateAddr >= oldRelaIpltStart && oldImmediateAddr <= oldRelaIpltEnd)
                  {
                     if (oldImmediateAddr==oldRelaIpltEnd)
                     newImmediateAddr = newRelaIpltEnd;
                     else
                     newImmediateAddr = newRelaIpltStart + (oldImmediateAddr-oldRelaIpltStart);
                  newOprnd = oprnd_new_imm(newImmediateAddr);
                  insn_set_oprnd(currentInsn,oprndId,newOprnd);
                     upd_assemble_insn(currentInsn,driver,FALSE,NULL);
                     DBGMSGLVL(1,"MODIFICATED IMMEDIATE: %#"PRIx64"\n",insnOperands[oprndId]->data.imm);
                  }
                  break;

                  case OT_POINTER:
               DBGMSGLVL(1,"POINTER: %#"PRIx64"\n",insn_check_refs(currentInsn,destType));
               oldDestinationAddr = insn_check_refs(currentInsn,destType);
                  if (oldDestinationAddr >= oldRelaIpltStart && oldDestinationAddr <= oldRelaIpltEnd)
                  {
                     if (oldDestinationAddr==oldRelaIpltEnd)
                     newDestinationAddr = newRelaIpltEnd;
                     else
                     newDestinationAddr = newRelaIpltStart + ( oldDestinationAddr-oldRelaIpltStart);
                  oprnd_set_ptr_addr(insnOperands[oprndId],newDestinationAddr);
                     insn_oprnd_updptr(currentInsn, insnOperands[oprndId]);
                     upd_assemble_insn(currentInsn,driver,FALSE,NULL);
                  DBGMSGLVL(1,"MODIFICATED POINTER: %#"PRIx64"\n",insn_check_refs(currentInsn,destType));
                  }
                  break;
                  default:
                  break;
               }
               oprndId++;
            }
            oprndId=0;
         }
         insnsLibcCsuIrel = insnsLibcCsuIrel->next;
      }
   }

   /*
    * Reorders sections in an Elf file
    * \param efile an elf file
    * \param codelen if not 0, size of code section to add
    * \param datalen if not 0, size of data section to add
    * \param pltlen if not 0, size of plt section to add
    * \param codeidx if codelen > 0, used to return the index of the created code section
    * \param dataidx if datalen > 0, used to return the index of the created data section
    * \param pltidx if pltlen > 0, used to return the index of the created plt section
    * \param lblname name of a label added at the begining of the first added section
    * \return an array containing new positions for sections
    * \todo can crash when working on .o files because of static patch code
    */
   int* elfexe_scnreorder (elffile_t* efile,asmfile_t* afile ,int64_t codelen, int64_t datalen, uint64_t tdataSize,
         uint64_t tbssSize, int64_t pltlen,
         int* codeidx, int* dataidx, int* tdataMadrasIdx, int tdataVarsSize, int* tbssMadrasIdx, int tbssVarsSize, int* pltidx,
         char* lblname, elffile_t** addedfiles, int64_t* addedfiles_newaddrs, int n_addedfiles,queue_t** pltList)
   {
      if (efile == NULL)
      return (NULL);

      //-------------------------------------------------------------------
      // A label can be added at the begining of a .madras section. As section sizes
      // must be known before reordering sections and the label needs the new .madras.code
      // section index, the label is created here, then updated at the end of the function
      // to change some unknown data
      int lblidx = -1;
      if (lblname != NULL && (datalen > 0 || pltlen > 0))
      lblidx = elffile_add_symlbl(efile, lblname, -1, 0, ELF64_ST_INFO(STB_GLOBAL,STT_NOTYPE),-1, FALSE);

      int nb_new_sections = 0;
      if (codelen > 0)
      nb_new_sections ++;
      if (datalen > 0)
      nb_new_sections ++;
      if (pltlen > 0)
      nb_new_sections ++;

      int i = 0, j = 0;
      int idx = 0;
      int* permtbl = lc_malloc (sizeof (int) * (efile->nsections + nb_new_sections));
      int altscnsz = 0;
      int idscnsz = 0;
      int shiftscnsz = 0;
      int altscn[efile->nsections];
      int shiftscn[efile->nsections];
      int idscn[efile->nsections];
      int64_t length_added_code = codelen + datalen + pltlen;
      int64_t address_added_code = -1;
      int64_t offset_added_code = -1;
      uint64_t curraddr = 0, curroff = 0;
      int** mapping_scn_seg = lc_malloc (sizeof (int*) * Elf_Ehdr_get_e_phnum (efile, NULL));

      //STATICPATCH
      uint64_t sizeToAddToBss = 0;
//   uint64_t tdataSize = 0;
//   uint64_t tbssSize = 0;
//   int tdataMadrasIdx = -1;
//   int tbssMadrasIdx = -1;
      *tdataMadrasIdx = -1;
      *tbssMadrasIdx = -1;

      int segTLSneeded = 0;
      int64_t madrasTLSsize = 0;
      unsigned elementInGotToAdd=0;
      int gotsize=0;
      uint64_t gotAddr = 0;
      uint64_t oldBssSize = 0;
      if (efile->indexes[BSS] != -1)
      oldBssSize = efile->sections[efile->indexes[BSS]]->scnhdr->sh_size;
      Elf64_Phdr* tlsHdr = getTlsSegHdr(efile);
      int tlsAlign = 0;

      //FOR MADRAS.PLT IN STATIC
      int nbNewGnuIfunc = 0;//Counter for GNU_IFUNC added to the binary.
      int newGotPltElement = 0;
      uint64_t originalRelaPltAddr = 0;
      uint64_t originalRelaPltSize = 0;
      queue_t* stubPltList = queue_new();
      int gotPltSize = 0;
      tableentry_t** madrasRelaPltEntries = NULL;
      int relaPltIdx = searchRelaPlt(efile);

   if(relaPltIdx > 0)
      {
         originalRelaPltAddr = Elf_Shdr_get_sh_addr (efile, relaPltIdx, NULL);
         originalRelaPltSize = Elf_Shdr_get_sh_size (efile, relaPltIdx, NULL);
      }

      if (efile->indexes[GOTPLT_IDX] != -1)
      {

         if (Elf_Ehdr_get_e_ident (efile, NULL)[EI_CLASS] == ELFCLASS64)
         gotPltSize = Elf_Shdr_get_sh_size (efile, efile->indexes[GOTPLT_IDX], NULL) / sizeof(Elf64_Addr);
         else if (Elf_Ehdr_get_e_ident (efile, NULL)[EI_CLASS] == ELFCLASS32)
         gotPltSize = Elf_Shdr_get_sh_size (efile, efile->indexes[GOTPLT_IDX], NULL) / sizeof(Elf32_Addr);
      }
      else
      gotPltSize = 0;
      //GET THE NUMBER OF GOTPC TYPE RELOCATION.
      elementInGotToAdd = scanGotPcRelocations(n_addedfiles,addedfiles);

      //GET THE SIZE OF THE .GOT SECTION
      if (efile->indexes[GOT_IDX] != -1)
      {
         if (Elf_Ehdr_get_e_ident (efile, NULL)[EI_CLASS] == ELFCLASS64)
         {
            gotsize = Elf_Shdr_get_sh_size (efile, efile->indexes[GOT_IDX], NULL) / sizeof(Elf64_Addr);
            gotAddr = Elf_Shdr_get_sh_addr (efile, efile->indexes[GOT_IDX], NULL);
         }
         else if (Elf_Ehdr_get_e_ident (efile, NULL)[EI_CLASS] == ELFCLASS32)
         {
            gotsize = Elf_Shdr_get_sh_size (efile, efile->indexes[GOT_IDX], NULL) / sizeof(Elf32_Addr);
            gotAddr = Elf_Shdr_get_sh_addr (efile, efile->indexes[GOT_IDX], NULL);
         }
      }

      // IF NECESSARY WE INCREASE THE SIZE OF THE .GOT SECTION
      if (elementInGotToAdd > 0)
      elffile_add_gotentries (efile, &elementInGotToAdd);

      DBGMSG("GOT'S VARIABLE ADDED = %u\n",elementInGotToAdd);

      //GET THE BSS AND TLS RELOCATIONS
      preliminaryScanOfSymtab(n_addedfiles,addedfiles,&oldBssSize,&sizeToAddToBss,&tdataSize,&tbssSize,&newGotPltElement);

      // IF NECESSARY WE INCREASE THE SIZE OF THE .GOT.PLT AND .PLT SECTION AND MODIFY/CREATE .RELA.PLT
      // FOR GNU_IFUNC
      if (newGotPltElement > 0)
      {
         elffile_addGotPltEntries (efile, &newGotPltElement);
         pltlen = 0x10 * newGotPltElement; //0x10 = ONE ENTRY SIZE IN THE PLT (TODO:ARCH SPECIFIC!! REPLACE!)
         DBGMSG("orginal RelaPlt size = %#"PRIx64" \n",originalRelaPltSize);
      if (relaPltIdx > 0)
         efile->sections[relaPltIdx]->scnhdr->sh_size += sizeof(Elf64_Rela) * newGotPltElement;
      /**\todo (2016-09-14) Adding this test on relaPltIdx to handle dynamic files compiled with a recent version of gcc,
       * where the info in rela.plt is not the index of the plt, and searchRelaPlt failed (for a dynamic file, the index of
       * JMPREL should have been updated correctly). Check that this does not cause a problem later on and if so, simply return
       * an error if we can't find the rela.plt*/
         if (Elf_Ehdr_get_e_ident (efile, NULL)[EI_CLASS] == ELFCLASS64)
         Elf_Shdr_set_sh_size (efile, relaPltIdx, NULL,
               Elf_Shdr_get_sh_size (efile, relaPltIdx, NULL) + sizeof(Elf64_Rela) * newGotPltElement);
         else if (Elf_Ehdr_get_e_ident (efile, NULL)[EI_CLASS] == ELFCLASS32)
         Elf_Shdr_set_sh_size (efile, relaPltIdx, NULL,
               Elf_Shdr_get_sh_size (efile, relaPltIdx, NULL) + sizeof(Elf32_Rela) * newGotPltElement);
      }
      else
      {
         //NEED TO CREATE A .RELA.PLT SECTION
         //elffile_add_scn (efile,"madras.rela.plt", SHT_RELA,SHF_ALLOC,curraddr,curroff,
         //                   LINK,INFO,ALIGN,0x18*newGotPltElement,NULL(DATA));
      }

      madrasTLSsize = tdataSize + tbssSize;
      if (madrasTLSsize > 0 && efile->indexes[TDATA_IDX] == -1 && efile->indexes[TBSS_IDX] == -1)
      {
         segTLSneeded = 1;
         //Elf64_Dyn newDynEntrie;
         //newDynEntrie.d_tag = DT_FLAGS;
         //newDynEntrie.d_un.d_val = DF_STATIC_TLS;
         //elffile_add_dynentrie(efile,newDynEntrie);
      }

      for (i = 0; i < Elf_Ehdr_get_e_phnum (efile, NULL); i++)
      {
         mapping_scn_seg[i] = malloc ((efile->nsections + 1) * sizeof (int));
         for (j = 0; j < efile->nsections; j++)
         mapping_scn_seg[i][j] = -1;
      }
      i = 0;
      j = 0;

      // ------------------------------------------------------------------
      // if needed, add new section names
      // This avoid to move a section before its size is fixed
      if (codelen > 0)
      elffile_upd_strtab (efile, Elf_Ehdr_get_e_shstrndx (efile, NULL), MADRAS_TEXTSCN_NAME);
      if (datalen > 0)
      elffile_upd_strtab (efile, Elf_Ehdr_get_e_shstrndx (efile, NULL), MADRAS_DATASCN_NAME);
      if (pltlen > 0)
      elffile_upd_strtab (efile, Elf_Ehdr_get_e_shstrndx (efile, NULL), MADRAS_PLTSCN_NAME);

      // ------------------------------------------------------------------
      // Change LOAD segments alignment
      for (i = 0; i < Elf_Ehdr_get_e_phnum (efile, NULL); i++)
      {
         if (Elf_Phdr_get_p_type (efile, i, NULL) == PT_LOAD
               && Elf_Phdr_get_p_align (efile, i, NULL) > NEW_SEGMENT_ALIGNMENT)
         Elf_Phdr_set_p_align (efile, i, NULL, NEW_SEGMENT_ALIGNMENT); //EUGENE
      }

      // ------------------------------------------------------------------
      // Sort sections by addresses
      // This is needed by following algorithms and some binaries does
      // not have their section headers table ordered
      Elf_Shdr** scn_sorted = lc_malloc (sizeof (Elf_Shdr *) * efile->nsections);
      int reordered_scn[efile->nsections];

      // create an array of sorted section headers
      for (i = 0; i < efile->nsections; i++)
      scn_sorted [i] = efile->sections[i]->scnhdr;
      //qsort (scn_sorted, efile->nsections, sizeof (Elf64_Shdr *), &compare_scn);

      //Here an page size is added to the current address.
      //All loaded sections located after this address are in a new segment
      //The loaded loads all physical pages it needs, not only parts of them.
      //For example, if a segment begining at the offset 0x4321 must be loaded
      //at address 0x404321, the loaded will load data from offset 0x4000 (instead
      //of 0x4321) into memory at address 0x404000 (instead of 0x404321). Because of
      //that, it can override bss content and produce bugs during the patched program
      //running.
      curraddr += 0x1000;

      // Save data for the new segment

      // create an array of sorted indexes
      for (i = 0; i < efile->nsections; i++)
      {
         for (j = 0; j < efile->nsections; j++)
         {
            if (scn_sorted[i] == efile->sections[j]->scnhdr)
            {
               reordered_scn[i] = j;
               break;
            }
         }
      }

      // ------------------------------------------------------------------
      // Compute the size of the code to add and move
      for (i = 1; i < efile->nsections - 1; i++)
      {
         //if current section overlap the next one
         //means the section is bigger and must be moved
         if (Elf_Shdr_get_sh_offset (efile, reordered_scn[i], NULL) + Elf_Shdr_get_sh_size (efile, reordered_scn[i], NULL)
               > Elf_Shdr_get_sh_offset (efile, reordered_scn[i] + 1, NULL)
               && Elf_Shdr_get_sh_type (efile, reordered_scn[i], NULL) != SHT_NOBITS)
         {
            // Special case for .got.plt section: we need to move the .got section too
            // .got is immediately before .got.plt
            if ((reordered_scn[i] == efile->indexes[GOTPLT_IDX]) && (altscn[altscnsz-1] != (reordered_scn[i] - 1)))
            {
               altscn[altscnsz++] = reordered_scn[i] - 1;
               length_added_code += Elf_Shdr_get_sh_size (efile, reordered_scn[i] - 1, NULL);
            }
            altscn[altscnsz++] = reordered_scn[i];
            length_added_code += Elf_Shdr_get_sh_size (efile, reordered_scn[i], NULL);
            if ((reordered_scn[i] == efile->indexes[GOT_IDX]))
            {
               altscn[altscnsz++] = reordered_scn[i+1];
               length_added_code += Elf_Shdr_get_sh_size (efile, reordered_scn[i+1], NULL);
               continue;
            }
         }
      }
      altscnsz = 0;

      // ------------------------------------------------------------------
      // Look for a place where to move the code
      // A free space must be located between two free spaces of size 0x1000
      // because the loader loads full pages, so the added segment could
      // override an existing segment and produce crashs
      for (i = 0; i < Elf_Ehdr_get_e_phnum (efile, NULL) - 1; i++)
      {
         if (Elf_Phdr_get_p_type (efile, i, NULL) == PT_LOAD)
         {
            int nb = 1;
            while (i + nb < Elf_Ehdr_get_e_phnum (efile, NULL)
                  && Elf_Phdr_get_p_type (efile, i + nb, NULL) != PT_LOAD)
            nb ++;

            if (i + nb >= Elf_Ehdr_get_e_phnum (efile, NULL)
                  || Elf_Phdr_get_p_type (efile, i + nb, NULL) != PT_LOAD)
            break;
            else
            {
               Elf_Phdr* cur_header = efile->progheader[i];
               Elf_Phdr* next_header = efile->progheader[i + nb];
               if (((int64_t)(Elf_Phdr_get_p_paddr (NULL, 0, next_header)
                                 - Elf_Phdr_get_p_paddr (NULL, 0, cur_header)
                                 - Elf_Phdr_get_p_memsz (NULL, 0, cur_header))) > 0
                     && (Elf_Phdr_get_p_paddr (NULL, 0, next_header)
                           - Elf_Phdr_get_p_paddr (NULL, 0, cur_header)
                           - Elf_Phdr_get_p_memsz (NULL, 0, cur_header) + 0x2000
                           > (uint64_t)(length_added_code + 256 * efile->nsections)))
               {
                  address_added_code = Elf_Phdr_get_p_paddr (NULL, 0, cur_header)
                  + Elf_Phdr_get_p_memsz (NULL, 0, cur_header) + 0x1000;
                  offset_added_code = Elf_Phdr_get_p_offset (NULL, 0, cur_header)
                  + Elf_Phdr_get_p_filesz (NULL, 0, cur_header);
                  break;
               }
            }
         }
      }

      // ALWAYS ACTIVATE THE "ENDING" PATCH PROCESS FOR STATIC PATCH - (WILL CHANGE WITH THE NEW LIBMPATCH)
      if (1)//(address_added_code == -1) || (offset_added_code == -1))
      {
         // In this case, not space is found between sections so add everything at the end
         // of the binary.
         address_added_code = 0;
         offset_added_code = 0;
         for (i = 0; i < Elf_Ehdr_get_e_phnum (efile, NULL); i++)
         {
            Elf_Phdr* cur_header = efile->progheader[i];
            if (Elf_Phdr_get_p_vaddr (NULL, 0, cur_header)
                  + Elf_Phdr_get_p_memsz (NULL, 0, cur_header)
                  + 0x1000 > (uint64_t)address_added_code)
            {
               //0x1000 is a constant empty space added to prevent some crashs due to the
               //loader. Indeed, when a segment is loaded, it is all the page which is loaded
               //and not only the needed part. To empty space is added to prevent the loader
               //to override some data of the previous section (especially if it contains
               //a NO_BITS section such as .bss)
               address_added_code = Elf_Phdr_get_p_vaddr (NULL, 0, cur_header)
               + Elf_Phdr_get_p_memsz (NULL, 0, cur_header) + 0x1000;
               //offset_added_code  = cur_header->p_offset + cur_header->p_filesz;
            }
            //Max Offset and Max Address could coming from different Segments... (NOBITS sections)
            if (Elf_Phdr_get_p_type (NULL, 0, cur_header) == PT_LOAD
                  && Elf_Phdr_get_p_offset (NULL, 0, cur_header)
                  + Elf_Phdr_get_p_filesz (NULL, 0, cur_header) > (uint64_t)offset_added_code)
            {
               offset_added_code = Elf_Phdr_get_p_offset (NULL, 0, cur_header)
               + Elf_Phdr_get_p_filesz (NULL, 0, cur_header);
            }
         }

         //align address on offset
         int align = (offset_added_code % NEW_SEGMENT_ALIGNMENT) - (address_added_code % NEW_SEGMENT_ALIGNMENT);
         if (align < 0)
         align += NEW_SEGMENT_ALIGNMENT;
         address_added_code += align;

         //HLTMSG("Insufficient space in ELF file to insert code (%"PRId64" bytes)\n",length_added_code);
      }

      DBGMSG ("Want to add 0x%"PRIx64" bytes\n",length_added_code );
      DBGMSG ("Offset = 0x%"PRIx64"\tAddress = 0x%"PRIx64"\n",offset_added_code, address_added_code);

      // ------------------------------------------------------------------
      // Look for modified / unmodified sections
      idscn[idscnsz ++] = 0;

      for (i = 1; i < efile->nsections - 1; i++)
      {
         //if current section overlap the next one
         //means the section is bigger and must be moved
         if (Elf_Shdr_get_sh_offset (efile, reordered_scn[i], NULL)
               + Elf_Shdr_get_sh_size (efile, reordered_scn[i], NULL)
               > Elf_Shdr_get_sh_offset (efile, reordered_scn[i] + 1, NULL)
               && Elf_Shdr_get_sh_type (efile, reordered_scn[i], NULL) != SHT_NOBITS)
         {
            if ((reordered_scn[i] == efile->indexes[GOTPLT_IDX]) && (altscn[altscnsz-1] != efile->indexes[GOT_IDX]))
            {
               altscn[altscnsz++] = efile->indexes[GOT_IDX];
               if (idscn[idscnsz - 1] == efile->indexes[GOT_IDX])
               idscnsz--;
               else if(shiftscn[shiftscnsz - 1] == efile->indexes[GOT_IDX])
               shiftscnsz --;

               //length_added_code += efile->sections[reordered_scn[i] - 1]->scnhdr->sh_size ;
            }
            altscn[altscnsz++] = reordered_scn[i];
            if ((reordered_scn[i] == efile->indexes[GOT_IDX]))
            {
               altscn[altscnsz++] = reordered_scn[++i]; //INSERT GOT.PLT IN ALTERED SCN
            }
            //length_added_code += efile->sections[reordered_scn[i]]->scnhdr->sh_size ;
         }
         else if (Elf_Shdr_get_sh_offset (efile, reordered_scn[i], NULL) >= (uint64_t)offset_added_code
               && !(Elf_Shdr_get_sh_type (efile, reordered_scn[i], NULL) == SHT_NOBITS
                     && Elf_Shdr_get_sh_offset (efile, reordered_scn[i], NULL) == (uint64_t)offset_added_code))
         {
            shiftscn[shiftscnsz ++] = reordered_scn[i];
         }
         else
         //means the section can stay on its current position
         {
            idscn[idscnsz++] = reordered_scn[i];
         }
      }
      shiftscn[shiftscnsz ++] = reordered_scn[efile->nsections - 1];

      //-------------------------------------------------------------------
      // Compute mapping between sections and segments
      for (i = 0; i < Elf_Ehdr_get_e_phnum (efile, NULL); i++)
      {
         int nb = 0;
         for (j = 1; j < efile->nsections; j++)
         {
            if (Elf_Shdr_get_sh_offset (efile, j, NULL) >= Elf_Phdr_get_p_offset (efile, i, NULL)
                  && Elf_Shdr_get_sh_offset (efile, j, NULL) < Elf_Phdr_get_p_offset (efile, i, NULL) + Elf_Phdr_get_p_memsz (efile, i, NULL)
                  && Elf_Shdr_get_sh_addr (efile, j, NULL) > 0)
            char modified = 0;
            int k;
            for (k = 0; k < altscnsz; k++)
            {
               if ( altscn[k] == j)
               {
                  modified = 1;
                  break;
               }
            }
            if (modified == 0) {
               if ( ( (Elf_Phdr_get_p_type(efile, i, NULL) == PT_TLS) && elfsection_istls(efile->sections[j]) )
                     || ( (Elf_Phdr_get_p_type(efile, i, NULL) != PT_TLS) && (!elfsection_istls(efile->sections[j]) ) )
                     || ( (Elf_Phdr_get_p_type(efile, i, NULL) == PT_LOAD) && elfsection_istls(efile->sections[j]) ) )
               mapping_scn_seg [i][nb ++] = j; //Mapping a section to a segment only if both or neither are flagged as TLS
            }
         }
      }
   }
   i = 0;
   j = 0;

   // ------------------------------------------------------------------
   // Now reorder sections into permtbl
   // add unchanged sections (the last one will be added after the loop)

   // DBGMSG0("*** Begin to reorder ***\n");
   // for (i = 0; i < idscnsz; i++)
   // {
   //    permtbl [idx++] = idscn[i];
   //    curraddr = efile->sections[idscn[i]]->scnhdr->sh_addr;
   //    curroff  = efile->sections[idscn[i]]->scnhdr->sh_offset;
   //    DBGMSG("At %d, unmodified section %d (addr = 0x%"PRIx64" off = 0x%"PRIx64" size=0x%"PRIx64")\n",
   //          idx - 1, permtbl [idx - 1], curraddr, curroff, efile->sections[idscn[i]]->scnhdr->sh_size);
   // }

   //   curraddr = address_added_code;
   //   curroff = offset_added_code;

   int idxSegmentWhereIsBss = getMappedSegmentIdx(efile,efile->indexes[BSS_IDX],mapping_scn_seg);
   uint64_t madrasTlsAddrInsertion = 0;
   uint64_t madrasTlsOffsetInsertion = 0;
   if (idxSegmentWhereIsBss != -1)
   {
      Elf64_Phdr* segToModify = efile->progheader[idxSegmentWhereIsBss];

      madrasTlsAddrInsertion = segToModify->p_vaddr - madrasTLSsize;
      madrasTlsOffsetInsertion = segToModify->p_offset - tdataSize;
      DBGMSG("TLS OFFSET INSERTION: %#"PRIx64", tdataSize: %#"PRIx64", segment's offset: %#"PRIx64"\n",
            madrasTlsOffsetInsertion, tdataSize, segToModify->p_offset);
   }

   DBGMSG0("*** Begin to reorder ***\n");
   for (i = 0; Elf_Shdr_get_sh_addr (efile, idscn[i], NULL) < madrasTlsAddrInsertion && i < idscnsz; i++)
   {
      permtbl [idx++] = idscn[i];
      curraddr = Elf_Shdr_get_sh_addr (efile, idscn[i], NULL);
      curroff = Elf_Shdr_get_sh_offset (efile, idscn[i], NULL);
      DBGMSG("At %d, unmodified section %d (addr = 0x%"PRIx64" off = 0x%"PRIx64" size=0x%"PRIx64")\n",
            idx - 1, permtbl [idx - 1], curraddr, curroff, Elf_Shdr_get_sh_size (efile, idscn[i], NULL));
   }
   curraddr = address_added_code;
   curroff = offset_added_code;

   //IF NECESSARY WE NEED TO CREATE .TDATA_MADRAS AND .TBSS_MADRAS SECTION
   if (madrasTLSsize > 0)
   {
      uint64_t insertionAddress = madrasTlsAddrInsertion;
      uint64_t insertionOffset = madrasTlsOffsetInsertion+0x1000;
      if (tlsHdr != NULL) // Segment TLS already exists
      {
         tlsAlign = (tlsHdr->p_vaddr - madrasTLSsize + tbssSize) % tlsHdr->p_align;
         if (tlsAlign != 0)
         {
            tdataSize += tlsAlign;
            madrasTLSsize += tlsAlign;
            fprintf(stderr,"tlsAlign = 0x%x\n",tlsAlign);
         }
      }
      else // WE NEED TO CREATE OUR TLS SEGMENT
      {
         tlsAlign= (insertionAddress - tdataSize) % 0x8; //0x8 IN TEST (ARCHI SPECIFIC)
         if (tlsAlign != 0)
         {
            insertionAddress += tlsAlign;
            insertionOffset += tlsAlign;
            curraddr += tlsAlign;
            curroff += tlsAlign;
            //address_added_code += tlsAlign;
            //offset_added_code += tlsAlign;
         }

      }

      DBGMSG(".tdata_madras [size] = %#"PRIx64" and .tbss_madras [size] = %#"PRIx64" @%#"PRIx64" \n",tdataSize,tbssSize,insertionAddress);
      //ADD .tdata_madras section
      elffile_add_scn(efile,".tdata_madras",SHT_PROGBITS,
            SHF_ALLOC|SHF_WRITE|SHF_TLS,
            insertionAddress,insertionOffset,SHN_UNDEF,0,8, tdataSize, NULL);
      // using align 1 to ease alignment congruences (used to be 4, arbitrarily chosen)
      permtbl = (int*)lc_realloc (permtbl, sizeof(int) * (efile->nsections + nb_new_sections));
      *tdataMadrasIdx = efile->nsections-1;
      //The TLS segment doesn't exist so we don't need to handle the adjacent particularity of TLS segment (madras_TLS + TLS).
      //if (tlsHdr == NULL)
      //{
      permtbl[idx++] = efile->nsections - 1;
      curraddr += tdataSize;
      curroff += tdataSize;
      //}

      //curraddr += tdataSize;
      //curroff += tdataSize;
      insertionAddress += tdataSize;
      insertionOffset += tdataSize;

      //ADD .tbss_madras section
      elffile_add_scn(efile,".tbss_madras",SHT_NOBITS,
            SHF_ALLOC|SHF_WRITE|SHF_TLS,
            insertionAddress,insertionOffset,SHN_UNDEF,0,8, tbssSize, NULL);
      // using align 1 to ease alignment congruences (used to be 4, arbitrarily chosen)
      permtbl = (int*)lc_realloc (permtbl, sizeof(int) * (efile->nsections + nb_new_sections));
      *tbssMadrasIdx = efile->nsections-1;
      //The TLS segment doesn't exist so we don't need to handle the adjacent particularity of TLS segment(madras_TLS + TLS).
      //if (tlsHdr == NULL)
      //{
      permtbl[idx++] = efile->nsections - 1;
      curraddr += tbssSize;
      curroff += tbssSize;
      //}
      //curraddr += tbssSize;
      //curroff += tbssSize;
      insertionAddress += tbssSize;
      insertionOffset += tbssSize;

      //FILL IN INFORMATION FOR THE NEW TLS_SEGMENT
      efile->address_newseg_TLS = efile->sections[*tdataMadrasIdx]->scnhdr->sh_addr;
      efile->offset_newseg_TLS = efile->sections[*tdataMadrasIdx]->scnhdr->sh_offset;
      efile->size_newseg_TLS = madrasTLSsize;

      FOREACH_INHASHTABLE(efile->targets, tgt)
      {
         if (((targetaddr_t*)GET_DATA_T(targetaddr_t*, tgt))->scnindx == NEWTDATASCNID)
         ((targetaddr_t*)GET_DATA_T(targetaddr_t*, tgt))->scnindx = *tdataMadrasIdx;
         if (((targetaddr_t*)GET_DATA_T(targetaddr_t*, tgt))->scnindx == NEWTBSSSCNID)
         ((targetaddr_t*)GET_DATA_T(targetaddr_t*, tgt))->scnindx = *tbssMadrasIdx;
      }
   }

   for (; i < idscnsz; i++)
   {
      permtbl [idx++] = idscn[i];
      curraddr = Elf_Shdr_get_sh_addr (efile, idscn[i], NULL);
      Elf_Shdr_set_sh_offset (efile, idscn[i], NULL, Elf_Shdr_get_sh_offset (efile, idscn[i], NULL) + 0x1000);
      DBGMSG("At %d, unmodified section %d (addr = 0x%"PRIx64" off = 0x%"PRIx64" size=0x%"PRIx64")\n",
            idx - 1, permtbl [idx - 1], curraddr, efile->sections[idscn[i]]->scnhdr->sh_offset, efile->sections[idscn[i]]->scnhdr->sh_size);
   }

   offset_added_code+=0x2000;

   curraddr = address_added_code;
   curroff = offset_added_code;

   // add data section
   if (datalen > 0)
   {
      /**\todo This part of the code is a quick and dirty fix to ensure that the added section of
       * code's beginning address will be aligned on 256 bits. Of course, the good way of doing it
       * would be to retrieve the alignment from the variables that have been added (it should be
       * the larger one) instead of hard coding it like that.*/
      int dataalign = 256;
      int alignmod = curraddr % dataalign;
      if ((alignmod) != 0)
      {
         curraddr += (dataalign - alignmod);
         curroff += (dataalign - alignmod);
         length_added_code += (dataalign - alignmod);
         address_added_code += (dataalign-alignmod);
         offset_added_code += (dataalign-alignmod);
      }

      elffile_add_scn (efile,MADRAS_DATASCN_NAME,SHT_PROGBITS,
            SHF_ALLOC|SHF_EXECINSTR|SHF_WRITE,
            curraddr, curroff, SHN_UNDEF, 0, 1, datalen,NULL);
      permtbl[idx++] = efile->nsections - 1;
      if (dataidx != NULL)
      *dataidx = efile->nsections - 1;
      DBGMSG("At %d, DATA section %d (addr = 0x%"PRIx64" off = 0x%"PRIx64" size = 0x%"PRIx64")\n",
            idx - 1, permtbl [idx - 1], curraddr, curroff, datalen);
      FOREACH_INHASHTABLE(efile->targets, tgt)
      {
         if (((targetaddr_t*)GET_DATA_T(targetaddr_t*, tgt))->scnindx == NEWDATASCNID)
         ((targetaddr_t*)GET_DATA_T(targetaddr_t*, tgt))->scnindx = efile->nsections - 1;
      }
      curraddr += datalen;
      curroff += datalen;
   }

   // add plt section
   if (pltlen > 0)
   {
      elffile_add_scn (efile, MADRAS_PLTSCN_NAME, SHT_PROGBITS,
            SHF_ALLOC|SHF_EXECINSTR|SHF_WRITE,
            curraddr, curroff, SHN_UNDEF, 0, 1, pltlen,NULL);
      permtbl[idx++] = efile->nsections - 1;
      if (pltidx != NULL)
      *pltidx = efile->nsections - 1;
      DBGMSG("At %d, PLT section %d (addr = 0x%"PRIx64" off = 0x%"PRIx64" size = 0x%"PRIx64")\n",
            idx - 1, permtbl [idx - 1], curraddr, curroff, pltlen);
      FOREACH_INHASHTABLE(efile->targets, tgt)
      {
         if (((targetaddr_t*)GET_DATA_T(targetaddr_t*, tgt))->scnindx == NEWPLTSCNID)
         ((targetaddr_t*)GET_DATA_T(targetaddr_t*, tgt))->scnindx = efile->nsections - 1;
      }
      curraddr += pltlen;
      curroff += pltlen;
   }

   // add code section
   if (codelen > 0)
   {
      elffile_add_scn (efile,MADRAS_TEXTSCN_NAME,
            SHT_PROGBITS,SHF_ALLOC|SHF_EXECINSTR,
            curraddr, curroff, SHN_UNDEF, 0, 1, codelen,NULL);
      permtbl[idx++] = efile->nsections - 1;
      if (codeidx != NULL)
      *codeidx = efile->nsections - 1;
      DBGMSG("At %d, code section %d (addr = 0x%"PRIx64" off = 0x%"PRIx64" size = 0x%"PRIx64")\n",
            idx - 1, permtbl [idx - 1], curraddr, curroff, codelen);
      curraddr += codelen;
      curroff += codelen;
   }

   // Adds the altered sections
   for (i = 0; i < altscnsz; i++)
   {
      if (Elf_Shdr_get_sh_addralign (efile, altscn[i], NULL) > 1)
      {
         int alignmod = curraddr % Elf_Shdr_get_sh_addralign (efile, altscn[i], NULL);
         if (alignmod)
         {
            curraddr += Elf_Shdr_get_sh_addralign (efile, altscn[i], NULL) - alignmod;
            curroff += Elf_Shdr_get_sh_addralign (efile, altscn[i], NULL) - alignmod;
         }
      }
      permtbl[idx++] = altscn[i];
      Elf_Shdr_set_sh_addr (efile, altscn[i], NULL, curraddr);
      Elf_Shdr_set_sh_offset (efile, altscn[i], NULL, curroff);

      DBGMSG("At %d, modified section %d (addr = 0x%"PRIx64" off = 0x%"PRIx64" size = 0x%"PRIx64")\n"
            , idx - 1, permtbl [idx - 1]
            , Elf_Shdr_get_sh_addr (efile, altscn[i], NULL)
            , Elf_Shdr_get_sh_offset (efile, altscn[i], NULL)
            , Elf_Shdr_get_sh_size (efile, altscn[i], NULL));

      curraddr += Elf_Shdr_get_sh_size (efile, altscn[i], NULL);
      if (Elf_Shdr_get_sh_type (efile, altscn[i], NULL) != SHT_NOBITS)
      curroff += Elf_Shdr_get_sh_size (efile, altscn[i], NULL);
   }

   // Adds the sections from inserted files ------------------------------------
   permtbl = insert_addedfiles(efile, addedfiles, n_addedfiles, permtbl, addedfiles_newaddrs, &idx, &length_added_code,
         &curraddr,&curroff,&gotsize,&gotPltSize,&gotAddr,&sizeToAddToBss,*tdataMadrasIdx,tdataVarsSize,tbssVarsSize,tlsAlign,
         segTLSneeded,*pltidx,stubPltList,&madrasRelaPltEntries,&nbNewGnuIfunc);

   //nbNewGnuIfunc=0;
   if (nbNewGnuIfunc > 0)
   {
      DBGMSG("IRELATIVE RELOCATION(S) ADDED : %d\n",nbNewGnuIfunc);
      //IN TEST
      int relaPltEntriesNb = 0;
      tableentry_t** relaPltEntries = NULL;
      if (relaPltIdx > 0)
      {
         relaPltEntries = efile->sections[relaPltIdx]->table_entries;
         relaPltEntriesNb = efile->sections[relaPltIdx]->nentries;
      }

      int newRelaPltEntriesNb = relaPltEntriesNb + nbNewGnuIfunc;
      tableentry_t** newRelaPltEntries = lc_malloc (sizeof(tableentry_t*) * newRelaPltEntriesNb);
      if (Elf_Ehdr_get_e_ident (efile, NULL)[EI_CLASS] == ELFCLASS64)
      {
         Elf64_Rela* newrela = (Elf64_Rela*)lc_malloc(sizeof(Elf64_Rela)*newRelaPltEntriesNb);
         for(i=0;i<relaPltEntriesNb;i++)
         {
            newRelaPltEntries[i] = relaPltEntries[i];
            newrela[i] = *(relaPltEntries[i]->entry_64->elfentry.rela);
         }

         for(j=0;j<nbNewGnuIfunc;j++,i++)
         {
            newRelaPltEntries[i] = madrasRelaPltEntries[j];
            newrela[i] = *(madrasRelaPltEntries[j]->entry_64->elfentry.rela);
         }

      if (relaPltIdx > 0) {
         efile->sections[relaPltIdx]->table_entries = newRelaPltEntries;
         efile->sections[relaPltIdx]->nentries = newRelaPltEntriesNb;

         //TODO: FREE THE MEMORY (newrela ETC.)
         lc_free(efile->sections[relaPltIdx]->bits);
         efile->sections[relaPltIdx]->bits = (void*) newrela;
      }
      else if (Elf_Ehdr_get_e_ident (efile, NULL)[EI_CLASS] == ELFCLASS32)
      {
         Elf32_Rela* newrela = (Elf32_Rela*)lc_malloc(sizeof(Elf32_Rela)*newRelaPltEntriesNb);
         for(i=0;i<relaPltEntriesNb;i++)
         {
            newRelaPltEntries[i] = relaPltEntries[i];
            newrela[i] = *(relaPltEntries[i]->entry_32->elfentry.rela);
         }

         for(j=0;j<nbNewGnuIfunc;j++,i++)
         {
            newRelaPltEntries[i] = madrasRelaPltEntries[j];
            newrela[i] = *(madrasRelaPltEntries[j]->entry_32->elfentry.rela);
         }
         efile->sections[relaPltIdx]->table_entries = newRelaPltEntries;
         efile->sections[relaPltIdx]->nentries = newRelaPltEntriesNb;

         //TODO: FREE THE MEMORY (newrela ETC.)
         lc_free(efile->sections[relaPltIdx]->bits);
         efile->sections[relaPltIdx]->bits = (void*)newrela;
      }
      //efile->sections[relaPltIdx]->scnhdr->sh_size = originalRelaPltSize + (sizeof(Elf64_Rela) * nbNewGnuIfunc);
   }

   //Resize the .rela.plt section : We know the real size only now...
   //Indeed, we could have extend the .rela.plt with false positives.
   //If one GNU_IFUNC is already present in the .rela.plt we only know at this time
   //but we have already extend the .rela.plt section.
   if (relaPltIdx > 0)
   {
      if (Elf_Ehdr_get_e_ident (efile, NULL)[EI_CLASS] == ELFCLASS64)
      Elf_Shdr_set_sh_size (efile, relaPltIdx, NULL, originalRelaPltSize + (sizeof(Elf64_Rela) * nbNewGnuIfunc));
      else if (Elf_Ehdr_get_e_ident (efile, NULL)[EI_CLASS] == ELFCLASS32)
      Elf_Shdr_set_sh_size (efile, relaPltIdx, NULL, originalRelaPltSize + (sizeof(Elf32_Rela) * nbNewGnuIfunc));
   }
   modifyLibcCsuIrel(efile,afile,originalRelaPltAddr,originalRelaPltSize);

   //MERGE THE MADRAS.PLT (when patching a dynamic binary with the static patcher)
   if (pltlen > 0 && n_addedfiles > 0)
   {
      if (*pltList == NULL)
      *pltList = queue_new();
      queue_append(*pltList,stubPltList);
   } else
      queue_free(stubPltList, NULL);   /**\note (2018-09-12) There was no else but a free of stubPltList there...*/

   length_added_code = curraddr - address_added_code;
   //add shifted version
   for (i = 0; i < shiftscnsz; i++)
   {
      //if (efile->sections[shiftscn[i]]->scnhdr->sh_type != SHT_NOBITS)
      //{
      // handle alignment between offset and address

      if ((Elf_Shdr_get_sh_addr (efile, shiftscn[i], NULL) % NEW_SEGMENT_ALIGNMENT) != (curroff % NEW_SEGMENT_ALIGNMENT)
            && Elf_Shdr_get_sh_addr (efile, shiftscn[i], NULL) != 0)
      {
         int align = (Elf_Shdr_get_sh_addr (efile, shiftscn[i], NULL) % NEW_SEGMENT_ALIGNMENT) -
         curroff % NEW_SEGMENT_ALIGNMENT;
         if (align < 0)
         align += NEW_SEGMENT_ALIGNMENT;
         curroff += align;
      }
      // }
      permtbl[idx++] = shiftscn[i];
      Elf_Shdr_set_sh_offset (efile, shiftscn[i], NULL, curroff);
      DBGMSG("At %d, shifted section %d (addr = 0x%"PRIx64" off = 0x%"PRIx64" size = 0x%"PRIx64")\n",
            idx - 1, permtbl [idx - 1]
            , Elf_Shdr_get_sh_addr (efile, shiftscn[i], NULL)
            , Elf_Shdr_get_sh_offset (efile, shiftscn[i], NULL)
            , Elf_Shdr_get_sh_size (efile, shiftscn[i], NULL));

      if (Elf_Shdr_get_sh_type (efile, shiftscn[i], NULL) != SHT_NOBITS)
      curroff += Elf_Shdr_get_sh_size (efile, shiftscn[i], NULL);
   }

   //-------------------------------------------------------------------
   // Update the added label
   // it was added with bad values for st_shndx and st_value because they
   // are not known at this time but it is now so the entry can be updated
   if (lblidx != -1)
   {
      int scnidx = -1;

      if (codelen > 0)
      scnidx = *codeidx;
      else if (pltlen > 0)
      scnidx = *pltidx;

      if (scnidx != -1)
      {
         int symidx = efile->indexes[SYMTAB_IDX];
         elfsection_t* symscn = efile->sections[symidx];
         if (symscn->table_entries[lblidx]->entry_64 != NULL)
         {
            symscn->table_entries[lblidx]->entry_64->elfentry.sym->st_shndx = scnidx;
            symscn->table_entries[lblidx]->entry_64->elfentry.sym->st_value = elffile_getscnstartaddr(efile, scnidx);
            Elf64_Sym newsym = *symscn->table_entries[lblidx]->entry_64->elfentry.sym;
            memcpy (&symscn->bits[lblidx * sizeof (newsym)], &newsym, sizeof (newsym));
         }
         else if (symscn->table_entries[lblidx]->entry_32 != NULL)
         {
            symscn->table_entries[lblidx]->entry_32->elfentry.sym->st_shndx = scnidx;
            symscn->table_entries[lblidx]->entry_32->elfentry.sym->st_value = elffile_getscnstartaddr(efile, scnidx);
            Elf32_Sym newsym = *symscn->table_entries[lblidx]->entry_32->elfentry.sym;
            memcpy (&symscn->bits[lblidx * sizeof (newsym)], &newsym, sizeof (newsym));
         }
      }
   }
   DBGMSG0("New mapping of segments to sections\n");
   DBG(int __i; int __j;
         for (__i = 0; __i < Elf_Ehdr_get_e_phnum(efile, NULL); __i++) {
            fprintf(stderr, "\tSegment %d:", __i);
            for (__j = 0; __j < efile->nsections; __j++)
            fprintf(stderr, "%d ", mapping_scn_seg[__i][__j]);
            fprintf(stderr, "\n");
         }
   )
   //-------------------------------------------------------------------
   //update segments
   for (i = 0; i < Elf_Ehdr_get_e_phnum (efile, NULL); i++)
   {
      if (Elf_Phdr_get_p_type (efile, i, NULL) == PT_DYNAMIC)
      {  // update dynamic segment
         int j = 0;
         for (j = 0; j < Elf_Ehdr_get_e_shnum (efile, NULL); j++)
         {
            if (Elf_Shdr_get_sh_type (efile, j, NULL) == SHT_DYNAMIC)
            {
               Elf_Phdr_set_p_offset (efile, i, NULL, Elf_Shdr_get_sh_offset (efile, j, NULL));
               Elf_Phdr_set_p_vaddr (efile, i, NULL, Elf_Shdr_get_sh_addr (efile, j, NULL));
               Elf_Phdr_set_p_paddr (efile, i, NULL, Elf_Shdr_get_sh_addr (efile, j, NULL));
               Elf_Phdr_set_p_memsz (efile, i, NULL, Elf_Shdr_get_sh_size (efile, j, NULL));
               Elf_Phdr_set_p_filesz (efile, i, NULL, Elf_Shdr_get_sh_size (efile, j, NULL));
               DBGMSGLVL(1, "Segment %d: Offset %#"PRIx64" - Address %#"PRIx64" - Size %#"PRIx64" (dynamic segment)\n",
                     i, Elf_Phdr_get_p_offset(efile, i, NULL), Elf_Phdr_get_p_paddr(efile, i, NULL), Elf_Phdr_get_p_memsz(efile, i, NULL));
               break;
            }
         }
      }
      else
      {  // update other ones
         if (mapping_scn_seg[i][0] != -1)
         {
            Elf_Phdr_set_p_offset (efile, i, NULL, Elf_Shdr_get_sh_offset (efile, mapping_scn_seg[i][0], NULL));
            Elf_Phdr_set_p_vaddr (efile, i, NULL, Elf_Shdr_get_sh_addr (efile, mapping_scn_seg[i][0], NULL));
            Elf_Phdr_set_p_paddr (efile, i, NULL, Elf_Shdr_get_sh_addr (efile, mapping_scn_seg[i][0], NULL));
            int idx = 0;
            uint64_t endAddrMax = 0, endAddrSection = 0, startAddrMin = 0, startAddrSection = 0,
            endOffMax = 0, endOffSection = 0, startOffMin = 0, startOffSection = 0;
            //Search the section in the current segment which have the biggest ending address
            // while (mapping_scn_seg[i][idx] != -1)
            //    {
            //    if(!elfsection_istls(efile->sections[mapping_scn_seg[i][idx]])) //NOT FOR TLS SECTION
            //    {
            //       startAddrSection = efile->sections[mapping_scn_seg[i][idx]]->scnhdr->sh_addr;
            //       endAddrSection = startAddrSection + efile->sections[mapping_scn_seg[i][idx]]->scnhdr->sh_size;
            //             if (startAddrSection < startAddrMin || startAddrMin == 0)
            //                startAddrMin = startAddrSection;
            //             if (endAddrSection > endAddrMax)
            //                endAddrMax = endAddrSection;
            //    }
            //          idx ++;
            //    }

            idx=0;
            //TODO : CODE REFACTORING!!!

            idx = 0;
            //efile->progheader [i]->p_memsz  = 0;
            while (mapping_scn_seg[i][idx] != -1)
            {
               if (Elf_Phdr_get_p_type (efile, i, NULL) == PT_TLS)
               {
                  if(elfsection_istls(efile->sections[mapping_scn_seg[i][idx]]))
                  {
                     startAddrSection = Elf_Shdr_get_sh_addr (efile, mapping_scn_seg[i][idx], NULL);
                     startOffSection = Elf_Shdr_get_sh_offset (efile, mapping_scn_seg[i][idx], NULL);
                     endAddrSection = startAddrSection + Elf_Shdr_get_sh_size (efile, mapping_scn_seg[i][idx], NULL);
                     endOffSection = startOffSection + Elf_Shdr_get_sh_size (efile, mapping_scn_seg[i][idx], NULL);
                     if (startAddrSection < startAddrMin || startAddrMin == 0)
                     startAddrMin = startAddrSection;
                     if ((startOffSection < startOffMin || startOffMin == 0) && efile->sections[mapping_scn_seg[i][idx]]->scnhdr->sh_type != SHT_NOBITS)
                     startOffMin = startOffSection;
                     if (endAddrSection > endAddrMax)
                     endAddrMax = endAddrSection;
                     if (endOffSection > endOffMax && Elf_Shdr_get_sh_type (efile, mapping_scn_seg[i][idx], NULL) != SHT_NOBITS)
                     endOffMax = endOffSection;

                     DBGMSGLVL(1, "Segment %d: Processing section %d. Size is now %#"PRIx64", Memory size is now %#"PRIx64"\n",
                           i, mapping_scn_seg[i][idx], endOffMax-startOffMin, endAddrMax-startAddrMin);
                  }
               }
               else
               {
                  startAddrSection = Elf_Shdr_get_sh_addr (efile, mapping_scn_seg[i][idx], NULL);
                  startOffSection = Elf_Shdr_get_sh_offset (efile, mapping_scn_seg[i][idx], NULL);
                  endAddrSection = startAddrSection + Elf_Shdr_get_sh_size (efile, mapping_scn_seg[i][idx], NULL);
                  endOffSection = startOffSection + Elf_Shdr_get_sh_size (efile, mapping_scn_seg[i][idx], NULL);
                  if (startAddrSection < startAddrMin || startAddrMin == 0)
                  startAddrMin = startAddrSection;
                  if (startOffSection < startOffMin || startOffMin == 0)
                  startOffMin = startOffSection;
                  if (endAddrSection > endAddrMax)
                  endAddrMax = endAddrSection;
                  if (endOffSection > endOffMax && Elf_Shdr_get_sh_type (efile, mapping_scn_seg[i][idx], NULL) != SHT_NOBITS)
                  endOffMax = endOffSection;

                  DBGMSGLVL(1, "Segment %d: Processing section %d. Size is now %#"PRIx64", Memory size is now %#"PRIx64"\n",
                        i, mapping_scn_seg[i][idx], endOffMax-startOffMin, endAddrMax-startAddrMin);
                  >>>>>>> arm
               }
               idx++;
            }

            Elf_Phdr_set_p_memsz (efile, i, NULL, endAddrMax-startAddrMin);
            Elf_Phdr_set_p_filesz (efile, i, NULL, endOffMax-startOffMin);
            //efile->progheader [i]->p_memsz  = nobits_size + new_size;
         }
         DBGMSGLVL(1, "Segment %d: Offset %#"PRIx64" - Address %#"PRIx64" - Size %#"PRIx64"\n",
               i, Elf_Phdr_get_p_offset(efile, i, NULL), Elf_Phdr_get_p_paddr(efile, i, NULL), Elf_Phdr_get_p_memsz(efile, i, NULL));
      }
   }

   //update elf header
   for (i = 0; i < efile->nsections; i++)
   {
      if (Elf_Ehdr_get_e_shstrndx (efile, NULL) == permtbl[i])
      {
         Elf_Ehdr_set_e_shstrndx (efile, NULL, i);
         break;
      }
   }
   Elf_Ehdr_set_e_shoff (efile, NULL, curroff);
   Elf_Ehdr_set_e_shnum (efile, NULL, efile->nsections);
   lc_free (scn_sorted);

   //-------------------------------------------------------------------
   // finally, add the new segment.
   // For the moment, it is assumed that this segment is not the first LOAD segment
   // because of the algorithm used to select the added code location.
   // When this assumption will be false, a piece of code will be added to
   // handle the following requirement:
   //     the PHDR segment must be contained in the first LOAD segment
   //*
   int64_t align = NEW_SEGMENT_ALIGNMENT;
   int nbNewSegment = 0;
   Elf_Phdr* segTLS = NULL;
   // Update offset for each section / segment
   for (i = 0; i < efile->elfheader->e_phnum; i++) {
      Elf_Phdr_set_p_offset (efile, i, NULL, Elf_Phdr_get_p_offset (efile, i, NULL) + align);
   }

   for (i = 1; i < efile->nsections; i++)
   {
      Elf_Shdr_set_sh_offset (efile, i, NULL, Elf_Shdr_get_sh_offset (efile, i, NULL) + align);
   }
   Elf_Ehdr_set_e_phoff (efile, NULL, Elf_Ehdr_get_e_phoff (efile, NULL) + align);
   Elf_Ehdr_set_e_shoff (efile, NULL, Elf_Ehdr_get_e_shoff (efile, NULL) + align);

   // Create and add a LOAD segment in the program header
   Elf_Phdr* seg = lc_malloc (sizeof (Elf_Phdr));
   if (Elf_Ehdr_get_e_ident (efile, NULL)[EI_CLASS] == ELFCLASS64)
   {
      seg->phdr64 = lc_malloc (sizeof (Elf64_Phdr));
      seg->phdr32 = NULL;
   }
   else if (Elf_Ehdr_get_e_ident (efile, NULL)[EI_CLASS] == ELFCLASS32)
   {
      seg->phdr32 = lc_malloc (sizeof (Elf32_Phdr));
      seg->phdr64 = NULL;
   }

   Elf_Phdr_set_p_type (NULL, 0, seg, PT_LOAD);
   Elf_Phdr_set_p_flags (NULL, 0, seg, PF_X | PF_R | PF_W);
   Elf_Phdr_set_p_offset (NULL, 0, seg, offset_added_code + align);
   Elf_Phdr_set_p_vaddr (NULL, 0, seg, address_added_code);
   Elf_Phdr_set_p_paddr (NULL, 0, seg, address_added_code);
   Elf_Phdr_set_p_filesz (NULL, 0, seg, length_added_code-sizeToAddToBss);
   Elf_Phdr_set_p_memsz (NULL, 0, seg, length_added_code);
   Elf_Phdr_set_p_align (NULL, 0, seg, align);
   nbNewSegment++;

   // Create and add a TLS segment in the program header
   if (segTLSneeded == 1)
   {

      //STDMSG("CREATE TLS_MADRAS SECTION\n");
      int idxSegmentWhereIsBss = getMappedSegmentIdx(efile,efile->indexes[BSS_IDX],mapping_scn_seg);
      Elf_Phdr* segToModify = efile->progheader[idxSegmentWhereIsBss];
      efile->sections[*tdataMadrasIdx]->scnhdr->sh_addr = segToModify->p_vaddr - tdataSize;
      efile->sections[*tbssMadrasIdx]->scnhdr->sh_addr = segToModify->p_vaddr;
      efile->sections[*tdataMadrasIdx]->scnhdr->sh_offset = segToModify->p_offset - tdataSize;
      efile->sections[*tbssMadrasIdx]->scnhdr->sh_offset = segToModify->p_offset;

      segTLS = lc_malloc (sizeof (Elf_Phdr));
      if (Elf_Ehdr_get_e_ident (efile, NULL)[EI_CLASS] == ELFCLASS64)
      {
         segTLS->phdr64 = lc_malloc (sizeof (Elf64_Phdr));
         segTLS->phdr32 = NULL;
      }
      else if (Elf_Ehdr_get_e_ident (efile, NULL)[EI_CLASS] == ELFCLASS32)
      {
         segTLS->phdr32 = lc_malloc (sizeof (Elf32_Phdr));
         segTLS->phdr64 = NULL;
      }

      Elf_Phdr_set_p_type (NULL, 0, segTLS, PT_TLS);
      Elf_Phdr_set_p_flags (NULL, 0, segTLS, PF_R);
      Elf_Phdr_set_p_offset (NULL, 0, segTLS, Elf_Phdr_get_p_offset (NULL, 0, segToModify) - madrasTLSsize);
      segTLS->p_vaddr = segToModify->p_vaddr - tdataSize;
      segTLS->p_paddr = segToModify->p_vaddr - tdataSize;
      Elf_Phdr_set_p_filesz (NULL, 0, segTLS, madrasTLSsize - tbssSize);
      Elf_Phdr_set_p_memsz (NULL, 0, segTLS, madrasTLSsize);
      segTLS->p_offset = segToModify->p_offset - tdataSize;
      segTLS->p_align = 0x4;

      segToModify->p_vaddr -= tdataSize;
      segToModify->p_paddr -= tdataSize;
      segToModify->p_offset -= tdataSize;
      segToModify->p_filesz += tdataSize;
      segToModify->p_memsz += madrasTLSsize;
      nbNewSegment++;

      //    segTLS = lc_malloc (sizeof (Elf64_Phdr));
      //    segTLS->p_type   = PT_TLS;
      //    segTLS->p_flags  = PF_R;
      //    segTLS->p_vaddr  = efile->address_newseg_TLS;
      //    segTLS->p_paddr  = efile->address_newseg_TLS;
      //    segTLS->p_filesz = efile->size_newseg_TLS - efile->sections[tbssMadrasIdx]->scnhdr->sh_size;
      //    segTLS->p_memsz  = efile->size_newseg_TLS;
      //    segTLS->p_offset = efile->offset_newseg_TLS+ align;
      //    segTLS->p_align  = 0x8;
      //    nbNewSegment++;
   }

   for (i = 0; i < Elf_Ehdr_get_e_phnum (efile, NULL); i++)
   lc_free (mapping_scn_seg[i]);
   lc_free (mapping_scn_seg);mapping_scn_seg=NULL;
   efile->progheader = lc_realloc (efile->progheader, (Elf_Ehdr_get_e_phnum (efile, NULL) + nbNewSegment) * sizeof(Elf_Phdr*));

   // locate the first LOAD segment whose address is bigger than the address of the
   // added code
   int new_phdr_idx = -1;
   for (i = 0; i < Elf_Ehdr_get_e_phnum (efile, NULL); i++)
   if (Elf_Phdr_get_p_type (efile, i, NULL) == PT_LOAD
         && Elf_Phdr_get_p_paddr (efile, i, NULL) > (uint64_t)address_added_code)
   new_phdr_idx = i;
   if (new_phdr_idx > -1)
   {
      for (i = Elf_Ehdr_get_e_phnum (efile, NULL) + nbNewSegment-1; i >= new_phdr_idx; i--)
      efile->progheader[i] = efile->progheader[i - nbNewSegment];
   }
   else
   new_phdr_idx = Elf_Ehdr_get_e_phnum (efile, NULL);

   DBGMSG ("New LOAD segment added at index %d\n",new_phdr_idx)

   if (segTLSneeded == 1)
   {
      efile->progheader[new_phdr_idx] = segTLS;
      new_phdr_idx++;
   }
   efile->progheader[new_phdr_idx] = seg;
   Elf_Ehdr_set_e_phnum (efile, NULL, Elf_Ehdr_get_e_phnum (efile, NULL) + nbNewSegment);
   int phdr_idx = -1;
   int load_idx = -1;

   //move the PHDR segment
   for (i = 0; i < Elf_Ehdr_get_e_phnum (efile, NULL); i++)
   {
      if (Elf_Phdr_get_p_type (efile, i, NULL) == PT_PHDR)
      {
         Elf_Phdr_set_p_offset (efile, i, NULL, Elf_Phdr_get_p_offset (efile, i, NULL) - nbNewSegment * Elf_Ehdr_get_e_phentsize(efile, NULL));
         Elf_Phdr_set_p_paddr (efile, i, NULL, Elf_Phdr_get_p_paddr (efile, i, NULL) - nbNewSegment * Elf_Ehdr_get_e_phentsize(efile, NULL));
         Elf_Phdr_set_p_vaddr (efile, i, NULL, Elf_Phdr_get_p_vaddr (efile, i, NULL) - nbNewSegment * Elf_Ehdr_get_e_phentsize(efile, NULL));
         Elf_Phdr_set_p_filesz (efile, i, NULL, Elf_Phdr_get_p_filesz (efile, i, NULL) + nbNewSegment * Elf_Ehdr_get_e_phentsize(efile, NULL));
         Elf_Phdr_set_p_memsz (efile, i, NULL, Elf_Phdr_get_p_memsz (efile, i, NULL) + nbNewSegment * Elf_Ehdr_get_e_phentsize(efile, NULL));
         phdr_idx = i;
         break;
      }
   }

   //move the first LOAD segment
   uint64_t phdr_address = -1, firstloadseg_address = UINT64_MAX;
   int firstloadseg_index = -1;
   for (i = 0; i < efile->elfheader->e_phnum; i++)
   {
      if (efile->progheader[i]->p_type == PT_PHDR)
      phdr_address = efile->progheader[i]->p_vaddr;
      if (efile->progheader[i]->p_type == PT_LOAD && efile->progheader[i]->p_vaddr < firstloadseg_address) {
         firstloadseg_address = efile->progheader[i]->p_vaddr;
         firstloadseg_index = i;
      }
   }

   DBGMSG("phdr_addr: %#"PRIx64", load addr: %#"PRIx64", load index: %d\n", phdr_address, firstloadseg_address, firstloadseg_index);
   //Updating first LOAD segment to include the PHDR segment
   if (firstloadseg_index > 0 && phdr_address > 0) {
      uint64_t shift = efile->progheader[firstloadseg_index]->p_vaddr - phdr_address;
      efile->progheader[firstloadseg_index]->p_memsz += shift;
      efile->progheader[firstloadseg_index]->p_filesz += shift;
      efile->progheader[firstloadseg_index]->p_offset -= shift;
      efile->progheader[firstloadseg_index]->p_vaddr -= shift;
      efile->progheader[firstloadseg_index]->p_paddr -= shift;
   }

   //TODO: load_idx and phdr_idx not used
   (void)load_idx;
   (void)phdr_idx;

   //TODO: CHECK IF WE HAVE ENOUGH SIZE TO DO THIS (OTHERWISE TRY TO MOVE THE TLS SEGMENT(.TDATA AND .TBSS) AT THE END!
   //Add the both sections of our madras TLS segment in the second LOAD segment AND Resize the TLS segment
   if (madrasTLSsize >0 )//&& tlsHdr != NULL)
   {
      if(tlsHdr !=NULL)
      {
         for (i = 0; i < Elf_Ehdr_get_e_phnum (efile, NULL); i++)
         {
            //move the LOAD segment that contains .tdata AND/OR .tbss
            if (Elf_Phdr_get_p_type (efile, i, NULL) == PT_LOAD
                  && Elf_Phdr_get_p_vaddr (efile, i, NULL) == Elf_Phdr_get_p_vaddr (NULL, 0, tlsHdr))
            {
               Elf_Phdr_set_p_offset (efile, i, NULL, Elf_Phdr_get_p_offset (efile, i, NULL) - madrasTLSsize);
               Elf_Phdr_set_p_paddr (efile, i, NULL, Elf_Phdr_get_p_paddr (efile, i, NULL) - madrasTLSsize);
               Elf_Phdr_set_p_vaddr (efile, i, NULL, Elf_Phdr_get_p_vaddr (efile, i, NULL) - madrasTLSsize);
               Elf_Phdr_set_p_filesz (efile, i, NULL, Elf_Phdr_get_p_filesz (efile, i, NULL) + madrasTLSsize);
               Elf_Phdr_set_p_memsz (efile, i, NULL, Elf_Phdr_get_p_memsz (efile, i, NULL) + madrasTLSsize);
               break;
            }
         }

         //TODO: CHECK IF WE HAVE ENOUGH SIZE TO DO THIS (OTHERWISE TRY TO MOVE THE TLS SEGMENT(.TDATA AND .TBSS) AT THE END!
         //We append our madras TLS segment at the front of the original TLS segment
         efile->sections[*tdataMadrasIdx]->scnhdr->sh_addr = tlsHdr->p_vaddr - madrasTLSsize;
         efile->sections[*tbssMadrasIdx]->scnhdr->sh_addr = tlsHdr->p_vaddr - tbssSize;
         efile->sections[*tdataMadrasIdx]->scnhdr->sh_offset = tlsHdr->p_offset - madrasTLSsize;
         efile->sections[*tbssMadrasIdx]->scnhdr->sh_offset = tlsHdr->p_offset - tbssSize;
         //EDIT THE EXISTING TLS SEGMENT (EXTEND WITH THE .MADRAS_TDATA AND .MADRAS_TBSS)
         Elf_Phdr_set_p_offset (NULL, 0, tlsHdr, Elf_Phdr_get_p_offset (NULL, 0, tlsHdr) - madrasTLSsize);
         Elf_Phdr_set_p_paddr (NULL, 0, tlsHdr, Elf_Phdr_get_p_paddr (NULL, 0, tlsHdr) - madrasTLSsize);
         Elf_Phdr_set_p_vaddr (NULL, 0, tlsHdr, Elf_Phdr_get_p_vaddr (NULL, 0, tlsHdr) - madrasTLSsize);
         Elf_Phdr_set_p_filesz (NULL, 0, tlsHdr, Elf_Phdr_get_p_filesz (NULL, 0, tlsHdr) + madrasTLSsize);
         Elf_Phdr_set_p_memsz (NULL, 0, tlsHdr, Elf_Phdr_get_p_memsz (NULL, 0, tlsHdr) + madrasTLSsize);
      }
   }
   Elf_Ehdr_set_e_phoff (efile, NULL, Elf_Ehdr_get_e_phoff (efile, NULL) - nbNewSegment * Elf_Ehdr_get_e_phentsize (efile, NULL));
   //*/

   return (permtbl);
}

/*
 * Converts a bitfield stream into its integer representation
 * \param mem A bitfield stream
 * \param size Size of the bitfield (size of the array)
 * \return The value of the bitfield
 */
static int64_t convert_bits_into_address (unsigned char* mem, int size)
{
   int64_t res = 0;
   int i = 0;

   if (mem != NULL)
   {
      for (i = size - 1; i >= 0; i--)
      res = (res << 8) + mem[i];
   }
   return (res);
}

/*
 * Gets the index of the first not null label in .rela.plt table
 * \param table Hashtable containing the .rela.plt entries
 * \param size Size of .rela.plt table
 * \param start_idx First index to use to look into the table (must be -1 at the first call,
 * then the value returned by this function)
 * \param add Base address of the table
 * \param align Size of an element
 * \return the index of the first found label, else -1
 */
static int get_next_label (hashtable_t* table, int size, int start_idx, int64_t add, int align)
{
   //start_idx is increased before each call in order to allow successive calls of the same function
   //without creating infinite loops, that is why the first value of start_idx must be -1.
   start_idx ++;
   while (start_idx < size)
   {
      if (hashtable_lookup(table, (void*)(add + start_idx * align)) != NULL)
      return (start_idx);
      start_idx ++;
   }
   return (-1);
}

/*
 * Generates label name using .rela.plt table
 * \param table Hashtable containing the .rela.plt table
 * \param addr Address in the table
 * \return A string "<label><EXT_LBL_SUF>"
 */
static char* generate_label_name (hashtable_t* table, int64_t addr)
{
   tableentry_t* te = hashtable_lookup(table, (void*)addr);
   char* tmp = NULL;

   if (te->entry_64 != NULL)
   tmp = te->entry_64->name;
   else if (te->entry_32 != NULL)
   tmp = te->entry_32->name;
   if (tmp == NULL)
   tmp = "<unknow>";
   char* lblname = str_new (strlen (tmp) + 5);
   sprintf(lblname, "%s" EXT_LBL_SUF, tmp);

   return (lblname);
}

/*
 * Update labels of .plt section using .rela.plt table and .got section
 * \param An asmfile to update
 * \return EXIT_SUCCESS or error code
 */
int asmfile_add_plt_labels (asmfile_t* asmf)
{
   if (asmf == NULL)
   return ERR_LIBASM_MISSING_ASMFILE;
   if (asmfile_test_analyze(asmf, PAR_ANALYZE) == TRUE)
   return ERR_BINARY_FILE_ALREADY_PARSED; //We don't want to execute this twice, so the ELF file must already have been parsed but the asmfile not completely updated yet
   binfile_t* bf = asmfile_get_binfile(asmf);//elf file structure
   elffile_t* efile = bf->driver->parsedbin;

   if (bf == NULL)
   return ERR_BINARY_MISSING_BINFILE;

   // Inserting a label at the beginning of the .plt section to avoid having instructions with no label
   // This is independent of the rest, but since it consists in adding labels to the .plt we add it here
   /**\todo Either get rid of this or make is systematically for every start of section*/
   if ( efile->indexes[PLT_IDX] >= 0 ) {
      label_t* pltlbl = label_new (FIRST_PLT_INSN_LBL, Elf_Shdr_get_sh_addr (efile->elf, efile->indexes[PLT_IDX]), 0, NULL);
      label_set_type(pltlbl, LBL_GENERIC);
      asmfile_add_label_unsorted (asmf, pltlbl);
      queue_add_tail(bf->unlinked_lbls, pltlbl); //Stores the label as having no target
   }

   if (efile->indexes[PLTGOT] < 0
         || efile->indexes[JMPREL] < 0
         || efile->indexes[PLTGOT] == efile->indexes[GOT_IDX])
   return ERR_BINARY_NO_EXTFCTS_SECTION;
   int64_t pltgot_add = Elf_Shdr_get_sh_addr (efile->elf, efile->indexes[PLTGOT]); //address of .plt.got section
   int64_t pltgot_size = Elf_Shdr_get_sh_size (efile->elf, efile->indexes[PLTGOT]);//size (in bytes) of .plt.got section
   int pltgot_align = Elf_Shdr_get_sh_addralign (efile->elf, efile->indexes[PLTGOT]);//size of an element of .plt.got section
   int pltgot_nelem = pltgot_size / pltgot_align;//number of elements of .plt.got section
   unsigned char* mem_bits = NULL;//bitfield used to store bytes coming from memory

   hashtable_t* jmprel_table = efile->sections[efile->indexes[JMPREL]]->hentries;//relocation table coming from .rela.plt section
   int64_t addr;//used to store addresses
   char* lblname;//string used to create label names
   label_t* lab1 = NULL, *lab2 = NULL;//labels
   int idx = -1;//index in .plt.got section
   insn_t* jmp2got, *retfromgot;//Instructions in a plt stub referencing and referenced by the .got.plt

   // Brief Summary: ----------------------------------------------------------
   // relocation table (jmprel_table) contains labels of dynamically loaded functions and their offsets
   // when they are loaded in memory. The table is saved into .rela.plt section.
   // For each label of the relocation table, there is a return address in .plt.got section (in the same order,
   // that means the first element of .got.plt corresponding to the first label in .rela.plt).
   // This function gets, for each label, the return address and the label, then modifies instructions labels
   // to add these "new" labels.

   // Update:
   // To allow retrieving the labels in a file that was not disassembled, we will use the architecture specific functions
   // for retrieving the structure of a plt entry, and deduce the distance between the beginning of a stub and the entry
   // pointed to by the .got entry

   // Retrieving a list of instructions composing a plt entry from the architecture specific functions
   asmbldriver_t* driver = asmbldriver_load(asmfile_get_arch(asmf));
   if (driver == NULL) {
      DBGMSG("[INTERNAL] Unable to retrieve plt stub for architecture %s: no plt labels will be added\n", asmfile_get_arch_name(asmf));
      return ERR_PATCH_ARCH_NOT_SUPPORTED;
      ;
   }
   queue_t* pltstub = driver->generate_insnlist_pltcall(0, NULL, &jmp2got, &retfromgot);
   if (pltstub == NULL) {
      DBGMSG("[INTERNAL] Unable to retrieve plt stub for architecture %s: no plt labels will be added\n", asmfile_get_arch_name(asmf));
      asmbldriver_free(driver);
      return ERR_PATCH_EXTFCT_STUB_NOT_GENERATED;
   }
   // Now we calculate the distance between the beginning of the stub and the instruction referenced in the .plt.got
   list_t* iterstub = queue_iterator(pltstub);
   int64_t distfromgot = 0;
   while (GET_DATA_T(insn_t*, iterstub) != retfromgot) {
      distfromgot += insn_get_size(GET_DATA_T(insn_t*, iterstub)) / 8;
      iterstub = iterstub->next;
   }
   DBGMSG("Distance between beginning of plt stub and .got.plt reference is %"PRId64" bytes\n", distfromgot);
   //Freeing the list of instruction and the driver, we don't need them now
   queue_free(pltstub, asmf->arch->insn_free);
   asmbldriver_free(driver);

   // Initialisation step -----------------------------------------------------
   // Gets the first label index (return if not exist)
   idx = get_next_label (jmprel_table, pltgot_nelem, -1, pltgot_add, pltgot_align);
   if (idx == -1)
   return EXIT_SUCCESS;
   // then gets the corresponding return address
   mem_bits = elfsection_get_databytes (efile, efile->indexes[PLTGOT], idx * pltgot_align, pltgot_align);
   addr = convert_bits_into_address (mem_bits, pltgot_align);
   lc_free (mem_bits);

   // Checks if the address is in plt section or madras.plt section. If not, return
   int target_scn = elffile_get_scnspanaddr(efile, addr);
   if (target_scn != efile->indexes[PLT_IDX]
         && target_scn != efile->indexes[MADRASPLT_IDX])
   return EXIT_SUCCESS;

   // then the label is created and assigned to the first instruction of the block
   lblname = generate_label_name (jmprel_table, pltgot_add + idx * pltgot_align);
   lab1 = label_new (lblname, addr - distfromgot, TARGET_INSN, NULL);
   label_set_type(lab1, LBL_EXTFUNCTION);
   asmfile_add_label_unsorted (asmf, lab1);
   queue_add_tail(bf->unlinked_lbls, lab1);//Stores the label as having no target
   str_free (lblname);
   DBGMSG("create label <%s> at address %"PRIx64" [%p]\n", lab1->name, lab1->address, lab1);
   DBGMSG("at address 0x%"PRIx64", set label <%s> [%p]\n", addr - distfromgot, lab1->name, lab1);

   // Iterative step ----------------------------------------------------------
   while (idx != -1) {
      // This step consists in finding the next label, then set the current labels to all instructions from
      // the current instruction to the instruction before the next label (not included)

      idx = get_next_label(jmprel_table, pltgot_nelem, idx, pltgot_add,
            pltgot_align);
      if (idx == -1) {
         // If there is no label anymore, then the function stops.
         // This piece of code is used as conclusion step.
         return EXIT_SUCCESS;
      }

      // First, the new index is got (if its value is -1, then the conclusion step is ran)
      // Then, the return address is retrieved from the .plt.got section
      mem_bits = elfsection_get_databytes(efile, efile->indexes[PLTGOT], idx * pltgot_align, pltgot_align);
      addr = convert_bits_into_address(mem_bits, pltgot_align);
      lc_free (mem_bits);

      // Checks if the address is in plt section or madras.plt section. If not, return
      target_scn = elffile_get_scnspanaddr(efile, addr);
      if (target_scn != efile->indexes[PLT_IDX]
            && target_scn != efile->indexes[MADRASPLT_IDX])
      return EXIT_SUCCESS;

      // new label is created and assigned
      lblname = generate_label_name(jmprel_table, pltgot_add + idx * pltgot_align);
      lab2 = label_new(lblname, addr - distfromgot, TARGET_INSN, NULL);
      label_set_type(lab2, LBL_EXTFUNCTION);
      asmfile_add_label_unsorted(asmf, lab2);
      queue_add_tail(bf->unlinked_lbls, lab1);//Stores the label as having no target
      str_free(lblname);
      DBGMSG("create label <%s> at address %"PRIx64" [%p]\n", lab2->name, lab2->address, lab2);
      DBGMSG("at address 0x%"PRIx64", set label <%s> [%p]\n", addr - distfromgot, lab2->name, lab2);

      // Set the new label as the current one
      lab1 = lab2;
   }
   return EXIT_SUCCESS;
}

#ifndef _WIN32
#endif

/*
 * Changes a dynamic library in a file (used to replace a dynamic library by its patched version)
 * \param efile An ELF file
 * \param oldname Name of the dynamic library to replace
 * \param newname Name of the new dynamic library
 * \return EXIT_SUCCESS if the library has been found and replaced, error code otherwise
 */
int elffile_rename_dynamic_library (elffile_t* efile, char* oldname, char* newname)
{
   if (efile == NULL)
   return ERR_BINARY_MISSING_BINFILE;
   if (oldname == NULL || newname == NULL)
   return ERR_COMMON_PARAMETER_MISSING;

   if (strcmp (oldname, newname) == 0)
   return WRN_MADRAS_NEWNAME_IDENTICAL;

   if (efile->indexes[DYNAMIC_IDX] > 0)
   {
      int i = 0;                                                     // counter
      int size = 0;// number of entries in dynamic section
      int strtab_idx = 0;// index of the string table
      int oldlib_idx = -1;                                           // index of the old library name in the string table (including path if present)
      int oldlib_idx_shift = 0;                                      // shift from the beginning of the library path to library name if preceded by a path
      int newlib_idx = -1;                                           // index of the new library name in the string table (including path if present)
      Elf64_Dyn* dyn_entrie;// used to store a dynamic table entry
      elfsection_t* dynscn = efile->sections[efile->indexes[DYNAMIC_IDX]];// contains the dynamic section
      Elf64_Dyn** dyns = ELF_get_dyn (efile, efile->indexes[DYNAMIC_IDX], &size);
      Elf64_Dyn* newdyn = (Elf64_Dyn*)lc_malloc(sizeof(Elf64_Dyn) * size);

      // Looks for STRTAB section index
      for (i = 0; i < size; i++)
      {
         dyn_entrie = dyns[i];
         if (dyn_entrie->d_tag == DT_STRTAB)
         {
            while (strtab_idx < efile->nsections
                  && Elf_Shdr_get_sh_addr (efile, strtab_idx, NULL ) != dyn_entrie->d_un.d_ptr)
            strtab_idx ++;
         }
      }

      //Detects if the string exists in the string section
      char* strtab  = NULL;
      char* newpath = NULL;
      int indx = 0;
      while ((unsigned int) indx < ELF_get_scn_header(efile, strtab_idx)->sh_size) {
         strtab = (char*) &((ELF_get_scn_bits(efile, strtab_idx))[indx]);

         //Looking for an exact match
         if (strcmp(oldname, strtab) == 0) {
            oldlib_idx = indx;
            break;
         } else if (strchr(oldname, '/') == NULL) {
            //We want to detect here if the library name is preceded by a path (containing '/' characters)
            //We only do this if the oldname provided is not a path itself. Otherwise, we consider we know what we are doing
            //and look for an exact match only
            char* last_sep = strrchr(strtab, '/');
            if (last_sep != NULL) {
               char* name_after_sep = last_sep + 1;
               //The string from the array contains a '/': comparing with the string following the separator
               if (strcmp(oldname, name_after_sep) == 0) {
                  //The string contains a path, and the last element is the same the library name we want to replace
                  oldlib_idx_shift = name_after_sep - strtab; //Pointer to the actual name of the library
                  oldlib_idx = indx; //Pointer to the full path containing the library name
                  //Building the string containing the new name and the old path
                  newpath = lc_malloc(
                        sizeof(*newpath)
                              * (oldlib_idx_shift + strlen(newname) + 1));
                  strncpy(newpath, strtab, oldlib_idx_shift);
                  strcpy(newpath + oldlib_idx_shift, newname);
                  break;
               }
            }
         }
         indx += strlen(strtab) + 1;
      }

      if (oldlib_idx < 0)                                                 // String not found: return
         return ERR_BINARY_EXTLIB_NOT_FOUND;

      newlib_idx = elffile_upd_strtab (efile, strtab_idx, (newpath != NULL)?newpath:newname);        // add the new name and get its address

      // Looks for entries in dynamic section whose value is the same than the old name
      // and replaces them by the new name
      for (i = 0; i < size; i++) {
         dyn_entrie = dyns[i];
         memcpy(&(newdyn[i]), dyn_entrie, sizeof(Elf64_Dyn));
         if (newdyn[i].d_tag == DT_NEEDED) {
            if ((int) newdyn[i].d_un.d_val == oldlib_idx)
               newdyn[i].d_un.d_val = newlib_idx;
            else if (oldlib_idx_shift > 0 && (int) newdyn[i].d_un.d_val == (oldlib_idx + oldlib_idx_shift))
               newdyn[i].d_un.d_val = newlib_idx + oldlib_idx_shift; //Case where the entry pointed to the library name in a path
         }
      }
      elfsection_upd_data (efile, efile->indexes[DYNAMIC_IDX], newdyn, size,

      if (newpath != NULL)
         lc_free(newpath);
         } else
         return ERR_BINARY_NO_EXTLIBS;
         return EXIT_SUCCESS;
      }
      /*
       * Get a list of all dynamic libraries
       * \param efile An ELF file
       * \return A list of strings containing the names of all mandatory dynamic libraries
       */
      list_t* elffile_get_dynamic_libs (elffile_t* efile)
      {
         if (efile == NULL || efile->indexes[DYN] <= 0)
         return (NULL);
         list_t* ret = NULL;
         elfsection_t* dynscn = efile->sections[efile->indexes[DYN]];
         int i = 0;
         int size = 0;
         int strtab_idx = 0;
         tableentry_t** entries = dynscn->table_entries;

         for (i = 0; i < dynscn->nentries; i++)
         {
            if (entries[i]->entry_64 != NULL)
            {
               Elf64_Dyn* dyn_entrie = entries[i]->entry_64->elfentry.dyn;
               if (dyn_entrie->d_tag == DT_STRTAB)
               {
                  while (strtab_idx < efile->nsections && efile->sections[strtab_idx]->scnhdr->scnhdr64->sh_addr != dyn_entrie->d_un.d_ptr)
                  strtab_idx ++;
               }
            }
            else if (entries[i]->entry_32 != NULL)
            {
               Elf32_Dyn* dyn_entrie = entries[i]->entry_32->elfentry.dyn;
               if (dyn_entrie->d_tag == DT_STRTAB)
               {
                  while (strtab_idx < efile->nsections && efile->sections[strtab_idx]->scnhdr->scnhdr32->sh_addr != dyn_entrie->d_un.d_ptr)
                  strtab_idx ++;
               }
            }
         }

         if (efile->elfheader->ehdr64 != NULL)
         {
            Elf64_Dyn* dyns = (Elf64_Dyn*)dynscn->bits;
            size = dynscn->scnhdr->scnhdr64->sh_size / sizeof (Elf64_Dyn);
            for (i = 0; i < size; i++)
            {
               if (dyns[i].d_tag == DT_NEEDED)
               {
                  char* str = ELF_strptr(efile, strtab_idx, dyns[i].d_un.d_val);
                  ret = list_add_before (ret, (void*) str);
               }
            }
         }
         else if (efile->elfheader->ehdr32 != NULL)
         {
            Elf32_Dyn* dyns = (Elf32_Dyn*)dynscn->bits;
            size = dynscn->scnhdr->scnhdr32->sh_size / sizeof (Elf32_Dyn);
            for (i = 0; i < size; i++)
            {
               if (dyns[i].d_tag == DT_NEEDED)
               {
                  char* str = ELF_strptr(efile, strtab_idx, dyns[i].d_un.d_val);
                  ret = list_add_before (ret, (void*) str);
               }
            }
         }
         return (ret);
      }

      static int elffile_load_from_elf (elffile_t* efile, Elf* elf)
      {
         int i = 0;
         int res = EXIT_SUCCESS;
         // Get Elf header from Elf
         efile->elfheader = lc_malloc (sizeof (Elf_Ehdr));
         if (elf64_getehdr(elf) != NULL)
         {
            efile->elfheader->ehdr64 = lc_malloc (sizeof (Elf64_Ehdr));
            efile->elfheader->ehdr32 = NULL;
            memcpy(efile->elfheader->ehdr64, elf64_getehdr(elf), sizeof (Elf64_Ehdr));
         }
         else if (elf32_getehdr(elf) != NULL)
         {
            efile->elfheader->ehdr32 = lc_malloc (sizeof (Elf32_Ehdr));
            efile->elfheader->ehdr64 = NULL;
            memcpy(efile->elfheader->ehdr32, elf32_getehdr(elf), sizeof (Elf32_Ehdr));
         }
         else
         return ERR_BINARY_HEADER_NOT_FOUND;

         // Get program header from Elf
         Elf64_Phdr* phdr64 = elf64_getphdr (elf);
         Elf32_Phdr* phdr32 = elf32_getphdr (elf);
         efile->progheader = lc_malloc (sizeof (Elf_Phdr*) * Elf_Ehdr_get_e_phnum(efile, NULL));
         for (i = 0; i < Elf_Ehdr_get_e_phnum (efile, NULL); i++)
         {
            efile->progheader[i] = lc_malloc (sizeof (Elf_Phdr));
            if (phdr64 != NULL)
            {
               efile->progheader[i]->phdr32 = NULL;
               efile->progheader[i]->phdr64 = lc_malloc (sizeof (Elf64_Phdr));
               memcpy(efile->progheader[i]->phdr64, &phdr64[i], sizeof (Elf64_Phdr));
            }
            else if (phdr32 != NULL)
            {
               efile->progheader[i]->phdr64 = NULL;
               efile->progheader[i]->phdr32 = lc_malloc (sizeof (Elf32_Phdr));
               memcpy(efile->progheader[i]->phdr32, &phdr32[i], sizeof (Elf32_Phdr));
            }

         }

         // Get sections from Elf
         efile->nsections = Elf_Ehdr_get_e_shnum (efile, NULL);
         if (efile->nsections > 0) {
            efile->sections = lc_malloc (efile->nsections * sizeof (elfsection_t*));
            for (i = 0; i < efile->nsections; i++)
            {
               Elf_Scn* scn = elf_getscn(elf, i);
               Elf_Data* scndata = elf_getdata(scn, NULL);

               efile->sections[i] = lc_malloc (sizeof (elfsection_t));
               efile->sections[i]->scnhdr = lc_malloc (sizeof(Elf_Shdr));
               if (Elf_Ehdr_get_e_ident (efile, NULL)[EI_CLASS] == ELFCLASS64)
               {
                  efile->sections[i]->scnhdr->scnhdr64 = lc_malloc (sizeof (Elf64_Shdr));
                  efile->sections[i]->scnhdr->scnhdr64 = memcpy(efile->sections[i]->scnhdr->scnhdr64, elf64_getshdr(scn), sizeof(Elf64_Shdr));
                  efile->sections[i]->scnhdr->scnhdr32 = NULL;
               }
               else if (Elf_Ehdr_get_e_ident (efile, NULL)[EI_CLASS] == ELFCLASS32)
               {
                  efile->sections[i]->scnhdr->scnhdr64 = NULL;
                  efile->sections[i]->scnhdr->scnhdr32 = lc_malloc (sizeof (Elf32_Shdr));
                  efile->sections[i]->scnhdr->scnhdr32 = memcpy(efile->sections[i]->scnhdr->scnhdr32, elf32_getshdr(scn), sizeof(Elf32_Shdr));
               }
               efile->sections[i]->table_entries = NULL;
               efile->sections[i]->hentries = NULL;
               efile->sections[i]->nentries = 0;

               if (Elf_Shdr_get_sh_type (efile, i, NULL) != SHT_NOBITS)
               {
                  efile->sections[i]->bits = lc_malloc (scndata->d_size);
                  efile->sections[i]->bits = memcpy(efile->sections[i]->bits, scndata->d_buf, scndata->d_size);
               }
               else
               {
                  efile->sections[i]->bits = NULL;
               }
            }

            // parse sections to extract interesting structures
            for (i = 0; i < efile->nsections; i++) {
               res = parse_elf_section(efile, i);
               if (ISERROR(res))
               return res;
            }

            elffile_reset_relnames (efile);
         }
         return res;
      }

      /*
       * Opens an ELF file and parses its content
       * \param filename The name or path to the ELF file to parse
       * \param filedes The file identifier of the ELF file to parse (used if filename is NULL)
       * \param armbrs Return parameter. If not NULL and if the file is an archive, will point to an array
       *        containing the parsed ELF members. If the file is not an archive, will point to a NULL parameter.
       * \param n_armbrs Return parameter. If not NULL and if the file is an archive, will point to the number
       *        of members, otherwise will point to 0.
       * \param asmf corresponding assembly file
       * \return A pointer to the structure containing the parsed ELF file, or NULL if an error
       *        occurred (file not found, invalid format, unable to open file). If the file is an archive and
       *        \e armbrs is NULL, this will be a pointer to the first member, otherwise, it will be an empty
       *        elffile_t containing only the Elf structure for the whole file
       */
      elffile_t* parse_elf_file (char* filename, FILE* filedes, elffile_t*** armbrs, int *n_armbrs, asmfile_t* asmf)
      {
         FILE* fd = NULL;
         elffile_t* efile = elffile_new();
         int res = EXIT_SUCCESS;
         if (filedes != NULL)
         fd = filedes;
         else if (filename != NULL)
         {
            fd = fopen (filename, "r");
            if (fd == NULL) {
               efile->last_error_code = ERR_COMMON_UNABLE_TO_OPEN_FILE;
               return efile;
            }
         }
         else {
            efile->last_error_code = ERR_COMMON_FILE_NAME_MISSING;
            return efile;
         }

         Elf* elf = elf_begin (fileno (fd), ELF_C_READ, NULL);
         switch (elf_kind (elf))
         {
            case ELF_K_ELF:
            res = elffile_load_from_elf (efile, elf);
            if (ISERROR(res)) {
               efile->last_error_code = res;
               return efile;
            }
            efile->filename = (filename != NULL)? lc_strdup(filename) : lc_strdup("[no name]");
            efile->filedes = fd;
            if (asmf != NULL
                  && asmfile_get_parameter (asmf, PARAM_MODULE_DWARF, PARAM_DWARF_DISABLE_DEBUG) == FALSE)
            efile->dwarf = dwarf_api_init_light (elf, efile->filename, asmf);
            else
            efile->dwarf = NULL;

<<<<<<< HEAD
            //Sets the values for the archive members to notify that this is not an archive
            if (armbrs != NULL)
            *armbrs = NULL;
            if (n_armbrs != NULL)
            *n_armbrs = 0;
            break;

            case ELF_K_AR:
            if (armbrs != NULL )
            {
               //The caller took into account the fact that we could be handling an archive: we will parse it
               Elf* ar_o = NULL;
               unsigned int nb_objects = elf_get_ar_size (elf);
               int i = 0;

               (*armbrs) = lc_malloc(nb_objects * sizeof (elffile_t*));
               while ((ar_o = elf_begin (fileno (fd), ELF_C_READ, elf)) != NULL)
               {
                  (*armbrs)[i] = elffile_new();
                  res = elffile_load_from_elf ((*armbrs)[i], ar_o);
                  if (ISERROR(res)) {
                     efile->last_error_code = res;
                     return efile;
                  }
                  (*armbrs)[i]->filename = (elf_getname(ar_o) != NULL)? lc_strdup(elf_getname(ar_o)) : lc_strdup("[no name]");
                  if (asmf != NULL
                        && asmfile_get_parameter (asmf, PARAM_MODULE_DWARF, PARAM_DWARF_DISABLE_DEBUG) == FALSE)
                  (*armbrs)[i]->dwarf = dwarf_api_init_light (ar_o, (*armbrs)[i]->filename, asmf);
                  else
                  (*armbrs)[i]->dwarf = NULL;
                  i++;
                  elf_end (ar_o);
               }

               if (n_armbrs != NULL )
               *n_armbrs = (unsigned int) i;
               efile = *armbrs[0];
               efile->filedes = fd;
            }
            else
            {
               //The caller does not want to be bothered with archive members: we will simply parse
               //the first member and print a warning
               Elf* ar_o = elf_begin (fileno (fd), ELF_C_READ, elf);
               if (ar_o == NULL) {
                  efile->last_error_code = ERR_BINARY_ARCHIVE_PARSING_ERROR;
                  return efile;
               }
               res = elffile_load_from_elf (efile, ar_o);
               if (ISERROR(res)) {
                  efile->last_error_code = res;
                  return efile;
               }
               efile->filename = (elf_getname(ar_o) != NULL)? lc_strdup(elf_getname(ar_o)) : lc_strdup("[no name]");
               if (asmf != NULL
                     && asmfile_get_parameter (asmf, PARAM_MODULE_DWARF, PARAM_DWARF_DISABLE_DEBUG) == FALSE)
               efile->dwarf = dwarf_api_init_light (elf, efile->filename, asmf);
               else
               efile->dwarf = NULL; efile = *armbrs[0];
               efile->filedes = fd;
               elf_end (ar_o);
               WRNMSG("File is an archive. Only the first member was parsed\n");
            }
            break;

            default:
            efile->last_error_code = ERR_BINARY_UNKNOWN_FILE_TYPE;
            fclose(fd);
            break;
         }
         elf_end (elf);
         return (efile);
      }
#endif
/***********************************************************************/
////////////NEW INTERFACE FOR USE WITH BINFILE_T/////////////////////////
// Functions used during parsing//
//////////////////////////////////
/*
 * Update labels of .plt section using .rela.plt table and .got section
 * \param An asmfile to update
 * \return EXIT_SUCCESS if successful, error code otherwise
 */
int elf_x86_64_asmfile_add_ext_labels(asmfile_t* asmf)
{
   if (asmfile_test_analyze(asmf, PAR_ANALYZE) == TRUE)
      return ERR_BINARY_FILE_ALREADY_PARSED; //We don't want to execute this twice, so the ELF file must already have been parsed but the asmfile not completely updated yet
   binfile_t* bf = asmfile_get_binfile(asmf);                //elf file structure
   elffile_t* efile = binfile_get_parsed_bin(bf);

   if (!efile)
      return ERR_BINARY_MISSING_BINFILE;

   if (binfile_get_type(bf) != BFT_EXECUTABLE)
      return EXIT_SUCCESS; //Invoking this function makes no sense if we are not working on an executable
   /**\todo (2014-12-08) Maybe improve this function to handle relocatable files as well (but I don't quite see which symbols we would be adding),
    * or create another one to be invoked post parsing for filling branches and the like*/

   if (efile->indexes[GOTPLT_IDX] < 0 || efile->indexes[RELAPLT_IDX] < 0
         || efile->indexes[GOTPLT_IDX] == efile->indexes[GOT_IDX])
      return ERR_BINARY_NO_EXTFCTS_SECTION;

   binscn_t* relaplt = binfile_get_scn(bf, efile->indexes[RELAPLT_IDX]); // Retrieves the .rela.plt section
   unsigned int i;

   /**\todo TODO (2014-07-08) (2015-10-06) Maybe move the arch specific functions like this one in another file*/

   // Distance between the beginning of the stub and the instruction referenced in the .got.plt
   int64_t distfromgot = 6;

   // Now distfromgot contains the distance in bytes between the address of the label and the address in the .got.plt

   // Scanning the entries in the rela.plt section
   for (i = 0; i < relaplt->n_entries; i++) {
      data_t* pltentry = binscn_get_entry(relaplt, i);
      binrel_t* pltrel = data_get_binrel(pltentry);
      //Retrieving the name of the label associated to the .rela.plt entry
      char* relname = label_get_name(binrel_get_label(pltrel));
      if (!relname)
         continue; //Exiting if the label associated to the .rela.plt entry has no name

      //Retrieving the entry in the .got.plt entry referenced by the .rela.plt entry
      data_t* gotentry = pointer_get_data_target(binrel_get_pointer(pltrel));
      if (!gotentry)
         continue; //Exiting in case the .got.plt entry can not be found

      //Retrieving the address stored in the .got.plt entry
      int64_t gotdest = pointer_get_addr(data_get_pointer(gotentry));

      //Computes the address of the start of the stub
      int64_t stubstart = gotdest - distfromgot;
      if (gotdest == SIGNED_ERROR)
         continue; //Exiting in case the address stored in the .got.plt is not valid

      //Checking that the pointed section is a .plt section
      binscn_t* destscn = binfile_lookup_scn_span_addr(bf, stubstart);
      uint16_t destscnid = binscn_get_index(destscn);

      if ((destscnid != efile->indexes[PLT_IDX])
            && (destscnid != efile->indexes[MADRASPLT_IDX]))
         continue; //Exiting if the address points outside a .plt section

      /** \todo TODO (2014-11-06) See what is easier: use the name without suffix for the label, add the suffix when printing with the elf_extlbl_print,
       * and use the LBL_EXTFUNCTION everywhere else to detect an external label,
       * OR set the label name here with EXT_LBL_SUF and create another driver function for computing an external label name whenever we need to add one
       */
      //Builds the new label name
      char pltlabel[strlen(relname) + strlen(EXT_LBL_SUF) + 1];
      sprintf(pltlabel, "%s" EXT_LBL_SUF, relname);

      //Adds the label to the file
      label_t* pltlbl = label_new(pltlabel, stubstart, TARGET_UNDEF, NULL);
      label_set_type(pltlbl, LBL_EXTFUNCTION);
      asmfile_add_label_unsorted(asmf, pltlbl);
   }
   return EXIT_SUCCESS;
}

/*
 * Retrieves the machine code of an ELF file, performing minimal parsing on the file
 * \param file_name Name of the file
 * \return ELF-specific identifier of the machine for which this ELF file was compiled, ELF_MACHINE_CODE_ERR if not an ELF file
 * or if the file does not exist
 * */
int elf_get_machine_code(char* file_name)
{
   if (file_name == NULL)
      return ELF_MACHINE_CODE_ERR;
   FILE* fd = fopen (file_name, "r");
   if (fd == NULL) {
      return ELF_MACHINE_CODE_ERR;
   }
   unsigned int code = get_elf_machine_code(fileno(fd));
   fclose(fd);
   if (code == EM_NONE)
      code = ELF_MACHINE_CODE_ERR;

   return code;
}

int elf_arm_asmfile_add_ext_labels(asmfile_t* asmf)
{
   (void) asmf;
//   WRNMSG(
//         "Addition of labels for external functions stubs currently not supported for the ARM architecture\n");
//   return EXIT_FAILURE;
   return EXIT_SUCCESS;
}

int elf_arm64_asmfile_add_ext_labels(asmfile_t* asmf)
{
   (void) asmf;
//   WRNMSG(
//         "Addition of labels for external functions stubs currently not supported for the AArch64 architecture\n");
//   return EXIT_FAILURE;
   return EXIT_SUCCESS;
}

/**
 * Dummy function for the case when the architecture has not been recognised
 * */
int elf_unknownarch_asmfile_add_ext_labels(asmfile_t* asmf)
{
   (void) asmf;
   return EXIT_FAILURE;
}

/**
 * Dummy function for the case when the architecture has not been recognised
 * */
pointer_t* elf_unknownarch_binfile_patch_add_ext_fct(binfile_t* bf, char* fctname,
      char* libname, int preload)
{
   (void) bf;
   (void) fctname;
   (void) libname;
   (void) preload;
   return NULL;
}

/**
 * Checks if a section in the ELF contains executable code
 * \param elf The structure containing the parsing of the ELF file. Must not be NULL
 * \param scnid Index of the section. Must be valid
 * \return TRUE if the section is of type PROGBITS with executable rights, FALSE otherwise
 * */
static int Elf_Scn_isprog(Elf* elf, uint16_t scnid)
{
   assert(elf);

   return ((Elf_Shdr_get_sh_type(elf, scnid) == SHT_PROGBITS)
         && (Elf_Shdr_get_sh_flags(elf, scnid) & SHF_EXECINSTR));
}

/**
 * Retrieves the name of an ELF section
 * \param elf The structure containing the parsing of the ELF file. Must not be NULL
 * \param scnid Index of the section. Must be valid
 * \return The name of the section
 * */
static char* Elf_Scn_getname(Elf* elf, uint16_t scnid)
{
   assert(elf && (scnid < Elf_Ehdr_get_e_shnum(elf)));
   return elf_strptr(elf, Elf_Ehdr_get_e_shstrndx(elf),
         Elf_Shdr_get_sh_name(elf, scnid));
}

/**
 * Retrieves the type of a label depending on the characteristics of the corresponding ELFSym entry
 * \param elf The structure containing the parsing of the ELF file.
 * \param symname Name of the label
 * \param shndx Index of the section referenced by the symbol
 * \param type Type of the symbol (retrieved with ELFxx_ST_TYPE(st_info) )
 * \param bind Binding attribute of the symbol (retrieved with ELFxx_ST_BIND(st_info) )
 * */
static uint8_t ElfSym_get_labeltype(Elf* elf, char* symname, uint32_t shndx,
      uint32_t type, uint32_t bind)
{
   assert(elf);
   uint8_t lbltype = LBL_GENERIC;
   /**\todo TODO (2014-11-14) See whether we need to represent labels of type SHN_COMMON and if so how*/

   if (shndx == SHN_UNDEF)
      lbltype = LBL_EXTERNAL;    //Label is defined in another file
   else if ((!Elf_Scn_isprog(elf, shndx)) || (symname[0] == '$'))
      lbltype = LBL_NOFUNCTION; //Label is not associated to a section containing code, it can't identify a function
   else if (strcmp(symname, LABEL_PATCHMOV) == 0)
      lbltype = LBL_PATCHSCN; //Label marks the beginning of the section added by the patcher
   else if (type == DUMMY_SYMBOL_TYPE)
      lbltype = LBL_DUMMY;      //Label is a "dummy" label added by the patcher
   else if (type == STT_FUNC)
      lbltype = LBL_FUNCTION; //No dwarf present and label marked as function in the ELF
   else if (bind == STB_LOCAL)
      lbltype = LBL_NOFUNCTION; //Label is not marked as a function and is local
   else if (strstr(symname, ".") != NULL)
      lbltype = LBL_NOFUNCTION; //Label contains a dot and is not marked as function

   if ((lbltype == LBL_NOFUNCTION) && (type == STT_OBJECT))
      lbltype = LBL_VARIABLE; //Label is not a function and is marked as object: should be a variable label

   //DIRTY FIX FOR _REAL_FINI //EUGENE
   if (strcmp(symname, "_real_fini") == 0) {
      lbltype = LBL_FUNCTION;
   }

   return lbltype;
}

/*
 * Returns the index of the section starting at the given address
 * \param ef Structure containing the parsed ELF file
 * \param addr Address
 * \return Index of the section in the file or UINT16_MAX if not found
 * */
static uint16_t Elf_getscnid_by_address(Elf* elf, int64_t addr)
{
   if (!elf)
      return UINT16_MAX;
   int scnid = 0;
   uint16_t nscn = Elf_Ehdr_get_e_shnum(elf);
   while (scnid < nscn) {
      if (Elf_Shdr_get_sh_addr(elf, scnid) == (uint64_t) addr)
         break;
      scnid++;
   }
   if (scnid >= nscn)
      scnid = UINT16_MAX;
   return scnid;
}

static int elf_loadsection_to_binfile(binfile_t* bf, elffile_t* efile,
      uint16_t scnid);

/**
 * Code for loading the symbols from a parsed ELF file depending on the word size of the file (32/64).
 * Intended to be used in elf_loadsymscn_to_binfile to factorise code.
 * \param WS Word size of the ELF file
 * */
#define LOADSYMSCN_FROM_ELF(WS) {\
   MACROVAL_CONCAT(MACRO_CONCAT(Elf,WS),_Sym)* syms = (MACROVAL_CONCAT(MACRO_CONCAT(Elf,WS),_Sym)*)binscn_get_data(scn, NULL);\
   /*Retrieves the symbol table*/ \
   /*Retrieves the entries in the symbol table*/ \
   for (i = 0; i < binscn_get_nb_entries(scn); i++) {\
      /*Retrieves the name of the symbol*/\
      char* symname = elf_strptr(elf, Elf_Shdr_get_sh_link (elf, scnid), syms[i].st_name);/**\todo Use the size-dependent version here*/\
      /*Creates a new label corresponding to this symbol.*/\
      /**\todo WARNING: Address changes in case of ARM. Check how to do it properly => (2015-10-06) May be done below*/\
      label_t* symlbl;\
      symlbl = label_new(symname, syms[i].st_value, TARGET_UNDEF, NULL);\
      /*Updates the type of the label depending on its properties from ELF*/\
      label_set_type(symlbl, ElfSym_get_labeltype(elf, symname, syms[i].st_shndx, MACROVAL_CONCAT(MACRO_CONCAT(ELF,WS),_ST_TYPE)(syms[i].st_info), MACROVAL_CONCAT(MACRO_CONCAT(ELF,WS),_ST_BIND)(syms[i].st_info)));\
      /*Identifies the section to which the symbol belongs*/\
      uint16_t symscnid = (syms[i].st_shndx != SHN_ABS && syms[i].st_shndx != SHN_COMMON && syms[i].st_shndx != SHN_UNDEF)?syms[i].st_shndx:UINT16_MAX;\
      /*Adds the label to the binary file*/\
      data_t* entry = binfile_addlabel(bf, scnid, i, oldnlbls + i, symlbl, 0, symscnid);\
      /*Looks up the data string entry representing the name of the symbol*/\
      uint64_t off = 0;\
      data_t* strentry = binscn_lookup_entry_by_offset(strscn, syms[i].st_name, &off);\
      assert(strentry);\
      /*Creates a pointer to the data entry*/\
      pointer_t* ptrstr = pointer_new(0, 0, strentry, POINTER_ABSOLUTE, TARGET_DATA);\
      pointer_set_offset_in_target(ptrstr, off);\
      /*Links the data entry representing the label to the string entry representing its name*/\
      hashtable_insert(efile->symnames, entry, ptrstr);\
   }\
}

/**
 * Loads the details about a section containing symbols from a parsed ELF file into a binfile_t structure.
 * This includes updating global tables in binfile_t
 * \param bf The binfile_t structure to fill. Its \c sections array must be initialised
 * \param elf The structure containing the parsing of the ELF file. Must be of type ELF
 * \param efile Internal libmtroll structure representing the file
 * \param scnid Index of the section being parsed. It must be of type SHT_SYMTAB or SHT_DYNSYM.
 * The corresponding binscn_t in the binfile_t must already have its data field set with the bytes of the section
 * and the number of entries must be set.
 * \return EXIT_SUCCESS if the operation was successful, error code otherwise
 * */
static int elf_loadsymscn_to_binfile(binfile_t* bf, elffile_t* efile,
      uint16_t scnid)
{
   assert(bf && efile);
   uint32_t i;
   binscn_t* scn = binfile_get_scn(bf, scnid);
   DBGMSGLVL(1,
         "Section %d (%s) at %s %#"PRIx64" contains symbols (%u entries of size %#"PRIx64")\n",
         scnid, binscn_get_name(scn),
         (binscn_get_addr(scn) > 0) ? "address" : "offset",
         (binscn_get_addr(scn) > 0) ?
               binscn_get_addr(scn) : binscn_get_offset(scn),
         binscn_get_nb_entries(scn), binscn_get_entry_size(scn));
   uint32_t oldnlbls = binfile_get_nb_labels(bf);
   Elf* elf = efile->elf;
   //Loads the string section associated to this section
   elf_loadsection_to_binfile(bf, efile, Elf_Shdr_get_sh_link(elf, scnid));
   //Retrieves a pointer to the parsed string section
   binscn_t* strscn = binfile_get_scn(bf, Elf_Shdr_get_sh_link(elf, scnid));
   //Update the array of labels present in the file
   binfile_set_nb_labels(bf, oldnlbls + binscn_get_nb_entries(scn));
   if (Elf_Ehdr_get_e_ident(elf)[EI_CLASS] == ELFCLASS64)
      LOADSYMSCN_FROM_ELF(64)
   else if (Elf_Ehdr_get_e_ident(elf)[EI_CLASS] == ELFCLASS32)
      LOADSYMSCN_FROM_ELF(32)

   return EXIT_SUCCESS;
}

/**
 * Code for loading a relocation into a binfile_t structure depending on the word size of the file (32/64) and the type of relocation (rel/rela)
 * It is intended to be invoked from elf_loadrelscn_to_binfile
 * \param WS Word size of the file (32/64)
 * \param RT Relocation type (_Rel/_Rela)
 * */
#define LOADRELSCN_FROM_ELF(WS, RT) {\
   MACROVAL_CONCAT(MACRO_CONCAT(Elf, WS), RT)* rels = (MACROVAL_CONCAT(MACRO_CONCAT(Elf, WS), RT)*)binscn_get_data(scn, NULL);\
   for (i = 0; i < binscn_get_nb_entries(scn); i++) {\
      /*Retrieves the index of the associated symbol*/\
      uint32_t relsym = MACROVAL_CONCAT(MACRO_CONCAT(ELF,WS),_R_SYM)(rels[i].r_info);\
      /*Retrieves the corresponding label*/\
      label_t* rellbl = data_get_data_label(binscn_get_entry(binfile_get_scn(bf, symtbl), relsym));\
      /*Sets the address or offset depending on the data contained in r_offset*/\
      int64_t address = (r_offset_isoff)?ADDRESS_ERROR:rels[i].r_offset;\
      uint64_t offset = (r_offset_isoff)?rels[i].r_offset:UINT64_MAX;\
      /*Adds a relocation entry to the binary file*/\
		binfile_addreloc(bf, scnid, i, rellbl, 0, address, offset, relscn, MACROVAL_CONCAT(MACRO_CONCAT(ELF,WS),_R_TYPE)(rels[i].r_info));\
   }\
}

/**
 * Loads the details about a section containing relocations from a parsed ELF file into a binfile_t structure.
 * This includes updating global tables in binfile_t
 * \param bf The binfile_t structure to fill. Its \c sections array must be initialised
 * \param elf The structure containing the parsing of the ELF file. Must be of type ELF
 * \param efile Internal libmtroll structure representing the file
 * \param scnid Index of the section being parsed. It must be of type SHT_REL or SHT_RELA.
 * The corresponding binscn_t in the binfile_t must already have its data field set with the bytes of the section
 * and the number of entries must be set.
 * \return EXIT_SUCCESS if the operation was successful, error code otherwise
 * */
static int elf_loadrelscn_to_binfile(binfile_t* bf, elffile_t* efile,
      uint16_t scnid)
{
   assert(bf && efile);

   uint32_t i;
   binscn_t* scn = binfile_get_scn(bf, scnid);
   DBGMSGLVL(1,
         "Section %d (%s) at %s %#"PRIx64" contains relocations (%u entries of size %#"PRIx64")\n",
         scnid, binscn_get_name(scn),
         (binscn_get_addr(scn) > 0) ? "address" : "offset",
         (binscn_get_addr(scn) > 0) ?
               binscn_get_addr(scn) : binscn_get_offset(scn),
         binscn_get_nb_entries(scn), binscn_get_entry_size(scn));
   Elf* elf = efile->elf;
   int64_t scnaddr = binscn_get_addr(scn);
   int r_offset_isoff = FALSE; //Identifies whether the r_offset field of the relocations contain an offset or an address
   //Retrieves the identifier of the associated symbol table and parses it
   uint16_t symtbl = Elf_Shdr_get_sh_link(elf, scnid);
   //Retrieves the identifier of the target section and parses it
   uint16_t relscn = Elf_Shdr_get_sh_info(elf, scnid);

   //Special handling depending on the type of the file
   if (relscn == 0 || binfile_get_type(bf) == BFT_EXECUTABLE
         || binfile_get_type(bf) == BFT_LIBRARY) {
      //Executables & libraries don't seem to take the sh_info into account and may set it to 0
      relscn = UINT16_MAX; //Forcing this to max ensures that the binfile will look up based on the address
      r_offset_isoff = FALSE; //The r_offset entry contains an address
   } else if (binfile_get_type(bf) == BFT_RELOCATABLE) {
      r_offset_isoff = TRUE;  //The r_offset entry contains an offset
   }

   DBGMSGLVL(2,
         "Relocation section %s uses symbol section %d (%s) and targets section %d (%s)\n",
         binscn_get_name(scn), symtbl, binfile_get_scn_name(bf, symtbl), relscn,
         binfile_get_scn_name(bf, relscn))

   if (Elf_Ehdr_get_e_ident(elf)[EI_CLASS] == ELFCLASS64) {
      switch (Elf_Shdr_get_sh_type(elf, scnid)) {
      case SHT_REL:
         LOADRELSCN_FROM_ELF(64, _Rel)
         ;
         break;
      case SHT_RELA:
         LOADRELSCN_FROM_ELF(64, _Rela)
         ;
         break;
      }
   } else if (Elf_Ehdr_get_e_ident(elf)[EI_CLASS] == ELFCLASS32) {
      switch (Elf_Shdr_get_sh_type(elf, scnid)) {
      case SHT_REL:
         LOADRELSCN_FROM_ELF(32, _Rel)
         ;
         break;
      case SHT_RELA:
         LOADRELSCN_FROM_ELF(32, _Rela)
         ;
         break;
      }
   }
   return EXIT_SUCCESS;
}

/**
 * Code for loading a dynamic entry into a binfile_t structure depending on the word size of the file (32/64)
 * It is intended to be invoked from elf_loaddynscn_to_binfile
 * \param WS Word size of the file (32/64)
 * */
#define LOADDYNSCN_FROM_ELF(WS) {\
   MACROVAL_CONCAT(MACRO_CONCAT(Elf,WS),_Dyn)* dyns = (MACROVAL_CONCAT(MACRO_CONCAT(Elf,WS),_Dyn)*)binscn_get_data(scn, NULL);\
   /*First pass to handle entries containing an address and thus referencing to other sections*/\
   for (i = 0; i < n_entries; i++) {\
      data_t* entry = NULL;\
      switch(dyns[i].d_tag) {\
         uint16_t refscn; /*Identifier of a referenced section*/\
      /*Dynamic entries containing an address*/\
      case DT_PLTGOT:\
         efile->indexes[GOTPLT_IDX] = Elf_getscnid_by_address(elf,dyns[i].d_un.d_ptr);\
      case DT_JMPREL:\
         if(dyns[i].d_tag==DT_JMPREL)\
            efile->indexes[RELAPLT_IDX] = Elf_getscnid_by_address(elf,dyns[i].d_un.d_ptr);\
      case DT_HASH:\
      case DT_STRTAB:\
         if ( (dyns[i].d_tag == DT_STRTAB) && (efile->indexes[DYNSTR_IDX] == -1) )\
            efile->indexes[DYNSTR_IDX] = Elf_getscnid_by_address(elf,dyns[i].d_un.d_ptr);/**\todo See if it is always the case*/\
      case DT_SYMTAB:\
      case DT_RELA:\
      case DT_INIT:\
      case DT_FINI:\
      case DT_REL:\
      case DT_VERSYM:\
         /*Retrieves the index of the corresponding section*/\
		   refscn = Elf_getscnid_by_address(elf,dyns[i].d_un.d_ptr);\
		   DBGMSGLVL(2, "Dynamic entry %d references address %#"MACROVAL_CONCAT(PRIx, WS)" which matches section %d\n", i, dyns[i].d_un.d_ptr, refscn);\
		   /*Parses the section*/\
         elf_loadsection_to_binfile(bf, efile, refscn);\
         /**\todo Check return value of elf_loadsection_to_binfile to detect cases when the section was not found */\
         /*Now creates a reference object in the file*/\
         entry = binfile_add_ref(bf, scnid, i, dyns[i].d_un.d_ptr, 0, refscn, binfile_get_scn(bf, refscn));\
         break;\
      /*Null dynamic entries*/\
      case DT_NULL:\
         /*Creating an entry of type NULL*/\
         entry = data_new(DATA_NIL, &(dyns[i]), sizeof(dyns[i]));\
         DBGMSGLVL(2, "Dynamic entry %d is of type NULL\n", i);\
         break;\
      /*Dynamic entries containing a string*/\
      case DT_NEEDED:\
      case DT_SONAME:\
      case DT_RPATH:\
         /*Creating a reference to the data object in the string section*/\
         entry = binfile_add_ref_byoffset(bf, scnid, i, efile->indexes[DYNSTR_IDX], dyns[i].d_un.d_val, 0);\
         assert(entry);\
         DBGMSGLVL(2, "Dynamic entry %d references string %s\n", i, data_get_string(pointer_get_data_target(data_get_pointer(entry))));\
         if (dyns[i].d_tag == DT_NEEDED)\
            binfile_addextlib(bf, entry); /*Adds the entry as a needed external library*/\
         break;\
      default:\
         /*Default case: creating a raw entry here just for coherence*/\
		   entry = data_new(DATA_RAW, &(dyns[i]), sizeof(dyns[i]));\
         DBGMSGLVL(1, "Dynamic entry %d is of type unknown\n", i);\
      }\
      /*Stores the dynamic entry in the section*/\
      binscn_add_entry(scn, entry, i);\
      DBGMSGLVL(3, "Stored entry %p at index %d (address %#"PRIx64") in section %d (%s)\n", entry, i, data_get_addr(entry), scnid, binscn_get_name(scn));\
   }\
}

/**
 * Loads the details about the dynamic section from a parsed ELF file into a binfile_t structure.
 * This includes updating global tables in binfile_t
 * \param bf The binfile_t structure to fill. Its \c sections array must be initialised
 * \param elf The structure containing the parsing of the ELF file. Must be of type ELF
 * \param efile Internal libmtroll structure representing the file
 * \param scnid Index of the section being parsed. It must be of type SHT_DYNAMIC.
 * The corresponding binscn_t in the binfile_t must already have its data field set with the bytes of the section
 * and the number of entries must be set.
 * \return EXIT_SUCCESS if the operation was successful, error code otherwise
 * */
static int elf_loaddynscn_to_binfile(binfile_t* bf, elffile_t* efile,
      uint16_t scnid)
{
   assert(bf && efile);

   uint32_t i;
   binscn_t* scn = binfile_get_scn(bf, scnid);
   DBGMSGLVL(1,
         "Section %d (%s) at %s %#"PRIx64" contains dynamic (%u entries of size %#"PRIx64")\n",
         scnid, binscn_get_name(scn),
         (binscn_get_addr(scn) > 0) ? "address" : "offset",
         (binscn_get_addr(scn) > 0) ?
               binscn_get_addr(scn) : binscn_get_offset(scn),
         binscn_get_nb_entries(scn), binscn_get_entry_size(scn));

   uint32_t n_entries = binscn_get_nb_entries(scn);
   int64_t scnaddr = binscn_get_addr(scn);

   Elf* elf = efile->elf;

   if (Elf_Ehdr_get_e_ident(elf)[EI_CLASS] == ELFCLASS64)
      LOADDYNSCN_FROM_ELF(64)
   else if (Elf_Ehdr_get_e_ident(elf)[EI_CLASS] == ELFCLASS32)
      LOADDYNSCN_FROM_ELF(32)

   return EXIT_SUCCESS;
}

/**
 * Loads the details about a section containing strings from a parsed ELF file into a binfile_t structure.
 * This includes updating global tables in binfile_t
 * \param bf The binfile_t structure to fill. Its \c sections array must be initialised
 * \param elf The structure containing the parsing of the ELF file. Must be of type ELF
 * \param efile Internal libmtroll structure representing the file
 * \param scnid Index of the section being parsed. It must be of type SHT_STRTAB.
 * The corresponding binscn_t in the binfile_t must already have its data field set with the bytes of the section
 * and the number of entries must be set.
 * \return EXIT_SUCCESS if the operation was successful, error code otherwise
 * */
static int elf_loadstrscn_to_binfile(binfile_t* bf, elffile_t* efile,
      uint16_t scnid)
{
   assert(bf && efile);

   binscn_t* scn = binfile_get_scn(bf, scnid);
   DBGMSGLVL(1,
         "Section %d (%s) at %s %#"PRIx64" contains strings (%u entries of size %#"PRIx64")\n",
         scnid, binscn_get_name(scn),
         (binscn_get_addr(scn) > 0) ? "address" : "offset",
         (binscn_get_addr(scn) > 0) ?
               binscn_get_addr(scn) : binscn_get_offset(scn),
         binscn_get_nb_entries(scn), binscn_get_entry_size(scn));

   return binscn_load_str_scn(scn);
}

/**
 * Code loading the contents of a .got section depending on the word size of the file
 * \param WS Word size of the binary file (32/64)
 * */
#define LOADGOTSCN_FROM_ELF(WS) {\
   /*Retrieves the data of the section as an array of addresses*/\
   MACROVAL_CONCAT(MACRO_CONCAT(Elf,WS),_Addr)* addrs = (MACROVAL_CONCAT(MACRO_CONCAT(Elf,WS),_Addr)*)binscn_get_data(scn, NULL);\
   /*Computes the size of the array*/\
   uint64_t naddrs = Elf_Shdr_get_sh_size (elf, scnid) / sizeof(*addrs);\
   uint64_t i;\
   /*Creates the array of data structures for this section*/\
   if (naddrs > 0) {\
      binscn_set_nb_entries(scn, naddrs);\
   }\
   /*Creates a data entry for each address in the array*/\
   for (i = 0; i < naddrs; i++) {\
      /*Creates a pointer to the address*/\
      binfile_add_ref(bf, scnid, i, addrs[i], 0, UINT16_MAX, NULL);\
   }\
}

/**
 * Loads the details about a section containing code from a parsed ELF file into a binfile_t structure.
 * This includes updating global tables in binfile_t
 * \param bf The binfile_t structure to fill. Its \c sections array must be initialised
 * \param elf The structure containing the parsing of the ELF file. Must be of type ELF
 * \param efile Internal libmtroll structure representing the file
 * \param scnid Index of the section being parsed. It must be of type SHT_PROGBITS.
 * The corresponding binscn_t in the binfile_t must already have its data field set with the bytes of the section
 * and the number of entries must be set.
 * \return EXIT_SUCCESS if the operation was successful, error code otherwise
 * */
static int elf_loadprgscn_to_binfile(binfile_t* bf, elffile_t* efile,
      uint16_t scnid)
{
   assert(bf);

   binscn_t* scn = binfile_get_scn(bf, scnid);
   DBGMSGLVL(1,
         "Section %d (%s) at %s %#"PRIx64" contains code (%u entries of size %#"PRIx64")\n",
         scnid, binscn_get_name(scn),
         (binscn_get_addr(scn) > 0) ? "address" : "offset",
         (binscn_get_addr(scn) > 0) ?
               binscn_get_addr(scn) : binscn_get_offset(scn),
         binscn_get_nb_entries(scn), binscn_get_entry_size(scn));
   Elf* elf = efile->elf;

   //The section holds some program bytecode
   //Special case : handling of the .got section, which is an array of addresses
   //!\todo Check if the handling of the .got section is not too specific to x86
   if (strstr(Elf_Scn_getname(elf, scnid), ".got") != NULL) {
      binscn_set_type(scn, SCNT_REFS);
      if (Elf_Ehdr_get_e_ident(elf)[EI_CLASS] == ELFCLASS64)
         LOADGOTSCN_FROM_ELF(64)
      else if (Elf_Ehdr_get_e_ident(elf)[EI_CLASS] == ELFCLASS32)
         LOADGOTSCN_FROM_ELF(32)

      //Stores the index of the .got section
      if (str_equal(Elf_Scn_getname(elf, scnid), GOTNAME))
         efile->indexes[GOT_IDX] = scnid;
   }

   //Resetting the number of entries to 0 for code sections. This is to handle the fact that the .plt can have a nonzero number of entries
   if (binscn_get_type(scn) == SCNT_CODE)
      binscn_set_nb_entries(scn, 0);

   //Handling the fact where the number of entries was set because the entry size was (I encountered it on a .rodata section...)
   if (binscn_get_nb_entries(scn) > 0 && binscn_get_type(scn) == SCNT_DATA) {
      assert(binscn_get_entry_size(scn) > 0); //This should be the only case for this to happen
      binscn_load_entries(scn, DATA_RAW);
   }
   char* scnname = binscn_get_name(scn);
   // Storing indexes of important sections
   //Storing the .got.plt section index (with static binary : no .dynamic section)
   if (str_equal(scnname, GOTPLTNAME) && efile->indexes[GOTPLT_IDX] == -1)
      efile->indexes[GOTPLT_IDX] = scnid;
   else if (str_equal(scnname, PLTNAME)) {
      //Storing the .plt section index
      efile->indexes[PLT_IDX] = scnid;
      binscn_add_attrs(scn, SCNA_STDCODE | SCNA_EXTFCTSTUBS);
   } else if (Elf_Shdr_get_sh_type(elf, scnid) == SHT_PROGBITS
         && (Elf_Shdr_get_sh_flags(elf, scnid) & SHF_TLS)) {
      //Storing the .tdata section index
      efile->indexes[TDATA_IDX] = scnid;
   } else if (str_equal(scnname, TXTNAME) || str_equal(scnname, ININAME)
         || str_equal(scnname, FINNAME)) {
      //Flagging the standard code sections
      binscn_add_attrs(scn, SCNA_STDCODE);
   } else if (str_equal(scnname, MADRAS_TEXTSCN_NAME)) {
      //Storing the .madras.code
      efile->indexes[MADRASTEXT_IDX] = scnid;
      binfile_set_patch_status(bf, BFP_MADRAS);
      binscn_add_attrs(scn, SCNA_PATCHED | SCNA_STDCODE);
   } else if (str_equal(scnname, MADRAS_PLTSCN_NAME)) {
      //Storing the .madras.plt
      efile->indexes[MADRASPLT_IDX] = scnid;
      binfile_set_patch_status(bf, BFP_MADRAS);
      binscn_add_attrs(scn, SCNA_PATCHED | SCNA_EXTFCTSTUBS);
   } else if (str_equal(scnname, MADRAS_DATASCN_NAME)) {
      //Storing the .madras.data
      efile->indexes[MADRASDATA_IDX] = scnid;
      binfile_set_patch_status(bf, BFP_MADRAS);
      binscn_add_attrs(scn, SCNA_PATCHED);
   } else if (str_equal(scnname, DYNINST_SCN_NAME)) {
      binscn_add_attrs(scn, SCNA_PATCHED);
   }

   return EXIT_SUCCESS;
}

/**Update an index in the ELF file if it was not already set
 * Intended to be invoked from a function where the elffile_t structure is called efile
 * \param IDX Index to update
 * \param I Value to give to the index*/
#define UPD_INDEX(IDX, I) if(efile->indexes[IDX] == -1) efile->indexes[IDX] = I;

/**
 * Returns the code defining the section type in a binscn_t from the ELF code
 * \param elfscntype ELF code representing the type of a section
 * \param attrs Attributes over the section (using SCNA_* values)
 * \return Corresponding code used to represent the type of a section in binfile
 * */
static scntype_t sh_type_to_scntype(Elf64_Word elfscntype, unsigned int attrs)
{
   scntype_t out = SCNT_UNKNOWN;
   switch (elfscntype) {
   case SHT_DYNAMIC:
   case SHT_GNU_versym:
      out = SCNT_REFS;
      break;
   case SHT_SYMTAB:
   case SHT_DYNSYM:
      out = SCNT_LABEL;
      break;
   case SHT_REL:
   case SHT_RELA:
      out = SCNT_RELOC;
      break;
   case SHT_STRTAB:
      out = SCNT_STRING;
      break;
   case SHT_GNU_verneed:
      //TODO ?
      break;
   case SHT_NOBITS:
      out = SCNT_ZERODATA;
      break;
   case SHT_PROGBITS:
      out = (attrs & SCNA_EXE) ? SCNT_CODE : SCNT_DATA;
      break;
   default:
      break;
   }
   return out;
}

/**
 * Loads the details about a section from a parsed ELF file into a binfile_t structure.
 * This includes updating global tables in binfile_t
 * \param bf The binfile_t structure to fill. Its \c sections array must be initialised
 * \param elf The structure containing the parsing of the ELF file. Must be of type ELF
 * \param efile Internal libmtroll structure representing the file
 * \param scnid Index of the section being parsed
 * \return EXIT_SUCCESS if the operation was successful, error code otherwise
 * */
static int elf_loadsection_to_binfile(binfile_t* bf, elffile_t* efile,
      uint16_t scnid)
{
   assert(bf && efile);
   if (scnid >= binfile_get_nb_sections(bf))
      return ERR_BINARY_SECTION_NOT_FOUND; //Section index out of range
   if (binfile_get_scn(bf, scnid))
      return EXIT_SUCCESS; //Section has already been parsed
   Elf* elf = efile->elf;

   DBGMSG("Loading section %d from ELF\n", scnid);

   Elf64_Word scntype = Elf_Shdr_get_sh_type(elf, scnid);  //Type of the section
   Elf64_Xword scnentrysz = Elf_Shdr_get_sh_entsize(elf, scnid); //Size of entries (is 0 for sections not containing entries)
   Elf_Data* scndata = elf_getdata(elf_getscn(elf, scnid), NULL);
   Elf64_Xword scnflags = Elf_Shdr_get_sh_flags(elf, scnid);
   unsigned int attrs = ((scnflags & SHF_WRITE) ? SCNA_WRITE : SCNA_READ) /*Section is accessible for writing*/
   | ((scnflags & SHF_EXECINSTR) ? SCNA_EXE : SCNA_READ) /*Section is executable*/
   | ((scnflags & SHF_TLS) ? SCNA_TLS : SCNA_READ) /*Section is used by thread local storage*/
   | ((scnflags & SHF_ALLOC) ? SCNA_LOADED : SCNA_READ); /*Section is loaded during execution*/

   //Creates the section in the binary file
   binscn_t* scn = binfile_init_scn(bf, scnid, Elf_Scn_getname(elf, scnid),
         sh_type_to_scntype(scntype, attrs), Elf_Shdr_get_sh_addr(elf, scnid),
         attrs);

   /*Retrieving fields from the section*/

   //Raw data contained in the section
   binscn_set_data(scn, (scndata) ? scndata->d_buf : NULL, FALSE);
   //Size in bytes of the section
   binscn_set_size(scn, Elf_Shdr_get_sh_size(elf, scnid));
   //Offset in the file where the section begins
   binscn_set_offset(scn, Elf_Shdr_get_sh_offset(elf, scnid));
   //Alignment of the section
   binscn_set_align(scn, Elf_Shdr_get_sh_addralign(elf, scnid));
   //Rights over the section

   DBGLVL(3,
         FCTNAMEMSG("Section %d (%s) contains data: ", binscn_get_index(scn), binscn_get_name(scn));
         unsigned char* __scn_buf = scndata->d_buf;
         if (__scn_buf == NULL) fprintf(stderr, "<NULL>\n"); else {
            uint32_t i; for (i = 0; i < Elf_Shdr_get_sh_size(elf, scnid); i++) fprintf(stderr, "%02x ", __scn_buf[i]);
            fprintf(stderr, "\n");
         })

   //Initialising the array of entries if the section contains one
   if ((scnentrysz > 0) && !(scnflags & SHF_EXECINSTR)) { //Also discarding code sections with an entry size not null (this happens with the .plt)
      /**\todo (2015-02-10) Check if we could use the entry size for the .plt for something or other (maybe retrieving external labels). Right now
       * I'm discarding it as it creates NULL entries and I don't want that*/
      //Setting size of entries
      binscn_set_entry_size(scn, scnentrysz);
      //Array is created containing NULL pointers, entries will be initialised depending on the type of data
      binscn_set_nb_entries(scn, Elf_Shdr_get_sh_size(elf, scnid) / scnentrysz);
   }

   //Type specific processing
   switch (scntype) {
   /**\todo TODO (2014-11-28) I already do the switch in elftype_to_scntype. See how we could avoid this*/
   case SHT_DYNAMIC:
      elf_loaddynscn_to_binfile(bf, efile, scnid);
      UPD_INDEX(DYNAMIC_IDX, scnid)
      ;
      break;
   case SHT_DYNSYM:
      elf_loadsymscn_to_binfile(bf, efile, scnid);
      UPD_INDEX(DYNSYM_IDX, scnid)
      ;
      UPD_INDEX(DYNSTR_IDX, Elf_Shdr_get_sh_link(elf, scnid))
      ;
      break;
   case SHT_REL:
      elf_loadrelscn_to_binfile(bf, efile, scnid);
      if (str_equal(Elf_Scn_getname(elf, Elf_Shdr_get_sh_link(elf, scnid)),
            ".text"))
         UPD_INDEX(RELO_IDX, scnid)
      ;
      break;
   case SHT_RELA:
      elf_loadrelscn_to_binfile(bf, efile, scnid);
      if (str_equal(Elf_Scn_getname(elf, Elf_Shdr_get_sh_link(elf, scnid)),
            ".text"))
         UPD_INDEX(RELA_IDX, scnid)
      ;
      break;
   case SHT_STRTAB:
      elf_loadstrscn_to_binfile(bf, efile, scnid);
      if (scnid == Elf_Ehdr_get_e_shstrndx(elf))
         UPD_INDEX(SHSTRTAB_IDX, scnid)
      ;
      break;
   case SHT_SYMTAB:
      elf_loadsymscn_to_binfile(bf, efile, scnid);
      UPD_INDEX(SYMTAB_IDX, scnid)
      ;
      UPD_INDEX(STRTAB_IDX, Elf_Shdr_get_sh_link(elf, scnid))
      ;
      break;
   case SHT_NOBITS:
      if (str_equal(scn->name, BSSNAME)) {
         UPD_INDEX(BSS_IDX, scnid)
      } else if (scnflags & SHF_TLS) {
         UPD_INDEX(TBSS_IDX, scnid);
      }
   case SHT_PROGBITS:
      elf_loadprgscn_to_binfile(bf, efile, scnid);
      break;
   case SHT_GNU_versym:
      //The section contains version symbols informations
      UPD_INDEX(VERSYM_IDX, scnid)
      ;
      binscn_load_entries(scn, DATA_VAL);
      break;
   case SHT_GNU_verneed:
      /**\todo (2015-04-28) The verneed section is so complicated (interleaved array with non homogeneous)
       * that it would be really hard to write it using the libbin and would not offer to much.
       * If the libbin evolves at some point in the future to allow to represent such arrays more easily, it may be
       * interesting to store the verneed entries using data_t and the libbin, but so far we will stick to that.
       * */
      break;
   default:
      //Default case: loads entries (if existing as raw elements)
      binscn_load_entries(scn, DATA_RAW);
      break;
   }
   //The section contains version symbols informations
   // Now setting the name of the section. We have to do this now to avoid problems when it's the section containing section names we are loading
   //Retrieves the section containing section names
   binscn_t* strscn = binfile_get_scn(bf, Elf_Ehdr_get_e_shstrndx(elf));
   //Finds the data entry representing the section name
   uint64_t off = 0;
   data_t* scnnament = binscn_lookup_entry_by_offset(strscn,
         Elf_Shdr_get_sh_name(elf, scnid), &off);
   assert(scnnament);
   //Creates a pointer entry to the entry representing the name
   pointer_t* nameptr = pointer_new(0, 0, scnnament, POINTER_ABSOLUTE,
         TARGET_DATA);
   pointer_set_offset_in_target(nameptr, off);
   //Links the section to the entry representing its name
   hashtable_insert(efile->scnnames, scn, nameptr);

   //Attempts to associate the section with at least one segment
#ifndef NDEBUG
   int out =
#endif
         binfile_addsection_tosegment(bf, scn, NULL);
   DBG(
         if (ISERROR(out)) FCTNAMEMSG("Section %d (%s) could not be associated to a segment\n", scnid, binscn_get_name(scn)))

   return EXIT_SUCCESS;
}

/**
 * Generates the name of an external label
 * \param lblname String representing the name of an external function
 * \return An allocated string containing the name of the label, or NULL if lblname is NULL
 * \todo TODO (2014-11-06) See what is easier: using this function only when looking for external label names,
 * OR do not set the label name in elf_*_asmfile_add_ext_labels with EXT_LBL_SUF
 * and create another driver function for printing an external label name (complicated for printing instructions)
 * and use the name without suffix everywhere else
 * */
char* elf_generate_ext_label_name(char* lblname)
{
   if (!lblname)
      return NULL; //Now that's done
   char* out = lc_malloc(strlen(lblname) + strlen(EXT_LBL_SUF) + 1);
   sprintf(out, "%s" EXT_LBL_SUF, lblname);

   return out;
}

static int elf_loaddriver_to_binfile(binfile_t* bf, elffile_t* efile);

/**
 * Loads the fields of a binfile_t structure from the results of the parsing of an ELF file
 * \param bf The binfile_t structure to fill
 * \param elf The structure containing the parsing of the ELF file. Must be of type ELF
 * \return EXIT_SUCCESS if the operation was successful, error code otherwise
 * */
static int elf_load_to_binfile(binfile_t* bf, Elf* elf)
{
   /**\todo TODO (2014-11-26) Create driver functions for each step in this function (setting format, setting type, etc)
    * Move everything into a generic function from la_binfile.c that will invoke those driver functions to fill itself
    * Have the binfile_load function from bfile_fmtinterface.c load the driver as well (and test if this works, well, everything)
    * This will ensure in particular that symbol sections are always loaded first and so on without having to think about it when
    * implementing a new format.*/
   assert(bf && elf);
   elffile_t* efile = elffile_new(elf);
   uint16_t i;

   // Setting up the format
   binfile_set_format(bf, BFF_ELF);

   //Type of the binary file
   switch (Elf_Ehdr_get_e_type(elf)) {
   case ET_EXEC:
      binfile_set_type(bf, BFT_EXECUTABLE);
      break;
   case ET_DYN:
      binfile_set_type(bf, BFT_LIBRARY);
      break;
   case ET_REL:
      binfile_set_type(bf, BFT_RELOCATABLE);
      break;
   default:
      binfile_set_type(bf, BFT_UNKNOWN);
      break;
   }

   uint8_t wordsize;
   // Setting up word size of the binary (32/64)*/
   switch (Elf_Ehdr_get_e_ident(elf)[EI_CLASS]) {
   case ELFCLASSNONE:
      wordsize = BFS_UNKNOWN;
      break;
   case ELFCLASS32:
      wordsize = BFS_32BITS;
      break;
   case ELFCLASS64:
      wordsize = BFS_64BITS;
      break;
   default:
      wordsize = BFS_UNKNOWN;
      break;
   }
   binfile_set_word_size(bf, wordsize);

   //Loading architecture
   binfile_set_arch(bf, getarch_bybincode(BFF_ELF, Elf_Ehdr_get_e_machine(elf)));

   /**\todo TODO - Retrieve the abi structure*/
   //bf->abi;
   //Loading the section headers
   if (Elf_Ehdr_get_e_shoff(elf) > 0)
      binfile_load_scn_header(bf, Elf_Ehdr_get_e_shoff(elf), 0,
            Elf_Ehdr_get_e_shentsize(elf) * Elf_Ehdr_get_e_shnum(elf),
            Elf_Ehdr_get_e_shentsize(elf),
            ((wordsize == BFS_64BITS) ?
                  (void*) elf64_getfullshdr(elf) :
                  (void*) elf32_getfullshdr(elf)));

   //Loading the segment headers
   if (Elf_Ehdr_get_e_phoff(elf) > 0)
      binfile_load_seg_header(bf, Elf_Ehdr_get_e_phoff(elf), 0,
            Elf_Ehdr_get_e_phentsize(elf) * Elf_Ehdr_get_e_phnum(elf),
            Elf_Ehdr_get_e_phentsize(elf),
            ((wordsize == BFS_64BITS) ?
                  (void*) elf64_getphdr(elf) : (void*) elf32_getphdr(elf)));
   binscn_t* segheader = binfile_get_seg_header(bf);

   //Loading segments
   uint16_t n_segments = Elf_Ehdr_get_e_phnum(elf);
   binfile_set_nb_segs(bf, n_segments);
   for (i = 0; i < n_segments; i++) {
      uint32_t flags = Elf_Phdr_get_p_flags(elf, i);
      //Computes the attribute of the segment
      uint8_t attrs = ((flags & PF_W) ? SCNA_WRITE : SCNA_NONE) /*Segment is accessible for writing*/
      | ((flags & PF_X) ? SCNA_EXE : SCNA_NONE) /*Segment is executable*/
      | ((flags & PF_R) ? SCNA_READ : SCNA_NONE) /*Segment is accessible for writing*/
      | ((Elf_Phdr_get_p_type(elf, i) == PT_TLS) ? SCNA_TLS : SCNA_NONE); /*Segment is TLS*/
      //Creates the segment
      binseg_t* seg = binfile_init_seg(bf, i, Elf_Phdr_get_p_offset(elf, i),
            Elf_Phdr_get_p_vaddr(elf, i), Elf_Phdr_get_p_filesz(elf, i),
            Elf_Phdr_get_p_memsz(elf, i), attrs, Elf_Phdr_get_p_align(elf, i));

      if (Elf_Phdr_get_p_type(elf, i) == PT_PHDR) {
         //The segment is the one containing the program header: associating it to the section representing the segment header
         assert(
               binscn_get_offset(segheader) == Elf_Phdr_get_p_offset(elf, i)
                     && binscn_get_size(segheader)
                           == Elf_Phdr_get_p_filesz(elf, i));
         //Updating the address of the segment header for coherence
         binscn_set_addr(segheader, Elf_Phdr_get_p_vaddr(elf, i));
      }
   }

   //Associates the segment header to any segment that could contain it
   binfile_addsection_tosegment(bf, segheader, NULL);

   //Loading sections
   uint16_t n_sections = Elf_Ehdr_get_e_shnum(elf);
   binfile_set_nb_scns(bf, n_sections);
   //These two arrays contain the indices of relocation sections and other sections respectively
   uint16_t relscns[n_sections];
   uint16_t norelscns[n_sections];
   uint16_t n_relscns = 0, n_norelscns = 0;

   //First loading the section containing section names
   elf_loadsection_to_binfile(bf, efile, Elf_Ehdr_get_e_shstrndx(elf));

   //First, loading the sections containing symbols
   for (i = 0; i < n_sections; i++) {
      uint16_t type = Elf_Shdr_get_sh_type(elf, i);
      if ((type == SHT_SYMTAB) || (type == SHT_DYNSYM))
         elf_loadsection_to_binfile(bf, efile, i);
      if (type == SHT_REL || type == SHT_RELA)
         relscns[n_relscns++] = i;
      else
         norelscns[n_norelscns++] = i;
   }
   //Handling the labels associated to label sections
   binfile_updatelabelsections(bf);

   //Now loads all other sections except the relocations, as they refer to another section
   for (i = 0; i < n_norelscns; i++) {
      elf_loadsection_to_binfile(bf, efile, norelscns[i]);
      /**\todo TODO (2014-11-21) The relocations are parsed last because I discovered a strange case where the sh_link field in a relocation
       * section header was set to 0. In this case, we will have to look for the target of the relocation based on its address, but for that
       * we will need all other sections to have been fully loaded. An alternate way to do it would be to partially load sections, or to trigger
       * the loading of a section based on its address instead of index, but this requires doing what binfile_lookup_scn_span_addr does using
       * only the content of the Elf structure, which is redundant. Find which is the more efficient/robust.*/
   }
   // Now the other sections (relocations only, then)
   for (i = 0; i < n_relscns; i++) {
      elf_loadsection_to_binfile(bf, efile, relscns[i]);
   }

   //Loads the driver in the binary file with the ELF specific functions
   elf_loaddriver_to_binfile(bf, efile);

   return EXIT_SUCCESS;
}

/**
 * Loads the fields of a binfile_t structure from the result of parsing an archive file
 * \param bf The binfile_t structure to fill
 * \param elf The structure containing the parsing of the ELF file. Must be of type archive
 * \param filestream The stream to the archive file
 * \return EXIT_SUCCESS if the operation was successful, error code otherwise
 * */
static int elf_binfile_load_from_ar(binfile_t* bf, Elf* elf, FILE* filestream)
{
   assert(elf && filestream);
   int i = 0;
   Elf* ar_o = NULL;
   int out = EXIT_SUCCESS;
   int res = EXIT_SUCCESS;
   unsigned int n_ar_elts = 0;

   // Setting up format
   binfile_set_format(bf, BFF_ELF);

   // Setting up file type
   binfile_set_type(bf, BFT_ARCHIVE);

   // Setting up number of members
   n_ar_elts = elf_get_ar_size(elf);
   binfile_set_nb_ar_elts(bf, n_ar_elts);

   while ((ar_o = elf_begin(fileno(filestream), ELF_C_READ, elf)) != NULL) {
      binfile_t* ar_elt = binfile_new(
            (elf_getname(ar_o) != NULL) ? elf_getname(ar_o) : "[no name]");
      res = elf_load_to_binfile(ar_elt, ar_o);
      if (!ISERROR(out) && res != EXIT_SUCCESS)
         out = res;
      binfile_set_ar_elt(bf, ar_elt, i++);
   }

   return out;
}

/*
 * Attempts to parse the debug information of a filled binfile_t structure.
 * \param bf A binary file structure containing a successfully parsed ELF file
 * \return A structure describing the debug information,
 * NULL if \c bf is NULL or not an ELF file
 * */
dbg_file_t* elf_binfile_parse_dbg(binfile_t* bf)
{
   if (!bf || (binfile_get_format(bf) != BFF_ELF) || !bf->driver.parsedbin)
      return NULL; //File not parsed or not ELF

   elffile_t* efile = binfile_get_parsed_bin(bf);

   //Attempts to parse DWARF format
   DwarfAPI* dwarf = dwarf_api_init_light(efile->elf, binfile_get_file_name(bf),
         NULL);
   if (dwarf) {
      return dbg_file_new(dwarf, DBG_FORMAT_DWARF);
   }
   //Add other formats here

   return dbg_file_new(NULL, DBG_NONE);
}

/*
 * Loads a binfile_t structure with the result of the parsing of an ELF file
 * \param bf A binary file structure containing an opened stream to the file to parse
 * \return EXIT_SUCCESS if the file could be successfully parsed as an ELF file, error code otherwise.
 * If successful, the structure representing the binary file will have been updated with the result of the parsing
 * */
int elf_binfile_load(binfile_t* bf)
{

   if (!bf)
      return ERR_BINARY_MISSING_BINFILE;
   char* filename = binfile_get_file_name(bf);
   FILE* filestream = NULL;
   if (filename != NULL) {
      filestream = fopen(filename, "r");
      if (filestream == NULL)
         return ERR_COMMON_UNABLE_TO_OPEN_FILE;
      binfile_set_filestream(bf, filestream);
   } else
      return ERR_COMMON_FILE_NAME_MISSING;

   Elf* elf = elf_begin(fileno(filestream), ELF_C_READ, NULL);
   switch (elf_kind(elf)) {
   case ELF_K_ELF:
      elf_load_to_binfile(bf, elf);
      break;
   case ELF_K_AR:
      elf_binfile_load_from_ar(bf, elf, filestream);
      break;
   default:
      elf_end(elf);
      fclose(filestream);
      return ERR_BINARY_FORMAT_NOT_RECOGNIZED;
      break;
   }

   return EXIT_SUCCESS;
}

// Functions used during patching //
////////////////////////////////////

int elf_binfile_writefile(binfile_t* bf, char* name)
{
   /**\todo (2014-10-06) May not be useful or only used for tests.*/

   return EXIT_FAILURE;
}

/*
 * Used for initialising the format specific internal structure of a file
 * \param bf A binary file in the process of being patched. Its \c creator and \c filestream members must be filled
 * \return EXIT_SUCCESS if successful, error cod eotherwise
 * */
int elf_binfile_patch_init_copy(binfile_t* bf)
{
   if (binfile_get_format(bf) != BFF_ELF)
      return ERR_BINARY_UNEXPECTED_FILE_FORMAT; //Making sure we are given something appropriate (surprising, isn't it?)
   if (!binfile_patch_is_patching(bf))
      return ERR_BINARY_FILE_NOT_BEING_PATCHED; //Making sure we are given something appropriate (surprising, isn't it?)
   if (!binfile_get_parsed_bin(bf))
      return ERR_BINARY_MISSING_BINFILE; //Making sure we are given something appropriate (surprising, isn't it?)
   elffile_t* origin = (binfile_get_driver(binfile_get_creator(bf)))->parsedbin;
   assert(origin);
   elffile_t* copy = elffile_new(
         elf_copy(origin->elf, binfile_get_file_stream(bf)));
   //Copies the indexes of the original to the copy
   memcpy(copy->indexes, origin->indexes, sizeof(*copy->indexes) * MAX_NIDX);
   //Flags the copy as such
   copy->flags |= ELFFILE_PATCHCOPY;
   //Initialises driver of the copy
   elf_loaddriver_to_binfile(bf, copy);
   return EXIT_SUCCESS;
}

/**
 * Retrieves the lowest start address for a loaded executable, depending on architecture and size
 * \param bf A parsed binary file
 * */
static int64_t elf_binfile_patch_findfirstloadableaddress(binfile_t* bf)
{
   /**\todo TODO (2015-02-24) This function should not be needed outside of this file (for the
    * computation of empty spaces). It needs to be updated depending on the architectures supported
    * (the information should be found in the respective ABI). Find something that makes it
    * more programmer-friendly to keep updated. Also, like everything I'm coding these days,
    * it could end up being unnecessary and be removed. Tough life*/
   assert(bf);
   uint8_t wsize = binfile_get_word_size(bf);
   switch (arch_get_code(binfile_get_arch(bf))) {
   default:
      HLTMSG(
            "[INTERNAL] Unable to retrieve minimal loadable address for architecture %s\n",
            arch_get_name(binfile_get_arch(bf)))
      ;
      break; //This is purely so that Eclipse does not complain about a missing break. Stupid Eclipse
   }
}

/**
 * Computes the first loaded address, taking into account the way page alignment impacts the loading of pages
 * \param bf A parsed binary file
 * \return Address of the first loaded address, or SIGNED_ERROR if it can not be computed or the file has no loaded section
 * */
int64_t elf_binfile_patch_get_first_load_addr(binfile_t* bf)
{
   if (!bf || binfile_get_nb_load_scns(bf) == 0)
      return SIGNED_ERROR;
   //Retrieves the address of the first loaded section
   int64_t addr = binscn_get_addr(binfile_get_load_scns(bf)[0]);
   //Now computes the address of the page to which it belongs
   return (addr - (addr % PAGE_SIZE));
}

/**
 * Computes the last loaded address, taking into account the way page alignment impacts the loading of pages
 * \param bf A parsed binary file
 * \return Address of the last loaded address, or SIGNED_ERROR if it can not be computed or the file has no loaded section
 * */
int64_t elf_binfile_patch_get_last_load_addr(binfile_t* bf)
{
   if (!bf || binfile_get_nb_load_scns(bf) == 0)
      return SIGNED_ERROR;
   binscn_t* lastscn = binfile_get_load_scns(bf)[binfile_get_nb_load_scns(bf) - 1];
   //Retrieves the end address of the last loaded section
   int64_t addr = binscn_get_addr(lastscn) + binscn_get_size(lastscn);
   //Now computes the end address of the page to which it belongs
   return (addr + PAGE_SIZE - (addr % PAGE_SIZE));
}

/**
 * Computes the empty spaces in a file.
 * \param bf The binary file. It must have been fully loaded
 * \todo TODO (2015-02-19) This function may be moved or changed or return an array/queue of intervals or something, or invoked
 * by another function or whatever. I'm really having trouble projecting.
 * => (2015-03-11) Doing this
 * \return Queue of empty spaces
 * */
queue_t* elf_binfile_build_empty_spaces(binfile_t* bf)
{
   /* What to do here :
    * - Each segment is considered to span between the closest addresses congruent to a page size and immediately superior (resp.
    * inferior) to its first and last address.
    * - Building intervals between those boundaries (beware of overlapping segments!)
    * - Now, inside a given segment, we identify if there are empty intervals between sections offsets, or memory addresses for ZERODATA sections,
    * or the beginning/end of the segment
    * => That one will be tough to do! Maybe postpone it for later as it raises the question on what we do with those intervals
    * -> We need to make the binfile_addemptyspace function (from la_binfile.c) more clever, in order to allow for empty spaces to be added
    * iteratively while checking for those that overlap
    * */
   int64_t lastaddress = 0;
   queue_t* out = queue_new();
   uint16_t i = 0;
   // First, building empty spaces based on segments
   //Retrieves the address of the first segment of non-null size (because sometimes there is a GNU_STACK segment with address and size to 0...
   while (i < binfile_get_nb_segments(bf)
         && binseg_get_msize(binfile_get_seg_ordered(bf, i)) == 0)
      i++;
   int64_t beginaddr = binseg_get_addr(binfile_get_seg_ordered(bf, i));
   if (beginaddr > 0) {
      //First segment has a positive address: we find the lowest possible address inferior to it
      int64_t firstbeginaddr = elf_binfile_patch_findfirstloadableaddress(bf); //Lowest possible address from which to start
      if (beginaddr > firstbeginaddr) {
         //The first segment begins after the lowest possible starting address
         //Finding the lowest possible address aligned on a page that is above the lowest allowed address
         int64_t firstalignedaddr = firstbeginaddr + PAGE_SIZE
               - (firstbeginaddr % PAGE_SIZE);
         if (firstalignedaddr < beginaddr) {
            //The lowest page-aligned starting address is below the first segment address
            //binfile_addemptyspace(bf, firstalignedaddr, beginaddr);
            queue_add_tail(out,
                  interval_new(firstalignedaddr,
                        (uint64_t) (beginaddr - firstalignedaddr)));
         } //Otherwise the first segment begins at the lowest page aligned address and there is no space before it
      } //Otherwise the first segment begins at the lowest possible address and there is no space before it
   }
   //Retrieves the ending address of the first segment of non null size
   lastaddress = binseg_get_end_addr(binfile_get_seg_ordered(bf, i));
   i++;

   //Scans all remaining segments to detect intervals between them
   while (i < binfile_get_nb_segments(bf)) {
      //Skips segments of null size
      while (i < binfile_get_nb_segments(bf)
            && binseg_get_msize(binfile_get_seg_ordered(bf, i)) == 0)
         i++;
      binseg_t* seg = binfile_get_seg_ordered(bf, i);
      if (binseg_check_attrs(seg, SCNA_TLS)) {
         i++;
         continue; //Skipping TLS segments as they don't work like anything else
      }
      /**\todo TODO (2015-02-24) Check that this works well with static patching*/
      int64_t segbegin = binseg_get_addr(seg);
      int64_t segend = binseg_get_end_addr(seg);
      if (segbegin <= lastaddress) {
         //This segment begins before the current ending address
         if (segend > lastaddress)
            lastaddress = segend; //Adjusting end address if segment ends after it
      } else {
         //This segment begins after the current ending address: creating a new interval
         //binfile_addemptyspace(bf, lastaddress, segbegin);
         queue_add_tail(out,
               interval_new(lastaddress, (uint64_t) (segbegin - lastaddress)));
         //Updating the current end address to skip to the end of the segment
         lastaddress = segend;
      }
      i++;
   }
   //Now adding an interval of infinite length after the last loaded address
   //binfile_addemptyspace(bf, lastaddress, -1);
   queue_add_tail(out, interval_new(lastaddress, UINT64_MAX));

   // Now, adding empty spaces based on spaces between sections
   /**\todo TODO (2015-02-25) Chickening out of it, as it would require binfile_addemptyspaces
    * to allow insertions of empty spaces anywhere, including overlapping existing spaces (see
    * also the TODO inside this function in la_binfile.c). If I get round to it later, add some
    * code here to detect spaces between sections inside segments*/
   return out;
}

/**
 * Function checking if a modified section can fit inside a given interval
 * */
interval_t* elf_binfile_patch_move_scn_to_interval(binfile_t* bf, uint16_t scnid,
      interval_t* interval)
{
   /**\todo TODO (2015-03-12) So far I let the function in la_binfile.c do the work, except for the special case of .got and .got.plt
    * In the future, we could check if there is an empty space between the section and
    * the one following it in the file large enough to contain the modified section and
    * use if it that's the case. This would allow to avoid moving some sections
    * => (2015-04-29) Done (I guess)*/

   elffile_t* efile = binfile_get_parsed_bin(bf);
   assert(efile); //I'm all for checking everything but if we arrive here with a NULL parsedbin something went very wrong
   interval_t* out = NULL;
   /*Special case: we have to handle the .got and the .got.plt.
    * If either one has been modified, we have to move both of them and keep them together*/
   if (((scnid == efile->indexes[GOT_IDX]) && (efile->indexes[GOTPLT_IDX] > -1))
         || ((scnid == efile->indexes[GOTPLT_IDX])
               && (efile->indexes[GOT_IDX] > -1))) {
      //The section is either the .got and there is a .got.plt, or it is the .got.plt
      assert(efile->indexes[GOTPLT_IDX] == (efile->indexes[GOT_IDX] + 1)); //The way I understood ELF, this should always happen if the .got.plt exists
      binscn_t* got = binfile_patch_get_scn_copy(bf, efile->indexes[GOT_IDX]);
      binscn_t* gotplt = binfile_patch_get_scn_copy(bf,
            efile->indexes[GOTPLT_IDX]);
      int64_t addralgn = 0;   //Value to add to the address to align it
      uint64_t fullsize = binscn_get_size(got) + binscn_get_size(gotplt); //Size to move
      uint64_t gotalign = binscn_get_align(got);
      assert(gotalign == binscn_get_align(gotplt)); //This should always be the case if I understood ELF correctly. I buy a round of beers the Friday I'm proved wrong

      /**\todo TODO (2015-03-31) The code below reuses part of the code from binfile_patch_move_scn_to_interval.
       * When (if) this is stabilised someday, find things to factorise between them. I'm fed up...*/
      if (gotalign > 0) {
         uint32_t intalign = interval_get_addr(interval) % gotalign;
         if (intalign > 0)
            addralgn = gotalign - intalign;
      }
      if (fullsize <= (interval_get_size(interval) + addralgn)) {
         //Both sections can fit inside the given interval: updating them
         int64_t newgotaddr = interval_get_addr(interval) + addralgn;
         binscn_set_addr(got, newgotaddr);
         binscn_set_addr(gotplt, newgotaddr + binscn_get_size(got)); //The .got.plt is set immediately after the .got
         binscn_add_attrs(got, SCNA_PATCHREORDER);
         binscn_add_attrs(gotplt, SCNA_PATCHREORDER);
         //Creating an interval representing the space where the sections have been moved
         out = interval_new(interval_get_addr(interval), fullsize + addralgn);
         DBG(
               FCTNAMEMSG("Section %s (%u) modified: forcing the relocation of section %s (%u) along with section %s (%u) to interval ", binscn_get_name(binfile_get_scn(bf, scnid)), scnid, binscn_get_name(got), binscn_get_index(got), binscn_get_name(gotplt), binscn_get_index(gotplt)); interval_fprint(out, stderr);STDMSG("\n");)
      }
   } else if (binscn_get_end_offset(binfile_get_scn(bf, scnid))
         < binscn_get_offset(binfile_get_scn(bf, scnid + 1))) {
      //There is some empty space between this section and the next one: no need to move it
      binscn_add_attrs(binfile_get_scn(bf, scnid), SCNA_PATCHREORDER);
      out = NULL;
   } else {
      //Convention: we signal the binfile_patch_move_scn_to_interval function in la_binfile.c that it must do the work in our stead
      out = interval;
   }
   return out;
}

/*
 * Adds an external library to a file being patched
 * \param bf The binary file (must be in the process of being patched)
 * \param extlibname Name of the library to add
 * \param priority Set to TRUE if it must have priority over the others, FALSE otherwise
 * \return EXIT_SUCCESS if successful, error code otherwise
 * */
int elf_binfile_patch_add_ext_lib(binfile_t* bf, char* extlibname,
      boolean_t priority)
{
//#if TODO_LATER_I_AM_FED_UP_AND_I_WANT_TO_COMPILE_THE_DISASSEMBLER_2014_07_11
   /**\todo TODO (2014-07-11) Hello! I changed some things in binfile since writing this function with regard to how duplication
    * is handled, and it's not over yet. Check what it does (especially for the thing about get_n_entries in the section from
    * the original file. Today I want to compile the disassembler so I don't have time for this.
    * (2014-11-06) Reactivating it but I still have no clue what it does. Also handle priority between libraries
    * (2015-05-28) Seems to be working, but the priority is not handled*/
   if (binfile_get_format(bf) != BFF_ELF)
      return ERR_BINARY_UNEXPECTED_FILE_FORMAT; //Making sure we are given something appropriate (surprising, isn't it?)
   if (!extlibname)
      return ERR_COMMON_FILE_NAME_MISSING; //Making sure we are given something appropriate (surprising, isn't it?)
   elffile_t* efile = binfile_get_parsed_bin(bf);
   if (!efile)
      return ERR_BINARY_MISSING_BINFILE; //And again
   if ((efile->indexes[DYNAMIC_IDX] <= 0) || (efile->indexes[DYNSYM_IDX] <= 0))
      return ERR_BINARY_NO_EXTLIBS; //No dynamic section
   /**\todo (2014-06-10) Later here we could see about inserting a dynamic section and turn a static exe into a dynamic one.
    * If we have nothing better to do.*/
   //Retrieving the corresponding section from the binary
   binscn_t* dynscn = binfile_patch_get_scn(bf, efile->indexes[DYNAMIC_IDX]);
//   //Same thing for the section containing the strings
//   binscn_t* dynsymscn = binfile_patch_get_scn_copy(bf, efile->indexes[DYNSTR_IDX]);

   //Creating an entry for the library name in the section containing dynamic strings
   data_t* libname = binfile_patch_add_str_entry(bf, extlibname,
         efile->indexes[DYNSTR_IDX]);

   //Inserting the library depending on its priority
   if (priority == FALSE) {
      //No priority over the others: we will simply add it at the end (using an existing duplicated NULL entry if available)
      //Trying to find if there are more than one NULL dynamic entry at the end
      uint32_t lastdynidx = binscn_get_nb_entries(dynscn) - 1;
      while ((data_get_type(
            binfile_patch_get_scn_entry(bf, efile->indexes[DYNAMIC_IDX],
                  lastdynidx)) == DATA_NIL) && (lastdynidx > 0))
         lastdynidx--;
      assert(lastdynidx < (binscn_get_nb_entries(dynscn) - 1)); //There MUST be a frigging NULL dynamic entry at the end or I quit

      //Retrieving the last entry
      data_t* lastent = binfile_patch_get_scn_entrycopy(bf,
            efile->indexes[DYNAMIC_IDX], lastdynidx + 1);

      //Updating the entry to turn it into a pointer to the name of the library
      data_upd_type_to_ptr(lastent, 0, 0, 0, libname, POINTER_NOADDRESS, TARGET_DATA);
      /**\todo (2014-06-11) WARNING! I set a size to 0 because it may not matter later. If it does, we need
       * to retrieve the size of an existing entry in the dynamic table to use it
       * (or we set as standard that if the size of a new entry is 0, it has to be the same as existing ones*/

      if (lastdynidx >= (binscn_get_nb_entries(dynscn) - 2)) {
         //There was only one NULL dynamic entry at the end: we need to add a new one at the end
         binfile_patch_add_entry(bf, data_new(DATA_NIL, NULL, 0),
               efile->indexes[DYNAMIC_IDX]);
         /**\todo (2014-06-11) WARNING: Same as above concerning the size of the new entry and its content*/
      } //There was more than 1 NULL dynamic entry: we used it to insert our new entry
   } else {
      //The new library must have priority over the others: we must insert it at the beginning of the array of dynamic entries
      /**\todo (2015-11-18) Currently the only easy way I say to do that is to duplicate all entries from the dynamic section,
       * so that we will not try to look for the original ones based on the entryid (since it will now be wrong). If at some
       * point we find something to avoid that, I'm all ears.*/

      HLTMSG("NOT IMPLEMENTED YET")
   }
//#endif
   return EXIT_SUCCESS;
}

/*
 * Rename an existing external library
 * \param bf The binary file (must be in the process of being patched)
 * \param oldname Name of the library to rename
 * \param newname New name of the library
 * \return EXIT_SUCCESS if successful, error code otherwise
 * */
int elf_binfile_patch_rename_ext_lib(binfile_t* bf, char* oldname, char* newname)
{
   /**\todo TODO (2014-11-05) Well, do it!*/
   if (binfile_get_format(bf) != BFF_ELF)
      return ERR_BINARY_UNEXPECTED_FILE_FORMAT; //Making sure we are given something appropriate (surprising, isn't it?)
   if (!oldname || !newname)
      return ERR_COMMON_FILE_NAME_MISSING; //Making sure we are given something appropriate (surprising, isn't it?)
   elffile_t* efile = binfile_get_parsed_bin(bf);
   if (!efile)
      return ERR_BINARY_MISSING_BINFILE; //And again
   if ((efile->indexes[DYNAMIC_IDX] <= 0) || (efile->indexes[DYNSYM_IDX] <= 0))
      return ERR_BINARY_NO_EXTLIBS; //No dynamic section
   //Retrieving the corresponding section from the binary
   binscn_t* dynscn = binfile_patch_get_scn(bf, efile->indexes[DYNAMIC_IDX]);

   //Creating an entry for the library name in the section containing dynamic strings
   data_t* libname_entry = binfile_patch_add_str_entry(bf, newname,
         efile->indexes[DYNSTR_IDX]);
   /**\note (2015-11-18) It would be tempting to simply modify the string entry containing the name of
    * the library to rename, but there is no guarantee that it's not used by some other entry elsewhere,
    * so we create a new entry just to be sure. We could have fun later adding a mechanism when writing
    * the file that detects all unused string entries and removes them
    * */

   //Now looking up for the entry containing the name of this external library
   uint16_t i;
   for (i = 0; i < binscn_get_nb_entries(dynscn); i++) {
      data_t* entry = binfile_patch_get_scn_entry(bf, efile->indexes[DYNAMIC_IDX],
            i);
      char* libname = data_get_string(
            pointer_get_data_target(data_get_pointer(entry)));
      /**\todo (2015-11-18) To be completely foolproof, I'd have to check here that the entry
       * really is an external library and not some random string that would be present in the dynamic
       * section and miraculously be identical to the name of an external library. The trick is that
       * I would have to retrieve the array of entries from the section, which depends on the word size
       * of the file, and it's a pain. After that, we simply need to check whether i is smaller than the
       * size of the array in the original file (since, by that time, the ELF array of the patched file has
       * not been built yet), and check if it's a DT_NEEDED*/
      if (str_equal(libname, oldname)) { //Found the entry containing the name of the library to rename
         //Duplicating this entry (if it was not already)
         data_t* newentry = binfile_patch_get_scn_entrycopy(bf,
               efile->indexes[DYNAMIC_IDX], i);
         //Changing the target of the pointer in the duplicated entry to point to the new library name
         pointer_t* ptr = data_get_pointer(newentry);
         pointer_set_data_target(ptr, libname_entry);
         return EXIT_SUCCESS;
      }
   }
   return ERR_BINARY_EXTLIB_NOT_FOUND;
}

/**
 * Adds a label to the file. The label is to be added in the specified symbol section
 * \param bf The binary file (must be in the process of being patched)
 * \param efile The associated structure representing the parsed ELF file for the copy
 * \param label Label to add. Its type will must be set to allow identifying how and where to insert it
 * \param symscnid Index of the section containing symbols
 * \return EXIT_SUCCESS if successful, error code otherwise
 * */
static int elf_binfile_patch_add_label_tosymscn(binfile_t* bf, elffile_t* efile,
      label_t* label, uint16_t symscnid, uint16_t symstrscnid)
{
   assert(bf && label && (symscnid < bf->n_sections));
   binscn_t* symscn = binfile_patch_get_scn_copy(bf, symscnid);
   //Retrieves the size of entries in the section
   uint32_t lblentsz = binscn_get_entry_size(symscn);
   //Creates an entry representing the label
   data_t* lblent = data_new(DATA_LBL, label, lblentsz);
   //Add the entry to the patched file
   int out = binfile_patch_add_entry(bf, lblent, symscnid);
   if (ISERROR(out)) {
      //Error occurred when attempting to insert an entry
      ERRMSG(
            "Unable to insert symbol entry for label %s at address %#"PRIx64"\n",
            label_get_name(label), label_get_addr(label));
      data_free(lblent);
      return out;
   }
   //Add the corresponding string to the associated string table
   data_t* strent = binfile_patch_add_str_entry(bf, label_get_name(label),
         symstrscnid);
   if (!strent) {
      //Error occurred when attempting to insert string entry for the name of the label
      ERRMSG(
            "Unable to insert string entry for name of label %s at address %#"PRIx64"\n",
            label_get_name(label), label_get_addr(label));
      data_free(lblent);
      out = binfile_get_last_error_code(bf);
      if (!ISERROR(out))
         return ERR_BINARY_FAILED_INSERTING_STRING;
      else
         return out;
   }
   //Creates a label targeting the string entry
   pointer_t* ptrstr = pointer_new(0, 0, strent, POINTER_ABSOLUTE, TARGET_DATA);
   //Links the entry representing the label to the pointer to the string entry
   assert(efile->symnames);
   hashtable_insert(efile->symnames, lblent, ptrstr);
   /**\todo TODO (2014-09-30) See whether we do that for ALL labels or only those added during patching
    * => (2015-04-22) I think we did*/

   if (symscnid == efile->indexes[DYNSYM_IDX]
         && efile->indexes[VERSYM_IDX] > -1) {
      //Updates the version informations table
      binscn_t* versymscn = binfile_patch_get_scn_copy(bf,
            efile->indexes[VERSYM_IDX]);
      data_t* newversym = data_new_imm(binscn_get_entry_size(versymscn),
            VER_NDX_GLOBAL);   //Adding a global entry
      binfile_patch_add_entry(bf, newversym, efile->indexes[VERSYM_IDX]);
      if (binscn_get_nb_entries(versymscn) != binscn_get_nb_entries(symscn))
         WRNMSG(
               "[INTERNAL] Version symbol table size mismatch (%d for %d entries)\n",
               binscn_get_nb_entries(versymscn), binscn_get_nb_entries(symscn));
   }

   return EXIT_SUCCESS;
}

/*
 * Adds a label to the file
 * \param bf The binary file (must be in the process of being patched)
 * \param label Label to add. Its type will must be set to allow identifying how and where to insert it
 * \return EXIT_SUCCESS if successful, error code otherwise
 * */
int elf_binfile_patch_add_label(binfile_t* bf, label_t* label)
{
   /**\todo (2014-09-29) See todo in la_binfile.c:binfile_patch_add_label with regard to the tests on file being patched*/
   if (!binfile_patch_is_patching(bf))
      return ERR_BINARY_FILE_NOT_BEING_PATCHED;
   if (!label)
      return ERR_LIBASM_LABEL_MISSING;
   if (binfile_get_format(bf) != BFF_ELF)
      return ERR_BINARY_UNEXPECTED_FILE_FORMAT;  //Not an ELF file: exiting
   elffile_t* efile = binfile_get_parsed_bin(bf);
   if (!efile)
      return ERR_BINARY_MISSING_BINFILE; //No binary file: exiting
   int16_t symscnid, symstrscnid;
   if (label_get_type(label) == LBL_EXTFUNCTION) {
      //External label: we create this in the section for dynamic labels
      symscnid = efile->indexes[DYNSYM_IDX];
      symstrscnid = efile->indexes[DYNSTR_IDX];
   } else {
      symscnid = efile->indexes[SYMTAB_IDX];
      symstrscnid = efile->indexes[STRTAB_IDX];
   }

   if (symscnid < 0)
      return ERR_BINARY_NO_SYMBOL_SECTION; //No symbol section: exiting
   /**\todo (2014-09-29) For a future evolution: we could *create* such a section if it does not exist*/
   /**\todo TODO (2015-05-27) Now that elf_binfile_patch_add_label_tosymscn is not invoked separately
    * from elf_x86_64_binfile_patch_add_ext_fct_lazybinding, we could merge this function with this one*/

   return elf_binfile_patch_add_label_tosymscn(bf, efile, label, symscnid,
         symstrscnid);
}

/*
 * Adds a new section to a file. If the address of the section is set it must use it
 * \param bf The binary file (must be in the process of being patched)
 * \param scn The section to add
 * \return EXIT_SUCCESS if successful, error code otherwise
 * */
int elf_binfile_patch_add_scn(binfile_t* bf, binscn_t* scn)
{
   if (!bf)
      return ERR_BINARY_MISSING_BINFILE;
   if (!scn)
      return ERR_BINARY_MISSING_SECTION;
   elffile_t* efile = binfile_get_parsed_bin(bf);
   if (!efile)
      return ERR_BINARY_MISSING_BINFILE;
   /**\todo TODO (2014-10-09) Don't forget to add the name of the section to the appropriate section*/
   uint16_t scnstr = efile->indexes[SHSTRTAB_IDX];
   if (scnstr <= 0)
      return ERR_BINARY_NO_STRING_SECTION; //No section containing names of sections

   //Adds a string entry equal to the section name
   data_t* strent = binfile_patch_add_str_entry(bf, binscn_get_name(scn), scnstr);

   //Creates a pointer to the string entry
   pointer_t* ptrstr = pointer_new(0, 0, strent, POINTER_NOADDRESS,
         TARGET_DATA);
   /**\todo TODO (2014-10-13) Add an offset here (update binfile_patch_add_str_entry to return it)*/

   //Links the section to the pointer
   hashtable_insert(efile->scnnames, scn, ptrstr);

   //Adds an entry to the section header
   binscn_t* scnhdr = binfile_get_scn_header(bf);
   binfile_patch_add_entry(bf, data_new_raw(binscn_get_entry_size(scnhdr), NULL),
         BF_SCNHDR_ID);
   /**\todo TODO (2015-04-24) I've yet to decide if this should be in the libbin (with a test over whether the header exists) or here
    * Whatever I choose, there should really be some mechanism to avoid retrieving the entry size for this table...
    * Additionally, we could already duplicate the ELF section header to put it into the data element, or somehow link the new entry
    * to the new section, but right now I mainly use this for catching the increase in size of the header*/

   /**\todo TODO (2015-05-05) Check if we need here to affect an alignment to the section based on those from the file. Alignment of code
    * sections (SCNT_CODE) could be identical to the alignment of the .text section, and alignment of data sections (SCNT_DATA) could be made identical
    * to the alignment of the .data or .rodata section. For now they are set statically (16 for SCNT_CODE, 8 for SCNT_DATA).
    * Beware in that case of sections that must receive addresses used for alignment, and of various architectures*/
   if (binscn_get_type(scn) == SCNT_CODE) {
      //Code section: setting its alignment to 16
      binscn_set_align(scn, 16);
   } else if (binscn_get_type(scn) == SCNT_DATA) {
      //Code section: setting its alignment to 8
      binscn_set_align(scn, 8);
   }

   return EXIT_SUCCESS;
}

/*
 * Adds a new segment to a file. If the address of the segment is set it must use it
 * \param bf The binary file (must be in the process of being patched)
 * \param seg The segment to add
 * \return EXIT_SUCCESS if successful, error code otherwise
 * */
int elf_binfile_patch_add_seg(binfile_t* bf, binseg_t* seg)
{
   if (!bf)
      return ERR_BINARY_MISSING_BINFILE;
   if (!seg)
      return ERR_BINARY_MISSING_SEGMENT;
   //Adds an entry to the segment header
   binscn_t* seghdr = binfile_get_seg_header(bf);
   binfile_patch_add_entry(bf, data_new_raw(binscn_get_entry_size(seghdr), NULL),
         BF_SEGHDR_ID);
   /**\todo TODO (2015-04-24) I've yet to decide if this should be in the libbin (with a test over whether the header exists) or here
    * Whatever I choose, there should really be some mechanism to avoid retrieving the entry size for this table...
    * Additionally, we could already duplicate the ELF segment header to put it into the data element, or somehow link the new entry
    * to the segment section, but right now I mainly use this for catching the increase in size of the header*/

   return EXIT_SUCCESS;
}

pointer_t* elf_arm_binfile_patch_add_ext_fct(binfile_t* bf, char* fctname,
      char* libname, int preload)
{
   (void) bf;
   (void) fctname;
   (void) libname;
   (void) preload;
   ERRMSG(
         "Insertion of external function calls not supported for ARM in the current version\n");
   return NULL;
}

/**
 * Computes the new offset at which a patched section must be moved
 * \param scn A patched section
 * \param newoff The current offset at which we are moving sections
 * \param forcealign Alignment to impose on the relation between section offset and virtual address. If
 * set to 0, will be computed from the segments to which the section belongs
 * \return The offset at which the section must be moved
 * */
static uint64_t elf_binscn_patch_findnewoffset(binscn_t* scn, uint64_t newoff,
      uint64_t forcealign)
{
   /**\todo TODO (2015-04-08) This function may be moved to la_binfile.c if it turns out not to be too ELF-specific
    * and not to need anything from elsewhere*/
   uint16_t j;
   uint64_t align = 0;
   uint64_t newoffset = newoff;
   if (forcealign != 0)
      align = forcealign;
   else if (binscn_get_nb_binsegs(scn) == 0)
      align = NEW_SEGMENT_ALIGNMENT; //Section not tied to a segment: it will be a new segment, so the new alignemnt
   else {
      //Retrieves the maximum alignment of the segment to which the section belongs
      for (j = 0; j < binscn_get_nb_binsegs(scn); j++) {
         binseg_t* seg = binscn_get_binseg(scn, j);
         uint64_t sgalgn = binseg_get_align(seg);
//            if (sgalgn > NEW_SEGMENT_ALIGNMENT) {
//               //The segment alignment is larger than the max value we accept: updating the segment alignment
//               binseg_set_align(seg, NEW_SEGMENT_ALIGNMENT);
//               sgalgn = NEW_SEGMENT_ALIGNMENT;
//            }
         if (sgalgn > align)
            align = sgalgn;
      } //Now align contains the maximum alignment among the alignments of segments containing this section
   }
   //Finding the closest offset obeying the constraints of alignment between address and offset

   maddr_t scnaddr = binscn_get_addr(scn);   //Address of the current section

   uint64_t offalign = newoffset % align; //Position of the offset with regard to the alignment of the segment
   uint64_t addralign = scnaddr % align; //Position of the address with regard to the alignment of the segment

   if (offalign < addralign)
      newoffset += addralign - offalign; //Increasing the offset to be aligned with the address
   else if (offalign > addralign)
      newoffset += align + addralign - offalign; //Increased the offset to the next page and be aligned with the address
   //Otherwise the offset is already aligned with the address and there is nothing to do

   DBGMSG(
         "Section %s (%d) will be relocated from offset %#"PRIx64" to offset %#"PRIx64" to respect alignment of %#"PRIx64" with new address %#"PRIx64"\n",
         binscn_get_name(scn), binscn_get_index(scn), binscn_get_offset(scn),
         newoffset, align, scnaddr);

   return newoffset;
}

/*
 * Finalises a binary file being patched by building its format specific structure (elffile_t and Elf)
 * \param bf The binary file (must be in the process of being patched)
 * \param emptyspaces Queue of interval_t structures representing the available empty spaces in the file
 * \return EXIT_SUCCSS if successful, error code otherwise
 * */
int elf_binfile_patch_finalise(binfile_t* bf, queue_t* emptyspaces)
{
   /*Hello! I am the successor to elfexe_scnreorder. Quake in fear, mortals*/
   /**\note (2014-10-27) We are considering that the binfile contains:
    * - All sections to be added are present in the file. They can be identified by their index superior to the number of sections in the creator
    * - All modifications to the ELF file are done (added labels, etc).
    * - Sizes of code and data sections are close to their final values
    *
    * We consider that added sections can be the following:
    * - Code
    * - Fixed code
    * - Data
    * - Plt (added by this elf_x86_64_binfile_patch_add_ext_fct)
    *
    * */
   if (binfile_get_format(bf) != BFF_ELF)
      return ERR_BINARY_UNEXPECTED_FILE_FORMAT; //Making sure we are given something appropriate (surprising, isn't it?)
   if (!binfile_patch_is_patching(bf))
      return ERR_BINARY_FILE_NOT_BEING_PATCHED; //Making sure we are given something appropriate (surprising, isn't it?)
   if (!binfile_get_parsed_bin(bf))
      return ERR_BINARY_MISSING_BINFILE; //Making sure we are given something appropriate (surprising, isn't it?)
   int out = EXIT_SUCCESS;

   /**\note This function will also edit the Elf* in bf->origin to update the segment table
    * It will also update the offsets of sections
    * It also needs to update the array of indexes in bf (the copy, not the original)
    * */
   //uint16_t n_segs; //Number of segments
   elffile_t* efile = binfile_get_parsed_bin(bf);
   elffile_t* efile_creator =
         binfile_get_driver(binfile_get_creator(bf))->parsedbin;

   //Initialises the Elf file structure with the correct number of sections and segments
   /**\note (2014-10-08) Unless I change my mind later, we don't fill the sections now, as some values may change
    * (for instance with the patcher updating the code or adding labels or whatever). This will be done in elf_binfile_patch_write_file*/

   /****
    * What the bloody algorithm for this function appears to be
    * - Create new label at the beginning of the patched section
    * - Retrieve TLS segment
    * - [static] Detect elements that must add an entry to the .got and add it
    * - [static] Look for uninitialised entries in the symtab
    * - [static] If there a GNU_IFUNC, add entries to the .got.plt and .plt and initialise or create the .rela.plt
    * - Initialises mapping segment to section (may be do it during parsing and store it in the efile_t)
    * - Updates segment alignment
    * - Adds a page after last section (should already be available)
    * - Identifies sections to move
    * - Redistributes moved sections. TODO Find the algorithm for this. [static] New sections at the end
    * - Maps moved sections to segments
    * - [static] Retrieve segment containing bss and note where to add the TLS
    * - [static] If there is a .tls section, update or add the TLS segment
    * - [static] Adds the .tdata section
    * - Add new sections. Align the one containing data
    * - Add moved sections
    * - [static] Add sections from inserted object files
    * - [static] Performs GNU_IFUNC relocations
    * - [static] Updates size of .rela.plt
    * - [static] Merge plt from inserted files to the .madras.plt if there is one
    * - Updates segment size
    * - Updates labels
    * - Updates segments
    * - Updates ELF header
    * - Updates segment
    * - [static] Add TLS segment if needed
    * - Moves PHDR
    * - [static] Move .madras.tls and .tbss
    *
    * New bloody algorithm or attempt at it
    * - Do all stuff needed by the static patching
    * - Do all stuff such as adding new labels at the beginning of patched section (if we still need it)
    * - Identify all sections that changed size and the new sections
    * - Identify EXISTING holes (I will not reuse the space left by moved sections, instead attempt to keep their code there)
    * - Start filling these holes with the following priorities:
    *   - Sections referenced by instructions must remain at <max reference> distance from the original code
    *   - Sections with identical rights must be grouped together
    * - Updates segments (alignment, etc)
    *
    * All new version (2015-03-23)
    * - Retrieve all sections that were modified and not already reordered
    * - Affect them an address in the offered intervals
    *   - A new NOBITS section must always be last
    *   - An existing NOBITS section should not be moved, but then again it would cause all sort of troubles if we were to change it
    *     with the way we handle intervals, so I guess we would have to avoid this, except for TLS
    * - Retrieve the offset following the last loaded section
    * - Order all modified or added sections by address
    * - Scan all modified sections, and affect them an offset following the last offset and congruent to 0x1000
    * - Affect a new segment to the sections. If the address between them is bigger than the offset, create a new segment
    * - Reorder the sections in the array
    * - Create the ELF segments and update their addresses
    * - Return and freak out when you realise that it does not cover a case you did not think about
    *
    * ***/
   binfile_t* creator = binfile_get_creator(bf);
   //Identifies loaded sections that have changed sizes
   uint32_t i, n_chgscns = 0;
   uint16_t n_loadscns = binfile_get_nb_load_scns(bf);
   /*Builds an array of modified sections**/
   binscn_t* chgscns[n_loadscns]; //Array of modified loaded sections (initialised to max possible size)
   /**\todo TODO (2015-03-27) I'm going back and forth between storing the sections or their indices. When this is settles,
    * pick up the one that is the most convenient*/
   //Initialises array to NULL
   memset(chgscns, 0, sizeof(*chgscns) * n_loadscns);
   //Scans the list of sections in the patched file and store those that changed
   for (i = 0; i < n_loadscns; i++) {
      binscn_t* scn = binfile_get_load_scn(bf, i);
      if (binscn_patch_is_bigger(scn)) {
         //Section has been modified from the original
         if (!binscn_check_attrs(scn, SCNA_PATCHREORDER)) {
            //Section has not been already relocated
            if (binscn_get_index(scn) == efile->indexes[GOTPLT_IDX]) {
               //Special case: the .got.plt section was modified, we must also move the .got section
               assert(efile->indexes[GOT_IDX] > -1); //If there is a .got.plt, there should always be a .got. Or maybe that's only for x86 and arm does what it likes
               assert(
                     (i > 0)
                           && (binscn_get_index(binfile_get_load_scn(bf, i - 1))
                                 == efile->indexes[GOT_IDX])); //The previous loaded section must be the .got
               binscn_t* gotscn = binfile_patch_get_scn_copy(bf,
                     efile->indexes[GOT_IDX]);
               if (chgscns[n_chgscns - 1] != gotscn) {
                  //Forces the .got section to be moved if it was not already
                  chgscns[n_chgscns++] = gotscn;
                  DBGMSG(
                        "Section %s (%d) forced to be relocated because the section %s (%d) is\n",
                        binscn_get_name(gotscn), binscn_get_index(gotscn),
                        binscn_get_name(scn), binscn_get_index(scn));
               }
            }
            chgscns[n_chgscns++] = scn;
            DBGMSG(
                  "Section %s (%d) has changed size and needs to be relocated\n",
                  binscn_get_name(scn), binscn_get_index(scn));
            if (binscn_get_index(scn) == efile->indexes[GOT_IDX]) {
               //Special case: the .got section was modified, we must also move the .got.plt section
               if (efile->indexes[GOTPLT_IDX] > -1) {
                  //There is a .got.plt: we will force it to be modified
                  assert(
                        ((i + 1) < n_loadscns)
                              && (binscn_get_index(binfile_get_load_scn(bf, i + 1))
                                    == (uint16_t )efile->indexes[GOTPLT_IDX])); //If the .got.plt exists, it is immediately after the .got
                  binscn_t* gotpltscn = binfile_patch_get_scn_copy(bf,
                        efile->indexes[GOTPLT_IDX]);
                  chgscns[n_chgscns++] = gotpltscn;
                  DBGMSG(
                        "Section %s (%d) forced to be relocated because section %s (%d) is\n",
                        binscn_get_name(gotpltscn), binscn_get_index(gotpltscn),
                        binscn_get_name(scn), binscn_get_index(scn));
                  i++;  //Skipping the .got.plt
               }
            }
         }
      }
   } //The array of sections should be ordered by address, as the array of loaded sections is and those are section that were not reordered
   /**\todo TODO (2015-03-27) The loop below reuses some of the code from patchfile_reservemeptyspaces_refscns in libmpatch.c
    * When everything is stabilised, find a way to factorise it.
    * Also, here I delete the spaces that I use, while I flag them in libmpatch.c. A common behaviour must be set (I'm partial
    * to deleting the used spaces instead of flagging them, but I wait until the refactoring is complete to check whether I won't
    * be using it later). If we keep the deletion, binfile_patch_move_scn_to_interval could simply return an address to avoid returning
    * an interval that has to be deleted afterward*/
   for (i = 0; i < n_chgscns; i++) {
      list_t* iter = queue_iterator(emptyspaces);
      while (iter) {
         interval_t* used = binfile_patch_move_scn_to_interval(bf,
               binscn_get_index(chgscns[i]), GET_DATA_T(interval_t*, iter));
         if (binscn_check_attrs(chgscns[i], SCNA_PATCHREORDER)) {
            if (used) {
               //The section used at least part of the interval: updating the list of intervals
               if (interval_get_end_addr(used)
                     == interval_get_end_addr(GET_DATA_T(interval_t*, iter))) {
                  //The section used the whole interval: we remove it
                  list_t* next = iter->next;
                  interval_free(queue_remove_elt(emptyspaces, iter));
                  iter = next;
                  //continue;
               } else {
                  //The section used part of the interval: we split it
                  //Resizing the interval so that it now begins at the end of the used interval
                  interval_upd_addr(GET_DATA_T(interval_t*, iter),
                        interval_get_end_addr(used));
               }
               interval_free(used);
            }
            break; //Section was successfully reordered: no need to keep looking
         }
         iter = iter->next;
      }
      if (!binscn_check_attrs(chgscns[i], SCNA_PATCHREORDER)) {
         ERRMSG("Unable to find a space to relocate section %d (%s)\n",
               binscn_get_index(chgscns[i]), binscn_get_name(chgscns[i]));
         out = ERR_BINARY_SECTION_NOT_RELOCATED;
      }
   }
   /**\todo TODO (2015-05-05) See the TODO in libmpatch.c that follows the invocation of binfile_patch_finalise. If we allow
    * the patcher to relocated the code and data sections after the other binary sections have been relocated, we could have to
    * exit here to allow for the patcher to move them. Then we would come back at this point to affect the new offsets and segments.
    * This TODO is rather important because I guess we will have trouble when the ranges of available intervals are reduced because
    * of large sections of code and/or data.*/

   //At this point, we have updated the addresses of all sections. We now have to reaffect their offsets
   uint16_t lastloadscnid = 0;
   uint16_t n_sections_creator = binfile_get_nb_sections(creator);
   //Find the loaded section with the higher index in the original file
   for (i = 0; i < n_sections_creator; i++) {
      if (binscn_check_attrs(binfile_get_scn(creator, i), SCNA_LOADED))
         lastloadscnid = i;
   }
   DBGMSG("Last loaded section in file %s is at index %d\n",
         binfile_get_file_name(creator), lastloadscnid);

   //Retrieving the offset from which we will start adding the new sections, arbitrarily set after the last loaded section
   uint64_t newoffset;
   if (lastloadscnid == (n_sections_creator - 1)) {
      //The last loaded section is the last section in the file: starting from its end
      binscn_t* lastscn = binfile_get_scn(creator, lastloadscnid);
      newoffset = binscn_get_offset(lastscn) + binscn_get_size(lastscn);
   } else {
      //Otherwise, use the starting offset of the following (unloaded) section
      newoffset = binscn_get_offset(binfile_get_scn(creator, lastloadscnid));
   }
   DBGMSG("Relocating sections after offset %#"PRIx64"\n", newoffset);

   //Now build an array of all moved sections
   //Reinitialises array to NULL
   n_chgscns = 0;
   memset(chgscns, 0, sizeof(*chgscns) * n_loadscns);
   //Scans the list of loaded sections in the patched file and store those that must be moved
   for (i = 0; i < n_loadscns; i++) {
      binscn_t* scn = binfile_get_load_scn(bf, i);
      if (binscn_patch_is_moved(scn)) {
         DBGMSGLVL(1,
               "Section %s (%d) has been moved and will be given a new offset \n",
               binscn_get_name(scn), binscn_get_index(scn));
         chgscns[n_chgscns++] = scn;
      }
   }
   //Reordering the array of moved sections by their new addresses
   qsort(chgscns, n_chgscns, sizeof(*chgscns), binscn_cmp_by_addr_qsort);

   /**\todo TODO (2015-03-27) Ensure that the TLS sections are grouped together (address-wise). If a ZERODATA TLS section
    * exists, it must be the last of the TLS sections*/
   /**\todo TODO (2015-03-30) Ensure that the (non TLS ) ZERODATA sections are last address-wise*/

   //Updates the alignment constraint of all segments to not be greater than NEW_SEGMENT_ALIGNMENT
   for (i = 0; i < binfile_get_nb_segments(bf); i++) {
      binseg_t* seg = binfile_get_seg(bf, i);
      if (binseg_get_align(seg) > NEW_SEGMENT_ALIGNMENT)
         binseg_set_align(seg, NEW_SEGMENT_ALIGNMENT);
   }
   /**\todo TODO (2015-04-02) We could do the adjustment above only if the segment header must be moved*/

   //Now affect new offsets to all displaced sections based on their addresses and obeying the congruence with their offset
   /*
    * (2015-03-27) New code to write here
    * - Create a new segment
    * - For each moved section
    *   - Retrieve the max alignment of the segments to which it belongs
    *   - If this alignment is above the new page size, we reset the alignment to the page size
    *   - Compute the offset of the section from its new address and the alignment of the segment
    *   - Create a new segment if:
    *     - If the current section is TLS and not the previous one
    *     - If the current section is not TLS and the previous one is not
    *     - If the previous section is NOBITS
    *     - If the original file contains a segment between the previous section and the current one
    *   - Otherwise associate the current section to the current segment
    * - (If all the segments to which the section is associated are associated to sections that have been moved,
    *   we don't associate the section with the current segment)
    * - Update the offsets of the last unloaded sections
    * - Update the program header and move it.
    * - Update the offsets of all sections if the program header changed them
    * - Reorder the array based on their offsets (this could be done by a function from binfile, simply reorder on offsets and make sure section 0 is first)
    * - Update the offsets and memory sizes of the segments
    * */
   //New segment
   binseg_t* currentseg = NULL;
   /**\todo TODO (2015-04-09) Here, add the Elf_Phdr structure representing the segment to the segment header, with
    * the appropriate type. This could even be used as a parameter for binfile_patch_add_seg but I have to implement
    * the modification by patching of the sections representing the section and segment header with the similar mechanism
    * as for the other sections (just find some way to use the index or something)
    * => (2015-05-07) Has been done by invoking the format-specific function in binfile_patch_add_seg*/

   for (i = 0; i < n_chgscns; i++) {
      //Detects if we need to change the current segment
      if (i > 0) {
         /*We create a new segment if the current section is TLS while the previous one is not,
          * if the current section is not TLS while the previous one is,
          * or if the previous section contains uninitialised data
          * or if there is an existing segment between the end of the previous section and the current one*/
         if ((binscn_check_attrs(chgscns[i], SCNA_TLS)
               && !binscn_check_attrs(chgscns[i - 1], SCNA_TLS))
               || (!binscn_check_attrs(chgscns[i], SCNA_TLS)
                     && binscn_check_attrs(chgscns[i - 1], SCNA_TLS))
               || (binscn_get_type(chgscns[i]) == SCNT_ZERODATA)
               || binfile_lookup_seg_in_interval(bf,
                     binscn_get_addr(chgscns[i]),
                     binscn_get_end_addr(chgscns[i - 1]))) {
            currentseg = binfile_patch_add_seg(bf,
                  SCNA_READ | SCNA_WRITE | SCNA_EXE, NEW_SEGMENT_ALIGNMENT);
         } else {
            //We are in the same segment: the address and offsets must keep the same relative distance
            newoffset += (binscn_get_addr(chgscns[i])
                  - binscn_get_end_addr(chgscns[i - 1]));
         }
         /**\todo TODO (2015-04-01) Check if we don't need to add a page in the case of SCNT_ZERODATA.
          * The intervals and addresses should have taken care of that but I'm a bit fuzzy on the details right now*/
      } else {
         //This is the first reordered section: initialising the new segment
         currentseg = binfile_patch_add_seg(bf,
               SCNA_READ | SCNA_WRITE | SCNA_EXE, NEW_SEGMENT_ALIGNMENT);
      }
      //Finding the offset congruent to the section address
      newoffset = elf_binscn_patch_findnewoffset(chgscns[i], newoffset, 0);

      //Updating the offset of the section
      binscn_set_offset(chgscns[i], newoffset);

      //Updating the offset with the size of the section if it does not contain uninitialised data
      if (binscn_get_type(chgscns[i]) != SCNT_ZERODATA)
         newoffset += binscn_get_size(chgscns[i]);

      if (binscn_check_attrs(chgscns[i], SCNA_TLS))
         binseg_add_attrs(currentseg, SCNA_TLS); //Flag the new segment as TLS if we are handling TLS sections

      //Removes the section from the segments to which it belongs, unless the segment contains only one section (this one)
      uint16_t j = 0;
      while (j < binscn_get_nb_binsegs(chgscns[i])) {
         binseg_t* seg = binscn_get_binseg(chgscns[i], j);
         if (binseg_get_nb_scns(seg) > 1)
            binseg_rem_scn(seg, chgscns[i]);
         else
            j++;
      }

      //Associates the section to the current segment
      binfile_addsection_tosegment(bf, chgscns[i], currentseg);
   } //Now all moved loaded sections have been updated with their new offset and new segment

   //Updating the offsets of the remaining unloaded sections
   for (i = lastloadscnid + 1; i < binfile_get_nb_sections(creator); i++) {
      binscn_t* scn = binfile_get_scn(bf, i);
      binscn_set_offset(scn, newoffset);
      newoffset += binscn_get_size(scn);
   }
   //Retrieves the section header
   binscn_t* scnhdr = binfile_get_scn_header(bf);
   //Retrieves the program header
   binscn_t* seghdr = binfile_get_seg_header(bf);

   //Updating the offset of the section header to be at the end of the file
   binscn_set_offset(scnhdr, newoffset);

   //Compute the size of the new program header
   uint64_t newphdrsz = binscn_get_size(seghdr); //((binfile_get_word_size(bf) == BFS_32BITS)?sizeof(Elf32_Phdr):sizeof(Elf64_Phdr))*binfile_get_nb_segments(bf);
   DBGMSGLVL(1, "New size of program header is %#"PRIx64" bytes\n", newphdrsz);

   //Computes the offset at which the new program header would end
   //uint64_t newphdrend = binscn_get_end_offset(seghdr);//Elf_Ehdr_get_e_phoff(efile_creator->elf) + newphdrsz;
   //Check if this new offset would overlap with an existing section
   i = 0;
   //Finds the section immediately preceding the segment header in the original file
   while (i < binfile_get_nb_sections(creator)
         && binscn_get_offset(binfile_get_scn(bf, i))
               < Elf_Ehdr_get_e_phoff(efile_creator->elf))
      i++;
   if (i < binfile_get_nb_sections(creator)
         && binscn_get_offset(binfile_get_scn(bf, i))
               < (Elf_Ehdr_get_e_phoff(efile_creator->elf) + newphdrsz)) {
      //The new size of the program header would now cause it to overlap the section immediately following it: we have to move it

//      int64_t seghdrend = binscn_get_end_addr(seghdr);  //Retrieves the original end of the segment header
//      binscn_set_size(seghdr, newphdrsz);  //Updates the size of the segment header

      //Finds the segment containing the program header
      /**\todo TODO (2015-04-08) I could probably do this using the segment that is associated to the segment header,
       * but it would be a bit longer, as more than one segment can contain the segment header. Maybe find some way
       * to reduce the number of calls this would cause to have something more streamlined, blah blah blah.*/
      uint16_t phdrsegid = 0;
      while (phdrsegid < binfile_get_nb_segments(creator)
            && Elf_Phdr_get_p_type(efile_creator->elf, phdrsegid) != PT_PHDR)
         phdrsegid++;
      assert(phdrsegid < binfile_get_nb_segments(creator)); //The segment containing the program header should exist
      /**\todo TODO (2015-04-02) Also, check at some point that there are segments in the file before patching it*/

//      //Retrieves the segment containing the program header
//      binseg_t* phdrseg = binfile_get_seg(bf, phdrsegid);
//      //Updates the size of the segment
//      binseg_set_fsize(phdrseg, newphdrsz);
//      binseg_set_msize(phdrseg, newphdrsz);

      //Computes the new address of the program header so that it ends at the same address as in the original file
      int64_t newphdraddr = binscn_get_end_addr(binfile_get_seg_header(creator))
            - newphdrsz;
      binscn_set_addr(seghdr, newphdraddr);

      uint64_t oldphdroff = binscn_get_offset(seghdr);

      //Computes the offset at which the program header must be moved
      uint64_t newphdroff = elf_binscn_patch_findnewoffset(seghdr, oldphdroff,
            NEW_SEGMENT_ALIGNMENT);
      assert(newphdroff > oldphdroff); //The header should have been moved forward
      //Updating the offset of the program header
      binscn_set_offset(seghdr, newphdroff);

      //Now increasing the offsets of all sections after section 0
      /**\note (2015-04-09) Since we forced the header to be aligned on NEW_SEGMENT_ALIGNMENT, which is also
       * the maximum alignment possible, we can simply propagate the increase in offset to all sections while
       * being sure the alignments will be kept. I think*/
      uint64_t shiftoff = newphdroff - oldphdroff + binscn_get_size(seghdr)
            - binscn_get_size(binfile_get_seg_header(creator));
      uint16_t j;
      DBGMSG("Shifting all section offsets by %#"PRIx64"\n", shiftoff);
      for (j = 1; j < binfile_get_nb_sections(bf); j++) {
         binscn_t* scn = binfile_get_scn(bf, j);
         DBGMSGLVL(2,
               "Shifting section %s (%d) from offset %#"PRIx64" to offset %#"PRIx64"\n",
               binscn_get_name(scn), binscn_get_index(scn), binscn_get_offset(scn),
               binscn_get_offset(scn) + shiftoff)
         binscn_set_offset(scn, binscn_get_offset(scn) + shiftoff);
      }
      //Increasing the offset of the section header
      binscn_set_offset(scnhdr, binscn_get_offset(scnhdr) + shiftoff);
   }

   //Now updates the segments
   DBGMSGLVL(2, "Segments of file %s updated to:\n", binfile_get_file_name(bf));
   for (i = 0; i < binfile_get_nb_segments(bf); i++) {
      //Realigning the beginning and end of the segment on the first and last section it contains
      binseg_t* seg = binfile_get_seg(bf, i);
      uint16_t n_scns = binseg_get_nb_scns(seg);
      binscn_t* firstscn = binseg_get_scn(seg, 0);
      binscn_t* lastscn = binseg_get_scn(seg, n_scns - 1);
      int64_t firstaddr = binscn_get_addr(firstscn);
      int64_t lastaddr = binscn_get_end_addr(lastscn);
      uint64_t firstoff = binscn_get_offset(firstscn);
      uint64_t lastoff = binscn_get_end_offset(lastscn);
      if (firstaddr == 0 && n_scns > 1 && binseg_get_addr(seg) != 0) {
         firstaddr = binscn_get_addr(binseg_get_scn(seg, 1));
         firstoff = binscn_get_offset(binseg_get_scn(seg, 1));
         assert(firstaddr != 0);
         /**\note (2015-04-28) This is a bit of a hack to ensure that the first PT_LOAD segments
          * begins correctly at offset 0 like in the original file (which will have associated it
          * to section 0), but not at address 0 if it was not*/
      }
      /**\todo TODO (2015-04-09) I could add a test to check if the section was modified before updating the segment,
       maddr_t firstaddr = binscn_get_addr(firstscn);
       maddr_t lastaddr = binscn_get_end_addr(lastscn);
       * but I need to detect if the section is a header*/
      binseg_set_offset(seg, firstoff);
      binseg_set_addr(seg, firstaddr);
      binseg_set_fsize(seg, lastoff - firstoff);
      binseg_set_msize(seg, lastaddr - firstaddr);
      DBGLVL(2, STDMSG(" [%d]", i); binseg_fprint(seg, stderr); STDMSG("\n");)
   }

   return out;
}

//// Functions used by elf_binfile_patch_write_file

/**
 * Retrieves the index into a string section representing the name associated to a given object
 * \param bf Binfile being patched
 * \param names Hashtable linking the objects to the entry representing their name
 * \param names Hashtable linking the objects to the entry representing their name in the origin file
 * \param strscnid Index of the string section in the patched file
 * \param object Object whose name we are looking for (it's the index in \c names)
 * \param originobject Object in the original file
 * \return Index of the name associated to the object in the corresponding section
 * */
static uint32_t elf_binfile_patch_getnameoffset(binfile_t* bf,
      hashtable_t* names, hashtable_t* originnames, uint16_t strscnid,
      void* object, void* originobject)
{
   //uint16_t strscn;
   //Retrieving the pointer to the string entry representing the section name
   pointer_t* scnnameptr = hashtable_lookup(names, object);
   //Retrieving it in the original if the section does not have a link to a name in the patched file
   if (!scnnameptr && originobject) {
      scnnameptr = hashtable_lookup(originnames, originobject);
//      strscn = originstrscnid;   //We will look up into the string table of the original file
   } else {
//      strscn = strscnid;   //We will look up into the string table of the patched file
   }
   assert(scnnameptr && pointer_get_target_type(scnnameptr) == TARGET_DATA); //Just because
   //Now computing the offset to the name
   Elf64_Word nameidx = binfile_patch_find_entry_offset_in_scn(bf, strscnid,
         pointer_get_data_target(scnnameptr));
   assert(nameidx != UINT32_MAX);
   //Adding the offset from the pointer (may be 0)
   nameidx += pointer_get_offset_in_target(scnnameptr);
   return nameidx;
}
/**
 * Retrieves the ELF type for an ElfSym entry from the type of the associated label
 * \param lbltype Type of the label (as defined in \ref label_type)
 * \return Type for the corresponding ElfSym entry
 * */
static uint32_t labeltype_to_ElfSymtype(uint8_t lbltype)
{
   assert(lbltype < LBL_ERROR);
   uint32_t out = 0;
   /**\todo (2014-10-17) We could reorder the cases so that all STT_NOTYPE are grouped, but in case we need to change someday...*/
   switch (lbltype) {
   case LBL_FUNCTION:
   case LBL_EXTFUNCTION:
      out = STT_FUNC;
      break;
   case LBL_GENERIC:
   case LBL_PATCHSCN:
      out = STT_NOTYPE;
      break;
   case LBL_NOFUNCTION:
      out = STT_NOTYPE;
      break;
   case LBL_DUMMY:
      out = DUMMY_SYMBOL_TYPE;
      break;
   case LBL_OTHER:
      out = STT_NOTYPE;
      break;
   case LBL_VARIABLE:
      out = STT_OBJECT;
      break;
   }
   return out;
}

/**
 * Retrieves the ELF bind for an ElfSym entry from the type of the associated label
 * \param lbltype Type of the label (as defined in \ref label_type)
 * \return Bind for the corresponding ElfSym entry
 * */
static uint32_t labeltype_to_ElfSymbind(uint8_t lbltype)
{
   assert(lbltype < LBL_ERROR);
   uint32_t out = 0;
   /**\todo (2014-10-17) We could reorder the cases so that all STT_NOTYPE are grouped, but in case we need to change someday...*/
   switch (lbltype) {
   case LBL_FUNCTION:
   case LBL_EXTFUNCTION:
   case LBL_GENERIC:
   case LBL_PATCHSCN:
      out = STB_GLOBAL;
      break;
   case LBL_NOFUNCTION:
   case LBL_DUMMY:
   case LBL_OTHER:
   case LBL_VARIABLE:
      out = STB_LOCAL;
      break;
   }
   return out;
}

/**
 * Code for storing symbols from a patched file into the parsed ELF file for writing depending on the word size of the file (32/64).
 * Intended to be used in elf_patchsection_from_binfile to factorise code.
 * \param WS Word size of the ELF file
 * \param IDXSTRSCNID Identifier of the index of the string section to use
 * \note This assumes that the symbol section has not been created. Beware in the case of static patching. Also, if we do add symbol
 * sections later, well, good luck to you.
 * */
#define PATCHSYMSCN_TO_ELF(WS, STRSCNID) {\
   MACROVAL_CONCAT(MACRO_CONCAT(Elf,WS),_Sym)* syms = lc_malloc0(sizeof(*syms)*binscn_get_nb_entries(scn));\
   MACROVAL_CONCAT(MACRO_CONCAT(Elf,WS),_Sym)* originsyms = (MACROVAL_CONCAT(MACRO_CONCAT(Elf,WS),_Sym)*)binscn_get_data(originscn, NULL);\
   /*Creates the new symbol table*/ \
   /*Updates the entries in the symbol table*/ \
   for (i = 0; i < binscn_get_nb_entries(scn); i++) {\
      /*Retrieves the entry*/\
      data_t* entry = binfile_patch_get_scn_entry(bf, scnid, i);\
      /*Original entry (may be identical or NULL)*/\
      data_t* originentry = binscn_get_entry(originscn, i);\
      /*Retrieves the associated label*/\
      label_t* symlbl = data_get_data_label(entry);\
      /*label_t* originsymlbl = data_get_data_label(originentry);*/\
      assert(symlbl); /*Just checking we are indeed dealing with a label entry and retrieved it correctly*/\
      /*Retrieving the original ELF entry if it exists*/\
      if (originentry) {\
         /*Initialising the entry with the values from the original*/\
         memcpy(&(syms[i]), &(originsyms[i]), sizeof(syms[i]));\
      } else {\
         /*New entry: updates other label values*/\
         /*For data targets, the size is the size of the data object. We set it to 0 for instructions*/\
         /**\todo (2014-10-20) For future evolutions, we could calculate the byte size of the function targeted by the label (yes, ELF does that)*/\
         syms[i].st_size = (label_get_target_type(symlbl) == TARGET_DATA)?data_get_size((data_t*)label_get_target(symlbl)):0;\
         syms[i].st_info = MACROVAL_CONCAT(MACRO_CONCAT(ELF,WS),_ST_INFO)(labeltype_to_ElfSymbind(label_get_type(symlbl)), labeltype_to_ElfSymtype(label_get_type(symlbl)));\
      }\
      /*Retrieves the name of the symbol*/\
      /*char* symname = label_get_name(symlbl);*/\
      /*Looks up the index of the name in the string section*/\
      uint32_t nameidx = elf_binfile_patch_getnameoffset(bf, patched->symnames, origin->symnames, patched->indexes[STRSCNID], entry, originentry);\
      assert(nameidx < UINT32_MAX);\
      syms[i].st_name = nameidx;\
      /*Updates the label address depending on the address of its target*/\
      label_upd_addr(symlbl);\
      /*Updates the entry with the label value*/\
      syms[i].st_value = label_get_addr(symlbl);\
      /**\todo WARNING: Address changes in case of ARM. Check where to put that*/\
      uint16_t symscnid = binscn_get_index(label_get_scn(symlbl));\
      if (symscnid != UINT16_MAX) {\
         syms[i].st_shndx = patched->oldscnid[symscnid];/**\todo (2014-10-20) Yes, I know, it's ugly.*/\
      } else if (!originentry) {\
         syms[i].st_shndx = SHN_UNDEF;\
      }/*Otherwise, the type of the original entry will be kept*/\
   }\
   /*Updates the array of elf symbols into the ELF file*/\
   binscn_patch_set_data(scn, (unsigned char*)syms);\
}

/**
 * Code for storing relocation from a patched binfile_t structure into the parsed ELF file for writing
 * depending on the word size of the file (32/64) and the type of relocation (rel/rela)
 * It is intended to be invoked from elf_loadrelscn_to_binfile
 * \param WS Word size of the file (32/64)
 * \param RT Relocation type (_Rel/_Rela)
 * \note This assumes that the relocation section has not been created. Beware in the case of static patching. Also, if we do add symbol
 * sections later, well, good luck to you.
 * */
#define PATCHRELSCN_TO_ELF(WS, RT) {\
   MACROVAL_CONCAT(MACRO_CONCAT(Elf, WS), RT)* rels = lc_malloc0(sizeof(*rels)*binscn_get_nb_entries(scn));\
   MACROVAL_CONCAT(MACRO_CONCAT(Elf, WS), RT)* originrels = (MACROVAL_CONCAT(MACRO_CONCAT(Elf,WS),RT)*)binscn_get_data(originscn, NULL);\
   /*Retrieves the updated indexes of the sections referenced by the relocation section*/\
   /*scnlink = patched->oldscnid[Elf_Shdr_get_sh_link(originelf, scnid)];*/\
   /*scninfo = patched->oldscnid[Elf_Shdr_get_sh_info(originelf, scnid)];*/\
   /*Update the entries in the relocation table*/\
   for (i = 0; i < binscn_get_nb_entries(scn); i++) {\
      /*Retrieves the entry*/\
      data_t* entry = binfile_patch_get_scn_entry(bf, scnid, i);\
      /*Original entry (may be identical or NULL)*/\
      data_t* originentry = binscn_get_entry(originscn, i);\
      /*Retrieving the relocation*/\
      binrel_t* rel = data_get_binrel(entry);\
      /*Retrieving the label associated to the relocation*/\
      label_t* rellbl = binrel_get_label(rel);\
      /*Retrieving the original ELF entry if it exists*/\
      if (originentry) {\
         /*Initialising the entry with the values from the original*/\
         memcpy(&(rels[i]), &(originrels[i]), sizeof(rels[i]));\
      } else {\
         /*New entry: initialising values*/\
         uint32_t relsym = binscn_find_label_id(binfile_patch_get_scn(bf, scnlink), rellbl);\
         uint32_t reltype = binrel_get_rel_type(rel);\
         rels[i].r_info = MACROVAL_CONCAT(MACRO_CONCAT(ELF,WS),_R_INFO)(relsym, reltype);\
         /**\todo (2014-10-21) BEWARE OF RELA!*/\
      }\
      /*Updates address of relocation*/\
      rels[i].r_offset = pointer_get_addr(binrel_get_pointer(rel));\
   }\
   /*Updates the array of elf relocations into the ELF file*/\
   binscn_patch_set_data(scn, (unsigned char*)rels);\
}

/**
 * Code for loading a dynamic entry into a binfile_t structure depending on the word size of the file (32/64)
 * It is intended to be invoked from elf_loaddynscn_to_binfile
 * \param WS Word size of the file (32/64)
 * */
#define PATCHDYNSCN_TO_ELF(WS) {\
   MACROVAL_CONCAT(MACRO_CONCAT(Elf, WS), _Dyn)* dyns = lc_malloc0(sizeof(*dyns)*binscn_get_nb_entries(scn));\
   MACROVAL_CONCAT(MACRO_CONCAT(Elf, WS), _Dyn)* origindyns = (MACROVAL_CONCAT(MACRO_CONCAT(Elf,WS),_Dyn)*)binscn_get_data(originscn, NULL);\
   /*First pass to handle entries containing an address and thus referencing to other sections*/\
   for (i = 0; i < binscn_get_nb_entries(scn); i++) {\
      /*Retrieves the entry*/\
      data_t* entry = binfile_patch_get_scn_entry(bf, scnid, i);\
      /*Original entry (may be identical or NULL)*/\
      data_t* originentry = binscn_get_entry(originscn, i);\
      /*Retrieving the original ELF entry if it exists*/\
      if (originentry) {\
         /*Initialising the entry with the values from the original*/\
         memcpy(&(dyns[i]), &(origindyns[i]), sizeof(dyns[i]));\
      }\
      if (data_get_type(entry) == DATA_PTR) {\
         /*Entry contains a pointer*/\
         pointer_t* ptr = data_get_pointer(entry);\
         uint32_t stridx = 0;\
         switch(pointer_get_target_type(ptr)) {\
         case TARGET_DATA:\
            /*Looks up the index of the name in the string section*/\
            stridx = binfile_patch_find_entry_offset_in_scn(bf, patched->indexes[DYNSTR_IDX], pointer_get_data_target(ptr));\
            assert(stridx < UINT32_MAX);\
            dyns[i].d_un.d_val = stridx + pointer_get_offset_in_target(ptr);\
            if (!originentry || origindyns[i].d_tag == DT_NULL) {\
               /*This entry was created or the original was NULL, so we must update its type*/\
               dyns[i].d_tag = DT_NEEDED;\
               /**\todo (2014-10-22) WARNING: So far this works well because the only entries we add are needed libraries. If we make other changes later we will need to identify the type of modification performed (here I can only suggest some table in the elffile_t structure)*/\
            }\
            break;\
         case TARGET_BSCN:\
            dyns[i].d_un.d_ptr = pointer_get_target_addr(ptr);\
            assert(originentry);/*We don't create those entries in the table*/\
            break;\
         default:\
         assert(FALSE);/*Should not happen. There.*/\
         }\
      } else {\
         /*Entry contains something else (should be either DATA_NIL or DATA_RAW)*/\
         assert(data_get_type(entry) == DATA_NIL || data_get_type(entry) == DATA_RAW);\
         assert(originentry);/*Entry should already exist*/\
         /*There should be nothing to do: the copy of the original entry has already been done above*/\
      }\
   }\
   /*Updates the array of elf dynamic entries into the ELF file*/\
   binscn_patch_set_data(scn, (unsigned char*)dyns);\
}

/**
 * Stores a string section from a patched file into a structure representing a parsed ELF file for writing.
 * \param bf The binfile_t structure representing the patched file
 * \param scn The binscn_t representing the patched section. Its size must be up to date
 * \param scnid Index of the section. It must be of type SHT_STRTAB.
 * \param patched Internal libmtroll structure representing the file
 * \param patchelf Structure representing the parsed ELF file to write
 * */
static void elf_patchstrscn_from_binfile(binfile_t* bf, binscn_t* scn,
      uint16_t scnid, elffile_t* patched, Elf* patchelf)
{
   assert(scn && patched && patchelf && binscn_get_type(scn) == SCNT_STRING);

   uint32_t i;
   uint64_t off = 0;
   //Initialises the string
   char* data = lc_malloc0(binscn_get_size(scn));

   //Loads all strings from the section into the string representing its data
   for (i = 0; i < binscn_get_nb_entries(scn); i++) {
      data_t* strent = binfile_patch_get_scn_entry(bf, scnid, i);
      strcpy(data + off, data_get_string(strent));
      off += data_get_size(strent); //Size of the entry is the size of the string + 1 (including terminating null character)
   }
   /*Updates the section data*/\
binscn_patch_set_data(scn,
         (unsigned char*) data);\
}

/**
 * Code for storing addresses in a .got section from a patched binfile_t structure into the parsed ELF file for writing
 * \param WS Word size of the binary file (32/64)
 * */
#define PATCHGOTSCN_TO_ELF(WS) {\
   MACROVAL_CONCAT(MACRO_CONCAT(Elf,WS),_Addr)* addrs = lc_malloc0(sizeof(*addrs)*binscn_get_nb_entries(scn));\
   for (i = 0; i < binscn_get_nb_entries(scn); i++) {\
      data_t* entry = binfile_patch_get_scn_entry(bf, scnid, i);\
      pointer_t* ptr = data_get_pointer(entry);\
      pointer_upd_addr(ptr);\
      addrs[i] = pointer_get_addr(ptr);\
   }\
   /*Updates the section data*/\
   binscn_patch_set_data(scn, (unsigned char*)addrs);\
}

/**
 * Loads the details about a patched section into a structure representing a parsed ELF file.
 * \param bf The structure representing the patched file
 * \param scn The section to update
 * \param scnid Index of the section in the patched file (after reordering)
 * \param patched Internal libmtroll structure representing the patched file
 * \param origin Internal libmtroll structure representing the original file
 * \return EXIT_SUCCESS if the operation was successful, error code otherwise
 * */
static int elf_patchsection_from_binfile(binfile_t* bf, binscn_t* scn,
      uint16_t scnid, elffile_t* patched, elffile_t* origin)
{
   assert(
         bf && scn && patched && origin
               && (scnid < binfile_get_nb_sections(bf)));

   Elf* patchelf = patched->elf;
   Elf* originelf = origin->elf;
   binfile_t* creator = binfile_get_creator(bf);
   uint16_t originscnid = binscn_get_index(scn);
   binscn_t* originscn = binfile_get_scn(creator, originscnid);

   Elf64_Word scntype;
   Elf64_Word scnlink = 0;
   Elf64_Word scninfo = 0;

   //Retrieving type and references to other sections
   if (originscnid < binfile_get_nb_sections(creator)) {
      //The section existed in the original file
      Elf64_Word oldtype = Elf_Shdr_get_sh_type(originelf, originscnid);
      Elf64_Word oldlink = Elf_Shdr_get_sh_link(originelf, originscnid);
      Elf64_Word oldinfo = Elf_Shdr_get_sh_info(originelf, originscnid);

      //The type is the same as the original
      scntype = oldtype;
      //Converting the link to the new index
      if ((oldlink != 0) && (oldlink != SHN_UNDEF))
         scnlink = patched->oldscnid[oldlink]; //Link contained a section index
      else
         scnlink = oldlink; //Link is not set or unused: same value will be kept

      //sh_info does not hold a section index if it is a symbol table or a VERNEED section (apparently)
      if ((oldinfo != 0) && (oldtype != SHT_SYMTAB) && (oldtype != SHT_DYNSYM)
            && (oldtype != SHT_GNU_verneed))
         scninfo = patched->oldscnid[oldinfo]; //Link contains a section index: converting index
      else
         scninfo = oldinfo;
   } else {
      //New section
      /**\todo TODO WARNING (2014-10-15) Beware of static patching. In this case, we will have added new sections, but they are linked
       * to a different file and may have a different type as the 3 accepted below.*/
      switch (binscn_get_type(scn)) {
      case SCNT_CODE:
      case SCNT_DATA:
         scntype = SHT_PROGBITS;
         break;
      case SCNT_ZERODATA:
         scntype = SHT_NOBITS;
         break;
      default:
         assert(FALSE); //So far we only create those types of sections
      }
   }

   //Initialising the ELF structures of the section
   if (binscn_get_type(scn) == SCNT_PATCHCOPY) {
      //Section has not been updated during patching: simply copying the contents of the original
      DBGMSG(
            "Section %s (%d) has not been modified and will be copied from the original (%d)\n",
            binscn_get_name(scn), scnid, originscnid);
      Elf_Scn* origscn = elf_getscn(originelf, originscnid);
      Elf_Scn* patchscn = elf_getscn(patchelf, scnid);
      elf_scn_copy(patchscn, origscn);
      //Linking the copy to the bytes of the original to reduce memory footprint (and simplify the freeing)
      elf_scn_setdatabytes(patchscn, elf_scn_getdatabytes(origscn));
//      //Updates the offset of the section
//      Elf_Shdr_set_sh_offset(patched->elf, scnid, binscn_get_offset(scn));
   } else {
      //Section has been updated during patching: we have to rebuild its content
      DBGMSG(
            "Section %s (%d) has been updated or created by patching and will be rebuilt\n",
            binscn_get_name(scn), scnid);
      //Name of the section
      //Retrieves the index to the string from in the associated section
      Elf64_Word scnname = elf_binfile_patch_getnameoffset(bf,
            patched->scnnames, origin->scnnames, patched->indexes[SHSTRTAB_IDX],
            scn, originscn);
      //Updating the name of the section
      Elf_Shdr_set_sh_name(patchelf, scnid, scnname);

      //Size in bytes of the section
      Elf_Shdr_set_sh_size(patchelf, scnid, binscn_get_size(scn));
      /**\todo TODO (2014-10-14) Somewhere before, we need to update the section data size. This is done for the sections
       * containing entries, but for code sections we have to use some combination of insnlist_getcoding and copying the
       * original content (for the existing ones)*/
      //Address at which the section will be loaded into memory
      Elf_Shdr_set_sh_addr(patchelf, scnid, binscn_get_addr(scn));
      //Size of entries
      Elf_Shdr_set_sh_entsize(patchelf, scnid, binscn_get_entry_size(scn));

      //Rights over the section
      Elf64_Xword scnflags = Elf_Shdr_get_sh_flags(originelf, originscnid); //Retrieving original flags (will return 0 if section is new)
      if (binscn_check_attrs(scn, SCNA_WRITE))
         scnflags |= SHF_WRITE;
      if (binscn_check_attrs(scn, SCNA_EXE))
         scnflags |= SHF_EXECINSTR;
      if (binscn_check_attrs(scn, SCNA_LOADED))
         scnflags |= SHF_ALLOC;
      /**\todo (2014-10-14) We could improve this by adding a binscn_get_attrs function and use it to retrieve the flags once and for all
       * then test them.
       * Also, check if there are not shitty cases where we could miss important attributes updates.*/
      Elf_Shdr_set_sh_flags(patchelf, scnid, scnflags);

      //Now for some type specific processing
      switch (scntype) {
      uint32_t i;
   case SHT_DYNAMIC:
      if (binfile_get_word_size(bf) == BFS_32BITS)
         PATCHDYNSCN_TO_ELF(32)
      else if (binfile_get_word_size(bf) == BFS_64BITS)
         PATCHDYNSCN_TO_ELF(64)
      break;
   case SHT_DYNSYM:
      if (binfile_get_word_size(bf) == BFS_32BITS)
         PATCHSYMSCN_TO_ELF(32, DYNSTR_IDX)
      else if (binfile_get_word_size(bf) == BFS_64BITS)
         PATCHSYMSCN_TO_ELF(64, DYNSTR_IDX)
      /**\todo TODO (2014-10-15) Also handle the versym here
       * => (2015-05-28) Has been done, see below*/
      break;
   case SHT_SYMTAB:
      if (binfile_get_word_size(bf) == BFS_32BITS)
         PATCHSYMSCN_TO_ELF(32, STRTAB_IDX)
      else if (binfile_get_word_size(bf) == BFS_64BITS)
         PATCHSYMSCN_TO_ELF(64, STRTAB_IDX)
      break;
   case SHT_REL:
      if (binfile_get_word_size(bf) == BFS_32BITS)
         PATCHRELSCN_TO_ELF(32, _Rel)
      else if (binfile_get_word_size(bf) == BFS_64BITS)
         PATCHRELSCN_TO_ELF(64, _Rel)
      break;
   case SHT_RELA:
      if (binfile_get_word_size(bf) == BFS_32BITS)
         PATCHRELSCN_TO_ELF(32, _Rela)
      else if (binfile_get_word_size(bf) == BFS_64BITS)
         PATCHRELSCN_TO_ELF(64, _Rela)
      break;
   case SHT_STRTAB:
      elf_patchstrscn_from_binfile(bf, scn, scnid, patched, patchelf);
      break;
   case SHT_GNU_verneed:
      //TODO ?
      break;
   case SHT_GNU_versym:
      //The section contains version symbols informations
      //scn->type = SCNT_REFS;
      /**\todo TODO HANDLE THEM (2014-05-23)
       * => (2015-04-28) Doing that*/
      binscn_patch_set_data_from_entries(scn);
      break;
   case SHT_NOBITS:
   case SHT_PROGBITS:
      if (scnid == patched->indexes[GOT_IDX]
            || scnid == patched->indexes[GOTPLT_IDX]) {
         if (binfile_get_word_size(bf) == BFS_32BITS)
            PATCHGOTSCN_TO_ELF(32)
         else if (binfile_get_word_size(bf) == BFS_64BITS)
            PATCHGOTSCN_TO_ELF(64)
      }
      //      binfile_patchprgscn_to_elf(bf, scn, scnid, patchelf, originelf);
      break;
   default:
      break;
      }
      //Updating section type (may have been modified depending on the type of the section)
      Elf_Shdr_set_sh_type(patchelf, scnid, scntype);

      //Updates the section data in the ELF structure
      elf_setdata(elf_getscn(patchelf, scnid), binscn_patch_get_data(scn));
      //Flags the section as not containing local data as the data we just put into it will be deleted when the ELF structure is
      //DBGMSGLVL(3, "Flagging section %s (%d) as not containing local data as it will be deleted with the ELF structure\n", binscn_get_name(scn), scnid);
      //binscn_rem_attrs(scn, SCNA_LOCALDATA);
      /**\todo TODO (2015-04-23) I'm really not fond of this way of proceeding. We may simply set the sections as never having local data,
       * but then there may be other formats where the parser/rewriter was implemented in such a way that it does not need the data or something
       * WARNING! There is another big problem, in that the libelf does not uses lc_malloc and lc_free, while the data has been allocated here
       * using them. If the lc_malloc allocates more, we will get some ugly crashes
       * => (2015-06-05) Removing this, and we will use elf_end_nodatafree when freeing files copied for a patching session. I do hope this
       * will take care of the problem*/
   }
   //Updating fields that are always potentially modified
   //Offset in the file where the section begins
   Elf_Shdr_set_sh_offset(patchelf, scnid, binscn_get_offset(scn));
   //Alignment of the section
   Elf_Shdr_set_sh_addralign(patchelf, scnid, binscn_get_align(scn));
   //Updating section link
   Elf_Shdr_set_sh_link(patchelf, scnid, scnlink);
   //Updating section info
   Elf_Shdr_set_sh_info(patchelf, scnid, scninfo);

   return EXIT_SUCCESS;
}

//// End of function used by elf_binfile_patch_write_file

int elf_binfile_patch_write_file(binfile_t* bf)
{
   /**\todo TODO (2014-09-30) This function may not be useful if we manage to do the writing in elf_binfile_patch_finalise
    * So far I'm using it to write some notes for implementing the writing of patched elf files into files, but it may
    * be moved to elf_binfile_patch_finalise*/

   /**\note Writing labels: if a label does not exist in the patched file, we simply copy the original ELF structure (updating targets if needed)
    * If it exists in the patched file, checks the hashtable to retrieve the associated string entry, then deduce its index (look up in the table,
    * juggling between original and copy, to add all sizes of string until reaching the entry)*/

   /**\note Here we assume here that the file has been completely reordered, and that all modifications are up to date, including:
    * - Modified or added data entries in any data section exist in the file
    * - Raw binary data is updated
    * - All values and references have their correct values
    * - Sections appear in the array in the order into which they are to appear in the file, and their scnid is the index of their original
    * - [CHANGING!]The Elf structure in bf->driver->parsedbin has been initialised in binfile_patch_finalise with the correct number of segments and
    * those segments are up to date
    * - The segments have not been reordered, all original segments are at the same place they were in the original and the new
    * segments have been added at the end. And binfile_patch_finalise updated all segments in the file
    * */

   /**\todo TODO (2014-10-06) Test here that the file is valid. So either invoke a binfile_patch_is_patching function, or invoke this function
    * only from a binfile wrapper (binfile_patch_write_file), or do the test here, but I don't like that one*/
   elffile_t* patched = binfile_get_parsed_bin(bf);
   elffile_t* origin = binfile_get_driver(binfile_get_creator(bf))->parsedbin;

   if (!patched->elf)
      patched->elf = elf_copy(origin->elf, binfile_get_file_stream(bf));

   assert(patched && origin && origin->elf && patched->elf); /**\todo (2014-10-07) Depending on the tests made before, one of those could become a real test (not an assert)*/
   int out = EXIT_SUCCESS;
   uint16_t i, n_sections, n_segments, n_segments_origin;

   n_sections = binfile_get_nb_sections(bf);
   //Initialising the array of sections
   elf_init_sections(patched->elf, n_sections);

   //Now reorders the sections according to their offsets
   /**\todo TODO (2015-04-23) If don't know if this would be interesting to put into the binfile_patch_write_file in la_binfile.c,
    * but maybe not all formats will be reordering sections*/
   out = binfile_patch_reorder_scn_by_offset(bf);
   if (ISERROR(out)) {
      ERRMSG("Unable to reorder sections in the binary file\n");
      return out;
   }

   //Initialises the array of correspondence between new and old sections indices
   /**\todo TODO (2014-10-15) This may be moved to elf_binfile_patch_write
    * => (2015-04-23) DONE. We may be able to remove oldscnid from the elffile_t structure and pass it as parameter*/
   patched->oldscnid = lc_malloc(
         sizeof(*patched->oldscnid) * binfile_get_nb_sections(bf));
   for (i = 0; i < binfile_get_nb_sections(bf); i++) {
      patched->oldscnid[binscn_get_index(binfile_get_scn(bf, i))] = i;
   }
   //And now updating the array of indices in the patched file because I'm tired to constantly do the conversion
   for (i = 0; i < MAX_NIDX; i++) {
      if (patched->indexes[i] > -1)
         patched->indexes[i] = patched->oldscnid[patched->indexes[i]];
   }

   /**\todo TODO (2015-04-09) Now that I have stored the section and segment header into the binfile_t structure, I could to
    * the conversion using those to fill up the ELF field. This may allow to streamline the process. Beware however, for that
    * to work we need to implement the mechanism for patching sections (with the thing about the empty data entries retrieving
    * the values from the original) for the sections representing the section and segment header as well. Can you guess that I am
    * fed up with this and want to get on with it and don't have time to rewrite everything once again ?
    * (seriously, though, do it, it would be cool and much more elegant)*/
   //Loads the sections
   for (i = 0; i < n_sections; i++) {
      //Retrieves the section
      binscn_t* scn = binfile_get_scn(bf, i);
      //Builds the ELF representation of the section
      elf_patchsection_from_binfile(bf, scn, i, patched, origin);
   }

   n_segments = binfile_get_nb_segments(bf);
   n_segments_origin = binfile_get_nb_segments(binfile_get_creator(bf));
   //Initialises the array of segments
   elf_init_segments(patched->elf, n_segments);

   //Loads the segments from their representation
   for (i = 0; i < n_segments; i++) {
      //Retrieves the segment
      binseg_t* seg = binfile_get_seg(bf, i);
      if (i < n_segments_origin) {
         //Segment existed in the original file: copying its type and flags from the origin
         Elf_Phdr_set_p_type(patched->elf, i,
               Elf_Phdr_get_p_type(origin->elf, i));
         Elf_Phdr_set_p_flags(patched->elf, i,
               Elf_Phdr_get_p_flags(origin->elf, i));
      } else {
         //New segment: setting it type as PT_LOAD or PT_TLS and computing its flag from its attributes
         uint8_t attrs = binseg_get_attrs(seg);
         if (attrs & SCNA_TLS)
            Elf_Phdr_set_p_type(patched->elf, i, PT_TLS);
         else
            Elf_Phdr_set_p_type(patched->elf, i, PT_LOAD);
         //Computes the flags of the segment from the binary representation of the segment
         uint32_t flags = ((attrs & SCNA_WRITE) ? PF_W : 0) /*Segment is accessible for writing*/
         | ((attrs & SCNA_EXE) ? PF_X : 0) /*Segment is executable*/
         | ((attrs & SCNA_READ) ? PF_R : 0); /*Segment is accessible for writing*/
         Elf_Phdr_set_p_flags(patched->elf, i, flags);
      }
      //Now updates all other values of the segment from its binary representation
      Elf_Phdr_set_p_offset(patched->elf, i, binseg_get_offset(seg));
      Elf_Phdr_set_p_vaddr(patched->elf, i, binseg_get_addr(seg));
      Elf_Phdr_set_p_paddr(patched->elf, i, binseg_get_addr(seg));
      Elf_Phdr_set_p_filesz(patched->elf, i, binseg_get_fsize(seg));
      Elf_Phdr_set_p_memsz(patched->elf, i, binseg_get_msize(seg));
      Elf_Phdr_set_p_align(patched->elf, i, binseg_get_align(seg));
   }

   //Updates the ELF header
   /**\todo TODO (2015-04-09) Here it would be also interesting to have kept my headers up to date in binfile_finalise
    * so that I would not need to retrieve my information from different sources*/
   binscn_t* scnhdr = binfile_get_scn_header(bf);
   binscn_t* seghdr = binfile_get_seg_header(bf);
   Elf_Ehdr_set_e_phoff(patched->elf, binscn_get_offset(seghdr));
   Elf_Ehdr_set_e_shoff(patched->elf, binscn_get_offset(scnhdr));
   Elf_Ehdr_set_e_phnum(patched->elf, binfile_get_nb_segments(bf));
   Elf_Ehdr_set_e_shnum(patched->elf, binfile_get_nb_sections(bf));
   Elf_Ehdr_set_e_shstrndx(patched->elf, patched->indexes[SHSTRTAB_IDX]);

   //Then, prints the ELF structure
   out &= elf_write(patched->elf, binfile_get_file_stream(bf));

   return out;
}

// Function for loading the binfile driver (declared here to avoid having problems with function declarations) //
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Loads the driver of a parsed binary file with all ELF-specific functions
 * \param bf A binfile_t structure to fill
 * \param efile Parsed ELF file
 * \return EXIT_SUCCESS if successful, error code otherwise
 * */
static int elf_loaddriver_to_binfile(binfile_t* bf, elffile_t* efile)
{
   /**\todo TODO (2014-06-10) Maybe move this to bfile_fmtinterface, with normalised names for the format specific functions*/
   if (!bf || !efile)
      return ERR_BINARY_MISSING_BINFILE;
   bf_driver_t* driver = binfile_get_driver(bf);

   // Sets the structure of the parsed binary file
   driver->parsedbin = efile;

   // Sets the function for freeing the parsed ELF file structure
   driver->parsedbin_free = elffile_free;
   // Sets the array of parsers for the debug information
   driver->binfile_parse_dbg = elf_binfile_parse_dbg;
//   // Sets the function retrieving the annotate for a section
//   driver->binfile_getscnannotate = elf_binfile_getscnannotate;
// Sets the function computing the first loaded address
   driver->binfile_patch_get_first_load_addr = elf_binfile_patch_get_first_load_addr;
   // Sets the function computing the last loaded address
   driver->binfile_patch_get_last_load_addr = elf_binfile_patch_get_last_load_addr;
   // Sets the function for writing a parsed file to a file
   driver->binfile_writefile = elf_binfile_writefile; /**\todo TODO (2014-09-30) May never be useful*/
   // Sets the function for printing an external label
   driver->generate_ext_label_name = elf_generate_ext_label_name;
   // Sets the function for printing the binary file
   driver->asmfile_print_binfile = elf_asmfile_print_binfile;
   // Sets the function for printing external functions from the binary file
   driver->asmfile_print_external_fcts = elf_asmfile_print_external_fcts;

   //Sets the function for computing the empty intervals in a file
   driver->binfile_build_empty_spaces = elf_binfile_build_empty_spaces;
   // Sets the function for initialising a patch operation
   driver->binfile_patch_init_copy = elf_binfile_patch_init_copy;
   // Sets the function for adding an external library to the file
   driver->binfile_patch_add_ext_lib = elf_binfile_patch_add_ext_lib;
   // Sets the function for renaming an existing external library
   driver->binfile_patch_rename_ext_lib = elf_binfile_patch_rename_ext_lib;
   // Sets the function for adding a label to the file
   driver->binfile_patch_add_label = elf_binfile_patch_add_label;
   // Sets the function for adding a new section to a file
   driver->binfile_patch_add_scn = elf_binfile_patch_add_scn;
   // Sets the function for adding a new segment to a file
   driver->binfile_patch_add_seg = elf_binfile_patch_add_seg;
   // Sets the function for finalising a patching session
   driver->binfile_patch_finalise = elf_binfile_patch_finalise;
   // Sets the function for writing a patched file to a file
   driver->binfile_patch_write_file = elf_binfile_patch_write_file; /**\todo TODO (2014-09-30) Maybe done by elf_binfile_patch_finalise*/
   // Sets the function for testing if a section can be moved to a given interval
   driver->binfile_patch_move_scn_to_interval = elf_binfile_patch_move_scn_to_interval;

   /**\todo TODO Fill the functions here once their list is complete*/

   // Setting architecture specific functions
   switch (arch_get_code(binfile_get_arch(bf))) {
   case ARCH_arm64:
      // Sets the function for loading external labels
      driver->asmfile_add_ext_labels = elf_arm64_asmfile_add_ext_labels;
      // Sets the function for adding a call to a function from an external library to a file
      driver->binfile_patch_add_ext_fct = elf_unknownarch_binfile_patch_add_ext_fct;
      break;
   default:
      //Setting dummy functions when the architecture is not recognised
      // Sets the function for loading external labels
      driver->asmfile_add_ext_labels = elf_unknownarch_asmfile_add_ext_labels;
      // Sets the function for adding a call to a function from an external library to a file
      driver->binfile_patch_add_ext_fct = elf_unknownarch_binfile_patch_add_ext_fct;
      break;
   }
   /**\todo TODO (2014-10-02) Once this is over, the architecture specific functions could be moved to different files for
    * better clarity. We could even move them to the relevant sub directory.*/

   driver->codescnname = MADRAS_TEXTSCN_NAME;
   driver->datascnname = MADRAS_DATASCN_NAME;
   driver->fixcodescnname = MADRAS_FIXTXTSCN_NAME;

   return EXIT_SUCCESS;
}

