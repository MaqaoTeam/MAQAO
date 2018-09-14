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

#ifndef _WIN32
#include <libgen.h>
#endif
#include <stdlib.h>
#include <inttypes.h>
#include "elf.h"
#include "dwarf.h"
#include "libelf.h"
#include "libdwarf.h"
#include "libmasm.h"
#include "libmcommon.h"
#include "DwarfLight.h"

// include all needed architectures
#ifdef _ARCHDEF_x86_64
extern void x86_64__dwarf_memloc_set_arch_reg (DwarfAPI* api, DwarfMemLoc* memloc, int index);
#endif
#ifdef _ARCHDEF_k1om
extern void k1om__dwarf_memloc_set_arch_reg (DwarfAPI* api, DwarfMemLoc* memloc, int index);
#endif
#ifdef _ARCHDEF_ia32
extern void ia32__dwarf_memloc_set_arch_reg (DwarfAPI* api, DwarfMemLoc* memloc, int index);
#endif

extern Dwarf_Signed _dwarf_decode_s_leb128(Dwarf_Small * leb128,
      Dwarf_Word * leb128_length);
static DwarfVar* __var_new(DwarfAPI* api, Dwarf_Die* d_die, DwarfFunction* func,
      DwarfFile* file);
static void __var_free(void* v);

/* ----------------------------- Data structures --------------------------- */
/*
 *	Structure containing DwarfAPI
 */
struct _DwarfAPI {
   queue_t *files; /**< All the source files compiled in the binary*/
   queue_t *lines; /**< All sources lines data*/
   Dwarf_Debug *dbg; /**< Dwarf debug structure*/
   hashtable_t* strct; /**< Hashtable containing DwarfStruct indexed by their offset
    in the Dwarf section*/
   Elf* elf; /**< Store input Elf file*/
   hashtable_t* functions; /**< Table of functions indexed by their address (lowpc)*/
   hashtable_t* functions_off; /**< Table of functions indexed by their offset in the Dwarf section*/
   hashtable_t* functions_linkname; /**< Table of functions indexed by their linkname (mangled name)*/
   DwarfFunction** fct_array; /**< An array of DwarfFunction sorted by lowpc*/
   asmfile_t* asmf; /**< Assembly file associated to dwarf data*/
   char* elf_name; /**< Name of the opened elf file corresponding to the entry file*/
   char* dwz_debug_str; /**< Content of corresponding dwz file or NULL if no dwz file*/
   unsigned int fct_array_size; /**< Size of fct_array array*/
   unsigned int dwz_debug_str_size; /**< Size of dwz_debug_str array*/
   char is_range; /**< Flag set to TRUE when ranges are computed*/
};

/*
 *	Structure containing DwarfFile
 *	Each DwarfFile is a compiled file
 */
struct _DwarfFile {
   hashtable_t* fcts_ao; /**< An hashtable containing functions indexed by their abstract objects*/

   char *name; /**< The current filename*/
   char *dir; /**< The directory where the source file has been compiled*/
   char *vendor; /**< Compiler vendor name, e.g. "GNU", "Intel"*/
   char *version; /**< Compiler version ,e.g. "4.4.2", "12.1.4"*/
   char *short_version; /**< */
   char *language; /**< Compiler language, e.g. "C", "C++", "Fortran"*/
   char *producer; /**< Compiler string saved in DWARF*/
   char ** filenames; /**< A list of file names */
   char * command_line_opts; /**< Extracted from ELF, contains command line data*/
   int64_t lowpc; /**< Low_pc member*/
   queue_t *functions; /**< Queue of (DwarfFunctions*) in this DwarfFile*/
   queue_t *global_var; /**< Queue of (DwarfVar*) in this DwarfFile*/
   Dwarf_Die *d_die; /**< Corresponding Dwarf DIE*/
   DwarfAPI* api; /**< Dwarf session the file belongs to*/
   long int off; /**< Offset of the CU in the section*/
   int filenames_size; /**< Number of elements in filenames*/
   int lang_code; /**< Code used to represent the language, defined by LANG_ macros*/
   int comp_code; /**< Code used to represent the compiler, defined by COMP_ macros*/
};

/*
 * Structure defining an inlined function
 */
struct _DwarfInlinedFunction {
   Dwarf_Die *d_die; /**< Corresponding Dwarf DIE*/
   DwarfFunction* function; /**< Function that has been inlined*/
   Dwarf_Ranges* ranges; /**< An array of ranges*/
   int64_t low_pc; /**< First address of the inlined function*/
   int64_t high_pc; /**< Last address of the inlined function*/
   int64_t abstract_origin;
   int nb_ranges; /**< Number of ranges*/
   int call_line; /**< */
   int call_column; /**< */
};

/*
 *	Structure defining a function
 */
struct _DwarfFunction {
   DwarfFile *file; /**< File containing the current function*/
   queue_t* ranges; /**< List of fct_range_t structures*/
   char *name; /**< Name of the function*/
   char *linkage_name; /**< Mangled name of the function (if available, or NULL)*/
   int64_t low_pc; /**< Address in the binary (start)*/
   int64_t high_pc; /**< Address in the binary (end)*/
   int64_t offset; /**< Offset of the DWARF entry*/
   int64_t abstract_origin; /**< Value attached to DW_AT_abstract_origin*/
   queue_t *parameters; /**< List of parameters (queue of DwarfVar *)*/
   queue_t *local_vars; /**< List of local variables (queue of DwarfVar *)*/
   queue_t *par; /**< List of functions representing parallel regions / loops*/
   queue_t *inlinedFunctions; /**< List of inlined functions in the current function*/
   struct _DwarfVar *ret; /**< Type of the function pointer (i.e. the return type value of this function)*/
   short flags; /**< Some flags*/
   int srcl; /**< Source line where the function is defined*/
   int decl_file; /**< Source file where the function is defined*/

   // C++
   char accessibility; /**< Accessibility of the function;
    0 = undefined,
    1 = public,
    2 = protected,
    3 = private*/
   // Do not use or modify
   Dwarf_Die *d_die; /**< Corresponding Dwarf DIE*/
};
#define DFUNC_NO_RET 0x0001

/*
 * Structure encapsulating the Dwarf_Line structure provided by libdwarf
 */
struct _DwarfLine {
   char *filename; /**< Filename source associated with the line*/
   Dwarf_Addr address; /**< Address associated with the line*/
   Dwarf_Unsigned no; /**< Line number associated with each line*/
};

/*
 *	Structure defining a variable
 */
struct _DwarfVar {
   char *name; /**< Name of the variable*/
   char *type; /**< Type of the variable (without attributes such as '*', 'const' ... ; e.g void*/
   DwarfStruct* struc; /**< If the variable is a structure, pointer on the DwarfStruct*/
   char *full_type; /**< Complete type of the variable; e.g. "static const char *filename"*/
   DwarfFunction *function; /**< Function containing the current variable*/
   DwarfFile* file; /**<*/
   int pointer; /**< Number of pointers. pointer = 2 if char **x, pointer = 3 if char ***x*/
   int array_size; /**< If the var is an array, this variable contains the size of the array, or 0 if not the case*/
   int state; /**< Flags used to store is the variable is constant, static ...*/
   DwarfMemLoc* location; /**< Used to store where parameters are located in memory*/
   // C++
   char accessibility; /**< Accessibility of the variable;
    0 = undefined,
    1 = public,
    2 = protected,
    3 = private*/
   int src_l; /**< Variable source line*/
   int src_c; /**< Variable source column*/

   // Used in structure members only
   int member_location;
   // Do no use or modify
   int type_cu_offset;
};
// Flags for state member
#define DL_CONST		0x0001	/**< Variable is constant*/
#define DL_STATIC		0x0002	/**< Variable is static*/

/*
 * Structure defining a source structure (struct, union ...)
 */
struct _DwarfStruct {
   char* name; /**< Name of the structure*/
   int size; /**< Size of the structure*/
   queue_t* members; /**< List of members*/
   char type; /**< Type of the source structure*/
};

/**
 *	Structure defining the memory location of a variable
 */
struct _DwarfMemLoc {
   DwarfMemLocType type; /**< Type of the memory location*/
   reg_t *reg; /**< Register*/
   union {
      Dwarf_Signed offset; /**< If the type is a register, mem = offset*/
      Dwarf_Addr address; /**< If the type is an address, mem = address*/
   } mem; /**< Memory location*/
   // Do not use or modify
   struct _DwarfVar *father; /**< The variable*/
   Dwarf_Unsigned index; /**< Index of the register*/
};

/* ----------------------------- Static functions -------------------------- */
/*
 * Extract a value from an attribute
 * \param form the attribute form
 * \param attr an attribute
 * \return the extrcated value
 */
static void* __dwarf_reader_attr_init_data(Dwarf_Half form,
      Dwarf_Attribute *attr, DwarfAPI* api)
{
   Dwarf_Error err;
   void* ret = NULL;
   unsigned long long offset;
   int dret = 0;
   (void) dret;

   switch (form) {
   case DW_FORM_strp:
   case DW_FORM_string:
      dret = dwarf_formstring(*attr, (char **) &ret, &err);
      if (ret == NULL)
         ret = (void*) "";
      break;

   case DW_FORM_data1:
   case DW_FORM_data2:
   case DW_FORM_data4:
   case DW_FORM_data8:
      dret = dwarf_formudata(*attr, (Dwarf_Unsigned *) &ret, &err);
      break;

   case DW_FORM_sdata:
      dret = dwarf_formsdata(*attr, (Dwarf_Signed *) &ret, &err);
      break;

   case DW_FORM_udata:
      dret = dwarf_formudata(*attr, (Dwarf_Unsigned *) &ret, &err);
      break;

   case DW_FORM_addr:
      dret = dwarf_formaddr(*attr, (Dwarf_Addr *) &ret, &err);
      break;

   case DW_FORM_flag:
   case DW_FORM_flag_present:
      dret = dwarf_formflag(*attr, (Dwarf_Bool *) &ret, &err);
      break;

   case DW_FORM_ref1:
   case DW_FORM_ref2:
   case DW_FORM_ref4:
   case DW_FORM_ref8:
   case DW_FORM_ref_udata:
      dret = dwarf_formref(*attr, (Dwarf_Off *) &ret, &err);
      break;

   case DW_FORM_ref_addr:
   case DW_FORM_sec_offset:
      dret = dwarf_global_formref(*attr, (Dwarf_Off *) &ret, &err);
      break;

   case DW_FORM_block1:
   case DW_FORM_block2:
   case DW_FORM_block4:
   case DW_FORM_block:
      dret = dwarf_formblock(*attr, (Dwarf_Block **) &ret, &err);
      break;

   case DW_FORM_ref_sig8:
      dret = dwarf_formsig8(*attr, (Dwarf_Sig8 *) &ret, &err);
      break;

   case DW_FORM_GNU_addr_index:
      //DBGMSG0 ("DW_FORM_GNU_addr_index\n");
      break;

   case DW_FORM_GNU_str_index:
      //DBGMSG0 ("DW_FORM_GNU_str_index\n");
      break;

   case DW_FORM_GNU_ref_alt:
      //DBGMSG0 ("DW_FORM_GNU_ref_alt\n");
      break;
      /*   
       case DW_FORM_GNU_strp_alt:
       dret = dwarf_formstring (*attr, (char **) &ret, &err);
       printf ("Current form is 0x%x !\n", form);
       printf ("%s\n", (char*)ret);
       break;
       */
   case DW_FORM_GNU_strp_alt:
      dret = dwarf_global_formref(*attr, &offset, &err);
      if (api->dwz_debug_str != NULL && offset >= 0
            && offset < api->dwz_debug_str_size) {
         return (&(api->dwz_debug_str[offset]));
      } else {
         return ((void*) "");
      }

      break;

   default:
      DBGMSG("!! Warning !! Unsupported DW_FORM value : %d\n", form)
      ;
      break;
   }
   return (ret);
}

/*
 * Creates a new file from a DIE
 * \param api current DWARF session
 * \param d_die a Dwarf DIE representing a file
 * \param off offset of the CU in the DWARF section
 * \return a function or NULL if there is a problem
 */
static DwarfFile* __file_new(DwarfAPI* api, Dwarf_Die* die, long int off)
{
   DwarfFile *file;                 // New file
   const char *str_ptr = NULL;      //
   char* str_ptr_space = NULL;      //
   char buffer[20];                 //
   unsigned int pos, len, count;    //
   Dwarf_Signed attrs_count = 0;        // Number of attributes
   Dwarf_Half form, attr;           // Data on an attribute
   Dwarf_Attribute *dwarf_attrs;    // DIE attributes
   int i = 0;                       // Counter
   int dwarf_lang;                  // Code used to store language (from DWARF)
   Dwarf_Error err;                 // Error handler

   dwarf_attrlist(*die, &dwarf_attrs, &attrs_count, &err);
   file = lc_malloc0(sizeof(DwarfFile));
   file->off = off;
   file->functions = queue_new();
   file->api = api;
   file->d_die = lc_malloc(sizeof(Dwarf_Die));
   file->fcts_ao = hashtable_new(&direct_hash, &direct_equal);
   memcpy(file->d_die, die, sizeof(Dwarf_Die));

   for (i = 0; i < attrs_count; i++) {
      dwarf_whatform(dwarf_attrs[i], &form, &err);
      dwarf_whatattr(dwarf_attrs[i], &attr, &err);

      switch (attr) {
      case DW_AT_low_pc:
         file->lowpc = (int64_t) __dwarf_reader_attr_init_data(form,
               &dwarf_attrs[i], api);
         break;
      case DW_AT_producer:
         file->producer = lc_strdup (__dwarf_reader_attr_init_data(form, &dwarf_attrs[i],
               api));
         break;
      case DW_AT_comp_dir:
         file->dir = lc_strdup(
               __dwarf_reader_attr_init_data(form, &dwarf_attrs[i], api));
         break;
      case DW_AT_name:
         file->name = lc_strdup(
               __dwarf_reader_attr_init_data(form, &dwarf_attrs[i], api));
         break;
      case DW_AT_language:
         dwarf_lang = (long int) __dwarf_reader_attr_init_data(form,
               &dwarf_attrs[i], api);
         switch (dwarf_lang) {
         case DW_LANG_Ada83:
         case DW_LANG_Ada95:
            file->language = "Ada";
            file->lang_code = LANG_ERR;
            break;

         case DW_LANG_C:
         case DW_LANG_C89:
         case DW_LANG_C99:
            file->language = "C";
            file->lang_code = LANG_C;
            break;

         case DW_LANG_C_plus_plus:
            file->language = "C++";
            file->lang_code = LANG_CPP;
            break;

         case DW_LANG_Cobol74:
         case DW_LANG_Cobol85:
            file->language = "Cobol";
            file->lang_code = LANG_ERR;
            break;

         case DW_LANG_D:
            file->language = "D";
            file->lang_code = LANG_ERR;
            break;

         case DW_LANG_Fortran90:
            file->language = "Fortran90";
            file->lang_code = LANG_FORTRAN;
            break;

         case DW_LANG_Fortran77:
            file->language = "Fortran77";
            file->lang_code = LANG_FORTRAN;
            break;

         case DW_LANG_Fortran95:
            file->language = "Fortran95";
            file->lang_code = LANG_FORTRAN;
            break;

         default:
            file->language = NULL;
            file->lang_code = LANG_ERR;
            break;
         }
         break;
      case 0x3b01:
         file->command_line_opts = lc_strdup (__dwarf_reader_attr_init_data(form, &dwarf_attrs[i], api));
         break;

      default:
    	 //printf ("DW_AT : 0x%x\n", attr);
         break;
      }
   }
   dwarf_dealloc(*(api->dbg), dwarf_attrs, DW_DLA_LIST);

   // Initialize
   file->vendor = NULL;
   file->version = NULL;

   // -- Get the vendor name by splitting the DW_AT_producer field at the first space " "
   str_ptr = strstr(file->producer, " ");
   pos = (int) (str_ptr - file->producer);

   // -- Check for buffer overflow... 20 bytes should be enough for vendor name, but who knows
   if (pos >= sizeof(buffer))
      pos = sizeof(buffer) - 1;

   strncpy(buffer, file->producer, pos);
   buffer[pos] = '\0';

   // We don't need the "(R)" ...
   if (!strcmp("Intel(R)", buffer))
      buffer[strlen("Intel")] = '\0';

   file->vendor = lc_strdup(buffer);

   // -- Get version
   if (!strcmp(file->vendor, "GNU")) {
      // GNU C 4.4.5
      // GNU Fortran 4.4.5
      // For GNU, this is always <GNU> <language> <version>. The version is always the 3rd argument
      file->comp_code = COMP_GNU;
      len = strlen(file->producer);

      for (pos = 0, count = 0; pos < len && count < 2; pos++) {
         if (file->producer[pos] == ' ')
            count++;
      }

      if (pos == len) {   // Version not found
         file->version = lc_strdup("Version not found");
      } else {
         str_ptr = &(file->producer[pos]);
         file->version = lc_strdup(str_ptr);
      }
   } else if (!strcmp(file->vendor, "Intel")) {
      // e.g. Intel(R) C Intel(R) 64 Compiler XE for applications running on Intel(R) 64, Version 12.1.4.319 Build ...
      file->comp_code = COMP_INTEL;
      str_ptr = strstr(file->producer, "Version ");

      if (str_ptr == NULL) {   // No version found
         file->version = lc_strdup("Version not found");
      } else {   // Get the version number after the word "Version "
         str_ptr += strlen("Version ");
         str_ptr_space = strstr(str_ptr, " ");
         len = (int) (str_ptr_space - str_ptr);
         file->version = strndup(str_ptr, len);
      }
   } else
      file->comp_code = COMP_ERR;

   if (file->version != NULL) {
      int i = 0;
      while (file->version[i] != ' ' && file->version[i] != '\0')
         i++;
      file->short_version = lc_malloc((i + 1) * sizeof(char));
      strncpy(file->short_version, file->version, i);
      file->short_version[i] = '\0';
   }

   return file;
}

/*
 * Frees a file structure
 * \param f a structure to free
 */
static void __file_free(void* f)
{
   DwarfFile* file = (DwarfFile*) f;
   hashtable_free(file->fcts_ao, NULL, NULL);

   if (file->vendor)
      lc_free(file->vendor);
   if (file->producer)
      lc_free(file->producer);
   if (file->name)
      lc_free(file->name);
   if (file->dir)
      lc_free(file->dir);
   if (file->d_die)
      lc_free(file->d_die);
   if (file->version)
      lc_free(file->version);
   if (file->global_var != NULL)
      queue_free(file->global_var, __var_free);
   if (file->filenames != NULL) {
      int i = 0;
      for (i = 0; i < file->filenames_size; i++)
         lc_free(file->filenames[i]);
      lc_free(file->filenames);
   }
   if (file->short_version)
      lc_free(file->short_version);

   lc_free(file);
}

static DwarfInlinedFunction* __inlinedFunction_new(DwarfAPI* api,
      Dwarf_Die *d_die, DwarfFile* file)
{
   DwarfInlinedFunction* ifunc = lc_malloc0(sizeof(DwarfInlinedFunction));
   Dwarf_Signed attrs_count = 0;     //!< Number of attributes
   Dwarf_Half form, attr;
   Dwarf_Attribute *dwarf_attrs; //!< libdwarf object
   Dwarf_Error err = NULL;
   int i = 0;

   dwarf_attrlist(*d_die, &dwarf_attrs, &attrs_count, &err);
   ifunc->d_die = lc_malloc0(sizeof(Dwarf_Die));
   memcpy(ifunc->d_die, d_die, sizeof(Dwarf_Die));
   ifunc->low_pc = -1;
   ifunc->high_pc = -1;
   ifunc->call_line = -1;
   ifunc->call_column = -1;

   for (i = 0; i < attrs_count; i++) {
      dwarf_whatform(dwarf_attrs[i], &form, &err);
      dwarf_whatattr(dwarf_attrs[i], &attr, &err);

      switch (attr) {
      case DW_AT_low_pc:
      case DW_AT_entry_pc:
         ifunc->low_pc = (Dwarf_Addr) __dwarf_reader_attr_init_data(form,
               &dwarf_attrs[i], api);
         break;
      case DW_AT_high_pc:
         ifunc->high_pc = (Dwarf_Addr) __dwarf_reader_attr_init_data(form,
               &dwarf_attrs[i], api);
         break;
      case DW_AT_call_line:
         ifunc->call_line = (long) __dwarf_reader_attr_init_data(form,
               &dwarf_attrs[i], api);
         break;
      case DW_AT_call_column:
         ifunc->call_column = (long) __dwarf_reader_attr_init_data(form,
               &dwarf_attrs[i], api);
         break;
      case DW_AT_abstract_origin:
         ifunc->abstract_origin = (int64_t) __dwarf_reader_attr_init_data(form,
               &dwarf_attrs[i], api);
         break;

      case DW_AT_ranges: {
         Dwarf_Off ranges_off = (Dwarf_Off) __dwarf_reader_attr_init_data(form,
               &dwarf_attrs[i], api);
         Dwarf_Signed nb_ranges = 0;
         Dwarf_Ranges* ranges = NULL;
         dwarf_get_ranges(*(api->dbg), ranges_off, &ranges, &nb_ranges, NULL,
               &err);
         if (ranges == NULL)
            break;
         ifunc->ranges = lc_malloc((nb_ranges - 1) * sizeof(Dwarf_Ranges));
         memcpy(ifunc->ranges, ranges, (nb_ranges - 1) * sizeof(Dwarf_Ranges));
         ifunc->nb_ranges = nb_ranges - 1;
         dwarf_ranges_dealloc(*(api->dbg), ranges, nb_ranges);

         // Ranges are relative to file lowpc value, so add it to store ranges real addresses
         int ii = 0;
         for (ii = 0; ii < ifunc->nb_ranges; ii++) {
            ifunc->ranges[ii].dwr_addr1 += file->lowpc;
            ifunc->ranges[ii].dwr_addr2 += file->lowpc;
         }
         break;
      }
      default:
         break;
      }
   }
   dwarf_dealloc(*(api->dbg), dwarf_attrs, DW_DLA_LIST);

   if (file->comp_code == COMP_GNU && ifunc->high_pc > 0 && ifunc->low_pc > 0
         && str_compare_version("4.8", file->short_version) >= 0)
      ifunc->high_pc += ifunc->low_pc;

   return (ifunc);
}

static void __inlinedFunction_free(void* p)
{
   if (p == NULL)
      return;
   DwarfInlinedFunction* ifunc = (DwarfInlinedFunction*) p;
   if (ifunc->d_die != NULL)
      lc_free(ifunc->d_die);
   if (ifunc->ranges != NULL)
      lc_free(ifunc->ranges);
   lc_free(ifunc);
}

/*
 * Creates a new function from a DIE
 * \param api current DWARF session
 * \param d_die a Dwarf DIE representing a function
 * \param file the file the function belongs to
 * \return a function or NULL if there is a problem
 */
static DwarfFunction* __function_new(DwarfAPI* api, Dwarf_Die *d_die,
      DwarfFile* file)
{
   DwarfFunction* func = lc_malloc0(sizeof(DwarfFunction));
   Dwarf_Signed attrs_count = 0;		//!< Number of attributes
   Dwarf_Half form, attr;
   Dwarf_Attribute *dwarf_attrs;	//!< libdwarf object
   Dwarf_Error err;
   int i = 0;

   if (d_die == NULL)
      return NULL;

//   printf("%p, %p(%d)\n", &dwarf_attrs, &attrs_count, attrs_count);
   dwarf_attrlist(*d_die, &dwarf_attrs, &attrs_count, &err);
   func->d_die = lc_malloc0(sizeof(Dwarf_Die));
   memcpy(func->d_die, d_die, sizeof(Dwarf_Die));
   func->file = file;
   func->par = queue_new();
   func->inlinedFunctions = queue_new();
   func->low_pc = -1;
   func->abstract_origin = -1;
   func->decl_file = -1;
   func->ranges = queue_new();
   dwarf_die_CU_offset(*d_die, (Dwarf_Off*) (&(func->offset)), &err);
   hashtable_insert(api->functions_off, (void*) (func->offset), func);
//   printf("%p, %p(%d)\n", &dwarf_attrs, &attrs_count, attrs_count);
   for (i = 0; i < attrs_count; i++) {
//      if (i < attrs_count) 
//         printf("WTF\n"); 
//      else 
//         printf("Apprends à comparer\n");
//      printf("%d(%d)\n", i, attrs_count);
//      fflush(stdout);
      dwarf_whatform(dwarf_attrs[i], &form, &err);
      dwarf_whatattr(dwarf_attrs[i], &attr, &err);

      switch (attr) {
      case DW_AT_name:
         func->name = lc_strdup(
               __dwarf_reader_attr_init_data(form, &dwarf_attrs[i], api));
         break;
      case DW_AT_accessibility:
         func->accessibility = (long int) __dwarf_reader_attr_init_data(form,
               &dwarf_attrs[i], api);
         break;
      case DW_AT_MIPS_linkage_name:
      case DW_AT_linkage_name:
         func->linkage_name = lc_strdup(
               __dwarf_reader_attr_init_data(form, &dwarf_attrs[i], api));
         if (func->linkage_name != NULL)
            hashtable_insert(api->functions_linkname, (char*)func->linkage_name, func);
         break;
      case DW_AT_low_pc:
         func->low_pc = (Dwarf_Addr) __dwarf_reader_attr_init_data(form,
               &dwarf_attrs[i], api);
         hashtable_insert(api->functions, (void*) (func->low_pc), func);
         break;
      case DW_AT_high_pc:
         func->high_pc = (Dwarf_Addr) __dwarf_reader_attr_init_data(form,
               &dwarf_attrs[i], api);
         if (form == DW_FORM_data8 || form == DW_FORM_data4 || form == DW_FORM_sec_offset) //Handling the case where high_pc is an offset
            func->high_pc += func->low_pc;
         break;
      case DW_AT_decl_line:
         func->srcl = (Dwarf_Addr) __dwarf_reader_attr_init_data(form,
               &dwarf_attrs[i], api);
         break;
      case DW_AT_decl_file:
         func->decl_file = (Dwarf_Addr) __dwarf_reader_attr_init_data(form,
               &dwarf_attrs[i], api);
         break;
      case DW_AT_abstract_origin:
         func->abstract_origin = (Dwarf_Addr) __dwarf_reader_attr_init_data(
               form, &dwarf_attrs[i], api);
         hashtable_insert(file->fcts_ao, (void*) (func->abstract_origin), func);
         break;
      default:
         break;
      }
//      printf("%d(%d)\n", i, attrs_count);
   }
   dwarf_dealloc(*(api->dbg), dwarf_attrs, DW_DLA_LIST);

   //dwarf_function_get_returned_var (func);
   dwarf_function_get_parameters(func);

// The code below should not be needed any more now that whether high_pc is an address or an offset is directly detected through its type
//   if (file->comp_code == COMP_GNU && func->high_pc > 0 && func->low_pc > 0
//         && str_compare_version("4.8", file->short_version) >= 0)
//      func->high_pc += func->low_pc;

   // Iterates over function DIE child to get sub functions added by Intel compilers
   // These sub functions are parallel regions and parallel loops.
   Dwarf_Die child_die, sibling_die; // Used to iterate in the DIE tree
   Dwarf_Half tagval = 0;           // Tag of a DIE
   if (dwarf_child(*(func->d_die), &child_die, &err) == DW_DLV_OK) {
      sibling_die = child_die;
      do {
         child_die = sibling_die;
         dwarf_tag(child_die, &tagval, &err);

         // Look into DWARF for more data about the parameter
         if (tagval == DW_TAG_subprogram) {
            DwarfFunction* df = __function_new(api, &child_die, file);
            queue_add_tail(func->par, df);
            queue_add_tail(file->functions, df);
         } else if (tagval == DW_TAG_inlined_subroutine) {
            DwarfInlinedFunction* idf = __inlinedFunction_new(api, &child_die,
                  file);
            switch (file->comp_code) {
            case COMP_GNU:
               if (idf->nb_ranges > 0) {
                  int i = 0;
                  for (i = 0; i < idf->nb_ranges; i++) {
                     idf->ranges[i].dwr_addr1 += file->lowpc;
                     idf->ranges[i].dwr_addr2 += file->lowpc;
                  }
               }
               break;
            }
            queue_add_tail(func->inlinedFunctions, idf);
         }
      } while (dwarf_siblingof_b(*(func->file->api->dbg), child_die, TRUE,
            &sibling_die, &err) == DW_DLV_OK);
   }
   return (func);
}

/*
 * Frees a function structure
 * \param f a structure to free
 */
static void __function_free(void* f)
{
   DwarfFunction* func = (DwarfFunction*) f;
   if (func->name)
      lc_free(func->name);
   if (func->d_die)
      lc_free(func->d_die);
   if (func->linkage_name)
      lc_free(func->linkage_name);
   if (func->parameters)
      queue_free(func->parameters, __var_free);
   if (func->ret != NULL)
      __var_free(func->ret);
   if (func->local_vars != NULL)
      queue_free(func->local_vars, __var_free);
   if (func->ranges != NULL)
      queue_free(func->ranges, &fct_range_free);
   if (func->par != NULL)
      queue_free(func->par, NULL);
   if (func->inlinedFunctions != NULL)
      queue_free(func->inlinedFunctions, &__inlinedFunction_free);
   lc_free(func);
}

/*
 * Allocate a register in a memloc
 * \param api current DWARF session
 * \param memloc the memory location
 * \param index the register index
 */
static void _dwarf_memloc_set_arch_reg(DwarfAPI* api, DwarfMemLoc* memloc,
      int index)
{
   // There is a list of registers in x86_64_arch library
   // Here is the array of indexes to convert the dwarf registers to the maqao registers

   switch (elf_getmachine(api->elf)) {
#ifdef _ARCHDEF_x86_64
   case EM_X86_64:
   x86_64__dwarf_memloc_set_arch_reg (api, memloc, index);
   break;
#endif
#ifdef _ARCHDEF_k1om
   case EM_K1OM:
   k1om__dwarf_memloc_set_arch_reg (api, memloc, index);
   break;
#endif
#ifdef _ARCHDEF_ia32
   case EM_386:
   ia32__dwarf_memloc_set_arch_reg (api, memloc, index);
   break;
#endif
   default:
      break;
   }
}

/*
 * Sets a register in a memory location
 * \param memloc a memory location
 * \param reg a register
 */
void dwarf_memloc_set_reg(DwarfMemLoc* memloc, reg_t* reg)
{
   if (memloc == NULL)
      return;
   memloc->reg = reg;
}

/*
 * Allocate a new DwarfMemLoc which describe a memory location
 * \param api current DWARF session
 * \param data an array which contains the description of the memory location
 * \param len the length of the array data
 */
static DwarfMemLoc* _dwarf_memloc_new(DwarfAPI* api, Dwarf_Small *data, int len)
{
   Dwarf_Unsigned type = data[0];
   Dwarf_Word size;
   DwarfMemLoc* memloc = lc_malloc0(sizeof(DwarfMemLoc));

//   switch (type)
//   {
//   case DW_OP_reg0 ... DW_OP_reg31:
   if (type >= DW_OP_reg0 && type <= DW_OP_reg31) {
      // Simple register
      memloc->type = DWARF_REG;
      memloc->index = type - DW_OP_reg0;
      memloc->mem.offset = 0x0;
      _dwarf_memloc_set_arch_reg(api, memloc, memloc->index);
   }
//   break;

//   case DW_OP_breg0 ... DW_OP_breg31:
   else if (type >= DW_OP_breg0 && type <= DW_OP_breg31) {
      // This is a base register
      memloc->type = DWARF_BREG;
      memloc->index = type - DW_OP_breg0;
      memloc->mem.offset = _dwarf_decode_s_leb128(&data[1], &size);
      _dwarf_memloc_set_arch_reg(api, memloc, memloc->index);
   }
//   break;

//   case DW_OP_regx:
   else if (type == DW_OP_regx) {
      // This is a register X, get the n° of the register, no offset
      memloc->type = DWARF_REG;
      memloc->mem.offset = 0x0;
      memloc->index = data[1];
      _dwarf_memloc_set_arch_reg(api, memloc, memloc->index);
   }
//      break;

//   case DW_OP_bregx:
   else if (type == DW_OP_bregx) {
      // This is a base register X
      memloc->type = DWARF_BREG;
      memloc->index = data[1]; // = X
      memloc->mem.offset = data[2];
      _dwarf_memloc_set_arch_reg(api, memloc, memloc->index);
   }
//      break;

//   case DW_OP_fbreg:
   else if (type == DW_OP_fbreg) {
      // This is a frame based register
      memloc->type = DWARF_FBREG_TBRES;
      memloc->mem.offset = _dwarf_decode_s_leb128(&data[1], &size);
      memloc->reg = NULL;

      // The register will be resolved later
   }
//      break;

//   case DW_OP_addr:
   else if (type == DW_OP_addr) {
      // This is a block data describing an adress (9 bytes => 1 for type, 8 for the adress)
      memloc->type = DWARF_ADDR;
      memloc->reg = NULL;
      memcpy(&memloc->mem.address, &data[1], len - 1);
   }
//      break;

//   default:
//      break;
//   }
   return memloc;
}

/**
 * Reads a memloc and return it
 * \param api current DWARF session
 * \param die The corresponding DwarfReaderDie of the variable
 */
static DwarfMemLoc* __read_memloc(DwarfAPI* api, Dwarf_Attribute* attr)
{
   Dwarf_Block *block;
   DwarfMemLoc *reg = NULL;
   Dwarf_Half form;        		// Form and value of an attribute
   Dwarf_Error err = 0;			// Error handler

   dwarf_whatform(*attr, &form, &err);

   // This is a simple location, there is only one location
   if (form == DW_FORM_block1) {
      block = __dwarf_reader_attr_init_data(form, attr, api);

      // Check the block length
      if (block->bl_len <= 0) {
         return NULL;
      }
      reg = _dwarf_memloc_new(api, block->bl_data, block->bl_len);
   }
   return (reg);
}

/*
 * Creates a new DwarfStruct from a DIE
 * \param api current DWARF session
 * \param d_die a Dwarf DIE representing a structure (union, struct ...)
 * \param func a function the structure can be linked
 * \param file a file the structure can be linked
 * \return an initialized DwarfStruct structure
 */
static DwarfStruct* __struct_new(DwarfAPI* api, Dwarf_Die* d_die,
      DwarfFunction* func, DwarfFile* file)
{
   if (api == NULL || api->dbg == NULL || d_die == NULL)
      return (NULL);

   int i = 0;						// Counter
   Dwarf_Attribute *dwarf_attrs;	// Attributes
   Dwarf_Signed attrs_count = 0;		// Number of attributes
   Dwarf_Half form, attr;			// Form and value of an attribute
   Dwarf_Half tagval = 0;			// Tag of current die
   Dwarf_Die child_die, sibling_die;			// Used to iterate over members
   Dwarf_Error err = 0;			// Error handler
   Dwarf_Off offset;				// Offset of current DIE

   // If DIE offset is in the hastable, return the data
   dwarf_dieoffset(*d_die, &offset, &err);
   DwarfStruct* struc = hashtable_lookup(api->strct, (void*) offset);
   if (struc != NULL)
      return (struc);

   // else create the structure and add it in the hashtable
   struc = lc_malloc0(sizeof(DwarfStruct));
   hashtable_insert(api->strct, (void*) offset, (void*) struc);
   struc->members = queue_new();
   dwarf_tag(*d_die, &tagval, &err);

   if (tagval == DW_TAG_structure_type)
      struc->type = DS_STRUCT;
   else if (tagval == DW_TAG_union_type)
      struc->type = DS_UNION;

   // Retrieves the name and the size of the structure
   dwarf_attrlist(*d_die, &dwarf_attrs, &attrs_count, &err);
   for (i = 0; i < attrs_count; i++) {
      dwarf_whatform(dwarf_attrs[i], &form, &err);
      dwarf_whatattr(dwarf_attrs[i], &attr, &err);
      if (attr == DW_AT_name)
         struc->name = lc_strdup(
               __dwarf_reader_attr_init_data(form, &dwarf_attrs[i], api));
      else if (attr == DW_AT_byte_size)
         struc->size = (long int) __dwarf_reader_attr_init_data(form,
               &dwarf_attrs[i], api);
   }
   dwarf_dealloc(*(api->dbg), dwarf_attrs, DW_DLA_LIST);

   // Now iterates over child to get members
   if (dwarf_child(*d_die, &child_die, &err) == DW_DLV_OK) {
      sibling_die = child_die;
      do {
         child_die = sibling_die;
         dwarf_tag(child_die, &tagval, &err);

         switch (tagval) {
         case DW_TAG_member: {
            DwarfVar * var = __var_new(api, &child_die, func, file);
            if (var != NULL)
               queue_add_tail(struc->members, var);
            break;
         }
         default:
            break;
         }
      } while (dwarf_siblingof_b(*(api->dbg), child_die, TRUE, &sibling_die,
            &err) == DW_DLV_OK);
   }
   return (struc);
}

/*
 * Frees a DwarfStruct structure
 * \param s a structure to free
 */
static void __struc_free(void* s)
{
   DwarfStruct* struc = (DwarfStruct*) s;

   if (struc->name != NULL)
      lc_free(struc->name);

   queue_free(struc->members, __var_free);
   lc_free(struc);
}

/*
 * Reads recursively the type of a variable
 * \param api current DWARF session
 * \param d_die DIE of the variable type
 * \param var current variable
 * \param offset offset of the current CU in the section
 */
static void __read_type(DwarfAPI* api, Dwarf_Die* d_die, DwarfVar* var,
      DwarfFile* file)
{
   Dwarf_Attribute *dwarf_attrs;	// Attributes
   Dwarf_Signed attrs_count = 0;		// Number of attributes
   Dwarf_Half form, attr;			// Form and value of an attribute
   Dwarf_Error err = 0;			// Error handler
   Dwarf_Half tagval = 0;			// Tag of current die
   Dwarf_Off type;					// Offset where to look for next die
   Dwarf_Die die_type;				// Next die
   int i = 0;						// Counter

   dwarf_tag(*d_die, &tagval, &err);

   switch (tagval) {
   case DW_TAG_structure_type:
      var->struc = __struct_new(api, d_die, var->function, file);
      break;
   case DW_TAG_string_type:
      var->type = lc_strdup("string");
      break;
   case DW_TAG_array_type:
      var->type = lc_strdup("array");
      break;
   case DW_TAG_typedef:
      dwarf_attrlist(*d_die, &dwarf_attrs, &attrs_count, &err);

      for (i = 0; i < attrs_count; i++) {
         dwarf_whatform(dwarf_attrs[i], &form, &err);
         dwarf_whatattr(dwarf_attrs[i], &attr, &err);
         if (attr == DW_AT_name)	// it is assumed here tagval == DW_TAG_base_type
         {
            char* name = __dwarf_reader_attr_init_data(form, &dwarf_attrs[i],
                  api);
            if (name != NULL) {
               var->type = lc_malloc((strlen(name) + 1) * sizeof(char));
               sprintf(var->type, "%s", name);
            }
            break;
         }
      }
      dwarf_dealloc(*(api->dbg), dwarf_attrs, DW_DLA_LIST);
      break;

   case DW_TAG_pointer_type:
   case DW_TAG_const_type:
   case DW_TAG_base_type:
      dwarf_attrlist(*d_die, &dwarf_attrs, &attrs_count, &err);

      if (tagval == DW_TAG_pointer_type)
         var->pointer++;
      else if (tagval == DW_TAG_const_type)
         var->state |= DL_CONST;

      for (i = 0; i < attrs_count; i++) {
         dwarf_whatform(dwarf_attrs[i], &form, &err);
         dwarf_whatattr(dwarf_attrs[i], &attr, &err);
         if (attr == DW_AT_type) {
            type = (long int) __dwarf_reader_attr_init_data(form,
                  &dwarf_attrs[i], api);
            if (type > 0) {
               if (dwarf_offdie(*(api->dbg), type + file->off, &die_type, &err)
                     != 0)
                  return;
               __read_type(api, &die_type, var, file);
            }
         } else if (attr == DW_AT_name)// it is assumed here tagval == DW_TAG_base_type
         {
            var->type = lc_strdup(
                  __dwarf_reader_attr_init_data(form, &dwarf_attrs[i], api));
         }
      }
      dwarf_dealloc(*(api->dbg), dwarf_attrs, DW_DLA_LIST);
      break;

   default:
      break;
   }
}

/*
 * Creates a new variable
 * \param api current DWARF session
 * \param d_die DIE of the variable to create
 * \param func function the variable belongs to
 * \param file file the variable belongs to
 * \return a new DwarfVar structure or NULL if there is a problem
 */
static DwarfVar* __var_new(DwarfAPI* api, Dwarf_Die* d_die, DwarfFunction* func,
      DwarfFile* file)
{
   if (asmfile_get_parameter(api->asmf, PARAM_MODULE_DEBUG,
         PARAM_DEBUG_ENABLE_VARS) == FALSE)
      return (NULL);

   if (api == NULL || api->dbg == NULL || d_die == NULL)
      return (NULL);
   Dwarf_Signed attrs_count = 0;      // Number of attributes
   Dwarf_Half form = 0, attr = 0; // Data about an attribute
   Dwarf_Attribute *dwarf_attrs = NULL;	 // Array of attributes
   Dwarf_Error err = NULL;        // Error handler
   Dwarf_Off off_type = 0;     // Offset of the DIE containing the variable type
   Dwarf_Die die_type = 0;        // DIE containing the variable type
   int i = 0;                     // Counter
   char* buff = NULL;             // Buffer
   int buff_size = 1;             // Size of buffer

   DwarfVar* var = lc_malloc0(sizeof(DwarfVar));
   var->function = func;
   var->file = file;
   var->location = NULL;

   // Iterate over attributes to get interesting data.
   dwarf_attrlist(*d_die, &dwarf_attrs, &attrs_count, &err);
   for (i = 0; i < attrs_count; i++) {
      dwarf_whatform(dwarf_attrs[i], &form, &err);
      dwarf_whatattr(dwarf_attrs[i], &attr, &err);

      switch (attr) {
      case DW_AT_name:	//name of the variable
         var->name = lc_strdup(
               __dwarf_reader_attr_init_data(form, &dwarf_attrs[i], api));
         break;
      case DW_AT_decl_line:
         var->src_l = (long) __dwarf_reader_attr_init_data(form,
               &dwarf_attrs[i], api);
         break;
      case DW_AT_decl_column:
         var->src_c = (long) __dwarf_reader_attr_init_data(form,
               &dwarf_attrs[i], api);
         break;
      case DW_AT_type:	//type of the variable
         off_type = (long int) __dwarf_reader_attr_init_data(form,
               &dwarf_attrs[i], api);
         if (dwarf_offdie(*(api->dbg), off_type + file->off, &die_type, &err)
               != 0)
            return (NULL);
         __read_type(api, &die_type, var, file);
         break;
      case DW_AT_data_member_location:
         var->member_location = (long int) __dwarf_reader_attr_init_data(form,
               &dwarf_attrs[i], api);
         break;
      case DW_AT_artificial:
         if (__dwarf_reader_attr_init_data(form, &dwarf_attrs[i], api)) {
            dwarf_dealloc(*(api->dbg), dwarf_attrs, DW_DLA_LIST);
            __var_free(var);
            return (NULL);
         }
         break;
      case DW_AT_location:
         var->location = __read_memloc(api, &(dwarf_attrs[i]));
         break;
      default:
         break;
      }
   }
   dwarf_dealloc(*(api->dbg), dwarf_attrs, DW_DLA_LIST);

   // Use data to create the variable full type.
   if (var->state & DL_STATIC)
      buff_size += 7;         //"static "
   if (var->state & DL_CONST)
      buff_size += 6;         //"const "
   if (var->type)
      buff_size += strlen(var->type);
   else if (var->struc && var->struc->name)
      buff_size += strlen(var->struc->name);
   if (var->pointer > 0)
      buff_size += (1 + var->pointer);

   buff = lc_malloc(buff_size * sizeof(char));
   buff[0] = '\0';
   if (var->state & DL_STATIC)
      sprintf(buff, "static ");
   if (var->state & DL_CONST)
      sprintf(buff, "%sconst ", buff);
   if (var->type)
      sprintf(buff, "%s%s", buff, var->type);
   else if (var->struc && var->struc->name)
      sprintf(buff, "%s%s", buff, var->struc->name);
   if (var->pointer > 0)
      sprintf(buff, "%s ", buff);
   for (i = 0; i < var->pointer; i++)
      sprintf(buff, "%s*", buff);
   var->full_type = buff;
   return (var);
}

/*
 * Frees a variable structure
 * \param v a structure to free
 */
static void __var_free(void* v)
{
   DwarfVar* var = (DwarfVar*) v;
   if (var->name != NULL)
      lc_free(var->name);
   if (var->type != NULL)
      lc_free(var->type);
   if (var->full_type != NULL)
      lc_free(var->full_type);
   if (var->location != NULL)
      lc_free(var->location);
   lc_free(var);
}

/*
 * Creates a new return variable
 * \param api current DWARF session
 * \param func function the variable belongs to
 * \off_type offset of the variable type
 * \return a new DwarfVar structure or NULL if there is a problem
 */
static DwarfVar* __ret_var_new(DwarfAPI* api, DwarfFunction* func,
      Dwarf_Off off_type)
{
   if (api == NULL || api->dbg == NULL)
      return (NULL);

   Dwarf_Error err;				// Error handler
   Dwarf_Die die_type;				// DIE containing the variable type
   int i = 0;						// Counter
   int buff_size = 1;
   char* buff = NULL;

   DwarfVar* var = lc_malloc0(sizeof(DwarfVar));
   var->function = func;
   var->name = lc_strdup("-RET-");

   if (dwarf_offdie(*(api->dbg), off_type + func->file->off, &die_type, &err)
         != 0)
      return NULL;
   __read_type(api, &die_type, var, func->file);

   // Use data to create the variable full type.
   if (var->state & DL_STATIC)
      buff_size += 7;         //"static "
   if (var->state & DL_CONST)
      buff_size += 6;         //"const "
   if (var->type)
      buff_size += strlen(var->type);
   else if (var->struc && var->struc->name)
      buff_size += strlen(var->struc->name);
   if (var->pointer > 0)
      buff_size += (1 + var->pointer);

   buff = lc_malloc(buff_size * sizeof(char));
   buff[0] = '\0';
   if (var->state & DL_STATIC)
      sprintf(buff, "static ");
   if (var->state & DL_CONST)
      sprintf(buff, "%sconst ", buff);
   if (var->type)
      sprintf(buff, "%s%s", buff, var->type);
   else if (var->struc && var->struc->name)
      sprintf(buff, "%s%s", buff, var->struc->name);
   if (var->pointer > 0)
      sprintf(buff, "%s ", buff);
   for (i = 0; i < var->pointer; i++)
      sprintf(buff, "%s*", buff);
   var->full_type = buff;
   return (var);
}

/*
 * Loads sources lines from a CU and add them in a queue
 * \param api current DWARF session
 * \param d_die a Dwarf DIE representing a CU
 * \param lines a queue where to add extracted sources lines
 * \param file current CU
 */
static void __load_lines_from_file(DwarfAPI* api, Dwarf_Die *d_die,
      queue_t* lines, DwarfFile* file)
{
   Dwarf_Error err;
   int i = 0;
   Dwarf_Line* d_lines;
   Dwarf_Signed line_count = 0;
   char* name;

   if (dwarf_srclines(*d_die, &d_lines, &line_count, &err) == DW_DLV_OK) {
      for (i = 0; i < line_count; i++) {
         DwarfLine* line = lc_malloc(sizeof(DwarfLine));
         memset(line, 0, sizeof(DwarfLine));
         dwarf_linesrc(d_lines[i], &name, &err);
         line->filename = lc_strdup(name);
         dwarf_dealloc(*(api->dbg), name, DW_DLA_STRING);
         dwarf_lineaddr(d_lines[i], &(line->address), &err);
         dwarf_lineno(d_lines[i], &(line->no), &err);
         queue_add_tail(lines, line);
      }
      dwarf_srclines_dealloc(*(api->dbg), d_lines, line_count);
   }

   char** file_buff = NULL;
   dwarf_srcfiles(*d_die, &file_buff, &line_count, &err);
   file->filenames_size = line_count;
   file->filenames = lc_malloc(line_count * sizeof(char*));
   if (file_buff != NULL) {
      for (i = 0; i < file->filenames_size; i++) {
         file->filenames[i] = lc_strdup(file_buff[i]);
         /*
          if (file_buff[i] != NULL
          &&  file_buff[i][0] != '<')
          free (file_buff[i]);
          */
      }
      //free (file_buff);
   }
}

/*
 * Frees a line structure
 * \param l a structure to free
 */
static void __line_free(void* l)
{
   DwarfLine* line = (DwarfLine*) l;
   lc_free(line->filename);
   lc_free(l);
}

/*
 * Iterates over Dwarf DIE tree to extract objects such as functions
 * \param api current DWARF session
 * \param root root DIE in the DIE tree
 * \param file current source file (generate from the compilation unit)
 */
static void __dwarf_traverse_DIE_tree(DwarfAPI* api, Dwarf_Die* root,
      DwarfFile* file)
{
   Dwarf_Die child_die, sibling_die;
   Dwarf_Half tagval = 0;
   Dwarf_Error err;

   if (dwarf_child(*root, &child_die, &err) == DW_DLV_OK) {
      sibling_die = child_die;
      do {
         child_die = sibling_die;
         dwarf_tag(child_die, &tagval, &err);

         switch (tagval) {
         case DW_TAG_subprogram: //new function
            queue_add_tail(file->functions,
                  __function_new(api, &child_die, file));
            break;

         default:
            __dwarf_traverse_DIE_tree(api, &sibling_die, file);
            break;
         }
      } while (dwarf_siblingof_b(*(api->dbg), child_die, TRUE, &sibling_die,
            &err) == DW_DLV_OK);
   }
}

// ------------------------------------------------------------------------
// FOR DEBBUGING ONLY
#ifndef NDEBUG
#ifdef _ARCHDEF_x86_64
#include "x86_64_arch.h"
#endif
static void __x86_64_print_loc (DwarfVar* var)
{
#ifdef _ARCHDEF_x86_64
   if (var->location != NULL)
   {
      switch (var->location->type)
      {
         case DWARF_REG:
         if (var->location->reg != NULL) {
            STDMSG (" [[%s]]", arch_get_reg_name (&x86_64_arch,
                     var->location->reg->type,
                     var->location->reg->name));
         } else {
            STDMSG (" [[r%d]]", var->location->index);
         }
         break;
         case DWARF_BREG:
         case DWARF_FBREG:
         if (var->location->reg != NULL) {
            STDMSG (" [[%s + %d]]", arch_get_reg_name (&x86_64_arch,
                     var->location->reg->type,
                     var->location->reg->name),
               var->location->mem.offset);
         } else {
            STDMSG (" [[r%d + %d]]", var->location->index, var->location->mem.offset);
         }
         break;
         case DWARF_ADDR:
            STDMSG (" [[0x%lx]]", var->location->mem.address);
         break;
         case DWARF_FBREG_TBRES:
            STDMSG (" [[%d]]", var->location->mem.offset);
         break;
         default:
            STDMSG (" [[++%d++]]", var->location->type);
         break;
      }
   }
#endif
}
static void __print_loc (DwarfVar* var, int elf_machine_code)
{
   if (elf_machine_code == EM_X86_64)
      __x86_64_print_loc(var);

}
// ------------------------------------------------------------------------
#endif

static int _dwarf_function_cmp(const void* p1, const void* p2)
{
   DwarfFunction* f1 = *((DwarfFunction**) p1);
   DwarfFunction* f2 = *((DwarfFunction**) p2);

   if (f1->low_pc < f2->low_pc)
      return (-1);
   else
      return (1);
}

static Elf_Scn* __get_scn_by_name(const char* scn_name, Elf* elf)
{
   int nb_scn = 0;         // Number of sections
   int i_scn = 0;          // Iterator on sections
   int elf_class = elf_getident(elf, NULL)[EI_CLASS];
   Elf_Scn* scn = NULL;    // Section
   Elf_Scn* scn_name_table = NULL;
   Elf_Data* data_name_table = NULL;

   if (elf_class == ELFCLASS64) {
      Elf64_Ehdr* ehdr = elf64_getehdr(elf);
      nb_scn = ehdr->e_shnum;
      scn_name_table = elf_getscn(elf, ehdr->e_shstrndx);
   } else if (elf_class == ELFCLASS32) {
      Elf32_Ehdr* ehdr = elf32_getehdr(elf);
      nb_scn = ehdr->e_shnum;
      scn_name_table = elf_getscn(elf, ehdr->e_shstrndx);
   } else {
      return NULL;
   }
   data_name_table = elf_getdata(scn_name_table, NULL);

   // Iterate over sections looking for .gnu_debugaltlink section.
   // If the section exists, get its content
   for (i_scn = 0; i_scn < nb_scn; i_scn++) {
      scn = elf_getscn(elf, i_scn);
      if (elf_class == ELFCLASS64) {
         Elf64_Shdr* shdr = elf64_getshdr(scn);
         char* _scn_name = &((char*) (data_name_table->d_buf))[shdr->sh_name];

         if (strcmp(_scn_name, scn_name) == 0)
            return (scn);
      } else if (elf_class == ELFCLASS32) {
         Elf32_Shdr* shdr = elf32_getshdr(scn);
         char* _scn_name = &((char*) (data_name_table->d_buf))[shdr->sh_name];

         if (strcmp(_scn_name, scn_name) == 0)
            return (scn);
      }
   }
   return NULL;
}

static void load_dwz(Elf* elf, DwarfAPI *api)
{
   Elf_Scn* scn = NULL;    // Section
   Elf_Data* data = NULL;
   char* dwz_str = "";
   char* dwz_name = NULL;
   FILE* file = NULL;

   scn = __get_scn_by_name(".gnu_debugaltlink", elf);
   if (scn != NULL) {
      data = elf_getdata(scn, NULL);
      dwz_str = strdup(data->d_buf);
      DBGMSG("===> DWZ: %s\n", dwz_str);
   } else
      return;

   // At this point, if dwr_str contains an empty string, this mean
   // there is not .gnu_debugaltlink section, so exit
   if (strcmp(dwz_str, "") == 0)
      return;
   if (api->elf_name == NULL)
      return;

   // Now look for the dwz file
   // Forge the string to the file, then check if it exists
   // elf_name should contains the name of the elf file
   // get the dirname, then concat dwz_str
   dwz_name = lc_malloc(
         (strlen(dwz_str) + strlen(api->elf_name)) * sizeof(char));
   sprintf(dwz_name, "%s/%s", lc_dirname(api->elf_name), dwz_str);
   DBGMSG("===> DWZ path: %s\n", dwz_name);
   file = fopen(dwz_name, "r");
   if (file == NULL)
      return;
   DBGMSG0("===> DWZ file found\n");

   // At this point, we know the file exists.
   // Open it as a Elf file and get the ".debug_str" section
   // Open the file
   int fd = fileno(file);
   Elf * dwz_elf = elf_begin(fd, ELF_C_READ, NULL);

   // Get the ".debug_str" section and copy its data
   scn = __get_scn_by_name(".debug_str", dwz_elf);
   if (scn != NULL) {
      data = elf_getdata(scn, NULL);
      api->dwz_debug_str_size = data->d_size;
      api->dwz_debug_str = lc_malloc(data->d_size);
      memcpy(api->dwz_debug_str, data->d_buf, data->d_size);
   }

   // Close the file
   elf_end(dwz_elf);
   fclose(file);
}

/* -------------------------- DwarfAPI functions --------------------------- */
/*
 * Initialize the Dwarf API
 * \param elf an Elf structure used as input for libdwarf
 * \param elf_name
 * \param asmf corresponding assembly file or NULL
 * \return The API object
 */
DwarfAPI* dwarf_api_init_light(Elf* elf, char* elf_name, asmfile_t* asmf)
{
   //TODO DEBUG DarfLight on Windows..
#ifdef _WIN32
   return NULL;
#else
   DBGMSG0("Start of dwarf_api_init_light\n");

   //Open the dwarf structure
   // Initialize libdwarf
   Dwarf_Error err = NULL;		//!< DWARF error
   Dwarf_Debug dbg;			//!< DWARF debug
   if (dwarf_elf_init(elf, DW_DLC_READ, NULL, NULL, &dbg, &err) != DW_DLV_OK) {
      return NULL;
   }

   // Initialize API structure
   DwarfAPI *api = lc_malloc0(sizeof(DwarfAPI));
   api->dbg = lc_malloc(sizeof(Dwarf_Debug));
   api->dbg = memcpy(api->dbg, &dbg, sizeof(Dwarf_Debug));
   api->files = queue_new();
   api->lines = queue_new();
   api->strct = hashtable_new(direct_hash, direct_equal);
   api->functions = hashtable_new(&direct_hash, &direct_equal);
   api->functions_off = hashtable_new(&direct_hash, &direct_equal);
   api->functions_linkname = hashtable_new(&str_hash, &str_equal);
   api->elf = elf;
   api->is_range = FALSE;
   api->asmf = asmf;
   api->fct_array_size = 0;
   api->dwz_debug_str = NULL;
   if (elf_name != NULL)
      api->elf_name = lc_strdup(elf_name);
   else
      api->elf_name = NULL;
   //
   load_dwz(elf, api);

   // Iterate over Compilation Units (CU)
   // Each CU represents a binary file
   int res;
   Dwarf_Half tagval = 0;
   Dwarf_Die die;
   Dwarf_Unsigned next_cu_header;
   Dwarf_Off offset;
   Dwarf_Off overall_offset;

   // Do the loop till the last of the CU headers
   while ((res = dwarf_next_cu_header_b(dbg, NULL, NULL, NULL, NULL, NULL, NULL,
         &next_cu_header, &err)) != DW_DLV_NO_ENTRY) {
      if (res != DW_DLV_OK)
         continue;

      // Get the first child
      if ((dwarf_siblingof_b(dbg, 0, TRUE, &die, &err)) == DW_DLV_ERROR)
         continue;

      dwarf_dieoffset(die, &overall_offset, &err);
      dwarf_die_CU_offset(die, &offset, &err);
      dwarf_tag(die, &tagval, &err);

      // We check that it is a CU
      if (tagval != DW_TAG_compile_unit)
         continue;

      // Here, we know the current DIE is a CU.
      // We can create the DwarfFile associated to the current CU
      DwarfFile* file = __file_new(api, &die, overall_offset - offset);
      if (file != NULL) {
         queue_add_tail(api->files, file);
         __load_lines_from_file(api, &die, api->lines, file);

         // Then all childs are traversed to look for functions. When a function is
         // found, do not need to traverse its sons. If the DIE is a type, it is saved too.
         __dwarf_traverse_DIE_tree(api, &die, file);

         //Here simplify functions of the file using abstract_origin and offset members
         FOREACH_INQUEUE(file->functions, it_f)
         {
            DwarfFunction* fct = GET_DATA_T(DwarfFunction*, it_f);
            // Simplify functions
            DwarfFunction* fctsib = hashtable_lookup(file->fcts_ao,
                  (void*) (fct->offset));
            if (fctsib != NULL) {
               fct->low_pc = fctsib->low_pc;
               fct->high_pc = fctsib->high_pc;
               hashtable_remove(api->functions, (void*) (fctsib->low_pc));
               hashtable_remove(file->fcts_ao, (void*) (fct->offset));
               hashtable_insert(api->functions, (void*) (fct->low_pc), fct);
               //queue_remove (file->functions, fctsib, &__function_free);
            }

            // Link inlined function to the function
            FOREACH_INQUEUE(fct->inlinedFunctions, it_inlined)
            {
               DwarfInlinedFunction* ifct = GET_DATA_T(DwarfInlinedFunction*,
                     it_inlined);
               DwarfFunction* fctsib = hashtable_lookup(api->functions_off,
                     (void*) (ifct->abstract_origin));
               if (fctsib != NULL)
                  ifct->function = fctsib;
            }
         }

      }
   }

   // ------------------------------------------------------------------------
   // FOR DEBBUGING ONLY
   DBGLVL(1, int elf_machine_code = elf_getmachine(elf);
      FOREACH_INQUEUE (api->files, it_file)
      {
         DwarfFile* file = GET_DATA_T (DwarfFile*, it_file);
         FCTNAMEMSG ("%s:%s\n", file->dir, file->name);
         STDMSG ("  producer: %s\n", file->producer);
         STDMSG ("  language: %s\n", file->language);
         STDMSG ("  cmd line: %s\n", file->command_line_opts);
         STDMSG ("\n");
         STDMSG ("  Structures:\n");
         STDMSG ("  Global variables:\n");

          FOREACH_INQUEUE (dwarf_file_get_global_variables (file), it_global)
          {
          DwarfVar* var = GET_DATA_T (DwarfVar*, it_global);
          STDMSG ("    %s %s", var->full_type, var->name);
          __print_loc (var, elf_machine_code);
          STDMSG ("\n");
          }

          STDMSG ("  Functions:\n");
         FOREACH_INQUEUE (file->functions, it_func)
         {
            DwarfFunction* func = GET_DATA_T (DwarfFunction*, it_func);
            STDMSG ("  + %s\n", func->name);
            STDMSG ("    0x%lx -> 0x%lx\n", func->low_pc, func->high_pc);

            FOREACH_INQUEUE (func->par, it_sf)
            {
               DwarfFunction* sfunc = GET_DATA_T (DwarfFunction*, it_sf);
               STDMSG ("     + %s\n", sfunc->name);
               STDMSG ("       0x%lx -> 0x%lx\n", sfunc->low_pc, sfunc->high_pc);
            }


             STDMSG ("    Return type:\n");
             DwarfVar* ret = dwarf_function_get_returned_var(func);
             if (ret != NULL)
                STDMSG ("      %s\n", ret->full_type);
             STDMSG ("    Parameters:\n");
             FOREACH_INQUEUE (dwarf_function_get_parameters (func), it_param)
             {
             DwarfVar* var = GET_DATA_T (DwarfVar*, it_param);
             STDMSG ("      %s %s", var->full_type, var->name);
             __print_loc (var, elf_machine_code);
             STDMSG ("\n");
             }
             STDMSG ("    Local variables:\n");
             FOREACH_INQUEUE (dwarf_function_get_local_variables (func),  it_local)
             {
             DwarfVar* var = GET_DATA_T (DwarfVar*, it_local);
             STDMSG ("      %s %s", var->full_type, var->name);
             __print_loc (var, elf_machine_code);
             STDMSG ("\n");

             }
             STDMSG("    Inlined functions:\n");
             FOREACH_INQUEUE(func->inlinedFunctions, it_inline) {
                DwarfInlinedFunction* inl_func = GET_DATA_T(DwarfInlinedFunction*, it_inline);
                STDMSG ("      - %s\n", inl_func->function->name);
                STDMSG ("        0x%lx -> 0x%lx\n", inl_func->low_pc, inl_func->high_pc);
             }
             STDMSG ("\n");
         }
      }
   )
   // ------------------------------------------------------------------------

   if (queue_length(api->files) == 0) {
      dwarf_api_close_light(api);
      return (NULL);
   }

   // Generate an array of function to improve search in next steps
   api->fct_array_size = 0;
   HASHTABLE_TO_MALLOC_ARRAY(DwarfFunction*, api->functions, fct_array,
         api->fct_array_size);
   qsort(fct_array, api->fct_array_size, sizeof(DwarfFunction*),
         &_dwarf_function_cmp);
   api->fct_array = fct_array;

   DBGMSG0("End of dwarf_api_init_light\n");
   return api;
#endif
}

/*
 * Frees a Dwarf API structure
 * \param api a Dwarf API created with dwarf_api_init_light
 */
void dwarf_api_close_light(DwarfAPI* api)
{
   if (api == NULL)
      return;
   Dwarf_Error err;

   FOREACH_INQUEUE(api->files, it_files) {
      DwarfFile* file = GET_DATA_T(DwarfFile*, it_files);
      queue_free(file->functions, __function_free);
   }
   queue_free(api->files, __file_free);
   queue_free(api->lines, __line_free);

   dwarf_finish(*(api->dbg), &err);
   hashtable_free(api->strct, __struc_free, NULL);
   hashtable_free(api->functions, NULL, NULL);
   hashtable_free(api->functions_off, NULL, NULL);
   hashtable_free(api->functions_linkname, NULL, NULL);
   if (api->elf_name)
      lc_free(api->elf_name);
   if (api->fct_array_size > 0)
      lc_free(api->fct_array);
   lc_free(api->dbg);
   lc_free(api);
}

/*
 * Set the asmfile associated to Dwarf data
 * \param api a Dwarf API created with dwarf_api_init_light
 * \param asmf an asmfile
 */
void dwarf_api_set_asmfile(DwarfAPI* api, asmfile_t* asmf)
{
   if (api == NULL || asmf == NULL || api->asmf != NULL)
      return;
   api->asmf = asmf;
}

static int compare_lines(const void* p1, const void* p2)
{
   const DwarfLine *i1 = *((void **) p1);
   const DwarfLine *i2 = *((void **) p2);

   if (i1->address <= i2->address)
      return (-1);
   else
      return (1);
}

/*
 * Retrieve all addresses, filenames and source lines from Dwarf
 * \param api The API object
 * \param filename used to return an array of source file name
 * \param addrs used to return an array of addresses
 * \param srcs  used to return an array of source lines
 * \param size  used to return the size of each array
 */
void dwarf_api_get_all_lines(DwarfAPI *api, char ***filename, maddr_t** addrs,
      int ** srcs, unsigned int* size)
{
   // No debug data on lines, set outputs to NULL then exit
   if (queue_length(api->lines) == 0) {
      if (addrs)
         (*addrs) = NULL;
      if (srcs)
         (*srcs) = NULL;
      if (filename)
         (*filename) = NULL;
      *size = 0;
      return;
   }

   if (addrs)
      (*addrs) = lc_malloc(queue_length(api->lines) * sizeof(**addrs));
   if (srcs)
      (*srcs) = lc_malloc(queue_length(api->lines) * sizeof(**srcs));
   if (filename)
      (*filename) = lc_malloc(queue_length(api->lines) * sizeof(**filename));
   unsigned int i = 0;

   queue_sort(api->lines, &compare_lines);

   FOREACH_INQUEUE(api->lines, line_link) {
      DwarfLine* line = GET_DATA_T(DwarfLine*, line_link);
      if (addrs)
         (*addrs)[i] = line->address;
      if (srcs)
         (*srcs)[i] = line->no;
      if (filename)
         (*filename)[i] = line->filename;
      i++;
   }
   *size = i;

   return;
}

/**
 * Retrieve all functions. The retrieved queue should be freed manually
 * \param api A DwarfAPI object
 * \return A queue of DwarfFunction pointer
 */
static queue_t* dwarf_api_get_functions_static(DwarfAPI *api)
{
   DwarfFile *file;
   queue_t* ret = queue_new();

   FOREACH_INQUEUE(api->files, file_link) {
      file = GET_DATA_T(DwarfFile*, file_link);

      FOREACH_INQUEUE(file->functions, it_f)
      {
         DwarfFunction* f = GET_DATA_T(DwarfFunction*, it_f);
         queue_add_tail(ret, f);
      }
   }
   return ret;
}

/*
 * Retrieve all functions. The retrieved queue should be freed manually
 * \param api A DwarfAPI object
 * \return A queue of DwarfFunction pointer
 */
queue_t* dwarf_api_get_functions(DwarfAPI *api)
{
   return dwarf_api_get_functions_static(api);
}

static int compare_fcts(const void* p1, const void* p2)
{
   const DwarfFunction *i1 = *((void **) p1);
   const DwarfFunction *i2 = *((void **) p2);

   if (i1->low_pc <= i2->low_pc)
      return (-1);
   else if (i1->low_pc == i2->low_pc) {
      //Equality: returning the function finishing last first
      if (i1->high_pc >= i2->high_pc)
         return -1;
      else if (i1->high_pc == i2->high_pc) {
         if (i1->name != NULL && i2->name != NULL)
            return strcmp(i1->name, i2->name);  //Using lexicographical order if addresses are equal
         //Cases where at least one name is null: the non-null one is inferior (will come first in an ordered list)
         else if (i1->name != NULL)
            return -1;
         else if (i2->name != NULL)
            return 1;
         else
            return 0;   //Both names are null: considering functions equal (we tried)
   } else
         return 1;
   } else
      return (1);
}

/*
 * Retrieve the ranges of addresses containing debug information, based on the ranges of dwarf functions.
 * Ranges are sorted by starting address and do not overlap
 * \param api A DwarfAPI object
 * \param starts_ranges Return parameter. Will contain the array of ranges starts
 * \param stops_ranges Return parameter. Will contain the array of ranges stops
 * \return The size of the returned arrays. If either parameter is NULL, 0
 */
unsigned int dwarf_api_get_debug_ranges(DwarfAPI *api, maddr_t** starts_ranges, maddr_t** stops_ranges)
{
   if (!api || !starts_ranges || !stops_ranges)
      return 0;
   queue_t* fcts = dwarf_api_get_functions_static(api);
   queue_sort(fcts, compare_fcts);
   unsigned int size = 0;
   maddr_t *starts = NULL;
   maddr_t *stops = NULL;
   FOREACH_INQUEUE(fcts, iter) {
      DwarfFunction* fct = GET_DATA_T(DwarfFunction*, iter);
      if (fct->low_pc <= 0) {
         //Case where the lowest address of the function is negative (it happens...)
         if (fct->high_pc <= 0)
            continue; //Excluding functions with a negative start and stop address
         else if (fct->high_pc > 0) {
            //Creating a range starting at 0
            size++;
            starts = lc_realloc(starts, sizeof(*starts) * size);
            stops = lc_realloc(stops, sizeof(*stops) * size);
            starts[size - 1] = 0;
            stops[size - 1] = fct->high_pc;
         }
      } else if (size > 0 && fct->low_pc >= starts[size - 1] && fct->high_pc <= stops[size - 1]) {
         continue; //Function range is encompassed into the previous one: excluding it
      } else if (size > 0 && fct->low_pc <= stops[size - 1] && fct->high_pc > stops[size - 1]) {
         //Function range overlaps with the previous one: updating the end of the previous range
         stops[size - 1] = fct->high_pc;
      } else {
         //Otherwise, add a new range
         size++;
         starts = lc_realloc(starts, sizeof(*starts) * size);
         stops = lc_realloc(stops, sizeof(*stops) * size);
         starts[size - 1] = fct->low_pc;
         stops[size - 1] = fct->high_pc;
      }
   }
   *starts_ranges = starts;
   *stops_ranges = stops;
   queue_free(fcts, NULL);
   return size;
}

/*
 * Retrieve all compile units in a queue of DwarfFile*
 * \param api A DwarfAPI object
 * \return A queue of DwarfFile pointer
 */
queue_t* dwarf_api_get_files(DwarfAPI *api)
{
   if (api == NULL)
      return (NULL);
   else
      return (api->files);
}

/*
 * Retrieve a function corresponding to an address
 * \param api A DwarfAPI object
 * \param low_pc the first instruction address of the function
 * \return A DwarfFunction pointer, or NULL if not found
 */
DwarfFunction* dwarf_api_get_function_by_addr(DwarfAPI *api, Dwarf_Addr low_pc)
{
   if (api == NULL)
      return (NULL);
   return (hashtable_lookup(api->functions, (void*) low_pc));
}

/*
 * Retrieves a function belonging to an interval
 * \param api A DwarfAPI object
 * \param low_pc the first instruction address where to start to look for the function
 * \param high_pc the last instruction address where to stop to look for the function
 * \return A DwarfFunction pointer, or NULL if not found
 */
DwarfFunction* dwarf_api_get_function_by_interval(DwarfAPI *api,
      Dwarf_Addr low_pc, Dwarf_Addr high_pc)
{
   if (api == NULL)
      return (NULL);

   if ((api == NULL) || (api->fct_array_size == 0)
         || (uint64_t) (api->fct_array[0]->low_pc) < low_pc)
      return NULL;
   int minidx = 0;
   int maxidx = api->fct_array_size;
   while ((maxidx - minidx) > 1) {
      int middleidx = (maxidx + minidx) / 2;
      if ((int64_t) low_pc <= api->fct_array[middleidx]->low_pc
            && api->fct_array[middleidx]->low_pc <= (int64_t) high_pc)
         return (api->fct_array[middleidx]);
      else if ((uint64_t) (api->fct_array[middleidx]->low_pc) < low_pc)
         maxidx = middleidx;
      else
         minidx = middleidx;
   }
   return NULL;
}

/*
 * Retrieve a function corresponding to instruction debug data
 * \param api A DwarfAPI object
 * \param name name of the source file containing the function
 * \param srcl instruction source line
 * \return A DwarfFunction pointer, or NULL if not found
 */
DwarfFunction* dwarf_api_get_function_by_src(DwarfAPI *api, const char* name,
      int srcl)
{
   FOREACH_INQUEUE(api->files, file_link) {
      DwarfFile *file = GET_DATA_T(DwarfFile*, file_link);
      int i = 0;

      if (strcmp(lc_basename(file->name), lc_basename(name)) == 0)
         return (dwarf_file_get_function_by_src(file, srcl));

      for (i = 0; i < file->filenames_size; i++)
         if (strcmp(lc_basename(file->filenames[i]), lc_basename(name)) == 0)
            return (dwarf_file_get_function_by_src(file, srcl));
   }
   return NULL;
}

/*
 * Retrieve a function corresponding to a given link name.
 * \param api A DwarfAPI object
 * \param linkname The name of the function as it appears in the binary (mangled name)
 * \return A DwarfFunction pointer, or NULL if not found
 */
DwarfFunction* dwarf_api_get_function_by_linkname(DwarfAPI *api, char* linkname)
{
   if (api == NULL || linkname == NULL)
      return (NULL);
   return (hashtable_lookup(api->functions_linkname, linkname));
}

/* ----------------------- DwarfFile functions ----------------------------- */
/*
 * Retrieve the filename
 * \param file the DwarfFile object
 * \return A string containing the name
 */
char* dwarf_file_get_name(DwarfFile *file)
{
   if (file != NULL)
      return file->name;
   else
      return (NULL);
}

/*
 * Retrieve the directory where the file operates
 * \param file the DwarfFile object
 * \return A string containing the directory
 */
char* dwarf_file_get_dir(DwarfFile *file)
{
   if (file != NULL)
      return file->dir;
   else
      return (NULL);
}

/*
 * Retrieve the directory where the file operates
 * \param file the DwarfFile object
 * \return A string containing the directory
 */
char* dwarf_file_get_vendor(DwarfFile *file)
{
   if (file != NULL)
      return file->vendor;
   else
      return (NULL);
}

/*
 * Retrieve the directory where the file operates
 * \param file the DwarfFile object
 * \return A string containing the directory
 */
char* dwarf_file_get_version(DwarfFile *file)
{
   if (file != NULL)
      return file->version;
   else
      return (NULL);
}

/*
 * Retrieve the source language of the file
 * \param file the DwarfFile object
 * \return A string containing a source language
 */
char* dwarf_file_get_language(DwarfFile *file)
{
   if (file != NULL)
      return file->language;
   else
      return (NULL);
}

/*
 * Retrieve the full producer string (compiler, version ...)
 * \param file the DwarfFile object
 * \return A string containing the producer
 */
char* dwarf_file_get_producer(DwarfFile *file)
{
   if (file != NULL)
      return file->producer;
   else
      return (NULL);
}

/*
 * Retrieves the compiler code of the file. Codes are described
 * in libmasm.h header file
 * \param file the DwarfFile object
 * \return A code containing a compiler
 */
int dwarf_file_get_producer_code(DwarfFile *file)
{
   if (file != NULL)
      return file->comp_code;
   else
      return (COMP_ERR);
}

/*
 * Retrieve the source language code of the file. Codes are described
 * in libmasm.h header file
 * \param file the DwarfFile object
 * \return A string containing a source language
 */
int dwarf_file_get_language_code(DwarfFile *file)
{
   if (file != NULL)
      return file->lang_code;
   else
      return (LANG_ERR);
}

/*
 * Retrieve a function corresponding to an address
 * \param file A DwarfFile object
 * \param low_pc the first instruction address of the function
 * \return A DwarfFunction pointer
 */
DwarfFunction* dwarf_file_get_function_by_addr(DwarfFile *file,
      Dwarf_Addr low_pc)
{
   if (file == NULL)
      return (NULL);
   DwarfFunction *function;

   FOREACH_INQUEUE(file->functions, function_link) {
      function = function_link->data;

      if (function->low_pc == (int64_t) low_pc)
         return function;
      else if (function->par != NULL) {
         FOREACH_INQUEUE(function->par, it_sf)
         {
            DwarfFunction *sfunction = GET_DATA_T(DwarfFunction*, it_sf);
            if (sfunction->low_pc == (int64_t) low_pc)
               return sfunction;
         }
      }
   }
   return NULL;
}

/*
 * Retrieve a function belonging to an interval
 * \param file A DwarfFile object
 * \param low_pc the first instruction address of the interval
 * \param high_pc the last instruction address of the interval
 * \return A DwarfFunction pointer
 */
DwarfFunction* dwarf_file_get_function_by_interval(DwarfFile *file,
      Dwarf_Addr low_pc, Dwarf_Addr high_pc)
{
   if (file == NULL)
      return (NULL);

   FOREACH_INQUEUE(file->functions, function_link) {
      DwarfFunction *function = GET_DATA_T(DwarfFunction*, function_link);
      if ((int64_t) low_pc <= function->low_pc
            && function->low_pc <= (int64_t) high_pc)
         return function;
   }
   return NULL;
}

/*
 * Retrieves global variables of a file from Dwarf
 * \param api A DwarfAPI object
 * \param file a file
 * \return a queue of DwarfVar structures or NULL if there is a problem
 */
queue_t* dwarf_file_get_global_variables(DwarfFile *file)
{
   // no function, error
   if (file == NULL)
      return NULL;
   // local variables already exist, return them
   if (file->global_var != NULL)
      return (file->global_var);

   Dwarf_Die child_die, sibling_die;	// Used to iterate in the DIE tree
   Dwarf_Half tagval = 0;				// Tag of a DIE
   Dwarf_Error err;					// Error handler
   file->global_var = queue_new();

   // Iterates over function DIE child to get parameters.
   // In the DIE tree, parameters are direct child of a function.
   // If tag == DW_TAG_formal_parameter means it is a regular parameter.
   // More data about the parameter (name, type ...) are retrives from the
   // DWARF data.
   // Else if tag == DW_TAG_unspecified_parameters means it is a var args
   // In this case, DWARF doesn't have more data, so default one are used.
   if (dwarf_child(*(file->d_die), &child_die, &err) == DW_DLV_OK) {
      sibling_die = child_die;
      do {
         child_die = sibling_die;
         dwarf_tag(child_die, &tagval, &err);

         // Look into DWARF for more data about the variable
         if (tagval == DW_TAG_variable) {
            DwarfVar* var = __var_new(file->api, &child_die, NULL, file);
            if (var != NULL)
               queue_add_tail(file->global_var, var);
         }
      } while (dwarf_siblingof_b(*(file->api->dbg), child_die, TRUE,
            &sibling_die, &err) == DW_DLV_OK);
   }
   return (file->global_var);
}

/*
 * Retrieve a function corresponding to instruction debug data
 * \param file A DwarfFile object
 * \param srcl instruction source line
 * \return A DwarfFunction pointer, or NULL if not found
 */
DwarfFunction* dwarf_file_get_function_by_src(DwarfFile *file, int srcl)
{
   if (file == NULL)
      return (NULL);
   DwarfFunction *fct = NULL;
   int diff = -1;

   FOREACH_INQUEUE(file->functions, function_link) {
      DwarfFunction *function = GET_DATA_T(DwarfFunction*, function_link);
      int d = srcl - function->srcl;

      if (d > 0 && (d < diff || diff == -1)) {
         fct = function;
         diff = d;
      } else if (d == 0)
         return (function);
   }
   return fct;
}

/*
 * Set the options used on command line to compile the file
 * \param file A DwarfFile object
 * \param opts the options
 */
void dwarf_file_set_command_line_opts(DwarfFile* file, const char* opts)
{
   if (file == NULL || opts == NULL)
      return;
   file->command_line_opts = lc_strdup(opts);
}

/*
 * Get the options used on command line to compile the file
 * \param file A DwarfFile object
 * \returns the options used to compile the file
 */
char* dwarf_file_get_command_line_opts(DwarfFile* file)
{
   if (file == NULL)
      return NULL;
   return (file->command_line_opts);
}

/* ------------------- DwarfFunction functions ----------------------------- */
/*
 * Retrieves the source file
 * \param func a function
 * \return the source file
 */
char* dwarf_function_get_decl_line(DwarfFunction *func)
{
   if (func == NULL || func->decl_file == -1)
      return NULL;
   return (func->file->filenames[func->decl_file]);
}

/*
 * Retrieve the file containing the function
 * \param function A DwarfFunction object
 * \return A DwarfFile pointer, should never fail
 */
DwarfFile* dwarf_function_get_file(DwarfFunction *function)
{
   if (function != NULL)
      return function->file;
   else
      return (NULL);
}

/*
 * Retrieve the name of the function
 * \param function A DwarfFunction object
 * \return A string containing the name of the function, or NULL if unavailable
 */
char* dwarf_function_get_name(DwarfFunction *function)
{
   if (function != NULL)
      return (char *) function->name;
   else
      return (NULL);
}

/*
 * Load parameters and variables
 * \param api A DwarfAPI object
 * \param func a function
 */
static void dwarf_function_load_variables(DwarfFunction *func)
{
   // no function, error
   if (func == NULL)
      return;
   // parameters already exist, return them
   if (func->parameters != NULL || func->local_vars != NULL)
      return;

   Dwarf_Die child_die, sibling_die; // Used to iterate in the DIE tree
   Dwarf_Half tagval = 0;           // Tag of a DIE
   Dwarf_Error err;              // Error handler
   func->parameters = queue_new();
   func->local_vars = queue_new();

   // Iterates over function DIE child to get parameters.
   if (dwarf_child(*(func->d_die), &child_die, &err) == DW_DLV_OK) {
      sibling_die = child_die;
      do {
         child_die = sibling_die;
         dwarf_tag(child_die, &tagval, &err);

         // Look into DWARF for more data about the parameter
         if (tagval == DW_TAG_formal_parameter) {
            DwarfVar* var = __var_new(func->file->api, &child_die, func,
                  func->file);
            if (var != NULL)
               queue_add_tail(func->parameters, var);
         }
         // Set default values for the parameter
         else if (tagval == DW_TAG_unspecified_parameters) {
            DwarfVar* var = lc_malloc0(sizeof(DwarfVar));
            var->name = lc_strdup("...");
            var->type = lc_strdup("var_args");
            var->full_type = lc_strdup("var_args");
            queue_add_tail(func->parameters, var);
         } else if (tagval == DW_TAG_variable) {
            DwarfVar* var = __var_new(func->file->api, &child_die, func,
                  func->file);
            if (var != NULL)
               queue_add_tail(func->local_vars, var);
         }
      } while (dwarf_siblingof_b(*(func->file->api->dbg), child_die, TRUE,
            &sibling_die, &err) == DW_DLV_OK);
   }
}

/*
 * Retrieves parameters of a function from Dwarf
 * \param api A DwarfAPI object
 * \param func a function
 * \return a queue of DwarfVar structures or NULL if there is a problem
 */
queue_t* dwarf_function_get_parameters(DwarfFunction *func)
{
   // no function, error
   if (func == NULL)
      return NULL;
   // parameters already exist, return them
   if (func->parameters != NULL)
      return (func->parameters);

   dwarf_function_load_variables(func);
   return (func->parameters);
}

/*
 * Retrieves local variables of a function from Dwarf
 * \param api A DwarfAPI object
 * \param func a function
 * \return a queue of DwarfVar structures or NULL if there is a problem
 */
queue_t* dwarf_function_get_local_variables(DwarfFunction *func)
{
   // no function, error
   if (func == NULL)
      return NULL;
   // local variables already exist, return them
   if (func->local_vars != NULL)
      return (func->local_vars);

   dwarf_function_load_variables(func);
   return (func->local_vars);
}

/*
 * Retrieves the function return variable
 * \param api A DwarfAPI object
 * \param func a function
 * \return a DwarfVar variable or NULL if there is a problem
 */
DwarfVar* dwarf_function_get_returned_var(DwarfFunction *func)
{
   if (func == NULL)
      return (NULL);
   if (func->ret != NULL || (func->flags & DFUNC_NO_RET) == 1)
      return (func->ret);

   // look into the DIE for an attribute with tag equals to DW_AT_type
   // This attribute is the offset of the returned variable type.
   Dwarf_Signed attrs_count = 0;		// Number of attributes
   Dwarf_Half form, attr;			// Data on an atrribute
   Dwarf_Attribute *dwarf_attrs;	// List of attributes
   Dwarf_Error err;				// Error handler
   int i = 0;						// Counter

   dwarf_attrlist(*(func->d_die), &dwarf_attrs, &attrs_count, &err);
   for (i = 0; i < attrs_count; i++) {
      dwarf_whatattr(dwarf_attrs[i], &attr, &err);
      if (attr == DW_AT_type) {
         dwarf_whatform(dwarf_attrs[i], &form, &err);
         func->ret = __ret_var_new(func->file->api, func,
               (long int) __dwarf_reader_attr_init_data(form, &dwarf_attrs[i],
                     func->file->api));
         break;
      }
   }
   if (func->ret == NULL)
      func->flags |= DFUNC_NO_RET;
   dwarf_dealloc(*(func->file->api->dbg), dwarf_attrs, DW_DLA_LIST);
   return (func->ret);
}

/*
 * Retrieves a list of subfunctions (parallel regions / loops)
 * \param func a function
 * \return a list of sub functions (DwarfFunction* structures) or NULL if there is a problem
 */
queue_t* dwarf_function_get_subfunctions(DwarfFunction *func)
{
   if (func == NULL)
      return (NULL);
   else
      return (func->par);
}

/*
 * Retrieves a low-pc (low Program Counter)
 * \param func a function
 * \return the low-pc
 */
int64_t dwarf_function_get_lowpc(DwarfFunction *func)
{
   if (func == NULL)
      return (-1);
   else
      return (func->low_pc);
}

/*
 * Retrieves a high-pc (high Program Counter)
 * \param func a function
 * \return the high-pc
 */
int64_t dwarf_function_get_highpc(DwarfFunction *func)
{
   if (func == NULL)
      return (-1);
   else
      return (func->high_pc);
}

/*
 * Retrieves the source line declaration
 * \param func a function
 * \return the source line declaration
 */
int dwarf_function_get_srcl(DwarfFunction *func)
{
   if (func == NULL)
      return (0);
   else
      return (func->srcl);
}

/*
 * Retrieves functions inlined in a function
 * \param func a function
 * \return the list of inlined function in func
 */
queue_t* dwarf_function_get_inlined(DwarfFunction *func)
{
   if (func == NULL)
      return (NULL);
   else
      return (func->inlinedFunctions);
}

/*
 * Add a range in a function
 * \param func a function
 * \param start first instruction
 * \param stop last instruction
 */
void dwarf_function_add_range(DwarfFunction *func, insn_t* start, insn_t* stop)
{
   if (func == NULL)
      return;
   queue_add_tail(func->ranges, fct_range_new(start, stop));
}

/*
 * Gets the ranges of a function
 * \param func a function
 * \return a list of ranges
 */
queue_t* dwarf_function_get_ranges(DwarfFunction *func)
{
   if (func == NULL)
      return (NULL);
   if (func->file->api->is_range == FALSE)
      asmfile_detect_debug_ranges(func->file->api->asmf);
   if (func->file->api->is_range == TRUE)
      return (func->ranges);
   else
      return (NULL);
}

/* ------------------- DwarfInlinedFunction functions ---------------------- */
/*
 * Retrieves the function the inlined function is extracted from
 * \param ifunc an inlined function
 * \return the function the inlined function is extracted from
 */
DwarfFunction* dwarf_inlinedFunction_get_origin_function(
      DwarfInlinedFunction* ifunc)
{
   if (ifunc == NULL)
      return (NULL);
   return (ifunc->function);
}

/*
 * Retrieves the source line the inline function is called
 * \param ifunc an inlined function
 * \return the source line where the inline function is called
 */
int dwarf_inlinedFunction_get_call_line(DwarfInlinedFunction* ifunc)
{
   if (ifunc == NULL)
      return (-1);
   return (ifunc->call_line);
}

/*
 * Retrieves the source column the inline function is called
 * \param ifunc an inlined function
 * \return the source column where the inline function is called
 */
int dwarf_inlinedFunction_get_call_column(DwarfInlinedFunction* ifunc)
{
   if (ifunc == NULL)
      return (-1);
   return (ifunc->call_column);
}

/*
 * Retrieves the address the inline function begins
 * \param ifunc an inlined function
 * \return the address where the inline function begins
 */
int64_t dwarf_inlinedFunction_get_low_pc(DwarfInlinedFunction* ifunc)
{
   if (ifunc == NULL)
      return (-1);
   return (ifunc->low_pc);
}

/*
 * Retrieves the address the inline function stops
 * \param ifunc an inlined function
 * \return the address where the inline function stops
 */
int64_t dwarf_inlinedFunction_get_high_pc(DwarfInlinedFunction* ifunc)
{
   if (ifunc == NULL)
      return (-1);
   return (ifunc->high_pc);
}

/*
 * Retrieves an array of ranges extracted from DWARF
 * \param ifunc an inlined function
 * \param size if not NULL, used to return the number of ranges
 * \return an array of ranges extracted from DWARF
 */
Dwarf_Ranges* dwarf_inlinedFunction_get_ranges(DwarfInlinedFunction* ifunc,
      int* size)
{
   if (ifunc == NULL)
      return (NULL);
   if (size != NULL)
      *size = ifunc->nb_ranges;
   return (ifunc->ranges);
}

/*
 * Retrieve the name of the inlined function
 * \param function A DwarfInlinedFunction object
 * \return A string containing the name of the inlined function, or NULL if unavailable
 */
char* dwarf_inlinedFunction_get_name(DwarfInlinedFunction *ifunc)
{
   if (ifunc != NULL) {
      DwarfFunction* ofunc = dwarf_inlinedFunction_get_origin_function(ifunc);
      return dwarf_function_get_name(ofunc);
   } else
      return (NULL);
}

/* ------------------------- DwarfVar functions ---------------------------- */
/*
 * Retrieves the variable name
 * \param var a variable
 * \return the variable name or NULL if there is a problem
 */
char* dwarf_var_get_name(DwarfVar* var)
{
   if (var == NULL)
      return (NULL);
   else
      return (var->name);
}

/*
 * Retrieves the variable type (without const, static ...)
 * \param var a variable
 * \return the variable type or NULL if there is a problem
 */
char* dwarf_var_get_type(DwarfVar* var)
{
   if (var == NULL)
      return (NULL);
   else
      return (var->type);
}

/*
 * Retrieves the variable full type (for example const char**)
 * \param var a variable
 * \return the variable full type or NULL if there is a problem
 */
char* dwarf_var_get_full_type(DwarfVar* var)
{
   if (var == NULL)
      return (NULL);
   else
      return (var->full_type);
}

/*
 * Retrieves the function a variable belongs to
 * \param var a variable
 * \return the function a variable belongs to or NULL if there is a problem
 */
DwarfFunction* dwarf_var_get_function(DwarfVar* var)
{
   if (var == NULL)
      return (NULL);
   else
      return (var->function);
}

/*
 * Retrieves the variable structure if it is not a native type
 * \param var a variable
 * \return the variable structure or NULL if there is a problem
 */
DwarfStruct* dwarf_var_get_structure(DwarfVar* var)
{
   if (var == NULL)
      return (NULL);
   else
      return (var->struc);
}

/*
 * Retrieves the position of the member in the structure
 * \param var a variable
 * \return the position of the member in the structure
 */
int dwarf_var_get_position_in_structure(DwarfVar* var)
{
   if (var == NULL)
      return (0);
   else
      return (var->member_location);
}

/*
 * Retrieves the location of a variable
 * \param var a variable
 * \return the location of the variable else NULL
 */
DwarfMemLoc* dwarf_var_get_location(DwarfVar* var)
{
   if (var == NULL)
      return (NULL);
   else
      return (var->location);
}

/*
 * Retrieves the source line of a variable
 * \param var a variable
 * \return the source line of the variable else -1
 */
int dwarf_var_get_source_line(DwarfVar* var)
{
   if (var == NULL)
      return (-1);
   else
      return (var->src_l);
}

/*
 * Retrieves the source column of a variable
 * \param var a variable
 * \return the source column of the variable else -1
 */
int dwarf_var_get_source_column(DwarfVar* var)
{
   if (var == NULL)
      return (-1);
   else
      return (var->src_c);
}

/*
 * Checks if the variable is a constant
 * \param var a variable
 * \return 1 if the variable is constant, else 0
 */
int dwarf_var_is_const(DwarfVar* var)
{
   if (var == NULL)
      return (0);
   else
      return (var->state & DL_CONST);
}

/*
 * Checks if the variable is static
 * \param var a variable
 * \return 1 if the variable is static, else 0
 */
int dwarf_var_is_static(DwarfVar* var)
{
   if (var == NULL)
      return (0);
   else
      return (var->state & DL_STATIC);
}

/*
 * Get the number of pointers.
 * \param var a variable
 * \return 1 if the variable is static, else 0
 */
int dwarf_var_get_pointer_number(DwarfVar* var)
{
   if (var == NULL)
      return (0);
   else
      return (var->pointer);
}

/* ------------------------ DwarfStruct functions -------------------------- */
/*
 * Retrieves the name of a structure
 * \param struc a structure
 * \return the structure name or NULL if there is a problem
 */
char* dwarf_struct_get_name(DwarfStruct* struc)
{
   if (struc == NULL)
      return (NULL);
   else
      return (struc->name);
}

/*
 * Retrieves the size of a structure
 * \param struc a structure
 * \return the structure size or 0 if there is a problem
 */
int dwarf_struct_get_size(DwarfStruct* struc)
{
   if (struc == NULL)
      return (0);
   else
      return (struc->size);
}

/*
 * Retrieves the members of a structure. Each member is a DwarfVar.
 * \param struc a structure
 * \return the queue of structure members or NULL if there is a problem
 */
queue_t* dwarf_struct_get_members(DwarfStruct* struc)
{
   if (struc == NULL)
      return (NULL);
   else
      return (struc->members);
}

/*
 * Retrieves the type of a structure
 * \param struc a structure
 * \return the structure type or DS_NOTYPE if there is a problem
 */
char dwarf_struct_get_type(DwarfStruct* struc)
{
   if (struc == NULL)
      return (DS_NOTYPE);
   else
      return (struc->type);
}

/*
 * Checks if the structure is an union
 * \param struc a structure
 * \return TRUE if the structure is an union else FALSE
 */
int dwarf_struct_is_union(DwarfStruct* struc)
{
   if (struc == NULL)
      return (FALSE);
   else
      return ((struc->type == DS_UNION) ? TRUE : FALSE);
}

/*
 * Checks if the structure is an struct
 * \param struc a structure
 * \return TRUE if the structure is an struct else FALSE
 */
int dwarf_struct_is_struct(DwarfStruct* struc)
{
   if (struc == NULL)
      return (FALSE);
   else
      return ((struc->type == DS_STRUCT) ? TRUE : FALSE);
}

/* ------------------------ DwarfMemLoc functions -------------------------- */
/*
 * Retrieves the type of a memory location
 * \param memloc a memory location
 * \return the type of a memory location or 0
 */
int dwarf_memloc_get_type(DwarfMemLoc* memloc)
{
   if (memloc == NULL)
      return (DWARF_NONE);
   else
      return (memloc->type);
}

/*
 * Retrieves the register of a memory location
 * \param memloc a memory location
 * \return the register of a memory location or NULL
 */
reg_t* dwarf_memloc_get_register(DwarfMemLoc* memloc)
{
   if (memloc == NULL)
      return (NULL);
   else
      return (memloc->reg);
}

/*
 * Retrieves the Dwarf index used to represent the register
 * \param memloc a memory location
 * \return the Dwarf index used to represent the register or -1
 */
int dwarf_memloc_get_register_index(DwarfMemLoc* memloc)
{
   if (memloc == NULL)
      return (-1);
   else
      return (memloc->index);
}

/*
 * Retrieves the offset member of a memory location
 * \param memloc a memory location
 * \return the offset member of a memory location or 0
 */
int dwarf_memloc_get_offset(DwarfMemLoc* memloc)
{
   if (memloc == NULL)
      return (0);
   else
      return (memloc->mem.offset);
}

/*
 * Retrieves the address member of a memory location
 * \param memloc a memory location
 * \return the address member of a memory location or 0
 */
int64_t dwarf_memloc_get_address(DwarfMemLoc* memloc)
{
   if (memloc == NULL)
      return (0);
   else
      return (memloc->mem.address);
}

#define RANGE_CODE_NONE 0  // Nothing special
#define RANGE_CODE_DEL  1  // Delete the returned element
#define RANGE_CODE_ADD  2  // Add the returned element

/*
 * Splits a range according to new bounds
 * \param range a range to split
 * \param start
 * \param stop
 * \param code used to return a RANGE_CODE_ value
 * \return
 */
static fct_range_t* __split_range(fct_range_t* range, insn_t* start,
      insn_t* stop, char* code)
{
   if (start == NULL || stop == NULL) {
      DBGMSG("Error: 0x%"PRIx64" 0x%"PRIx64"\n", INSN_GET_ADDR(start),
            INSN_GET_ADDR(stop));
      return (NULL);
   }

   // Check that we are not out of bounds -------------------------------------
   // if yes, change start / stop to be at the first value in the bounds
   if (INSN_GET_ADDR (start) < INSN_GET_ADDR(range->start)) {
      while (INSN_GET_ADDR (start) < INSN_GET_ADDR(range->start))
         start = INSN_GET_NEXT(start);
   }
   if (INSN_GET_ADDR (stop) > INSN_GET_ADDR(range->stop)) {
      while (INSN_GET_ADDR (stop) > INSN_GET_ADDR(range->stop))
         stop = INSN_GET_PREV(stop);
   }

   DBGMSG("[0x%"PRIx64"; 0x%"PRIx64"]\n\t", INSN_GET_ADDR(start),
         INSN_GET_ADDR(stop));

   // According to values, update the input range -----------------------------
   if (range->start >= start && range->stop <= stop) {
      *code = RANGE_CODE_DEL;
      DBGMSG("To delete: [0x%"PRIx64"; 0x%"PRIx64"]\n",
            INSN_GET_ADDR(range->start), INSN_GET_ADDR(range->stop));
      return (range);
   } else if (range->start == start) {
      DBGMSG("[0x%"PRIx64"; 0x%"PRIx64"] => [0x%"PRIx64"; 0x%"PRIx64"]\n",
            INSN_GET_ADDR(range->start), INSN_GET_ADDR(range->stop),
            INSN_GET_ADDR(stop), INSN_GET_ADDR(range->stop));
      *code = RANGE_CODE_NONE;
      range->start = stop;
   } else if (range->stop == stop) {
      DBGMSG("[0x%"PRIx64"; 0x%"PRIx64"] => [0x%"PRIx64"; 0x%"PRIx64"]\n",
            INSN_GET_ADDR(range->start), INSN_GET_ADDR(range->stop),
            INSN_GET_ADDR(range->start), INSN_GET_ADDR(INSN_GET_PREV(start)));
      *code = RANGE_CODE_NONE;
      range->stop = INSN_GET_PREV(start);
   } else {
      DBGMSG(
            "[0x%"PRIx64"; 0x%"PRIx64"] => [0x%"PRIx64"; 0x%"PRIx64"] + [0x%"PRIx64"; 0x%"PRIx64"]\n",
            INSN_GET_ADDR(range->start), INSN_GET_ADDR(range->stop),
            INSN_GET_ADDR(range->start), INSN_GET_ADDR(INSN_GET_PREV(start)),
            INSN_GET_ADDR(stop), INSN_GET_ADDR(range->stop));

      fct_range_t* new_range = fct_range_new(stop, range->stop);
      range->stop = INSN_GET_PREV(start);

      *code = RANGE_CODE_ADD;
      return (new_range);
   }
   return (NULL);
}

static void _find_inlined_ranges(asmfile_t* asmf, DwarfFunction* dfct)
{
   // Iterate over DWARF ranges to find inlined ranges
   FOREACH_INQUEUE(dwarf_function_get_inlined(dfct), it_difct)
   {
      DwarfInlinedFunction* difct = GET_DATA_T(DwarfInlinedFunction*, it_difct);
      int nb_ranges = 0;
      Dwarf_Ranges* dranges = dwarf_inlinedFunction_get_ranges(difct,
            &nb_ranges);
      DwarfFunction* odifct = dwarf_inlinedFunction_get_origin_function(difct);

      if (odifct == NULL)
         continue;

      // If there is ranges, analyze each of them
      if (nb_ranges > 0) {
         int i = 0;
         for (i = 0; i < nb_ranges; i++) {
            insn_t* stop = asmfile_get_insn_by_addr(asmf, dranges[i].dwr_addr2);
            if (stop == NULL)
               continue;
            stop = INSN_GET_PREV(stop);

            fct_range_t* range = fct_range_new(
                  asmfile_get_insn_by_addr(asmf, dranges[i].dwr_addr1), stop);
            range->type = RANGE_INLINED;
            DBGMSG(
                  "INLINED range added in function %s [0x%"PRIx64"; 0x%"PRIx64"]\n",
                  odifct->name, (int64_t )dranges[i].dwr_addr1,
                  (int64_t )dranges[i].dwr_addr2);
            queue_add_tail(odifct->ranges, range);
         }
      }
      // Else analyze the inlined function using its low_pc / high_pc members
      else {
         insn_t* stop = asmfile_get_insn_by_addr(asmf,
               dwarf_inlinedFunction_get_high_pc(difct));
         if (stop == NULL)
            continue;
         stop = INSN_GET_PREV(stop);
         fct_range_t* range = fct_range_new(
               asmfile_get_insn_by_addr(asmf,
                     dwarf_inlinedFunction_get_low_pc(difct)), stop);
         range->type = RANGE_INLINED;
         DBGMSG(
               "INLINED range added in function %s [0x%"PRIx64"; 0x%"PRIx64"]\n",
               odifct->name, dwarf_inlinedFunction_get_low_pc(difct),
               dwarf_inlinedFunction_get_high_pc(difct));
         queue_add_tail(odifct->ranges, range);
      }
   }
}

static void _find_ranges(asmfile_t* asmf, DwarfFunction* dfct)
{
   // Iterate over original ranges to split them if they contain inlined ranges
   FOREACH_INQUEUE(dfct->ranges, it_ranges1)
   {
      fct_range_t* range = GET_DATA_T(fct_range_t*, it_ranges1);
      FOREACH_INQUEUE(dwarf_function_get_inlined(dfct), it_difct)
      {
         DwarfInlinedFunction* difct = GET_DATA_T(DwarfInlinedFunction*,
               it_difct);

         int nb_ranges = 0;
         int i = 0;
         Dwarf_Ranges* dranges = dwarf_inlinedFunction_get_ranges(difct,
               &nb_ranges);

         if (nb_ranges > 0) {
            for (i = 0; i < nb_ranges; i++) {
               if ((INSN_GET_ADDR(range->start)
                     <= (int64_t) dranges[i].dwr_addr1
                     && INSN_GET_ADDR(range->stop)
                           >= (int64_t) dranges[i].dwr_addr1)
                     || (INSN_GET_ADDR(range->start)
                           >= (int64_t) dranges[i].dwr_addr2
                           && INSN_GET_ADDR(range->stop)
                                 <= (int64_t) dranges[i].dwr_addr2)) {
                  char code = 0;
                  fct_range_t* res_range = __split_range(range,
                        asmfile_get_insn_by_addr(asmf, dranges[i].dwr_addr1),
                        asmfile_get_insn_by_addr(asmf, dranges[i].dwr_addr2),
                        &code);
                  if (code == RANGE_CODE_ADD)
                     queue_add_tail(dfct->ranges, res_range);
               }
            }
         } else if ((INSN_GET_ADDR(range->start)
               <= dwarf_inlinedFunction_get_low_pc(difct)
               && INSN_GET_ADDR(range->stop)
                     >= dwarf_inlinedFunction_get_low_pc(difct))
               || (INSN_GET_ADDR(range->start)
                     >= dwarf_inlinedFunction_get_high_pc(difct)
                     && INSN_GET_ADDR(range->stop)
                           <= dwarf_inlinedFunction_get_high_pc(difct))) {
            char code = 0;
            fct_range_t* res_range = __split_range(range,
                  asmfile_get_insn_by_addr(asmf,
                        dwarf_inlinedFunction_get_low_pc(difct)),
                  asmfile_get_insn_by_addr(asmf,
                        dwarf_inlinedFunction_get_high_pc(difct)), &code);
            if (code == RANGE_CODE_ADD) {
               queue_add_tail(dfct->ranges, res_range);
            }
         }
      }
   }
}

/*
 * Uses debug data to find ranges (can be used to detect inlining)
 * \param asmf an assembly file containing debug data
 */
void asmfile_detect_debug_ranges(asmfile_t* asmf)
{
   if (asmf == NULL || asmf->debug == NULL || asmf->debug->data == NULL)
      return;

   DwarfAPI* api = asmf->debug->data;
   FOREACH_INQUEUE(api->files, it_fi) {
      DwarfFile* dfile = GET_DATA_T(DwarfFile*, it_fi);
      FOREACH_INQUEUE(dfile->functions, it_fu)
      {
         DwarfFunction* df = GET_DATA_T(DwarfFunction*, it_fu);
         DBGMSG("*** Analyze debug ranges for function %s ***\n", df->name)
         FOREACH_INQUEUE(df->ranges, it_r) {
            fct_range_t* fctr = GET_DATA_T(fct_range_t*, it_r);
            if (fctr->type == 0) {
               DBGMSG("Start range of function %s [0x%"PRIx64"; 0x%"PRIx64"]\n",
                     df->name, fctr->start->address, fctr->stop->address);
            }
         }
         _find_inlined_ranges(asmf, df);
         _find_ranges(asmf, df);
      }
   }
   api->is_range = TRUE;
}
