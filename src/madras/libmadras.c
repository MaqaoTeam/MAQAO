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
 * \file libmadras.c
 * \brief This file contains the code of the libmadras API (defined in libmadras.h)
 * */

/**\todo This whole interface needs to be heavily updated to fully use the possibilities of libmasm, especially for the functions
 * returning details about the file or instructions list
 * => TODO (2014-11-18) Rewrite everything after the patcher rewrite. Non exhaustive list:
 * - Change the bloody name of the structure to madras_t or something like that (beware to the LUA plug-ins, which use that name)
 * - Remove most if not all getter functions
 * - Remove the cursor
 * (more to be added as I think of them)*/

#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdarg.h>

#include "assembler.h"
#include "libmadras.h"
#include "libmdbg.h"
#include "bfile_fmtinterface.h"

/**\bug The convention of always checking whether a pointer is NULL before applying it the -> operator to access its fields
 * is almost never obeyed in this file*/

/* Structures */
/**************/

struct logger_s {
   char* tracefile; /**<Name of the file where MADRAS traces will be written*/
   FILE* tracestream; /**<Stream to the trace file*/
   unsigned char trace; /**<Set to 0 to disable the tracing. Nonzero value will enable it
    (in future implementations, will set trace level)*/
};

#define STR_INSN_BUF_SIZE 4096

/**
 * Macro used for writing tracing messages
 * \param M Pointer to a madras structure
 * \param ... Other arguments
 * **/
#define TRACE(M,...) {if(M && M->loginfo->trace) { fprintf(M->loginfo->tracestream,__VA_ARGS__);}DBG(STDMSG(__VA_ARGS__));}

/**
 * Macro used for writing the end of a tracing message for a function returning a pointer to a structure
 * \param MDRS Pointer to a madras structure
 * \param PTR Pointer to the structure
 * \param TYPE Name of the structure (used for printing its id)
 * */
#define TRACE_END(MDRS,PTR,TYPE) TRACE(MDRS,")="#TYPE"_%ld\n",(((int64_t)PTR > 0)?PTR->TYPE##_id:(int64_t)PTR));

/*    Trace functions    */
/*************************/

/*
 * Enable trace logging of the operations
 * \param ed A pointer to the structure holding the disassembled file
 * \param filename Name of the file to log the trace to. If set to NULL at the first invocation of \c madras_traceon,
 * the file specified by DFLT_TRACELOG will be used. This parameter is ignored for subsequent invocation of the function
 * \param lvl Level of the tracing. Currently not used
 * */
void madras_traceon(elfdis_t *ed, char* filename, unsigned int lvl)
{
   (void) lvl;
   if (ed == NULL) {
      // error
      return;
   }

   if (!ed->loginfo->tracefile) { //First call to madras_traceon
      //Sets the trace file name
      if (filename)
         ed->loginfo->tracefile = lc_strdup(filename);
      else
         ed->loginfo->tracefile = lc_strdup(DFLT_TRACELOG);
      //Initializes trace file
      if ((ed->loginfo->tracestream = fopen(ed->loginfo->tracefile, "w"))) {
         //Enable tracing mode
         ed->loginfo->trace = 1; //For further implementation, use lvl (capped at 255)
      } else {      //Disable trace mode if the file could not be opened
         fprintf(stderr, "Error, unable to create trace file %s\n",
               ed->loginfo->tracefile);
         ed->loginfo->trace = 0;
      }
   } else {      //The trace file has already been created
      //Reopens trace file
      if ((ed->loginfo->tracestream = fopen(ed->loginfo->tracefile, "a"))) {
         //Enable tracing mode
         ed->loginfo->trace = 1; //For further implementation, use lvl (capped at 255)
      } else {      //Disable trace mode if the file could not be opened
         fprintf(stderr, "Error, unable to reopen trace file %s\n",
               ed->loginfo->tracefile);
         ed->loginfo->trace = 0;
      }
   }
}

/*
 * Disable trace logging of the operations
 * \param ed A pointer to the structure holding the disassembled file
 * \param filename Name of the concerned trace file. This parameter is currently not used
 * */
void madras_traceoff(elfdis_t *ed, char* filename)
{
   (void) filename;
   if (ed == NULL) {
      //error
      return;
   }

   if (ed->loginfo->tracestream) {
      if (fclose(ed->loginfo->tracestream)) {
         fprintf(stderr, "Warning, unable to close trace file %s\n",
               ed->loginfo->tracefile);
      }
      ed->loginfo->tracestream = NULL;
   }
   ed->loginfo->trace = 0;
}

/*
 * Returns the code of the last error encountered and resets it to EXIT_SUCCESS
 * \param ed A pointer to the MADRAS structure
 * \return The existing error code stored in \c ed or ERR_MADRAS_MISSING_MADRAS_STRUCTURE if \c ed is NULL
 * */
int madras_get_last_error_code(elfdis_t* ed)
{
   if (!ed)
      return ERR_MADRAS_MISSING_MADRAS_STRUCTURE;
   int errcode = ed->last_error_code;
   ed->last_error_code = EXIT_SUCCESS;
   return errcode;
}

/**
 * Sets the code of the last error encountered
 * \param ed A pointer to the MADRAS structure
 * \param errcode The error code to set
 * \return The existing error code stored in \c ed or ERR_MADRAS_MISSING_MADRAS_STRUCTURE if \c ed is NULL
 * */
static int madras_set_last_error_code(elfdis_t* ed, int errcode)
{
   int out;
   if (!ed)
      return ERR_MADRAS_MISSING_MADRAS_STRUCTURE;
   out = ed->last_error_code;
   ed->last_error_code = errcode;
   return out;
}

/**
 * Sets the code of the last error encountered and uses a default value if the error code given is 0
 * \param ed A pointer to the MADRAS structure
 * \param errcode The error code to set
 * \param dflterrcode The error code to use instead of \c errcode if errcode is EXIT_SUCCESS
 * \return The existing error code stored in \c ed or ERR_MADRAS_MISSING_MADRAS_STRUCTURE if \c ed is NULL
 * */
static int madras_transfer_last_error_code(elfdis_t* ed, int errcode,
      int dflterrcode)
{
   int out;
   if (errcode != EXIT_SUCCESS)
      out = madras_set_last_error_code(ed, errcode);
   else
      out = madras_set_last_error_code(ed, dflterrcode);
   return out;
}

/**
 * Returns the type for a condition given its code
 * \param condcode Type of the condition
 * \return Type of the condition:
 * - equal if 'e',
 * - non equal if 'n'
 * - less or equal if 'l'
 * - less if 'L'
 * - greater or equal if 'g'
 * - greater if 'G'
 * - none for unrecognized type
 * \todo Write this someplace else, like all low level functions devoted to conditions.
 * */
static char cond_type_fromcode(char condcode)
{
   switch (condcode) {
   case '.':
      return COND_AND;      //That one is only for coherence with cond_typecode
   case '+':
      return COND_OR;      //That one is only for coherence with cond_typecode
   case 'e': /**<EQUAL condition (to be used between an operand and a value)*/
      return COND_EQUAL;
   case 'n': /**<NOT EQUAL condition  (to be used between an operand and a value)*/
      return COND_NEQUAL;
   case 'L': /**<LESS condition (to be used between an operand and a value)*/
      return COND_LESS;
   case 'G': /**<MORE condition (to be used between an operand and a value)*/
      return COND_GREATER;
   case 'l': /**<LESS or EQUAL condition (to be used between an operand and a value)*/
      return COND_EQUALLESS;
   case 'g': /**<MORE or EQUAL condition (to be used between an operand and a value)*/
      return COND_EQUALGREATER;
   }
   return COND_VOID;
}

/*
 * Prints a condition to a string
 * \param ed A pointer to the MADRAS structure
 * \param cond The condition
 * \param str The string to print to
 * \param size Size of the string to print to
 * */
void madras_cond_print(elfdis_t* ed, cond_t* cond, char* str, size_t size)
{
   cond_print(cond, str, size, asmfile_get_arch(ed->afile));
}

/**
 * Creates a new, empty disassembled file structure
 * \return A pointer to the empty structure
 * */
static elfdis_t* elfdis_new()
{
   elfdis_t* new = (elfdis_t*) lc_malloc0(sizeof(elfdis_t));

   new->loginfo = lc_malloc0(sizeof(logger_t));
   return new;
}

/**
 * Frees a patcher context
 * \param ed The MADRAS structure containing the patching session
 * */
static void modifs_free(elfdis_t *ed)
{
   if (!ed)
      return;

   patchfile_free(ed->patchfile);
}
#if 0
/**
 * Finds an ELF section index containing a given address
 * \param ed Structure from the MADRAS file
 * \param addr The address to look for
 * \return The section id containing the address, or -1 if no such section was found
 * \todo Find exactly why the function elffile_getscnspanaddr needs to be more complicated than that
 * */
static int elfdis_findcodescn_frominsnaddr(elfdis_t* ed, int64_t addr)
{
   int i = 0;
   if (ed == NULL)
      return -1;

   elffile_t* efile = asmfile_get_origin(ed->afile);
   while (i < efile->nsections) {
      if (elffile_getscnstartaddr(efile, i) >= addr)
         break;
      i++;
   }
   if (i == efile->nsections)
      return -1;
   else
      return i;
}
#endif
/**
 * Fills an elfdis structure from its disassembled elf file \e afile member
 * \param ed A pointer to a structure for holding a disassembled file, whose \e afile member
 * already contains a disassembled file
 * \todo This function's purpose has changed a lot since the implementation of libmasm. It may become useless when the cursor is
 * completely removed
 * */
static void elfdis_initialize(elfdis_t* ed)
{
   if (ed != NULL && ed->afile != NULL) {
      ed->cursor = queue_iterator(asmfile_get_insns(ed->afile));
   }
}

/**
 * Refreshes an elfdis file from the file it was created from
 * \param ed Pointer to a structure holding the disassembled file
 * \bug This function may cause memory leaks because of bugs in the functions called by
 * libmpatch:patchfile_reload
 * */
static void elfdis_refresh(elfdis_t* ed)
{
   //Reloads the elffile object
   //patchfile_reload(ed->elffile); //Don't delete this code, it will be needed when the bug above is resolved
   elfdis_initialize(ed);
}
#if 0
/**
 * Finds an instruction at a given address
 * \param ed Structure for storing the madras file
 * \param addr Address to look for
 * \param nscn Return parameter. Will contain the number of the section where the instruction was found
 * \return List object containing the instruction
 * \todo This function may become unnecessary after the patcher refactoring and the full use of libmasm by libmadras
 * */
static list_t* elfdis_findinsnataddress(elfdis_t* ed, int64_t addr, int *nscn)
{
   if (ed == NULL || addr < 0) {
      //error
      return (NULL);
   }

   /**\todo Remove nscn from this function, it is only used by insnmodify_apply*/
   list_t* out = NULL;

   // Looking up the instruction in the asmfile from its address
   insn_t* insn = asmfile_get_insn_by_addr(ed->afile, addr);
   if ((insn == NULL) || ((out = insn_get_sequence(insn)) == NULL)) {
      DBGMSGLVL(1,
            "Instruction at address %#"PRIx64" was not found using asmfile_get_insn_by_addr\n",
            addr);
      //We could not find the instruction in the asmfile or it does not have a sequence: we take the slow path and look up all addresses
      FOREACH_INQUEUE(asmfile_get_insns(ed->afile), iter)
      {
         if (INSN_GET_ADDR(GET_DATA_T(insn_t*, iter)) == addr) {
            out = iter;
            DBGMSGLVL(1, "Found instruction at address %#"PRIx64"\n", addr);
            break;
         }
      }
   }
   if ((out != NULL) && (nscn != NULL)) {
      int scn = elfdis_findcodescn_frominsnaddr(ed, addr);
      /**\todo This is a useless waste of time. Fix it, we don't need to constantly juggle between the section index and the
       * code section index in the patchfile*/
      int i;
      for (i = 0; i < ed->patchfile->n_codescn_idx; i++)
      if (scn == ed->patchfile->codescn_idx[i])
      break;
      *nscn = i;
      DBGMSGLVL(1, "Instruction is in section %d\n", *nscn);
   }
   if ((out == NULL) && (ed->patchfile != NULL)
         && (ed->patchfile->patch_list != NULL)) {
      FOREACH_INQUEUE(ed->patchfile->patch_list, iter) {
         if (INSN_GET_ADDR(GET_DATA_T(insn_t*, iter)) == addr) {
            out = iter;
            DBGMSGLVL(1, "Found instruction at address %#"PRIx64"\n", addr);
            break;
         }
      }
      if ((out != NULL) && (nscn != NULL)) {
         *nscn = -1;
         DBGMSGLVL(1, "Instruction is in section %d\n", *nscn);
      }
   }
   return out;
}
#endif
/**
 * Converts option flags from libmadras to limbpatch
 * \param flags The flags to convert
 * \return The converted flags
 * */
static int flags_madras2patcher(int flags)
{
   /**\todo TODO (2014-10-28) I really, really hate having to do this. Find some way to avoid it. For instance, declare enums for flags,
    * with the actual options being a #define myoption (1<<correspondingenum), and add some assert to check that we have the same max number
    * in the enum of the patcher and the enum of madras. Only thing is I would like to avoid having the enum and the defines appear in libmadras.h*/
   int out = PATCHFLAG_NONE;

   if (flags & PATCHOPT_FORCEINS)
      out |= PATCHFLAG_FORCEINSERT;

   if (flags & PATCHOPT_MOVEFCTS)
      out |= PATCHFLAG_MOVEFCTS;

   if (flags & PATCHOPT_MOV1INSN)
      out |= PATCHFLAG_MOV1INSN;

   if (flags & PATCHOPT_STACK_MOVE)
      out |= PATCHFLAG_NEWSTACK;

   if (flags & PATCHOPT_NO_UPD_INTERNAL_BRANCHES)
      out |= PATCHFLAG_INSERT_NO_UPD_FROMFCT;

   if (flags & PATCHOPT_NO_UPD_EXTERNAL_BRANCHES)
      out |= PATCHFLAG_INSERT_NO_UPD_OUTFCT;

   if (flags & PATCHOPT_FCTCALL_NOWRAP)
      out |= PATCHFLAG_NOWRAPFCTCALL;

   if (flags & PATCHOPT_FCTCALL_FCTONLY)
      out |= PATCHFLAG_INSERT_FCTONLY;

   if (flags & PATCHOPT_BRANCHINS_NO_UPD_DST)
      out |= PATCHFLAG_BRANCH_NO_UPD_DST;

   if (flags & PATCHOPT_MODIF_FIXED)
      out |= PATCHFLAG_MODIF_FIXED;

   return out;
}

/*
 * Disassembles a file
 * \param filename The name or relative path of the file to disassemble
 * \return A pointer to a structure holding the disassembling results, or a NULL or negative value
 * if an error occurred
 * */
elfdis_t* madras_disass_file(char* filename)
{
   if (filename == NULL) {
      //error
      return (NULL);
   }

   /**\todo Merge this function with madras_modifs_init to have a single function madras_loadfile.
    * Also add functions for changing the stack handling*/
   elfdis_t* ed = NULL;

   /**\todo Error handling*/
   /*Creates a structure holding the disassembly results*/
   ed = elfdis_new();
   ed->name = lc_strdup(filename);
   ed->patchfile = NULL;

   ed->afile = asmfile_new(filename);

   /*Disassembles the file*/
   asmfile_disassemble(ed->afile); /**\todo If this is used only by the patcher, we may use DISASSEMBLY_NODBG here => (2014-11-18) OBSOLETE, REWRITE THIS*/
   elfdis_initialize(ed);
   asmfile_load_dbg(ed->afile);
   ed->loaded = FALSE;

   return ed;
}

/*
 * Generate an \c elfdis_t structure from an already disassembled file.
 * \warning This function is here to allow a bridge between applications using the high-level API (libmadras.h)
 * and applications which used MADRAS low-level API (madrasapi.h) to disassemble a file. madras_disass_file should be
 * used instead.
 * \param parsed A asmfile_t structure (from libmasm.h)
 * \return A pointer to a structure holding the disassembling results, or a NULL or negative value
 * if an error occurred
 * */
elfdis_t* madras_load_parsed(asmfile_t* parsed)
{
   /** \todo Update or remove */

   if (parsed == NULL) {
      //error
      return (NULL);
   }

   elfdis_t* ed = elfdis_new();
   if (parsed->name)
      ed->name = lc_strdup(asmfile_get_name(parsed));
   ed->patchfile = NULL;

   //Disassembles the file
   ed->afile = parsed;
   elfdis_initialize(ed);
   ed->loaded = TRUE;

   return ed;
}

/*
 * Removes a parsed file from an \em elfdis_t structure.
 * \warning This function should only be used on elfdis_t structures retrieved from a call to \em madras_load_parsed.
 * It allows to use \em madras_terminate on the structure, but the parsedfile_t structure used in madras_load_parsed must
 * be freed elsewhere
 * \param ed A pointer to the structure holding the disassembled file
 * \return A pointer to the parsedfile_t structure used to initialise the file
 * */
asmfile_t* madras_unload_parsed(elfdis_t* ed)
{
   /** \todo Update or remove */

   if (ed == NULL) {
      //error
      return (NULL);
   }

   asmfile_t* parsed = ed->afile;
   ed->afile = NULL;
   return parsed;
}

/*
 * Retrieves the type of a disassembled file (executable, shared, relocatable)
 * \param ed A pointer to the structure holding the disassembled file
 * \return An integer equal to the type coding of the file, as defined in \b \ref bf_types.
 * */
int madras_get_type(elfdis_t* ed)
{
   int out = UNKNOWN_FT;
   if ((ed) && (asmfile_get_binfile(ed->afile))) {
      out = binfile_get_type(asmfile_get_binfile(ed->afile));
//        if (elffile_isexe(asmfile_get_origin(ed->afile)))
//            out = EXECUTABLE_FT;
//        else if ( elffile_isdyn(asmfile_getorigin(ed->afile)) )
//            out = DYNLIBRARY_FT;
//        else if ( elffile_isobj(asmfile_getorigin(ed->afile)))
//            out = RELOCATABLE_FT;
   }
   return out;
}

/*
 * Retrieves the architecture for which a disassembled file is intended
 * \param ed A pointer to the structure holding the disassembled file
 * \return An string representation of the architecture coding of the file
 * */
char* madras_get_arch(elfdis_t* ed)
{
   if (ed == NULL) {
      //error
      return (NULL);
   }
   return arch_get_name(asmfile_get_arch(ed->afile));
}

/*
 * Retrieves the name of the ELF section the instruction pointed to by the cursor
 * belongs to
 * \param ed A pointer to the structure holding the disassembled file
 * \return The name of the ELF section the cursor is in, or a NULL or negative value
 * if an error occurred
 * */
char* madras_get_scn_name(elfdis_t* ed)
{
   if (ed == NULL) {
      //error
      return (NULL);
   }

   /**\todo Don't use a negative value but errno instead */
   char* out = NULL;
   if ((ed->cursor != NULL) && (GET_DATA_T(insn_t*, ed->cursor) != NULL)) {
      out = binscn_get_name(label_get_scn(insn_get_fctlbl(GET_DATA_T(insn_t*, ed->cursor))));
//        int nscn = elfdis_findcodescn_frominsnaddr(ed,insn_get_addr(GET_DATA_T(insn_t*,ed->cursor)));
//        if ( (nscn >= 0 ) && ( asmfile_getorigin(ed->afile) != NULL) )
//            out = elfsection_name(asmfile_getorigin(ed->afile),nscn);
   }

   return out;
}

/*
 * Retrieves the start and end addresses of a given ELF section
 * \param ed: Structure holding the disassembled file
 * \param scnname: Name of the ELF section
 * \param scnidx: Index of the ELF section (will be taken into account only if scnname is NULL)
 * \param start: Return parameter, will contain the starting address of the section if not NULL
 * \param end: Return parameter, will contain the end address of the section if not NULL
 * \return EXIT_SUCCESS if successful, error code otherwise.
 * */
int madras_get_scn_boundaries(elfdis_t* ed, char* scnname, int scnidx,
      int64_t* start, int64_t* end)
{
   /**\todo TODO (2015-05-05) This function is not used anywhere except in test files. I'm updating it
    * to take the new binfile into account, but in the future, it should simply be removed, or simplified
    * and moved to the libbin*/
   if (ed == NULL) {
      //error
      return (0);
   }

   /**\todo Update the error code*/
   int out = 1;

   binscn_t* scn = NULL;
//    elffile_t* efile = asmfile_getorigin(ed->afile);
   binfile_t* bf = asmfile_get_binfile(ed->afile);
   // Retrieves the section index
   if (scnname) {
      scn = binfile_lookup_scn_by_name(bf, scnname);
   } else {
      scn = binfile_get_scn(bf, scnidx);
   }
   if (scn) {   //Section index valid
      //Retrieves start address
      if (start)
         *start = binscn_get_addr(scn);
      //Retrieves end address
      if (end)
         *end = binscn_get_end_addr(scn);
   } else {
      out = ERR_BINARY_SECTION_NOT_FOUND;
   }


   return out;
}

/*
 * Links a branch instruction to another instruction at a given address
 * \param ed Pointer to the madras structure
 * \param in Pointer to the branch instruction to link
 * \param addr Address in the file to which the instruction must point
 * \param update Indicate if the target address has to be updated when insertion are performed before it
 * \return EXIT_SUCCESS if successful, error code if no instruction was found at the given address
 * \note The use of \ref madras_branch_insert is recommended for inserting branch instructions to a given address
 * */
int madras_set_branch_target(elfdis_t* ed, insn_t* in, int64_t addr, int update)
{
   char buf[STR_INSN_BUF_SIZE];
   insn_print(in, buf, sizeof(buf));
   TRACE(ed, "madras_set_branch_target(in=%#"PRIx64":%s,addr=%#"PRIx64", update=%d)\n", insn_get_addr(in), (in)?buf:NULL, addr, update);
   insn_t* dst = asmfile_get_insn_by_addr(ed->afile, addr);
   //ERRMSG("madras_set_branch_target is not implemented in this version\n");
   /**\todo TODO (2015-01-21) Implement this using the refactoring. Basically we will either point to the original instruction or
    * its duplicate in a patchinsn structure, but patchinsn structures are crated during commit, so I guess we would need to create a
    * special type of modification for this (or simply use an insnmodify with the correct parameters)
    * => (2015-06-03) Now using the new function madras_modify_branch for that. I position the cursor before passing it an address of 0
    * so that it will not look up the instruction again, but something more streamlined has to be set up*/
   ed->cursor = insn_get_sequence(in);
   if (ed->cursor) {
      modif_t* modif = madras_modify_branch(ed, 0, FALSE, NULL, addr);
      if (!modif)
         return FALSE;
      if (!update)
         modif->flags |= PATCHFLAG_BRANCH_NO_UPD_DST;
   } else {
      //This is a new instruction that does belong to the original file
      /**\todo TODO (2015-06-03) This case should not happen. Branch instructions could be inserted regularly
       * using madras_add_insn(s) or madras_insnlist_add, and properly linked to their destination. I'm handling
       * this for compatibility with existing uses of this function by MIL, which apparently adds new instructions
       * to it.*/
      insn_t* dst = asmfile_get_insn_by_addr(ed->afile, addr);
   if (!dst) {
      ERRMSG("No instruction found at address %#"PRIx64"\n", addr);
      return ERR_LIBASM_INSTRUCTION_NOT_FOUND;
   }
      patchfile_setbranch(ed->patchfile, in, dst, NULL);
   }
#if 0
   if (update) {
      patchfile_setbranch(ed->patchfile, in, dst);/**\todo Also add a check on the return value of this function (currently void)*/
   } else {
      patchfile_setbranch_noupd(ed->patchfile, in, dst);/**\todo Also add a check on the return value of this function (currently void)*/
   }
#endif
   //DBGMSGLVL(1,"Linking instruction %p to instruction %p at address %#"PRIx64"\n", in, dst, addr);

   return EXIT_SUCCESS;
}

/* Call madras_set_branch_target with update set to False */
int madras_linkbranch_toaddr(elfdis_t* ed, insn_t* in, int64_t addr)
{
   /**\todo Remove this function (it is used in MIL). Use \ref madras_branch_insert if necessary*/
   return madras_set_branch_target(ed, in, addr, FALSE);
}

/*
 * Returns a branch instruction opposite to the instruction provided
 * \param ed Pointer to the structure holding the disassembled file containing the instruction
 * \param in Pointer to the branch instruction to reverse
 * \param addr Address of the branch instruction to reverse (will be taken into account if \c in is NULL)
 * \param cond Return parameter. Will contain a condition corresponding to the reverse branch if it has no opposite in this architecture
 * \return Pointer to a new instruction corresponding to the reversed branch, NULL if \c in is not a branch or is not reversible
 * In this case, the last error code in \c ed will be updated
 * */
insn_t* madras_get_oppositebranch(elfdis_t* ed, insn_t* in, int64_t addr,
      cond_t** cond)
{
   patchdriver_t* driver;
   insn_t* insn, *out;
   oprnd_t* condop = NULL;
   int64_t condval = 0;
   char condtype = 0;
   if (ed == NULL) {
      return NULL;
   }
   char buf[STR_INSN_BUF_SIZE];
   insn_print(in, buf, sizeof(buf));
   TRACE(ed, "madras_get_oppositebranch(in=%#"PRIx64":%s,addr=%#"PRIx64")\n", insn_get_addr(in), (in)?buf:NULL, addr);
   if (in == NULL)
      insn = asmfile_get_insn_by_addr(ed->afile, addr);
   else
      insn = in;
   if (insn == NULL) {
      ed->last_error_code = ERR_LIBASM_INSTRUCTION_NOT_FOUND;
      return NULL;
   }

   //Retrieves a driver for the given architecture
   if (ed->patchfile == NULL)
      driver = patchdriver_load(asmfile_get_arch(ed->afile));
   else
      driver = ed->patchfile->patchdriver;

   /*Invokes the architecture specific function for inverting a branch. It will return NULL if insn is not a branch,
    * and insn if insn has not opposite*/
   out = driver->generate_opposite_branch(insn, &condop, &condval, &condtype);
   /**\todo Do we print error messages ?*/
   if (out == NULL) {
      ed->last_error_code = ERR_LIBASM_INSTRUCTION_NOT_BRANCH; //Not a branch
      return NULL;
   }
   if (out == in) {
      if ((condop != NULL) && (cond != NULL)) {
         //In this case, the opposite of a branch can't be represented by a simple branch and a condition must be used.
         *cond = cond_new(ed->patchfile, cond_type_fromcode(condtype), condop,
               condval, NULL, NULL);
         oprnd_free(condop); //The operand has been duplicated inside the new condition
         ed->last_error_code = WRN_LIBASM_BRANCH_OPPOSITE_COND;
         return NULL;
      } else {
         //Either this branch has no opposite or it would need a condition but no pointer was passed for it
         ed->last_error_code = WRN_LIBASM_BRANCH_HAS_NO_OPPOSITE; //Branch has no opposite
         return NULL;
      }
   }

   return out;
}

/*
 * Retrieves the list of dynamic libraries
 * \param ed A pointer to the structure holding the disassembled file
 * \return a list of strings with dynamic library names. Do not free these strings
 */
list_t* madras_get_dynamic_libraries(elfdis_t* ed)
{
   if (ed == NULL)
      return (NULL);
//    return (elffile_get_dynamic_libs ((elffile_t*) asmfile_getorigin(ed->afile)));
   binfile_t* bf = asmfile_get_binfile(ed->afile);
   list_t* out = NULL;
   uint16_t i;
   uint16_t nlibs = binfile_get_nb_ext_libs(bf);
   if (nlibs > 0) {
      out = list_new(binfile_get_ext_lib_name(bf, 0));
      for (i = 1; i < nlibs; i++) {
         list_add_after(out, binfile_get_ext_lib_name(bf, i));
      }
   }
   return out;
}

/*
 * Retrieves the list of dynamic libraries from a previously not parsed file
 * \param filepath Path to the file
 * \return A queue of strings (allocated with lc_malloc) of the dynamic library names,
 * or NULL if filepath is NULL or the file could not be found.
 * */
queue_t* madras_get_file_dynamic_libraries(char* filepath)
{
   /**\todo (2018-02-23) This function may be moved elsewhere as it uses no structure from madras, unless we change this API
    * to centralise all madras functions*/
   queue_t* out = NULL;
   binfile_t* bf = binfile_parse_new(filepath, binfile_load);
   if (bf == NULL)
      return NULL;
   out = queue_new();
   uint16_t i;
   uint16_t nlibs = binfile_get_nb_ext_libs(bf);
   for (i = 0; i < nlibs; i++) {
      queue_add_tail(out, lc_strdup(binfile_get_ext_lib_name(bf, i)));
   }
   binfile_free(bf);
   return out;
}

/*
 * Tests whether a file is a valid ELF file
 * \param filename The path of the file to check
 * \param archcode If not NULL, the value pointed to by it will be set to the code
 * corresponding to the architecture of the file (as defined in \b elf.h)
 * \note The code for x86_64 is 62. The code for i386 is 3. The code for i64 is 50.
 * \param filecode If not NULL, the value pointed to by it will be set to the code
 * corresponding to the type of the file (as defined in \b elf.h)
 * \note The code for an executable file is 2. The code for an object file is 1.
 * The code for a shared library is 3. The code for a core file is 4.
 * \return TRUE if the file is a valid ELF file, FALSE otherwise or if an error occurred
 * */
boolean_t madras_is_file_valid(char* filename, int* archcode, int* typecode)
{
   /**\todo TODO (2014-11-18) Update the documentation of this function to reflect the change in values returned.
    * Better yet, change where it's invoked to use something else instead or whatever.*/
   if (filename == NULL) {
      //error
      return (0);
   }

   /**\todo Update this function to return types defined in libmasm and the architecture name instead of relying on the libelf*/
   /**\todo Update this function to reply 0 if the file is of the wrong architecture*/
   int out = 0;
   asmfile_t* asmf = asmfile_new(filename);
//   asmfile_add_parameter (asmf, PARAM_MODULE_DWARF, PARAM_MODULE_DWARF, TRUE);
   asmfile_add_parameter(asmf, PARAM_MODULE_DISASS, PARAM_DISASS_OPTIONS,
         (void*) DISASS_OPTIONS_PARSEONLY);
//   elffile_t* elf = parse_elf_file (filename, NULL, NULL, NULL, asmf);
   int res = asmfile_disassemble(asmf);
   if (!ISERROR(res)) {
//      if(ELF_get_kind (elf->filedes)== ELFkind_ELF) {
         out = 1;
         if ((archcode) || (typecode)) {   //Also retrieves the architecture
            if (archcode)
            *archcode = arch_get_code(binfile_get_arch(asmfile_get_binfile(asmf))); //Elf_Ehdr_get_e_machine (elf, NULL);
            if (typecode)
            *typecode = binfile_get_type(asmfile_get_binfile(asmf)); //Elf_Ehdr_get_e_type (elf, NULL);
      }
//      }
//      elffile_free(elf);
   }
   asmfile_free(asmf);
   return out;
}
#if 0
/*
 * Checks if the file has dedug data
 * \param ed A pointer to the structure holding the disassembled file
 * \return TRUE if the file has debug data, FALSE otherwise
 */
boolean_t madras_check_dbgdata(elfdis_t* ed)
{
   if (ed == NULL || ed->afile == NULL || asmfile_get_origin(ed->afile) == NULL)
      return FALSE;
   if (((elffile_t*) asmfile_get_origin(ed->afile))->dwarf == NULL)
      return FALSE;
   else
      return TRUE;
}
#endif
/*
 * Checks if a label is of type function.
 * \param ed A pointer to the structure holding the disassembled file
 * \param label Name of the label
 * \return 1 if the label is of type function in the ELF file, 0 otherwise. If the label was
 * not found or if no symbol table is present in the file, returns -1.
 * \note This function relies on the libelf, which only checks if the label has ELF type \e STT_FUNC. This does not guarantee
 * that the associated label marks the beginning of a function in the binary
 * */
int madras_label_isfunc(elfdis_t* ed, char* label)
{
   if (ed == NULL)
      return -1;
   /**\todo TODO (2015-05-05) This function is not used anywhere except in test files. I'm updating it
    * to take the new binfile into account, but in the future, it should simply be removed, or simplified
    * and moved to the libbin*/
   if (!ed)
      return FALSE;
   label_t* lbl = asmfile_lookup_label(ed->afile, label);
   int lbltype = label_get_type(lbl);
   return
         ((lbltype == LBL_FUNCTION) || (lbltype == LBL_EXTFUNCTION)) ?
               TRUE : FALSE;
   //return elffile_label_isfunc(asmfile_getorigin(ed->afile),label);
}

/*
 * Retrieves the line in the source file corresponding to the instruction
 * pointed to by the cursor
 * \param ed A pointer to the structure holding the disassembled file
 * \param srcfilename If not NULL, will contain a pointer to a char array
 * containing the name of the source file the line belongs to
 * \param srcline If not NULL, contains:
 * - The line in the source file corresponding to the instruction
 * - 0 if no line is associated to this instruction
 * - -1 if the file contains no debug information
 * \param srcol If not NULL, contain the column corresponding to the instruction.
 * \warning In the current version, column index is not retrieved (always 0).
 * \return EXIT_SUCCESS if the operation succeeded, error code otherwise (no defined cursor or no
 * debug information present in the file).
 * */
int madras_get_insn_srcline(elfdis_t* ed, char** srcfilename, int64_t* srcline,
      int64_t* srccol)
{
   if (ed == NULL) {
      return ERR_MADRAS_MISSING_MADRAS_STRUCTURE;
   }

   int out = EXIT_SUCCESS;

   if (srccol)
      *srccol = 0; //Column is not retrieved in the current version
   if (ed->cursor != NULL) {
      insn_t* in = GET_DATA_T(insn_t*, ed->cursor);
      if (in == NULL || in->debug == NULL) {
         out = WRN_LIBASM_NO_DEBUG_DATA;
         if (srcline)
            *srcline = 0;
         if (srcfilename)
            *srcfilename = NULL;
      } else {
         if (srcline)
            *srcline = insn_get_src_line(in);
         if (srcfilename)
            *srcfilename = insn_get_src_file(in);
      }
   }

   return out;
}

/**
 * Sets the instruction cursor at the position of \e label or, if NULL, at \e addr, or,
 * if \e addr is -1, at the beginning of the section with name \e scnname
 * If \e scnname is NULL, sets instruction cursor at the beginning of the first section
 * in the disassembled file
 * We use this function for coherence with the traces
 * \param ed Madras structure
 * \param label Label where to position the cursor
 * \param addr Address at which to position the cursor (if label is NULL)
 * \param scnname Section name at the beginning of which the cursor must be positioned (if addr is < 0)
 * \return EXIT_SUCCESS if the positioning was successful, error code if it failed
 * (label not found, address not found or section name not found)
 * */
static int cursor_init(elfdis_t* ed, char* label, int64_t addr, char* scnname)
{
   if (ed == NULL) {
      return ERR_MADRAS_MISSING_MADRAS_STRUCTURE;
   }
   /**\todo TODO (2014-11-18) Completely rewrite this function, especially as far as sections are concerned*/
   int out = EXIT_SUCCESS, i = 0;
   /**\todo TODO (2015-06-03) Decide how to handle the cursor. On the one hand, we should not need it any more.
    * On the other hand, if we are patching a file that is being analysed, we could also directly accept the nodes
    * containing the instructions to patch as parameters for all madras functions, which would allow to avoid
    * having to look them up twice. But I would like to avoid adding too many parameters to the API functions,
    * so this is where madras_init_cursor would come in handy.*/
   insn_t* cursins;
//   elffile_t* efile = asmfile_getorigin(ed->afile);
   binfile_t* bf = asmfile_get_binfile(ed->afile);

   if (label) {
      //Positioning the cursor at the given label
      //Looks for an instruction linked to this label
      cursins = asmfile_get_insn_by_label(ed->afile, label);
      if (cursins) {
         //An instruction was found
         //Looking for the instruction in the lists
         if (cursins->sequence != NULL) {
            ed->cursor = cursins->sequence;
         } else {
            out = ERR_LIBASM_INSTRUCTION_NOT_FOUND; //Instruction not found in instructions list (unlikely)
            ERRMSG(
                  "Instruction linked to label %s not found in instruction list\n",
                  label);
         }
      } else {
         out = ERR_LIBASM_INSTRUCTION_NOT_FOUND; //No instruction found for this label
         ERRMSG("No instruction found linked to label %s\n", label);
      }
   } else if (addr >= 0) {
      //Positioning the cursor at the given address
      ed->cursor = insn_get_sequence(asmfile_get_insn_by_addr(ed->afile, addr)); //elfdis_findinsnataddress(ed,addr,NULL);
      if (!ed->cursor) {
         out = ERR_LIBASM_INSTRUCTION_NOT_FOUND; //No instruction found at given address
         ERRMSG("No instruction found at address %#"PRIx64"\n", addr);
      }
   } else if (scnname) {
      //Positioning the cursor at the beginning of the specified section
      i = 0;
      while ((i < binfile_get_nb_sections(bf))
            && (strcmp(binfile_get_scn_name(bf, i), scnname)))
         i++;
      if (i < binfile_get_nb_sections(bf)) {
         //Section found
         ed->cursor = binscn_get_first_insn_seq(binfile_get_scn(bf, i)); //elfdis_findinsnataddress(ed,elffile_getscnstartaddr(efile,i),NULL);
         if (ed->cursor == NULL) {
            out = ERR_BINARY_SECTION_EMPTY;
            ERRMSG("Section %s is empty\n", scnname);
         }
      } else {
         //Section not found
         out = ERR_BINARY_SECTION_NOT_FOUND; //Section not found
         ERRMSG("Section %s does not exist or contain executable code\n",
               scnname);
      }
   } else {
      //No position given: position the cursor as the beginning of the disassembled file*/
      if (asmfile_get_insns(ed->afile) != NULL)
         ed->cursor = queue_iterator(asmfile_get_insns(ed->afile));
      else {
         out = ERR_BINARY_NO_SECTIONS_FOUND; //File has no section. Might indicate a problem
         ERRMSG("File %s has no section\n", asmfile_get_name(ed->afile));
      }
   }
   return out;
}

/*
 * Positions the instruction cursor at the location of the given instruction object
 * \warning This function is here to allow a bridge between applications using the high-level API (libmadras.h)
 * and applications which used MADRAS low-level API (madrasapi.h) to disassemble a file. It should not be used on files
 * disassembled with madras_disass_file.
 * \param ed A pointer to the structure holding the disassembled file
 * \param ins A pointer to a structure holding an instruction
 * \return EXIT_SUCCESS if the positioning was successful, error code otherwise
 * */
int madras_align_cursor(elfdis_t* ed, insn_t* ins)
{
   int out = EXIT_SUCCESS;
   if (ins) {
      out = cursor_init(ed, NULL, ins->address, NULL);
      if (ISERROR(out))
         return out;
      if (ed->cursor->data != ins) {
         out = ERR_MADRAS_CURSOR_NOT_ALIGNED;
      }
   } else {
      out = ERR_LIBASM_INSTRUCTION_MISSING;
   }
   return out;
}

/*
 * Positions the instruction cursor at the given location
 * \param ed A pointer to the structure holding the disassembled file
 * \param label Label at which the cursor must be set.
 * \param addr Address at which the cursor must be set, if \e label is NULL
 * \param scnname Name of the ELF section at the beginning of which the cursor must be set,
 *            if \e label is NULL and \e addr is -1. If also NULL, the cursor is set at the
 *            beginning of the first section in the disassembled file
 * \return EXIT_SUCCESS if the positioning was successful, error code if it failed
 * (label not found, address not found or section name not found)
 * */
int madras_init_cursor(elfdis_t* ed, char* label, int64_t addr, char* scnname)
{
   TRACE(ed, "madras_init_cursor(label=%s,addr=%#"PRIx64",scnname=%s)\n", label,
         addr, scnname);
   return cursor_init(ed, label, addr, scnname);
}

/*
 * Checks if the cursor instruction is at the end of the current ELF section
 * \param ed A pointer to the structure holding the disassembled file
 * \return TRUE if the cursor instruction is at the end of the current ELF section,
 * FALSE otherwise. If an error occurred, the last error code will be filled in \c ed
 * */
boolean_t madras_insn_endofscn(elfdis_t* ed)
{
   /**\todo TODO (2014-11-19) Remove this function. It's only used in the tests of the patcher anyway*/
   if (ed == NULL) {
      //error
      return FALSE;
   }

   if (ed->cursor != NULL) {
      binscn_t* scn = label_get_scn(
            insn_get_fctlbl(GET_DATA_T(insn_t*, ed->cursor)));
      if (binscn_get_last_insn_seq(scn) == ed->cursor)
            return TRUE; //Next instruction is at the address of a section: cursor is then at the end of a section
      else
            return FALSE; //We are not at the end of a section
//      if (ed->cursor->next != NULL) {
//         if (elffile_get_scnataddr(asmfile_getorigin(ed->afile),insn_get_addr(GET_DATA_T(insn_t*,ed->cursor->next))) > 0 ) {
//            return 1;//Next instruction is at the address of a section: cursor is then at the end of a section
//         } else {
//            return 0;//We are not at the end of a section
//         }
//      } else
         return TRUE; //Cursor at the end of file: we are indeed at the end of a section (the last one)
   } else {
      ed->last_error_code = ERR_MADRAS_MISSING_CURSOR;
      return FALSE;
//         return 1;//Cursor at the end of file: we are indeed at the end of a section (the last one)
   }
}

/*
 * Steps to the next instruction.
 * If the instruction is at the end of its section, steps to the beginning
 * of the next disassembled section
 * \param ed A pointer to the structure holding the disassembled file
 * \return EXIT_FAILURE if the current instruction is the last of the disassembled file,
 * an error code if an error occurred, EXIT_SUCCESS otherwise
 * */
int madras_insn_next(elfdis_t* ed)
{
   int out = EXIT_SUCCESS;
   if (ed == NULL) {
      return ERR_MADRAS_MISSING_MADRAS_STRUCTURE;
   }

   TRACE(ed, "madras_insn_next(%s)\n", "");
   if (!ed->cursor) {
      ERRMSG("No cursor defined. Stepping failed\n");
      return ERR_MADRAS_MISSING_CURSOR; //No cursor defined
   }
   if (ed->cursor->next != NULL)
      ed->cursor = ed->cursor->next;
   else
      out = EXIT_FAILURE;
   return out;
}

/*
 * Steps to the previous instruction.
 * If the instruction is at the beginning of its section, steps to the end
 * of the previous disassembled section
 * \param ed A pointer to the structure holding the disassembled file
 * \return EXIT_FAILURE if the current instruction is the first of the disassembled file,
 * an error code if an error occurred, EXIT_SUCCESS otherwise
 * */
int madras_insn_prev(elfdis_t* ed)
{
   int out = EXIT_SUCCESS;
   if (ed == NULL) {
      return ERR_MADRAS_MISSING_MADRAS_STRUCTURE;
   }

   TRACE(ed, "madras_insn_prev(%s)\n", "");
   if (!ed->cursor) {
      ERRMSG("No cursor defined. Stepping failed\n");
      return ERR_MADRAS_MISSING_CURSOR; //No cursor defined
   }
   if (ed->cursor->prev != NULL)
      ed->cursor = ed->cursor->prev;
   else
      out = EXIT_FAILURE;
   return out;
}

/*
 * Return the hexadecimal coding of the instruction in a string format
 * \param ed A pointer to the structure holding the disassembled file
 * \return A string containing the representation of the coding, or NULL if the instruction has no coding.
 * \note This string has been allocated using malloc
 * */
char* madras_get_insn_hexcoding(elfdis_t* ed)
{
   if (ed == NULL) {
      //error
      return (NULL);
   }
   char* out = NULL;
   bitvector_t* icod = insn_get_coding((insn_t*) ed->cursor->data);

   if (icod) {
      int buffer_size = sizeof(char)
            * (((3 * BITVECTOR_GET_BITLENGTH(icod)) >> 3) + 2);
      out = (char*) lc_malloc(buffer_size);
      bitvector_hexprint(icod, out, buffer_size, " ");
   }
   return out;
}

/*
 * Retrieves the name of the instruction pointed to by the cursor
 * \param ed A pointer to the structure holding the disassembled file
 * \return The name (mnemonic) of the instruction the cursor is at, or NULL if an error occurred
 * (in this case, the last error code will be updated in \c ed if it is not NULL)
 * */
char* madras_get_insn_name(elfdis_t* ed)
{
   if (ed == NULL) {
      return NULL;
   }

   char* out = NULL;
   if ((ed->cursor) && (ed->cursor->data)) {
      out = insn_get_opcode((insn_t*) ed->cursor->data);
   } else {
      out = NULL;
      ed->last_error_code = ERR_MADRAS_MISSING_CURSOR;
   }
   return out;
}

/*
 * Retrieves the bit size of the instruction pointed to by the cursor
 * \param ed A pointer to the structure holding the disassembled file
 * \return Size in bits of the instruction, or -1 if an error occurred
 * (in this case, the last error code will be updated in \c ed if it is not NULL)
 * */
int madras_get_insn_size(elfdis_t* ed)
{
   if (ed == NULL) {
      //error
      return (-1);
   }

   int out = 0;
   if ((ed->cursor) && (ed->cursor->data)) {
      out = insn_get_size((insn_t*) ed->cursor->data);
   } else {
      out = -1;
      ed->last_error_code = ERR_MADRAS_MISSING_CURSOR;
   }
   return out;
}

/*
 * Retrieves the address of the instruction pointed to by the cursor
 * \param ed A pointer to the structure holding the disassembled file
 * \return The virtual address of the instruction, or ADDRESS_ERROR if an error occurred
 * (in this case, the last error code will be updated in \c ed if it is not NULL)
 * */
int64_t madras_get_insn_addr(elfdis_t* ed)
{
   if (ed == NULL) {
      //error
      return (-1);
   }

   int64_t out = 0;
   if ((ed->cursor) && (ed->cursor->data)) {
      out = INSN_GET_ADDR((insn_t* )ed->cursor->data);
   } else {
      out = ADDRESS_ERROR;
      ed->last_error_code = ERR_MADRAS_MISSING_CURSOR;
   }
   return out;
}

/*
 * Retrieves the ELF label (function name or label) associated to the instruction pointed to by the cursor
 * \param ed A pointer to the structure holding the disassembled file
 * \return The name of the label. If no label is associated with this instruction, returns NULL.
 * If an error occurred, the last error code in \c ed will be updated
 * */
char* madras_get_insn_lbl(elfdis_t* ed)
{
   if (ed == NULL) {
      return NULL;
   }

   char* out = NULL;
   if ((ed->cursor) && (ed->cursor->data)) {
      label_t* lbl = insn_get_fctlbl(GET_DATA_T(insn_t*, ed->cursor));
      if (label_get_target(lbl) == ed->cursor->data)
         out = label_get_name(lbl);
   } else {
      out = NULL;
      ed->last_error_code = ERR_MADRAS_MISSING_CURSOR;
   }
   return out;
}

/*
 * Retrieves the type of a parameter of the instruction pointed to by the cursor
 * \param ed A pointer to the structure holding the disassembled file
 * \param pos The index of the parameter as it appears in an assembly instruction written in
 *        AT&T language (source first), starting at 0
 * \return
 * - Value \c OT_REGISTER for registers
 * - Value \c OT_IMMEDIATE for immediate
 * - Value \c OT_POINTER for pointers
 * - Value \c OT_MEMORY for memory
 * - A null value if \e pos is an incorrect parameter index or
 * an error occurred. In this case, the last error code in \c ed will be filled
 * */
int madras_get_insn_paramtype(elfdis_t* ed, int pos)
{
   int out = 0;
   if (!ed)
      return 0;
   if (ed->cursor && ed->cursor->data) {
      oprnd_t* oprnd = insn_get_oprnd((insn_t*) ed->cursor->data, pos);
      if (oprnd) {
         switch (oprnd_get_type(oprnd)) {
         case OT_REGISTER:
            out = OT_REGISTER;
            break;
         case OT_MEMORY:
            out = OT_MEMORY;
            break;
         case OT_IMMEDIATE:
            out = OT_IMMEDIATE;
            break;
         case OT_POINTER:
            out = OT_POINTER;
            break;
         default:
            out = 0;
         }
      } else {
         ed->last_error_code = ERR_LIBASM_OPERAND_NOT_FOUND;
      }
   } else {
      ed->last_error_code = ERR_MADRAS_MISSING_CURSOR;
   }
   return out;
}

/*
 * Retrieves the number of parameters of the instruction pointed to by the cursor
 * \param ed A pointer to the structure holding the disassembled file
 * \return The number of operands of the instruction or 0 if an error occurred.
 * In this case the last error code in \c ed will be filled
 * */
int madras_get_insn_paramnum(elfdis_t* ed)
{
   int out = 0;
   if (!ed)
      return 0;

   if (ed->cursor && ed->cursor->data) {
      out = insn_get_nb_oprnds((insn_t*) ed->cursor->data);
   } else {
      ed->last_error_code = ERR_MADRAS_MISSING_CURSOR;
   }
   return out;
}

/*
 * Retrieves a given operand from the instruction pointed to by the cursor
 * \param ed A pointer to the structure holding the disassembled file
 * \param pos The index of the operand (starting at 0 for the first operand)
 * \return The operand in string form in the AT&T assembly format, or NULL
 * if an error occurred. In this case the last error code in \c ed will be filled
 * */
char* madras_get_insn_paramstr(elfdis_t* ed, int pos)
{
   char *out = NULL;
   if (!ed)
      return NULL;

   if (ed->cursor && ed->cursor->data) {
      oprnd_t* oprnd = insn_get_oprnd((insn_t*) ed->cursor->data, pos);
      if (oprnd) {
         //The longest parameter should not exceed 40 characters in length
         int buffer_size = sizeof(char) * 40;
         out = (char*) lc_malloc(buffer_size);
         oprnd_print((insn_t*) ed->cursor->data, oprnd, out, buffer_size,
               asmfile_get_arch(ed->afile));
      } else {
         ed->last_error_code = ERR_LIBASM_OPERAND_NOT_FOUND;
      }
   } else {
      ed->last_error_code = ERR_MADRAS_MISSING_CURSOR;
   }
   return out;
}

/*
 * Return a register name used in a parameter (of OT_REGISTER type)
 * \param ed A pointer to the structure holding the disassembled file
 * \param pos The index of the operand (starting at 0 for the first operand)
 * \return A string with the register name, or NULL if an error occurred.
 * In this case the last error code in \c ed will be filled
 * */
char* madras_get_register_name(elfdis_t* ed, int pos)
{
   char* out = NULL;
   if (!ed)
      return NULL;

   if (ed->cursor && ed->cursor->data) {
      oprnd_t* oprnd = insn_get_oprnd((insn_t*) ed->cursor->data, pos);
      if (oprnd) {
         if (oprnd_is_reg(oprnd) == TRUE)
            out = arch_get_reg_name(asmfile_get_arch(ed->afile),
                  reg_get_type(oprnd_get_reg(oprnd)),
                  reg_get_name(oprnd_get_reg(oprnd)));
         else
            ed->last_error_code = ERR_LIBASM_OPERAND_NOT_REGISTER;
      } else
         ed->last_error_code = ERR_LIBASM_OPERAND_NOT_FOUND;
   } else
      ed->last_error_code = ERR_MADRAS_MISSING_CURSOR;

   return out;
}

/*
 * Return a register name used as base in a parameter (of OT_MEMORY type)
 * \param ed A pointer to the structure holding the disassembled file
 * \param pos The index of the operand (starting at 0 for the first operand)
 * \return A string with the register name, or NULL if an error occurred.
 * In this case the last error code in \c ed will be filled
 * */
char* madras_get_base_name(elfdis_t* ed, int pos)
{
   char* out = NULL;
   if (!ed)
      return NULL;

   if (ed->cursor && ed->cursor->data) {
      oprnd_t* oprnd = insn_get_oprnd((insn_t*) ed->cursor->data, pos);
      if (oprnd) {
         if (oprnd_is_mem(oprnd) == TRUE)
            out = arch_get_reg_name(asmfile_get_arch(ed->afile),
                  reg_get_type(oprnd_get_base(oprnd)),
                  reg_get_name(oprnd_get_base(oprnd)));
         else
            ed->last_error_code = ERR_LIBASM_OPERAND_NOT_MEMORY;
      } else
         ed->last_error_code = ERR_LIBASM_OPERAND_NOT_FOUND;
   } else
      ed->last_error_code = ERR_MADRAS_MISSING_CURSOR;

   return out;
}

/*
 * Return a register name used as index in a parameter (of OT_MEMORY type)
 * \param ed A pointer to the structure holding the disassembled file
 * \param pos The index of the operand (starting at 0 for the first operand)
 * \return A string with the register name, or NULL if an error occurred.
 * In this case the last error code in \c ed will be filled
 * */
char* madras_get_index_name(elfdis_t* ed, int pos)
{
   char* out = NULL;
   if (!ed)
      return NULL;

   if (ed->cursor && ed->cursor->data) {
      oprnd_t* oprnd = insn_get_oprnd((insn_t*) ed->cursor->data, pos);
      if (oprnd) {
         if (oprnd_is_mem(oprnd) == TRUE)
            out = arch_get_reg_name(asmfile_get_arch(ed->afile),
                  reg_get_type(oprnd_get_index(oprnd)),
                  reg_get_name(oprnd_get_index(oprnd)));
         else
            ed->last_error_code = ERR_LIBASM_OPERAND_NOT_MEMORY;
      } else
         ed->last_error_code = ERR_LIBASM_OPERAND_NOT_FOUND;
   } else
      ed->last_error_code = ERR_MADRAS_MISSING_CURSOR;

   return out;
}

/*
 * Return an offset used in a parameter (of OT_MEMORY type)
 * \param ed A pointer to the structure holding the disassembled file
 * \param pos The index of the operand (starting at 0 for the first operand)
 * \return A 64b integer with the offset, 0 if there is no offset or if an error occured
 * In this case the last error code in \c ed will be filled
 * */
int64_t madras_get_offset_value(elfdis_t* ed, int pos)
{
   int64_t out = 0;
   if (!ed)
      return 0;

   if (ed->cursor && ed->cursor->data) {
      oprnd_t* oprnd = insn_get_oprnd((insn_t*) ed->cursor->data, pos);
      if (oprnd) {
         if (oprnd_is_mem(oprnd) == TRUE)
            out = oprnd_get_offset(oprnd);
         else
            ed->last_error_code = ERR_LIBASM_OPERAND_NOT_MEMORY;
      } else
         ed->last_error_code = ERR_LIBASM_OPERAND_NOT_FOUND;
   } else
      ed->last_error_code = ERR_MADRAS_MISSING_CURSOR;

   return out;
}

/*
 * Return a integer used as scale in a parameter (of OT_MEMORY type)
 * \param ed A pointer to the structure holding the disassembled file
 * \param pos The index of the operand (starting at 0 for the first operand)
 * \return An integer with the scale, 0 if there is no scale or a negative value if an error occured
 * In this case the last error code in \c ed will be filled
 * */
int madras_get_scale_value(elfdis_t* ed, int pos)
{
   int out = 0;
   if (!ed)
      return 0;

   if (ed->cursor && ed->cursor->data) {
      oprnd_t* oprnd = insn_get_oprnd((insn_t*) ed->cursor->data, pos);
      if (oprnd) {
         if (oprnd_is_mem(oprnd) == TRUE)
            out = oprnd_get_scale(oprnd);
         else
            ed->last_error_code = ERR_LIBASM_OPERAND_NOT_MEMORY;
      } else
         ed->last_error_code = ERR_LIBASM_OPERAND_NOT_FOUND;
   } else
      ed->last_error_code = ERR_MADRAS_MISSING_CURSOR;

   return out;
}

/*
 * Return a constant value in a parameter (of OT_CONSTANT type)
 * \param ed A pointer to the structure holding the disassembled file
 * \param pos The index of the operand (starting at 0 for the first operand)
 * \return A 64b integer with the value, or 0 value if an error occured
 * In this case the last error code in \c ed will be filled
 * */
int64_t madras_get_constant_value(elfdis_t* ed, int pos)
{
   int64_t out = 0;
   if (!ed)
      return 0;

   if (ed->cursor && ed->cursor->data) {
      oprnd_t* oprnd = insn_get_oprnd((insn_t*) ed->cursor->data, pos);
      if (oprnd) {
         if (oprnd_is_imm(oprnd) == TRUE)
            out = oprnd_get_imm(oprnd);
         else if (oprnd_is_ptr(oprnd))
            out = oprnd_get_refptr_addr(oprnd);
         else
            ed->last_error_code = ERR_LIBASM_OPERAND_NOT_IMMEDIATE;
      } else
         ed->last_error_code = ERR_LIBASM_OPERAND_NOT_FOUND;
   } else
      ed->last_error_code = ERR_MADRAS_MISSING_CURSOR;

   return out;
}

/*
 * Prints the current instruction in a format similar to objdump
 * \param ed A pointer to the structure holding the disassembled file
 * */
void madras_insn_print(elfdis_t* ed)
{
   /**\todo TODO (2014-11-25) Remove this function, it is redundant and only used in one of the test files anyway*/
   char buffer[255];

   if ((ed) && (ed->cursor) && (ed->cursor->data)) {
      insn_print(ed->cursor->data, buffer, sizeof(buffer));
      printf("%s", buffer);
   }
}

#define coding_max_size "30"  /**< used to align strings during printing. This one contains the size of the coding field.
                                Spaces are added to fill it.*/

/**
 * Prints and instruction in a format similar to objdump into an open file
 * \param ed A pointer to the structure holding the disassembled file
 * \param ins The instruction to print
 * \param stream The stream to print to
 * \param printaddr Prints the address before an instruction
 * \param printcoding Prints the coding before an instruction
 * \param printlbl Prints the labels (if present)
 * \param before Function to execute before printing an instruction (for printing additional informations)
 * \param after Function to execute after printing an instruction (for printing additional informations)
 * */
static void madras_insn_fprint(elfdis_t* ed, insn_t* ins, FILE* stream,
      int printlbl, int printaddr, int printcoding,
      void (*before)(elfdis_t*, insn_t*, FILE*),
      void (*after)(elfdis_t*, insn_t*, FILE*))
{
   list_t* label_container = NULL;
   //Printing the label
   if (printlbl == TRUE) {
      label_t* label = insn_get_fctlbl(ins);
      /**\todo TODO (2014-11-06) Use the extlabel_print from binfile to print t, OR find something completely different*/
      DBGMSGLVL(3, "%#"PRIx64": Insn %p label %s (%p) points to %p\n",
            insn_get_addr(ins), ins, label_get_name(label), label,
            label_get_target(label));
      //Prints label name if the label the instruction belongs to is the target of the label
      if (label_get_target(label) == ins)
         fprintf(stream, "%"PRIx64" <%s>:\n", insn_get_addr(ins),
               label_get_name(label));

   }

   if (before != NULL)
      before(ed, ins, stream);

   //Printing the instruction's address
   if (printaddr == TRUE) {
      fprintf(stream, " %"PRIx64":\t", insn_get_addr(ins));
   }

   //Printing the instruction's coding
   if (printcoding == TRUE) {
      char coding[128];
      bitvector_hexprint(insn_get_coding(ins), coding, sizeof(coding), " ");

      fprintf(stream, "%-"coding_max_size"s ", coding);
   }

   //Prints instruction
   insn_fprint(ins, stream);
   //Prints RIP destination if an operand uses RIP register
//        int isinsn = -1;
//        int64_t rip_dest = insn_checkrefs (ins, &isinsn);
//        if (isinsn == 0 && rip_dest >= 0)
//            fprintf(stream, "\t   # 0x%"PRIx64"", rip_dest);
   oprnd_t* refop = insn_lookup_ref_oprnd(ins);
   if (oprnd_get_type(refop) == OT_MEMORY_RELATIVE) {
      pointer_t* ptr = oprnd_get_memrel_pointer(refop);
      data_t* target = pointer_get_data_target(ptr);
      if (target) {
         char* targetlbl;
         int64_t off;
         int64_t targetaddr = data_get_addr(target)
               + pointer_get_offset_in_target(ptr);
         label_t* datalbl = data_get_label(target);
         targetlbl = label_get_name(datalbl);
         //Computes the name of the label associated to the target and the offset to the label
         if (targetlbl && strlen(targetlbl)) {
            off = targetaddr - label_get_addr(datalbl);
         } else {
            //No label: using the section name
            binscn_t* datascn = data_get_section(target);
            targetlbl = binscn_get_name(datascn);
            off = targetaddr - binscn_get_addr(datascn);
         }
         //Printing the target and its associated label
         if (off != 0)
            fprintf(stream, "\t   # 0x%"PRIx64" <%s+%#"PRIx64">", targetaddr,
                  targetlbl, off);
         else
            fprintf(stream, "\t   # 0x%"PRIx64" <%s>", targetaddr, targetlbl);
      } else {
         //Memory relative operand whose target was not found: simply printing the destination
         fprintf(stream, "\t   # 0x%"PRIx64"", oprnd_get_refptr_addr(refop));
      }
   }

   if (after != NULL)
      after(ed, ins, stream);
   fprintf(stream, "\n");
}


/*
 * Prints a list of instructions
 * \param ed A pointer to the structure holding the disassembled file
 * \param stream The stream to print to
 * \param startaddr The address at which printing must begin. If 0 or negative, the first address in the instruction list will be taken
 * \param stopaddr The address at which printing must end. If 0 or negative, the last address in the instruction list will be taken
 * \param printaddr Prints the address before an instruction
 * \param printcoding Prints the coding before an instruction
 * \param printlbl Prints the labels (if present)
 * \param before Function to execute before printing an instruction (for printing additional informations)
 * \param after Function to execute after printing an instruction (for printing additional informations)
 * */
extern void madras_insns_print(elfdis_t* ed, FILE* stream, int64_t startaddr,
      int64_t stopaddr, int printlbl, int printaddr, int printcoding,
      void (*before)(elfdis_t*, insn_t*, FILE*),
      void (*after)(elfdis_t*, insn_t*, FILE*))
{
   //Exits if the file is NULL, or its instruction list is NULL or empty
   if (ed == NULL)
      return;
   queue_t* insns = asmfile_get_insns(ed->afile);
   if (queue_length(insns) == 0)
      return;

   list_t* iter;
   insn_t* ins;
   int64_t start, stop;
   binfile_t* bf = asmfile_get_binfile(ed->afile);

   if (startaddr <= 0)
      start = insn_get_addr((insn_t* )queue_peek_head(insns));
   else
      start = startaddr;

   if (stopaddr <= 0)
      stop = insn_get_addr((insn_t* )queue_peek_tail(insns));
   else
      stop = stopaddr;

   // Handling the case of files not associated to a parsed binary file
   if ((bf == NULL) || (binfile_get_nb_code_scns(bf) == 0)) {
      //No binary file associated to this file or no code section: we simply scan the list of instructions
      iter = queue_iterator(asmfile_get_insns(ed->afile));
      //Skipping instructions until we reach the first address to print
      while ((iter != NULL) && (insn_get_addr(GET_DATA_T(insn_t*,iter)) < start))
         iter = iter->next;
      //Now printing instructions
      while ((iter != NULL) && (insn_get_addr(GET_DATA_T(insn_t*,iter)) <= stop)) {
         insn_t* insn = GET_DATA_T(insn_t*, iter);
         madras_insn_fprint(ed, insn, stream, printlbl, printaddr, printcoding,
               before, after);
         iter = iter->next;
      }
      return;
   }

   //Now to the general case
   uint16_t i;
   int printscn = FALSE;

   //Scanning all sections containing code and printing their content
   for (i = 0; i < binfile_get_nb_code_scns(bf); i++) {
      binscn_t* scn = binfile_get_code_scn(bf, i);
      list_t* iter = binscn_get_first_insn_seq(scn);
      list_t* lastiter = binscn_get_last_insn_seq(scn);
      /**\todo TODO (2014-12-08) Handle here the A_BEGINLIST and A_ENDLIST flags*/
      //Checking if this section contains the first address to print
      if ((insn_get_addr(GET_DATA_T(insn_t*, iter)) <= start)
            && (insn_get_addr(GET_DATA_T(insn_t*, lastiter)) >= start))
         printscn = TRUE;

      //Printing the content of the section if we are in the range of addresses being printed
      if (printscn) {
         fprintf(stream, "\nDisassembly of section %s:\n", binscn_get_name(scn));
         //Skipping instructions until we reach the first address to print
         if ((insn_get_addr(GET_DATA_T(insn_t*, iter)) < start)) {
            fprintf(stream, "...\n");
            while ((iter != NULL)
                  && (insn_get_addr(GET_DATA_T(insn_t*,iter)) < start))
               iter = iter->next;
         }
         //Prints the instructions belonging to the section
         while (iter && (insn_get_addr(GET_DATA_T(insn_t*,iter)) <= stop)) {
            madras_insn_fprint(ed, GET_DATA_T(insn_t*, iter), stream, printlbl,
                  printaddr, printcoding, before, after);
            if (iter == lastiter)
               break;
      iter = iter->next;
   }
         //Printing dots if we reached the last instruction to print before the end of the section
         if (iter && iter != lastiter) {
            fprintf(stream, "...\n");
            break; //Exiting the loop over sections as we passed the last instruction to print
         }
      }
   }

//    //Reaches the beginning of the list to print
//    iter = queue_iterator(ed->afile->insns);
//    while( (iter != NULL) && (insn_get_addr(GET_DATA_T(insn_t*,iter)) < start) ) iter = iter->next;
//
//    //while( ( iter != NULL ) && (insn_get_addr(GET_DATA_T(insn_t*,iter)) <= stop) ) {
//    while(  iter != NULL ) {
//        ins = GET_DATA_T(insn_t*,iter);
//
//       // Check if a section is a the current address
//       int scn_idx = -1;//elffile_get_progscnataddr(asmfile_getorigin(ed->afile), insn_get_addr (ins));
//       if (scn_idx != -1)
//          fprintf(stream, "\nDisassembly of section %s:\n", binfile_get_scn_name (asmfile_get_binfile(ed->afile), scn_idx));
//
//       madras_insn_fprint(ed, stream, ins, printlbl, printaddr, printcoding, before, after);
//
//       iter = iter->next;
//    }
}
/*
 * Prints a list of instructions in shell code
 * \param ed A pointer to the structure holding the disassembled file
 * \param stream The stream to print to
 * \param startaddr The address at which printing must begin. If 0 or negative, the first address in the instruction list will be taken
 * \param stopaddr The address at which printing must end. If 0 or negative, the last address in the instruction list will be taken
 * */
void madras_insns_print_shellcode(elfdis_t* ed, FILE* stream, int64_t startaddr,
      int64_t stopaddr)
{
   /**\todo TODO (2014-11-19) Rewrite this function to use the binfile_t properties (especially when printing the "Disassembly of section", which I disabled*/
   //Exits if the file is NULL, or its instruction list is NULL or empty
   if ((ed == NULL) || (ed->afile == NULL) || (ed->afile->insns == NULL)
         || (queue_length(ed->afile->insns) == 0))
      return;

   list_t* iter;
   insn_t* ins;
   char buffer[1024];
   int64_t start, stop;

   if (startaddr <= 0)
      start = INSN_GET_ADDR((insn_t* )queue_peek_head(ed->afile->insns));
   else
      start = startaddr;

   if (stopaddr <= 0)
      stop = INSN_GET_ADDR((insn_t* )queue_peek_tail(ed->afile->insns));
   else
      stop = stopaddr;

   //Reaches the beginning of the list to print
   iter = queue_iterator(ed->afile->insns);
   while ((iter != NULL) && (INSN_GET_ADDR(GET_DATA_T(insn_t*, iter)) < start))
      iter = iter->next;

   while ((iter != NULL) && (INSN_GET_ADDR(GET_DATA_T(insn_t*, iter)) <= stop)) {
      ins = GET_DATA_T(insn_t*, iter);

      // Check if a section is a the current address
      int scn_idx = -1; //elffile_get_progscnataddr(asmfile_getorigin(ed->afile), insn_get_addr (ins));

      if (scn_idx != -1)
         fprintf(stream, "\nDisassembly of section %s:\n",
               binfile_get_scn_name(asmfile_get_binfile(ed->afile), scn_idx));

      //Printing the label
      label_t* label = insn_get_fctlbl(ins);

      //Prints label name if the label the instruction belongs to is the target of the label
      if (label_get_target(label) == ins)
         fprintf(stream, "\n%"PRIx64" <%s>:\n", INSN_GET_ADDR(ins),
               label_get_name(label));

      //Prints instruction
      bitvector_hexprint(insn_get_coding(ins), buffer, sizeof(buffer), "\\x");
      fprintf(stream, "%s", buffer);
      iter = iter->next;
   }
   fprintf(stream, "\n");
}

/*
 * Get bytes from memory
 * \param ed A pointer to the structure holding the disassembled file
 * \param addr start address in the memory of the part to return
 * \param len size (in number of bytes) of the part to return
 * \return An array of bytes or NULL if there is a problem.
 * In this case, the last error code in \c ed will be filled
 */
unsigned char* madras_getbytes(elfdis_t* ed, int64_t addr, unsigned int len)
{
   if (!ed)
      return NULL;
   int64_t startaddr;
   uint64_t seclen;
//   elffile_t* efile = asmfile_getorigin(ed->afile);
//   int secindx = elffile_get_scnspanaddr(efile, addr);
   unsigned char* sec = NULL;
   unsigned char* sec_ret = NULL;
   unsigned int i = 0;

   binscn_t* scn = binfile_lookup_scn_span_addr(asmfile_get_binfile(ed->afile),
         addr);
   if (!scn) {
      ed->last_error_code = ERR_BINARY_SECTION_NOT_FOUND;
      return NULL;
   }
   sec = binscn_get_data(scn, &seclen);
   startaddr = binscn_get_addr(scn);

   if ((sec == NULL) || (addr + len > startaddr + seclen)) {
      ed->last_error_code = ERR_BINARY_SECTION_EMPTY;
      return NULL;
   }

   sec_ret = lc_malloc((len + 1) * sizeof(unsigned char));

   for (i = 0; i < len; i++)
      sec_ret[i] = sec[addr - startaddr + i];

   sec_ret[len] = '\0';
   return (sec_ret);
}

/*
 * Prepares a disassembled file for modification.
 * \param ed A pointer to the structure holding the disassembled file
 * \param stacksave Specifies the method to use to save the stack before
 * inserting a function call.
 * - \e STACK_KEEP will not move the stack (can cause bugs for codes
 * using \c \%rsp or \c \%rbp as a base address)
 * - \e STACK_MOVE will move the stack to a new section in the ELF file
 * (can cause crashes for multi-threaded codes)
 * - \e STACK_SHIFT will shift the stack before its bottom (can cause crashes
 * depending on the structure of the memory)
 * \param stackshift Value to shift the stack of. Must be specified is
 * \c stacksave is set to \e STACK_SHIFT (a warning will be printed
 * if specified for another value)
 * \return EXIT_SUCCESS if everything is ok, or an error code if an error
 * happened (file already being modified for instance)
 * */
int madras_modifs_init(elfdis_t* ed, char stacksave, int64_t stackshift)
{
   int out = EXIT_SUCCESS;
   TRACE(ed, "madras_modifs_init(stacksave=%d,stackshift=%#"PRIx64")\n",
         stacksave, stackshift);

   if (ed == NULL) {
      return ERR_MADRAS_MISSING_MADRAS_STRUCTURE;
   }

   if (!ed->patchfile) {
      ed->patchfile = patchfile_init(ed->afile);
      if (!ed->patchfile) {
         ERRMSG("Unable to initialise patched file from file %s\n", ed->name);
         return FALSE;
      }
      ed->options = PATCHOPT_NONE;
      if (stacksave == STACK_SHIFT) {
         ed->patchfile->stackshift = stackshift;
         if (!stackshift) {
            WRNMSG("Shift stack method used with shift value of 0\n");
            out = WRN_MADRAS_STACK_SHIFT_NULL;
         }
      } else {
         ed->patchfile->stackshift = 0;
         if (stacksave == STACK_MOVE)
            ed->options |= PATCHOPT_STACK_MOVE;
      }

   } else {
      //The modifs queue already exists
      WRNMSG("File %s is already ready for modification\n",
            asmfile_get_name(ed->afile));
      out = WRN_MADRAS_MODIFS_ALREADY_INIT;
   }
   return out;
}

/*
 * Adds a patch option
 * \param ed Pointer to the madras structure
 * \param option Modification flag(s) to add
 * \return EXIT_SUCCESS if update was successful, error code otherwise
 * */
int madras_modifs_addopt(elfdis_t* ed, int option)
{
   int out = EXIT_SUCCESS;

   if (ed == NULL) {
      return ERR_MADRAS_MISSING_MADRAS_STRUCTURE;
   }

   TRACE(ed, "madras_modifs_addopt(option=%x)\n", option);
   if (ed->patchfile) {
      ed->options |= option;
   } else {
      out = ERR_PATCH_NOT_INITIALISED;
   }
   return out;
}

/*
 * Removes a patch option
 * \param ed Pointer to the madras structure
 * \param option Modification flag(s) to remove
 * \return EXIT_SUCCESS if update was successful, error code otherwise
 * */
int madras_modifs_remopt(elfdis_t* ed, int option)
{
   int out = EXIT_SUCCESS;

   if (ed == NULL) {
      return ERR_MADRAS_MISSING_MADRAS_STRUCTURE;
   }

   TRACE(ed, "madras_modifs_remopt(option=%x)\n", option);
   if (ed->patchfile) {
      ed->options &= ~option;
   } else {
      out = ERR_PATCH_NOT_INITIALISED;
   }
   return out;
}

/*
 * Overrides the default choice of instruction used to pad blocks moved because of modifications for all modifications.
 * By default, the smallest \c nop instruction (no operation) for the relevant architecture is used.
 * If an instruction larger than this \c nop instruction is provided, an error will be returned
 * \param ed Pointer to the madras structure
 * \param insn Pointer to a structure representing the instruction to use
 * \param strinsn String representation of the assembly code for the instruction (used if \c insn is NULL)
 * \return EXIT_SUCCESS if successful, error code otherwise
 * */
int madras_modifs_setpaddinginsn(elfdis_t* ed, insn_t* insn, char* strinsn)
{
   if ((!ed) || (!ed->patchfile))
      return ERR_MADRAS_MISSING_MADRAS_STRUCTURE;

   char buf[64];
   insn_print(insn, buf, sizeof(buf));
   TRACE(ed, "madras_modifs_setpaddinginsn(insn=%s,strinsn=%s)\n",
         (insn)?buf:NULL, strinsn);
   insn_t* newpaddinginsn = NULL;
   if (insn != NULL)
      newpaddinginsn = insn_copy(insn);
   else {
      newpaddinginsn = insn_parsenew(strinsn, asmfile_get_arch(ed->afile));
      if (!newpaddinginsn) {
         ERRMSG(
               "Unable to parse instruction \"%s\" to set as padding for modifications\n",
               strinsn);
         return ERR_LIBASM_INSTRUCTION_NOT_PARSED;

      }
      int res = assemble_insn(newpaddinginsn, ed->patchfile->asmbldriver);
      if (ISERROR(res)) {
         ERRMSG(
               "Unable to assemble instruction \"%s\" to set as padding for modifications\n",
               strinsn);
         return res;
      }
      if (res != EXIT_SUCCESS)
         ed->last_error_code = res;
   }
   if (insn_get_size(newpaddinginsn)
         > insn_get_size(ed->patchfile->paddinginsn)) {
      char buf1[128], buf2[128];
      insn_print(newpaddinginsn, buf1, sizeof(buf1));
      insn_print(ed->patchfile->paddinginsn, buf2, sizeof(buf2));
      ERRMSG(
            "Instruction %s provided as new padding instruction for the patching session is larger than current instruction %s. " "Update cancelled\n",
            buf1, buf2);
      return ERR_PATCH_PADDING_INSN_TOO_BIG;
   }

   ed->patchfile->paddinginsn = newpaddinginsn;
   return EXIT_SUCCESS;
}

/**
 * Wrapper to the function for disassembling a file containing possibly multiple files
 * \param af File to disassemble
 * \param afs Array of disassembled files if more than one file was present
 * \param fd Descriptor of the file to disassemble, will be used if \c af is NULL
 * \return Number of file disassembled or -1 if an error occurred. In this case, the last error code in \c af will have been udpated
 * */
int multiple_disassembler(asmfile_t* af, asmfile_t*** afs, int fd)
{
   /**\todo TODO (2014-11-19) Rewrite this once the multiple disassembly is restored*/
   HLTMSG("IMPLEMENTATION DISABLED FOR NOW\n");
//    return  asmfile_disassemble_n(af,afs,DISASSEMBLY_FULL,fd);
}

/*
 * Adds a library as a mandatory external library
 * \param ed A pointer to the structure holding the disassembled file
 * \param extlibname The name of the library to add
 * \return A pointer to the library modification object representing the addition if the operation succeeded, a null value otherwise
 * In that case, the last error code in \c ed will be filled
 * */
modiflib_t* madras_extlib_add(elfdis_t* ed, char* extlibname)
{
   TRACE(ed, "madras_extlib_add(extlibname=%s", extlibname);
   modiflib_t* modlib = add_extlib(ed->patchfile, extlibname, 0,
         multiple_disassembler);
   TRACE_END(ed, modlib, modiflib);
   if (modlib != NULL) {
      return modlib;
   } else {
      ERRMSG("Unable to add library name %s for insertion", extlibname);
      madras_transfer_last_error_code(ed,
            patchfile_get_last_error_code(ed->patchfile),
            ERR_MADRAS_ADD_LIBRARY_FAILED);
      return NULL;
   }
}

/*
 * Returns the labels defined in an inserted library.
 * \param ed Pointer to the madras structure
 * \param modlib The \ref modiflib_t structure describing an library insertion
 * \param labels Queue to be filled with the labels in the library.
 * Must be initialised or set to NULL if it is not needed.
 * \param labels_table Hashtable (indexed on label names) to be filled with the labels in the library.
 * Must be initialised (using str_hash and str_equal for hashing and comparison functions) or set to NULL if it is not needed.
 * \return EXIT_SUCCESS if the operation went successfully, error code otherwise (e.g. \c modlib is not a library insertion).
 * EXIT_SUCCESS will be returned if \c labels and \c labels_table are both NULL (but it is useless)
 * */
int madras_modiflib_getlabels(elfdis_t* ed, modiflib_t* modlib, queue_t* labels,
      hashtable_t* labels_table)
{
   (void) ed; //Avoiding warning at compilation. \c ed can be used later for printing traces
   return modiflib_getlabels(modlib, labels_table, labels);

}

/*
 * Adds a flag to an inserted library.
 * \param ed Pointer to the madras structure
 * \param modlib The \ref modiflib_t structure describing an library insertion
 * \param flag a flag to add. Allowed flags are PATCHOPT_LIBFLAG_...
 * \return EXIT_SUCCESS if the operation went successfully, error code otherwise
 */
int madras_modiflib_add_flag(elfdis_t* ed, modiflib_t* modlib, int flag)
{
   (void) ed;
   if (modlib == NULL)
      return ERR_PATCH_MISSING_MODIF_STRUCTURE;
   if (modlib->type != ADDLIB)
      return ERR_PATCH_WRONG_MODIF_TYPE;
   TRACE(ed, "madras_modiflib_add_flag(modlib=%s%d,flag=%x)\n",
         (modlib) ? "modiflib_" : "", MODIFLIB_ID(modlib), flag);

   modlib->data.inslib->flags |= flag;

   return EXIT_SUCCESS;
}

/*
 * Returns the library associated to a new function call
 * \param ed Pointer to the madras structure
 * \param modif A function insertion
 * \return The modiflib associated to a function insertion, else NULL.
 * In this case, the last error code in \c ed will be filled
 */
modiflib_t* madras_fctlib_getlib(elfdis_t* ed, modif_t* modif)
{
   if (modif == NULL) {
      madras_set_last_error_code(ed, ERR_PATCH_MISSING_MODIF_STRUCTURE);
      return NULL;
   }

//   if (modif->type != MODTYPE_INSERT)
//      return (NULL);
//   else
//   {
   //insert_t* insert = modif->data.insert;
   if (modif->fct == NULL) {
      madras_set_last_error_code(ed, ERR_PATCH_MISSING_MODIF_STRUCTURE);
      return NULL;
   }
   return modif->fct->srclib;
}

/*
 * Adds a library as a mandatory external library from a file descriptor
 * \param ed A pointer to the structure holding the disassembled file
 * \param extlibname The name of the library to add (mandatory). It may be different than the actual name of the file whose descriptor is provided
 * in \c filedesc.
 * \param filedesc The descriptor of the file containing the library to add (must be > 0)
 * \return A pointer to the library modification object representing the addition if the operation succeeded, a null value otherwise
 * If an error occurred, the last error code in \c ed will be filled
 * */
modiflib_t* madras_extlib_add_fromdescriptor(elfdis_t* ed, char* extlibname,
      int filedesc)
{
   if (filedesc <= 0) {
      ERRMSG(
            "Invoked madras_extlib_add_fromdescriptor with an invalid file descriptor (%d)\n",
            filedesc);
      madras_set_last_error_code(ed, ERR_COMMON_FILE_INVALID);
      return NULL;
   }
   TRACE(ed, "madras_extlib_add_fromdescriptor(extlibname=%s,filedesc=%d",
         extlibname, filedesc);
   modiflib_t* modlib = add_extlib(ed->patchfile, extlibname, filedesc,
         multiple_disassembler);
   TRACE_END(ed, modlib, modiflib);
   if (modlib != NULL) {
      return modlib;
   } else {
      ERRMSG(
            "Unable to add library from file with descriptor %d under name name %s for insertion",
            filedesc, extlibname);
      madras_set_last_error_code(ed, ERR_MADRAS_ADD_LIBRARY_FAILED);
      return NULL;
   }
}

/*
 * Renames a dynamic library
 * \param ed a disassembled ELF file
 * \param oldname Name of the library to rename. If this name does not contain the '/' character, every references to this name (including
 * when preceded by a path) will be renamed (the preceding path will be kept identical if present).
 * Otherwise (if the name contains at least one '/' character), only the references matching exactly with the name given will be renamed.
 * \param newname New name of the library
 * \return A pointer to the library modification object representing the modification if the operation succeeded, a null value otherwise
 * If an error occurred, the last error code in \c ed will be filled
 */
modiflib_t* madras_extlib_rename(elfdis_t* ed, char* oldname, char* newname)
{
   if (ed == NULL || oldname == NULL || newname == NULL) {
      madras_set_last_error_code(ed, ERR_COMMON_PARAMETER_MISSING);
      return NULL;
   }
   if (strcmp(oldname, newname) == 0) {
      madras_set_last_error_code(ed, WRN_MADRAS_NEWNAME_IDENTICAL);
      return NULL;
   }
   TRACE(ed, "madras_extlib_rename(oldname=%s,newname=%s", oldname, newname);
   //Sanity check: detected whether there is an existing external library by that name
   uint16_t i;
   for (i = 0; i < binfile_get_nb_ext_libs(ed->patchfile->bfile); i++)
      if (str_equal(binfile_get_ext_lib_name(ed->patchfile->bfile, i), oldname))
         break;
   if (i == binfile_get_nb_ext_libs(ed->patchfile->bfile)) {
      //Error: no library by that name found
      madras_set_last_error_code(ed, ERR_BINARY_EXTLIB_NOT_FOUND);
      return NULL;
   }
   //Now next sanity check: detecting if we have an existing request for renaming this library
   FOREACH_INQUEUE(ed->patchfile->modifs_lib, iter) {
      modiflib_t* mod = GET_DATA_T(modiflib_t*, iter);
      if (mod->type == RENAMELIB
            && str_equal(mod->data.rename->oldname, oldname)) {
         //Error: renaming request already existing for this library
         /**\todo (2015-11-18) To be decided whether this counts as an error, or is flagged as a warning. If a warning, also
          * decide whether we overwrite the previous renaming or do nothing*/
         madras_set_last_error_code(ed, ERR_MADRAS_RENAMING_LIBRARY_EXISTING);
         return NULL;
      }
   }
   //Now that everything is going well, creating the modification request
   renamed_lib_t* rl = lc_malloc(sizeof(renamed_lib_t));
   modiflib_t* modlib = NULL;
   rl->oldname = oldname;
   rl->newname = newname;
   modlib = modiflib_add(ed->patchfile, RENAMELIB, rl);
   if (!modlib)
      madras_transfer_last_error_code(ed,
            patchfile_get_last_error_code(ed->patchfile),
            ERR_MADRAS_MODIF_LIBRARY_FAILED);
   TRACE_END(ed, modlib, modiflib);
   return modlib;
}

/*
 * Renames a dynamic symbol
 * \param ed a disassembled ELF file
 * \param library The library the new label must point to
 * \param oldname The old name of the label
 * \param newname The new name of the label
 * \return EXIT_SUCCESS or an error code
 */
int madras_extfct_rename(elfdis_t* ed, char* library, char* oldname,
      char* newname)
{
   if (ed == NULL || oldname == NULL || newname == NULL)
      return ERR_COMMON_PARAMETER_MISSING;

   if (strcmp(oldname, newname) == 0)
      return WRN_MADRAS_NEWNAME_IDENTICAL;

   TRACE(ed, "madras_extfct_rename(library=%s,oldname=%s,newname=%s\n", library,
         oldname, newname);

   queue_add_tail(ed->patchfile->modifs_lbl,
         modiflbl_new(0, newname, LABELTYPE_NONE, NULL, oldname, RENAMELABEL));
   madras_extlib_add(ed, library);

   return madras_get_last_error_code(ed);
}

/**
 * Retrieves the node containing the instruction at a given address.
 * \param ed The madras structure
 * \param addr The address to find a node in if strictly positive. If negative, the cursor will be returned. If null, NULL will be returned
 * \return The list node containing the instruction at \c addr. If addr is not null and NULL is returned, an error has occurred.
 * In this case, the last error code in \c ed will be filled
 * */
static list_t* get_node_from_address(elfdis_t* ed, int64_t addr)
{
   list_t* cursor = NULL;
   if (addr > 0) {
      //An address is given: positioning the cursor
      insn_t* inscursor = asmfile_get_insn_by_addr(ed->afile, addr);
      if (inscursor) {
         cursor = insn_get_sequence(inscursor);
         ed->cursor = cursor;
      } else {
         ERRMSG("Unable to find instruction at address %#"PRIx64"\n", addr);
         madras_set_last_error_code(ed, ERR_LIBASM_INSTRUCTION_NOT_FOUND);
         return NULL;
      }
   } else if (addr < 0) {
      if (ed->cursor != NULL)
         cursor = ed->cursor;
      else {
         ERRMSG("No specified address for list insertion\n");
         madras_set_last_error_code(ed, ERR_LIBASM_ADDRESS_INVALID);
         return NULL;
      }
   }
   return cursor;
}

/**
 * Inserts a list of instructions into the file.
 * \param ed A pointer to the structure holding the disassembled file
 * \param insns A queue of insn_t structures containing the instructions to add
 * \param addr Address at which the instruction list must be inserted, or -1 if the address of the instruction
 * pointed to by the cursor must be used. If 0, the instruction list will be inserted in the file but not linked to any address
 * \param pos Specifies where the instructions must be inserted relatively to the given address or instruction, using values from insertpos_t
 * \param linkedvars Array of pointer to global variables structures. This array must contain exactly as many entries as \e insnlist contains
 * instructions referencing a global variable, in the order into which they appear in the list. It is advised to have it NULL-terminated to
 * detect errors more easily, but this is not required. If set to -1, its content will not be printed in the TRACE
 * \param assemble Set to TRUE if the list must be assembled, FALSE if it already is
 * \todo Get rid of the \c assemble parameter
 * \return A pointer to the code modification object representing the insertion if the operation succeeded, a null value otherwise
 * If an error occurred, the last error code in \c ed will be filled
 * */
static modif_t* insns_add(elfdis_t* ed, queue_t* insns, int64_t addr,
      insertpos_t pos, globvar_t** linkedvars, tlsvar_t** linkedtlsvars,
      boolean_t assemble)
{
   int asmresult = EXIT_SUCCESS;
   modif_t* mod = NULL;
   list_t* cursor = NULL;

   cursor = get_node_from_address(ed, addr);
   if ((!cursor) && (addr)) {
      return NULL; //get_node_from_address will have updated the error code in ed
   }

   if (insns && ((queue_length(insns) > 0) || (addr == 0))) { //Preventing insertion of empty lists except for floating insertions
      //Generating the corresponding queue of insn_t objects
      if (assemble)
         asmresult = assemble_list(ed->patchfile->asmbldriver, insns);

#ifndef NDEBUG //This is to ensure we have the same output in debug as in trace mode
      boolean_t dbg = FALSE;
      DBG(dbg = TRUE);
      //Printing the list of global variables in the trace output
      if ((ed->loginfo->trace || dbg) && ((int64_t) linkedvars >= 0)) {
#else
         if ( ed->loginfo->trace && ((int64_t)linkedvars >= 0)) {
#endif
         //The test on linkedvars is only to avoid printing it if this function is invoked from an API function where they are not an argument (madras_branch_insert)
         /**\todo This code was copied from insert_newlist and is used only for the TRACE. See how it could be mutualised*/
         if ((queue_length(insns) > 0) && (linkedvars != NULL)) {
            TRACE(ed, ",linkedvars={");
            int n_gv = 0, i = 0;
            FOREACH_INQUEUE(insns, iter) {
               int isinsn;
               int64_t dest;
               pointer_t* refptr = oprnd_get_memrel_pointer(
                     insn_lookup_ref_oprnd(GET_DATA_T(insn_t*, iter)));
               /**\todo TODO (2015-05-20) I'm updating this code quickly so that the trace is identical to what it was
                * before, to validate the non-regression tests. The next update will simply be to remove the array of globvar
                * passed in parameters, as the instructions in the list will already be linked to the data*/
//                   if ( (( (dest = insn_checkrefs(GET_DATA_T(insn_t*,iter), &isinsn)) >= 0 ) && ( isinsn == 0 ) )
//                       && ( dest == (insn_get_addr(GET_DATA_T(insn_t*,iter)) + insn_get_size(GET_DATA_T(insn_t*,iter)) / 8) ) ) {
                  /*Instruction is a reference to a data structure*/
               if (refptr && !pointer_get_data_target(refptr)) {
                  TRACE(ed, "%s%s%d", (n_gv == 0) ? "" : ",",
                        (linkedvars[n_gv]) ? "globvar_" : "",
                        GLOBVAR_ID(linkedvars[n_gv]));
                  n_gv++;
               }
               i++;
            }
            TRACE(ed, "}");
         } else
            TRACE(ed, ",linkedvars=%p", linkedvars);
      }

#ifndef NDEBUG //This is to ensure we have the same output in debug as in trace mode
      //Printing the list of tls variables in the trace output
      if ((ed->loginfo->trace || dbg) && ((int64_t) linkedtlsvars >= 0)) {
#else
         if ( ed->loginfo->trace && ((int64_t)linkedtlsvars >= 0)) {
#endif
         //The test on linkedtlsvars is only to avoid printing it if this function is invoked from an API function where they are not an argument (madras_branch_insert)
         /**\todo This code was copied from insert_newlist and is used only for the TRACE. See how it could be mutualised*/
         if ((queue_length(insns) > 0) && (linkedtlsvars != NULL)) {
            TRACE(ed, ",linkedtlsvars={");
            int n_tls = 0, i = 0;
            FOREACH_INQUEUE(insns, iter) {
               int isinsn;
               int64_t dest;
               if ((((dest = insn_check_refs(GET_DATA_T(insn_t*, iter), &isinsn))
                     >= 0) && (isinsn == 0))
                     && (dest
                           == (INSN_GET_ADDR(GET_DATA_T(insn_t*, iter))
                                 + insn_get_size(GET_DATA_T(insn_t*, iter)) / 8))) {
                  /*Instruction is a reference to a data structure*/
                  TRACE(ed, "%s%s%d", (n_tls == 0) ? "" : ",",
                        (linkedtlsvars[n_tls]) ? "tlsvar_" : "",
                        TLSVAR_ID(linkedtlsvars[n_tls]));
                  n_tls++;
               }
               i++;
            }
            TRACE(ed, "}");
         } else
            TRACE(ed, ",linkedtlsvars=%p", linkedtlsvars);
      }

      if (!ISERROR(asmresult)) {
         //Creating the insertion request and adding it to the list of requests
         mod = insert_newlist(ed->patchfile, insns, addr, cursor,
               (pos == INSERT_BEFORE) ? MODIFPOS_BEFORE : MODIFPOS_AFTER,
               ((int64_t) linkedvars >= 0) ? linkedvars : NULL,
               ((int64_t) linkedtlsvars >= 0) ? linkedtlsvars : NULL);
         int res = patchfile_get_last_error_code(ed->patchfile);
         if (res != EXIT_SUCCESS)
            ed->last_error_code = res;
      } else {
         ERRMSG("Unable to assemble instruction list\n");
         madras_set_last_error_code(ed, asmresult);
         mod = NULL;
      }
   } else {
      DBGMSG(
            "Instruction list to insert at address %#"PRIx64" is NULL or empty\n",
            addr);
      madras_set_last_error_code(ed, ERR_COMMON_PARAMETER_MISSING);
      mod = NULL;
   }
   return mod;
}

/*
 * Inserts a list of instruction into the file.
 * \param ed A pointer to the structure holding the disassembled file
 * \param insns A queue of insn_t structures containing the instructions to add
 * \param addr Address at which the instruction list must be inserted, or -1 if the address of the instruction
 * pointed to by the cursor must be used. If 0, the instruction list will be inserted in the file but not linked to any address
 * \param pos Specifies where the instructions must be inserted relatively to the given address or instruction, using values from insertpos_t
 * \param linkedvars Array of pointer to global variables structures. This array must contain exactly as many entries as \c insns contains
 * instructions referencing a global variable, in the order into which they appear in the list. It is advised to have it NULL-terminated to
 * detect errors more easily, but this is not required.
 * \param reassemble Boolean used to know if given instructions must be reassemble (TRUE) or not (FALSE).
 * \return A pointer to the code modification object representing the insertion if the operation succeeded, a null value otherwise
 * If an error occurred, the last error code in \c ed will be filled
 * */
modif_t* madras_add_insns(elfdis_t* ed, queue_t* insns, int64_t addr,
      insertpos_t pos, globvar_t** linkedvars, tlsvar_t** linkedtlsvar,
      boolean_t reassemble)
{
   /**\todo This function should return an insert_t pointer => Yup, done*/
   if (ed == NULL) {
      //error
      return NULL;
   }

   modif_t* out;
   TRACE(ed, "madras_add_insns(insns=");
   if (insns != NULL) {
      TRACE(ed, "{");
      FOREACH_INQUEUE(insns, titer) {
         char buf[STR_INSN_BUF_SIZE];
         insn_print(GET_DATA_T(insn_t*, titer), buf, sizeof(buf));
         TRACE(ed, "%s", buf);
         if (titer != queue_iterator_rev(insns)) {
            TRACE(ed, "\\n");
         }
      }
      TRACE(ed, "}");
   } else
      TRACE(ed, "%p", insns);
   TRACE(ed, ",addr=%#"PRIx64",after=%d", addr, pos);
   out = insns_add(ed, insns, addr, pos, linkedvars, linkedtlsvar, reassemble);

   TRACE(ed, ",reassemble=%s", (reassemble == TRUE) ? "TRUE" : "FALSE")
   TRACE_END(ed, out, modif);
   return out;
}

/*
 * Inserts one instructions into the file.
 * \param ed A pointer to the structure holding the disassembled file
 * \param insn A pointer to an insn_t structure representing the instruction to add
 * \param addr Address at which the instruction list must be inserted, or -1 if the address of the instruction
 * pointed to by the cursor must be used. If 0, the instruction list will be inserted in the file but not linked to any address
 * \param pos Specifies where the instructions must be inserted relatively to the given address or instruction, using values from insertpos_t
 * \param linkedvar A pointer to a global variable structure, if the inserted instruction references one.
 * \param reassemble Boolean used to know if given instructions must be reassemble (TRUE) or not (FALSE).
 * \return A pointer to the code modification object representing the insertion if the operation succeeded, a null value otherwise
 * If an error occurred, the last error code in \c ed will be filled
 * */
modif_t* madras_add_insn(elfdis_t* ed, insn_t* insn, int64_t addr,
      insertpos_t pos, globvar_t* linkedvar, tlsvar_t* linkedtlsvar,
      boolean_t reassemble)
{
   if (ed == NULL) {
      //error
      return NULL;
   }
   modif_t* out;
   queue_t* inslist;
   globvar_t* lvars[2];
   tlsvar_t* tlsvars[2];

   TRACE(ed, "madras_add_insn(insn=");
   if (insn != NULL) {
      char buf[STR_INSN_BUF_SIZE];
      insn_print(insn, buf, sizeof(buf));
      TRACE(ed, "\"%s\"", buf);
   } else
      TRACE(ed, "%p", insn);
   TRACE(ed, ",addr=%#"PRIx64",after=%d", addr, pos);
   inslist = queue_new();
   add_insn_to_insnlst(insn, inslist);
   lvars[0] = linkedvar;
   lvars[1] = NULL;
   tlsvars[0] = linkedtlsvar;
   tlsvars[1] = NULL;

   out = insns_add(ed, inslist, addr, pos, (linkedvar) ? lvars : NULL,
         (linkedtlsvar) ? tlsvars : NULL, reassemble);
   TRACE(ed, ",reassemble=%s", (reassemble == TRUE) ? "TRUE" : "FALSE")
   TRACE_END(ed, out, modif);
   return out;
}

/**
 * Creates an insert function call request
 * \param ed Pointer to the MADRAS structure
 * \param fctname Name of the function to insert
 * \param libname Name of the library the function belongs to
 * \param addr Address at which to insert the function call
 * \param pos Specifies where the instructions must be inserted relatively to the given address or instruction, using values from insertpos_t
 * \param wrap Set to TRUE to wrap the call with instructions for saving/restoring the context and aligning the stack
 * \return Pointer to an modif_t structure containing the details about the insertion (in an insert_t structure), NULL if an error occurred
 * In that case, the last error code in \c ed will be filled
 * */
static modif_t* fctcall_new(elfdis_t* ed, char* fctname, char* libname,
      int64_t addr, insertpos_t pos, boolean_t wrap, reg_t** reglist, int nreg)
{
   /**\todo This function should be moved to \e libmpatch (multiple_disassembler needs to be passed as parameter)*/
   assert(ed);
   insfct_t* fct = NULL;
   modif_t* mod = NULL;

   list_t* cursor = NULL;

   cursor = get_node_from_address(ed, addr);
   //No error occurred when positioning the cursor
   if ((!cursor) && (addr)) {
      return NULL; //get_node_from_address will have updated the error code in ed
   }

   if (ed->patchfile) {
      fct = insfct_new(fctname, NULL, 0, NULL, reglist, nreg);
      if (!fct) {
         ERRMSG("Unable to create insertion request for function %s\n", fctname);
         return NULL;
      }
      if (libname) {
         //Adds a request for insertion for  the library this function is defined in and retrieves the insertion library object
         modiflib_t* fctlib = add_extlib(ed->patchfile, libname, 0,
               multiple_disassembler);
         //Attaches the library object to the insertion function
         fct->srclib = fctlib;
      }
      //Adds the insertion function call in the list of modifications for the disassembled file
      //The list is ordered with the lowest insertion addresses at the beginning
      mod = modif_add(ed->patchfile, addr, cursor, MODTYPE_INSERT,
            (pos == INSERT_BEFORE) ? MODIFPOS_BEFORE : MODIFPOS_AFTER);
      if (!mod) {
         madras_transfer_last_error_code(ed,
               patchfile_get_last_error_code(ed->patchfile),
               ERR_MADRAS_MODIF_CODE_FAILED);
      } else {
         mod->fct = fct;
         if (!wrap)
            mod->flags |= flags_madras2patcher(PATCHOPT_FCTCALL_NOWRAP);
      }
   } else {
      ERRMSG("File %s is not open for modifications\n",
            asmfile_get_name(ed->afile));
      ed->last_error_code = ERR_PATCH_NOT_INITIALISED;
      mod = NULL;
   }
   return mod;
}

/*
 * Finalises a modification.
 * \param ed A pointer to the structure holding the disassembled file
 * \param modif The modification
 * \return TRUE if the modif can be enqueued, FALSE otherwise
 * */
int madras_modif_commit(elfdis_t* ed, modif_t* modif)
{
   if (!ed)
      return FALSE;
   return patchfile_modif_finalise(ed->patchfile, modif);
}

/*
 * Inserts a list of instruction into the file.
 * \param ed A pointer to the structure holding the disassembled file
 * \param insnlist The list of instructions to add (terminated with character '\\0').
 * The instructions must be separated from one another by a '\\n' character from the next.
 * An instruction must respect the following format: \c MNEMO \c OPLIST where \c MNEMO is
 * the mnemonic of the instruction, and \c OPLIST the list of its operands, separated from one
 * another by ',' and not containing any space. The operands format and order must be conform
 * to the AT&T syntax.
 * \param addr Address at which the instruction list must be inserted, or -1 if the address of the instruction
 * pointed to by the cursor must be used. If 0, the instruction list will be inserted in the file but not linked to any address
 * \param pos Specifies where the instructions must be inserted relatively to the given address or instruction, using values from insertpos_t
 * \param linkedvars Array of pointer to global variables structures. This array must contain exactly as many entries as \e insnlist contains
 * instructions referencing a global variable, in the order into which they appear in the list. It is advised to have it NULL-terminated to
 * detect errors more easily, but this is not required.
 * \return A pointer to the code modification object representing the insertion if the operation succeeded, a null value otherwise
 * If an error occurred, the last error code in \c ed will have been filled
 * */
modif_t* madras_insnlist_add(elfdis_t* ed, char* insn_list, int64_t addr,
      insertpos_t pos, globvar_t** linkedvars, tlsvar_t** linkedtlsvars)
{
   if (ed == NULL) {
      //error
      return NULL;
   }

   if (ed->patchfile == NULL) {
      ERRMSG(
            "madras_insnlist_add invoked on a file not prepared for modification\n");
      ed->last_error_code = ERR_PATCH_NOT_INITIALISED;
      return NULL;
   }

   modif_t* mod = NULL;
   queue_t** insnq = (queue_t**) lc_malloc0(sizeof(queue_t*));
   TRACE(ed, "madras_insnlist_add(insnlist=\"%s\",addr=%#"PRIx64",after=%d",
         insn_list, addr, pos);

   if (insn_list) {
      //Generating the corresponding queue of insn_t objects
      int assembling_status = assemble_strlist(ed->patchfile->asmbldriver,
            insn_list, ed->afile, insnq);
      if (*insnq && !ISERROR(assembling_status)) {
         //May not be useful, but who knows...
         FOREACH_INQUEUE(*insnq, iter1)
            insn_set_addr(GET_DATA_T(insn_t*, iter1), -1);

         mod = insns_add(ed, *insnq, addr, pos, linkedvars, linkedtlsvars,
               TRUE);
         if (!ISERROR(
               ed->last_error_code) && !ISWARNING(ed->last_error_code) && assembling_status != EXIT_SUCCESS)
            ed->last_error_code = assembling_status;
      } else {
         ERRMSG("Unable to assemble instruction list\n");
         ed->last_error_code = assembling_status;
         mod = NULL;
      }
   } else {
      mod = NULL;
      ERRMSG("Instruction list to insert is NULL\n");
      ed->last_error_code = ERR_COMMON_PARAMETER_MISSING;
   }
   lc_free(insnq);

   TRACE_END(ed, mod, modif);
   return mod;
}

/*
 * Creates a new request for a modification of an instruction
 * \warning The current implementation of this function does not make any "sanity check" concerning the
 * validity of the instruction provided as replacement. It is currently mainly intended for replacing
 * SSE instructions by other SSE instructions (and memory operands by registers). Its behaviour in other
 * uses is not guaranteed
 * \param ed A pointer to the structure holding the disassembled file
 * \param addr The address of the instruction to replace. If set to -1, the instruction pointed to by
 * the cursor will updated instead. Otherwise, it will be set at this address
 * \param withpadding If set to 1, and if an instruction is replaced by a shorter one, the difference in
 * size will be padded by a NOP instruction. Otherwise, no padding will take place. This is ignored if the
 * instruction is replaced by a longer one.
 * \param newopcode The opcode to replace the original opcode with, or NULL if the original must be kept
 * \param noperands The number of operands of the new instruction.
 * \param operands The operands, each in char* format, that must replace the operands of the original instruction.
 * They must appear in the order they would appear in in the AT&T assembly representation of the instruction.
 * A NULL value indicates that the corresponding operand must not be replaced. The number of operands in this
 * last array must be the same as the value of noperands.
 * \return A pointer to the code modification object representing the modification if the operation succeeded, null value otherwise
 * If an error occurred, the last error code in \c ed will have been filled
 * */
modif_t* madras_modify_insn(elfdis_t* ed, int64_t addr, boolean_t withpadding,
      char* newopcode, int noperands, ...)
{
   va_list ops;
   char ** operands = NULL;

   if (noperands > 0) {
      operands = lc_malloc(noperands * sizeof(char *));
      va_start(ops, noperands);
      int i;
      for (i = 0; i < noperands; i++) {
         operands[i] = va_arg(ops, char*);
      }
   }

   modif_t * ret = madras_modify_insn_array(ed, addr, withpadding, newopcode,
         noperands, operands);

   if (noperands > 0) {
      lc_free(operands);
      va_end(ops);
   }

   return ret;
}

/*
 * Creates a new request for a modification of an instruction
 * \warning The current implementation of this function does not make any "sanity check" concerning the
 * validity of the instruction provided as replacement. It is currently mainly intended for replacing
 * SSE instructions by other SSE instructions (and memory operands by registers). Its behaviour in other
 * uses is not guaranteed
 * \param ed A pointer to the structure holding the disassembled file
 * \param addr The address of the instruction to replace. If set to -1, the instruction pointed to by
 * the cursor will updated instead. Otherwise, it will be set at this address
 * \param withpadding If set to 1, and if an instruction is replaced by a shorter one, the difference in
 * size will be padded by a NOP instruction. Otherwise, no padding will take place. This is ignored if the
 * instruction is replaced by a longer one.
 * \param newopcode The opcode to replace the original opcode with, or NULL if the original must be kept
 * \param noperands The number of operands of the new instruction.
 * \param ... The operands, each in char* format, that must replace the operands of the original instruction.
 * They must appear in the order they would appear in in the AT&T assembly representation of the instruction.
 * A NULL value indicates that the corresponding operand must not be replaced. The number of operands in this
 * last list must be the same as the value of noperands.
 * \return A pointer to the code modification object representing the modification if the operation succeeded, null value otherwise
 * If an error occurred, the last error code in \c ed will have been filled
 * */
modif_t* madras_modify_insn_array(elfdis_t* ed, int64_t addr,
      boolean_t withpadding, char* newopcode, int noperands, char ** operands)
{
   if (ed == NULL)
      return NULL;
   /**\todo TODO (2014-12-18) Perform a test here to detect if there was not already a modification of type modify or delete
    * at this address, and raise a warning if so.*/
   int out = EXIT_SUCCESS;
   modif_t* mod = NULL;
   oprnd_t** newoperands = NULL;
   TRACE(ed,
         "madras_modify_insn(addr=%#"PRIx64",withpadding=%d,newopcode=%s,noperands=%d",
         addr, withpadding, newopcode, noperands);
   if (noperands > 0) {
      int i;
      char* opstr;
      newoperands = (oprnd_t**) lc_malloc(sizeof(oprnd_t*) * noperands);
      for (i = 0; i < noperands; i++) {
         opstr = operands[i];
         if (opstr) {
            int c = 0;
            TRACE(ed, ",operand=\"%s\"", opstr);
            newoperands[i] = oprnd_parsenew(opstr, &c,
                  asmfile_get_arch(ed->afile));
         } else {
            TRACE(ed, ",operand=%s", opstr);
            newoperands[i] = NULL;
         }
      }
   }
   if (addr) {
      //An address is given: positioning the cursor
      out = cursor_init(ed, NULL, addr, NULL);
   }
   if (!ISERROR(out)) {
      //No error occurred when positioning the cursor
      if (ed->cursor) {
         if ((newopcode) || (noperands > 0)) {
            insnmodify_t* imod;
            //Building a request with all modifications
            imod = insnmodify_new(newopcode, newoperands, noperands,
                  withpadding);
            //Adding the request to the list of modifications
            mod = modif_add(ed->patchfile,
                  INSN_GET_ADDR(GET_DATA_T(insn_t*, ed->cursor)), ed->cursor,
                  MODTYPE_MODIFY, MODIFPOS_REPLACE);
            if (!mod) {
               madras_transfer_last_error_code(ed,
                     patchfile_get_last_error_code(ed->patchfile),
                     ERR_MADRAS_MODIF_CODE_FAILED);
               return mod;
            }
            mod->insnmodify = imod;
         } else {
            mod = NULL;
            ed->last_error_code = ERR_COMMON_PARAMETER_MISSING;
            ERRMSG("No modifications requested to the instruction\n");
         }
      } else {
         ERRMSG(
               "Cursor instruction for disassembled file has not been initialized\n");
         mod = NULL;
         ed->last_error_code = ERR_MADRAS_MISSING_CURSOR;
      }
   } else
      ed->last_error_code = out;
   TRACE_END(ed, mod, modif);
   return mod;
}

/*
 * Creates a new request for modifying a direct branch instruction
 * \param ed A pointer to the structure holding the disassembled file
 * \param addr The address of the branch instruction to replace. If set to -1, the instruction pointed to by
 * the cursor will updated instead. Otherwise, it will be set at this address. If the instruction at this address
 * is not a branch, an error will be returned.
 * \param withpadding If set to 1, and if an instruction is replaced by a shorter one, the difference in
 * size will be padded by a NOP instruction. Otherwise, no padding will take place. This is ignored if the
 * instruction is replaced by a longer one.
 * \param newopcode The opcode to replace the original opcode with, or NULL if the original must be kept.
 * If the new opcode does not correspond to a branch instruction, an error will be returned
 * \param newdestaddr New destination address of the branch
 * \return A pointer to the code modification object representing the modification if the operation succeeded, null or negative value otherwise
 * */
modif_t* madras_modify_branch(elfdis_t* ed, int64_t addr, int withpadding,
      char* newopcode, int64_t newdestaddr)
{
   if (ed == NULL) {
      //error
      return NULL;
   }
   /**\todo TODO (2014-12-18) Perform a test here to detect if there was not already a modification of type modify or delete
    * at this address, and raise a warning if so.*/
   int out = 1;
   modif_t* mod = NULL;
   oprnd_t** newoperands = NULL;
   TRACE(ed,
         "madras_modify_branch(addr=%#"PRIx64",withpadding=%d,newopcode=%s,newdestaddr=%#"PRIx64,
         addr, withpadding, newopcode, newdestaddr);
   if (addr) {
      //An address is given: positioning the cursor
      out = cursor_init(ed, NULL, addr, NULL);
   }
   if (out > 0) {
      //No error occurred when positioning the cursor
      if (ed->cursor) {
         insn_t* in = GET_DATA_T(insn_t*, ed->cursor);
         if (!insn_is_direct_branch(in)) {
            ERRMSG(
                  "Unable to create request for branch modification at address %#"PRIx64": instruction is not a direct branch\n",
                  addr);
            TRACE_END(ed, mod, modif);
            return NULL;
         }
         if (newopcode) {
            WRNMSG(
                  "Modification of branch opcode not supported in this version: branch at address %#"PRIx64" will not be changed to %s\n",
                  addr, newopcode);
            newopcode = NULL;
            /**\todo TODO (2015-06-03) Implement this. We need to find an easy way to look up the new opcode into the arch_t structure to find if its
             * associated default annotation specifies that it is a branch, and return an error if it is not.
             * Of course, once it's done, remove the newopcode=NULL line
             * */
         }
         if (newopcode || newdestaddr != insn_get_addr(insn_get_branch(in))) {
            insnmodify_t* imod;
            int n_oprnds = insn_get_nb_oprnds(in);
            //Finds the instruction at the new target address
            insn_t* newdest = asmfile_get_insn_by_addr(ed->afile, newdestaddr);
            if (!newdest) {
               ERRMSG("No instruction found at address %#"PRIx64"\n",
                     newdestaddr);
               TRACE_END(ed, mod, modif);
               return NULL;
            }
            //Finds the index of the operand in the instruction that is a pointer
            uint8_t i = 0;
            while ((i < n_oprnds)
                  && oprnd_get_type(insn_get_oprnd(in, i)) != OT_POINTER)
               i++;
            assert(i < n_oprnds);
            newoperands = lc_malloc0(n_oprnds * sizeof(*newoperands));
            pointer_t* newptr = pointer_copy(
                  oprnd_get_ptr(insn_get_oprnd(in, i)));
            pointer_set_insn_target(newptr, newdest);
            /**\todo TODO (2015-06-03) Be very careful. I'm linking a new instruction to an existing one. Because
             * of that, I must perform a special handling in the patcher to add the branch in pf->newbranches,
             * and do some more tests afterwards. Also, this does not allow to restrict the update of the branch
             * using the PATCHOPT_BRANCHINS_NO_UPD_DST flag, but I'm not sure we still use it anyway (it can be set
             * with modif_setnext when we want to link modifications together).*/
            newoperands[i] = oprnd_new_pointer(newptr);
            //Building a request with all modifications
            imod = insnmodify_new(newopcode, newoperands, n_oprnds,
                  withpadding);
            //Adding the request to the list of modifications
            mod = modif_add(ed->patchfile, addr, ed->cursor, MODTYPE_MODIFY,
                  MODIFPOS_REPLACE);
            if (!mod) {
               ERRMSG(
                     "Unable to create request for modification of instruction at address %#"PRIx64"\n",
                     addr);
               TRACE_END(ed, mod, modif);
               return mod;
            }
            mod->insnmodify = imod;
         } else {
            mod = NULL;     //(modif_t*)WRN_PARAM_NULL;
            WRNMSG("No modifications requested to the instruction\n");
         }
      } else {
         WRNMSG(
               "Cursor instruction for disassembled file has not been initialized\n");
         mod = NULL;     //(modif_t*)WRN_CURSOR_NOTINIT;
      }
   }
   TRACE_END(ed, mod, modif);
   return mod;
}

/*
 * Creates a new request for a replacement of a list of instructions
 * \param ed A pointer to the structure holding the disassembled file
 * \param ninsn Number of instructions to replace, starting either from the cursor's location or \e addr
 * \param addr Address of the first instruction to be replaced, or 0 if the instruction pointed to by
 * the cursor is the first address to be deleted.  If an address is provided, the cursor will be positioned
 * at this address (equivalent to a call to \ref madras_init_cursor)
 * \param fillerver Version of the function generating the instruction for replacing the instruction. It must have been
 * defined in the source file associated to the architecture. If set to 0, NOP instructions will be used.
 * \return A pointer to the code modification object representing the replacement if the operation succeeded, null value otherwise
 * If an error occurred, the last error code in \c ed will have been filled
 * */
modif_t* madras_replace_insns(elfdis_t* ed, int ninsn, int64_t addr,
      int fillerver)
{
   /**\todo The fillerver parameter is not useful anymore. This function could be updated to perform more specific tasks.*/
   int out = EXIT_SUCCESS;
   /**\todo TODO (2014-12-23) Remove ninsn as a parameter. This will only ever target one instruction
    * => (2015-05-07) Done through madras_replace_insn*/
   modif_t* mod = NULL;
//   TRACE(ed,"madras_replace_insns(ninsn=%d,addr=%#"PRIx64",fillerver=%d",ninsn,addr,fillerver);
   if (addr) {
      //An address is given: positioning the cursor
      out = cursor_init(ed, NULL, addr, NULL);
   }
   if (!ISERROR(out)) {
      //No error occurred when positioning the cursor
      if (ed && ed->cursor) {
         mod = madras_replace_insn(ed, addr);
         int nbmod = 1;
         list_t* iter = ed->cursor->next;
         while (iter && nbmod < ninsn) {
            addr = insn_get_addr(GET_DATA_T(insn_t*, iter));
            madras_replace_insn(ed, addr);
            iter = iter->next;
            nbmod++;
         }
//         replace_t* del = replace_new(NULL);
//         //Adds the replace request in the list of modifications for the disassembled file
//         //The list is ordered with the lowest insertion addresses at the beginning
//         mod = modif_add(ed->patchfile,insn_get_addr((insn_t*)ed->cursor->data),ed->cursor,MODTYPE_REPLACE,del);
      } else {
         WRNMSG(
               "Cursor instruction for disassembled file has not been initialized\n");
         ed->last_error_code = ERR_MADRAS_MISSING_CURSOR;
         mod = NULL;
      }
   } else
      ed->last_error_code = out;
//   TRACE_END(ed,mod,modif);
   return mod;
}

/*
 * Creates a new request for a replacement an instruction
 * \param ed A pointer to the structure holding the disassembled file
 * \param addr Address of the instruction to be replaced, or 0 if the instruction pointed to by
 * the cursor is the first address to be deleted.  If an address is provided, the cursor will be positioned
 * at this address (equivalent to a call to \ref madras_init_cursor)
 * \param fillerver Version of the function generating the instruction for replacing the instruction. It must have been
 * defined in the source file associated to the architecture. If set to 0, NOP instructions will be used.
 * \return A pointer to the code modification object representing the replacement if the operation succeeded, null otherwise
 * If an error occurred, the last error code in \c ed will have been filled
 * */
modif_t* madras_replace_insn(elfdis_t* ed, int64_t addr)
{
   /**\todo The fillerver parameter is not useful anymore. This function could be updated to perform more specific tasks.*/
   /**\todo TODO (2014-12-23) Remove ninsn as a parameter. This will only ever target one instruction
    * => (2015-04-07) Doing it now, as well as fillerver*/
   int out = EXIT_SUCCESS;
   modif_t* mod = NULL;
   TRACE(ed, "madras_replace_insn(addr=%#"PRIx64"", addr);
   if (addr) {
      //An address is given: positioning the cursor
      out = cursor_init(ed, NULL, addr, NULL);
   }
   if (!ISERROR(out)) {
      //No error occurred when positioning the cursor
      if (ed && ed->cursor) {
         //replace_t* del = replace_new(NULL);
         //Adds the replace request in the list of modifications for the disassembled file
         //The list is ordered with the lowest insertion addresses at the beginning
         mod = modif_add(ed->patchfile,
               INSN_GET_ADDR((insn_t* )ed->cursor->data), ed->cursor,
               MODTYPE_REPLACE, MODIFPOS_REPLACE);
         if (!mod)
            madras_transfer_last_error_code(ed,
                  patchfile_get_last_error_code(ed->patchfile),
                  ERR_MADRAS_MODIF_CODE_FAILED);
      } else {
         WRNMSG(
               "Cursor instruction for disassembled file has not been initialized\n");
         ed->last_error_code = ERR_MADRAS_MISSING_CURSOR;
         mod = NULL;
      }
   } else
      ed->last_error_code = out;
   TRACE_END(ed, mod, modif);
   return mod;
}

/*
 * Creates a new request for a deletion of a list of instructions
 * \param ed A pointer to the structure holding the disassembled file
 * \param inisn Number of instructions to delete, starting either from the cursor's location or \e addr
 * \param addr Address of the first instruction to be deleted, or 0 if the instruction pointed to by
 * the cursor is the first address to be deleted.  If an address is provided, the cursor will be positioned
 * at this address (equivalent to a call to \ref madras_init_cursor)
 * \return A pointer to the code modification object representing the deletion if the operation succeeded, null value otherwise
 * If an error occurred, the last error code in \c ed will have been filled
 * */
modif_t* madras_delete_insns(elfdis_t* ed, int ninsn, int64_t addr)
{
   int out = EXIT_SUCCESS;
   /**\todo TODO (2014-12-23) Remove ninsn as a parameter. This will only ever target one instruction
    * =>(2015-05-07) Using madras_delete_insn to handle the case*/
   modif_t* mod = NULL;
//   TRACE(ed,"madras_delete_insns(ninsn=%d,addr=%#"PRIx64,ninsn,addr);
   if (addr) {
      //An address is given: positioning the cursor
      out = cursor_init(ed, NULL, addr, NULL);
   }
   if (!ISERROR(out)) {
      //No error occurred when positioning the cursor
      if (ed && ed->cursor) {
         mod = madras_delete_insn(ed, addr);
         int nbdel = 1;
         list_t* iter = ed->cursor->next;
         while (iter && nbdel < ninsn) {
            addr = insn_get_addr(GET_DATA_T(insn_t*, iter));
            madras_delete_insn(ed, addr);
            iter = iter->next;
            nbdel++;
         }
//         delete_t* del = delete_new();
//         //Adds the delete request in the list of modifications for the disassembled file
//         //The list is ordered with the lowest insertion addresses at the beginning
//         mod = modif_add(ed->patchfile,insn_get_addr((insn_t*)ed->cursor->data),ed->cursor,MODTYPE_DELETE,del);
      } else {
         WRNMSG(
               "Cursor instruction for disassembled file has not been initialized\n");
         ed->last_error_code = ERR_MADRAS_MISSING_CURSOR;
         mod = NULL;
      }
   } else
      ed->last_error_code = out;
//   TRACE_END(ed,mod,modif);
   return mod;
}

/*
 * Creates a new request for a deletion of an instruction
 * \param ed A pointer to the structure holding the disassembled file
 * \param addr Address of the instruction to be deleted, or 0 if the instruction pointed to by
 * the cursor is the first address to be deleted.  If an address is provided, the cursor will be positioned
 * at this address (equivalent to a call to \ref madras_init_cursor)
 * \return A pointer to the code modification object representing the deletion if the operation succeeded, null value otherwise
 * If an error occurred, the last error code in \c ed will have been filled
 * */
modif_t* madras_delete_insn(elfdis_t* ed, int64_t addr)
{
   /**\todo TODO (2014-12-23) Remove ninsn as a parameter. This will only ever target one instruction*/
   int out = EXIT_SUCCESS;
   modif_t* mod = NULL;
   TRACE(ed, "madras_delete_insn(addr=%#"PRIx64, addr);
   if (addr) {
      //An address is given: positioning the cursor
      out = cursor_init(ed, NULL, addr, NULL);
   }
   if (!ISERROR(out)) {
      //No error occurred when positioning the cursor
      if (ed && ed->cursor) {
//         delete_t* del = delete_new();
         //Adds the delete request in the list of modifications for the disassembled file
         //The list is ordered with the lowest insertion addresses at the beginning
         mod = modif_add(ed->patchfile,
               INSN_GET_ADDR((insn_t* )ed->cursor->data), ed->cursor,
               MODTYPE_DELETE, MODIFPOS_REPLACE);
         if (!mod)
            madras_transfer_last_error_code(ed,
                  patchfile_get_last_error_code(ed->patchfile),
                  ERR_MADRAS_MODIF_CODE_FAILED);
      } else {
         WRNMSG(
               "Cursor instruction for disassembled file has not been initialized\n");
         ed->last_error_code = ERR_MADRAS_MISSING_CURSOR;
      }
   } else
      ed->last_error_code = out;
   TRACE_END(ed, mod, modif);
   return mod;
}

/*
 * Creates a new request for the insertion of a function call. This function is identicall to \ref madras_fctcall_new except that
 * the inserted function call will not be surrounded by instructions for saving/restoring the context and aligning the stack.
 * Therefore, it is up to the inserted function to take care of those task if needed, otherwise the patched file may crash or
 * present an undefined behaviour.
 * \param ed A pointer to the structure holding the disassembled file
 * \param fctname Name of the function to which a call must be inserted
 * \param libname Name of the library containing the function. If not NULL, equivalent to adding
 * \e libname through \ref madras_extlib_add
 * \param addr Address at which the function call must be inserted, or -1 if the address of the instruction
 * pointed to by the cursor must be used. If an address is provided, the cursor will be positioned
 * at this address (equivalent to a call to \ref madras_init_cursor). If 0, the function call will be inserted in the file
 * but not linked to any address
 * \param pos Specifies where the instructions must be inserted relatively to the given address or instruction, using values from insertpos_t
 * \return A pointer to the code modification object representing the insertion request object or a NULL value if a failure occurred.
 * In that case, the last error code in \c ed will have been filled
 * */
modif_t* madras_fctcall_new_nowrap(elfdis_t* ed, char* fctname, char* libname,
      int64_t addr, insertpos_t pos)
{
   modif_t* out = NULL;
   TRACE(ed,
         "madras_fctcall_new_nowrap(fctname=%s,libname=%s,addr=%#"PRIx64",after=%d",
         fctname, libname, addr, pos);
   out = fctcall_new(ed, fctname, libname, addr, pos, FALSE, NULL, 0);
   TRACE_END(ed, out, modif);
   return out;
}

/*
 * Creates a new request for the insertion of a function in a file. The function is not necessarily called
 * \param ed A pointer to the structure holding the disassembled file
 * \param fctname Name of the function to insert
 * \param libname Name of the library containing the function.
 * - If not NULL, the library will be also added as with \ref madras_extlib_add.
 * - If NULL, this means the function is already present in the patched file.
 * \param fctcode Not implemented, reserved for future evolutions
 * \return A pointer to the code modification object representing the insertion if successful, NULL otherwise.
 * If an error occurred, the last error code in \c ed will have been filled
 * \warning This function is currently mainly intended for added function stubs needed by MIL. Its behaviour is undefined for many operations
 * */
modif_t* madras_fct_add(elfdis_t* ed, char* fctname, char* libname,
      char* fctcode)
{
   /**\todo Extend the possibilities of this function + check its behaviour for multiple uses
    * fctcode is supposed to hold the assembly code of the function to insert (if not NULL, overrides libname)*/
   modif_t* out = NULL;
   insfct_t* insfct;
   TRACE(ed, "madras_fct_add(fctname=%s,libname=%s,fctcode=%s", fctname,
         libname, fctcode);
   insfct = insfct_new(fctname, NULL, 0, NULL, NULL, 0);

   if (libname) {
      //Adds a request for insertion for  the library this function is defined in and retrieves the insertion library object
      modiflib_t* fctlib = add_extlib(ed->patchfile, libname, 0,
            multiple_disassembler);
      //Attaches the library object to the insertion function
      insfct->srclib = fctlib;
   }
   //Adds the insertion function call in the list of modifications for the disassembled file
   //The list is ordered with the lowest insertion addresses at the beginning, so using 0 ensures it will be performed first
   out = modif_add(ed->patchfile, 0, NULL, MODTYPE_INSERT, MODIFPOS_KEEP);
   if (!out)
      madras_transfer_last_error_code(ed,
            patchfile_get_last_error_code(ed->patchfile),
            ERR_MADRAS_MODIF_CODE_FAILED);
   else {
      //Sets the type of insert to specify that this is only the function, not the call
      out->flags |= flags_madras2patcher(PATCHOPT_FCTCALL_FCTONLY);
      out->fct = insfct;
   }
   TRACE_END(ed, out, modif);
   return out;
}

/*
 * Creates a new request for the insertion of a function call
 * \param ed A pointer to the structure holding the disassembled file
 * \param fctname Name of the function to which a call must be inserted
 * \param libname Name of the library containing the function.
 * - If not NULL, the library will be also added as with \ref madras_extlib_add.
 * - If NULL, this means the function is already present in the patched file.
 * \warning The behaviour of parameter \e libname is different from release 1.0
 * \param addr Address at which the function call must be inserted, or -1 if the address of the instruction
 * pointed to by the cursor must be used. If an address is provided, the cursor will be positionned
 * at this address (equivalent to a call to \ref madras_init_cursor). If 0, the function call will be inserted in the file
 * but not linked to any address
 * \param pos Specifies where the instructions must be inserted relatively to the given address or instruction, using values from insertpos_t
 * \return A pointer to the code modification object representing the insertion request object or NULL if a failure occurred
 * In that case, the last error code in \c ed will have been filled
 * */
modif_t* madras_fctcall_new(elfdis_t* ed, char* fctname, char* libname,
      int64_t addr, insertpos_t pos, reg_t** reglist, int nreg)
{
   modif_t* out = NULL;
   TRACE(ed,
         "madras_fctcall_new(fctname=%s,libname=%s,addr=%#"PRIx64",after=%d,reglist=",
         fctname, libname, addr, pos);
   if ((nreg > 0) && (reglist)) {
      int i;
      TRACE(ed, "{%s",
            arch_get_reg_name(asmfile_get_arch(ed->afile),
                  reg_get_type(reglist[0]), reg_get_name(reglist[0])));
      for (i = 1; i < nreg; i++) {
         TRACE(ed, ",%s",
               arch_get_reg_name(asmfile_get_arch(ed->afile),
                     reg_get_type(reglist[i]), reg_get_name(reglist[i])));
      }
      TRACE(ed, "},nreg=%d", nreg);
   } else {
      TRACE(ed, "%p,nreg=%d", reglist, nreg);
   }
   out = fctcall_new(ed, fctname, libname, addr, pos, TRUE, reglist, nreg);
   TRACE_END(ed, out, modif);
   return out;
}

/*
 * Adds a parameter, given in string format, to a function call request
 * \param ed A pointer to the structure holding the disassembled file
 * \param modif Pointer to the modification object representing the function call insertion request
 * \param param Parameter to add, given in the format used by the assembly code of the
 *          current architecture in AT&T format
 *          (e.g. for x86: \%RAX for a register, $42 for an immediate, 0x42(\%RAX) for memory)
 * \param opt Contains options for the parameter:
 *        - 'a' means the address pointed to by the memory operand must be taken as parameter
 *        - 'q', 'l', 'w' or 'b' means the value of the area pointed to by the memory
 *         operand must be taken as parameter, with the given size.
 *         In the current version, all of these values are treated as 'q' (quadword)
 * \note \e opt is only used for memory and register operands
 * \return EXIT_SUCCESS if the operation succeeded, error code otherwise
 * */
int madras_fctcall_addparam_fromstr(elfdis_t* ed, modif_t* modif, char* param,
      char opt)
{
   int out = EXIT_SUCCESS;
   if (ed == NULL)
      return ERR_MADRAS_MISSING_MADRAS_STRUCTURE;

   TRACE(ed,
         "madras_fctcall_addparam_fromstr(modif=%s%d,param=\"%s\",opt=%c)\n",
         (modif) ? "modif_" : "", MODIF_ID(modif), param, opt);
   if (!modif)
      return ERR_PATCH_MISSING_MODIF_STRUCTURE;
   if (modif->type == MODTYPE_INSERT && modif->fct) {
      int i = 0;
      //Parses the parameter depending on the target architecture
      oprnd_t* par = oprnd_parsenew(param, &i, asmfile_get_arch(ed->afile));
      if (par) {
         //Adds the parameter to the list of parameters for the function call
         out = fctcall_add_param(modif->fct, par, opt);
      } else {
         ERRMSG("Parameter unrecognized: %s\n", param);
         out = ERR_LIBASM_OPERAND_NOT_PARSED;
      }
   } else {
      ERRMSG("Modif %d is not an insert function call\n", modif->modif_id);
      out = ERR_PATCH_WRONG_MODIF_TYPE;
   }
   return out;
}

/*
 * Adds an immediate parameter to a function call request.
 * \param ed A pointer to the structure holding the disassembled file
 * \param modif Pointer to the modification object representing the function call insertion request
 * \param imm Value of the immediate
 * \param opt Contains options for the parameter.
 * \note In the current version, \e opt it is ignored (always as "\0")
 * \return EXIT_SUCCESS if the operation succeeded, error code otherwise
 * */
int madras_fctcall_addparam_imm(elfdis_t* ed, modif_t* modif, int64_t imm,
      char opt)
{
   int out = EXIT_SUCCESS;
   if (ed == NULL) {
      return ERR_MADRAS_MISSING_MADRAS_STRUCTURE;
   }

   TRACE(ed, "madras_fctcall_addparam_imm(modif=%s%d,imm=%#"PRIx64",opt=%d)\n",
         (modif) ? "modif_" : "", MODIF_ID(modif), imm, opt);
   if (!modif)
      return ERR_PATCH_MISSING_MODIF_STRUCTURE;
   if (modif->type == MODTYPE_INSERT && modif->fct) {
      //Creates the immediate parameter
      oprnd_t* oprnd = oprnd_new_imm(imm);
      if (oprnd) {
         //Adds the parameter to the list of parameters for the function call
         out = fctcall_add_param(modif->fct, oprnd, opt);
      } else {
         ERRMSG("Unable to create immediate parameter with value: %#"PRIx64"\n",
               imm);
         out = ERR_LIBASM_OPERAND_NOT_CREATED;
      }
   } else {
      ERRMSG("Modif %d is not an insert function call\n", modif->modif_id);
      out = ERR_PATCH_WRONG_MODIF_TYPE;
   }
   return out;
}

/*
 * Adds a parameter, taken from the instruction the cursor points to or at a given address,
 * to a function call request
 * \param ed A pointer to the structure holding the disassembled file
 * \param modif Pointer to the modification object representing the function call insertion request
 * \param idx Index of the parameter to add (starting at 0)
 * \param opt Contains options for the parameter:
 *        - 'a' means the address pointed to by the memory operand must be taken as parameter
 *        - 'q', 'l', 'w' or 'b' means the value of the area pointed to by the memory
 *         operand must be taken as parameter, with the given size
 *         In the current version, all of these values are treated as 'q' (quadword)
 * \param addr Address of the instruction from which the parameter must be taken, or 0 if the instruction
 * pointed to by the cursor must be used. If an address is provided, the cursor will be positioned
 * at this address (equivalent to a call to \ref madras_init_cursor)
 * \note So far \e opt it is used only for memory operands
 * \return EXIT_SUCCESS if the operation succeeded, error code otherwise
 * */
int madras_fctcall_addparam_frominsn(elfdis_t* ed, modif_t* modif, int idx,
      char opt, int64_t addr)
{
   int out = EXIT_SUCCESS;
   oprnd_t* oprnd = NULL;
   TRACE(ed,
         "madras_fctcall_addparam_frominsn(modif=%s%d,idx=%d,opt=%c,addr=%#"PRIx64")\n",
         (modif) ? "modif_" : "", MODIF_ID(modif), idx, opt, addr);
   if (!modif)
      return ERR_PATCH_MISSING_MODIF_STRUCTURE;
   if (modif->type == MODTYPE_INSERT && modif->fct) {
      if (addr) {
         //Address provided: positioning the cursor
         out = cursor_init(ed, NULL, addr, NULL);
      }
      if (!ISERROR(out)) {
         //Check if no error occurred so far
         if (ed && ed->cursor) {
            //Retrieves the parameter
            arch_t* cursor_arch = insn_get_arch((insn_t*) ed->cursor->data);
            oprnd = cursor_arch->oprnd_copy(insn_get_oprnd((insn_t*) ed->cursor->data, idx));
         } else {
            ERRMSG(
                  "Cursor instruction for disassembled file has not been initialized\n");
            out = ERR_MADRAS_MISSING_CURSOR;
         }
         if (oprnd) {
            //Adds the parameter to the list of parameters for the function call
            out = fctcall_add_param(modif->fct, oprnd, opt);
            //Checks if this operand points to a global variable
            /**\todo Find a better way to handle global variables (those existing and those having to be created)*/
            if (oprnd_is_mem(oprnd) == TRUE) {
               /**\todo TODO (2014-11-12) Remove this, and update the copy of the instruction to retrieve the data_t
                * Anyway this will never be executed as an operand referencing a data_t is not of type OT_MEMORY, but OT_MEMORY_RELATIVE
                * (2014-11-18) => Actually I think it's already done (check how pointers are copied)
                * However, we DO need to retrieve the pointed data and use it in the globvar (because parameters use globvar_t structures)
                * => (2015-05-12) This is now done in patchfile_createpatchinsn, where we update the insnrefs table from the patched file.
                * I'm really not sure there is any reason left for storing the operand as a globvar here. If not, delete this block*/
//               int64_t varaddr = insn_checkrefs((insn_t*)ed->cursor->data, NULL);
//               if (varaddr >= 0 ) {
//                   DBGMSGLVL(1,"Parameter from instruction at address %#"PRIx64" references a global variable at address %#"PRIx64"\n",addr,varaddr);
//                  //The operand points to a global variable: we will create one with this offset
//                  globvar_t* gvext = globvar_new(ed->patchfile,VAR_EXIST,0,NULL);
//                  gvext->offset = varaddr;
               //This variable already exists and won't have to be created
//                  modifvars_add(ed->patchfile,NOUPDATE,gvext);//This is mainly to be able to free it
               //Links the global variable to the operand
//                  ifct->fct->paramvars[ifct->fct->nparams-1] = gvext;
//               }
            }
         } else {
            ERRMSG("Unable to retrieve parameter %d for current instruction\n",
                  idx);
            out = ERR_LIBASM_OPERAND_NOT_FOUND;
         }
      }
   } else {
      ERRMSG("Modif %d is not an insert function call\n", modif->modif_id);
      out = ERR_PATCH_WRONG_MODIF_TYPE;
   }
   return out;
}

/*
 * Adds a parameter to a function call equal to a pointer to a global variable.
 * \param ed A pointer to the structure holding the disassembled file
 * \param modif Pointer to the modification object representing the function call insertion request
 * \param gv Pointer to the global variable to use as parameter
 * \param str Pointer to a string to use for parameter (will be used only if \e gv is NULL)
 * \param opt Contains options for the parameter:
 *        - 'a' means the address of the global variable must be taken as parameter
 *        - 'q', 'l', 'w' or 'b' means the value of the area pointed to by the global variable
 *        must be taken as parameter, with the given size.
 *        In the current version, all of these values are treated as 'q' (quadword)
 *        This parameter is ignored if the \e str parameter is used
 * \return EXIT_SUCCESS if the operation succeeded, error code otherwise
 */
int madras_fctcall_addparam_fromglobvar(elfdis_t* ed, modif_t* modif,
      globvar_t* gv, char* str, char opt)
{
   if (ed == NULL)
      return ERR_MADRAS_MISSING_CURSOR;

   int out = EXIT_SUCCESS;
   TRACE(ed,
         "madras_fctcall_addparam_fromglobvar(modif=%s%d,gv=%s%d,str=%s%s%s,opt=%c)\n",
         (modif) ? "modif_" : "", MODIF_ID(modif), (gv) ? "globvar_" : "",
         GLOBVAR_ID(gv), (str) ? "\"" : "", str, (str) ? "\"" : "",
         (opt) ? opt : '0');
   if (!modif)
      return ERR_PATCH_MISSING_MODIF_STRUCTURE;
   if (modif->type == MODTYPE_INSERT && modif->fct) {
      globvar_t* varop = NULL;
      char option;
      if (gv != NULL) {
         varop = gv;
         option = opt;
      } else if ((gv == NULL) && (str != NULL)) {
         varop = globvar_new(ed->patchfile, NULL, VAR_CREATED, strlen(str) + 1,
               str);
         modifvars_add(ed->patchfile, ADDGLOBVAR, varop);
         option = 'a';
      }
      if (varop != NULL) {
         oprnd_t* opvar = ed->patchfile->patchdriver->generate_oprnd_globvar(0);
         //Adds the parameter to the list of parameters for the function call
         out = fctcall_add_param(modif->fct, opvar, option);
         //Links the global variable to the operand
         modif->fct->paramvars[modif->fct->nparams - 1] = varop;
         /**\todo TODO (2015-05-20) ^Not sure that this is still necessary with the refactor => Yes I do so far, it's used in the <arch>_patcher.c*/
         //pointer_set_data_target(oprnd_get_ptr(ifct->fct->paramvars[ifct->fct->nparams-1]), gv->data);
      } else {
         ERRMSG("No global variable given for parameter\n");
         out = ERR_MADRAS_MISSING_GLOBVAR;
      }
   } else {
      ERRMSG("Modif %d is not an insert function call\n", modif->modif_id);
      out = ERR_PATCH_WRONG_MODIF_TYPE;
   }
   return out;
}

int madras_fctcall_addparam_fromtlsvar(elfdis_t* ed, modif_t* modif,
      tlsvar_t* tlsv, char* str, char opt)
{
   if (ed == NULL)
      return ERR_MADRAS_MISSING_CURSOR;
   HLTMSG("Insertion of TLS variables is disabled in this version\n")
#if 0
   int out = EXIT_SUCCESS;
   TRACE(ed,
         "madras_fctcall_addparam_fromtlsvar(modif=%s%d,tlsv=%s%d,str=%s%s%s,opt=%c)\n",
         (modif) ? "modif_" : "", MODIF_ID(modif), (tlsv) ? "tlsvar_" : "",
         TLSVAR_ID(tlsv), (str) ? "\"" : "", str, (str) ? "\"" : "",
         (opt) ? opt : '0');
   if (!modif)
   return ERR_PATCH_MISSING_MODIF_STRUCTURE;
   if (modif->type == MODTYPE_INSERT && modif->data.insert) {
      insert_t* ifct = modif->data.insert;
      tlsvar_t* varop = NULL;
      char option;
      if (tlsv != NULL) {
         varop = tlsv;
         option = opt;
      } else if ((tlsv == NULL) && (str != NULL)) {
         varop = tlsvar_new(ed->patchfile, VAR_CREATED, strlen(str) + 1, str);
         modifvars_add(ed->patchfile, ADDTLSVAR, varop);
         option = 'a';
      }
      if (varop != NULL) {
         oprnd_t* opvar = ed->patchfile->patchdriver->generate_oprnd_tlsvar(0);
         //Adds the parameter to the list of parameters for the function call
         out = fctcall_add_param(ifct->fct, opvar, option);
         //Links the global variable to the operand
         ifct->fct->paramtlsvars[ifct->fct->nparams - 1] = varop;
      } else {
         ERRMSG("No global variable given for parameter\n");
         out = ERR_MADRAS_MISSING_GLOBVAR;
      }
   } else {
      ERRMSG("Modif %d is not an insert function call\n", modif->modif_id);
      out = ERR_PATCH_WRONG_MODIF_TYPE;
   }
   return out;
#endif
   return FALSE;
}

/*
 * Adds a return value to a function call request. The return value will be copied to a global variable
 * \param ed A pointer to the structure holding the disassembled file
 * \param modif Pointer to the modification object representing the function call insertion request
 * \param ret Pointer to the global variable object, which must already be initialised
 * \return EXIT_SUCCESS if the operation succeeded, error code otherwise
 * */
int madras_fctcall_addreturnval(elfdis_t* ed, modif_t* modif, globvar_t* ret)
{
   if (ed == NULL)
      return ERR_MADRAS_MISSING_CURSOR;

   int out = EXIT_SUCCESS;
   TRACE(ed, "madras_fctcall_addreturnval(modif=%s%d,ret=%s%d)\n",
         (modif) ? "modif_" : "", MODIF_ID(modif), (ret) ? "globvar_" : "",
         GLOBVAR_ID(ret));
   if (!modif)
      return ERR_PATCH_MISSING_MODIF_STRUCTURE;
   if (modif->type == MODTYPE_INSERT && modif->fct) {
      if (ret != NULL) {
         modif->fct->retvar = ret;
      } else {
         ERRMSG("No global variable given for parameter\n");
         out = ERR_MADRAS_MISSING_GLOBVAR;
      }
   } else {
      ERRMSG("Modif %d is not an insert function call\n", modif->modif_id);
      out = ERR_PATCH_WRONG_MODIF_TYPE;
   }
   return out;
}

/*
 * Adds a return value to a function call request. The return value will be copied to a tls variable
 * \param ed A pointer to the structure holding the disassembled file
 * \param modif Pointer to the modification object representing the function call insertion request
 * \param ret Pointer to the tls variable object, which must already be initialised
 * \return EXIT_SUCCESS if the operation succeeded, error code otherwise
 * */
int madras_fctcall_addreturntlsval(elfdis_t* ed, modif_t* modif, tlsvar_t* ret)
{
   if (ed == NULL)
      return ERR_MADRAS_MISSING_CURSOR;

   int out = EXIT_SUCCESS;
   TRACE(ed, "madras_fctcall_addreturntlsval(modif=%s%d,ret=%s%d)\n",
         (modif) ? "modif_" : "", MODIF_ID(modif), (ret) ? "tlsvar_" : "",
         TLSVAR_ID(ret));
   if (!modif)
      return ERR_PATCH_MISSING_MODIF_STRUCTURE;
   if (modif->type == MODTYPE_INSERT && modif->fct) {
      if (ret != NULL) {
         modif->fct->rettlsvar = ret;
      } else {
         ERRMSG("No tls variable given for parameter\n");
         out = ERR_MADRAS_MISSING_GLOBVAR;
      }
   } else {
      ERRMSG("Modif %d is not an insert function call\n", modif->modif_id);
      out = ERR_PATCH_WRONG_MODIF_TYPE;
   }
   return out;
}

/*
 * Inserts an unconditional branch in the code to an existing address or another modification
 * \param ed A pointer to the MADRAS structure
 * \param addr The address where to insert the branch or -1 if the address of the instruction
 * pointed to by the cursor must be used. If 0, the branch will be inserted in the file but not linked to any address
 * \param pos Specifies where the instructions must be inserted relatively to the given address or instruction, using values from insertpos_t
 * \param modif An existing modification to point to (only MODTYPE_INSERT modifications are allowed)
 * \param dstaddr An address in the file (take into account only if \c modif is NULL) to point to. If set to -1 and \c modif is NULL, a return
 * instruction will be inserted
 * \param upd_if_patched Conditions the way the branch will be updated if \c dstaddr is used and and insertion is performed before the instruction
 * at this address. If set to TRUE, the inserted branch will point to the code inserted before the instruction, otherwise it will always point
 * to the instruction that was present at \c dstaddr
 * \return A pointer to the modification, or a NULL if the operation failed. In that case, the last error code in \c ed will be updated
 * */
modif_t* madras_branch_insert(elfdis_t* ed, int64_t addr, insertpos_t pos,
      modif_t* modif, int64_t dstaddr, boolean_t upd_if_patched)
{
   if (!ed || !ed->patchfile) {
      madras_set_last_error_code(ed, ERR_PATCH_NOT_INITIALISED);
      return NULL;
   }
   TRACE(ed,
         "madras_branch_insert(addr=%#"PRIx64",after=%d,modif=%s%d,dstaddr=%#"PRIx64",upd_if_patched=%s",
         addr, pos, (modif) ? "modif_" : "", MODIF_ID(modif), dstaddr,
         (upd_if_patched == TRUE) ? "TRUE" : "FALSE");
   modif_t* out = NULL;
   queue_t* insbranch;
   insn_t* next = NULL;

   if ((modif) || (dstaddr > 0))
      insbranch = queue_new();
   else
      insbranch = ed->patchfile->patchdriver->generate_insnlist_return(NULL);

   if (dstaddr > 0) {
      next = asmfile_get_insn_by_addr(ed->afile, dstaddr);
      if (!next) {
         ERRMSG(
               "No instruction found at address %#"PRIx64" to insert a branch pointing to\n",
               dstaddr);
         ed->last_error_code = ERR_LIBASM_INSTRUCTION_NOT_FOUND;
         TRACE_END(ed, out, modif);
         return out;
      }
   }

   out = insns_add(ed, insbranch, addr, pos, (globvar_t**) -1, (tlsvar_t**) -1,
         FALSE);
   /*Passing -1 for the linked global variables avoid printing the variables in the trace*/

   /**\todo Factorise the following code with that of madras_modif_setnext (beware of upd_if_patched)*/
   if (out) {
      if (modif)
         out->nextmodif = modif;
      else {
         out->nextinsn = next;
         if (!upd_if_patched)
            out->flags |= flags_madras2patcher(PATCHOPT_BRANCHINS_NO_UPD_DST);
      }
   }
   TRACE_END(ed, out, modif);
   return out;
}

/*
 * Flags an instruction at a given address to be moved to the section of relocated code. No other modification is performed.
 * The size of the block moved around the instruction will depend on the flags set on the modif_t object.
 * \param ed A pointer to the structure holding the disassembled file
 * \param addr Address of the instruction to move, or 0 if the address of the instruction
 * pointed to by the cursor must be used
 * \return A pointer to the code modification object representing the forced relocation if the operation succeeded, NULL otherwise.
 * If an error occurred, the last error code in \c ed will be filled
 * */
modif_t* madras_relocate_insn(elfdis_t* ed, int64_t addr)
{
   if (ed == NULL)
      return NULL;

   if (ed->patchfile == NULL) {
      ERRMSG(
            "madras_relocate_insn invoked on a file not prepared for modification\n");
      ed->last_error_code = ERR_PATCH_NOT_INITIALISED;
      return NULL;
   }

   int out = 1;
   modif_t* mod = NULL;
   TRACE(ed, "madras_relocate_insn(addr=%#"PRIx64"", addr);

   if (addr) {
      //An address is given: positioning the cursor
      out = cursor_init(ed, NULL, addr, NULL);
   }
   if (!ISERROR(out)) {
      //No error occurred when positioning the cursor
      if (ed && ed->cursor) {
         //Adds the relacation request in the list of modifications for the disassembled file
         //The list is ordered with the lowest insertion addresses at the beginning
         mod = modif_add(ed->patchfile,
               INSN_GET_ADDR((insn_t* )ed->cursor->data), ed->cursor,
               MODTYPE_RELOCATE, MODIFPOS_KEEP);
         if (!mod)
            madras_transfer_last_error_code(ed,
                  patchfile_get_last_error_code(ed->patchfile),
                  ERR_MADRAS_MODIF_CODE_FAILED);
      } else {
         WRNMSG(
               "Cursor instruction for disassembled file has not been initialized\n");
         ed->last_error_code = ERR_MADRAS_MISSING_CURSOR;
      }
   } else
      ed->last_error_code = out;
   TRACE_END(ed, mod, modif);
   return mod;
}

/*
 * Creates a new condition
 * \param ed A pointer to the MADRAS structure
 * \param condtype Type of the condition
 * \param oprnd Operand whose value is needed for the comparison
 * \param condval Value to compare the operand to (for comparison conditions)
 * \param cond1 Sub-condition to use (for logical conditions)
 * \param cond2 Sub-condition to use (for logical conditions)
 * \return A pointer to the new condition
 * */
cond_t* madras_cond_new(elfdis_t* ed, int condtype, oprnd_t* oprnd,
      int64_t condval, cond_t* cond1, cond_t* cond2)
{

   char buf[32];
   oprnd_print(NULL, oprnd, buf, sizeof(buf), asmfile_get_arch(ed->afile));
   TRACE(ed,
         "madras_cond_new(condtype=%d,oprnd=%s,condval=%#"PRIx64",cond1=%s%d,cond2=%s%d",
         condtype, (oprnd)?buf:NULL, condval, (cond1) ? "cond_" : "",
         COND_ID(cond1), (cond2) ? "cond_" : "", COND_ID(cond2));
   int type;
   switch (condtype) {
   case LOGICAL_AND:
      type = COND_AND;
      break;
   case LOGICAL_OR:
      type = COND_OR;
      break;
   case COMP_EQUAL:
      type = COND_EQUAL;
      break;
   case COMP_NEQUAL:
      type = COND_NEQUAL;
      break;
   case COMP_LESS:
      type = COND_LESS;
      break;
   case COMP_GREATER:
      type = COND_GREATER;
      break;
   case COMP_EQUALLESS:
      type = COND_EQUALLESS;
      break;
   case COMP_EQUALGREATER:
      type = COND_EQUALGREATER;
      break;
   default:
      type = COND_VOID;
   }
   cond_t* out = cond_new(ed->patchfile, type, oprnd, condval, cond1, cond2);
   if (!out)
      madras_transfer_last_error_code(ed,
            patchfile_get_last_error_code(ed->patchfile),
            ERR_MADRAS_MODIF_ADD_COND_FAILED);
   TRACE_END(ed, out, cond);
   return out;
}

/*
 * Adds a condition to the execution of a modified code. If the condition is satisfied the modified code will be executed, otherwise the original code will be.
 * \warning The current version of this function only supports code insertion modifications. Use with deletion or modification is not supported
 * \param ed A pointer to the MADRAS structure
 * \param modif A pointer to the modification object to add the condition to
 * \param cond The condition to add
 * \param condtype If an existing condition was already present for this insertion, the new condition will be logically added to the existing one
 * If set to 0, LOGICAL_AND will be used
 * \return EXIT_SUCCESS if successful, error code otherwise
 * */
int madras_modif_addcond(elfdis_t* ed, modif_t* modif, cond_t* cond,
      int condtype)
{

   if (!ed) {
      ERRMSG("Unable to add new condition to insertion: file is NULL\n");
      return ERR_MADRAS_MISSING_MADRAS_STRUCTURE;
   }
   TRACE(ed, "madras_modif_addcond(modif=%s%d,cond=%s%d,condtype=%d)\n",
         (modif) ? "modif_" : "", MODIF_ID(modif), (cond) ? "cond_" : "",
         COND_ID(cond), condtype);
   if (!ed->patchfile) {
      ERRMSG(
            "Unable to add new condition to insertion: file is not prepared for modification\n");
      return ERR_PATCH_NOT_INITIALISED;
   }
   return modif_addcond(ed->patchfile, modif, cond, NULL, condtype, NULL);
}

/*
 * Adds a condition from its string representation to the execution of a modified code. See \ref madras_modif_addcond for precisions on conditions
 * \warning The current version of this function only supports code insertion modifications. Use with deletion or modification is not supported
 * \param ed A pointer to the MADRAS structure
 * \param modif A pointer to the modification object to add the condition to
 * \param strcond The string representation of the condition. Conditions must obey the following syntax:
 * - A condition must be enclosed by parenthesis ( '(' and ')' )
 * - A condition is either written as: ( oprnd comparison value ) or ( condition1 logical condition2 )
 * -- oprnd is a register or memory assembly operand, enclosed by quotes ( '"' )
 * -- comparison is one of the following comparison operators: ==, !=, <, >, <=, >=
 * -- value is a numerical value
 * -- condition1 and condition2 are other conditions (obeying the same syntax)
 * -- logical is one of the following logical operators: &&, ||
 * -- Spaces can be present between the various elements of a comparison
 * \param gvars Array of added global variables to be used in the conditions. \note NOT USED IN THIS VERSION
 * \return EXIT_SUCCESS if successful, error code otherwise
 * */
int madras_modif_setcond_fromstr(elfdis_t* ed, modif_t* modif, char* strcond,
      globvar_t** gvars)
{
   if (!ed) {
      ERRMSG("Unable to add new condition to insertion: file is NULL\n");
      return ERR_MADRAS_MISSING_MADRAS_STRUCTURE;
   }
   TRACE(ed,
         "madras_modif_addcond_fromstr(modif=%s%d,strcond=\"%s\",gvars=%p)\n",
         (modif) ? "modif_" : "", MODIF_ID(modif), strcond, gvars);
   if (!ed->patchfile) {
      ERRMSG(
            "Unable to add new condition to insertion: file is not prepared for modification\n");
      return ERR_PATCH_NOT_INITIALISED;
   }
   return modif_addcond(ed->patchfile, modif, NULL, strcond, 0, NULL);
}

/*
 * Adds an option flag to an existing modification
 * \param ed The madras structure
 * \param modif The modification to update
 * \param opt The flag corresponding to the option to add to the modification
 * \return EXIT_SUCCESS if the operation succeeded, error code otherwise
 * */
int madras_modif_addopt(elfdis_t* ed, modif_t* modif, int opt)
{
   if (!ed)
      return ERR_MADRAS_MISSING_MADRAS_STRUCTURE;
   if (!modif)
      return ERR_PATCH_MISSING_MODIF_STRUCTURE;
   /**\todo Check that the modification is actually in the list of modifications for this file ?*/
   TRACE(ed, "madras_modif_addopt(modif=%s%d,opt=%x)\n",
         (modif) ? "modif_" : "", MODIF_ID(modif), opt);

   if (modif_isprocessed(modif) && opt == PATCHOPT_MODIF_FIXED) {
      //Special case: Modification was flagged as fixed and has been processed while the option to remove is the fixed flag: we forbid it
      WRNMSG(
            "Unable to add flag PATCHOPT_MODIF_FIXED to option %d: option has already been processed\n",
            modif->modif_id);
      return EXIT_FAILURE; /**\todo TODO (2015-09-09) Create an error code if we keep the mechanism of fixed modifications*/
   }

   modif->flags |= flags_madras2patcher(opt);

   return EXIT_SUCCESS;
}

/*
 * Removes an option flag to an existing modification
 * \param ed The madras structure
 * \param modif The modification to update
 * \param opt The flag corresponding to the option to remove to the modification
 * \return EXIT_SUCCESS if the operation succeeded, error code  otherwise
 * */
int madras_modif_remopt(elfdis_t* ed, modif_t* modif, int opt)
{
   if (!ed)
      return ERR_MADRAS_MISSING_MADRAS_STRUCTURE;
   if (!modif)
      return ERR_PATCH_MISSING_MODIF_STRUCTURE;
   /**\todo Check that the modification is actually in the list of modifications for this file ?*/
   TRACE(ed, "madras_modif_remopt(modif=%s%d,opt=%x)\n",
         (modif) ? "modif_" : "", MODIF_ID(modif), opt);

   if (modif_isfixed(modif) && opt == PATCHOPT_MODIF_FIXED) {
      //Special case: Modification was flagged as fixed and has been processed while the option to remove is the fixed flag: we forbid it
      WRNMSG(
            "Unable to remove flag PATCHOPT_MODIF_FIXED from option %d: option has already been processed\n",
            modif->modif_id);
      return FALSE;
   }

   modif->flags &= ~(flags_madras2patcher(opt));

   return EXIT_SUCCESS;
}

/*
 * Adds a modification to perform if the modification made on a condition is not met. This has no effect if the modification has no condition
 * \note In the current version only modifications of type MODTYPE_INSERT are supported for \c modif and \c elsemod
 * \param ed The madras structure
 * \param modif The modification for which we want to add an else condition
 * \param elsemod The modification to execute if the condition is not met
 * \return EXIT_SUCCESS if the operation succeeded, error code otherwise
 * */
int madras_modif_addelse(elfdis_t* ed, modif_t* modif, modif_t* elsemod)
{
   if (!ed)
      return ERR_MADRAS_MISSING_MADRAS_STRUCTURE;
   if (!modif)
      return ERR_PATCH_MISSING_MODIF_STRUCTURE;
   TRACE(ed, "madras_modif_addelse(modif=%s%d,elsemod=%s%d)\n",
         (modif) ? "modif_" : "", MODIF_ID(modif), (elsemod) ? "modif_" : "",
         MODIF_ID(elsemod));
   if (!modif->condition) {
      ERRMSG(
            "Attempted to add \"else\" code to modification %d that has no condition\n",
            MODIF_ID(modif));
      return ERR_MADRAS_MODIF_COND_MISSING;
   }
   if (modif->condition->elsemodif != NULL) {
      ERRMSG(
            "Attempted to add \"else\" code to modification %d that already has such a code already set\n",
            MODIF_ID(modif));
      return ERR_MADRAS_MODIF_ALREADY_HAS_ELSE;
   }
   /*Checking that \c elsemod is a floating modification ?*/
   if (elsemod->addr != 0) {
      ERRMSG(
            "Attempted to add \"else\" code from non-floating modification %d (address not 0)\n",
            MODIF_ID(elsemod));
      return ERR_MADRAS_ELSE_MODIF_IS_FIXED;
   }
   modif->condition->elsemodif = elsemod;
   /*Setting A_MODIF_ATTACHED to avoid having the elsemodif be detected as an orphan if it is a floating one (and it should always be), and
    * A_MODIF_ISELSE to store the fact that it is an else modification (needed for special handling of the save/restore context instructions) */
   elsemod->annotate |= A_MODIF_ATTACHED;
   //Flags the modification and all its successors as being else modifications
   modif_annotate_else(elsemod);

   return EXIT_SUCCESS;
}

/*
 * Links a code modification to another or an address. The control flow in the patched file will jump to the beginning
 * of the modification or the address in the original code after having executed the first modification, regardless of where
 * it was supposed to continue to.
 * \note In the current version only modifications of type MODTYPE_INSERT are supported for \c modif and \c modln.
 * \param ed The madras structure
 * \param modif The modification we want to link to another
 * \param modln Another modification to link to. The code from this modification will be executed right after this one.
 * \param addrln If \c modln is NULL, the address in the original code to link to.
 * \return EXIT_SUCCESS if the operation succeeded, error code otherwise
 * */
int madras_modif_setnext(elfdis_t* ed, modif_t* modif, modif_t* modln,
      int64_t addrln)
{
   if (!ed)
      return ERR_MADRAS_MISSING_MADRAS_STRUCTURE;
   if (!modif)
      return ERR_PATCH_MISSING_MODIF_STRUCTURE;
   if (!modln && addrln <= 0)
      return ERR_COMMON_PARAMETER_MISSING;
   TRACE(ed, "madras_modif_setnext(modif=%s%d,modln=%s%d,addrln=%#"PRIx64")\n",
         (modif) ? "modif_" : "", MODIF_ID(modif), (modln) ? "modif_" : "",
         MODIF_ID(modln), addrln);
   if ((modif->type != MODTYPE_INSERT)
         || ((modln) && (modln->type != MODTYPE_INSERT))) {
      ERRMSG(
            "Attempted to enforce next modification to a non-insert modification (not supported yet). Operation not performed\n");
      return ERR_MADRAS_MODIF_TYPE_NOT_SUPPORTED;
   }

   if (modln != NULL) {
      modif_t* curr = modif;
      modif_t* next = modif->nextmodif;
      //If \c modif already had an enforced next modification, follows the chain until finding a modification without enforced follower
      while (next != NULL) {
         curr = next;
         next = next->nextmodif;
      }

      curr->nextmodif = modln;
      /*If the modification was an ELSE modification, we have to flag the modification we add as next and all its successors
       * as ELSE modifications themselves*/
      if (modif->annotate & A_MODIF_ISELSE)
         modif_annotate_else(modln);
   } else {
      insn_t* next = asmfile_get_insn_by_addr(ed->afile, addrln);
      if (!next) {
         ERRMSG(
               "No instruction found at address %#"PRIx64" to link modif %d to\n",
               addrln, MODIF_ID(modif));
         return ERR_LIBASM_INSTRUCTION_NOT_FOUND;
      }
      modif->nextinsn = next;
   }
   return EXIT_SUCCESS;

}

/*
 * Force the padding instruction to be used for a given modification. This overrides the instruction chosen for the patching
 * sessions (the \c nop instruction by default, or the instruction provided by \ref madras_modifs_setpaddinginsn).
 * If an instruction larger than this \c nop instruction is provided, an error will be returned
 * \param ed Pointer to the madras structure
 * \param modif The modification for which we want to update the padding type
 * \param insn Pointer to a structure representing the instruction to use
 * \param strinsn String representation of the assembly code for the instruction (used if \c in is NULL)
 * \return EXIT_SUCCESS if successful, error code otherwise
 * \todo Define this function in libmpatch and invoke it here
 * */
int madras_modif_setpaddinginsn(elfdis_t* ed, modif_t* modif, insn_t* insn,
      char* strinsn)
{
   if (!ed)
      return ERR_MADRAS_MISSING_MADRAS_STRUCTURE;
   if (!modif)
      return ERR_PATCH_MISSING_MODIF_STRUCTURE;
   if (!ed->patchfile)
      return ERR_PATCH_NOT_INITIALISED;
   char buf[64];
   insn_print(insn, buf, sizeof(buf));
   TRACE(ed, "madras_modif_setpaddinginsn(modif=%s%d,insn=%s,strinsn=%s)\n",
         (modif) ? "modif_" : "", MODIF_ID(modif), (insn)?buf:NULL, strinsn);
   if (modif->paddinginsn != NULL) {
      ERRMSG(
            "Attempted to set custom padding instruction for modification %d, which already has a custom padding instruction\n",
            MODIF_ID(modif));
      return ERR_MADRAS_MODIF_HAS_CUSTOM_PADDING;
   }
   insn_t* newpaddinginsn = NULL;
   if (insn != NULL)
      newpaddinginsn = insn_copy(insn);
   else {
      newpaddinginsn = insn_parsenew(strinsn, asmfile_get_arch(ed->afile));
      if (!newpaddinginsn) {
         ERRMSG(
               "Unable to parse instruction \"%s\" to set as padding for modification %d\n",
               strinsn, MODIF_ID(modif));
         return ERR_LIBASM_INSTRUCTION_NOT_PARSED;

      }
      if (assemble_insn(newpaddinginsn,
            ed->patchfile->asmbldriver) != EXIT_SUCCESS) {
         ERRMSG(
               "Unable to assemble instruction \"%s\" to set as padding for modification %d\n",
               strinsn, MODIF_ID(modif));
         return ERR_ASMBL_INSTRUCTION_NOT_ASSEMBLED;
      }
   }

   if (insn_get_size(newpaddinginsn)
         > insn_get_size(ed->patchfile->paddinginsn)) {
      char buf1[128], buf2[128];
      insn_print(newpaddinginsn, buf1, sizeof(buf1));
      insn_print(ed->patchfile->paddinginsn, buf2, sizeof(buf2));
      ERRMSG(
            "Instruction %s provided as new padding instruction for modification %d is larger than current instruction %s. " "Update canceled\n",
            buf1, MODIF_ID(modif), buf2);
      return ERR_PATCH_PADDING_INSN_TOO_BIG;
   }

   modif->paddinginsn = newpaddinginsn;
   return EXIT_SUCCESS;
}

/*
 * Adds a request to a new global variable insertion into the file
 * \param ed The madras structure
 * \param size The size in bytes of the global variable
 * \param value A pointer to the value of the global variable (if NULL, will be filled with 0)
 * \return A pointer to the object defining the global variable or NULL if \c ed is NULL
 * \note You may notice that I'm completely copying the mechanism of a modification for global variables.
 * This is because I'm anticipating the fact that, at some point in the future, we may want to add the
 * possibility to modify existing variables or reuse them. Of course, it may also never happen and this
 * cumbersome mechanism of modification type set to ADDGLOBVAR in all cases will be useless. This is life
 * */
globvar_t* madras_globalvar_new(elfdis_t* ed, int size, void* value)
{
   /**\todo TODO (2015-05-13) Add the name as parameter of this function + propagate wherever it is called*/
   globvar_t* out = NULL;
   if (ed == NULL) {
      return NULL;
   }

   TRACE(ed, "madras_globalvar_new(size=%d,value=%p", size, value);

   out = globvar_new(ed->patchfile, NULL, VAR_CREATED, size, value);
   modifvars_add(ed->patchfile, ADDGLOBVAR, out);

   TRACE_END(ed, out, globvar);
   return out;
}

/*
 * Adds a request to a new tls variable insertion into the file
 * \param ed The madras structure
 * \param size The size in bytes of the tls variable or NULL if \c ed is NULL
 * \param value A pointer to the value of the tls variable (if NULL, will be filled with 0)
 * \param type The type of the tls variable (INITIALIZED or UNINITIALIZED)
 * \return A pointer to the object defining the tls variable
 */
tlsvar_t* madras_tlsvar_new(elfdis_t* ed, int size, void* value, int type)
{
   tlsvar_t* out = NULL;
   if (ed == NULL)
      return NULL;

   TRACE(ed, "madras_tlsvar_new(size=%d,value=%p", size, value);

   out = tlsvar_new(ed->patchfile, type, size, value);
   modifvars_add(ed->patchfile, ADDTLSVAR, out);

   TRACE_END(ed, out, tlsvar);
   return out;
}

/*
 * Updates the value of a global variable. This does not allow to change its size.
 * This function can be invoked after the invocation of madras_modifs_precommit
 * \param ed The madras structure
 * \param gv The global variable
 * \param value The new value of the variable
 * \return EXIT_SUCCESS if the operation is successful, error code otherwise
 * */
int madras_globvar_updatevalue(elfdis_t* ed, globvar_t* gv, void* value)
{
   if (!ed)
      return ERR_MADRAS_MISSING_MADRAS_STRUCTURE;
   if (!gv) {
      ERRMSG(
            "madras_globvar_updatevalue invoked with NULL variable: no update performed\n");
      return ERR_MADRAS_MISSING_GLOBVAR;
   }
   TRACE(ed, "madras_globvar_updatevalue(gv=%s%d,value=%p)\n",
         (gv) ? "globvar_" : "", GLOBVAR_ID(gv), value);
   return patchfile_patch_updatedata(ed->patchfile, gv, value);
}

/*
 * Adds a request for inserting label into the file
 * \param ed The madras structure
 * \param lblname The name of the label to add
 * \param addr The address at which the label must be added. If set to -1, the current cursor will be used
 * \param lbltype The type of the label to add (FUNCTION, NONE or DUMMY)
 * \param fixed If set to TRUE, the label will be inserted at the given address. If set to FALSE,
 * the label will be inserted at the closest possible location of instruction at adress \e addr, even if it was moved or deleted
 * \return EXIT_SUCCESS if operation succeeded, error code otherwise
 * */
int madras_label_add(elfdis_t* ed, char* lblname, int64_t addr, int lbltype,
      int fixed)
{
   /**\todo TODO (2014-09-30) Change this function to use the labels types defined in libmasm.h for lbltype*/
   if (!ed)
      return ERR_MADRAS_MISSING_MADRAS_STRUCTURE;

   int out = EXIT_SUCCESS;
   TRACE(ed,
         "madras_label_add(lblname=%s,add=%#"PRIx64",lbltype=%d,fixed=%s)\n",
         lblname, addr, lbltype, (fixed == TRUE) ? "TRUE" : "FALSE");
   if (addr >= 0) {
      //An address is given: positioning the cursor
      out = cursor_init(ed, NULL, addr, NULL);
   }
   if (!ISERROR(out)) {
      //No error occurred when positioning the cursor
      if (ed->cursor) {
         /**\todo Add some coherence tests regarding addr and the cursor*/
         modiflbl_t* modif = modiflbl_new(addr, lblname, lbltype,
               (fixed == FALSE) ? ed->cursor : NULL, NULL, NEWLABEL);
         if (modif)
            queue_add_tail(ed->patchfile->modifs_lbl, modif);
         else
            out = ERR_MADRAS_MODIF_LABEL_FAILED;
      } else {
         WRNMSG(
               "Cursor instruction for disassembled file has not been initialized\n");
         out = ERR_MADRAS_MISSING_CURSOR;
      }
   }
   return out;
}

/*
 * Adds a request for inserting label into the file
 * \param ed The madras structure
 * \param lblname The name of the label to add
 * \param list An instruction list where the label must be added.
 * \param lbltype The type of the label to add (FUNCTION, NONE or DUMMY)
 * the label will be inserted at the closest possible location of instruction at adress \e addr, even if it was moved or deleted
 * \return EXIT_SUCCESS if operation succeeded, error code otherwise
 * */
int madras_label_add_to_insnlist(elfdis_t* ed, char* lblname, list_t* list,
      int lbltype)
{
   if (!ed)
      return ERR_MADRAS_MISSING_MADRAS_STRUCTURE;
   if (list == NULL)
      return ERR_COMMON_PARAMETER_MISSING;

//   TRACE(ed,"madras_label_add_to_insnlist(lblname=%s,add=%#"PRIx64",lbltype=%d,fixed=%s)\n",lblname,addr,lbltype,(fixed==TRUE)?"TRUE":"FALSE");
   queue_add_tail(ed->patchfile->modifs_lbl,
         modiflbl_new(0, lblname, lbltype, list, NULL, NEWLABEL));
   return EXIT_SUCCESS;
}

/*
 * Changes the OS the binary is computed for
 * \param ed The madras structure
 * \param code The code of the new targeted OS. Values are defined
 *             by macros ELFOSABI_... in elf.h file
 * \return EXIT_SUCCESS if operation succeeded, error code otherwise
 */
int madras_change_OSABI(elfdis_t* ed, char code)
{
   if (ed == NULL)
      return ERR_MADRAS_MISSING_MADRAS_STRUCTURE;
   return (patchfile_patch_changeOSABI(ed->patchfile, code));
}

/*
 * Change the targeted machine the binary is compute for
 * \param ed The madras structure
 * \param machine The new value of the targeted machine, defined by macros EM_...
 *        in elf.h file
 * \return EXIT_SUCCESS if operation succeeded, error code otherwise
 */
int madras_change_ELF_machine(elfdis_t* ed, int ELF_machine_code)
{
   if (ed == NULL)
      return ERR_MADRAS_MISSING_MADRAS_STRUCTURE;
   return patchfile_patch_changemachine(ed->patchfile, ELF_machine_code);
}

insn_t* madras_generate_NOP(elfdis_t* ed, int size)
{
   if (ed == NULL)
      return NULL;
   if (!ed->patchfile) {
      ed->last_error_code = ERR_PATCH_NOT_INITIALISED;
      return NULL;
   }
   if (size <= 0) {
      ed->last_error_code = ERR_COMMON_PARAMETER_INVALID;
      return NULL;
   }
   return (ed->patchfile->patchdriver->generate_insn_nop(size));
}

/*
 * Retrieves a queue of madras_addr_t structures, containing the correspondence between addresses in the original file and
 * in the patched file. This function must be invoked after madras_modif_precommit, and the PATCHOPT_TRACK_ADDRESSES option
 * must have been set beforehand.
 * \param modifonly If set to TRUE, the returned list will only contain modified addresses, otherwise the full list of addresses
 * will be returned
 * \return Queue of madras_addr_t structures, containing an element per instruction in the original file if modifonly is FALSE,
 * or one element per modified address if modifonly is TRUE, and ordered by increasing original address,
 * or NULL if the PATCHOPT_TRACK_ADDRESSES option was not set.
 * \warning This function may display undefined behaviour and possibly cause a crash if the patching session involved invocations
 * of \ref madras_replace_insns or madras_modify_insn
 * */
queue_t* madras_getnewaddresses(elfdis_t* ed, int modifonly)
{
   if (!ed || !ed->patchfile) {
      madras_set_last_error_code(ed, ERR_PATCH_NOT_INITIALISED);
      return NULL;
   }
   if (!ed->patchfile->insnaddrs) {
      ed->last_error_code = ERR_MADRAS_ADDRESSES_NOT_TRACKED;
      return NULL;
   }
   queue_t* addrs = queue_new();

   //Scans the list of insnaddrs instructions and builds the queue
   FOREACH_INQUEUE(ed->patchfile->insnaddrs, iter) {
      insnaddr_t* insnaddr = GET_DATA_T(insnaddr_t*, iter);
      if (!modifonly || (insnaddr->addr != INSN_GET_ADDR(insnaddr->insn))) {
         madras_addr_t* addr = lc_malloc(sizeof(*addr));
         addr->oldaddr = insnaddr->addr;
         if (insn_check_annotate(insnaddr->insn, A_PATCHDEL))
            addr->newaddr = -1; //Instruction has been deleted
         else
            addr->newaddr = INSN_GET_ADDR(insnaddr->insn);
         queue_add_tail(addrs, addr);
      }
   }
   return addrs;
}

/*
 * Frees a queue of madras_addr_t structures, such as returned by madras_getnewaddresses
 * \param madras_addrs A queue of madras_addr_t structures
 * */
void madras_newaddresses_free(queue_t* madras_addrs)
{
   if (!madras_addrs)
      return;
   queue_free(madras_addrs, lc_free);
}

/*
 * Commits the modifications made to a disassembled file and saves the result to another file
 * All pending modification requests will be committed, then flushed.
 * \param ed A pointer to the structure holding the disassembled file
 * \param newfilename Name of the file under which the modified file must be saved
 * \return EXIT_SUCCESS if successful or error code otherwise
 * */
int madras_modifs_commit(elfdis_t* ed, char* newfilename)
{
   if (ed == NULL)
      return ERR_MADRAS_MISSING_MADRAS_STRUCTURE;
   if (newfilename == NULL)
      return ERR_COMMON_FILE_NAME_MISSING;

   int out = EXIT_SUCCESS;

   TRACE(ed, "madras_modifs_commit(newfilename=%s)\n", newfilename);
   if (ed && ed->loginfo->trace)
      fflush(ed->loginfo->tracestream); //Enforcing most of the trace file is written in case something goes wrong during patching
   if (ed->patchfile != NULL) {
      //Updates global patching options
      ed->patchfile->flags = flags_madras2patcher(ed->options);

      out = patchfile_finalise(ed->patchfile, newfilename);
      if (!ISERROR(out))
         out = patchfile_patch_write(ed->patchfile);

      //Reinitializes the structure from the original elffile
      elfdis_refresh(ed);
      /**\bug Because of the problem with the reinit, there is a memory leak here*/
      //Flushes the pending changes now they have been made
      modifs_free(ed);
      ed->patchfile = NULL;
   } else {
      ERRMSG(
            "madras_modifs_commit invoked on a file with no pending modification\n");
      out = ERR_PATCH_NOT_INITIALISED;
   }

   return out;
}

/*
 * Commits the modifications made to a disassembled file without writing the patched file
 * All pending modification requests will be committed.
 * \param ed A pointer to the structure holding the disassembled file
 * \param newfilename Name of the file under which the modified file must be saved
 * \return EXIT_SUCCESS if successful or error code otherwise
 * */
int madras_modifs_precommit(elfdis_t* ed, char* newfilename)
{
   if (ed == NULL)
      return ERR_MADRAS_MISSING_MADRAS_STRUCTURE;
   if (newfilename == NULL)
      return ERR_COMMON_FILE_NAME_MISSING;
   int out = EXIT_SUCCESS;

   TRACE(ed, "madras_modifs_precommit(newfilename=%s)\n", newfilename);
   if (ed->patchfile != NULL) {
      //Updates global patching options
      ed->patchfile->flags = flags_madras2patcher(ed->options);

      //Initialises the tracking of addresses if it was requested
      if (ed->options & PATCHOPT_TRACK_ADDRESSES)
         patchfile_trackaddresses(ed->patchfile);
      //Patches the file without writing it
      out = patchfile_finalise(ed->patchfile, newfilename);

   } else {
      ERRMSG(
            "madras_modifs_precommit invoked on a file with no pending modification\n");
      out = ERR_PATCH_NOT_INITIALISED;
   }

   return out;
}

/*
 * Writes a patched file whose modifications have already been committed
 * All pending modification requests will be flushed.
 * \param ed A pointer to the structure holding the disassembled file
 * \return EXIT_SUCCESS if successful or EXIT_FAILURE or error code otherwise
 * */
int madras_modifs_write(elfdis_t* ed)
{
   if (ed == NULL)
      return ERR_MADRAS_MISSING_MADRAS_STRUCTURE;

   int out = EXIT_SUCCESS;

   TRACE(ed, "madras_modifs_write()\n");
   if (ed->patchfile != NULL) {
      out = patchfile_patch_write(ed->patchfile);

      //Reinitializes the structure from the original elffile
      elfdis_refresh(ed);
      /**\bug Because of the problem with the reinit, there is a memory leak here*/
      //Flushes the pending changes now they have been made
      modifs_free(ed);
      ed->patchfile = NULL;
   } else {
      ERRMSG(
            "madras_modifs_write invoked on a file with no pending modification\n");
      out = ERR_PATCH_NOT_INITIALISED;
   }

   return out;
}

/*
 * Frees the elfdis structure, and closes the associated file
 * \param ed A pointer to the structure holding the disassembled file
 * */
void madras_terminate(elfdis_t *ed)
{
   if (ed == NULL) {
      //error
      return;
   }

   /**\bug Because of the problem with the free of the functions for the elffile, a lot of the structures used are not
    * freed, which causes a a memory leak here*/
   madras_traceoff(ed, NULL);
   asmfile_unload_dbg(ed->afile);
   lc_free(ed->loginfo->tracefile);
   lc_free(ed->loginfo);
   lc_free(ed->name);
   //Frees any pending modif//
   if (ed->patchfile)
      modifs_free(ed);
   if (ed->loaded == FALSE) {
      asmfile_free(ed->afile);
   }
   lc_free(ed);
}
