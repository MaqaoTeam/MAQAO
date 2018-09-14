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
 * \file libmdisass.c
 * \brief This file contains the functions needed to disassemble a file by invoking the appropriate parser.
 * */

#include <stdlib.h>
#include <inttypes.h>
#include <assert.h>
#include <fcntl.h>

#include "bfile_fmtinterface.h"
#include "dsmb_archinterface.h"
#include "archinterface.h"
#include "libmdisass.h"
#include "libmdbg.h"

/**
 * \brief Internal structure holding additional informations about a parsed or disassembled elf file
 * */
typedef struct parsedfile_s {
   int n_codescn_idx; /**<Size of the array containing the code sections indexes*/
   int* codescn_idx; /**<Indexes of the ELF sections containing executable code*/
   dsmbldriver_t *driver; /**<Pointer to the architecture specific driver*/
   asmfile_t* af; /**<Pointer to the file being parsed or disassembled*/
   queue_t* dummylabels; /**<List of dummy labels to be associated to instructions, but not the opposite*/
   int last_error_code; /**<Last error code encountered*/
} parsedfile_t;

/***Utility functions for handling a disassembled ELF file***/

#if 0
static parsedfile_t* parsedfile_new() {
   parsedfile_t* pf = NULL;
   pf = (parsedfile_t*) lc_malloc0(sizeof(parsedfile_t));
   pf->dummylabels = queue_new();
   return pf;
}

/**
 * Creates a new structure for storing additional data relative to a parsed file
 * \param pf The structure holding the additional data about the disassembled file
 * \param af The asmfile we are parsing. Its origin must already be set. It will be updated to be filled with informations about the architecture
 * \param archname Name of the architecture of the parsed file. Will be used only if \c efile is NULL
 * \return EXIT_SUCCESS or error code
 * */
static int parsedfile_fill(parsedfile_t* pf, asmfile_t* af, char* archname)
{
   arch_t* arch = NULL;
   assert(pf != NULL && af != NULL);
   elffile_t* efile = asmfile_get_origin(af);
   if (efile != NULL) {
      pf->codescn_idx = elffile_getprogscns(efile, &(pf->n_codescn_idx));
      arch = getarch_bybincode(BFF_ELF, elffile_getmachine(efile));
   } else if (archname != NULL) {
      arch = getarch_byname(archname);
   } else {
      assert(FALSE);
      //What do you expect, it is a static function, we should never arrive here
   }
   if (!arch)
   return ERR_LIBASM_ARCH_UNKNOWN;
   //Updates the architecture in the ASM file
   asmfile_set_arch(af, arch);
   pf->af = af;

   pf->driver = dsmbldriver_load(arch);
   if (!pf->driver)
   return ERR_DISASS_ARCH_NOT_SUPPORTED;

   return EXIT_SUCCESS;
}

/**
 * Creates a new structure for storing additional data relative to a parsed file
 * \param pf The structure holding the additional data about the disassembled file
 * \param af The asmfile we are parsing. Its origin must already be set. It will be updated to be filled with informations about the architecture
 * \param archname Name of the architecture of the parsed file. Will be used only if \c efile is NULL
 * \return A pointer to the structure storing details about a file being parsed
 * */
static parsedfile_t* parsedfile_init(asmfile_t* af, char* archname)
{
   parsedfile_t* pf = parsedfile_new();
   pf->last_error_code = parsedfile_fill(pf, af, archname);
   return pf;
}

/**
 * Frees a structure containing additional data relative to a parsed file
 * \param pf The structure holding the additional data about the disassembled file
 * */
static void parsedfile_terminate(parsedfile_t* pf)
{

   if (pf == NULL)
   return;

   if (pf->codescn_idx)
   lc_free(pf->codescn_idx);
   queue_free(pf->dummylabels, NULL);
   if (pf->driver)
   dsmbldriver_free(pf->driver);
   lc_free(pf);
}
#endif
/*** Disassembly functions ***/
#if 0
/**
 * Updates the tables registering the branch addresses for an instruction (either the
 * table holding the branch addresses, or the ELF file's table holding the internal links)
 * \param in The current instruction
 * \param iref The hashtable containing branches instructions (indexed on their addresses)
 * \param efile The structure holding the parsed ELF file the instruction belongs to
 * */
static void update_branchtbl(insn_t* in, hashtable_t* iref, elffile_t* efile)
{
   int isinsn;
   int64_t destaddr;
   destaddr = insn_check_refs(in, &isinsn);
   DBGMSGLVL(1,
         "Instruction at address %#"PRIx64" points to address: %#"PRIx64" (which is%s an instruction)\n",
         INSN_GET_ADDR(in), destaddr, (isinsn == 1) ? "" : " not");
   if (destaddr >= 0) {
      switch (isinsn) {
         case 0: /*Destination is a memory offset*/
         elffile_add_targetaddr(efile, in, destaddr, NULL, 0, 0);

/////// JE SUIS LÀ (2014-07-10) Mais faut que je mette en oeuvre les MEMORY_RELATIVE. Ça va être chaud

         /* The function for updating an instruction referencing another address is not yet known, it will be updated by the patcher*/
         /**\todo Handle the case where we could need different functions*/
         break;
         case 1: /*Destination is an instruction*/
         hashtable_insert(iref, (void*) destaddr, in);
         /**\bug We should be using a structure or a pointer to an int64_t instead of casting destaddr as a pointer*/
         break;
      }
   }
}
#endif
/**
 * Updates the references to an instruction in an asmfile and of this instruction to other elements
 * \param af The asmfile
 * \param bf The binary file from wich the asmfile originates (here to avoid having to retrieve it from asmfile)
 * \param in The instruction
 * \param addr The instruction address (here to avoid having to retrieve it from insn)
 * \param unlinked_targets Queue of targets from the binfile (data elements containing a pointer_t) with a NULL target
 * \param branches Queue of branch instructions with a NULL target
 * */
static void asmfile_upd_references(asmfile_t* af, binfile_t* bf, insn_t* in,
      int64_t addr, label_t** varlabels, uint32_t n_varlabels,
      queue_t* unlinked_targets, queue_t* branches)
{
   assert(af && in);
   //Checks if the binary file contains unlinked pointers to this address
   while (queue_length(unlinked_targets) > 0) {
      data_t* unlinked = (data_t*) queue_peek_head(unlinked_targets);
      assert(
            (data_get_type(unlinked) == DATA_PTR)
                  || (data_get_type(unlinked) == DATA_REL));  // Just checking
      pointer_t* ptr = data_get_ref_ptr(unlinked);
      assert(!pointer_has_target(ptr)); //Checking, this may be changed into a regular if statement if this can legitimately happen (2014-07-10)
      int64_t linkaddr = pointer_get_addr(ptr);
      if ((addr <= linkaddr) && (linkaddr < (addr + insn_get_bytesize(in)))) {
         //Removing the data element from the target table in the binfile
         binfile_remove_unlinked_target(bf, unlinked);
         //The targeted address corresponds to the instruction address or inside it: linking it
         pointer_set_insn_target(ptr, in);
         //Updating offset if needed
         if (linkaddr > addr)
            pointer_set_offset_in_target(ptr, linkaddr - addr);
         //Adding the data element indexed by the referenced instruction to the asmfile
         asmfile_add_data_ptr_to_insn(af, unlinked, in);
         //Removing the element from the list of unlinked targets
         queue_remove_head(unlinked_targets);
      } else if (addr < linkaddr)
         break; //We have not reached the first element in the queue of unlinked targets: stopping here, we will reach it later
      else if (addr > linkaddr)
         queue_remove_head(unlinked_targets); //We passed the first element in the list: removing it
   }

   //Now linking the instruction to other elements in the file
   oprnd_t* refop = insn_lookup_ref_oprnd(in);
   if (refop) {
      //Updates the destination address of the pointer
      insn_oprnd_updptr(in, refop);
      //The instruction has an operand referencing another address
      int64_t ref = oprnd_get_refptr_addr(refop); //Retrieves the referenced address
      if (oprnd_get_type(refop) == OT_POINTER) {
         queue_add_tail(branches, in); //Stores the instruction in the queue of branches
      } else if (oprnd_get_type(refop) == OT_MEMORY_RELATIVE) {
         uint64_t off = 0;
         label_t** varlabel = NULL;
         if (n_varlabels > 0) {
            //Check if there is a variable label at this address
            varlabel = bsearch(&ref, varlabels, n_varlabels, sizeof(*varlabels),
                  label_cmpaddr_forbsearch);
         }
         //Attempts to find a data entry in the binary file, creating it if it was not already there.
         data_t* data = binfile_adddata(bf, ref, &off,
               (varlabel) ? *varlabel : NULL);
         if (data) {
            /**\todo (2014-07-16) Still making this up. If it can legitimately happen, use an if statement instead*/
            assert(oprnd_get_type(refop) == OT_MEMORY_RELATIVE);
            //An entry was found: linking the operand to it
            pointer_t* ptr = oprnd_get_memrel_pointer(refop);
            pointer_set_data_target(ptr, data);
            pointer_set_offset_in_target(ptr, off);
         }
         //Adds the instruction to the table of instructions referencing data
         asmfile_add_insn_ptr_to_data(af, in, data);
      }
   }
}

/*
 * Error handler for the disassembler that will be invoked whenever a parsing error occurs.
 * It sets the current instruction's opcode as "bad"
 * \param fc The FSM context
 * \param i Pointer to the address of the instruction being parsed
 * \param a Pointer to the asmfile_t being disassembled
 * */
void error_handler(fsmcontext_t* fc, void** i, void* a)
{
   insn_t** in = (insn_t**) i;
   asmfile_t* asmf = (asmfile_t*) a;
   (void) fc;
   insn_t* current_insn = insn_new(asmfile_get_arch(asmf)); //(insn_t*)in;
   /*Adds a "bad" instruction and moves on */
   insn_set_opcode(current_insn, BAD_INSN_CODE);
   *in = current_insn;
}

/**
 * Finds the annotation to set on instructions depending on the section to which they belong
 * \param scnattr Annotation of the section containing the instructions
 * \return Annotation to add to the instructions in this section
 * */
static int get_insnannotate(uint16_t scnattr)
{
   int scnanno = A_NA;
   /*Detects the section*/
   if (scnattr & SCNA_STDCODE)
      scnanno |= A_STDCODE;   //Standard code section

   if (scnattr & SCNA_EXTFCTSTUBS)
      scnanno |= A_EXTFCT; //Section containing external stubs

   if (scnattr & SCNA_PATCHED)
      scnanno |= A_PATCHED;   //Patched section

   return scnanno;
}

/**
 * Parses a stream of bytes depending on the associated architecture
 * \param fc The context of the FSM used to disassemble this stream
 * \param af The ASM file this stream belongs to
 * \param bytestream The stream of bytes
 * \param bslen The length of the stream of bytes
 * \param startaddr Address of the first instruction in the stream
 * \param scn The binary section to which the stream belongs (can be NULL if doing raw disassembly)
 * \param unlinked_targets Queue of data_t structures from the binfile containing pointers with an unknown destination
 * \param branches Queue of branch instructions with a NULL target
 * \return A queue of instructions corresponding to the disassembly of the stream defined in \e fc
 * */
static queue_t* stream_parse(fsmcontext_t* fc, asmfile_t* af,
      unsigned char* bytestream, uint64_t bslen, int64_t startaddr,
      binscn_t* scn, queue_t* unlinked_targets, queue_t* branches)
{
   /**\todo This function may gain in readability from being split, but it will be tough*/
   queue_t* output = queue_new();
   label_t* lastlbl = NULL;
   int64_t current_addr;
   insn_t* current_insn = NULL;
   unsigned int n_fctlabels;    //Size of the array of eligible labels
   int last_fctlbl_idx; //Index of the last eligible label before the current address
   int fsmerror;
   int errcount = 0;	//Counts the number of consecutive parser errors. Not used now but may be for future heuristics on the validity of a disassembly
   int64_t next_fctlbl_addr = 0;    //Address of the next label
   binfile_t* bf = asmfile_get_binfile(af);
   //Setting the annotate to set depending on the section. This may be removed later
   int scnanno;
   if (!scn)
      scnanno = A_STDCODE;
   else
      scnanno = get_insnannotate(binscn_get_attrs(scn));
   /**\todo TODO (2015-06-15) The annotate when the section is NULL is for handling case where we obtained a stream to disassemble from a different
    * way than a binary file (e.g. parsed assembly file), so in that case we can be pretty confident that we want the code we are handling to be analysed.
    * If/when this evolves, adapt this accordingly (possibly something more streamlined when handling an assembled stream)*/

   //Retrieving the array of eligible labels once and for all to avoid invoking an asmfile API function too often
   label_t** fctlabels = asmfile_get_fct_labels(af, &n_fctlabels);
   list_t** lastlabel = (list_t**) lc_malloc0(sizeof(list_t*));
   *lastlabel = queue_iterator(asmfile_get_labels(af));

   //Retrieving the array of labels associated to the section we are disassembling to be able to link other labels
   uint32_t n_labels = 0;
   label_t** labels = binfile_get_labels_by_scn(bf, binscn_get_index(scn),
         &n_labels);
   uint32_t lblidx = 0; //Index of the current label in the section

   //Handling the otherwordly case where the first label associated to this section would have an address lower than the first address of the section
   while ((lblidx < n_labels) && (label_get_addr(labels[lblidx]) < startaddr))
      lblidx++;

   //Retrieving the array of labels eligible to be associated to variables
   uint32_t n_varlabels = 0;
   label_t** varlabels = asmfile_getvarlabels(af, &n_varlabels);

//   DBGMSG0("Entering\n");

   /*Updates the FSM */
   unsigned char *str = lc_malloc0(bslen);
   ;
   memcpy(str, bytestream, bslen);
   fsm_setstream(fc, str, bslen, startaddr);
   fsm_parseinit(fc);

   DBGMSG("Input stream size is %"PRId64"\n", bslen);
   /*\0 can not be used to check the end stream as it can occur in a binary sequence*/

   //Initialising the label
   lastlbl = asmfile_get_last_fct_label(af, fsm_getcurrentaddress(fc),
         &last_fctlbl_idx);
   DBGMSG("First label %s found at address %#"PRIx64"\n",
         label_get_name(lastlbl), label_get_addr(lastlbl));

   //Initialising the address of the next label
   if ((last_fctlbl_idx + 1) < (int) n_fctlabels)
      next_fctlbl_addr = label_get_addr(fctlabels[last_fctlbl_idx + 1]);
   else
      next_fctlbl_addr = startaddr + (int64_t) bslen; //Last label, next address is the end of the file

   //Initialising architecture information for endianness and interworking handling
   arch_t* current_arch = asmfile_get_arch(af);
   int64_t reset_addr;
   uint8_t current_endian = current_arch->endianness;
   uint8_t previous_endian = current_arch->endianness;
   int current_archcode = current_arch->code;
   int nb_parsed_bytes = 0;
   int inverted_bytes = 0;
   dsmbldriver_t* current_driver = dsmbldriver_load_byarchcode(
         current_archcode);

   if (current_driver == NULL)
      return NULL;

   current_addr = fsm_getcurrentaddress(fc);

   while (!fsm_isparsecompleted(fc)) {

      //Track the number of bytes inverted when parsing the last instruction (endianness)
      inverted_bytes = inverted_bytes
            - (fsm_getcurrentaddress(fc) - current_addr);
      current_addr = fsm_getcurrentaddress(fc);
      nb_parsed_bytes = current_addr - startaddr;  // Index in the byte stream
      DBGMSG("Parsing new instruction at address %#"PRIx64"\n", current_addr);

      //Handling interworking switch
      int next_archcode;
      dsmbldriver_t* next_driver = NULL;
      //Getting the architecture's code of the next instruction
      reset_addr = current_addr;
      next_archcode = current_driver->switchfsm(af, current_addr, &reset_addr,
            lastlabel);

      //Do we need to switch ?
      if (next_archcode != current_archcode) {
         /*
          * If the required reset must occurs at the last function label
          * we need to reset the fsm to the correct address and be cautious about our variables
          */
         if (reset_addr != current_addr) {
            DBGMSGLVL(1,
                  "Interworking: Trying another architecture for the function starting at %#"PRIx64"!\n",
                  reset_addr);
            insn_t* lastinsn;
            //Updating variables for endianness handling
            nb_parsed_bytes = reset_addr - startaddr;
            inverted_bytes = 0;
            current_addr = reset_addr;

            //Removing the last disassembled instructions until we reach the reset address
            while (insn_get_addr((insn_t*)queue_peek_tail(output)) > reset_addr) {
               lastinsn = (insn_t*) queue_remove_tail(output);
               DBG(
                     char buf[256]; insn_print(lastinsn,buf, sizeof(buf)); DBGMSGLVL(1,"Removing instruction %#"PRIx64":%s\n",insn_get_addr(lastinsn),buf));
               //Deleting references to the current instruction
               if (queue_peek_tail(branches) == lastinsn)
                  queue_remove_tail(branches);
               //Deleting the instruction
               af->arch->insn_free(lastinsn);
            }

            //Resetting the parser to point at the address of the reset
            fsm_resetstream(fc, reset_addr);
         }

         //In both cases we switch to
         DBGMSGLVL(1,
               "Interworking: Loading another architecture's FSM  (code %d)!\n",
               next_archcode);
         next_driver = dsmbldriver_load_byarchcode(next_archcode);
         fsm_reinit(fc, next_driver->fsmloader);
         current_archcode = next_archcode;

         // Updating architecture related variables
         current_arch = next_driver->getarch();
         lc_free(current_driver);
         current_driver = next_driver;
      }

      DBGMSG0LVL(2, "Endianness: Handling code's endianness\n");
      previous_endian = current_endian;
      current_endian = current_arch->endianness;

      int i = nb_parsed_bytes;
      if (current_endian == CODE_ENDIAN_LITTLE_16B) {
         DBGMSG0LVL(2, "Endianness: CODE_ENDIAN_LITTLE_16B\n");
         DBGMSGLVL(2, "bytes read so far: %d/%d\n", nb_parsed_bytes, bslen);
         DBGMSGLVL(2, "bytes already swapped: %d\n", inverted_bytes);
         // We need to check that the previous swap was made
         // with the same chunk size (here it is 16 bits)
         if ((inverted_bytes != 0)
               && (inverted_bytes < ((int) fsm_getmaxinsnlength(fc) / 8))
               && (previous_endian != CODE_ENDIAN_LITTLE_16B)) {
            // The previous swap was made with another endianness, we have to roll it back.
            // TODO Find a way for handling the rollback without listing every possibility..
            if (previous_endian == CODE_ENDIAN_LITTLE_32B) {
               str[nb_parsed_bytes] = bytestream[nb_parsed_bytes];
               str[nb_parsed_bytes + 1] = bytestream[nb_parsed_bytes + 1];
               str[nb_parsed_bytes + 2] = bytestream[nb_parsed_bytes + 2];
               str[nb_parsed_bytes + 3] = bytestream[nb_parsed_bytes + 3];
            }
         } else if ((inverted_bytes != 0)
               && (inverted_bytes < ((int) fsm_getmaxinsnlength(fc) / 8))) {
            // Else we skip the chunks already swapped
            i = nb_parsed_bytes + inverted_bytes;
         }
         // Stop at the end of the bytesteam
         if (i + 2 <= bslen) {
            // Apply correct endianness for the maximum instruction length of the current architecture
            for (;
                  (i + 2)
                        <= (nb_parsed_bytes
                              + ((int) fsm_getmaxinsnlength(fc) / 8));
                  i = i + 2) {
               unsigned char tmp = str[i];
               str[i] = str[i + 1];
               str[i + 1] = tmp;
            }
            inverted_bytes = (int) fsm_getmaxinsnlength(fc) / 8;
         }
      } else if (current_endian == CODE_ENDIAN_LITTLE_32B) {
         // We need to check that the previous swap was made
         // with the same chunk size (here it is 32 bits)
         if ((inverted_bytes != 0)
               && (inverted_bytes < ((int) fsm_getmaxinsnlength(fc) / 8))
               && (previous_endian != CODE_ENDIAN_LITTLE_32B)) {
            // The previous swap was made with another endianness, we have to roll it back.
            // TODO Find a way for handling the rollback without listing every possiblity..
            if (previous_endian == CODE_ENDIAN_LITTLE_16B) {
               str[nb_parsed_bytes] = bytestream[nb_parsed_bytes];
               str[nb_parsed_bytes + 1] = bytestream[nb_parsed_bytes + 1];
            }
         } else if ((inverted_bytes != 0)
               && (inverted_bytes < ((int) fsm_getmaxinsnlength(fc) / 8))) {
            // Else we skip the chunks already swapped
            i = nb_parsed_bytes + inverted_bytes;
         }
         // Stop at the end of the bytesteam
         if (i + 4 <= bslen) {
            // Apply correct endianness for the maximum instruction length of the current architecture
            for (;
                  (i + 4)
                        <= (nb_parsed_bytes
                              + ((int) fsm_getmaxinsnlength(fc) / 8));
                  i = i + 4) {
               unsigned char tmp = str[i];
               str[i] = str[i + 3];
               str[i + 3] = tmp;
               tmp = str[i + 1];
               str[i + 1] = str[i + 2];
               str[i + 2] = tmp;
            }
            inverted_bytes = (int) fsm_getmaxinsnlength(fc) / 8;
         }
      }

      fsmerror = fsm_parse(fc, (void**) (&current_insn), af,
            af);

//      DBGMSG("INSN: %p\n",current_insn);
      /*Initializes the address of the decoded instruction*/
      insn_set_addr(current_insn, current_addr);
      /*Updates instruction's coding*/
      insn_set_coding(current_insn, NULL, 0, fsm_getcurrentcoding(fc));

      /*Updates the label the instruction belongs to*/
      if (next_fctlbl_addr <= current_addr) {
         //The address of the next label is inferior to the current address: we have to change the label
         lastlbl = fctlabels[last_fctlbl_idx + 1];
         last_fctlbl_idx++;
         DBGMSG("New label %s (type %d) at address %#"PRIx64"\n",
               label_get_name(lastlbl), label_get_type(lastlbl),
               label_get_addr(lastlbl));
         //Updating address of the next label
         if ((last_fctlbl_idx + 1) < (int) n_fctlabels)
            next_fctlbl_addr = label_get_addr(fctlabels[last_fctlbl_idx + 1]);
         else
            next_fctlbl_addr = startaddr + (int64_t) bslen; //Last label, next address is the end of the file

         /*Now we will perform a "sanity check" to see if the last instruction found did not overlap with the new label. If so, we will
          * reset the disassembler to the new label if it is a function. This is to handle the case of functions ending with badly disassembled
          * instructions (for instance if they contain data interleaved with the code) which caused the beginning of the new function to be missed
          * For the record, this is how objdump seems to be doing, and also for the record, this case happens with files compiled with icc where
          * a lot of specific functions are added to the file*/
         if ((label_get_addr(lastlbl) != current_addr) /*It is not defined at the current address*/
         && (label_get_addr(lastlbl) >= startaddr) /*It actually belongs to the section we are parsing*/
         ) {
            insn_t* lastinsn;
            bitvector_t* overlap;

            //Undoing endianness bytes swap (the inverted bytes include the parsed current instruction)
            int i = nb_parsed_bytes + inverted_bytes - 1;
            for (; i >= nb_parsed_bytes; i--)
               str[i] = bytestream[i];
            inverted_bytes = 0;

            //The last label is a new function label but it is not at the same address at the current instruction. Something went wrong
            DBGMSG(
                  "Label %s encountered first at address %#"PRIx64" but is associated to address %#"PRIx64"\n",
                  label_get_name(lastlbl), current_addr,
                  label_get_addr(lastlbl));
            //Removing the last disassembled instructions until we find the one overlapping this label
            while (INSN_GET_ADDR((insn_t* )queue_peek_tail(output))
                  > label_get_addr(lastlbl)) {
               lastinsn = (insn_t*) queue_remove_tail(output);
               DBG(
                     char buf[256];insn_print(lastinsn, buf, sizeof(buf)); DBGMSG("Removing instruction %#"PRIx64":%s\n",INSN_GET_ADDR(lastinsn),buf));
               //Deleting references to the instruction
               if (queue_peek_tail(branches) == lastinsn)
                  queue_remove_tail(branches);
               //Undoing endianness bytes swap for this instruction
               i = nb_parsed_bytes - 1;
               for (; i >= (int) (nb_parsed_bytes - insn_get_bytesize(lastinsn));
                     i--)
                  str[i] = bytestream[i];
               nb_parsed_bytes -= insn_get_bytesize(lastinsn);
               //Deleting the instruction
            }
            //We will split the last disassembled instruction so that it ends at the new label
            lastinsn = (insn_t*) queue_peek_tail(output); //Retrieving the last disassembled instruction
            //Undoing endianness bytes swap
            i = nb_parsed_bytes - 1;
            for (; i >= (int) (nb_parsed_bytes - insn_get_bytesize(lastinsn));
                  i--)
               str[i] = bytestream[i];
            nb_parsed_bytes -= insn_get_bytesize(lastinsn);
            //Ensuring that this is really the first time we encounter the label and that the last instruction overlaps it
            assert(
                  (insn_get_fctlbl(lastinsn)!=lastlbl) && (INSN_GET_ADDR(lastinsn) < label_get_addr(lastlbl)));
            //Cutting the coding of the last encountered instruction so that we remove the overlap
            overlap = bitvector_cutright(insn_get_coding(lastinsn),
                  (current_addr - label_get_addr(lastlbl)) * 8);
            DBG(
                  char buf[256];bitvector_hexprint(overlap, buf, sizeof(buf), " "); DBGMSG("Last instruction overlapped with the new label %s on the bytes %s\n",label_get_name(lastlbl),buf););
            //Deleting the overlap, we will disassemble it again
            bitvector_free(overlap);
            //Even if it was not flagged as a "bad" instruction, we now know that is certainly is one
            insn_set_opcode(lastinsn, BAD_INSN_CODE);    //Bad instruction, bad
            insn_set_nb_oprnds(lastinsn, 0);               //No operand for you
            insn_set_annotate(lastinsn, A_NA);          //And no annotate either
            //Deleting references to the last instruction
            if (queue_peek_tail(branches) == lastinsn)
               queue_remove_tail(branches);
            //Deleting the current instruction, it is very probably badly disassembled
            insn_free(current_insn);
            //Resetting the parser to point at the address of the label (most likely the beginning of a function)
            fsm_resetstream(fc, label_get_addr(lastlbl));
            //Resetting the error counter
            errcount = 0;
            //Updating the variables used for retrieving bytes swapped
            current_addr = fsm_getcurrentaddress(fc);
            //Resuming the parsing
            continue;
         }         //End of the processing of an instruction overlapping a label
      }            // Otherwise there is no need to update the label
      insn_link_fct_lbl(current_insn, lastlbl);
      DBGMSGLVL(1, "Instruction follows label %s\n",
            (lastlbl!=NULL)?label_get_name(insn_get_fctlbl(current_insn)):"<none found>");

      //Linking other labels from the section to the instructions
      while ((lblidx < n_labels)
            && (label_get_addr(labels[lblidx]) == current_addr)) {
         //A label in the section has the same address as the current instruction
         if (labels[lblidx] != lastlbl) {
            //This label is not the current label (this test is mainly to avoid affecting it twice)
            label_set_target_to_insn(labels[lblidx], current_insn);
            DBGMSGLVL(1, "Linking non-function label %s to instruction %p\n",
                  label_get_name(labels[lblidx]), current_insn);
         } //Otherwise, we just did associating this label with this instruction
         lblidx++; //On to the next label
      } //The loop is for handling the case of multiple labels at the same address

      /*There is no need to try updating the branches if the instruction was not correctly disassembled*/
      if (!ISERROR(fsmerror)) {
         //Updates references between this instruction and the binary file or other instructions
         asmfile_upd_references(af, bf, current_insn, current_addr, varlabels,
               n_varlabels, unlinked_targets, branches);
         //Flagging the instruction as suspicious if there were errors before (no need to flag badly disassembled instructions)
         if (errcount > 0) {
            insn_add_annotate(current_insn, A_SUSPICIOUS);
            DBGMSG("Instruction at address %#"PRIx64" flagged as suspicious\n",
                  INSN_GET_ADDR(current_insn));
         }
         if (errcount > 0) {
            errcount--;
            /*Decrements the number of consecutive errors. We don't set it to 0 as we want to be able to detect an correctly parsed
             * instruction in the middle of a block of errors (it is very probably an error as well)*/
         }
         //Stores in the file that the instruction set for which this instruction is defined is used
         asmfile_set_iset_used(af, insn_get_iset(current_insn));
      } else {
         errcount++;            //Increments the counter of consecutive errors
         insn_set_arch(current_insn, current_arch);
      }

      // add by M.T: update annotate field in instruction corresponding to the section
      insn_add_annotate(current_insn, scnanno);

      /*Adds instruction to list*/
      add_insn_to_insnlst(current_insn, output);

      DBGLVL(1,
            { char buf1[256];char buf2[256]; insn_print(current_insn, buf1, sizeof(buf1)); bitvector_hexprint(insn_get_coding(current_insn), buf2, sizeof(buf2), " "); DBGMSG("Decoded instruction %#"PRIx64": %s (%p)\t%s\n",INSN_GET_ADDR(current_insn),buf2,current_insn,buf1); });
   }
   DBGMSGLVL(3, "%#"PRIx64": Insn %p label %s (%p) points to %p\n",
         insn_get_addr(current_insn), current_insn, label_get_name(lastlbl),
         lastlbl, label_get_target(lastlbl));

   DBGMSG("Processing complete : %"PRId64" bytes processed\n",
         fsm_getcurrentaddress(fc) - startaddr);

   /*Checks the architecture state*/
   if (asmfile_get_arch_code(af) != current_archcode) {
      fsm_reinit(fc,
            dsmbldriver_load_byarchcode(asmfile_get_arch(af)->code)->fsmloader);
   }

   /*Frees the global variables*/
   fsm_parseend(fc);

   lc_free(str);
   lc_free(current_driver);
   lc_free(lastlabel);

   DBGMSG0("\nParsing complete\n");

   return output;
}

/**
 * Parses a stream of bytes depending on the associated architecture and stops after disassembling a single instruction
 * \param fc The context of the FSM used to disassemble this stream
 * \param bytestream The stream of bytes
 * \param bslen The length of the stream of bytes
 * \return A disassembled instruction
 * */
static insn_t* stream_parse_single(fsmcontext_t* fc, asmfile_t* af,
      unsigned char* bytestream, int bslen)
{
   insn_t* insn = NULL;
   int fsmerror;

   DBGMSG0("Entering\n");

   /*Updates the FSM */
   fsm_setstream(fc, bytestream, bslen, 0);
   fsm_parseinit(fc);

   fsmerror = fsm_parse(fc, (void**) (&insn), af, af);

   if (fsmerror != EXIT_SUCCESS)
      asmfile_set_last_error_code(af, fsmerror);

   /*Updates instruction's coding*/
   insn_set_coding(insn, NULL, 0, fsm_getcurrentcoding(fc));

   /*Frees the global variables*/
   fsm_parseend(fc);

   return insn;
}

/** 
 * Detects gaps between a new list of instruction and the existing one to which it is appended, and updates
 * the flag signaling the beginning or end of an instruction list accordingly
 * \note This function is simply here for code clarity
 * \param asminsns Existing list of instructions
 * \param insn_queue List appended to \c asminsns
 * */
void detect_gaps(queue_t* asminsns, queue_t* insn_queue)
{
   assert(asminsns && insn_queue);

   insn_t* last = queue_peek_tail(asminsns);
   if (!last) {
      //List of instructions to append to is empty: first appended instruction is the last of the list
      insn_add_annotate(queue_peek_head(insn_queue), A_BEGIN_LIST);
   } else {
      //Not empty: checking whether there is a gap between the last instruction and the first of the appended list
      if ((insn_get_addr(last) + insn_get_size(last) / 8)
            < insn_get_addr(queue_peek_head(insn_queue))) {
         //The previous list ends before the new one begins: we flag the instructions as such
         insn_add_annotate(last, A_END_LIST);
         insn_add_annotate(queue_peek_head(insn_queue), A_BEGIN_LIST);
      } //Otherwise the new list directly follows the existing list
   }
}

/*** High-level functions ***/
/**
 * Disassembles a elf file from its parsed elf file content
 * \param af A pointer to the asmfile_t structure that will receive the disassembled file. The
 * efile member of the structure must point to a parsed ELF file
 * \param pf A pointer to the parsefile_t structure for this file
 * \return EXIT_SUCCESS if the file was successfully disassembled, error code otherwise
 * */
static int disassemble_parsed_asmfile(asmfile_t* af)
{

   uint64_t bslen = 0, i;
   int64_t startaddr;
   unsigned char* bytestream = NULL;
   queue_t* insn_queue, *asminsns;
   fsmcontext_t* fc;
   int res = EXIT_SUCCESS;

   assert(af != NULL);
   binfile_t* bf = asmfile_get_binfile(af);
   assert(bf);

   /*Retrieves the driver to use to parse the file, depending on architecture*/
   dsmbldriver_t* driver = dsmbldriver_load(binfile_get_arch(bf));
   if (driver == NULL) {
      return ERR_DISASS_ARCH_NOT_SUPPORTED;
   }

   /*Retrieves a pointer to the list of instructions in the ASM file*/
   asminsns = asmfile_get_insns(af);

   /*Initialises the FSM*/
   fc = fsm_init(driver->fsmloader);

   /*Sets the error handler*/
   fsm_seterrorhandler(fc, error_handler);

   DBGMSG("FileName: %s\n", asmfile_get_name(af));

   //Retrieves a list of data elements containing pointers with an unknown destination from the binfile
   queue_t* unlinked_targets = binfile_lookup_unlinked_ptrs(bf);

   //Initialises the list of branch instructions
   queue_t* branches = queue_new();

   /*Disassemble all sections containing program data*/
   for (i = 0; i < binfile_get_nb_code_scns(bf); i++) {
      binscn_t* scn = binfile_get_code_scn(bf, i);

      DBGMSG("Disassembling section %s\n", binscn_get_name(scn));

      /*Gets the section's bytecode*/
      bytestream = binscn_get_data(scn, &bslen);
      startaddr = binscn_get_addr(scn);

      //Removes elements from the queue of data with unlinked pointers whose pointer references an address below the first in the section
      if (unlinked_targets) {
         while ((queue_length(unlinked_targets) > 0)
               && (pointer_get_addr(
                     data_get_pointer(
                           (data_t*) queue_peek_head(unlinked_targets)))
                     < startaddr)) {
            queue_remove_head(unlinked_targets);
         }
      }

      /*Parse instructions*/
      if (bytestream != NULL) {
         /*Invoke the parser on the bytecode and retrieve the disassembled instruction list*/
         insn_queue = stream_parse(fc, af, bytestream, bslen, startaddr, scn,
               unlinked_targets, branches);

         /*Updates the current section first and last instruction*/
         binscn_set_first_insn_seq(scn, queue_iterator(insn_queue));
         binscn_set_last_insn_seq(scn, queue_iterator_rev(insn_queue));

         //Detects if the instruction list contains gaps between the previous section
         detect_gaps(asminsns, insn_queue);

         /*Adds the disassembled instruction list to the list of instructions in the ASM file*/
         queue_append(asminsns, insn_queue);
         /**\todo For later evolutions, handle the fact that we could be disassembling instructions not sequentially,
          * so we would need to insert instructions in the middle of the instruction list*/
      }
   }
   //Flagging the last instruction as the last in the list
   insn_add_annotate(queue_peek_tail(asminsns), A_END_LIST);

   /*Calculates the branches inside the instruction lists*/
   asmfile_upd_insns_with_branches(af, branches);

   //Removing remaining unlinked targets
   if (unlinked_targets)
      queue_free(unlinked_targets, NULL);

   //Frees the list of branch instructions
   queue_free(branches, NULL);

   fsm_terminate(fc);
   dsmbldriver_free(driver);

   return res;
}
#if 0
/**
 * Loads the content of an ELF file into an asmfile structure, but without disassembling it
 * \param A pointer to the structure holding the ASM file. It must contain the name of the file to disassemble.
 * If the parsing is successful, it will contain the label hashtables and a pointer to the object containing the parsed file
 * \param options Options for disassembly
 * \return EXIT_SUCCESS if the parsing was successful or if the file was already parsed, error code otherwise
 * */
static int asmfile_parse(parsedfile_t* pf, asmfile_t* af, int options)
{
   (void) options;
   int res = EXIT_SUCCESS;
   if (asmfile_test_analyze(af, PAR_ANALYZE) == FALSE) {
      elffile_t* efile = NULL;

      if (asmfile_get_origin(af) != NULL) {
         //Handling a case where the ELF file has already been parsed but the asmfile has not
         //been updated to contain the result. Simply retrieving the parsed file
         efile = asmfile_get_origin(af);

      } else {
         if ((asmfile_get_name(af) != NULL )&& (asmfile_get_name(af) != PTR_ERROR)) {
               && (asmfile_get_name(af) != PTR_ERROR)) {
            //Opening the file
            FILE* fileid = fopen(asmfile_get_name(af), "r");

            if (fileid != NULL) {
               //Parsing the ELF file ignoring any members if this is an archive (this
               //would have to be taken care of earlier)
               efile = parse_elf_file(asmfile_get_name(af), fileid, NULL, NULL,
                     af);
            } else {
               ERRMSG("Unable to open file %s for reading\n",
                     asmfile_get_name(af));
               return ERR_COMMON_UNABLE_TO_OPEN_FILE;
            }
         } else {
            ERRMSG(
                  "File name unspecified. Unable to open file for parsing and disassembly\n");
            return ERR_COMMON_FILE_NAME_MISSING;
         }
      }
      res = elffile_get_last_error_code(efile);
      if (ISERROR(res))
      return res;
      //Adds the parsed ELF file to the ASM file, along with the function for freeing it
      asmfile_set_origin(af, efile, asmfile_disassembled_free, ASMF_ORIGIN_ELF);

      //Updates the asmfile with informations about the architecture
      res = parsedfile_fill(pf, af, NULL);
      if (ISERROR(res))
         return res;

      //Adds labels for the PLT section
      res = asmfile_add_plt_labels(af);
      if (ISERROR(res)) {
         DBGMSG("%s: %s (%x)",
               errcode_getmsg(WRN_DISASS_EXT_FCTS_LBLS_NOT_RETRIEVED),
               errcode_getmsg(res), res);
         res = WRN_DISASS_EXT_FCTS_LBLS_NOT_RETRIEVED;
      }

      //Loads the labels list and hashtable into the ASM file
      if (efile->labels != NULL) {
         /**\todo Fill in the type and target for the labels. Find a more efficient way to fill this table
          * than by scanning another table*/
         FOREACH_INHASHTABLE(efile->labels, iter)
         {
            //Creates the label
            label_t* lbl = elffile_label_getaslabel(efile,
                  GET_DATA_T(tableentry_t*, iter));
            DBGMSG("Added label %s (pointing to %#"PRIx64") to asmfile\n",
                  label_get_name(lbl), label_get_addr(lbl));
            //Adds the label into the label hashtable and to the ordered list of labels
            asmfile_add_label_unsorted(af, lbl);
            //If it is a dummy label, store it into the list of labels to update afterwards
            if (label_get_type(lbl) == LBL_DUMMY)
               queue_add_tail(pf->dummylabels, lbl);
         }
      }
      //ADD LABELS FROM DEBUG SECTIONS
      res = asmfile_add_debug_labels(af);
      if (ISERROR(res)) {
         DBGMSG("%s: %s (%x)",
               errcode_getmsg(WRN_DISASS_DBG_LBLS_NOT_RETRIEVED),
               errcode_getmsg(res), res);
         res = WRN_DISASS_DBG_LBLS_NOT_RETRIEVED;
      }

      //Finalises the update of the labels
      asmfile_upd_labels(af);

      //In some binaries, there is no label at the beginning of code sections. Iterate over sections
      //and check if a label should be added.
      /**\todo Move this before the invocation of asmfile_upd_labels*/
      int i = 0;
      for (i = 0; i < pf->n_codescn_idx; i++) {
         int scnid = pf->codescn_idx[i];
         label_t* lastlabel = asmfile_get_last_label(af,
               efile->sections[scnid]->scnhdr->sh_addr, NULL);

         // If there is no label or if the last label is not at the section start
         // create a label
         if (lastlabel == NULL
               || lastlabel->address
                     != (int64_t) efile->sections[scnid]->scnhdr->sh_addr
               || label_get_type(lastlabel) >= LBL_NOFUNCTION) {
            char* lblname = lc_malloc(
                  (strlen(elfsection_name(efile, scnid)) + strlen("@start") + 1)
                        * sizeof(char));
            sprintf(lblname, "%s%s", elfsection_name(efile, scnid), "@start");
            DBGMSG("Create label %s at address 0x%"PRIx64"\n", lblname,
                  efile->sections[scnid]->scnhdr->sh_addr);
            label_t* lab = label_new(lblname,
                  efile->sections[scnid]->scnhdr->sh_addr, TARGET_INSN, NULL);
            if (get_insnannotate(elfsection_name(efile, scnid)) == A_PATCHMOV) {
               DBGMSG("Label %s marks the beginning of a patched section\n",
                     lblname);
               label_set_type(lab, LBL_PATCHSCN); //Detects if this section contains code moved by a patch operation
            }
            asmfile_add_label(af, lab);
            lc_free(lblname);
         }
      }
      /*
       for (i = 0; i < af->n_fctlabels; i++)
       printf ("0x%"PRIx64"  %s\n", af->fctlabels[i]->address, af->fctlabels[i]->name);
       */
      //Updates the analysis status of the file
      asmfile_add_analyzis(af, PAR_ANALYZE);

   } else {
      elffile_t* efile = asmfile_get_origin(af);
      res = elffile_get_last_error_code(efile);
      if (ISERROR(res)) {
         ERRMSG("Unable to retrieve origin binary for parsed file %s\n",
               asmfile_get_name(af));
         return res;
      } else {
         pf = parsedfile_init(af, NULL);
         DBGMSG0("File has already been parsed: no operation performed\n");
      }
   }
   return res;
}
#endif
/*
 * Disassembles a stream of bytes into a list of instructions
 * \param af Structure containing the resulting list of instructions
 * \param stream The stream
 * \param len The size in bytes of the stream
 * \param startaddr Address of the first instruction in the stream
 * \param arch Structure containing the architecture for which the stream is to be disassembled
 * \param archname Name of the architecture. Will be used if \c arch is NULL
 * \return EXIT_SUCCESS if the stream could be successfully disassembled, error code otherwise
 * */
int stream_disassemble(asmfile_t* af, unsigned char* stream, uint64_t len,
      int64_t startaddr, arch_t* arch, char* archname)
{
   /**\todo Streamline this, it was a quick and dirty fix for an urgent need*/
   if (!af) {
      ERRMSG("Unable to disassemble stream: asmfile structure is NULL\n");
      return ERR_LIBASM_MISSING_ASMFILE;
   }
   if (!stream || !len) {
      ERRMSG("Unable to disassemble stream: stream is NULL or length is zero\n");
      return ERR_DISASS_STREAM_EMPTY;
   }
   if (!asmfile_get_arch(af) && !arch && !archname) {
      ERRMSG("Unable to disassemble stream: no architecture given\n");
      return ERR_LIBASM_ARCH_MISSING;
   }
   dsmbldriver_t* driver = NULL;
   if (arch)
      driver = dsmbldriver_load(arch);
   if (!driver && archname)
      driver = dsmbldriver_load_byarchname(archname);
   if (!driver) {
      ERRMSG("Unable to create disassembly driver for architecture %s\n",
            (arch) ? arch_get_name(arch) : archname);
      return ERR_DISASS_ARCH_NOT_SUPPORTED;
   }
   if (!arch && !archname)
      arch = asmfile_get_arch(af);

   /*Updates the architecture for the ASM file*/
   if (!asmfile_get_arch(af))
      asmfile_set_arch(af, driver->getarch());

   /*Retrieves a pointer to the list of instructions in the ASM file*/
   queue_t* asminsns = asmfile_get_insns(af);

   /*Initialises the FSM*/
   fsmcontext_t* fc = fsm_init(driver->fsmloader);

   /*Sets the error handler*/
   fsm_seterrorhandler(fc, error_handler);

   //Initialises the list of branch instructions
   queue_t* branches = queue_new();

   /*Invoke the parser on the bytecode and retrieve the disassembled instruction list*/
   queue_t* insn_queue = stream_parse(fc, af, stream, len, startaddr, NULL,
         NULL, branches);

   /*Adds the disassembled instruction list to the list of instructions in the ASM file*/
   queue_append(asminsns, insn_queue);

   /*Calculates the branches inside the instruction lists*/
   asmfile_upd_insns_with_branches(af, branches);

   //Frees the list of branch instructions
   queue_free(branches, NULL);

   fsm_terminate(fc);

   /*Updates the analysis status of the file*/
   asmfile_add_analyzis(af, DIS_ANALYZE);

   return EXIT_SUCCESS;
}

/*
 * Performs a disassembly of the instructions in a file already containing instructions and their coding,
 * and update the existing structures with the informations from the disassembly
 * \param af Asmfile
 * \return EXIT_SUCCESS / error code
 * */
int asmfile_disassemble_existing(asmfile_t* af)
{
   if (!af)
      return ERR_LIBASM_MISSING_ASMFILE;
   int res = EXIT_SUCCESS;

   dsmbldriver_t* driver = dsmbldriver_load_byarchname(
         arch_get_name(asmfile_get_arch(af)));
   /*Initialises the FSM*/
   fsmcontext_t* fc = fsm_init(driver->fsmloader);
   /*Sets the error handler*/
   fsm_seterrorhandler(fc, error_handler);

   FOREACH_INQUEUE(asmfile_get_insns(af), iter) {
      insn_t* current = GET_DATA_T(insn_t*, iter);
      unsigned char* stream;
      int streamlen;
      insn_t* parsed = NULL;
      stream = bitvector_charvalue(insn_get_coding(current), &streamlen,
            arch_get_endianness(insn_get_arch(current)));
      if (stream && streamlen > 0)
         parsed = stream_parse_single(fc, af, stream, streamlen);
      if (!parsed) {
         ERRMSG(
               "Unable to disassemble coding of instruction at address %#"PRIx64"\n",
               insn_get_addr(current));
         if (res == EXIT_SUCCESS) {
            res = asmfile_get_last_error_code(af);
            if (!ISERROR(res)) {
               res = WRN_DISASS_INCOMPLETE_DISASSEMBLY;
            }
         }
      } else if (insn_equal(current, parsed) == FALSE) {
         WRNMSG(
               "Coding of instruction at address %#"PRIx64" was disassembled into a different instruction\n",
               insn_get_addr(current));
         if (res == EXIT_SUCCESS) {
            res = asmfile_get_last_error_code(af);
            if (!ISERROR(res)) {
               res = WRN_DISASS_INCOMPLETE_DISASSEMBLY;
            }
         }
      } else {
         /**\todo (2015-07-06) This could be done by a separate function defined in libmasm.h, but it is very
          * dependent on what we retrieve during disassembly that we can't have otherwise, so I keep it here
          * as the knowledge is more or less disassembly-specific*/
         uint8_t i;
         //Parsed instruction is identical to the original: we can copy values initialised by disassembly to the original instruction
         //Updating instruction variant
         insn_set_variant_id(current, insn_get_variant_id(parsed));
         //"Stealing" the extension from the parsed instruction to the original one
         insn_set_ext(current, insn_get_ext(parsed));
         insn_set_ext(parsed, NULL);
         //Updates element sizes
         insn_set_input_element_size_raw(current,
               insn_get_input_element_size_raw(parsed));
         insn_set_output_element_size_raw(current,
               insn_get_output_element_size_raw(parsed));
         //Updates element types
         insn_set_input_element_type(current,
               insn_get_input_element_type(parsed));
         insn_set_output_element_type(current,
               insn_get_output_element_type(parsed));
         //Updates read_size
         insn_set_read_size(current, insn_get_read_size(parsed));
         //Updates annotate
         insn_add_annotate(current, insn_get_annotate(parsed));
         //Updates attributes and roles
         for (i = 0; i < insn_get_nb_oprnds(parsed); i++) {
            oprnd_t* op_parsed = insn_get_oprnd(parsed, i);
            oprnd_t* op_current = insn_get_oprnd(current, i);
            oprnd_set_bitsize(op_current, oprnd_get_bitsize(op_parsed));
            oprnd_set_role(op_current, oprnd_get_role(op_parsed));
         }
         //Freeing the parsed instruction
         driver->getarch()->insn_free(parsed);
      }
      if (stream)
         lc_free(stream);
   }
   /*Updates the analysis status of the file*/
   asmfile_add_analyzis(af, DIS_ANALYZE);

   //Terminates the FSM
   fsm_terminate(fc);

   //Frees the driver
   dsmbldriver_free(driver);

   return res;
}

/*
 * Performs a raw disassembly of a file whose name is contained into an asmfile structure
 * The raw content of the file will be disassembled without any preliminary parsing of the file. This function
 * is mainly intended for allowing comparisons with other disassemblers or to disassemble a file containing the
 * extracted binary code of another. Use with caution, especially for the
 * \param af A pointer to the structure holding the ASM file. It must contain the name of the file to disassemble
 * If the disassembly is successful, it will contain the instructions from the disassembled file
 * \param offset The offset at which to start disassembly in the file
 * \param len The number of bytes to disassemble in the file. If 0 the whole length will be disassembled
 * \param startaddr Address associated to the first instruction.
 * \param archname Name of the architecture the file is defined in
 * \return EXIT_SUCCESS if the file could be successfully disassembled, error code otherwise
 * */
int asmfile_disassemble_raw(asmfile_t* af, uint64_t offset, uint64_t len,
      int64_t startaddr, char* archname)
{

   if (!asmfile_get_name(af))
      return ERR_COMMON_FILE_NAME_MISSING;
   FILE* filestream = NULL;
   size_t filelen = 0;

   if (!fileExist(asmfile_get_name(af))) {
      ERRMSG("Unable to open file %s\n", asmfile_get_name(af));
      return ERR_COMMON_FILE_NOT_FOUND;
   }

   unsigned char* stream = (unsigned char*) getFileContent(asmfile_get_name(af),
         &filestream, &filelen);

   /*Limits offset if larger than the file itself*/
   if (offset > filelen)
      offset = 0;
   /*Limits size if larger than the file itself*/
   if ((len == 0) || (filelen < (len + offset)))
      len = filelen - offset;

   int dissres = stream_disassemble(af, stream + offset, len, startaddr, NULL,
         archname);

   releaseFileContent((char*) stream, filestream);

   return dissres;
}

// New functions using binfiles //
/////////////////////////////////

/**
 * Creates data structures for sections containing data
 * \param asmf Structure representing an assembly file. Its binfile member must have been set
 * \return EXIT_SUCCESS if successful, error code otherwise
 * */
static int asmfile_parsedata_fromlabels(asmfile_t* asmf)
{
   assert(asmf && asmfile_get_binfile(asmf));
   binfile_t* bf = asmfile_get_binfile(asmf);
   unsigned int i;

   /*Hou ça fait longtemps (2014-07-15). Bon en gros l'algo, ce sera de récupérer pour chaque section
    * marquée comme étant de type data parmi les loadscns le tableau des labels associés, puis d'initialiser
    * les entries de ce tableau à la taille du tableau de labels.
    * Ensuite on parcourt tranquillement du premier à l'avant dernier label en créant un objet data
    * à cette adresse, associé au label correspondant, et dont la taille est la différence entre son adresse et la suivante.
    * Le dernier a pour taille la différence entre son adresse et la fin de la section
    * Oh, et faudra aussi ajouter les data qu'on a créés dans le asmfile. En fait, je me demande si on les crée dans le binfile
    * comme je suis en train de faire, ou dans le asmfile directement. Raaaah flûte*/

   /**\todo TODO (2014-10-27) I have absolutely no clue whether what is implemented below is what is described in the algorithm above (in French)
    * Check this, especially with regard to the last data. I might need to use binfile_adddata, which I created for the instructions
    * (2014-11-24) And since I'm there, I should also factorise the code here, especially for the loop and its last element
    * (2014-12-02) Also use varlabel in asmfile now. The only trouble is that we know in which section we are when using the labels by
    * section arrays.*/

   for (i = 0; i < binfile_get_nb_load_scns(bf); i++) {
      binscn_t* scn = binfile_get_load_scn(bf, i);
      //Scans the loaded sections marked as data
      if ((binscn_get_type(scn) != SCNT_CODE)
            && (binscn_get_nb_entries(scn) == 0)) {
         uint32_t nlbls = 0, j;
         //Retrieves the labels associated to this section
         label_t** lbls = binfile_get_labels_by_scn(bf, binscn_get_index(scn),
               &nlbls);
         /**\todo TODO (2014-11-28) See how we could get rid of the building of this array here (or maybe putting it in the binfile_t) and get
          * rid of all this crap with the lastlblidx_byscn and nextlbldix_byscn and what not*/
         // First builds the array of labels that can be associated to variables
         uint32_t n_varlbls = 0; //Number of labels that can be associated to variables
         label_t** varlbls = lc_malloc0(sizeof(*varlbls) * nlbls); //Array of labels that can be associated to variables (initialised to max possible size)
         for (j = 0; j < nlbls; j++) {
            if (label_get_type(lbls[j]) == LBL_VARIABLE)
               varlbls[n_varlbls++] = lbls[j];
         }
         DBGMSG(
               "Creating data structures for variables in section %s (contains %u variable label%s out of %u)\n",
               binscn_get_name(scn), n_varlbls, (nlbls > 1) ? "s" : "", nlbls);
         if (n_varlbls <= 1) {
            //One or no label is associated to this section: we create a single data entry for the whole section
            binscn_set_nb_entries(scn, 1);
            DBGMSGLVL(1, "Creating single data entry for section %s%s%s\n",
                  binscn_get_name(scn), (n_varlbls == 1) ? ": " : "",
                  (n_varlbls == 1) ? label_get_name(varlbls[0]) : "");
            uint64_t datalen = 0;
            void* scndata = binscn_get_data(scn, &datalen);
            data_t* entry = data_new(DATA_RAW, scndata, datalen);
//            //Updates the address of the entry
//            data_set_addr(entry, binscn_get_addr(scn));
            //Adds the entry to the list of entries
            binscn_add_entry(scn, entry, 0);
            if (n_varlbls == 1) {
               //Links the label to the entry if there is one defined for this section
               data_link_label(entry, varlbls[0]);
            }
         } else {
            /**\todo TODO (2014-12-03) Handle the labels that have the same address as the end of the section or beyond (they will probably happen)*/
            //More than 1 label
            //Initialises the number of entries in the section based on the number of labels it contains.
            binscn_set_nb_entries(scn, n_varlbls);
            //Creates a data entry for each label
            for (j = 0; j < (n_varlbls - 1); j++) {
               /**\todo (2014-11-25) Improve this to avoid too many tests. I want to keep the fact that the first label at a given address
                * will be given preference to have the same behaviour as for instructions so I can't use the while loop at the beginning of the
                * for loop but there are too many tests in the for loop
                * => (2014-11-28) Chaaaaangiiiing! It did not work anyway because of the bloody motherfucking labels with identical addresses,
                * so now I'm flagging labels eligible to be associated to data entries with type LBL_VARIABLE while the others are LBL_NOVARIABLE
                * (sounds familiar ?) during parsing in the libbin, then I'm using these results*/
//               if (j < (n_varlbls -1) )  {
               assert(
                     label_get_addr(varlbls[j])
                           != label_get_addr(varlbls[j + 1])); //Because that's how we built the bloody LBL_VARIABLE labels
                           /**\todo (2014-07-16) This reuses some code from binfile_adddata. See how we can mutualise that
                            * Either we move this function to binfile, or we separate binfile_adddata into smaller components
                            * and use one of them here*/
               DBGMSGLVL(1,
                     "Creating data entry for label %s at address %#"PRIx64" and size %"PRIu64" bytes\n",
                     label_get_name(varlbls[j]), label_get_addr(varlbls[j]),
                     label_get_addr(varlbls[j + 1])
                           - label_get_addr(varlbls[j]));
               data_t* entry = data_new(DATA_RAW,
                     binscn_get_data_at_offset(scn,
                           (uint64_t) (label_get_addr(varlbls[j])
                                 - binscn_get_addr(scn))),
                     label_get_addr(varlbls[j + 1])
                           - label_get_addr(varlbls[j]));
               //Updates the address of the entry
               data_set_addr(entry, label_get_addr(varlbls[j]));
               /**\note (2014-12-03) The label is automatically associated to the entry in binscn_add_entry, but since we know
                * it here it saves us the bother of finding which one to associate in binscn_add_entry, and it allows to handle
                * the annoying case of labels associated to a section but with an address below the starting address of the section
                * (no, that should not be possible, but ELF did it)
                * => (2015-09-29) Now tweaking this. We may have to create a specific function for that, as data_set_addr, data_link_label
                * and binscn_add_entry are all partially redundant*/
               //Links the label to the entry
               data_link_label(entry, varlbls[j]);
               //Adds the entry to the list of entries
               binscn_add_entry(scn, entry, j);
//               }
//               //Skips labels with an identical address to avoid creating redundant data
//               while ( (j < (n_varlbls -1) ) && (label_get_addr(varlbls[j]) == label_get_addr(varlbls[j+1])) )
//                  j++;
            }
            if (j == (n_varlbls - 1)) {
               //Creates the data entry for the last label. At this point, j contains the index of the last label
               DBGMSGLVL(1,
                     "Creating data entry for label %s at address %#"PRIx64" and size %"PRIu64" bytes\n",
                     label_get_name(varlbls[j]), label_get_addr(varlbls[j]),
                     binscn_get_addr(scn) + binscn_get_size(scn)
                           - label_get_addr(varlbls[j]));
               data_t* entry = data_new(DATA_RAW,
                     binscn_get_data_at_offset(scn,
                           (uint64_t) (label_get_addr(varlbls[j])
                                 - binscn_get_addr(scn))),
                     binscn_get_addr(scn) + binscn_get_size(scn)
                           - label_get_addr(varlbls[j]));
               //Updates the address of the entry
               data_set_addr(entry, label_get_addr(varlbls[j]));
               //Links the label to the entry
               data_link_label(entry, varlbls[j]);
               //Adds the entry to the list of entries
               binscn_add_entry(scn, entry, j);
            }
         }
         lc_free(varlbls); //Freeing the temporary array. We really should put this in binfile_t instead
      }
   }
   return EXIT_SUCCESS;
}

/**
 * Loads the results of a parsed binary file into the asmfile_t structure
 * \param asmf Structure representing an assembly file. Its binfile member must have been set
 * \return EXIT_SUCCESS if successful, error code otherwise
 * */
static int asmfile_loadbinfile(asmfile_t* asmf)
{
   assert(asmf && asmfile_get_binfile(asmf));
   unsigned int i;
   binfile_t* bf = asmfile_get_binfile(asmf);
   int out = EXIT_SUCCESS;
   /*Updates the architecture for the ASM file*/
   asmfile_set_arch(asmf, binfile_get_arch(bf));

   //label_t** labels = binfile_get_file_labels(bf);
   // Loads all labels from the binfile into the asmfile. Stores label not linked to an object and not tied to an executable code section
   /**\todo TODO (2014-11-28) We could use this pass to only load the labels we find interesting: the dummy labels we added with the patcher,
    * the LBL_FUNCTION or LBL_GENERIC, and the LBL_VARIABLE (which would simplify our task in asmfile_parsedata_fromlabels). The trouble then
    * would be to know which labels must be deleted in asmfile and which in binfile.*/
   for (i = 0; i < binfile_get_nb_labels(bf); i++) {
      label_t* lbl = binfile_get_file_label(bf, i);

      //First, additional check if the debug data is present and the label is not of type function
      if ((label_get_type(lbl) < LBL_NOFUNCTION)
            && (label_get_type(lbl) != LBL_FUNCTION)) {
         //Retrieves the debug object at the address of the label
         char* dbgfct = asmfile_has_dbg_function(asmf, label_get_addr(lbl),
               label_get_addr(lbl), NULL);
         if (dbgfct && str_equal(dbgfct, label_get_name(lbl)))
            label_set_type(lbl, LBL_FUNCTION); //Label is marked as being a function in the debug info
         else
            label_set_type(lbl, LBL_NOFUNCTION); //Debug information is present and does not mark the label as a function
      }

      DBGMSG(
            "Adding label %s (pointing to %#"PRIx64" in section %s) to asmfile\n",
            label_get_name(lbl), label_get_addr(lbl),
            binscn_get_name(label_get_scn(lbl)));

      //Adds the label into the label hashtable and to the ordered list of labels
      asmfile_add_label_unsorted(asmf, lbl);

//      //If the label is not eligible to be associated to an instruction and not linked to an object, store it into the list of unlinked labels
//      if (!label_get_target(lbl) && (label_get_type(lbl) >= LBL_NOFUNCTION) )
//         queue_add_tail(unlinked_lbls, lbl);
   }

   // Adds labels from the external functions (they are created as function labels, so they will not be added to the queue of unlinked labels)
   int res = asmfile_add_ext_labels(asmf);
   if (ISERROR(res)) {
      WRNMSG(
            "Unable to add labels for external functions to the representation of file %s\n",
            asmfile_get_name(asmf));
      out = WRN_DISASS_EXT_FCTS_LBLS_NOT_RETRIEVED;
   }

   // Adds labels from debug sections (they are created as function labels, so they will not be added to the queue of unlinked labels)
   res = asmfile_add_debug_labels(asmf);
   if (ISERROR(res)) {
      WRNMSG(
            "Unable to add labels from debug section to the representation of file %s\n",
            asmfile_get_name(asmf));
      out = WRN_DISASS_DBG_LBLS_NOT_RETRIEVED;
   }

   //Sorts the existing labels in the file to allow a search by address
   asmfile_sort_labels(asmf);

   // Adds labels at the beginning of sections
   //In some binaries, there is no label at the beginning of code sections. Iterate over sections
   //and check if a label should be added.

   for (i = 0; i < binfile_get_nb_code_scns(bf); i++) {
      /**\todo Check if we do this for code sections only or for all loaded sections (2014-05-27)*/
      binscn_t* scn = binfile_get_code_scn(bf, i);
      int64_t scnaddress = binscn_get_addr(scn);
      label_t* lastlabel = NULL;
      list_t* lastlabelseq = NULL;
      lastlabel = asmfile_get_last_label(asmf, scnaddress, &lastlabelseq);
      //Attempts to find a label of type function associated to this section at at this address
      while (lastlabel && label_get_addr(lastlabel) == scnaddress
            && strlen(label_get_name(lastlabel)) == 0
            && label_get_scn(lastlabel) == scn
            && !label_is_type_function(lastlabel)) {
         lastlabel = asmfile_get_last_label(asmf, scnaddress, &lastlabelseq);
         lastlabelseq = list_getnext(lastlabelseq);
      }

      // If there is no label or if the last label is not at the section start
      // create a label
      if ((lastlabel == NULL)
            || label_get_addr(lastlabel) != (int64_t) scnaddress
            || strlen(label_get_name(lastlabel)) == 0
            || label_get_scn(lastlabel) != scn
            || !label_is_type_function(lastlabel)) {
         char* scnname = binscn_get_name(scn);
         char* lblname = lc_malloc(
               (strlen(scnname) + strlen("@start") + 1) * sizeof(char));
         sprintf(lblname, "%s%s", scnname, "@start");
         DBGMSG("Create label %s at address 0x%"PRIx64"\n", lblname, scnaddress);
         label_t* lab = label_new(lblname, scnaddress, TARGET_INSN, NULL);
         //if (binfile_get_driver(bf)->binfile_getscnannotate(bf, scn) == A_PATCHMOV) {
         if (binscn_check_attrs(scn, SCNA_PATCHED)) {
            DBGMSG("Label %s marks the beginning of a patched section\n",
                  lblname);
            label_set_type(lab, LBL_PATCHSCN); //Detects if this section contains code moved by a patch operation
         }
         label_set_scn(lab, scn); //Ties this label to the section
         asmfile_add_label_unsorted(asmf, lab);
//         queue_add_tail(unlinked_lbls, lab);
         lc_free(lblname);
      }
   }
   //Finalises the update of the labels
   asmfile_upd_labels(asmf);

   DBGLVL(2,
         uint32_t n_fctlbls;uint32_t n_varlbls; label_t** fctlbls = asmfile_get_fct_labels(asmf, &n_fctlbls);label_t** varlbls = asmfile_getvarlabels(asmf, &n_varlbls); FCTNAMEMSG0("Function labels: \n");for (i = 0; i < n_fctlbls; i++) STDMSG ("\t%#"PRIx64"\t\"%s\" in section %s\n", label_get_addr(fctlbls[i]), label_get_name(fctlbls[i]), binscn_get_name(label_get_scn(fctlbls[i]))); DBGMSG0("Variable labels: \n");for (i = 0; i < n_varlbls; i++) STDMSG ("\t%#"PRIx64"\t\"%s\" in section %s\n", label_get_addr(varlbls[i]), label_get_name(varlbls[i]), binscn_get_name(label_get_scn(varlbls[i]))));

   /**\todo TODO Reste à gérer les targets : est-ce qu'on prend celles de binfile qu'on copie dans asmfile, est-ce qu'on duplique... ?*/

   return out;

}

/**
 * Finalises the parsing of a binary file
 * \param asmf An asmfile structure
 * \param bf The associated binary file. If NULL, will be retrieved from the asmf
 * \return EXIT_SUCCESS if successful, error code otherwise
 * */
static int asmfile_parse_finalise(asmfile_t* asmf, binfile_t* bf)
{
   assert(asmf);
   int out = EXIT_SUCCESS;
   if (!bf)
      bf = asmfile_get_binfile(asmf);
   assert(bf);
   //Retrieves the debug data if it is required (need to do it now to correctly identify function labels)
   if (asmfile_get_parameter(asmf, PARAM_MODULE_DEBUG,
         PARAM_DEBUG_DISABLE_DEBUG) == FALSE) {
      dbg_file_t* dbg = binfile_parse_dbg(bf);
      if (!dbg) {
         WRNMSG("Unable to parse debug data from file %s\n",
               asmfile_get_name(asmf));
         out = binfile_get_last_error_code(bf);
      }
      asmfile_setdebug(asmf, dbg);
   }
   //Loads the results of the parsed file into the assembly file
   int res = asmfile_loadbinfile(asmf);
   if (ISERROR(res)) {
      ERRMSG(
            "[INTERNAL]: Error while loading the contents of a parsed binary file to the representation of the assembly file\n");
      return res;
   }
   out = res;

   //Updates the analysis status of the file
   asmfile_add_analyzis(asmf, PAR_ANALYZE);
   return out;
}

/*
 * Parses the file referenced in an asmfile_t structure, and updates all fields in the asmfile from the result of parsing
 * \param asmf An asmfile_t structure. It must contain the name of the file to parse
 * \return EXIT_SUCCESS if the file could be correctly parsed, error code otherwise
 * */
static int asmfile_parsebinfile(asmfile_t* asmf)
{
   if (!asmf)
      return ERR_LIBASM_MISSING_ASMFILE;
   binfile_t* bf = NULL;
   int out = EXIT_SUCCESS;

   //Detecting if the file has been flagged as already parsed
   if (asmfile_test_analyze(asmf, PAR_ANALYZE) == TRUE) {
      bf = asmfile_get_binfile(asmf);
      if (bf) {
         DBGMSG("File %s has already been parsed: no operation performed\n",
               asmfile_get_name(asmf));
         return out;
      } else {
         /**\todo See whether we exit with an error here or go through the whole parsing operation instead.
          * The interest of exiting here would be to support files not parsed from a binary file
          * (the error message would have to be removed then)*/
         ERRMSG("Unable to retrieve origin binary for parsed file %s\n",
               asmfile_get_name(asmf));
         return ERR_BINARY_MISSING_BINFILE;
      }
   }

   /**\todo TODO HANDLE ARCHIVES
    * =>(2015-05-22) Doing it*/
   if (asmfile_get_binfile(asmf) != NULL) {
      //Handling a case where the binary file has already been parsed but the asmfile has not
      //been updated to contain the result. Simply retrieving the parsed file
      bf = asmfile_get_binfile(asmf);
      assert(binfile_get_asmfile(bf) == asmf);
   } else {
      bf = binfile_parse_new(asmfile_get_name(asmf), binfile_load);
      // Links the parsed binary file to the ASM file
      binfile_set_asmfile(bf, asmf);
   }
   int res = binfile_get_last_error_code(bf);
   if (ISERROR(res)) {
      //ERRMSG("Unable to parse file %s\n", asmfile_get_name(asmf));
      return res;
   }

   //Adds the parsed binary file to the ASM file
   asmfile_set_binfile(asmf, bf);

   if (binfile_get_type(bf) == BFT_ARCHIVE) {
      //File is an archive: we have to finalise the parsing of each of its members
      uint16_t i;
      for (i = 0; i < binfile_get_nb_ar_elts(bf); i++) {
         binfile_t* ar_elt = binfile_get_ar_elt(bf, i);
         out &= asmfile_parse_finalise(binfile_get_asmfile(ar_elt), ar_elt);
      }
      //Updates the analysis status of the archive file
      asmfile_add_analyzis(asmf, PAR_ANALYZE);
   } else {
      //Standard file: loading the results of the parsing into the file and parses the debug if necessary
      out = asmfile_parse_finalise(asmf, bf);
   }

   return out;
}

/**
 * Finalises a disassembly by attempting to link all remaining pointers
 * \param af A disassembled or parsed asmfile
 * \return EXIT_SUCCESS if successful, error code otherwise
 * */
static int asmfile_disass_finalise(asmfile_t* af)
{

   //Attempts to link unlinked data references
   binfile_link_data_ptrs(asmfile_get_binfile(af));
   /**\todo TODO (2015-02-10) If this is the only function we invoke here, we could remove asmfile_disass_finalise and directly
    * move the invocation of binfile_link_data_ptrs to asmfile_disassemble*/

   return EXIT_SUCCESS;
}

/**
 * Processes a file for disassembly depending on its options
 * \param af An asmfile
 * \param options Options for disassembly
 * \return EXIT_SUCCESS if the file could be successfully disassembled, error code otherwise
 * */
static int disassembler_process(asmfile_t* af, int64_t options)
{
   assert(af && !asmfile_is_archive(af));
   int out = EXIT_SUCCESS;
   if (!(options & DISASS_OPTIONS_NODATAPARSE)) {
      //Generates data structures depending on the labels
      asmfile_parsedata_fromlabels(af);
   }

   if (!(options & DISASS_OPTIONS_NODISASS)) {
      /*Disassembles the file*/
      int res = disassemble_parsed_asmfile(af);
      if (ISERROR(res)) {
         ERRMSG("Unable to disassemble file %s\n", asmfile_get_name(af));
         return res;
      }
      out = res;

      /*Updates number of instructions*/
      asmfile_update_counters(af);

      /*Updates the analysis status of the file*/
      asmfile_add_analyzis(af, DIS_ANALYZE);
   }

   //Associates the debug data with the instructions if it is required
   if (asmfile_get_parameter(af, PARAM_MODULE_DEBUG, PARAM_DEBUG_DISABLE_DEBUG)
         == FALSE) {
      int res = asmfile_load_dbg(af);
      if (ISERROR(res))
         WRNMSG(
               "Unable to associate debug data from file %s to the instructions\n",
               asmfile_get_name(af));
      out = WRN_DISASS_DBG_LBLS_NOT_RETRIEVED;
   }

   //Finalises disassembly
   asmfile_disass_finalise(af);

   return out;
}

///////////////////END OF NEW FUNCTIONS USING BINFILE

/*
 * Disassembles a file whose name is contained into an asmfile structure
 * \param A pointer to the structure holding the ASM file. It must contain the name of the file to disassemble.
 * If the file has not already been parsed (by invoking asmfile_parse) it will be at this point
 * If the disassembly is successful, it will contain the instructions from the disassembled file, the label hashtables, and
 * a pointer to the object containing the disassembled file
 * \param options Options for disassembly
 * \return EXIT_SUCCESS if the file could be successfully disassembled, error code otherwise
 * \todo Handle error codes
 * */
int asmfile_disassemble(asmfile_t* af)
{
   if (af == NULL)
      return ERR_LIBASM_MISSING_ASMFILE;
   //Retrieves the options
   int64_t options = (int64_t) asmfile_get_parameter(af, PARAM_MODULE_DISASS,
         PARAM_DISASS_OPTIONS);
//   if ((options & DISASSEMBLY_FROMARCHIVE) == 0 && !fileExist(af->name)) {
//      ERRMSG("File %s not found.\n", af->name);
//      return FALSE;
//   }
   //Parses the file and returns a parsedfile structure (if it was already parsed we will not parse it again)
   int res = asmfile_parsebinfile(af);
   if (ISERROR(res))
      return res;
   if (asmfile_test_analyze(af, PAR_ANALYZE) == FALSE) {
      //ERRMSG("File %s could not be correctly parsed\n", asmfile_get_name(af));
      return ERR_DISASS_FILE_NOT_PARSED;
   }

   if (!asmfile_is_archive(af)) {
      //File is not an archive: processing it
      res = disassembler_process(af, options);
      if (ISERROR(res))
         return res;
   } else {
      //File is an archive: processing the archive members
      uint16_t i;
      int out = EXIT_SUCCESS;
      for (i = 0; i < asmfile_get_nb_archive_members(af); i++) {
         res = disassembler_process(asmfile_get_archive_member(af, i), options);
         if (!ISERROR(out) && res != EXIT_SUCCESS)
            out = res;
      }
      return out;
   }

   //parsedfile_terminate(pf);

   return EXIT_SUCCESS;
}
#if TO_BE_UPDATED
/*
 * Disassembles a file containing potentially multiple files (it is the case for archives) and returns an asmfile for each of them
 * \param af Empty structure containing the name of the file to disassemble
 * \param afile Return parameter. Will contain an array of asmfile structures corresponding to the disassembly of each member found in the file, or
 * point to NULL if the file is not an archive
 * \param options Options for disassembly
 * \param filedesc Descriptor of the file to disassemble. If strictly positive, will override the name of the file contained in \c af and disassemble
 * the file with this descriptor instead. Otherwise it will be ignored.
 * \return
 * - 0 if the file was not an archive and was successfully disassembled
 * - The size of the \e afiles array if it was and archive and was successfully disassembled
 * - Negative value if an error occurred. In that case, the last error code in the \c af will have been updated
 * \todo To be checked against memory leaks because of the way Elf structures are handled between members of an archive
 * */
int asmfile_disassemble_n(asmfile_t* af, asmfile_t*** afiles, int options,
      int filedesc)
{
   int n_files = 0;
   char* filename = asmfile_get_name(af);
   int errcode = EXIT_SUCCESS;

   if (filename != NULL) {
      if (!fileExist(filename)) {
         ERRMSG("File %s not found.\n", filename);
         asmfile_set_last_error_code(af, ERR_COMMON_FILE_NOT_FOUND);
         return -1;
      }
      elffile_t** members = NULL;
      elffile_t* first = NULL;

      // Parsing the file and retrieving all of its members
      if (filedesc > 0) {
         //		    first = parse_elf_file( NULL, filedesc, &members, &n_files);
      } else {
         if ((options & DISASSEMBLY_NODEBUG) != 0)
         asmfile_add_parameter (af, PARAM_MODULE_DEBUG, PARAM_DEBUG_DISABLE_DEBUG, (void*)TRUE);
                  PARAM_DWARF_DISABLE_DEBUG, (void*) (long long int) TRUE);
         first = parse_elf_file(filename, NULL, &members, &n_files, af);
      }

      if ((n_files == 0) && (first != NULL)) {
         //The file is not an archive: it is a standard disassembly
         DBGMSG("File %s is not an archive\n", filename);
         asmfile_set_origin(af, first, asmfile_disassembled_free,
               ASMF_ORIGIN_ELF);

         errcode = asmfile_disassemble(af, options);
         if (ISERROR(errcode)) {
            errcode_printfullmsg(errcode);
            asmfile_set_last_error_code(af, errcode);
            n_files = -1;
         }

         if (afiles != NULL)
         *afiles = NULL;

      } else if ((n_files > 0) && (afiles != NULL)) {
         int i;
         DBGMSG("File %s is an archive and contains %d files\n", filename,
               n_files);
         *afiles = lc_malloc0(sizeof(asmfile_t*) * n_files);

         for (i = 0; i < n_files; i++) {
            //Creates a new asmfile for this member
            (*afiles)[i] = asmfile_new(elffile_getfilename(members[i]));

            //Sets the origin of the new asmfile
            asmfile_set_origin((*afiles)[i], members[i],
                  asmfile_disassembled_free, ASMF_ORIGIN_ELF);

            //Duplicates any characteristics of the initial file
            asmfile_set_proc((*afiles)[i], asmfile_get_proc(af));

            //Disassembles the file
            errcode = asmfile_disassemble((*afiles)[i],
                  (options | DISASSEMBLY_FROMARCHIVE));

            //Updates the error code
            if (ISERROR(errcode))
            asmfile_set_last_error_code((*afiles)[i], errcode);

            //Sets the origin in the original asmfile (it will contain a raw elffile)
            //af->origin = NULL;  => WHO WROTE THIS ? We don't access members of asmfile_t directly!!!
            //asmfile_set_origin (af, first, asmfile_disassembled_free);
            asmfile_set_origin(af, NULL, asmfile_disassembled_free,
                  ASMF_ORIGIN_ELF);
         }
         //Freeing the array of parsed ELF files
         lc_free(members);
      } else {
         ERRMSG("File could not be correctly parsed\n");
         n_files = -1;
         asmfile_set_last_error_code(af, ERR_DISASS_FILE_PARSING_FAILED);
      }
   } // Else no name or no return parameter was given and we can't do anything about it

   return n_files;
}
#endif

#if OBSOLETE
/*
 * Frees all members in an asmfile_t structure that were updated in the asmfile_disassemble function
 * \param a The asmfile_t structure holding the disassembled file. After execution, all members will be freed and set to 0
 * */
void asmfile_disassembled_free(void* a)
{
   if (a != NULL) {
      asmfile_t* af = (asmfile_t*) a;

//      /*Frees the parsed file structure*/
//      if (asmfile_getorigin(af) != NULL ) {
//         elffile_free(asmfile_getorigin(af));
//         asmfile_setorigin(af, NULL, NULL, ASMF_ORIGIN_UNKNOWN);
//      }

      /*Frees the list of labels*/
      queue_flush(af->label_list, NULL);

      /*Frees the table of labels and the labels*/
      hashtable_flush(af->label_table, label_free, NULL);

      /*Frees the list of instructions*/
      queue_flush(asmfile_get_insns(af), insn_free);

   }
}
#endif

