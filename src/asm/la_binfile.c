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
 * \file la_binfile.c
 * \brief This file contains all high level functions needed to parse, modify and create binary files
 * */
#include <inttypes.h>
#include <fcntl.h>   //I need this to create patched file and directly assign executable permissions to them
#include "libmcommon.h"
#include "libmasm.h"
#include "libmdbg.h"

/**
 * Human-readable names for the formats of a binary file.
 * \warning Be careful to use the same order as in the \ref bf_format_e enum
 * */
const char* bf_format_names[BFF_MAX] = { "Unknown", "ELF", "PE/COFF", "Mach-O" };

/**
 * Sorts an array of sections by addresses
 * \param A The array
 * \param S The number of elements in the array
 * */
#define SORT_SCNARRAY(A, S) qsort(A, S, sizeof(*A), binscn_cmp_by_addr_qsort)

#define SEGHDR_NAME "Segment header"
#define SCNHDR_NAME "Section header"
#define SYMTBL_NAME "Symbol table"

/***************************************/
// Utility functions //
///////////////////////
/*
 * Retrieves the name of a binary format
 * \param formatid Identifier of the binary format
 * \return Constant string representing the name of the format or NULL if formatid is out of range
 * */
char* bf_format_getname(bf_format_t formatid)
{
   if (formatid >= BFF_MAX)
      return NULL;
   return (char*) (bf_format_names[formatid]);
}

/**
 * Prints a mask of attributes to a string
 * \param attrs Mask of attributes of a section or segment
 * \param stream The stream
 * */
static void scnattrs_fprint(uint16_t attrs, FILE* stream)
{
   assert(stream);
   if (attrs & SCNA_READ)
      fprintf(stream, "R");
   if (attrs & SCNA_WRITE)
      fprintf(stream, "W");
   if (attrs & SCNA_EXE)
      fprintf(stream, "X");
   if (attrs & SCNA_LOADED)
      fprintf(stream, "L");
   if (attrs & SCNA_TLS)
      fprintf(stream, "T");
   if (attrs & SCNA_STDCODE)
      fprintf(stream, "C");
   if (attrs & SCNA_INSREF)
      fprintf(stream, "R");
   if (attrs & SCNA_PATCHREORDER)
      fprintf(stream, "P");
   if (attrs & SCNA_LOCALDATA)
      fprintf(stream, "D");
}

//////////////////////////////
// Creators and destructors //
//////////////////////////////

/**
 * Creates a new relocation object
 * \param label Label to associate to the relocation
 * \param address Address targeted by the relocation
 * \param disp Displacement from the origin to the relocation (if relative)
 * \param next Element targeted by the relocation (NULL if unknown)
 * \param type Type of the pointer value (using the constants defined in \ref pointer_type)
 * \param target_type Type of the pointer target (using the constants defined in \ref target_type)
 * \param reltype Format specific type of the relocation
 * \return New relocation object or NULL if label is NULL
 * */
static binrel_t* binrel_new(label_t* label, int64_t address, int64_t disp,
      void* next, pointer_type_t type, target_type_t target_type,
      uint32_t reltype)
{
   if (!label)
      return NULL;
   binrel_t* rel = lc_malloc0(sizeof(*rel));
   rel->label = label;
   rel->ptr = pointer_new(address, disp, next, type, target_type);
   rel->reltype = reltype;
   return rel;
}

/**
 * Duplicates a relocation object
 * \param rel Relocation object to copy
 * \return Copy of the object or NULL if \c rel is NULL
 * */
static binrel_t* binrel_copy(binrel_t* rel)
{
   if (!rel)
      return NULL;
   binrel_t* relcopy = lc_malloc0(sizeof(*relcopy));
   relcopy->label = rel->label;
   relcopy->ptr = pointer_copy(rel->ptr);
   relcopy->reltype = rel->reltype;
   return relcopy;
}

/**
 * Frees a relocation object
 * \param rel Relocation object to free
 * */
static void binrel_free(binrel_t* rel)
{
   if (!rel)
      return;
   if (rel->ptr)
      lc_free(rel->ptr);
   lc_free(rel);
}

/**
 * Creates a new structure representing a section in a binary file
 * \param bf The binary file to which the section belongs
 * \param scnid The index of the section
 * \param name Name of the section
 * \param type Type of the section
 * \param address Address of the section
 * \param attrs Attributes of the section
 * \return A new structure representing the section
 * */
static binscn_t* binscn_new(binfile_t* bf, uint16_t scnid, char* name,
      scntype_t type, int64_t address, unsigned attrs)
{
   binscn_t* scn = lc_malloc0(sizeof(*scn));
   scn->binfile = bf;
   scn->name = name;
   scn->type = type;
   scn->address = address;
   scn->attrs = attrs;
   scn->scnid = scnid;
   return scn;
}

static int binfile_patch_isfinalised(binfile_t* bf);

/**
 * Frees a structure representing a section in a binary file
 * \param scn The structure
 * */
static void binscn_free(binscn_t* scn)
{
   if (!scn)
      return;
   //Freeing entries if present
   if (scn->n_entries > 0) {
      uint32_t i;
      for (i = 0; i < scn->n_entries; i++) {
         data_free(scn->entries[i]);
      }
      lc_free(scn->entries);
   }
   //Freeing the array of segments to which the section belongs
   if (scn->n_binsegs)
      lc_free(scn->binsegs);
   /**\todo (2015-05-27) We may move this in the patcher or in some callback function from the format-specific libraries*/
   //Detecting if the section belongs to a file being patched, did not exist in the original file, and contains instructions
   if (binfile_patch_isfinalised(scn->binfile)
         && scn->scnid >= scn->binfile->creator->n_sections) {
      //Frees the instructions present in the section
      list_free(scn->firstinsnseq, insn_free);
   }
   //Freeing the data stored in the section if it was allocated locally
   if (scn->attrs & SCNA_LOCALDATA)
      lc_free(scn->data);

   //Freeing section
   lc_free(scn);
}

/**
 * Creates a structure representing a segment in a binary file
 * \param segid Index of the segment in the array of segment for the file
 * \param offset Offset in the file at which the segment starts
 * \param address Virtual address at which the segment is loaded to memory
 * \param fsize Size of the segment in the file
 * \param msize Size of the segment in memory
 * \param attrs Attributes of the segment
 * \param align Alignement of the segment
 * \return The structure representing the segment
 * */
static binseg_t* binseg_new(uint16_t segid, uint64_t offset, int64_t address,
      uint64_t fsize, uint64_t msize, uint8_t attrs, uint64_t align)
{
   binseg_t* seg = lc_malloc0(sizeof(*seg));
   seg->segid = segid;
   seg->offset = offset;
   seg->address = address;
   seg->fsize = fsize;
   seg->msize = msize;
   seg->attrs = attrs;
   seg->align = align;
   return seg;
}

/**
 * Frees a structure representing a segment in a binary file
 * \param seg The structure representing the segment
 * */
static void binseg_free(binseg_t* seg)
{
   if (!seg)
      return;
   if (seg->scns)
      lc_free(seg->scns);
   lc_free(seg);
}

/*
 * Initialises a new structure representing a binary file
 * \param filename Name of the binary file (string will be duplicated)
 * \return An empty structure representing a binary file
 * */
binfile_t* binfile_new(char* filename)
{
   binfile_t* bf = lc_malloc0(sizeof(*bf));
   //Fills the name of the binary file
   if (filename)
      bf->filename = lc_strdup(filename);
   //Initialising hashtable of targets
   bf->data_ptrs_by_target_data = hashtable_new(direct_hash, direct_equal);
   //Initialising hashtable of sections targets
   bf->data_ptrs_by_target_scn = hashtable_new(direct_hash, direct_equal);

   return bf;
}

/*
 * Frees a structure representing a binary file
 * \param bf The binary file structure to free
 * */
void binfile_free(binfile_t* bf)
{
   if (!bf)
      return;
   unsigned int i;

   //Freeing hashtable of targets
   hashtable_free(bf->data_ptrs_by_target_data, NULL, NULL);

   //Freeing hashtable of section targets
   hashtable_free(bf->data_ptrs_by_target_scn, NULL, NULL);
   if (bf->n_labels > 0) {
      if (bf->asmfile == NULL) {
         //The binary file is not used in an asmfile structure: we have to delete the labels here
         unsigned int j;
         for (j = 0; j < bf->n_labels; j++)
            label_free(bf->labels[j]);
      } //Otherwise the labels are freed in the asmfile
        //Freeing the array of labels
      lc_free(bf->labels);
   }

   //Freeing sections
   binfile_set_nb_scns(bf, 0);

   if (bf->relocs) {
      //Freeing relocations
      for (i = 0; i < bf->n_relocs; i++) {
         binrel_free(bf->relocs[i]);
      }
      lc_free(bf->relocs);
   }

   //Freeing array of code sections
   if (bf->n_codescns > 0)
      lc_free(bf->codescns);
   //Freeing array of loaded sections
   if (bf->n_loadscns > 0)
      lc_free(bf->loadscns);
   //Freeing array of label sections
   if (bf->n_lblscns > 0)
      lc_free(bf->lblscns);

   //Freeing sections representing headers
   if (bf->segheader)
      binscn_free(bf->segheader);
   if (bf->scnheader)
      binscn_free(bf->scnheader);
   if (bf->symtable)
      binscn_free(bf->symtable);

   //Freeing segments
   binfile_set_nb_segs(bf, 0);

   //Freeing array of external libraries
   if (bf->n_extlibs > 0) {
      lc_free(bf->extlibs);
   }

   //Freeing the archive members
   if (bf->n_ar_elts > 0) {
      uint16_t i;
      for (i = 0; i < bf->n_ar_elts; i++) {
         //Special case: archive members have an associated asmfile that we must then free
         if (bf->ar_elts[i]->asmfile)
            asmfile_free(bf->ar_elts[i]->asmfile); //This will invoke binfile_free on the archive member
         else
            binfile_free(bf->ar_elts[i]); //Should not happen
      }
   }

   //Closing stream
   if (bf->filestream)
      fclose(bf->filestream);
   //Freeing name
   if (bf->filename)
      lc_free(bf->filename);
   //Freeing associated format specific structure
   if (bf->driver.parsedbin)
      bf->driver.parsedbin_free(bf->driver.parsedbin);

   //Freeing the structures used when patching
   if (bf->entrycopies)
      hashtable_free(bf->entrycopies, NULL, NULL);

   lc_free(bf);
}

// Utility functions //
///////////////////////

/*
 * Returns the code of the last error encountered and resets it
 * \param bf The binfile
 * \return The last error code encountered. It will be reset to EXIT_SUCCESS afterward.
 * If \c bf is NULL, ERR_BINARY_MISSING_BINFILE will be returned
 * */
int binfile_get_last_error_code(binfile_t* bf)
{
   if (bf != NULL) {
      int out = bf->last_error_code;
      bf->last_error_code = EXIT_SUCCESS;
      return out;
   } else
      return ERR_BINARY_MISSING_BINFILE;
}

/*
 * Sets the code of the last error encountered.
 * \param bf The binfile
 * \param error_code The error code to set
 * \return ERR_BINARY_MISSING_BINFILE if \c bf is NULL, or the previous error value of the last error code in the binfile
 * */
int binfile_set_last_error_code(binfile_t* bf, int error_code)
{
   if (bf != NULL) {
      int out = bf->last_error_code;
      bf->last_error_code = error_code;
      return out;
   } else
      return ERR_BINARY_MISSING_BINFILE;
}

/*
 * Compares two sections based on their address. This function is intended to be used by qsort
 * \param s1 First section
 * \param s2 Second section
 * \return 1 if s1 has a higher address than s2, 0 if both have equal address and -1 otherwise
 * */
int binscn_cmp_by_addr_qsort(const void* s1, const void* s2)
{
   binscn_t *scn1 = *((void **) s1);
   binscn_t *scn2 = *((void **) s2);

   maddr_t address1 = binscn_get_addr(scn1);
   maddr_t address2 = binscn_get_addr(scn2);

   if (address1 > address2)
      return 1;
   else if (address1 < address2)
      return -1;
   else
      return 0;
}

/**
 * Compares two sections based on their offset. This function is intended to be used by qsort
 * \param s1 First section
 * \param s2 Second section
 * \return 1 if s1 has a higher offset than s2, 0 if both have equal offset and -1 otherwise
 * */
int binscn_cmpbyoffset_qsort(const void* s1, const void* s2)
{
   binscn_t *scn1 = *((void **) s1);
   binscn_t *scn2 = *((void **) s2);

   maddr_t offset1 = binscn_get_offset(scn1);
   maddr_t offset2 = binscn_get_offset(scn2);

   if (offset1 > offset2)
      return 1;
   else if (offset1 < offset2)
      return -1;
   else
      return 0;
}

/**
 * Compares two segments based on their address. This function is intended to be used by qsort
 * \param s1 First segment
 * \param s2 Second segment
 * \return 1 if s1 has a higher address than s2, 0 if both have equal address and -1 otherwise
 * */
static int binseg_cmpbyaddress_qsort(const void* s1, const void* s2)
{
   binseg_t *bs1 = *((void **) s1);
   binseg_t *bs2 = *((void **) s2);
   assert(bs1 && bs2);

   if (bs1->address > bs2->address)
      return 1;
   else if (bs1->address < bs2->address)
      return -1;
   else
      return 0;
}

/**
 * Attempts to link a data entry containing a previously unlinked reference to another data entry. The attempt is based on the respective
 * addresses of the target and the address targeted by the pointer
 * \param bf Structure representing a binary file.
 * \param entry Entry containing a reference
 * \param target Entry for which we check whether the entry  points to
 * \param targetaddr Address of \c target
 * \param endtargetaddr End address of \c target
 * \return EXIT_SUCCESS if the link could be made (\c targets overlaps the address referenced by the pointer in \c entry), error code otherwise
 * \note Using this for simplify the code of binfile_link_data_ptrs
 * */
static int binfile_link_unlinked_target(binfile_t* bf, data_t* entry,
      data_t* target, int64_t targetaddr, int64_t endtargetaddr)
{
   assert(bf && entry && target);
   //Retrieves the pointer of the entry containing a reference and its address
   pointer_t* ptr = data_get_ref_ptr(entry);
   int64_t ptraddr = pointer_get_addr(ptr);
   //Checks if the tentative target overlaps the destination address of the pointer
   if ((ptraddr >= targetaddr) && (ptraddr < endtargetaddr)) {
      //Target overlaps the destination of the pointer: we can link it to the target
      //First, removing the entry from the table with a NULL index
      hashtable_remove_elt(bf->data_ptrs_by_target_data, NULL, entry);
      //Linking the pointer to the target
      pointer_set_data_target(ptr, target);
      //Updating the offset if necessary
      if (ptraddr > targetaddr)
         pointer_set_offset_in_target(ptr, ptraddr - targetaddr);
      //Storing the target in the table indexed by its destination
      hashtable_insert(bf->data_ptrs_by_target_data, target, entry);
      //Signals that the link could take place
      return EXIT_SUCCESS;
   } else
      return EXIT_FAILURE; //Target out of range
}

/*
 * Attempts to link unlinked pointer data entries
 * \param bf Structure representing a binary file. It must be fully loaded and the array of loaded sections sorted by address
 * */
void binfile_link_data_ptrs(binfile_t* bf)
{
   if (!bf)
      return;

   //Retrieving all unlinked pointer data entries
   queue_t* unlinked_targets = binfile_lookup_unlinked_ptrs(bf);
   if (!unlinked_targets)
      return;  //No unlinked pointer data entry
   list_t* iter = queue_iterator(unlinked_targets);
   uint16_t i;
   for (i = 0; i < bf->n_loadscns; i++) {
      binscn_t* scn = bf->loadscns[i];
      //Skips pointers whose targets address is below the current loaded section
      while (iter
            && (pointer_get_addr(data_get_ref_ptr(GET_DATA_T(data_t*, iter)))
                  < scn->address)) {
         iter = iter->next;
      }
      uint32_t j = 0;
      //Loops over all entries in the section
      while (j < scn->n_entries) {
         data_t* entry = scn->entries[j];
         int64_t entryaddr = data_get_addr(entry);
         uint64_t entryendaddr = entryaddr + data_get_size(entry);
         //Links all pointers pointing to the current entry or inside it
         while (iter
               && binfile_link_unlinked_target(bf, GET_DATA_T(data_t*, iter),
                     entry, entryaddr, entryendaddr) == EXIT_SUCCESS)
            iter = iter->next;
         //Skips to the next entry
         j++;
      }
   }
   queue_free(unlinked_targets, NULL);
}

/*
 * Finalises the loading of a binary file by filling internal fields. This function has to be called when the loading is complete
 * \param bf Structure representing a binary file
 * */
void binfile_finalise_load(binfile_t* bf)
{
   if (!bf)
      return;
   if (bf->type == BFT_ARCHIVE) {
      //File is an archive: we will finalise each of its members
      uint16_t arid;
      for (arid = 0; arid < bf->n_ar_elts; arid++) {
         //Finalise the member
         binfile_finalise_load(bf->ar_elts[arid]);

         //Creates an asmfile structure associated to the member
         bf->ar_elts[arid]->asmfile = asmfile_new(bf->ar_elts[arid]->filename);
         asmfile_set_binfile(bf->ar_elts[arid]->asmfile, bf->ar_elts[arid]);
      }
   }

   //Sorts the array of loaded and code sections by address
   if (bf->codescns)
      SORT_SCNARRAY(bf->codescns, bf->n_codescns);
   if (bf->loadscns)
      SORT_SCNARRAY(bf->loadscns, bf->n_loadscns);

   //Sorts the array of segments
   if (bf->segments)
      qsort(bf->segments, bf->n_segments, sizeof(binseg_t*),
            binseg_cmpbyaddress_qsort);

   //Sorts the arrays of sections by segments
   uint16_t i;
   for (i = 0; i < bf->n_segments; i++)
      if (bf->segments[i]->n_scns > 0)
         SORT_SCNARRAY(bf->segments[i]->scns, bf->segments[i]->n_scns);
   /**\todo (2015-02-17) We could perform a sort on section offset here instead, but this sort is mostly cosmetic when printing the file.
    * Until I realise I need it for the patch, of course (but then again, sorting against virtual address makes more sense).*/

   //Attempts to link unlinked pointer data entries
   binfile_link_data_ptrs(bf);
}

/*
 * Parses a binary file and stores the result in a new structure
 * \param filename Path to the binary file to parse
 * \param binfile_loader Pointer to a function performing the loading of a binary file in a binfile structure and returning TRUE if successful
 * \return A newly allocated structure representing the parsed file
 * */
binfile_t* binfile_parse_new(char* filename, binfile_loadfct_t binfile_loader)
{
   //First, checking that the file actually exist
   if (!fileExist(filename)) {
      //ERRMSG("Unable to parse binary file %s: file does not exist\n", filename);
      return NULL;
   }
   binfile_t* bf = binfile_new(filename);
   int res = binfile_loader(bf);
   if (ISERROR(res)) {
      ERRMSG("File %s has an unrecognised or unsupported format\n", filename);
      binfile_free(bf);
      bf = NULL;
   }
   if (bf) {
      binfile_finalise_load(bf);
   }
   return bf;
}

/*
 * Attempts to load the debug information contained in the file and returns a description of them
 * \param bf Structure representing a binary file
 * \return A structure describing the debug information contained in the file, or NULL if \c bf is NULL.
 * If the file does not contain debug information, the returned structure will have its format set to \DBG_NONE
 * */
dbg_file_t* binfile_parse_dbg(binfile_t* bf)
{
   if (!bf)
      return NULL;
   return bf->driver.binfile_parse_dbg(bf);
}

/**
 * Retrieves the index of a given entry in a section
 * \param scn The section
 * \param entry The entry
 * \return Index of the entry, or BF_ENTID_ERROR if not found
 * */
static uint32_t binscn_findentryid(binscn_t* scn, data_t* entry)
{
   /**\todo TODO (2014-06-10) Either find some way to remove the use of this function, or set it as accessible from elsewhere.
    * It could horribly slow down some processes*/
   assert(scn && entry);
   uint32_t i = 0;
   maddr_t addr = data_get_addr(entry);
   //First attempting to retrieve the index based on the address of the entry
   if (addr > 0) {
      if (scn->entrysz > 0) {
         //If the section has entries of fixed sizes, attempting to retrieve its index from it
         i = (addr - scn->address) / scn->entrysz;
         if (i < scn->n_entries && scn->entries[i] == entry)
            return i;
      } else {
         //Entries have variable size: performing a binary search to find the entry (we can't use bsearch here since we need the index)
         uint32_t begin = 0;
         uint32_t end = scn->n_entries - 1;
         uint32_t middle = (begin + end) / 2;
         while (end > (begin + 1)
               && data_get_addr(scn->entries[middle]) != addr) {
            if (data_get_addr(scn->entries[middle]) > addr) {
               end = middle;
            } else if (data_get_addr(scn->entries[middle]) < addr) {
               begin = middle;
            }
            middle = (begin + end) / 2;
         }
         if (scn->entries[middle] == entry)
            return middle;
      }
   }
   //Otherwise we have to take the slow path to find the index
   for (i = 0; i < scn->n_entries; i++)
      if (scn->entries[i] == entry)
         return i;

   return BF_ENTID_ERROR;
}

/*
 * Finds the index of a label stored in a section
 * \param scn The section
 * \param label The label
 * \return Index of the label, or BF_ENTID_ERROR if not found
 * */
uint32_t binscn_find_label_id(binscn_t* scn, label_t* label)
{
   /**\todo TODO (2014-10-21) See the comment on binscn_findentryid*/
   if (!scn || !label || scn->type != SCNT_LABEL)
      return BF_ENTID_ERROR;
   uint32_t i = 0;
   for (i = 0; i < scn->n_entries; i++)
      if (data_get_data_label(scn->entries[i]) == label)
         return i;
   return BF_ENTID_ERROR;
}

/**
 * Finds the next index of a label in an array of ordered labels with a greater address than the current index, and with a non empty name in case of ties
 * \param lbls Array of ordered labels
 * \param n_lbls Size of \c lbls
 * \param startidx Index at which to start the search.
 * \return The index or BF_ENTID_ERROR if no label was found
 * */
static uint32_t find_lblnextaddr(label_t** lbls, uint32_t n_lbls,
      uint32_t startidx)
{
   /**\todo TODO (2014-11-27) Refactor this, I'm pretty sure we can find a more efficient algorithm, but I already spent too much time on this bloody problem*/
   assert(lbls);
   if (startidx >= (n_lbls - 1))
      return BF_ENTID_ERROR; //We are at the last element in the array or beyond: no more indices to find

   int64_t addr = label_get_addr(lbls[startidx]);
   uint32_t i = startidx;

   //Looks for the first label with an address greater than the address of the current label
   while ((i < n_lbls) && (label_get_addr(lbls[i]) == addr))
      i++;

   if (i == n_lbls)
      return BF_ENTID_ERROR; //Reached the end of the array without finding a label with a superior address

   if ((i == (n_lbls - 1)) || (strlen(label_get_name(lbls[i])) > 0))
      return i; //Reached the last cell of the array or found a label with non empty name

   // At this point we need to find the first label with a non empty name (labels are ordered by address then name, and labels with an empty name come first)
   uint32_t nextidx = i;
   addr = label_get_addr(lbls[i]);
   while ((i < n_lbls) && (label_get_addr(lbls[i]) == addr)
         && (strlen(label_get_name(lbls[i])) == 0))
      i++;

   // If we reached either the end of the array or a label with a different address encountering only labels with empty names, we return the label we had found previously
   if ((i == n_lbls) || (label_get_addr(lbls[i]) != addr))
      return nextidx;
   else
      return i;  // We found a label at the same address with a non empty name
}

/**
 * Sets the target of labels in an array before and after a given index and having the same address as the label at the address (or the entry)
 * \param lbls Array of ordered labels
 * \param n_lbls Size of \c lbls
 * \param lastlblidx Index at which to start the search
 * \param entry The entry
 * \param addr The address
 * */
static void set_lbls_target(label_t** lbls, uint32_t n_lbls,
      uint32_t lastlblidx, data_t* entry, int64_t addr)
{
   assert(
         lbls && n_lbls && (lastlblidx < n_lbls) && entry
               && addr == data_get_addr(entry)
               && addr == label_get_addr(lbls[lastlblidx]));

   uint32_t i = lastlblidx + 1;
   //Links labels following the current label to the entry
   while ((i < n_lbls) && label_get_addr(lbls[i]) == addr) {
      if (!label_is_type_function(lbls[i]))
         label_set_target_to_data(lbls[i], entry); //Updates the label only if it can not be associated to an instruction
      i++;
   }
   i = lastlblidx; //We are using unsigned here, so we can't start at lastlblidx-1.... *trollface*
   //Links labels preceding the current label to the entry
   while ((i > 0) && label_get_addr(lbls[i - 1]) == addr) {
      if (!label_is_type_function(lbls[i - 1]))
         label_set_target_to_data(lbls[i - 1], entry); //Updates the label only if it can not be associated to an instruction
      i--;
   }
   /**\todo TODO (2015-02-04) The test of the label not being associated to an instruction is to avoid accidentally reassociating a label
    * already associated to an instruction to a data structure. This occurred when a badly disassembled instruction used a RIP-based operand
    * corresponding to the address of an existing instruction. Of course, the reverse can happen as well, with a legitimate RIP-based operand
    * pointing to a data interleaved with code, which we would miss to associate to a label in this case. So... find something more rigorous to handle this.
    * Also, avoid invoking two bloody functions to check whether a label was already associated to an instruction (possibly creating a label_getinsntarget function)
    * => (2015-02-09) Updated to check on the type of the label. To be checked if it works (especially if we have to check whether it's >=LBL_NOFUNCTION as it
    * is now or >LBL_NOFUNCTION. And check for any other cases*/
}

/**
 * Associates an entry with labels
 * */
static void link_data_to_lbl(label_t** lbls, uint32_t n_lbls,
      uint32_t lastlblidx, data_t* entry, int64_t addr)
{
   assert(lbls);
   if (lastlblidx < n_lbls) {
      //The last label exists: linking the entry to it
      if (!label_is_type_function(lbls[lastlblidx]))
         data_link_label(entry, lbls[lastlblidx]); //Updates the label only if it can not be associated to an instruction
      /**\todo TODO (2015-02-09) See the TODO above in set_lbls_target about the test on the type of label to associate it to a data entry*/
      if (addr == label_get_addr(lbls[lastlblidx])) {
         //We are at the exact address of the last label: we will also associate the labels at the same address to this entry
         set_lbls_target(lbls, n_lbls, lastlblidx, entry, addr);
      } // End of the case where the last label has the same address as the entry
   } //End of the case where there is a last label
}

/**
 * Finds the first label of type LBL_VARIABLE in an array before a given address
 * \param lbls Array of labels
 * \param n_lbls Size of the array
 * \param addr Address that must be superior or equal to the address of the label found
 * \return Index of the label of type LBL_VARIABLE whose address is lower than or equal \c addr and the
 * highest possible in \c lbls to obey this, or BF_ENTID_ERROR if no such label was found
 * */
static uint32_t find_varlbl_beforeaddr(label_t** lbls, uint32_t n_lbls,
      maddr_t addr)
{
   assert(lbls && n_lbls > 0);
   uint32_t lblidx = BF_ENTID_ERROR;

   if (addr < label_get_addr(lbls[0]))
      return lblidx; //Address strictly inferior to the address of the first label: no label found
   if (addr >= label_get_addr(lbls[n_lbls - 1]))
      lblidx = n_lbls - 1; //Address superior or equal to the address of the last label: it's the last label
   else {
      uint32_t begin = 0;
      uint32_t end = n_lbls - 1;
      uint32_t middle = (begin + end) / 2;
      //Performs a dichotomy to find i so that lbls[i]->address <= addr < lbls[i+1]->address
      while (end > (begin + 1) && label_get_addr(lbls[middle]) != addr) {
         if (label_get_addr(lbls[middle]) > addr) {
            end = middle;
         } else if (label_get_addr(lbls[middle]) < addr) {
            begin = middle;
         }
         middle = (begin + end) / 2;
      }
      lblidx = middle;
   }
   //Now removing labels of type LBL_NOVARIABLE
   while (lblidx > 0 && label_get_type(lbls[lblidx]) >= LBL_NOVARIABLE)
      lblidx--;
   //Reached the first label without finding a label of type variable
   if (lblidx == 0 && label_get_type(lbls[lblidx]) >= LBL_NOVARIABLE)
      lblidx = BF_ENTID_ERROR;

   return lblidx;
}

/**
 * Updates the label of a given entry in a section and links the label to the entry if necessary.
 * \param scn The section
 * \param entry The entry (assumed to belong to the section)
 * */
static void binscn_updentrylbl(binscn_t* scn, data_t* entry)
{
   assert(scn && entry);

   binfile_t* bf = scn->binfile;
   uint16_t scnid = scn->scnid;
   assert(
         bf && (scnid < bf->n_sections) && bf->lbls_by_scn
               && bf->n_lbls_by_scn); //If the case can happen where scn->binfile is NULL, use an if instead

   if (bf->n_lbls_by_scn[scnid] == 0)
      return; //This section does not contain labels: no labels will ever need to be associated to entries

   uint32_t n_lbls = bf->n_lbls_by_scn[scnid];
   label_t** lbls = bf->lbls_by_scn[scnid];
   int64_t addr = data_get_addr(entry);

   uint32_t lastlblidx = find_varlbl_beforeaddr(lbls, n_lbls, addr);

   if (lastlblidx < n_lbls) {
      //Label found: linking it to the entry
      link_data_to_lbl(lbls, n_lbls, lastlblidx, entry, addr);
   }
}

// Functions for loading the data in a binary file into a binfile //
////////////////////////////////////////////////////////////////////

/**
 * Adds an entry to a binary section at a given index.
 * If the index is larger than the number of entries in the section, the array of entries will be increased by one
 * and the entry added as the last.
 * If the section is flagged as loaded to memory, the address of the entry will be updated based on the address of the previous entry.
 * \param scn The section
 * \param entry The entry to add
 * \param entryid The index where to insert the entry. If higher than the number of entries (e.g. BF_ENTID_ERROR or scn->n_entries),
 * the array of entries in the section will be increased by one and the entry inserted last.
 * */
static void binscn_add_entry_s(binscn_t* scn, data_t* entry, uint32_t entryid)
{
   assert(scn && entry);

   //Adds the entry to the file
   if (scn->n_entries <= entryid) {
      //Given index larger than the number of entries in the section
      ADD_CELL_TO_ARRAY(scn->entries, scn->n_entries, entry)
      entryid = scn->n_entries - 1;
   } else {
      scn->entries[entryid] = entry;
   }
   if (scn->attrs & SCNA_LOADED) { //Address is only useful if the section is to be loaded to memory
      /*Stores the address of the entry*/
      if (entryid > 0) //Not the first entry: we add the address of the previous entry to its size to get the address of this entry
         data_set_addr(entry, data_get_end_addr(scn->entries[entryid - 1]));
      else if (!data_get_label(entry)) //First entry, no associated label: its address is the address of the section
         data_set_addr(entry, scn->address);
      else
         //Associated label: using its address
         data_set_addr(entry, label_get_addr(data_get_label(entry)));
      /**\note (2014-12-03) The test of the label is here to handle those labels that are associated to a section but with an address
       * below the address of the section, which are incidentally beginning to annoy the hell out of me*/
      if (!data_get_label(entry)) {
         //Updates the label associated to the entry if it does not already have one
         binscn_updentrylbl(scn, entry);
      }
   }
   //Forcing the type of data to DATA_NIL if the section is of type ZERODATA
   if (scn->type == SCNT_ZERODATA)
      data_set_type(entry, DATA_NIL);

   //Associates the entry to the section if it was not already associated to a label
   if (!data_get_label(entry))
      data_set_scn(entry, scn);
}

/*
 * Parses a section containing strings to build an array of data entries.
 * \param scn The binscn_t structure to fill. Its type must be SCNT_STRING and its \c data must be filled
 * \return EXIT_SUCCESS if the operation was successful, error code otherwise
 * */
int binscn_load_str_scn(binscn_t* scn)
{
   if (!scn)
      return ERR_BINARY_MISSING_SECTION;
   if (scn->type != SCNT_STRING)
      return ERR_BINARY_BAD_SECTION_TYPE;
   if (!scn->data)
      return ERR_BINARY_SECTION_EMPTY;

   uint64_t off = 0, len = 0;
   char* data = (char*) scn->data;
   len = scn->size;

   // For coherence, we will load each string in the table in data_t structure of type STRING.
   // The catch here is that we need to find the end of each string manually. They are assumed to be NULL-terminated
   while (off < len) {
      //Retrieves the string at the current offset
      char* str = data + off;
      //Creates a new entry containing the string
      binscn_add_entry_s(scn, data_new(DATA_STR, str, strlen(str) + 1),
            BF_ENTID_ERROR);
      //Updates offset
      off += strlen(str) + 1;
   }

   return EXIT_SUCCESS;
}

/*
 * Parses a section containing entries with a fixed length and not associated to a type requiring a special handling.
 * \param scn The binscn_t structure to fill. Its \c data, \c n_entries and \c entrysz members must be filled
 * \param type Type of the data to create. Accepted values are DATA_RAW, DATA_VAL and DATA_NIL
 * Entries created with type DATA_RAW will point to the data element in the original section, while entries with DATA_VAL will contain
 * the corresponding value.
 * \return EXIT_SUCCESS if the operation was successful, error code otherwise, including if the section has more than one entry while entrysz is set to 0
 * */
int binscn_load_entries(binscn_t* scn, datatype_t type)
{
   if (!scn)
      return ERR_BINARY_MISSING_SECTION;
   if (!scn->data)
      return ERR_BINARY_SECTION_EMPTY;
   if (!scn->entrysz && scn->n_entries > 1)
      return ERR_BINARY_BAD_SECTION_ENTRYSZ;
   if (type != DATA_RAW && type != DATA_VAL && type != DATA_NIL)
      return ERR_LIBASM_INCORRECT_DATA_TYPE;
   //Handling a special case where the section contains a single entry
   if (scn->n_entries == 1) {
      binscn_add_entry_s(scn, data_new(type, scn->data, scn->size), 0);
      return EXIT_SUCCESS;
   }
   //Initialising the number of entries if it was not already
   if (scn->entrysz > 0 && scn->n_entries == 0)
      binscn_set_nb_entries(scn, scn->size / scn->entrysz);

   //Now processing the general case
   uint32_t i;
   for (i = 0; i < scn->n_entries; i++) {
      binscn_add_entry_s(scn,
            data_new(type, scn->data + i * scn->entrysz, scn->entrysz), i);
   }
   return EXIT_SUCCESS;
}

/**
 * Loads a binary header as a section. This is used for loading the section and/or segment header from the file
 * \param bf The binary file
 * \param hdrid Identifier of the header
 * \param offset Offset into the file at which the table of headers can be found
 * \param address Address into which the table of headers is loaded in memory
 * \param size Size in bytes of the table of headers
 * \param hdrentsz Size in bytes of an entry in the table of headers
 * \param data The table of headers
 * \return The new section if the operation was successful, NULL otherwise
 * */
static binscn_t* binfile_loadheader(binfile_t* bf, uint16_t hdrid,
      uint64_t offset, int64_t address, uint64_t size, uint64_t hdrentsz,
      void* data)
{
   assert(bf);

   binscn_t* header = binscn_new(bf, hdrid, NULL, 0, SCNT_HEADER, SCNA_NONE);
   header->offset = offset;
   header->address = address;
   header->size = size;
   header->entrysz = hdrentsz;
   header->data = data;

   //Initialises the array of data
   binscn_load_entries(header, DATA_RAW);

   return header;
}

/*
 * Loads the table of section headers from a binary file
 * \param bf The binary file
 * \param offset Offset into the file at which the table of section headers can be found
 * \param address Address into which the table of section headers is loaded in memory
 * \param size Size in bytes of the table of section headers
 * \param hdrentsz Size in bytes of an entry in the table of section headers
 * \param data The table of section headers
 * \return EXIT_SUCCESS if the operation was successful, error code otherwise or if the section header was already loaded
 * */
int binfile_load_scn_header(binfile_t* bf, uint64_t offset, int64_t address,
      uint64_t size, uint64_t hdrentsz, void* data)
{
   /**\todo TODO (2015-04-07) Depending on how this evolves, we could tie this with the creation of sections*/
   if (!bf)
      return ERR_BINARY_MISSING_BINFILE;
   if (!data)
      return ERR_BINARY_HEADER_NOT_FOUND;
   //Creates the section header if it was not already existing
   if (!bf->scnheader) {
      bf->scnheader = binfile_loadheader(bf, BF_SCNHDR_ID, offset, address,
            size, hdrentsz, data);
      return EXIT_SUCCESS;
   } else
      return ERR_BINARY_HEADER_ALREADY_PARSED;
}

/*
 * Loads the table of segment headers from a binary file
 * \param bf The binary file
 * \param offset Offset into the file at which the table of segment headers can be found
 * \param address Address into which the table of segment headers is loaded in memory
 * \param size Size in bytes of the table of segment headers
 * \param hdrentsz Size in bytes of an entry in the table of segment headers
 * \param data The table of segment headers
 * \return EXIT_SUCCESS if the operation was successful, error code otherwise or if the segment header was already loaded
 * */
int binfile_load_seg_header(binfile_t* bf, uint64_t offset, int64_t address,
      uint64_t size, uint64_t hdrentsz, void* data)
{
   /**\todo TODO (2015-04-07) Depending on how this evolves, we could tie this with the creation of segments*/
   if (!bf)
      return ERR_BINARY_MISSING_BINFILE;
   if (!data)
      return ERR_BINARY_HEADER_NOT_FOUND;
   //Creates the section header if it was not already existing
   if (!bf->segheader) {
      bf->segheader = binfile_loadheader(bf, BF_SEGHDR_ID, offset, address,
            size, hdrentsz, data);
      return EXIT_SUCCESS;
   } else
      return ERR_BINARY_HEADER_ALREADY_PARSED;
}

/*
 * Initialises the section containing the symbol table.
 * This is reserved for binary formats where the symbol table is not a section
 * \param bf The binary file
 * \return EXIT_SUCCESS or error code
 * */
int binfile_init_sym_table(binfile_t* bf)
{
   return
         (binfile_init_scn(bf, BF_SYMTBL_ID, NULL, SCNT_LABEL, 0, SCNA_READ)) ?
               EXIT_SUCCESS : EXIT_FAILURE;
}

// Setters and getters //
/////////////////////////

/*
 * Returns the architecture for which this binary file was compiled
 * \param bf Structure representing a binary file
 * \return arch_t structure characterising the architecture, or NULL if \c bf is NULL
 * */
arch_t* binfile_get_arch(binfile_t* bf)
{
   return (bf) ? bf->arch : NULL;
}

/*
 * Returns the name of the file of this binary file
 * \param bf Structure representing a binary file
 * \return Name of the file, or NULL if \c bf is NULL
 * */
char* binfile_get_file_name(binfile_t* bf)
{
   return (bf) ? bf->filename : NULL;
}

/*
 * Returns the name of an external dynamic library used by this binary file.
 * \param bf Structure representing a binary file
 * \param i Index of the external library
 * \return Name of the external library at the given address, or NULL if \c bf is NULL
 * or \c i is superior to the number of external libraries to the file
 * */
char* binfile_get_ext_lib_name(binfile_t* bf, unsigned int i)
{
   if (!bf || (i >= bf->n_extlibs))
      return NULL;
   return data_get_string(pointer_get_data_target(data_get_pointer(bf->extlibs[i])));
}

/*
 * Returns the number of sections in a binary file
 * \param bf Structure representing a binary file
 * \return The number of sections in the file
 * */
uint16_t binfile_get_nb_sections(binfile_t* bf)
{
   return (bf) ? bf->n_sections : 0;
}

/*
 * Returns the number of segments in a binary file
 * \param bf Structure representing a binary file
 * \return The number of segments in the file
 * */
uint16_t binfile_get_nb_segments(binfile_t* bf)
{
   return (bf) ? bf->n_segments : 0;
}

/*
 * Returns the array of sections containing executable code in a binary file
 * \param bf Structure representing a binary file
 * \return The array of sections containing executable code in the file, or NULL if \c bf is NULL
 * */
binscn_t** binfile_get_code_scns(binfile_t* bf)
{
   return (bf) ? bf->codescns : NULL;
}

/*
 * Returns a section containing code base on its id among other section containing code
 * \param bf Structure representing a binary file
 * \param codescnid Index of a section in the array of sections containing code
 * \return The section containing code at the index \c codescnid in the codescns array or NULL if codescnid is out of range or if \c bf is NULL
 * */
binscn_t* binfile_get_code_scn(binfile_t* bf, uint16_t codescnid)
{
   if (!bf || (codescnid >= bf->n_codescns))
      return NULL;
   return bf->codescns[codescnid];
}

/*
 * Retrieves the name of a section in a binary file
 * \param scn The section
 * \return The name of the section or PTR_ERROR if the section is NULL
 * */
char* binscn_get_name(binscn_t* scn)
{
   if (!scn)
      return PTR_ERROR;
   if (scn->name)
      return scn->name;
   //Detecting special cases of virtual sections
   switch (scn->scnid) {
   case BF_SEGHDR_ID:
      return SEGHDR_NAME;
   case BF_SCNHDR_ID:
      return SCNHDR_NAME;
   case BF_SYMTBL_ID:
      return SYMTBL_NAME;
   }
   return PTR_ERROR;
}

/*
 * Returns the data contained in a section
 * \param scn Structure representing a parsed binary section
 * \param len Return parameter. If not NULL, contains the size in bytes of the data
 * \return A stream of data corresponding to the bytes present in the section, or NULL if the \c scn is NULL
 * */
unsigned char* binscn_get_data(binscn_t* scn, uint64_t* len)
{
   if (!scn)
      return NULL;
   if (len)
      *len = scn->size;
   return scn->data;
}

/*
 * Returns a pointer to the data contained in a section at a given offset
 * \param scn Structure representing a parsed binary section
 * \param off Offset where to look in the section data.
 * \return A stream of data corresponding to the bytes present in the section at the given offset, or NULL if \c scn is NULL
 * or if \c off is larger than its length
 * */
unsigned char* binscn_get_data_at_offset(binscn_t* scn, uint64_t off)
{
   uint64_t len = 0;
   unsigned char* data = binscn_get_data(scn, &len);
   if (data && (off < len))
      return data + off;
   else
      return NULL;
}

/*
 * Retrieves the internal structure associated to a given entry in an binary section.
 * \param scn A parsed binary section
 * \param entryid Index of the entry in the section
 * \return Pointer to the object corresponding to the entry. This only works for sections containing entries with a fixed size
 * (should be SCNT_REFS, SCNT_RELOC, SCNT_LABEL). It's up to the caller to cast it into the appropriate type
 * */
void* binscn_get_entry_data(binscn_t* scn, uint32_t entryid)
{
   if ((!scn) || (scn->entrysz == 0) || (entryid >= scn->n_entries))
      return NULL;

   //Retrieves the size of the entry (it should be the same for all entries in the section)
   assert(scn->data && (scn->size >= (scn->entrysz * (entryid + 1)))); //We really should not be out of the section, or something very wrong occurred

   return (scn->data + entryid * scn->entrysz);
}

/*
 * Retrieves the size of a section in a binary file
 * \param scn The section
 * \return The size of the section data or UNSIGNED_ERROR if the section is NULL
 * */
uint64_t binscn_get_size(binscn_t* scn)
{
   return (scn) ? scn->size : UNSIGNED_ERROR;
}

/*
 * Retrieves the index of a section in a binary file
 * \param scn The section
 * \return The index of the section or BF_SCNID_ERROR if the section is NULL
 * */
uint16_t binscn_get_index(binscn_t* scn)
{
   return (scn) ? scn->scnid : BF_SCNID_ERROR;
}

/*
 * Returns the attributes of a section
 * \param scn The section
 * \return The attributes of the section
 */
uint16_t binscn_get_attrs(binscn_t* scn)
{
   return (scn) ? scn->attrs : UNSIGNED_ERROR;
}

/*
 * Retrieves the virtual address of a section in a binary file
 * \param scn The section
 * \return The address of the section or ADDRESS_ERROR if the section is NULL
 * */
int64_t binscn_get_addr(binscn_t* scn)
{
   return (scn) ? scn->address : ADDRESS_ERROR ;
}

/*
 * Retrieves the virtual address of the end of a section in a binary file
 * \param scn The section
 * \param The sum of the address of the section with its size, or ADDRESS_ERROR if the section is null
 * */
int64_t binscn_get_end_addr(binscn_t* scn)
{
   return (scn) ? (scn->address + (int64_t) scn->size) : ADDRESS_ERROR ;
}

/*
 * Retrieves alignement of a section in a binary file
 * \param scn The section
 * \return The alignment of the section or UINT32_MAX if the section is NULL
 * */
uint64_t binscn_get_align(binscn_t* scn)
{
   return (scn) ? scn->align : UINT64_MAX;
}

/*
 * Retrieves the offset of a section in a binary file
 * \param scn The section
 * \return The offset of the section or OFFSET_ERROR if the section is NULL
 * */
uint64_t binscn_get_offset(binscn_t* scn)
{
   return (scn) ? scn->offset : OFFSET_ERROR;
}

/*
 * Retrieves the offset of the end of a section in a binary file
 * \param scn The section
 * \param The sum of the offset of the section with its size if it is not of type SCNT_ZERODATA, the offset
 * of the section if it is, and OFFSET_ERROR if the section is null
 * */
uint64_t binscn_get_end_offset(binscn_t* scn)
{
   if (!scn)
      return OFFSET_ERROR;
   uint64_t size = scn->size;
   if (scn->type == SCNT_ZERODATA)
      size = 0;   //Section has a null size in the file
   else if (scn->type == SCNT_PATCHCOPY
         && binscn_get_type(binscn_patch_get_origin(scn)) == SCNT_ZERODATA)
      size = 0;   //Section is a copy of a section with a null size in the file
   return scn->offset + size;
}

/*
 * Retrieves the array of entries of a section in a binary file
 * \param scn The section
 * \return The array of entries or PTR_ERROR if the section is NULL
 * */
data_t** binscn_get_entries(binscn_t* scn)
{
   return (scn) ? scn->entries : PTR_ERROR;
}

/*
 * Retrieves an entry from a section in a binary file
 * \param scn The section
 * \param entryid The index of the entry in the section
 * \return Pointer to the data_t structure representing the entry, or NULL if scn is NULL or entryid greater than the number of its entries
 * */
data_t* binscn_get_entry(binscn_t* scn, uint32_t entryid)
{
   return (scn && (entryid < scn->n_entries)) ? scn->entries[entryid] : NULL;
}

/*
 * Retrieves the number of data entries of a section in a binary file
 * \param scn The section
 * \return The number of data entries of the section or UNSIGNED_ERROR if the section is NULL
 * */
uint32_t binscn_get_nb_entries(binscn_t* scn)
{
   return (scn) ? scn->n_entries : UNSIGNED_ERROR;
}

/*
 * Retrieves the type of a section in a binary file
 * \param scn The section
 * \return The type of the section or SCNT_UNKNOWN if the section is NULL
 * */
uint8_t binscn_get_type(binscn_t* scn)
{
   return (scn) ? scn->type : SCNT_UNKNOWN;
}

/*
 * Checks if a given flag is present among the attributes of a section
 * \param scn The section
 * \param attr The flag to check in the section attributes
 * \return TRUE if the attribute is present, FALSE if not or if the section is NULL
 * */
int binscn_check_attrs(binscn_t* scn, uint16_t attr)
{
   return (scn && ((scn->attrs & attr) == attr)) ? TRUE : FALSE;
}

/*
 * Retrieves the size of entries in a section
 * \param scn Structure representing a binary section
 * \return The size of entries in the section or UNSIGNED_ERROR if the section is NULL
 * */
uint64_t binscn_get_entry_size(binscn_t* scn)
{
   return (scn) ? scn->entrysz : UNSIGNED_ERROR;
}

/*
 * Retrieves the binary file to which a section belongs
 * \param scn The section
 * \return A pointer to the binary file containing the section or PTR_ERROR if the section is NULL
 * */
binfile_t* binscn_get_binfile(binscn_t* scn)
{
   return (scn) ? scn->binfile : PTR_ERROR;
}

/*
 * Retrieves the first instruction in the section
 * \param scn Structure representing a binary section
 * \return List node containing the first instruction of the section or NULL if \c scn is NULL
 * */
list_t* binscn_get_first_insn_seq(binscn_t* scn)
{
   return (scn) ? scn->firstinsnseq : NULL;
}

/*
 * Retrieves the last instruction in the section
 * \param scn Structure representing a binary section
 * \return List node containing the last instruction of the section or NULL if \c scn is NULL
 * */
list_t* binscn_get_last_insn_seq(binscn_t* scn)
{
   return (scn) ? scn->lastinsnseq : NULL;
}

/*
 * Retrieves one of the segments to which the section is associated
 * \param scn A section
 * \param sgid Index of the segment in the list of segments to which the section is associated
 * \return Pointer to the segment at the given index among the segments associated to this section,
 * or NULL if scn is NULL or sgid greater than or equal the number of segments associated to this section
 * */
binseg_t* binscn_get_binseg(binscn_t* scn, uint16_t sgid)
{
   return (scn && sgid < scn->n_binsegs) ? scn->binsegs[sgid] : NULL;
}

/*
 * Retrieves the number of segments to which the section is associated
 * \param scn A section
 * \return Number of segments associated to this section or 0 if scn is NULL
 * */
uint16_t binscn_get_nb_binsegs(binscn_t* scn)
{
   return (scn) ? scn->n_binsegs : 0;
}

/*
 * Sets the index of a section in a binary file
 * \param scn  Structure representing a binary section
 * \param scnid Index of the section in the binary file to which it belongs
 * */
void binscn_set_id(binscn_t* scn, uint16_t scnid)
{
   if (scn)
      scn->scnid = scnid;
}

/*
 * Adds an entry to a binary section at a given index.
 * If the index is larger than the number of entries in the section, the array of entries will be increased by one
 * and the entry added as the last.
 * If the section is flagged as loaded to memory, the address of the entry will be updated based on the address of the previous entry.
 * \param scn The section
 * \param entry The entry to add
 * \param entryid The index where to insert the entry. If higher than the number of entries (e.g. BF_ENTID_ERROR or scn->n_entries),
 * the array of entries in the section will be increased by one and the entry inserted last.
 * */
void binscn_add_entry(binscn_t* scn, data_t* entry, uint32_t entryid)
{
   if (scn) {
      binscn_add_entry_s(scn, entry, entryid);
   }
}

/*
 * Sets the size of entries in a section
 * \param scn Structure representing a binary section
 * \param entrysz Size of entries
 * */
void binscn_set_entry_size(binscn_t* scn, uint64_t entrysz)
{
   if (scn)
      scn->entrysz = entrysz;
}

/*
 * Initialises the array of entries in a binary section
 * \param scn Structure representing a binary section
 * \param n_entries Number of entries
 * */
void binscn_set_nb_entries(binscn_t* scn, uint32_t n_entries)
{
   if (!scn || (n_entries == 0) || (n_entries == scn->n_entries))
      return;
   //Updating the size of the array of entries
   scn->entries = lc_realloc(scn->entries, sizeof(*scn->entries) * n_entries);
   if (scn->n_entries < n_entries) {
      //Size increase: setting new entries to NULL
      memset(scn->entries + scn->n_entries, 0,
            sizeof(*scn->entries) * (n_entries - scn->n_entries));
   }
   //Updating the number of entries
   scn->n_entries = n_entries;
}

/*
 * Sets the type of a binary section
 * \param scn Structure representing a binary section
 * \param type Type of the section
 * */
void binscn_set_type(binscn_t* scn, uint8_t type)
{
   if (scn)
      scn->type = type;
}

/*
 * Adds an attribute to the attributes of a binary section
 * \param scn Structure representing a binary section
 * \param attrs Attributes to add
 * */
void binscn_add_attrs(binscn_t* scn, uint16_t attrs)
{
   if (scn)
      scn->attrs |= attrs;
}

/*
 * Removes an attribute from the attributes of a binary section
 * \param scn Structure representing a binary section
 * \param attrs Attributes to remove
 * */
void binscn_rem_attrs(binscn_t* scn, uint16_t attrs)
{
   if (scn)
      scn->attrs &= ~attrs;
}

/*
 * Sets the attributes of a binary section
 * \param scn Structure representing a binary section
 * \param attrs Attributes mask to set
 * */
void binscn_set_attrs(binscn_t* scn, uint16_t attrs)
{
   if (scn)
      scn->attrs = attrs;
}

/*
 * Sets the binary file to which a binary section belongs
 * \param scn Structure representing a binary section
 * \param binfile Pointer to the binary file
 * */
void binscn_set_binfile(binscn_t* scn, binfile_t* binfile)
{
   if (scn)
      scn->binfile = binfile;
}

/*
 * Returns the stream to the file from which this binary file structure was parsed
 * \param bf Structure representing a binary file
 * \return Pointer to the stream or PTR_ERROR if \c bf is NULL
 * */
FILE* binfile_get_file_stream(binfile_t* bf)
{
   return (bf) ? bf->filestream : PTR_ERROR;
}

/*
 * Returns the patching status of a parsed binary file (as defined in \ref bf_patch)
 * \param bf Structure representing a binary file
 * \return Identifier of the patching status, or BFP_NONE if \c bf is NULL
 * */
unsigned binfile_get_patch_status(binfile_t* bf)
{
   return (bf) ? bf->patch : BFP_NONE;
}

/*
 * Returns the word size of a parsed binary file (as defined in \ref bf_wordsz)
 * \param bf Structure representing a binary file
 * \return Identifier of the word size, or BFS_UNKNOWN if \c bf is NULL
 * */
uint8_t binfile_get_word_size(binfile_t* bf)
{
   return (bf) ? bf->wordsize : BFS_UNKNOWN;
}

/*
 * Returns the ABI for which a binary file is compiled
 * \param bf Structure representing a binary file
 * \return abi_t structure characterising the ABI, or NULL if \c bf is NULL
 * */
abi_t* binfile_get_abi(binfile_t* bf)
{
   return (bf) ? bf->abi : NULL;
}

/*
 * Returns the array of sections in a binary file
 * \param bf Structure representing a binary file
 * \return Array of sections in the file, or NULL if \c bf is NULL
 * */
binscn_t** binfile_get_scns(binfile_t* bf)
{
   return (bf) ? bf->sections : NULL;
}

/*
 * Returns the number of external dynamic libraries used by this binary file.
 * \param bf Structure representing a binary file
 * \return Number of external libraries, or UNSIGNED_ERROR if \c bf is NULL
 * */
uint16_t binfile_get_nb_ext_libs(binfile_t* bf)
{
   return (bf) ? bf->n_extlibs : UNSIGNED_ERROR;
}

/*
 * Returns the number of executable loaded sections in this binary file.
 * \param bf Structure representing a binary file
 * \return Number of executable loaded sections, or UNSIGNED_ERROR if \c bf is NULL
 * */
uint16_t binfile_get_nb_code_scns(binfile_t* bf)
{
   return (bf) ? bf->n_codescns : UNSIGNED_ERROR;
}

/*
 * Returns the array of loaded sections in this binary file.
 * \param bf Structure representing a binary file
 * \return Array of loaded sections, or NULL if \c bf is NULL
 * */
binscn_t **binfile_get_load_scns(binfile_t* bf)
{
   return (bf) ? bf->loadscns : NULL;
}

/*
 * Returns a loaded section based on its id among other section containing code
 * \param bf Structure representing a binary file
 * \param loadscnid Index of a section in the array of loaded sections
 * \return The loaded section at the index \c loadscnid in the loadscns array or NULL if loadscnid is out of range or if \c bf is NULL
 * */
binscn_t* binfile_get_load_scn(binfile_t* bf, uint16_t loadscnid)
{
   if (!bf || (loadscnid >= bf->n_loadscns))
      return NULL;
   return bf->loadscns[loadscnid];
}

/*
 * Returns the number of loaded sections in this binary file.
 * \param bf Structure representing a binary file
 * \return Number of loaded sections, or UNSIGNED_ERROR if \c bf is NULL
 * */
uint16_t binfile_get_nb_load_scns(binfile_t* bf)
{
   return (bf) ? bf->n_loadscns : UNSIGNED_ERROR;
}

/*
 * Returns the number of labels in this binary file.
 * \param bf Structure representing a binary file
 * \return Number of labels, or UNSIGNED_ERROR if \c bf is NULL
 * */
uint32_t binfile_get_nb_labels(binfile_t* bf)
{
   return (bf) ? bf->n_labels : UNSIGNED_ERROR;
}

/*
 * Returns the a label at a given index in this binary file.
 * \param bf Structure representing a binary file
 * \param labelid Index of a label
 * \return Label at this index in the list of labels defined in the file, or NULL if \c bf is NULL or labelid above the number of labels
 * */
label_t* binfile_get_file_label(binfile_t* bf, uint32_t labelid)
{
   return (bf && (labelid < bf->n_labels)) ? bf->labels[labelid] : NULL;
}

/*
 * Returns a pointer to the driver containing functions for accessing the underlying format-dependent structure representing the binary file
 * \param bf Structure representing a binary file
 * \return Driver containing functions for accessing the underlying format-dependent structure representing the binary file, or NULL if \c bf is NULL
 * */
bf_driver_t* binfile_get_driver(binfile_t* bf)
{
   return (bf) ? &(bf->driver) : NULL;
}

/*
 * Returns a pointer to the format-specific structure representing the parsed binary file
 * \param bf Structure representing a binary file
 * \return Pointer to a format-specific structure built by the format-specific binary parser or NULL if \c bf is NULL or has not been parsed
 * */
void* binfile_get_parsed_bin(binfile_t* bf)
{
   return (bf) ? bf->driver.parsedbin : NULL;
}

/*
 * Returns the structure representing the given archive member in this binary file if it is an archive.
 * \param bf Structure representing a binary file
 * \param i Index of the archive member to be retrieved
 * \return Structure representing the archive member at the given index in an archive file,
 * or NULL if \c bf is NULL, not an archive, or if i is higher than its number of archive elements
 * */
binfile_t* binfile_get_ar_elt(binfile_t* bf, uint16_t i)
{
   return (bf && i < bf->n_ar_elts) ? bf->ar_elts[i] : NULL;
}

/*
 * Returns the number of archive members in this binary file.
 * \param bf Structure representing a binary file
 * \return Number of members, or UNSIGNED_ERROR if \c bf is NULL
 * */
uint16_t binfile_get_nb_ar_elts(binfile_t* bf)
{
   return (bf) ? bf->n_ar_elts : UNSIGNED_ERROR;
}

/*
 * Returns a pointer to the binary file from which this file is a copy
 * \param bf Structure representing a binary file
 * \return A pointer to the original file, or NULL if \c bf is NULL or not copied from another binfile_t structure
 * */
binfile_t* binfile_get_creator(binfile_t* bf)
{
   return (bf) ? bf->creator : NULL;
}

/*
 * Returns a pointer to the binary file representing an archive from which this file is a member
 * \param bf Structure representing a binary file
 * \return A pointer to the binfile_t structure representing the archive containing file, or NULL if \c bf is NULL or not an archive member
 * */
binfile_t* binfile_get_archive(binfile_t* bf)
{
   return (bf) ? bf->archive : NULL;
}

/*
 * Gets a section at a given index
 * \param bf Structure representing a binary file
 * \param scnid Index of the section
 * \return Pointer to the binscn_t structure representing the section or NULL if \c bf is NULL or scnid greater than its number of sections
 * or not one of the identifiers for the sections representing a header or a symbol table
 * */
binscn_t* binfile_get_scn(binfile_t* bf, uint16_t scnid)
{
   if (!bf)
      return NULL;
   if (scnid < bf->n_sections)
      return bf->sections[scnid];
   if (scnid == BF_SEGHDR_ID)
      return bf->segheader;
   if (scnid == BF_SCNHDR_ID)
      return bf->scnheader;
   if (scnid == BF_SYMTBL_ID)
      return bf->symtable;
   return NULL;
}

/*
 * Gets a segment at a given index
 * \param bf Structure representing a binary file
 * \param segid Index of the segment
 * \return Pointer to the binseg_t structure representing the section or NULL if \c bf is NULL or segid greater than its number of sections
 * */
binseg_t* binfile_get_seg(binfile_t* bf, uint16_t segid)
{
   /**\todo TODO (2015-02-24) I'm still playing with the segments. Right now I'm considering
    * that the array of segments has been ordered by addresses. Therefore, to get a segment
    * from its index, we need to find the segment with the given index. Files should not contain
    * a lot of segments, so I'm using something rather primitive to look for a segment based
    * on its index. If we remove the fact that segments are ordered, we could remove this code*/
   if (!bf || segid >= bf->n_segments)
      return NULL;
   uint16_t i = 0;
   while (i < bf->n_segments && bf->segments[i]->segid != segid)
      i++;
   assert(i < bf->n_segments); //Just checking, we could remove the test below as well
   return (i < bf->n_segments) ? bf->segments[i] : NULL;
}

/*
 * Gets a segment at a given index among segments ordered by starting address
 * \param bf Structure representing a binary file
 * \param segid Index of the segment among the ordered segments
 * \return Pointer to the binseg_t structure representing the section or NULL if \c bf is NULL or segid greater than its number of sections
 * */
binseg_t* binfile_get_seg_ordered(binfile_t* bf, uint16_t segid)
{
   /**\todo TODO (2015-02-24) See the todo above in binfile_get_seg. If we remove the ordering
    * of segments, this function could go.*/
   return (bf && (segid < bf->n_segments)) ? bf->segments[segid] : NULL;
}

/*
 * Returns a pointer to the asm file based on this binary file
 * \param bf Structure representing a binary file
 * \return A pointer to the asmfile_t structure representing the file contained in this binary file or NULL if \c bf is NULL
 * */
asmfile_t* binfile_get_asmfile(binfile_t* bf)
{
   return (bf) ? bf->asmfile : NULL;
}

/*
 * Returns the name of a section in a binary file
 * \param bf Structure representing a parsed binary file
 * \param scnid Index of the section
 * \return The name of the section or NULL if \c bf is NULL or scnid greater than its number of sections
 * */
char* binfile_get_scn_name(binfile_t* bf, uint16_t scnid)
{
   return
         (bf && (scnid < bf->n_sections) && (binfile_get_scn(bf, scnid))) ?
               binfile_get_scn(bf, scnid)->name : NULL;
}

/*
 * Returns the byte order in a binary file
 * \param bf Structure representing a parser binary file
 * \return The byte order of the binary file or UNSIGNED_ERROR if \c bf is NULL
 */
uint8_t binfile_get_byte_order(binfile_t* bf)
{
   return (bf) ? bf->byte_order : UNSIGNED_ERROR;
}

/*
 * Sets the name of a binary file
 * \param bf Structure representing a binary file
 * \param filename Name of the binary file
 * */
void binfile_set_filename(binfile_t* bf, char* filename)
{
   if (bf)
      bf->filename = filename;
}

/*
 * Sets the stream to a binary file
 * \param bf Structure representing a binary file
 * \param filestream Stream to the binary file
 * */
void binfile_set_filestream(binfile_t* bf, FILE* filestream)
{
   if (bf)
      bf->filestream = filestream;
}

/*
 * Sets the format of a binary file.
 * \param bf Structure representing a binary file
 * \param format Format of the binary file. Uses values from enum \ref bf_format
 * */
void binfile_set_format(binfile_t* bf, uint8_t format)
{
   if (bf)
      bf->format = format;
}

/*
 * Sets the type to a binary file
 * \param bf Structure representing a binary file
 * \param type Type of the binary file. Uses values from enum \ref bf_type
 * */
void binfile_set_type(binfile_t* bf, unsigned type)
{
   if (bf)
      bf->type = type;
}

/*
 * Sets the patching status to a binary file
 * \param bf Structure representing a binary file
 * \param patch State of this file with regard to patching: whether it is a patched file or being patched. Uses values from enum \ref bf_patch
 * */
void binfile_set_patch_status(binfile_t* bf, unsigned patch)
{
   if (bf)
      bf->patch = patch;
}

/*
 * Sets the wordsize of a binary file
 * \param bf Structure representing a binary file
 * \param wordsize Word size of the binary (32/64). Uses values from enum \ref bf_wordsz
 * */
void binfile_set_word_size(binfile_t* bf, uint8_t wordsize)
{
   if (bf)
      bf->wordsize = wordsize;
}

/*
 * Sets the architecture of a binary file
 * \param bf Structure representing a binary file
 * \param arch Pointer to the structure describing the architecture for which the file is compiled
 * */
void binfile_set_arch(binfile_t* bf, arch_t* arch)
{
   if (bf)
      bf->arch = arch;
}

/*
 * Sets the abi of a binary file
 * \param bf Structure representing a binary file
 * \param abi Pointer to the structure describing the ABI for which the file is compiled
 * */
void binfile_set_abi(binfile_t* bf, abi_t* abi)
{
   if (bf)
      bf->abi = abi;
}

/*
 * Sets a section at a given index in a binary file
 * \param bf Structure representing a binary file
 * \param section binscn_t structure representing the section to set in the file.
 * \param scnid Index of the section to add. The number of sections will be increased if it is higher than the current number.
 * */
void binfile_set_scn(binfile_t* bf, binscn_t* section, uint16_t scnid)
{
   if (!bf)
      return;
   if (scnid >= bf->n_sections && scnid < BF_LAST_ID)
      binfile_set_nb_scns(bf, bf->n_sections + 1);
   if (scnid < BF_LAST_ID)
      bf->sections[scnid] = section;
   else {
      switch (scnid) {
      case BF_SEGHDR_ID:
         bf->segheader = section;
         break;
      case BF_SCNHDR_ID:
         bf->scnheader = section;
         break;
      case BF_SYMTBL_ID:
         bf->symtable = section;
         break;
      }
   }
   if (section) {
      section->binfile = bf;
      section->scnid = scnid;
   }
}

/*
 * Sets the byte order in a binary file
 * \param bf Structure representing a binary file
 * \param byte_order Byte order scheme of the binary file. Uses values from enum \ref bf_byte_order
 * */
void binfile_set_byte_order(binfile_t* bf, uint8_t byte_order)
{
   if (bf)
      bf->byte_order = byte_order;
}

/**
 * Adds a section in a binary file to the list of loaded sections
 * \param bf Structure representing a binary file
 * \param scn The section
 * */
static void binfile_addloadscn_s(binfile_t* bf, binscn_t* scn)
{
   assert(bf && scn);
   DBGMSG("Adding section %s to the list of loaded sections of file %s\n",
         scn->name, bf->filename);
   ADD_CELL_TO_ARRAY(bf->loadscns, bf->n_loadscns, scn);
}

/**
 * Adds a section in a binary file to the list of executable loaded sections
 * \param bf Structure representing a binary file
 * \param scn The section
 * */
static void binfile_addcodescn_s(binfile_t* bf, binscn_t* scn)
{
   assert(bf && scn);
   DBGMSG("Adding section %s to the list of executable sections of file %s\n",
         scn->name, bf->filename);
   ADD_CELL_TO_ARRAY(bf->codescns, bf->n_codescns, scn);
}

/**
 * Initialises the labels associated to a section
 * \param bf Structure representing a binary file
 * \param scnid Index of the section
 * \param scn The section
 * \param updlbls Set to TRUE if we need to update labels to be able to link them to entries afterwards
 * */
static void binfile_init_scnlabels(binfile_t* bf, uint16_t scnid,
      binscn_t* scn, boolean_t updlbls)
{
   assert(bf && scn);
   if (bf->n_lbls_by_scn[scnid] == 0)
      return;
   //Updates the labels belonging to the section
   uint32_t i, nextvarlbl = 0;
   label_t** lbls = bf->lbls_by_scn[scnid];
   uint32_t n_lbls = bf->n_lbls_by_scn[scnid];
   qsort(lbls, n_lbls, sizeof(label_t*), label_cmp_qsort);
   //Finds the first label that could be associated to an entry if we need to update the labels
   if (updlbls) {
      i = 0;
      //Looks up the first label with a non empty name and the same address as the first label in the section
      while ((i < n_lbls) && (strlen(label_get_name(lbls[i])) == 0)
            && (label_get_addr(lbls[i]) == label_get_addr(lbls[0])))
         i++;
      if ((i < n_lbls) && (strlen(label_get_name(lbls[i])) != 0)
            && (label_get_addr(lbls[i]) == label_get_addr(lbls[0])))
         nextvarlbl = i; //Found a label with a non empty name and the same address as the first label: it will be used as the first
      //Otherwise we will use the first label
   }
   for (i = 0; i < n_lbls; i++) {
      //Associates the label to the section
      label_set_scn(lbls[i], scn);
      if (updlbls) {
         //We have to update label types as this section could contain entries later on
         if (i == nextvarlbl) {
            //This label can be associated to an entry: setting its type accordingly
            label_set_type(lbls[i], LBL_VARIABLE);
            DBGMSGLVL(2,
                  "Label %s at address %#"PRIx64" in section %s (%u) can be associated to variables\n",
                  label_get_name(lbls[i]), label_get_addr(lbls[i]), scn->name,
                  scnid);
            //Looks up for the next label that could be associated to entries
            nextvarlbl = find_lblnextaddr(lbls, n_lbls, i);
         } else {
            //Initialising the label type to specify that this label can't be associated to an entry
            label_set_type(lbls[i], LBL_NOVARIABLE);
         }
      }
   }
   //Sorting the labels again if we updated their types, so that labels of type LBL_VARIABLE will come first
   if (updlbls)
      qsort(lbls, n_lbls, sizeof(label_t*), label_cmp_qsort);
}

/*
 * Initialises a section at a given index in a binary file.
 * \param bf Structure representing a binary file
 * \param scnid Index of the section to add. The number of sections will be increased if it is higher than the current number.
 * \param name Name of the section
 * \param type Type of the section
 * \param address Address of the section
 * \param attrs Attributes of the section
 * \return The new section
 * */
binscn_t* binfile_init_scn(binfile_t* bf, uint16_t scnid, char* name,
      scntype_t type, int64_t address, unsigned attrs)
{
   if (!bf)
      return NULL;
   binscn_t* scn = binscn_new(bf, scnid, name, type, address, attrs);
   int updlbls = FALSE; //Detects that labels will have to be updated to be linked later to data entries
   /**\todo TODO (2014-12-02) It occurs to me that we may actually want to link labels in code sections to data entries
    * (for interleaved code and bloody ARM). So we would have always updlbls to TRUE. BUT for code sections we don't want to
    * mix labels associated to instructions (GENERIC, FUNCTION) and those possibly associated to variables (VARIABLES).
    * See how to separate the two, I'm beginning to lose focus.*/
   binfile_set_scn(bf, scn, scnid);
   //Handling loaded sections
   if (attrs & SCNA_LOADED) {
      /**\todo TODO (2014-11-28) As said in libtroll, check whether we can handle the TLS here of if it is too format specific for that.
       * Anyways so far I'm excluding them from loaded sections as they are too complicated to handle in ELF (they are loaded but their address
       * is not coherent with the other loaded sections), check with other formats if there would be a reason why they would need to be added
       * to the array of loaded section*/
      if (!(attrs & SCNA_TLS))
         binfile_addloadscn_s(bf, scn);
      //Handling executable sections
      if (type == SCNT_CODE) {
         //scn->type = SCNT_CODE;
         //Storing the section as containing executable code
         binfile_addcodescn_s(bf, scn);
      } else
         updlbls = TRUE; //We will have to update labels in order to be able to link them to entries later on
   }
   if (scn->type == SCNT_LABEL) {
      //Label section: stores it into the array of sections containing labels
      ADD_CELL_TO_ARRAY(bf->lblscns, bf->n_lblscns, scn);
   }

   //No need to link to labels if this is a section containing labels
   if ((scn->type != SCNT_LABEL) && (scn->type != SCNT_STRING))
      binfile_init_scnlabels(bf, scnid, scn, updlbls);

   return scn;
}

/* Adds a segment to a binary file
 * \param bf Structure representing a binary file
 * \param segid Index of the segment to add. The number of segments will be increased if it is higher than the current number.
 * \param offset Offset in the file at which the segment starts
 * \param address Virtual address at which the segment is loaded to memory
 * \param fsize Size of the segment in the file
 * \param msize Size of the segment in memory
 * \param attrs Attributes of the segment
 * \param align Alignment of the segment
 * \return The structure representing the segment
 * */
binseg_t* binfile_init_seg(binfile_t* bf, uint16_t segid, uint64_t offset,
      int64_t address, uint64_t fsize, uint64_t msize, uint8_t attrs,
      uint64_t align)
{
   if (!bf)
      return NULL;

   if (segid >= bf->n_segments)
      binfile_set_nb_segs(bf, segid + 1);

   bf->segments[segid] = binseg_new(segid, offset, address, fsize, msize, attrs,
         align);

   return bf->segments[segid];
}

/*
 * Adds a section in a binary file to the list of executable loaded sections
 * \param bf Structure representing a binary file
 * \param scnid Index of the section
 * */
void binfile_addcodescn(binfile_t* bf, uint16_t scnid)
{
   /**\todo TODO (2014-11-28) If this function does not need to be invoked from outside the libbin, remove it and keep only the static version*/
   if (bf && (scnid < bf->n_sections) && (binfile_get_scn(bf, scnid))) {
      binfile_addcodescn_s(bf, binfile_get_scn(bf, scnid));
   }
}

/*
 * Adds a label in a binary file
 * \param bf Structure representing a binary file
 * \param scnid Section where the label is defined
 * \param entryid The index where to add the data entry representing the label in the section. If higher than the current number of
 * entries in the section, it will be inserted after the last one.
 * \param labelid The index of the label in the array of labels from the file. If higher than the current number of labels in the file,
 * it will be inserted after the last one
 * \param label label_t structure representing the label to add
 * \param size Size of the entry representing the label
 * \param symscnid Index of the section associated to the label, or BF_SCNID_ERROR if the label is not associated to a section
 * \return A pointer to the entry representing the label
 * */
data_t* binfile_addlabel(binfile_t* bf, uint32_t scnid, uint32_t entryid,
      uint32_t labelid, label_t* label, uint64_t size, uint32_t symscnid)
{
   /**\todo TODO (2015-05-05) This cumbersome thing with scnid/entryid/labelid is due to the ELF, where: 1) there can be more than
    * one section containing labels (instead of a single table like for the other formats), 2) since we know the size of the sections,
    * we can set in advance the number of entries in a section and the number of labels as we discover each section.
    * The problem is that we can end up having to add a label that would be the first from the section but the nth in the file (where
    * n in the number of labels added by previous sections), so I want to track this efficiently, hence the use of labelid. If you
    * don't want to be bothered by those indices of labels, simply set labelid and/or entryid to UINT32_MAX.
    * In the future, find something more streamlined.
    * Oh and for the record, I tried using a local static variable that kept track of the top index in the array of labels,
    * and that does not work if you try to parse multiple files in the same execution (yep, one of my tests does that).
    * We could store this top index in the binfile_t structure, but that would be a little cumbersome (the top indices of labels per
    * section is already stupid as it is)*/
   binscn_t* scn = binfile_get_scn(bf, scnid);
   if (!scn || (scn->type != SCNT_LABEL))
      return NULL; //NULL section or wrong section type
   data_t* entry = NULL; //Data entry representing the reference
   uint64_t entrysz = (scn->entrysz > 0) ? scn->entrysz : size;
   DBGMSG("Adding label %s at address %#"PRIx64" in section %u from file %s\n",
         label_get_name(label), label_get_addr(label), symscnid,
         binfile_get_file_name(bf));

   if (labelid >= bf->n_labels) {
      //Max number of labels reached: increasing the size of labels
      bf->labels = lc_realloc(bf->labels,
            sizeof(*bf->labels) * (bf->n_labels + 1));
      labelid = bf->n_labels;
      bf->n_labels = bf->n_labels + 1;
   }
   bf->labels[labelid] = label;

   if (symscnid < bf->n_sections) {
      /*Stores the label in the array of labels for this section*/
      ADD_CELL_TO_ARRAY(bf->lbls_by_scn[symscnid], bf->n_lbls_by_scn[symscnid],
            label);
   } else {
      //Attempts to find a section associated to this label
      binscn_t* symscn = binfile_lookup_scn_span_addr(bf,
            label_get_addr(label));
      if (symscn)
         ADD_CELL_TO_ARRAY(bf->lbls_by_scn[symscnid],
               bf->n_lbls_by_scn[symscnid], label);
   }

   /*Creates an entry containing the label*/
   entry = data_new(DATA_LBL, label, entrysz);

   //Adds the entry to the array of entries in the section
   binscn_add_entry_s(scn, entry, entryid);

   return entry;
}

/*
 * Adds a label at a given index in a binary file. This function is intended to be used in loops when initialising labels
 * after the array of labels has been set using \ref binfile_set_nb_labels
 * \param bf Structure representing a binary file
 * \param labels label_t structure representing the label to add
 * \param labelid Index where to insert the label. Nothing will be done if it is higher than the current level of labels
 * */
void binfile_setlabel(binfile_t* bf, label_t* label, uint32_t labelid)
{
   if (!bf || (labelid >= bf->n_labels))
      return;
   bf->labels[labelid] = label;
}

/*
 * Sets the number of labels in a binary file. This also initialises the array of labels.
 * If the array is already initialised, it will be resized if the new size is
 * bigger than the current size.
 * \param bf Structure representing a binary file
 * \param n_labels Number of labels in the file
 * */
void binfile_set_nb_labels(binfile_t* bf, uint32_t n_labels)
{
   if (!bf || (n_labels <= bf->n_labels))
      return;
   //New number of labels is bigger
   bf->labels = lc_realloc(bf->labels, sizeof(*bf->labels) * n_labels);
   //Setting new slots to NULL
   memset(bf->labels + bf->n_labels, 0,
         sizeof(*bf->labels) * (n_labels - bf->n_labels));
   //Updating number of labels
   bf->n_labels = n_labels;
}

/*
 * Updates the labels in a binary file associated to labels or string sections. This function must be invoked once all the
 * sections containing labels have been parsed, and before other sections are parsed
 * \param bf Structure representing a binary file
 * */
void binfile_updatelabelsections(binfile_t* bf)
{
   /**\note (2015-11-17) This function does a pass on all labels to associate them to sections.
    * This could be avoided if we manage to set them during parsing, but this would be really complicated
    * because of the way they are set (see binfile_init_scnlabels)
    * It's also used to associate labels to sections containing labels (yeah...) since it is not used
    * elsewhere as we now order the labels when initialising a section*/
   if (!bf)
      return;
   uint16_t i;
   for (i = 0; i < bf->n_sections; i++) {
      binscn_t* scn = binfile_get_scn(bf, i);
      if (scn && ((scn->type == SCNT_LABEL) || (scn->type == SCNT_STRING))
            && (bf->n_lbls_by_scn[i] > 0)) {
         uint32_t j;
         // Section contains labels or strings and at least one label is affected to it
         // First initialises the labels in the section (ordering them and selecting those affected to variables)
         binfile_init_scnlabels(bf, i, scn, TRUE);
         for (j = 0; j < scn->n_entries; j++)
            binscn_updentrylbl(scn, scn->entries[j]);
      }
   }
}

/*
 * Returns an array of labels associated to a given section, ordered by address
 * \param bf Structure representing a binary file
 * \param scnid Index of the section
 * \param n_lbls Return parameter. Will contain the number of labels in the array if not NULL
 * \return Array of labels associated to the section \c scnid, ordered by address, or NULL if bf is NULL or scnid out of range
 * */
label_t** binfile_get_labels_by_scn(binfile_t* bf, uint16_t scnid,
      uint32_t* n_lbls)
{
   if (!bf || (scnid >= bf->n_sections)) {
      if (n_lbls)
         *n_lbls = 0;
      return NULL;
   }
   if (n_lbls)
      *n_lbls = bf->n_lbls_by_scn[scnid];
   return bf->lbls_by_scn[scnid];
}

static int binseg_addsection(binseg_t* seg, binscn_t* scn)
{
   assert(seg && scn);

   //Adds the section to the array of sections in the segments
   ADD_CELL_TO_ARRAY(seg->scns, seg->n_scns, scn);

   //Adds the segment to the array of segments to which this section belongs
   ADD_CELL_TO_ARRAY(scn->binsegs, scn->n_binsegs, seg)

   return EXIT_SUCCESS;
}

/*
 * Adds a section to one or more segments. If the segment is not provided, it will be searched for based on the offset in the file
 * This requires that the segments have already been loaded in the binfile
 * \param bf Structure representing a binary file
 * \param scn The section
 * \param seg The segment. If NULL, eligible segments will be searched using the offset in the file of the section
 * \return EXIT_SUCCESS if the section is not NULL could be successfully added to at least one segment, error code otherwise
 * */
int binfile_addsection_tosegment(binfile_t* bf, binscn_t* scn, binseg_t* seg)
{
   if (!bf)
      return ERR_BINARY_MISSING_BINFILE;
   if (!scn)
      return ERR_BINARY_MISSING_SECTION;

   if (!seg) {
      int out = ERR_BINARY_SECTION_SEGMENT_NOT_FOUND;
      //Segment not provided: we will scan the segments to find those to which the section can belong
      /*A section can belong to a segment if the following conditions are met:
       * - The section offset in the file is above or equal to the segment offset in the file
       * - The end offset of the section in the file is inferior to the end offset of the segment in the file
       * */
      /**\todo TODO (2015-02-17) Check what to do with regard to the TLS sections and segments*/
      uint16_t i;
      //Computes the end offset of the section
      uint64_t scnend = scn->offset
            + ((scn->type != SCNT_ZERODATA) ? scn->size : 0);
      //Scans all segments
      for (i = 0; i < bf->n_segments; i++) {
         binseg_t* sg = bf->segments[i];
         if ((sg->offset <= scn->offset) && (scnend <= binseg_get_end_offset(sg))) {
            //The section in the file is comprised in the segment
            DBGMSG(
                  "Section %s (%d) spanning %#"PRIx64"-%#"PRIx64" belongs to segment %d spanning %#"PRIx64"-%#"PRIx64"\n",
                  binscn_get_name(scn), scn->scnid, scn->offset, scnend, i,
                  sg->offset, binseg_get_end_offset(sg));
            out = EXIT_SUCCESS; //Notes that at least one match was found
            //Associates the section and the segment
            binseg_addsection(sg, scn);
         }
      }
      return out;
   } else {
      //Segment provided: we simply associate the section to it
      DBGMSG(
            "Associating section %s (%d) to segment %d spanning %#"PRIx64"-%#"PRIx64"\n",
            binscn_get_name(scn), scn->scnid, seg->segid, seg->offset,
            binseg_get_end_offset(seg))
      return binseg_addsection(seg, scn);
   }
}

/*
 * Retrieves the pointers referencing a particular object
 * \param bf Structure representing a binary file
 * \param dest Object referenced by the pointer
 * \return Queue of data_t structures containing pointers or relocations referencing this object
 * */
queue_t* binfile_lookup_ptrs_by_target(binfile_t* bf, void* dest)
{
   return (bf) ? hashtable_lookup_all(bf->data_ptrs_by_target_data, dest) : NULL;
}

/*
 * Retrieves the data_t structures containing pointers referencing a particular address
 * \param bf Structure representing a binary file
 * \param dest Object referenced by the pointer
 * \param addr Address of the object
 * \return Queue of data_t structures containing pointers or relocations referencing this object or address
 * \warning This function may cause overhead as it will attempt to find unlinked pointers as well
 * */
queue_t* binfile_lookup_ptrs_by_addr(binfile_t* bf, void* dest, int64_t addr)
{
   if (!bf)
      return NULL;
   queue_t* targets = queue_new();
   //Looking for unlinked pointers referencing this address
   queue_t* unlinked = hashtable_lookup_all(bf->data_ptrs_by_target_data, dest);
   //Now adding unlinked pointers referencing this address
   if (unlinked) {
      FOREACH_INQUEUE(unlinked, iter) {
         if (pointer_get_addr(data_get_ref_ptr(GET_DATA_T(data_t*, iter)))
               == addr)
            queue_add_tail(targets, GET_DATA_T(data_t*, iter));
      }
      queue_free(unlinked, NULL);
   }
   if (dest) {
      //Retrieving the pointers referencing this destination
      queue_t* refs = hashtable_lookup_all(bf->data_ptrs_by_target_data, dest);
      if (refs) {
         queue_append(targets, refs);
      }
   }
   if (queue_length(targets) == 0) {
      queue_free(targets, NULL);
      targets = NULL;
   }
   return targets;
}

/*
 * Retrieves a queue of data_t structures referencing an address with an unknown target, ordered by the referenced address
 * \param bf Structure representing a binary file
 * \return A queue of data_t structure of type DATA_POINTER containing a pointer_t with a NULL target, ordered by the address
 * referenced by the pointer
 * */
queue_t* binfile_lookup_unlinked_ptrs(binfile_t* bf)
{
   if (!bf)
      return NULL;
   queue_t* out = hashtable_lookup_all(bf->data_ptrs_by_target_data, NULL);
   if (out)
      queue_sort(out, data_cmp_by_ptr_addr_qsort);
   return out;
}

/*
 * Sets the structure representing the member of an archive binary file at a given index
 * \param bf Structure representing a binary file
 * \param ar_elt Structure describing the binary member
 * \param eltid Index of the archive member to add. If higher than the current number of archive members, the size of the
 * array will be increased by one.
 * */
void binfile_set_ar_elt(binfile_t* bf, binfile_t* ar_elt, uint16_t eltid)
{
   if (!bf || bf->type != BFT_ARCHIVE)
      return;
   //Adds the archive member to the file
   if (bf->n_ar_elts <= eltid) {
      //Given index larger than the number of entries in the section
      ADD_CELL_TO_ARRAY(bf->ar_elts, bf->n_ar_elts, ar_elt)
   } else {
      bf->ar_elts[eltid] = ar_elt;
   }
   //Linking the archive member to the archive
   binfile_set_archive(ar_elt, bf);
}

/*
 * Sets the number of structures representing the members of an archive binary file. This will resize the array accordingly
 * \param bf Structure representing a binary file
 * \param n_ar_elts Number of archive members
 * */
void binfile_set_nb_ar_elts(binfile_t* bf, uint16_t n_ar_elts)
{
   if (!bf)
      return;
   unsigned int i;

   if (n_ar_elts > bf->n_ar_elts) {
      //New number of archive members is bigger
      bf->ar_elts = lc_realloc(bf->segments, sizeof(*bf->ar_elts) * n_ar_elts);
      //Setting new slots to NULL
      memset(bf->ar_elts + bf->n_ar_elts, 0,
            sizeof(*bf->ar_elts) * (n_ar_elts - bf->n_ar_elts));
   }
   if (n_ar_elts < bf->n_ar_elts) {
      //New number of archive members is smaller
      //Freeing extra number of archive members
      for (i = n_ar_elts; i < bf->n_ar_elts; i++) {
         binfile_free(bf->ar_elts[i]);
      }
      if (n_ar_elts == 0) {
         //Number of archive members set to 0: freeing the array of archive members
         lc_free(bf->ar_elts);
         bf->ar_elts = NULL;
      } else {
         //Resizing the array
         bf->ar_elts = lc_realloc(bf->ar_elts,
               sizeof(*bf->ar_elts) * n_ar_elts);
      }
   }
   //Updating number of archive members
   bf->n_ar_elts = n_ar_elts;
}

/*
 * Retrieves a pointer to the data stored in a binary file depending on its address
 * \param bf A parsed binary file
 * \param address The virtual address at which the researched data is to be looked
 * \return Pointer to the data bytes stored in the binary that are loaded at the given
 * address, or NULL if \c bf is NULL or \c address is outside of the load addresses ranges
 * */
unsigned char* binfile_get_data_at_addr(binfile_t* bf, int64_t address)
{
   //Looks up for a section loaded at this address
   binscn_t* scn = binfile_lookup_scn_span_addr(bf, address);
   if (!scn)
      return NULL; //binfile_lookup_scn_span_addr returns NULL if bf is NULL or address is out of range
   return (scn->data + (address - scn->address));
}

/*
 * Sets the creator of a copied binary file
 * \param bf Structure representing a binary file
 * \param creator Pointer to the binfile_t structure representing the binary file from which this is a copy
 * */
void binfile_set_creator(binfile_t* bf, binfile_t* creator)
{
   if (bf)
      bf->creator = creator;
}

/*
 * Sets the archive from which this binary file is a member
 * \param bf Structure representing a binary file
 * \param archive Pointer to the binary file structure containing this file
 * */
void binfile_set_archive(binfile_t* bf, binfile_t* archive)
{
   if (bf)
      bf->archive = archive;
}

/*
 * Sets the asm file based on this binary file
 * \param bf Structure representing a binary file
 * \param A pointer to the asmfile_t structure representing the file contained in this binary file
 * */
void binfile_set_asmfile(binfile_t* bf, asmfile_t* asmfile)
{
   if (bf)
      bf->asmfile = asmfile;
}

/*
 * Removes a data entry of type DATA_PTR with an unlinked pointer from the table of entries
 * \param bf Structure representing a binary file
 * \param unlinked The data entry containing an unlinked pointer. Will not be removed if the pointer is linked to a target of type data
 * */
void binfile_remove_unlinked_target(binfile_t* bf, data_t* unlinked)
{
   if (!bf || !unlinked)
      return;
   /**\todo (2104-12-05) Those sanity checks can be removed if they cause too much of an overhead*/
   pointer_t* ptr = data_get_ref_ptr(unlinked);
   if (pointer_get_data_target(ptr))
      return;
   hashtable_remove_elt(bf->data_ptrs_by_target_data, NULL, unlinked);
}

/*
 * Returns the type of a parsed binary file
 * \param bf Structure representing a binary file
 * \return Identifier of the type, or BFT_UNKNOWN if \c bf is NULL
 * */
unsigned int binfile_get_type(binfile_t* bf)
{
   return (bf) ? bf->type : BFT_UNKNOWN;
}

/*
 * Prints a formatted representation of the code areas. A code area contains a number of consecutive
 * sections containing executable code
 * \param bf Structure representing a binary file
 * */
void binfile_print_code_areas(binfile_t* bf)
{
   /**\todo TODO (2014-11-19) I moved thids from elfprinter.c as it seems to be format-independent. If this is confirmed, remove the use of getters
    * to access the properties of the file and sections*/
   if (!bf)
      return;
   int i = 0;
   if (bf->n_codescns == 0) {
      printf("File %s does not contain sections containing executable code\n",
            bf->filename);
      return;
   }

   //Prints the header
   printf("\nCode areas------------------------------------------------\n");
   printf("Address       Offset     Size       End address   End offset \n");
   printf("-------------------------------------------------------------\n");

   //Scans the list of sections containing executable code
   while (i < bf->n_codescns) {
      int64_t saddr, soff, sz, eaddr, eoff;
      binscn_t* sscn = bf->codescns[i];
      assert(sscn);
      saddr = sscn->address;
      soff = sscn->offset;
      //Retrieves all consecutive sections
      while (i < (bf->n_codescns - 1)
            && bf->codescns[i]->scnid == (bf->codescns[i + 1]->scnid - 1))
         i++;
      //Now i is either the last code section or the last section in the code area
      binscn_t* escn = bf->codescns[i];
      if (escn->scnid < (bf->n_sections - 1)) { //Last code section is not the last ELF section
         binscn_t* nextscn = binfile_get_scn(bf, escn->scnid + 1);
         sz = nextscn->offset - sscn->offset;
         eoff = nextscn->offset;

         if (nextscn->address > escn->address) //Next section is loaded in memory
            eaddr = nextscn->address;
         else
            //Next section is not loaded in memory or at a lower address than the last code section
            eaddr = binscn_get_end_addr(escn);
      } else { //Last code section is the last section in the file
         sz = binscn_get_end_offset(escn) - binscn_get_offset(sscn);
         eaddr = binscn_get_end_addr(escn);
         eoff = binscn_get_end_offset(escn);
      }
      //      Address      Offset       Size         End address  End offset
      printf(
            "%#-14"PRIx64"%#-11"PRIx64"%#-11"PRIx64"%#-14"PRIx64"%#-8"PRIx64"\n",
            saddr, soff, sz, eaddr, eoff);
      i++;
   }
}

/*
 * Sets the name of a binary section
 * \param scn Structure representing a binary section
 * \param name The name of the section. It is assumed it is allocated elsewhere and will not be freed when freeing the section
 * */
void binscn_set_name(binscn_t* scn, char* name)
{
   if (scn)
      scn->name = name;
}

/*
 * Sets the data bytes of a binary section
 * \param scn Structure representing a binary section
 * \param data The data bytes contained in the section.
 * \param local Set to TRUE if the data must be freed along with the section,
 * and FALSE if it is allocated elsewhere and will not be freed when freeing the section
 * */
void binscn_set_data(binscn_t* scn, unsigned char* data, unsigned local)
{
   /**\todo TODO (2015-04-14) I'm keeping the \c local parameter because libmworm uses it, although I don't quite see why.
    * If we can remove it, this function would always assume that the data is allocated elsewhere, and that only sections
    * in patched files use data allocated in the section (in which case we would use binscn_patch_set_data)*/
   if (scn) {
      scn->data = data;
      if (local)
         scn->attrs |= SCNA_LOCALDATA;
   }
}

/*
 * Sets the first instruction in the section
 * \param scn Structure representing a binary section
 * \param insnseq List node containing the first instruction of the section
 * */
void binscn_set_first_insn_seq(binscn_t* scn, list_t* insnseq)
{
   if (scn)
      scn->firstinsnseq = insnseq;
}

/*
 * Sets the last instruction in the section
 * \param scn Structure representing a binary section
 * \param insnseq List node containing the last instruction of the section
 * */
void binscn_set_last_insn_seq(binscn_t* scn, list_t* insnseq)
{
   if (scn)
      scn->lastinsnseq = insnseq;
}

/*
 * Sets the size of a binary section
 * \param scn Structure representing a binary section
 * \param size The size in bytes of the section
 * */
void binscn_set_size(binscn_t* scn, uint64_t size)
{
   if (scn)
      scn->size = size;
}

/*
 * Sets the address of a binary section
 * \param scn Structure representing a binary section
 * \param address The virtual address of the section.
 * */
void binscn_set_addr(binscn_t* scn, int64_t address)
{
   if (scn)
      scn->address = address;
}

/*
 * Sets the alignement of a section in a binary file
 * \param scn The section
 * \param align The alignment of the section
 * */
void binscn_set_align(binscn_t* scn, uint64_t align)
{
   if (scn)
      scn->align = align;
}

/*
 * Sets the offset of a binary section
 * \param scn Structure representing a binary section
 * \param offset The offset of the section.
 * */
void binscn_set_offset(binscn_t* scn, uint64_t offset)
{
   if (scn)
      scn->offset = offset;
}

/*
 * Sets the number of sections in a binary file and initialises the associated arrays.
 * If the array is already initialised, it will be resized.
 * \param bf Structure representing a binary file
 * \param nscns Number of sections
 * */
void binfile_set_nb_scns(binfile_t* bf, uint16_t nscns)
{
   if (!bf)
      return;
   unsigned int i;

   if (nscns > 0 && binfile_patch_is_patching(bf) == FALSE) {
      //Resizing the arrays of labels by sections (no need to update them for files being patched)
      bf->lbls_by_scn = lc_realloc(bf->lbls_by_scn,
            sizeof(*bf->lbls_by_scn) * nscns); //Initialises the arrays of labels by section
      bf->n_lbls_by_scn = lc_realloc(bf->n_lbls_by_scn,
            sizeof(*bf->n_lbls_by_scn) * nscns); //Initialises the number of labels by sections
   }

   if (nscns > bf->n_sections) {
      //New number of sections is bigger
      bf->sections = lc_realloc(bf->sections, sizeof(*bf->sections) * nscns);
      //Setting new slots to NULL
      memset(bf->sections + bf->n_sections, 0,
            sizeof(*bf->sections) * (nscns - bf->n_sections));

      if (binfile_patch_is_patching(bf) == FALSE) { //No need to update the arrays of labels by sections for files being patched
         //Setting new slots in arrays of labels by sections to NULL
         memset(bf->lbls_by_scn + bf->n_sections, 0,
               sizeof(*bf->lbls_by_scn) * (nscns - bf->n_sections));
         memset(bf->n_lbls_by_scn + bf->n_sections, 0,
               sizeof(*bf->n_lbls_by_scn) * (nscns - bf->n_sections));
      }
   }
   if (nscns < bf->n_sections) {
      //New number of sections is smaller
      //Freeing extra number of sections
      for (i = nscns; i < bf->n_sections; i++) {
         binscn_free(bf->sections[i]);
         //Freeing array of labels by section
         if (bf->lbls_by_scn && bf->lbls_by_scn[i])
            lc_free(bf->lbls_by_scn[i]);
      }
      if (nscns == 0) {
         //Number of sections set to 0: freeing the array of sections
         lc_free(bf->sections);
         bf->sections = NULL;
         //Freeing the arrays of labels by sections
         if (bf->lbls_by_scn)
            lc_free(bf->lbls_by_scn);
         if (bf->n_lbls_by_scn)
            lc_free(bf->n_lbls_by_scn);
         bf->lbls_by_scn = NULL;
         bf->n_lbls_by_scn = NULL;
      } else {
         //Resizing the array
         bf->sections = lc_realloc(bf->sections, sizeof(*bf->sections) * nscns);
      }
   }
   //Updating number of sections
   bf->n_sections = nscns;
}

/*
 * Sets the number of segments in a binary file and initialises the associated arrays.
 * If the array is already initialised, it will be resized.
 * \param bf Structure representing a binary file
 * \param nsegs Number of segments
 * */
void binfile_set_nb_segs(binfile_t* bf, uint16_t nsegs)
{
   if (!bf)
      return;
   unsigned int i;

   if (nsegs > bf->n_segments) {
      //New number of segments is bigger
      bf->segments = lc_realloc(bf->segments, sizeof(*bf->segments) * nsegs);
      //Setting new slots to NULL
      memset(bf->segments + bf->n_segments, 0,
            sizeof(*bf->segments) * (nsegs - bf->n_segments));
   }
   if (nsegs < bf->n_segments) {
      //New number of segments is smaller
      //Freeing extra number of segments
      for (i = nsegs; i < bf->n_segments; i++) {
         binseg_free(bf->segments[i]);
      }
      if (nsegs == 0) {
         //Number of segments set to 0: freeing the array of segments
         lc_free(bf->segments);
         bf->segments = NULL;
      } else {
         //Resizing the array
         bf->segments = lc_realloc(bf->segments, sizeof(*bf->segments) * nsegs);
      }
   }
   //Updating number of segments
   bf->n_segments = nsegs;
}

/*
 * Adds an external library to the array in a binary file
 * \param bf Structure representing a binary file
 * \param extlib Data structure containing a pointer to the name of the external library to add
 * */
void binfile_addextlib(binfile_t* bf, data_t* extlib)
{
   if (!bf)
      return;
   //Increases size of array containing external library names
   bf->extlibs = lc_realloc(bf->extlibs,
         sizeof(*bf->extlibs) * (bf->n_extlibs + 1));
   //Stores name in the latest cell
   bf->extlibs[bf->n_extlibs] = extlib;
   //Increases number of external libraries
   bf->n_extlibs = bf->n_extlibs + 1;
}

/*
 * Retrieves the data object at a given offset inside a binary section
 * \param bs Structure representing a binary section
 * \param off Offset from the beginning of the section of the data to find
 * \param diff Return parameter. If not NULL, will contain the difference between the beginning of the returned entry and the actual offset
 * \return Data object beginning at or containing the given offset, or NULL if no object at this offset was found or if \c bs is NULL
 * */
data_t* binscn_lookup_entry_by_offset(binscn_t* bs, uint64_t off, uint64_t* diff)
{
   /**\todo (2014-11-20) I'm pretty sure there is an easier way to write this function. Find it someday*/
   data_t* out = NULL;
   uint64_t len = 0;
   uint64_t diffoff = 0;
   uint32_t i = 0;
   if (!bs || (off > bs->size))
      return out; // No section or offset out of range
   DBGMSGLVL(2,
         "Looking for entry at offset %#"PRIx64" in section %s [%d] of size %#"PRIx64" containing %d entries\n",
         off, bs->name, bs->scnid, bs->size, bs->n_entries);
   //Scans the list of entries
   while ((i < bs->n_entries) && (len < off)) {
      //Updates the offset in the section with the size of the entry
      len += data_get_size(bs->entries[i]);
      i++;
   }
   if (len == off) {
      //Required offset reached: the entry is pointed by i
      out = bs->entries[i];
      diffoff = 0;
   } else if (len > off) {
      //The entry pointed by i-1 spans the given offset
      assert(i > 0); //Just checking
      out = bs->entries[i - 1];
      diffoff = off - (len - data_get_size(bs->entries[i - 1]));
   }
   DBGMSGLVL(2,
         "Reached length %#"PRIx64" at entry %u. Entry is %u with diff %#"PRIx64"\n",
         len, i, (len == off) ? i : i - 1, diffoff);
   if (diff)
      *diff = diffoff;
   return out;
}

/*
 * Retrieves the data object at a given address inside a binary section
 * \param scn Structure representing a binary section
 * \param addr Absolute address of the data to find
 * \return Data object beginning at the given offset,
 * or NULL if no object at this offset was found or if \c bs is NULL or does not contain an array of entries
 * */
data_t* binscn_lookupentry_ataddress(binscn_t* scn, int64_t addr)
{
   data_t* out = NULL;

   if (!scn || scn->n_entries == 0) /**\note (2014-12-03) No check on the address because of the labels with address below section...*/
      return out; //Section does not contain entries

   //Looks up into the entries to find the one at the given address
   data_t** ent = bsearch(&addr, scn->entries, scn->n_entries,
         sizeof(*scn->entries), data_cmp_by_addr_bsearch);
   if (ent)
      out = *ent;

   return out;
}

/*
 * Looks up a label at a given address
 * \param bf Structure representing a parsed binary file
 * \param scn Structure representing the binary section where to find the label. If NULL, will be deduced from the address
 * \param addr The address where to look up for a label
 * \return The label if one was found at this address and in this section (if not NULL), and NULL otherwise
 * */
label_t* binfile_lookup_label_at_addr(binfile_t* bf, binscn_t* scn,
      int64_t addr)
{
   if (!bf)
      return NULL;
   if (scn
         && ((scn->scnid >= bf->n_sections) || (scn->binfile != bf)
               || (addr < scn->address)
               || (addr > (scn->address + (int64_t) scn->size))))
      return NULL; //Section does not belong to the file, has a wrong identifier or address is out of the section range
   label_t** labels;
   uint32_t n_labels;
   if (scn) {
      //Section provided: we look only for labels associated to this section
      labels = bf->lbls_by_scn[scn->scnid];
      n_labels = bf->n_lbls_by_scn[scn->scnid];
   } else {
      //The lookup will be performed against all label
      labels = bf->labels;
      n_labels = bf->n_labels;
   }

   label_t** label = bsearch(&addr, labels, n_labels, sizeof(*labels),
         label_cmpaddr_forbsearch);

   return label ? *label : NULL;
}

/*
 * Looks up into a section to find a data object at a given address or spanning over it
 * \param scn Structure representing a binary section. Assumed to be not NULL
 * \param addr Absolute address of the data to find. Assumed to be contained by \c scn
 * \param off Return parameter. If not NULL, will contain the offset from the beginning of the entry to the address
 * \return Data object beginning at or containing the given offset,
 * or NULL if no object at this offset was found or if \c bs is NULL or does not contain an array of entries
 * */
static data_t* binscn_lookupentry_byaddress(binscn_t* scn, int64_t addr,
      uint64_t *off)
{
   assert(scn);/**\note (2014-12-03) No check on the address because of the labels with address below section...*/

   if (scn->n_entries == 0)
      return NULL; //No entries in the section: no need to look up for one

   //Now looks up into the entries to find the one at the given address
   data_t* entry = binscn_lookupentry_ataddress(scn, addr);
   if (entry) {
      //Found a matching entry
      if (off)
         *off = 0; //Setting offset to 0 as the address is the same as the one of the entry
      return entry;
   }
   //Otherwise the entry may span over the searched address. We have to scan the array of entries manually
   uint32_t i = 0;
   while ((i < scn->n_entries) && (data_get_addr(scn->entries[i]) < addr))
      i++;
   if ((i > 0 && i < scn->n_entries) /*We found an entry before the end with an address superior or equal to the given address*/
         || ((i == scn->n_entries)
               && (data_get_addr(scn->entries[i - 1]) <= addr)
               && (addr
                     < (data_get_addr(scn->entries[i - 1])
                           + (int64_t) data_get_size(scn->entries[i - 1])))) /*Address is encompassed by the last entry*/) {
      /**\todo TODO (2014-11-21) The test on i>0 should not be needed, as if we stopped on the first element it means its address is equal to the searched address
       * and we should have found it with bsearch. However I'm now battling with bloody ELF loading, and because of recursive references between sections
       * I'm occasionally looking into arrays with NULL entries, which causes bsearch to fail randomly. When/if I get something more foolproof later on, we
       * could remove the test on i > 0 to gain some time.*/
//      assert(i > 0); //We must have gone beyond the first entry, otherwise it means its address is equal and bsearch would have found it
      entry = scn->entries[i - 1]; //It's the previous entry
      if (off)
         *off = addr - data_get_addr(scn->entries[i - 1]);
      return entry;
   }
   return NULL;
}

/*
 * Retrieves a section spanning over a given address. The section is assumed to be loaded
 * \param bf Structure representing a parsed binary file
 * \param addr Address to search for
 * \return Pointer to the section spanning over this address, or NULL if it is not found
 * */
binscn_t* binfile_lookup_scn_span_addr(binfile_t* bf, int64_t addr)
{
   if (!bf)
      return NULL;
   binscn_t* out = NULL;
   uint16_t i = 0;
   while (i < bf->n_loadscns) {
      if ((bf->loadscns[i]->address <= addr)
            && (addr
                  < (int64_t) (bf->loadscns[i]->address + bf->loadscns[i]->size))) {
         out = bf->loadscns[i];
         break;
      }
      i++;
   }
   return out;
}

/*
 * Retrieves a section with a given name
 * \param bf Structure representing a parsed binary file
 * \param name Name of the section to search for
 * \param Pointer to the first section found with that name, or NULL if no such section was found
 * */
binscn_t* binfile_lookup_scn_by_name(binfile_t* bf, char* name)
{
   /**\todo TODO (2015-05-05) I'm adding this because a function in libmadras (now used only by test files)
    * needs it, but in the future, either it could be removed, or if invoked frequently an hashtable linking
    * the sections to their names could be added (then again, there should never be a lot of sections in a file)*/
   if (!bf || !name)
      return NULL;
   uint16_t i;
   for (i = 0; i < bf->n_sections; i++)
      if (str_equal(bf->sections[i]->name, name))
         return bf->sections[i];
   return NULL;
}

/*
 * Looks up into a file to find a data object at a given address or spanning over it
 * \param bf Structure representing a parsed binary file
 * \param scnid Index of the section containing the data object to look for, or BF_SCNID_ERROR if not known
 * \param addr Absolute address of the data to find.
 * \param off Return parameter. If not NULL, will contain the offset from the beginning of the entry to the address
 * \return Data object beginning at or containing the given offset,
 * or NULL if no object at this offset was found
 * */
static data_t* binfile_lookupentry_byaddress(binfile_t* bf, uint32_t scnid,
      int64_t addr, uint64_t *off)
{
   binscn_t* scn = NULL;
   data_t* entry = NULL;

   if (scnid < bf->n_sections) {
      scn = binfile_get_scn(bf, scnid);
      if (!scn)
         return NULL; /**\todo TODO (2014-11-21) See the comment in binscn_lookupentry_byaddress. Remove this if I find a more stable way to handle ELF files during their creation*/
      assert(
            scn && (addr >= scn->address)
                  && (addr < (scn->address + (int64_t )scn->size)));
   } else
      //Attempts to find the target of the reference
      scn = binfile_lookup_scn_span_addr(bf, addr);
   if (scn)
      entry = binscn_lookupentry_byaddress(scn, addr, off);
   return entry;
}

/*
 * Looks up a segment in a file from its start and stop address
 * \param bf Structure representing a parsed binary file. Must have been fully loaded and finalised
 * \param start First address from which to look up the segment
 * \param stop Last address up to which to look up the segment
 * \return The first segment encountered whose starting address is equal to or above \c start and
 * ending address is equal to or below \c stop, or NULL if \c bf is NULL or if no such segment can be found
 * */
binseg_t* binfile_lookup_seg_in_interval(binfile_t* bf, maddr_t start,
      maddr_t stop)
{
   /**\todo TODO (2015-04-01) We may need to detect if more than one segment is present in the interval and find
    * some way to return them. Conversely, we may not need the segment itself, only that at least one can be found
    * In that case, we would return a boolean*/
   if (!bf)
      return NULL;
   uint16_t i = 0;
   while (i < bf->n_segments) {
      binseg_t* seg = bf->segments[i];
      if ((seg->address >= start) && (binseg_get_end_addr(seg) <= stop))
         return seg; //Found it
      if (seg->address > stop)
         break;   //We overshot the end address: no need to keep looking
      /**\todo TODO (2015-04-01) WARNING! If the segments are not ordered this won't work. Check when they are and remove the
       * previous test if this function can be invoked when they are not. So far I only envision it during patching.*/
      i++;
   }
   return NULL;
}

/*
 * Retrieves a queue of external labels (type LBL_EXTERNAL, indicating a label defined in another file)
 * \param bf Structure representing a parsed binary file
 * \return Queue of labels of type LBL_EXTERNAL or NULL if \c bf is NULL, of an unsupported type, or without labels
 * */
queue_t* binfile_find_ext_labels(binfile_t* bf)
{
   /**\todo (2014-11-14) Check if this works, or if we add a test on whether we are dealing with a relocatable file*/
   if (!bf || bf->format != BFF_UNKNOWN || bf->n_labels == 0)
      return NULL;
   queue_t* out = queue_new();
   uint32_t i;
   for (i = 0; i < bf->n_labels; i++)
      if (label_get_type(bf->labels[i]) == LBL_EXTERNAL)
         queue_add_tail(out, bf->labels[i]);
   return out;
}

/*
 * Adds an internal reference in a binary file (pointer to an object inside the file).
 * If the destination can't be found it will be added to the table of unlinked targets
 * The size of the entry representing the reference will be set to the size of entries in the section (\c entrysz).
 * If it is not fixed for this section, it's up to the caller to update it afterwards. => Ok, remove this if we keep \c size as a parameter
 * \param bf Structure representing a binary file
 * \param scnid Index of the section to which the reference is to be added (must be of type SCNT_REF)
 * \param entryid The index where to add the data entry representing the reference in the section. If higher than the current number of
 * entries in the section, it will be inserted after the last one.
 * \param addr Address pointed by the reference
 * \param size Size of the entry representing the reference (needed only if the size of entries in the section is not fixed)
 * \param refscnid Index of the section containing the reference (if known)
 * \param dstscn Section pointed by the reference. If not NULL, will be used as the target of the reference
 * \return A Pointer to the entry representing the reference
 * */
data_t* binfile_add_ref(binfile_t* bf, uint16_t scnid, uint32_t entryid,
      int64_t addr, uint64_t size, uint16_t refscnid, binscn_t* dstscn)
{
   if (!bf)
      return NULL; //NULL binary file
   binscn_t* scn = binfile_get_scn(bf, scnid);
   if (!scn || (scn->type != SCNT_REFS)) {
      //NULL section or section index out of range or wrong section type
      if (!scn)
         bf->last_error_code = ERR_BINARY_SECTION_NOT_FOUND;
      else if (scn->type != SCNT_REFS)
         bf->last_error_code = ERR_BINARY_BAD_SECTION_TYPE;
      return NULL;
   }
   data_t* entry = NULL; //Data entry representing the reference
   uint64_t entrysz = (scn->entrysz > 0) ? scn->entrysz : size;

   if (dstscn) { //The reference targets a section
      DBGMSGLVL(1,
            "Section %s (%d): creating reference %d to section %s (%d)\n",
            scn->name, scnid, entryid, dstscn->name, dstscn->scnid);
      //Creates the data entry representing the pointer
      entry = data_new_ptr(entrysz, addr, 0, dstscn, POINTER_ABSOLUTE,
            TARGET_BSCN);
      //Adds the entry to the table of pointers to a section
      hashtable_insert(bf->data_ptrs_by_target_scn, dstscn, entry);
   } else {
      uint64_t off = 0;
      /**\todo TODO (2014-07-15) This may cause an assert error in pointer_set_offset_in_target if we end up with a too large offset,
       * but I want to use binfile_lookupentry_byaddress also for looking up entries representing data and such offsets may
       * appear then. We will have to see as time goes if pointer_t could legitimately use bigger offsets, or if we simply
       * find another mechanism here*/
      data_t* dest = binfile_lookupentry_byaddress(bf, refscnid, addr, &off);
      if (dest) {
         //Target entry found
         DBGMSGLVL(1,
               "Section %s (%d): creating reference %d to data entry at address %#"PRIx64" in section %s\n",
               scn->name, scnid, entryid, addr,
               binscn_get_name(binfile_get_scn(bf, refscnid)));
         //Creating the entry
         entry = data_new_ptr(entrysz, addr, 0, dest, POINTER_ABSOLUTE,
               TARGET_DATA);
         if (off > 0) //Updating offset if necessary
            pointer_set_offset_in_target(data_get_pointer(entry), off);
      } else {
         // Target not found
         entry = data_new_ptr(entrysz, addr, 0, dest, POINTER_ABSOLUTE,
               TARGET_UNDEF);
      }
      //Adds the entry to the table of pointers
      hashtable_insert(bf->data_ptrs_by_target_data, dest, entry);
   }
   //Adds the entry to the array of entries in the section
   binscn_add_entry_s(scn, entry, entryid);
   return entry;
}

/*
 * Adds an internal reference in a binary file (pointer to an object inside the file) based on an offset inside a section
 * If the destination can't be found it will be added to the table of unlinked targets
 * The size of the entry representing the reference will be set to the size of entries in the section (\c entrysz).
 * If it is not fixed for this section, it's up to the caller to update it afterwards. => Ok, remove this if we keep \c size as a parameter
 * \param bf Structure representing a binary file
 * \param scnid Index of the section to which the reference is to be added (must be of type SCNT_REF)
 * \param entryid The index where to add the data entry representing the reference in the section. If higher than the current number of
 * entries in the section, it will be inserted after the last one.
 * \param refscnid Index of the section containing the reference. Must be set
 * \param offset Offset pointed by the reference inside the section \c refscnid
 * \param size Size of the entry representing the reference (needed only if the size of entries in the section is not fixed)
 * \return A Pointer to the entry representing the reference or NULL if the file is invalid or no entry could be found at this offset
 * */
data_t* binfile_add_ref_byoffset(binfile_t* bf, uint16_t scnid, uint32_t entryid,
      uint16_t refscnid, uint64_t offset, uint64_t size)
{
   if (!bf || (scnid >= bf->n_sections) || (refscnid >= bf->n_sections))
      return NULL; //NULL binary file
   binscn_t* scn = binfile_get_scn(bf, scnid);
   binscn_t* refscn = binfile_get_scn(bf, refscnid);
   if (!scn || !refscn || (scn->type != SCNT_REFS)) {
      //NULL section or section index out of range or wrong section type
      if (!scn || !refscn)
         bf->last_error_code = ERR_BINARY_SECTION_NOT_FOUND;
      else if (scn && scn->type != SCNT_REFS)
         bf->last_error_code = ERR_BINARY_BAD_SECTION_TYPE;
      return NULL;
   }
   data_t* entry = NULL; //Data entry representing the reference
   uint64_t entrysz = (scn->entrysz > 0) ? scn->entrysz : size;

   uint64_t off = 0;
   /*Finds the data object representing this string in the string table*/\
data_t* dest =
         binscn_lookup_entry_by_offset(refscn, offset, &off);
   if (dest) {
      //Target entry found
      DBGMSGLVL(1,
            "Section %s (%d): creating reference %d to data entry at offset %#"PRIx64" in section %s\n",
            scn->name, scnid, entryid, offset, binscn_get_name(refscn));
      //Creating the entry
      entry = data_new_ptr(entrysz, 0, 0, dest, POINTER_NOADDRESS, TARGET_DATA);
      /*Updating the pointer with the offset if the entry does not match the exact offset*/
      if (off > 0)
         pointer_set_offset_in_target(data_get_pointer(entry), off);
      //Adds the entry to the table of pointers
      hashtable_insert(bf->data_ptrs_by_target_data, dest, entry);
      //Adds the entry to the array of entries in the section
      binscn_add_entry_s(scn, entry, entryid);
   }
   return entry;
}

/*
 * Adds a relocation to a binary file
 * If the destination of the relocation can't be found it will be added to the table of unlinked targets.
 * The size of the entry representing the relocation will be set to the size of entries in the section (\c entrysz).
 * If it is not fixed for this section, it's up to the caller to update it afterwards. => Ok, remove this if we keep \c size as a parameter
 * \param bf Structure representing a binary file
 * \param scnid Index of the section containing the relocation (must be of type SCNT_RELOC)
 * \param entryid The index where to add the data entry representing the label in the section. If higher than the current number of
 * entries in the section, it will be inserted after the last one.
 * \param label Label associated to the relocation
 * \param addr Address pointed by the relocation. If set to ADDRESS_ERROR, \c offset will be used instead
 * \param offset Offset in relscnid pointed by the relocation. If set to UINT64_MAX while \c addr is set to ADDRESS_ERROR, an error will be returned
 * \param size Size of the entry representing the relocation (needed only if the size of entries in the section is not fixed)
 * \param relscnid Index of the section pointed by the relocation. If higher than the current number of sections, the section will be searched based on \c addr
 * \param reltype Format specific type of the relocation
 * \return A Pointer to the entry representing the relocation
 * */
data_t* binfile_addreloc(binfile_t* bf, uint16_t scnid, uint32_t entryid,
      label_t* label, uint64_t size, int64_t addr, uint64_t offset,
      uint16_t relscnid, uint32_t reltype)
{
   if (!bf)
      return NULL; //NULL binary file
   binscn_t* scn = binfile_get_scn(bf, scnid);
   if (!scn || (scn->type != SCNT_RELOC)) {
      //NULL section or section index out of range or wrong section type
      if (!scn)
         bf->last_error_code = ERR_BINARY_SECTION_NOT_FOUND;
      else if (scn->type != SCNT_RELOC)
         bf->last_error_code = ERR_BINARY_BAD_SECTION_TYPE;
      return NULL;
   }
   if (!label) {/**\todo TODO (2014-07-04) This test may not be necessary*/
      bf->last_error_code = ERR_LIBASM_LABEL_MISSING;
      return NULL; //NULL label
   }
   data_t* entry = NULL; //Data entry representing the relocation
   uint64_t entrysz = (scn->entrysz > 0) ? scn->entrysz : size;

   uint64_t off = 0;
   binrel_t* rel = NULL;
   //Retrieving the object targeted by the relocation
   data_t* dest = NULL;
   if (addr != ADDRESS_ERROR) {
      //Address provided: looking up the target based on the address
      dest = binfile_lookupentry_byaddress(bf, relscnid, addr, &off);
   } else if ((offset != UINT64_MAX) && (relscnid < bf->n_sections)) {
      //Offset provided: looking up the target in the targeted section based on the offset
      dest = binscn_lookup_entry_by_offset(bf->sections[relscnid], offset, &off);
   } else {
      ERRMSG(
            "[INTERNAL] No valid address or offset for relocation %d in section %s (%d): relocation not created\n",
            entryid, binscn_get_name(scn), scn->scnid);
      bf->last_error_code = ERR_BINARY_BAD_RELOCATION_ADDRESS;
      return NULL;
   }
   /**\todo TODO (2015-06-10) Check that the behaviour of relocations (either an absolute address or an offset from the beginning of the section)
    * is not too ELF-specific and adjust accordingly if necessary*/
   DBGMSGLVL(1,
         "Section %s (%d): creating relocation between label %s and entry at address %#"PRIx64"\n",
         scn->name, scnid, label_get_name(label), addr);
   //Creates a new data entry containing a new relocation object
   if (dest) {
      //Destination found
      rel = binrel_new(label, addr, 0, dest, POINTER_ABSOLUTE, TARGET_DATA,
            reltype);
      entry = data_new(DATA_REL, rel, entrysz);
   } else {
      //Destination not found
      rel = binrel_new(label, addr, 0, dest, POINTER_ABSOLUTE, TARGET_UNDEF,
            reltype);
      entry = data_new(DATA_REL, rel, entrysz);
   }
   //Adds the entry to the table of pointers
   hashtable_insert(bf->data_ptrs_by_target_data, dest, entry);
   //Adds a new entry to the relocation table
   ADD_CELL_TO_ARRAY(bf->relocs, bf->n_relocs, rel)
   /**\todo TODO (2014-10-21) WE MAY NEED TO ADD THIS LABEL TO ANOTHER HASHTABLE. It will contain the entries targeted by a label.
    * This is used by the patch to duplicated labels when needed. Alternatively we would need to be clever when changing the address
    * of an object and duplicate the corresponding label.*/
   //Adds the entry to the array of entries in the section
   binscn_add_entry_s(scn, entry, entryid);

   return entry;
}

/*
 * Adds a data entry to a binary file at a given address and returns the result.
 * If an entry already exists at this address, it is returned instead.
 * The data is only created if the address is found to be in a section containing data.
 * This function is intended to allow the creation of data objects corresponding to a target
 * from an instruction.
 * \param bf Structure representing a binary file
 * \param add Address at which the data is located.
 * \param off Return parameter. If present, will be updated to contain the offset between the address of the returned
 * data and the actual required address. This is only the case when the data entry is not created (section not of type SCNT_DATA)
 * \param label Optional parameter. If not NULL, will be used to retrieve the address and section of the data to create
 * \return A data object found at the given address, or NULL if \c bf is NULL, \c addr is outside the range
 * of addresses existing in the file, or if its corresponds to a section not containing data nor an existing entry
 * at this address.
 * */
data_t* binfile_adddata(binfile_t* bf, int64_t addr, uint64_t *off,
      label_t* label)
{
   /**\todo (2014-07-15) See the remark in binfile_add_ref about offset size. In essence, we'll have to wait and see
    * whether we actually need those offsets on 64 bits in pointer_t or if we can keep them at uint8_t*/
   /**\todo (2014-07-16) Also look up the closest label and associate it to the entry*/
   if (!bf)
      return NULL;

   data_t* out = NULL;

   binscn_t* scn = NULL;
   if (label && label_get_scn(label)) {
      addr = label_get_addr(label);
      scn = label_get_scn(label);
   } else {
      scn = binfile_lookup_scn_span_addr(bf, addr);
   }
   if (!scn) {
      DBGMSG("Address %#"PRIx64" is not inside any loaded section\n", addr);
      /**\todo TODO (2014-11-28) I discovered this weird case (in example/madras/daxpy, check for labels at address 0x600e24) where a label
       * is marked as associated to a section while having an address below the starting address of this section (don't ask), and, funnily
       * enough, I handle this case during parsing when identifying the labels to associate with data entries. So we could here look for a
       * label with this address and if we find one attempt to use the section to which it is associated (if it is associated to a section...)
       * Trouble is, so far, I don't have an easy way to look for labels by address (with an efficient way like bsearch of course).*/
      return NULL; //No section found at this address
   }
   //First checking if there is an entry at the given address
   uint64_t offset = 0;
   //Checking whether the section already contains an entry at this address
   out = binscn_lookupentry_byaddress(scn, addr, &offset);
   if (out
         && /*We definitely found an entry*/
         (!offset
               || /*.. and it is at the required address*/
               ((scn->type != SCNT_DATA) && (scn->type != SCNT_ZERODATA)
                     && (scn->type != SCNT_CODE)) /*... or the section is not of type code or data*/
         )) {
      DBGMSGLVL(1,
            "Section %s (%d): found data at address %#"PRIx64" with offset %#"PRIx64"\n",
            scn->name, scn->scnid, addr, offset);
      //Either we found an entry at exactly the required address, or we are not in a section of type data: we return the found entry
      if (off)
         *off = offset;
   } else {
      //Section of type data or code: we will look up the entry and create it if necessary
      //Retrieving a pointer to the data bytes that will be associated to the entry
      unsigned char* entrydata = binscn_get_data_at_offset(scn,
            (uint64_t) (addr - scn->address));
      if (scn->n_entries == 0) {

         DBGMSGLVL(1,
               "Section %s (%d): Initialising data with single entry of size %#"PRIx64"\n",
               scn->name, scn->scnid,
               scn->address + scn->size - (uint64_t )addr);
         //No entry yet: we create one at this address and taking all available space until the end of the section
         out = data_new(DATA_RAW, entrydata,
               scn->address + scn->size - (uint64_t) addr);

         binscn_add_entry_s(scn, out, 0); //Adding the entry to the section

      } else if (addr < data_get_addr(scn->entries[0])) { //The address is before the first data entry in this section

         DBGMSGLVL(1,
               "Section %s (%d): Adding data entry before first entry with size %#"PRIx64"\n",
               scn->name, scn->scnid,
               data_get_addr(scn->entries[0]) - (uint64_t )addr);
         //Creating a data object taking all available space before the first entry
         out = data_new(DATA_RAW, entrydata,
               data_get_addr(scn->entries[0]) - (uint64_t) addr);

         //Increasing the size of the array of entries in the section
         scn->entries = lc_realloc(scn->entries,
               sizeof(*scn->entries) * (scn->n_entries + 1));
         //Moving the entries up one slot
         memmove(scn->entries + 1, scn->entries,
               sizeof(*scn->entries) * (scn->n_entries));
         scn->n_entries = scn->n_entries + 1;

         //Adding the entry to the section
         binscn_add_entry_s(scn, out, 0);

      } else { //General case: we will find an entry overlapping this address and split it
         //First looking for an existing entry at this address
         out = binscn_lookupentry_ataddress(scn, addr);
         /**\note Doing this only now because it could return NULL for the reasons above, and we would
          * have then to do additional tests to know which ones
          * \todo TODO (2014-12-03 and before) We may however be able to merge the cases above with those below, but since I'm making this
          * up as I go along, I don't know if that will be possible => It is, we look up an entry at a given address multiple times*/
         if (!out) {
            //No entry found at this precise address
            uint32_t entryid = 0;
            //Attempting to find an entry spanning over it
            while ((entryid < scn->n_entries)
                  && (data_get_addr(scn->entries[entryid]) < addr))
               entryid++;
            //Now at this point entryid-1 contains the index of the entry overlapping this address
            //Updating the size of the entry overlapping the address so that it stops at the address
            data_set_size(scn->entries[entryid - 1],
                  addr - data_get_addr(scn->entries[entryid - 1]));

            if (entryid == scn->n_entries) { //The entry is the last in the section
               DBGMSGLVL(1,
                     "Section %s (%d): Cropping last entry to size %#"PRIx64" and adding new last entry with size %#"PRIx64"\n",
                     scn->name, scn->scnid,
                     addr - data_get_addr(scn->entries[entryid - 1]),
                     scn->address + scn->size - (uint64_t )addr);
               //Creating an entry taking all available space until the end of the section
               out = data_new(DATA_RAW, entrydata,
                     scn->address + scn->size - (uint64_t) addr);
               //Adding the entry at the end of the array of entries
               binscn_add_entry_s(scn, out, entryid);
            } else { //Not the last entry: we have to move the array up one slot
               DBGMSGLVL(1,
                     "Section %s (%d): Cropping entry %u to size %#"PRIx64" and adding new entry at index %u with size %#"PRIx64"\n",
                     scn->name, scn->scnid, entryid - 1,
                     addr - data_get_addr(scn->entries[entryid - 1]), entryid,
                     data_get_addr(scn->entries[entryid]) - (uint64_t )addr);
               //First creating an entry taking all available space before the next entry
               out = data_new(DATA_RAW, entrydata,
                     data_get_addr(scn->entries[entryid]) - (uint64_t) addr);
               //Then increasing the size of the array of entries in the section
               scn->entries = lc_realloc(scn->entries,
                     sizeof(*scn->entries) * (scn->n_entries + 1));
               //Moving the entries up one slot
               memmove(scn->entries + entryid + 1, /*Address of the slot following the next entry*/
               scn->entries + entryid, /*Address of the next entry*/
               sizeof(*scn->entries) * (scn->n_entries - entryid)); /*Size between next entry and end of array*/
               scn->n_entries = scn->n_entries + 1;

               //Adding the entry to the section
               binscn_add_entry_s(scn, out, entryid);
            }
         } else {
            //Otherwise the entry we found suits just fine
            DBGMSGLVL(1, "Section %s (%d): found data at address %#"PRIx64"\n",
                  scn->name, scn->scnid, addr);
         }
      }
      if (off)
         *off = 0;
   }
   return out;
}

/*
 * Retrieves the format of a binary file
 * \param bf Structure representing a binary file
 * \return Identifier of the format or BFF_UNKNOWN if \c bf is NULL or not parsed
 * */
uint8_t binfile_get_format(binfile_t* bf)
{
   return ((bf) ? bf->format : BFF_UNKNOWN);
}

/*
 * Retrieves the section representing the section header
 * \param bf Structure representing a binary file
 * \return The section representing the section header, or NULL if \c bf is NULL or not parsed
 * */
binscn_t* binfile_get_scn_header(binfile_t* bf)
{
   return (bf) ? bf->scnheader : NULL;
}

/*
 * Retrieves the section representing the segment header
 * \param bf Structure representing a binary file
 * \return The section representing the segment header, or NULL if \c bf is NULL or not parsed
 * */
binscn_t* binfile_get_seg_header(binfile_t* bf)
{
   return (bf) ? bf->segheader : NULL;
}

/*
 * Retrieves the name of a segment in a binary file
 * \param scn The segment
 * \return The name of the segment or PTR_ERROR if the segment is NULL
 * */
char* binseg_get_name(binseg_t* seg)
{
   return (seg) ? seg->name : PTR_ERROR;
}

/*
 * Returns the number of sections associated to a segment
 * \param seg The segment
 * \return The number of sections associated to the segment or 0 if \c seg is NULL
 * */
uint16_t binseg_get_nb_scns(binseg_t* seg)
{
   return (seg) ? seg->n_scns : 0;
}

/*
 * Returns a section associated to a segment
 * \param seg The segment
 * \param scnid The index of the section inside the segment (this is not the index of the section in the binary file)
 * \return The section at the given index among the sections associated to the segment, or NULL if \c seg is NULL our scnid is
 * greater than or equal the number of sections associated to this segment
 * */
binscn_t* binseg_get_scn(binseg_t* seg, uint16_t scnid)
{
   return (seg && (scnid < seg->n_scns)) ? seg->scns[scnid] : NULL;
}

/*
 * Returns the offset of a segment in the file
 * \param seg The segment
 * \return The offset of the segment in the file, or 0 if \c seg is NULL
 * */
uint64_t binseg_get_offset(binseg_t* seg)
{
   return (seg) ? seg->offset : 0;
}

/*
 * Returns the offset of the end of a segment in the file
 * \param seg The segment
 * \return The offset of the end of the segment in the file, or 0 if \c seg is NULL
 * */
uint64_t binseg_get_end_offset(binseg_t* seg)
{
   return (seg) ? seg->offset + seg->fsize : 0;
}

/*
 * Returns the virtual address of a segment
 * \param seg The segment
 * \return The virtual address of the segment, or SIGNED_ERROR if \c seg is NULL
 * */
int64_t binseg_get_addr(binseg_t* seg)
{
   return (seg) ? seg->address : SIGNED_ERROR;
}

/*
 * Returns the virtual address of the end of a segment
 * \param seg The segment
 * \return The virtual address of the end of the segment, or SIGNED_ERROR if \c seg is NULL
 * */
int64_t binseg_get_end_addr(binseg_t* seg)
{
   return (seg) ? seg->address + (int64_t) seg->msize : SIGNED_ERROR;
}

/*
 * Returns the size in bytes of a segment in the file
 * \param seg The segment
 * \return The size in bytes of the segment in the file, or 0 if \c seg is NULL
 * */
uint64_t binseg_get_fsize(binseg_t* seg)
{
   return (seg) ? seg->fsize : 0;
}

/*
 * Returns the size in bytes of a segment in memory
 * \param seg The segment
 * \return The size in bytes of the segment in memory, or 0 if \c seg is NULL
 * */
uint64_t binseg_get_msize(binseg_t* seg)
{
   return (seg) ? seg->msize : 0;
}

/*
 * Returns the id of a segment
 * \param seg The segment
 * \return The id of the segment
 */
uint16_t binseg_get_id(binseg_t* seg)
{
   return (seg) ? seg->segid : UNSIGNED_ERROR;
}

/*
 * Returns the attributes of a segment
 * \param seg The segment
 * \return The attributes of the segment
 */
uint8_t binseg_get_attrs(binseg_t* seg)
{
   return (seg) ? seg->attrs : UNSIGNED_ERROR;
}

/*
 * Retrieves the binary file to which a segment belongs
 * \param seg The segment
 * \return A pointer to the binary file containing the segment or PTR_ERROR if the segment is NULL
 * */
binfile_t* binseg_get_binfile(binseg_t* seg)
{
   return (seg) ? seg->binfile : NULL;
}

/*
 * Returns the alignment of a segment
 * \param seg The segment
 * \return The alignment of the segment or 0 if \c seg is NULL
 * */
uint64_t binseg_get_align(binseg_t* seg)
{
   return (seg) ? seg->align : 0;
}

/*
 * Checks if a given mask of attributes is set for a segment
 * \param seg The segment
 * \param attrs Mask of attributes (using SCNA_* values) to check against those of the segment
 * \return TRUE if all attributes set in \c attr are set for \c seg, FALSE otherwise
 * */
int binseg_check_attrs(binseg_t* seg, uint8_t attrs)
{
   return (seg && ((seg->attrs & attrs) == attrs)) ? TRUE : FALSE;
}

/*
 * Adds an attribute to the attributes of a binary segment
 * \param seg Structure representing a binary segment
 * \param attrs Attributes to add
 * */
void binseg_add_attrs(binseg_t* seg, uint8_t attrs)
{
   if (seg)
      seg->attrs |= attrs;
}

/*
 * Sets the alignment of a segment
 * \param seg The segment
 * \param align The alignment
 * */
void binseg_set_align(binseg_t* seg, uint64_t align)
{
   if (seg)
      seg->align = align;
}

/*
 * Sets the offset of a segment in the file
 * \param seg The segment
 * \param offset Offset of the segment in the file
 * */
void binseg_set_offset(binseg_t* seg, uint64_t offset)
{
   if (seg)
      seg->offset = offset;
}

/*
 * Sets the virtual address of a segment
 * \param seg The segment
 * \param address The virtual address of the segment
 * */
void binseg_set_addr(binseg_t* seg, int64_t address)
{
   if (seg)
      seg->address = address;
}

/*
 * Sets the size in bytes of a segment in the file
 * \param seg The segment
 * \param fsize The size in bytes of the segment in the file
 * */
void binseg_set_fsize(binseg_t* seg, uint64_t fsize)
{
   if (seg)
      seg->fsize = fsize;
}

/*
 * Sets the size in bytes of a segment in memory
 * \param seg The segment
 * \param msize The size in bytes of the segment in memory
 * */
void binseg_set_msize(binseg_t* seg, uint64_t msize)
{
   if (seg)
      seg->msize = msize;
}

/*
 * Removes a section from those associated to a segment
 * \param seg A segment
 * \param scn A section associated to the segment
 * */
void binseg_rem_scn(binseg_t* seg, binscn_t* scn)
{
   if (!seg || !scn)
      return;
   uint16_t i;
   DBGMSGLVL(1, "Removing association between section %s (%d) and segment %d\n",
         scn->name, scn->scnid, seg->segid)
   //Looks up the section in the list of sections associated to the segment
   for (i = 0; i < seg->n_scns; i++) {
      if (seg->scns[i] == scn) {
         //Found the section: removing it
         REMOVE_CELL_FROM_ARRAY(seg->scns, seg->n_scns, i);
         break;
      }
   }
   //Now looking up the segment in the list of segments associated to this section
   for (i = 0; i < scn->n_binsegs; i++) {
      if (scn->binsegs[i] == seg) {
         //Found the section: removing it
         REMOVE_CELL_FROM_ARRAY(scn->binsegs, scn->n_binsegs, i);
         break;
      }
   }
}

/*
 * Prints the details of a segment to a stream
 * \param seg The segment
 * \param stream The stream
 * */
void binseg_fprint(binseg_t* seg, FILE* stream)
{
   if (!seg || !stream)
      return;
   fprintf(stream,
         "Segment: Offset [%#"PRIx64" - %#"PRIx64"] (%"PRIu64" bytes) - Address [%#"PRIx64" - %#"PRIx64"] (%"PRIu64" bytes) "
         "- Align %#"PRIx64" - Attrs: ", seg->offset, binseg_get_end_offset(seg),
         seg->fsize, seg->address, binseg_get_end_addr(seg), seg->msize,
         seg->align);
   scnattrs_fprint(seg->attrs, stream);
   if (seg->n_scns > 0) {
      uint16_t i = 0;
      fprintf(stream, "\n\t{%s (%d)", binscn_get_name(seg->scns[i]),
            seg->scns[i]->scnid);
      for (i = 1; i < seg->n_scns; i++) {
         fprintf(stream, ", %s (%d)", binscn_get_name(seg->scns[i]),
               seg->scns[i]->scnid);
      }
      fprintf(stream, "}");
   } else {
      fprintf(stream, "\n\t{}");
   }
}

/*
 * Returns the label associated to a relocation
 * \param rel Structure representing a relocation
 * \return The label associated to the relocation or NULL if \c rel is NULL
 * */
label_t* binrel_get_label(binrel_t* rel)
{
   return (rel) ? rel->label : NULL;
}

/*
 * Returns the pointer associated to a relocation
 * \param rel Structure representing a relocation
 * \return The pointer associated to the relocation or NULL if \c rel is NULL
 * */
pointer_t* binrel_get_pointer(binrel_t* rel)
{
   return (rel) ? rel->ptr : NULL;
}

/*
 * Returns the format specific type associated to a relocation
 * \param rel Structure representing a relocation
 * \return The format specific type associated to the relocation or UINT32_MAX if \c rel is NULL
 * */
uint32_t binrel_get_rel_type(binrel_t* rel)
{
   return (rel) ? rel->reltype : UINT32_MAX;
}

/*
 * Prints a binary relocation
 * \param rel The relocation
 * \param str The string to print to
 * \param size Size of the string
 * */
void binrel_print(binrel_t* rel, char* str, size_t size)
{
   if (!rel || !str)
      return;

   PRINT_INSTRING(str, size, "Relocation: %s <=> ", label_get_name(rel->label));
   pointer_print(rel->ptr, str, size);
}

// Functions for modifying a binary file //
///////////////////////////////////////////

/*
 * Checks if a binfile is being patched. Intended to factorise tests done at the beginning of each binfile_patch_* function
 * \param bf Binary file
 * \return TRUE if the bf is not NULL, is flagged as being patched but not finalised, and has a non NULL creator
 * */
int binfile_patch_is_patching(binfile_t* bf)
{
   return (bf && bf->patch == BFP_PATCHING && bf->creator) ? TRUE : FALSE;
}

/*
 * Checks if a binfile is being patched and finalised. Intended to factorise tests done at the beginning of each binfile_patch_* function
 * \param bf Binary file
 * \return TRUE if the bf is not NULL, is flagged as being finalised, and has a non NULL creator
 * */
static int binfile_patch_isfinalised(binfile_t* bf)
{
   return
         (bf && bf->patch >= BFP_FINALISED && bf->patch < BFP_PATCHED
               && bf->creator) ? TRUE : FALSE;
}

/*
 * Checks if a binfile is in the middle of a patching session. Intended to factorise tests done at the beginning of each binfile_patch_* function
 * \param bf Binary file
 * \return TRUE if the bf is not NULL, is flagged as being patched or finalised, and has a non NULL creator
 * */
int binfile_patch_is_valid(binfile_t* bf)
{
   return
         (bf && bf->patch > BFP_NONE && bf->patch < BFP_PATCHED && bf->creator) ?
               TRUE : FALSE;
}

/*
 * Checks if a section has been impacted by of a patching operation
 * \param scn The section
 * \return TRUE if the section is new or has a different size than the section at the same index in the original, FALSE otherwise
 * */
int binscn_patch_is_bigger(binscn_t* scn)
{
   if (!scn || !binfile_patch_is_valid(scn->binfile))
      return FALSE;
   return
         ((scn->type != SCNT_PATCHCOPY)
               && ((scn->scnid >= scn->binfile->creator->n_sections)
                     || (binfile_get_scn(scn->binfile->creator, scn->scnid)->size
                           < scn->size))) ? TRUE : FALSE;
}

/*
 * Checks if a section has been impacted by of a patching operation
 * \param bf Binary file. Must be being patched
 * \param scnid Index of the section
 * \return TRUE if the section is new or has a different size than the section at the same index in the original, FALSE otherwise
 * */
int binfile_patch_is_scn_bigger(binfile_t* bf, uint16_t scnid)
{
   return (binscn_patch_is_bigger(binfile_get_scn(bf, scnid)));
}

/*
 * Function testing if a patched section can be moved to an interval representing an empty space in the file.
 * If the section can be moved, its address will be updated and the SCNA_PATCHREORDER attribute will be set for it.
 * \param bf Binary file. Must be being patched
 * \param xscnid Index of the section
 * \param interval Interval where we attempt to move the section.
 * \return Interval representing a subrange of addresses inside \c interval and where the section can be moved,
 * or NULL if the section can't be moved into this interval or does not need to be moved. If an error occurred, the
 * last error code in \c bf will updated
 * */
interval_t* binfile_patch_move_scn_to_interval(binfile_t* bf, uint16_t scnid,
      interval_t* interval)
{
   if (!binfile_patch_is_valid(bf) || scnid >= bf->n_sections) { //|| !binfile_patch_is_scn_bigger(bf, scnid) (not this test, it was made before and I may have to force it)
      if (scnid >= bf->n_sections)
         bf->last_error_code = ERR_BINARY_SECTION_NOT_FOUND;
      else
         binfile_set_last_error_code(bf, ERR_BINARY_FILE_NOT_BEING_PATCHED);
      return NULL;
   }
   /**\todo TODO (2015-03-12) Improve this as needed to avoid returning an interval, that will most likely
    * have to be destroyed afterward. But we need to know if 1) the section could be moved (that's the SCNA_PATCHREORDER)
    * and 2) if we used the interval that was provided to do it, and if so, how much of it. It's possible that the section
    * can be moved without using the interval (if the format specific function finds another range for instance)*/
   binscn_t* scn = binfile_get_scn(bf, scnid);
   /**\note (2015-03-12) The intent here is that the format specific function attempts to perform
    * format specific repositioning not covered by this function. It can even do nothing if all
    * cases are covered here*/

   if (scn->attrs & SCNA_PATCHREORDER) {
      assert(scn->address != ADDRESS_ERROR);
      DBGMSG(
            "Section %s (%d) size %"PRIu64" has already been relocated to address %#"PRIx64"\n",
            scn->name, scn->scnid, scn->size, scn->address);
      return NULL; //The format specific function could reposition the section: exiting successfully
   }

   //Invokes the format specific function for attempting to move the section
   interval_t* out = bf->driver.binfile_patch_move_scn_to_interval(bf, scnid, interval);
   /**\note (2015-03-31) If the format specific function wants to let this function do the work, it MUST return \c interval.
    * If it does not flag the section as reordered and returns NULL, this will mean that the section can't fit in the interval
    * Otherwise, it will have flagged the section as reordered and returned an interval or NULL if it managed to relocate it without using the interval*/
   /**\todo (2015-03-31) Find a better way to handle binfile_patch_move_scn_to_interval and the format-specific version, or document it in detail*/
   if (scn->attrs & SCNA_PATCHREORDER) {
      DBG(
            FCTNAMEMSG("Section %s (%d) size %"PRIu64" moved to interval ", scn->name, scn->scnid, scn->size); interval_fprint(out, stderr);STDMSG(" by format-specific function\n");)
      return out; //The format specific function could reposition the section: exiting successfully
   }
   if (out == interval) {
      //The format-specific function leaves us do the work: performing standard repositioning
      out = NULL;
      int64_t addralgn = 0;   //Value to add to the address to align it
      if (scn->align > 0) {
         uint32_t intalign = interval_get_addr(interval) % scn->align;
         if (intalign > 0)
            addralgn = scn->align - intalign;
      }
      if (scn->size <= (interval_get_size(interval) + addralgn)) {
         //The section can fit inside the given interval: updating it
         scn->address = interval_get_addr(interval) + addralgn;
         scn->attrs |= SCNA_PATCHREORDER;
         //Creating an interval representing the space where the section is moved
         out = interval_new(interval_get_addr(interval),
               scn->size + addralgn);
         DBG(
               FCTNAMEMSG("Section %s (%d) size %"PRIu64" moved to interval ", scn->name, scn->scnid, scn->size); interval_fprint(out, stderr);STDMSG("\n");)
      }
   } //Otherwise the format-specific function concluded that this section can't be moved in the given interval
   DBGLVL(1,
         if (!out) {FCTNAMEMSG("Section %s (%d) size %"PRIu64" could not be moved to interval ", scn->name, scn->scnid, scn->size); interval_fprint(interval, stderr);STDMSG("\n");})
   return out;
}

/*
 * Checks if the address of a section has changed from the original because of a patching operation
 * \param scn The section
 * \return TRUE if the section is flagged as reordered (SCNA_PATCHREORDER) and has a different address
 * than in the original file or is new, FALSE otherwise
 * */
int binscn_patch_is_moved(binscn_t* scn)
{
   if (!scn || !binfile_patch_is_valid(scn->binfile))
      return FALSE;

   if (!(scn->attrs & SCNA_PATCHREORDER))
      return FALSE;  //Section is not flagged as reordered: not moved

   if (scn->scnid >= scn->binfile->creator->n_sections)
      return TRUE; //New section: it is always considered different from the original

   if (scn->address
         != binfile_get_scn(scn->binfile->creator, scn->scnid)->address)
      return TRUE;   //Address has changed
   return FALSE;
}

/*
 * Checks if the address of a section has changed from the original because of a patching operation
 * \param bf Binary file, must be being patched
 * \param scnid Index of the section
 * \return TRUE if the section is flagged as reordered (SCNA_PATCHREORDER) and has a different address
 * than in the original file or is new, FALSE otherwise
 * */
int binfile_patch_is_scn_moved(binfile_t* bf, uint16_t scnid)
{
   return (binscn_patch_is_moved(binfile_get_scn(bf, scnid)));
}

/*
 * Checks if a given section has been added through a patching operation
 * \param scn A section
 * \return TRUE if the section belongs to a file being patched and does not exist in the creator, FALSE otherwise
 * */
int binscn_patch_is_new(binscn_t* scn)
{
   if (!scn || !binfile_patch_is_valid(scn->binfile))
      return FALSE; //Section does not exist or is not associated to a file being patched

   return (scn->scnid >= scn->binfile->creator->n_sections) ? TRUE : FALSE;
}

/*
 * Retrieves the type of a section in a file being patched. If the section is of type SCNT_PATCHCOPY, the type from its origin
 * will be returned instead
 * \param scn A section
 * \return The type of the section, or the type of the section from which it was copied if it is of type SCNT_PATCHCOPY, or SCNT_UNKNOWN
 * if \c scn is NULL or is not associated to a file being patched
 * */
uint8_t binscn_patch_get_type(binscn_t* scn)
{
   if (!scn || !binfile_patch_is_valid(scn->binfile))
      return SCNT_UNKNOWN; //Section does not exist or is not associated to a file being patched
   if (scn->type == SCNT_PATCHCOPY) {
      assert(scn->scnid < scn->binfile->creator->n_sections);
      return binfile_get_scn(scn->binfile->creator, scn->scnid)->type;
   } else
      return scn->type;
}

/*
 * Retrieves the first instruction of a section in a file being patched.
 * If the section is of type SCNT_PATCHCOPY or if its first instruction is NULL, the first instructions from its origin will be returned instead
 * \param scn A section
 * \return The node containing the first instruction of the section, or the container containing the first instruction of the section
 * from which it was copied if it is of type SCNT_PATCHCOPY, or NULL if \c scn is NULL or is not associated to a file being patched
 * */
list_t* binscn_patch_get_first_insn_seq(binscn_t* scn)
{
   if (!scn || !binfile_patch_is_valid(scn->binfile))
      return NULL; //Section does not exist or is not associated to a file being patched
   if (scn->type == SCNT_PATCHCOPY || scn->firstinsnseq == NULL) {
      return binscn_get_first_insn_seq(
            binfile_get_scn(scn->binfile->creator, scn->scnid));
   } else
      return scn->firstinsnseq;
}

/*
 * Retrieves the last instruction of a section in a file being patched.
 * If the section is of type SCNT_PATCHCOPY or if its last instruction is NULL, the last instructions from its origin will be returned instead
 * \param scn A section
 * \return The node containing the last instruction of the section, or the container containing the last instruction of the section
 * from which it was copied if it is of type SCNT_PATCHCOPY, or NULL if \c scn is NULL or is not associated to a file being patched
 * */
list_t* binscn_patch_get_last_insn_seq(binscn_t* scn)
{
   if (!scn || !binfile_patch_is_valid(scn->binfile))
      return NULL; //Section does not exist or is not associated to a file being patched
   if (scn->type == SCNT_PATCHCOPY || scn->lastinsnseq == NULL) {
      return binscn_get_last_insn_seq(
            binfile_get_scn(scn->binfile->creator, scn->scnid));
   } else
      return scn->lastinsnseq;
}

/*
 * Retrieves an entry in a section being patched. If it has not yet been duplicated from the original, the entry
 * from the original will be returned.
 * \warning This function is intended solely when wanting to test some properties on an entry whether it has been duplicated
 * or not, NOT when modifying it (\ref binfile_patch_get_scn_entrycopy_s must be used instead)
 * \param scn The section
 * \param entryid The index of the entry
 * \return A pointer to the new entry or the original, or NULL if \c bf is NULL, not in the process of being patched, or \c scnid or \c entryid invalid
 * */
static data_t* binscn_patch_getentry(binscn_t* scn, uint32_t entryid)
{
   assert(scn);

   if (scn->type != SCNT_PATCHCOPY && (entryid < scn->n_entries)
         && scn->entries[entryid])
      return scn->entries[entryid]; //Entry has already been copied
   else {
      assert(binfile_patch_is_valid(scn->binfile));
      binscn_t* originscn = binfile_get_scn(scn->binfile->creator,
            scn->scnid);
      if (entryid < originscn->n_entries && originscn->entries[entryid])
         return originscn->entries[entryid]; //Original entry
   }
   return NULL; //Section index or entry index is out of range of the original

}

/*
 * Returns the original section of a section in a patched binary file
 * \param scn A section. Must belong to a file being patched
 * \return The section in the original file from which this section is a copy, or NULL if \c scn is NULL,
 * does not belong to a valid file being patched, or has been added by the patching session
 * */
binscn_t* binscn_patch_get_origin(binscn_t* scn)
{
   if (!scn || !binfile_patch_is_valid(scn->binfile))
      return NULL; //Section does not exist or is not associated to a file being patched

   return binfile_get_scn(scn->binfile->creator, scn->scnid);
}

/*
 * Returns the data bytes of a binary section in a file being patched.
 * If the section had not been modified yet, the bytes from the original section will be copied
 * \param scn Structure representing a binary section
 * \return A pointer to the bytes contained in the section, or NULL if the section is NULL or does not belong to a file being patched
 * */
unsigned char* binscn_patch_get_data(binscn_t* scn)
{
   /**\todo (2015-04-28) So far I'm not using this function, but I keep it if a patching operation requires a direct
    * access to the content of a section that is not of any other supported types (e.g. the GNU_verneed sections from ELF)*/
   if (!scn || !binfile_patch_is_valid(scn->binfile))
      return NULL; //Section does not exist or is not associated to a file being patched
   unsigned char* out = scn->data;
   if (!out) {
      //Section data is NULL: checking the state of the origin
      binscn_t* origin = binscn_patch_get_origin(scn);
      if (origin && scn->size > 0 && origin->data) {
         if (binfile_patch_isfinalised(scn->binfile)) {
            //File is finalised: we return the data from the original file as it is not supposed to be modified
            out = origin->data;
         } else {
            //File is being patched: initialising its data from the original (similar behaviour to binfile_patch_get_scn_entry)
            //Section has a non null size and the origin contains data: initialising section data
            unsigned char* data = lc_malloc(sizeof(*data) * scn->size);
            //Copying the data from the original
            memcpy(data, origin->data, origin->size);
            //Setting the byte to the section (this will mark it as having being copied)
            binscn_patch_set_data(scn, data);
            out = scn->data;
         }
      }
   }
   return out;
}

/*
 * Sets the data bytes of a binary section in a file being patched.
 * This will also flag the section has having been modified (if its type was set to SCNT_PATCHCOPY, it will be updated to the type of the original)
 * \param scn Structure representing a binary section
 * \param data The data bytes contained in the section. It will be freed along with the section.
 * \return EXIT_SUCCESS if the section belongs to a file being patched and has no data or data allocated locally, error code otherwise
 * */
int binscn_patch_set_data(binscn_t* scn, unsigned char* data)
{
   /**\todo TODO (2015-04-28) See if it would be interesting here to provide the size as well and update it, or
    * if we assume that the section size has already been set*/
   if (!scn)
      return ERR_BINARY_MISSING_SECTION;  //Section does not exist
   if (!binfile_patch_is_valid(scn->binfile))
      return ERR_BINARY_FILE_NOT_BEING_PATCHED; //Section is not associated to a file being patched
   if (scn->data && !(scn->attrs & SCNA_LOCALDATA))
      return ERR_BINARY_SECTION_DATA_NOT_LOCAL; //Section contains data not allocated locally: we can't do it
   if (!scn->data) {
      //Section had no data: setting it
      scn->data = data;
      scn->attrs |= SCNA_LOCALDATA;
   } else if (scn->data && (scn->attrs & SCNA_LOCALDATA)) {
      //Section already contains existing data allocated locally: freeing it before setting the new data
      lc_free(scn->data);
      scn->data = data;
   }
   //Updating the type of the section
   if (scn->type == SCNT_PATCHCOPY) {
      assert(binscn_patch_get_origin(scn));
      scn->type = binscn_get_type(binscn_patch_get_origin(scn));
      DBGMSGLVL(1,
            "Updating data bytes of section %s: flagging section as modified\n",
            scn->name);
   }
   return EXIT_SUCCESS;
}

/*
 * Generates the data bytes of a section from its entries
 * \param scn Structure representing a binary section
 * \return EXIT_SUCCESS if successful, error code otherwise
 * */
int binscn_patch_set_data_from_entries(binscn_t* scn)
{
   if (!scn)
      return ERR_BINARY_MISSING_SECTION;  //Section does not exist
   if (!binfile_patch_is_valid(scn->binfile))
      return ERR_BINARY_FILE_NOT_BEING_PATCHED; //Section is not associated to a file being patched
   int out = EXIT_SUCCESS;
   if (scn->type == SCNT_ZERODATA)
      return out; //This section does not contains bytes: no need to create its bytes
   uint32_t j;
   uint64_t size = scn->size;
   assert(size > 0); //Just checking, because right now I'm not sure of anything any more (2015-04-14)
   //Reserves the space of bytes for the section
   unsigned char* scndata = lc_malloc(sizeof(*scndata) * size);
   uint64_t off = 0;
   for (j = 0; j < binscn_get_nb_entries(scn); j++) {
      data_t* entry = binscn_patch_getentry(scn, j);
      unsigned char* entrybytes = data_to_bytes(entry);
      if (!entrybytes) {
         if (data_get_type(entry) != DATA_NIL) {
            //Skipping entries that we could not convert as bytes
            ERRMSG(
                  "Unable to store data entry %u into section %s. Skipping entry\n",
                  j, binscn_get_name(scn));
            out = ERR_BINARY_FAILED_SAVING_DATA_TO_SECTION;
         } else if (data_get_size(entry) > 0) {
            //Data contains a NULL value but has a length: adding null bytes into the section
            memset(scndata + off, 0, data_get_size(entry));
         }
      } else {
         //Copying the entry data into the section
         memcpy(scndata + off, entrybytes, data_get_size(entry));
      }
      off += data_get_size(entry);
   }
   assert(off == size); //Checking again
   //Storing the data into the section
   binscn_patch_set_data(scn, scndata);
   return out;
}

/*
 * Adds an entry to a patched section. The entry will be appended to the array of entries
 * \param scn The section. Must belong to a file being patched
 * \param entry The entry to add
 * \return EXIT_SUCCESS if successful, error code if \c scn or \c entry is NULL or if \c scn does not belong to a file being patched
 * */
int binscn_patch_add_entry(binscn_t* scn, data_t* entry)
{
   if (!scn)
      return ERR_BINARY_MISSING_SECTION;  //Section does not exist
   if (!binfile_patch_is_valid(scn->binfile))
      return ERR_BINARY_FILE_NOT_BEING_PATCHED; //Section is not associated to a file being patched
   if (!entry)
      return ERR_COMMON_PARAMETER_MISSING;

   //Handle addresses
   if (scn->attrs & SCNA_LOADED) { //Address is only useful if the section is to be loaded to memory
      //Setting the address preceding the entry from either the section or the previous section
      int64_t lastaddr =
            (scn->n_entries > 0) ?
                  data_get_end_addr(scn->entries[scn->n_entries - 1]) :
                  scn->address;
      int64_t entryaddr = data_get_addr(entry);
      if (lastaddr != ADDRESS_ERROR) {
         if (entryaddr == ADDRESS_ERROR) {
            //Updating the address of the entry from the address of either the section or the previous entry
            data_set_addr(entry, lastaddr); //First entry of the section: updating its address from the address of the section
         } else if (entryaddr != ADDRESS_ERROR && entryaddr > lastaddr) {
            //There is a gap between the entry address and the address of the previous entry or the section: inserting padding
            data_t* padding = data_new(DATA_NIL, NULL, entryaddr - lastaddr);
            data_set_addr(padding, lastaddr);
            binscn_patch_add_entry(scn, padding);
         }
      }
   }

   //Inserting the entry into the array
   ADD_CELL_TO_ARRAY(scn->entries, scn->n_entries, entry);

   //Increases the size of the section
   scn->size += data_get_size(entry);

   //Forcing the type of data to DATA_NIL if the section is of type ZERODATA
   if (scn->type == SCNT_ZERODATA)
      data_set_type(entry, DATA_NIL);

   //Associates the entry to the section if it was not already associated to a label
   if (!data_get_section(entry))
      data_set_scn(entry, scn);

   return EXIT_SUCCESS;
}

/*
 * Adds a new segment to a binary file in the process of being patched
 * \param bf The binary file. It must be being patched
 * \param attrs Attributes to set on the segment
 * \param align Alignment of the new segment
 * \return A pointer to an empty new segment, or NULL if \c bf is NULL or not being patched
 * */
binseg_t* binfile_patch_add_seg(binfile_t* bf, unsigned int attrs,
      uint64_t align)
{
   if (!binfile_patch_is_patching(bf))
      return NULL;
   binseg_t* newseg = binseg_new(bf->n_segments, 0, 0, 0, 0, attrs, align);
   ADD_CELL_TO_ARRAY(bf->segments, bf->n_segments, newseg);
   //Invokes the format specific function for additional handling
   bf->driver.binfile_patch_add_seg(bf, newseg);
   return newseg;
}

/*
 * Reorders the sections in a binary file according to their offset
 * \param bf The binary file. It must be being patched
 * \return EXIT_SUCCESS if successful, error code if \c is NULL or not being patched
 * */
int binfile_patch_reorder_scn_by_offset(binfile_t* bf)
{
   /**\todo (2015-05-20) Add a parameter here to specify whether we want to replace gaps between sections with
    * a copy of the original section or not*/
   if (!binfile_patch_isfinalised(bf))
      return ERR_BINARY_PATCHED_FILE_NOT_FINALISED;
   qsort(bf->sections, bf->n_sections, sizeof(*bf->sections),
         binscn_cmpbyoffset_qsort);
   //Reorders the arrays of loaded and code sections by address
   if (bf->codescns)
      SORT_SCNARRAY(bf->codescns, bf->n_codescns);
   if (bf->loadscns)
      SORT_SCNARRAY(bf->loadscns, bf->n_loadscns);
   bf->patch = BFP_REORDERED;

   DBG(
         FCTNAMEMSG("Binary file %s sections have been reordered. New order of sections:\n", bf->filename); uint16_t __sid; for (__sid = 0; __sid < bf->n_sections; __sid++) { binscn_t* __s = binfile_get_scn(bf, __sid); STDMSG("\t[%d]: %s (previously %d) - address %#"PRIx64" - offset %#"PRIx64" - size %"PRIu64" - (%s%s%s%s)\n", __sid, __s->name, __s->scnid, __s->address, __s->offset, __s->size, (__s->type == SCNT_PATCHCOPY)?"not modified":"modified", (binscn_patch_is_new(__s))?" new":"", (binscn_patch_is_bigger(__s) && !binscn_patch_is_new(__s))?" bigger":"", (binscn_patch_is_moved(__s))?" moved":""); })

   return EXIT_SUCCESS;
}

/**
 * Initialises a new section structure in a binary file as a copy of another
 * \param copy A copied binary file
 * \param origin An section from the original file
 * \return A partly initialised section copied from the original
 * */
static binscn_t* binfile_patch_initscncopy(binfile_t* copy, binscn_t* scnorigin)
{
   assert(binfile_patch_is_patching(copy) && scnorigin);
   binscn_t* scncopy = binscn_new(copy, scnorigin->scnid, scnorigin->name,
         SCNT_PATCHCOPY, scnorigin->address, scnorigin->attrs);
   scncopy->size = scnorigin->size;
   scncopy->align = scnorigin->align;
   scncopy->offset = scnorigin->offset;
   scncopy->entrysz = scnorigin->entrysz;

   /**\todo TODO (2015-03-30) I'm not sure if I will need to duplicate the array of segments. If not, remove what follows*/
   if (scnorigin->n_binsegs > 0) {
      uint16_t j;
      //Duplicates the array of segments to which the section belongs
      scncopy->binsegs = lc_malloc0(
            sizeof(*scncopy->binsegs) * scnorigin->n_binsegs);
      scncopy->n_binsegs = scnorigin->n_binsegs;
      //Fills the array of segments with the references to the copies of the segment
      for (j = 0; j < scnorigin->n_binsegs; j++) {
         scncopy->binsegs[j] = binfile_get_seg(copy,
               scnorigin->binsegs[j]->segid);
      }
   }
   return scncopy;
}

/*
 * Retrieves a section in a file being patched. If it has not yet been duplicated from the original, the section
 * from the original will be returned.
 * \warning This function is intended solely when wanting to test some properties on a section whether it has been duplicated
 * or not, NOT when modifying it (\ref binfile_patch_get_scn_copy_s must be used instead)
 * \param bf The binary file. Must be in the process of being patched
 * \param scnid The index of the section
 * \return A pointer to the new section or the original, or NULL if \c bf is NULL, not in the process of being patched, or \c scnid invalid
 * */
binscn_t* binfile_patch_get_scn(binfile_t* bf, uint16_t scnid)
{
   if (!binfile_patch_is_valid(bf)) {
      binfile_set_last_error_code(bf, ERR_BINARY_FILE_NOT_BEING_PATCHED);
      return NULL;
   }
   if (scnid < bf->n_sections) {
      assert(binfile_get_scn(bf, scnid));
      if (binfile_get_scn(bf, scnid)->type != SCNT_PATCHCOPY)
         return binfile_get_scn(bf, scnid); // Section has already been duplicated or added
      else if (binfile_get_scn(bf, scnid)->scnid < bf->creator->n_sections)
         return binfile_get_scn(bf->creator, scnid); //Section has not been duplicated: returning the original
      else {
         assert(FALSE); //This should never happen if the file has been correctly duplicated using binfile_patch_init_copy
         return NULL; //Section has not been duplicated and is out of range of the original
      }
   } else
      return NULL; //Section index is out of range of the copied file
}

/*
 * Retrieves an entry in a file being patched. If it has not yet been duplicated from the original, the entry
 * from the original will be returned.
 * \warning This function is intended solely when wanting to test some properties on an entry whether it has been duplicated
 * or not, NOT when modifying it (\ref binfile_patch_get_scn_entrycopy_s must be used instead)
 * \param bf The binary file. Must be in the process of being patched
 * \param scnid The index of the section
 * \param entryid The index of the entry
 * \return A pointer to the new entry or the original, or NULL if \c bf is NULL, not in the process of being patched, or \c scnid or \c entryid invalid
 * */
data_t* binfile_patch_get_scn_entry(binfile_t* bf, uint16_t scnid,
      uint32_t entryid)
{
   /**\todo TODO (2015-04-20) Refactorise this using binfile_patch_get_scn, could make the code simpler
    * Oh and maybe I would stop having trouble with finalised files where the indices between both files have changed*/
   if (!binfile_patch_is_valid(bf)) {
      binfile_set_last_error_code(bf, ERR_BINARY_FILE_NOT_BEING_PATCHED);
      return NULL;
   }
   binscn_t* scn = binfile_patch_get_scn(bf, scnid);
   if (scn) {
      return binscn_patch_getentry(scn, entryid);
   } else
      return NULL;   //Section index is out of range of the copied file
}

/**
 * Retrieves a section in a file being patched, duplicating it from the original if needed.
 * This is intended to be used when the section will be modified
 * \param bf The binary file
 * \param scnid The index of the section
 * \return A pointer to the new section, or NULL if \c bf is NULL or not in the process of being patched
 * */
static binscn_t* binfile_patch_get_scn_copy_s(binfile_t* bf, uint16_t scnid)
{
   assert(binfile_patch_is_valid(bf));
   binscn_t* copy = binfile_get_scn(bf, scnid);
   binscn_t* origin = binfile_get_scn(bf->creator, scnid);

   if (!copy || !origin)
      return NULL; //Section out of range or not existing in the creator (this is beginning to look bad by the way)

   //Section has already been duplicated
   if (copy && (copy->type != SCNT_PATCHCOPY))
      return copy;

   assert(binfile_patch_is_patching(bf)); //We should not have to create copies of sections after a finalisation

   //Initialises the array of entries as empty
   copy->entries = lc_malloc0(sizeof(*copy->entries) * origin->n_entries);
   copy->n_entries = origin->n_entries;

   //Flags the section as copied
   copy->type = origin->type;

   return copy;
}

/*
 * Retrieves a section in a file being patched, duplicating it from the original if needed.
 * This is intended to be used when the section will be modified
 * \param bf The binary file
 * \param scnid The index of the section
 * \return A pointer to the new section, or NULL if \c bf is NULL or not in the process of being patched
 * */
binscn_t* binfile_patch_get_scn_copy(binfile_t* bf, uint16_t scnid)
{
   if (!binfile_patch_is_patching(bf) || scnid >= bf->n_sections)
      return NULL;
   return binfile_patch_get_scn_copy_s(bf, scnid);
}

static data_t* binfile_patch_get_scn_entrycopy_s(binfile_t* bf, uint16_t scnid,
      uint32_t entryid);

/**
 * Duplicates the entry containing a label and returns the label from the copy
 * \param bf The binary file
 * \param label The label
 * \return Copy of the label that is in the copied entry or the label itself if it had already been copied
 * */
static label_t* binfile_patch_getlabelcopy(binfile_t* bf, label_t* label)
{
   assert(binfile_patch_is_valid(bf) && label);
   binscn_t* lblscn = label_get_scn(label);
   label_t* labelcopy = label;
   if (!lblscn || lblscn->binfile != bf) {
      //The label has not yet been duplicated or is not associated to a section (happens for relocations): duplicating the entry containing it
      assert(!lblscn || lblscn->binfile == bf->creator); //Just checking
      //Finds the section containing the entry containing the label
      uint16_t i;
      uint32_t lblid;
      //Scans all sections containing labels to try to find the entry containing it and its index
      for (i = 0; i < bf->creator->n_lblscns; i++) {
         lblid = binscn_find_label_id(bf->creator->lblscns[i], label);
         if (lblid != BF_ENTID_ERROR)
            break;
         /**\todo TODO (2015-04-29) WARNING! As said in binscn_find_label_id, this is a potentially huge bottleneck.
          * If it turns out to slow down the patching process (which I strongly suspect it will), create a hashtable
          * to link a label to the entry containing it, and another to link an entry to its section and index in the section */
      }
      assert(lblid < BF_ENTID_ERROR || !lblscn); //Either we found the label id, or it's not associated to a section and has already been duplicated
      if (lblid != BF_ENTID_ERROR && i < bf->creator->n_lblscns) {
         //Duplicates the entry containing the label
         data_t* labelentcopy = binfile_patch_get_scn_entrycopy_s(bf,
               bf->creator->lblscns[i]->scnid, lblid);
         //Returns the copy of the label
         labelcopy = data_get_data_label(labelentcopy);
      }  //Otherwise we assume the label has already been duplicated
   }
   return labelcopy;
}

/**
 * Create entry copies for all entries referencing or referenced by a copied entry.
 * \param bf The binary file
 * \param original Original entry
 * \param copy Copied entry
 * */
static void binfile_patch_dupscnentryrefs(binfile_t* bf, data_t* original,
      data_t *copy)
{
   assert(binfile_patch_is_valid(bf) && original && copy);

   // Just checking in case it contains a pointer: duplicating it too
   pointer_t* ptr = data_get_ref_ptr(copy);
   if (ptr) {
      /**\todo TODO TODO TODO (2014-06-10) This may not be necessary depending on how I deal with those kind of entries in the reorder.
       * => (2015-04-28) Pretty sure it is*/
      data_t *target = pointer_get_data_target(ptr);
      binscn_t* targetscn = data_get_section(target);
      if (targetscn && (targetscn->binfile == bf->creator)
            && (targetscn->type != SCNT_DATA)
            && (targetscn->type != SCNT_CODE)) {
         //Duplicating the target entry if it is not a data or code entry, and belongs to the creator
         uint32_t entryid = binscn_findentryid(targetscn, target);/**\todo TODO TODO TODO (2014-06-10) WARNING POTENTIAL BOTTLENECK*/
         //Duplicating the target entry
         data_t* newtarget = binfile_patch_get_scn_entrycopy_s(bf,
               targetscn->scnid, entryid);
         //Setting the duplicated entry as the new target
         pointer_set_data_target(ptr, newtarget);
         //Storing the copy in the table indexed by its destination
         hashtable_insert(bf->data_ptrs_by_target_data, newtarget, copy);
      }
   }
   //Duplicating the label associated to the entry
   label_t* originlbl = data_get_label(original);
   if (label_get_target(originlbl) == original && !data_get_label(copy)) {
      //The original entry is the target of a label and the copy has not been associated to a label: duplicating the label
      binfile_patch_getlabelcopy(bf, originlbl); //This also links the label with the copied entry
   }

   //Retrieving the data entries referencing the original data
   queue_t* refs = hashtable_lookup_all(bf->creator->data_ptrs_by_target_data, original);
   //Duplicates each of those entries
   FOREACH_INQUEUE(refs, iter) {
      data_t* ref = GET_DATA_T(data_t*, iter);
      data_t* patchedref = binfile_patch_get_entry_copy(bf, ref);
      if (patchedref) {
         //Linking the duplicated origin to the the duplicated entry
         pointer_set_data_target(data_get_ref_ptr(patchedref), copy);
         //Stores the copy in the table of data_ptrs_by_target_data indexed by its destination
         hashtable_insert(bf->data_ptrs_by_target_data, patchedref, copy);
      }
   }
   queue_free(refs, NULL);
}

/**
 * Retrieves an entry from a section in a file being patched, duplicating it from the original if needed.
 * This is intended to be used when the entry will be modified
 * \param bf The binary file
 * \param scnid The index of the section
 * \param entryid The index of the entry
 * \return A pointer to the entry, or NULL if \c bf is NULL or not in the process of being patched
 * */
static data_t* binfile_patch_get_scn_entrycopy_s(binfile_t* bf, uint16_t scnid,
      uint32_t entryid)
{
   assert(binfile_patch_is_valid(bf));

   binscn_t* scn = binfile_patch_get_scn_copy_s(bf, scnid);

   if (!scn)
      return NULL; //Something wrong with the section

   if (scn->entries[entryid])
      return scn->entries[entryid]; //Entry has already been duplicated or created

   if ((entryid >= scn->n_entries)
         || (entryid >= binfile_get_scn(bf->creator, scn->scnid)->n_entries)
         || (!binfile_get_scn(bf->creator, scn->scnid)->entries[entryid]))
      return NULL; //Entry out of range or not existing with the creator

   data_t* originalentry =
         binfile_get_scn(bf->creator, scn->scnid)->entries[entryid];
   //Duplicating the entry from the original
   scn->entries[entryid] = data_copy(originalentry);
   //Handling special entries
   switch (data_get_type(scn->entries[entryid])) {
   label_t* lblcopy, *lblorigin;
   binrel_t* relcopy;
case DATA_LBL:
   assert(data_get_raw(scn->entries[entryid]) == data_get_raw(originalentry)); //Checking that the object has not been duplicated
   lblorigin = data_get_data_label(originalentry);
   //Entry contains a label: we need to duplicate the label_t structure as well
   lblcopy = label_copy(lblorigin);
   label_set_scn(lblcopy,
         binfile_patch_get_scn_copy(bf,
               binscn_get_index(label_get_scn(lblorigin)))); //Associates the label to the copy of the section
   //Stores the label in the list of labels of the copied file
   ADD_CELL_TO_ARRAY(bf->labels, bf->n_labels, lblcopy)
   ;
   //Updates the target of the label if it is of type data
   if (label_get_target_type(lblcopy) == TARGET_DATA) {
      data_t* lbltarget = label_get_target(lblcopy);
      //Duplicates the target of the label
      data_t* targetcopy = binfile_patch_get_entry_copy(bf, lbltarget);
      //Links the copied label to the copied entry
      data_link_label(targetcopy, lblcopy);
   } //Otherwise it is an instruction and so far we don't move the label with them
   /**\todo TODO (2015-04-29) If at some point we want the labels to follow the moved instructions,
    * we would need to scan the array of label of binfile in the patcher, detect those targeting an
    * instruction, and update them so that they target the patched insn associated to the original
    * target if there is one. But I don't think that would be useful.
    * We may copy those labels, though, using a different name*/
   //Stores the label in the entry
   data_set_content(scn->entries[entryid], lblcopy, DATA_LBL);
   break;
case DATA_REL:
   assert(data_get_raw(scn->entries[entryid]) == data_get_raw(originalentry)); //Checking that the object has not been duplicated
   //Entry contains a relocation: we need to duplicate the binrel_t structure as well
   relcopy = binrel_copy(data_get_binrel(originalentry));
   assert(relcopy);  //If this can legitimately happen, use an if statement here
   //Stores the relocation in the list of relocations of the copied file
   ADD_CELL_TO_ARRAY(bf->relocs, bf->n_relocs, relcopy)
   ;
   //We need to update the label as well (the pointer will be taken care of in binfile_patch_dupscnentryrefs)
   relcopy->label = binfile_patch_getlabelcopy(bf, relcopy->label);
   //Stores the relocation in the entry
   data_set_content(scn->entries[entryid], relcopy, DATA_REL);
   break;
   }
   //Linking the copy to the section
   data_set_scn(scn->entries[entryid], scn);
   //Linking the duplicated entry to its original through the hashtable
   hashtable_insert(bf->entrycopies, originalentry, scn->entries[entryid]);

   //Creating copies for all entries referencing or referenced by this entry
   binfile_patch_dupscnentryrefs(bf, originalentry, scn->entries[entryid]);

   return scn->entries[entryid];
}

/*
 * Retrieves an entry from a section in a file being patched, duplicating it from the original if needed.
 * This is intended to be used when the entry will be modified
 * \param bf The binary file
 * \param scnid The index of the section
 * \param entryid The index of the entry
 * \return A pointer to the entry, or NULL if \c bf is NULL or not in the process of being patched
 * */
data_t* binfile_patch_get_scn_entrycopy(binfile_t* bf, uint16_t scnid,
      uint32_t entryid)
{
   if (!binfile_patch_is_patching(bf))
      return NULL;
   return binfile_patch_get_scn_entrycopy_s(bf, scnid, entryid);
}

/*
 * Returns the duplicated entry for a given entry. If the file is not finalised, a copy will be made if the entry
 * has not already been duplicated.
 * \param bf The binary file. It must be in the process of being patched
 * \param entry An existing entry in the original binary file
 * \return The entry in the patched file, or NULL if \c bf or \c entry is NULL or if \c entry was not found in the patched file creator
 * or if the file has already been finalised and \c entry had not been already duplicated
 * */
data_t* binfile_patch_get_entry_copy(binfile_t* bf, data_t* entry)
{
   /**\todo TODO (2015-03-04) Warning. This function may cause a significant overhead if used repeatedly. In the future, we may need some
    * sort of hashtable or something to link an entry to its index in the section.
    * Also, it considers that the entry belongs to the creator file. I'm not quite sure of its behaviour if we pass an entry from the patched
    * file. This should not happen in my current view of the algorithm, but since it keeps changing there is no way to be sure and it would
    * not hurt to add a fallback
    * => (2015-04-13) Attempting to reduce the problem using the entrycopies hashtable*/
   if (!binfile_patch_is_valid(bf) || !entry)
      return NULL;
   data_t* out = NULL;
   uint32_t entryid = BF_ENTID_ERROR;
   uint16_t scnid = BF_SCNID_ERROR;

   //Attempts to find the entry in the hashtable of duplicated entries
   out = hashtable_lookup(bf->entrycopies, entry);
   if (out || bf->patch == BFP_FINALISED)
      return out; //We could find it or the file has been finalised: returning the copy

   //Otherwise we have to look it up painfully

   //Retrieves the section of the entry
   binscn_t* scn = data_get_section(entry);
   if (!scn) {
      //Problem: we can't find the section. We will have to look up inside each section to find the entry
      if (data_get_addr(entry) > 0) {
         //We can use the address of the entry to find the section to which it belongs
         scn = binfile_lookup_scn_span_addr(bf->creator,
               data_get_addr(entry));
         scnid = scn->scnid;
         entryid = binscn_findentryid(binfile_get_scn(bf->creator, scnid),
               entry);
      }
      if (!scn) {
         //We are royally screwed here actually: we have to scan each section separately
         uint16_t scnid;
         uint32_t entryid;
         for (scnid = 0; scnid < bf->n_sections; scnid++) {
            entryid = binscn_findentryid(binfile_get_scn(bf->creator, scnid),
                  entry);
            if (entryid < BF_ENTID_ERROR)
               break;
         }
      }
   } else {
      scnid = scn->scnid;
      //Section found: we can look up the entry inside
      entryid = binscn_findentryid(scn, entry);
   }
   if (bf->patch == BFP_FINALISED) {
      //Special case: the sections have been reordered, so the scnid we found is the scnid in the creator file
      /**\todo TODO (2015-04-20) If this search has to occur often, we will have to add an array for
       * the correspondence between old and new scnids in the binfile_t (it's the one currently computed in the libtroll)*/
      uint16_t newscnid = 0;
      //Scans the file to fine the section whose original index is the one we are looking for
      for (newscnid = 0; newscnid < bf->n_sections; newscnid++)
         if (binfile_get_scn(bf, newscnid)->scnid == scnid)
            break;
      assert(newscnid < bf->n_sections);
      scnid = newscnid;
   }

   out = binfile_patch_get_scn_entrycopy_s(bf, scnid, entryid);

   return out;
}

/*
 * Initialises a new binary file structure as a copy of another that will be used for patching.
 * \param bf The original binary file
 * \return A binary file referencing \c bf as its creator or NULL if an error occurred (file could not be created for instance)
 * */
binfile_t* binfile_patch_init_copy(binfile_t* bf)
{
   if (!bf)
      return NULL;
   uint16_t i; //Increment for sections

   //Creates a temporary name for the file
   const char* tmpsuf = " (patching) ";
   char tmpname[strlen(bf->filename) + strlen(tmpsuf) + 1];
   sprintf(tmpname, "%s%s", bf->filename, tmpsuf);

   //Creates the copy
   binfile_t* copy = binfile_new(tmpname);
   //Sets the original as its creator
   copy->creator = bf;
   //Sets the patching status of the file
   copy->patch = BFP_PATCHING;
   //Sets some characteristics of the file to be identical to its creator
   copy->type = bf->type;
   copy->format = bf->format;
   copy->driver = bf->driver;
   copy->arch = bf->arch;
   copy->wordsize = bf->wordsize;

   /**\todo TODO (2015-03-30) I'm not sure if I will need to duplicate the array of segments. If not, remove what follows*/
   //Duplicating the array of segments
   if (bf->n_segments > 0) {
      copy->segments = lc_malloc0(sizeof(*copy->segments) * bf->n_segments);
      copy->n_segments = bf->n_segments;
      //Initialises the array of sections of the copy
      for (i = 0; i < bf->n_segments; i++) {
         binseg_t* seg = bf->segments[i];
         copy->segments[i] = binseg_new(seg->segid, seg->offset, seg->address,
               seg->fsize, seg->msize, seg->attrs, seg->align);
         copy->segments[i]->scns = lc_malloc0(
               sizeof(*copy->segments[i]->scns) * seg->n_scns);
         copy->segments[i]->n_scns = bf->segments[i]->n_scns;
      }
   }

   //Duplicating the section and segment headers, but not the
   if (bf->scnheader)
      copy->scnheader = binfile_patch_initscncopy(copy, bf->scnheader);

   if (bf->segheader)
      copy->segheader = binfile_patch_initscncopy(copy, bf->segheader);

   //Duplicating the array of sections
   if (bf->n_sections > 0) {
      binfile_set_nb_scns(copy, bf->n_sections);
      //Initialises all sections in the duplicated array as being empty copies
      for (i = 0; i < copy->n_sections; i++) {
         copy->sections[i] = binfile_patch_initscncopy(copy, bf->sections[i]);
      }
   }
   //Duplicating the arrays of code and loaded sections
   for (i = 0; i < bf->n_loadscns; i++)
      binfile_addloadscn_s(copy, copy->sections[bf->loadscns[i]->scnid]);
   for (i = 0; i < bf->n_codescns; i++)
      binfile_addcodescn_s(copy, copy->sections[bf->codescns[i]->scnid]);

   /**\todo TODO (2015-03-30) I'm not sure if I will need to duplicate the array of segments. If not, remove what follows*/
   //Updating the array of sections by segments
   for (i = 0; i < copy->n_segments; i++) {
      uint16_t j;
      for (j = 0; j < copy->segments[i]->n_scns; j++) {
         copy->segments[i]->scns[j] = binfile_get_scn(copy,
               bf->segments[i]->scns[j]->scnid);
      }
   }

   //Duplicating the array of external libraries
   if (bf->n_extlibs > 0) {
      copy->extlibs = lc_malloc0(sizeof(*copy->extlibs) * bf->n_extlibs);
      copy->n_extlibs = bf->n_extlibs;
   }

   //Creating the structure establishing the correspondence between original entries and their moved or modified counterparts
   copy->entrycopies = hashtable_new(direct_hash, direct_equal);

   // Initialises the format specific internal structures
   copy->driver.binfile_patch_init_copy(copy);

   return copy;
}

/*
 * Finds the offset of a given entry in a section belonging to a patched file
 * \param bf Structure representing a binary file being patched
 * \param scnid Index of the section in the patched file
 * \param creatorscnid Index of the section this
 * \return Offset in the section at which the entry is found,
 * or UINT32_MAX if it does not belong to the section or \c scn or \c entry is NULL
 * */
uint32_t binfile_patch_find_entry_offset_in_scn(binfile_t* bf, uint16_t scnid,
      data_t* entry)
{
   /**\todo (2014-10-10) Part of this function may be moved to the format-specific files. So far I only
    * need it to retrieve the index of a string in a string section when referencing names in ELF.
    * Of course we may also be able to get rid of it if I find a better way to link the name of a label or section
    * to the corresponding string in a string section*/
   /**\todo TODO (2014-10-10) Maybe refactor this to avoid too much code duplication*/
   if (!binfile_patch_is_valid(bf) || !entry || (scnid >= bf->n_sections))
      return UINT32_MAX;
   uint32_t i = 0;
   uint32_t off = 0;
   binscn_t* scn = binfile_get_scn(bf, scnid);
   if (scn->type == SCNT_PATCHCOPY) {
      //Section has not been updated by the patching operation
      assert(scn->scnid < bf->creator->n_sections); //If the section has been correctly duplicated, this should not happen
      binscn_t* creatorscn = binfile_get_scn(bf->creator, scn->scnid);
      //We will simply scan the original until we find the correct entry
      while (i < creatorscn->n_entries) {
         if (creatorscn->entries[i] == entry)
            break;
         off += creatorscn->entries[i]->size;
         i++;
      }
      if (i == creatorscn->n_entries)
         off = UINT32_MAX; //There is an error, we scanned without finding the correct entry
   } else if (scn->scnid >= bf->creator->n_sections) {
      //Section is a new section: scanning its entries until we find the correct entry
      //We will simply scan the original until we find the correct entry
      while (i < scn->n_entries) {
         assert(scn->entries[i]); //All entries in a new section should be initialised
         if (scn->entries[i] == entry)
            break;
         off += scn->entries[i]->size;
         i++;
      }
      if (i == scn->n_entries)
         off = UINT32_MAX; //There is an error, we scanned without finding the correct entry
   } else {
      //Section has been updated by the patching operation: we will have to switch between both sections
      assert(scn->scnid < bf->creator->n_sections); //If the section has been correctly duplicated, this should not happen
      binscn_t* creatorscn = binfile_get_scn(bf->creator, scn->scnid);
      binscn_t* entryscn = data_get_section(entry); //Retrieves the section to which the entry belongs
      while (i < scn->n_entries) {
         data_t* entsrch = NULL; //Entry to compare with the entry we are looking for
         data_t* entsz = NULL;   //Entry whose size we will use
         if (scn->entries[i]) {
            entsz = scn->entries[i]; //Section has an updated entry
         } else {
            assert(i < creatorscn->n_entries);
            entsz = creatorscn->entries[i];
         }
         if ((i < creatorscn->n_entries) && (creatorscn == entryscn))
            entsrch = creatorscn->entries[i]; //The entry we are searching belongs to the original file: we will compare it with entries from it
         else
            entsrch = scn->entries[i]; //The entry we are searching belongs to the patched file or there is no entry in the original: comparing with patched entries
         if (entsrch == entry)
            break;
         off += data_get_size(entsz);
         i++;
      }
      if (i == scn->n_entries)
         off = UINT32_MAX; //There is an error, we scanned without finding the correct entry
   }
   return off;
}

/*
 * Adds a section to a binary file during patching.
 * \param bf Structure representing a binary file being patched
 * \param name Name of the section
 * \param address Address at which the section must be set. If set to -1, its address will be computed later
 * \param size Size in bytes of the section
 * \param type Type of the section
 * \return Pointer to the new section or NULL if bf is NULL or not in the process of being patched
 * */
static binscn_t* binfile_patch_add_scn(binfile_t* bf, char* name,
      int64_t address, uint64_t size, scntype_t type, unsigned int attrs)
{
   if (!binfile_patch_is_patching(bf))
      return NULL;
   //Generates the new section
   binscn_t* scn = binscn_new(bf, bf->n_sections, name, type, address, attrs);
   //Fills up its details
   scn->name = name;
   scn->type = type;
   scn->address = address;
   scn->size = size;
   scn->attrs = attrs;
   //Adds the section to the array of sections
   binfile_set_nb_scns(bf, bf->n_sections + 1);
   bf->sections[bf->n_sections - 1] = scn;

   /**\todo Maybe add a check here that the section is in an available interval*/

   //Flags the section as reordered if its address is set
   if (address != ADDRESS_ERROR)
      scn->attrs |= SCNA_PATCHREORDER;

   //Adds the section to the array of loaded and code sections
   if (attrs & SCNA_LOADED) {
      binfile_addloadscn_s(bf, scn);
      if (type == SCNT_CODE) {
         binfile_addcodescn_s(bf, scn);
      }
   }

   //Invokes the format specific function for additional handling
   bf->driver.binfile_patch_add_scn(bf, scn);
   return scn;
}

/*
 * Adds a code section to a binary file.
 * \param bf Structure representing a binary file being patched
 * \param name Name of the section. If left to NULL, the default name for code sections from the format will be used
 * \param address Address at which the section must be set. If set to -1, its address will be computed later
 * \param size Size in bytes of the section
 * \return Pointer to the new section or NULL if bf is NULL or not in the process of being patched
 * */
binscn_t* binfile_patch_add_code_scn(binfile_t* bf, char* name, int64_t address,
      uint64_t size)
{
   if (!name)
      name = (bf) ? bf->driver.codescnname : NULL;
   return binfile_patch_add_scn(bf, name, address, size, SCNT_CODE,
         (SCNA_EXE | SCNA_LOADED));
}

/*
 * Adds a code section at a fixed address to a binary file.
 * \param bf Structure representing a binary file being patched
 * \param name Name of the section. If left to NULL, the default name for code sections from the format will be used
 * \param address Address at which the section must be set. If set to -1, its address will be computed later
 * \param size Size in bytes of the section
 * \return Pointer to the new section or NULL if bf is NULL or not in the process of being patched
 * */
binscn_t* binfile_patch_add_code_scn_fixed_addr(binfile_t* bf, char* name,
      int64_t address, uint64_t size)
{
   if (!name)
      name = (bf) ? bf->driver.fixcodescnname : NULL;
   return binfile_patch_add_scn(bf, name, address, size, SCNT_CODE,
         (SCNA_EXE | SCNA_LOADED));
}

/*
 * Adds a data section to a binary file.
 * \param bf Structure representing a binary file being patched
 * \param name Name of the section. If left to NULL, the default name for data sections from the format will be used
 * \param address Address at which the section must be set. If set to -1, its address will be computed later
 * \param size Size in bytes of the section
 * \return Pointer to the new section or NULL if bf is NULL or not in the process of being patched
 * */
binscn_t* binfile_patch_add_data_scn(binfile_t* bf, char* name, int64_t address,
      uint64_t size)
{
   /**\todo TODO Maybe add another option to choose whether we want it read only*/
   if (!name)
      name = (bf) ? bf->driver.datascnname : NULL;
   return binfile_patch_add_scn(bf, name, address, size, SCNT_DATA,
         (SCNA_WRITE | SCNA_LOADED));
}

/**
 * Adds an entry to a section in a file being patched. This is an internal function used for code factorisation
 * and assumes that the file is valid as a file being patched. Oh see the assert anyway
 * \param bf Structure representing a binary file being patched
 * \param entry The entry to add
 * \param scnid The index of the section where to insert this entry
 * \return EXIT_SUCCESS if successful, error code if \c bf is NULL or not in the process of being patched
 * */
static int binfile_patch_add_entry_s(binfile_t* bf, data_t* entry,
      uint16_t scnid)
{
   assert(binfile_patch_is_patching(bf));
   binscn_t* scn = binfile_patch_get_scn_copy_s(bf, scnid);
   if (!scn)
      return ERR_BINARY_PATCHED_SECTION_NOT_CREATED; //Well something went wrong somewhere...

   //Adds the entry to the list of entries
   ADD_CELL_TO_ARRAY(scn->entries, scn->n_entries, entry)

   DBGLVL(2,
         FCTNAMEMSG("Adding new entry to section %s (%d) with value: ", binscn_get_name(scn), scn->scnid); data_fprint(entry, stderr); STDMSG("\n");)
   //Updates the size of the section
   if (scn->entrysz > 0) {
      //Fixed entry size: increasing by one
      scn->size += scn->entrysz;
   } else {
      //Otherwise, simply add the size of the entry
      scn->size += data_get_size(entry);
   }
   //Associates the entry to the section
   if (!data_get_section(entry))
      data_set_scn(entry, scn);

   return EXIT_SUCCESS;
}

/*
 * Adds an entry to a section in a file being patched.
 * \param bf Structure representing a binary file being patched
 * \param entry The entry to add
 * \param scnid The index of the section where to insert this entry
 * \return EXIT_SUCCESS if successful, error code if \c bf is NULL or not in the process of being patched
 * */
int binfile_patch_add_entry(binfile_t* bf, data_t* entry, uint16_t scnid)
{
   if (!binfile_patch_is_patching(bf))
      return ERR_BINARY_FILE_NOT_BEING_PATCHED;
   return binfile_patch_add_entry_s(bf, entry, scnid);
}

/*
 * Adds an entry to a string table in a section of a file being patched
 * \param bf Structure representing a binary file being patched
 * \param str The string to add
 * \param scnid The index of the section where to insert this entry
 * \return A pointer to the corresponding added entry if successful,
 * NULL if \c bf is NULL, not in the process of being patched, or if the section at scnid does not contain strings
 * */
data_t* binfile_patch_add_str_entry(binfile_t* bf, char* str, uint16_t scnid)
{
   if (!binfile_patch_is_patching(bf))
      return NULL;
   binscn_t* scn = binfile_patch_get_scn(bf, scnid);
   if (scn && scn->type == SCNT_STRING) {
      // The section exists and contains strings: we check if the string is not already present
      uint32_t i;
      for (i = 0; i < scn->n_entries; i++) {
         char* string = data_get_string(scn->entries[i]);
         if (string && str_equal(str, string))
            return binfile_patch_get_scn_entrycopy_s(bf, scnid, i); //Found an entry: returning its duplication
      }
      /**\todo TODO (2014-10-13) We should handle here the fact that a string can be included in another string
       * (don't use str_equal, but something like strstr with additional check to ensure both chain have the same end
       * This means also returning an offset to contain the offset with the beginning of the file (cf. binscn_lookup_entry_by_offset)*/
      // Creating the string entry
      data_t* entry = data_new_str(str);
      // Adding the entry to the section
      int res = binfile_patch_add_entry_s(bf, entry, scnid);
      if (ISERROR(res))
         return NULL;
      return entry;
   } else
      return NULL;
}

/*
 * Adds a label to a file being patched
 * \param bf Structure representing a binary file being patched
 * \param label Structure representing the label to add
 * \return EXIT_SUCCESS if successful, error code if \c bf is NULL or not in the process of being patched
 * */
int binfile_patch_add_label(binfile_t* bf, label_t* label)
{
   /**\todo (2014-09-29) Beware of not testing twice that the file is in the process of being patched (in the driver function)*/
   if (!binfile_patch_is_patching(bf))
      return ERR_BINARY_FILE_NOT_BEING_PATCHED;
   if (!label)
      return ERR_LIBASM_LABEL_MISSING;
   //Invoking the format specific function, as we don't know it's handled down below
   int out = bf->driver.binfile_patch_add_label(bf, label);
   if (!ISERROR(out)) {
      //Operation successful: adding the label to the array of labels in the file
      ADD_CELL_TO_ARRAY(bf->labels, bf->n_labels, label)
   }
   return out;
}

/*
 * Adds a relocation to a file being patched
 * \param bf Structure representing a binary file being patched
 * \param scnid Index of the section where to add the relocation (must be of type SCNT_RELOC)
 * \param label Structure representing the label to link to the relocation
 * \param addr Address of the object concerned by the relocation
 * \param dest Destination of the relocation
 * \param type Type of the relocation pointer
 * \param target_type Type of the destination of the relocation
 * \param reltype Format specific type of the relocation
 * \return TRUE if successful, FALSE if \c bf is NULL or not in the process of being patched
 * */
int binfile_patch_add_reloc(binfile_t* bf, uint16_t scnid, label_t* label,
      int64_t addr, void* dest, unsigned type, unsigned target_type,
      uint32_t reltype)
{
   if (!binfile_patch_is_patching(bf))
      return ERR_BINARY_FILE_NOT_BEING_PATCHED;
   if (!label)
      return ERR_LIBASM_LABEL_MISSING;
   //Retrieves a duplicate of the section where to add the relocation
   binscn_t* scn = binfile_patch_get_scn(bf, scnid);
   if (!scn)
      return ERR_BINARY_SECTION_NOT_FOUND;
   if (scn->type != SCNT_RELOC)
      return ERR_BINARY_BAD_SECTION_TYPE;
   //Creates the relocation
   binrel_t* newrel = binrel_new(label, addr, 0, dest, type, target_type,
         reltype);
   //Creates the entry containing the relocation
   data_t* newrelent = data_new(DATA_REL, newrel, scn->entrysz);
   //Stores the entry in the table of pointers
   hashtable_insert(bf->data_ptrs_by_target_data, dest, newrelent);
   //Adds the new entry to the relocation table
   ADD_CELL_TO_ARRAY(bf->relocs, bf->n_relocs, newrel)
   //Adds the entry to the file
   return binfile_patch_add_entry_s(bf, newrelent, scnid);
}

/*
 * Adds an external library to a file
 * \param bf A binary file. Must be in the process of being patched
 * \param extlibname Name of the file
 * \param priority Set to TRUE if it must have priority over the others, FALSE otherwise
 * \return EXIT_SUCCESS if successful, error code otherwise
 * */
int binfile_patch_add_ext_lib(binfile_t* bf, char* extlibname, boolean_t priority)
{
   if (!binfile_patch_is_valid(bf))
      return ERR_BINARY_FILE_NOT_BEING_PATCHED;
   return bf->driver.binfile_patch_add_ext_lib(bf, extlibname, priority);
}

/*
 * Adds an external function to a file
 * \param bf A binary file. Must be in the process of being patched
 * \param fctname Name of the function
 * \param libname Name of the library where it is defined
 * \param preload Set to TRUE if the function address is preloaded, FALSE otherwise (lazy binding)
 * \return A pointer to the function or NULL if an error occurred
 * */
pointer_t* binfile_patch_add_ext_fct(binfile_t* bf, char* fctname, char* libname,
      int preload)
{
   if (!binfile_patch_is_valid(bf))
      return NULL;
   return bf->driver.binfile_patch_add_ext_fct(bf, fctname, libname, preload);
}

/*
 * Renames an existing library in a file
 * \param bf A binary file. Must be in the process of being patched
 * \param oldname Name of the library
 * \param newname New name of the library
 * \return EXIT_SUCCESS if successful, error code otherwise
 * */
int binfile_patch_rename_ext_lib(binfile_t* bf, char* oldname, char* newname)
{
   if (!binfile_patch_is_valid(bf))
      return ERR_BINARY_FILE_NOT_BEING_PATCHED;
   if (!oldname || !newname)
      return ERR_COMMON_PARAMETER_MISSING;
   return bf->driver.binfile_patch_rename_ext_lib(bf, oldname, newname);
}

/*
 * Adds a list of instruction to a patched section. This is valid only if the section has been created during patching session
 * \param scn Section to which instructions must be added
 * \param insns List of instructions. Will be freed after addition
 * \param firstinsn Node containing the first instruction to add. Will be used if insns is NULL and lastinsn is not
 * \param lastinsn Node containing the last instruction to add. Will be used if insns is NULL and firstinsn is not
 * \return EXIT_SUCCESS if successful, error code if \c scn is NULL, belongs to a file not being patched, or exists in the original file
 * */
int binscn_patch_add_insns(binscn_t* scn, queue_t* insns, list_t* firstinsn,
      list_t* lastinsn)
{
   /**\note (2014-10-03) I'm using this function with an scn as input instead of the usual binfile+scnid because I expect
    * it will be used to add code to new sections, where it will be easier to manipulate the pointer the section than its id*/
   if (!scn)
      return ERR_BINARY_MISSING_SECTION;
   if (!binfile_patch_is_patching(scn->binfile))
      return ERR_BINARY_FILE_NOT_BEING_PATCHED; //File not being patched
   if (scn->scnid < scn->binfile->creator->n_sections)
      return ERR_BINARY_SECTION_ALREADY_EXISTING; //Section existing in the original file
   if (!queue_length(insns) && (!firstinsn || !lastinsn))
      return ERR_PATCH_INSERT_LIST_EMPTY; //File not being patched or existing in the original file
   list_t* first, *last;
   if (queue_length(insns) > 0) {
      first = queue_iterator(insns);
      last = queue_iterator_rev(insns);
   } else {
      first = firstinsn;
      last = lastinsn;
   }
   if (!scn->firstinsnseq)
      scn->firstinsnseq = first; //No existing instructions: the first in the section is the first in the added list
   else {
      //Instructions already exist in this section: we have to append those from the added list
      scn->lastinsnseq->next = first;
      first->prev = scn->lastinsnseq;
   }
   scn->lastinsnseq = last;
   scn->size += (insnlist_bitsize(insns, NULL, NULL) >> 3);
   lc_free(insns);
   return EXIT_SUCCESS;
}

/*
 * Attempts to create the patched file
 * \param bf Binary file
 * \param newfilename Name under which to save the file
 * \return EXIT_SUCCESS if the file could be created, error code otherwise
 * */
int binfile_patch_create_file(binfile_t* bf, char* newfilename)
{
   if (!binfile_patch_is_patching(bf))
      return ERR_BINARY_FILE_NOT_BEING_PATCHED;
   if (!newfilename)
      return ERR_COMMON_FILE_NAME_MISSING;

   //Attempts to open the file for writing
   int fd = open(newfilename, O_CREAT | O_WRONLY | O_TRUNC, 0777);
   if (fd < 0) {
      ERRMSG("Unable to create file %s\n", newfilename);
      return ERR_COMMON_UNABLE_TO_OPEN_FILE;
   }
   FILE* filestream = fdopen(fd, "w");
   if (!filestream) {
      ERRMSG("Unable to create file %s\n", newfilename);
      return ERR_COMMON_UNABLE_TO_OPEN_FILE;
   }
   //Sets the stream
   bf->filestream = filestream;
   //Sets the output name
   if (bf->filename)
      lc_free(bf->filename);
   bf->filename = lc_strdup(newfilename);
   /**\todo TODO (2015-04-02) Add here an invocation of a format-specific function that checks if the file can be patched and exits instantly
    * if not. Conversely, we can use the format-specific invocation of binfile_patch_init_copy at the end for that.
    * Also check somewhere that all the driver functions have been filled*/
   return EXIT_SUCCESS;
}

/*
 * Finalises a patched file
 * \param bf Binary file
 * \param spaces Queue of intervals representing the remaining empty spaces in the file
 * \return EXIT_SUCCESS if successful, error code otherwise
 * */
int binfile_patch_finalise(binfile_t* bf, queue_t* spaces)
{
   /**\todo TODO (2015-03-19) We may rebuild the spaces if spaces is NULL and print some kind of warning instead of exiting here. Maybe*/
   if (!binfile_patch_is_patching(bf))
      return ERR_BINARY_FILE_NOT_BEING_PATCHED;
   if (!spaces)
      return ERR_COMMON_PARAMETER_MISSING;
   if (!bf->filestream)
      return ERR_COMMON_FILE_STREAM_MISSING;
   //Ordering the arrays of sections
   if (bf->loadscns)
      SORT_SCNARRAY(bf->loadscns, bf->n_loadscns);
   if (bf->codescns)
      SORT_SCNARRAY(bf->codescns, bf->n_codescns);

   int out = bf->driver.binfile_patch_finalise(bf, spaces);
   if (ISERROR(out))
      return out; //Error during format-specific reordering

   DBGMSG(
         "Binary file %s finalised. Duplicating all moved entries and updating their addresses.\n",
         bf->filename)

   //Now duplicating all entries in the file and updating their addresses
   uint16_t i;
   /**\todo TODO (2015-04-13) I'm duplicating whole entries here, so basically I'm duplicating the entire file.
    * What I should be doing instead is creating entries of type DATA_NIL for all entries that were set to NULL,
    * and update all accesses to them to detect when an entry is of type DATA_NIL while not in a SCNT_ZERODATA section
    * and handle it as if it was NULL. But I want to set addresses on my entries*/
   for (i = 0; i < bf->n_sections; i++) {
      if (binfile_patch_is_scn_moved(bf, i)) {
         binscn_t* scn = binfile_get_scn(bf, i);
         //The section has been moved: duplicating its entries
         uint32_t j;
         maddr_t entryaddr = scn->address;
         for (j = 0; j < scn->n_entries; j++) {
            data_t* entry = binfile_patch_get_scn_entrycopy_s(bf, i, j);
            data_set_addr(entry, entryaddr);
            entryaddr += data_get_size(entry);
         }
         //Creates a copy for the pointers referencing this section
         queue_t* refs = hashtable_lookup_all(bf->creator->data_ptrs_by_target_scn,
               binscn_patch_get_origin(scn));
         FOREACH_INQUEUE(refs, iter) {
            data_t* copy = binfile_patch_get_entry_copy(bf,
                  GET_DATA_T(data_t*, iter));
            //Points the pointer of the copy to the patched section
            pointer_set_bscn_target(data_get_pointer(copy), scn);
            //References the copy in the hashtable
            hashtable_insert(bf->data_ptrs_by_target_scn, scn, copy);
            /**\todo TODO (2015-04-28) I'm not sure if this is necessary, but it is for coherence*/
         }
         queue_free(refs, NULL);
      }
   }
   //Updates the addresses of all pointers
   /**\todo TODO (2015-04-29) This may be a bottleneck. If it is, we could remove this and always use the target
    * addresses of pointers (instead of their own address), but it may cause problem with relative pointers, so...*/
   FOREACH_INHASHTABLE(bf->data_ptrs_by_target_data, iter) {
      pointer_upd_addr(data_get_ref_ptr(GET_DATA_T(data_t*, iter)));
   }

   bf->patch = BFP_FINALISED;
   return out;
}

/*
 * Writes a finalised binary file and closes the stream
 * \param bf A binary file. Must be finalised
 * \return EXIT_SUCCESS if successful, error code otherwise
 * */
int binfile_patch_write_file(binfile_t* bf)
{
   if (!binfile_patch_isfinalised(bf)) {
      ERRMSG(
            "Unable to write file %s: file is not finalised or has no creator\n",
            binfile_get_file_name(bf));
      return ERR_BINARY_FILE_NOT_BEING_PATCHED;
   }

   int out = bf->driver.binfile_patch_write_file(bf);
   if (ISERROR(out)) {
      ERRMSG(
            "Format-specific driver returned an error while writing file %s. File not created or incorrectly written.\n",
            bf->filename);
      return out;
   }

   //Closing the file stream
   fclose(bf->filestream);
   bf->filestream = NULL;
   /**\todo TODO (2015-04-21) Resetting it to NULL to allow the later invocation of binfile_free to succeed
    * Of course we may simply merge binfile_patch_terminate with this one*/

   return out;
}

/*
 * Terminates a file being patched
 * \param bf A binary file. Must be in the process of being patched or finalised
 * \return EXIT_SUCCESS if successful, error code otherwise
 * */
int binfile_patch_terminate(binfile_t* bf)
{
   if (!binfile_patch_is_valid(bf))
      return ERR_BINARY_FILE_NOT_BEING_PATCHED;
   binfile_free(bf);
   return EXIT_SUCCESS;
}
