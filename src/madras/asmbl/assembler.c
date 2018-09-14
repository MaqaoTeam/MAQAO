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
 * \file assembler.c
 * \brief This file contains all the functions needed to assemble 
 * ASM instructions into an ELF file
 * */

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <assert.h>
#include "libmcommon.h"
#include "libmasm.h"
#include "asmb_archinterface.h"
#include "assembler.h"

/*
 * Assemble an instruction and sets its coding
 * \param in The instruction. At least its opcode must be set, but not its coding (must use upd_assemble_insn instead)
 * \param driver The driver for the architecture the instruction belongs to
 * \return
 * - EXIT_SUCCESS if the instruction could be correctly assembled. Its coding will have been set
 * - Error code if the instruction could not be correctly assembled or if its coding was not NULL.
 * */
int assemble_insn(insn_t* in, asmbldriver_t* driver)
{
   bitvector_t* newbv = NULL, *oldbv = insn_get_coding(in);

   if ( BITVECTOR_GET_BITLENGTH(oldbv) > 0) {
      char buf[256];
      insn_print(in, buf, sizeof(buf));
      ERRMSG(
            "instruction %#"PRIx64":%s already has a coding. Use upd_assemble_insn instead\n",
            INSN_GET_ADDR(in), buf);
      return ERR_ASMBL_INSTRUCTION_HAS_CODING;
   }
   DBG(
         char buf[256];insn_print(in, buf, sizeof(buf));DBGMSG("Assembling instruction %s\n",buf));

   newbv = driver->insn_gencoding(in);

   DBG(
         char buf[256]; bitvector_hexprint(newbv, buf, sizeof(buf), " "); DBGMSG("Instruction has coding: %s\n", buf));

   if (newbv != NULL) {
      insn_set_coding(in, NULL, 0, newbv);
      return EXIT_SUCCESS;
   } else {
      char* env_v = getenv("_MAQAO_DBG_MSG");
      // Here use an environment variable to enable or not the message.
      // If the variable does not exist, print it
      // If the variable exists and is 1, print it
      if ((env_v != NULL && strcmp(env_v, "1") == 0) || env_v == NULL) {
         char buf[256];
         insn_print(in, buf, sizeof(buf));
         ERRMSG("instruction %#"PRIx64":%s could not be assembled\n",
               INSN_GET_ADDR(in), buf);
         {
            DBG(
                  DBGMSG0("Operand size: ");int it;for(it=0;it<insn_get_nb_oprnds(in);it++) fprintf(stderr,"%d ",oprnd_get_bitsize(insn_get_oprnd(in,it)));fprintf(stderr,"\n");)
         }
      }
      return ERR_ASMBL_INSTRUCTION_NOT_ASSEMBLED;
   }
}

/*
 * Updates an instruction coding for a given architecture.
 * \param in The instruction
 * \param d Assembler driver for the architecture
 * \param chgsz If set to TRUE, the coding of the instruction will be updated even if its size changes.
 * If set to FALSE, and if the new coding of the instruction changes size, a warning will be printed and the instruction's
 * coding will not be updated
 * \param shiftaddr Contains the value by which instructions have been shift because of coding changing sizes. It will be updated
 * if chgsz is set to TRUE and the instruction's coding changes. It will also be used to update an instruction's pointer operand
 * value
 * \return
 * - EXIT_SUCCESS if the instruction could be correctly updated. Its coding will have been set
 * - Error code if the instruction could not be correctly assembled or if its new coding had a different size while \e chgsz was set to FALSE.
 * In that case, its coding will not have been updated.
 * */
int upd_assemble_insn(insn_t* in, void* d, int chgsz, int64_t *shiftaddr)
{
   /**\todo TODO (2015-01-12) Change this function to get rid of all unused parameters. Possibly remove it to only use assemble_insn as well
    * (to avoid having two different functions depending on whether we want to update or set a coding), with the tests on the possible change
    * in size being performed elsewhere in the patcher*/
   /** \bug This shiftaddr is the remnant of the previous version of MADRAS and is used to count how many instructions have been
    * expanded so that it's possible to keep track on how much the offsets must be shifted. It should be removed after the patcher refactoring*/
   bitvector_t* oldcod, *newcod;
   asmbldriver_t* driver = (asmbldriver_t*) d;
   /*See the bug in the function header*/
   if ((shiftaddr)
         && (insn_is_branch(in)
               && (pointer_get_type(oprnd_get_ptr(insn_get_oprnd(in, 0)))
                     == POINTER_RELATIVE))) {
      int64_t offset = pointer_get_offset(oprnd_get_ptr(insn_get_oprnd(in, 0)));
      DBGMSG("Offset is %#"PRIx64"\n", offset);
      offset -= *shiftaddr; //In case of instructions having shifted because increasing in size
      DBGMSG("Shift of addresses %#"PRIx64" changes offset to %#"PRIx64"\n",
            *shiftaddr, offset);
      pointer_set_offset(oprnd_get_ptr(insn_get_oprnd(in, 0)), offset);
   }

   DBG(
         char buf[256];insn_print(in, buf, sizeof(buf));DBGMSG("Assembling instruction %s\n",buf));

   newcod = driver->insn_gencoding(in);

   DBG(
         char buf[256]; bitvector_hexprint(newcod, buf, sizeof(buf), " "); DBGMSG("Instruction has coding: %s\n", buf));

   oldcod = insn_get_coding(in);

   if ((newcod == NULL)
         || (( BITVECTOR_GET_BITLENGTH(newcod)
               != BITVECTOR_GET_BITLENGTH(oldcod)) && (chgsz == FALSE))) {
      char buf[256];
      int out = EXIT_FAILURE;
      insn_print(in, buf, sizeof(buf));
      if (newcod == NULL) {
         ERRMSG("assembling of %#"PRIx64":%s failed. No updates performed\n",
               INSN_GET_ADDR(in), buf);
         out = ERR_ASMBL_INSTRUCTION_NOT_ASSEMBLED;
      } else if ( BITVECTOR_GET_BITLENGTH(newcod)
            != BITVECTOR_GET_BITLENGTH(oldcod) && (chgsz == FALSE)) {
         ERRMSG(
               "New coding of %#"PRIx64":%s would have a different size (%d instead of %d). No updates performed\n",
               INSN_GET_ADDR(in), buf, BITVECTOR_GET_BITLENGTH(newcod),
               BITVECTOR_GET_BITLENGTH(oldcod));
         out = ERR_ASMBL_CODING_HAS_DIFFERENT_LENGTH;
      }
      DBGMSG("Failed to assemble instruction %p\n", in);
      {
         DBG(
               DBGMSG0("Operand size: ");int it;for(it=0;it<insn_get_nb_oprnds(in);it++) fprintf(stderr,"%d ",oprnd_get_bitsize(insn_get_oprnd(in,it)));fprintf(stderr,"\n");)
      }
      bitvector_free(newcod);
      return out;
   }

   /*See the bug in the function header*/
   if ((shiftaddr)
         && ( BITVECTOR_GET_BITLENGTH(newcod) != BITVECTOR_GET_BITLENGTH(oldcod))
         && (chgsz == TRUE)) {
      *shiftaddr += (int) ( BITVECTOR_GET_BITLENGTH(newcod) >> 3)
            - (int) ( BITVECTOR_GET_BITLENGTH(oldcod) >> 3);
      DBGMSG("Shift of addresses updated to %#"PRIx64"\n", *shiftaddr);
   }

   insn_set_coding(in, NULL, 0, newcod);

   return EXIT_SUCCESS;
}

/*
 * Modifies an instruction according to a model
 * \param orig The original instruction to modify
 * \param newopcode The new opcode of the instruction, or NULL if it must be kept
 * \param newparams Array of new parameters. Those in the array that are NULL specify that the original parameter must be kept
 * \param n_newparams Size of the newparams array
 * \param driver Assembler driver for the architecture
 * \return The modified instruction
 * */
insn_t* modify_insn(insn_t* orig, char* newopcode, oprnd_t** newparams,
      int n_newparams, asmbldriver_t* driver)
{
   int i;
   arch_t* arch_orig = insn_get_arch(orig);
   insn_t* out = insn_new(arch_orig);
   /*Sets the instruction's opcode to the new value if there is one*/
   if (newopcode != NULL) {
      insn_set_opcode_str(out, newopcode);
   } else {
      insn_set_opcode(out, insn_get_opcode_code(orig));
   }

   /*Updating the operands*/
   for (i = 0; i < n_newparams; i++) {
      oprnd_t* op = NULL;
      if (newparams[i] != NULL) {
         /*New operand value*/
         op = arch_orig->oprnd_copy(newparams[i]);
      } else if (i < insn_get_nb_oprnds(orig)) {
         /*Keeping the old operand value if there was one*/
         op = arch_orig->oprnd_copy(insn_get_oprnd(orig, i));
      }
      if (op != NULL) {
         insn_add_oprnd(out, op);
      }
   }

   /*Preserving the address of the instruction*/
   insn_set_addr(out, INSN_GET_ADDR(orig));

   /**\todo BEWARE THE LABEL (see where this function is called how it is handled)*/

   assemble_insn(out, driver);

   return out;

}

/*
 * Assemble the given instructions
 * \param driver Driver to the corresponding architecture
 * \param insnlist_string Instruction list (separated by '\n' characters). See \ref insnlist_parsenew for the accepted format
 * \param asmfile The asmfile with the architecture the instruction belongs to
 * \param insnlist Queue of instructions
 * \return EXIT_SUCCESS if the assembly succeeded for all instructions, error code otherwise
 * */
int assemble_strlist(asmbldriver_t* driver, char* insnlist_string,
      asmfile_t* asmfile, queue_t** insnlist)
{
   int out = EXIT_SUCCESS;
   /**\todo (2015-06-12) Stupidity alert! I really don't see the point of passing insnlist as a pointer, and
    * at the very least I should CHECK THAT IT EXISTS!*/
   DBGMSG("GLOUGLOU: %s\n", insnlist_string);
   if (driver && insnlist_string && asmfile_get_arch(asmfile)) {
      *insnlist = insnlist_parse(insnlist_string, asmfile_get_arch(asmfile));
      if (*insnlist != NULL) {
         uint64_t listsz = 0;
         /*Assembling all instructions*/
         int assembling_status;
         FOREACH_INQUEUE(*insnlist, iter) {
            assembling_status = assemble_insn(GET_DATA_T(insn_t*, iter),
                  driver);
            if (!ISERROR(out) && assembling_status != EXIT_SUCCESS)
               out = assembling_status;
         }
         if (out == EXIT_SUCCESS) {
            /*Updating addresses*/
            insnlist_upd_addresses(*insnlist, 0, NULL, NULL);
            //Now we need to update the addresses and recalculate the branches. And this can cause instruction sizes to change
            do {
               listsz = insnlist_bitsize(*insnlist, NULL, NULL);
               insnlist_upd_branchaddr(*insnlist, upd_assemble_insn, 1, driver,
                     asmfile, NULL, NULL);
               insnlist_upd_addresses(*insnlist, 0, NULL, NULL);
            } while (listsz != insnlist_bitsize(*insnlist, NULL, NULL));
         }
      }
   }

   return out;
}

/*
 * Assemble the given instructions for an architecture
 * \param insnlist Instruction list (separated by '\n' characters). See \ref insnlist_parsenew for the accepted format
 * \param arch The structure representing the architecture
 * \param archname The name of the architecture the instruction list belongs to. Will be used if \c arch is NULL
 * \param insnlist Queue of instructions
 * \return EXIT_SUCCESS if the assembly succeeded for all instructions, error code otherwise
 * */
int assemble_strlist_forarch(char* insnlist_string, arch_t* arch,
      char* archname, queue_t** insnlist)
{
   /**\todo Streamline this, it was a quick and dirty fix for an urgent need*/
   int out = EXIT_SUCCESS;
   asmbldriver_t* driver =
         (arch) ?
               asmbldriver_load(arch) : asmbldriver_load_byarchname(archname);
   if (driver) {
      asmfile_t* asmfile = asmfile_new("tmp");
      asmfile_set_arch(asmfile, driver->getarch());
      out = assemble_strlist(driver, insnlist_string, asmfile, insnlist);

      asmfile_free(asmfile);
      asmbldriver_free(driver);
   }
   return out;
}

/*
 * Assemble the given instructions
 * \param driver Driver to the corresponding architecture
 * \param insnlist Queue of instructions
 * \return EXIT_SUCCESS if the assembly succeeded for all instructions, error code otherwise
 * */
int assemble_list(asmbldriver_t* driver, queue_t* insnlist)
{
   /** \todo Advanced error handling */
   int out = EXIT_SUCCESS;
   if ((driver != NULL) && (insnlist != NULL)) {
      FOREACH_INQUEUE(insnlist, iter)
      {
         if (insn_get_coding(GET_DATA_T(insn_t*, iter)) != NULL) {
            out = upd_assemble_insn(GET_DATA_T(insn_t*, iter), driver, TRUE,
                  NULL);
         } else {
            out = assemble_insn(GET_DATA_T(insn_t*, iter), driver);
         }
         if (ISERROR(out)) {
            ERRMSG("Error when assembling instruction list\n");
            break;
         }
      }
   } else {
      out = ERR_COMMON_PARAMETER_MISSING;
   }
   return out;
}

/*
 * Parses and assembles an assembly file and returns the corresponding bytes
 * \param asmfile An asmfile structure. It must contain a path to a valid assembly file
 * \param archname Name of the architecture to use for assembling
 * \param len Return parameter, will contain the length of the returned byte string
 * \return The assembled code of the assembly instructions found in the file, or NULL if an error occurred
 * In that case, the last error code in \c asmfile will be updated
 * */
unsigned char* assemble_asm_file(asmfile_t* asmfile, char* archname, int* len)
{
   if (!asmfile_get_name(asmfile) || !archname) {
      ERRMSG(
            "Empty file name or empty architecture name: unable to assemble file\n");
      asmfile_set_last_error_code(asmfile, ERR_LIBASM_ARCH_MISSING);
      if (len)
         *len = 0;
      return NULL;
   }
   FILE* stream = NULL;
   asmbldriver_t* driver = asmbldriver_load_byarchname(archname);

   if (!driver) {
      if (len)
         *len = 0;
      asmfile_set_last_error_code(asmfile, ERR_ASMBL_ARCH_NOT_SUPPORTED);
      return NULL;
   }
   //Sets the architecture into the asmfile
   asmfile_set_arch(asmfile, driver->getarch());

   //Loads the content of the file into a string
   char* filetext = getFileContent(asmfile_get_name(asmfile), &stream, NULL);
   if (!filetext) {
      ERRMSG("Unable to read the content of file %s\n",
            asmfile_get_name(asmfile));
      if (len)
         *len = 0;
      asmfile_set_last_error_code(asmfile, ERR_COMMON_UNABLE_TO_OPEN_FILE);
      return NULL;
   }
   //Parses and assembles the content of the file
   queue_t *insnl;
   int res = assemble_strlist(driver, filetext, asmfile, &insnl);
   if (ISERROR(res)) {
      ERRMSG("Unable to assemble list of instructions from file %s\n",
            asmfile_get_name(asmfile));
      if (len)
         *len = 0;
      asmfile_set_last_error_code(asmfile, res);
      return NULL;
   }
   asmfile_set_insns(asmfile, insnl); /**\bug (2015-06-12) Warning, memory leak here due to the way asmfile_set_insns is implemented*/
   //Retrieves the binary code from the file
   int codelen;
   unsigned char* codebytes = insnlist_getcoding(insnl, &codelen, NULL, NULL);
   if (len)
      *len = codelen;
   asmbldriver_free(driver);
   releaseFileContent(filetext, stream);

   return codebytes;
}

#if 0
/*
 * Frees all members in an asmfile_t structure that were updated in the asmfile_assemble_fromtxtfile function
 * \param a The asmfile_t structure holding the disassembled file. After execution, all members will be freed and set to 0
 * */
void asmfile_assembled_fromtxtfile_free(void* a)
{
   /**\todo TODO (2015-07-07) I'm pretty sure we could get rid of this function (and its counterpart in libmdisass.c)
    * There should be no need to distinguish the types
    * => Currently not used (see comment at the end of asmfile_assemble_fromtxtfile)*/
   if (a != NULL) {
      asmfile_t* af = (asmfile_t*) a;
      txtfile_t* txtorigin = asmfile_gettxtfile(af);
      /*Frees the parsed file structure*/
      if (txtorigin != NULL) {
         txtfile_close(txtorigin);
         asmfile_clearorigin(af);
      }

      /*Frees the list of labels*/
      queue_flush(af->label_list, NULL);

      /*Frees the table of labels and the labels*/
      hashtable_flush(af->label_table, label_free, NULL);

      /*Frees the list of instructions*/
      queue_flush(asmfile_get_insns(af), af->arch->insn_free);
   }
}
#endif

/*
 * Parses and assembles a formatted text file
 * The text file must be formatted so as to be able to be handled by the txtfile_* functions from libmcommon.
 * \param asmfile An asmfile structure. It must contain a path to a valid formatted text file
 * \param archname Name of the architecture to use for assembling
 * \param txtfile Parsed text file
 * \param fieldnames Array of names of the fields characterising the assembly elements present in the text file
 * \return EXIT_SUCCESS / error code
 * */
int asmfile_assemble_fromtxtfile(asmfile_t* asmfile, char* archname,
      txtfile_t* txtfile, const asm_txt_fields_t fieldnames)
{
   /**\todo (2015-07-07) If we could manage to update MINJAG enough that we can retrieve here all information about
    * instructions (extensions, operand roles, etc), this function could become an entry point from analysis, at the same
    * level as asmfile_disassemble in libmdisass.c. As of now, we will need to redisassemble instructions afterwards to retrieve
    * those information*/
   if (!asmfile_get_name(asmfile))
      return ERR_COMMON_FILE_NAME_MISSING;
   int retcode;
   uint32_t i;

   //Parses the text file if necessary
   if (!txtfile) {
      txtfile = txtfile_open(asmfile_get_name(asmfile));
//   txtfile_setcommentsdelim(txtfile, "//", NULL, NULL);
//   txtfile_setscntags(txtfile, "_@M", "_begin", "_end", "body", "header");
//   txtfile_setfieldtags(txtfile, "str", "num", ":", NULL);
      retcode = txtfile_parse(txtfile);
      if (ISERROR(retcode)) {
         //Error during parsing
         txtfile_close(txtfile);
         return retcode;
      }
   }
   //File successfully parsed
   //Retrieving the architecture if it was present in the file
   uint32_t n_archscns;
   txtscn_t** archscns = txtfile_getsections_bytype(txtfile, fieldnames.scnarch,
         &n_archscns);
   if (n_archscns > 0) {
      if (n_archscns > 1) {
         /**\todo (2016-04-19) Allow multiple declarations of architectures inside the file*/
         assert(archscns && archscns[0]);
         WRNMSG(
               "Multiple sections characterising the architecture found: keeping values from line %u\n",
               txtscn_getline(archscns[0]));
      }
      txtfield_t* archfield = txtscn_getfield(archscns[0],
            fieldnames.archfieldnames[ARCHF_NAME]);
      char* arch = txtfield_gettxt(archfield);
      if (arch != NULL) {
         //An architecture name is defined in the file: using it
         if (!str_equal(arch, archname)) {
            WRNMSG(
                  "Overriding parameter %s for architecture name with value from %s from the file\n",
                  archname, arch);
         }
         archname = arch;
      }
   }

   if (!archname)
      return ERR_LIBASM_ARCH_MISSING;

   asmbldriver_t* driver = asmbldriver_load_byarchname(archname);
   if (!driver) {
      return ERR_ASMBL_ARCH_NOT_SUPPORTED;
   }

   queue_t* insns = queue_new();

   //Sets the architecture into the asmfile
   asmfile_set_arch(asmfile, driver->getarch());
   //Ordering the instructions by addresses
   retcode = txtfile_sort_bodylines(txtfile,
         fieldnames.insnfieldnames[INSNF_ADDRESS]);
   if (retcode != EXIT_SUCCESS) {
      //Error when ordering instructions by addresses: not critical, but serious
      WRNMSG(
            "Unable to order instructions by addresses in parsed text file %s: %s\n",
            txtfile_getname(txtfile), errcode_getmsg(retcode));
   }

   //Now parsing the contents
   //Parsing the information relative to labels
   //Retrieving the address of the first instruction
   int64_t firstinsnaddr = txtfield_getnum(
         txtscn_getfield(txtfile_getbodyline(txtfile, 0),
               fieldnames.insnfieldnames[INSNF_ADDRESS]));
   int hasfirstlabel = FALSE; //Storing whether there is a label before or at the first instruction
   txtscn_t** scnlbls = NULL; //Array of text sections representing labels declaration
   uint32_t n_scnlbls = 0;
   scnlbls = txtfile_getsections_bytype(txtfile, fieldnames.scnfctlbls,
         &n_scnlbls);
   if (n_scnlbls > 0) {
      //There is at least one section declaring labels
      for (i = 0; i < n_scnlbls; i++) {
         //Retrieves the field containing the name of the label
         char* lblname = txtfield_gettxt(
               txtscn_getfield(scnlbls[i],
                     fieldnames.labelfieldnames[LBLF_NAME]));
         //Retrieves the field containing the address of the label
         txtfield_t* addrfield = txtscn_getfield(scnlbls[i],
               fieldnames.labelfieldnames[LBLF_ADDRESS]);
         int64_t lbladdr;
         if (addrfield != NULL) {
            lbladdr = txtfield_getnum(addrfield);
         } else {
            //Address not filled in the function : using the address of the first instruction following it
            lbladdr = txtfield_getnum(
                  txtscn_getfield(txtscn_getnextbodyline(scnlbls[i]),
                        fieldnames.insnfieldnames[INSNF_ADDRESS]));
         }
         if (lblname) {
            DBGMSG(
                  "Adding function label %s at address %#"PRIx64", declared at line %u in text file %s\n",
                  lblname, lbladdr, txtscn_getline(scnlbls[i]),
                  txtfile_getname(txtfile))
            label_t* lbl = label_new(lblname, lbladdr, TARGET_INSN, NULL);
            label_set_type(lbl, LBL_FUNCTION); //By default we assume every label declared that way corresponds to a function
            //Adds the label to the file
            asmfile_add_label_unsorted(asmfile, lbl);
            //Checks if this label is before or at the first instruction
            if (lbladdr <= firstinsnaddr)
               hasfirstlabel = TRUE;
         }
      }
      lc_free(scnlbls);
   }
   if (hasfirstlabel == FALSE) {
      //Either there is no section declaring labels, or there is no label before or at the first instruction: we will create one at the beginning of the file
      //Retrieving the address of the first instruction
      //Creating a label "main" at the beginning of the file
      DBGMSG(
            "Adding function label main at address %#"PRIx64" in text file %s\n",
            firstinsnaddr, txtfile_getname(txtfile))
      label_t* lbl = label_new("main", firstinsnaddr, TARGET_INSN, NULL);
      label_set_type(lbl, LBL_FUNCTION);
      //Adds the label to the file
      asmfile_add_label_unsorted(asmfile, lbl);
   }
   //Updates labels in the file
   asmfile_upd_labels(asmfile);
   //Retrieving the function labels
   uint32_t n_fctlabels, currentlblidx = 0;
   label_t** fctlabels = asmfile_get_fct_labels(asmfile, &n_fctlabels);
   assert(n_fctlabels >= 1); //The way we built it there is at least one label, and it's before or after the first instruction

   //Now parsing the information relative to labels used for branches. Those are not function labels and will not be added to the asmfile
   scnlbls = txtfile_getsections_bytype(txtfile, fieldnames.scnbrchlbls,
         &n_scnlbls);
   queue_t* brchlbls = queue_new();
   if (n_scnlbls > 0) {
      //There is at least one section declaring labels for branches
      for (i = 0; i < n_scnlbls; i++) {
         //Retrieves the field containing the name of the label
         char* lblname = txtfield_gettxt(
               txtscn_getfield(scnlbls[i],
                     fieldnames.labelfieldnames[LBLF_NAME]));
         //Retrieves the field containing the address of the label
         int64_t lbladdr = txtfield_getnum(
               txtscn_getfield(scnlbls[i],
                     fieldnames.labelfieldnames[LBLF_ADDRESS]));
         if (lblname) {
            DBGMSG(
                  "Adding branch label %s at address %#"PRIx64", declared at line %u in text file %s\n",
                  lblname, lbladdr, txtscn_getline(scnlbls[i]),
                  txtfile_getname(txtfile))
            label_t* lbl = label_new(lblname, lbladdr, TARGET_INSN, NULL);
            label_set_type(lbl, LBL_NOFUNCTION); //Just to be sure there won't be any problem
            //Adds the label to the file
            queue_add_tail(brchlbls, lbl);
         }
      }
      lc_free(scnlbls);
   }
   /**\todo (2015-07-07) For optimisation, we could order the queue of branch labels by names, which would allow
    * to perform a bsearch when looking for one of them. So far I consider we will have few enough labels to not need that.
    * Of course, if we later use extensive files, we could also use a hashtable.*/

   int64_t addrprev = ADDRESS_ERROR; //This will be for detecting duplicated instructions
   //Parsing the instructions
   for (i = 0; i < txtfile_getn_bodylines(txtfile); i++) {
      txtscn_t* line = txtfile_getbodyline(txtfile, i);
      txtfield_t* field = NULL;
      field = txtscn_getfield(line,
            fieldnames.insnfieldnames[INSNF_FULL_ASSEMBLY]);
      char* insntxt = txtfield_gettxt(field);
      int64_t insnaddr = ADDRESS_ERROR;
      if (!insntxt) {
         ERRMSG("No instruction present at line %d\n", txtscn_getline(line));
         continue;
      }
      //Retrieving address of instruction
      field = txtscn_getfield(line, fieldnames.insnfieldnames[INSNF_ADDRESS]);
      if (field) {
         insnaddr = txtfield_getnum(field);
         //Checking if the address was already found and discarding the instruction if it was
         if (insnaddr == addrprev) {
            WRNMSG(
                  "Discarding instruction with already declared address %#"PRIx64" at line %u\n",
                  insnaddr, txtscn_getline(line));
            continue;
         }
      }

      insn_t* insn = insn_parsenew(insntxt, driver->getarch());
      if (!insn) {
         ERRMSG("Unable to parse instruction at line %d\n",
               txtscn_getline(line));
         continue;
      }
      //Flags the instruction as belonging to a section containing code to analyse because otherwise I don't see the point
      insn_add_annotate(insn, A_STDCODE);

      //Checking for other characteristics in the instruction
      if (insnaddr != ADDRESS_ERROR) {
         insn_set_addr(insn, insnaddr);
         addrprev = insnaddr;
      }
      char* srcfile = txtfield_gettxt(
            txtscn_getfield(line,
                  fieldnames.insnfieldnames[INSNF_DBG_SRCFILE]));
      uint32_t srcline = txtfield_getnum(
            txtscn_getfield(line,
                  fieldnames.insnfieldnames[INSNF_DBG_SRCLINE]));
      if (srcfile || srcline) {
         /**\todo TODO (2015-07-07) USE SETTERS FOR THIS!!!! (I did it on the patcher refactoring branch I think)*/
         insn->debug = lc_malloc(sizeof(dbg_insn_t));
         insn->debug->srcfile = srcfile;
         insn->debug->srcline = srcline;
      }

      //Now handling branches
      if (oprnd_is_ptr(insn_get_oprnd(insn, 0)) == TRUE) {
         /**\todo (2015-07-07)
          * ************WARNING DIRTY CODE AHEAD**********
          * We are still using the trick of storing the index in the string where we found a branch label into the offset of
          * the pointer itself. BUT now we can also accept an address provided in the expression of the instruction. So we'll
          * make some test to try identifying if the pointer contains an index or an address. It's very dirty and whatever, but
          * so far I really don't see another option. And for a change I'm working under a deadline so I don't have the time to
          * do something more clever
          * Also, we should backport what has been done with pointers on the patcher refactoring branch here
          * */
         DBGMSGLVL(1, "Found branch instruction at line %u\n",
               txtscn_getline(line))
         pointer_t* ptr = oprnd_get_ptr(insn_get_oprnd(insn, 0));
         int64_t addr = pointer_get_addr(ptr);
         if (addr < (int64_t) strlen(insntxt)
               && (insntxt[addr] == '\0' || insntxt[addr] == '<'
                     || insntxt[addr] == '.' || insntxt[addr] == '_'
                     || (insntxt[addr] >= 'a' && insntxt[addr] <= 'z')
                     || (insntxt[addr] >= 'A' && insntxt[addr] <= 'Z'))) {
            /**\todo (2015-07-07) Here is the dirty code that was announced above: we consider we have a pointer to the name of
             * the branch label*/
            char lblname[strlen(insntxt)];
            int c;
            if (insntxt[addr] == '<') {
               //Label delimited by brackets: copying the content between the brackets into the temporary buffer for the label name
               c = 1;
               while (insntxt[addr + c] != '\0' && insntxt[addr + c] != '>') {
                  lblname[c - 1] = insntxt[addr + c];
                  c++;
               }
               lblname[c - 1] = '\0';
            } else {
               //Standard label name: we copy its content until reaching a comma, space, or end of line
               c = 0;
               while (insntxt[addr + c] != '\0' && insntxt[addr + c] != ' '
                     && insntxt[addr + c] != '\t' && insntxt[addr + c] != ',') {
                  lblname[c] = insntxt[addr + c];
                  c++;
               }
               lblname[c] = '\0';
            }
            //Now looking for a branch label with that name
            FOREACH_INQUEUE(brchlbls, iter) {
               if (str_equal(lblname,
                     label_get_name(GET_DATA_T(label_t*, iter))))
                  break;
            }
            if (iter) {
               //Found an existing branch label with this name
               int64_t dest = label_get_addr(GET_DATA_T(label_t*, iter));
               //Adding the destination to the table of branches in the file
               hashtable_insert(asmfile_get_branches(asmfile), (void*) dest,
                     insn);
            } else {
               label_t* fctlbl = asmfile_lookup_label(asmfile, lblname);
               if (fctlbl) {
                  //Label with this name found: adding the destination to the table of branches in the file
                  hashtable_insert(asmfile_get_branches(asmfile),
                        (void*) label_get_addr(fctlbl), insn);
               } //Otherwise this branch has no existing destination in the file
            }

         } else {
            //We have a regular address: adding it to the table of branches of the file
            hashtable_insert(asmfile_get_branches(asmfile), (void*) addr, insn);
         }
      }  //End of handling branches

      //Handling labels
      if (currentlblidx < (n_fctlabels - 1)
            && insn_get_addr(insn)
                  >= label_get_addr(fctlabels[currentlblidx + 1]))
         currentlblidx++; //Reached the address of the next label: updating to the next label
      //Linking the instruction to the current label
      insn_link_fct_lbl(insn, fctlabels[currentlblidx]);

      add_insn_to_insnlst(insn, insns);
   }
   queue_t* unlinked_branches = queue_new();
   //Now performing a second pass on the instructions to resolve branch destinations
   FOREACH_INQUEUE(insns, iteri) {
      insn_t* in = GET_DATA_T(insn_t*, iteri);
      int64_t addr = insn_get_addr(in);
      //Retrieves all branches pointing to this address
      queue_t* branches = hashtable_lookup_all(asmfile_get_branches(asmfile),
            (void*) addr);
      //Looks up if the instruction has a pointer operand
      pointer_t* refptr = oprnd_get_ptr(insn_lookup_ref_oprnd(in));
      if (refptr != NULL) {
         //Found a pointer operand: updating its address depending on its target
         driver->getarch()->oprnd_updptr(in, refptr); //We don't use insn_oprnd_updptr as we want only branches to be updated that way
      }
      //Try assembling the instruction
      if (insn_get_coding(in))
         upd_assemble_insn(in, driver, TRUE, NULL); //Branches may have been updated before reaching them
      else if (oprnd_get_ptr(insn_get_oprnd(in, 0)) == NULL
            || insn_get_branch(in) != NULL)
         assemble_insn(in, driver); //We want to avoid assembling branches whose destination has not been set yet
      else
         queue_add_tail(unlinked_branches, in); //Storing branches with no destination

      DBGMSGLVL(1,
            "Found %d instruction(s) pointing to instruction at address %#"PRIx64"\n",
            queue_length(branches), addr);
      //Linking branches to the instruction
      FOREACH_INQUEUE(branches, iterb) {
         insn_t* branch = GET_DATA_T(insn_t*, iterb);
         DBGMSGLVL(2,
               "Linking branch instruction at address %#"PRIx64" to instruction at address %#"PRIx64"\n",
               insn_get_addr(branch), addr)
         //Retrieves pointer operand of the branch
         pointer_t* refptr = oprnd_get_ptr(insn_lookup_ref_oprnd(branch));
         //Links the pointer to the instruction
         pointer_set_insn_target(refptr, in);
         //Updates pointer address
         driver->getarch()->oprnd_updptr(branch, refptr);
         /**\todo TODO (2015-07-07) BACKPORT HERE THE FUNCTIONS HANDLING POINTERS FROM THE PATCHER REFACTORING
          * (because oprdn_setptraddr has some architecture-specific elements)
          * => (2015-07-30) Done. Leaving the TODO here as I don't like the direct access to oprnd_updptr. Create a wrapper for that*/
         //Updating coding of the branch if it had already been assembled
         if (insn_get_coding(branch))
            upd_assemble_insn(branch, driver, TRUE, NULL);
         else
            assemble_insn(branch, driver);
         //Remove the branch from the list of unlinked branches
         queue_remove(unlinked_branches, branch, NULL);
      }
      queue_free(branches, NULL);
   }
   //Updating coding of unlinked branches
   FOREACH_INQUEUE(unlinked_branches, iterub) {
      insn_t* branch = GET_DATA_T(insn_t*, iterub);
      //Updates pointer address
      driver->getarch()->oprnd_updptr(branch,
            oprnd_get_ptr(insn_lookup_ref_oprnd(branch)));
      //Assemble instruction
      assemble_insn(branch, driver);
   }
   queue_free(unlinked_branches, NULL);

   //Setting the instruction list into the assembly file
   asmfile_set_insns(asmfile, insns);
   //Stores the txtfile in the file
   asmfile_set_txtfile(asmfile, txtfile, &fieldnames);

   asmbldriver_free(driver);
   queue_free(brchlbls, label_free);

   return EXIT_SUCCESS;
}
