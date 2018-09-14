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
 * \file
 * */
#include <inttypes.h>

#include "libmasm.h"
#include "libmdbg.h"
#include "asmb_archinterface.h"
///////////////////////////////////////////////////////////////////////////////
//                                  asmfile                                  //
///////////////////////////////////////////////////////////////////////////////
/*
 * Create a new empty asmfile
 * \param asmfile_name name of the file (must be not null)
 * \return a new asmfile, or PTR_ERROR is asmfile_name is NULL
 */
asmfile_t* asmfile_new(char* asmfile_name)
{
   if (asmfile_name == NULL)
      return NULL;

   asmfile_t* new = lc_malloc0(sizeof(*new));
   new->insns = queue_new();
   new->insns_gaps = queue_new();
   new->label_table = hashtable_new(str_hash, str_equal);
   new->ht_functions = hashtable_new(str_hash, str_equal);
   new->functions = queue_new();
   new->name = lc_strdup(asmfile_name);
   new->label_list = queue_new();
   new->analyze_flag = NO_ANALYZE;
   new->branches_by_target_insn = hashtable_new(direct_hash, direct_equal);/**\todo Avoid this direct_hash, passing addresses as pointers => (2014-12-05)Finally being done...*/
   new->insn_ptrs_by_target_data = hashtable_new(direct_hash, direct_equal);
   new->data_ptrs_by_target_insn = hashtable_new(direct_hash, direct_equal);

   return new;
}

/**\todo Not used, very similar to madras_insns_print => factor code*/
/*
 * Prints the list of instructions in an ASM file
 * \param asmf The ASM file to print
 * \param stream The stream to print to
 * \param startaddr The address at which printing must begin. If 0 or negative, the first address in the instruction list will be taken
 * \param stopaddr The address at which printing must end. If 0 or negative, the last address in the instruction list will be taken
 * \param printaddr Prints the address before an instruction
 * \param printcoding Prints the coding before an instruction
 * \param printlbl Prints the labels (if present)
 * \param before Function to execute before printing an instruction (for printing additional informations)
 * \param after Function to execute after printing an instruction (for printing additional informations)
 * */
void asmfile_print_insns(asmfile_t* asmf, FILE* stream, int64_t startaddr,
      int64_t stopaddr, int printlbl, int printaddr, int printcoding,
      void (*before)(asmfile_t*, insn_t*, FILE*),
      void (*after)(asmfile_t*, insn_t*, FILE*))
{
   //Exits if the file is NULL, or its instruction list is NULL or empty
   queue_t *insns = asmfile_get_insns(asmf);
   if (queue_length(insns) == 0)
      return;

   if (startaddr <= 0)
      startaddr = INSN_GET_ADDR(queue_peek_head(insns));
   if (stopaddr <= 0)
      stopaddr = INSN_GET_ADDR(queue_peek_tail(insns));

   FOREACH_INQUEUE(insns, it)
   {
      insn_t* insn = GET_DATA_T(insn_t*, it);
      int64_t addr = INSN_GET_ADDR(insn);

      if (addr < startaddr)
         continue;
      if (addr > stopaddr)
         break;

      //Printing the label
      if (printlbl == TRUE) {
         label_t* label = insn_get_fctlbl(insn);

         //Prints label name if the label the instruction belongs to is the target of the label
         if (label_get_target(label) == insn)
            fprintf(stream, "%"PRIx64" <%s>:\n", addr, label_get_name(label));
      }

      if (before != NULL)
         before(asmf, insn, stream);

      //Printing the instruction's address
      if (printaddr == TRUE) {
         fprintf(stream, " %"PRIx64":\t", addr);
      }

      //Printing the instruction's coding
      if (printcoding == TRUE) {
         char coding[128];
         bitvector_hexprint(insn_get_coding(insn), coding, sizeof(coding), " ");
         const int coding_max_size = 30; // used to align strings during printing.
         // This one contains the size of the coding field. Spaces are added to fill it.

         int i = 0;
         while (coding[i] != '\0')
            i++;
         while (i < coding_max_size)
            coding[i++] = ' ';
         coding[i++] = '\0';
         fprintf(stream, "%s", coding);
      }

      //Prints instruction
      char buffer[255];
      insn_print(insn, buffer, sizeof(buffer));
      fprintf(stream, "%s", buffer);

      //Print branch label (if exists)
      int64_t branch = insn_find_pointed(insn);
      if (branch > 0) {
         insn_t* branchdest = insn_get_branch(insn);
         label_t* destlbl = insn_get_fctlbl(branchdest); //Retrieves the label of the destination

         if (destlbl != NULL) {
            int64_t lbloffs = branch - label_get_addr(destlbl); //Calculates the offset to the label
            if (lbloffs > 0)
               fprintf(stream, " <%s+%#"PRIx64">", label_get_name(destlbl),
                     lbloffs);
            else
               fprintf(stream, " <%s>", label_get_name(destlbl));
         }
      }

      if (after != NULL)
         after(asmf, insn, stream);

      fprintf(stream, "\n");
   }
}

/*
 * Finds a label in an asmfile depending on its name
 * \param asmf The asmfile to look into
 * \param lblname The name of the label
 * \return A pointer to the label with this name, or NULL if no such label was found
 * */
label_t* asmfile_lookup_label(asmfile_t* asmf, char* lblname)
{
   if (!asmf || !lblname)
      return NULL;

   return hashtable_lookup(asmf->label_table, lblname);
}

/*
 * Finds an instruction in an asmfile depending on its label
 * \param asmf The asmfile to look into
 * \param lblname The name of the label at which an application is looked for
 * \return A pointer to the instruction at this label, or NULL if no such label was found or an error occurred
 * */
insn_t* asmfile_get_insn_by_label(asmfile_t* asmf, char* lblname)
{
   /*Looking for the label in the list of label in the asmfile*/
   label_t* lbl = asmfile_lookup_label(asmf, lblname);

   /*Label found: returning the associated instruction if there is one*/
   return (label_get_target_type(lbl) == TARGET_INSN) ? lbl->target : PTR_ERROR;
}

/*
 * Retrieves a pointer to the list of labels in a file
 * \param asmf The asmfile to look into
 * \return The queue containing the labels of the asmfile, or NULL if problem.
 */
queue_t* asmfile_get_labels(asmfile_t* asmf)
{
   return (asmf != NULL) ? asmf->label_list : NULL;
}

/**
 * Compares the address of an instruction to a given address. Intended to be used in bsearch
 * @param pkey Address
 * @param pelem Instruction
 * @return 1 of the instruction has the given address, -1 if it is above, 1 if it is below
 */
static int compar_getinsn(const void *pkey, const void *pelem)
{
   int64_t* key = (int64_t*) pkey;
   insn_t** elem = (insn_t**) pelem;

   if (*key < (*elem)->address)
      return -1;
   else if (*key == (*elem)->address)
      return 0;
   else
      return 1;
}

/*
 * Finds an instruction in an asmfile depending on its address
 * \param asmf The asmfile to look into
 * \param addr The address at which an instruction must be looked for
 * \return A pointer to the instruction at this address, or NULL if no address was found or an error occurred
 * */
insn_t* asmfile_get_insn_by_addr(asmfile_t* asmf, int64_t addr)
{
   if (asmf == NULL || queue_length(asmf->insns) <= 0 || addr < 0)
      return NULL;

   // instruction table empty => generate it
   if (asmf->insns_table == NULL) {
      int i = 0;
      asmf->insns_table = lc_malloc(
            queue_length(asmf->insns) * sizeof(asmf->insns_table[0]));
      FOREACH_INQUEUE(asmf->insns, it)
      {
         insn_t* insn = GET_DATA_T(insn_t*, it);
         asmf->insns_table[i++] = insn;
      }
   }

   insn_t** insn = bsearch(&addr, asmf->insns_table, queue_length(asmf->insns),
         sizeof(*insn), &compar_getinsn);

   return (insn != NULL) ? *insn : NULL;
}

static void free_cg (asmfile_t* asmf)
{
   // Save CG nodes to free the whole callgraph at once
   array_t *cg_nodes = array_new();
   FOREACH_INQUEUE (asmf->functions, fct_iter) {
      fct_t *fct = GET_DATA_T (fct_t *, fct_iter);
      array_add (cg_nodes, fct->cg_node);
   }
   FOREACH_INLIST (asmf->plt_fct, plt_fct_iter) {
      fct_t *fct = GET_DATA_T (fct_t *, plt_fct_iter);
      array_add (cg_nodes, fct->cg_node);
   }

   // Free the whole CG (call graph)
   graph_free_from_nodes(cg_nodes, NULL, NULL);

   // Free the temporary set of CG nodes
   array_free(cg_nodes, NULL);
}

/*
 * Delete an existing asmfile and all data it contains
 * \param p a pointer on an asmfile to free
 */
void asmfile_free(void* p)
{
   if (p == NULL)
      return;

   asmfile_t* asmf = p;
   if (asmf->unload_dbg != NULL)
      asmf->unload_dbg(asmf);
   else
      asmfile_unload_dbg(asmf);

   str_free(asmf->name);
   switch (asmf->origin_type) {
   case ASMF_ORIGIN_BIN:
      binfile_free(asmf->origin.binfile);
      break;
   case ASMF_ORIGIN_TXT:
      asm_txt_origin_free(asmf->origin.txtorigin);
      break;
   }

   lc_free(asmf->varlabels);

   free_cg (asmf);

   hashtable_free(asmf->ht_functions, NULL, NULL);
   queue_free(asmf->functions, &fct_free_except_cg_node);
   lc_free(asmf->used_isets);

   lc_free(asmf->fctlabels);
   queue_free(asmf->label_list, &label_free);
   hashtable_free(asmf->label_table, NULL, NULL);
   queue_free(asmf->insns, arch_get_insn_free(asmf->arch));
   queue_free(asmf->insns_gaps, NULL);
   list_free(asmf->plt_fct, &fct_free_except_cg_node);
   hashtable_free(asmf->branches_by_target_insn, NULL, NULL);
   lc_free(asmf->insns_table);

   hashtable_free(asmf->data_ptrs_by_target_insn, NULL, NULL);
   hashtable_free(asmf->insn_ptrs_by_target_data, NULL, NULL);
   lc_free(asmf);

}

/*
 * Update counters such as n_loops in an existing asmfile
 * \param asmf an existing asmfile
 */
void asmfile_update_counters(asmfile_t* asmf)
{
   if (asmf == NULL)
      return;

   asmf->n_loops = 0;
   asmf->n_functions = 0;
   asmf->n_insns = 0;
   asmf->n_blocks = 0;

   FOREACH_INQUEUE(asmf->functions, itf)
   {
      fct_t* f = GET_DATA_T(fct_t*, itf);
      asmf->n_functions++;

      FOREACH_INQUEUE(f->blocks, itb) {
         block_t* b = GET_DATA_T(block_t*, itb);
         asmf->n_blocks++;

         FOREACH_INSN_INBLOCK(b, iti)
         {
            asmf->n_insns++;
         }
      }
      asmf->n_loops += queue_length(f->loops);
   }
}

/*
 * Set the queue of instructions of an asmfile.
 * \param asmf an asmfile
 * \param insns a queue of insn_t*
 */
void asmfile_set_insns(asmfile_t* asmf, queue_t* insns)
{
   if (asmf != NULL) {
      queue_free(asmf->insns, arch_get_insn_free(asmf->arch));
      asmf->insns = insns;
   }
}

/*
 * Sets the binary file for an asmfile
 * \param asmf An asmfile
 * \param bf The binary file structure
 * */
void asmfile_set_binfile(asmfile_t* asmf, binfile_t* bf)
{
   if (asmf != NULL) {
      asmf->origin.binfile = bf;
      asmf->origin_type = ASMF_ORIGIN_BIN;
   }
}

/*
 * Sets the text file for an asmfile
 * \param f An asmfile
 * \param tf The formatted text file structure
 * */
void asmfile_set_txtfile(asmfile_t* f, txtfile_t* tf,
      asm_txt_fields_t* fieldnames)
{
   if (f != NULL) {
      f->origin.txtorigin = asm_txt_origin_new(tf, fieldnames);
      f->origin_type = ASMF_ORIGIN_TXT;
   }
}

/*
 * Resets the origin of an asmfile
 * \param f An asmfile
 * */
void asmfile_clearorigin(asmfile_t* f)
{
   if (f != NULL) {
      f->origin.binfile = NULL;
      f->origin_type = ASMF_ORIGIN_UNKNOWN;
   }
}

/*
 * Sets the debug information for an asmfile
 * \param f The asmfile
 * \param df The structure describing the debug information
 * */
void asmfile_setdebug(asmfile_t* f, dbg_file_t* df)
{
   if (f != NULL)
      f->debug = df;
}

/*
 * Sets the architecture for an asmfile
 * \param asmf An asmfile
 * \param arch Pointer to the instance of the architecture structure for this architecture
 * */
void asmfile_set_arch(asmfile_t* asmf, arch_t* arch)
{
   if (asmf == NULL)
      return;

   asmf->arch = arch;
   lc_free(asmf->used_isets);

   unsigned int nb_isets = arch_get_nb_isets(arch);
   if (nb_isets > 0) {
      //Initialises the array of used instruction sets for this file
      asmf->used_isets = lc_malloc0(nb_isets);
   }
}

/*
 * Sets the processor version of an asmfile
 * \param asmfile An asmfile
 * \param proc a processor version
 */
void asmfile_set_proc(asmfile_t* asmfile, proc_t* proc)
{
   if (asmfile == NULL)
      return;
   asmfile->proc = proc;
}

/*
 * Updates the flag for performed analysis to adds a new analysis step to the file
 * \param asmf The asmfile
 * \param analyzis_flag The flag to add
 * */
void asmfile_add_analyzis(asmfile_t* asmf, int analyzis_flag)
{
   if (asmf != NULL)
      asmf->analyze_flag |= analyzis_flag;
}

/*
 * Indexes a branch instruction with its destination in an asmfile_t
 * \param asmf The asmfile
 * \param branch The branch instruction. It will be added to the table of branches indexed by \c dest.
 * \param dest The instruction targeted by the branch. It will be the index of the branch in the table.
 * \warning For performance reasons, no test is performed on whether \c branch actually points to \c dest.
 * */
void asmfile_add_branch(asmfile_t* asmf, insn_t* branch, insn_t* dest)
{
   /**\todo (2014-12-05) No tests are done on branch actually referencing dest as this is intended to
    * be done during disassembly precisely after linking an instruction to a branch. If the case occurs
    * where this could be legitimately happen, add a test, but I'd better not
    * */
   if (!asmf || !branch)
      return;
   hashtable_insert(asmf->branches_by_target_insn, dest, branch);
}

/*
 * Indexes an instruction with the data it references in an asmfile_t
 * \param asmf The asmfile
 * \param refinsn The instruction referencing a data. It will be added to the table of instructions referencing data indexed by \c dest.
 * \param dest The data referenced by the instruction. It will be the index of the instruction in the table.
 * \warning For performance reasons, no test is performed on whether \c refinsn actually references to \c dest.
 * */
void asmfile_add_insn_ptr_to_data(asmfile_t* asmf, insn_t* refinsn, data_t* dest)
{
   /**\todo (2014-12-05) No tests are done on refinsn actually referencing dest as this is intended to
    * be done during disassembly precisely after linking an instruction to the referenced data. If the case occurs
    * where this could be legitimately happen, add a test, but I'd better not
    * */
   if (!asmf || !refinsn)
      return;
   hashtable_insert(asmf->insn_ptrs_by_target_data, dest, refinsn);
   //Flags the section to which the data belongs as containing references from instructions
   binscn_add_attrs(data_get_section(dest), SCNA_INSREF);
   /**\todo TODO (2015-02-11) ^This may possibly slow down the disassembly process. So far I think I will need this for the patch
    * when reordering sections. If not, is has to be removed, or something more optimised should be found.*/
}

/*
 * Indexes a data entry with the instruction it references an asmfile_t
 * \param asmf The asmfile
 * \param refdata The data entry. It will be added to the table of data referencing instructions indexed by \c dest.
 * \param dest The instruction referenced by the entry. It will be the index of the data entry in the table.
 * \warning For performance reasons, no test is performed on whether \c branch actually points to \c dest.
 * */
void asmfile_add_data_ptr_to_insn(asmfile_t* asmf, data_t* refdata, insn_t* dest)
{
   /**\todo (2014-12-05) No tests are done on refdata actually referencing dest as this is intended to
    * be done during disassembly precisely after linking a data to the referenced instruction. If the case occurs
    * where this could be legitimately happen, add a test, but I'd better not
    * */
   if (!asmf || !refdata)
      return;
   hashtable_insert(asmf->data_ptrs_by_target_insn, dest, refdata);
}

/*
 * Retrieves the table of instructions referencing a data structure
 * \param asmf The asmfile
 * \return A table containing all instructions referencing a data structure, indexed by the referenced data,
 * or NULL if \c asmf is NULL
 * */
hashtable_t* asmfile_get_insn_ptrs_by_target_data(asmfile_t* asmf)
{
   return (asmf) ? asmf->insn_ptrs_by_target_data : NULL;
}

/*
 * Retrieves the table of data structures referencing an instruction
 * \param asmf The asmfile
 * \return A table containing all data structures referencing an instruction, indexed by the referenced instruction,
 * or NULL if \c asmf is NULL
 * */
hashtable_t* asmfile_get_data_ptrs_by_target_insn(asmfile_t* asmf)
{
   return (asmf) ? asmf->data_ptrs_by_target_insn : NULL;
}

/*
 * Gets an asmfile project
 * \param asmf an asmfile
 * \return a project or PTR_ERROR if there is a problem
 */
project_t* asmfile_get_project(asmfile_t* asmf)
{
   return (asmf != NULL) ? asmf->project : PTR_ERROR;
}

/*
 * Gets an asmfile name
 * \param asmf an asmfile
 * \return a string or PTR_ERROR if there is a problem
 */
char* asmfile_get_name(asmfile_t* asmf)
{
   return (asmf != NULL) ? asmf->name : PTR_ERROR;
}

/*
 * Gets functions for an asmfile
 * \param asmf an asmfile
 * \return a queue of functions or PTR_ERROR if there is a problem
 */
queue_t* asmfile_get_fcts(asmfile_t* asmf)
{
   return (asmf != NULL) ? asmf->functions : PTR_ERROR;
}

/*
 * Get the list of plt functions
 * \param asmf The asmfile to look into
 * \return a list that contains plt's functions name or PTR_ERROR if there is a problem.
 */
list_t* asmfile_get_fct_plt(asmfile_t* asmf)
{
   return (asmf != NULL) ? asmf->plt_fct : PTR_ERROR;
}

/*
 * Gets instructions of an asmfile
 * \param asmf an asmfile
 * \return a queue of instructions or PTR_ERROR if there is a problem
 */
queue_t* asmfile_get_insns(asmfile_t* asmf)
{
   return (asmf != NULL) ? asmf->insns : PTR_ERROR;
}

/*
 * Gets the positions of gaps between decompiled instructions of an
 * asmfile.
 * \param asmf an asmfile
 * \return a queue of list_t of instructions from the instruction list
 * or PTR_ERROR if there is a problem. If it returns an empty queue, any
 * a instructions should be added to the end of the instruction queue.
 */
queue_t* asmfile_get_insns_gaps(asmfile_t* asmf)
{
   return (asmf != NULL) ? asmf->insns_gaps : PTR_ERROR;
}

/*
 * Gets the number of instructions in an asmfile
 * \param asmf an asmfile
 * \return number of instructions
 */
int asmfile_get_nb_insns(asmfile_t* asmf)
{
   return (asmf != NULL) ? asmf->n_insns : 0;
}

/*
 * Gets the number of blocks in an asmfile
 * \param asmf an asmfile
 * \return number of blocks
 */
int asmfile_get_nb_blocks(asmfile_t* asmf)
{
   return (asmf != NULL) ? asmf->n_blocks : 0;
}

/*
 * Gets the number of blocks in an asmfile, excluding virtual blocks
 * \param asmf an asmfile
 * \return number of blocks
 */
int asmfile_get_nb_blocks_novirtual(asmfile_t* asmf)
{
   int nb = 0;

   FOREACH_INQUEUE(asmfile_get_fcts(asmf), it) {
      fct_t* f = GET_DATA_T(fct_t*, it);
      nb += fct_get_nb_blocks_novirtual(f);
   }

   return nb;
}

/*
 * Gets the number of loops in an asmfile
 * \param asmf an asmfile
 * \return number of loops
 */
int asmfile_get_nb_loops(asmfile_t* asmf)
{
   return (asmf != NULL) ? asmf->n_loops : 0;
}

/*
 * Gets the number of functions in an asmfile
 * \param asmf an asmfile
 * \return number of functions
 */
int asmfile_get_nb_fcts(asmfile_t* asmf)
{
   return (asmf != NULL) ? asmf->n_functions : 0;
}

/*
 * Gets the binary file from which an asmfile is built
 * \param asmf an asmfile
 * \return Pointer to the binfile_t structure or NULL if \c asmf is NULL or does not originates from a binary file
 */
binfile_t* asmfile_get_binfile(asmfile_t* asmf)
{
   return
         (asmfile_get_origin_type(asmf) == ASMF_ORIGIN_BIN) ?
               asmf->origin.binfile : PTR_ERROR;
}

/*
 * Gets the origin structure of an asmfile if it's a formatted text file
 * \param asmf an asmfile
 * \return Pointer to the origin structure if it is a formatted text file, NULL otherwise
 */
txtfile_t* asmfile_get_txtfile(asmfile_t* asmf)
{
   asm_txt_origin_t* origin = asmfile_get_txt_origin(asmf);
   return (origin) ? origin->txtfile : PTR_ERROR;
}

/*
 * Gets the txtfile_t structure contained in the origin structure of an asmfile if it's a formatted text file
 * \param asmf an asmfile
 * \return Pointer to the structure representing the formatted text file from the origin structure if
 * the asmfile represents a formatted assembly file, NULL otherwise
 */
asm_txt_origin_t* asmfile_get_txt_origin(asmfile_t* asmf)
{
   return
         (asmfile_get_origin_type(asmf) == ASMF_ORIGIN_TXT) ?
               asmf->origin.txtorigin : PTR_ERROR;
}

/*
 * Retrieves the structure describing the name of the fields used in an asmfile created from a formatted assembly file
 * \param asmf The asmfile
 * \return A structure describing the names of the fields used in the formatted assembly file represented by the asmfile,
 * or NULL is \c asmf is NULL or not parsed from a formatted assembly file
 */
asm_txt_fields_t* asmfile_get_txtfile_field_names(asmfile_t* asmf)
{
   asm_txt_origin_t* origin = asmfile_get_txt_origin(asmf);
   return (origin) ? origin->fields : NULL;
}

/*
 * Gets the type of the origin structure of an asmfile
 * \param asmf an asmfile
 * \return Identifier of the origin structure
 */
uint8_t asmfile_get_origin_type(asmfile_t* asmf)
{
   return (asmf != NULL) ? asmf->origin_type : ASMF_ORIGIN_UNKNOWN;
}

/*
 * Gets the architecture of an asmfile
 * \param asmf An asmfile
 * \return A pointer to the structure describing the architecture of the asmfile
 * */
arch_t* asmfile_get_arch(asmfile_t* asmf)
{
   return (asmf != NULL) ? asmf->arch : PTR_ERROR;
}

/*
 * Gets the architecture name of an asmfile
 * \param asmf An asmfile
 * \return The name of the architecture of the asmfile
 * */
char* asmfile_get_arch_name(asmfile_t* asmf)
{
   return arch_get_name(asmfile_get_arch(asmf));
}

/*
 * Gets the architecture code of an asmfile
 * \param asmf An asmfile
 * \return The code associated to the architecture of the asmfile as defined in arch.h
 * */
char asmfile_get_arch_code(asmfile_t* asmf)
{
   return arch_get_code(asmfile_get_arch(asmf));
}

/*
 * Gets the table containing the branches in an asmfile
 * \param asmf The asmfile
 * \return A pointer to its table containing branch instructions (indexed on their destination), or NULL if asmf is NULL
 * */
hashtable_t* asmfile_get_branches(asmfile_t* asmf)
{
   return (asmf != NULL) ? asmf->branches_by_target_insn : PTR_ERROR;
}

/*
 * Gets the array of labels associated to functions in an asmfile
 * \param asmf The asmfile
 * \param nfctlabels Return parameter. Will contain the size of the array if not NULL
 * \return A pointer to the array of labels associated to functions or NULL if asmf is NULL
 * */
label_t** asmfile_get_fct_labels(asmfile_t* asmf, unsigned int* nfctlabels)
{
   if (asmf != NULL) {
      if (nfctlabels != NULL)
         *nfctlabels = asmf->n_fctlabels;
      return asmf->fctlabels;
   } else
      return PTR_ERROR;
}

/*
 * Gets the array of labels associated to variables in an asmfile
 * \param asmf The asmfile
 * \param nfctlabels Return parameter. Will contain the size of the array if not NULL
 * \return A pointer to the array of labels associated to variables or NULL if asmf is NULL
 * */
label_t** asmfile_getvarlabels(asmfile_t* asmf, unsigned int* nvarlabels)
{
   if (asmf != NULL) {
      if (nvarlabels != NULL)
         *nvarlabels = asmf->n_varlabels;
      return asmf->varlabels;
   } else
      return PTR_ERROR;
}

/*
 * Finds the first label before the given address in an asmfile
 * \param asmf The asmfile
 * \param addr The address at which the label must be searched
 * \param container If not pointing to a NULL pointer, will be used as the starting
 * point for the search in the list of ordered labels, and will be updated to contain the container
 * of the label found
 * \return If there is a label at the given address, will return it, otherwise returns the first label found
 * immediately before the \e addr and after the label contained in \e lastcontainer (or the beginning of
 * the label list if NULL). If the address is lower than the address of the starting point, NULL will be returned
 */
label_t* asmfile_get_last_label(asmfile_t* asmf, int64_t addr,
      list_t** lastcontainer)
{
   list_t* iter, *found = NULL;
   if ((asmf == NULL) || (queue_length(asmf->label_list) == 0))
      return PTR_ERROR;

   //Initialises the search entry point
   if ((lastcontainer == NULL) || (*lastcontainer == NULL))
      iter = queue_iterator(asmf->label_list);
   else
      iter = *lastcontainer;

   //Case where the address is before the starting point
   if (addr < label_get_addr(GET_DATA_T(label_t*, iter)))
      return NULL;

   //Case where the address is the one of the starting point
   if (addr == label_get_addr(GET_DATA_T(label_t*, iter)))
      found = iter;
   else {
      //Scans the list of ordered labels from the entry point until the address of
      //the label is below the required address
      while ((iter != NULL)
            && (addr > label_get_addr(GET_DATA_T(label_t*, iter)))) {
         //Handling the special case where there are multiple labels at the same address:
         //in that case, we will check if the first next label encountered that has a different
         //address is below the given address. If it is, we will continue searching forward,
         //otherwise we will stop here. This avoids having systematically the last label with
         //the same address being returned
         list_t* peek = iter->next;

         //Looking forward the first label that has not the same address
         while ((peek != NULL)
               && (label_get_addr(GET_DATA_T(label_t*, peek))
                     == label_get_addr(GET_DATA_T(label_t*, iter))))
            peek = peek->next;

         found = iter;
         if ((peek == NULL)
               || (addr < label_get_addr(GET_DATA_T(label_t*, peek)))) {
            //There is no label with a higher address or the first label with a different address
            //is above the given address: we stop the search here
            break;
         } else {
            //Otherwise we will resume the search at the first label with a different address
            iter = peek;
         }
      }
      if (addr == label_get_addr(GET_DATA_T(label_t*, iter)))
         found = iter;
   }
   //Updating pointer to the container
   if (lastcontainer != NULL)
      *lastcontainer = found;

   return GET_DATA_T(label_t*, found);
}

/*
 * Finds the label at a given address which matches the searched string
 * \param asmf The asmfile
 * \param addr The address at which the label must be searched
 * \param name The name or a part of the label's name searched
 * \param container If not pointing to a NULL pointer, will be used as the starting
 * point for the search in the list of ordered labels, and will be updated to contain the container
 * of the label found
 * \return If there is a label at the given address which matches the searched string, will return it.
 * If the address is lower than the address of the starting point, NULL will be returned
 */
label_t* asmfile_getlabel_byaddressandname(asmfile_t* asmf, int64_t addr,
      char* name, list_t** container)
{
   list_t* iter, *found = NULL;
   label_t* label = NULL;
   if ((asmf == NULL) || (queue_length(asmf->label_list) == 0))
      return PTR_ERROR;

   //Initialises the search entry point
   if ((container == NULL) || (*container == NULL))
      iter = queue_iterator(asmf->label_list);
   else
      iter = *container;

   //Case where the address is before the starting point
   if (addr < label_get_addr(GET_DATA_T(label_t*, iter)))
      return NULL;

   //Case where the address is the one of the starting point
   if ((addr == label_get_addr(GET_DATA_T(label_t*, iter)))
         && (strstr(label_get_name(GET_DATA_T(label_t*, iter)), name)))
      found = iter;
   else {
      //Scans the list of ordered labels from the entry point until the address of
      //the label is at the required address
      while ((iter != NULL)
            && (addr >= label_get_addr(GET_DATA_T(label_t*, iter)))) {
         if ((iter == NULL)
               || (addr < label_get_addr(GET_DATA_T(label_t*, iter)))
               || ((addr == label_get_addr(GET_DATA_T(label_t*, iter)))
                     && (strstr(label_get_name(GET_DATA_T(label_t*, iter)), name)))) {
            //There is no label with a higher address or the first label with a different address
            //is above the given address or we found the searched label: we stop the search here
            break;
         } else {
            iter = iter->next;
         }
      }
      if ((iter != NULL)
            && (addr == label_get_addr(GET_DATA_T(label_t*, iter)))
            && (strstr(label_get_name(GET_DATA_T(label_t*, iter)), name)))
         found = iter;
   }

   //Updating pointer to the container
   if (container != NULL)
      *container = iter;

   if (found != NULL) {
      label = GET_DATA_T(label_t*, found);
      DBGMSG("Label search: %s(%"PRIx64")\n",
            label_get_name(GET_DATA_T(label_t*,found)),
            label_get_addr(GET_DATA_T(label_t*,found)));
   }

   return label;
}

/*
 * Finds the first label eligible to be associated to an instruction before the given address in an asmfile
 * \param asmf The asmfile
 * \param addr The address before which we look for a label
 * \param lblidx Return parameter. If not NULL, will contain the index of the label in the array, or -1 if NULL is returned
 * \return The label eligible to be associated to an instruction whose address is inferior or equal to \c addr, or NULL
 * if the file does not contain any function labels or if the address is below the first label
 * */
label_t* asmfile_get_last_fct_label(asmfile_t* asmf, int64_t addr, int* lblidx)
{

   if ((asmf == NULL) || (asmf->n_fctlabels == 0)
         || (addr < label_get_addr(asmf->fctlabels[0]))) {
      if (lblidx)
         *lblidx = -1;
      return NULL;
   }
   int minidx = 0, maxidx = asmf->n_fctlabels;

   // looks like a binary search => consider call libc bsearch if equivalent performance
   while ((maxidx - minidx) > 1) {
      int middleidx = (maxidx + minidx) / 2;
      int64_t middleaddr = label_get_addr(asmf->fctlabels[middleidx]);
      if (addr == middleaddr) {
         if (lblidx)
            *lblidx = middleidx;
         return asmf->fctlabels[middleidx];
      }
      if (addr < middleaddr)
         maxidx = middleidx;
      else
         minidx = middleidx;
   }
   if (lblidx)
      *lblidx = minidx;
   return asmf->fctlabels[minidx];
}

/*
 * Gets the processor version of an asmfile
 * \param asmfile An asmfile
 * \return the processor version associated to the project
 */
proc_t* asmfile_get_proc(asmfile_t* asmfile)
{
   if (asmfile == NULL)
      return PTR_ERROR;
   return asmfile->proc;
}

/*
 * Gets the name of a processor version of a asmfile
 * \param asmfile an asmfile
 * \return the name of the processor version associated to the asmfile
 */
char* asmfile_get_proc_name(asmfile_t* asmfile)
{
   if (asmfile == NULL)
      return PTR_ERROR;
   //Returning the name of the processor version associated to the asmfile
   return proc_get_name(asmfile->proc);
}

/*
 * Retrieves the name of the micro architecture for an asmfile
 * \param asmfile An asmfile
 * \return The name of the micro architecture associated to the asmfile
 */
char *asmfile_get_uarch_name(asmfile_t* asmfile)
{
   if (asmfile == NULL)
      return PTR_ERROR;
   //Returning the name of the uarch of the processor version associated to the asmfile
   return (uarch_get_name(proc_get_uarch(asmfile->proc)));
}

/*
 * Retrieves the identifier of the micro architecture for an asmfile
 * \param asmfile An asmfile
 * \return The identifier of the micro architecture associated to the asmfile
 * */
unsigned int asmfile_get_uarch_id(asmfile_t* asmfile)
{
   return (uarch_get_id(proc_get_uarch(asmfile_get_proc(asmfile))));
}

/*
 * Adds to an asmfile the labels from external functions at the location of the corresponding stubs
 * \param asmf The asmfile
 * \return EXIT_SUCCESS if successful, error code otherwise
 * */
int asmfile_add_ext_labels(asmfile_t* asmf)
{
   if (!asmf)
      return ERR_LIBASM_MISSING_ASMFILE;
   bf_driver_t* driver = binfile_get_driver(asmfile_get_binfile(asmf));
   if (!driver)
      return ERR_LIBASM_ARCH_UNKNOWN;
   return driver->asmfile_add_ext_labels(asmf);
}

/*
 * Checks if a given analysis was performed on the file
 * \param asmf The asmfile
 * \param flag The flag to test for
 * \return TRUE if the analysis specified by the flag has already been
 * performed, FALSE otherwise, and PTR_ERROR if asmf is NULL
 * */
int asmfile_test_analyze(asmfile_t* asmf, int flag)
{
   if (asmf != NULL)
      return (asmf->analyze_flag & flag) ? TRUE : FALSE;
   else
      return SIGNED_ERROR;
}

/*
 * Returns the code of the last error encountered and resets it
 * \param asmf The asmfile
 * \return The last error code encountered. It will be reset to EXIT_SUCCESS afterward.
 * If \c asmf is NULL, ERR_LIBASM_MISSING_ASMFILE will be returned
 * */
int asmfile_get_last_error_code(asmfile_t* asmf)
{
   return asmfile_set_last_error_code(asmf, EXIT_SUCCESS);
}

/*
 * Sets the code of the last error encountered.
 * \param asmf The asmfile
 * \param error_code The error code to set
 * \return ERR_LIBASM_MISSING_ASMFILE if \c asmf is NULL, or the previous error value of the last error code in the asmfile
 * */
int asmfile_set_last_error_code(asmfile_t* asmf, int error_code)
{
   if (asmf != NULL) {
      int out = asmf->last_error_code;
      asmf->last_error_code = error_code;
      return out;
   } else
      return ERR_LIBASM_MISSING_ASMFILE;
}

/*
 * \return The number of archive elements contained in the file if it has been parsed (PAR_ANALYZE) and is an archive, 0 otherwise
 * */
uint16_t asmfile_get_nb_archive_members(asmfile_t* asmf)
{
   if (!asmf || !(asmf->analyze_flag & PAR_ANALYZE))
      return 0;
   return binfile_get_nb_ar_elts(asmfile_get_binfile(asmf));
}

/*
 * Retrieves the asmfile associated to a given archive element in an asmfile
 * \param asmf The asmfile
 * \param i Index of the archive element to retrieve
 * \return The asmfile associated to the archive member at the given index, or NULL if asmf is NULL, has not been parsed (PAR_ANALYZE),
 * is not an archive, or if i is bigger than its number of archive members
 * */
asmfile_t* asmfile_get_archive_member(asmfile_t* asmf, uint16_t i)
{
   if (!asmf || !(asmf->analyze_flag & PAR_ANALYZE))
      return NULL;
   return binfile_get_asmfile(binfile_get_ar_elt(asmfile_get_binfile(asmf), i));
}

/*
 * Checks if an asmfile is an archive
 * \param asmf The asmf
 * \return TRUE if the file has been parsed and its associated binary file is an archive, FALSE otherwise
 * */
int asmfile_is_archive(asmfile_t* asmf)
{
   if (!asmf || !(asmf->analyze_flag & PAR_ANALYZE))
      return FALSE;
   return
         (binfile_get_type(asmfile_get_binfile(asmf)) == BFT_ARCHIVE) ?
               TRUE : FALSE;
}

/**\todo All the functions insnlist_xxx are mainly used by the patcher or the madras API. They should be replaced by functions taking the asmfile
 * as parameter and operating on its instruction list, plus any additional field from asmfile that could be used (like the label list for instruction search)
 * Right now (29/09/2011), they are being quickly adapted to allow the patcher to work in the PERF infrastructure, but this should change soon*/

/*
 * Returns a pointer to the instruction in an instruction list which is at given address
 * \param insn_list The instruction list to look up
 * \param addr The address at which an instruction is looked for
 * \param start Element in the queue at which to start the search (NULL if must be its head)
 * \param stop Element in the queue at which to stop the search (NULL if it must be its tail)
 * \return A pointer to the list containing the instruction at the given address,
 * or PTR_ERROR if none was found
 */
list_t* insnlist_addrlookup(queue_t* insn_list, int64_t addr, list_t* start,
      list_t* stop)
{
   FOREACH_INSN_IN_INSNLIST(insn_list, start, stop, it) {
      insn_t* insn = GET_DATA_T(insn_t *, it);

      if (insn_get_addr(insn) == addr)
         return it;
   }

   return NULL;
}

/*
 * Returns the length in bits of an instruction list
 * \param insn_list The instruction list
 * \param start Element in the queue at which to start the count (NULL if must be its head)
 * \param stop Element in the queue at which to stop the count (NULL if it must be its tail)
 * \return The sum of all lengths in bits of the instructions in the list
 */
uint64_t insnlist_bitsize(queue_t* insn_list, list_t* start, list_t* stop)
{
   if (insn_list == NULL)
      return 0;

   uint64_t len = 0;
   FOREACH_INSN_IN_INSNLIST(insn_list, start, stop, it) {
      insn_t* insn = GET_DATA_T(insn_t *, it);

      len += BITVECTOR_GET_BITLENGTH(insn_get_coding(insn));
   }

   return len;
}

/*
 * Returns the length in bytes of an instruction list
 * \param inl The instruction list
 * \param start Element in the queue at which to start the count (NULL if must be its head)
 * \param stop Element in the queue at which to stop the count (NULL if it must be its tail)
 * \return The sum of all lengths in bits of the instructions in the list
 */
uint64_t insnlist_findbytesize(queue_t* inl, list_t* start, list_t* stop)
{
   uint64_t len = 0;
   list_t* iter, *begin, *end;

   if (inl == NULL)
      return len;

   if (start != NULL)
      begin = start;
   else
      begin = queue_iterator(inl);

   end = stop;

   for (iter = begin; iter != end; iter = iter->next)
      len += insn_get_bytesize(GET_DATA_T(insn_t*, iter));

   return len;
}

/*
 * Returns the entire coding of the instruction list in the form of a character string
 * \param insn_list The instruction list
 * \param size Pointer to the length of the returned array
 * \param start Element in the queue at which to start the retrieval (NULL if must be its head)
 * \param stop Element in the queue at which to stop the retrieval (NULL if it must be its tail)
 * \return The coding of the list in the form of an array of characters
 */
unsigned char* insnlist_getcoding(queue_t* insn_list, int* size, list_t* start,
      list_t* stop)
{
   //First identifying the size of the instruction list
   int fullsize = 0;
   FOREACH_INSN_IN_INSNLIST(insn_list, start, stop, it1) {
      insn_t* insn = GET_DATA_T(insn_t *, it1);

      fullsize += insn_get_size(insn) / 8;
   }
   unsigned char* fullstr = lc_malloc(sizeof(*fullstr) * fullsize);

   int endfull = 0;
   FOREACH_INSN_IN_INSNLIST(insn_list, start, stop, it2) {
      insn_t* insn = GET_DATA_T(insn_t *, it2);
      bitvector_t* insn_coding = insn_get_coding(insn);

      if (insn_coding != NULL) {
         int insn_size;
         unsigned char* insnstr = bitvector_charvalue(insn_coding, &insn_size,
               arch_get_endianness(insn_get_arch(insn)));
         memcpy(fullstr + endfull, insnstr, insn_size);
         endfull += insn_size;
         lc_free(insnstr);
      }
   }

   *size = fullsize;
   return fullstr;
}

void insnlist_add_inplace(asmfile_t *af, queue_t *toAdd)
{
   list_t *insnToInsertBefore = NULL;
   if (af == NULL || queue_is_empty(toAdd)) {
      lc_free(toAdd);
      return;
   }
   int64_t toAddAddress = INSN_GET_ADDR(queue_peek_head(toAdd));
   FOREACH_INQUEUE(af->insns_gaps, gapIter) {
      if (INSN_GET_ADDR(((list_t*)gapIter->data)->data) > toAddAddress) {
         insnToInsertBefore = gapIter;
         break;
      }
   }
   // Adding toAdd to the instruction list, updating the gap list.
   if (!insnToInsertBefore) {
      queue_add_tail(af->insns_gaps, toAdd->head);
      queue_append(af->insns, toAdd);
   } else {
      queue_insertbefore(af->insns_gaps, insnToInsertBefore, toAdd->head);
      queue_insert(af->insns, toAdd, insnToInsertBefore->data, 0);
   }
}

/*
 * Retrieves the instruction at the corresponding address
 * \param insn_list The instruction list
 * \param addr The address at which to look up
 * \param start Element in the queue at which to start the search (NULL if must be its head)
 * \param stop Element in the queue at which to stop the search (NULL if it must be its tail)
 * \return A pointer to the instruction at the given address, or PTR_ERROR if none was found
 */
insn_t* insnlist_insnataddress(queue_t* insn_list, int64_t addr, list_t* start,
      list_t* stop)
{
   FOREACH_INSN_IN_INSNLIST(insn_list, start, stop, it) {
      insn_t* insn = GET_DATA_T(insn_t *, it);

      if (insn_get_addr(insn) == addr)
         return insn;
   }

   return NULL;
}

/*
 * Returns 1 if an instruction list has been fully disassembled (its last instruction is
 * not NULL or BAD_INSN).
 * \param insn_list The instruction list to check
 * \return TRUE if last instruction is not NULL or BAD_INSN, FALSE otherwise
 * \note This function does not check for BAD_INSN instructions in the middle of the list
 */
int insnlist_is_valid(queue_t* insn_list)
{
   /**\todo Update this function or create another one that checks if there are errors in the middle of the list*/
   insn_t *last_insn = queue_peek_tail(insn_list);
   int16_t opcode = insn_get_opcode_code(last_insn);
   if (opcode == UNSIGNED_ERROR)
      return FALSE; // must be consistent with error code returned by insn_get_opcode_code

   return ((opcode != R_NONE) && (opcode != BAD_INSN_CODE)) ? TRUE : FALSE;
}

/*
 * Replaces in an instruction list the instructions at an address by the ones given
 * Uses the padding instruction (created with the genpadding function) to match the correct length
 * \param inl The instruction list to update
 * \param repl The instruction list to insert into inl
 * \param addr The address at which repl must be inserted
 * \param seq The list_t element containing the instruction at the address of which repl must be inserted
 * \param paddinginsn The instruction to be used as padding (usually the NOP instruction)
 * \param nextinsn Will hold a pointer to the instruction in the list immediately after the one inserted
 * \param start Pointer to the element in the queue at which to start the search for instructions to replace (NULL if must be its head). Will be updated if it was the
 * first replaced instruction
 * \param stop Pointer to the last element in the queue at which to search for instructions to replace (NULL if it must be its tail). Will be updated if it was the
 * last replaced instruction
 * \return The instruction list that has been replaced in the original (including by padding)
 */
queue_t* insnlist_replace(queue_t* inl, queue_t* repl, int64_t addr,
      list_t* seq, insn_t* paddinginsn, insn_t **nextinsn, list_t** start,
      list_t** stop)
{
   /**\todo This function is only used by libmpatch.c:patchedfile_switchblocks. It may be moved to libmpatch.c later*/
   /**\todo It would probably be safer to get rid of the addr oprnd and use only seq*/
   /**\todo The mechanism with start and stop is only needed because the patcher works on sections in an instruction list for the whole file.
    * It can be removed if a better way is found to handle this.*/
   list_t* st = NULL, *end = NULL, *pend = NULL, *stopsrch = NULL;
   queue_t* extract = repl;     // This will hold the extracted instruction list
   int inlen = 0;
   int padlen = insn_get_size(paddinginsn);   // Size of the padding instruction
   int64_t replen = insnlist_bitsize(repl, NULL, NULL); // Length of the instructions to replace with
   label_t* origlbl;
   DBGMSG(
         "Replacing instructions at address %#"PRIx64" in list starting at %#"PRIx64" (object %p) with %d elements using instruction opcode %d as padding\n",
         (seq)?INSN_GET_ADDR(GET_DATA_T(insn_t*,seq)):addr,
         (start && *start)?INSN_GET_ADDR(GET_DATA_T(insn_t*,(*start))):INSN_GET_ADDR(queue_peek_head(inl)),
         GET_DATA_T(insn_t*,(*start)), queue_length(repl),
         insn_get_opcode_code(paddinginsn));
   if (stop != NULL)
      stopsrch = (*stop)->next;
   // The stop list element is actually the last element we want to use, not the one at which we must stop

   if (!seq) {
      //Looks for address at which replacement must occur
      if ((start != NULL) && (*start != NULL))
         st = *start;
      else
         st = queue_iterator(inl);
      while ((st != stopsrch) && (((insn_t*) st->data)->address != addr))
         st = st->next;
   } else
      st = seq;

   if (st == stopsrch)
      return NULL; //No instruction at the required address

   //Now we find how many instructions we must extract to have the correct length
   end = st;
   if (end)
      pend = end->prev;

   while (inlen != replen) {
      //Advancing in the instruction list until length is at least equal to what we want to insert
      while (inlen < replen) {
         if (end == stopsrch)
            return NULL; //End of list reached before finding the correct length
         inlen += insn_get_size((insn_t*) end->data);
         pend = end;
         end = end->next;
      }
      //Padding the instructions to insert until their length is at least equal to what we want to replace
      while (replen < inlen) {
         add_insn_to_insnlst(insn_copy(paddinginsn), extract);
         replen += padlen;
      }
   }
   //Returns the address of the instruction next to the block we are swapping
   if ((end != stopsrch) && (end != NULL) && (nextinsn != NULL))
      /**\todo Check if end being NULL is is a legitimate behaviour (occurs when invoked from patchfile_switchblocks)*/
      *nextinsn = (insn_t*) end->data;
   else if (pend != stopsrch) {
      // If we reached the end of the section, we will return the address of the previous instruction.
      // This should not alter the normal execution, as the instruction at this address will have been noped and
      // anyway the jump will never be executed (as otherwise it would mean the original code tried to reach
      // after the end of the section)
      *nextinsn = (insn_t*) pend->data;
   }
   //Updating the boundaries if they were given
   if ((start) && (*start) && (*start == st))
      *start = queue_iterator(extract);

   if ((stop) && (*stop) && ((*stop)->next == end))
      *stop = queue_iterator_rev(extract);

   if ((origlbl = insn_get_fctlbl(GET_DATA_T(insn_t*, st))) != NULL) {
      //Sets labels to the inserted list to be the same as the original
      FOREACH_INQUEUE(extract, iter)
      {
         if (insn_get_fctlbl(GET_DATA_T(insn_t*, iter)) == NULL) { /**\todo This test may slow things down, see if we actually need it*/
            insn_link_fct_lbl(GET_DATA_T(insn_t*, iter), origlbl); //Sets the label of inserted instructions to be the same as the original
            /**\todo Add a test to see if the label changes between start and stop,
             and display a warning if that is the case. May not be needed however*/
         }
      }
   }
   DBGMSGLVL(1,
         "Swapping instructions between addresses %#"PRIx64" and %#"PRIx64" with %d instructions\n",
         INSN_GET_ADDR(GET_DATA_T(insn_t*, st)),
         INSN_GET_ADDR(GET_DATA_T(insn_t*, pend)), queue_length(extract));
   //Swapping the instruction list with what we want to insert and returning the swapped list
   queue_swap_elts(inl, st, pend, extract);

   return extract;
}

/*
 * Resets all addresses in an instruction list to SIGNED_ERROR
 * \param insn_list The list of instructions to update
 * \param start Element in the queue at which to start the reset (NULL if must be its head)
 * \param stop Element in the queue at which to stop the reset (NULL if it must be its tail)
 * \warning The function insnlist_upd_addresses will have to be used afterwards on this list
 * in order for it to have coherent information
 */
void insnlist_reset_addresses(queue_t* insn_list, list_t* start, list_t* stop)
{
   FOREACH_INSN_IN_INSNLIST(insn_list, start, stop, it)
   {
      insn_t* insn = GET_DATA_T(insn_t *, it);

      insn->address = SIGNED_ERROR;
   }
}

/*
 * Updates all addresses in an instructions list (if necessary) based on
 * the length of their coding and the address of the first instruction
 * \param insn_list The list of instructions to update
 * \param startaddr The address of the first instruction in the list
 * \param start Element in the queue at which to start the update (NULL if must be its head)
 * \param stop Element in the queue at which to stop the update (NULL if it must be its tail)
 * \return The number of instructions whose address was modified
 */
unsigned int insnlist_upd_addresses(queue_t* insn_list, int64_t startaddr,
      list_t* start, list_t* stop)
{
   /**\todo TODO (2015-04-21) This function could be moved to libmpatch.c as I think it's only used there.
    * And while we are at it, maybe change the way stop is handled (we stop before the instruction at stop,
    * which is counter-intuitive)*/
   /**\todo Maybe add here an invocation of the function for recalculating branches*/
   unsigned int nmodifs = 0;
   int64_t addr = startaddr; /*First instruction doesn't necessarily have the correct address*/

   /** \todo Check there is no strange cases*/
   FOREACH_INSN_IN_INSNLIST(insn_list, start, stop, it) {
      insn_t* insn = GET_DATA_T(insn_t*, it);

      if (INSN_GET_ADDR (insn) != addr) {
         DBGMSG(
               "Updating address of instruction (%p) from %#"PRIx64" to %#"PRIx64"\n",
               insn, INSN_GET_ADDR (insn), addr);
         insn_set_addr(insn, addr);
         nmodifs++;
      }

      addr += insn_get_size(insn) / 8;
      DBGMSGLVL(2,
            "Instruction (%p) at address %#"PRIx64" has length %d bytes: next address set to %#"PRIx64"\n",
            insn, INSN_GET_ADDR (insn), insn_get_size(insn) / 8, addr);
   }

   return nmodifs;
}

/*
 * Updates all offsets of branch instructions in an instruction list, including their coding
 * if the \e updinsncoding parameter is not NULL
 * \param insn_list The instruction list
 * \param updinsncoding Pointer to the function to use for updating the instruction's coding
 * \param updopcd If set to 1, will try to update the whole coding of instructions, allowing it to
 * change size. If set to 0, the coding will be updated but an error will be raised in case of size changing.
 * \param driver The architecture specific driver for the instruction list
 * \param asmfile The asmfile where the instructions are stored
 * \param start Element in the queue at which to start the update (NULL if must be its head)
 * \param stop Element in the queue at which to stop the update (NULL if it must be its tail)
 */
void insnlist_upd_branchaddr(queue_t* insn_list,
      int (*updinsncoding)(insn_t*, void*, int, int64_t*), int updopcd,
      void* driver, asmfile_t* asmfile, list_t* start, list_t* stop)
{
   /**\todo Update this function when the assembler is stabilised
    * (2018-02-19) Actually refactor the whole process for updating instruction lists (and move all into assembler to avoid the callback)*/
   int64_t shiftaddr = 0;
   arch_t* current_arch = asmfile->arch;
   void* arch_driver = driver;

   FOREACH_INSN_IN_INSNLIST(insn_list, start, stop, it)
   {
      insn_t* insn = GET_DATA_T(insn_t*, it);
      oprnd_t* refop = insn_lookup_ref_oprnd(insn);
      if (insn->arch != current_arch) {
         DBGMSG("Switched from %s to %s at address %#"PRIx64".\n",
               current_arch->name, insn->arch->name, insn_get_addr(insn));
         current_arch = insn->arch;
         arch_driver = asmbldriver_load_byarchcode(current_arch->code);
      }

      DBG(
            char buffer[256]; insn_print(insn, buffer, sizeof(buffer)); fprintf(stderr,"%s\n", buffer););

      pointer_t* pointer = oprnd_get_ptr(refop);
      insn_t* target = pointer_get_insn_target(oprnd_get_ptr(refop));
      /**\todo Fix once and for all how we detect direct branches
       * => (2015-07-30) This should do the trick*/
      if (target != NULL) {
         int insn_anno = insn_get_annotate(insn);
         int target_anno = insn_get_annotate(target);
         /*If the size of instructions is not allowed to change, we will try updating the code only if the address has to be updated,
          * which is if the branch or its target have been moved, modified, or added, or if the coding was previously not set*/
         if (updopcd == 1 || (insn_anno & A_PATCHMOV)|| (insn_anno & A_PATCHUPD) || (insn_anno & A_PATCHNEW)
               || (target_anno & A_PATCHMOV) || (target_anno & A_PATCHUPD) || (target_anno & A_PATCHNEW)
               || insn_get_coding(insn) == NULL) {
            DBGMSG0("Updating instruction offset\n");
            DBG(
               char buf1[256];char buf2[256];insn_print(insn,buf1,sizeof(buf1));insn_print(pointer_get_insn_target(pointer),buf2,sizeof(buf2)); fprintf(stderr,"\t%#"PRIx64":%s (%p) -> %#"PRIx64":%s (%p)\n", insn_get_addr(insn),buf1,insn,insn_get_addr(pointer_get_insn_target(pointer)),buf2,pointer_get_insn_target(pointer)););
         oprnd_set_ptr_addr(refop,
               pointer_get_target_addr(pointer)
                     + pointer_get_offset_in_target(pointer));
         insn_get_arch(insn)->oprnd_updptr(insn, pointer);/**\todo TODO (2014-12-04) That one may not be useful if we invoke it in updinsncoding*/
         //Updates coding//
            if (updinsncoding != NULL)
               updinsncoding(insn, driver, updopcd, &shiftaddr);
         }
      }
   }
}

/*
 * Updates the coding of instructions in an instruction list, based on the assembly
 * functions contained in the architecture-specific driver oprnd
 * \param insn_list The instruction list
 * \param updinsncoding Pointer to the function to use for updating the instruction's coding
 * \param updopc If set to 1, will try to update the whole coding of the instruction including
 * the opcode. Otherwise will only try to update the operands (and display an warning if an error
 * is encountered)
 * \param start Element in the queue at which to start the update (NULL if must be its head)
 * \param stop Element in the queue at which to stop the update (NULL if it must be its tail)
 */
void insnlist_upd_coding(queue_t* insn_list,
      void*(*updinsncoding)(insn_t*, int, int64_t*), int updopcd, list_t* start,
      list_t* stop)
{
   if (updinsncoding == NULL)
      return;

   int64_t shiftaddr = 0;
   FOREACH_INSN_IN_INSNLIST(insn_list, start, stop, it)
   {
      insn_t* insn = GET_DATA_T(insn_t *, it);

      updinsncoding(insn, updopcd, &shiftaddr);
   }
}

/*
 * Updates the branch instructions in a list with a link to the instruction at their
 * destination's address, using a branches hashtable
 * \param insn_list The instruction list whose branches we want to update
 * \param branches A hashtable containing the branch instructions in the list, indexed by their destination addresses
 * */
void insnlist_linkbranches(queue_t* insn_list, hashtable_t* branches)
{
   /**\todo TODO (2014-12-05) This function is now unused during disassembly and only used for parsing. See how to get rid of it*/
   if (insn_list == NULL || branches == NULL)
      return;

   int unreachable = FALSE;

   /*Loops over all the instructions in the list*/
   FOREACH_INQUEUE(insn_list, iter1)
   {
      /*Retrieves all instructions pointing to this instruction's address*/
      insn_t *insn1 = GET_DATA_T(insn_t*, iter1);
      int64_t addr1 = INSN_GET_ADDR(insn1);
      array_t* branchfrom = hashtable_lookup_all_array(branches, (void *) addr1);

      DBGMSGLVL(1,
            "Retrieving instructions pointing to instruction @ %#"PRIx64" (%p)\n",
            addr1, insn1);

      /*Loops over the instructions pointing to the current instruction's address*/
      if (branchfrom != NULL) {
         FOREACH_INARRAY(branchfrom, iter2) {
            /*Sets the current instruction as the instruction pointed to by the pointer instruction */
            insn_t *insn2 = ARRAY_GET_DATA(insn2, iter2);
            insn_set_branch(insn2, insn1);

            DBGMSG(
               "Linked instruction @ %#"PRIx64" (%p) to instruction @ %#"PRIx64" (%p)\n",
               INSN_GET_ADDR (insn2), insn2, addr1, insn1);
         }
      }

      //Tests if the instruction is reachable
      if (label_get_addr(insn_get_fctlbl(insn1)) == addr1 /*There is a label at the instruction's address*/
      || (array_length(branchfrom) > 0) /*Direct branches point to this instruction*/
      )
         unreachable = FALSE;

      //Flags the instruction if unreachable
      if (unreachable) {
         insn_add_annotate(insn1, A_UNREACHABLE);
         DBGMSG(
               "Instruction at address %"PRIx64" is unreachable with direct branches\n",
               addr1);
      }

      //Checks if we reached a unconditional jump
      if (insn_check_annotate(insn1, A_JUMP)
            && !insn_check_annotate(insn1, A_CONDITIONAL))
         unreachable = TRUE;

      /*Freeing the list of instructions pointing to this address*/
      array_free(branchfrom, NULL);
   }
}

/*
 * Updates the instructions in an asmfile with regard to branches. This involves identifying the targets of branches and flagging unreachable instructions.
 * \param af The asmfile
 * \param branches A queue of the branch instructions present in the file. It will be emptied upon completion (but not freed)
 * */
void asmfile_upd_insns_with_branches(asmfile_t* af, queue_t* branches)
{
   if (!af || !branches || !queue_length(af->insns))
      return;

   //Orders the queue of branches by destination address
   queue_sort(branches, insn_cmpptraddr_qsort);
   //Initialises the flag  of unreachable instructions
   int unreachable = FALSE;
   /*Loops over all the instructions in the list*/
   FOREACH_INQUEUE(af->insns, iter) {
      insn_t* insn = GET_DATA_T(insn_t*, iter);
      int64_t addr = insn_get_addr(insn);

      /**\todo TODO (2014-12-05) The following block is an exact copy/paste of what I used in libmdisass.c:asmfile_upd_references
       * for linking data entries to instructions. Find a way to factorise this code*/
      //Checks if the assembly file contains unlinked branches to this address
      while (queue_length(branches) > 0) {
         insn_t* branch = (insn_t*) queue_peek_head(branches);
         //      assert(insn_check_annotate(branch, A_JUMP));  // Just checking
         oprnd_t* refop = insn_lookup_ref_oprnd(branch);
         pointer_t* ptr = oprnd_get_ptr(refop);
         assert(ptr);
         assert(!pointer_has_target(ptr)); //Checking, this may be changed into a regular if statement if this can legitimately happen (2014-07-10)
         int64_t linkaddr = pointer_get_addr(ptr);
         if ((addr <= linkaddr)
               && (linkaddr < (addr + insn_get_bytesize(insn)))) {
            //The targeted address corresponds to the instruction address or inside it: linking it
            pointer_set_insn_target(ptr, insn);
            //Updating offset if needed
            if (linkaddr > addr)
               pointer_set_offset_in_target(ptr, linkaddr - addr);
            DBGMSG(
                  "Linked instruction @ %#"PRIx64" (%p) to instruction @ %#"PRIx64" (%p)\n",
                  insn_get_addr(branch), branch, insn_get_addr(insn), insn);
            //Adding the data element indexed by the referenced instruction to the asmfile
            asmfile_add_branch(af, branch, insn);
            //Removing the element from the list of unlinked targets
            queue_remove_head(branches);
            //Flags the instruction as reachable
            unreachable = FALSE;
         } else if (addr < linkaddr)
            break; //We have not reached the first element in the queue of unlinked targets: stopping here, we will reach it later
         else if (addr > linkaddr)
            queue_remove_head(branches); //We passed the first element in the list: removing it
      }

      //Tests if there is a label at the instruction's address
      if (label_get_addr(insn_get_fctlbl(insn)) == insn_get_addr(insn))
         unreachable = FALSE;

      //Flags the instruction if unreachable
      if (unreachable) {
         insn_add_annotate(insn, A_UNREACHABLE);
         DBGMSG(
               "Instruction at address %"PRIx64" is unreachable with direct branches\n",
               insn_get_addr(insn));
      }
      //Checks if we reached a unconditional jump
      if (insn_check_annotate(insn, A_JUMP)
            && !insn_check_annotate(insn, A_CONDITIONAL))
         unreachable = TRUE;
   }
   //Now inserting the remaining branches in the table of branches with a NULL index
   while (queue_length(branches) > 0) {
      insn_t* branch = queue_remove_head(branches);
      asmfile_add_branch(af, branch, NULL);
   }
}
/*
 * Copies parts of an instruction list
 * \param insn_list An instruction queue
 * \param start First element to copy, or NULL to copy from the start
 * \param stop Last element to copy, or NULL to copy up to the end
 * \return A copied instruction queue, in the same size and order as insn_list. Any copied instructions whose original pointed
 * to another instruction to be copied will now point to the copy of that instruction, while copied instructions whose original pointed
 * to an instruction that was not to be copied will point to the same instruction as the original.
 * Labels and blocks are not initialised in the copies. If \e insn_list is NULL, will return NULL
 * \warning Instructions copied this way will not be linked to any asmfile structure, so they will have to be freed manually
 * */
queue_t* insnlist_copy(queue_t* insn_list, list_t* start, list_t* stop)
{
   if (insn_list == NULL)
      return NULL;

   /*Initialising hashtable for storing branches targets*/
   hashtable_t* targets = hashtable_new(direct_hash, direct_equal); /**\todo Avoid this direct_hash, passing addresses as pointers*/

   queue_t* out = queue_new();

   /*Copying the queue*/
   FOREACH_INSN_IN_INSNLIST(insn_list, start, ((stop) ? stop->next : NULL), it) {
      insn_t* src = GET_DATA_T(insn_t*, it);
      insn_t* dest;

      /*Duplicating instruction*/
      insn_t* cpy = insn_copy(src);

      /*Updating targets if the copy points to an instruction*/
      if ((dest = insn_get_branch(cpy)) != NULL)
         hashtable_insert(targets, (void *) INSN_GET_ADDR(dest), cpy);

      /*Add duplicated instruction to the list*/
      add_insn_to_insnlst(cpy, out);
   }

   /*Now updating the targets (we have to do this in two passes, like in the disassembly, because targets can be any instructions, including
    * those that have not been copied yet*/
   /**\todo See if in that case we could not avoid the two passes (I doubt it)*/
   insnlist_linkbranches(out, targets);

   return out;
}

/*
 * Looks for functions exits
 * \param asmf an asmfile containing functions to analyze
 */
void asmfile_detect_end_of_functions(asmfile_t* asmf)
{
   FOREACH_INQUEUE(asmfile_get_fcts(asmf), itf)
   {
      fct_t* f = GET_DATA_T(fct_t*, itf);

      FOREACH_INQUEUE(f->blocks, itb) {
         block_t* b = GET_DATA_T(block_t*, itb);
         insn_t* last_insn = block_get_last_insn(b);

         // skip padding block or blocks with no instruction
         if (block_is_padding(b) != FALSE || last_insn == NULL)
            continue;

         // Set last instruction
         if (f->last_insn == NULL ||
         INSN_GET_ADDR (last_insn) > INSN_GET_ADDR(f->last_insn))
            f->last_insn = last_insn;

         // Set exits
         // Natural exit, last instruction is a RET
         if (insn_check_annotate(last_insn, A_RTRN)) {
            queue_add_tail(f->exits, b);
            insn_add_annotate(last_insn, A_NATURAL_EX);
            DBGMSG("Block %d is a NATURAL EXIT of %s\n", b->global_id,
                  fct_get_name(f));
            continue;
         }

         oprnd_t *last_insn_oprnd = insn_get_oprnd(last_insn, 0);
         insn_t *last_insn_target = pointer_get_insn_target(
               oprnd_get_ptr(last_insn_oprnd));

         // Early exit, a jump going to another function
         if (insn_check_annotate(last_insn, A_JUMP)
               && insn_get_nb_oprnds(last_insn) == 1
               && block_get_fct(insn_get_block(last_insn_target)) != f) {
            queue_add_tail(f->exits, b);
            insn_add_annotate(last_insn, A_EARLY_EX);
            DBGMSG("Block %d is an EARLY EXIT of %s\n", b->global_id,
                  fct_get_name(f));
         }
         // Potential exit, indirect branch
         else if (insn_check_annotate(last_insn, A_JUMP)
               && insn_get_nb_oprnds(last_insn) == 1
               && (oprnd_is_mem(last_insn_oprnd) == TRUE
                     || oprnd_is_reg(last_insn_oprnd) == TRUE)) {
            queue_add_tail(f->exits, b);
            insn_add_annotate(last_insn, A_POTENTIAL_EX);
            DBGMSG("Block %d is a POTENTIAL EXIT of %s\n", b->global_id,
                  fct_get_name(f));
         }
         // Handler exit, call to a handler function
         else if (insn_check_annotate(last_insn, A_HANDLER_EX)) {
            queue_add_tail(f->exits, b);
            DBGMSG("Block %d is a HANDLER EXIT of %s\n", b->global_id,
                  fct_get_name(f));
         } // if early, potential or handler exit
      } // for each block
      DBGMSG("%s :: 0x%"PRIx64"\n", fct_get_name(f), f->last_insn->address);
   } // for each function
}

static void _add_range(insn_t* start, insn_t* stop, fct_t* fct, int dbg_pos)
{
   fct_range_t* range = fct_range_new(start, stop);

   queue_add_tail(fct->ranges, range);
   fct_add_range(fct, start, stop);

   DBGMSG("%d:: %s: 0x%"PRIx64" -> 0x%"PRIx64"\n", dbg_pos, fct_get_name(fct),
         start->address, stop->address);
}

/*
 * Looks for functions ranges
 * \param asmf an asmfile containing functions to analyze
 */
void asmfile_detect_ranges(asmfile_t* asmf)
{
   if (asmf == NULL)
      return;

   fct_t* range_fct = NULL; // 2-states automaton: in a range or outside
   insn_t* start = NULL; // first instruction of a new range

   FOREACH_INQUEUE(asmf->insns, it) {
      insn_t* insn = GET_DATA_T(insn_t*, it); // current instruction
      fct_t* fct = insn_get_fct(insn);       // current function

      if (range_fct == NULL && fct != NULL) {
         // Entering into a new function
         range_fct = fct;
         start = insn;
      } else if (range_fct != NULL && fct == NULL) {
         // Exiting current function to a no-function section
         // Add range for previous function
         insn_t* stop = INSN_GET_PREV(insn);
         _add_range(start, stop, range_fct, 2);

         range_fct = NULL;
      } else if (range_fct != NULL && fct != range_fct) {
         // Exiting current function to a new one
         // Add range for previous function
         insn_t* stop = INSN_GET_PREV(insn);
         _add_range(start, stop, range_fct, 1);

         range_fct = fct;
         start = insn;
      }
   }

   // At this point, the last range is missing
   insn_t* stop = queue_peek_tail(asmf->insns);
   if (range_fct != NULL) {
      _add_range(start, stop, range_fct, 3);
   }
}

/*
 * Set a paramter in an asmfile
 * \param asmfile an asmfile
 * \param module_id a value from params_modules_id_e enum corresponding to a module
 * \param param_id a value from params_<module>_id_e enum corresponding to the option
 * \param value the value of the parameter
 */
void asmfile_add_parameter(asmfile_t* asmf, int module_id, int param_id,
      void* value)
{
   if (asmf == NULL
         || module_id >= _NB_PARAM_MODULE || param_id >= _NB_OPT_BY_MODULE)
      return;
   asmf->params[module_id][param_id] = value;
}

/*
 * Get a parameter from an asmfile
 * \param asmfile an asmfile
 * \param module_id a value from params_modules_id_e enum corresponding to a module
 * \param param_id a value from params_<module>_id_e enum corresponding to the option
 * \return the corresponding value or NULL
 */
void* asmfile_get_parameter(asmfile_t* asmf, int module_id, int param_id)
{
   if (asmf == NULL
         || module_id >= _NB_PARAM_MODULE || param_id >= _NB_OPT_BY_MODULE)
      return NULL;
   return asmf->params[module_id][param_id];
}

/*
 * Specifies that a given instruction set is used in this file
 * \param asmfile An asmfile
 * \param iset A code for an instruction set
 * */
void asmfile_set_iset_used(asmfile_t* asmf, unsigned int iset)
{
   if (!asmf || !asmf->used_isets || !asmf->arch
         || (iset >= asmf->arch->nb_isets))
      return;
   asmf->used_isets[iset] = TRUE;
}

/*
 * Checks if a given instruction set is used in this file
 * \param asmfile An asmfile
 * \param iset A code for an instruction set
 * \return TRUE if at least an instruction in this file belongs to the given instruction set, FALSE otherwise
 * */
int asmfile_check_iset_used(asmfile_t* asmf, unsigned int iset)
{
   if (!asmf || !asmf->used_isets || !asmf->arch
         || (iset >= asmf->arch->nb_isets))
      return FALSE;
   return (asmf->used_isets[iset]);
}

/*
 * Creates a structure for storing the origin of an asmfile parsed from a formatted assembly file
 * \param txtfile The structure storing the results of the parsing of the formatted file
 * \param fields The structure describing the names of the fields (will be duplicated)
 * \return A new structure containing the description of the origin of the file
 * */
asm_txt_origin_t* asm_txt_origin_new(txtfile_t* txtfile,
      const asm_txt_fields_t* fields)
{
   asm_txt_origin_t* new = lc_malloc(sizeof *new);

   new->txtfile = txtfile;
   new->fields = lc_malloc(sizeof *(new->fields));
   memcpy(new->fields, fields, sizeof *(new->fields));

   return new;
}

/*
 * Frees a structure storing the origin of an asmfile parsed from a formatted assembly file
 * \param txtorigin The structure
 * */
void asm_txt_origin_free(asm_txt_origin_t* txtorigin)
{
   if (!txtorigin)
      return;

   lc_free(txtorigin->fields);
   txtfile_close(txtorigin->txtfile);
   lc_free(txtorigin);
}
