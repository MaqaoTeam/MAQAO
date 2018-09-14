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
 * \file elfprinter.c
 * \brief Helper file used to print the formatted contents of an ELF file
 * */
#include <stdio.h>
#include <inttypes.h>

#include "libmasm.h"
#include "libmtroll.h"

/**\todo This code currently assumes that the file belongs to x86_64 architecture (this has an impact only on the names of relocation types)
 * A test should be added on the machine type from the ELF header to identify the names to use for relocation types*/
#define ARCH "X86_64"

/**\todo TODO (2014-06-11) Use the libbin to print most of the data and revert to ELF-specific functions only when printing ELF-specific
 * characteristics such as segments. Alternatively, use a separate function for printing libbin structures, and a format specific printing,
 * and allow to choose between both (printing using the libbin would for instance lose all info with regard to structure types)
 * Oh, also, all these functions should be referenced in the bloody driver.*/

/*
 * Prints external functions (sorted) using ELF file data
 * \param asmf an assembly file
 * \param outfile File where to print to
 * \return EXIT_SUCCESS if successful, error code otherwise
 */
int elf_asmfile_print_external_fcts(asmfile_t* asmf, char* outfile)
{
   FILE* out;										// output file
   int answ = EXIT_SUCCESS;
   // opens the output stream. ---------------------------------------
   // It is stdout by default, and it can be set using -o option
   if (outfile == NULL)
      out = stdout;
   else {
      out = fopen(outfile, "w");
      if (out == NULL) {
         ERRMSG("Unable to open file %s\n", outfile);
         return ERR_COMMON_UNABLE_TO_OPEN_FILE;
      }
   }
   binfile_t* bf = asmfile_get_binfile(asmf);
   if (!bf || (binfile_get_format(bf) != BFF_ELF))
      return EXIT_FAILURE;
   elffile_t* efile = binfile_get_parsed_bin(bf);
   Elf* elf = efile->elf;
   fprintf(out, "\nExternal functions:\n");
   char* lbl_table[binfile_get_nb_labels(bf)];
   uint32_t n_lbls = 0;
   uint32_t i;
   // case of .o : I don't know how to find functions
   if (Elf_Ehdr_get_e_type(elf) == ET_REL) {
      int idx = -1;

      // Get the symtab index
      for (i = 0; i < binfile_get_nb_sections(bf); i++) {
         if (Elf_Shdr_get_sh_type(elf, i) == SHT_SYMTAB) {
            idx = i;
            break;
         }
      }
      // Traverse entries
      if (idx > -1) {
         binscn_t* scn = binfile_get_scn(bf, idx);
         for (i = 0; i < binscn_get_nb_entries(scn); i++) {
//            if (efile->sections[idx]->table_entries[i]->entry_64 != NULL
//            && ELF64_ST_TYPE(efile->sections[idx]->table_entries[i]->entry_64->elfentry.sym->st_info) == STT_NOTYPE
//            && ELF64_ST_BIND(efile->sections[idx]->table_entries[i]->entry_64->elfentry.sym->st_info) == STB_GLOBAL)
//               printf ("\t%s\n", efile->sections[idx]->table_entries[i]->entry_64->name);
//            else if (efile->sections[idx]->table_entries[i]->entry_32 != NULL
//            && ELF32_ST_TYPE(efile->sections[idx]->table_entries[i]->entry_32->elfentry.sym->st_info) == STT_NOTYPE
//            && ELF32_ST_BIND(efile->sections[idx]->table_entries[i]->entry_32->elfentry.sym->st_info) == STB_GLOBAL)
//               printf ("\t%s\n", efile->sections[idx]->table_entries[i]->entry_32->name);
            label_t* lbl = data_get_label(binscn_get_entry(scn, i));
            if (label_get_scn(lbl) == NULL) {
               lbl_table[n_lbls++] = label_get_name(lbl);
            }
         }
      }
   } else {
      binscn_t* relaplt = binfile_get_scn(bf, efile->indexes[RELAPLT_IDX]); // Retrieves the .rela.plt section
      // Scanning the entries in the rela.plt section
      for (i = 0; i < relaplt->n_entries; i++) {
         data_t* pltentry = binscn_get_entry(relaplt, i);
         //Retrieving the name of the label associated to the .rela.plt entry
         char* relname = label_get_name(data_get_label(pltentry));
         if (relname)
            lbl_table[n_lbls++] = relname;

//      // gets data from ELF --------------------------------------------
//      int64_t pltgot_add  = Elf_Shdr_get_sh_addr (elf, efile->indexes[PLTGOT]); //address of .plt.got section
//      int64_t pltgot_size = Elf_Shdr_get_sh_size (elf, efile->indexes[PLTGOT]); //size (in bytes) of .plt.got section
//      int pltgot_align    = Elf_Shdr_get_sh_addralign (elf, efile->indexes[PLTGOT]); //size of an element of .plt.got section
//      int pltgot_nelem    = pltgot_size / pltgot_align;                                 //number of element
//      hashtable_t* jmprel_table = efile->sections[efile->indexes[JMPREL]]->hentries;    //relocation table coming from .rela.plt section
//      int idx = 0, nlbl = 0;
//      char* lbl_table [hashtable_size(jmprel_table)];
//
//
//      // traverses the ELF section -------------------------------------
//      for (idx = 0; idx < pltgot_nelem; idx++)
//      {
//         tableentry_t* te = (tableentry_t*)hashtable_lookup(jmprel_table, (void*)(pltgot_add + idx * pltgot_align));
//         if (te != NULL && te->entry_64 != NULL)
//            lbl_table [nlbl ++] = te->entry_64->name;
//         else if (te != NULL && te->entry_32 != NULL)
//            lbl_table [nlbl ++] = te->entry_32->name;
      }

   }
      // sorts functions names, then prints them -----------------------
   qsort(lbl_table, n_lbls, sizeof(char*), &strcmp_qsort);

   for (i = 0; i < n_lbls; i++)
      fprintf(out, "\t%s\n", lbl_table[i]);

   // close the stream ----------------------------------------------
   // It is needed when the output stream is not stdout or stderr
   if (out != stdout && out != stderr)
      fclose(out);
   return answ;
}

///////////////////////////////////////////////////////////////////////////////
//    Helper functions to print ELF (low (variable)/ middle (object) level   //
///////////////////////////////////////////////////////////////////////////////
/*
 * Gets a string corresponding to Elf64_Ehdr->e_ident[EI_CLASS]
 * \param code a possible value for Elf64_Ehdr->e_ident[EI_CLASS]
 * \return a string corresponding to the value of \e code
 */
static const char* tostring_e_ident_EI_CLASS(int code)
{
   switch (code) {
   case ELFCLASS32:
      return ("ELFCLASS32");
      break;
   case ELFCLASS64:
      return ("ELFCLASS64");
      break;
   case ELFCLASSNONE:
      return ("ELFCLASSNONE");
      break;
   default:
      return ("(unsupported value)");
      break;
   }
   return ("(unsupported value)");
}

/*
 * Gets a string corresponding to Elf64_Ehdr->e_ident[EI_DATA]
 * \param code a possible value for Elf64_Ehdr->e_ident[EI_DATA]
 * \return a string corresponding to the value of \e code
 */
static const char* tostring_e_ident_EI_DATA(int code)
{
   switch (code) {
   case ELFDATA2LSB:
      return ("2's complement, little endian");
      break;
   case ELFDATA2MSB:
      return ("2's complement, big endian");
      break;
   case ELFDATANONE:
      return ("Invalid data encoding");
      break;
   default:
      return ("(unsupported value)");
      break;
   }
   return ("(unsupported value)");
}

/*
 * Gets a string corresponding to Elf64_Ehdr->e_ident[EI_OSABI]
 * \param code a possible value for Elf64_Ehdr->e_ident[EI_OSABI]
 * \return a string corresponding to the value of \e code
 */
static const char* tostring_e_ident_EI_OSABI(int code)
{
   switch (code) {
   case ELFOSABI_NONE:
      return ("UNIX System V ABI");
      break;
   case ELFOSABI_HPUX:
      return ("HP-UX");
      break;
   case ELFOSABI_NETBSD:
      return ("NetBSD");
      break;
   case ELFOSABI_LINUX:
      return ("Linux");
      break;
   case ELFOSABI_SOLARIS:
      return ("Sun Solaris");
      break;
   case ELFOSABI_AIX:
      return ("IBM AIX");
      break;
   default:
      return ("(unsupported value)");
      break;
   }
   return ("(unsupported value)");
}

/*
 * Gets a string corresponding to Elf64_Ehdr->e_type
 * \param code a possible value for Elf64_Ehdr->e_type
 * \return a string corresponding to the value of \e code
 */
static const char* tostring_e_type(int code)
{
   switch (code) {
   case ET_NONE:
      return ("No file type");
      break;
   case ET_REL:
      return ("Relocatable file");
      break;
   case ET_EXEC:
      return ("Executable file");
      break;
   case ET_DYN:
      return ("Shared object file");
      break;
   case ET_CORE:
      return ("Core file");
      break;
   case ET_NUM:
      return ("Number of defined types");
      break;
   case ET_LOOS:
      return ("OS-specific range start");
      break;
   case ET_HIOS:
      return ("OS-specific range end");
      break;
   case ET_LOPROC:
      return ("Processor-specific range start");
      break;
   case ET_HIPROC:
      return ("Processor-specific range end");
      break;
   default:
      return ("(unsupported value)");
      break;
   }
   return ("(unsupported value)");
}

/*
 * Gets a string corresponding to Elf64_Ehdr->e_machine
 * \param code a possible value for Elf64_Ehdr->e_machine
 * \return a string corresponding to the value of \e code
 */
static const char* tostring_e_machine(int code)
{
   switch (code) {
   case EM_NONE:
      return ("No machine");
      break;
   case EM_M32:
      return ("AT&T WE 32100");
      break;
   case EM_SPARC:
      return ("SUN SPARC");
      break;
   case EM_386:
      return ("Intel 80386");
      break;
   case EM_68K:
      return ("Motorola m68k family");
      break;
   case EM_88K:
      return ("Motorola m88k family");
      break;
   case EM_860:
      return ("Intel 80860");
      break;
   case EM_MIPS:
      return ("MIPS R3000 big-endian");
      break;
   case EM_S370:
      return ("IBM System/370");
      break;
   case EM_MIPS_RS3_LE:
      return ("MIPS R3000 little-endian");
      break;
   case EM_PARISC:
      return ("HPPA");
      break;
   case EM_VPP500:
      return ("Fujitsu VPP500");
      break;
   case EM_SPARC32PLUS:
      return ("Sun's v8plus");
      break;
   case EM_960:
      return ("Intel 80960");
      break;
   case EM_PPC:
      return ("PowerPC");
      break;
   case EM_PPC64:
      return ("PowerPC 64-bit");
      break;
   case EM_S390:
      return ("IBM S390");
      break;
   case EM_V800:
      return ("NEC V800 series");
      break;
   case EM_FR20:
      return ("Fujitsu FR20");
      break;
   case EM_RH32:
      return ("TRW RH-32");
      break;
   case EM_RCE:
      return ("Motorola RCE");
      break;
   case EM_ARM:
      return ("ARM");
      break;
   case EM_FAKE_ALPHA:
      return ("Digital Alpha");
      break;
   case EM_SH:
      return ("Hitachi SH");
      break;
   case EM_SPARCV9:
      return ("SPARC v9 64-bit");
      break;
   case EM_TRICORE:
      return ("Siemens Tricore");
      break;
   case EM_ARC:
      return ("Argonaut RISC Core");
      break;
   case EM_H8_300:
      return ("Hitachi H8/300");
      break;
   case EM_H8_300H:
      return ("Hitachi H8/300H");
      break;
   case EM_H8S:
      return ("Hitachi H8S");
      break;
   case EM_H8_500:
      return ("Hitachi H8/500");
      break;
   case EM_IA_64:
      return ("Intel Merced");
      break;
   case EM_MIPS_X:
      return ("Stanford MIPS-X");
      break;
   case EM_COLDFIRE:
      return ("Motorola Coldfire");
      break;
   case EM_68HC12:
      return ("Motorola M68HC12");
      break;
   case EM_MMA:
      return ("Fujitsu MMA Multimedia Accelerator");
      break;
   case EM_PCP:
      return ("Siemens PCP");
      break;
   case EM_NCPU:
      return ("Sony nCPU embeeded RISC");
      break;
   case EM_NDR1:
      return ("Denso NDR1 microprocessor");
      break;
   case EM_STARCORE:
      return ("Motorola Start*Core processor");
      break;
   case EM_ME16:
      return ("Toyota ME16 processor");
      break;
   case EM_ST100:
      return ("STMicroelectronic ST100 processor");
      break;
   case EM_TINYJ:
      return ("Advanced Logic Corp. Tinyj emb.fam");
      break;
   case EM_X86_64:
      return ("AMD x86-64 architecture");
      break;
   case EM_PDSP:
      return ("Sony DSP Processor");
      break;
   case EM_FX66:
      return ("Siemens FX66 microcontroller");
      break;
   case EM_ST9PLUS:
      return ("STMicroelectronics ST9+ 8/16 mc");
      break;
   case EM_ST7:
      return ("STmicroelectronics ST7 8 bit mc");
      break;
   case EM_68HC16:
      return ("Motorola MC68HC16 microcontroller");
      break;
   case EM_68HC11:
      return ("Motorola MC68HC11 microcontroller");
      break;
   case EM_68HC08:
      return ("Motorola MC68HC08 microcontroller");
      break;
   case EM_68HC05:
      return ("Motorola MC68HC05 microcontroller");
      break;
   case EM_SVX:
      return ("Silicon Graphics SVx");
      break;
   case EM_ST19:
      return ("STMicroelectronics ST19 8 bit mc");
      break;
   case EM_VAX:
      return ("Digital VAX");
      break;
   case EM_CRIS:
      return ("Axis Communications 32-bit embedded processor");
      break;
   case EM_JAVELIN:
      return ("Infineon Technologies 32-bit embedded processor");
      break;
   case EM_FIREPATH:
      return ("Element 14 64-bit DSP Processor");
      break;
   case EM_ZSP:
      return ("LSI Logic 16-bit DSP Processor");
      break;
   case EM_MMIX:
      return ("Donald Knuth's educational 64-bit processor");
      break;
   case EM_HUANY:
      return ("Harvard University machine-independent object files");
      break;
   case EM_PRISM:
      return ("SiTera Prism");
      break;
   case EM_AVR:
      return ("Atmel AVR 8-bit microcontroller");
      break;
   case EM_FR30:
      return ("Fujitsu FR30");
      break;
   case EM_D10V:
      return ("Mitsubishi D10V");
      break;
   case EM_D30V:
      return ("Mitsubishi D30V");
      break;
   case EM_V850:
      return ("NEC v850");
      break;
   case EM_M32R:
      return ("Mitsubishi M32R");
      break;
   case EM_MN10300:
      return ("Matsushita MN10300");
      break;
   case EM_MN10200:
      return ("Matsushita MN10200");
      break;
   case EM_PJ:
      return ("picoJava");
      break;
   case EM_OPENRISC:
      return ("OpenRISC 32-bit embedded processor");
      break;
   case EM_ARC_A5:
      return ("ARC Cores Tangent-A5");
      break;
   case EM_XTENSA:
      return ("Tensilica Xtensa Architecture");
      break;
   case EM_K1OM:
      return ("Intel Many Integrated Core Architecture");
      break;
   case EM_AARCH64:
      return ("ARM 64-bit processor");
      break;
   default:
      return ("(unsupported value)");
      break;
   }
   return ("(unsupported value)");
}

/*
 * Gets a string corresponding to Elf64_Shdr->sh_type
 * \param code a possible value for Elf64_Shdr->sh_type
 * \return a string corresponding to the value of \e code
 */
static const char* tostring_sh_type(int code)
{
   switch (code) {
   case SHT_NULL:
      return ("NULL          ");
      break;
   case SHT_PROGBITS:
      return ("PROGBITS      ");
      break;
   case SHT_SYMTAB:
      return ("SYMTAB        ");
      break;
   case SHT_STRTAB:
      return ("STRTAB        ");
      break;
   case SHT_RELA:
      return ("RELA          ");
      break;
   case SHT_HASH:
      return ("HASH          ");
      break;
   case SHT_DYNAMIC:
      return ("DYNAMIC       ");
      break;
   case SHT_NOTE:
      return ("NOTE          ");
      break;
   case SHT_NOBITS:
      return ("NOBITS        ");
      break;
   case SHT_REL:
      return ("REL           ");
      break;
   case SHT_SHLIB:
      return ("SHLIB         ");
      break;
   case SHT_DYNSYM:
      return ("DYNSYM        ");
      break;
   case SHT_INIT_ARRAY:
      return ("INIT_ARRAY    ");
      break;
   case SHT_FINI_ARRAY:
      return ("FINI_ARRAY    ");
      break;
   case SHT_PREINIT_ARRAY:
      return ("PREINIT_ARRAY ");
      break;
   case SHT_GROUP:
      return ("GROUP         ");
      break;
   case SHT_SYMTAB_SHNDX:
      return ("SYMTAB_SHNDX  ");
      break;
   case SHT_NUM:
      return ("NUM           ");
      break;
   case SHT_LOOS:
      return ("LOOS          ");
      break;
   case SHT_GNU_ATTRIBUTES:
      return ("GNU_ATTRIBUTES");
      break;
   case SHT_GNU_HASH:
      return ("GNU_HASH      ");
      break;
   case SHT_GNU_LIBLIST:
      return ("GNU_LIBLIST   ");
      break;
   case SHT_CHECKSUM:
      return ("CHECKSUM      ");
      break;
   case SHT_LOSUNW:
      return ("LOSUNW        ");
      break;
   case SHT_SUNW_COMDAT:
      return ("SUNW_COMDAT   ");
      break;
   case SHT_SUNW_syminfo:
      return ("SUNW_syminfo  ");
      break;
   case SHT_GNU_verdef:
      return ("VERDEF        ");
      break;
   case SHT_GNU_verneed:
      return ("VERNEED       ");
      break;
   case SHT_GNU_versym:
      return ("VERSYM        ");
      break;
   case SHT_LOPROC:
      return ("LOPROC        ");
      break;
   case SHT_HIPROC:
      return ("HIPROC        ");
      break;
   case SHT_LOUSER:
      return ("LOUSER        ");
      break;
   case SHT_HIUSER:
      return ("HIUSER        ");
      break;
   default:
      return ("(unsupported) ");
      break;
   }
   return ("(unsupported)");
}

/*
 * Gets a string corresponding to Elf64_Phdr->sh_type
 * \param code a possible value for Elf64_Phdr->sh_type
 * \return a string corresponding to the value of \e code
 */
static const char* tostring_p_type(int code)
{
   switch (code) {
   case PT_NULL:
      return ("NULL");
      break;
   case PT_LOAD:
      return ("LOAD");
      break;
   case PT_DYNAMIC:
      return ("DYNAMIC");
      break;
   case PT_INTERP:
      return ("INTERP");
      break;
   case PT_NOTE:
      return ("NOTE");
      break;
   case PT_SHLIB:
      return ("SHLIB");
      break;
   case PT_PHDR:
      return ("PHDR");
      break;
   case PT_TLS:
      return ("TLS");
      break;
   case PT_NUM:
      return ("NUM");
      break;
   case PT_LOOS:
      return ("LOOS");
      break;
   case PT_GNU_EH_FRAME:
      return ("GNU_EH_FRAME");
      break;
   case PT_GNU_STACK:
      return ("GNU_STACK");
      break;
   case PT_GNU_RELRO:
      return ("GNU_RELRO");
      break;
   case PT_SUNWBSS:
      return ("SUNWBSS");
      break;
   case PT_SUNWSTACK:
      return ("SUNWSTACK");
      break;
   case PT_HISUNW:
      return ("HISUNW");
      break;
   case PT_LOPROC:
      return ("LOPROC");
      break;
   case PT_HIPROC:
      return ("HIPROC");
      break;
   default:
      return ("(unsupported)");
      break;
   }
   return ("(unsupported)");
}

/*
 * Gets a string corresponding to Elf64_Dyn->d_tag
 * \param dyn a Elf64_Dyn structure
 * \param ptr used to return the content of dyn->d_un
 * \param val used to return the content of dyn->d_un
 * \note \e ptr and \e val can not be filled at the same time. Only one of
 *       them is filled, the other one is set to -1
 * \return a string corresponding to the value of dyn->d_tag
 */
static const char* tostring_d_tag_64(Elf64_Dyn* dyn, int64_t* ptr, int64_t* val)
{
   char* type;

   switch (dyn->d_tag) {
   case DT_NULL:
      type = "(NULL)";
      *ptr = dyn->d_un.d_ptr;
      *val = -1;
      break;
   case DT_PLTGOT:
      type = "(PLTGOT)";
      *ptr = dyn->d_un.d_ptr;
      *val = -1;
      break;
   case DT_HASH:
      type = "(HASH)";
      *ptr = dyn->d_un.d_ptr;
      *val = -1;
      break;
   case DT_STRTAB:
      type = "(STRTAB)";
      *ptr = dyn->d_un.d_ptr;
      *val = -1;
      break;
   case DT_SYMTAB:
      type = "(SYMTAB)";
      *ptr = dyn->d_un.d_ptr;
      *val = -1;
      break;
   case DT_RELA:
      type = "(RELA)";
      *ptr = dyn->d_un.d_ptr;
      *val = -1;
      break;
   case DT_INIT:
      type = "(INIT)";
      *ptr = dyn->d_un.d_ptr;
      *val = -1;
      break;
   case DT_FINI:
      type = "(FINI)";
      *ptr = dyn->d_un.d_ptr;
      *val = -1;
      break;
   case DT_REL:
      type = "(REL)";
      *ptr = dyn->d_un.d_ptr;
      *val = -1;
      break;
   case DT_DEBUG:
      type = "(DEBUG)";
      *ptr = dyn->d_un.d_ptr;
      *val = -1;
      break;
   case DT_JMPREL:
      type = "(JMPREL)";
      *ptr = dyn->d_un.d_ptr;
      *val = -1;
      break;
   case DT_INIT_ARRAY:
      type = "(INIT_ARRAY)";
      *ptr = dyn->d_un.d_ptr;
      *val = -1;
      break;
   case DT_FINI_ARRAY:
      type = "(FINI_ARRAY)";
      *ptr = dyn->d_un.d_ptr;
      *val = -1;
      break;
   case DT_GNU_HASH:
      type = "(GNU_HASH)";
      *ptr = dyn->d_un.d_ptr;
      *val = -1;
      break;
   case DT_MOVETAB:
      type = "(MOVETAB)";
      *ptr = dyn->d_un.d_ptr;
      *val = -1;
      break;
   case DT_SYMINFO:
      type = "(SYMINFO)";
      *ptr = dyn->d_un.d_ptr;
      *val = -1;
      break;
   case DT_VERSYM:
      type = "(VERSYM)";
      *ptr = dyn->d_un.d_ptr;
      *val = -1;
      break;
   case DT_VERDEF:
      type = "(VERDEF)";
      *ptr = dyn->d_un.d_ptr;
      *val = -1;
      break;
   case DT_VERNEED:
      type = "(VERNEED)";
      *ptr = dyn->d_un.d_ptr;
      *val = -1;
      break;
   case DT_AUXILIARY:
      type = "(AUXILIARY)";
      *ptr = dyn->d_un.d_ptr;
      *val = -1;
      break;
   case DT_TLSDESC_PLT:
      type = "(TLSDESC_PLT)";
      *ptr = dyn->d_un.d_ptr;
      *val = -1;
      break;
   case DT_TLSDESC_GOT:
      type = "(TLSDESC_GOT)";
      *ptr = dyn->d_un.d_ptr;
      *val = -1;
      break;
   case DT_GNU_CONFLICT:
      type = "(GNU_CONFLICT)";
      *ptr = dyn->d_un.d_ptr;
      *val = -1;
      break;
   case DT_PREINIT_ARRAY:
      type = "(PREINT_ARAY)";
      *ptr = dyn->d_un.d_ptr;
      *val = -1;
      break;

   case DT_GNU_PRELINKED:
      type = "(GNU_PRELINKED)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_GNU_CONFLICTSZ:
      type = "(GNU_CONFLICTSZ)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_GNU_LIBLISTSZ:
      type = "(GNU_LIBLISTSZ)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_GNU_LIBLIST:
      type = "(GNU_LIBLIST)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_INIT_ARRAYSZ:
      type = "(INIT_ARRAYSZ)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_FINI_ARRAYSZ:
      type = "(FINI_ARRAYSZ)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_PREINIT_ARRAYSZ:
      type = "(PREINT_ARRAYSZ)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_RUNPATH:
      type = "(RUNPATH)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_FLAGS:
      type = "(FLAGS)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_CONFIG:
      type = "(CONFIG)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_DEPAUDIT:
      type = "(DEPAUDIT)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_AUDIT:
      type = "(AUDIT)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_PLTPAD:
      type = "(PLTPAD)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_RELACOUNT:
      type = "(RELACOUNT)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_RELCOUNT:
      type = "(RELCOUNT)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_FLAGS_1:
      type = "(FLAG1)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_NEEDED:
      type = "(NEEDED)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_PLTRELSZ:
      type = "(PLTRELSZ)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_RELASZ:
      type = "(RELASZ)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_RELAENT:
      type = "(RELAENT)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_STRSZ:
      type = "(STRSZ)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_SYMENT:
      type = "(SYMENT)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_SONAME:
      type = "(SONAME)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_RPATH:
      type = "(RPATH)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_SYMBOLIC:
      type = "(SYMBOLIC)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_VERNEEDNUM:
      type = "(VERNEEDNUM)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_VERDEFNUM:
      type = "(VERDEFNUM)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_CHECKSUM:
      type = "(CHECKSUM)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_PLTPADSZ:
      type = "(PLTPADSZ)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_MOVEENT:
      type = "(MOVEENT)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_MOVESZ:
      type = "(MOVESZ)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_FEATURE_1:
      type = "(FEATURE1)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_POSFLAG_1:
      type = "(POSFLAG1)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_SYMINSZ:
      type = "(SYMINSZ)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_SYMINENT:
      type = "(SYMINENT)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_ADDRRNGLO:
      type = "(ADDRRNGLO)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_NUM:
      type = "(NUM)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_LOOS:
      type = "(LOOS)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_HIOS:
      type = "(HIOS)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_LOPROC:
      type = "(LOPROC)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_HIPROC:
      type = "(HIPROC)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_MIPS_NUM:
      type = "(MIPS_NUM)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_VALRNGLO:
      type = "(VALRNGLO)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_RELSZ:
      type = "(RELSZ)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_RELENT:
      type = "(RELENT)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_PLTREL:
      type = "(PLTREL)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_BIND_NOW:
      type = "(BIND_NOW)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_TEXTREL:
      type = "(TEXTREL)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   default:
      type = "(unsupported)";
      *val = -1;
      *ptr = -1;
      break;
   }
   return (type);
}

static const char* tostring_d_tag_32(Elf32_Dyn* dyn, int64_t* ptr, int64_t* val)
{
   char* type;

   switch (dyn->d_tag) {
   case DT_NULL:
      type = "(NULL)";
      *ptr = dyn->d_un.d_ptr;
      *val = -1;
      break;
   case DT_PLTGOT:
      type = "(PLTGOT)";
      *ptr = dyn->d_un.d_ptr;
      *val = -1;
      break;
   case DT_HASH:
      type = "(HASH)";
      *ptr = dyn->d_un.d_ptr;
      *val = -1;
      break;
   case DT_STRTAB:
      type = "(STRTAB)";
      *ptr = dyn->d_un.d_ptr;
      *val = -1;
      break;
   case DT_SYMTAB:
      type = "(SYMTAB)";
      *ptr = dyn->d_un.d_ptr;
      *val = -1;
      break;
   case DT_RELA:
      type = "(RELA)";
      *ptr = dyn->d_un.d_ptr;
      *val = -1;
      break;
   case DT_INIT:
      type = "(INIT)";
      *ptr = dyn->d_un.d_ptr;
      *val = -1;
      break;
   case DT_FINI:
      type = "(FINI)";
      *ptr = dyn->d_un.d_ptr;
      *val = -1;
      break;
   case DT_REL:
      type = "(REL)";
      *ptr = dyn->d_un.d_ptr;
      *val = -1;
      break;
   case DT_DEBUG:
      type = "(DEBUG)";
      *ptr = dyn->d_un.d_ptr;
      *val = -1;
      break;
   case DT_JMPREL:
      type = "(JMPREL)";
      *ptr = dyn->d_un.d_ptr;
      *val = -1;
      break;
   case DT_INIT_ARRAY:
      type = "(INIT_ARRAY)";
      *ptr = dyn->d_un.d_ptr;
      *val = -1;
      break;
   case DT_FINI_ARRAY:
      type = "(FINI_ARRAY)";
      *ptr = dyn->d_un.d_ptr;
      *val = -1;
      break;
   case DT_GNU_HASH:
      type = "(GNU_HASH)";
      *ptr = dyn->d_un.d_ptr;
      *val = -1;
      break;
   case DT_MOVETAB:
      type = "(MOVETAB)";
      *ptr = dyn->d_un.d_ptr;
      *val = -1;
      break;
   case DT_SYMINFO:
      type = "(SYMINFO)";
      *ptr = dyn->d_un.d_ptr;
      *val = -1;
      break;
   case DT_VERSYM:
      type = "(VERSYM)";
      *ptr = dyn->d_un.d_ptr;
      *val = -1;
      break;
   case DT_VERDEF:
      type = "(VERDEF)";
      *ptr = dyn->d_un.d_ptr;
      *val = -1;
      break;
   case DT_VERNEED:
      type = "(VERNEED)";
      *ptr = dyn->d_un.d_ptr;
      *val = -1;
      break;
   case DT_AUXILIARY:
      type = "(AUXILIARY)";
      *ptr = dyn->d_un.d_ptr;
      *val = -1;
      break;
   case DT_TLSDESC_PLT:
      type = "(TLSDESC_PLT)";
      *ptr = dyn->d_un.d_ptr;
      *val = -1;
      break;
   case DT_TLSDESC_GOT:
      type = "(TLSDESC_GOT)";
      *ptr = dyn->d_un.d_ptr;
      *val = -1;
      break;
   case DT_GNU_CONFLICT:
      type = "(GNU_CONFLICT)";
      *ptr = dyn->d_un.d_ptr;
      *val = -1;
      break;
   case DT_PREINIT_ARRAY:
      type = "(PREINT_ARAY)";
      *ptr = dyn->d_un.d_ptr;
      *val = -1;
      break;
   case DT_GNU_PRELINKED:
      type = "(GNU_PRELINKED)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_GNU_CONFLICTSZ:
      type = "(GNU_CONFLICTSZ)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_GNU_LIBLISTSZ:
      type = "(GNU_LIBLISTSZ)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_GNU_LIBLIST:
      type = "(GNU_LIBLIST)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_INIT_ARRAYSZ:
      type = "(INIT_ARRAYSZ)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_FINI_ARRAYSZ:
      type = "(FINI_ARRAYSZ)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_PREINIT_ARRAYSZ:
      type = "(PREINT_ARRAYSZ)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_RUNPATH:
      type = "(RUNPATH)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_FLAGS:
      type = "(FLAGS)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_CONFIG:
      type = "(CONFIG)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_DEPAUDIT:
      type = "(DEPAUDIT)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_AUDIT:
      type = "(AUDIT)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_PLTPAD:
      type = "(PLTPAD)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_RELACOUNT:
      type = "(RELACOUNT)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_RELCOUNT:
      type = "(RELCOUNT)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_FLAGS_1:
      type = "(FLAG1)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_NEEDED:
      type = "(NEEDED)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_PLTRELSZ:
      type = "(PLTRELSZ)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_RELASZ:
      type = "(RELASZ)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_RELAENT:
      type = "(RELAENT)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_STRSZ:
      type = "(STRSZ)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_SYMENT:
      type = "(SYMENT)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_SONAME:
      type = "(SONAME)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_RPATH:
      type = "(RPATH)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_SYMBOLIC:
      type = "(SYMBOLIC)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_VERNEEDNUM:
      type = "(VERNEEDNUM)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_VERDEFNUM:
      type = "(VERDEFNUM)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_CHECKSUM:
      type = "(CHECKSUM)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_PLTPADSZ:
      type = "(PLTPADSZ)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_MOVEENT:
      type = "(MOVEENT)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_MOVESZ:
      type = "(MOVESZ)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_FEATURE_1:
      type = "(FEATURE1)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_POSFLAG_1:
      type = "(POSFLAG1)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_SYMINSZ:
      type = "(SYMINSZ)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_SYMINENT:
      type = "(SYMINENT)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_ADDRRNGLO:
      type = "(ADDRRNGLO)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_NUM:
      type = "(NUM)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_LOOS:
      type = "(LOOS)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_HIOS:
      type = "(HIOS)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_LOPROC:
      type = "(LOPROC)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_HIPROC:
      type = "(HIPROC)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_MIPS_NUM:
      type = "(MIPS_NUM)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_VALRNGLO:
      type = "(VALRNGLO)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_RELSZ:
      type = "(RELSZ)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_RELENT:
      type = "(RELENT)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_PLTREL:
      type = "(PLTREL)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_BIND_NOW:
      type = "(BIND_NOW)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   case DT_TEXTREL:
      type = "(TEXTREL)";
      *val = dyn->d_un.d_val;
      *ptr = -1;
      break;
   default:
      type = "(unsupported)";
      *val = -1;
      *ptr = -1;
      break;
   }
   return (type);
}

/*
 * Gets a string corresponding to ELF64_R_TYPE(Elf64_Rela->r_info)
 * \param code a possible value for ELF64_R_TYPE(Elf64_Rela->r_info)
 * \param machine value of Elf64Ehdr->e_machine
 * \return a string corresponding to the value of ELF64_R_TYPE(Elf64_Rela->r_info)
 */
static const char* tostring_TYPE_r_info(int code, int machine)
{
   switch (machine) {
   case EM_X86_64:
      switch (code) {
      case R_X86_64_NONE:
         return ("R_X86_64_NONE");
         break;
      case R_X86_64_64:
         return ("R_X86_64_64");
         break;
      case R_X86_64_PC32:
         return ("R_X86_64_PC32");
         break;
      case R_X86_64_GOT32:
         return ("R_X86_64_GOT32");
         break;
      case R_X86_64_PLT32:
         return ("R_X86_64_PLT32");
         break;
      case R_X86_64_COPY:
         return ("R_X86_64_COPY");
         break;
      case R_X86_64_GLOB_DAT:
         return ("R_X86_64_GLOB_DAT");
         break;
      case R_X86_64_JUMP_SLOT:
         return ("R_X86_64_JUMP_SLOT");
         break;
      case R_X86_64_RELATIVE:
         return ("R_X86_64_RELATIVE");
         break;
      case R_X86_64_GOTPCREL:
         return ("R_X86_64_GOTPCREL");
         break;
      case R_X86_64_32:
         return ("R_X86_64_32");
         break;
      case R_X86_64_32S:
         return ("R_X86_64_32S");
         break;
      case R_X86_64_16:
         return ("R_X86_64_16");
         break;
      case R_X86_64_PC16:
         return ("R_X86_64_PC16");
         break;
      case R_X86_64_8:
         return ("R_X86_64_8");
         break;
      case R_X86_64_PC8:
         return ("R_X86_64_PC8");
         break;
      case R_X86_64_DTPMOD64:
         return ("R_X86_64_DTPMOD64");
         break;
      case R_X86_64_DTPOFF64:
         return ("R_X86_64_DTPOFF64");
         break;
      case R_X86_64_TPOFF64:
         return ("R_X86_64_TPOFF64");
         break;
      case R_X86_64_TLSGD:
         return ("R_X86_64_TLSGD");
         break;
      case R_X86_64_TLSLD:
         return ("R_X86_64_TLSLD");
         break;
      case R_X86_64_DTPOFF32:
         return ("R_X86_64_DTPOFF32");
         break;
      case R_X86_64_GOTTPOFF:
         return ("R_X86_64_GOTTPOFF");
         break;
      case R_X86_64_TPOFF32:
         return ("R_X86_64_TPOFF32");
         break;
      case R_X86_64_PC64:
         return ("R_X86_64_PC64");
         break;
      case R_X86_64_GOTOFF64:
         return ("R_X86_64_GOTOFF64");
         break;
      case R_X86_64_GOTPC32:
         return ("R_X86_64_GOTPC32");
         break;
         //	case R_X86_64_GOT64:           return ("R_X86_64_GOT64");        break;
         //	case R_X86_64_GOTPCREL64:      return ("R_X86_64_GOTPCREL64");   break;
         //	case R_X86_64_GOTPC64:         return ("R_X86_64_GOTPC64");      break;
         //	case R_X86_64_GOTPLT64:        return ("R_X86_64_GOTPLT64");     break;
         //	case R_X86_64_PLTOFF64:        return ("R_X86_64_PLTOFF64");     break;
         //	case R_X86_64_SIZE32:          return ("R_X86_64_SIZE32");       break;
         //	case R_X86_64_SIZE64:          return ("R_X86_64_SIZE64");       break;
      case R_X86_64_GOTPC32_TLSDESC:
         return ("R_X86_64_GOTPC32_TLSDESC");
         break;
      case R_X86_64_TLSDESC_CALL:
         return ("R_X86_64_TLSDESC_CALL");
         break;
      case R_X86_64_TLSDESC:
         return ("R_X86_64_TLSDESC");
         break;
      case R_X86_64_IRELATIVE:
         return ("R_X86_64_IRELATIVE");
         break;
      default:
         return ("(unsupported)");
         break;
      }
      break;

   default:
      return ("(unsupported)");
      break;
   }
   return ("(unsupported)");
}

/*
 * Gets a string corresponding to ELF64_ST_BIND(Elf64_Sym->st_info)
 * \param code a possible value for ELF64_ST_BIND(Elf64_Sym->st_info)
 * \return a string corresponding to the value of ELF64_ST_BIND(Elf64_Sym->st_info)
 */
static const char* tostring_BIND_st_info(int code)
{
   switch (code) {
   case STB_LOCAL:
      return ("LOCAL");
      break;
   case STB_GLOBAL:
      return ("GLOBAL");
      break;
   case STB_WEAK:
      return ("WEAK");
      break;
   case STB_NUM:
      return ("NUM");
      break;
   case STB_LOOS:
      return ("LOOS");
      break;
   case STB_HIOS:
      return ("HIOS");
      break;
   case STB_LOPROC:
      return ("LOPROC");
      break;
   case STB_HIPROC:
      return ("HIPROC");
      break;
   default:
      return ("(unsupported)");
      break;
   }
   return ("(unsupported)");
}

/*
 * Gets a string corresponding to ELF64_ST_TYPE(Elf64_Sym->st_info)
 * \param code a possible value for ELF64_ST_TYPE(Elf64_Sym->st_info)
 * \return a string corresponding to the value of ELF64_ST_TYPE(Elf64_Sym->st_info)
 */
static const char* tostring_TYPE_st_info(int code)
{
   switch (code) {
   case STT_NOTYPE:
      return ("NOTYPE");
      break;
   case STT_OBJECT:
      return ("OBJECT");
      break;
   case STT_FUNC:
      return ("FUNC");
      break;
   case STT_SECTION:
      return ("SECTION");
      break;
   case STT_FILE:
      return ("FILE");
      break;
   case STT_COMMON:
      return ("COMMON");
      break;
   case STT_TLS:
      return ("TLS");
      break;
   case STT_NUM:
      return ("NUM");
      break;
   case STT_LOOS:
      return ("LOOS");
      break;
   case STT_HIOS:
      return ("HIOS");
      break;
   case STT_LOPROC:
      return ("LOPROC");
      break;
   case STT_HIPROC:
      return ("HIPROC");
      break;
   default:
      return ("(unsupported)");
      break;
   }
   return ("(unsupported)");
}

/*
 * Gets a string corresponding to ELF64_ST_VISIBILITY(Elf64_Sym->st_other)
 * \param code a possible value for ELF64_ST_VISIBILITY(Elf64_Sym->st_other)
 * \return a string corresponding to the value of ELF64_ST_VISIBILITY(Elf64_Sym->st_other)
 */
static const char* tostring_VISIBILITY_st_other(int code)
{
   switch (code) {
   case STV_DEFAULT:
      return ("DEFAULT");
      break;
   case STV_INTERNAL:
      return ("INTERNAL");
      break;
   case STV_HIDDEN:
      return ("HIDDEN");
      break;
   case STV_PROTECTED:
      return ("PROTECTED");
      break;
   default:
      return ("(unsupported)");
      break;
   }
   return ("(unsupported)");
}

/*
 * Prints an Elf64_Ehdr structure
 * \param elfheader an ELF header to print
 */
static void print_Elf64_Ehdr(Elf64_Ehdr *elfheader)
{
   int i = 0;
   printf("ELF Header -----------------------------------------------\n");
   printf("    Magic: ");
   for (i = 0; i < EI_NIDENT; i++)
      printf("%02x ", elfheader->e_ident[i]);
   printf("\n");

   printf("    Class:                                     %s\n",
         tostring_e_ident_EI_CLASS(elfheader->e_ident[EI_CLASS]));
   printf("    Data:                                      %s\n",
         tostring_e_ident_EI_DATA(elfheader->e_ident[EI_DATA]));
   printf("    Version:                                   %s\n",
         (elfheader->e_version == EV_CURRENT) ? "1 (current)" : "0 (error)");
   printf("    OS/ABI:                                    %s\n",
         tostring_e_ident_EI_OSABI(elfheader->e_ident[EI_OSABI]));
   printf("    Type:                                      %s\n",
         tostring_e_type(elfheader->e_type));
   printf("    Machine:                                   %s\n",
         tostring_e_machine(elfheader->e_machine));

   printf("    ELF header size:                           %d\n",
         elfheader->e_ehsize);
   printf("    Entry point address:                       %#"PRIx64"\n",
         elfheader->e_entry);
   printf("    Program headers address:                   %#"PRIx64"\n",
         elfheader->e_phoff);
   printf("    Program headers entry size                 0x%x\n",
         elfheader->e_phentsize);
   printf("    Program headers entry count                %d\n",
         elfheader->e_phnum);
   printf("    Section headers address:                   %#"PRIx64"\n",
         elfheader->e_shoff);
   printf("    Section headers entry size                 0x%x\n",
         elfheader->e_shentsize);
   printf("    Section headers entry count                %d\n",
         elfheader->e_shnum);
   printf("    Index of string table section header       %d\n",
         elfheader->e_shstrndx);
   printf("\n");
}

/*
 * Prints an Elf32_Ehdr structure
 * \param elfheader an ELF header to print
 */
static void print_Elf32_Ehdr(Elf32_Ehdr *elfheader)
{
   int i = 0;
   printf("ELF Header -----------------------------------------------\n");
   printf("    Magic: ");
   for (i = 0; i < EI_NIDENT; i++)
      printf("%02x ", elfheader->e_ident[i]);
   printf("\n");

   printf("    Class:                                     %s\n",
         tostring_e_ident_EI_CLASS(elfheader->e_ident[EI_CLASS]));
   printf("    Data:                                      %s\n",
         tostring_e_ident_EI_DATA(elfheader->e_ident[EI_DATA]));
   printf("    Version:                                   %s\n",
         (elfheader->e_version == EV_CURRENT) ? "1 (current)" : "0 (error)");
   printf("    OS/ABI:                                    %s\n",
         tostring_e_ident_EI_OSABI(elfheader->e_ident[EI_OSABI]));
   printf("    Type:                                      %s\n",
         tostring_e_type(elfheader->e_type));
   printf("    Machine:                                   %s\n",
         tostring_e_machine(elfheader->e_machine));

   printf("    ELF header size:                           %d\n",
         elfheader->e_ehsize);
   printf("    Entry point address:                       %#"PRIx32"\n",
         elfheader->e_entry);
   printf("    Program headers address:                   %#"PRIx32"\n",
         elfheader->e_phoff);
   printf("    Program headers entry size                 0x%x\n",
         elfheader->e_phentsize);
   printf("    Program headers entry count                %d\n",
         elfheader->e_phnum);
   printf("    Section headers address:                   %#"PRIx32"\n",
         elfheader->e_shoff);
   printf("    Section headers entry size                 0x%x\n",
         elfheader->e_shentsize);
   printf("    Section headers entry count                %d\n",
         elfheader->e_shnum);
   printf("    Index of string table section header       %d\n",
         elfheader->e_shstrndx);
   printf("\n");
}

/*
 * Prints an Elf64_Ehdr structure
 * \param elf an ELF structure whose header is to print
 */
static void print_Elf_Ehdr(Elf *elf)
{
   if (elf == NULL) {
      fprintf(stdout, "print_Elf64_Ehdr: elf NULL\n");
      return;
   }
   if (elf64_getehdr(elf) != NULL)
      print_Elf64_Ehdr(elf64_getehdr(elf));
   else if (elf32_getehdr(elf) != NULL)
      print_Elf32_Ehdr(elf32_getehdr(elf));
}

/*
 * Prints an Elf64_Shdr structure
 * \param elfheader an ELF section header to print
 * \param scn_name name of the section
 * \param i index of the section header in the array
 */
static void print_Elf64_Shdr(Elf64_Shdr* scnheader, const char* scn_name, int i)
{
   const char* scn_type = tostring_sh_type(scnheader->sh_type);

   //	     [idx] Name Type  Address      Offset         Size           End         EntSz          Align     Link Info  Flags
   printf(
         "[%3d] %s %s 0x%-10"PRIx64"  0x%-8"PRIx64" 0x%-8"PRIx64" 0x%-8"PRIx64"  0x%-4"PRIx64" %-5"PRId64" %-4d %-4d ",
         i, scn_name, scn_type, scnheader->sh_addr, scnheader->sh_offset,
         scnheader->sh_size, scnheader->sh_size + scnheader->sh_addr,
         scnheader->sh_entsize, scnheader->sh_addralign, scnheader->sh_link,
         scnheader->sh_info);
   if (scnheader->sh_flags & SHF_WRITE)
      printf("W");
   if (scnheader->sh_flags & SHF_ALLOC)
      printf("A");
   if (scnheader->sh_flags & SHF_EXECINSTR)
      printf("X");
   if (scnheader->sh_flags & SHF_MERGE)
      printf("M");
   if (scnheader->sh_flags & SHF_STRINGS)
      printf("S");
   if (scnheader->sh_flags & SHF_INFO_LINK)
      printf("I");
   if (scnheader->sh_flags & SHF_LINK_ORDER)
      printf("L");
   if (scnheader->sh_flags & SHF_GROUP)
      printf("G");
   if (scnheader->sh_flags & SHF_TLS)
      printf("T");
   if (scnheader->sh_flags & SHF_EXCLUDE)
      printf("E");
   if (scnheader->sh_flags & SHF_OS_NONCONFORMING)
      printf("O");
   if (scnheader->sh_flags & SHF_MASKOS)
      printf("o");
   if (scnheader->sh_flags & SHF_MASKPROC)
      printf("p");
   printf("\n");
}

/*
 * Prints an Elf32_Shdr structure
 * \param elfheader an ELF section header to print
 * \param scn_name name of the section
 * \param i index of the section header in the array
 */
static void print_Elf32_Shdr(Elf32_Shdr* scnheader, const char* scn_name, int i)
{
   const char* scn_type = tostring_sh_type(scnheader->sh_type);

   //      [idx] Name Type  Address      Offset         Size           End         EntSz          Align     Link Info  Flags
   printf(
         "[%3d] %s %s 0x%-10"PRIx32"  0x%-8"PRIx32" 0x%-8"PRIx32" 0x%-8"PRIx32"  0x%-4"PRIx32" %-5"PRId32" %-4d %-4d ",
         i, scn_name, scn_type, scnheader->sh_addr, scnheader->sh_offset,
         scnheader->sh_size, scnheader->sh_size + scnheader->sh_addr,
         scnheader->sh_entsize, scnheader->sh_addralign, scnheader->sh_link,
         scnheader->sh_info);
   if (scnheader->sh_flags & SHF_WRITE)
      printf("W");
   if (scnheader->sh_flags & SHF_ALLOC)
      printf("A");
   if (scnheader->sh_flags & SHF_EXECINSTR)
      printf("X");
   if (scnheader->sh_flags & SHF_MERGE)
      printf("M");
   if (scnheader->sh_flags & SHF_STRINGS)
      printf("S");
   if (scnheader->sh_flags & SHF_INFO_LINK)
      printf("I");
   if (scnheader->sh_flags & SHF_LINK_ORDER)
      printf("L");
   if (scnheader->sh_flags & SHF_GROUP)
      printf("G");
   if (scnheader->sh_flags & SHF_TLS)
      printf("T");
   if (scnheader->sh_flags & SHF_EXCLUDE)
      printf("E");
   if (scnheader->sh_flags & SHF_OS_NONCONFORMING)
      printf("O");
   if (scnheader->sh_flags & SHF_MASKOS)
      printf("o");
   if (scnheader->sh_flags & SHF_MASKPROC)
      printf("p");
   printf("\n");
}

/*
 * Prints an Elf_Shdr structure
 * \param elfheader an ELF section header to print
 * \param scn_name name of the section
 * \param i index of the section header in the array
 */
static void print_Elf_Shdr(Elf_Scn* scn, const char* scn_name, int i)
{
   if (scn == NULL) {
      fprintf(stdout, "print_Elf_Shdr: scn NULL\n");
      return;
   } else if (elf64_getshdr(scn))
      print_Elf64_Shdr(elf64_getshdr(scn), scn_name, i);
   else if (elf32_getshdr(scn))
      print_Elf32_Shdr(elf32_getshdr(scn), scn_name, i);
}

/*
 * Prints an Elf64_Phdr structure
 * \param seg an ELF program header to print
 * \param i index of the program header in the array
 */
static void print_Elf64_Phdr(Elf64_Phdr* seg, int i)
{
   //      [idx] Type     Offset      VirtAddress     PhysAddr       Filesize       Memsize       EndVirt       Align         Flags
   printf(
         "[%3d] %-14s 0x%-8"PRIx64" 0x%-13"PRIx64" 0x%-13"PRIx64" 0x%-9"PRIx64" 0x%-9"PRIx64" 0x%-13"PRIx64" 0x%-6"PRIx64" ",
         i, tostring_p_type(seg[i].p_type), seg[i].p_offset, seg[i].p_vaddr,
         seg[i].p_paddr, seg[i].p_filesz, seg[i].p_memsz,
         seg[i].p_memsz + seg[i].p_vaddr, seg[i].p_align);

   if (seg[i].p_flags & PF_R)
      printf("R");
   if (seg[i].p_flags & PF_W)
      printf("W");
   if (seg[i].p_flags & PF_X)
      printf("X");
   if (seg[i].p_flags & PF_MASKPROC)
      printf("p");
   if (seg[i].p_flags & PF_MASKOS)
      printf("o");
   printf("\n");
}

/*
 * Prints an Elf32_Phdr structure
 * \param seg an ELF program header to print
 * \param i index of the program header in the array
 */
static void print_Elf32_Phdr(Elf32_Phdr* seg, int i)
{
   //      [idx] Type     Offset      VirtAddress     PhysAddr       Filesize       Memsize       EndVirt       Align         Flags
   printf(
         "[%3d] %-14s 0x%-8"PRIx32" 0x%-13"PRIx32" 0x%-13"PRIx32" 0x%-9"PRIx32" 0x%-9"PRIx32" 0x%-13"PRIx32" 0x%-6"PRIx32" ",
         i, tostring_p_type(seg[i].p_type), seg[i].p_offset, seg[i].p_vaddr,
         seg[i].p_paddr, seg[i].p_filesz, seg[i].p_memsz,
         seg[i].p_memsz + seg[i].p_vaddr, seg[i].p_align);

   if (seg[i].p_flags & PF_R)
      printf("R");
   if (seg[i].p_flags & PF_W)
      printf("W");
   if (seg[i].p_flags & PF_X)
      printf("X");
   if (seg[i].p_flags & PF_MASKPROC)
      printf("p");
   if (seg[i].p_flags & PF_MASKOS)
      printf("o");
   printf("\n");
}

/*
 <<<<<<< HEAD
 * Prints a segment, using data from the Elf_Phdr structure for what is not stored into the relevant binseg_t structure
 * \param bf Binary file
 * \param elf Structure representing the ELF file
 * \param i index of the program header in the array
 */
static void print_Elf_Phdr(binfile_t* bf, Elf* elf, int i)
{
   if (elf == NULL) {
      fprintf(stdout, "print_Elf64_Phdr: seg NULL\n");
      return;
   }
   binseg_t* seg = binfile_get_seg(bf, i);
   printf(
         "[%3d] %-14s 0x%-8"PRIx64" 0x%-13"PRIx64" 0x%-13"PRIx64" 0x%-9"PRIx64" 0x%-9"PRIx64" 0x%-13"PRIx64" 0x%-6"PRIx64" ",
         i, tostring_p_type(Elf_Phdr_get_p_type(elf, i)), binseg_get_offset(seg),
         binseg_get_addr(seg), Elf_Phdr_get_p_paddr(elf, i),
         binseg_get_fsize(seg), binseg_get_msize(seg), binseg_get_end_addr(seg),
         Elf_Phdr_get_p_align(elf, i));

   if (binseg_check_attrs(seg, SCNA_READ))
      printf("R");
   if (binseg_check_attrs(seg, SCNA_WRITE))
      printf("W");
   if (binseg_check_attrs(seg, SCNA_EXE))
      printf("X");
   if (Elf_Phdr_get_p_flags(elf, i) & PF_MASKPROC)
      printf("p");
   if (Elf_Phdr_get_p_flags(elf, i) & PF_MASKOS)
      printf("o");
   printf("\n");
//   if (elf64_getphdr(elf) != NULL)
//      print_Elf64_Phdr (elf64_getphdr(elf), i);
//   else if (elf32_getphdr(elf) != NULL)
//      print_Elf32_Phdr (elf32_getphdr(elf), i);
}

/*
 * Prints an Elf64_Sym structure
 * \param sym an ELF symbol to print
 * \param sym_name name linked to the symbol
 * \param i index of the symbole in the array
 */
static void print_Elf64_Sym(Elf64_Sym* sym, const char* sym_name, int i)
{
   if (sym == NULL) {
      fprintf(stdout, "print_Elf64_Sym: sym NULL\n");
      return;
   }
   const char* bind = tostring_BIND_st_info(ELF64_ST_BIND(sym->st_info));
   const char* type = tostring_TYPE_st_info(ELF64_ST_TYPE(sym->st_info));
   const char* vis = tostring_VISIBILITY_st_other(
         ELF64_ST_VISIBILITY(sym->st_other));

   //       Value:   Size Type        Bind     Vis    Ndx Name
   printf("  %-3d: 0x%-10"PRIx64" %-4"PRId64" %-10s %-10s %-10s ", i,
         sym->st_value, sym->st_size, type, bind, vis);
   if (sym->st_shndx == STN_UNDEF)
      printf("UND ");
   else if (sym->st_shndx == 65521)
      printf("ABS ");
   else
      printf("%-3d ", sym->st_shndx);
   printf("%s\n", sym_name);
}

/*
 * Prints an Elf32_Sym structure
 * \param sym an ELF symbol to print
 * \param sym_name name linked to the symbol
 * \param i index of the symbole in the array
 */
static void print_Elf32_Sym(Elf32_Sym* sym, const char* sym_name, int i)
{
   if (sym == NULL) {
      fprintf(stdout, "print_Elf32_Sym: sym NULL\n");
      return;
   }
   const char* bind = tostring_BIND_st_info(ELF32_ST_BIND(sym->st_info));
   const char* type = tostring_TYPE_st_info(ELF32_ST_TYPE(sym->st_info));
   const char* vis = tostring_VISIBILITY_st_other(
         ELF32_ST_VISIBILITY(sym->st_other));
   //       Value:   Size Type        Bind     Vis    Ndx Name
   printf("  %-3d: 0x%-10"PRIx32" %-4"PRId32" %-10s %-10s %-10s ", i,
         sym->st_value, sym->st_size, type, bind, vis);
   if (sym->st_shndx == STN_UNDEF)
      printf("UND ");
   else if (sym->st_shndx == 65521)
      printf("ABS ");
   else
      printf("%-3d ", sym->st_shndx);
   printf("%s\n", sym_name);
}
///////////////////////////////////////////////////////////////////////////////
//    Helper functions to print ELF (high level: Array or abstraction ...)   //
///////////////////////////////////////////////////////////////////////////////
/*
 * Prints the ELF header 
 * \param efile a structure representing an ELF file
 */
static void print_elf_header(elffile_t* efile)
{
   print_Elf_Ehdr(efile->elf);
}

/*
 * Prints the array of ELF section headers 
 * \param efile a structure representing an ELF file
 */
static void print_section_header(binfile_t* bf)
{
   int i;
   if (!bf || (binfile_get_format(bf) != BFF_ELF))
      return;
   elffile_t* efile = binfile_get_parsed_bin(bf);
   if (!efile)
      return;
   Elf *elf = efile->elf;
   if (!elf)
      return;/**\todo (2014-06-11) Instead of that we could use the structure from the libbin only in that case*/

   printf("\nSection headers ------------------------------------------\n");
   printf(
         "[idx] Name                 Type           Address       Offset     Size       End         EntSz  Align Link Info Flags\n");
   printf(
         "----------------------------------------------------------------------------------------------------------------------\n");
   for (i = 0; i < Elf_Ehdr_get_e_shnum(elf); i++) {
      int j;
      binscn_t* scn = binfile_get_scn(bf, i);
      char* tmp = binscn_get_name(scn);
      char scn_name[] = { ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
            ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', '\0' };

      if (tmp != NULL)
         for (j = 0; j < 20 && j < (int) strlen(tmp); j++)
            scn_name[j] = tmp[j];

      print_Elf_Shdr(elf_getscn(elf, i), scn_name, i);
   }
   printf(
         "----------------------------------------------------------------------------------------------------------------------\n");
   printf(
         "Key to Flags:\n"
               " W (write), A (alloc), X (execute), M (merge), S (strings), I (info), L (link order), G (group)\n"
               " T (TLS), E (exclude), O (extra OS processing required), o (OS specific), p (processor specific)\n");
}

/*
 * Prints the array of ELF program header 
 * \param efile a structure representing an ELF file
 */
static void print_segment_header(binfile_t* bf)
{
   int i;
   if (!bf || (binfile_get_format(bf) != BFF_ELF))
      return;
   elffile_t* efile = binfile_get_parsed_bin(bf);
   if (!efile)
      return;
   Elf *elf = efile->elf;
   if (!elf)
      return;/**\todo (2014-06-11) Instead of that we could use the structure from the libbin only in that case*/

   printf("\n\nProgram headers ------------------------------------------\n");
   printf(
         "[idx] Type           Offset     VirtAddress     PhysAddr        Filesize    Memsize     EndVirt         Align    Flags\n");
   printf(
         "----------------------------------------------------------------------------------------------------------------------\n");
   for (i = 0; i < binfile_get_nb_segments(bf); i++) {
      print_Elf_Phdr(bf, elf, i);
   }
   printf(
         "----------------------------------------------------------------------------------------------------------------------\n");
   printf(
         "Key to Flags:\n"
               " R (read), W (write), X (execute), o (OS specific), p (processor specific)\n\n");
}

/*
 * Prints the dynamic section content
 * \param efile a structure representing an ELF file
 */
static void print_dynamic_section(binfile_t* bf)
{
   if (!bf || (binfile_get_format(bf) != BFF_ELF))
      return;
   elffile_t* efile = binfile_get_parsed_bin(bf);
   if (!efile)
      return;/**\todo (2014-06-11) Instead of that we could use the structure from the libbin only in that case*/

   binscn_t* scn = binfile_get_scn(bf, efile->indexes[DYNAMIC_IDX]);
//   int strtab_idx = 0;
   uint32_t i;      //, lastextlib = 0;
   unsigned int wordsize = binfile_get_word_size(bf);

   // get the position of strtab 
//   for (i = 0; i < binscn_get_nb_entries(scn); i++)
//   {
//      tableentry_t* te = scn->table_entries [i];
//      if (te->entry_64 != NULL && te->entry_64->elfentry.dyn->d_tag == DT_STRTAB)
//      {
//         while (strtab_idx < efile->nsections
//         &&     Elf_Shdr_get_sh_addr (efile, strtab_idx, NULL) != te->entry_64->elfentry.dyn->d_un.d_ptr)
//            strtab_idx ++;
//      }
//      else if (te->entry_32 != NULL && te->entry_32->elfentry.dyn->d_tag == DT_STRTAB)
//      {
//         while (strtab_idx < efile->nsections
//         &&     Elf_Shdr_get_sh_addr (efile, strtab_idx, NULL) != te->entry_32->elfentry.dyn->d_un.d_ptr)
//            strtab_idx ++;
//      }
//   }

   printf("\n\nDynamic section at offset 0x%"PRIx64" contains %d entries:\n",
         binscn_get_offset(scn), binscn_get_nb_entries(scn));
   printf("  Tag            Type               Name/Value\n");
   for (i = 0; i < binscn_get_nb_entries(scn); i++) {
      data_t* entry = binscn_get_entry(scn, i);
      void* elfentry = binscn_get_entry_data(scn, i);
      int64_t val = -1;
      int64_t ptr = -1;
      const char* type = NULL;
      int isextlib = FALSE;
      /**\todo TODO (2014-06-11) Use macros here to avoid making the test at each iteration (cf. what is made when loading)*/
      if (wordsize == BFS_64BITS) {
         Elf64_Dyn* dyn = (Elf64_Dyn*) elfentry;
         type = tostring_d_tag_64(dyn, &ptr, &val);
         printf("  0x%-12"PRIx64" %-18s ", dyn->d_tag, type);
         isextlib = (dyn->d_tag == DT_NEEDED);
      } else if (wordsize == BFS_32BITS) {
         Elf32_Dyn* dyn = (Elf32_Dyn*) elfentry;
         type = tostring_d_tag_32(dyn, &ptr, &val);
         printf("  0x%-12"PRIx32" %-18s ", dyn->d_tag, type);
         isextlib = (dyn->d_tag == DT_NEEDED);
      }

      if (isextlib) {
//         char* str = &(((char*)efile->sections[strtab_idx]->bits)[val]);
         printf("Shared library: [%s]\n",
               data_get_string(pointer_get_data_target(data_get_pointer(entry))));
      } else if (ptr == -1)
         printf("%"PRId64"\n", val);
      else
         printf("0x%"PRIx64"\n", ptr);
   }
}

/*
 * Prints the mapping between sections and segments
 * \param efile a structure representing an ELF file
 */
static void print_mapping_segment_sections(binfile_t* bf)
{
   if (!bf || (binfile_get_format(bf) != BFF_ELF))
      return;
   uint16_t i;
   /**\todo TODO (2015-02-17) Move this to binfile*/
   printf("\nSection to Segment mapping:\n");
   printf("Segment idx | Sections\n");
   for (i = 0; i < binfile_get_nb_segments(bf); i++) {
      int size = 0;
      int max_size = 80;
      uint16_t j;
      binseg_t* seg = binfile_get_seg(bf, i);
      printf("[%3d]       | ", i);
      for (j = 0; j < binseg_get_nb_scns(seg); j++) {
         binscn_t* scn = binseg_get_scn(seg, j);
         char* tmp = binscn_get_name(scn);
//         Elf64_Off sh_off    = Elf_Shdr_get_sh_offset (elf, j);
//         Elf64_Off p_offset  = Elf_Phdr_get_p_offset  (elf, i);
//         Elf64_Xword p_memsz = Elf_Phdr_get_p_memsz   (elf, i);
//
//         if (sh_off >= p_offset
//         &&  sh_off < p_offset + p_memsz)
//         {
         if (tmp) {
            if (size + (int) strlen(tmp) > max_size) {
               printf("\n            | ");
               size = 0;
            }
            printf("%s  ", tmp);
            size += strlen(tmp);
//         }
         }
      }
      printf("\n");
   }
}

/*
 * Prints DYNSYM and SYMTAB sections
 * \param efile a structure representing an ELF file
 * \param dynstr_idx index of section containing names
 */
static void print_sym_tables(binfile_t* bf)
{
   if (!bf || (binfile_get_format(bf) != BFF_ELF))
      return;
   elffile_t* efile = binfile_get_parsed_bin(bf);
   if (!efile)
      return;/**\todo (2014-06-11) Instead of that we could use the structure from the libbin only in that case*/
   Elf *elf = efile->elf;
   if (!elf)
      return;/**\todo (2014-06-11) Instead of that we could use the structure from the libbin only in that case*/

   uint16_t i;
   //uint32_t labelidx = 0;
   unsigned int wordsize = binfile_get_word_size(bf);

   for (i = 0; i < binfile_get_nb_sections(bf); i++) {
      binscn_t* scn = binfile_get_scn(bf, i);

      if (binscn_get_type(scn) == SCNT_LABEL) {
         /**\todo (2014-06-11) Big ugly hack. As I have not yet found a satisfactory way to link an entry in the binscn_t
          * representation of a label section to the actual label, I'm using the array of labels from the binfile_t, using
          * the knowledge that it was built while parsing the label sections in the order into which they were found.
          * => (2014-07-04) I think that now I store the label to the entry. It's a bit hard to remember what I did before
          * my defense but I think I had the time to implement it.*/
         uint32_t j = 0;
//         int tabstr = -1;
//
//         if (Elf_Shdr_get_sh_type (NULL, 0, scn->scnhdr) == SHT_DYNSYM)
//            tabstr = dynstr_idx;
//         else
//            tabstr = Elf_Shdr_get_sh_link (NULL, 0, scn->scnhdr);

         printf("\n\nSymbol table '%s' contains %d entries:\n",
               binscn_get_name(scn), binscn_get_nb_entries(scn));
         printf(
               "  Num: Value        Size Type       Bind       Vis        Ndx Name\n");
         for (j = 0; j < binscn_get_nb_entries(scn); j++) {
            data_t* entry = binscn_get_entry(scn, j);
            //label_t* label = binfile_get_file_label(bf, labelidx);
            label_t* label = data_get_raw(entry);
            void* elfentry = binscn_get_entry_data(scn, j);
            /**\todo TODO (2014-06-11) Use macros here to avoid making the test at each iteration (cf. what is made when loading)*/
            if (wordsize == BFS_64BITS) {
               Elf64_Sym* sym = (Elf64_Sym*) elfentry;
//               char* sym_name = &(((char*)efile->sections[tabstr]->bits)[te->entry_64->elfentry.sym->st_name]);
               print_Elf64_Sym(sym, label_get_name(label), j);
            } else if (wordsize == BFS_32BITS) {
               Elf32_Sym* sym = (Elf32_Sym*) elfentry;
//               char* sym_name = &(((char*)efile->sections[tabstr]->bits)[te->entry_32->elfentry.sym->st_name]);
               print_Elf32_Sym(sym, label_get_name(label), j);
            }
            //labelidx++;
         }
      }
   }
}

/*
 * Prints RELA section
 * \param efile a structure representing an ELF file
 * \param dynstr_idx index of section containing names
 * \param symtab_idx index of the symbols table
 */
static void print_rela_section(binfile_t* bf)
{
   if (!bf || (binfile_get_format(bf) != BFF_ELF))
      return;
   elffile_t* efile = binfile_get_parsed_bin(bf);
   if (!efile)
      return;/**\todo (2014-06-11) Instead of that we could use the structure from the libbin only in that case*/
   Elf *elf = efile->elf;
   if (!elf)
      return;/**\todo (2014-06-11) Instead of that we could use the structure from the libbin only in that case*/

   // To print relocation table, we need the symbolic tab (symtab) and the
   // dynamic symbolic table (dynsym). Table addresses are saved in dynamic table.
   // Symtab contains symbols. The targeted symbol index is got using ELF64_R_SYM macro.
   // Once the index is got, the field <value> is got with the field st_value from symbol structure (Elf64_Sym), and the name
   // is given at the index st_name in dynsym.
   int i;
   unsigned int wordsize = binfile_get_word_size(bf);

   for (i = 0; i < binfile_get_nb_sections(bf); i++) {
      binscn_t* scn = binfile_get_scn(bf, i);
      if (binscn_get_type(scn) == SCNT_RELOC) {
         uint32_t j = 0;

         printf(
               "\n\nRelocation section '%s' at offset 0x%"PRIx64" contains %d entries:\n",
               binscn_get_name(scn), binscn_get_offset(scn),
               binscn_get_nb_entries(scn));
         printf(
               "  Offset         Info           Type                Sym. Value    Sym. Name + Addend\n");
         for (j = 0; j < binscn_get_nb_entries(scn); j++) {
            data_t* entry = binscn_get_entry(scn, j);
            void* elfentry = binscn_get_entry_data(scn, j);
            binrel_t* rel = data_get_binrel(entry);

            label_t* rellbl = binrel_get_label(rel);
            int64_t sym_value = label_get_addr(rellbl);
            char* sym_name = label_get_name(rellbl);

            pointer_t* relptr = binrel_get_pointer(rel);
            int64_t reldest = pointer_get_addr(relptr);

            if (wordsize == BFS_64BITS) {
               Elf64_Rela *elfrela = (Elf64_Rela*) elfentry;
//               int64_t sym_value = efile->sections[symtab_idx]->table_entries[ELF64_R_SYM(te->entry_64->elfentry.rela->r_info)]->entry_64->elfentry.sym->st_value;
//               char* sym_name = &(((char*)efile->sections[dynstr_idx]->bits)
//                     [efile->sections[symtab_idx]->table_entries[ELF64_R_SYM(te->entry_64->elfentry.rela->r_info)]->entry_64->elfentry.sym->st_name]);
               const char* type = tostring_TYPE_r_info(
                     ELF64_R_TYPE(elfrela->r_info),
                     Elf_Ehdr_get_e_machine(elf));

//               printf("  0x%-12"PRIx64" 0x%-12"PRIx64" %-19s 0x%-11"PRIx64" %s%s%"PRId64"\n",
//                     te->entry_64->elfentry.rela->r_offset, te->entry_64->elfentry.rela->r_info, type, sym_value, sym_name,
//                     ((te->entry_64->elfentry.rela->r_addend >= 0)? " + ":" - "),
//                     ((te->entry_64->elfentry.rela->r_addend >= 0)? te->entry_64->elfentry.rela->r_addend : -te->entry_64->elfentry.rela->r_addend));
               printf(
                     "  0x%-12"PRIx64" 0x%-12"PRIx64" %-19s 0x%-11"PRIx64" %s%s%"PRId64"\n",
                     reldest, elfrela->r_info, type, sym_value, sym_name,
                     ((elfrela->r_addend >= 0) ? " + " : " - "),
                     ((elfrela->r_addend >= 0) ?
                           elfrela->r_addend : -elfrela->r_addend));
            } else if (wordsize == BFS_32BITS) {
               Elf32_Rela *elfrela = (Elf32_Rela*) elfentry;
//               int64_t sym_value = efile->sections[symtab_idx]->table_entries[ELF32_R_SYM(te->entry_32->elfentry.rela->r_info)]->entry_32->elfentry.sym->st_value;
//               char* sym_name = &(((char*)efile->sections[dynstr_idx]->bits)
//                     [efile->sections[symtab_idx]->table_entries[ELF32_R_SYM(te->entry_32->elfentry.rela->r_info)]->entry_32->elfentry.sym->st_name]);
               const char* type = tostring_TYPE_r_info(
                     ELF32_R_TYPE(elfrela->r_info),
                     Elf_Ehdr_get_e_machine(elf));

               printf(
                     "  0x%-12"PRIx32" 0x%-12"PRIx32" %-19s 0x%-11"PRIx64" %s%s%"PRId32"\n",
                     (int32_t) reldest, elfrela->r_info, type, sym_value,
                     sym_name, ((elfrela->r_addend >= 0) ? " + " : " - "),
                     ((elfrela->r_addend >= 0) ?
                           elfrela->r_addend : -elfrela->r_addend));
//                     te->entry_32->elfentry.rela->r_offset, te->entry_32->elfentry.rela->r_info, type, sym_value, sym_name,
//                     ((te->entry_32->elfentry.rela->r_addend >= 0)? " + ":" - "),
//                     ((te->entry_32->elfentry.rela->r_addend >= 0)? te->entry_32->elfentry.rela->r_addend : -te->entry_32->elfentry.rela->r_addend));
            }
         }
      }
   }
}

/*
 * Prints VERSYM section
 * \param efile a structure representing an ELF file
 * \param indx index of the versym section to print
 */
static void print_versym64(binfile_t *bf, Elf* elf, int indx)
{
   /**\todo TODO (2014-07-07) Rewrite this function using only libbin functions
    * Also use macros to have only one body*/
   unsigned int i;
   uint64_t len = 0;
   binscn_t* scn = binfile_get_scn(bf, indx);
   unsigned char* bytes = binscn_get_data(scn, &len); //ELF_get_scn_bits (efile, indx);
//   Elf_Shdr* header = efile->sections[indx]->scnhdr;
   Elf64_Versym* versym = lc_malloc(Elf_Shdr_get_sh_size(elf, indx));
   memcpy(versym, bytes, Elf_Shdr_get_sh_size(elf, indx));

   printf(
         "\n\nVersym section '%s' at offset 0x%"PRIx64" contains %d entries:\n",
         binscn_get_name(scn), binscn_get_offset(scn),
         (int) (binscn_get_size(scn) / Elf_Shdr_get_sh_entsize(elf, indx)));
   printf(" Addr: %#"PRIx64"  Offset: %#"PRIx64"  Link: %d (%s)\n",
         binscn_get_addr(scn), binscn_get_offset(scn),
         Elf_Shdr_get_sh_link(elf, indx),
         binfile_get_scn_name(bf, Elf_Shdr_get_sh_link(elf, indx)));

   // For each line, print the current offset in the section, then 4 values.
   for (i = 0; i < (binscn_get_size(scn) / Elf_Shdr_get_sh_entsize(elf, indx));
         i++) {
      // First element of the line: offset
      if ((i % 4) == 0)
         printf(" %-4x:", i);
      printf("\t%d", versym[i]);

      // All line elements have been printed, go to a new line
      if ((i % 4) == 3)
         printf("\n");
   }
   printf("\n");
   lc_free(versym);
}

/*
 * Prints VERSYM section
 * \param efile a structure representing an ELF file
 * \param indx index of the versym section to print
 */
static void print_versym32(binfile_t *bf, Elf* elf, int indx)
{
   /**\todo TODO (2014-07-07) Rewrite this function using only libbin functions
    * Also use macros to have only one body*/
   unsigned int i;
   uint64_t len = 0;
   binscn_t* scn = binfile_get_scn(bf, indx);
   unsigned char* bytes = binscn_get_data(scn, &len); //ELF_get_scn_bits (efile, indx);
//   Elf_Shdr* header = efile->sections[indx]->scnhdr;
   Elf32_Versym* versym = lc_malloc(Elf_Shdr_get_sh_size(elf, indx));
   memcpy(versym, bytes, Elf_Shdr_get_sh_size(elf, indx));

   printf(
         "\n\nVersym section '%s' at offset 0x%"PRIx64" contains %d entries:\n",
         binscn_get_name(scn), binscn_get_offset(scn),
         (int) (binscn_get_size(scn) / Elf_Shdr_get_sh_entsize(elf, indx)));
   printf(" Addr: 0x%"PRIx64"  Offset: 0x%"PRIx64"  Link: %d (%s)\n",
         binscn_get_addr(scn), binscn_get_offset(scn),
         Elf_Shdr_get_sh_link(elf, indx),
         binfile_get_scn_name(bf, Elf_Shdr_get_sh_link(elf, indx)));

   // For each line, print the current offset in the section, then 4 values.
   for (i = 0; i < (binscn_get_size(scn) / Elf_Shdr_get_sh_entsize(elf, indx));
         i++) {
      // First element of the line: offset
      if ((i % 4) == 0)
         printf(" %-4x:", i);
      printf("\t%d", versym[i]);

      // All line elements have been printed, go to a new line
      if ((i % 4) == 3)
         printf("\n");
   }
   printf("\n");
   lc_free(versym);
}

/*
 * Prints VERSYM section
 * \param efile a structure representing an ELF file
 * \param indx index of the versym section to print
 */
static void print_versym(binfile_t *bf, Elf* elf, int indx)
{
   if (Elf_Ehdr_get_e_ident(elf)[EI_CLASS] == ELFCLASS64)
      print_versym64(bf, elf, indx);
   else if (Elf_Ehdr_get_e_ident(elf)[EI_CLASS] == ELFCLASS32)
      print_versym32(bf, elf, indx);
}

/*
 * Prints VERNEED section
 * \param efile a structure representing an ELF file
 * \param indx index of the verneed section to print
 */
static void print_verneed64(binfile_t *bf, Elf* elf, int indx)
{
   /**\todo TODO (2014-07-07) Rewrite this function using only libbin functions
    * Also use macros to have only one body*/
   uint64_t len = 0;
   binscn_t* scn = binfile_get_scn(bf, indx);
   unsigned char* bytes = binscn_get_data(scn, &len); //ELF_get_scn_bits (elf, indx);
   //Elf_Shdr* header = Elfefile->sections[indx]->1scnhdr;

   Elf64_Verneed verneed;
   Elf64_Vernaux vernaux;
   int posverneed = 0;
   int nextverneed = 0;

   printf("\n\nVersym section '%s' contains "/*%d*/" entries:\n",
         binscn_get_name(scn)
         /*(int)(header->sh_size / header->sh_entsize)*/);
   printf(" Addr: 0x%"PRIx64"  Offset: 0x%"PRIx64"  Link: %d (%s)\n",
         binscn_get_addr(scn), binscn_get_offset(scn),
         Elf_Shdr_get_sh_link(elf, indx),
         binfile_get_scn_name(bf, Elf_Shdr_get_sh_link(elf, indx)));
//         elfsection_name (efile, Elf_Shdr_get_sh_link (NULL, 0, header)));

   // For the moment, versyms and vernaux are not extracted from the Elf section.
   // The section begins with a verneed entry at offset 0. This entry is linked to the next one
   // using vn_next member which contains the offset of the next verneed entry.
   // The verneed entry has a member verneed.vn_aux which contains the offset from verneed start
   // of the first vernaux entry belonging to current verneed. Then the vernaux entry has a member
   // vna_next which contains the offset from current vernaux offset of the next offset. If
   // vna_next is equal to 0, this means there is no vernaux for current verneed.
   // If all this text is not explicit enough, just imagine that tables (verneed and vernaux) are melted
   // together as they are printed by readelf or current function.
   do {
      memcpy(&(verneed), &bytes[posverneed], sizeof(verneed));
      printf(" 0x%-5x: Version: %d  File: %s  Cnt: %d\n", posverneed,
            verneed.vn_version,
            (char*) binscn_get_data_at_offset(
                  binfile_get_scn(bf, Elf_Shdr_get_sh_link(elf, indx)),
                  verneed.vn_file), verneed.vn_cnt);
      int nextvernaux = verneed.vn_aux;
      int posvernaux = posverneed + nextvernaux;
      while (nextvernaux != 0) {
         memcpy(&(vernaux), &bytes[posvernaux], sizeof(vernaux));
         printf(" 0x%-5x:   Name: %s  Flags: %d  Version: %d\n", posvernaux,
               (char*) binscn_get_data_at_offset(
                     binfile_get_scn(bf, Elf_Shdr_get_sh_link(elf, indx)),
                     vernaux.vna_name), vernaux.vna_flags, vernaux.vna_other);
         nextvernaux = vernaux.vna_next;
         posvernaux += nextvernaux;
      }
      nextverneed = verneed.vn_next;
      posverneed += nextverneed;
   } while (nextverneed != 0);

}

/*
 * Prints VERNEED section
 * \param efile a structure representing an ELF file
 * \param indx index of the verneed section to print
 */
static void print_verneed32(binfile_t *bf, Elf* elf, int indx)
{
   /**\todo TODO (2014-07-07) Rewrite this function using only libbin functions
    * Also use macros to have only one body*/
   uint64_t len = 0;
   binscn_t* scn = binfile_get_scn(bf, indx);
   unsigned char* bytes = binscn_get_data(scn, &len); //ELF_get_scn_bits (efile, indx);
//   Elf_Shdr* header = efile->sections[indx]->scnhdr;
   Elf32_Verneed verneed;
   Elf32_Vernaux vernaux;
   int posverneed = 0;
   int nextverneed = 0;

   printf("\n\nVersym section '%s' contains "/*%d*/" entries:\n",
         binscn_get_name(scn)
         /*(int)(header->sh_size / header->sh_entsize)*/);
   printf(" Addr: 0x%"PRIx64"  Offset: 0x%"PRIx64"  Link: %d (%s)\n",
         binscn_get_addr(scn), binscn_get_offset(scn),
         Elf_Shdr_get_sh_link(elf, indx),
         binfile_get_scn_name(bf, Elf_Shdr_get_sh_link(elf, indx)));

   // For the moment, versyms and vernaux are not extracted from the Elf section.
   // The section begins with a verneed entry at offset 0. This entry is linked to the next one
   // using vn_next member which contains the offset of the next verneed entry.
   // The verneed entry has a member verneed.vn_aux which contains the offset from verneed start
   // of the first vernaux entry belonging to current verneed. Then the vernaux entry has a member
   // vna_next which contains the offset from current vernaux offset of the next offset. If
   // vna_next is equal to 0, this means there is no vernaux for current verneed.
   // If all this text is not explicit enough, just imagine that tables (verneed and vernaux) are melted
   // together as they are printed by readelf or current function.
   do {
      memcpy(&(verneed), &bytes[posverneed], sizeof(verneed));
      printf(" 0x%-5x: Version: %d  File: %s  Cnt: %d\n", posverneed,
            verneed.vn_version,
            (char*) binscn_get_data_at_offset(
                  binfile_get_scn(bf, Elf_Shdr_get_sh_link(elf, indx)),
                  verneed.vn_file), verneed.vn_cnt);
      int nextvernaux = verneed.vn_aux;
      int posvernaux = posverneed + nextvernaux;
      while (nextvernaux != 0) {
         memcpy(&(vernaux), &bytes[posvernaux], sizeof(vernaux));
         printf(" 0x%-5x:   Name: %s  Flags: %d  Version: %d\n", posvernaux,
               (char*) binscn_get_data_at_offset(
                     binfile_get_scn(bf, Elf_Shdr_get_sh_link(elf, indx)),
                     vernaux.vna_name), vernaux.vna_flags, vernaux.vna_other);
         nextvernaux = vernaux.vna_next;
         posvernaux += nextvernaux;
   }
      nextverneed = verneed.vn_next;
      posverneed += nextverneed;
   } while (nextverneed != 0);

}

/*
 * Prints VERNEED section
 * \param efile a structure representing an ELF file
 * \param indx index of the verneed section to print
 */
static void print_verneed(binfile_t* bf, Elf* elf, int indx)
{
   if (Elf_Ehdr_get_e_ident(elf)[EI_CLASS] == ELFCLASS64)
      print_verneed64(bf, elf, indx);
   else if (Elf_Ehdr_get_e_ident(elf)[EI_CLASS] == ELFCLASS32)
      print_verneed32(bf, elf, indx);
}
#if 0
/*
 * Looks for several sections index
 * \param efile a structure representing an ELF file
 * \param dynstr_idx used to return index of section containing names
 * \param symtab_idx used to return index of the symbols table
 */
static void lookfor_sections (elffile_t* efile, int* dynstr_idx, int* symtab_idx)
{
   int i;

   if ( efile->indexes[DYNAMIC_IDX] > -1 )
   {
      //The file contains a dynamic section: we retrieve the symbolic table and its linked string table from it
      elfsection_t* dynscn = efile->sections[efile->indexes[DYNAMIC_IDX]];

      for (i = 0; i < dynscn->nentries; i++)
      {
         tableentry_t* te = dynscn->table_entries[i];
         if (te->entry_64 != NULL)
         {
            if (te->entry_64->elfentry.dyn->d_tag == DT_SYMTAB)	// get the symtab index
            *symtab_idx = elffile_get_scnataddr(efile, te->entry_64->elfentry.dyn->d_un.d_ptr);
            else if (te->entry_64->elfentry.dyn->d_tag == DT_STRTAB)// get the strtab index
            *dynstr_idx = elffile_get_scnataddr(efile, te->entry_64->elfentry.dyn->d_un.d_ptr);
         }
         else if (te->entry_32 != NULL)
         {
            if (te->entry_32->elfentry.dyn->d_tag == DT_SYMTAB) // get the symtab index
            *symtab_idx = elffile_get_scnataddr(efile, te->entry_32->elfentry.dyn->d_un.d_ptr);
            else if (te->entry_32->elfentry.dyn->d_tag == DT_STRTAB)// get the strtab index
            *dynstr_idx = elffile_get_scnataddr(efile, te->entry_32->elfentry.dyn->d_un.d_ptr);
         }
      }
   }
   else
   {
      //The file does not contain a dynamic section (static executable or relocatable file): 
      //   we will use the section headers to retrieve the tables
      *symtab_idx = efile->indexes[SYM];
      if (*symtab_idx != -1)
         *dynstr_idx = efile->sections[*symtab_idx]->scnhdr->sh_link;
   }
}
#endif
/*
 * Prints the formatted contents of a parsed ELF file
 * \param asmf Structure containing a parsed ELF file
 * */
void elf_asmfile_print_binfile(asmfile_t* asmf)
{
   if ((asmf == NULL) || (asmfile_test_analyze(asmf, PAR_ANALYZE) != TRUE))
      return;
   binfile_t* bf = asmfile_get_binfile(asmf);
   elffile_t* efile = binfile_get_parsed_bin(bf);
   if (efile == NULL)
      return;/**\todo (2014-06-11) Instead of that we could use the structure from the libbin only in that case*/
   Elf* elf = efile->elf;
   if (!elf)
      return;/**\todo (2014-06-11) Instead of that we could use the structure from the libbin only in that case*/
   DBGMSG("Print ELF structures for file %s\n", binfile_get_file_name(bf));
//   int symtab_idx = -1;
//   int dynstr_idx = -1;
   int i;
   // Retrieves options
   int64_t options = (int64_t) asmfile_get_parameter(asmf, PARAM_MODULE_BINARY,
         PARAM_BINPRINT_OPTIONS);

   if (options & BINPRINT_OPTIONS_HDR)
   print_elf_header(efile);
   if (options & BINPRINT_OPTIONS_SCNHDR)
      print_section_header(bf);

   if ((options & BINPRINT_OPTIONS_SEGHDR) && (Elf_Ehdr_get_e_phnum(elf) > 0)) {
      print_segment_header(bf);
      print_mapping_segment_sections(bf);
   }

   if ((options & BINPRINT_OPTIONS_DYN) && (efile->indexes[DYNAMIC_IDX] != -1))
      print_dynamic_section(bf);

//   lookfor_sections (efile, &dynstr_idx, &symtab_idx);
//   if (symtab_idx != -1 && dynstr_idx != -1)
//   {
   if (options & BINPRINT_OPTIONS_REL)
      print_rela_section(bf);
   if (options & BINPRINT_OPTIONS_SYM)
      print_sym_tables(bf);
//   }

   if (options & BINPRINT_OPTIONS_VER) {
      for (i = 0; i < binfile_get_nb_sections(bf); i++) {
         if (Elf_Shdr_get_sh_type(elf, i) == SHT_GNU_verneed)
            print_verneed(bf, elf, i);
         if (Elf_Shdr_get_sh_type(elf, i) == SHT_GNU_versym)
            print_versym(bf, elf, i);
      }
   }
}

