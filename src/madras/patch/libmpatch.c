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
 * \file libmpatch.c
 * \brief This file contains functions used to patch a file: moving blocks, inserting or modifying instructions lists, inserting
 * labels, etc
 * */

/**\todo TODO (2014-07-14) I'm updating this file quickly in order to compile MAQAO with the libbin, but it will change a lot afterwards
 * as we actually *use* the libbin, instead of just, you know, replacing the variables with others so that it works
 * => This is taking too long. I'm beginning to do the modifications directly*/

/**\todo This file is slated for a major refactoring*/
/**\todo This file has been heavily updated to move some functions and structures from libmadras, and as a result, its code needs some cleanup
 * and can be improved. This should be ideally done before the refactoring*/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <inttypes.h>

#include "asmb_archinterface.h"
#include "libmpatch.h"

#define INSN_INLIST(L) GET_DATA_T(insn_t*,L)  /**< Simplification macro for retrieving an instruction in a list*/
#define NEWSTACKSIZE 1048576                /**< Size of the area reserved for a moved stack*/
/**\todo Don't hard code the value of NEWSTACKSIZE*/

#define INSN_MAX_BYTELEN 256  /**<Max size in bytes of the coding of an instruction*/
/**\todo TODO (2015-05-26) Define this someplace else (possibly in the arch_t structure, or the libmasm.h)*/

/**\todo Move these error codes in a common file => WRN_INSN_ATADDR_NOTFOUND is not used anymore there*/
#define ERR_NO_PLTSCN -1025                 /**< No .plt section found in elf file*/
#define WRN_INSN_ATADDR_NOTFOUND -15        /**< No instruction found at address*/

/**
 * Updates a variable containing a return code from a new value, only if the new value does not indicate a success
 * and the variable does not indicate an error
 * \param ERR Variable containing the error code
 * \param RES New error code value
 * \todo (2015-09-03) If this is needed elsewhere, move this macro to maqaoerrs.h. In this case,
 * also create macros for handling the various other cases (replace if ERR is not a warning, etc),
 * using explicit names for the macros.
 * */
#define UPDATE_ERRORCODE(ERR, RES) if (!ISERROR(ERR) && RES != EXIT_SUCCESS) ERR = RES;

/*
 * Returns the code of the last error encountered and resets it to EXIT_SUCCESS
 * \param pf A pointer to the patched file
 * */
int patchfile_get_last_error_code(patchfile_t* pf)
{
   if (!pf)
      return ERR_PATCH_NOT_INITIALISED;
   int errcode = pf->last_error_code;
   pf->last_error_code = EXIT_SUCCESS;
   return errcode;
}
/*
 * Sets the code of the last error encountered
 * \param pf A pointer to the patched file
 * \return The existing error code stored in \c pf or ERR_PATCH_NOT_INITIALISED if \c pf is NULL
 * */
int patchfile_set_last_error_code(patchfile_t* pf, int errcode)
{
   int out;
   if (!pf)
      return ERR_PATCH_NOT_INITIALISED;
   out = pf->last_error_code;
   pf->last_error_code = errcode;
   return out;
}

/*
 * Sets the code of the last error encountered and uses a default value if the error code given is 0
 * \param ed A pointer to the patched file
 * \param errcode The error code to set
 * \param dflterrcode The error code to use instead of \c errcode if errcode is EXIT_SUCCESS
 * \return The existing error code stored in \c ed or ERR_PATCH_NOT_INITIALISED if \c ed is NULL
 * */
int patchfile_transfer_last_error_code(patchfile_t* pf, int errcode,
      int dflterrcode)
{
   int out;
   if (errcode != EXIT_SUCCESS)
      out = patchfile_set_last_error_code(pf, errcode);
   else
      out = patchfile_set_last_error_code(pf, dflterrcode);
   return out;
}
////NEW VARIABLES USED BY PATCHER REFACTORING
#define DIRJMP_SAFETY 0x100 /**<Safety distance used when checking whether a given offset can be reached by a direct jump*/
#define MEMREL_SAFETY 0x100 /**<Safety distance used when checking whether a given offset can be reached using a memory relative operand (RIP)*/
/**\todo TODO (2015-03-13) I may have to use different safety distances for direct jumps, depending on where I'm using them*/

//The following definitions are used for storing and retrieving info in the flags associated to an interval representing an empty space in the file
/**\todo TODO (2015-03-10) I'm defining these macros here so that the flag member of interval_t does not become too binfile-specific
 * and can be used for other purposes. If it happens that only binfile and the patcher use it, we could get rid of those macros
 * and replace the flag member in interval_t with bit fields for reach, reserved, and used*/
#define REACH_SZ 2      /**<Size of the REACH sub field in the flag member of the interval_t structure*/
#define REACH_POS 0     /**<Position of the REACH sub field in the flag member of the interval_t structure*/
#define RESERVED_SZ 2   /**<Size of the RESERVED sub field in the flag member of the interval_t structure*/
#define RESERVED_POS 2  /**<Position of the RESERVED sub field in the flag member of the interval_t structure*/
#define USED_SZ 2       /**<Size of the USED sub field in the flag member of the interval_t structure*/
#define USED_POS 4      /**<Position of the USED sub field in the flag member of the interval_t structure*/

#define INTERVALFLAG_GETELEMENT(FLAG, ELEMENT) FLAG_GETSUBVALUE(FLAG, MACRO_CONCAT(ELEMENT, _POS), MACRO_CONCAT(ELEMENT, _SZ))

#define INTERVALFLAG_UPDELEMENT(FLAG, VALUE, ELEMENT) FLAG_UPDSUBVALUE(FLAG, VALUE, MACRO_CONCAT(ELEMENT, _POS), MACRO_CONCAT(ELEMENT, _SZ))

/**
 * Flags intended for characterising an interval of addresses with regard to how it can be reached from executable code in a binary file,
 * or for the type of sections for which it has been reserved or is used.
 * */
#define INTERVAL_NOFLAG          0  /**<No special flag on this interval*/
#define INTERVAL_DIRECTBRANCH    1  /**<Interval can be reached with a direct branch, or is reserved or used for code */
#define INTERVAL_REFERENCE       2  /**<Interval can be reached with a data reference, or is reserved or used for data */
#define INTERVAL_INDIRECTBRANCH  3  /**<Interval is used for code reached with an indirect branch (do NOT use for reachable or reserved)*/

///**
// * Type affected to an interval of addresses in a binary file
// * */
//typedef enum intervaltype_e {
//   INTERVAL_TYPE_NONE = 0,          /**<No special type*/
//   INTERVAL_RESERVED_DIRECTBRANCH,  /**<Addresses reserved for storing instructions reachable by direct branches*/
//   INTERVAL_RESERVED_REFERENCE,     /**<Addresses reserved for storing data reachable by references*/
//   INTERVAL_USED_DIRECTBRANCH,      /**<Addresses used for storing instructions reachable by direct branches*/
//   INTERVAL_USED_REFERENCE,         /**<Addresses used for storing data reachable by references*/
//   INTERVAL_TYPE_MAX                /**<Maximum number of interval types*/
//} intervaltype_t;

////////////NEW FUNCTIONS ADDED BY PATCHER REFACTORING
/**\todo TODO (2014-11-05) Reorder those new functions and possibly move some of them to patchutils.c once this is over*/
#ifndef NDEBUG
//Functions intended for debug printing

static void movedblock_fprint(movedblock_t* mb, FILE* stream)
{
   if (!mb)
      return;
   fprintf(stream,
         "block between addresses %#"PRIx64" and %#"PRIx64" (max size %"PRIu64")",
         insn_get_addr(GET_DATA_T(insn_t*, mb->firstinsn)),
         insn_get_end_addr(GET_DATA_T(insn_t*, mb->lastinsn)), mb->maxsize);
}

/**
 * \param addrinsn Instruction from which we retrieve the address of the instruction to print
 * */
static void patcher_insn_fprint_withaddr_nocr(insn_t* insn, insn_t* addrinsn,
      FILE* stream, int annotate)
{
   assert(insn && addrinsn && stream);
   char bufc[128];
   if (insn_get_coding(insn))
      bitvector_hexprint(insn_get_coding(insn), bufc, sizeof(bufc), " ");
   else
      bufc[0] = '\0';
   fprintf(stream, "\t%#"PRIx64":%s\t", insn_get_addr(addrinsn), bufc);
   insn_fprint(insn, stderr);
   fprintf(stream, "(%p)", addrinsn);
   oprnd_t* refop = insn_lookup_ref_oprnd(insn);
   if (refop != NULL) {
      pointer_t* ptr = oprnd_get_refptr(refop);
      insn_t* dest;
      data_t* refd;
      switch (pointer_get_target_type(ptr)) {
      case TARGET_INSN:
         dest = pointer_get_insn_target(ptr);
         fprintf(stream, "\t->%#"PRIx64":", insn_get_addr(dest));
         insn_fprint(dest, stream);
         fprintf(stream, "(%p)", dest);
         break;
      case TARGET_DATA:
         refd = pointer_get_data_target(ptr);
         fprintf(stream, "\t# %#"PRIx64":", data_get_addr(refd));
         data_fprint(refd, stream);
         fprintf(stream, "(%p)", refd);
         break;
      }
   }

   fprintf(stream, " %s %s %s %s",
         (insn_check_annotate(insn, A_PATCHMOV) || (annotate & A_PATCHMOV)) ?
               "M" : " ",
         (insn_check_annotate(insn, A_PATCHNEW) || (annotate & A_PATCHNEW)) ?
               "N" : " ",
         (insn_check_annotate(insn, A_PATCHDEL) || (annotate & A_PATCHDEL)) ?
               "D" : " ",
         (insn_check_annotate(insn, A_PATCHUPD) || (annotate & A_PATCHUPD)) ?
               "U" : " ");
   if (insn_get_addr(insn) != insn_get_addr(addrinsn)
         && (insn_check_annotate(insn, A_PATCHMOV) || (annotate & A_PATCHMOV))) {
      fprintf(stream, " [<-%#"PRIx64"]", insn_get_addr(insn));
   }
}

/**
 * Prints an instruction belonging to a patched file
 * */
static void patcher_insn_fprint_nocr(insn_t* insn, FILE* stream, int annotate)
{
   patcher_insn_fprint_withaddr_nocr(insn, insn, stream, annotate);
}
/**
 * \param addrinsn Instruction from which we retrieve the address of the instruction to print
 * */
static void patcher_insn_fprint_withaddr(insn_t* insn, insn_t* addrinsn,
      FILE* stream, int annotate)
{
   patcher_insn_fprint_withaddr_nocr(insn, addrinsn, stream, annotate);
   fprintf(stream, "\n");
}

/**
 * Prints an instruction belonging to a patched file
 * */
static void patcher_insn_fprint(insn_t* insn, FILE* stream, int annotate)
{
   patcher_insn_fprint_withaddr(insn, insn, stream, annotate);
}
#endif

/*
 * Checks the reachable status of an interval
 * \param interval The interval structure
 * \param reachable Flag specifying the reachable status to check for
 * \return TRUE if the interval can be reached as described by \c reachable, FALSE otherwise
 * */
static int patcher_interval_checkreachable(interval_t* interval,
      uint8_t reachable)
{
   uint8_t reach = INTERVALFLAG_GETELEMENT(interval_get_flag(interval), REACH);
   return (!reachable || (reach & reachable)) ? TRUE : FALSE;
}

/*
 * Adds a flag characterising the reachable status of an interval
 * \param interval The interval structure
 * \param reachable Flag specifying the reachable status to flag the interval with
 * */
static void patcher_interval_addreachable(interval_t* interval,
      uint8_t reachable)
{
   uint8_t flag = interval_get_flag(interval);
   uint8_t reach = INTERVALFLAG_GETELEMENT(flag, REACH);
   interval_set_flag(interval,
         INTERVALFLAG_UPDELEMENT(flag, (reach | reachable), REACH));
   DBGLVL(1,
         FCTNAMEMSG0("Flagging interval "); interval_fprint(interval, stderr); fprintf(stderr, " as reachable with %s\n",(reachable==INTERVAL_DIRECTBRANCH)?"branches":"references");)
}

/*
 * Retrieves the reserved status of an interval
 * \param interval The interval structure
 * \return The reserved status of the interval
 * */
static int patcher_interval_getreserved(interval_t* interval)
{
   return INTERVALFLAG_GETELEMENT(interval_get_flag(interval), RESERVED);
}

/*
 * Sets the reserved status of an interval
 * \param interval The interval structure
 * \param reserved Flag specifying the reserved status to flag the interval with
 * */
static void patcher_interval_setreserved(interval_t* interval, uint8_t reserved)
{
   interval_set_flag(interval,
         INTERVALFLAG_UPDELEMENT(interval_get_flag(interval), reserved,
               RESERVED));
   DBGLVL(1,
         FCTNAMEMSG0("Flagging interval "); interval_fprint(interval, stderr); fprintf(stderr, " as reserved for %s\n",(reserved==INTERVAL_DIRECTBRANCH)?"branches":"references");)
}

/*
 * Retrieves the used status of an interval
 * \param interval The interval structure
 * \return The used status of the interval
 * */
static int patcher_interval_getused(interval_t* interval)
{
   return INTERVALFLAG_GETELEMENT(interval_get_flag(interval), USED);
}

/**
 * Sets the used status of an interval
 * \param interval The interval structure
 * \param used Flag specifying the used status to flag the interval with
 * */
static void patcher_interval_setused(interval_t* interval, uint8_t used)
{
   interval_set_flag(interval,
         INTERVALFLAG_UPDELEMENT(interval_get_flag(interval), used, USED));
}

/**
 * Prints an interval, along with its flags as we use them in the patcher (reachable/reserved/used)
 * */
static void patcher_interval_fprint(interval_t* interval, FILE* stream)
{
   assert(interval && stream);
   interval_fprint(interval, stderr);
   uint8_t flag = interval_get_flag(interval);
   fprintf(stream, " (reach: %s %s",
         (INTERVALFLAG_GETELEMENT(flag, REACH) & INTERVAL_DIRECTBRANCH) ?
               "br" : "",
         (INTERVALFLAG_GETELEMENT(flag, REACH) & INTERVAL_REFERENCE) ?
               "ref" : "");
   fprintf(stream, " - reserved: %s",
         (INTERVALFLAG_GETELEMENT(flag, RESERVED) == INTERVAL_DIRECTBRANCH) ?
               "br" : "ref");
   char* used;
   if (INTERVALFLAG_GETELEMENT(flag, USED) == INTERVAL_DIRECTBRANCH)
      used = "br";
   else if (INTERVALFLAG_GETELEMENT(flag, USED) == INTERVAL_REFERENCE)
      used = "ref";
   else if (INTERVALFLAG_GETELEMENT(flag, USED) == INTERVAL_INDIRECTBRANCH)
      used = "indbr";
   else
      used = "";
   fprintf(stream, " - used: %s )", used);
}

/**
 * Splits an existing interval around a given address. A new interval beginning at the address
 * of the interval and ending at the given address will be created and added before the given
 * interval, whose address will be set to the given address
 * \param pf Patched file. Its empty spaces must have already been computed
 * \param iter List element in the queue of intervals containing the interval to split
 * \param splitaddr Address around which to split
 * \return The new interval
 * */
static interval_t* patchfile_splitemptyspace(patchfile_t* pf, list_t* iter,
      int64_t splitaddr)
{
   assert(
         pf && iter && splitaddr > interval_get_addr(GET_DATA_T(interval_t*, iter)) && splitaddr < interval_get_end_addr(GET_DATA_T(interval_t*, iter)));

   //Splitting the interval and retrieving the interval starting at the original address
   interval_t* part = interval_split(GET_DATA_T(interval_t*, iter), splitaddr);
   //Insert the new interval before the one we found
   queue_insertbefore(pf->emptyspaces, iter, part);
   return part;
}

/**
 * Flags empty intervals as reachable from the code using either branch instructions or data references
 * \param pf Patched file. Its empty spaces must have already been computed
 * \param start Lowest address to flag. If this address is inside an interval, it will be split.
 * \param end Highest address to flag. If this address is inside an interval, it will be split.
 * \param reachable Flag specifying how the addresses are reachable from original code (using INTERVAL_* flags)
 * */
static void patchfile_flagemptyspaces_reachable(patchfile_t* pf, int64_t start,
      int64_t end, uint8_t reachable)
{
   assert(pf);
   if (start >= end || queue_length(pf->emptyspaces) == 0)
      return;  //Start address higher than the end address or no empty spaces
   list_t* iter = queue_iterator(pf->emptyspaces), *first, *last;

   //Finds the first interval with an ending address superior to the starting address
   while (iter) {
      if (start < interval_get_end_addr(GET_DATA_T(interval_t*, iter)))
         break;
      iter = iter->next;
   }
   if (!iter)
      return; //Lowest address to flag is set after the end of the last empty space: nothing can be done
   //Checking if the start address is in the middle of the interval and splitting it if so
   if (start > interval_get_addr(GET_DATA_T(interval_t*, iter))) {
      //We have to start in the middle of the interval: we split it and insert a new interval before
      patchfile_splitemptyspace(pf, iter, start);
   }
   first = iter;  //Storing the node of the first interval to be flagged
   //Now looking from the last interval to flag
   while (iter) {
      if (end <= interval_get_end_addr(GET_DATA_T(interval_t*, iter)))
         break;
      iter = iter->next;
   }
   if (!iter) {
      //Highest address to flag is set after the end of the last empty space: all remaining intervals will be flagged
      last = NULL;
//   } else if (end <= interval_get_addr(GET_DATA_T(interval_t*, iter))) {
//      //Highest address to flag falls between two intervals: the last interval to flag is the previous one
//      last = iter;
   } else {
      if (end < interval_get_end_addr(GET_DATA_T(interval_t*, iter))) {
         //The end address is in the middle of the interval: we split it
         //Creating an interval running from the beginning of the found interval to the ending address
         patchfile_splitemptyspace(pf, iter, end);
      }
      last = iter; //Stores the node containing the interval where to stop flagging
   }
   //Now finally flagging the intervals
   for (iter = first; iter != last; iter = iter->next)
      patcher_interval_addreachable(GET_DATA_T(interval_t*, iter), reachable);
}

/*
 * Flags the intervals in a file depending on their reachable status.
 * \param pf File being patched
 * \param reachable Flag specifying how this interval is reachable from the original code (using INTERVAL_* flags).
 * The same flag will be set to mark that an interval is reserved
 * \param override If set to TRUE, allows to flag intervals already flagged as something else than INTERVAL_NONE.
 * Otherwise, those intervals will not be flagged
 * \param maxsize Maximum size of intervals to flag. If the size is reached in the middle of an interval,
 * it will be split. Can be set to UINT64_MAX if all available size is to be flagged
 * \return The total size of the flagged empty spaces
 * */
static uint64_t patchfile_reserveemptyspaces(patchfile_t* pf, unsigned flag,
      int override, uint64_t maxsize)
{
   /**\todo TODO (2015-02-27) This function is dependent on how well the patcher feeds it, so it might be moved
    * to libmpatch.c. Also, the override parameter may not be needed afterwards, as I only think I will need it
    * later if I want to flag intervals with INTERVAL_USED_*. If I end up not using this flag, we can remove the
    * override or handle it differently (because I don't want to reflag intervals already reserved for something)
    * => Done the moving to libmpatch.c (I don't know when).
    * => (2015-05-28) The comment on the override parameters still stands*/
   assert(pf);
   uint64_t flaggedsz = 0;
   FOREACH_INQUEUE(pf->emptyspaces, iter) {
      DBGLVL(2,
            FCTNAMEMSG0("Checking interval "); patcher_interval_fprint(GET_DATA_T(interval_t*, iter), stderr); STDMSG("\n");)
      if ((patcher_interval_checkreachable(GET_DATA_T(interval_t*, iter), flag))
            && (override
                  || (patcher_interval_getreserved(
                        GET_DATA_T(interval_t*, iter)) == INTERVAL_NOFLAG))
            && (patcher_interval_getused(GET_DATA_T(interval_t*, iter))
                  == INTERVAL_NOFLAG) && (flaggedsz < maxsize)) {
         //Interval is reachable as specified and not already flagged for something else or they can be overriden
         uint64_t intervalsz = interval_get_size(GET_DATA_T(interval_t*, iter));
         if ((intervalsz == UINT64_MAX)
               || ((flaggedsz + intervalsz) > maxsize)) {
            //This interval is larger than needed for filling the maximal size: we have to split it
            int64_t splitaddr = interval_get_addr(
                  GET_DATA_T(interval_t*, iter)) + maxsize - flaggedsz;
            interval_t* newint = patchfile_splitemptyspace(pf, iter, splitaddr);
            //Updates size of the current interval (it is the beginning of the split interval)
            intervalsz = interval_get_size(newint);
            /**\todo (2015-02-27) Actually this new size is maxsize - flaggedsz so we could use this instead.
             * I guess, I'm beginning to be a bit hazy on this programming stuff thing I'm supposed to know about*/
            //Sets the flag on the interval
            patcher_interval_setreserved(newint, flag);
         } else {
            //Sets the flag on the interval
            patcher_interval_setreserved(GET_DATA_T(interval_t*, iter), flag);
         }
         //Updates the size of the flagged space
         flaggedsz += intervalsz;
      }
   }
   return flaggedsz;
}

/**
 * Function computing the estimated size needed for patched code, based on the size of existing code
 * \param codesz Size of existing code
 * \return Estimated size needed for patched code
 * \todo TODO (2015-02-27) This is used in available_size_isok and patchfile_init. See the todo comment
 * in available_size_isok on the heuristics we are using. Change this function to tweak the heuristic
 * (this may involve retrieving more information about the file we are patching, such as the number of
 * basic blocks)
 * */
static uint64_t get_estimated_patchcode_size(uint64_t codesz)
{
   return 2 * codesz;
}

/**
 * Function computing the estimated size needed for patched referenced sections,
 * based on the size of those sections
 * \param refsz Size of referenced sections
 * \return Estimated size needed for patched referenced sections
 * \todo TODO (2015-02-27) This is used in available_size_isok and patchfile_init. See the todo comment
 * in available_size_isok on the heuristics we are using. Change this function to tweak the heuristic
 * (this may involve retrieving more information about the file we are patching, such as binary format
 * specific data about those referenced sections and how much they are likely to grow during a patching
 * session)
 * */
static uint64_t get_estimated_patchrefs_size(uint64_t refssz)
{
   return 2 * refssz;
}

/**
 * Function estimating whether the empty spaces reachable with direct branches and memory references form
 * the original code are large enough to contain modifications.
 * \warning (2015-02-27) This is an completely experimental heuristic function that will need some tweaking in the future.
 * Currently I am using as a guideline the estimate that the displaced code will be twice the size of the original
 * code and that the displaced sections referenced by instructions will be twice their original sizes.
 * \todo TODO (2015-02-27) Update this function to make it easier to tweak. This involves possibly declaring
 * the multipliers as defines, or even create functions computing the estimated displaced size from the original.
 * They could involve the number of basic blocks for instance if they are known in advance
 * \param codesz Total size of sections containing executable code
 * \param refssz Total size of sections referenced by instructions
 * \param reachable_codesz Size of the empty space reachable from the original code using direct branches
 * \param reachable_refssz Size of the empty space reachable from the original code using memory references
 * \param reachable_bothsz Size of the empty space reachable from both direct branches and memory references from the original code
 * \return TRUE if the estimated size of patched code and modified references is less than or equal to the empty
 * space available, FALSE otherwise
 * */
static int available_size_isok(uint64_t codesz, uint64_t refssz,
      uint64_t reachable_codesz, uint64_t reachable_refssz,
      uint64_t reachable_bothsz)
{
   /**\todo (2015-02-27) Change this to tweak this heuristic*/
   uint64_t estimated_patchcodesz = get_estimated_patchcode_size(codesz); //Estimation of the size needed for displaced code
   uint64_t estimated_patchrefssz = get_estimated_patchrefs_size(refssz); //Estimation of the size needed for displaced referenced sections

   // Checks if the estimated size can fit into the non overlapping empty spaces
   if ((estimated_patchcodesz <= (reachable_codesz - reachable_bothsz))
         && (estimated_patchrefssz <= (reachable_refssz - reachable_bothsz)))
      return TRUE;
   // Checks if the estimated sizes can fit into the whole empty spaces, including overlapping
   if ((estimated_patchcodesz + estimated_patchrefssz)
         <= (reachable_codesz + reachable_refssz - reachable_bothsz))
      return TRUE;
   //Estimated size used by displaced code and/or section don't fit into the available spaces
   return FALSE;
}

/**
 * Frees a structure representing an instruction being patched
 * \param p Structure establishing the correspondence between an initial and a patched instruction
 * */
static void patchinsn_free(void* p)
{
   patchinsn_t *pi = p;
   assert(pi);
   insn_free(pi->patched);
   lc_free(pi);
}

/**
 * Adds a patchinsn_t instruction to a list and updates the sequence accordingly
 * \param list Queue of patchinsn_t structures
 * \param pi patchinsn_t structure to add to the list
 * */
static void add_patchinsn_to_list(queue_t* list, patchinsn_t* pi)
{
   assert(list && pi);
   queue_add_tail(list, pi);
   pi->seq = queue_iterator_rev(list);
}

/**
 * Initialises a structure representing an instruction being patched.
 * By default, the patched instruction is considered identical to the original except for its address.
 * If the original contains a pointer however, the full instruction will be copied to allow for updates
 * \param insn The original instruction
 * \param newinsn The new instruction. If equal to insn, a partial or full copy of the original will be made
 * \return A new patchinsn_t structure allowing the correspondence between an original instruction and its copy,
 * */
static patchinsn_t* patchinsn_new(insn_t* insn, insn_t* newinsn)
{
   assert(insn || newinsn); //Either one can be NULL, but both makes no sense
   patchinsn_t* out = lc_malloc0(sizeof(*out));
   out->origin = insn;
//   if (annotate == A_PATCHNEW) {
//      out->patched = insn;
//      insn_add_annotate(insn, annotate);
//   } else {
   if (insn == newinsn) { //The new instruction must be a copy of the original: deciding if we perform a partial or full copy
      /**\todo TODO (2014-10-31) I'm not quite sure of what we will be needing here. Maybe the copy will have to occur in all cases
       * Otherwise I may need to add a function for copying an already existing instruction in case we initialised the instruction
       * as blank then later on need to update it (e.g. moving an instruction because it's in a basic block where an instruction has
       * to be modified, then later having a modification targeting this instruction)*/
      if (insn_lookup_ref_oprnd(insn)) {
         //Instruction contains a pointer or is flagged as modified: full copy as we will need to update it
         out->patched = insn_copy(insn);
         DBGMSGLVL(2,
               "Creating full copy of instruction at address %#"PRIx64" (%p): %p\n",
               insn_get_addr(insn), insn, out->patched);
      } else {
         //Blank initialisation of the instruction as we will only need its address and annotation
         out->patched = insn_new(insn_get_arch(insn));
         insn_set_addr(out->patched, insn_get_addr(insn));
         insn_set_annotate(out->patched, insn_get_annotate(insn));
         insn_set_opcode(out->patched, BAD_INSN_CODE); //To distinguish the instructions that have only been partially copied
         /**\todo TODO (2015-01-12) I'm using the "bad insn" code to identify copies. To be seen what is more efficient - an additional
          * field in patchinsn_t or something else, assuming we keep the partial copy mechanism. Boy, am I beginning to be fed up with this*/
         /**\todo TODO (2015-03-17) We could compute the maximal size of the instruction list in the moved block here to have only a single pass*/
         DBGMSGLVL(2,
               "Creating partial copy of instruction at address %#"PRIx64" (%p): %p\n",
               insn_get_addr(insn), insn, out->patched);
      } //If the instruction is flagged as deleted, we leave it at blank
   } else
      out->patched = newinsn;

   return out;
}

/**
 * Checks if an instruction has been anotated as being modified by a patching operation
 * \param insn The instruction
 * \return TRUE if the instruction is annotated with either A_PATCHMOV, A_PATCHUPD, A_PATCHDEL or A_PATCHMOD, FALSE otherwise
 * */
static int insn_ispatched(insn_t* insn)
{
   assert(insn);
   return insn_check_annotate(insn, A_PATCHMOV | A_PATCHUPD | A_PATCHDEL); //|A_PATCHMOD
}

/**
 * Links a jump instruction to its destination and updates the branch table
 * \param pf Pointer to the structure holding the disassembled file
 * \param jmp The jump instruction to link
 * \param dest The destination instruction
 * \param ptr The pointer operand from \c jmp (can be NULL)
 * */
void patchfile_setbranch(patchfile_t* pf, insn_t* jmp, insn_t* dest,
      pointer_t* ptr)
{
   /**\todo TODO (2015-04-14) I'm restoring this function so that I can store all branches in the hashtable.
    * This means that the driver functions generating list of instructions corresponding to a jump must now also return
    * the pointer to the jump instruction itself, as it used to be before. If in the end the hashtable is not useful
    * or does not bring any speedup when updating branches, simply remove the use of this function and the return
    * of the branch instruction as well from the driver functions*/
   if (!pf || !jmp || !dest)
      return;
///** \todo Check this function is used everywhere in this file*/
   /*Retrieves the old destination of the branch*/
   insn_t* olddest = insn_get_branch(jmp);
   if (olddest == dest) {
      //The branch was already set: simply checking if it is present in the table
      if (hashtable_lookup_elt(pf->newbranches, dest, jmp)) {
         DBGLVL(2,
               FCTNAMEMSG0("Instruction "); patcher_insn_fprint_nocr(jmp,stderr, A_NA); STDMSG(" was already in the branches table\n");)
         return; //Nothing to do
      }
   } else {
      //Branch not set
      DBGLVL(1,
            FCTNAMEMSG0("Linking instruction "); patcher_insn_fprint_nocr(jmp,stderr, A_NA); STDMSG(" to instruction ");patcher_insn_fprint_nocr(dest, stderr, A_NA); STDMSG("\n"));
#ifndef NDEBUG //To avoid compilation warnings when not in debug mode
      int res =
#endif
            hashtable_remove_elt(pf->newbranches, olddest, jmp);
      DBGLVL(2,
            FCTNAMEMSG0("Instruction "); patcher_insn_fprint_nocr(jmp,stderr, A_NA); STDMSG(" was %s branches table\n", (res==1)?"removed from":"not found in");)
      //Points the jmp instruction to the destination
      if (ptr)
         pointer_set_insn_target(ptr, dest);
      else
         insn_set_branch(jmp, dest);
   }
   //Stores this jump into the branch hashtable
   hashtable_insert(pf->newbranches, dest, jmp);
}

/**
 * Creates a patchinsn_t structure in a patched file, or return an existing one
 * \param pf The patched file
 * \param insn The instruction
 * \param newinsn The instruction replacing the instruction (can be NULL). If identical to \c insn, a copy will be created
 * \param mb The moved block to which the instruction belong (can be NULL). If not NULL, newinsn will be linked to it through the movedblocksbyinsns table
 * \return Either a new patchedinsn structure about the instruction or an existing one if the instruction was already flagged
 * */
static patchinsn_t* patchfile_createpatchinsn(patchfile_t* pf, insn_t* insn,
      insn_t* newinsn, movedblock_t* mb)
{
   assert(pf);
   patchinsn_t* pi = NULL;
   if (insn) {
      //Existing original instruction: attempting to retrieve a patchinsn_t instruction already based on it
      pi = hashtable_lookup(pf->patchedinsns, insn);
      /**\todo TODO (2015-01-07) We may need to add a sanity check here to detect instructions whose patchinsn_t have already been created
       * but for another reason (right now I don't see how it could happen, but I'm confident it will when I last expect it)
       * => (2015-05-28) Normally by now all patchinsn_t are created using this function, so they should be present in the table, except
       * those without an original instruction, of course.*/
      if (!pi) {
         //No patchinsn_t structure had yet been created for this instruction: we create it
         pi = patchinsn_new(insn, newinsn);
         hashtable_insert(pf->patchedinsns, insn, pi);

         //Check if this instruction was not already in the newbranches table and updating the branches to it
         queue_t* existingnewbranches = hashtable_lookup_all(pf->newbranches,
               insn);
         FOREACH_INQUEUE(existingnewbranches, iter) {
            //Reassigns all new branches to point to the copy of the instruction
            patchfile_setbranch(pf, GET_DATA_T(insn_t*, iter), pi->patched,
                  NULL);
         }
         queue_free(existingnewbranches, NULL);
      }
   } else {
      //Creating a patchinsn_t without an original instruction (new instruction): simply creating the structure
      pi = patchinsn_new(insn, newinsn);
      //Checking if the new instruction references an inserted global variable
      pointer_t* refptr = oprnd_get_memrel_pointer(insn_lookup_ref_oprnd(newinsn));
      if (refptr) {
         //The instruction contains a memory relative pointer
         data_t* refdata = pointer_get_data_target(refptr);
         if (refdata && !data_get_section(refdata)) {
            //The pointer references a data object not associated to a section: we assume it's a new variable inserted by the patcher
            //Associating the targeted data to the new instruction
            hashtable_insert(pf->insnrefs, refdata, newinsn);
         } else {
            //The pointer references a data object associated to a section: it references an existing variable
            //Retrieving the copy (if it is not already one)
            data_t* copyref = binfile_patch_get_entry_copy(pf->patchbin, refdata);
            //Associated the copy to the new instruction
            hashtable_insert(pf->insnrefs, copyref, newinsn);
         }
      }
   }

   if (newinsn) {
      //New instruction

      //Trying to associate it to a moved block
      //Retrieves the moved block associated to the original instruction if it exists and no block was passed as parameter
      if (insn && !mb)
         mb = hashtable_lookup(pf->movedblocksbyinsns, insn);

      //Associates the new instruction to the moved block if one was found or passed
      if (mb)
         hashtable_insert(pf->movedblocksbyinsns, newinsn, mb);

//      //Detecting if the new instruction is a branch and enforcing its target to be present in the newbranches table
//      //if (insn && insn_is_direct_branch(newinsn) && insn_is_direct_branch(insn) && insn_get_branch(newinsn) != insn_get_branch(insn)) {
//      if (insn_is_direct_branch(newinsn) && insn_get_branch(newinsn) != insn_get_branch(insn)) {
//         /**\todo TODO (2015-06-03) I'm adding this on the fly to make madras_set_branch_target work, event though I would really
//          * like not to use this API function any more afterward, but I need it for now for non-regression tests. I'm not
//          * quite sure this will not add wrong entries to the newbranches table, for instance by inserting an branches multiple times
//          * (this would be a lesser evil, but there are probably other, more dangerous, possible bugs. I'm really fed up today.*/
//         //The new instruction is a direct branch, but the new instruction does not target the same instruction as the original (or the original is NULL)
//         insn_t* newtarget = insn_get_branch(newinsn);
//         if (!insn_check_annotate(newtarget, A_PATCHNEW)) {
//            DBGLVL(2, FCTNAMEMSG0("Creating new patched instruction for the destination of new instruction ");
//               patcher_insn_fprint(newinsn, stderr, A_NA);STDMSG("\n"));
//            //Target is not a new instruction: creating a patchinsn for it
//            patchinsn_t* newpi = patchfile_createpatchinsn(pf, newtarget, newtarget, NULL);
//            newtarget = newpi->patched;
//         }
//         //Links the new branch to the new instruction. It may have been already correctly linked, but now it will be added to the newbranches table
//         patchfile_setbranch(pf, newinsn, newtarget, NULL);
//      }
   }
//   insn_add_annotate(insn, annotate); /**\todo (2014-10-31) Maybe add some test to check if the annotate is not already present when already existing*/

   return pi;
}

#if THOUGHT_I_WOULD_NEED_IT_BUT_I_NEVER_USED_IT
/**
 * Updates the coding and address of a patched instruction.
 * \param patchinsn The patchinsn_t structure representing the patched instruction
 * */
static void patchinsn_update(patchinsn_t* patchinsn) {

}

/**
 * Retrieves the size in bytes of a patched instruction.
 * \param patchinsn The patched instruction
 * \return Size in bytes of the patched instruction, 0 if this instruction is deleted, or the size of the original instruction if
 * it is a partial copy (representing an instruction moved but not modified)
 * */
static unsigned int patchinsn_get_bytesize(patchinsn_t* patchinsn) {
   assert(patchinsn);
   if (patchinsn->patched) {
      //The patched instruction exists
      if (insn_get_opcode_code(patchinsn->patched) == BAD_INSN_CODE)
      return insn_get_bytesize(patchinsn->origin);//Partially copied instruction: its size should not have changed
      else
      return insn_get_bytesize(patchinsn->patched);//Fully copied instruction: returning its size
   } else
   return 0;   //Patched instruction is NULL (has been deleted): size is 0
}
#endif

/**
 * Retrieve the byte code of a patched instruction
 * \return The length of the code in bytes
 * */
static unsigned patchinsn_getbytescoding(patchinsn_t* patchinsn,
      unsigned char* str)
{
   assert(patchinsn && str);
   bitvector_t* coding;
   /**\todo TODO (2015-04-16) I could improve this even further by retrieving the bytes of the original instruction
    * and simply returning them when the instruction was partially copied*/
   if (patchinsn->patched) {
      //The patched instruction exists
      if (insn_get_opcode_code(patchinsn->patched) == BAD_INSN_CODE) {
         coding = insn_get_coding(patchinsn->origin); //Partially copied instruction: same coding as the original
         DBGLVL(2,
               patcher_insn_fprint_withaddr(patchinsn->origin, patchinsn->patched, stderr, A_NA))
      } else {
         coding = insn_get_coding(patchinsn->patched); //Fully copied instruction: using the new coding
         DBGLVL(2, patcher_insn_fprint(patchinsn->patched, stderr, A_PATCHUPD))
      }
   } else {
      DBGLVL(2, patcher_insn_fprint(patchinsn->origin, stderr, A_PATCHDEL))
      return 0;   //Patched instruction is NULL (has been deleted)
   }
   return bitvector_printbytes(coding, str,
         arch_get_endianness(insn_get_arch(patchinsn->origin)));
}

/**
 * Updates the coding and addresses of a list of patched instructions.
 * \param patchinsn The patchinsn_t structure representing the patched instruction
 * \param firstaddr Address of the first instruction in the list
 * \param driver Driver for assembling the instruction
 * \return Address of the end of the last instruction in the block
 * */
static int64_t patchinsnlist_update(queue_t* patchinsns, int64_t firstaddr,
      asmbldriver_t* driver)
{
   /**\todo TODO (2015-01-12) Get rid of the need of driver. For that, retrieve it in assemble_insn through the instruction architecture*/
   assert(patchinsns);
   int64_t addr = firstaddr;
   FOREACH_INQUEUE(patchinsns, iter) {
      patchinsn_t* pi = GET_DATA_T(patchinsn_t*, iter);
      if (pi->patched) {
         //Patched instruction not NULL
         //Updates the address of the current patched instruction
         insn_set_addr(pi->patched, addr);
         //Updates the coding of the current patched instruction
         if (insn_get_opcode_code(pi->patched) != BAD_INSN_CODE) {
            //Instruction is a full copy
            upd_assemble_insn(pi->patched, driver, TRUE, NULL);
            if (pi->origin && !insn_check_annotate(pi->origin, A_PATCHMOV)
                  && (insn_get_bytesize(pi->origin)
                        != insn_get_bytesize(pi->patched))) {
               //Instruction is not moved and has changed size: raising an error
               /**\todo TODO (2015-01-13) This should never happen, as this function is intended to be invoked on the lists in movedblocks
                * Remove this test if it is indeed what we do*/
               ERRMSG("New coding of %#"PRIx64":", insn_get_addr(pi->patched));
               insn_fprint(pi->patched, stderr);
               STDMSG(
                     " would have a different size (%d bytes instead of %d). No updates performed on this instruction.\n",
                     insn_get_bytesize(pi->patched),
                     insn_get_bytesize(pi->origin));
               insn_free(pi->patched);
               pi->patched = insn_copy(pi->origin);
               /**\todo TODO (2015-01-13) See what is more coherent: new copy or simply updating the coding from the coding of pi->origin (assuming we keep this)*/
            }
            //Increases the address based on the size of the patched instruction
            addr += insn_get_bytesize(pi->patched);
            DBGLVL(3, patcher_insn_fprint(pi->patched, stderr, A_NA))
         } else {
            //Instruction is a partial copy: increasing the address based on the size from the original
            addr += insn_get_bytesize(pi->origin);
            DBGLVL(3,
                  patcher_insn_fprint_withaddr(pi->origin, pi->patched, stderr, A_NA))
         }
      }  //Else the patched instruction is NULL
   }
   return addr;
}

////////////END OF NEW FUNCTIONS

/**
 * Flags all instructions present in the list as having been moved to the new section
 * \param inl The list
 * */
static void insnlist_setmoved(queue_t* inl)
{
   int hasindirect = FALSE;
#ifndef NDEBUG //To avoid compilation warnings when not in debug mode
   int64_t indiraddr = -1;
#endif
   FOREACH_INQUEUE(inl, iter) {
      insn_add_annotate(GET_DATA_T(insn_t*, iter), A_PATCHMOV);
      if (insn_is_indirect_branch(INSN_INLIST(iter)) == TRUE) {
         hasindirect = TRUE;
#ifndef NDEBUG //To avoid compilation warnings when not in debug mode
            if (INSN_GET_ADDR(GET_DATA_T(insn_t*,iter)) >= 0)
                indiraddr = INSN_GET_ADDR(GET_DATA_T(insn_t*,iter));
#endif
      }
   }
   if (hasindirect == TRUE) {
      DBGMSG(
            "WARNING: Patching moved indirect branch present in function %s (address %#"PRIx64"). Patched file may crash\n",
            label_get_name(insn_get_fctlbl((insn_t* )queue_peek_head(inl))),
            indiraddr);
   }
}

/**
 * Checks if an instruction has been moved
 * \param in The instruction
 * \return TRUE if an instruction is present in the list of displaced instructions, FALSE otherwise
 * */
static int insn_ismoved(insn_t* in) {
    return (insn_check_annotate(in, A_PATCHMOV));
}

#ifndef NDEBUG //This function is used for debug only, so we avoid compilation warnings. To be changed if we need it in non-debug mode
#if NOT_USED_ANYMORE
static void patchinsn_print(insn_t* insn, FILE* stream)
{
   assert(insn && stream);
   char buf[256], bufc[128];
   insn_print(insn, buf, sizeof(buf));
   if (insn_get_coding(insn)) bitvector_hexprint(insn_get_coding(insn), bufc, sizeof(bufc), " ");
   else bufc[0] = '\0';
   fprintf(stream, "\t%#"PRIx64":%s\t%s (%p)", INSN_GET_ADDR(insn), bufc, buf, insn);
   if (insn_get_branch(insn) != NULL) {
       char bufd[256];
       insn_print(insn_get_branch(insn), bufd, sizeof(bufd));
       fprintf(stream, "\t->%#"PRIx64":%s (%p)", INSN_GET_ADDR(insn_get_branch(insn)), bufd, insn_get_branch(insn));
   }
   fprintf(stream, "%s", (insn_check_annotate(insn, A_PATCHMOV)) ? "M" : " ");
   fprintf(stream, "%s", (insn_check_annotate(insn, A_PATCHNEW)) ? "N" : " ");
   fprintf(stream, "%s", (insn_check_annotate(insn, A_PATCHDEL)) ? "D" : " ");
   fprintf(stream, "%s", (insn_check_annotate(insn, A_PATCHUPD)) ? "U" : " ");
   fprintf(stream,"\n");
}

/**
 * Dumps the content of the patch_list in a patched file
 * Mainly intended for debugs
 * \param pf The structure holding the patched file
 * \param stream The stream to write into
 * */
static void patchfile_dumppatchlist(patchfile_t* pf, FILE* stream) {
   assert(pf && stream);
   if ( (pf) && (pf->patch_list)) {
      FOREACH_INQUEUE(pf->patch_list, iter) {
           patchinsn_print(GET_DATA_T(insn_t*, iter), stream);
      }
   }
}
#endif
#endif

#if NOT_USED_ANYMORE
/**
 * Swaps the elements of a list at a given point with those of a second list
 * \param src Queue to swap elements in. The new elements will be flagged as A_PATCHNEW
 * \param start Element from which the replacement must start
 * \param stop Element at which the replacement must stop
 * \param dst Queue to insert. Will contain the extracted elements after the operation is performed, flagged as A_PATCHDEL
 * \param annotate Annotation to add to the added instructions (if needed)
 * */
static void insnlist_swap(queue_t* src, list_t* start, list_t* stop, queue_t* dst, unsigned int annotate) {
   /**\todo Return a value to allow error handling*/
   FOREACH_INQUEUE(dst,itersrc) {
       insn_add_annotate(GET_DATA_T(insn_t*, itersrc), A_PATCHNEW | annotate);
   }
   queue_swap_elts(src, start, stop, dst);
   FOREACH_INQUEUE(dst,iterdst) {
       insn_add_annotate(GET_DATA_T(insn_t*, iterdst), A_PATCHDEL);
   }
}
#endif

/**
 * Sets an annotate flag on a modification and propagates it to all the modifications that have been set as next to this one
 * \param modif The modification
 * \param annotate The flag to set
 * */
static void modif_setannotate_propagate(modif_t* modif, int8_t annotate)
{

   modif_t* cursor = modif;
   while (cursor != NULL) {
      cursor->annotate |= annotate;
      cursor = cursor->nextmodif;
   }
}

/*
 * Flags a modification as an else modification. All its successors will be flagged as well
 * \param modif The modification
 * */
void modif_annotate_else(modif_t* modif)
{
   modif_setannotate_propagate(modif, A_MODIF_ISELSE);
}
#if 0 //We are to use the binfile_t type
/**
 * Tests if a disassembled file is an executable
 * \param pf A structure holding the disassembled file
 * \return 1 if the file is an executable, -1 if it is a shared library, 0 otherwise
 * \todo This function is not called anywhere. Delete it if is not needed by the implementation of the static patching
 * */
int patchfile_isexe(patchfile_t* pf) {
   /**\todo Improve this function to detect other cases (shared, relocatable) => partly done
    * */
//    if(elffile_gettype(pf->efile)==ET_EXEC)
//        return 1;
//    else if(elffile_gettype(pf->efile)==ET_DYN)
//        return -1;
   if(binfile_get_type(pf->bfile)==BFT_EXECUTABLE)
   return 1;
   else if(binfile_get_type(pf->bfile)==BFT_LIBRARY)
   return -1;
   else return 0;
}
#endif

#if 0 //Use libbin
/**
 * Returns the index of instruction list in the lists array whose ELF section has the
 * specified name or index
 * \param pf The structure holding the disassembled file
 * \param name The name of the ELF section to look for
 * \param idx The index of the ELF section to look for (if name is NULL)
 * \return The index of the corresponding instruction list in the list array, or -1 if no
 * instruction list with those criteria was found
 * */
static int patchfile_getprogidx(patchfile_t* pf,char* name,int idx) {
   int i=0;

   if(name==NULL) {/*Looks for instruction list with the specified ELF section index*/
      while(i<pf->n_codescn) {
         if(binscn_get_index(pf->codescn[i])==idx) break;
         i++;
      }
   } else {/*Looks for instruction list with the specified ELF section name*/
      while(i<pf->n_codescn) {
         if(str_equal(binscn_get_name(pf->codescn[i]),name)) break;
         i++;
      }
   }
   if(i!=pf->n_codescn) return i;
   else return -1;  //No instruction list found
}
#endif

/**
 * Returns the code for an condition type
 * \param condtype Code of the condition type
 * \param noreverse If to FALSE, the opposite will be returned
 * \return Code of the type:
 * - 'e' for equal,
 * - 'n' for non equal
 * - 'l' for less or equal
 * - 'L' for less strict
 * - 'g' for greater or equal
 * - 'G' for greater strict
 * - '\0' for unrecognized type (should never happen)
 * */
static char cond_typecode(char condtype, char noreverse)
{
   switch (condtype) {
   case COND_AND:
      return ((noreverse == TRUE) ? '&' : '/'); //That one is only for debug printing
   case COND_OR:
      return ((noreverse == TRUE) ? '|' : '-'); //That one is only for debug printing
   case COND_EQUAL: /**<EQUAL condition (to be used between an operand and a value)*/
      return ((noreverse == TRUE) ? 'e' : 'n');
   case COND_NEQUAL: /**<NOT EQUAL condition  (to be used between an operand and a value)*/
      return ((noreverse == TRUE) ? 'n' : 'e');
   case COND_LESS: /**<LESS condition (to be used between an operand and a value)*/
      return ((noreverse == TRUE) ? 'L' : 'g');
   case COND_GREATER: /**<MORE condition (to be used between an operand and a value)*/
      return ((noreverse == TRUE) ? 'G' : 'l');
   case COND_EQUALLESS: /**<LESS or EQUAL condition (to be used between an operand and a value)*/
      return ((noreverse == TRUE) ? 'l' : 'G');
   case COND_EQUALGREATER: /**<MORE or EQUAL condition (to be used between an operand and a value)*/
      return ((noreverse == TRUE) ? 'g' : 'L');
   }
   return 0;
}

/**
 * Numbers the conditions from left to right (basically we are ordering the leaves of a binary tree in depth first search. I think)
 * \param cond Condition whose childrens we want to number
 * \param conds Array of conditions
 * \param nconds Size of the array
 * \todo See the comment on cond_serialize
 * \note This function is separate because it is recursive
 * */
static void cond_numbers(cond_t* cond, cond_t*** conds, int *nconds)
{
   if (cond->type < COND_LAST_LOGICAL) {
      cond_numbers(cond->cond1, conds, nconds);
      cond_numbers(cond->cond2, conds, nconds);
   }
   if (cond->type > COND_LAST_LOGICAL) {
      *conds = lc_realloc((*conds), sizeof(cond_t*) * ((*nconds) + 1));
      (*conds)[(*nconds)] = cond;
      (*nconds) = (*nconds) + 1;
   }
}

/**
 * Serialises a condition into a sequential list of conditions and fills the arrays in the condition describing each condition values
 * \param cond The condition. Assumed not to be NULL
 * \return Number of conditions
 * */
static int cond_serialize(cond_t* cond)
{
   int nconds = 0;
   int i;
   cond_t** conds = NULL;

   cond_numbers(cond, &conds, &nconds);
   assert(nconds > 0); /**\todo Add a nice test here instead (or do it in the calling function)*/
   insertconds_t* insertconds = insertconds_new(nconds);
   for (i = 0; i < nconds; i++) {
      insertconds->condoprnds[i] = conds[i]->condop;
      insertconds->condvals[i] = conds[i]->condval;
      /*Now for the tricky part... We have to decide what to test (the condition or its inverse) so that the branch after the test is not the
       * next condition in the array*/
      /**\todo This is assembly-specific code and should definitely be in the libmpatch... However, could we assume or not that all assembly
       * languages will only write tests in the form of: if \<cond\> jump over there ?
       * => (2014-07-17) It is now in the libmpatch. See what happens after refactoring. And check with ARM how conditions would be written*/
      /**\todo I think the code below can be factorized*/
      if (i == (nconds - 1)) {
         //Special case: this is the last condition: if it is false, everything else is
         insertconds->condtypes[i] = cond_typecode(conds[i]->type, FALSE);
         insertconds->conddst[i] = -1;
      } else if (conds[i]->parent != NULL) {
         cond_t* next = NULL;
         if (conds[i] == conds[i]->parent->cond1) {
            /*This is the first condition of the parent condition*/
            if (conds[i]->parent->type == COND_AND) {
               cond_t* c = conds[i]->parent, *cc = conds[i];
               while ((c != NULL) && ((c->type == COND_AND) || (c->cond2 == cc))) {
                  //Finds the first parent that is not a logical AND, or whose we are not a subcondition of the second operand
                  cc = c;
                  c = c->parent;
               }
               if (c == NULL) {
                  //All parents are logical ANDs: if this condition fails, all fail
                  insertconds->condtypes[i] = cond_typecode(conds[i]->type,
                        FALSE);
                  insertconds->conddst[i] = -1;
               } else if ((c->cond1 == cc) || (c->type == COND_OR)) {/**\todo I really think this should be an && instead of ||*/
                  /*We found a parent whose our condition is a subcondition to the first operand of a logical OR:
                   * the next step if the condition fails is this first comparison condition subordinate to the second operand of the OR*/
                  insertconds->condtypes[i] = cond_typecode(conds[i]->type,
                        FALSE);
                  next = c->cond2;
                  while (next->type < COND_LAST_LOGICAL)
                     next = next->cond1;
               } else {
                  assert(0);  //Should not happen
               }
            } else if (conds[i]->parent->type == COND_OR) {
               /*We are the first condition of a logical OR: if it is true, this condition is true as well and we have to go up to
                * find the first AND parent of which we are not the second operand*/
               cond_t* c = conds[i]->parent, *cc = conds[i];
               while ((c != NULL) && ((c->type == COND_OR) || (c->cond2 == cc))) {
                  cc = c;
                  c = c->parent;
               }
               if (c == NULL) {
                  //All parents are logical ORs: if this condition succeeds, all succeed
                  insertconds->condtypes[i] = cond_typecode(conds[i]->type,
                        TRUE);
                  insertconds->conddst[i] = 0;
               } else if ((c->cond1 == cc) || (c->type == COND_AND)) {/**\todo I really think this should be an && instead of ||*/
                  /*We found a parent whose our condition is a subcondition to the first operand of a logical AND:
                   * the next step if the condition succeeds is the first comparison condition subordinate to the second operand of the AND*/
                  insertconds->condtypes[i] = cond_typecode(conds[i]->type,
                        TRUE);
                  next = c->cond2;
                  while (next->type < COND_LAST_LOGICAL)
                     next = next->cond1;
               } else {
                  assert(0);  //Should not happen
               }
            }
         } else {
            /*This is the second condition of the parent condition*/
            cond_t* c = conds[i]->parent, *cc = conds[i];
            while ((c != NULL) && ((c->cond2 == cc) || (c->type == COND_AND))) {
               //Looking for the first condition of which we are not a subcondition of the second operand nor a logical AND
               cc = c;
               c = c->parent;
            }
            if (c == NULL) {
               if (cc->type == COND_AND) {
                  //This condition is the last one, or is in a line of logical ANDs: if it fails, everything fails
                  insertconds->condtypes[i] = cond_typecode(conds[i]->type,
                        FALSE);
                  insertconds->conddst[i] = -1;
               } else if (cc->type == COND_OR) {
                  //This condition is the last one, or is in a line of logical ANDs with a logical OR above all: if it succeeds, everything succeeds
                  insertconds->condtypes[i] = cond_typecode(conds[i]->type,
                        TRUE);
                  insertconds->conddst[i] = 0;
               }
            } else {
               //Now at this point, we are a sub condition of the first operand of a logical OR: if this condition succeeds, we skip to the next
               //cond_t* c = conds[i]->parent, *cc = conds[i];
               while ((c != NULL) && ((c->type == COND_OR) || (c->cond2 == cc))) {
                  cc = c;
                  c = c->parent;
               }
               if (c == NULL) {
                  //All parents are logical ORs: if this condition succeeds, all succeed
                  insertconds->condtypes[i] = cond_typecode(conds[i]->type,
                        TRUE);
                  insertconds->conddst[i] = 0;
               } else if ((c->cond1 == cc) || (c->type == COND_AND)) {/**\todo I really think this should be an && instead of ||*/
                  /*We found a parent whose our condition is a subcondition to the first operand of a logical AND:
                   * the next step if the condition succeeds is the first comparison condition subordinate to the second operand of the AND*/
                  insertconds->condtypes[i] = cond_typecode(conds[i]->type,
                        TRUE);
                  next = c->cond2;
                  while (next->type < COND_LAST_LOGICAL)
                     next = next->cond1;
               } else {
                  assert(0);                    //Should not happen
               }
            }
         }
         /*If there is a next condition, find its index*/
         /**\todo Ugly code*/
         if (next != NULL) {
            int j;
            for (j = i + 1; j < nconds; j++)
               if (conds[j] == next)
                  break;
            assert(j != nconds);
            insertconds->conddst[i] = j;
         }
      } else {
         /*This should happen only if there is only one comparison condition*/
         insertconds->condtypes[i] = cond_typecode(conds[i]->type, FALSE);
         insertconds->conddst[i] = -1;
      }
   }
   insertconds->nconds = nconds;
   cond->insertconds = insertconds;
   lc_free(conds);
   return nconds;
}

/*
 * Creates a new patched file
 * \param af File to be patched
 * \return A new \ref patchedfile_t structure
 * */
static patchfile_t* patchfile_new(asmfile_t* af)
{
   /** \todo Like everything else in this file, will have to be updated */
   patchfile_t* pf = NULL;

   if ((af == NULL) || (asmfile_get_binfile(af) == NULL)) {
      ERRMSG(
            "Unable to initialise patched file: original file NULL or lacking binary file description\n");
      return pf;
   }
//        int i, nscn = 0;
//        elffile_t* efile;
   binfile_t* bfile;
//        list_t* scnstart;
   pf = (patchfile_t*) lc_malloc0(sizeof(patchfile_t));
//        efile = asmfile_getorigin(af);
   bfile = asmfile_get_binfile(af);
   /*Retrieving the identifiers of sections containing code*/
   pf->codescn = binfile_get_code_scns(bfile); //elffile_getprogscns(efile, &nscn);
   pf->n_codescn = binfile_get_nb_code_scns(bfile);                    //nscn;
   /*Looks for the section boundaries*/
   /**\todo This is a quite dirty code, as it does not perform any check on the coherence between the
    * instructions list and the sections (we assume they have all been disassembled and are in the correct order)
    * It will be removed during patcher re-factoring*/

//        pf->codescn_bounds = lc_malloc(sizeof(elfscnbounds_t) * (nscn));
//        pf->tlssize = elffile_gettlssize(efile);
//        scnstart = queue_iterator(asmfile_get_insns(af));
//        for ( i = 0; i < (nscn-1); i++) {
//            pf->codescn_bounds[i].begin = scnstart;
//            while ( ( scnstart != NULL) && ( insn_get_addr(GET_DATA_T(insn_t*,scnstart)) != binscn_get_addr(pf->codescn[i])/*elffile_getscnstartaddr(efile, pf->codescn_idx[i+1])*/ )  )
//                scnstart = scnstart->next;
//            if (scnstart != NULL)
//                pf->codescn_bounds[i].end = scnstart->prev;
//            else //this case happens only with the last section
//                pf->codescn_bounds[i].end = queue_iterator_rev(asmfile_get_insns(af));
//            DBGMSG("Code ELF section %d has boundaries %#"PRIx64" and %#"PRIx64"\n",
//                    i,insn_get_addr(GET_DATA_T(insn_t*,pf->codescn_bounds[i].begin)),insn_get_addr(GET_DATA_T(insn_t*,pf->codescn_bounds[i].end)));
//        }
//        /*Last section*/
//        pf->codescn_bounds[i].begin = scnstart;
//        while ( scnstart != NULL)
//            scnstart = scnstart->next;
//        pf->codescn_bounds[i].end = queue_iterator_rev(asmfile_get_insns(af));
//        assert(i==(nscn-1)); //Just checking, I have some memcheck error flying around and I'm attempting to catch it. Besides, it's clearer
//        /****\todo TODO (2014-06-13) Temporary quick and dirty fix to retrieve the section index while the libbin is not implemented ***/
//        char* scnname = elfsection_name(efile, pf->codescn_idx[i]);
//        if (str_equal(".text", scnname)) {
//           pf->scntxtidx = i;
//        } else if (str_equal(".init", scnname)) {
//           pf->scniniidx = i;
//        } else if (str_equal(".fini", scnname)) {
//           pf->scnfinidx = i;
//        } else if (str_equal(".plt", scnname)) {
//           pf->scnpltidx = i;
//        }
//        /****End of temporary quick and dirty fix***/
//        DBGMSG("Code ELF section %d has boundaries %#"PRIx64" and %#"PRIx64"\n",
//                i,insn_get_addr(GET_DATA_T(insn_t*,pf->codescn_bounds[i].begin)),insn_get_addr(GET_DATA_T(insn_t*,pf->codescn_bounds[i].end)));
   pf->asmbldriver = asmbldriver_load(asmfile_get_arch(af));/**\bug valgrind detects a memory leak here (old madras, check if it still exists)*/
   pf->patchdriver = patchdriver_load(asmfile_get_arch(af));/**\bug valgrind detects a memory leak here (old madras, check if it still exists)*/
//        pf->efile = efile;
   pf->bfile = bfile;
   pf->bindriver = binfile_get_driver(bfile);
   /**\todo REFACTOR Dupliquer le binfile*/
   pf->afile = af;
   pf->branches = asmfile_get_branches(af);
   pf->branches_noupd = hashtable_new(direct_hash, direct_equal);
   pf->insn_list = af->insns;
//        pf->varrefs = queue_new();
   pf->modifs = queue_new();
   pf->modifs_lib = queue_new();
   pf->modifs_var = queue_new();
   pf->modifs_lbl = queue_new();
   pf->insertedfcts = queue_new();
   pf->insertedobjs = queue_new();
   pf->insertedlibs = queue_new();
   pf->extsymbols = hashtable_new(str_hash, str_equal);
   pf->current_cond_id = 1; //We'll have the id's start at 1 to differentiate with NULL values
   pf->current_globvar_id = 1;
   pf->current_modif_id = 1;
   pf->current_modiflib_id = 1;

//        pf->data = NULL; //The malloc0 should have taken care of that, but just to be sure
//        pf->reordertbl = NULL; //The malloc0 should have taken care of that, but just to be sure
//        pf->tdata = NULL;
//        pf->lastrelfctidx = -1; /**\todo Get rid of this member - see todo in the header file*/
   pf->paddinginsn = pf->patchdriver->generate_insn_nop(8);
   pf->insnvars = queue_new();
   /*We will be updating the elffile target hashtable : the functions associated to instructions were initalised by NULL in the disassembler*/
   /**\todo To be updated with the ELF parser refactoring*/
//        FOREACH_INHASHTABLE(efile->targets,iter) {
//            targetaddr_t* ta = GET_DATA_T(targetaddr_t*,iter);
//            if (ta->updaddr == NULL) ta->updaddr = pf->patchdriver->upd_dataref_coding;
//               ta->updaddr = pf->driver->upd_dataref_coding;
//        }
   pf->new_OSABI = -1;
   //////////NEW ELEMENTS ADDED BY PATCHER REFACTORING
   pf->arch = asmfile_get_arch(af);
   /**\todo TODO (2015-04-14) I need the architecture to update pointers in data_t structures, so the big bad interworking
    * should not be a bother here, but keep it in mind. Also, remove this or move the oprnd_updptr function away from arch_t*/
   pf->movedblocks = queue_new(); //Queue of all movedblock_t structures representing blocks to be moved
   pf->fix_movedblocks = queue_new(); //Queue of movedblock_t structures representing blocks to be moved at a fixed address
   pf->movedblocksbyinsns = hashtable_new(direct_hash, direct_equal); //Tables containing movedblock_t structures indexed by the instructions they contain
   pf->patchedinsns = hashtable_new(direct_hash, direct_equal); //Table of patchinsn_t structures indexed by the original instruction
   pf->movedblocksbyscn = hashtable_new(direct_hash, direct_equal);
   //binfile_t* patchbin;            //Copy of the structure representing the binary file, used for patching
   pf->reladdrs = queue_new(); //Queue of data_t structures containing addresses to be used by jumps to displaced code using memory relative operands
   pf->insnreplacemodifs = hashtable_new(direct_hash, direct_equal); //Hashtable of all modif_t structures replacing an instruction, indexed by the instruction they target
   pf->insnbeforemodifs = hashtable_new(direct_hash, direct_equal); //Hashtable of all modif_t structures inserted before an instruction, indexed by the instruction they target
   pf->memreladdrs = queue_new(); //Queue of data_t structures containing addresses used by branches with a memrel_t operand to access displaced code
   pf->addrsize = pf->patchdriver->get_addrsize(binfile_get_word_size(pf->bfile));

   pf->insnrefs = hashtable_new(direct_hash, direct_equal);
   pf->datarefs = hashtable_new(direct_hash, direct_equal);
   pf->newbranches = hashtable_new(direct_hash, direct_equal);

   pf->smalljmp_maxdistneg = pf->patchdriver->get_smalljmp_maxdistneg(); /**<Smallest signed distance reachable with the smallest (in instruction size) direct jump instruction*/
   pf->smalljmp_maxdistpos = pf->patchdriver->get_smalljmp_maxdistpos(); /**<Largest signed distance reachable with the smallest (in instruction size) direct jump instruction*/
   pf->jmp_maxdistneg = pf->patchdriver->get_jmp_maxdistneg(); /**<Smallest signed distance reachable with the standard direct jump instruction*/
   pf->jmp_maxdistpos = pf->patchdriver->get_jmp_maxdistpos(); /**<Largest signed distance reachable with the standard direct jump instruction*/
   pf->relmem_maxdistneg = pf->patchdriver->get_relmem_maxdistneg(); /**<Smallest signed distance between an instruction using a relative memory operand and the referenced address*/
   pf->relmem_maxdistpos = pf->patchdriver->get_relmem_maxdistpos(); /**<Largest signed distance between an instruction using a relative memory operand and the referenced address*/

   pf->smalljmpsz = pf->patchdriver->get_smalljmpsz(); /**<Size in byte of the smallest direct jump instruction list*/
   pf->jmpsz = pf->patchdriver->get_jmpsz(); /**<Size in byte of the direct jump instruction list*/
   pf->relmemjmpsz = pf->patchdriver->get_relmemjmpsz(); /**<Size in byte of the jump instruction list using a relative memory operand as destination.*/
   pf->indjmpaddrsz = pf->patchdriver->get_indjmpaddrsz(); /**<Size in bytes of the indirect jump instruction list*/
   return pf;
}

static void movedblock_free(void* m)
{
   movedblock_t* mb = m;
   assert(mb);
   DBGLVL(3,
         FCTNAMEMSG0("Freeing block "); movedblock_fprint(mb, stderr); STDMSG("\n"));
   if (mb->trampsites)
      queue_free(mb->trampsites, NULL);
   queue_free(mb->modifs, NULL);
   if (mb->patchinsns) {
      //Freeing new patchinsns (the others will be freed when the hashtable of patchinsns is freed in patchfile)
      FOREACH_INQUEUE(mb->patchinsns, iter) {
         patchinsn_t* pi = GET_DATA_T(patchinsn_t*, iter);
         if (!pi->origin)
            patchinsn_free(pi);
      }
      queue_free(mb->patchinsns, NULL);
   }
   //if (mb->localdata)
   FOREACH_INQUEUE(mb->localdata, iter) {
      data_free(GET_DATA_T(globvar_t*, iter)->data);
   }
   queue_free(mb->localdata, NULL);
   if (mb->newinsns)
      queue_free(mb->newinsns, insn_free);
   lc_free(mb);
}

static void patcher_interval_free(void* i)
{
   interval_t* interval = i;
   queue_t* q = interval_get_data(interval);
   if (q)
      queue_free(q, NULL);
   interval_free(interval);
}

/*
 * Frees a patchfile structure
 * \param pf The patchfile_t structure to free
 * */
void patchfile_free(patchfile_t* pf)
{
   if (pf == NULL)
      return;
//    lc_free(pf->codescn_idx);
//    lc_free(pf->codescn_bounds);
   if (pf->asmbldriver)
      asmbldriver_free(pf->asmbldriver);
   if (pf->patchdriver)
      patchdriver_free(pf->patchdriver);
//    queue_free(pf->varrefs,lc_free);
   queue_free(pf->insertedfcts, insertfunc_free);
   hashtable_free(pf->extsymbols, NULL, NULL);
   /**\todo There will have to be some freeing for the asmfiles representing objects*/
   queue_free(pf->insertedobjs, NULL);
   queue_free(pf->insertedlibs, NULL);

   if (pf->patch_list != NULL)
      queue_free(pf->patch_list, insn_free);

//    if ( pf->plt_list != NULL )
//        queue_free(pf->plt_list,insn_free);
//
//    lc_free(pf->reordertbl);
//
//	if ( pf->objfiles != NULL )
//		lc_free(pf->objfiles);
   insn_free(pf->paddinginsn);
   queue_free(pf->insnvars, lc_free);
   DBGMSG0LVL(1, "Freeing modification queue in patchfile\n");
   /**\todo TODO (2014-11-14) Remove the passing of arch to free functions (we don't need it now that insn_t contains a pointer to arch_t)
    * => (2015-05-27) DONE*/
//    FOREACH_INQUEUE(pf->modifs,itr){
//        modif_free(GET_DATA_T(modif_t*,itr),pf->afile->arch);
//    }
   //////////NEW ELEMENTS ADDED BY PATCHER REFACTORING
   //Has to be done before freeing the globvars
   queue_free(pf->movedblocks, movedblock_free); /**<Queue of all movedblock_t structures representing blocks to be moved*/
   //////////
   queue_free(pf->modifs, modif_free);
   /**\bug If modifs_free is called after a successful commit and terminate, the instructions
    * have already been freed and a segfault occurs*/
   /** \todo Find a way to avoid trying to lc_free the instructions list in mod->modifs if they have
    * already been deleted (see buglist)*/
   queue_free(pf->modifs_lib, modiflib_free);
   queue_free(pf->modifs_var, modifvar_free);
   queue_free(pf->modifs_lbl, modiflbl_free);

   hashtable_free(pf->branches_noupd, NULL, NULL);

   //////////NEW ELEMENTS ADDED BY PATCHER REFACTORING
   queue_free(pf->fix_movedblocks, movedblock_free);
   hashtable_free(pf->movedblocksbyinsns, NULL, NULL); /**<Tables containing movedblock_t structures indexed by the instructions they contain*/
   hashtable_free(pf->patchedinsns, patchinsn_free, NULL); /**<Table of patchinsn_t structures indexed by the original instruction*/
   hashtable_free(pf->movedblocksbyscn, NULL, NULL); /**<Table of patchinsn_t structures indexed by the original instruction*/
   //binfile_t* patchbin;            /**<Copy of the structure representing the binary file, used for patching*/
//    if (pf->data)
//       lc_free(pf->data);

   hashtable_free(pf->insnrefs, NULL, NULL);
   hashtable_free(pf->datarefs, NULL, NULL);
   hashtable_free(pf->newbranches, NULL, NULL);

   if (pf->insnaddrs)
      queue_free(pf->insnaddrs, lc_free);

   queue_free(pf->reladdrs, data_free); /**<Queue of data_t structures containing addresses to be used by jumps to displaced code using memory relative operands*/
   /**\todo TODO (2014-09-17) Add free of further new elements added by patcher refactoring (anyway a simple memcheck would show them)*/
   hashtable_free(pf->insnreplacemodifs, NULL, NULL); //Hashtable of all modif_t structures, indexed by the instruction at the address of which they are set
   hashtable_free(pf->insnbeforemodifs, NULL, NULL); //Hashtable of all modif_t structures, indexed by the instruction at the address of which they are set
   queue_free(pf->memreladdrs, data_free); //Queue of data_t structures containing addresses used by branches with a memrel_t operand to access displaced code
   queue_free(pf->emptyspaces, patcher_interval_free);
   binfile_patch_terminate(pf->patchbin);
   lc_free(pf);
}
#if 0 //Libbin
/**
 * Returns the index of the section containing a given address or instruction.
 * \param pf Patched file
 * \param in Instruction to look for. If NULL, the address will be used
 * \param addr The address
 * \return Index of the scn in the patched file (not its index in the ELF file) or -1 if not found
 * */
static int patchfile_getscnid_byaddr(patchfile_t* pf, insn_t* in, int64_t addr) {
   /**\todo This function needs to be updated or removed if not needed.
    * There should be one an only one way of linking an instruction to a section in the ELF file, and there should be no
    * need to link the section of moved instructions to the ELF file
    * (the codescn_idx[n_codescn_idx-1]==-1 is a remnant of the old MADRAS and should disappear)*/

   int indx;
   if ( ( pf->codescn_idx[pf->n_codescn_idx - 1] == -1) || ( pf->patch_list != NULL ) ) {
      if ( ( in != NULL ) && ( insn_ismoved(in) == TRUE ) )
      return (pf->n_codescn_idx - 1);
      else if (in == NULL) {
         /*There is a block of moved instructions: we will look up our instruction there first*/
         /**\todo Use something else (annotate is the candidate) to identify a moved instruction*/
         FOREACH_INQUEUE(pf->patch_list,iter) {
               if (INSN_GET_ADDR(GET_DATA_T(insn_t*, iter)) == addr)
                    return (pf->n_codescn_idx - 1);
            }
        }
        /*Instruction not found in the added section: starting the search in the last original section*/
        indx = pf->n_codescn_idx - 2;
    } else
        indx = pf->n_codescn_idx - 1;

   /*Starting from the end so that we will not incorrectly attribute an address to a section that would have
    * become bigger because of insertions and overlapped with the next one*/
   while ( indx >= 0 ) {
       if ((addr >= INSN_GET_ADDR(GET_DATA_T(insn_t*, pf->codescn_bounds[indx].begin)))
          && (addr <= INSN_GET_ADDR(GET_DATA_T(insn_t*, pf->codescn_bounds[indx].end))))
      break;
      indx--;
   }
   if ( indx < 0 )
   return -1;
   else
   return indx;
}
#endif

#if 0
/**
 * Returns a pointer to the instruction at the specified address in a disassembled elf file
 * \param pf The structure holding the disassembled file
 * \param addr The address to look for
 * \param secidx Will contains the instruction list index where the instruction was found
 * \param lstinsn If not NULL, will contain a pointer to the libmasm:list_t object containing
 * the instructions
 * \return A pointer to the structure holding the instruction found at the given address,
 * or NULL if no instruction was found
 * */
static insn_t* patchfile_getinsnataddr(patchfile_t* pf,int64_t addr,int* secidx,list_t** lstinsn) {
   insn_t* in=NULL;
   list_t* lst = NULL;
   int indx;
   DBGMSGLVL(1,"Looking up instruction at address %#"PRIx64"\n",addr);
    /**\todo Use asmfile_get_insn_by_addr here. Currently a file being patched can be too tangled for this function to work.*/
    if ( ( in != NULL ) && ( insn_get_sequence(in) != NULL ) ) {
      /*The function could be found: we look for its section*/
      if ( lstinsn != NULL)
            *lstinsn = insn_get_sequence(in);
      if ( secidx != NULL) {
         *secidx = patchfile_getscnid_byaddr(pf, NULL, addr);
      }
   } else {
      /*The instruction could not be found using the search in the file or does not have a sequence: we take the slow path*/
      if(!patchfile_isexe(pf)) {
         /*File is a relocatable : assuming the interesting code is in the .text section only*/
         indx = patchfile_getprogidx(pf,".text",0);
            in = insnlist_insnataddress(asmfile_get_insns(pf->afile),addr,pf->codescn_bounds[indx].begin,pf->codescn_bounds[indx].end->next);
      } else {
         /*File is a program (or shared library) : we have to scan every disassembled section*/
         /*We start from the end and check if a section starting address is lower than
          * the address we look for before searching it
          * We also exclude from the search any instruction list that doesn't have a valid
          *  associated section address (this is the case if we are on the process of adding code) */
         for(indx=pf->n_codescn_idx-1;indx>=0;indx--) {
            if(elffile_getscnstartaddr(pf->efile,pf->codescn_idx[indx])<=addr) {
               if (pf->codescn_idx[indx] == -1) { //We are looking at the section containing added data
                  /*Looking in the section containing added data but only for original instructions (no problem with address overlapping then)*/
                  DBGMSG0LVL(2,"Searching section section containing inserted data\n");
                  FOREACH_INQUEUE(pf->patch_list,iters) {
                           if ((INSN_GET_ADDR(GET_DATA_T(insn_t*, iters)) == addr)
                              && ((insn_check_annotate(GET_DATA_T(insn_t*, iters), A_PATCHNEW) == FALSE)
                              || (insn_check_annotate(GET_DATA_T(insn_t*, iters), A_PATCHUPD) == TRUE)))
                     break;
                     /**\todo Update the handling of the flags. We don't want to catch an inserted instruction that was duplicated from another
                      * (so this would include its address) but we want to catch instructions that have been updated and that have effectively
                      * replaced the original instruction.*/
                  }
                  lst = iters;
               } else {
                  DBGMSGLVL(2,"Searching section %d (%d in ELF)\n",indx,pf->codescn_idx[indx]);
                        lst = insnlist_addrlookup(asmfile_get_insns(pf->afile),addr,pf->codescn_bounds[indx].begin,pf->codescn_bounds[indx].end->next);
               }
               DBGMSGLVL(2,"Found instruction %p (in list object%p)\n",(lst!=NULL)?lst->data:NULL,lst);
               if(lst) {
                  in = (insn_t*)lst->data;
                  if(secidx!=NULL) *secidx=indx;
                  if(lstinsn) *lstinsn = lst;
                  break;
               }
            }
         }
      }
   }
   return in;
}
#endif
/**
 * Inserts a label in a file at a given address or at the location of a given instruction
 * \param pf Pointer to the structure holding the patched file
 * \param name Name of the label
 * \param linkednode Container of the instruction at which the label must be set
 * \param address Address at which to insert the label (if positive and linkednode is NULL)
 * \param type Type of the label, as defined in the label_types enum
 * \return EXIT_SUCESS or error code
 * */
static int patchfile_insertlabel(patchfile_t* pf, char* name,
      list_t* linkednode, int64_t address, int type)
{
   int out = ERR_PATCH_LABEL_INSERT_FAILURE;
   assert((pf != NULL ) && ( name != NULL ));
   binscn_t* scn = NULL;
   int64_t lbladdress = -1;
   insn_t* in = NULL;
   if ((linkednode != NULL) && (linkednode->data != NULL)) {
      //Label is linked to an instruction
      in = GET_DATA_T(insn_t*, linkednode);
      lbladdress = insn_get_addr(in);
      if (insn_check_annotate(in, A_PATCHMOV)) {
         //Instruction moved: we retrieve the section where the instruction is moved
         movedblock_t* mb = hashtable_lookup(pf->movedblocksbyinsns, in);
         assert(mb);
         scn = mb->newscn;
      } else {
         //Instruction not moved
         scn = label_get_scn(insn_get_fctlbl(in));
      }
   } else if (address >= 0) {
      //Label address is fixed
      lbladdress = address;
      scn = binfile_lookup_scn_span_addr(pf->patchbin, address);
   }
   if ((lbladdress >= 0) && (scn)) {
      int result;
      label_t* newlbl = label_new(name, lbladdress, TARGET_INSN, in);
      label_set_scn(newlbl, scn);
      DBGMSG("Inserting label %s of type %s at address %#"PRIx64"\n", name,
            (type == LABELTYPE_FCT) ? "FUNCTION" :
            (type == LABELTYPE_DUMMY) ? "DUMMY" : "NONE", lbladdress);
      /**\todo TODO (2014-09-30) Change the MADRAS API function for creating labels to use the labels types defined in libmasm.h*/
      switch (type) {
      case LABELTYPE_FCT:
         label_set_type(newlbl, LBL_FUNCTION);
         break;
      case LABELTYPE_DUMMY:
         label_set_type(newlbl, LBL_DUMMY);
         break;
      case LABELTYPE_NONE:
      default:
         label_set_type(newlbl, LBL_GENERIC);
         break; //Shut up Eclipse
      }
      out = binfile_patch_add_label(pf->patchbin, newlbl);
   }
   return out;
}

/**
 * Rename a dynamic symbol label
 * \param pf Pointer to the structure holding the patched file
 * \param newname The new name of the label
 * \param oldname The old name of the label
 * \return EXIT_SUCCESS if the update is successful, error code otherwise
 */
static int patchfile_patch_renamelabel(patchfile_t* pf, char* newname,
      char* oldname)
{
   int out;
#if 0
   if (pf != NULL && newname != NULL && oldname != NULL) {
      out = elffile_modify_dinsymlbl(pf->efile, newname, oldname);
   } else
   out = ERR_COMMON_PARAMETER_MISSING;
#endif
   HLTMSG("Label renaming disabled in this version of the patcher\n")
   return EXIT_FAILURE;
}

/**
 * Performs a label modification
 * \param pf Pointer to structure describing the patched file
 * \param modif Pointer to the structure containing details about the label modification
 * \return EXIT_SUCCESS if the modification succeeded, error code otherwise
 * */
static int modiflbl_apply(patchfile_t* pf, modiflbl_t* modif)
{
   if (pf == NULL)
      return ERR_PATCH_NOT_INITIALISED;
   if (modif == NULL)
      return ERR_PATCH_MISSING_MODIF_STRUCTURE;
   int out = EXIT_SUCCESS;
   switch (modif->type) {
   case NEWLABEL:
      out = patchfile_insertlabel(pf, modif->lblname, modif->linkednode,
            modif->addr, modif->lbltype);
      if (out != EXIT_SUCCESS) {
         ERRMSG("Unable to insert label %s\n", modif->lblname);
      }
      break;
   case RENAMELABEL:
      out = patchfile_patch_renamelabel(pf, modif->lblname, modif->oldname);
      if (out != EXIT_SUCCESS) {
         ERRMSG("Unable to rename label %s\n", modif->oldname);
      }
      break;
   default:
      out = ERR_PATCH_WRONG_MODIF_TYPE;
   }
   return out;
}
#if 0 //There will simply be a queue of data_t extracted from the globvar_t, that will have been reordered according to alignment
/**
 * Inserts a variable into a new data section of the patched file
 * \param pf Structure holding the details about the file being patched
 * \param vardata Structure describing the global variable to insert
 * \return EXIT_SUCCESS if successful, error code otherwise
 * */
static int patchfile_patch_insertdata(patchfile_t* pf, globvar_t* vardata) {
   int out = EXIT_SUCCESS;

   pf->data = lc_realloc(pf->data,pf->datalen + vardata->size);
   if( vardata->value != NULL ) {
      /*The inserted data has a fixed value*/
      DBGMSG("Inserting global variable %p (string value: \"%s\") of length %d at offset %#"PRIx64" (%"PRId64")\n",
            vardata->value,(char*)vardata->value,vardata->size,pf->datalen,pf->datalen);
      memcpy((void*)((size_t)pf->data + pf->datalen), vardata->value, vardata->size);
   } else {
      /*No fixed value: the space will be filled with zeroes*/
      DBGMSG("Inserting empty global variable of length %d at offset %#"PRIx64" (%"PRId64")\n",vardata->size,pf->datalen,pf->datalen);
      memset((void*)((size_t)pf->data + pf->datalen), 0, vardata->size);
   }
   vardata->offset = pf->datalen;
   pf->datalen += vardata->size;
   return out;
}
/**
 * Inserts a tls variable into the patched file
 * \param pf Structure holding the details about the file being patched
 * \param tlsvar Structure describing the tls variable to insert
 * \return EXIT_SUCCESS if successful, error code otherwise
 */
static int patchfile_patch_inserttlsvar(patchfile_t* pf, tlsvar_t* tlsvar) {

   int out = EXIT_SUCCESS;
   if (tlsvar->type == INITIALIZED) {
      pf->tdata = lc_realloc(pf->tdata, pf->tdatalen + tlsvar->size);
      memmove((void*)((size_t)pf->tdata + tlsvar->size), pf->tdata, pf->tdatalen);
      if (tlsvar->value != NULL) {
         DBGMSG("Inserting initialized thread variable %p (string value: \"%s\") of length %d at offset %#"PRIx64" (from tdata start)\n",
               tlsvar->value, (char*)tlsvar->value, tlsvar->size, pf->tdatalen);
         memcpy(pf->tdata, tlsvar->value, tlsvar->size);
      } else {
         DBGMSG("Inserting empty initialized thread variable %p of length %d at offset %#"PRIx64" (from tdata start)\n",
               tlsvar->value, tlsvar->size, pf->tdatalen);
         memset(pf->tdata, 0, tlsvar->size);
      }
      tlsvar->offset = pf->tdatalen;
      pf->tdatalen += tlsvar->size;
   } else if (tlsvar->type == UNINITIALIZED) {
      DBGMSG("Inserting uninitialized thread variable %p of length %d at offset %#"PRIx64" (from tbss start)\n",
            tlsvar->value, tlsvar->size, pf->tbsslen);
      tlsvar->offset = pf->tbsslen;
      pf->tbsslen += tlsvar->size;
   }
   return out;
}
#endif

/*
 * Updates the value of a global variable. If the file has already been finalised, this will be propagated to the binary code
 * \param pf Structure holding the details about the file being patched
 * \param vardata Structure describing the global variable to update
 * \param value New value of the global variable
 * \return EXIT_SUCCESS if successful, error code otherwise
 * */
int patchfile_patch_updatedata(patchfile_t* pf, globvar_t* vardata, void* value)
{
   if (!pf)
      return ERR_PATCH_NOT_INITIALISED;
   if (!vardata)
      return ERR_PATCH_GLOBVAR_MISSING;
   /**\todo TODO (2015-01-20) Update or remove this function depending on what we do with it in the refactoring
    * I'm guessing we would simply have to update the associated data_t, if it can be invoked before generating
    * the final binary code for the sections. Otherwise, simply detecting the offset of the globvar in the section
    * and update the code directly*/
#if 0
   if( value != NULL ) {
      //The new data has a fixed value
      DBGMSG("Updating global variable %d of length %d at offset %#"PRIx64" (%"PRId64") with new value %p\n",
            vardata->globvar_id,vardata->size,pf->datalen,pf->datalen, value);
      memcpy((void*)((size_t)pf->data + vardata->offset), value, vardata->size);
      if (!vardata->value)
      vardata->value = lc_malloc(vardata->size);
      memcpy(vardata->value, value, vardata->size);
   } else {
      //No fixed value: the space will be filled with zeroes
      DBGMSG("Updating global variable %d of length %d at offset %#"PRIx64" (%"PRId64") with empty value\n",
            vardata->globvar_id,vardata->size,pf->datalen,pf->datalen);
      memset((void*)((size_t)pf->data + vardata->offset), 0, vardata->size);
      if (vardata->value)
      lc_free(vardata->value);
      vardata->value = value;
   }
   if (pf->ready2write)
   elffile_upd_progbits(pf->efile,pf->data,pf->datalen,pf->datascnidx); //Updates the file bytes if it had already been finalised
#endif
   return EXIT_SUCCESS;
}

#if NOT_USED_ANYMORE
/**
 * Adds a global variable to a file
 * \param pf Pointer to structure describing the patched file
 * \param newgv Pointer to the structure containing the details about the new global variable
 * \return EXIT_SUCCESS or error code
 * */
static int newglobvar_apply(patchfile_t* pf, globvar_t* newgv) {
   int out = EXIT_SUCCESS;
   /**\todo TODO (2014-11-14) We can't use this separately as wee need to take alignemnts into account. So all new variables will
    * be ordered at once, creating a queue of globvar_t ordered by size and alignment, then the data_list will be derived from it*/
//   if (pf == NULL) {
//      //error
//      return FALSE;
//   }
//
//   if (newgv) {
//      out = patchfile_patch_insertdata(pf,newgv);
//   }
   return out;
}

/**
 * Adds a tls variable to a file
 * \param pf Pointer to structure describing the patched file
 * \param newtls Pointer to the structure containing the details about the new tls variable
 * \return EXIT_SUCCESS or error code
 * */
static int newtlsvar_apply(patchfile_t* pf, tlsvar_t* newtls) {
   int out = EXIT_SUCCESS;
   if (pf == NULL) {
      return ERR_PATCH_NOT_INITIALISED;
   }

   if (newtls) {
      patchfile_patch_inserttlsvar(pf,newtls);
   }
   return out;
}
#endif

/**
 * Updates all labels requests using an instruction node as link to point them to the next instruction node
 * \param pf Patched file
 * \param linknode Node containing the instruction to search
 * \param newlinknode New node to set
 * */
static void modiflbls_upd(patchfile_t* pf, list_t* linknode,
      list_t* newlinknode)
{
   if ((pf == NULL) || (linknode == NULL) || (newlinknode == NULL))
      return;

   FOREACH_INQUEUE(pf->modifs_lbl, iter)
   {
      modiflbl_t* mod = GET_DATA_T(modiflbl_t*, iter);
      if (mod->linkednode == linknode)
         mod->linkednode = newlinknode;
   }
}

#if 0
/**
 * Updates a branch instruction to point it to the first instruction that is not an unconditional branch if
 * it originally pointed to an unconditional branch
 * \param pf The patched file
 * \param iter Node containing the branch instruction
 * \param delnodes Queue of nodes containing deleted branch instructions
 * \return The node containing the branch instruction, or NULL if it has been deleted (the node will have been added to delnodes then)
 * */
static list_t* patchfile_branch_remove_rebounds(patchfile_t* pf, list_t* iter) {
   assert (pf && iter && insn_is_branch(INSN_INLIST(iter))>0);
   list_t* ret = NULL;
   insn_t* insn = INSN_INLIST(iter);
   insn_t* dest = insn_get_branch(insn);
   if ( (insn_is_branch(dest)>0) /*Destination is a branch*/
   /*&& ( (insn_get_annotate(INSN_INLIST(iter))&A_CALL)==0 )*/ ) /*I thought there would be a bug with those (because of the return address) but apparently not*/
   {
      DBG(
            char buf[256]= {'\0'};insn_print(insn, buf, sizeof(buf));
            char bufd[256]= {'\0'};insn_print(dest, bufd, sizeof(bufd));
            DBGMSG("Instruction %#"PRIx64":%s (%p) points to %#"PRIx64":%s (%p)\n",
                       INSN_GET_ADDR(insn),buf,insn,INSN_GET_ADDR(dest),bufd,dest);
      );
      /*Following the branches destinations until the destination is not a branch*/
       /**\todo Fix up what insn_is_branch returns and how do we detect if it is a direct or indirect branch*/
       /*We don't want a jump pointed to a CALL to be treated as a rebound, hence the check on A_CALL. Maybe implement this test into insn_is_branch*/
       while( dest && ( insn_is_branch(dest) ) && ( oprnd_is_ptr(insn_get_oprnd(dest,0)) == TRUE )
               && ( (insn_get_annotate(dest)&A_CALL)==0 ) && ( (insn_get_annotate(dest)&A_CONDITIONAL)==0 ) ) {
           dest = insn_get_branch(dest);
         DBG(
               char buf[256]= {'\0'};insn_print(insn, buf, sizeof(buf));
               char bufd[256]= {'\0'};insn_print(dest, bufd, sizeof(bufd));
               DBGMSG("Instruction %#"PRIx64":%s (%p) now points to %#"PRIx64":%s (%p)\n",
                           INSN_GET_ADDR(insn),buf,insn,INSN_GET_ADDR(dest),bufd,dest);
         );
      }
      /*Updates the branch so that its destination is the first non branch instruction in the chain of branches*/
       patchfile_setbranch(pf, insn,dest);
   }
   /*Checking that the branch has not become redundant*/
   if ((iter->next != NULL) && (dest == GET_DATA_T(insn_t*, iter->next))
           && ( (insn_get_annotate(insn)&A_CALL)==0 ) && ( (insn_get_annotate(insn)&A_CONDITIONAL)==0 )
           && ( insn_check_annotate(insn,A_PATCHNEW) == TRUE ) ) {
      /*Special case: the instruction branches unconditionally to the instruction immediately following it.
       * In that case, it is unnecessary and will be deleted*/
      //insn_t* idlejmp =  GET_DATA_T (insn_t*, iter);//queue_remove_elt(inl, iter);
      DBGMSG("Updating branches pointing to %p (addr %#"PRIx64") before its deletion\n", insn, insn_get_addr(insn));
      //Updating all branches pointing to the branch to be deleted to point to its destination
      queue_t* orig_branches = hashtable_lookup_all(pf->branches, insn_get_addr(insn));
      FOREACH_INQUEUE(orig_branches, orig_iter) {
         insn_t* orig_branch = GET_DATA_T(insn_t*, orig_iter);
         DBGMSGLVL(4, "Checking if instruction %p pointing to %#"PRIx64" points to instruction %p\n", orig_branch, insn_get_addr(insn), insn);
         if (insn_get_branch(orig_branch) == insn) {
            patchfile_setbranch(pf, orig_branch, dest);
         }
      }
      queue_free(orig_branches, NULL);
      /*Also removing the instruction from the branches hashtable otherwise freeing it will cause a seg fault later on when
       * attempting to update the branches.*/
       int64_t idlejmpdestaddr = INSN_GET_ADDR(dest);
#ifndef NDEBUG //To avoid compilation warnings when not in debug mode
      int res =
#endif
      hashtable_remove_elt(pf->branches,(void*)idlejmpdestaddr,insn);
      DBG(
            char buf[256]= {'\0'};insn_print(insn, buf, sizeof(buf));
            DBGMSG("Instruction %#"PRIx64":%s (%p) points to the instruction following it and will be deleted\n",
                       INSN_GET_ADDR(insn),buf,insn);
            DBGMSG("Instruction %p pointing to %#"PRIx64" was %s branches table\n",insn,idlejmpdestaddr,(res==1)?"removed from":"not found in");
      );
      insn_free(insn);
   } else {
      //The branch is kept: we return it
      ret = iter;
   }
   /**\todo Add here a code to transform any succession of JCOND + JMP into JOPPOSITECOND*/
   /**\todo Add here a code to transform a JUMP pointing to a NOP into a JUMP to the first instruction after its target that is not a NOP (and remove the nop)*/
   return ret;
}

/**
 * Updates the branches instructions in a patched file so that, if the instruction they point to was a branch instruction,
 * they point to the destination of this instruction instead
 * \param pf The patched file
 * \return Number of branches among the moved/added instructions
 * \warning Use this function with caution, as it could potentially put a destination out of range
 * */
/** \todo Rewrite this function, and decide precisely which rebounds it must remove.*/
static int patchfile_remove_branches_rebound(patchfile_t* pf) {
   queue_t* inl = pf->patch_list;
   int n_branch = 0;
#ifndef NDEBUG
   uint32_t n_dels = 0;
#endif
   queue_t* branchnodes = queue_new();
   list_t* iter = queue_iterator(inl);
   while (iter != NULL) {
        if((insn_is_branch(INSN_INLIST(iter))>0)) {/*Instruction is a branch*/
         //n_branch++;
         //Removes the rebounds for this branch
         list_t* node = patchfile_branch_remove_rebounds(pf, iter);
         if (node) {
            queue_add_tail(branchnodes, node); //The branch was not deleted: storing the node containing it
            iter = iter->next;
         } else {
            //The branch was deleted: freeing the node containing it
            list_t* next = iter->next;
            queue_remove_elt(inl, iter);
            iter = next;
#ifndef NDEBUG
            n_dels++;
#endif
         }
      } else
      iter = iter->next;
   }
   //Now performs another pass on the branches to be sure that no redundant branch remains
   while (n_branch != queue_length(branchnodes)) {
      n_branch = queue_length(branchnodes);
      list_t* iter_nodes = queue_iterator(branchnodes);
      while (iter_nodes != NULL) {
         if (patchfile_branch_remove_rebounds(pf, GET_DATA_T(list_t*, iter_nodes)) == NULL) {
            //The branch was deleted: removing the node that contained it in the instruction list
            queue_remove_elt(inl, GET_DATA_T(list_t*, iter_nodes));
            //Also removing it from the list of branches
            list_t* next_node = iter_nodes->next;
            queue_remove_elt(branchnodes, iter_nodes);
            iter_nodes = next_node;
#ifndef NDEBUG
            n_dels++;
#endif
         } else {
            iter_nodes = iter_nodes->next;
         }
      }
   }

   DBGLVL(2, if (n_dels > 0) {DBGMSG0("Patch list is now:\n");patchfile_dumppatchlist(pf,stderr);})

   queue_free (branchnodes, NULL);

   DBGMSGLVL(1,"Section of added code contains %d branch instructions\n",n_branch);
   return n_branch;
}
#endif

/***Functions used for updating a program***/
#if 0 //That one was not useful before, I see no reason to keep it now. Maybe for some tests when I'm pulling hairs trying to make this work... (2014-09-30)
/**
 * Writes a patched file to a file whose name or descriptor is given as parameter
 * \param pf A pointer to the structure holding the patched file
 * \param filename The name of the file to write to
 * \param filedes The file descriptor of the file to write to (if filename is NULL)
 * \note This function is mainly kept around for testing purposes
 * */
void patchfile_tofile(patchfile_t* pf,char* filename,int filedes) {
   /**\todo Update this function to take an asmfile as parameter. And move it elsewhere*/

   create_elf_file(pf->efile,filename,filedes);

}
#endif

#if 0 //To be done differently and in one go
/**
 * Updates the binary coding of a disassembled section from its instruction list
 * \param pf A pointer to the structure holding the disassembled file
 * \param scnidx The index of the instruction list in the array of lists in pf
 * \param ispatched The section is the patched list
 * */
static void patchfile_updscncoding(patchfile_t* pf, int scnidx, int ispatched) {
   unsigned char* codbin;
   int codlen;
   if (ispatched == FALSE) {
      /*Retrieves binary coding corresponding to the instruction list*/
      DBGMSG("Updating section %d (bounds: %#"PRIx64" - %#"PRIx64")\n",
           scnidx, INSN_GET_ADDR(GET_DATA_T(insn_t*, pf->codescn_bounds[scnidx].begin)),
           INSN_GET_ADDR(GET_DATA_T(insn_t*, pf->codescn_bounds[scnidx].end)));

        codbin = insnlist_getcoding(asmfile_get_insns(pf->afile),&codlen,pf->codescn_bounds[scnidx].begin,pf->codescn_bounds[scnidx].end->next);
      DBGMSG("New code size is %d\n",codlen);
        DBGLVL(4,int i;for(i=0;i<codlen;i++) fprintf(stderr,"%x ",codbin[i]);fprintf(stderr,"\n"););
      /*Updates section data with new binary coding*/
      elffile_upd_progbits(pf->efile,codbin,codlen,pf->codescn_idx[scnidx]);
      lc_free(codbin);
   } else if (pf->patch_list != NULL) {
      /*Retrieves binary coding corresponding to the instruction list*/
      codbin = insnlist_getcoding(pf->patch_list,&codlen,NULL,NULL);
      DBGMSG("Updating inserted section: code size is %d\n",codlen);
        DBGLVL(4,int i;for(i=0;i<codlen;i++) fprintf(stderr,"%x ",codbin[i]);fprintf(stderr,"\n"););
      /*Updates section data with new binary coding*/
      elffile_upd_progbits(pf->efile,codbin,codlen,pf->codescn_idx[scnidx]);
      lc_free(codbin);
   }
}

/**
 * Links a jump instruction to its destination and updates the branch table
 * \param pf Pointer to the structure holding the disassembled file
 * \param jmp The jump instruction to link
 * \param dest The destination instruction
 * */
void patchfile_setbranch(patchfile_t* pf,insn_t* jmp,insn_t* dest) {
/** \todo Check this function is used everywhere in this file*/
    /*Retrieves the old destination of the branch*/
    int64_t olddestaddr = INSN_GET_ADDR(insn_get_branch(jmp));
#ifndef NDEBUG //To avoid compilation warnings when not in debug mode
    int res =
#endif
    hashtable_remove_elt(pf->branches,(void*)olddestaddr,jmp);
    DBGMSG("Instruction %p pointing to %#"PRIx64" (%p) was %s branches table\n",jmp,olddestaddr, insn_get_branch(jmp),(res==1)?"removed from":"not found in");
    /*Points the jmp instruction to the beginning of the basic block*/
    insn_set_branch(jmp,dest);
    /*Stores this jump into the branch hashtable*/
    hashtable_insert(pf->branches,(void *)INSN_GET_ADDR(dest),jmp); /*Dirty, but effective*/

    DBG(char deststr[256];char jmpstr[256];insn_print(dest, deststr, sizeof(deststr));insn_print(jmp, jmpstr, sizeof(jmpstr));
        DBGMSG("Linking instruction \"%#"PRIx64":%s\" (%p) to instruction \"%#"PRIx64":%s\" (%p)\n",INSN_GET_ADDR(jmp),jmpstr,jmp,
            INSN_GET_ADDR(dest),deststr,dest));
}

/*
 * Links a jump instruction to its destination, without updating the main table for storing branches. This is intended
 * for branches that should not be updated when insertions are performed before the destination.
 * \param pf Pointer to the structure holding the disassembled file
 * \param jmp The jump instruction to link
 * \param dest The destination instruction
 * */
void patchfile_setbranch_noupd(patchfile_t* pf, insn_t* jmp, insn_t* dest) {
   /**\todo Factorise the code of this function with the patchfile_setbranch*/
   /**\todo Add a return value for error handling*/
   /*We will actually load the branch in the branches_noupd hashtable, to be able to still update the destination
    * if it is deleted or modified*/
   /*Retrieves the old destination of the branch*/
    int64_t olddestaddr = INSN_GET_ADDR(insn_get_branch(jmp));
#ifndef NDEBUG //To avoid compilation warnings when not in debug mode
   int res =
#endif
   hashtable_remove_elt(pf->branches_noupd,(void*)olddestaddr,jmp);
   DBGMSG("Instruction %p pointing to %#"PRIx64" was %s branches table\n",jmp,olddestaddr,(res==1)?"removed from":"not found in");
   /*Points the jmp instruction to the beginning of the basic block*/
    insn_set_branch(jmp,dest);
   /*Stores this jump into the branch hashtable*/
    hashtable_insert(pf->branches_noupd,(void *)INSN_GET_ADDR(dest),jmp); /*Dirty, but effective*/

   DBG(char deststr[256];char jmpstr[256];insn_print(dest, deststr, sizeof(deststr));insn_print(jmp, jmpstr, sizeof(jmpstr));
        DBGMSG("Linking instruction \"%#"PRIx64":%s\" (%p) to instruction \"%#"PRIx64":%s\" (%p)\n",INSN_GET_ADDR(jmp),jmpstr,jmp,
            INSN_GET_ADDR(dest),deststr,dest));
}
#endif

/**
 * Creates the patchinsn_t structure corresponding to a branch
 * \param pf Patched file
 * \param originbranch The original branch instruction
 * \param newdest The new destination of the patched instruction
 * */
static void patchfile_createpatchbranch(patchfile_t* pf, insn_t* originbranch,
      insn_t* newdest)
{
   assert(pf && originbranch && newdest);
   patchinsn_t* patchbranch = patchfile_createpatchinsn(pf, originbranch,
         originbranch, NULL);
   //Flags the instruction as being updated because of the patched operation
   insn_add_annotate(originbranch, A_PATCHUPD);
   patchfile_setbranch(pf, patchbranch->patched, newdest, NULL);
}

/**
 * Retrieves a function encompassing an instruction.
 * The "function" is defined as the instructions between the first label located before the instruction up to
 * the first label located after the instruction
 * \param insnl List object containing the instruction
 * \param startfct If not NULL, will contain the list element located at the beginning of the function
 * \param fctlen If not NULL, will contain the length in bits of the function
 * \return 1 if the function is found to contain an indirect branch instruction, 0 otherwise
 * \todo Add another return code for error
 * */
static int patchfile_getfunction(list_t* insnl, list_t** startfct,
      uint64_t *fctlen)
{
   list_t* iter, *ffirst = insnl;
   int hasindirect = 0;
   uint64_t flen = 0;
   label_t* lbl;

   iter = ffirst;
    lbl = insn_get_fctlbl(INSN_INLIST(insnl));

   /*Adds the instruction's size to the size of the function*/
    flen+= insn_get_bytesize(INSN_INLIST(ffirst));
   /*Detects indirect branches in the function*/
    if( insn_is_indirect_branch(INSN_INLIST(ffirst)) )
      hasindirect = 1;
   /*Steps to next instruction*/
   iter = ffirst->next;
   /*Finds the end of the function*/
   while ((iter)/*Instruction exists*/
   /*Instruction label is the same as the label of the instruction*/
            &&( insn_get_fctlbl(INSN_INLIST(iter)) == lbl ) ) {
      DBGMSGLVL(1,
            "Forward search: instruction %p at address %#"PRIx64" follows label %s\n",
            INSN_INLIST(iter), insn_get_addr(INSN_INLIST(iter)),
            label_get_name(lbl));
      /*Detects indirect branches in the function*/
        if( insn_is_indirect_branch(INSN_INLIST(iter)) )
         hasindirect = 1;
      /*Increases the function size*/
      flen += insn_get_bytesize(INSN_INLIST(iter));
      iter = iter->next;
   }
    /*We can't use insn_get_fctlbl here, unless we make some change in the displace block function: if the beginning of the function was
    * moved, it has been replaced by new instructions that don't have a label. To change this we have to update any insertion of a basic
    * block so that the inserted instructions have the label they belong to => Should be done now, under tests*/
   if (/*Instruction label is the same as the label of the instruction*/
            ( insn_get_fctlbl(INSN_INLIST(ffirst)) == lbl ) ) {
      /*Instruction at the given address is not the beginning of the block*/
      /*Adds all the previous instructions to the block until a branch or branch destination is found*/
        while( (ffirst->prev) && ( insn_get_fctlbl(INSN_INLIST(ffirst->prev)) == lbl ) ) {
         /*Detects indirect branches in the function*/
            if( insn_is_indirect_branch(INSN_INLIST(ffirst)) )
            hasindirect = 1;
         DBGMSGLVL(1,
               "Backward search: instruction %p at address %#"PRIx64" follows label %s\n",
               INSN_INLIST(ffirst), insn_get_addr(INSN_INLIST(ffirst)),
               label_get_name(lbl));
         /*Otherwise, we move the beginning of the function back one instruction and increase the
          * size of the block*/
         ffirst = ffirst->prev;
         flen += insn_get_bytesize(INSN_INLIST(ffirst));
         /*If the new beginning of the function is pointed to by its label*/
            if ( label_get_target(lbl) == INSN_INLIST(ffirst) )
            break;
      }
   }
   if (startfct) {
      *startfct = ffirst;
   }
   if (fctlen) {
      *fctlen = flen;
   }
   DBG(
         FCTNAMEMSG("Function around instruction %#"PRIx64":", insn_get_addr(INSN_INLIST(insnl))); insn_fprint(INSN_INLIST(insnl),stderr); STDMSG(" begins at %#"PRIx64":", insn_get_addr(INSN_INLIST(insnl))); insn_fprint(INSN_INLIST(ffirst), stderr); STDMSG(" and is %"PRId64" bytes long\n", flen););

   return hasindirect;
}

/**
 * Adds nop instructions following a basic block to the block. What is actually done is reaching the first instruction that is not a nop
 * and add the length of the nop instructions found to its length. This function is intended to be invoked from \ref patchfile_getbasicblock
 * and is only used to factorise the code
 * \param pf Structure holding the patched file details
 * \param iter Node containing the first instruction after the end of the block
 * \param scnidx Identifier of the section containing the code
 * \param len Pointer to the length of the nop. Will be updated after completion. Assumed to be not NULL
 * \return Pointer to the node containing the first non-nop instruction after the block
 * */
static list_t* add_nops_to_block(patchfile_t* pf, list_t* iter, binscn_t* scn,
      uint64_t* len)
{
   uint64_t blen = *len;
   list_t* blast = iter;
   /*We append to it all the nop instructions that follow the branch (this mainly covers the case of NOP at the end of functions)*/
   while ((iter)/*Instruction exists*/
   /*Instruction is a nop*/
   && (pf->patchdriver->instruction_is_nop(INSN_INLIST(iter)))
   /*Instruction is not a branch destination*/
   && (!hashtable_lookup(pf->branches, (insn_t*) iter->data))
         /**\todo Bug here: in move 1 instruction mode, we will see as the beginning of a block an instruction pointed to by a
          * return branch from a displaced block. However we can't simply go past that branch, as what we have before are either
          * NOPs from this displaced instruction or the JMP to the displaced block, and we have to adapt to that*/
         /*Instruction does not correspond to a label (for instance main)*/
         && (!binfile_lookup_label_at_addr(pf->bfile, scn,
               insn_get_addr(GET_DATA_T(insn_t*, iter))))) {
      blen += insn_get_bytesize((insn_t*) iter->data);
      blast = iter;
      iter = iter->next;
   }
   *len = blen;
   return blast;
}

/**
 * Adds the instructions before a block to the block until an branch instruction or destination is reached, or the minimal block size
 * is reached (if we are displacing single instructions instead of basic blocks). This function is intended to be invoked from \ref patchfile_getbasicblock
 * and is only used to factorise the code
 * \param pf Structure holding the patched file details
 * \param bfirst Node containing the first instruction of the block
 * \param scnidx Identifier of the section containing the code
 * \param len Pointer to the length of the nop. Will be updated after completion. Assumed to be not NULL
 * \param move1insn Set to 1 if single instructions are displaced instead of whole basic blocks
 * \param minsize Minimal size to reach for the basic block
 * \return Pointer to the node containing the beginning of the block
 * */
static list_t* add_previous_to_block(patchfile_t* pf, list_t* bfirst,
      binscn_t* scn, uint64_t *len, int move1insn, uint64_t minsize)
{
   uint64_t blen = *len;
   /*Adds all the previous instructions to the block until a branch or branch destination is found, or an instruction added by the patcher*/
   while ((bfirst->prev)
         && (!insn_check_annotate(GET_DATA_T(insn_t*, bfirst->prev), A_PATCHNEW))) {
      /*If the instruction before the beginning of the block is a branch, we stop here*/
      if (insn_is_branch((insn_t*) bfirst->prev->data) != 0)
         break;
      /*Otherwise, we move the beginning of the block back one instruction and increase the
       * size of the block*/
      bfirst = bfirst->prev;
      blen += insn_get_bytesize((insn_t*) bfirst->data);
      /*If the new beginning of the block is the destination of a branch or a label,
       * we stop here*/
      if ((hashtable_lookup(pf->branches, (insn_t*) bfirst->data))
            || (binfile_lookup_label_at_addr(pf->bfile, scn,
                  insn_get_addr(GET_DATA_T(insn_t*, bfirst)))))
         break;
      /*If we only move 1 instruction instead of the whole block and we reached the right size, we stop here*/
      if ((move1insn) && (blen >= minsize))
         break;
      //Checking that the instruction is not at the beginning of a list of instructions (interleaved data for example)*/
      if (insn_check_annotate(GET_DATA_T(insn_t*, bfirst), A_BEGIN_LIST))
         break;
   }
   *len = blen;
   return bfirst;
}

#if 0
/**
 * Retrieves the index of the section to which an instruction belongs, based on its annotate.
 * THIS IS TEMPORARY AND SHOULD BE REMOVED, DELETED, DESTROYED, AND SALTED OVER WHEN THE LIBBIN IS IMPLEMENTED.
 * I MEAN IT (2014-06-13)
 * \param pf Patchfile structure
 * \param in Instruction for which we want the section
 * \return Index of the section, or -1 of it does not have an annotate indicating its section
 * */
static int patchfile_getinsnscn(patchfile_t* pf, insn_t* in) {
   assert(pf && in);
   int scnid = -1;
   uint32_t anno = insn_get_annotate(in);
   if (anno&A_SCTINI)
   scnid = pf->scniniidx;
   else if (anno&A_SCTPLT)
   scnid = pf->scnpltidx;
   else if (anno&A_SCTTXT)
   scnid = pf->scntxtidx;
   else if (anno&A_SCTFIN)
   scnid = pf->scnfinidx;
   DBGLVL(1, char instr[256];insn_print(in, instr, sizeof(instr));
       DBGMSG("Instruction \"%#"PRIx64":%s\" (%p) is in section %d\n",INSN_GET_ADDR(in), instr, in, pf->codescn_idx[scnid]) );
   return scnid;
}

/**
 * \brief Retrieves a basic block encompassing an address
 *
 * The block:
 * - starts at the first instruction before the given address that is the destination of a branch,
 * or after the first branch instruction found before the given address (or the beginning of the section)
 * - ends before the first instruction after the given address that is the destination of a branch
 * or a branch instruction (or the end of the section)
 * - if the address given corresponds to a branch instruction, the block will be the branch instruction itself and all following NOP instructions
 * (so this is not quite the same as the "official" definition of a basic block as it won't include the last branch instruction)
 * This function also returns an extended basic block
 * This extended basic block the additional instructions:
 * - if the original instruction was a non-branch instruction, the branch instruction (and the possible subsequent NOP instructions) following the original basic block
 * - if the original instruction was a branch, all consecutive branch instructions and the first non-branch instruction encountered, until the minimal size is reached
 * \param pf Pointer to the structure containing the disassembled ELF file
 * \param addr List object containing the instruction around which we are looking for a basic block
 * \param len Will contain the length in bits of the found block
 * \param minsize The minimal size in bits a basic block must have (usually the size needed for the jump instruction
 * that will be inserted in its place)
 * \return A pointer to the list node containing the first instruction in the block
 * \todo Add a return parameter specifying if this contains a branch instruction, perhaps. Or this should be all done in the calling function.
 * \todo Remove addr as parameter, only use the list_t object containing the instruction (but we need to also pass scndix)
 * \todo Factorise this code. There is now little need to separate the case between branch/non branch for the instruction (and in case you are wondering, yes,
 * there was a time when it was needed because the behaviour was not the same) => Not that easy. We still have some major differences. Factorising into sub
 * functions could help => Partly done with \ref add_previous_to_block and \ref add_nops_to_block
 * */
static list_t* patchfile_getbasicblock(patchfile_t* pf, list_t* inseq,uint64_t* len,uint64_t minsize) {
   assert (pf && inseq && len);
   uint64_t blen=0;
   list_t* bfirst=NULL,*iter;
   int scnidx;
   int move1insn = (pf->current_flags&PATCHFLAG_MOV1INSN)?TRUE:FALSE;
   /**\todo REFONTE Renvoyer ici une structure movedblock_t. Sinon, on fait le calcul ci-dessous pour trouver un movedblock_t,
    * on le crée, et on le renvoie*/

    int64_t addr = INSN_GET_ADDR(INSN_INLIST(inseq));
   DBGMSGLVL(1,"Looking for %s around instruction at address %#"PRIx64"\n",(move1insn)?"one instruction (if possible)":"one full basic block", addr);

   scnidx = patchfile_getinsnscn(pf, INSN_INLIST(inseq));
   if (scnidx >= 0) {
      //we could find the section of the instruction
      bfirst = inseq;
   } else {
      //We need to find its section
      patchfile_getinsnataddr(pf,addr,&scnidx,&bfirst);
   }

   //Looks for the instruction at the given address
   if(scnidx >= 0) {
        if(!insn_is_branch((insn_t*)bfirst->data)) {//Instruction is not a branch
         /*Adds the instruction's size to the size of the block*/
            blen+= insn_get_size((insn_t*)bfirst->data);
         /*Steps to next instruction*/
         iter = bfirst->next;
         /*Finds the end of the basic block*/
         while((iter)/*Instruction exists*/
               /*Instruction is not a branch*/
                    &&(!insn_is_branch((insn_t*)iter->data))
               /*Instruction is not a branch destination*/
                    &&(!hashtable_lookup(pf->branches,(void *)INSN_GET_ADDR((insn_t*)iter->data)))
               /*Instruction does not correspond to a label (for instance main)*/
                    &&(!elffile_getlabel(pf->efile,INSN_GET_ADDR((insn_t*)iter->data),pf->codescn_idx[scnidx]))
               /*Block length has not reached minsize or we are in the mode where we move whole basic blocks*/
               &&( (!move1insn)||(blen < minsize) ) ) {
                blen += insn_get_size((insn_t*)iter->data);
            iter = iter->next;
         }
         DBGMSGLVL(1,"End of %s found at %#"PRIx64"\n",
               (move1insn) ? "instruction (or small basic block)" : "basic block", insn_get_addr(GET_DATA_T(insn_t*, iter)));
         /*Adding following branch instruction (if existing)*/
         if(
               /*Instruction exists*/
               (iter)
               /*Next instruction is a branch*/
                    &&(insn_is_branch(INSN_INLIST(iter)))
               /*Instruction is not a branch destination*/
                    &&(!hashtable_lookup(pf->branches,(void *)INSN_GET_ADDR((insn_t*)iter->data)))
               /*Instruction does not correspond to a label (for instance main)*/
                    &&(!elffile_getlabel(pf->efile,INSN_GET_ADDR((insn_t*)iter->data),pf->codescn_idx[scnidx]))
               /*Block length has not reached minsize or we are in the mode where we move whole basic blocks*/
               &&( (!move1insn)||(blen < minsize) ) ) {
                blen = blen+insn_get_size(INSN_INLIST(iter));
            //Adding following nops
            iter = add_nops_to_block(pf, iter, scnidx, &blen);
         }
         DBGMSGLVL(1,"End of %s found at %#"PRIx64" after adding following branch and nop instructions\n",
               (move1insn) ? "instruction (or small basic block)" : "basic block", (iter) ? insn_get_addr(GET_DATA_T(insn_t*, iter)) : -1);
         if( /*Instruction is not  a branch destination*/
                    (!hashtable_lookup(pf->branches,(void *)INSN_GET_ADDR((insn_t*)bfirst->data)))
               /**\todo There is a potential bug here, if we encounter an instruction with address -1. A more thorough
                * check would be needed (that the value found is not the instruction)*/
               /*Instruction does not correspond to a label (for instance main)*/
                    &&(!elffile_getlabel(pf->efile,INSN_GET_ADDR((insn_t*)bfirst->data),pf->codescn_idx[scnidx]))
               /*Block length has not reached minsize or we are in the mode where we move whole basic blocks*/
               &&( (!move1insn)||(blen < minsize) ) ) {
            /*Instruction at the given address is not the beginning of the block*/
            /*Finds the beginning of the block*/
            bfirst = add_previous_to_block(pf, bfirst, scnidx, &blen, move1insn, minsize);
            DBGMSGLVL(1,"Beginning of %s found at %#"PRIx64"\n",
                   (move1insn) ? "instruction (or small basic block)" : "basic block", insn_get_addr(GET_DATA_T(insn_t*, bfirst)));
         } else {
            DBGMSGLVL(1,"Instruction at address %#"PRIx64" is a branch destination or the block minimal size (%"PRIu64") was reached (block size is %"PRIu64")\n",
                   insn_get_addr(GET_DATA_T(insn_t*, bfirst)), minsize / 8, blen / 8);
         }
      } else {  //Instruction is a branch
         /**\todo As said in the function header, merge this case with the one above => At least the add_xxx_to_block factorised the code a bit*/
         /*Adds the instruction's size to the size of the block*/
            blen += insn_get_size((insn_t*)bfirst->data);
         /*Steps to next instruction*/
         iter = bfirst->next;
         //Adding following nops
         iter = add_nops_to_block(pf, iter, scnidx, &blen);
         DBGMSGLVL(1,"End of %s found at %#"PRIx64" after adding following nop instructions\n",
               (move1insn) ? "instruction (or small basic block)" : "basic block", (iter) ? insn_get_addr(GET_DATA_T(insn_t*, iter)) : -1);
         /*Adding preceding branch instructions and the first non-branch instruction to the basic block*/
         if( /*The branch is not itself the target of a branch instruction (can happen with RET)*/
                    (!hashtable_lookup(pf->branches,(void *)INSN_GET_ADDR((insn_t*)bfirst->data)))
               /**\todo There is a potential bug here, if we encounter an instruction with address -1. A more thorough
                * check would be needed (that the value found is not the instruction)*/
               /*Instruction does not correspond to a label (for instance main)*/
                    &&(!elffile_getlabel(pf->efile,INSN_GET_ADDR((insn_t*)bfirst->data),pf->codescn_idx[scnidx]))
               /*Block length has not reached minsize or we are in the mode where we move whole basic blocks*/
               &&( (!move1insn)||(blen < minsize) ) ) {
            /*In this case the branch is not pointed to by another branch. We will include all branch instructions
             * immediately preceding the branch, then the basic block immediately before that. */
            /**\todo Clean up this code and factorise it */
            /*Initialising iterator*/
            iter = bfirst->prev;
            /*Looking for the branch instructions immediately before that one*/
                while((iter)&&(insn_is_branch((insn_t*)iter->data))&&(blen<minsize)
                      &&(!hashtable_lookup(pf->branches,(void *)INSN_GET_ADDR((insn_t*)iter->data)))
                        &&(!elffile_getlabel(pf->efile,INSN_GET_ADDR((insn_t*)iter->data),pf->codescn_idx[scnidx]))
                  /*Block length has not reached minsize or we are in the mode where we move whole basic blocks*/
                  &&( (!move1insn)||(blen < minsize) ) ) {
                    blen += insn_get_size((insn_t*)iter->data);
               iter = iter->prev;
            }
            /*At this point we are pointing to the first instruction before the target and that is not a branch*/
            if(iter) {
               bfirst = iter->next;
            }
            /*Finds the beginning of the block*/
            bfirst = add_previous_to_block(pf, bfirst, scnidx, &blen, move1insn, minsize);
            DBGMSGLVL(1,"Beginning of %s found at %#"PRIx64"\n",
                   (move1insn) ? "instruction (or small basic block)" : "basic block", insn_get_addr(GET_DATA_T(insn_t*, bfirst)));
         } else {
            DBGMSGLVL(1,"Instruction at address %#"PRIx64" is a branch destination or the block minimal size (%"PRIu64") was reached (block size is %"PRIu64")\n",
                   insn_get_addr(GET_DATA_T(insn_t*, bfirst)), minsize / 8, blen / 8);
         }
      }
   } else {
      pf->last_error_code = ERR_LIBASM_INSTRUCTION_NOT_FOUND;
      /*Instruction not found: print an error message*/
      ERRMSG("No instruction found at address %#"PRIx64"\n",addr);
   }
   if(bfirst) {
        DBGMSG("Address %#"PRIx64" is in block starting at %#"PRIx64" with length %#"PRIx64" bytes\n",addr,INSN_GET_ADDR(INSN_INLIST(bfirst)),blen/8);
      if(len) *len = blen;
      return bfirst;
   }
   else {
      pf->last_error_code = ERR_PATCH_BASIC_BLOCK_NOT_FOUND;
      return NULL;/**\todo Handle the error code return values*/
   }
}

/**
 * Replaces a block of code of a given length by a jump instruction (or instruction list) and returns the instruction that have
 * been replaced
 * \param pf Pointer to the structure holding the parsed file
 * \param jmpinl An instruction list coding a jump
 * \param jmp A pointer to an instruction in \e jmpinl containing the branch address
 * \param lststartblock The list object containing the first instruction in the block to replace
 * \param block_bsize The size in bits of the block to replace
 * \param endblock Will contain a pointer to the address of the instruction immediately after the replaced block
 * \param scnidx Index of the ELF section into which the replacement must occur
 * \param paddinginsn The instruction to use as padding for replacing moved instructions
 * \return The list of replaced instructions
 * */
static queue_t* patchfile_switchblocks(patchfile_t* pf,queue_t* jmpinl,insn_t* jmp,list_t *lststartblock,
      uint64_t block_bsize,insn_t** endblock, int scnidx, insn_t* paddinginsn) {
   uint64_t len;
   queue_t* replacedinl;
   insn_t* startblock = (insn_t*)lststartblock->data, *retinsn;
   /*Points the jmp instruction to the beginning of the basic block*/
   DBGMSG0("Pointing jump instruction to the beginning of the basic block\n");
   patchfile_setbranch(pf,jmp,startblock); /**\bug At this point, the branch destination of jmp is NULL and patchfile_setbranch may not detect it*/

   //Building a block with the same size as the block to insert
   queue_t* fillblock = queue_new();
   /*Adds the jump instruction list at the beginning of the block*/
    queue_append_and_keep(fillblock,jmpinl);
   len = insnlist_bitsize(fillblock,NULL,NULL); //Initialising the size of the block
   if(len<block_bsize) { //Checks if we need to fill the remaining of the block with NOP instructions
      /*Retrieves the size of the nop instruction*/
      /**\todo Improve the retrieval of the size of NOP instruction.*/
        int noplen = insn_get_size(paddinginsn);
      /*Adds nop instructions at the end of the block until the right length is reached*/
      while(len<block_bsize) {
         add_insn_to_insnlst(insn_copy(paddinginsn),fillblock);
         len+=noplen;
      }
   }
   DBGMSG("Replacing basic block with filler block of size %#"PRIx64" bytes\n",insnlist_bitsize(fillblock,NULL,NULL)/8);
   //Replaces the basic block with the new code
    replacedinl = insnlist_replace(asmfile_get_insns(pf->afile),fillblock,INSN_GET_ADDR(startblock),lststartblock,paddinginsn,&retinsn,&(pf->codescn_bounds[scnidx].begin),&(pf->codescn_bounds[scnidx].end));
   //queue_free(fillblock, NULL);
   //Stores the instruction immediately after the end of the block
   if(endblock) *endblock=retinsn;
   return replacedinl;
}

/**
 * Position a list object at the end of a block
 * \param startblock List object containing the instruction at the beginning of the block
 * \param pos Cursor to position start the search from. If NULL or outside the block, the search will start from startblock
 * \param blen Size in bits of the block
 * \return A pointer to the list object containing the last element in the block ! Or is it the first element after the block ???
 * \todo Use the block_t structure to handle blocks and remove this function
 * */
static list_t* reach_endblock(list_t* startblock,list_t* pos,uint64_t blen) {
   list_t* out;
    if((pos)&&((INSN_GET_ADDR(INSN_INLIST(pos))-INSN_GET_ADDR(INSN_INLIST(startblock)))>=0)
            &&((uint64_t)(INSN_GET_ADDR(INSN_INLIST(pos))-INSN_GET_ADDR(INSN_INLIST(startblock)))<=(blen/8))) {
      out = pos;
   } else {
      out = startblock;
   }
    while(out&&(((uint64_t)(INSN_GET_ADDR(INSN_INLIST(out))-INSN_GET_ADDR(INSN_INLIST(startblock)))<(blen/8)))) {
      out = out->next;
   }
    DBGMSG("End of block starting at %#"PRIx64" found at address %#"PRIx64"\n",INSN_GET_ADDR(INSN_INLIST(startblock)),INSN_GET_ADDR(INSN_INLIST(out)));
   return out;
}

/**
 * Replaces a block by a given jump instruction (filled with nops), and returns an instruction list containing the instructions of the block, followed
 * by a jump pointing to the next instruction after the block
 * \param pf Pointer to the structure containing the parsed file
 * \param scnidx Index of the code section containing the basic block we want to move
 * \param jmpinl Instruction list performing a jump operation.
 * \param jmp Pointer to the instruction in \e jmpinl
 * \param lstbb List object containing the first element of the basic block to replace
 * \param bblen Length in bits of the basic block
 * \param paddinginsn Instruction to use to replace moved instructions
 * \return An instruction list containing the displaced basic block, followed by a jump instruction pointing to the next instruction after the basic block
 * */
static queue_t* patchfile_displaceblock(patchfile_t* pf,int scnidx,queue_t* jmpinl,insn_t* jmp,list_t* lstbb,uint64_t bblen, insn_t* paddinginsn) {
   queue_t* origins,*replacedinl,*retjmpinl;
   insn_t* retinsn,*retjmp;

   /*Retrieves all instructions pointing to the beginning of the block*/
    origins=hashtable_lookup_all(pf->branches,(void *)INSN_GET_ADDR(INSN_INLIST(lstbb)));

   /*Replaces the block with the jump instruction*/
   replacedinl = patchfile_switchblocks(pf,jmpinl,jmp,lstbb,bblen,&retinsn,scnidx, paddinginsn);

   if(!replacedinl) {    //Insertion failed
        ERRMSG("Unable to insert instructions at address %#"PRIx64"\n",INSN_GET_ADDR(INSN_INLIST(lstbb)));
      pf->last_error_code = ERR_PATCH_INSERT_INSNLIST_FAILED;
      return NULL;
   }
   DBG(char jmpstr[256];insn_print(jmp, jmpstr, sizeof(jmpstr));
         DBGMSG("Replaced block of size %#"PRIx64" bytes with jump instruction \"%s\" (%p)\n",insnlist_bitsize(replacedinl,NULL,NULL)/8,jmpstr,jmp));
   /*Updates instructions pointing to the address so that they
    * now point to the inserted jump instruction instead*/
   if(origins) {
      DBGMSG("Found %d instructions pointing to %#"PRIx64" (%p)\n",queue_length(origins),insn_get_addr(INSN_INLIST(lstbb)),INSN_INLIST(lstbb));
      FOREACH_INQUEUE(origins,iter) {
           if (insn_get_branch(GET_DATA_T(insn_t*, iter)) == INSN_INLIST(lstbb))
         patchfile_setbranch(pf, GET_DATA_T(insn_t*, iter), (insn_t*)queue_peek_head(jmpinl));
      }
      queue_free(origins,NULL);
   }
   //Stores the extracted basic block into the section holding inserted code
   /*Creates a return jmp instruction*/
   retjmpinl = pf->patchdriver->generate_insnlist_jmpaddr(NULL,&retjmp);
   /*Points the return jmp instruction to the instruction immediately after the instructions
    * that have been replaced*/
   DBGMSG0("Linking return jump instruction to the end of the displaced block\n");
   patchfile_setbranch(pf,retjmp,retinsn);/**\bug At this point, the branch destination of retjmp is NULL and patchfile_setbranch may not detect it*/
   /*Appends the return jmp instruction to the extracted block*/
   queue_append(replacedinl,retjmpinl);
   /*Returns the list of instructions of the displaced block followed by the jump back*/
   return replacedinl;
}

/**
 * Looks inside an already displaced block for a sub block of suitable size to host a trampoline jump from a block to small to contain a larger jump
 * \param pf Pointer to the structure holding the disassembled file
 * \param origdist Distance from the \e iter instruction to the first instruction of the basic block we search a trampoline for. Will be updated at the end
 * \param it Pointer to the node containing the first (if \e fwd=1) or last (if \e fwd=0) instruction of the displaced basic block. will be updated at the end
 * \param minblocksize Minimal size in bits required for a basic block. If a basic block has to be moved, it must be twice this size
 * \param maxdist Maximal distance in bits acceptable from the origin to the basic block
 * \param fwd If set to 1, looks for blocks located after the origin, otherwise looks for blocks located before
 * \param after If set to 1, means that the block we are considering is located after the block we search a trampoline for, otherwise it is before it
 * \return A list object containing the first instruction in the displaced basic block of a sub block suitable to host the trampoline jump
 * \todo Have the distance a signed integer to avoid having to use \e after.
 * */
static list_t* patchfile_findtrampoline_in_movedblock(patchfile_t* pf,uint64_t *origdist,list_t** it,uint64_t minblocksize,uint64_t maxdist,int fwd,int after) {
   list_t* out=NULL,*bstartseq,*iter;
   uint64_t blen,dist=*origdist;
   iter = *it;

   DBGMSG("Looking for a trampoline in a displaced block at distance %#"PRIx64" bytes from the origin\n",*origdist/8);
   if(fwd) {
      /*Looking for the first instruction in the displaced block that is a nop*/
      while(iter&&(INSN_GET_ADDR(INSN_INLIST(iter))==-1)&&(!pf->patchdriver->instruction_is_nop(INSN_INLIST(iter)))) {
         //Keeps track of the distance
            if(after) dist+=insn_get_size(INSN_INLIST(iter));
            else dist-=insn_get_size(INSN_INLIST(iter));
         iter = iter->next;
      }
      DBGMSGLVL(1,"Empty space in moved block begins at distance %#"PRIx64" bytes from the origin\n",dist/8);
        if(iter&&(INSN_GET_ADDR(INSN_INLIST(iter))==-1)&&(dist<=maxdist)) {
         //We are still in the displaced basic block, the instruction is a nop, and the max distance is not reached
         blen=0;
         bstartseq = iter;
         //Tries to find a group of NOP instructions in the displaced block that has the required length
            while((iter)&&(INSN_GET_ADDR(INSN_INLIST(iter))==-1)&&(pf->patchdriver->instruction_is_nop(INSN_INLIST(iter)))&&(blen<minblocksize)) {
            //Keeps track of the distance
                if(after) dist+=insn_get_size(INSN_INLIST(iter));
                else dist-=insn_get_size(INSN_INLIST(iter));
                blen+=insn_get_size(INSN_INLIST(iter));
            iter = iter->next;
         }
         if (blen>=minblocksize) {
            /*We found enough NOP instructions in the block to reach the correct size*/
            out = bstartseq;
            DBGMSG("Trampoline found forward in displaced block with length %#"PRIx64" bytes at distance %#"PRIx64" bytes\n",blen/8,dist/8);
         } else { //Otherwise iter is either at the end of the displaced block or pointing to a non-nop displaced instruction
            DBGMSG("Not enough space found in displaced block (length reached %#"PRIx64" bytes instead of %#"PRIx64" at distance %#"PRIx64" bytes)\n",
                  blen/8, minblocksize/8, dist/8);
         }
      } else { //Otherwise iter is just at the end of the displaced block
         if (iter)
         DBGMSGLVL(1,"No more empty space in moved block: address %#"PRIx64" reached\n",insn_get_addr(INSN_INLIST(iter)));
      }
   } else {
      /*Looking for the first instruction in the displaced block that is a nop (in theory that should be the first encountered as we are scanning
       * in reverse, but it could be a block that was already used as a trampoline)*/
        while(iter&&(INSN_GET_ADDR(INSN_INLIST(iter))==-1)&&(!pf->patchdriver->instruction_is_nop(INSN_INLIST(iter)))) {
         iter = iter->prev;
         //Keeps track of the distance (we are going backward, so it is the size of the previous instructions that we remove/add to the distance)
            if(after) dist-=insn_get_size(INSN_INLIST(iter));
            else dist+=insn_get_size(INSN_INLIST(iter));
      }
      DBGMSGLVL(1,"Empty space in moved block begins at distance %#"PRIx64" bytes from the origin\n",dist/8);
        if(iter&&(INSN_GET_ADDR(INSN_INLIST(iter))==-1)&&(dist<=maxdist)) {
         //We are still in the displaced basic block, the instruction is a nop and the max distance is not reached
         blen=0;
         //Tries to find a group of NOP instructions in the displaced block that has the required length
            while((iter&&(INSN_GET_ADDR(INSN_INLIST(iter))==-1)&&(pf->patchdriver->instruction_is_nop(INSN_INLIST(iter))))&&(dist<=maxdist)) {
            /**\note We don't stop when minblocksize is reached, as we want to go as far back as possible in case we stumbled upon
             * a big moved block, so as to leave room for trampolines for further modifications, that will be more distant
             * since we are progressing by increasing modification address and this is the backward search.*/
                blen+=insn_get_size(INSN_INLIST(iter));
            iter = iter->prev;
            if ( !iter )
            break;
            //Keeps track of the distance
                if(after) dist-=insn_get_size(INSN_INLIST(iter));
                else dist+=insn_get_size(INSN_INLIST(iter));
         }
         if((blen>=minblocksize)/*&&(pf->patchdriver->instruction_is_nop(INSN_INLIST(iter)))*/) {
            /*We found enough NOP instructions in the block to reach the correct size*/
            out = iter->next;
            DBGMSG("Trampoline found backward in displaced block with length %#"PRIx64" bytes at distance %#"PRIx64" bytes\n",blen/8,dist/8);
         } else { //Otherwise iter is either just at the beginning of the displaced block or pointing to a non-nop instruction in the block
            DBGMSG("Not enough space found in displaced block (length reached %#"PRIx64" bytes instead of %#"PRIx64" at distance %#"PRIx64" bytes)\n",
                  blen/8, minblocksize/8, dist/8);
         }
      } else { //Otherwise iter is just at the end of the displaced block
         if (iter)
         DBGMSGLVL(1,"No more empty space in moved block: address %#"PRIx64" reached\n",insn_get_addr(INSN_INLIST(iter)));
      }
   }
   *origdist=dist;
   *it=iter;
   return out;
}

/**
 * Checks if a node is the beginning of an ELF section in the file
 * \param pf Pointer to the structure holding the details about the file being patched
 * \param node Pointer to the node to check
 * \return TRUE if the node is the beginning of an ELF section, FALSE otherwise
 * */
static int patchfile_is_elfscn_startnode(patchfile_t* pf, list_t* node) {
   int i;
   for( i = 0; i < pf->n_codescn_idx; i++)
   if ((pf->codescn_idx[i] != -1) && (pf->codescn_bounds[i].begin == node))
   return TRUE;
   return FALSE;
}

/**
 * Checks if a node is the end of an ELF section in the file
 * \param pf Pointer to the structure holding the details about the file being patched
 * \param node Pointer to the node to check
 * \return TRUE if the node is the end of an ELF section, FALSE otherwise
 * */
static int patchfile_is_elfscn_endnode(patchfile_t* pf, list_t* node) {
   int i;
   for( i = 0; i < pf->n_codescn_idx; i++)
   if ((pf->codescn_idx[i] != -1) && (pf->codescn_bounds[i].end == node))
   return TRUE;
   return FALSE;
}

/**
 * Looks for a block of a suitable size to host a trampoline jump from a block too small to contain a larger jump
 * \param pf Pointer to the structure holding the disassembled file
 * \param origin Pointer to the list object from which we are performing the search
 * \param bdist Pointer to the original distance in bits from the \e origin pointer to the small basic block. Will be updated on exit
 * \param minblocksize Minimal size required for a basic block. If a basic block has to be moved, it must be twice this size
 * \param maxdist Maximal distance acceptable from the origin to the basic block
 * \param fwd If set to 1, looks for blocks located after the origin, otherwise looks for blocks located before
 * \param tblen If not NULL, will contain the size of the found basic block. If the block found is already displaced, will be set to 0
 * \return A pointer to the list object containing the closest instruction belonging to a large enough basic block
 * and that can be used for inserting a trampoline, or NULL if no such block was found
 * \note This function will probably be needed only for x86, where there are multiple sizes of jump instruction
 * \todo Find which is the most useful way of choosing the instruction to return between the closest or farthest instruction, possibly depending
 * on the direction we are searching
 * \todo The check on distances from the original instruction to the trampoline may not be optimal (possibly considering the maximal distance between the
 * origin and the trampoline has been reached when one more instruction could be reached). To be checked
 * */
static list_t* patchfile_findtrampolineblock(patchfile_t *pf,list_t* origin,uint64_t *bdist,uint64_t minblocksize,uint64_t maxdist,int fwd,uint64_t* tblen) {
   uint64_t blen,dist; //blen is the size in bits of a block, dist is the distance in bits from the basic block
   list_t* iter,*bstartseq,*out=NULL;
   int64_t originaddr;//Address of the origin (where the replacement should take place)

   DBGMSG("Looking for a trampoline starting at address %#"PRIx64" of minimal size %#"PRIx64" bytes at max distance %#"PRIx64" bytes\n",
            (fwd==1)?INSN_GET_ADDR(INSN_INLIST(origin)):INSN_GET_ADDR(INSN_INLIST(origin->next)),minblocksize/8,maxdist/8);
   dist = *bdist;
   if(fwd) { //Searching forward
      /**\todo Since we are separating those two cases, there is probably some optimisation to be done due to the fact that we know we always discover a
       * new block from its beginning or its end => Maybe not so much now that a "basic block" can be one or more instructions if the mov1insn mode is on*/
      iter = origin;
        originaddr = INSN_GET_ADDR(INSN_INLIST(origin));
      while( (iter) && (dist<maxdist) && (patchfile_is_elfscn_endnode(pf, iter) == FALSE) ) {
         /*The test on the node is to prevent attempting to create a trampoline in a different section the one we are in, as it could be
          * dangerous if for instance the next or previous section is the plt*/
         DBGMSG("Looking for a trampoline around address %#"PRIx64" distance %#"PRIx64" bytes forward\n",
                  ((INSN_GET_ADDR(INSN_INLIST(iter))>-1)?INSN_GET_ADDR(INSN_INLIST(iter)):INSN_GET_ADDR(INSN_INLIST(origin))+(int64_t)(dist-*bdist)/8),dist/8);
            if(INSN_GET_ADDR(INSN_INLIST(iter))>-1) {//This is not an already displaced block
            //Looking for a basic block encompassing this instruction
            bstartseq = patchfile_getbasicblock(pf,iter,&blen,2*minblocksize);
                dist += 8*(INSN_GET_ADDR(INSN_INLIST(iter))-INSN_GET_ADDR(INSN_INLIST(bstartseq)));//dist now contains the distance from the origin to the beginning of the block
            if((blen>=(2*minblocksize))
                  &&((dist+minblocksize)<=maxdist)
                        &&((INSN_GET_ADDR(INSN_INLIST(bstartseq)))>= originaddr ) ) {
               /*We have found the instruction inside a big enough block and its beginning plus the minimal block size is at a distance from the origin
                * smaller than the maximal distance and the beginning of the basic block is located after the origin point*/
               out = bstartseq; //Storing the list object containing the beginning of the block
               if(tblen) *tblen=blen;//Storing the length of the block
               DBGMSG("Trampoline found in block beginning at %#"PRIx64" with length %#"PRIx64" bytes at distance %#"PRIx64" bytes\n"
                            ,INSN_GET_ADDR(INSN_INLIST(out)),blen/8,dist/8);
               break;//Exiting the main loop scanning for basic blocks
            } else {                //The basic block is not big enough
               iter = reach_endblock(bstartseq,iter,blen);//Positions the cursor at the end of the block
               dist+= blen;
               DBGMSGLVL(1,"Trampoline in block beginning at %#"PRIx64" is not large enough (%"PRIu64" bytes instead of %"PRIu64"), "
                     "overlaps with block to move (begins at %#"PRIx64" should be above %#"PRIx64"), or is too far"
                     "(distance %"PRIu64" bytes while max is %"PRIu64")\n",
                                    INSN_GET_ADDR(INSN_INLIST(bstartseq)),blen/8,2*minblocksize/8,
                                    (INSN_GET_ADDR(INSN_INLIST(bstartseq))),originaddr,
                     dist+minblocksize,maxdist);
            }
         } else {       //This instruction belongs to an already displaced block
            if((out=patchfile_findtrampoline_in_movedblock(pf,&dist,&iter,minblocksize,maxdist,fwd,1))) {
               if(tblen) *tblen=0;
               break;          //Exiting the main loop scanning for basic blocks
            }
         }
      }
   } else {                //Searching backward
      iter = origin;
        originaddr = INSN_GET_ADDR(INSN_INLIST(origin->next));//When this function is invoked in backward search, origin is the instruction preceding the one where the insertion should take place
      while( (iter) && (dist<maxdist) && (patchfile_is_elfscn_startnode(pf, iter->next) == FALSE) ) { //This is invoked with iter->prev
         /*The test on the node is to prevent attempting to create a trampoline in a different section the one we are in, as it could be
          * dangerous if for instance the next or previous section is the plt*/
         DBGMSG("Looking for a trampoline around address %#"PRIx64" distance %#"PRIx64" bytes backward\n",
                  ((INSN_GET_ADDR(INSN_INLIST(iter))>-1)?INSN_GET_ADDR(INSN_INLIST(iter)):INSN_GET_ADDR(INSN_INLIST(origin->next))-(int64_t)(dist-*bdist + insn_get_size(INSN_INLIST(origin)))/8),dist/8);
            if(INSN_GET_ADDR(INSN_INLIST(iter))>-1) {//This is not an already displaced block
            //Looking for a basic block encompassing this instruction
            bstartseq = patchfile_getbasicblock(pf,iter,&blen,2*minblocksize/*,NULL,NULL*/);
                dist += 8*(INSN_GET_ADDR(INSN_INLIST(iter))-INSN_GET_ADDR(INSN_INLIST(bstartseq)));//dist now contains the distance from the origin to the beginning of the block
            if((blen>=(2*minblocksize))
                  &&((dist+minblocksize)<=maxdist)
                        &&((INSN_GET_ADDR(INSN_INLIST(bstartseq))+(int64_t)blen/8)<=originaddr ) ) {
               /*We have found the instruction inside a big enough block, and the end of the block minus the minimal size is at a distance from the origin
                * smaller than the maximal acceptable distance, and the end of the block is located before the origin point
                * (we just don't want any overlap) */
               out = bstartseq; //Storing the list object containing the beginning of the block
               if(tblen) *tblen=blen;//Storing the length of the block
               DBGMSG("Trampoline found in block beginning at %#"PRIx64" with length %#"PRIx64" bytes at distance %#"PRIx64" bytes\n"
                            ,INSN_GET_ADDR(INSN_INLIST(out)),blen/8,dist/8);
               break;//Exiting the main loop scanning for basic blocks
            } else {                //The basic block is not big enough
               iter = bstartseq;//Positions the cursor at the beginning of the block
               DBGMSGLVL(1,"Trampoline in block beginning at %#"PRIx64" is not large enough (%"PRIu64" bytes instead of %"PRIu64"), "
                     "overlaps with block to move (begins at %#"PRIx64", should be below %#"PRIx64"), or is too far"
                     "(distance %"PRIu64" bytes while max is %"PRIu64")\n",
                                    INSN_GET_ADDR(INSN_INLIST(bstartseq)),blen/8,2*minblocksize/8,
                                    (INSN_GET_ADDR(INSN_INLIST(bstartseq))+(int64_t)blen/8),originaddr,
                     dist+minblocksize,maxdist);
            }
            iter = iter->prev;
            if(iter)
                    dist+=insn_get_size(INSN_INLIST(iter));
         } else {       //This instruction belongs to an already displaced block
            if((out=patchfile_findtrampoline_in_movedblock(pf,&dist,&iter,minblocksize,maxdist,fwd,0))) {
               if(tblen) *tblen=0;
               break;          //Exiting the main loop scanning for basic blocks
            }
         }
      }
   }
   *bdist = dist;
   return out;
}

/**
 * Creates a new code section in a parsed file with a corresponding ELF section index set to -1 (this section will be used to add displaced code)
 * \param pf Pointer to the structure holding the disassembled file
 * */
static void patchfile_addcodescn(patchfile_t* pf) {
   pf->codescn_idx = (int*)lc_realloc(pf->codescn_idx,sizeof(int)*(pf->n_codescn_idx+1));
   pf->codescn_idx[pf->n_codescn_idx] = -1;
   pf->n_codescn_idx++;
   pf->patch_list = queue_new();
}

/**
 * Moves the basic block encompassing an instruction in another section.
 * The following operations are performed:
 * The basic block surrounding the instruction is found. It is then moved into the instruction
 * list storing the insertions (created if not existing) (it is the last instruction list in
 * the array of the disassembled file, which is associated to a ELF section of index -1,
 * meaning it does not correspond yet to any ELF section). A jump instruction is inserted at
 * \e addr, replacing the block (padding with nop instructions until the correct length is reached).
 * The jump instruction points to the beginning of the extracted block. A return jump, pointing to
 * the next instruction after the original block, is appended at the end of this displaced block.
 * The addresses of the instructions from the displaced block are not updated, while the inserted JUMP
 * and NOP instructions have an address of -1
 * \param pf Pointer to the structure holding the disassembled file
 * \param insnseq Pointer to the list object containing the instruction to be moved
 * \param insnscn Index of the section containing the instruction
 * \param flags Flags altering the behaviour of the patcher for this modification
 * \param paddinginsn Instruction to use to replace moved instructions
 * \return EXIT_SUCCESS if the block was successfully moved, error code otherwise
 * \todo Implement handling of error code
 * */
static int patchfile_moveblock(patchfile_t* pf,list_t* insnseq,int insnscn, int flags, insn_t* paddinginsn) {
   /* We will perform the following operations:
    * - Retrieve the basic block encompassing the instruction and the function if the original basic block is too small and moving functions is enabled
    * (see \ref patchfile_getbasicblock for definitions of the basic blocks used)
    * - If the basic block is larger than a jump instruction, there will be a replacement of the block
    * - If the basic block is smaller than a jump instruction
    *      -- If moving functions is enabled and the function is larger than the jump instruction, there will be a replacement of the function
    *      -- If there is a smaller jump instruction which is smaller than the basic block, the trampoline method will be attempted on the block (small jump pointing to a larger jump)
    *      -- If there is a smaller jump instruction which is larger than the basic block but smaller than the function and moving functions is enabled, the trampoline method
    *          will be attempted on the function
    *      -- If there is no smaller jump instruction or it is also larger than the block, moving functions is disabled or
    *          the function is smaller than all available jump instruction, or the trampoline method was attempted but no trampoline could be found,
    *          ---- If insertions enforcing is enabled, a warning is printed and there will be a replacement of the block.
    *          ---- If insertions enforcing is disabled, an error message will be printed and this patching operation will be aborted
    */
   uint64_t blocklen,jmplen,extblocklen = 0,wbblen; //wblen: working block length, will be either blocklen or wblen
   list_t* lststartblock,*extlststartblock = NULL,*wbbstart;//wbbstart: working basic block start, will be either lststartblock or extlststartblock
   queue_t* replacedinl,*jmpinl = NULL,*sjmpinl = NULL,*trjmpinl;
   insn_t* trjmp,*jmp,*sjmp;
   uint64_t sjmplen = 0,maxdistpos,maxdistneg,tramplen;
   int sizeok=1;//Flag indicating we have found a basic block of the correct size
   int trampmethod=0;//Is set to 1 if we will use a trampoline
   int trampok=0;//Is set to 1 if the trampoline method worked
   int fcthasindirect = 0;//Set to 1 if the function containing the instruction contains an indirect branch
   int move1insn = (flags&PATCHFLAG_MOV1INSN)?TRUE:FALSE;
   int forceinserts = (flags&PATCHFLAG_FORCEINSERT)?TRUE:FALSE;
   int movefcts = (flags&PATCHFLAG_MOVEFCTS)?TRUE:FALSE;
   int out = EXIT_SUCCESS;

   /*Create the instruction list storing the inserted list if it didn't exist*/
   if(pf->patch_list == NULL) {
      DBGMSG0("Creating instruction list for inserted instructions\n");
      patchfile_addcodescn(pf);
   }
   /*Retrieves the jump instruction list for the relevant architecture*/
   jmpinl = pf->patchdriver->generate_insnlist_jmpaddr(NULL,&jmp);
   DBG(char jmpstr[256];insn_print(jmp, jmpstr, sizeof(jmpstr));
         DBGMSG("Branch instruction created: %p (%s)\n",jmp,jmpstr));
   /*Calculates the size of the jump instruction list*/
   jmplen = insnlist_bitsize(jmpinl, NULL, NULL);

//    if (move1insn != FALSE) {
//        /*Special mode: we will only move an instruction instead of a basic block*/
//        lststartblock = insnseq;
//        blocklen = insn_get_size(INSN_INLIST(insnseq));
//        if ( (forceinserts != FALSE) && ( blocklen < jmplen) ) {
//            /*Special special mode: if we lack space, we will try to add the next instruction(s) in the function until we reach
//             * the right size.
//             * WARNING! In this mode there is absolutely no test on the presence of branches or whatever on the instructions we are
//             * moving*/
//            list_t* iter = insnseq;
//            while ( ( blocklen < jmplen ) && ( iter != NULL ) && ( insn_get_fctlbl(INSN_INLIST(iter)) == insn_get_fctlbl(INSN_INLIST(insnseq)) ) ) {
//                iter = iter->next;
//                blocklen+= insn_get_size(INSN_INLIST(insnseq));
//            }
//            if (blocklen < jmplen) {
//                /*However in that case we won't perform the patch if there is not enough size after the instruction in this function*/
//                ERRMSG("Insufficient size after instruction %#"PRIx64" in function %s. No patch done at this address\n",
//                        INSN_GET_ADDR(INSN_INLIST(insnseq)), label_get_name(insn_get_fctlbl(INSN_INLIST(insnseq))));
//                return EXIT_FAILURE;
//            }
//        }
//    } else {
   //Retrieving the basic block encompassing the address
   lststartblock = patchfile_getbasicblock(pf,insnseq,&blocklen,jmplen);
   /**\todo To avoid looking twice for the instruction, insnseq should be passed as parameter*/
   //Retrieving the small jump instruction if it exists
   sjmpinl = pf->patchdriver->generate_insnlist_smalljmpaddr(NULL,&sjmp,&maxdistpos,&maxdistneg);//This function must returns NULL if no smaller jump exists
   /**\todo We retrieve this instruction even though it may not be needed it, but this avoids a lot of if/else statements afterwards. Find a better way*/
   if(sjmpinl)
   sjmplen = insnlist_bitsize(sjmpinl, NULL, NULL);
   else
   sjmplen = 0;
   /*Moving functions is allowed: we retrieve the function encompassing the instruction as an extended block*/
   if(movefcts != FALSE) {
      if((blocklen<jmplen)&&(blocklen<sjmplen)) { //The block is smaller than the jump: we will need an extended block
         fcthasindirect = patchfile_getfunction(insnseq,&extlststartblock,&extblocklen);
      }
   }
//    }
   /*Choosing what method to use. If there is no smaller jump, we will try the extended block if it is larger than the large jump, or
    * the standard block if it is smaller (we are screwed anyway, so better not move too much code around)
    * If there is a smaller jump, the trampoline will be favoured over using the extended block, itself favoured over using the trampoline
    * on the extended block*/
   if(blocklen<jmplen) { //The block size is smaller than the jump instruction
      if(sjmpinl) {        //There is a smaller jump instruction
         DBGMSG("Architecture contains a smaller jump instruction with length %"PRIu64" bytes, reaching between -%"PRIu64" and %"PRIu64" bytes\n",
               sjmplen/8,maxdistneg/8,maxdistpos/8);
         /**\todo Add the data about the small jump to patchfile_t once and for all to avoid having to call it each time*/
         if(blocklen>=sjmplen) {
            //The small jump is smaller than the block: we can use a trampoline
            wbbstart = lststartblock;
            wbblen = blocklen;
            trampmethod=1;//We must use a trampoline on the normal block
         } else if((blocklen<sjmplen)&&(extblocklen>=jmplen)) {
            //The small jump is larger than the block but the extended block is larger than the large jump
            wbbstart = extlststartblock;
            wbblen = extblocklen;
            trampmethod=0;//We will use the replacement on the extended block
         } else if((blocklen<sjmplen)&&(extblocklen<jmplen)&&(extblocklen>=sjmplen)) {
            //The small jump is smaller than the extended block: we can use a trampoline, but on the extended block
            wbbstart = extlststartblock;
            wbblen = extblocklen;
            trampmethod=1;//We will use the trampoline on the extended block
         } else {
            //The extended block and normal block are both smaller than the small jump instruction
            /**\todo In the future, this will invoke the Interrupt solution*/
            wbbstart = lststartblock;
            wbblen = blocklen;
            trampmethod=0; //This is temporary, but we will try a replacement anyway
            sizeok = 0;//We are screwed
         }
      } else {        //There is no smaller jump instruction
         if(extblocklen>=jmplen) {
            //The extended block is larger than the jump instruction
            wbbstart = extlststartblock;
            wbblen = extblocklen;
            trampmethod=0;//We will use the replacement on the extended block
         } else {
            //The extended block is still smaller than the large jump instruction
            wbbstart = lststartblock;
            wbblen = blocklen;
            trampmethod=0;//This is temporary, we will try a replacement anyway on the extended block
            sizeok = 0;//We are screwed
         }
      }
   } else {
      //The block is larger than the large block instruction
      wbbstart = lststartblock;
      wbblen = blocklen;
      trampmethod=0;//We use the replacement method on the normal block
   }
   //The block size is too small no matter what we tried
   if(!sizeok) {
      if ( (forceinserts == FALSE) || (move1insn != FALSE) ) {
         ERRMSG("%snstruction at address %#"PRIx64" in function %s%s. No patch done at this address%s\n",
               (move1insn == FALSE)?"Insufficient available size around i":"I",
                    INSN_GET_ADDR(INSN_INLIST(insnseq)), label_get_name(insn_get_fctlbl(INSN_INLIST(insnseq))),
               (move1insn == FALSE)?"":" is too small (moving only one instruction)",
               (move1insn == FALSE)?"":" (safe mode on)");
         return ERR_PATCH_INSUFFICIENT_SIZE_FOR_INSERT;
      } else {
         WRNMSG("Insufficient available size around instruction at address %#"PRIx64" in function %s. Patched file may crash (safe mode off)\n",
                    INSN_GET_ADDR(INSN_INLIST(insnseq)), label_get_name(insn_get_fctlbl(INSN_INLIST(insnseq))));
         /**\todo In the future, a further option would allow choosing the INT 3 method if it is implemented*/
         out = WRN_PATCH_SIZE_TOO_SMALL_FORCED_INSERT;
      }
   }

    DBGMSG("Using %s method on block starting at %#"PRIx64" with length %#"PRIx64" bytes\n",(trampmethod?"trampoline":"replacement"),INSN_GET_ADDR(INSN_INLIST(wbbstart)),wbblen/8);
   if(trampmethod) {        //Using a trampoline
      list_t* tramppos = NULL;//Position of the instruction at which the trampoline jump must be inserted
      list_t* trampoline = NULL;
      uint64_t maxdist,dist;
      int trampafter;
      /*We'll be looking for a trampoline backward first, since updates are usually performed in order of increasing address, thus
       * increasing the chance of finding an already displaced block*/
      /**\todo This is very much x86_64 specific. In other languages, a jump may not be relative, or its offset may not be counted from the end of the jump*/
      /*What we are looking for are basic blocks outside the current small one. So this means that if we search backward, we will start at the first instruction
       * before the beginning of the basic block and if we search forward, we will start at the first instruction at the end of the basic block
       * What will happen if we find the trampoline is that the small basic block will be replaced by a small jump, followed by padding.
       * As jump offsets are calculated from the end of the jump instruction (e.g. if we jump to the instruction immediately after the jump, the offset will be 0.
       * Conversely, if we jump at the beginning of the jump, the offset will be -(byte size of the jump)), we will then initialise the original distance as follows:
       * - If we go backward, it will be the size of the small jump added to the size of the first instruction before the small basic block (where we start)
       * - If we go forward, it will be the length of the small basic block minus the length of the small jump
       * If you still don't understand, make a picture, it helps.*/
      //Searching backward
      if(wbbstart->prev) {
            dist = sjmplen+insn_get_size(INSN_INLIST(wbbstart->prev));
         trampoline = patchfile_findtrampolineblock(pf,wbbstart->prev,&dist,jmplen,maxdistneg,0,&tramplen);
      }
      if(trampoline) {        //A trampoline was found searching backward
         maxdist = maxdistneg;
         trampafter=0;
         //Now we will search forward if we have found nothing backward
      } else if(reach_endblock(wbbstart,NULL,wbblen)) {
         dist = wbblen-sjmplen;
         trampoline = patchfile_findtrampolineblock(pf,reach_endblock(wbbstart,NULL,wbblen),&dist,jmplen,maxdistpos,1,&tramplen);
         if(trampoline) {        //A trampoline was found searching forward
            maxdist=maxdistpos;
            trampafter=1;
         }
      }
      if(trampoline) {        //A block suitable for a trampoline was found
         //Creates the trampoline
         DBGMSG("Trampoline block found %s at distance %#"PRIx64" bytes beginning at address %#"PRIx64"\n",
                  trampafter?"forward":"backward", dist/8, INSN_GET_ADDR(INSN_INLIST(wbbstart)) + ((int64_t)sjmplen + (trampafter?(int64_t)dist:(-(int64_t)dist)))/8 );
            if(INSN_GET_ADDR(INSN_INLIST(trampoline))>-1) {//The trampoline is a block that was not already displaced
            queue_t* btrjmpinl,*movedtrampl;
            list_t* btrjmpinliter;
            insn_t* btrjmp;
            /*Creates a large jump instruction list for the relevant architecture*/
            btrjmpinl = pf->patchdriver->generate_insnlist_jmpaddr(NULL,&btrjmp);
            btrjmpinliter = queue_iterator(btrjmpinl);
            //Moving the trampoline block
            movedtrampl = patchfile_displaceblock(pf,insnscn,btrjmpinl,btrjmp,trampoline,tramplen,paddinginsn);
            lc_free(btrjmpinl);
            if(!movedtrampl) {
               out = patchfile_get_last_error_code(pf);
               if (!ISERROR(out))
               out = ERR_PATCH_UNABLE_TO_MOVE_TRAMPOLINE;
               return out;
            }
            /*Stores the trampoline code in the instruction list*/
            queue_append_and_keep(pf->patch_list,movedtrampl);
            /*Marks the instructions has having been moved*/
            insnlist_setmoved(movedtrampl);
            lc_free(movedtrampl);      //Don't use queue_free, we need the nodes
            /*Retrieves in the space freed from the moved block an instruction suitable for inserting the trampoline*/
            tramppos = patchfile_findtrampoline_in_movedblock(pf,&dist,&btrjmpinliter,jmplen,maxdist,1,trampafter);
         } else {
            tramppos = trampoline;
         }
         /*Now we create the actual trampoline*/
         /*Replacing the small block with a small jump and retrieving it into replacedinl*/
         replacedinl = patchfile_displaceblock(pf,insnscn,sjmpinl,sjmp,wbbstart,wbblen,paddinginsn);
         lc_free(sjmpinl);   //Don't use queue_free, we need the nodes
         sjmpinl = NULL;
         if(!replacedinl) {
            out = patchfile_get_last_error_code(pf);
            if (!ISERROR(out))
            out = ERR_PATCH_UNABLE_TO_CREATE_TRAMPOLINE;
            return out;
         }
         DBGMSG0("Connecting trampoline branches\n");
         /*Retrieving a large jump instruction for the relevant architecture*/
         trjmpinl = pf->patchdriver->generate_insnlist_jmpaddr(NULL,&trjmp);
         /*Inserting the large jump into the block used as a trampoline and freeing the instruction in the displaced block*/
         queue_t *displ = patchfile_switchblocks(pf,trjmpinl,trjmp,tramppos,jmplen,NULL,insnscn,paddinginsn);
         lc_free(trjmpinl);
         /*Linking the large jump to the displaced small block*/
         DBGMSG0("Linking the large jump to the displaced small block\n");
            patchfile_setbranch(pf,trjmp,insn_get_branch(sjmp));
         /*Linking the small jump to the large jump*/
         DBGMSG0("Linking the small jump to the large jump\n");
         patchfile_setbranch(pf,sjmp,trjmp);
         queue_free(displ, insn_free); //Should only be inserted NOP so this is safe. We do this now as the setbranch functions will have looked up into it
         //Work complete
         trampok=1;
      } else {
         if ( (forceinserts == FALSE) || (move1insn != FALSE) ) {
            ERRMSG("Instruction at address %#"PRIx64" in function %s is too small and no trampoline was found. No patch done at this address\n",
                        INSN_GET_ADDR(INSN_INLIST(insnseq)), label_get_name(insn_get_fctlbl(INSN_INLIST(insnseq))));
            return ERR_PATCH_INSUFFICIENT_SIZE_FOR_INSERT;
         } else {
            WRNMSG("Insufficient available size around instruction at address %#"PRIx64" in function %s. Patched file may crash (safe mode off)\n",
                        INSN_GET_ADDR(INSN_INLIST(insnseq)), label_get_name(insn_get_fctlbl(INSN_INLIST(insnseq))));
            trampok=0;
            out = WRN_PATCH_SIZE_TOO_SMALL_FORCED_INSERT;
            /**\todo In the future, a further option would allow choosing the INT 3 method if it is implemented*/
         }
      }
   }
   if((!trampmethod)||(!trampok)) { //Not using trampoline or the trampoline failed, and we enforce insertions
      if(movefcts != FALSE) {
         if ( (trampmethod) && (!trampok) ) {
            wbbstart = extlststartblock;   //We will be using the extended block
            wbblen = extblocklen;
         }
      }
      replacedinl = patchfile_displaceblock(pf,insnscn,jmpinl,jmp,wbbstart,wbblen,paddinginsn);
      lc_free(jmpinl);    //Don't use queue_free, we need the nodes
      jmpinl = NULL;
      if(!replacedinl) {
         out = patchfile_get_last_error_code(pf);
         if (!ISERROR(out))
         out = ERR_PATCH_INSERT_INSNLIST_FAILED;
         return out;
      }
   }
   if(movefcts != FALSE) {
      if(wbbstart == extlststartblock) {
            WRNMSG("Function %s (beginning at %#"PRIx64") was displaced.\n",label_get_name(insn_get_fctlbl(INSN_INLIST(extlststartblock))),INSN_GET_ADDR(INSN_INLIST(extlststartblock)));
         out = WRN_PATCH_FUNCTION_MOVED;
         if (fcthasindirect) {
            WRNMSG("Displaced function contained indirect branch. The patched executable is very likely to fail.\n");
            out = WRN_PATCH_MOVED_FUNCTION_HAS_INDIRECT_BRCH;
         }
      }
   }
   if(jmpinl)
   queue_free(jmpinl,insn_free); //We created a large jump instruction list but never used it
   if(sjmpinl)
   queue_free(sjmpinl,insn_free);//We created a small jump instruction list but never used it

   /*Stores the inserted code in the instruction list*/
   queue_append_and_keep(pf->patch_list,replacedinl);
   /*Marks the instructions has having been moved*/
   insnlist_setmoved(replacedinl);
   lc_free(replacedinl);    //don't use queue_free, we need the nodes
   return out;
}
#endif
#if 0
/**
 * Removes a number of instructions of the given length at the given address in a disassembled file.
 * If the instruction found at address \e addr is not in the instruction list storing the insertions,
 * the basic block encompassing it will be moved.
 * \param pf A pointer to the structure holding the disassembled file
 * \param addr The address at which to insert \e code (it must correspond to an instruction)
 * \param node The list object containing the instruction at which the insertion must be made (if NULL, a search on \e addr will be performed)
 * \param bitlen The length in bits of all the code to remove, starting from the given address.
 * It must correspond to a number of instructions.
 * in \e code will actually be at address \e addr)
 * \param flags Flags altering the behaviour of the patcher for this modification
 * \param paddinginsn Instruction to use as padding for replacing removed code
 * \return The list of deleted instructions
 */
static queue_t* patchfile_patch_removecode(patchfile_t* pf,int64_t addr,list_t* node,int64_t bitlen, int flags,insn_t* paddinginsn) {

   /**\bug We never check if we have to replace more instructions as the number of instructions in the basic block*/
   insn_t* rminsn;
   queue_t* origins,*deleted = NULL;
   list_t* nextiter,*rmseq;
   int scnidx;
   int64_t rmlen=0,rmaddr=addr,nextaddr;
   DBGMSG("Must remove %"PRId64" bytes at address %#"PRIx64" (node %p -> %p)\n",bitlen/8,addr,node,(node!=NULL)?node->data:NULL);

   pf->current_flags &= ~(PATCHFLAG_MOV1INSN);
   /**\todo This is for avoiding problems because of the bug above (no checking on the number of instructions in the basic block). If we move only one
    * instruction and want to delete more than 1, we are almost certain to delete the jump back from the displaced block. When the bug is resolved, this
    * can go => Idea for fixing it: first build a list of the instructions to delete, then displace the block, then perform the deletion (checking for
    * each if it has been displaced)*/

   while(rmlen<bitlen) { //SCans instructions while we have not reached the required length
      /*Retrieve the instruction at which to start the removal of code*/
       if ((node != NULL) && (GET_DATA_T(insn_t*, node) != NULL) && (INSN_GET_ADDR(GET_DATA_T(insn_t*, node)) == rmaddr)
          && (insn_check_annotate(GET_DATA_T(insn_t*, node), A_PATCHDEL) == FALSE)){
         /*Using the node only if this is the first instruction to be deleted, if we are in a new iteration we have to find the instruction manually*/
         /**\todo Understand why I am doing this (the nested loop). There is probably a good reason (contiguous instructions or something) but I don't remember it.*/
         rmseq = node;
         rminsn = GET_DATA_T(insn_t*, node);
            scnidx = patchfile_getscnid_byaddr(pf, rminsn, INSN_GET_ADDR(rminsn));
      } else {
         rminsn = patchfile_getinsnataddr(pf,rmaddr,&scnidx,&rmseq);
      }
      if(!rminsn) {    //No instruction found at this address
         ERRMSG("Unable to find instruction at address %#"PRIx64"\n",rmaddr);
         pf->last_error_code = ERR_LIBASM_INSTRUCTION_NOT_FOUND;
         return deleted;
      }
      DBG(
            char buf[256];
            insn_print((void*)rminsn, buf, sizeof(buf));
                DBGMSG("Found instruction %#"PRIx64":%s (%p) in section %d\n",INSN_GET_ADDR(rminsn),buf,rminsn,scnidx);
      )

      /*Checking if the deletion must be made into the original code or in an already displaced
       * area of code*/
      if (insn_ismoved(rminsn) == FALSE) {
         /*Moving the basic block to another section*/
         int out = patchfile_moveblock(pf,rmseq,scnidx, flags,paddinginsn);
         if (ISERROR(out)) {
            pf->last_error_code = out;
            return deleted; //Moving the basic block failed: exiting
         }
      }
      /*Initialising list of deleted instructions*/
      deleted = queue_new();
      /*Now we will remove instructions from the list as long as they are contiguous*/
      do {
         list_t* removed;
         nextiter = rmseq->next; //Stores the next instruction
         rminsn = GET_DATA_T(insn_t*, rmseq);
         /*We want to preserve the list_t object containing the removed instruction in case a subsequent modification pointed to it,
          * but we have to flag the instruction as having been deleted*/
         removed = queue_extract_node(pf->patch_list,rminsn);
            insn_add_annotate(rminsn, A_PATCHDEL);
         queue_append_node(deleted, removed);
            rmlen+= insn_get_size(rminsn);//Updates length of removed instructions
            nextaddr = INSN_GET_ADDR(rminsn)+insn_get_size(rminsn)/8;//Update next address
         /*Updating the branch pointing to the instruction so that they now point to the instruction immediately after*/
         /*Retrieves all instructions pointing to the deleted instruction*/
         DBGMSG("Checking if branches point to removed instruction %p\n",rminsn);
            origins=hashtable_lookup_all(pf->branches,(void *)INSN_GET_ADDR(rminsn));
         /*Updates instructions pointing to the address so that they
          * now point to the instruction immediately after the deleted one instead*/
         if(origins) {
            FOREACH_INQUEUE(origins,iter) {
               DBG(
                     char buf[256];
                     insn_print(INSN_INLIST(iter), buf, sizeof(buf));
                            DBGMSG("Found instruction %#"PRIx64":%s (%p) pointing to instruction %p\n",INSN_GET_ADDR(INSN_INLIST(iter)),buf,INSN_INLIST(iter),rminsn);
               )
                    if(insn_get_branch((insn_t*)iter->data)==rminsn) {
                  /**\todo Find something more clever than looking each time for the address...*/
                    	insn_t* newnext = nextiter->data;
                        //insn_t* newnext = patchfile_getinsnataddr(pf,nextaddr,NULL,NULL);
                  DBG(
                        char buf[256];
                        insn_print(newnext, buf, sizeof(buf));
                                DBGMSG("Updating branch from %p to next instruction %#"PRIx64":%s (%p)\n",INSN_INLIST(iter),INSN_GET_ADDR(newnext),buf,newnext);
                  )
                  patchfile_setbranch(pf,INSN_INLIST(iter),newnext);
               }
            }
            queue_free(origins,NULL);
         }
         /**\todo Factorise the following code on non-updatable branches with the one above*/
         /*Same work for branches that are not supposed to be updated*/
            origins=hashtable_lookup_all(pf->branches_noupd,(void *)INSN_GET_ADDR(rminsn));
         /*Updates instructions pointing to the address so that they
          * now point to the instruction immediately after the deleted one instead*/
         if(origins) {
            FOREACH_INQUEUE(origins,iter) {
               DBG(
                     char buf[256];
                     insn_print(INSN_INLIST(iter), buf, sizeof(buf));
                            DBGMSG("Found instruction %#"PRIx64":%s (%p) pointing to instruction %p\n",INSN_GET_ADDR(INSN_INLIST(iter)),buf,INSN_INLIST(iter),rminsn);
               )
                    if(insn_get_branch((insn_t*)iter->data)==rminsn) {
                  /**\todo Find something more clever than looking each time for the address...*/
                    	insn_t* newnext = nextiter->data;
                    	//insn_t* newnext = patchfile_getinsnataddr(pf,nextaddr,NULL,NULL);
                  DBG(
                        char buf[256];
                        insn_print(newnext, buf, sizeof(buf));
                                DBGMSG("Updating branch from %p to next instruction %#"PRIx64":%s (%p)\n",INSN_INLIST(iter),INSN_GET_ADDR(newnext),buf,newnext);
                  )
                  patchfile_setbranch_noupd(pf,INSN_INLIST(iter),newnext);
               }
            }
            queue_free(origins,NULL);
         }
         DBG(
               char buf[256];
               insn_print(rminsn, buf, sizeof(buf));
                    DBGMSG("Deleting instruction %#"PRIx64":%s (%p)\n",INSN_GET_ADDR(rminsn),buf,rminsn);
         )
         //queue_add_tail(deleted,rminsn);//To avoid seg faults for instructions present in the hashtable
         /**\bug The instruction is added twice to the list. This causes a seg fault when attempting to free it*/
         rmseq = nextiter;
        } while((rmlen<bitlen)&&(nextaddr==INSN_GET_ADDR(INSN_INLIST(rmseq))));//This checks that instructions remain contiguous
      rmaddr = nextaddr;
      DBGMSG("%"PRId64" bytes removed\n",rmlen/8);
   }
   DBGLVL(2,DBGMSG0("Patch list is now:\n");patchfile_dumppatchlist(pf,stderr);)
   return deleted;
}
#endif
#if 0
/**
 * Insert an instruction list at the given address in a disassembled file.
 * If the instruction found at address \e addr is not in the instruction list storing the insertions,
 * the basic block encompassing it will be moved.
 * The code to insert is then directly inserted into the list of instructions, at the given address. This is not
 * done directly; instead, a jmp instruction is inserted at the given address, replacing the present instruction up to the jmp instruction
 * length. Those instruction are then appended to an instruction list at the
 * end of the instruction lists tables, which is identified by an associated section number of -1
 * (it is created if it did not exist)
 * \param pf A pointer to the structure holding the disassembled file
 * \param code The instruction list to insert
 * \param addr The address at which to insert \e code
 * \param node The list object containing the instruction at which the insertion must be made (if NULL, a search on \e addr will be performed)
 * \param after If nonzero, the instruction list \e code will be inserted after the instruction
 * present at address \e addr, otherwise, it will be inserted before (so that the first instruction
 * in \e code will actually be at address \e addr)
 * \param flags Flags altering the behaviour of the patcher for this modification
 * \param paddinginsn Instruction to use as padding for replacing moved instructions
 * \return EXIT_SUCCESS / error code
 */
static int patchfile_insertcode(patchfile_t* pf,queue_t* code,int64_t addr,list_t* node,char after, int flags,insn_t* paddinginsn) {
   insn_t* probed;
   queue_t* origins;
   list_t* lstprobe;
   int scnidx;
   int out = EXIT_SUCCESS;
   if ( (queue_length(code) == 0) && (addr > 0) ) {
      //List to insert is empty: we only allow that for floating insertions, as there will be an insertion for linking them to something
      WRNMSG("List of instructions to insert at address %#"PRIx64" is empty: no insertion performed\n", addr);
      return ERR_PATCH_INSERT_LIST_EMPTY;
   }
   DBGMSG("Inserting %d instruction at address %#"PRIx64"\n",queue_length(code), addr);
   if (addr == 0) {
      DBGMSG0("Handling floating insertion\n");
      //Special case: this is a "floating" insertion
      if (pf->patch_list == NULL) {
         DBGMSG0("Creating instruction list for inserted instructions\n");
         patchfile_addcodescn(pf);
      }
        insnlist_setmoved(code);
        queue_append_and_keep(pf->patch_list, code);

   } else {
      /*Retrieve the instruction at which to insert the code*/
       if ((node != NULL) && (GET_DATA_T(insn_t*, node) != NULL) && (insn_check_annotate(GET_DATA_T(insn_t*, node), A_PATCHDEL) == FALSE)) {
         lstprobe = node;
         probed = GET_DATA_T(insn_t*, node);
         DBGMSGLVL(1,"Retrieving instruction %p from node %p\n",probed,lstprobe);
             scnidx = patchfile_getscnid_byaddr(pf, probed, INSN_GET_ADDR(probed));
      } else {
         probed = patchfile_getinsnataddr(pf,addr,&scnidx,&lstprobe);
      }
      if (!probed) { //No instruction found at this address
         ERRMSG("Unable to find instruction at address %#"PRIx64"\n",addr);
         return ERR_LIBASM_INSTRUCTION_NOT_FOUND;
      }
      DBG(
            char buf[256];
            insn_print(probed, buf, sizeof(buf));
             DBGMSG("Found instruction %#"PRIx64":%s (%p) in section %d\n",INSN_GET_ADDR(probed),buf,probed,scnidx);
      )

      /*Checking if the insertion must be made into the original code or in an already displaced
       * area of code*/
      if (insn_ismoved(probed) == FALSE) {
         /*Moving the basic block to another section*/
         out = patchfile_moveblock(pf,lstprobe,scnidx, flags,paddinginsn);
         if (ISERROR(out))
         return out; //Block could not be moved
      }

      /*At this point, the portion of code into which the insertion must be made is now in the section
       * containing new code, but with its original address*/
      /*Inserting new code*/
         DBGMSG("Inserting code %s instruction at address %#"PRIx64" and object %p\n",((after)?"after":"before"),INSN_GET_ADDR((insn_t*)lstprobe->data),lstprobe);
      /*Marks the instructions has having been moved (has to be done before the insertion as the list boundaries will have changed)*/
      insnlist_setmoved(code);
         queue_insert_and_keep(pf->patch_list,code,lstprobe,after);

      /*If the insertion must be made before an instruction, update all pointers to this instruction*/
      if(!after) {
         /*Retrieves all instructions pointing to the instruction at which the insertion must be made*/
             origins=hashtable_lookup_all(pf->branches,(void *)INSN_GET_ADDR(probed));
         /*Updates instructions pointing to the address so that they
          * now point to the head of the inserted instruction list instead*/
         if(origins) {
                 DBGMSG("Found %d instructions pointing to %#"PRIx64" (%p)\n",queue_length(origins),INSN_GET_ADDR(probed),probed);
            FOREACH_INQUEUE(origins,iter) {
               insn_t* orig = GET_DATA_T(insn_t*, iter);
                     if(insn_get_branch(orig)==probed) {
                  /*Handling exclusion of insertions*/
                         if ( ( ( !(flags&PATCHFLAG_INSERT_NO_UPD_OUTFCT) ) || ( insn_get_fctlbl(orig) == insn_get_fctlbl(probed) ) )
                                 && ( ( !(flags&PATCHFLAG_INSERT_NO_UPD_FROMFCT) ) || ( insn_get_fctlbl(orig) != insn_get_fctlbl(probed) ) ) ) {
                     DBGMSG("Instruction %p pointed to %p\n",orig,probed);
                     patchfile_setbranch(pf,orig,(insn_t*)queue_peek_head(code));
                  } else {
                     DBGMSG("Instruction %p pointed to %p but was not updated because of restrictions on updates\n",orig,probed);
                  }
               }
            }
            queue_free(origins,NULL);
         } else {
                 DBGMSG("No instruction found pointing to %#"PRIx64" (%p)\n",INSN_GET_ADDR(probed),probed);
         }
      }
   }
   DBGLVL(2,DBGMSG0("Patch list is now:\n");patchfile_dumppatchlist(pf,stderr);)
   return out;
}
#endif
#if 0
/**
 * Replaces one or more instructions by a list of instructions.
 * \param pf Pointer to the structure holding the disassembled file
 * \param lold Pointer to the list object containing the first instruction to replace
 * \param ninsns Number of instructions to replace from the one contained in lold (included)
 * \param scnidx Section number
 * \param newins The list of instructions to replace the old one with
 * \param upd Set to 1 if the replacement is actually an instruction being changed, otherwise means we are simply replacing instructions by others
 * \param flags Flags altering the behaviour of the patcher for this modification
 * \param paddinginsn Instruction to use for padding to replace moved instructions
 * \return EXIT_SUCCESS / error code
 * */
static int patchfile_patch_replaceinsn(patchfile_t* pf,list_t* lold, int ninsns, int scnidx,queue_t* newins, int upd, int flags,insn_t* paddinginsn) {
   /**\bug We never check if we have to replace more instructions as the number of instructions in the basic block*/
   list_t* end;
   queue_t* origins;
   int scn = scnidx;
   int n;
   int out = EXIT_SUCCESS;
   if(!newins) {
      ERRMSG("Replace required with unspecified instructions\n");
      return ERR_PATCH_INSERT_LIST_EMPTY;
   }
    if ((!lold) || (!lold->data) || (insn_check_annotate(GET_DATA_T(insn_t*, lold), A_PATCHDEL) == TRUE)) {
      ERRMSG("No target given for instruction replacement operation\n");
      return ERR_LIBASM_INSTRUCTION_NOT_FOUND;
   }
   pf->current_flags &= ~(PATCHFLAG_MOV1INSN);
   /**\todo This is for avoiding problems because of the bug above (no checking on the number of instructions in the basic block). If we move only one
    * instruction and want to replace more than 1, we are almost certain to replace the jump back from the displaced block. When the bug is resolved, this
    * can go*/

   if (scnidx == -1)
        scn = patchfile_getscnid_byaddr(pf,INSN_INLIST(lold),INSN_GET_ADDR(INSN_INLIST(lold)));

   DBG(char oldstr[256];char newstr[256];
         insn_print(INSN_INLIST(lold), oldstr, sizeof(oldstr));
         insn_print(queue_peek_head(newins), newstr, sizeof(newstr));
         DBGMSG("Replacing %d instruction%s %#"PRIx64":%s (size %d) in section %d by instruction list (size %"PRId64") beginning with %s\n",ninsns,(ninsns>1)?"s":"",
            INSN_GET_ADDR(INSN_INLIST(lold)),oldstr,insn_get_size(INSN_INLIST(lold)),scnidx,
               insnlist_bitsize(newins, NULL, NULL),newstr));

    if(insnlist_bitsize(newins, NULL, NULL)!=(unsigned int)insn_get_size(INSN_INLIST(lold))) { /**\todo Have insn_get_size return an unsigned value*/
      /*In this case, the new and old instruction have a different size: we need to move the block*/
      /*Checking if the modification must be made into the original code or in an already displaced
       * area of code*/
      if (insn_ismoved(INSN_INLIST(lold)) == FALSE) {
         /*Moving the basic block to another section*/
         out = patchfile_moveblock(pf,lold,scn, flags,paddinginsn);
         if (ISERROR(out))
         return out; //Basic block could not be moved
         scn = pf->n_codescn_idx-1;//Updating the section where we are working
      }
   }
   /*We also have to check if the new instruction does not contain a reference to a memory address*/
   /**\todo Warning! The following code supposes that the old instruction contained a reference to a memory address, and that
    * the new instruction contains a reference to the same address. It will have to take into account the possibility of updating
    * an instruction by adding a reference to a memory address (global variable)*/
   int isinsn;
   int64_t destaddr;
   /*If the new instruction contains a reference to memory, we will check that it was using the old instruction*/
    if(insn_check_refs((insn_t*)queue_peek_head(newins),&isinsn)>=0){
      /*Using a architecture specific function to identify the new instruction's destination*/
        destaddr = insn_check_refs(INSN_INLIST(lold),&isinsn);
      /*Registers the instruction if it is a memory offset*/
      if((destaddr>=0)&&(isinsn==0)) {
         elffile_add_targetaddr(pf->efile,(insn_t*)queue_peek_head(newins),destaddr,pf->patchdriver->upd_dataref_coding,0,0);
      }
      /**\todo Check if we also have to insert in the branch hashtable*/
   }
   /*We have to preserve a pointer to the old instructions otherwise, the way inslist_swap works, lold will contain the new one afterward*/
   insn_t* oldins = INSN_INLIST(lold),*newinsn = queue_peek_head(newins);
   /*Setting up the last instruction to replace*/
   end = lold;
   n = 1;
   while (n < ninsns) {
      end = end->next;
      n++;
   }
   /*Now we just have to replace our instructions*/
   if ( ( scn == (pf->n_codescn_idx-1) ) && ( pf->patch_list != NULL ) ) /**\todo Get rid of this way to detect whether we are in the patched section or not*/
        insnlist_swap(pf->patch_list,lold,end,newins,(upd==1)?A_PATCHUPD|insn_get_annotate(INSN_INLIST(lold)):A_NA);
   else {
      /*Handling the section boundaries*/
      /**\todo As elsewhere, get rid of this section boundaries thing and find something more clever*/
      if (lold == pf->codescn_bounds[scn].begin)
      pf->codescn_bounds[scn].begin = queue_iterator(newins);
      if (lold == pf->codescn_bounds[scn].end)
      pf->codescn_bounds[scn].end = queue_iterator_rev(newins);
        insnlist_swap(asmfile_get_insns(pf->afile),lold,end,newins,(upd==1)?A_PATCHUPD|insn_get_annotate(INSN_INLIST(lold)):A_NA);//queue_swap(asmfile_get_insns(pf->afile)/*pf->insn_lists[scn]*/,INSN_INLIST(lold),INSN_INLIST(lold),newins);
   }
   /* In case you are wondering what the thing with annotate is, this is to track instructions that were first moved because belonging to a modified
    * block, then have to be updated.*/
   /**\todo Calling insnlist_swap may not be optimised, as it forces to look again and again for the instruction
    * => Might have fixed this now that I used the list_t object instead of the insn_t ones, and that insnlist_swap uses queue_swap_elts*/

   /*Retrieves all instructions pointing to the replaced instruction*/
    origins=hashtable_lookup_all(pf->branches,(void *)INSN_GET_ADDR(oldins));
   /*Updates instructions pointing to the address so that they
    * now point to the head of the inserted instruction list instead*/
   if(origins) {
        DBGMSG("Found %d instructions pointing to %#"PRIx64" (%p)\n",queue_length(origins),INSN_GET_ADDR(oldins),oldins);
      FOREACH_INQUEUE(origins,iter) {
           if (insn_get_branch(GET_DATA_T(insn_t*, iter)) == oldins)
         patchfile_setbranch(pf, GET_DATA_T(insn_t*, iter), (insn_t*)newinsn);
      }
      queue_free(origins,NULL);
   }

   /*Special case: since the instruction has been replaced by another one, we have to update all branches pointing to it,
    * even those that should not be updated.*/
   /*Retrieves all instructions pointing to the replaced instruction*/
      origins=hashtable_lookup_all(pf->branches_noupd,(void *)INSN_GET_ADDR(oldins));
   /*Updates instructions pointing to the address so that they
    * now point to the head of the inserted instruction list instead*/
   if(origins) {
          DBGMSG("Found %d non-updatable instructions pointing to %#"PRIx64" (%p)\n",queue_length(origins),INSN_GET_ADDR(oldins),oldins);
      FOREACH_INQUEUE(origins,iter) {
             if (insn_get_branch(GET_DATA_T(insn_t*, iter)) == oldins)
         patchfile_setbranch_noupd(pf, GET_DATA_T(insn_t*, iter), (insn_t*)newinsn);
      }
      queue_free(origins,NULL);
   }

   DBGLVL(2,DBGMSG0("Patch list is now:\n");patchfile_dumppatchlist(pf,stderr);)
   return out;
}
#endif
#if MOVED_ELSEWHERE
/**
 * Adds mandatory external libraries (.so files) to an executable
 * \param pf A pointer to the structure holding the disassembled file
 * \param inslibs An array containing inserted libraries
 * \param libnum The size of the array \e libnames
 * \return EXIT_SUCCESS / error code
 * */
static int patchfile_add_extlibs(patchfile_t* pf,inslib_t** inslibs,int libnum) {
   int i;
   int out = EXIT_SUCCESS;
   for(i=0;i<libnum;i++) {
      DBGMSG("Inserting library %s\n",inslibs[i]->name);
      int res = binfile_patch_add_ext_lib(pf->patchbin, inslibs[i]->name, (inslibs[i]->flags & LIBFLAG_PRIORITY));
      if (ISERROR(res)) {
         ERRMSG("Unable to insert library %s\n", inslibs[i]->name)
         out = res;
      }
   }
   return out;
}
#endif
#if 0 //done in the libbin
/**
 * Adds to a disassembled file the stub used to invoke a function present in an external library
 * \param pf Pointer to the structure holding the disassembled ELF file
 * \param funcname The name of the function
 * \return A pointer to the instruction to call to when invoking the fonction, or NULL if the
 * operation failed. In that case, the last error code in \c pf will be updated
 * */
static insn_t* patchfile_insextfctcallstub(patchfile_t* pf,char* funcname) {
   int64_t jmpreladdr,pltendaddr;
   insn_t* gotcallins,*gotretins,*pltfirstins;
   int lblidx,relidx,pltidx;
   queue_t* pltcall;

   /*Adds the new label to file*/
   lblidx=elffile_add_symlbl(pf->efile,funcname,0,0,ELF64_ST_INFO(STB_GLOBAL,STT_FUNC),SHN_UNDEF,TRUE);
   /**\todo Find how the size of an ELF symbol is determined (so far 0 seems to work but it's a guess)*/
   if ( lblidx < 0 ) {
      ERRMSG("Unable to add dynamic symbol %s to file. Patched file may not run properly\n",funcname);
      pf->last_error_code = ERR_PATCH_LABEL_INSERT_FAILURE;
   }
   /*Retrieves the index of the last entry in the JMPREL table*/
   if ( pf->lastrelfctidx < 0 )    //First time a new stub is added
   relidx = elfexe_get_jmprel_lastidx(pf->efile);
   else
   relidx = pf->lastrelfctidx;/**\todo See the comments in the header file for why this is used but really should not*/
   /*Retrieving the first instruction in the .plt section*/
   pltidx = patchfile_getprogidx(pf,NULL,elffile_getpltidx(pf->efile));
   if(pltidx<0) {
      ERRMSG("No .plt section found\n");
      pf->last_error_code = ERR_BINARY_NO_EXTFCTS_SECTION;
      return NULL;
   }
   /**\warning Part of this function's behaviour may be partly specific to x86 architectures*/
   pltfirstins = GET_DATA_T(insn_t*, pf->codescn_bounds[pltidx].begin);
   /*Gets the architecture specific list of instructions used in the procedure linkage table*/
   /*gotcallins holds the instruction linking to the global offset table (it is also the instruction
    * to call to in order to call the external function) and gotretins the instruction
    * to which control comes back from the GOT*/
   pltcall = pf->patchdriver->generate_insnlist_pltcall(relidx+1,pltfirstins,&gotcallins,&gotretins);

   /*Adding the stub into the section of moved instructions*/
   if(pf->plt_list == NULL) {
      DBGMSG0("Creating instruction list for inserted function calls\n");
      pf->plt_list = queue_new();
      pltendaddr = 0;
   } else
        pltendaddr = INSN_GET_ADDR((insn_t*)queue_peek_tail(pf->plt_list))+insn_get_size((insn_t*)queue_peek_tail(pf->plt_list))/8;

   /*Updates addresses of the generated list from the last address of the plt section*/
   insnlist_upd_addresses(pltcall,pltendaddr,NULL,NULL);
   /*Adds this list to the list of instructions in the plt section*/
   queue_append(pf->plt_list,pltcall);

    DBGMSGLVL(1, "Adding PLTGOT entry to address %#"PRIx64" for instruction %p\n", INSN_GET_ADDR(gotretins), gotcallins);
   /*Updates the global offset table linked to the procedure linkage table with the address
    * of the return instruction in the PLT, which is pointed to by the first instruction of the new block*/
    elfexe_add_pltgotaddr(pf->efile,INSN_GET_ADDR(gotretins),gotcallins,pf->patchdriver->upd_dataref_coding);

   DBGMSGLVL(1, "Update JMPREL table with symbol %s\n", funcname);
   /*Updates the JMPREL table*/
   /*We have to do this now, otherwise the link between the entry created in PLTGOT with this new entry won't be correctly made*/
   jmpreladdr=elfexe_add_jmprel(pf->efile,funcname,NULL);
   if ( jmpreladdr < 0 ) {
      ERRMSG("Unable to add dynamic relocation for symbol %s to file. Patched file may not run properly\n",funcname);
      int res = elffile_get_last_error_code(pf->efile);
      if(ISERROR(res))
      pf->last_error_code = res;
      else
      pf->last_error_code = ERR_PATCH_RELOCATION_NOT_ADDED;
   }
   DBGMSG("Created stub for function %s\n", funcname);
   //Updates the last index in the .plt section
   pf->lastrelfctidx = relidx + 1; /**\todo Once again, see the comment in the header file about why this should not be here*/

   return gotcallins;
}
#endif
/**
 * Retrieves a pointer used to call an external function. If it did not exist in the file, it will be added
 * \param pf Structure holding the details about the file to patch
 * \param fct Structure describing the function to call.
 * \return EXIT_SUCCESS if a pointer to the first instruction of the function could be found, error code if it could not be found or inserted
 * If successful the \c fct will have been updated
 * */
static int patchfile_getinsextfctcall(patchfile_t* pf, insertfunc_t *fct)
{
   assert(pf && fct);
   char* funcname = fct->name;
   int out = EXIT_FAILURE;
//       int64_t calladdr;
//    insn_t* gotcallins = NULL;
//    pointer_t* fctcall = NULL;
   //Generates the name for a symbol representing this external function
   char* extname = pf->bindriver->generate_ext_label_name(funcname);
   /**\todo TODO (2015-05-27) Find another mechanism for getting external function label names, this one forces to
    * do a malloc/free.*/
   assert(extname);
   // Looks up this name in the list of labels
   insn_t* extfctinsn = asmfile_get_insn_by_label(pf->afile, extname);
//    /*Looks for the function call address*/
//    calladdr = elfexe_get_extlbl_addr(pf->efile,funcname);
   if (!extfctinsn) { /*The label has not yet been added*/
      /*Creating the stub to call to for invoking the external function and retrieving its first
       * instruction*/
      fct->fctptr = binfile_patch_add_ext_fct(pf->patchbin, funcname,
            fct->libname, FALSE);
      /**\todo TODO (2014-11-07) Forcing the last parameter to FALSE because so far I have not implemented the
       * insertion of preloaded functions (no lazy binding). Of course, this will have to change when it is done*/
      out = EXIT_SUCCESS;
   } else {
      //Function already exists
      /**\todo TODO (2014-11-07) Same as above: when the invocation of preloaded function is implemented, we have
       * to check that we actually found the same pointer. In any cases, I think we would also have to change the
       * function for retrieving it above
       * => (2014-11-17) Also, we can differentiate preloaded vs directly called functions by the *target*, not the type of pointer
       * TARGET_DATA means it's preloaded, TARGET_INSN means we use lazy binding*/
      fct->fctptr = pointer_new(insn_get_addr(extfctinsn), 0, extfctinsn,
            POINTER_RELATIVE, TARGET_INSN);
      out = EXIT_SUCCESS;
      /**\todo TODO (2014-11-07) WARNING!!!!! Beware of my stuff with duplicating instructions between the original and the copy
       * I might have to create a patchinsn_t structure for that one. But maybe the address will be enough...*/
//        /*The label exists: we retrieve the instruction it points to (it is the instruction to
//         * call to to invoke this external function)*/
//        /* We can't use here patchfile_getinsnataddr directly, as it will look in the entire file.
//         * This will lead to a problem because some addresses may be be duplicated - the plt section is
//         * increasing, and may be overlapping the .text section, whose addresses we don't want to update
//         * yet as there may be some further changes to make to it.
//         * Therefore we will only look for addresses in the PLT section*/
//        int pltidx = patchfile_getprogidx(pf,NULL,elffile_getpltidx(pf->efile));
//        if(pltidx<0) {
//            STDMSG("No .plt section found\n");
//        } else {
//            list_t* lst = insnlist_addrlookup(asmfile_get_insns(pf->afile),calladdr,pf->codescn_bounds[pltidx].begin,pf->codescn_bounds[pltidx].end->next);
//            if((lst)&&(lst->data)) {
//               fct->fctptr = lst->data;
//               out = TRUE;
//            } else {//Instruction not found - should not happen
//                STDMSG("No entry found in the .plt section for function %s at address %#"PRIx64"\n",funcname,calladdr);
//            }
//        }
   }
   lc_free(extname);
   return out;
}

/**
 * Inserts an object file into a file
 * \param pf Pointer to the structure holding the details about the patching operation
 * \param insert A parsed and disassembled object file. If it contains undefined symbols and the file is dynamic,
 * a stub will be added for invoking this symbol. If the file is static, an error will be printed
 * \return EXIT_SUCCESS if the insertion was successful, EXIT_FAILURE or an error code otherwise
 * \note (2015-09-03) This function is not used any more and could be removed. This will be done in the patcher refactoring
 * */
int patchfile_insertobjfile(patchfile_t* pf, asmfile_t* insert)
{
   int out = EXIT_SUCCESS;
   if ((pf == NULL) || (insert == NULL)) {
      out = EXIT_FAILURE;
      /**\todo Add an error code or a message depending on the error encountered*/
   } else if (asmfile_test_analyze(insert, DIS_ANALYZE) == FALSE) {
      STDMSG(
            "Internal error: Attempting to insert an incorrectly disassembled object file\n");
      out = EXIT_FAILURE;
   } else if (binfile_get_type(asmfile_get_binfile(insert)) != BFT_RELOCATABLE) {
      STDMSG("Internal error: Attempting to insert an non relocatable file\n");
      out = EXIT_FAILURE;
   } else {

   }
   return out;
}

/**
 * Try to add an inserted function defined as static (present in an external library) to a patched file.
 * \param pf A pointer to the structure holding the parsed file
 * \param funcname Name of the function
 * \param insfunc The inserted function
 * \return EXIT_SUCCESS if the inserted function could be found, error code otherwise
 * */
static int patchfile_addstaticinsertfunc(patchfile_t* pf, insertfunc_t* insfunc)
{
   /*Looking up the function in the list of static files added to the patched file*/
   /**\note Doing so loses the information we have in libmadras.c about the link between an instruction
    * and a .a file, however, since a .a contains multiple .o, it would be complicated to try keeping track
    * of which is where and looking only in those ones. And anyway I'm fed up with this*/
   /**\todo Remove elements from insertedlibs as they are inserted into insertedobjs, so as to avoid having to look them up twice later*/
   int out = ERR_LIBASM_INSTRUCTION_NOT_FOUND;
   char* funcname = insfunc->name;
   FOREACH_INQUEUE(pf->insertedlibs, oiter) {
      insn_t* firstinsn = asmfile_get_insn_by_label(GET_DATA_T(asmfile_t*, oiter), funcname);
      if (firstinsn != NULL) {
         DBGMSG(
               "Function %s found in file %s beginning at address %#"PRIx64"\n",
               funcname, asmfile_get_name(GET_DATA_T(asmfile_t*, oiter)),
               insn_get_addr(firstinsn));
         insfunc->fctptr = pointer_new(0, 0, firstinsn, POINTER_RELATIVE,
               TARGET_INSN);
         /**\todo TODO (2014-11-14) Use here the calltype to check whether we use an absolute or relative pointer (depending on direct/indirect call)
          * Warning! Check also that we can't have indirect calls using a relative address.*/
         insfunc->objfile = GET_DATA_T(asmfile_t*, oiter);
         if (queue_lookup(pf->insertedobjs, direct_equal,
               GET_DATA_T(asmfile_t*, oiter)) == NULL)
            queue_add_tail(pf->insertedobjs, GET_DATA_T(asmfile_t*, oiter));
         out = EXIT_SUCCESS;
         break;
      }
   }
   return out;
}

/**
 * Finds an inserted function in a patched file. If it does not exist, create it an add it to the list of inserted functions
 * \param pf A pointer to the structure holding the parsed file
 * \param funcname Name fo the function
 * \param functype Type of the function (as defined in the insertfunc_types enum)
 * \return A pointer to the structure holding the inserted function's details. Should never return NULL
 * If an error occurred, the last error code in \c pf will be updated
 * */
static insertfunc_t* patchfile_getinsertfunc(patchfile_t* pf, insfct_t* fct)
{
   assert(pf && fct);
   insertfunc_t* out = NULL;
   char* funcname = fct->funcname;
   char* libname = NULL;
   int functype;
   if ((fct->srclib != NULL) && (fct->srclib->type == ADDLIB)) {
      if (fct->srclib->data.inslib->type == DYNAMIC_LIBRARY) {
         functype = DYNAMIC;
         libname = fct->srclib->data.inslib->name;
      } else if (fct->srclib->data.inslib->type == STATIC_LIBRARY) {
         functype = STATIC;
         libname = fct->srclib->data.inslib->name;
      }
   } else {
      functype = INTERNAL;
   }
   /**\todo Perform some tests to see when it becomes faster to use a hashtable. Right now I am betting that there will not be too many different functions
    to insert in a file to justify using a hashtable*/
   list_t* iter = queue_iterator(pf->insertedfcts);
   while ((iter != NULL)
         && (strcmp((GET_DATA_T(insertfunc_t*, iter))->name, funcname)))
      iter = iter->next;
   if (iter != NULL) {
      /*A function was found*/
      out = GET_DATA_T(insertfunc_t*, iter);
      //Maybe add a test on the type, but that would be a strange case...
   } else {
      /*No function found: we will create a new entry*/
      out = insertfunc_new(funcname, functype, libname);
      switch (functype) {
      insn_t* firstinsn = NULL; //First instruction
   case UNDEFINED: //Currently not used
      /*We will first look the label inside the file, then in other object files, and finally add it as
       * dynamic if the file supports it*/
      if ((firstinsn = asmfile_get_insn_by_label(pf->afile, funcname)) != NULL) {
         /*File found inside the file*/
         out->type = INTERNAL;
         out->fctptr = pointer_new(0, 0, firstinsn, POINTER_RELATIVE,
               TARGET_INSN);
         /**\todo TODO (2014-11-14) Use here the calltype to check whether we use an absolute or relative pointer (depending on direct/indirect call)
          * Warning! Check also that we can't have indirect calls using a relative address.*/
      } else if (patchfile_addstaticinsertfunc(pf, out) == TRUE) {
         out->type = STATIC;
      } else if ((binfile_get_nb_ext_libs(pf->patchbin) > 0)
            && (patchfile_getinsextfctcall(pf, out) == EXIT_SUCCESS)) {
         /**\warning (2014-11-07) I'm supposing here that 0 external libraries <=> static executable
          * We might have the case where a file is dynamic yet does not have any external library
          * (for instance with ELF, we could have a .dynamic section but not containing any DT_NEEDED entry)
          * I don't know if it is possible or even makes sense but if the case happens, this test will be wrong.*/
         out->type = DYNAMIC;
         WRNMSG(
               "Symbol %s has been added to the file as an external call. " "Patched file may fail if the symbol is not defined in an external library\n",
               funcname);
         pf->last_error_code = WRN_PATCH_SYMBOL_ADDED_AS_EXTERNAL;
      } else {
         ERRMSG(
               "Label %s is not defined and can not be added as external call as the %s is static." "Patched file will fail.\n",
               funcname, asmfile_get_name(pf->afile));
         pf->last_error_code = ERR_BINARY_SYMBOL_NOT_FOUND;
      }
      break;
   case INTERNAL:
      firstinsn = asmfile_get_insn_by_label(pf->afile, funcname);
      if (firstinsn == NULL) {
         //Checking if the function is not in the plt
         char extfuncname[strlen(funcname) + strlen(EXT_LBL_SUF) + 1];
         sprintf(extfuncname, "%s" EXT_LBL_SUF, funcname);
         firstinsn = asmfile_get_insn_by_label(pf->afile, extfuncname);
         if (firstinsn == NULL) {
            ERRMSG("Internal function %s could not be found in the file\n",
                  funcname);
            pf->last_error_code = ERR_LIBASM_FUNCTION_NOT_FOUND;
         }
      }
      if (firstinsn)
         out->fctptr = pointer_new(0, 0, firstinsn, POINTER_RELATIVE,
               TARGET_INSN);
      break;
   case STATIC:
      if (patchfile_addstaticinsertfunc(pf, out) != EXIT_SUCCESS) {
         ERRMSG(
               "External function %s could not be found in any added static library\n",
               funcname);
         pf->last_error_code = ERR_BINARY_EXTFCT_NOT_FOUND;
         /**\todo Add the function as an dynamic call*/
      }
      break;
   case DYNAMIC:
      if (patchfile_getinsextfctcall(pf, out) != EXIT_SUCCESS) {
         ERRMSG("External function %s could not be found or added to file\n",
               funcname);
         pf->last_error_code = ERR_BINARY_EXTFCT_NOT_FOUND;
      }
      break;
   default:/**\todo May not be done that way*/
      /*We will add the function call as a dynamic call, considering it might be defined in an external library,
       * but only if the executable can use functions from external libraries*/
      if (binfile_get_nb_ext_libs(pf->patchbin) > 0) {
         /**\warning (2014-11-07) I'm supposing here that 0 external libraries <=> static executable
          * We might have the case where a file is dynamic yet does not have any external library
          * (for instance with ELF, we could have a .dynamic section but not containing any DT_NEEDED entry)
          * I don't know if it is possible or even makes sense but if the case happens, this test will be wrong.*/
         int res = patchfile_getinsextfctcall(pf, out);
         if (res != EXIT_SUCCESS) {
            if (!ISERROR(pf->last_error_code))
               pf->last_error_code = ERR_PATCH_FUNCTION_NOT_INSERTED;
         }
      } else {
         ERRMSG("Label %s is not defined\n", funcname);
         pf->last_error_code = ERR_BINARY_SYMBOL_NOT_FOUND;
      }
      break; //There, are you happy Eclipse ?
      }
      /*Adding the function insertion to the list of inserted functions*/
      /*We do this even if it was not found or could not be inserted: in that case, firstinsn will be NULL so we will know we already
       * tried and failed to find it and will not try again*/
      queue_add_tail(pf->insertedfcts, out);
   }
   return out;
}

/**
 * Modifies a list of instructions to insert by adding the instructions representing the conditions (if necessary)
 * \param pf The patched file
 * \param inslstmod The modification containing the insertion request
 * \return EXIT_SUCCESS / error code (currently always EXIT_SUCCESS)
 * */
static int patchfile_insertlist_setconditions(patchfile_t* pf,
      modif_t* inslstmod)
{
   int out = EXIT_SUCCESS;
   if (inslstmod->condition == NULL)
      return out;
//    insn_t **stackinsns;
//    int newstack = ((inslstmod->flags&PATCHFLAG_NEWSTACK)?1:0);

   /*If this modification is the "else" of another modification, we don't need to save the flags storing the result of a comparison
    * as this will have been already done by the code of the condition*/
   if (inslstmod->annotate & A_MODIF_ISELSE)
      inslstmod->condition->insertconds->flags_nosave = TRUE;

   pf->patchdriver->add_conditions_to_insnlist(inslstmod->newinsns,
         inslstmod->condition->insertconds, pf->newstack,
         inslstmod->stackshift);

//    if(newstack) {
//        insn_t** iter;
//        /*Adds the instructions pointing to the stack address to the list of targets in the ELF file*/
//        for(iter=stackinsns;*iter!=NULL;iter++) {
//            elffile_add_targetscnoffset(pf->efile, (*iter), NEWDATASCNID,pf->datalen + NEWSTACKSIZE - 8, pf->patchdriver->upd_dataref_coding, 0, 0);
//        }
   /**\todo See the comment about NEWSTACKSIZE. It may need to be arch-dependent*/
//    }
   return out;
}

/**
 * Scans all the inserted relocatable files and checks if they have undefined symbols.
 * If they do, try adding a function insertion for each undefined symbol
 * \param pf A pointer to the structure holding the patched file
 * \return EXIT_SUCCESS if all symbols could be resolved, error code otherwise
 * */
static int patchfile_resolve_objssyms(patchfile_t* pf)
{
   int found = TRUE, out = EXIT_SUCCESS;
   FOREACH_INQUEUE(pf->insertedobjs, oiter) {
      /*Retrieves all unresolved symbols from the file*/
      queue_t* unresolved = binfile_find_ext_labels(
            asmfile_get_binfile(GET_DATA_T(asmfile_t*, oiter)));
      FOREACH_INQUEUE(unresolved, siter) {
         label_t* symlbl = GET_DATA_T(label_t*, siter);
         char* sym = label_get_name(symlbl);
         found = FALSE;
         /*Looks up for the symbol in the current file*/
         label_t* existlbl = asmfile_lookup_label(pf->afile, sym);
         if (existlbl && (label_get_type(existlbl) != LBL_EXTERNAL)) {
            /*Symbol is already present in the file: we add it to the hashtable of resolved symbols*/
            hashtable_insert(pf->extsymbols, sym, pf->afile);
            found = TRUE;
         } else {
            /*Looking up the symbol in the list of already inserted object files*/
            FOREACH_INQUEUE(pf->insertedobjs, oit) {
               existlbl = asmfile_lookup_label(GET_DATA_T(asmfile_t*, oit), sym);
               if (existlbl && (label_get_type(existlbl) != LBL_EXTERNAL)) {
                  /*Symbol is present in the list of already inserted files: just registering it*/
                  hashtable_insert(pf->extsymbols, sym,
                        GET_DATA_T(asmfile_t*, oit));
                  found = TRUE;
                  break;
               }
            }
            if (found == FALSE) {
               /*Symbol was not found in the already inserted object files: looking it up into all inserted libraries*/
               /**\todo See the todo in patchfile_addstaticinsertfunc: we are scanning some libraries twice as long as
                * they are not removed from insertedlibs when added into insertedobjs*/
               FOREACH_INQUEUE(pf->insertedlibs, lit)
               {
                  existlbl = asmfile_lookup_label(GET_DATA_T(asmfile_t*, lit),
                        sym);
                  if (existlbl && (label_get_type(existlbl) != LBL_EXTERNAL)) {
                     /*Found the symbol into another linked library*/
                     queue_add_tail(pf->insertedobjs,
                           GET_DATA_T(asmfile_t*, lit));/**\todo Here also, move them, etc*/
                     hashtable_insert(pf->extsymbols, sym,
                           GET_DATA_T(asmfile_t*, lit));
                     found = TRUE;
                     break;
                  }
               } //End of loop in inserted libraries (not actually inserted)
            }
//				if ( found == FALSE ) {
//					/*We still have not found the symbol anywhere: if it is a function, we will try to add it as an external symbol*/
//					if ( elffile_label_isfunc(asmfile_getorigin(GET_DATA_T(asmfile_t*, oiter)), sym) == 1 ) {
//						if ( elffile_isexedyn(pf->efile) == TRUE ) {
//							//patchfile_getinsertfunc(pf, sym, DYNAMIC);
//							WRNMSG("Symbol %s was not found in any of the linked libraries and was added as a dynamic symbol. "
//									"Patched file may fail if the symbol is not defined in an external library\n", sym);
//							//found = TRUE;
//							found = FALSE;
            if (!ISERROR(out))
               out = WRN_PATCH_SYMBOL_ADDED_AS_EXTERNAL;
//				}
         }
         if (found == FALSE) {
            ERRMSG(
                  "Symbol %s, present in %s, could not be found in any linked libraries. Patched file will fail\n",
                  sym, asmfile_get_name(GET_DATA_T(asmfile_t*, oiter)));
            out = ERR_PATCH_UNRESOLVED_SYMBOL;
         }
         //out &= found;
      }
      queue_free(unresolved, NULL);
   }
   return out;
}

/**
 * Generates the code for a function call to an executable file at a specified address.
 * The instructions composing the call are not inserted by this function.
 * \param pf A pointer to the structure holding the disassembled file
 * \param insfctmodif Structure containing the description of the function call to insert
 * \returns EXIT_SUCCESS if the operation succeeded, error code otherwise
 * */
static int patchfile_insfctcall(patchfile_t* pf, modif_t* insfctmodif)
{
   /**\todo RENAME THIS FUNCTION, IT DOES NOT PERFORM INSERTIONS ANYMORE BUT GENERATES THE CODE NECESSARY*/
   /**\todo TODO (2014-11-12) Do it. All functions need to be renamed anyway*/
   int out = EXIT_SUCCESS, res;
//    int out=TRUE;//,i;
//    tlsvar_t* rettlsvar = insfctmodif->data.insert->fct->rettlsvar;
//    globvar_t* retvar = insfctmodif->data.insert->fct->retvar;
//    insn_t* callinsn,**stackinsns,**iter,*gvinsns[nparams],*retinsn;
   pointer_t* fctstart = insfctmodif->fct->insfunc->fctptr;
//    int newstack = (insfctmodif->flags&PATCHFLAG_NEWSTACK)?1:0;
//    globvar_t** paramvars = insfctmodif->data.insert->fct->paramvars;
//    tlsvar_t** paramtlsvars = insfctmodif->data.insert->fct->paramtlsvars;
   queue_t* funccall;
   insn_t* callinsn = NULL;
   /*Gets the instruction list corresponding to the function call*/
   funccall = pf->patchdriver->generate_insnlist_functioncall(insfctmodif,
         &callinsn, fctstart, pf->newstack);
//    if (retvar != NULL) {
//        funccall = pf->patchdriver->generate_insnlist_functioncall(insfctmodif,fctstart,&callinsn,&stackinsns,gvinsns,&retinsn);
//        /*Stores the return instruction (referencing a global variable) if existing*/
//    } else if (rettlsvar != NULL) {
//        funccall = pf->patchdriver->generate_insnlist_functioncall(insfctmodif,fctstart,&callinsn,&stackinsns,gvinsns,&retinsn);
//        /*Stores the return instruction (referencing a global variable) if existing*/
//        if (retinsn != NULL) {
//            if (rettlsvar->type == INITIALIZED) {
//                out = elffile_add_targetscnoffset(pf->efile, retinsn, NEWTDATASCNID, pf->tdatalen - rettlsvar->offset - rettlsvar->size, pf->patchdriver->upd_tlsref_coding, 0, 0);
//            }
//            else if (rettlsvar->type == UNINITIALIZED) {
//            	out = elffile_add_targetscnoffset(pf->efile, retinsn, NEWTBSSSCNID, pf->tbsslen - rettlsvar->offset - rettlsvar->size, pf->patchdriver->upd_tlsref_coding, 0, 0);
//            }
//        }
//        /**\todo See the comment about NEWSTACKSIZE. It may need to be arch-dependent*/
//    }
//    /*Stores the instructions corresponding to parameters referencing a global variable*/
//    for(i = 0; i < nparams; i++) {
//        if ( paramvars[i] != NULL ) {
//            if ( paramvars[i]->type == VAR_CREATED ) {
//                /*New variable: adding it to the elf file with its offset along with the instruction pointing to it, linked to the future data section*/
//                elffile_add_targetscnoffset(pf->efile, gvinsns[i], NEWDATASCNID, paramvars[i]->offset, pf->patchdriver->upd_dataref_coding, 0, 0);
//            } else if ( paramvars[i]->type == VAR_EXIST ) {
//                /*Existing variable: we add a link to it referencing the instruction in the variable table*/
//                elffile_add_targetaddr(pf->efile, gvinsns[i], paramvars[i]->offset, pf->patchdriver->upd_dataref_coding, 0, 0);
//            }
//        }
//    }
   insfctmodif->newinsns = funccall;
   hashtable_insert(pf->newbranches, pointer_get_insn_target(fctstart), callinsn);

//    /*Stores the instructions corresponding to parameters referencing a global variable*/
//    for(i = 0; i < nparams; i++) {
//        if ( paramtlsvars[i] != NULL ) {
//             if (paramtlsvars[i]->type == INITIALIZED) {
//                elffile_add_targetscnoffset(pf->efile, gvinsns[i], NEWTDATASCNID, pf->tdatalen - paramtlsvars[i]->offset - paramtlsvars[i]->size, pf->driver->upd_tlsref_coding, 0, 0);
//                DBGMSG("Added target from initialized tls vars. Offset set to %#"PRIx64" (%#"PRIx64" - %#"PRIx64" - %#"PRIx32")\n",
//                      pf->tdatalen - paramtlsvars[i]->offset - paramtlsvars[i]->size, pf->tdatalen, paramtlsvars[i]->offset, paramtlsvars[i]->size);
//             }
//             if (paramtlsvars[i]->type == UNINITIALIZED) {
//                elffile_add_targetscnoffset(pf->efile, gvinsns[i], NEWTBSSSCNID, pf->tbsslen - paramtlsvars[i]->offset - paramtlsvars[i]->size, pf->driver->upd_tlsref_coding, 0, 0);
//                DBGMSG("Added target from uninitialized tls vars. Offset set to %#"PRIx64" (%#"PRIx64" - %#"PRIx64" - %#"PRIx32")\n",
//                      pf->tbsslen - paramtlsvars[i]->offset - paramtlsvars[i]->size, pf->tbsslen, paramtlsvars[i]->offset, paramtlsvars[i]->size);
//             }
//        }
//    }
//  lc_free(funccall); //Don't use queue_free, as the elements of the queue have been added to the asmfile
//  /**\todo Update this after the patcher refactoring if the elements of the list are not added directly to the asmfile*/

   return out;
}

///**
// * Adds a function to an executable, without any call pointing to it.
// * If the type is DYNAMIC, a function stub will be added. If it is STATIC, nothing will be done and an error will be returned.
// * If it is INTERNAL, nothing will be done to the file
// * \param pf A pointer to the structure holding the disassembled file
// * \param fctname Name of the function to add
// * \param fcttype Type of the function
// * \return EXIT_SUCCESS if the insertion was successful, error code otherwise
// * */
//static int patchfile_patch_insertfct(patchfile_t* pf, char* fctname, int fcttype) {
//    /**\todo Improve this function to support more options*/
//    return ((patchfile_getinsertfunc(pf, fctname, fcttype) != NULL)?TRUE:FALSE);
//}

/**
 * Processes, but does not apply, a request for replacing an instruction
 * \param pf Pointer to structure describing the patched file
 * \param repmod Structure containing the details about the replacement (it's assumed to be a modif_t of type MODTYPE_REPLACE)
 * \return EXIT_SUCCESS if successful, error code otherwise
 * \note Currently does nothing, but for use in future evolutions
 * */
static int replace_process(patchfile_t* pf, modif_t* repmod)
{
   /**\todo TODO (2014-12-23) Rewrite all this now that I have removed the possibility of adding multiple instructions to replace*/
   /**\todo TODO (2014-12-23) We may completely remove this function and handle it as a delete with a special kind of padding
    * (2015-01-06) Actually it should be a insnmodify, where the new instruction is a nop of the same size (or the closest inferior)
    * with padding enabled*/
//    (void)pf; //Avoiding compilation warnings
   assert(pf && repmod);
   DBGMSG("Processing replacement modif_%d at address %#"PRIx64"\n",
         MODIF_ID(repmod), repmod->addr);
///////PATCHER REFACTOR - Copying code from replace_apply (which we will not need afterward)
//    int64_t addr = repmod->addr;
   list_t* node = repmod->modifnode;
   //replace_t* delete = repmod->data.replace;
   assert(node);
//    int nins = 0;
   uint16_t delsize = 0;
   list_t* seq;
//    if (node != NULL)
   seq = node;
//    else
//        seq = insn_get_sequence(patchfile_getinsnataddr(pf,addr,NULL,NULL)); //delins->sequence could be used instead
   insn_t* delins = GET_DATA_T(insn_t*, seq);
   assert(seq == insn_get_sequence(delins));
   insn_t* deliter = delins;
   queue_t* noplist = queue_new();
   /*Calculate the size of the list of instructions to remove*/
//    while(nins<delete->ninsn) {
   //Generates a NOP instruction of the same size as the instruction we are deleting
   //delsize=0;
   insn_t* del;
   del = pf->patchdriver->generate_insn_nop(insn_get_size(deliter));
   if (del)
      add_insn_to_insnlst(del, noplist);
   delsize += insn_get_bytesize(del);
   //Creating NOP instructions until the right size is reached in case we could not reach the right size the first time
   while (delsize < insn_get_bytesize(deliter)) {
      //We allow for the fact that the nop instruction could be a list <===== Seems I changed my mind since I wrote this ??
//          queue_t* delq = queue_new();
      del = pf->patchdriver->generate_insn_nop(0);
      if (del)
         add_insn_to_insnlst(del, noplist);
//             add_insn_to_insnlst(del,delq);
      delsize += insn_get_bytesize(del);    //insnlist_bitsize(delq, NULL, NULL);
//          //Adds the nop to the list of instructions
//          if(delq)
//             queue_append(noplist,delq);
   }
   /*Updates any label insertion request so that it will point to the next address*/
   modiflbls_upd(pf, seq, seq->next);
   /**\todo TODO (2014-12-15) The invocation of modiflbls_upd is not needed anymore, but beware how you handle label modifications*/

//       seq = seq->next;
//       if(seq) deliter = (insn_t*)seq->data;
//       else break;/**\todo ERRMSG HANDLING*/
//       nins++;
//    }
//    repmod->patchmodif = patchmodif_new(node, seq, noplist, MODIFPOS_REPLACE, 0);
   repmod->newinsns = noplist;
   repmod->size = 0;
///////PATCHER REFACTOR - End
   repmod->annotate |= A_MODIF_PROCESSED;
   return EXIT_SUCCESS;
}
#if 0
/**
 * Replaces an instruction
 * \param pf Pointer to structure describing the patched file
 * \param repmod Structure containing the details about the replacement (it's assumed to be a modif_t of type MODTYPE_REPLACE)
 * \return EXIT_SUCCESS / error code
 * */
static int replace_apply(patchfile_t* pf,modif_t* repmod) {
   int out=EXIT_SUCCESS;
   if(!repmod)
   return ERR_PATCH_MISSING_MODIF_STRUCTURE;
   if (pf == NULL) {
      return ERR_PATCH_NOT_INITIALISED;
   }
   DBGMSG("Applying replacement modif_%d at address %#"PRIx64"\n",MODIF_ID(repmod),repmod->addr);
   int64_t addr = repmod->addr;
   list_t* node = repmod->modifnode;
   replace_t* delete = repmod->data.replace;

   int nins = 0,delsize = 0;
   list_t* seq;
   if (node != NULL)
   seq = node;
   else
       seq = insn_get_sequence(patchfile_getinsnataddr(pf,addr,NULL,NULL)); //delins->sequence could be used instead
   /**\todo The line above is stupid, simplify it*/
   insn_t* delins = GET_DATA_T(insn_t*, seq);
   assert(seq==insn_get_sequence(delins));
   insn_t* deliter = delins;
   queue_t* noplist = queue_new();
   /*Calculate the size of the list of instructions to remove*/
   while(nins<delete->ninsn) {
      //Generates a NOP instruction of the same size as the instruction we are deleting
      delsize=0;
      //Creating NOP instructions until the right size is reached
      do {
         insn_t* del;
         //We allow for the fact that the nop instruction could be a list
         queue_t* delq = queue_new();
         del = pf->patchdriver->generate_insn_nop(insn_get_size(deliter));
         if(del)
         add_insn_to_insnlst(del,delq);
         delsize += insnlist_bitsize(delq, NULL, NULL);
         //Adds the nop to the list of instructions
         if(delq)
            queue_append_and_keep(noplist,delq);
      } while(delsize<insn_get_size(deliter));
      /*Updates any label insertion request so that it will point to the next address*/
      modiflbls_upd(pf, seq, seq->next);

      seq = seq->next;
      if(seq) deliter = (insn_t*)seq->data;
      else break;/**\todo ERRMSG HANDLING*/
      nins++;
   }
   /**\todo Improve this function to avoid looking again for beginning and end*/
   out = patchfile_patch_replaceinsn(pf,insn_get_sequence(delins),delete->ninsn,-1,noplist,0,repmod->flags,repmod->paddinginsn);
   delete->deleted = noplist;
   /**\todo And what about the branches pointing to the instruction that was just replaced ? Fix this*/
   /**\todo Instead of calling queue_swap here, invoke a function from libmpatch.c where the branches are updated, the annotate, etc
    * => Attempting to do it, but it's not sure this will work*/

   repmod->annotate |= A_MODIF_APPLIED;

   return out;
}
#endif
/**
 * Processes, but does not apply, a request for an instruction deletion
 * \param pf Pointer to structure describing the patched file
 * \param delmod Structure containing the details about the deletion (it's assumed to be a modif_t of type MODTYPE_DELETE)
 * \return EXIT_SUCCESS if successful, error code otherwise
 * \note Currently does nothing, but for use in future evolutions
 * */
static int delete_process(patchfile_t* pf, modif_t* delmod)
{
   /**\todo TODO (2014-12-23) Rewrite all this now that I have removed the possibility of adding multiple instructions to delete*/
//    (void)pf; //Avoiding compilation warnings
   assert(pf && delmod);
   DBGMSG("Processing deletion modif_%d at address %#"PRIx64"\n",
         MODIF_ID(delmod), delmod->addr);
///////PATCHER REFACTOR - Copying code from delete_apply (which we will not need afterward)
//    int64_t addr = delmod->addr;
//    list_t* node = delmod->modifnode;
//    delete_t* delete = delmod->data.delete;
//    assert(node);
//    int nins=0;
//    int64_t delsize=0;
//    list_t* seq, *iter;
////    if ( node  != NULL )
//       seq = node;
////    else
////       seq = insn_get_sequence(patchfile_getinsnataddr(pf,addr,NULL,NULL)); //delins->sequence could be used instead
   /**\todo The line above is stupid, simplify it*/
//   insn_t* delins = GET_DATA_T(insn_t*,delmod->modifnode);
//    if(!delins) {
//       STDMSG("Unable to retrieve instruction for deletion at address %#"PRIx64"\n",addr);
//       return FALSE;
//    }
   //DBGMSG("Applying deletion of instruction at address %#"PRIx64"\n",addr);
   //Calculate the size of the list of instructions to remove
   //iter = seq;
   //queue_t* delinsns = queue_new();
//    while(iter&&(nins<delete->ninsn)) {
   delmod->size = -(insn_get_bytesize(GET_DATA_T(insn_t*, delmod->modifnode)));
//       nins++;
   //Updates any label insertion request so that it will point to the next address
   modiflbls_upd(pf, delmod->modifnode, delmod->modifnode->next);
   //add_patchinsn_to_list(delinsns, patchinsn_new(GET_DATA_T(insn_t*,iter), NULL, A_PATCHDEL));
   /**\todo TODO (2014-12-15) The invocation of modiflbls_upd is not needed anymore, but beware how you handle label modifications*/

//       iter = iter->next;
//    }
//    delmod->patchmodif = patchmodif_new(seq, iter, NULL, MODIFPOS_REPLACE, -delsize);
   /**\todo (2014-11-17) For later: if we need to link deletion modifications together (for DECAN for instance), */
///////PATCHER REFACTOR - End copy
   delmod->annotate |= A_MODIF_PROCESSED;
   return EXIT_SUCCESS;
}
#if 0
/**
 * Performs an instruction deletion
 * \param pf Pointer to structure describing the patched file
 * \param delmod Structure containing the details about the deletion (it's assumed to be a modif_t of type MODTYPE_DELETE)
 * \return EXIT_SUCCESS if successful, error code otherwise
 * */
static int delete_apply(patchfile_t* pf,modif_t* delmod) {
   int out=EXIT_SUCCESS;/**\todo ERROR HANDLING*/
   if(!delmod)
   return ERR_PATCH_MISSING_MODIF_STRUCTURE;
   if (pf == NULL)
   return ERR_PATCH_NOT_INITIALISED;

   DBGMSG("Performing deletion modif_%d at address %#"PRIx64"\n",MODIF_ID(delmod),delmod->addr);
   int64_t addr = delmod->addr;
   list_t* node = delmod->modifnode;
   delete_t* delete = delmod->data.delete;

   int nins=0;
   int64_t delsize=0;
   list_t* seq, *iter;
   if ( node != NULL )
   seq = node;
   else
      seq = insn_get_sequence(patchfile_getinsnataddr(pf,addr,NULL,NULL)); //delins->sequence could be used instead
   /**\todo The line above is stupid, simplify it*/
   insn_t* delins = GET_DATA_T(insn_t*, seq);
   if(!delins) {
      ERRMSG("Unable to retrieve instruction for deletion at address %#"PRIx64"\n",addr);
      return ERR_LIBASM_INSTRUCTION_NOT_FOUND;
   }
   DBGMSG("Applying deletion of %d instruction(s) at address %#"PRIx64"\n",delete->ninsn,addr);
   //Calculate the size of the list of instructions to remove
   iter = seq;
   while(iter&&(nins<delete->ninsn)) {
      delsize += insn_get_size(GET_DATA_T(insn_t*, iter));
      nins++;
      //Updates any label insertion request so that it will point to the next address
      modiflbls_upd(pf, iter, iter->next);

      iter = iter->next;
   }
   //Performs the actual deletion
   delete->deleted = patchfile_patch_removecode(pf,addr,seq,delsize,delmod->flags,delmod->paddinginsn);
   /**\todo Check that seq is actually used to avoid having to look for it again ?*/
   if (!delete->deleted)
   out = patchfile_get_last_error_code(pf);

   delmod->annotate |= A_MODIF_APPLIED;
   return out;
}
#endif
/**
 * Processes, but does not apply, a request for an instruction modification
 * \param pf Pointer to structure describing the patched file
 * \param insmod Structure containing the details about the instruction modification (it's assumed to be a modif_t of type MODTYPE_REPLACE)
 * \return EXIT_SUCCESS if successful, error code otherwise
 * \note Currently does nothing, but for use in future evolutions
 * */
static int insnmodify_process(patchfile_t* pf, modif_t* insmod)
{
   assert(pf && insmod);
//    (void)pf; //Avoiding compilation warnings
   DBGMSG(
         "Processing instruction modification modif_%d at address %#"PRIx64"\n",
         MODIF_ID(insmod), insmod->addr);
///////PATCHER REFACTOR - Copying code from insmodify_apply (which we will not need afterward)
   int64_t addr = insmod->addr;
   list_t* node = insmod->modifnode;
   insnmodify_t* imod = insmod->insnmodify;
   int64_t pmsz = 0;
//   int nscn = -1;
   list_t* seq;
   assert(node);
//   if (node != NULL) {
   seq = node;
//   } else
//      seq = insn_get_sequence(patchfile_getinsnataddr(pf,addr,&nscn,NULL)); //delins->sequence could be used instead
   /**\todo The line above is stupid, simplify it*/
   queue_t* newinsq = queue_new();
   insn_t* modins = NULL;
   if (seq != NULL)
      modins = GET_DATA_T(insn_t*, seq);
   if (!modins) {
      ERRMSG(
            "Unable to retrieve instruction for modification at address %#"PRIx64"\n",
            addr);
      return ERR_LIBASM_INSTRUCTION_NOT_FOUND;
   }
   insn_t* newins = modify_insn(modins, imod->newopcode, imod->newparams,
         imod->n_newparams, pf->asmbldriver);

   /**\todo Error handling here*/
   add_insn_to_insnlst(newins, newinsq);
   /**\todo TODO (2014-12-15) I'm making this up as I go along so the following code is fugly but I'm tired and vacations are one week too far*/
//   add_patchinsn_to_list(newinsq, patchinsn_new(modins, newins, A_PATCHMOD));
   //Updates any label insertion request so that it will point to the new node
   modiflbls_upd(pf, seq, queue_iterator(newinsq));
   /**\todo TODO (2014-12-15) The invocation of modiflbls_upd is not needed anymore, but beware how you handle label modifications*/

   //Special case: detecting if the instruction is modified into a branch
   if (insn_is_direct_branch(newins)) {
      insn_t* newtarget = insn_get_branch(newins);
      hashtable_insert(pf->newbranches, newtarget, newins);
   }

   pmsz = insn_get_bytesize(newins) - insn_get_bytesize(modins);
   if ((pmsz < 0) && imod->withpadding) {
      //Special case: The new instruction is shorter than the old one, and we want to have some padding
      uint16_t size = insn_get_bytesize(newins);
      //Generates a NOP instruction of the same size as the instruction we are modifying
      //Creating NOP instructions until the right size is reached
      do {
         insn_t* del;
         //We allow for the fact that the nop instruction could be a list
         del = pf->patchdriver->generate_insn_nop(insn_get_size(modins) - size);
         size += insn_get_bytesize(del);
         //Adds the nop to the list of instructions
         if (del)
            add_insn_to_insnlst(del, newinsq);
//         add_patchinsn_to_list(newinsq, patchinsn_new(NULL, del, A_PATCHNEW));
      } while (size < insn_get_bytesize(modins));
      pmsz = 0;
   }
   //insmod->patchmodif = patchmodif_new(seq, seq, newinsq, MODIFPOS_REPLACE, pmsz);
   insmod->newinsns = newinsq;
   insmod->size = pmsz;

///////PATCHER REFACTOR - End copy
   insmod->annotate |= A_MODIF_PROCESSED;
   return EXIT_SUCCESS;
}

#if 0
/**
 * Perform a modification at the given address
 * \param pf Pointer to structure describing the patched file
 * \param insmod Structure containing the details about the instruction modification (it's assumed to be a modif_t of type MODTYPE_REPLACE)
 * \return EXIT_SUCCESS if successful, error code otherwise
 * */
static int insnmodify_apply(patchfile_t* pf, modif_t* insmod) {
   int out = EXIT_SUCCESS;
   if (pf == NULL) {
      return ERR_PATCH_NOT_INITIALISED;
   }

   if(insmod) {
      DBGMSG("Performing instruction modification modif_%d at address %#"PRIx64"\n",MODIF_ID(insmod),insmod->addr);
      int64_t addr = insmod->addr;
      list_t* node = insmod->modifnode;
      insnmodify_t* imod = insmod->data.insnmodify;

      int nscn = -1;
      list_t* seq;
      if (node != NULL) {
         seq = node;
      } else
         seq = insn_get_sequence(patchfile_getinsnataddr(pf,addr,&nscn,NULL)); //delins->sequence could be used instead
      /**\todo The line above is stupid, simplify it*/
      queue_t* newinsq = queue_new();
      insn_t* modins = NULL;
      if ( seq != NULL )
      modins = GET_DATA_T(insn_t*, seq);
      if(!modins) {
         ERRMSG("Unable to retrieve instruction for modification at address %#"PRIx64"\n",addr);
         return ERR_LIBASM_INSTRUCTION_NOT_FOUND;
      }
      insn_t* newins = modify_insn(modins,imod->newopcode,imod->newparams,imod->n_newparams, pf->asmbldriver);
      /**\todo Error handling here*/
      add_insn_to_insnlst(newins,newinsq);
      //Updates any label insertion request so that it will point to the new node
      modiflbls_upd(pf, seq, queue_iterator(newinsq));

      if((insn_get_size(modins)>insn_get_size(newins))&&imod->withpadding) {
         //Special case: The new instruction is shorter than the old one, and we want to have some padding
         int size = insn_get_size(newins);
         //Generates a NOP instruction of the same size as the instruction we are deleting
         //Creating NOP instructions until the right size is reached
         do {
            insn_t* del;
            //We allow for the fact that the nop instruction could be a list
            del = pf->patchdriver->generate_insn_nop(insn_get_size(modins)-size);
            size += insn_get_size(del);
            //Adds the nop to the list of instructions
            if(del) add_insn_to_insnlst(del,newinsq);
         } while(size<insn_get_size(modins));
      }

      //Now calling the function to perform the replacement (and moving the code around, etc)
      out = patchfile_patch_replaceinsn(pf,seq,1,nscn,newinsq,1,insmod->flags,insmod->paddinginsn);
      /**\bug newinsq is never freed. However there is a bug because the lists objects could be already present in some pending modifs*/
   }
   insmod->annotate |= A_MODIF_APPLIED;

   return out;
}

/**
 * Applies a pending insertion
 * \param pf Pointer to structure describing the patched file
 * \param insmod Structure containing the details about the insertion (it's assumed to be a modif_t of type MODTYPE_INSERT)
 * \return EXIT_SUCCESS or error code
 * */
static int insert_apply(patchfile_t* pf, modif_t* insmod) {
   DBGMSG("Applying insertion modif_%d at address %#"PRIx64"\n",MODIF_ID(insmod),insmod->addr);
   if ( (insmod->addr == 0) && ( !(insmod->annotate&A_MODIF_ATTACHED) ) && ( !(insmod->flags&PATCHFLAG_INSERT_FCTONLY) ) ) {
      ERRMSG("Modification %d is not associated to an address and no other modification points to it. It will not be applied\n",
            MODIF_ID(insmod));
      return ERR_PATCH_MISSING_MODIF_ADDRESS;
   }
   return patchfile_insertcode(pf, insmod->data.insert->insnlist, insmod->addr, insmod->modifnode, insmod->data.insert->after, insmod->flags, insmod->paddinginsn);
}
#endif

static int insert_process(patchfile_t* pf, modif_t* insmod);

/**
 * Handles the connection of an insertion to an address
 * \param pf Patched file
 * \param insmod Modification of type insertion
 * \return EXIT_SUCCESS / error code (currently always EXIT_SUCCESS)
 * */
static int insert_handle_nextinsn(patchfile_t* pf, modif_t* insmod)
{
   int out = EXIT_SUCCESS;
   /*When inserting branches, we could have both nextinsn and nextmodif not NULL (this should be the only case where this happen), if we linked
    * to another instruction and followed the branch by another modification. In this case, we have to link to the destination first then append to the
    * modification
    * */
   /**\todo Add some sanity checks to be sure of what we are doing if nextinsn and nextmodif are both not NULL*/
   if (insmod->nextinsn != NULL) {
      //Branching to an existing instruction in the file
      pointer_t* jmpptr;
      insn_t* jmp;
      queue_t* jmpls = pf->patchdriver->generate_insnlist_jmpaddr(NULL, &jmp,
            &jmpptr);
      DBGMSG(
            "Condition %d ends with an unconditional branch to address %#"PRIx64"\n",
            MODIF_ID(insmod), insn_get_addr(insmod->nextinsn));
      //Points branch instruction to the instruction (may be updated later during finalisation)
      patchfile_setbranch(pf, jmp, insmod->nextinsn, jmpptr);
      //pointer_set_insn_target(jmpptr, insmod->nextinsn);
//        if (insmod->flags & PATCHFLAG_BRANCH_NO_UPD_DST)
//            patchfile_setbranch_noupd(pf, jmp,insmod->nextinsn);
//        else
//            patchfile_setbranch(pf, jmp,insmod->nextinsn);
      queue_append(insmod->newinsns, jmpls);
      //lc_free(jmpls);
      /**\todo Factorise this code with the one below*/
   }
   return out;
}

/**
 * Handles the connection of an insertion to another
 * \param pf Patched file
 * \param insmod Modification of type insertion
 * \return EXIT_SUCCESS / error code
 * */
static int insert_handle_nextmodif(patchfile_t* pf, modif_t* insmod)
{
   int out = EXIT_SUCCESS;
   if (insmod->nextmodif != NULL) {
      assert(insmod->nextmodif->type == MODTYPE_INSERT); //For now, we don't support other types of linking
      if (!(insmod->nextmodif->annotate & A_MODIF_PROCESSED)) {
         out = insert_process(pf, insmod->nextmodif);
         if (ISERROR(out)) {
            //Propagates error detected when processing one of the linked modifications
            insmod->annotate |= A_MODIF_ERROR;
         }
      }
      if ((insmod->nextmodif->position == MODIFPOS_FLOATING)
            && (!(insmod->nextmodif->annotate & A_MODIF_ATTACHED))
            /*&& (queue_length(insmod->data.insert->insnlist) > 0)*//**\todo WARNING! This is a bit of a hack to detect branch insertions, where we must create a branch*/
            ) {
         DBGMSG("Modification %d will be appended to modification %d\n",
               MODIF_ID(insmod), MODIF_ID(insmod));
         //"floating" modification: we just append it
         queue_append(insmod->newinsns, insmod->nextmodif->newinsns);
         //lc_free(insmod->nextmodif->newinsns);
         insmod->nextmodif->newinsns = NULL;
         insmod->nextmodif->annotate |= (A_MODIF_ATTACHED | A_MODIF_APPLIED); //Registers that the modif has been appended to another, and we will not have to apply it
      } else {
         //Not a floating modification: we have to add a link to it
         pointer_t* jmpptr;
         queue_t* jmpls = pf->patchdriver->generate_insnlist_jmpaddr(NULL, NULL,
               &jmpptr);
         DBGMSG("Creating a branch from modification %d to modification %d\n",
               MODIF_ID(insmod), MODIF_ID(insmod));
         /*If the branch was fixed to this instruction, we don't add it to the table of branches so that it will not
          * be updated if something is inserted before it. Otherwise, we add it normally using patchfile_setbranch,
          * which updates the branches hashtable*/
//            if (insmod->flags & PATCHFLAG_BRANCH_NO_UPD_DST)
//                patchfile_setbranch_noupd(pf, jmp,(insn_t*)queue_peek_head(insmod->nextmodif->data.insert->insnlist));
//            else
//                patchfile_setbranch(pf, jmp,(insn_t*)queue_peek_head(insmod->nextmodif->data.insert->insnlist));
         /**\todo TODO (2014-11-18) This pointer will have to be updated when we finalise all modifications (when we are sure the target can't be removed)*/
         insmod->nextmodifptr = jmpptr;
         queue_append(insmod->newinsns, jmpls);
         //lc_free(jmpls);
      }

   } else if ( /*(insmod->nextmodif == NULL) &&*/(insmod->nextinsn == NULL)
         && (insmod->addr == 0) && (!(insmod->annotate & A_MODIF_ATTACHED))
         && (!(insmod->flags & PATCHFLAG_INSERT_FCTONLY))) {
      /*Sanity check: we found a "floating" modification at address 0 that has no successor */
      ERRMSG(
            "Modification %d has no fixed address and no successor. Its predecessors and itself will not be applied\n",
            MODIF_ID(insmod));
      insmod->annotate |= A_MODIF_ERROR;
      out = ERR_PATCH_FLOATING_MODIF_NO_SUCCESSOR;
   }
   return out;
}

/**
 * Perform an insertion at the given address
 * \param pf Pointer to structure describing the patched file
 * \param insmod Structure containing the details about the insertion (it's assumed to be a modif_t of type MODTYPE_INSERT)
 * \return EXIT_SUCCESS or error code
 * */
static int insert_process(patchfile_t* pf, modif_t* insmod)
{
   /**\todo TODO (2014-11-05) Refactor this to remove a lot of intermediary steps and sub structures that we won't need anymore*/
   assert(pf && insmod);
   int out = EXIT_SUCCESS;
   int res = EXIT_SUCCESS;
   if (pf == NULL)
      return ERR_PATCH_NOT_INITIALISED;

   if ((insmod->annotate & A_MODIF_PROCESSED)) {
      DBGMSG("Insertion modif_%d has already been processed\n",
            MODIF_ID(insmod));
      return out;
   }

   DBGMSG("Processing insertion modif_%d at address %#"PRIx64"\n",
         MODIF_ID(insmod), insmod->addr);
//       insert_t* insert = insmod->data.insert;
   //int fcttype = UNDEFINED;
   if (insmod->condition != NULL) {
      DBG(
            char strcond[8192]; cond_print(insmod->condition, strcond, sizeof(strcond), asmfile_get_arch(pf->afile)); DBGMSG("Insertion has condition:%s\n", strcond));
      cond_serialize(insmod->condition);
      if (insmod->condition->elsemodif != NULL) {
         out = insert_process(pf, insmod->condition->elsemodif);
         if (ISERROR(out)) {
            //Error when processing the else condition
            insmod->annotate |= A_MODIF_ERROR;
         } else {
            //Links the else modif to the conditions
            insmod->condition->insertconds->elsecode =
                  insmod->condition->elsemodif->newinsns;
            insmod->condition->elsemodif->annotate |= A_MODIF_APPLIED;
            /**\todo It should be marked with A_MODIF_ATTACHED as well, but this was done in libmadras.c, check how
             * this could be done here instead*/
         }
      }
   }
   if (insmod->fct != NULL) {
      //Function call
      if (insmod->flags & PATCHFLAG_INSERT_FCTONLY) {
         //Only inserting the function stub
         DBGMSG("Inserting function %s\n", insmod->fct->funcname);
         res = patchfile_getinsertfunc(pf, insmod->fct);
         UPDATE_ERRORCODE(out, res)
      } else {
         //DBGMSG("Inserting function call at address %#"PRIx64"\n",insmod->addr);
         insertfunc_t* isf;
         DBGMSG("Inserting call to function %s at address %#"PRIx64"\n",
               insmod->fct->funcname, insmod->addr);
         /*Looks up the function in the list of inserted function calls, and create it if it did not exist*/
         isf = patchfile_getinsertfunc(pf, insmod->fct);
         /*Generates the list of instruction for this */
         if ((isf != NULL) && (isf->fctptr != NULL)) {
            insmod->fct->insfunc = isf;
            /*The function was found*/
            out = patchfile_insfctcall(pf, insmod);
         } else {
            res = patchfile_get_last_error_code(pf);
            UPDATE_ERRORCODE(out, res)
         }

      }
      //Now we will link this modification with another address
      res = insert_handle_nextinsn(pf, insmod);
      UPDATE_ERRORCODE(out, res)
      //Now we will link this modification with any other that is not set at a given address
      res = insert_handle_nextmodif(pf, insmod);
      UPDATE_ERRORCODE(out, res)
   } else if (insmod->newinsns != NULL) {
      //Instruction lists
      //Now we will link this modification with another address
      res = insert_handle_nextinsn(pf, insmod);
      UPDATE_ERRORCODE(out, res)
      //We separate this one from the next insert_handle_nextmodif to handle the case of branch insertion and create the instruction list

      //Applies the conditions
      DBGMSG("Adding conditions to list insertion %d\n", MODIF_ID(insmod));
      res = patchfile_insertlist_setconditions(pf, insmod);
      UPDATE_ERRORCODE(out, res)

      //Now we will link this modification with any other that is not set at a given address
      res = insert_handle_nextmodif(pf, insmod);
      UPDATE_ERRORCODE(out, res)
   }
   uint64_t insertsz = insnlist_findbytesize(insmod->newinsns, NULL, NULL); //insnlist_bitsize(insert->insnlist, NULL, NULL)/8;
   /**\todo TODO (2014-12-15) Modify the patcher arch-specific functions to return queues of patchinsn_t structures, instead of insn_t
    * The queues of instructions set in the modif_t sub structures should contain those, so we can directly use them.
    * This loop should therefore not be needed afterward
    * => (2015-01-06) Modifying this once again: patchmodif_t contains only a queue of instruction, this loop will have to be done when
    * finalising everything*/
//      FOREACH_INQUEUE(insert->insnlist, iter) {
//         iter->data = patchinsn_new(NULL, GET_DATA_T(insn_t*, iter), A_PATCHNEW);
//         GET_DATA_T(patchinsn_t*, iter)->seq = iter;
//      }
//      if (!(insmod->annotate & A_MODIF_ATTACHED))  //Not creating patchmodif for attached modifs, they will be created in the main modif
//         insmod->patchmodif = patchmodif_new(insmod->modifnode, insmod->modifnode, insert->insnlist,
//            insert->after?MODIFPOS_AFTER:MODIFPOS_BEFORE, insertsz);
   /**\todo TODO (2015-05-27) I've said it before and I'll say it again: remove those patchmodifs once and for all
    * The hack above is to handle the free correctly. Anyway the modif_t structures would gain to be heavily simplified
    * => (2015-05-28) Doing it now (or attempting to anyway)*/
   insmod->size = insertsz;
   insmod->annotate |= A_MODIF_PROCESSED;

   return out;
}

/**
 * Processes, but does not apply, a request for an instruction relocation
 * \param pf Pointer to structure describing the patched file
 * \param movmod Structure containing the details about the instruction relocation (it's assumed to be a modif_t of type MODTYPE_RELOCATE)
 * \return EXIT_SUCCESS if successful, error code otherwise
 * */
static int relocate_process(patchfile_t* pf, modif_t* movmod)
{
   /**\todo TODO (2015-01-21) Creating an empty patchmodif object for coherence. To be modified if I find something more clever to handle
    * forced relocations modifications.*/
   (void) pf; //Avoiding compilation warnings
   DBGMSG("Processing instruction relocation modif_%d at address %#"PRIx64"\n",
         MODIF_ID(movmod), movmod->addr);
   //movmod->patchmodif = patchmodif_new(movmod->modifnode, movmod->modifnode, NULL, MODIFPOS_KEEP, 0);
   movmod->position = MODIFPOS_KEEP;
   movmod->annotate |= A_MODIF_PROCESSED;
   return EXIT_SUCCESS;
}
#if 0
/**
 * Apply a relocation only modification. The block (as defined by the various flags) containing the instruction
 * will be moved, no other modifications will be applied
 * \param pf Pointer to structure describing the patched file
 * \param movmod Structure containing the details about the relocation only modification (it's assumed to be a modif_t of type MODTYPE_RELOCATE)
 * \return EXIT_SUCCESS or error code
 * */
static int relocate_apply(patchfile_t* pf, modif_t* movmod) {
   assert(pf && movmod && movmod->type == MODTYPE_RELOCATE);

   int scnidx;
   int64_t addr = movmod->addr;
   list_t* node = movmod->modifnode;
   list_t* lstmoved = NULL;
   insn_t* moved = NULL;
   DBGMSG("Relocating block around instruction at address %#"PRIx64"\n",addr);
   /*Retrieve the instruction at which to insert the code*/
   if ((node != NULL) && (GET_DATA_T(insn_t*, node) != NULL) && (insn_check_annotate(GET_DATA_T(insn_t*, node), A_PATCHDEL) == FALSE)) {
      lstmoved = node;
      moved = GET_DATA_T(insn_t*, node);
      DBGMSGLVL(1,"Retrieving instruction %p from node %p\n",moved,node);
         scnidx = patchfile_getscnid_byaddr(pf, moved, INSN_GET_ADDR(moved));
   } else {
      moved = patchfile_getinsnataddr(pf,addr,&scnidx,&lstmoved);
   }
   if (!moved) { //No instruction found at this address
      ERRMSG("Unable to find instruction at address %#"PRIx64"\n",addr);
      return ERR_LIBASM_INSTRUCTION_NOT_FOUND;
   }
   DBG(
         char buf[256];
         insn_print(moved, buf, sizeof(buf));
         DBGMSG("Found instruction %#"PRIx64":%s (%p) in section %d\n",INSN_GET_ADDR(moved),buf,moved,scnidx);
   )

   /*Checking if the insertion must be made into the original code or in an already displaced
    * area of code*/
   if (insn_ismoved(moved) == FALSE) {
      /*Moving the basic block to another section*/
      int out = patchfile_moveblock(pf,lstmoved,scnidx, movmod->flags,movmod->paddinginsn);
      if (ISERROR(out))
      return out; //Block could not be moved
   }
   DBGLVL(2,DBGMSG0("Patch list is now:\n");patchfile_dumppatchlist(pf,stderr);)
   return EXIT_SUCCESS;
}
#endif
#if 0
static elfscnbounds_t* asmfile_getscnbounds(asmfile_t* af, int *n_scn, int** codescns) {
   /**\todo This is a quite dirty code, as it does not perform any check on the coherence between the
    * instructions list and the sections (we assume they have all been disassembled and are in the correct order)
    * It will be removed during patcher re-factoring */
   /**\todo Change! We will be needing those boundaries for the inserted object files used in static patching.
    * So we will be reusing this function for that purpose, unless a better solution is found*/

   list_t* scnstart;
    elffile_t* efile = asmfile_get_origin(af);
   int* codescn_idx = NULL;
   int i, nscn = 0;
   elfscnbounds_t* codescn_bounds = NULL;

   //Retrieving sections containing code and not empty (yes, it can happen. Check the .text section of stdio.o in libc.a and weep)
   for (i = 0; i < elffile_getnscn(efile); i++) {
      if ( (elffile_scnisprog(efile, i) == TRUE) && (elffile_getscnsize(efile, i)) ) {
         codescn_idx = (int*) lc_realloc(codescn_idx, sizeof(int) * (nscn + 1));
         codescn_idx[nscn] = i;
            DBGMSGLVL(1,"ELF section %d in file %s contains code\n",codescn_idx[nscn],asmfile_get_name(af));
         nscn++;
      }
   }
   if (nscn > 0) { //Just in case the file does not contain actually any nonempty code section. The stdio.o example strikes again
      codescn_bounds = lc_malloc(sizeof(elfscnbounds_t) * (nscn));
        scnstart = queue_iterator(asmfile_get_insns(af));
      for ( i = 0; i < (nscn-1); i++) {
         int64_t scnlen = 0;
         codescn_bounds[i].begin = scnstart;
         while ( ( scnstart != NULL) && ( scnlen != elffile_getscnsize(efile, codescn_idx[i]) ) ) {
               scnlen += insn_get_size(GET_DATA_T(insn_t*, scnstart)) / 8;
            scnstart = scnstart->next;
         }
         if (scnstart != NULL)
         codescn_bounds[i].end = scnstart->prev;
         else //this case happens only with the last section
                codescn_bounds[i].end = queue_iterator_rev(asmfile_get_insns(af));
         DBGMSGLVL(1,"Code ELF section %d has boundaries %#"PRIx64" and %#"PRIx64"\n",
               i, INSN_GET_ADDR(GET_DATA_T(insn_t*, codescn_bounds[i].begin)), INSN_GET_ADDR(GET_DATA_T(insn_t*, codescn_bounds[i].end)));
      }
      /**\todo Either get rid of this last part (and let the previous loop run for all sections), or remove the test on scnstart in the loop*/
      /*Last section*/
      codescn_bounds[i].begin = scnstart;
      while ( scnstart != NULL)
      scnstart = scnstart->next;
        codescn_bounds[i].end = queue_iterator_rev(asmfile_get_insns(af));

      DBGMSGLVL(1,"Code ELF section %d has boundaries %#"PRIx64" and %#"PRIx64"\n",
           i, INSN_GET_ADDR(GET_DATA_T(insn_t*, codescn_bounds[i].begin)), INSN_GET_ADDR(GET_DATA_T(insn_t*, codescn_bounds[i].end)));
   }

   if (n_scn)
   *n_scn = nscn;
   if (codescns)
   *codescns = codescn_idx;
   else if (codescn_idx != NULL)
   lc_free(codescn_idx); //Avoiding leaks

   return codescn_bounds;
}
#endif

#if 0 // Moved to patchfile_finalise, and I also want to compile this blasted thing for once (2014-11-18)
/**
 * Finalizes the changes made to an executable file but does not saves the results
 * \param pf A pointer to the structure holding the disassembled file
 * \return EXIT_SUCCESS or error code
 * */
static int patchfile_patch_finalize(patchfile_t* pf) {
   /** \todo The separation of patchedfile_patch_finalize and patchedfile_patch_write is only for the purpose of label insertions, and probably some
    * further updates. It should become useless if we move most of the code for patching from libmadras.c to this file. => So now it's done, is it really ?*/
   int out = EXIT_SUCCESS;
   int* reordertbl,i,scnmax,codescn = -1,datascn = -1,tdatascn = -1,tbssscn = -1,safetylen=0,pltscn = -1;
   /**\todo TODO REFACTOR (2014-10-17) At some point here, between the invocation of binfile_patch_finalise (which reorders)
    * and binfile_patch_write (which writes it), we have to update all structures, including labels, whose addresses have changed (based on targets)
    * This is necessary for creating the binary file to write.*/

   uint64_t newcodlen=0,altcodlen,pltlen = 0;
//	elffile_t **objfiles = NULL;
   int64_t *objs_newaddrs = NULL;
   int newstack = ((pf->flags&PATCHFLAG_NEWSTACK)?1:0);

   if ( queue_length(pf->insertedobjs) > 0 ) {/**\todo Propagate the value of queue_length(pf->insertedobjs)*/
      int e = 0;
      //Checks that no symbols are undefined
      out = patchfile_resolve_objssyms(pf);
      /*Generates an array of the parsed ELF files to be added to the file*/
      pf->objfiles = (elffile_t**)lc_malloc0(sizeof(elffile_t*)*queue_length(pf->insertedobjs));
      objs_newaddrs = (int64_t*)lc_malloc0(sizeof(int64_t)*queue_length(pf->insertedobjs));
      FOREACH_INQUEUE(pf->insertedobjs,itere) {
         pf->objfiles[e] = asmfile_get_origin(GET_DATA_T(asmfile_t*, itere));
         e++;
      }
   }
   /*Updates sizes in the dynamic section*/
   elfexe_dyntbl_updsz(pf->efile);
   /*If some code has been inserted: we retrieve its size*/
   if(pf->patch_list != NULL) {/*Inserted instructions are stored in the last list, associated to elf index -1*/
      int n_branch;
      /*First of all, removing branch destinations rebounds in the section containing displaced instructions*/
      DBGMSG0("Removing branch rebounds\n");
      n_branch = patchfile_remove_branches_rebound(pf);

      /*Retrieves the length of the inserted instruction list*/
      newcodlen = insnlist_bitsize(pf->patch_list, NULL, NULL)/8;

      /*Creating a safety length for instructions that will change size. Only branches can do that so we will reserve a space equal
       * to the difference between a small and a big jump (if a small jump exist for this architecture), times the number of branches
       * in the patch list*/
      //safetylen = pf->patch_list->length;//For safety, and a dirty hack with changing instructions
      queue_t* smalljmp = pf->patchdriver->generate_insnlist_smalljmpaddr(NULL,NULL,NULL,NULL);
      if(smalljmp!=0) {
         queue_t* bigjmp = pf->patchdriver->generate_insnlist_jmpaddr(NULL,NULL);
         safetylen = n_branch * (insnlist_bitsize(bigjmp,NULL,NULL) - insnlist_bitsize(smalljmp,NULL,NULL))/8 * 2;
         /**\todo (2015-03-16) The *2 multiplier at the end of the safetylen formula is there to increase the safety margin.
          * Actually, it's here because I discovered that, in x86_64, the difference between large unconditional jumps and small
          * unconditional jumps (3) is not the highest that can happen, as the difference between some conditional jumps can reach
          * 4 bytes. The correct way to do this would be to add a new function to the patchdriver that returns the maximal difference in
          * bytes between a small and a large jump, however, 1) I don't want to add a new function to the patchdriver right now and have
          * to propagate it to all other architectures, 2) The correct way to write this function would be to have MINJAG compute
          * the difference and this may take some time, 3) All of this will hopefully become unnecessary once the patcher refactoring
          * is complete, and 4) Multiplying the safetylen by 2 should allow to cover the difference between all types of branches in
          * the current supported architectures.
          * Also, the *2 multiplier is appended after the /8 (instead of simply dividing by 4) because it's clearer that way*/
         queue_free(bigjmp,insn_free);
      }
      queue_free(smalljmp,insn_free);
      /**\todo Find a better way to handle the fact that instructions sizes can change (the best would be to ask the libtroll the address at which the
       * list can be inserted, then recalculate the codings, then invoke elfexe_scnreorder with the final, correct length for the code: we would not
       * need any safety length). */
      DBGMSG("Safety length for patch list set to %d\n",safetylen);
      scnmax = pf->n_codescn_idx-1;
   } else {
      scnmax = pf->n_codescn_idx;
   }
   if (pf->plt_list != NULL) {/*There is a list for inserted function call stubs*//**\todo ELF-specific, x86 specific, to be removed*/
      pltlen = insnlist_bitsize(pf->plt_list, NULL, NULL)/8;
   }
   /*Updates the bytecode of the sections holding code*/
   /*We have to do this a first time in order to refresh the data buffers size for the elf sections
    * whose code total length has been modified*/
   DBGMSG0("Updating sections binary coding (first pass)\n");
   for(i=0;i<scnmax;i++) {
      patchfile_updscncoding(pf,i,FALSE);
   }
   //Gets the size of the added variables in the tdata and tbss
   int tdataVarsSize = 0, tbssVarsSize = 0;
   FOREACH_INQUEUE(pf->insnvars,ittlsvar) {
      insnvar_t* varref = GET_DATA_T(insnvar_t*, ittlsvar);
      if (varref->type == TLS_VAR) {
         if (varref->var.tlsvar->type == INITIALIZED)
         tdataVarsSize += varref->var.tlsvar->size;
         else if (varref->var.tlsvar->type == UNINITIALIZED)
         tbssVarsSize += varref->var.tlsvar->size;
      }
   }
   /*Gets the reordering table of the sections. The inserted code is the first reordered section (not counting section 0)*/
   DBGMSG0("Reordering ELF sections\n");
   reordertbl = elfexe_scnreorder(pf->efile,pf->afile,newcodlen+safetylen,pf->datalen + ((newstack)?NEWSTACKSIZE:0),
         pf->tdatalen,pf->tbsslen,pltlen,
         &codescn,&datascn,&tdatascn,tdataVarsSize,&tbssscn,tbssVarsSize,&pltscn,LABEL_PATCHMOV,pf->objfiles, objs_newaddrs,
         queue_length(pf->insertedobjs),&(pf->plt_list));
   /**\todo The value of the size should to be retrieved from the architecture, it's the size needed to save the system state
    * - To be updated, as the stack can be then used by the called function*/
   /**\todo The value of the size has been chosen to be a multiple of 64, which is the alignment of the following code section,
    * otherwise there was an alignment mismatch in the elf file. This behaviour must be investigated, as
    *  in theory any value could be chosen with elfhandler.c:elfexe_scnreorder being
    * able to find the proper offsets.*/

   /*Error control on elfexe_scnreorder*/
   if (reordertbl == NULL) {
      ERRMSG("Unable to reorder sections in the patched ELF file\n");
      out = elffile_get_last_error_code(pf->efile);
      if (!ISERROR(out))
      out = ERR_BINARY_SECTIONS_NOT_REORDERED;
      return out;
   }
   //Updates index of data section
   pf->datascnidx = datascn;

   /*Now updating the addresses of the inserted files if there are any*/
   FOREACH_INQUEUE(pf->insertedobjs, oiter) {
      if (asmfile_test_analyze(GET_DATA_T(asmfile_t*, oiter), DIS_ANALYZE) == TRUE) {//Checks if the file has been disassembled
         int nscn, *codescns;
         /*Retrieving the boundaries of the sections in the file. We will be needing this because some inserted files can
          * have multiple code sections, possibly not contiguous nor starting at 0*/
         elfscnbounds_t* bounds = asmfile_getscnbounds(GET_DATA_T(asmfile_t*, oiter), &nscn, &codescns);
         for (i = 0; i < nscn; i++) {
            list_t* scni = bounds[i].begin;
            /*Retrieving the new address of the section. It has been updated in the \e libmtroll:insert_addedfiles (see
             * warning about that, as the original structure representing the inserted file has been modified*/
              int64_t newscnaddr = elffile_getscnstartaddr(asmfile_get_origin(GET_DATA_T(asmfile_t*, oiter)), codescns[i]);
            DBGMSG("Updating addresses in section %d of file %s to start at %#"PRIx64"\n",
                  codescns[i], asmfile_get_name(GET_DATA_T(asmfile_t*, oiter)), newscnaddr);
            while((scni)&&(scni != bounds[i].end)) {
                 insn_set_addr(GET_DATA_T(insn_t*, scni), INSN_GET_ADDR(GET_DATA_T(insn_t*, scni)) + newscnaddr);
               scni = scni->next;
            }
         }
         lc_free(bounds);
         lc_free(codescns);
      }
   }

   /**\todo This may not work as there is already a label at this point (_end) so there is overlapping. Label _end has to be moved if it exists*/
   /*Adds a label signaling the beginning of the section*/
   /*First we need to find the first modified section*/
   /*	if ( codescn >= 0 ) firstscn = codescn;
    else if ( datascn >= 0 ) firstscn = datascn;
    else if ( pltscn >= 0 ) firstscn = pltscn;
    if ( firstscn >= 0 )
    elffile_add_symlbl(pf->efile,LABEL_PATCHMOV,elffile_getscnstartaddr(pf->efile, firstscn),0,GELF_ST_INFO(STB_GLOBAL,STT_NOTYPE),reordertbl[firstscn],FALSE);
    */
   /*Now that we have the index of the new data section, we will update the references for all RIP-based instructions*/
   FOREACH_INQUEUE(pf->insnvars,itvar) {
      insnvar_t* varref = GET_DATA_T(insnvar_t*, itvar);
      if (varref->type == GLOBAL_VAR) {
         DBG(
               char buf[256];insn_print(varref->insn, buf, sizeof(buf));
               DBGMSG("Performing linking of instruction %#"PRIx64": %s (%p) to variable at offset %#"PRIx64" in section %d\n",
                           INSN_GET_ADDR(varref->insn),buf,varref->insn,varref->var.gvar->offset,datascn);
         )
         elffile_add_targetscnoffset(pf->efile,varref->insn,datascn,varref->var.gvar->offset,pf->patchdriver->upd_dataref_coding,0,0);
      } else if (varref->type == TLS_VAR) {
         if (varref->var.tlsvar->type == INITIALIZED) {
            elffile_add_targetscnoffset(pf->efile,varref->insn,tdatascn,
                  pf->tdatalen - varref->var.tlsvar->offset - varref->var.tlsvar->size,pf->patchdriver->upd_tlsref_coding,0,0);
            DBGMSG("Added target from initialized tls vars. Offset set to %#"PRIx64" (%#"PRIx64" - %#"PRIx64" - %#"PRIx32")\n",
                  pf->tdatalen - varref->var.tlsvar->offset - varref->var.tlsvar->size, pf->tdatalen, varref->var.tlsvar->offset, varref->var.tlsvar->size);
         } else if (varref->var.tlsvar->type == UNINITIALIZED) {
            elffile_add_targetscnoffset(pf->efile,varref->insn,tbssscn,
                  pf->tbsslen - varref->var.tlsvar->offset - varref->var.tlsvar->size,pf->patchdriver->upd_tlsref_coding,0,0);
            DBGMSG("Added target from uninitialized tls vars. Offset set to %#"PRIx64" (%#"PRIx64" - %#"PRIx64" - %#"PRIx32")\n",
                  pf->tbsslen - varref->var.tlsvar->offset - varref->var.tlsvar->size, pf->tbsslen, varref->var.tlsvar->offset, varref->var.tlsvar->size);
         }
      }
   }
   /*Generating the data section*/
   /*Filling up the data section with zeroes if we are using the new stack*/
   if ( newstack ) {
      pf->data = lc_realloc(pf->data,pf->datalen + NEWSTACKSIZE);
      memset((void*)((size_t)pf->data + pf->datalen), 0, NEWSTACKSIZE);
      pf->datalen += NEWSTACKSIZE;
   }
   /*Updating the data section's coding*/
   if ( pf->datalen >0 ) {
      elffile_upd_progbits(pf->efile,pf->data,pf->datalen,datascn);
   }
   if ( pf->tdatalen >0 ) {
      elffile_upd_progbits(pf->efile,pf->tdata,pf->tdatalen,tdatascn);
   }

   /*Updating the section for alternate PLT*/
   if ( pf->plt_list != NULL ) {
      insnlist_upd_addresses(pf->plt_list, elffile_getscnstartaddr(pf->efile, pltscn), NULL, NULL);
      insnlist_upd_branchaddr(pf->plt_list,upd_assemble_insn,0,pf->asmbldriver, NULL,NULL);
   }

   if(pf->patch_list != NULL)
   pf->codescn_idx[pf->n_codescn_idx-1] = codescn; /*Updates index of the new section*/
   /*Updates all instructions addresses in the file */
   DBGMSG0("Updating instruction addresses\n");
   for(i=0;i<pf->n_codescn_idx-1;i++) {
      DBGMSG("Updating address in section %d\n",i);
        insnlist_upd_addresses(asmfile_get_insns(pf->afile),elffile_getscnstartaddr(pf->efile,pf->codescn_idx[i]),pf->codescn_bounds[i].begin,pf->codescn_bounds[i].end->next);
        DBGLVL(3, FCTNAMEMSG("Section %d is now:\n", i); list_t* dbg_iter = pf->codescn_bounds[i].begin;
           while (dbg_iter != pf->codescn_bounds[i].end->next) { patchinsn_print(GET_DATA_T(insn_t*, dbg_iter), stderr);dbg_iter = dbg_iter->next;}
        )
   }

   if(pf->patch_list != NULL) {
      uint64_t newcodelenbits;
      unsigned int naddrmodifs = 0;
      DBGMSG("Updating addresses in section %d (patched section)\n",i);
      insnlist_upd_addresses(pf->patch_list,elffile_getscnstartaddr(pf->efile,pf->codescn_idx[i]),NULL,NULL);
      /*Recalculate branch offsets for the inserted section. We want to force branch instructions displaced
       * here to update their coding, and we have to do this now as it may change the size of the instruction's list*/
      do {
         /* We have to repeat this process until the size of the section is stabilized, otherwise it could mean that
          * updating a branch led to increasing the size of the instruction, thus shifting the addresses, thus causing
          * other branches to need an update, thus potentially needing them to increase in size.
          * We know we will reach a stable state in the end as this would mean every branch has been increased to its maximum size*/
         newcodelenbits = insnlist_bitsize(pf->patch_list, NULL, NULL);
         DBGMSG("Patched section contains %"PRId64" bits\n",newcodelenbits);
         insnlist_upd_branchaddr(pf->patch_list,upd_assemble_insn,1,pf->asmbldriver, NULL,NULL);
         /**\todo insnlist_upd_branchaddr should be updated to return an information allowing to be sure whether the list size changed
          * This would allow to avoid invoke insnlist_bitsize twice (above and in the while condition)*/
         /**\bug This loop has a risk of looping forever if we have two branch instructions, the update of each causing a change of size for the other.
          * In that case, the sizes and addresses of the list will keep being updated at each pass and cause the need for a new update.
          * One quick (and dirty) way of fixing this would be to stop after an arbitrary number of iterations. Another would be to be able
          * to detect such a cycle and send a signal to the assembler to try assembling using the largest available sizes for branch instructions*/

            /*And we must also recalculate their addresses as they may have changed if we changed an instruction's opcode
          * which would throw off course the branches from the original code pointing to them*/
         naddrmodifs = insnlist_upd_addresses(pf->patch_list,elffile_getscnstartaddr(pf->efile,pf->codescn_idx[pf->n_codescn_idx-1]), NULL, NULL);
         DBGMSG("%u instructions had their address updated\n",naddrmodifs);
      }while( (newcodelenbits!=insnlist_bitsize(pf->patch_list,NULL,NULL)) || (naddrmodifs>0) );
      DBGMSG("Patched section stabilised at %"PRId64" bits\n",insnlist_bitsize(pf->patch_list,NULL,NULL));
        DBGLVL(2,FCTNAMEMSG0("Patch list now is:\n");patchfile_dumppatchlist(pf,stderr);)
   }

   /*Updates all addresses pointers in the ELF file (has to be done after the update in the inserted
    * section, as it can affect RIP addresses)*/
   DBGMSG0("Updating pointers\n");
   elffile_updtargets(pf->efile);
   //Other pass on the patched section as it may have changed some addresses
   if (insnlist_upd_addresses(pf->patch_list,elffile_getscnstartaddr(pf->efile,pf->codescn_idx[i]),NULL,NULL) > 0) {
      DBGMSG0("Update of pointers changed addresses in the patch list: updating addresses and branch offsets\n");
      elffile_updtargets(pf->efile);
      insnlist_upd_branchaddr(pf->patch_list,upd_assemble_insn,1,pf->asmbldriver, NULL,NULL);
       DBGLVL(2,FCTNAMEMSG("Updated patch list is (length %"PRId64" bytes):\n",insnlist_bitsize(pf->patch_list,NULL,NULL)/8);patchfile_dumppatchlist(pf,stderr);)
   }

   /*Updates the bytecode of the sections holding code*/
   /*We can do this only now because the previous operation can also affect bytecodes*/
   DBGMSG0("Updating sections binary coding (second pass)\n");
   for(i=0;i<pf->n_codescn_idx-1;i++) {
      /*Recalculate branch offsets*/
        insnlist_upd_branchaddr(asmfile_get_insns(pf->afile)/*s[i]*/,upd_assemble_insn,0,pf->asmbldriver,pf->codescn_bounds[i].begin,pf->codescn_bounds[i].end->next);
      /*Update sections codings*/
      patchfile_updscncoding(pf,i,FALSE);
        DBGLVL(3, FCTNAMEMSG("Section %d is now:\n", i); list_t* dbg_iter = pf->codescn_bounds[i].begin;
           while (dbg_iter != pf->codescn_bounds[i].end->next) { patchinsn_print(GET_DATA_T(insn_t*, dbg_iter), stderr);dbg_iter = dbg_iter->next;}
        )
   }
   if (pf->patch_list != NULL) {
      /*Now the size has possibly changed, we'll have to top the end of the list with nop instructions*/
      //Resets the new code len
      altcodlen = insnlist_bitsize(pf->patch_list,NULL,NULL)/8;
      /*Retrieves the size of the nop instruction*/
      /**\todo Improve the retrieval of the size of NOP instruction. The size given should be 0 instead of 8 since
       * we don't know the size of NOP for a given architecture*/
        int noplen = insn_get_size(pf->paddinginsn)/8;

      while(altcodlen<(newcodlen+safetylen)) {
         add_insn_to_insnlst(insn_copy(pf->paddinginsn),pf->patch_list);
         altcodlen+=noplen;
      }
   }

   /*Updates coding in the added PLT section*/
   if(pf->plt_list != NULL) {
      unsigned char* pltbin;
      int pltbinlen; /**\todo Useless, as we have already pltlen, but it is not the right type*/
      /**\todo This block of code was lifted from patchfile_updscncoding, and should return back to it with
       * the appropriate options*/
      /*Retrieves binary coding corresponding to the instruction list*/
      pltbin = insnlist_getcoding(pf->plt_list,&pltbinlen,NULL,NULL);
      DBGMSG("Updating inserted PLT section: code size is %d\n",pltbinlen);
        DBGLVL(4,uint64_t i;for(i=0;i<pltlen;i++) fprintf(stderr,"%x ",pltbin[i]);fprintf(stderr,"\n"););
      /*Updates section data with new binary coding*/
      elffile_upd_progbits(pf->efile,pltbin,pltbinlen,pltscn);
      lc_free(pltbin);
   }
    DBGLVL(2,FCTNAMEMSG("Final patch list is (length %"PRId64" bytes):\n",insnlist_bitsize(pf->patch_list,NULL,NULL)/8);patchfile_dumppatchlist(pf,stderr);)

   /*Update sections codings*/
   patchfile_updscncoding(pf,pf->n_codescn_idx-1,TRUE);

   /*Stores the reorder table into the patched file structure*/
   pf->reordertbl = reordertbl;

   if (pf->new_OSABI != -1)
   {
      unsigned char* ident = Elf_Ehdr_get_e_ident (pf->efile, NULL);
      ident[EI_OSABI] = pf->new_OSABI;
      Elf_Ehdr_set_e_ident (pf->efile, NULL, ident);
      //pf->efile->elfheader->e_ident[EI_OSABI] = pf->new_OSABI;
   }
}
#endif

#if 0 // Moved to patchfile_finalise, and I also want to compile this blasted thing for once (2014-11-18)
/*
 *  Saves a patched file to a new file
 * \param pf A pointer to the structure holding the disassembled file
 * \param newfilename The name of the file to save the results to. If set to NULL, the patched will not be created (this allows
 * further updates to be performed before) and patchfile_patch_write will need to be invoked afterward.
 * \return EXIT_SUCCESS / error code
 * */
int patchfile_patch(patchfile_t* pf, char* newfilename) {
   int out = EXIT_SUCCESS;
   int res = EXIT_SUCCESS;
   list_t* iter;
   if(!pf)
   return ERR_PATCH_NOT_INITIALISED;

   //If there are pending modifications
   /*Storing if the global patching session needs a new stack*/
   int newstack = ((pf->flags&PATCHFLAG_NEWSTACK)?TRUE:FALSE);

   if ( queue_length(pf->modifs_lib) > 0 ) {
      //Handling libraries modifications
      inslib_t** inslibs = NULL;
      int nlibs = 0;

      FOREACH_INQUEUE(pf->modifs_lib, liter) {
         modiflib_t* modiflib = GET_DATA_T(modiflib_t*,liter);
         int i;
         //Adding libraries
         switch (modiflib->type) {
            case ADDLIB:
            switch ( modiflib->data.inslib->type ) {
               case STATIC_LIBRARY:
               //Adds all files defined in the library to the queue in the patchfile object
               for (i = 0; i < modiflib->data.inslib->n_files; i++ ) {
                  queue_add_tail( pf->insertedlibs, modiflib->data.inslib->files[i] );
               }
               break;
               case DYNAMIC_LIBRARY:
               //Adds the name of the dynamic library to insert to the array of dynamic libraries
               inslibs = lc_realloc(inslibs, sizeof(inslib_t*)* (nlibs + 1));
               inslibs[nlibs] = modiflib->data.inslib;
               nlibs++;
               break;
            }
            break;
            case RENAMELIB:
                    res = elffile_rename_dynamic_library (asmfile_get_origin(pf->afile), modiflib->data.rename->oldname, modiflib->data.rename->newname);
            UPDATE_ERRORCODE(out, res)
            break;
         }
      }
      if ( nlibs >0 ) {
         //Inserts the external library names
         int res = patchfile_add_extlibs(pf,inslibs,nlibs);
         lc_free(inslibs);
         UPDATE_ERRORCODE(out, res)
      }
   }

   //Inserting the global variables
   FOREACH_INQUEUE(pf->modifs_var,iterv) {
      /**\todo TODO (2014-11-14) Move this into a separate functions that will do the following steps:
       * - Compute the total size of data needed (don't forget to add the newstack if it is here)
       * - Retrieve the address at which we can insert them
       * - Create a temporary queue of globvar to add, ordered by alignment
       * - Extract a list of data to add, inserting new datas if needed to have gaps between variables to obey alignment
       * - This is the data_list queue, whose bytes we will directly inject later into the patched file*/
      modifvar_t* modvar = GET_DATA_T(modifvar_t*,iterv);
      switch(modvar->type) {
         case ADDGLOBVAR:
         newglobvar_apply(pf,modvar->data.newglobvar);
         break;
         case ADDTLSVAR:
         newtlsvar_apply(pf,modvar->data.newtlsvar);
         break;
      }
   }
   //Sorting the queue of modifications by address
   queue_sort(pf->modifs, modif_cmp_qsort);

   //Inserting into the code
   for(iter=queue_iterator(pf->modifs);iter!=NULL;iter=iter->next) {
      modif_t* mod = (modif_t*)iter->data;
      DBGMSG("Processing modification modif_%d\n",MODIF_ID(mod));
      /*Very special case: we have to detect if at least one modification needs to use a new stack while the file does not and propagate
       * this for the whole file afterward so that one will be created*/
      if((!newstack)&&(mod->flags&PATCHFLAG_NEWSTACK))
      newstack = TRUE;

      /*Updating for the modification flags to add the flags for the global session*/
      mod->flags |= pf->flags;
      /**\todo The use of pf->current_flags makes the update of mod->flags unnecessary, but mod->flags may be used in other functions, so it will remain
       * here as long as they have not been removed*/
      /**\todo Remove the passing of flags as parameter everywhere and use pf->current_flags instead*/
      pf->current_flags = mod->flags;
      /*If the modification has no custom padding insn set, we will use the padding instruction set for the whole file*/
      if(mod->paddinginsn == NULL)
      mod->paddinginsn = pf->paddinginsn;
      switch(mod->type) {
         case MODTYPE_INSERT:
         res = insert_process(pf,mod);
         break;
         /*The following cases are not useful in the current version of the code but they are here for
          * compatibility with the future versions*/
         case MODTYPE_REPLACE:
         res = replace_process(pf,mod);
         break;
         case MODTYPE_MODIFY:
         res = insnmodify_process(pf,mod);
         break;
         case MODTYPE_DELETE:
         res = delete_process(pf,mod);
         break;
         case MODTYPE_RELOCATE:
         res = relocate_process(pf,mod);
         break;
      }
      UPDATE_ERRORCODE(out, res)
   }
   if(newstack)
   pf->flags |= PATCHFLAG_NEWSTACK;

   /*Now performing a second pass to apply all insertions that were not applied
    * In the current version, this only applies to MODTYPE_INSERT modifications*/
   FOREACH_INQUEUE(pf->modifs, iter2) {
      modif_t* mod = (modif_t*)iter2->data;
      DBGMSG("Applying modification modif_%d\n",MODIF_ID(mod));
      if ( !(mod->annotate&A_MODIF_PROCESSED) ) {
         WRNMSG("Modification %d was not processed and will not be applied\n",MODIF_ID(mod));
         if (!ISERROR(out))
         out = WRN_PATCH_MODIF_NOT_PROCESSED;
         continue;
      }

      if ( !(mod->annotate&A_MODIF_APPLIED) ) {
         pf->current_flags = mod->flags;
         switch(mod->type) {
            case MODTYPE_INSERT:
            res = insert_apply(pf,mod);
            break;
            case MODTYPE_REPLACE:
            res = replace_apply(pf,mod);
            break;
            case MODTYPE_MODIFY:
            res = insnmodify_apply(pf,mod);
            break;
            case MODTYPE_DELETE:
            res = delete_apply(pf,mod);
            break;
            case MODTYPE_RELOCATE:
            res = relocate_apply(pf,mod);
            break;
         }
         UPDATE_ERRORCODE(out, res)
      }
      /*If the modification has the same padding instruction as the one for the whole file, this means it had no special padding instruction set
       * We reset it to NULL so as to avoid trying to free it twice (the alternate solution would have been to make a copy but it would probably
       * be costier)*/
      if(mod->paddinginsn == pf->paddinginsn)
      mod->paddinginsn = NULL;

   }

   int nb_newlbl = 0;
   FOREACH_INQUEUE(pf->modifs_lbl, iterlb0)
   {
      modiflbl_t* modif = GET_DATA_T(modiflbl_t*, iterlb0);
      switch (modif->type) {
         case NEWLABEL:
         {
            elfsection_t* symscn = pf->efile->sections[pf->efile->indexes[SYM]];
            elffile_upd_strtab (pf->efile, symscn->scnhdr->sh_link, modif->lblname);
            nb_newlbl++;
            break;
            elffile_upd_strtab (pf->efile, symscn->scnhdr->scnhdr32->sh_link, modif->lblname);
         }
         case RENAMELABEL:
         {
            elfsection_t* dynsymscn = pf->efile->sections[pf->efile->indexes[DYNSYM]];
            elffile_upd_strtab (pf->efile, dynsymscn->scnhdr->sh_link, modif->lblname);
            break;
         }
      }
   }
   // Here increase the symbol table size to allocate the space for new symboles
   elffile_allocate_symlbl(pf->efile, nb_newlbl);
   UPDATE_ERRORCODE(out, res)
   //Performing the label modification requests (we have to do them now as the instructions are in their final place)
   FOREACH_INQUEUE(pf->modifs_lbl,iterlb) {
      res = modiflbl_apply(pf, GET_DATA_T(modiflbl_t*,iterlb));
      UPDATE_ERRORCODE(out, res)
   }
   if (newfilename) {
      patchfile_patch_finalize(pf);

      //Writes the patched file
      DBGMSG("Creating new file %s\n",newfilename);
      copy_elf_file_reorder(pf->efile, newfilename, pf->reordertbl);
   } else
   pf->ready2write = TRUE; //Marks the file as ready to be written
} else {
   WRNMSG("Attempted to finalize modifications on disassembled file while no modification was pending\n");
   out=EXIT_FAILURE;
   return out;
}
#endif
/*
 *  Saves a patched file to a new file
 * \param pf A pointer to the structure holding the disassembled file
 * \return EXIT_SUCCESS / error code
 * */
int patchfile_patch_write(patchfile_t* pf)
{
   /**\todo TODO (2015-01-21) Remove filename from the parameters. The target file name is supposed to be stored in the binfile
    * I'm commenting all uses of newfilename on purpose in order to raise compilation warnings and not forget this*/
   if (!pf) {
      ERRMSG("Missing patched file\n");
      return ERR_PATCH_NOT_INITIALISED;
   }
//   if (!newfilename) {
//      ERRMSG("Attempted to write patched file with missing name\n");
//      return ERR_COMMON_FILE_NAME_MISSING;
//   }
//   if (!pf->ready2write) {
//      ERRMSG("Attempted to write patched file not finalised\n");
//      return ERR_PATCH_FILE_NOT_FINALISED;
//   }
   //Writes the patched file
   //DBGMSG("Creating new file %s\n",newfilename);
   //copy_elf_file_reorder(pf->efile, newfilename, pf->reordertbl);
   return binfile_patch_write_file(pf->patchbin);
//   return EXIT_SUCCESS;
}

/*
 * Change the targeted OS of a patched file
 * \param pf Structure holding the details about the file being patched
 * \param OSABI The new value of the targeted OS, defined by macros ELFOSABI_...
 *        in elf.h file
 * \return EXIT_SUCCESS if successful, error code otherwise
 * */
int patchfile_patch_changeOSABI(patchfile_t* pf, char OSABI)
{
   if (pf == NULL)
      return ERR_PATCH_NOT_INITIALISED;
   pf->new_OSABI = OSABI;
   return EXIT_SUCCESS;
}

/*
 * Change the targeted machine of a patched file
 * \param pf Structure holding the details about the file being patched
 * \param machine The new value of the targeted machine, defined by macros EM_...
 *        in elf.h file
 * \return EXIT_SUCCESS if successful, error code otherwise
 * */
int patchfile_patch_changemachine(patchfile_t* pf, int machine)
{
   if (pf == NULL)
      return ERR_PATCH_NOT_INITIALISED;
   /**\todo TODO (2014-11-18) UPDATE THIS*/
   return EXIT_SUCCESS;
//    Elf_Ehdr_set_e_machine (pf->efile, NULL, machine);
   return FALSE;
}

/*
 * Builds a queue of insnaddr_t structures tying an instruction in the file with its address for all instructions in the file.
 * This queue is stored in the insnaddr member of patchfile_t
 * \param pf Pointer to the structure holding the disassembled file
 * \return EXIT_SUCCESS if successful, error code otherwise (including if the queue had already been built)
 * */
int patchfile_trackaddresses(patchfile_t* pf)
{
   if (!pf)
      return ERR_PATCH_NOT_INITIALISED;
   if (pf->insnaddrs)
      return ERR_PATCH_ADDRESS_LIST_ALREADY_CREATED; //Already existing, something went wrong above
   pf->insnaddrs = queue_new();
   //Scans all instruction in the file
   FOREACH_INQUEUE(asmfile_get_insns(pf->afile), iter) {
      insnaddr_t* insnaddr = lc_malloc(sizeof(*insnaddr));
      insnaddr->insn = GET_DATA_T(insn_t*, iter);
      insnaddr->addr = INSN_GET_ADDR(GET_DATA_T(insn_t*, iter));
      queue_add_tail(pf->insnaddrs, insnaddr);
   }
   return EXIT_SUCCESS;
}
/////////////////////////NEW FUNCTIONS FROM PATCHER REFACTORING

/**
 * Creates a data entry for the new stack, if one is required
 * \param pf Structure representing the file being patched
 * \return EXIT_SUCCESS if successful, error code otherwise
 * */
static int patchfile_createnewstack(patchfile_t* pf)
{
   /**\todo TODO (2014-11-12) This will change when I upgrade the creation of global variables*/
   assert(pf);
   if (pf->newstack)
      return EXIT_SUCCESS; //New stack has already been created
   globvar_t* newstack = globvar_new(pf, "madras_stack", VAR_CREATED,
         NEWSTACKSIZE, NULL);
   if (!newstack)
      return EXIT_FAILURE;
   /**\todo TODO (2014-11-12) Align correctly the new stack (may need to look up in the ABI)*/
   modifvars_add(pf, ADDGLOBVAR, newstack);
   pf->newstack = newstack->data;
   return EXIT_SUCCESS;
}

/**
 * Finds the first address where a moved block can be displaced
 * \param pf Patched file
 * \param fixed Set to TRUE if the moved block must have a fixed address
 * \return The first available address where to add a moved block
 * */
int64_t patchfile_findmovedaddr(patchfile_t* pf, int fixed)
{
   /**\todo TODO (2014-09-16) As explained in patchfile_init, that one depends on a number of factors:
    * If fixed is TRUE, we will return the last available address in the file, or the end address of the previous fixed block
    * If fixed is FALSE,
    *  - If there is no previous block: Find a hole large enough (see the heuristics in patchfile_init) and return the first address
    *  - If there is a previous block: check if the hole this block belongs to is large enough to receive a modification. If not,
    *  jump to the next hole, otherwise we will return the end address of the previous block.
    *  For now I'm writing some simplified version */
   int64_t out = 0;
   //Retrieves the queue where this block will be added
   queue_t* blocks = (fixed) ? pf->fix_movedblocks : pf->movedblocks;
   //Updates the begin address of the moved block
   if (queue_length(blocks) > 0) {
      movedblock_t* prev = queue_peek_tail(blocks);
      assert(prev);
      out = prev->newlastaddr; // + insn_get_bytesize(prev->lastinsn); (i think size of basic blocks is calculated so that it is the one after the last insn)
   } else {
      /**\todo TODO (2014-09-16) This will be the answer when fixed is TRUE. For fixed=FALSE, we would add a some safety value after this and return it,
       * or find large enough holes before that in the file.
       * Now we add a completely arbitrary buffer for storing potential addresses used by relative jumps. It would not work of course as we need to check
       * whether this location is reachable from the main code by a RIP address.*/
      out = pf->bindriver->binfile_patch_get_last_load_addr(pf->patchbin)
            + queue_length(asmfile_get_insns(pf->afile)) * pf->addrsize;

   }
   return out;
}

/**
 * Finds an available area to insert an address. This is used for inserting data values used by jumps to displaced code using memory relative operand
 * \param pf Patched file
 * */
int64_t patchfile_findreladdr(patchfile_t* pf)
{
   /**\todo TODO (2014-09-17) This version simply returns the address of the last inserted data representing a jump address, added by its size.
    * The actual version should be checking that the remaining space in whatever hole we are inserting is enough to insert a pointer (pf->patchdriver->addrsize)
    * and that we are overlapping with other insertions*/
   int64_t out = 0;
   if (queue_length(pf->reladdrs) > 0) {
      out = data_get_addr(queue_peek_tail(pf->reladdrs)) + pf->addrsize;
   } else {
      /**\todo TODO (2014-09-17) Like in patchfile_findmovedaddr above, we need here to check if other holes in the original file can't contain some addresses
       * Otherwise this is pretty useless, especially in Intel 64, as RIP and direct branches have the same maximal reach.
       * */
      out = pf->bindriver->binfile_patch_get_last_load_addr(pf->patchbin);
   }
   return out;
}

///**
// * Retrieves the jump instructions for reaching a displaced area
// * \param pf Structure holding the details about the file being patched
// * \param addr Address where the jump will be inserted
// * \param dest Address to reach with the jump
// * \param jmp Return parameter. Will contain a pointer to the actual jump instruction
// * \param dstptr Return parameter. Will contain a pointer to be linked to the destination object if the jump is indirect
// * \return List of instruction allowing to reach the destination from the given address
// * */
//static queue_t* patchfile_getjump(patchfile_t* pf, int64_t addr, int64_t dest, insn_t** jmp, pointer_t** dstptr) {
//   assert(pf && jmp && dstptr);
//   *dstptr = NULL;
////   insn_t* j = NULL;
//   queue_t* out = pf->patchdriver->generate_insnlist_jmpaddr(&addr, dstptr);
////   *jmp = j;
//   return out;
//}

/**
 * Finds the type of jump to use for a given distance between original code and displaced code
 * \param pf Pointer to the structure containing details about the patched file
 * \param insnnode List object containing the instruction
 * \param fixed TRUE if we are inserting code at a fixed address
 * \return Code as defined in jumptypes specifying the type of jump to use
 * */
static jumptype_t patchfile_findjumptype(patchfile_t* pf, list_t* insnnode,
      int fixed)
{
   /**\todo TODO (2015-03-03) I'm rewriting this to take into account the size of available code, so the idea that the address moved blocks
    * is estimated is now obsolete. However, I have still to handle the fixed blocks. Do NOT delete the commented code below as some of it
    * may still be useful someplace else. Or not. Or here. Or whatever.*/
   assert(pf);
   uint8_t out;
   if (pf->availsz_codedirect >= DIRJMP_SAFETY) {
      //The remaining size available for storing displaced code can still contain something
      out = JUMP_DIRECT;
      /**\todo TODO (2015-03-03) Either improve this to have more safeties, or implement a mechanism that would allow a displaced basic block
       * to begin in an area reachable with direct jumps and then use an indirect branch to jump further away mid block*/
   } else if (pf->availsz_datarefs >= pf->addrsize + MEMREL_SAFETY) {
      //The remaining size available for storing data references is enough for a memory address
      out = JUMP_MEMREL;
      /**\todo TODO (2015-03-03) When initialising availsz_datarefs, reserve the size for the displaced sections referenced by instructions*/
   } else {
      //As a fallback, we have to rely on a list of instructions performing a full indirect jump that does not use a memory relative operand
      out = JUMP_INDIRECT;
   }

//   int64_t insnaddr = insn_get_addr(GET_DATA_T(insn_t*, insnnode));
//   // Retrieve the last available address depending on whether we are fixed or not
//   int64_t lastinsnaddr = patchfile_findmovedaddr(pf, fixed);
//   // Also retrieves the last available address for inserting a data containing a relative memory address
//   int64_t lastdataddr = patchfile_findreladdr(pf);
//   int64_t distjmp = lastinsnaddr - insnaddr;
//   int64_t distrel = lastdataddr - insnaddr;
//   /**\todo TODO (2014-09-17) Adjust the safety value depending on how well we keep track of the distance to the displaced code*/
//   if ( ( distjmp > (int64_t)(pf->patchdriver->jmp_maxdistneg + DIRJMP_SAFETY) ) && (distjmp < (int64_t)(pf->patchdriver->jmp_maxdistpos - DIRJMP_SAFETY) ) ) {
//      //Displaced code can be reached with a direct jump
//      out = JUMP_DIRECT;
//   } else if ( ( distrel > (int64_t)(pf->patchdriver->relmem_maxdistneg + MEMREL_SAFETY) ) && (distrel < (int64_t)(pf->patchdriver->relmem_maxdistpos - MEMREL_SAFETY) ) ) {
//      //Addresses for indirect jump using relative memory operands can be accessed from the original code
//      out = JUMP_MEMREL;
//      /**\todo TODO (2014-09-17) We need to create a data object and point the dst pointer_t on it, then add it to reladdrs*/
//   } else {
//      //As a fallback, we have to rely on a list of instructions performing a full indirect jump that does not use a memory relative operand
//      out = JUMP_INDIRECT;
//   }
   return out;
}

/**
 * Retrieves the list of jump instructions depending on the type to use
 * \param pf Pointer to the structure containing details about the patched file
 * \param jumptype Code as defined in jumptypes specifying the type of jump to use
 * \param insnaddr Address of the insertion of the jump
 * \param Return parameter, will contain the pointer to the destination
 * \return List of instructions representing the required jump
 * */
static queue_t* patchfile_getjump(patchfile_t* pf, uint8_t jumptype,
      int64_t insnaddr, pointer_t** dst)
{
   /**\todo TODO (2014-09-15) Ok, here's the deal: I've been stuck on how to write this function correctly for 2 weeks because
    * I can't find a correct heuristic to know where I should be inserting depending on the available space left.
    * So for now we will always be returning the large direct jump (as before the refactor). What we will have to do later:
    * - Compare the distance with the max reach of the direct jump
    * - If below, return a standard direct jump
    * - If above, check that an available space can contain the destination address and be within reach of an indirect operand from addr (and reach dest)
    * - If not found, raise an error
    * - Otherwise, return an indirect jump and create a new data insertion request containing a pointer to dest*/
   assert(pf && dst && (jumptype < JUMP_MAXTYPES));
   queue_t* jmpl = NULL;
   switch (jumptype) {
   case JUMP_DIRECT:
      //Displaced code can be reached with a direct jump
      jmpl = pf->patchdriver->generate_insnlist_jmpaddr(&insnaddr, NULL, dst);
      break;
   case JUMP_MEMREL:
      //Addresses for indirect jump using relative memory operands can be accessed from the original code
      jmpl = pf->patchdriver->generate_insnlist_ripjmpaddr(&insnaddr, NULL,
            dst);
      /**\todo TODO (2014-09-17) We need to create a data object and point the dst pointer_t on it, then add it to reladdrs*/
      break;
   case JUMP_INDIRECT:
      //As a fallback, we have to rely on a list of instructions performing a full indirect jump that does not use a memory relative operand
      jmpl = pf->patchdriver->generate_insnlist_indjmpaddr(&insnaddr, NULL,
            dst);
      break;
   case JUMP_TRAMPOLINE:
      jmpl = pf->patchdriver->generate_insnlist_smalljmpaddr(&insnaddr, NULL,
            dst);
      break;
   }
   return jmpl;
}

/**
 * Retrieves the size of the list of jump instructions depending on the type to use
 * \param pf Pointer to the structure containing details about the patched file
 * \param jumptype Code as defined in jumptypes specifying the type of jump to use
 * \return Size in bytes of the list of instructions representing the required jump
 * */
static uint64_t patchfile_getjumpsize(patchfile_t* pf, uint8_t jumptype)
{
   assert(pf && (jumptype < JUMP_MAXTYPES));
   uint64_t jmpsz = 0;
   switch (jumptype) {
   case JUMP_DIRECT:
      //Displaced code can be reached with a direct jump
      jmpsz = pf->jmpsz;
      break;
   case JUMP_MEMREL:
      //Addresses for indirect jump using relative memory operands can be accessed from the original code
      jmpsz = pf->relmemjmpsz;
      break;
   case JUMP_INDIRECT:
      //As a fallback, we have to rely on a list of instructions performing a full indirect jump that does not use a memory relative operand
      jmpsz = pf->indjmpaddrsz;
      break;
   case JUMP_TRAMPOLINE:
      jmpsz = pf->smalljmpsz;
      break;
   }
   return jmpsz;
}

static uint64_t patchfile_findjumpsize(patchfile_t* pf, list_t* insnnode,
      int fixed)
{
   return patchfile_getjumpsize(pf, patchfile_findjumptype(pf, insnnode, fixed));
}

/**
 * Creates a new moved block structure corresponding to the moving of a block of code
 * \param pf Pointer to the structure containing details about the patched file
 * \param start First instruction of the block
 * \param stop Last instruction of the block
 * \param size Size of the block
 * \param fixed Set to TRUE if the displaced block must be at a fixed address
 * \return New moved block structure. It will also be added to the list of blocks in patchfile_t
 * */
movedblock_t* movedblock_new(patchfile_t* pf, list_t* start, list_t* stop,
      uint64_t size, int fixed, int jumptype)
{
   movedblock_t* mb = lc_malloc0(sizeof(*mb));
   mb->firstinsn = start;
   mb->lastinsn = stop;
//   mb->size = size;// - insn_get_bytesize(GET_DATA_T(insn_t*, start));
   //Retrieves the queue where this block will be added
   queue_t* blocks = (fixed) ? pf->fix_movedblocks : pf->movedblocks;
//   //Updates the begin address of the moved block
//   mb->newfirstaddr = patchfile_findmovedaddr(pf, fixed);
//   //Calculates a tentative last address for the moved block
//   mb->newlastaddr = mb->newfirstaddr + size;
   if (fixed) {
      //Fixed moved block: we need the last address to be precise, so we add the size of the return jump
      mb->newlastaddr += patchfile_getjumpsize(pf, jumptype);
      /**\todo TODO (2015-01-13) Check the size again! We may not need the same jump for returning than for going there. Anyway I'm beginning
       * to wonder if the fixed blocks is really a good idea after all*/
   }
   mb->localdata = queue_new();
   //Adds the moved block to the relevant list of blocks
   queue_add_tail(blocks, mb);
   mb->sequence = queue_iterator_rev(blocks);
   //Stores the type of jump used
   mb->jumptype = jumptype;
   //Updates available size remaining in the block
   mb->availsz = size - patchfile_getjumpsize(pf, jumptype);
   //Marks the elements in the block as moved
   list_t* iter = start;
   do {
      insn_add_annotate(GET_DATA_T(insn_t*, iter), A_PATCHMOV);
      hashtable_insert(pf->movedblocksbyinsns, GET_DATA_T(insn_t*, iter), mb);
      iter = iter->next;
   } while (iter != stop->next);
   //Initialises queue of associated modifs
   mb->modifs = queue_new();
   //Initialises size of displaced block
   mb->newsize = insnlist_bitsize(pf->insn_list, start, stop) >> 3;

   return mb;
}

/**
 * Returns the annotation to set on an instruction depending on the type of the modification
 * \param modiftype Type of the modification
 * \return Annotate to set on the instruction
 * */
static int get_insnannotate_modiftype(modiftype_t modiftype)
{
   switch (modiftype) {
   case MODTYPE_NONE:
   case MODTYPE_INSERT:
      return A_NA; //Instructions targeted by an insertion will only be flagged as A_PATCHMOV if moved
   case MODTYPE_MODIFY:
      return A_PATCHUPD;
      //return A_PATCHMOD;
   case MODTYPE_REPLACE:
   case MODTYPE_DELETE:
      return A_PATCHDEL;
   case MODTYPE_RELOCATE:
      return A_NA;
   default:
      assert(FALSE); //It's bloody static, mate! We should *not* call it with another type!
      return A_NA;   //That's for making Eclipse happy
   }
}
#if NOT_USED_ANYMORE
/**
 * Generates the patchinsns_t structures associated to a patchmodif and appends them to a queue of existing patchinsn_t
 * \param pf The patched file
 * \param patchinsns A queue of patchinsn_t structures
 * \param pm A patchmodif_t structure
 * \param mb The movedblock where the modif takes place
 * */
static void patchfile_patchmodif_append(patchfile_t* pf, queue_t* patchinsns, patchmodif_t* pm, movedblock_t* mb) {
   /**\todo (2015-01-07) The current code creates list_t structures for new instructions twice: first when creating the list of insn_t, then
    * now when creating the corresponding patchinsn_t. I considered using patchinsn_t structures in patchmodif, but I want a 1-1 correspondence
    * between existing instructions and their patched counterparts using a hashtable, which is complicated to keep if we remove some modifications
    * after creating a patchinsn_t because of them. An alternative would be to have either insn_t or patchinsn_t structures in patchmodif_t depending
    * on the type (and possibly state) of the patchmodif, but that's too risky and dirty. So unless we encounter big memory problems with the patcher
    * (or find a better solution, I'm still making this up as I go along), I'd suggest leaving it as it is now.*/
   assert(pf && patchinsns && pm);
   if (pm->position == MODIFPOS_REPLACE) {
      //The modification replaces an instruction: we have to create a patchinsn_t linking the original instruction to its replacement
      //Retrieving the first instruction in the list of new instructions associated to the patchmodif
      insn_t* newinsn = queue_peek_head(pm->newinsns);//queue_peek_head returns NULL if pm->newinsns is NULL (case of a deletion)
      //Creates a patchinsn_t linking the original instruction to the new one
      patchinsn_t* pi = patchfile_createpatchinsn(pf, GET_DATA_T(insn_t*, pm->firstinsnseq), newinsn, mb);
      //Adds the patchinsn_t to the list
      add_patchinsn_to_list(patchinsns, pi);
      //Now adds remaining instructions replacing the instruction (handled like new instructions)
      if (queue_length(pm->newinsns) > 1) {
         list_t* iter = (queue_iterator(pm->newinsns))->next;
         while(iter) {
            add_patchinsn_to_list(patchinsns, patchfile_createpatchinsn(pf, NULL, GET_DATA_T(insn_t*, iter), mb));
            iter = iter->next;
         }
      }
   } else {
      //The modification is appended or prepended to an instruction: the patchinsn_t structures contain a new instruction
      FOREACH_INQUEUE(pm->newinsns, iter) {
         add_patchinsn_to_list(patchinsns, patchfile_createpatchinsn(pf, NULL, GET_DATA_T(insn_t*, iter), mb));
      }
   }
}
#endif
/**
 * Associates an additional data to an interval
 * */
static void interval_adddata(interval_t* interval, void* data)
{
   /**\todo TODO (2015-04-14) UGLY!!!!!! Find something else to associate an interval to a movedblock*/
   assert(interval && data);
   queue_t* intmb = interval_get_data(interval);
   if (!intmb)
      intmb = queue_new();
   queue_add_tail(intmb, data);
   interval_set_data(interval, intmb);
}

/**
 * Links a moved block to an interval (mainly used to factorise code)
 * */
static void movedblock_setspace(movedblock_t* mb, list_t* spacenode)
{
   assert(mb && spacenode);
   mb->spacenode = spacenode;
   mb->newfirstaddr = interval_get_addr(GET_DATA_T(interval_t*, spacenode));
   mb->newlastaddr = interval_get_end_addr(GET_DATA_T(interval_t*, spacenode));
   //Links the interval to the movedblock
   interval_adddata(GET_DATA_T(interval_t*, spacenode), mb);
}

/**
 * Finds an empty space large enough to contain a moved block, and updates the moved block and the list of empty spaces accordingly
 * \param pf File being patched
 * \param mb Moved block to relocate
 * \return EXIT_SUCCESS if the moved block could be successfully moved, error code otherwise
 * */
static int movedblock_findspace(patchfile_t* pf, movedblock_t* mb)
{
   assert(pf && mb);

   //Type of the empty space we will be looking for, depending on the type of the branch used to reach the moved block
   uint8_t estype, usetype;
   if (mb->jumptype == JUMP_DIRECT) {
      estype = INTERVAL_DIRECTBRANCH;
      usetype = INTERVAL_DIRECTBRANCH;
   } else {
      estype = INTERVAL_NOFLAG;
      usetype = INTERVAL_INDIRECTBRANCH;
   }
   DBGLVL(1,
         FCTNAMEMSG0("Finding space for relocating "); movedblock_fprint(mb, stderr); fprintf(stderr, " using %s branch\n",(estype==INTERVAL_DIRECTBRANCH)?"direct":"indirect"));
   //Attempts to find a free empty space large enough to contain the block
   FOREACH_INQUEUE(pf->emptyspaces, iter) {
      interval_t* es = GET_DATA_T(interval_t*, iter);
      if ((interval_get_size(es) >= (mb->maxsize))
            && (patcher_interval_getreserved(es) == estype)
            && (patcher_interval_getused(es) == INTERVAL_NOFLAG)) {
         //Interval can contain the moved block: splitting it and reserving it
         if (mb->maxsize == interval_get_size(es)) {
            //The maximal size of the moved block is the one of the whole interval: we mark it as used
            patcher_interval_setused(es, usetype);
            //Links the interval to the block
            movedblock_setspace(mb, iter);
         } else {
            //The maximal size of the moved block is smaller than the interval: we split it
            interval_t *used = patchfile_splitemptyspace(pf, iter,
                  interval_get_addr(es) + mb->maxsize);
            //Flagging the new interval as being used
            patcher_interval_setused(used, usetype);
            //Links the interval to the block
            movedblock_setspace(mb, iter->prev);
            /**\todo TODO (2015-03-18) As I said for the references, I may simply remove the intervals, but I think I will use them*/
            iter = iter->prev; //We will always break after that, this is simply to avoid duplicating the debug message
         }
         DBGLVL(1,
               FCTNAMEMSG0("The "); movedblock_fprint(mb, stderr); fprintf(stderr, " using %s branch was relocated in interval ",(estype==INTERVAL_DIRECTBRANCH)?"direct":"indirect"); interval_fprint(GET_DATA_T(interval_t*, iter), stderr); fprintf(stderr, "\n");)
         break;
      }
      DBGLVL(2,
            FCTNAMEMSG0("Block can't be relocated to interval "); patcher_interval_fprint(es, stderr); fprintf(stderr, "\n"));
   }
   if (!iter)
      return ERR_PATCH_NO_SPACE_FOUND_FOR_BLOCK;
   return EXIT_SUCCESS;
}

/**
 * Links a global variable to an interval
 * \param gv The variable
 * \param spacenode List node containing the empty space
 * \param addralign Value with which to align the address of the data
 * */
static void globvar_setspace(globvar_t* gv, list_t* spacenode,
      int64_t addralign)
{
   assert(gv && spacenode);
   gv->spacenode = spacenode;
   data_set_addr(gv->data,
         interval_get_addr(GET_DATA_T(interval_t*, spacenode)) + addralign);
   //Links the interval to the variable
   interval_adddata(GET_DATA_T(interval_t*, spacenode), gv);
}

/**
 * Finds an empty space large enough to contain an inserted global variable, and updates the variable and the list of empty spaces accordingly
 * \param pf File being patched
 * \param gv Global variable to insert
 * \param restype Reserved status of the interval (INTERVAL_REFERENCE/INTERVAL_NOFLAG)
 * \return EXIT_SUCCESS if the variable could be successfully inserted, error code otherwise
 * */
static int patchfile_globvar_findspace(patchfile_t* pf, globvar_t* gv,
      unsigned int restype)
{
   assert(pf && gv);
   /**\todo TODO (2015-05-12) This function reuses a lot of code from movedblock_findspace and binfile_patch_move_scn_to_interval.
    * Find how to factorise some of it (I tried creating interval_can_contain_size but it is incomplete for what we need)*/

   DBGMSGLVL(1,
         "Finding space for relocating global variable %s (globvar_%d), %sreferenced from the original code\n",
         gv->name, gv->globvar_id, (restype==INTERVAL_REFERENCE)?"":"not ");

   //Attempts to find a free empty space large enough to contain the variable
   FOREACH_INQUEUE(pf->emptyspaces, iter) {
      interval_t* es = GET_DATA_T(interval_t*, iter);
      //Computes the address researched inside the interval for the variable depending on its alignment
      int64_t addralgn = 0;   //Value to add to the address to align it
      if (gv->align > 0) {
         uint32_t intalign = interval_get_addr(es) % gv->align;
         if (intalign > 0)
            addralgn = gv->align - intalign;
      }
      uint64_t datasz = data_get_size(gv->data) + addralgn;
      if ((interval_get_size(es) >= (datasz))
            && (patcher_interval_getreserved(es) == restype)
            && (patcher_interval_getused(es) == INTERVAL_NOFLAG)) {
         //Interval can contain the variable: splitting it and reserving it
         if (datasz == interval_get_size(es)) {
            //The maximal size of the variable is the one of the whole interval: we mark it as used
            patcher_interval_setused(es, INTERVAL_REFERENCE);
            //Links the interval to the variable
            globvar_setspace(gv, iter, addralgn);
         } else {
            //The maximal size of the variable is smaller than the interval: we split it
            interval_t *used = patchfile_splitemptyspace(pf, iter,
                  interval_get_addr(es) + datasz);
            //Flagging the new interval as being used
            patcher_interval_setused(used, INTERVAL_REFERENCE);
            //Links the interval to the block
            globvar_setspace(gv, iter->prev, addralgn);
            /**\todo TODO (2015-03-18) As I said for the references, I may simply remove the intervals, but I think I will use them*/
            iter = iter->prev; //We will always break after that, this is simply to avoid duplicating the debug message
         }
         DBGLVL(1,
               FCTNAMEMSG("The global variable %s (globvar_%d) was relocated in interval ",gv->name, gv->globvar_id); interval_fprint(GET_DATA_T(interval_t*, iter), stderr); fprintf(stderr, "\n");)
         break;
      }
      DBGLVL(2,
            FCTNAMEMSG0("Variable can't be relocated to interval "); patcher_interval_fprint(es, stderr); fprintf(stderr, "\n"));
   }
   if (!iter)
      return ERR_PATCH_NO_SPACE_FOUND_FOR_GLOBVAR;
   return EXIT_SUCCESS;
}

/**
 * Checks if a modification has a restriction over the update of branches pointing to its original node
 * \param modif The modification
 * \return TRUE if the modification is flagged by either PATCHFLAG_INSERT_NO_UPD_FROMFCT,
 * PATCHFLAG_INSERT_NO_UPD_OUTFCT or PATCHFLAG_INSERT_NO_UPD_FROMLOOP, FALSE otherwise
 * */
static int modif_hasbranchupd_restrictions(modif_t* modif)
{
   assert(modif);
   return
         (modif->flags
               & (PATCHFLAG_INSERT_NO_UPD_FROMFCT
                     | PATCHFLAG_INSERT_NO_UPD_OUTFCT
                     | PATCHFLAG_INSERT_NO_UPD_FROMLOOP)) ? TRUE : FALSE;
}

/**
 * Retrieves all branches pointing to an instruction and add them to a queue
 * \param branches Hashtable of branches indexed by their destination
 * \param originbranches Queue of branch instructions
 * \param insn Instruction for which we are looking the branches pointing to
 * */
static void get_origin_branches(hashtable_t* branches, queue_t* originbranches,
      insn_t* insn)
{
   assert(branches && originbranches && insn);
   //Retrieves branches pointing to instruction
   queue_t* origins = hashtable_lookup_all(branches, insn);
   if (origins) {
      DBGMSGLVL(2,
            "Found %d branch instructions pointing to instruction at address %#"PRIx64" (%p)\n",
            queue_length(origins), insn_get_addr(insn), insn);
      //Appending origins to the list of branches
      queue_append(originbranches, origins);
      //lc_free(origins);
      /**\todo TODO (2014-12-18) Once and for all, decide whether queue_append frees the appended queue or not
       * => (2015-05-29) DECIDED*/
   }
}

/**
 * Checks if both instructions belong to the same loop
 * \param insn1 First insn
 * \param insn2 Second insn
 * \return TRUE if both insns are linked to the same non-null loop_t structure, FALSE otherwise
 * */
static int insns_sameloop(insn_t* insn1, insn_t* insn2)
{
   assert(insn1 && insn2);
   loop_t* loop1 = block_get_loop(insn_get_block(insn1));
   loop_t* loop2 = block_get_loop(insn_get_block(insn2));
   if (!loop1 || !loop2)
      return FALSE; //At least one loop is NULL, so we can't identify if they belong to the same
   return (loop1 == loop2) ? TRUE : FALSE;
}

/**
 * Checks if both instructions belong to the same function
 * \param insn1 First insn
 * \param insn2 Second insn
 * \return TRUE if both insns are linked to the same non-null fct_t structure,
 * or are linked to a null fct_t but share the same fctlabel, FALSE otherwise
 * */
static int insns_samefct(insn_t* insn1, insn_t* insn2)
{
   assert(insn1 && insn2);
   fct_t* fct1 = block_get_fct(insn_get_block(insn1));
   fct_t* fct2 = block_get_fct(insn_get_block(insn2));
   if (!fct1 || !fct2) {
      //At least one function is NULL: we must use the labels to identify functions
      return (insn_get_fctlbl(insn1) == insn_get_fctlbl(insn2)) ? TRUE : FALSE;
   }
   return (fct1 == fct2) ? TRUE : FALSE;
}

/**
 * Appends the code of a single modification associated to an instruction to a moved block. Used to factorise code of movedblock_finalise
 * \param pf The patched file
 * \param mb The moved block
 * \param nextmodifiter Node in the list of modifications associated to this block
 * \param iter Pointer to the node containing the instruction for which we are appending modifications.
 * \param position Position of the modifications with regard to the instruction
 * \return Next node in the list of modifications associated to this block
 * */
static list_t* append_modifcode_toblock(patchfile_t* pf, movedblock_t* mb,
      list_t* nextmodifiter, list_t* iterinsn, uint8_t position)
{
   /**\todo TODO (2015-06-04) Split this function between the various position cases*/
   assert(pf && mb && nextmodifiter && iterinsn);
//   list_t* iter = iterinsn;
   modif_t* nextmodif = (GET_DATA_T(modif_t*, nextmodifiter));
   //patchmodif_t* nextpm = nextmodif->patchmodif;
   DBGMSGLVL(1,
         "Creating patched instructions for modification %d %s instruction at address %#"PRIx64" (%p)\n",
         nextmodif->modif_id,
         (position == MODIFPOS_BEFORE) ?
               "set before" :
               ((position == MODIFPOS_AFTER) ? "set after" : "replacing"),
         nextmodif->addr, GET_DATA_T(insn_t*, nextmodif->modifnode));
//   /**\todo TODO (2015-06-02) WARNING, potential bottleneck! I'm trying to code the updating of branches to an instruction
//    * before which code has been inserted while taking into account the flags specifying how the updates must happen depending
//    * on where the origin branch is with regard to the instruction. Right now I'm struggling with how to code this properly,
//    * so I'm implementing something that will probably cause a lot of loops over the branches to the instructions, especially
//    * if there are a lot of modifs to this instruction. Find something more streamlined later.
//    * -> I'm doing a lot of tests inside a loop, so maybe it would be interesting to swap the tests and the loop*/
//   //Handling branches pointing to an insertion
//   if (position == MODIFPOS_BEFORE) {
//      list_t* iterbranch = queue_iterator(originbranches);
//      //Inserting before: we have to update all branches pointing to this instruction to point to the insertion
//      while(iterbranch) {
//         insn_t* branch = GET_DATA_T(insn_t*, iterbranch);
//         if ( !modif_hasbranchupd_restrictions(nextmodif)
//               || ( (nextmodif->flags & PATCHFLAG_INSERT_NO_UPD_FROMFCT) && (!insns_samefct(branch, GET_DATA_T(insn_t*, iterinsn))) )
//               || ( (nextmodif->flags & PATCHFLAG_INSERT_NO_UPD_OUTFCT) && (insns_samefct(branch, GET_DATA_T(insn_t*, iterinsn))) )
//               || ( (nextmodif->flags & PATCHFLAG_INSERT_NO_UPD_FROMLOOP) && (!insns_sameloop(branch, GET_DATA_T(insn_t*, iterinsn))) ) ) {
//            /*Modification has either no restrictions on the branches,
//             * or branches from the same function must not be updated and the branch belongs to another function,
//             * or branches from others functions must not be updated and the branch belongs to the same function,
//             * or branches from the same loop must not be updated and the branch does not belong to the same loop:
//             * updating branches pointing to the instruction to point to the first added instruction*/
//            patchfile_createpatchbranch(pf, branch, queue_peek_head(nextmodif->newinsns));
//            //Removing the branch from the list of branches to update
//            list_t* nextiterbranch = iterbranch->next;
//            queue_remove_elt(originbranches, iterbranch);
//            iterbranch = nextiterbranch;
//            continue;
//         }
//         iterbranch = iterbranch->next;
//      }
//   }

   //Creating patchinsn_t structures for this modification
   //patchfile_patchmodif_append(pf, mb->patchinsns, nextpm, mb);
   /**\todo (2015-01-07) The current code creates list_t structures for new instructions twice: first when creating the list of insn_t, then
    * now when creating the corresponding patchinsn_t. I considered using patchinsn_t structures in patchmodif, but I want a 1-1 correspondence
    * between existing instructions and their patched counterparts using a hashtable, which is complicated to keep if we remove some modifications
    * after creating a patchinsn_t because of them. An alternative would be to have either insn_t or patchinsn_t structures in patchmodif_t depending
    * on the type (and possibly state) of the patchmodif, but that's too risky and dirty. So unless we encounter big memory problems with the patcher
    * (or find a better solution, I'm still making this up as I go along), I'd suggest leaving it as it is now.*/
   if (position == MODIFPOS_REPLACE) {
      //The modification replaces an instruction: we have to create a patchinsn_t linking the original instruction to its replacement
      //Retrieving the first instruction in the list of new instructions associated to the patchmodif
      insn_t* newinsn = queue_peek_head(nextmodif->newinsns); //queue_peek_head returns NULL if pm->newinsns is NULL (case of a deletion)
      //Creates a patchinsn_t linking the original instruction to the new one
      patchinsn_t* pi = patchfile_createpatchinsn(pf,
            GET_DATA_T(insn_t*, nextmodif->modifnode), newinsn, mb);
      //Adds the patchinsn_t to the list
      add_patchinsn_to_list(mb->patchinsns, pi);
      //Now adds remaining instructions replacing the instruction (handled like new instructions)
      if (queue_length(nextmodif->newinsns) > 1) {
         list_t* iter = (queue_iterator(nextmodif->newinsns))->next;
         while (iter) {
            add_patchinsn_to_list(mb->patchinsns,
                  patchfile_createpatchinsn(pf, NULL, GET_DATA_T(insn_t*, iter),
                        mb));
            iter = iter->next;
         }
      }
      //Skipping to next modification
      nextmodifiter = nextmodifiter->next;
      //Handling the strange case where there would be other modifications tied to the replaced instructions (weird, I know)
      /**\todo TODO (2014-12-22) For now, I'm forbidding this. Check later if it can legitimately happen.
       * If so, I have to recursively invoke this function on it until we exit. I should better split this function into little ones
       * to make it easier. A reason I'm getting so much trouble is that I'd like to anticipate the handling of linked modifications that
       * would force an instruction to have multiple copies (for instance: loop with deleted instructions, and an if/else case without deleted
       * instructions). For now I consider this case to be handled when committing instructions by generating the appropriate list of patchinsn_t*/
      while (nextmodifiter
            && GET_DATA_T(modif_t*, nextmodifiter)->modifnode == iterinsn) {
         modif_t* errnextmodif = GET_DATA_T(modif_t*, nextmodifiter);
         WRNMSG(
               "Modification %d targets instruction at address %#"PRIx64", which is %s by modification %d. Modification %d will be ignored\n",
               errnextmodif->modif_id,
               insn_get_addr(GET_DATA_T(insn_t*, iterinsn)),
               (nextmodif->type == MODTYPE_DELETE) ? "deleted" : "replaced",
               nextmodif->modif_id, errnextmodif->modif_id);
         //Flags the modification as not to be applied
         errnextmodif->annotate |= A_MODIF_ERROR;
         nextmodifiter = nextmodifiter->next;
      }
      insn_add_annotate(GET_DATA_T(insn_t*, iterinsn),
            get_insnannotate_modiftype(nextmodif->type));
      //get_origin_branches(pf->branches, originbranches, GET_DATA_T(insn_t*, iterinsn));
   } else {
      //The modification is appended or prepended to an instruction: the patchinsn_t structures contain a new instruction
      FOREACH_INQUEUE(nextmodif->newinsns, iter) {
         add_patchinsn_to_list(mb->patchinsns,
               patchfile_createpatchinsn(pf, NULL, GET_DATA_T(insn_t*, iter),
                     mb));
      }
      //Skipping to next modification
      nextmodifiter = nextmodifiter->next;
   }
   //Marks the modification as applied
   nextmodif->annotate |= A_MODIF_APPLIED;

   return nextmodifiter;
}

/**
 * Appends the code of modifications associated to an instruction to a moved block. Used to factorise code of movedblock_finalise
 * \param pf The patched file
 * \param mb The moved block
 * \param nextmodifiter Node in the list of modifications associated to this block
 * \param iter Pointer to the node containing the instruction for which we are appending modifications.
 * \param position Position of the modifications with regard to the instruction
 * \return First node in the list of modifications associated to this block that is not associated to the instruction contained in \c iter,
 * or that is associated but with a different position
 * */
static list_t* append_modifscode_toblock(patchfile_t* pf, movedblock_t* mb,
      list_t* nextmodifiter, list_t* iterinsn, uint8_t position)
{
   assert(pf && mb && iterinsn);
//   list_t* iter = *iterinsn;
   while (nextmodifiter
         && (iterinsn == (GET_DATA_T(modif_t*, nextmodifiter))->modifnode)
         && ((GET_DATA_T(modif_t*, nextmodifiter))->position == position)) {
      nextmodifiter = append_modifcode_toblock(pf, mb, nextmodifiter, iterinsn,
            position);
   }
//   *iterinsn = iter;
   return nextmodifiter;
}

/**
 * Generates the patchedinsn_t structures for a moved block
 * \param pf The patched file
 * \param mb The moved block
 * \return EXIT_SUCCESS if everything went successfully, error code otherwise
 * */
static int movedblock_finalise(patchfile_t* pf, movedblock_t* mb,
      queue_t* originbranches, queue_t* references)
{
   /**\todo TODO (2014-12-22) Try to find how to factorise this code (especially the two loops over modifications before and after)*/
   assert(pf && mb && mb->firstinsn && mb->lastinsn && originbranches);
   mb->patchinsns = queue_new();
   list_t* iter = mb->firstinsn;
   list_t* nextmodifiter = queue_iterator(mb->modifs);
   DBGLVL(1,
         FCTNAMEMSG0("Finalising "); movedblock_fprint(mb, stderr); STDMSG("\n"));
   do {
      /**\todo TODO (2015-03-04) Maybe factorise the code retrieving references to instructions
       * => DONE (2015-05-06) => (2015-06-02) NOPE, I said REFERENCES, not branches. Moron*/
      //Retrieves branches pointing to the current instruction
      //get_origin_branches(pf->branches, originbranches, GET_DATA_T(insn_t*, iter));
      queue_t* origins = hashtable_lookup_all(pf->branches,
            GET_DATA_T(insn_t*, iter));
      DBGLVL(2,
            if (origins) FCTNAMEMSG("Found %d branch instructions pointing to instruction at address %#"PRIx64" (%p)\n", queue_length(origins), insn_get_addr(GET_DATA_T(insn_t*, iter)), GET_DATA_T(insn_t*, iter)));
      //Appending origins to the list of branches
      queue_append(originbranches, origins);
      //Retrieves data entries referencing the current instruction in the basic block
      queue_t* refs = hashtable_lookup_all(asmfile_get_insn_ptrs_by_target_data(pf->afile),
            GET_DATA_T(insn_t*, iter));
      if (refs) {
         //Appending refs to the list of data entries
         queue_append(references, refs);
         //lc_free(refs);
         /**\todo TODO (2014-12-18) Once and for all, decide whether queue_append frees the appended queue or not
          * => (2015-05-29) DECIDED*/
      }
      //Appending all modifications set before the current instruction
      nextmodifiter = append_modifscode_toblock(pf, mb, nextmodifiter, iter,
            MODIFPOS_BEFORE);
      //Appending the instruction itself or what replaces it
      if (nextmodifiter
            && (iter == (GET_DATA_T(modif_t*, nextmodifiter))->modifnode)
            && ((GET_DATA_T(modif_t*, nextmodifiter))->position
                  == MODIFPOS_REPLACE)) {
         //There is a modification that replaces the instruction: appending it
         nextmodifiter = append_modifscode_toblock(pf, mb, nextmodifiter, iter,
               MODIFPOS_REPLACE);
      } else {
         //The instruction is not replaced
         /*First detecting if the current instruction had associated modifications not associated to the block
          * This can happen if the modifications did not require the instruction to be moved yet were moved because
          * belonging to a block moved by another modification*/
         queue_t* othermodifs = hashtable_lookup_all(pf->insnreplacemodifs,
               GET_DATA_T(insn_t*, iter));
         int hasmodif = FALSE;
         if (othermodifs) {
            //There are other modifications associated to the instruction
            FOREACH_INQUEUE(othermodifs, iterother) {
               modif_t* other = GET_DATA_T(modif_t*, iterother);
               if (other->movedblock == NULL) {
                  //The modification has not been associated to the current block
                  assert(
                        other->position == MODIFPOS_REPLACE
                              && other->size == 0); //Just checking
                  append_modifcode_toblock(pf, mb, iterother, iter,
                        MODIFPOS_REPLACE);
                  hasmodif = TRUE; //Flags that the instruction is associated to at least one modification not associated to the block
               }
            }
            queue_free(othermodifs, NULL);
         }
         if (!hasmodif) {
            //The instructions has not been targeted by a modification not associated to the block: creating a patchinsn_t for it
            DBGMSGLVL(2,
                  "Creating patched instructions for moved instruction at address %#"PRIx64" (%p)\n",
                  insn_get_addr(GET_DATA_T(insn_t*, iter)),
                  GET_DATA_T(insn_t*, iter));
            add_patchinsn_to_list(mb->patchinsns,
                  patchfile_createpatchinsn(pf, GET_DATA_T(insn_t*, iter),
                        GET_DATA_T(insn_t*, iter), mb));
         }
         if (nextmodifiter
               && (iter == (GET_DATA_T(modif_t*, nextmodifiter))->modifnode)
               && ((GET_DATA_T(modif_t*, nextmodifiter))->position
                     == MODIFPOS_KEEP)) {
            //This was a blank modification: skipping to next modification
            nextmodifiter = nextmodifiter->next;
         }
      }
      //Appending all modifications set after the current instruction
      nextmodifiter = append_modifscode_toblock(pf, mb, nextmodifiter, iter,
            MODIFPOS_AFTER);
      //Next instruction
      iter = iter->next;
   } while (iter && (iter != mb->lastinsn->next));
   assert(!nextmodifiter); //Just checking, we should have completed all modifications by then

   return EXIT_SUCCESS;
}

/**
 * Computes the size of a moved block
 * \param mb The moved block
 * \param pf The patched file
 * */
static void movedblock_computesize(patchfile_t* pf, movedblock_t* mb)
{
   /*Computing the maximal size of the block based on the instructions it contains. It is defined as :
    * - The sum of the sizes of all instructions
    *   - For instructions with a reference operand (relative or memory relative), we use their size max (retrieved from the driver)
    * - Additional maximum size of an unconditional branch (for the return branch)
    * */
   /**\todo TODO (2015-03-17) Try to do this in the pass above to limit the number of passes on the instructions. Also, maybe remove
    * the invocation to the driver function*/
   DBGLVL(1,
         FCTNAMEMSG0("Computing maximal size of "); movedblock_fprint(mb, stderr); STDMSG("\n");)

   mb->maxsize = 0; //Should already be the case because of the lc_malloc0 during initialising, but at least it's clear that way
   FOREACH_INQUEUE(mb->patchinsns, iterpi) {
      patchinsn_t* pi = GET_DATA_T(patchinsn_t*, iterpi);
      if (pi->patched && insn_get_opcode_code(pi->patched) == BAD_INSN_CODE) {
         mb->maxsize += insn_get_bytesize(pi->origin); //Instruction is supposed to be identical to the original
         DBGMSGLVL(3,
               "Max size of block increased by %d bytes (size of instruction at original address %#"PRIx64")\n",
               insn_get_bytesize(pi->origin), insn_get_addr(pi->origin));
      } else {
         //Patched instruction has been fully copied: computing its maximal size
         mb->maxsize += pf->patchdriver->movedinsn_getmaxbytesize(pi->patched);
         DBGLVL(3,
               FCTNAMEMSG("Max size of block increased by %d bytes (maximal size of instruction ", insn_get_bytesize(pi->patched)); insn_fprint(pi->patched, stderr); STDMSG(")\n"));
      }
   }
   //Adding the size of an unconditional branch
   mb->maxsize += (mb->jumptype == JUMP_DIRECT) ? pf->jmpsz : pf->relmemjmpsz;
   /**\todo TODO (2015-03-17) Handle the case where the return jump would not be the same as the jump to reach this block*/
   DBGLVL(2,
         FCTNAMEMSG0("Max size of the "); movedblock_fprint(mb, stderr); STDMSG(" set to %"PRIu64" bytes after computing the instructions\n", mb->maxsize);)

   /**\todo TODO (2015-03-17) Align the variables that must be present in the block*/
   //Now adding the size of the global variables that need to be added to this block
   if (queue_length(mb->localdata) > 0) {
      /**\note (2015-05-13) If I'm not mistaken, the global variables in the block should still be ordered by decreasing alignment,
       * as they are associated to blocks in a loop over global variables after they have been aligned*/
      //First, adding the size for the alignment of the first variable to be sure it will be aligned (worst case scenario)
      mb->maxsize +=
            GET_DATA_T(globvar_t*, queue_iterator(mb->localdata))->align;
      FOREACH_INQUEUE(mb->localdata, itergv)
      {
         globvar_t* gv = GET_DATA_T(globvar_t*, itergv);
         assert(
               gv->align <= GET_DATA_T(globvar_t*, queue_iterator(mb->localdata))->align); //Just checking
         //Adding the size necessary for aligning this variable after the next
         if (gv->align) {
            uint64_t align = (mb->maxsize % gv->align);
            if (align > 0) {
               mb->maxsize += gv->align - align;
               DBGLVL(3,
                     FCTNAMEMSG("Max size of block increased by %#"PRIx64" bytes (alignment of variable %s ", gv->align - align, gv->name); data_fprint(gv->data, stderr); STDMSG(")\n"));
            }
         }
         mb->maxsize += data_get_size(gv->data);
         DBGLVL(3,
               FCTNAMEMSG("Max size of block increased by %#"PRIx64" bytes (size of variable %s ", data_get_size(gv->data), gv->name); data_fprint(gv->data, stderr); STDMSG("\n"));
         /**\todo TODO (2015-03-17) Take into account the alignment of those variables, which could lead to some additional size
          * due to padding.
          * => (2015-05-13) Should be done now. Now to align them properly when creating the block*/
      }
   }
   DBGLVL(2,
         FCTNAMEMSG0("Max size of the "); movedblock_fprint(mb, stderr); STDMSG(" set to %"PRIu64" bytes after adding the local data (%d variables)\n", mb->maxsize, queue_length(mb->localdata));)

}

/**
 * Fuses an empty space with the one immediately following it and remove the following interval
 * \pf File being patched
 * \iter Node containing an empty space in the list of empty spaces
 * */
static void patchfile_fuseemptyspaces(patchfile_t* pf, list_t* iter)
{
   assert(
         pf && iter && iter->next && interval_get_end_addr(GET_DATA_T(interval_t*, iter)) == interval_get_addr(GET_DATA_T(interval_t*, iter->next)));
   /**\todo TODO (2015-06-02) I knew I had something like that already... Now I've created interval_merge because I needed it in
    * patchfile_finalise to merge intervals before affecting elements to them. Get rid of either this function or interval_merge,
    * and actually, rethink the whole interval thing, it's really becoming too complicated and clunky.*/
   DBGLVL(3,
         FCTNAMEMSG0("Fusing interval "); patcher_interval_fprint(GET_DATA_T(interval_t*, iter), stderr); STDMSG(" with interval "); patcher_interval_fprint(GET_DATA_T(interval_t*, iter->next), stderr); STDMSG("\n"));

   //Increases the size of the interval to encompass the next
   interval_upd_end_addr(GET_DATA_T(interval_t*, iter),
         interval_get_end_addr(GET_DATA_T(interval_t*, iter->next)));

   //Associates the moved blocks associated to the next empty space to the current one
   /**\todo TODO (2015-04-14) UGLY!!!!!! Find something else to associate an interval to a movedblock*/
   queue_t* mbs_next = interval_get_data(GET_DATA_T(interval_t*, iter->next));
   if (mbs_next) {
      queue_t* mbs = interval_get_data(GET_DATA_T(interval_t*, iter));
      if (!mbs)
         mbs = queue_new();
      queue_append(mbs, mbs_next);
      //lc_free(mbs_next);
      interval_set_data(GET_DATA_T(interval_t*, iter->next), NULL);
   }

   //Removes the following interval
   patcher_interval_free(queue_remove_elt(pf->emptyspaces, iter->next));
}

/**
 * Updates the coding and addresses of patchinsn_t instruction in movedblocks, and creates return branches from the them
 * \param pf File being patched. \ref movedblock_finalise must have already been invoked
 * \param movedblocks Queue of movedblock_t structures for which we want to create return branches
 * \return Size in bytes of the code for all blocks
 * */
static void patchfile_movedblocks_finalise(patchfile_t* pf,
      queue_t* movedblocks)
{
   /**\todo (2015-01-14) Maybe factorise this because it's becoming really huge. But that counts for a lot of other functions in this wretched file*/
   assert(pf);
   /**\todo TODO (2015-03-04) I'm changing again things, so the return value should not be used and fields in the patchfile_t structure should be filled instead
    * But I'm keeping the return value in case, you know*/
   //uint64_t fullsize = 0;  //Return value
   //Initialises the current address
   int64_t address; // = ((movedblock_t*)queue_peek_head(movedblocks))->newfirstaddr;

   FOREACH_INQUEUE(movedblocks, iter) {
      movedblock_t* mb = GET_DATA_T(movedblock_t*, iter);
      DBGLVL(2,
            FCTNAMEMSG0("Updating coding and address of instructions in "); movedblock_fprint(mb, stderr); STDMSG("\n"));
      //First recompute the block instruction and addresses
      address = patchinsnlist_update(mb->patchinsns, mb->newfirstaddr,
            pf->asmbldriver); //address now contains the address after the last instruction in the list
      mb->newlastaddr = address;
      /*We do not need a return branch if either of this condition is met:
       * - The last instruction in the block is flagged either a return (A_RTRN) or a unconditional jump (A_JUMP && !A_CONDITIONAL)
       * - The block immediately following the current block in the original code has also been moved and immediately follows the current
       * block in the moved section (this is not automatic for fixed moved blocks since they are not reordered by origin address)*/
      DBG(
            FCTNAMEMSG0("Finalising "); movedblock_fprint(mb, stderr); STDMSG(" relocated between addresses %#"PRIx64" and %#"PRIx64"\n", mb->newfirstaddr, mb->newlastaddr);)
      //Retrieves the last instruction from the block
      patchinsn_t* lastpi = queue_peek_tail(mb->patchinsns);
      // First, check if the last instruction in the moved block is either a return or an unconditional jump
      //Retrieves the annotation of the last instruction in the block
      uint32_t lastinsnanno = insn_get_annotate(lastpi->patched);
      if ((lastinsnanno & A_RTRN)
            || (!(lastinsnanno & A_CONDITIONAL) && (lastinsnanno & A_JUMP))) {
         DBGLVL(1,
               FCTNAMEMSG0("The "); movedblock_fprint(mb, stderr); STDMSG(" ends with an unconditional branch: no return branch needed\n");)
         continue; //Last instruction is an unconditional branch: no need to add a return to this block, skipping to the next
      }

      //Now checking if the next moved block is the next block in the original code and the block is not the last of this list of moved blocks
      if (!mb->lastinsn->next /*Last instruction of the moved block is the last in the file*/
            || ((iter->next
                  && (mb->lastinsn->next
                        == GET_DATA_T(movedblock_t*, iter->next)->firstinsn) /*Next moved block directly follows it*/
                  /*&& (mb->spacenode->next == GET_DATA_T(movedblock_t*, iter->next)->spacenode ) And has been affected to the next empty space*/
                  && (interval_get_end_addr(
                        GET_DATA_T(interval_t*, mb->spacenode))
                        == interval_get_addr(
                              GET_DATA_T(interval_t*,
                                    GET_DATA_T(movedblock_t*, iter->next)->spacenode))) /*And had been affected to the next contiguous empty space*/))) {
         //The last original instruction of the current block is either the last or followed by the first original instruction of the next block
         //Fusing the spaces where both blocks have been moved
         patchfile_fuseemptyspaces(pf, mb->spacenode);
         //Linking the next block to the space of the current block
         GET_DATA_T(movedblock_t*, iter->next)->spacenode = mb->spacenode;
         //Updating the next block start and end address
         GET_DATA_T(movedblock_t*, iter->next)->newfirstaddr = address;
         GET_DATA_T(movedblock_t*, iter->next)->newlastaddr =
               interval_get_end_addr(GET_DATA_T(interval_t*, mb->spacenode));
         //Fusing the data of this block with the next one
         if (mb->localdata) {
            //The next block will keep all the local data, so that we know we can always add it in the end
            if (GET_DATA_T(movedblock_t*, iter->next)->localdata) {
               queue_prepend(GET_DATA_T(movedblock_t*, iter->next)->localdata,
                     mb->localdata);
               //lc_free(mb->localdata);
            } else {
               GET_DATA_T(movedblock_t*, iter->next)->localdata = mb->localdata;
            }
            mb->localdata = NULL;
         }
         DBGLVL(1,
               FCTNAMEMSG0("The "); movedblock_fprint(mb, stderr); STDMSG(" and the "); movedblock_fprint(GET_DATA_T(movedblock_t*, iter->next), stderr); STDMSG(" follow each other in the original code: fusing their intervals\n"));
         //Skipping to the next
         continue;
      }

      //Now we're cooking and we know we need to add this return branch
      insn_t* returninsn = GET_DATA_T(insn_t*, mb->lastinsn->next);

      if (insn_check_annotate(returninsn, A_PATCHDEL)) {
         //Special case: the instruction to return to has been deleted, so we need to find the next non deleted instruction
         list_t* iteri = mb->lastinsn->next->next;
         while (iteri
               && insn_check_annotate(GET_DATA_T(insn_t*, iteri), A_PATCHDEL))
            iteri = list_getnext(iteri);
         if (iteri)
            returninsn = GET_DATA_T(insn_t*, iteri);
         else
            ERRMSG(
                  "Unable to find where to return from displaced block originally ending at address %#"PRIx64": all following instructions deleted\n",
                  insn_get_addr(GET_DATA_T(insn_t*, mb->lastinsn->next)));
      }

      int64_t returnaddr = insn_get_addr(returninsn);
      //First retrieving the jump instruction(s) needed depending on the distance with the origin.
      /**\todo (2015-01-12) I'm not using the functions used to find the type of jump to use from original code to moved basic block
       * (patchfile_getjump and co) because they perform additional checks for adding data structures in the case of indirect branches,
       * which we don't need now as we are already in the moved code section and can add whatever we need where we want*/
      queue_t* jmpinsns = NULL;
      pointer_t* ptr = NULL;
      insn_t* jmp = NULL;
      int64_t distjmp = returnaddr - address;
      DBGLVL(1,
            FCTNAMEMSG0("Creating return branch for "); movedblock_fprint(mb, stderr); fprintf(stderr, " from address %#"PRIx64" to address %#"PRIx64" (distance %"PRId64")\n", address, returnaddr, distjmp));
      if ((distjmp > (int64_t) (pf->jmp_maxdistneg + DIRJMP_SAFETY))
            && (distjmp < (int64_t) (pf->jmp_maxdistpos - DIRJMP_SAFETY))) {
         //Original code can be reached with a direct jump
         jmpinsns = pf->patchdriver->generate_insnlist_jmpaddr(&address, &jmp,
               &ptr);
      } else {
         //An indirect jump will be needed to return to the original code
         jmpinsns = pf->patchdriver->generate_insnlist_indjmpaddr(&address,
               &jmp, &ptr);
         /**\todo TODO (2015-01-13) We need to create a data object and point the dst pointer_t on it, then add it to reladdrs*/
         WRNMSG(
               "Indirect branch needed for returning to address %#"PRIx64" while this is not implemented currently\n",
               returnaddr);
      }
      if (!ptr || !jmpinsns) {
         ERRMSG(
               "Unable to create return branches from displaced block to address %#"PRIx64"\n",
               returnaddr);
         continue; /**\todo (2015-01-13) Find some unified thing to do at this point. Maybe raise a critical error*/
      }
      //Create patchinsn_t structures from the returned instruction list
      FOREACH_INQUEUE(jmpinsns, iteri) {
         patchinsn_t* new = patchfile_createpatchinsn(pf, NULL,
               GET_DATA_T(insn_t*, iteri), mb);
         add_patchinsn_to_list(mb->patchinsns, new);
         //address += insn_get_bytesize(new->patched);
      }
      queue_free(jmpinsns, NULL);
      //Now update the pointer to point to the instruction to return to
      if (insn_ispatched(returninsn)) {
         //Instruction has been patched: we have to use the corresponding patched instruction
         patchinsn_t* returnpi = hashtable_lookup(pf->patchedinsns, returninsn);
         assert(returnpi && returnpi->patched); //We skipped deleted instructions, so the patched instruction should always exist. I think
         //pointer_set_insn_target(ptr, returnpi->patched);
         patchfile_setbranch(pf, jmp, returnpi->patched, ptr);
      } else {
         //Instruction was not patched: we can simply link to the original instruction
         //pointer_set_insn_target(ptr, returninsn);
         patchfile_setbranch(pf, jmp, returninsn, ptr);
      }
      mb->newlastaddr = address;
   }

   //Computes the address of the code
   //fullsize = address - ((movedblock_t*)queue_peek_head(movedblocks))->newfirstaddr;

   return;// fullsize;
}

/**
 * Updates all addresses and branches in the moved blocks from a patchfile
 * \param pf Patchfile
 * \param movedblocks Queue of movedblocks
 * */
void patchfile_movedblocks_updateaddresses(patchfile_t* pf,
      queue_t* movedblocks)
{
   /**\todo TODO (2015-01-14) I'm fed up. I need to do something here to detect branch instructions that change size, and therefore
    * cause the offset of other branch instructions to change, with a possible recursivity as updating those would cause other branch instructions
    * to change size. I'm really to fed up now to try to find something that works.
    * => Add some sanity test to detect if we are not doing an infinite loop
    * => (2015-03-18) Major update. Now my blocks are all associated with a given empty space. Those that were contiguous in the original code share the same space.
    * I have to perform all my calculations but keep the blocks aligned on the empty space that contain them (and add some nops to pad when generating the binary code).
    * Then I have to regroup all contiguous empty spaces to request the new code sections to add. So the code below has to be updated to avoid moving a block below
    * the beginning of the emptyspace that contains it*/
   /**\todo TODO (2015-06-04) If we end upd removing the queue of fixed moved blocks, remove movedblocks from the parameters*/
   int hadshift = FALSE;
   //Performs a pass to update all branch targets
   do {
      int64_t shiftaddr = 0; //Stores the shift in address due to instructions changing sizes because of updates
      hadshift = FALSE; //Resets the marker signaling that a shift occurred
      DBGMSG0("Updating branches in all blocks\n")
      //Loop as long as at least one address changed
      FOREACH_INQUEUE(movedblocks, iter2) {
         DBGLVL(1,
               FCTNAMEMSG0("Updating branches in "); movedblock_fprint(GET_DATA_T(movedblock_t*, iter2), stderr);STDMSG("\n"));
         if (iter2->prev
               && GET_DATA_T(movedblock_t*, iter2)->spacenode
                     != GET_DATA_T(movedblock_t*, iter2->prev)->spacenode)
            shiftaddr = 0; //Resets the shift in addresses as we begin a block in a different reserved space
         FOREACH_INQUEUE(GET_DATA_T(movedblock_t*, iter2)->patchinsns, iterpi) {
            if (!GET_DATA_T(patchinsn_t*, iterpi)->patched)
               continue;   //Skipping deleted instructions
            insn_t* insn = GET_DATA_T(patchinsn_t*, iterpi)->patched;
            if (shiftaddr != 0) {
               //Updates the instruction address if the addresses have shift
               insn_set_addr(insn, insn_get_addr(insn) + shiftaddr);
            }
            oprnd_t* refop = insn_lookup_ref_oprnd(insn);
            if (refop) {
               //The patched instruction contains a reference
               //Storing the size of the instruction
               unsigned int oldsz = insn_get_bytesize(insn);
               //Updating the offset to the reference
               //insn_get_arch(insn)->oprnd_updptr(insn, refop);
               insn_oprnd_updptr(insn, refop);
               /**\todo TODO (2015-01-14) Check that the architecture is not NULL, maybe create a function taking the instruction as parameter
                * and performing the retrieval of the refoprnd then its update
                * => (2015-04-14) DONE with the use of insn_oprnd_updptr*/
               //Updates the coding of the instruction
               upd_assemble_insn(insn, pf->asmbldriver, TRUE, NULL);
               //Checks if the instruction changed size
               unsigned int newsz = insn_get_bytesize(insn);
               if (oldsz != newsz) {
                  //The instruction changed size: we have to update the shift in addresses
                  shiftaddr += (int) newsz - (int) oldsz;
                  hadshift = TRUE; //Storing that addresses changed at lest once
                  DBGLVL(2,
                        FCTNAMEMSG("Addresses shift by %"PRId64" due to new size of instruction ", shiftaddr); patcher_insn_fprint_nocr(insn, stderr, A_NA); STDMSG("(%u bytes instead of %d)\n", newsz, oldsz));
               }
            }
         } //End loop on instructions in the moved block
           //Shifting the end address of the block
         DBGLVL(1,
               FCTNAMEMSG0("End address of "); movedblock_fprint(GET_DATA_T(movedblock_t*, iter2), stderr); STDMSG(" will be shift from %#"PRIx64, GET_DATA_T(movedblock_t*, iter2)->newlastaddr));
         GET_DATA_T(movedblock_t*, iter2)->newlastaddr += shiftaddr;
         DBGLVL(1,
               STDMSG(" to %#"PRIx64" (shift is %"PRId64"\n", GET_DATA_T(movedblock_t*, iter2)->newlastaddr, shiftaddr));
         //Sanity check: checking that the moved block does not go beyond the interval that contains it
         assert(
               GET_DATA_T(movedblock_t*, iter2)->newlastaddr <= interval_get_end_addr(GET_DATA_T(interval_t*, GET_DATA_T(movedblock_t*, iter2)->spacenode)));
         /**\todo TODO (2015-03-19) We may want to keep that as an if instead of an assert*/
      } //End loop on blocks
      DBGMSG(
            "Addresses among all blocks have shift by %"PRId64". A new pass is %sneeded.\n",
            shiftaddr, (hadshift) ? "" : "not ");
      //Updates the size of code
      //fullsize += shiftaddr;
   } while (hadshift);

   int64_t address;
   //Now update the addresses of the associated data for all blocks (I need to do this only now when the sizes have stabilised)
   DBGMSG0("Updating addresses of local variables in all blocks\n");
   FOREACH_INQUEUE(movedblocks, iter3)
   {
      movedblock_t* mb = GET_DATA_T(movedblock_t*, iter3);
      address = mb->newlastaddr;
      DBGLVL(1,
            FCTNAMEMSG0("Updating addresses of local variables in "); movedblock_fprint(mb, stderr); STDMSG("\n");)
      FOREACH_INQUEUE(mb->localdata, iter) {
         globvar_t* gv = GET_DATA_T(globvar_t*, iter);
         if (gv->align) {
            int64_t addralign = address % gv->align;
            if (addralign > 0)
               address += gv->align - addralign;
         }
         data_set_addr(gv->data, address);
         DBGLVL(2,
               FCTNAMEMSG("Updated address of local variable %s : # %#"PRIx64" ", gv->name, address); data_fprint(gv->data, stderr); STDMSG("\n");)
         address += data_get_size(gv->data);
      }
      //Sanity check: checking that the moved block with the added instructions does not go beyond the interval that contains it
      assert(
            address <= interval_get_end_addr(GET_DATA_T(interval_t*, mb->spacenode)));
   }
}

/**
 * Marks a block as being used as trampoline by another and performs the necessary references
 * \param pf Pointer to the structure containing details about the patched file
 * \param mb Block using \c tramp as a trampoline
 * \param tramp Block used as trampoline
 * */
static void movedblock_addtrampoline(patchfile_t* pf, movedblock_t* mb,
      movedblock_t* tramp)
{
   assert(pf && tramp && mb);
   DBG(
         FCTNAMEMSG0("Using "); movedblock_fprint(tramp, stderr); STDMSG(" as trampoline for "); movedblock_fprint(mb, stderr); STDMSG("\n"));
   //Marks the block as using tramp as a trampoline
   mb->trampoline = tramp;
   //Adds the block to the list of blocks using tramp as a trampoline
   if (!tramp->trampsites)
      tramp->trampsites = queue_new();
   queue_add_tail(tramp->trampsites, mb);
   //Updates available size in trampoline block
   tramp->availsz = tramp->availsz - patchfile_getjumpsize(pf, mb->jumptype);
}

/**
 * \brief Retrieves a basic block encompassing an address
 *
 * The block:
 * - starts at the first instruction before the given address that is the destination of a branch,
 * or after the first branch instruction found before the given address (or the beginning of the section)
 * - ends before the first instruction after the given address that is the destination of a branch
 * or a branch instruction (or the end of the section)
 * - if the address given corresponds to a branch instruction, the block will be the branch instruction itself and all following NOP instructions
 * (so this is not quite the same as the "official" definition of a basic block as it won't include the last branch instruction)
 * This function also returns an extended basic block
 * This extended basic block the additional instructions:
 * - if the original instruction was a non-branch instruction, the branch instruction (and the possible subsequent NOP instructions) following the original basic block
 * - if the original instruction was a branch, all consecutive branch instructions and the first non-branch instruction encountered, until the minimal size is reached
 * \param pf Pointer to the structure containing the disassembled ELF file
 * \param inseq List object containing the instruction around which we are looking for a basic block
 * \param minsize The minimal size in bits a basic block must have (usually the size needed for the jump instruction
 * that will be inserted in its place)
 * \param len Return parameter, will contain the length in bits of the found block
 * \param start Return parameter, will contain a pointer to the container of the first instruction in the basic block
 * \param stop Return parameter, will contain a pointer to the container of the last instruction in the basic block
 * \return TRUE if a basic block could be found, FALSE otherwise
 * \todo Add a return parameter specifying if this contains a branch instruction, perhaps. Or this should be all done in the calling function.
 * \todo Remove addr as parameter, only use the list_t object containing the instruction (but we need to also pass scndix)
 * \todo Factorise this code. There is now little need to separate the case between branch/non branch for the instruction (and in case you are wondering, yes,
 * there was a time when it was needed because the behaviour was not the same) => Not that easy. We still have some major differences. Factorising into sub
 * functions could help => Partly done with \ref add_previous_to_block and \ref add_nops_to_block
 * */
static uint8_t patchfile_findbasicblock(patchfile_t* pf, list_t* inseq,
      int fixed, uint64_t* len, list_t** start, list_t** stop)
{
   assert(pf && inseq && len && start && stop);

   // Compare this address with the reach of the jump instructions and deduce the jump instruction to use
   uint8_t jumptype = patchfile_findjumptype(pf, inseq, fixed);
   //Retrieves the associated size of the list
   uint64_t minsize = patchfile_getjumpsize(pf, jumptype);

   uint64_t blen = 0;
   list_t* bfirst = NULL, *blast = NULL, *iter;
   binscn_t* scn;
   int move1insn = (pf->current_flags & PATCHFLAG_MOV1INSN) ? TRUE : FALSE;

   int64_t addr = insn_get_addr(INSN_INLIST(inseq));
   DBGMSGLVL(1, "Looking for %s around instruction at address %#"PRIx64"\n",
         (move1insn) ? "one instruction (if possible)" : "one full basic block",
         addr);

   scn = label_get_scn(insn_get_fctlbl(GET_DATA_T(insn_t*, inseq)));
   assert(scn);
//    if (scn) {
//       //we could find the section of the instruction
   bfirst = inseq;
//    } else {
//       //We need to find its section
//       patchfile_getinsnataddr(pf,addr,&scn,&bfirst);
//    }

   //Looks for the instruction at the given address
   if (scn) {/**\todo TODO (2015-05-19) I just asserted that scn is not NULL, so remove this test*/
      /*Adds the instruction's size to the size of the block*/
      blen += insn_get_bytesize(GET_DATA_T(insn_t*, bfirst));
      blast = bfirst;
      /*Steps to next instruction*/
      iter = bfirst->next;
      if (!insn_is_branch(GET_DATA_T(insn_t*, bfirst))) { //Instruction is not a branch
         /*Finds the end of the basic block*/
         while ((iter)/*Instruction exists*/
         /*Instruction is not a branch*/
         && (!insn_is_branch(GET_DATA_T(insn_t*, iter)))
         /*Instruction is not a branch destination*/
         && (!hashtable_lookup(pf->branches, GET_DATA_T(insn_t*, iter)))
               /*Instruction does not correspond to a label (for instance main)*/
               && (!binfile_lookup_label_at_addr(pf->bfile, scn,
                     insn_get_addr(GET_DATA_T(insn_t*, iter))))
               /*Block length has not reached minsize or we are in the mode where we move whole basic blocks*/
               && ((!move1insn) || (blen < minsize))) {
            blen += insn_get_bytesize(GET_DATA_T(insn_t*, iter));
            blast = iter;
            iter = iter->next;
            //Checking that the instruction is not at the end of a list of instructions (interleaved data for example)*/
            if (insn_check_annotate(GET_DATA_T(insn_t*, iter), A_END_LIST))
               break;
         }
         DBGMSGLVL(1, "End of %s found at %#"PRIx64"\n",
               (move1insn) ?
                     "instruction (or small basic block)" : "basic block",
               insn_get_addr(GET_DATA_T(insn_t*,blast)));
         /*Adding following branch instruction (if existing)*/
         if (
         /*Instruction exists*/
         (iter)
         /*Next instruction is a branch*/
         && (insn_is_branch(INSN_INLIST(iter)))
         /*Instruction is not a branch destination*/
         && (!hashtable_lookup(pf->branches, (insn_t*) iter->data))
               /*Instruction does not correspond to a label (for instance main)*/
               && (!binfile_lookup_label_at_addr(pf->bfile, scn,
                     insn_get_addr(GET_DATA_T(insn_t*, iter))))
               /*Block length has not reached minsize or we are in the mode where we move whole basic blocks*/
               && ((!move1insn) || (blen < minsize))) {
            blen = blen + insn_get_bytesize(INSN_INLIST(iter));
            //Adding following nops
            blast = add_nops_to_block(pf, iter, scn, &blen);
         }
         //blast = iter;
         DBGMSGLVL(1,
               "End of %s found at %#"PRIx64" after adding following branch and nop instructions\n",
               (move1insn) ?
                     "instruction (or small basic block)" : "basic block",
               (blast)?insn_get_addr(GET_DATA_T(insn_t*,blast)):ADDRESS_ERROR);
         if ( /*Instruction is not  a branch destination*/
         (!hashtable_lookup(pf->branches, (insn_t*) bfirst->data))
               /**\todo There is a potential bug here, if we encounter an instruction with address -1. A more thorough
                * check would be needed (that the value found is not the instruction)*/
               /*Instruction does not correspond to a label (for instance main)*/
               && (!binfile_lookup_label_at_addr(pf->bfile, scn,
                     insn_get_addr(GET_DATA_T(insn_t*, bfirst))))
               /*Block length has not reached minsize or we are in the mode where we move whole basic blocks*/
               && ((!move1insn) || (blen < minsize))) {
            /*Instruction at the given address is not the beginning of the block*/
            /*Finds the beginning of the block*/
            bfirst = add_previous_to_block(pf, bfirst, scn, &blen, move1insn,
                  minsize);
            DBGMSGLVL(1, "Beginning of %s found at %#"PRIx64"\n",
                  (move1insn) ?
                        "instruction (or small basic block)" : "basic block",
                  insn_get_addr(GET_DATA_T(insn_t*,bfirst)));
         } else {
            DBGMSGLVL(1,
                  "Instruction at address %#"PRIx64" is a branch destination or the block minimal size (%"PRIu64") was reached (block size is %"PRIu64")\n",
                  insn_get_addr(GET_DATA_T(insn_t*,bfirst)), minsize, blen);
         }
      } else {      //Instruction is a branch
         /**\todo As said in the function header, merge this case with the one above => At least the add_xxx_to_block factorised the code a bit*/
         //Adding following nops
         blast = add_nops_to_block(pf, blast, scn, &blen);
         DBGMSGLVL(1,
               "End of %s found at %#"PRIx64" after adding following nop instructions\n",
               (move1insn) ?
                     "instruction (or small basic block)" : "basic block",
               (blast)?insn_get_addr(GET_DATA_T(insn_t*,blast)):ADDRESS_ERROR);
         /*Adding preceding branch instructions and the first non-branch instruction to the basic block*/
         if ( /*The branch is not itself the target of a branch instruction (can happen with RET)*/
         (!hashtable_lookup(pf->branches, (insn_t*) bfirst->data))
               /**\todo There is a potential bug here, if we encounter an instruction with address -1. A more thorough
                * check would be needed (that the value found is not the instruction)*/
               /*Instruction does not correspond to a label (for instance main)*/
               && (!binfile_lookup_label_at_addr(pf->bfile, scn,
                     insn_get_addr(GET_DATA_T(insn_t*, bfirst))))
               /*Block length has not reached minsize or we are in the mode where we move whole basic blocks*/
               && ((!move1insn) || (blen < minsize))) {
            /*In this case the branch is not pointed to by another branch. We will include all branch instructions
             * immediately preceding the branch, then the basic block immediately before that. */
            /**\todo Clean up this code and factorise it */
            /*Initialising iterator*/
            iter = bfirst->prev;
            /*Looking for the branch instructions immediately before that one*/
            while ((iter) && (insn_is_branch((insn_t*) iter->data))
                  && (blen < minsize)
                  && (!hashtable_lookup(pf->branches, (insn_t*) iter->data))
                  && (!binfile_lookup_label_at_addr(pf->bfile, scn,
                        insn_get_addr(GET_DATA_T(insn_t*, iter))))
                  /*Block length has not reached minsize or we are in the mode where we move whole basic blocks*/
                  && ((!move1insn) || (blen < minsize))) {
               blen += insn_get_bytesize((insn_t*) iter->data);
               iter = iter->prev;
            }
            /*At this point we are pointing to the first instruction before the target and that is not a branch*/
            if (iter) {
               bfirst = iter->next;
            }
            /*Finds the beginning of the block*/
            bfirst = add_previous_to_block(pf, bfirst, scn, &blen, move1insn,
                  minsize);
            DBGMSGLVL(1, "Beginning of %s found at %#"PRIx64"\n",
                  (move1insn) ?
                        "instruction (or small basic block)" : "basic block",
                  insn_get_addr(GET_DATA_T(insn_t*,bfirst)));
         } else {
            DBGMSGLVL(1,
                  "Instruction at address %#"PRIx64" is a branch destination or the block minimal size (%"PRIu64") was reached (block size is %"PRIu64")\n",
                  insn_get_addr(GET_DATA_T(insn_t*,bfirst)), minsize, blen);
         }
      }
   } else {
      /*Instruction not found: print an error message*/
      ERRMSG(
            "Unable to find binary section containing instruction at address %#"PRIx64"\n",
            addr);
   }

   *start = bfirst;
   *stop = blast;
   if (bfirst && blast) {
      DBGMSG(
            "Address %#"PRIx64" is in block starting at %#"PRIx64" and ending at %#"PRIx64" with length %#"PRIx64" bytes\n",
            addr, insn_get_addr(INSN_INLIST(bfirst)),
            insn_get_end_addr(INSN_INLIST(blast)), blen);
      *len = blen;
      return jumptype;
   } else {
      *len = 0;
      return JUMP_NONE;/**\todo Handle the error code return values*/
   }
}

/**
 * Looks for a block of a suitable size to host a trampoline jump from a block too small to contain a larger jump by searching backward
 * \param pf Pointer to the structure holding the disassembled file
 * \param origin Pointer to the list object from which we are performing the search
 * \param bdist Pointer to the original distance in bits from the \e origin pointer to the small basic block. Will be updated on exit
 * \param minblocksize Minimal size required for a basic block. If a basic block has to be moved, it must be twice this size
 * \param maxdist Maximal distance acceptable from the origin to the basic block
 * \param fwd If set to 1, looks for blocks located after the origin, otherwise looks for blocks located before
 * \param tblen If not NULL, will contain the size of the found basic block. If the block found is already displaced, will be set to 0
 * \return A pointer to the list object containing the closest instruction belonging to a large enough basic block
 * and that can be used for inserting a trampoline, or NULL if no such block was found
 * \note This function will probably be needed only for x86, where there are multiple sizes of jump instruction
 * \todo Find which is the most useful way of choosing the instruction to return between the closest or farthest instruction, possibly depending
 * on the direction we are searching
 * \todo The check on distances from the original instruction to the trampoline may not be optimal (possibly considering the maximal distance between the
 * origin and the trampoline has been reached when one more instruction could be reached). To be checked
 * */
static movedblock_t* patchfile_findtrampolinebw(patchfile_t *pf, list_t* origin,
      int fixed)
{

   assert(pf && origin);
   movedblock_t* out = NULL;
   uint64_t blen;      //blen is the size in bits of a block
   list_t* iter, *bstartseq, *bstopseq = NULL;
   int64_t originaddr; //Address of the origin (where the replacement should take place)
   binscn_t* scn = label_get_scn(insn_get_fctlbl(GET_DATA_T(insn_t*, origin)));
   DBGMSG(
         "Looking for a trampoline starting at address %#"PRIx64" and proceeding backward\n",
         insn_get_addr(GET_DATA_T(insn_t*, origin)));
   iter = origin->prev;
   originaddr = insn_get_addr(GET_DATA_T(insn_t*, origin));
   while ((iter)
         && (pf->patchdriver->smalljmp_reachaddr(originaddr,
               insn_get_addr(GET_DATA_T(insn_t*, iter))))
         && (iter->next != binscn_patch_get_first_insn_seq(scn))) {
      /*The test on the node is to prevent attempting to create a trampoline in a different section the one we are in, as it could be
       * dangerous if for instance the next or previous section is the plt*/
      DBGMSGLVL(1,
            "Looking for a trampoline around address %#"PRIx64" backward\n",
            insn_get_addr(INSN_INLIST(iter)));
      if (!insn_check_annotate(GET_DATA_T(insn_t*, iter), A_PATCHMOV)) { //This is not an already displaced block
         //Looking for a basic block encompassing this instruction
         int jumptype = patchfile_findbasicblock(pf, iter, FALSE, &blen,
               &bstartseq, &bstopseq);
         //Retrieves the size of the jump used to reach the displaced code of the trampoline block
         uint64_t trampjmpsz = patchfile_findjumpsize(pf, bstartseq, FALSE);
         //Retrieves the size of the jump required to reach the displaced code of the block using a trampoline
         uint64_t jmpsz = patchfile_findjumpsize(pf, bstartseq, fixed);
         /**\todo TODO (2014-09-24) This should actually be calculated using not bstartseq but bstartseq + trampjmpsz. I assume here that the safety
          * areas will be enough to take care of that. Oh, and we also should change the destination address to take into account that we moved
          * the trampoline block*/
         if ((blen >= (trampjmpsz + jmpsz))
               && pf->patchdriver->smalljmp_reachaddr(originaddr,
                     insn_get_addr(GET_DATA_T(insn_t*, bstartseq)) + trampjmpsz)
               && (insn_get_addr(INSN_INLIST(bstopseq)) < originaddr)) {
            /*We have found the instruction inside a big enough block, and the end of the block minus the minimal size is at a distance from the origin
             * smaller than the maximal acceptable distance, and the end of the block is located before the origin point
             * (we just don't want any overlap) */
            DBGMSG(
                  "Trampoline found in block beginning at %#"PRIx64" and ending at %#"PRIx64"\n",
                  insn_get_addr(INSN_INLIST(bstartseq)),
                  insn_get_addr(INSN_INLIST(bstopseq)));
            //Creating the basic block for the moved block
            out = movedblock_new(pf, bstartseq, bstopseq, blen, fixed,
                  jumptype);
            break;             //Exiting the main loop scanning for basic blocks
         } else {             //The basic block is not big enough
            iter = bstartseq; //Positions the cursor at the beginning of the block
            DBGMSGLVL(1,
                  "Block between %#"PRIx64" and %#"PRIx64" inadequate for trampoline: " "size is %s (%"PRIu64" bytes, %"PRIu64" required), " "overlapping with block to move %s (ends at %#"PRIx64", below %#"PRIx64" required), " "distance is %s\n",
                  insn_get_addr(INSN_INLIST(bstartseq)),
                  insn_get_addr(INSN_INLIST(bstopseq)),
                  (blen >= (trampjmpsz + jmpsz) ? "OK" : "NOK"), blen,
                  trampjmpsz + jmpsz,
                  ((insn_get_addr(INSN_INLIST(bstopseq))<originaddr )?"OK":"NOK"),
                  (insn_get_addr(INSN_INLIST(bstopseq))), originaddr,
                  pf->patchdriver->smalljmp_reachaddr(originaddr, insn_get_addr(GET_DATA_T(insn_t*, bstartseq)) + trampjmpsz )?"OK":"NOK");
         }
      } else {          //This instruction belongs to an already displaced block
         //Retrieving the corresponding block
         movedblock_t* mb = hashtable_lookup(pf->movedblocksbyinsns,
               GET_DATA_T(insn_t*, iter));
         //Checks if the block has enough available space for storing the trampoline branch
         if (mb->availsz >= patchfile_findjumpsize(pf, mb->firstinsn, fixed)) {
            out = mb;
            break; //Exiting the main loop scanning for basic blocks
         }
         iter = mb->firstinsn; //Jumps to the first instruction in the block to skip the whole block
      }
      iter = iter->prev;
   }
   return out;
}

/**
 * Looks for a block of a suitable size to host a trampoline jump from a block too small to contain a larger jump by searching forward
 * \param pf Pointer to the structure holding the disassembled file
 * \param origin Pointer to the list object from which we are performing the search
 * \param bdist Pointer to the original distance in bits from the \e origin pointer to the small basic block. Will be updated on exit
 * \param minblocksize Minimal size required for a basic block. If a basic block has to be moved, it must be twice this size
 * \param maxdist Maximal distance acceptable from the origin to the basic block
 * \param fwd If set to 1, looks for blocks located after the origin, otherwise looks for blocks located before
 * \param tblen If not NULL, will contain the size of the found basic block. If the block found is already displaced, will be set to 0
 * \return A pointer to the list object containing the closest instruction belonging to a large enough basic block
 * and that can be used for inserting a trampoline, or NULL if no such block was found
 * \note This function will probably be needed only for x86, where there are multiple sizes of jump instruction
 * \todo Find which is the most useful way of choosing the instruction to return between the closest or farthest instruction, possibly depending
 * on the direction we are searching
 * \todo The check on distances from the original instruction to the trampoline may not be optimal (possibly considering the maximal distance between the
 * origin and the trampoline has been reached when one more instruction could be reached). To be checked
 * */
static movedblock_t* patchfile_findtrampolinefw(patchfile_t *pf, list_t* origin,
      int fixed)
{

   assert(pf && origin);
   movedblock_t* out = NULL;
   uint64_t blen; //blen is the size in bits of a block
   list_t* iter, *bstartseq, *bstopseq = NULL;
   int64_t originaddr; //Address of the origin (where the replacement should take place)
   binscn_t* scn = label_get_scn(insn_get_fctlbl(GET_DATA_T(insn_t*, origin)));
   DBGMSG(
         "Looking for a trampoline starting at address %#"PRIx64" and proceeding forward\n",
         insn_get_addr(GET_DATA_T(insn_t*, origin)));
   iter = origin;
   originaddr = insn_get_addr(GET_DATA_T(insn_t*, origin));
   while ((iter)
         && (pf->patchdriver->smalljmp_reachaddr(originaddr,
               insn_get_addr(GET_DATA_T(insn_t*, iter))))
         && (iter->prev != binscn_patch_get_first_insn_seq(scn))) {
      /*The test on the node is to prevent attempting to create a trampoline in a different section the one we are in, as it could be
       * dangerous if for instance the next or previous section is the plt*/
      DBGMSGLVL(1,
            "Looking for a trampoline around address %#"PRIx64" forward\n",
            insn_get_addr(INSN_INLIST(iter)));
      if (!insn_check_annotate(GET_DATA_T(insn_t*, iter), A_PATCHMOV)) { //This is not an already displaced block
         //Looking for a basic block encompassing this instruction
         int jumptype = patchfile_findbasicblock(pf, iter, FALSE, &blen,
               &bstartseq, &bstopseq);
         //Retrieves the size of the jump used to reach the displaced code of the trampoline block
         uint64_t trampjmpsz = patchfile_findjumpsize(pf, bstartseq, FALSE);
         //Retrieves the size of the jump required to reach the displaced code of the block using a trampoline
         uint64_t jmpsz = patchfile_findjumpsize(pf, bstartseq, fixed);
         /**\todo TODO (2014-09-24) This should actually be calculated using not bstartseq but bstartseq + trampjmpsz. I assume here that the safety
          * areas will be enough to take care of that. Oh, and we also should change the destination address to take into account that we moved
          * the trampoline block*/
         if ((blen >= (trampjmpsz + jmpsz))
               && pf->patchdriver->smalljmp_reachaddr(originaddr,
                     insn_get_addr(GET_DATA_T(insn_t*, bstartseq)) + trampjmpsz)
               && (insn_get_addr(INSN_INLIST(bstartseq)) > originaddr)) {
            /*We have found the instruction inside a big enough block and its beginning plus the minimal block size is at a distance from the origin
             * smaller than the maximal distance and the beginning of the basic block is located after the origin point*/
            DBGMSG(
                  "Trampoline found in block beginning at %#"PRIx64" and ending at %#"PRIx64"\n",
                  insn_get_addr(INSN_INLIST(bstartseq)),
                  insn_get_addr(INSN_INLIST(bstopseq)));
            //Creating the basic block for the moved block
            out = movedblock_new(pf, bstartseq, bstopseq, blen, fixed,
                  jumptype);
            break;             //Exiting the main loop scanning for basic blocks
         } else {             //The basic block is not big enough
            iter = bstopseq;      //Positions the cursor at the end of the block
            DBGMSGLVL(1,
                  "Block between %#"PRIx64" and %#"PRIx64" inadequate for trampoline: " "size is %s (%"PRIu64" bytes, %"PRIu64" required), " "overlapping with block to move %s (begins at %#"PRIx64", above %#"PRIx64" required), " "distance is %s\n",
                  insn_get_addr(INSN_INLIST(bstartseq)),
                  insn_get_addr(INSN_INLIST(bstopseq)),
                  (blen >= (trampjmpsz + jmpsz) ? "OK" : "NOK"), blen,
                  trampjmpsz + jmpsz,
                  ((insn_get_addr(INSN_INLIST(bstartseq))>originaddr )?"OK":"NOK"),
                  (insn_get_addr(INSN_INLIST(bstopseq))), originaddr,
                  pf->patchdriver->smalljmp_reachaddr(originaddr, insn_get_addr(GET_DATA_T(insn_t*, bstartseq)) + trampjmpsz )?"OK":"NOK");
         }
      } else {          //This instruction belongs to an already displaced block
         //Retrieving the corresponding block
         movedblock_t* mb = hashtable_lookup(pf->movedblocksbyinsns,
               GET_DATA_T(insn_t*, iter));
         //Checks if the block has enough available space for storing the trampoline branch
         if (mb->availsz >= patchfile_findjumpsize(pf, mb->firstinsn, fixed)) {
            out = mb;
            break; //Exiting the main loop scanning for basic blocks
         }
         iter = mb->lastinsn; //Jumps to the last instruction in the block to skip the whole block
      }
      iter = iter->next;
   }
   return out;
}

/**
 * Creates a new moved block structure corresponding to the moving of a block of code
 * \param pf Pointer to the structure containing details about the patched file
 * \param modif Modification for which we must create the block
 * \param fixed Set to TRUE if the block must be moved at a fixed address, FALSE otherwise
 * */
movedblock_t* movedblock_create(patchfile_t* pf, modif_t* modif, int fixed)
{
   /**\todo REFONTE On commence par rechercher l'instruction à l'adresse donnée
    * (/!\ MERGE AVEC MASTER pour récupérer un list_t au lieu de addr). => DONE
    * Ensuite, on regarde dans la hashtable des movedblock pour vérifier si cette instruction est déjà associée à un movedblock_t
    * Si oui, on le renvoie. Sinon, appeler patchfile_getbasicblock pour créer le movedblock.
    * S'il renvoie NULL, on renvoie NULL, sinon on l'ajoute dans la hashtable des movedblock_t
    * */
   assert(pf && modif);
   list_t* innode = modif->modifnode;
   movedblock_t* out = NULL;
   insn_t* insn = GET_DATA_T(insn_t*, innode);
   if (insn_check_annotate(insn, A_PATCHMOV)) {
      // The instruction is part of a block that has already been moved: we retrieve the corresponding block and return it
      out = hashtable_lookup(pf->movedblocksbyinsns, insn);
      assert(out);
   } else {
      // We have to create a new block
      /***
       * First, retrieve the last available address (patchfile_findmovedaddr)
       * Then, deduce the jump to use
       * Invoke find basic block using the size of the jump
       *   Basic block found: create a moved block with its size, start and stop instructions (modify patchfile_getbasicblock)
       *   Too small: look up for a trampoline (using A_PATCHMOV for identifying moved instructions)
       * */
      //queue_t* jmpl = NULL;
      //pointer_t* dst = NULL;
      //Now attempts to find a basic block
      list_t* start = NULL, *stop = NULL;
      uint64_t len = 0;
      uint8_t jumptype = patchfile_findbasicblock(pf, innode, fixed, &len,
            &start, &stop);
      if (jumptype != JUMP_NONE) {
         //Found a basic block around the instruction: checking its size
         if (len >= pf->jmpsz) {
            out = movedblock_new(pf, start, stop, len, fixed, jumptype);
//            out->jump = patchfile_getjump(pf, jumptype, insnaddr, &dst);
         } else if (len >= pf->smalljmpsz) {
            //We will try to find a trampoline
            movedblock_t* tramp = patchfile_findtrampolinebw(pf, start, fixed);
            if (!tramp) //No trampoline found backward: searching forward
               tramp = patchfile_findtrampolinefw(pf, stop, fixed);
            if (tramp) { //Trampoline found
               //Create small moved block
               out = movedblock_new(pf, start, stop, len, fixed,
                     patchfile_findjumptype(pf, tramp->firstinsn, fixed));
               //Update trampoline block to reference the new block and vice-versa
               movedblock_addtrampoline(pf, out, tramp);
            } //No trampoline found
         } //Block too small even for small jumps
         /**\todo TODO (2014-09-19) Handle the case of moving functions. Problem: if we do this as soon as we create a modification,
          how will it be possible to set the flag specifying that we allow function lookup for this modif only (would only work if it
          is set for all). Unless we create a modif_init, that we can flag*/
         if (!out) {
            //Still not found basic block: trampolines could not be found or are deactivated, or block is too small
            ERRMSG(
                  "Unable to create a basic block around address %#"PRIx64" - moving functions or neighbouring basic blocks disabled in this version\n",
                  insn_get_addr(insn));
            return out;
            /**\todo TODO (2014-09-25) Handle here moving a whole function if the option is set*/
            /**\todo TODO (2014-09-25) Handle here forcing the insertion by attempting to move the neighbouring basic blocks.*/
         }
         /**\todo (2014-09-25) Idea!: We should distinguish basic blocks whose boundaries are targets of branch instructions, then see
          * if those instructions can be updated to point to a displaced code. In this case, this would mean that we can move these blocks in
          * some circumstances. And also if a block is too small we could extend it by moving a consecutive block with it.
          * Yeah ok it may work in only a handful of cases.*/
      } //else we could not find a basic block at all and we will return NULL
   }
   if (out) {
      //Associates the moved block to the modification
      /**\todo TODO (2014-11-05) These 2 lines might be moved to movedblock_create*/
      modif->movedblock = out;
      queue_add_tail(out->modifs, modif);
      //Updates the size of the moved block
      out->newsize += modif->size; /**\todo TODO (2015-03-03) I may not need this after all*/
      //If the jump is direct, updates the available size remaining for blocks moved from direct branches
      if (out->jumptype == JUMP_DIRECT)
         pf->availsz_codedirect -= modif->size;
   }

   return out;
}

/**
 * Create the patchmodif_t object associated to a modification
 * \param pf Patched file
 * \param modif the modif
 * \return EXIT_SUCCESS if successful, error code otherwise
 * */
static int modif_createpatchmodif(patchfile_t* pf, modif_t* modif)
{
   /**\todo TODO (2014-11-04) I'm trying to be faster now, so I'm reusing old functions, with names that may not be relevant anymore.
    * Also, the calls may change, so, once it's fixed, refactor these to avoid too many calls and sub calls and whatnot*/
   assert(pf && modif);
   int out = ERR_PATCH_WRONG_MODIF_TYPE;
   DBGMSG("Processing modification modif_%d\n", MODIF_ID(modif));
   /**\todo TODO (2014-11-07) Check if the MOVE_STACK is selected either for this modif or for the whole file, and if so create
    * the appropriate data object if it is not*/
   /*Updating for the modification flags to add the flags for the global session*/
   modif->flags |= pf->flags;
   switch (modif->type) {
   case MODTYPE_INSERT:
      out = insert_process(pf, modif);
      break;
      /*The following cases are not useful in the current version of the code but they are here for
       * compatibility with the future versions*/
   case MODTYPE_REPLACE:
      out = replace_process(pf, modif);
      break;
   case MODTYPE_MODIFY:
      out = insnmodify_process(pf, modif);
      break;
   case MODTYPE_DELETE:
      out = delete_process(pf, modif);
      break;
   case MODTYPE_RELOCATE:
      out = relocate_process(pf, modif);
      /**\todo TODO (2014-11-04) See how to adapt my quick and dirty work on master to here
       * => (2015-01-21) */
      break;
   }

   return out;
}

/*
 * Attempts to finalise a modification. This includes trying to find its encompassing block
 * \param pf Structure containing the patched file
 * \param modif The modification
 * \return EXIT_SUCCESS if the modification could be successfully processed, error code otherwise.
 * */
int patchfile_modif_finalise(patchfile_t* pf, modif_t* modif)
{
   if (!pf)
      return ERR_PATCH_NOT_INITIALISED;
   if (!modif)
      return ERR_PATCH_MISSING_MODIF_STRUCTURE;
   if ((modif->annotate & A_MODIF_FINALISED)
         || (modif->annotate & A_MODIF_ATTACHED))
      return ERR_PATCH_MODIF_NOT_FINALISED;

   /**\note (2014-10-28)(2014-10-31) What we will be doing here:
    * - Return an error if we are attempting to finalise a fixed modification while the last fixed modification was not finalised
    * - Builds the patchmodif_t object corresponding to its finalisation
    *   - Check all linked modifications. If they are floating, finalise them (what we do in insert_process)
    *   - Generate the code for the modification (what is done in patch_insert, patch_remove, patch_replace, etc)
    * - Return an error if the patchmodif_t can not be created
    * - Identifies if the modification will require a change of size (different size in lists of patchmodif_t): if so, invoke movedblock_create.
    * - Return an error if movedblock_create was unsuccessful
    * - Add the patchmodif_t to the modif_t
    * - Add the modif_t to the movedblock_t
    * */
   int fixed = (modif->flags & PATCHFLAG_MODIF_FIXED) ? TRUE : FALSE;
   int out = EXIT_SUCCESS;
   if (fixed && (queue_length(pf->fix_movedblocks) > 0)) {
      //Not the first fixed modif: we will be checking that the previous fixed modification has been finalised
      modif_t* previous = queue_peek_tail(
            ((movedblock_t*) queue_peek_tail(pf->fix_movedblocks))->modifs);
      assert(previous);
      if (!(previous->annotate & A_MODIF_FINALISED)) {
         //Previous modification not finalised: finalising it
         WRNMSG(
               "Finalising modification %d: forcing finalisation of previous fixed modification %d\n",
               modif->modif_id, previous->modif_id);
         out = patchfile_modif_finalise(pf, previous);
         if (ISERROR(out)) {
            //Error when finalising previous modification: exiting this finalisation
            ERRMSG(
                  "Unable to finalise previous fixed modification %d: aborting finalisation of modification %d\n",
                  previous->modif_id, modif->modif_id);
            return out;
         }
      }
   }
   //Creates a new stack if this modification or the whole patching session has been set to use it
   if ((modif->flags & PATCHFLAG_NEWSTACK) || (pf->flags & PATCHFLAG_NEWSTACK))
      out = patchfile_createnewstack(pf);
   if (ISERROR(out)) {
      ERRMSG(
            "Unable to create new stack for modification %d at address %#"PRIx64"\n",
            modif->modif_id, modif->addr);
      return out;
   }

   //Creates the instruction lists associated to the modification
   out = modif_createpatchmodif(pf, modif);
   if (ISERROR(out)) {
      ERRMSG(
            "Unable to generate instruction updates for modification %d. Aborting finalisation of modification\n",
            modif->modif_id);
      return out;
   }
   if ((modif->size != 0) || (modif->type == MODTYPE_RELOCATE)) {
      //The modification induces a change in code size or is a forced relocation: we will have to create a movedblock_t
      movedblock_t* mb = movedblock_create(pf, modif, fixed);
      if (!mb) {
         ERRMSG(
               "Unable to find block around address %#"PRIx64" for modification %d. Aborting finalisation of modification\n",
               insn_get_addr(GET_DATA_T(insn_t*, modif->modifnode)),
               modif->modif_id);
         return ERR_PATCH_BASIC_BLOCK_NOT_FOUND;
      }
   }
   //Associates the instruction to the modification
   if (modif->position == MODIFPOS_REPLACE)
      hashtable_insert(pf->insnreplacemodifs,
            GET_DATA_T(insn_t*, modif->modifnode), modif);
   else if (modif->position == MODIFPOS_BEFORE)
      hashtable_insert(pf->insnbeforemodifs,
            GET_DATA_T(insn_t*, modif->modifnode), modif);

   //Flags the modification as having been finalised
   modif->annotate |= A_MODIF_FINALISED;

   return out;
}

patchfile_t* patchfile_init(asmfile_t* af)
{
   patchfile_t* pf = patchfile_new(af);
   if (!pf)
      return NULL;
   //Duplicating the binfile for updates
   pf->patchbin = binfile_patch_init_copy(pf->bfile);
   //Checking if the duplication succeeded and exiting if not
   if (!pf->patchbin) {
      //Error message already printed in binfile_patch_init_copy, see if we add one here
      patchfile_free(pf);
      return NULL;
   }

   /*
    * 1) Calculer la taille de toutes les sections loadées, sans les NOBITS
    * 2) Identifier les intervalles vides contenant au moins 2 fois cette taille
    * 3) Récupérer les portées des jumps directs et indirects et des RIP-based operands
    * 4) Comparer les portées trouvées avec la distance entre les sections loadées et les intervalles trouvés
    * 5) On va se limiter à 2 cas : on arrive à trouver une zone à portée de jump direct du début et de la fin du code original,
    *    ou on trouve une zone à portée de jump indirect du début ou de la fin du code original et une zone plus petite (à voir de combien)
    *    à portée de rip-based du début et de la fin du code original
    *    Sinon, error
    * 6)
    *
    *
    * ===> Trop compliqué. On va dire qu'on s'insère toujours à la fin des sections loadées, et si la distance avec la première est trop grande,
    * on passe en indirect tout de suite. Dans ce cas on cherche une zone à distance de RIP-based operand qui contienne une taille suffisante (insn*8 octets ?)
    *
    *
    *
    * - Pour les ajouts dans des sections nobits (si on décide d'ajouter des vars dans la bss, en somme) :
    *   - Si la section nobits est la dernière et qu'on n'a pas demandé d'ajout à adresse fixe, on ajoute à la fin de la section nobits (! page)
    *   - Sinon, on ajoute une autre section nobits à la fin des sections loadées ajoutées
    * */
   /**\todo (2014-09-04) Maybe move this in separate subfunctions . Some of this may also be moved to the libbin*/
   /* (2015-02-25) Hello, I'm back, and this time I'm writing in English because I feel much more sure of myself
    * Now the steps will be:
    * !!!!!! This whole calculation should probably be moved into an architecture and binary format-specific code
    * - Compute the empty intervals in the file (invoking function from driver, or implicitely invoked by binfile_patch_init
    * - Compute the total size reachable from the original code with references
    *   RefSize = [Lowest code address + max dist ref fw] - [Highest code address - max dist ref back]
    *   /!\ Use an architecture specific function for this, so that we can handle ARM and its strange jumps
    * - Compute the total size reachable from the original code with direct branches
    *   CodeSize = [Lowest code address + max dist jump fw] - [Highest code address - max dist jump back]
    *   /!\ Use an architecture specific function for this, so that we can handle ARM and its strange jumps
    * - Detect if those two sizes overlap or not, for further calculations ==> HOW ???
    * - Check if CodeSize can contain twice the size of the original code => Tweak this value!
    * - Check if RefSize can contain twice the size of the sections referenced by code (except SCNT_DATA) => Tweak this value
    * - /!\ Take into account the overlapping of the addresses reachable with references and with branches
    *   -> Test to do: code <= CodeSize - BothSize and references <= RefSize - BothSize,
    *   OR code - (CodeSize - BothSize) + references - (RefSize - BothSize) <= BothSize.
    *   Wait, what ? -> Of course, stupid. It means that code + references <= CodeSize + RefSize - BothSize.
    *   You know, the A + B = A union B - A inter B thing. Moron.
    * - If both test pass simultaneously, we reserve twice the size of sections referenced from code in space
    *   reachable from references, then begin adding code from the first remaining available address
    * - If at least one test does not pass, we reserve all available size reachable from references, then begin
    *   adding code in the first remaining available address
    *
    * */

   //Calculates the size of code sections
   uint16_t i;

   uint64_t codesz = 0;
   binscn_t** codescns = binfile_get_code_scns(pf->patchbin);
   uint16_t n_codescns = binfile_get_nb_code_scns(pf->patchbin);

   for (i = 0; i < n_codescns; i++) {
      codesz += binscn_get_size(codescns[i]);
   }

   //Computes the size of sections not containing variables or code but referenced by the original code
   uint64_t refssz = 0;
   binscn_t** loadscns = binfile_get_load_scns(pf->patchbin);
   uint16_t n_loadscns = binfile_get_nb_load_scns(pf->patchbin);
   for (i = 0; i < n_loadscns; i++) {
      uint8_t type = binscn_get_type(loadscns[i]);
      if (type != SCNT_CODE && type != SCNT_DATA && type != SCNT_ZERODATA
            && binscn_check_attrs(loadscns[i], SCNA_INSREF))
         refssz += binscn_get_size(loadscns[i]);
   }
   DBGMSG(
         "File contains %"PRIu64" bytes of code and %"PRIu64" bytes of data sections referenced by the code\n",
         codesz, refssz);

   //Computes the empty intervals in the file
   pf->emptyspaces = binfile_get_driver(pf->patchbin)->binfile_build_empty_spaces(
         pf->patchbin);
   /**\todo TODO (2015-02-20) Work in progress - this function needs to be invoked there, but I'm not sure yet how it should work, for instance
    * if it should return a queue of intervals or create them in the binfile. Also, binfile_finalise will probably need it, so we need to find a
    * way to enforce it has been invoked (maybe in binfile_patchinitcopy)*/
   queue_sort(pf->emptyspaces, interval_cmp_addr_qsort);

   DBG(
         FCTNAMEMSG("Empty spaces from file %s are:\n", binfile_get_file_name(pf->patchbin)); FOREACH_INQUEUE(pf->emptyspaces, iteredbg) { fprintf(stderr, "\t"); interval_fprint(GET_DATA_T(interval_t*, iteredbg), stderr); fprintf(stderr, "\n"); })

   /**\todo TODO (2015-02-26) The functions returning the lowest and highest address accessible from
    * the original code should be architecture, and possibly format, specific. I'm just a bit fed up
    * at the moment to write them that way.*/
   int64_t codebegin = binscn_get_addr(binfile_get_code_scn(pf->patchbin, 0));
   int64_t codeend = binscn_get_end_addr(
         binfile_get_code_scn(pf->patchbin, n_codescns - 1));
   /**\todo TODO (2015-04-17) Add a test here to check if there are code sections. It's an extreme case, I know,
    * but we should at least be able to detect it (we could also forbid any patching, but it would be technically
    * possible to have to patch a file that does not contain any code, only data, and add some data or even code to it*/
   //Highest address reachable with a direct branch from the code
   int64_t maxjmpdest = pf->jmp_maxdistpos + codebegin;
   //Lowest address reachable with a direct branch from the code
   int64_t minjmpdest = codeend + pf->jmp_maxdistneg;
   //Highest address reachable with a memory relative from the code
   int64_t maxrefdest = pf->relmem_maxdistpos + codebegin;
   //Lowest address reachable with a memory relative from the code
   int64_t minrefdest = codeend + pf->relmem_maxdistneg;
   /**\todo TODO (2015-02-26) Just in case I was not clear enough, the code above up to the previous TODO
    * should be factorised somewhere and set into architecture specific functions. This would even allow to get
    * rid of the *_m[ai][xn]distpos members and replace all functions using them with architecture-specific ones*/

   /**\todo TODO (2015-02-26) This whole process of computing available sizes and so on could be moved
    * somewhere where it is more relevant once everything is more stabilised. Also, possible refactoring.
    * For instance, merging the flagging of intervals with the computation of available sizes (beware,
    * we need also the size of the overlapping intervals reachable with either branches or references)
    * (2015-03-11) I'm modifying this so that most of the handling of empty spaces is done here in libmpatch.c,
    * but the other comments above still stand.*/

   //Now flag the empty intervals with regard to their reachability from the original code
   patchfile_flagemptyspaces_reachable(pf, minjmpdest, maxjmpdest,
         INTERVAL_DIRECTBRANCH);
   patchfile_flagemptyspaces_reachable(pf, minrefdest, maxrefdest,
         INTERVAL_REFERENCE);

   //Now computes the sizes of those empty intervals
   uint64_t reachable_codesz = 0, reachable_refssz = 0, reachable_bothsz = 0;
   //uint32_t j;
//   uint32_t n_emptyspaces = binfile_get_nb_empty_spaces(pf->patchbin);
//   interval_t** emptyspaces = binfile_get_empty_spaces(pf->patchbin);
   FOREACH_INQUEUE(pf->emptyspaces, iter) {
      int reachcode = FALSE, reachrefs = FALSE;
      uint64_t intervalsz = interval_get_size(GET_DATA_T(interval_t*, iter));
      if (patcher_interval_checkreachable(GET_DATA_T(interval_t*, iter),
            INTERVAL_DIRECTBRANCH)) {
         reachable_codesz += intervalsz;
         reachcode = TRUE;
      }
      if (patcher_interval_checkreachable(GET_DATA_T(interval_t*, iter),
            INTERVAL_REFERENCE)) {
         reachable_refssz += intervalsz;
         reachrefs = TRUE;
      }
      if (reachcode && reachrefs)
         reachable_bothsz += intervalsz;
   }
   /**\todo TODO (2015-02-27) Like everything else in this function, this could be tweaked further
    * to avoid using indirect branches as much as possible*/
   //Now checking if the reachable empty intervals can contain the code and references
   if (available_size_isok(codesz, refssz, reachable_codesz, reachable_refssz,
         reachable_bothsz)) {
      //The estimated size of displaced code and referenced section can fit into the reachable empty spaces
      /*We will reserve the estimated size for displaced referenced sections in the space reachable
       * with references, then reserve everything else in the space reachable with direct branches
       * for code*/
      // Reserves for referenced data the size estimated for them
      pf->availsz_datarefs = patchfile_reserveemptyspaces(pf,
            INTERVAL_REFERENCE, FALSE, get_estimated_patchrefs_size(refssz));
      // Reserves for patched code the size estimated for it
      pf->availsz_codedirect = patchfile_reserveemptyspaces(pf,
            INTERVAL_DIRECTBRANCH, FALSE, get_estimated_patchrefs_size(codesz));
   } else {
      //At least one of the displaced code or section don't fit into the reachable empty spaces
      /*We will reserve the whole space reachable with references for displaced referenced sections
       * and addresses used by indirect branches, then reserve for code what remains in the space reachable
       * with direct branches and everything else*/
      // Reserves reachable available size for referenced data
      pf->availsz_datarefs = patchfile_reserveemptyspaces(pf,
            INTERVAL_REFERENCE, FALSE, UINT64_MAX);
      // Reserves for patched code whatever remains
      pf->availsz_codedirect = patchfile_reserveemptyspaces(pf,
            INTERVAL_DIRECTBRANCH, FALSE, UINT64_MAX);
   }
   DBGMSG(
         "Size reserved for references: %"PRIu64". Size reserved for code reachable with direct branches: %"PRIu64"\n",
         pf->availsz_datarefs, pf->availsz_codedirect);
//////TODO Update the functions retrieving the last available address where to insert code to use the intervals

//   //Computes the distance between the last loaded address in the file and the first
//   int64_t fullloadsz = pf->bindriver->binfile_patch_get_last_load_addr(pf->bfile) - pf->bindriver->binfile_patch_get_first_load_addr(pf->bfile);

   //

   return pf;
}

/**
 * Compares two movedblock_t based on their original starting address. This function is intended to be used with qsort
 * \param m1 First movedblock
 * \param m2 Second movedblock
 * \return -1 if the original starting address of m1 is less than the original starting address of m2, 1 if the original starting
 * address of m2 is less than the original starting address of m1, and 0 if both are equal
 * \todo TODO (2014-12-12) Check if we can improve this to handle the case of blocks that are inside other blocks (which would
 * happen for modifications moving only one instruction intermixed with modifications moving a whole basic block
 * */
static int movedblock_cmporigaddr_qsort(const void* m1, const void* m2)
{
   movedblock_t* mb1 = *(movedblock_t**) m1;
   movedblock_t* mb2 = *(movedblock_t**) m2;
   int64_t addr1 = insn_get_addr(GET_DATA_T(insn_t*, mb1->firstinsn));
   int64_t addr2 = insn_get_addr(GET_DATA_T(insn_t*, mb2->firstinsn));

   if (addr1 < addr2)
      return -1;
   else if (addr1 > addr2)
      return 1;
   else
      return 0;
}

/**
 * Compares two movedblock_t based on their new starting address. This function is intended to be used with qsort
 * \param m1 First movedblock
 * \param m2 Second movedblock
 * \return -1 if the new starting address of m1 is less than the new starting address of m2, 1 if the new starting
 * address of m2 is less than the new starting address of m1, and 0 if both are equal
 * \todo TODO (2014-12-12) Check if we can improve this to handle the case of blocks that are inside other blocks (which would
 * happen for modifications moving only one instruction intermixed with modifications moving a whole basic block
 * */
static int movedblock_cmpnewaddr_qsort(const void* m1, const void* m2)
{
   movedblock_t* mb1 = *(movedblock_t**) m1;
   movedblock_t* mb2 = *(movedblock_t**) m2;
   int64_t addr1 = mb1->newfirstaddr;
   int64_t addr2 = mb2->newfirstaddr;

   if (addr1 < addr2)
      return -1;
   else if (addr1 > addr2)
      return 1;
   else
      return 0;
}

/*
 * Reserves empty spaces for sections referenced by instructions and modified or added by the patch operation
 * \param bf Binary file. Must be being patched
 * \return EXIT_SUCCESS if empty spaces could be reserved for all modified referenced sections, error code otherwise
 * */
static int patchfile_reserveemptyspaces_refscns(patchfile_t* pf)
{
   /**\todo TODO (2015-03-06) I may use this function for other flagging of empty spaces.
    * Update name, parameters and return value accordingly */

   uint16_t i;
   int out = EXIT_SUCCESS;
   /**\todo TODO (2015-03-09) Add another pass to order the sections and spaces by size, so that we try first
    * to fit the largest sections into the largest spaces. Or something more clever (it's a bit of a backpack problem)*/

   /**\todo TODO (2015-03-30) BIG FLOODY BUCKING BUG HERE WITH THE .got AND .got.plt IN x86 ELF. We have to find a way
    * to force them to be moved together, and I'm fed up.
    * Ok maybe we can handle it in the format-specific version of binfile_patch_move_scn_to_interval*/

   //Computes the size of sections referenced by instructions that have been modified or created
   for (i = 0; i < binfile_get_nb_sections(pf->patchbin); i++) {
      if (binfile_patch_is_scn_bigger(pf->patchbin, i)) {
         //Section has been impacted by patching
         binscn_t* scn = binfile_patch_get_scn(pf->patchbin, i);
         if (binscn_check_attrs(scn, SCNA_INSREF)) {
            //Section is referenced by instructions: scanning the empty spaces to find one large enough to contain it
            FOREACH_INQUEUE(pf->emptyspaces, iter) {
               if ((patcher_interval_getreserved(GET_DATA_T(interval_t*, iter))
                     == INTERVAL_REFERENCE)
                     && (patcher_interval_getused(GET_DATA_T(interval_t*, iter))
                           == INTERVAL_NOFLAG)) {
                  /**\todo TODO (2015-03-12) The test on the interval being used may be removed if I end up simply deleting moved
                   * intervals instead of flagging them. So far I'm keeping them and marking them as used*/
                  //The empty space is reserved for sections referenced by instructions: checking if it can be used to move the section
                  interval_t* used = binfile_patch_move_scn_to_interval(pf->patchbin, i,
                        GET_DATA_T(interval_t*, iter));
                  if (binscn_check_attrs(scn, SCNA_PATCHREORDER)) {
                     //The section could be moved: checking if it used the interval provided
                     if (used) {
                        //The section used at least part of the interval: updating the list of intervals
                        if (interval_get_end_addr(used)
                              == interval_get_end_addr(
                                    GET_DATA_T(interval_t*, iter))) {
                           //The section used the whole interval: we mark it as used
                           patcher_interval_setused(
                                 GET_DATA_T(interval_t*, iter),
                                 INTERVAL_REFERENCE);
                           patcher_interval_free(used);
                        } else {
                           //The section used part of the interval: we split it
                           //Resizing the interval so that it now begins at the end of the used interval
                           interval_upd_addr(GET_DATA_T(interval_t*, iter),
                                 interval_get_end_addr(used));
                           //Flagging the new interval as being used
                           patcher_interval_setused(used, INTERVAL_REFERENCE);
                           //Inserting the new interval in the queue
                           queue_insertbefore(pf->emptyspaces, iter, used);
                           /**\todo TODO (2015-03-12) As I said above, I could simply remove it*/
                        }
                     } //Otherwise the section managed to be moved without using the interval
                     break;   //No need to keep searching anyway
                  }  //Otherwise the section could not be moved
               } //Otherwise this interval has not been reserved for sections referenced by instructions
            }
            if (!iter)
               out = ERR_PATCH_NO_SPACE_FOUND_FOR_SECTION; //No space could be found for this section
         }  //Otherwise the section is not referenced by instructions
      } //Otherwise this section has not been modified by the patching operation
   }
   return out;
}

/**
 * Loads a data section with a queue of data_t structures
 * \param scn The section
 * \param datas Queue of data_t structures
 * */
static void load_dataqueue_toscn(binscn_t* scn, queue_t* datas)
{
   assert(scn && datas);
   //uint32_t i = 0;
   /**\todo TODO (2015-04-16) Now would be a good time to add entries for padding between aligned data,
    * unless we took care of that when performing the alignment and the building of the queue of data
    * from the list of global variable requests*/
   //Storing the data entries into the section
//   binscn_set_nb_entries(scn, queue_length(datas));
   FOREACH_INQUEUE(datas, iter)
   {
      /**\todo TODO (2015-05-12) DANGER! If the section was created with no address, binscn_add_entry will give erroneous
       * addresses to the data inside, which can cause problem if they had some. Maybe we should create a binscn_patch_add_entry instead.
       * Conversely, I'm not sure when this function will be invoked (I'm writing another one for data sections), so maybe it won't matter
       * => (2015-05-13) Changing this. But now I will be doing a lot of realloc on the array of entries for the section. So, if it
       * becomes a bottleneck, rewrite binscn_patch_add_entry to allow adding entries at a given index instead or the last*/
      binscn_patch_add_entry(scn, GET_DATA_T(data_t*, iter));  //, i++
   }
}

/**
 * Fills bytes with some padding
 * \param data The byte field where we need to add padding
 * \param off Offset into \c data where we are adding padding
 * \param maxlen Maximum length of padding to write
 * \param padding Bytes to add as padding
 * \param padlen Length of \c padding*/
static uint64_t gen_padding_bytes(void* data, uint64_t off, uint64_t maxlen,
      unsigned char* padding, uint64_t padlen)
{
   /**\todo TODO (2015-05-06) Use this in the 2 functions below (I have trouble with the stop conditions), but I have other
    * bugs to take care of and don't have time right now to change everything. Blarg*/
   assert(data && padding);
   uint64_t len = 0;
   while (len < maxlen) {
      memcpy(data + off, padding, padlen);
      off += padlen;
      len += padlen;
   }
}

/**
 * Regenerates the byte code of a binary section containing moved blocks. Used only to reduce the size of code in patchfile_finalise
 * \param pf Patched file
 * \param scn The section.
 * \return EXIT_SUCCESS / error code
 * */
static int patchfile_codescn_genbytes_fromblocks(patchfile_t *pf, binscn_t* scn)
{
   assert(pf && scn);
   queue_t* mbs = hashtable_lookup_all(pf->movedblocksbyscn, scn);
   assert(mbs);
   queue_sort(mbs, movedblock_cmpnewaddr_qsort);
   unsigned char* data = lc_malloc(sizeof(*data) * binscn_get_size(scn));
   int64_t scnaddr = binscn_get_addr(scn);
   uint64_t off = 0;
   unsigned char padding[INSN_MAX_BYTELEN];
   unsigned int padlen = bitvector_printbytes(insn_get_coding(pf->paddinginsn),
         padding, arch_get_endianness(insn_get_arch(pf->paddinginsn)));
   /**\todo TODO (2015-04-15) Either move the computation of the padding bytes out of the loop over sections (no need to
    * create it/free it at each section), or implement something more thorough to identify which padding instruction we
    * are supposed to use (override in the associated modifs, etc). This is why I recompute it at each function using it
    * instead of storing it someplace convenient (patchfile_t for instance)*/
   DBGMSG(
         "Generating binary code for new code section %s from the moved blocks it contains\n",
         binscn_get_name(scn));
   //DBGMSG0LVL(2, "Instructions are:\n");
   int out = EXIT_SUCCESS, res = EXIT_SUCCESS;

   /**\todo TODO (2015-04-16) Find another way to associate movedblocks to sections to avoid sorting them multiple times*/
   FOREACH_INQUEUE(mbs, iter) {
      movedblock_t* mb = GET_DATA_T(movedblock_t*, iter);
      DBGLVL(1,
            FCTNAMEMSG0("Code for "); movedblock_fprint(mb, stderr); STDMSG(" relocated between addresses %#"PRIx64" and %#"PRIx64"\n", mb->newfirstaddr, mb->newlastaddr));
      //Creates the code for each patched instruction
      FOREACH_INQUEUE(mb->patchinsns, iterpi) {
         patchinsn_t* pi = GET_DATA_T(patchinsn_t*, iterpi);
         uint16_t len;
         unsigned char insnstr[INSN_MAX_BYTELEN];
         len = patchinsn_getbytescoding(pi, insnstr);
         if (len > 0) {
            memcpy(data + off, insnstr, len);
            off += len;
         }
      }
      uint32_t nentry = 0;
      //Now adds data entries
      DBGLVL(2,
            if (queue_length(mb->localdata) > 0) {FCTNAMEMSG0("Variables for "); movedblock_fprint(mb, stderr); STDMSG(" relocated between addresses %#"PRIx64" and %#"PRIx64"\n", mb->newfirstaddr, mb->newlastaddr);})
      FOREACH_INQUEUE(mb->localdata, iterd) {
         data_t* entry = GET_DATA_T(globvar_t*, iterd)->data;
         /**\todo TODO (2015-04-16) WARNING! I describe localdata as being globvar_t structures in patchutils.h, and here I'm
          * using them as data_t. See how I fill them later. Maybe I can avoid the globvar_t thing
          * => (2015-05-13) Fixed. But I leave the comment for now to remember about trying to get rid of the use of globvar_t (don't think so)*/
         /**\todo TODO (2015-05-13) This block of code is very similar to the code in binscn_patch_set_data_from_entries. Find a way to factorise
          * it (and, more importantly, have it only in one of the two files)*/
         //Checking if we need to add some padding to respect alignments
         if (data_get_addr(entry) > ((maddr_t) off + scnaddr)) {
            //Address of the data is higher than the current one: adding some padding
            memset(data + off, 0, data_get_addr(entry) - (off + scnaddr));
            off = data_get_addr(entry) - scnaddr;
         }
         unsigned char* entrybytes = data_to_bytes(entry);
         if (!entrybytes) {
            //Skipping entries that we could not convert as bytes
            ERRMSG(
                  "Unable to store data entry %u at offset %#"PRIx64" into loaded section %s. Skipping entry\n",
                  nentry, off, binscn_get_name(scn));
            out = ERR_LIBASM_ERROR_RETRIEVING_DATA_BYTES;
         } else {
            //Copying the entry data into the section
            memcpy(data + off, entrybytes, data_get_size(entry));
            DBGLVL(2, STDMSG("\t"); data_fprint(entry, stderr);STDMSG("\n");)
         }
         off += data_get_size(entry);
         nentry++;
      }
      assert(off <= binscn_get_size(scn));  //Just checking
      //Now adds padding if the next moved block does not begin right after the current one
      if (iter->next) {
         while (((maddr_t) off + scnaddr)
               < GET_DATA_T(movedblock_t*, iter->next)->newfirstaddr) {
            /**\todo TODO (2015-04-16) FACTORIIIIISE!!!!!!*/
            //Pads the remaining size until the next block
            memcpy(data + off, padding, padlen);
            off += padlen;
            DBGLVL(2,
                  STDMSG("\t Padding to next block ("); insn_fprint(pf->paddinginsn, stderr); STDMSG(")\n");)
         }
      }
   }
   //Now add padding after the last block to complete the section
   while (((maddr_t) off + scnaddr) < binscn_get_end_addr(scn)) {
      /**\todo TODO (2015-04-16) FACTORIIIIISE!!!!!!*/
      //Pads the remaining size until the next block
      memcpy(data + off, padding, padlen);
      off += padlen;
      DBGLVL(2,
            STDMSG("\t Padding to end of section ("); insn_fprint(pf->paddinginsn, stderr); STDMSG(")\n");)
   }
   queue_free(mbs, NULL);
   //Stores the data inside the section
   res = binscn_patch_set_data(scn, data);
   UPDATE_ERRORCODE(out, res)

   return out;
}

/**
 * Regenerates the byte code of a binary section containing instructions. Used only to reduce the size of code in patchfile_finalise
 * \param pf Patched file
 * \param scn The section. Must contain instructions
 * \return EXIT_SUCCESS / error code
 * */
static int patchfile_codescn_genbytes_frominsns(patchfile_t* pf, binscn_t* scn)
{
   /**\todo TODO (2015-05-11) Check for the A_END_LIST and A_BEGIN_LIST annotates to detect instructions that are not contiguous,
    * and check if there are some data to put between them*/
   assert(pf && binscn_patch_get_first_insn_seq(scn));
   binscn_t* origin = binscn_patch_get_origin(scn);
   list_t* firstinsnseq = binscn_patch_get_first_insn_seq(scn);
   list_t* lastinsnseq = binscn_patch_get_last_insn_seq(scn);
   assert(lastinsnseq); //If we have a first insn, we should have a last insn, otherwise we royally screwed up somewhere
   int64_t firstaddr = insn_get_addr(GET_DATA_T(insn_t*, firstinsnseq));
   int64_t lastaddr = insn_get_end_addr(GET_DATA_T(insn_t*, lastinsnseq));
   unsigned char* data;
   uint64_t size;
   list_t* iter;
   uint64_t off = 0;
   uint16_t insize = 0;
   unsigned char insnstr[INSN_MAX_BYTELEN];
   int out = EXIT_SUCCESS;
   int res = EXIT_SUCCESS;

   if (!origin) {
      //Section is new: we have to build its content instruction by instruction
      int64_t nextdataaddr;
      uint32_t j;
      //First, detecting sections whose addresses have not been updated (this can happen if the section was added by the binfile)
      if (firstaddr == ADDRESS_ERROR) {
         insnlist_upd_addresses(NULL, binscn_get_addr(scn), firstinsnseq,
               lastinsnseq->next);
         firstaddr = insn_get_addr(GET_DATA_T(insn_t*, firstinsnseq));
         lastaddr = insn_get_end_addr(GET_DATA_T(insn_t*, lastinsnseq));
      }

      size = lastaddr - firstaddr;
      DBGMSG(
            "Generating binary code for new code section %s of size %#"PRIx64" from its instructions\n",
            binscn_get_name(scn), size);
      DBGMSG0LVL(2, "Instructions are:\n");
      //Computing its size from the addresses of the instructions it contains (all addresses should have been updated)
      if (binscn_get_nb_entries(scn) > 0) {
         /**\todo TODO (2015-04-15) In the case of sections containing data interleaved with code, I consider
          * that the A_BEGIN_LIST and A_END_LIST annotation have been properly added to the instructions*/
         //Section contains data entries: adding their size
         for (j = 0; j < binscn_get_nb_entries(scn); j++)
            size += data_get_size(binscn_get_entry(scn, j));
         nextdataaddr = data_get_addr(binscn_get_entry(scn, 0));
      } else
         nextdataaddr = INT64_MAX; /**\todo TODO (2015-04-15) Because some day I would love addresses to be unsigned and it's easier*/

      //Allocates the data for the section
      data = lc_malloc(sizeof(*data) * size);
      j = 0;
      //Now generates the coding of the section
      for (iter = firstinsnseq; iter != lastinsnseq->next; iter = iter->next) {
         insn_t* insn = GET_DATA_T(insn_t*, iter);
         //There is a data entry before this instruction: inserting it
         while (insn_get_addr(insn) > nextdataaddr) {
            /**\todo TODO (2015-04-15) BEWARE OF EVENTUAL PADDING BETWEEN DATA AND INSTRUCTIONS!
             * Ideally this should be already taken care of, with data entries representing the padding*/
            //Inserting data that would be interleaved with the code
            data_t* entry = binscn_get_entry(scn, j);
            unsigned char* datastr = data_to_bytes(entry);
            memcpy(data + off, datastr, data_get_size(entry));
            off += data_get_size(entry);
            j++;
            //Storing address of the next data object. If it was the last, setting address to infinity
            nextdataaddr =
                  (j < binscn_get_nb_entries(scn)) ?
                        data_get_addr(binscn_get_entry(scn, j)) : INT64_MAX;
         }
         insize = bitvector_printbytes(insn_get_coding(insn), insnstr,
               arch_get_endianness(insn_get_arch(insn)));
         memcpy(data + off, insnstr, insize);
         off += insize;
         //lc_free(insnstr);
         /**\todo TODO (2015-04-15) Change bitvector_charvalue and use a stack-allocated array to avoid this free
          * => (2015-05-26) DONE*/
         DBGLVL(2, patcher_insn_fprint(insn, stderr, A_NA))
      }
      //Inserting all data entries that would be remaining after the last instruction
      while (j < binscn_get_nb_entries(scn)) {
         /**\todo TODO (2015-04-15) Guess what? Factorise this with the code above.*/
         data_t* entry = binscn_get_entry(scn, j);
         unsigned char* datastr = data_to_bytes(entry);
         memcpy(data + off, datastr, data_get_size(entry));
         off += data_get_size(entry);
         j++;
      }
      assert(off == size); //Just checking
      binscn_set_size(scn, size);
   } else {
      /**\todo (2015-04-22) We may add some way to detect if the code itself has been changed (simply flag sections where we create
       * patchinsn_t structures) and exit if neither instructions nor data structures have been changed (we will copy the original section
       * when writing instead)*/
      //Section already existed: we can import some of its code
      int64_t scnaddr = binscn_get_addr(origin);
      unsigned char padding[insn_get_bytesize(pf->paddinginsn)];
      unsigned padlen = bitvector_printbytes(insn_get_coding(pf->paddinginsn),
            padding,
            arch_get_endianness(insn_get_arch(pf->paddinginsn)));
      /**\todo TODO (2015-04-15) Either move the computation of the padding bytes out of the loop over sections (no need to
       * create it/free it at each section), or implement something more thorough to identify which padding instruction we
       * are supposed to use (override in the associated modifs, etc)*/
      assert(
            binscn_get_type(scn) == SCNT_PATCHCOPY
                  || binscn_get_nb_entries(scn) == binscn_get_nb_entries(origin)); //So far we don't support the addition of data entries into existing sections
      size = binscn_get_size(origin); //We assume the section did not change size
      //Initialising the data of the section with a copy from the original
      data = lc_malloc(sizeof(*data) * size);
      memcpy(data, binscn_get_data(origin, NULL), size);
      DBGMSG(
            "Generating binary code for existing code section %s of size %#"PRIx64" from its instructions\n",
            binscn_get_name(scn), size);
      DBGMSG0LVL(2, "Instructions are:\n");

      //Scanning the instructions in the section until we reach a moved or modified instruction
      for (iter = firstinsnseq; iter != lastinsnseq->next; iter = iter->next) {
         insn_t* insn = GET_DATA_T(insn_t*, iter);
         if (!insn_check_annotate(insn, A_PATCHMOV)
               && !insn_check_annotate(insn, A_PATCHUPD)
               && !insn_check_annotate(insn, A_PATCHDEL)) {
            DBGLVL(2, patcher_insn_fprint(insn, stderr, A_NA))
            continue;
         }
         if (insn_check_annotate(insn, A_PATCHMOV)) {
            //Instruction has been moved
            //Retrieving the associated moved block
            movedblock_t* mb = hashtable_lookup(pf->movedblocksbyinsns, insn);
            assert(mb);
            off = insn_get_addr(insn) - scnaddr;
            //Copies the code of the branch instructions replacing the moved block
            FOREACH_INQUEUE(mb->newinsns, itermbi) {
               /**\todo TODO (2015-04-15) Factorise this snippet of code!*/
               insize = bitvector_printbytes(
                     insn_get_coding(GET_DATA_T(insn_t*, itermbi)), insnstr,
                     arch_get_endianness(
                           insn_get_arch(
                                 GET_DATA_T(insn_t*, itermbi))));
               memcpy(data + off, insnstr, insize);
               off += insize;
               //lc_free(insnstr);
               /**\todo TODO (2015-04-15) Change bitvector_charvalue and use a stack-allocated array to avoid this free
                * => (2015-05-26) DONE*/
               DBGLVL(2,
                     patcher_insn_fprint(GET_DATA_T(insn_t*, itermbi), stderr, A_PATCHNEW))
            }
            int64_t nextaddr = off + scnaddr;
            //Skips instructions that were replaced by the branch instruction from the moved block
            do {
               iter = iter->next;
//               DBGLVL(2, patcher_insn_fprint(pf, GET_DATA_T(insn_t*, iter), stderr, A_NA))
            } while (iter
                  && insn_get_end_addr(GET_DATA_T(insn_t*, iter)) < nextaddr);
            //Adds some padding if we are now in the middle of an instruction
            while (nextaddr < insn_get_end_addr(GET_DATA_T(insn_t*, iter))) {
               /**\todo TODO (2015-04-16) FACTORIIIIISE!!!!!!*/
               //Pads the remaining size
               memcpy(data + off, padding, padlen);
               off += padlen;
               nextaddr += padlen;
               DBGLVL(2,
                     patcher_insn_fprint(pf->paddinginsn, stderr, A_PATCHNEW))
            }
            //Debug: printing the instructions that we skip
            DBGLVL(2,
                  if (iter != mb->lastinsn->next) { list_t* __iterdbg = iter->next; while (__iterdbg != mb->lastinsn->next) { patcher_insn_fprint(GET_DATA_T(insn_t*, __iterdbg), stderr, A_NA); __iterdbg = __iterdbg->next; } })
            //Now skips to the end of the movedblock
            iter = mb->lastinsn; //The iter = iter->next of the FOREACH should then skip to the first instruction after the block
         } else if (insn_check_annotate(insn, A_PATCHUPD)
               || insn_check_annotate(insn, A_PATCHDEL)) {
            //Instruction has been modified
            //Retrieving the associated patched insn
            patchinsn_t* pi = hashtable_lookup(pf->patchedinsns, insn);
            assert(pi);
            off = insn_get_addr(insn) - scnaddr;
            insize = patchinsn_getbytescoding(pi, insnstr);
            if (insize > 0) {
               memcpy(data + off, insnstr, insize);
               //lc_free(insnstr);
               /**\todo TODO (2015-04-15) Change bitvector_charvalue and use a stack-allocated array to avoid this free
                * => (2015-05-26) DONE*/
               off += insize;
            }
            //Checks if the patched instruction already contains some padding and attempt to add some
            list_t* iterpad = list_getnext(insn_get_sequence(pi->patched));
            /**\todo TODO (2015-05-28) This is a little hack I'm not very proud of. I'm using the fact that, for replaced instructions,
             * we created the patchinsn_t from a queue of instructions that already contained the padding instructions. So I'm using the sequence
             * of these instructions to be sure to access them. It is necessary to do that if we want to retrieve the padding specified for the
             * replacement instead of the default one, but I don't find it very pretty.*/
            while (iterpad) {
               uint16_t padsize = bitvector_printbytes(
                     insn_get_coding(GET_DATA_T(insn_t*, iterpad)), insnstr,
                     arch_get_endianness(
                           insn_get_arch(
                                 GET_DATA_T(insn_t*, iterpad))));
               memcpy(data + off, insnstr, padsize);
               off += padsize;
               insize += padsize;
               iterpad = iterpad->next;
            }
            assert(insize <= insn_get_bytesize(insn));
            //Adds some padding to reach the end of the original instruction
            while (insize < insn_get_bytesize(insn)) {
               //Pads the remaining size
               memcpy(data + off, padding, padlen);
               off += padlen;
               insize += padlen;
               DBGLVL(2,
                     patcher_insn_fprint(pf->paddinginsn, stderr, A_PATCHNEW))
            }
         } //Otherwise the instruction was not modified and its code will remain as it is
      }
      //lc_free(padding);
      /**\todo TODO (2015-04-15) Change bitvector_charvalue and use a stack-allocated array to avoid this free
       * => (2015-05-26) DONE*/
   }
   //Stores the data inside the section
   res = binscn_patch_set_data(scn, data);
   if (ISERROR(res)) {
      ERRMSG("Unable to update binary content of section %s\n",
            binscn_get_name(scn));
   }
   UPDATE_ERRORCODE(out, res)

   return out;
}

/**
 * Creates the label request for a global variable.
 * \param pf The patched file
 * \param gv A global variable
 * \param scn The section where the global variable will be stored
 * */
static void patchfile_addlabel_forglobvar(patchfile_t* pf, globvar_t* gv,
      binscn_t* scn)
{
   assert(pf && gv && scn);
   label_t* lbl = label_new(gv->name, data_get_addr(gv->data), TARGET_DATA,
         gv->data);
   label_set_scn(lbl, scn);
   label_set_type(lbl, LBL_VARIABLE);
   binfile_patch_add_label(pf->patchbin, lbl);

}

/**
 * Creates the label request for a moved block.
 * \param pf The patched file
 * \param mb A moved block
 * \param scn The section where the moved block will be moved
 * */
static void patchfile_addlabel_formovedblock(patchfile_t* pf, movedblock_t* mb,
      binscn_t* scn)
{
   assert(pf && mb && scn);
   //First, create the name of the label
   insn_t* firstin = GET_DATA_T(insn_t*, mb->firstinsn);
   label_t* firstinlbl = insn_get_fctlbl(firstin);
   char buf[strlen(label_get_name(firstinlbl)) + 20]; //This should be enough to contain "@0x" + 16 numbers + the null byte
   sprintf(buf, "%s@%#"PRIx64, label_get_name(firstinlbl),
         insn_get_addr(firstin));
   //Now finds the first instruction of the new block
   list_t* iter = queue_iterator(mb->patchinsns);
   while (iter && !GET_DATA_T(patchinsn_t*, iter)->patched)
      iter = iter->next;
   assert(iter);
   label_t* lbl = label_new(buf,
         insn_get_addr(GET_DATA_T(patchinsn_t*, iter)->patched), TARGET_INSN,
         GET_DATA_T(patchinsn_t*, iter)->patched);
   label_set_scn(lbl, scn);
   label_set_type(lbl, LBL_DUMMY);
   binfile_patch_add_label(pf->patchbin, lbl);
}

/**
 * Creates a new data section containing a list of global variables
 * \param pf The patched file
 * \param gvars Queue of globvar_t structures containing the data to add
 * */
static binscn_t* patchfile_adddatasection(patchfile_t* pf, queue_t* gvars)
{
   assert(pf && gvars);
//   list_t* iter = queue_iterator(gvars);
   data_t* data = GET_DATA_T(globvar_t*, queue_iterator(gvars))->data;
   int64_t scnaddr = data_get_addr(data);
//   uint64_t scnsz = data_get_size(data);
   //Initialising the section with the size and address of the first entry it contains
   binscn_t* scn = binfile_patch_add_data_scn(pf->patchbin, NULL, scnaddr, 0);
//   //Adding the first entry
//   binscn_patch_add_entry(scn, data);
//   iter = iter->next;   //Starting at the second data in the list to avoid having to test on the existence of the previous data in the loop
   /**\todo TODO (2015-05-12) Be careful, binscn_add_entry updates the address of the data we add, so this may cause problems with our computation
    * of paddings. If necessary, create a binscn_patch_add_entry instead that does what is done here
    * => (2015-05-13) Doing that now because of other problems*/
   FOREACH_INQUEUE(gvars, iter) {
      data = GET_DATA_T(globvar_t*, iter)->data;
//      if (data_get_addr(data) > data_get_end_addr(GET_DATA_T(globvar_t*, iter->prev)->data)) {
//         //The data is not contiguous with the previous one: creating a data that serves as padding
//         data_t* padding = data_new(DATA_NIL, NULL, data_get_addr(data) - data_get_end_addr(GET_DATA_T(globvar_t*, iter->prev)->data));
//         binscn_add_entry(scn, padding, UINT32_MAX);
//         scnsz += data_get_size(padding);
//      }
      //Adding the data to the section
      binscn_patch_add_entry(scn, data);
      /**\todo TODO (2015-05-19) Maybe add a flag here on the globvar to notify that its data was stored into a section.
       * If this flag is set, we don't delete the data_t when deleting the globvar_t in globvar_free, as it will be freed
       * when the section is freed*/
//      scnsz += data_get_size(data);
      //Creating a label for the global variable
      /**\todo TODO (2015-05-12) It is not very coherent to have this being done in this function, but the main interest
       * is that I can link the section to the new label right now. Maybe move it elsewhere more appropriate.
       * Also, take into account the future flags from the MADRAS API specifying how we are adding labels to the file
       * (no labels, only for blocks, etc)*/
      patchfile_addlabel_forglobvar(pf, GET_DATA_T(globvar_t*, iter), scn);
//      iter = iter->next;
   }
//   //Updates the size of the section
//   binscn_set_size(scn, scnsz);
   return scn;
}

/**
 * Finds spaces for storing each variable in a list, then fuses all contiguous spaces containing variables
 * (identified as being used with INTERVAL_REFERENCE), and create a data section for each fused space, and deletes the interval.
 * \param pf The patchfile
 * \param data Queue of globvar_t structures to insert
 * \param restype Type of reserved interval to use for storing the variable
 * \param annotate Additional flag to set on the section (it will be flagged as SCNA_PATCHREORDER)
 * \return EXIT_SUCCESS or error code
 * */
static int patchfile_createdatascns(patchfile_t* pf, queue_t* datas,
      unsigned int restype, uint16_t annotate)
{
   assert(pf && queue_length(datas) > 0);
   /**\todo TODO (2015-05-12) This is the basically the same code as the one below for affecting moved blocks to new code sections.
    * Find some way to factorise it*/
   int out = EXIT_SUCCESS;
   //Finding the empty spaces for containing variables in the list
   FOREACH_INQUEUE(datas, iternd_ir) {
      globvar_t* gv = GET_DATA_T(globvar_t*, iternd_ir);
      int res = patchfile_globvar_findspace(pf, gv, restype);
      if (ISERROR(res)) {
         ERRMSG(
               "Unable to find space for variable %s (globvar_%d). Variable will not be added\n",
               gv->name, gv->globvar_id);
         /**\todo TODO (2015-05-12) Add some handling here*/
         UPDATE_ERRORCODE(out, res)
         continue;
      }
   }
   //Now fusing all spaces associated to variables and create a section for each of them
   list_t* iterds = queue_iterator(pf->emptyspaces);
   while (iterds) {
      interval_t* interd = GET_DATA_T(interval_t*, iterds);
      if (patcher_interval_getused(interd) != INTERVAL_REFERENCE
            || !interval_get_data(interd)) {
         iterds = iterds->next;
         continue;   //Interval has not been used for storing data
      }
      /**\todo TODO (2015-05-13) OK, so the test on interval_get_data is to distinguish the intervals that have been used by the libbin
       * to store sections referenced by instruction (they will be flagged as INTERVAL_REFERENCE too) from those we are looking for
       * (they will contain the data). This is becoming increasingly complicated and clunky, so I guess the best way to handle this
       * would be simply to have MORE floody bucking INTERVAL_XXX flags so that we can distinguish all the cases. And I'm not too fond
       * of the use of the interval data either. In the meantime, well, that will be how I roll.*/
      while (iterds->next
            && (patcher_interval_getused(GET_DATA_T(interval_t*, iterds->next))
                  == INTERVAL_REFERENCE)
            && (interval_get_addr(GET_DATA_T(interval_t*, iterds->next))
                  == interval_get_end_addr(interd))
            && interval_get_data(GET_DATA_T(interval_t*, iterds->next))) {
         //The next interval exists, is contiguous, and has been affected to another variable: fusing it with the current one
         patchfile_fuseemptyspaces(pf, iterds);
      }
      //At this point the current empty space contains all contiguous variables (taking the alignment into account): creating the section
      queue_t* gvars = interval_get_data(interd);
      binscn_t* scn = patchfile_adddatasection(pf, gvars);
      //Flagging the section as reachable from the code and also as having been already affected a new address
      binscn_add_attrs(scn, annotate | SCNA_PATCHREORDER);

      //Deleting the space (it's easier that way, I think)
      list_t* nextiterds = iterds->next;
      patcher_interval_free(queue_remove_elt(pf->emptyspaces, iterds));
      /**\todo TODO (2015-05-12) Blah blah blah coherence when deleting spaces and flagging them as used, blah blah blah*/
      iterds = nextiterds;
   }
   return out;
}

/**
 * Finalises modifications concerning libraries
 * \param pf File being patched
 * \return EXIT_SUCCESS if successful, error code otherwise
 * */
static int patchfile_finalise_modiflibs(patchfile_t* pf)
{
   assert(pf);
   int out = EXIT_SUCCESS;
   if (queue_length(pf->modifs_lib) > 0) {
      //Handling libraries modifications
      inslib_t** inslibs = NULL;
      int nlibs = 0;
      int res = EXIT_SUCCESS;

      FOREACH_INQUEUE(pf->modifs_lib, liter) {
         modiflib_t* modiflib = GET_DATA_T(modiflib_t*, liter);
         int i;
         //Adding libraries
         switch (modiflib->type) {
         case ADDLIB:
            switch (modiflib->data.inslib->type) {
            case STATIC_LIBRARY:
               //Adds all files defined in the library to the queue in the patchfile object
               for (i = 0; i < modiflib->data.inslib->n_files; i++) {
                  queue_add_tail(pf->insertedlibs,
                        modiflib->data.inslib->files[i]);
               }
               break;
            case DYNAMIC_LIBRARY:
               //Adds the name of the dynamic library to insert to the array of dynamic libraries
               inslibs = lc_realloc(inslibs, sizeof(inslib_t*) * (nlibs + 1));
               inslibs[nlibs] = modiflib->data.inslib;
               nlibs++;
               break;
            }
            break;
         case RENAMELIB:
            res = binfile_patch_rename_ext_lib(pf->patchbin,
                  modiflib->data.rename->oldname,
                  modiflib->data.rename->newname);
            if (!ISERROR(out) && res != EXIT_SUCCESS)
               out = res;
            break;
         }
      }
      if (nlibs > 0) {
         //Inserts the external library names
         //out &= patchfile_add_extlibs(pf,inslibs,nlibs);
         int i;
         for (i = 0; i < nlibs; i++) {
            DBGMSG("Inserting library %s\n", inslibs[i]->name);
            res = binfile_patch_add_ext_lib(pf->patchbin, inslibs[i]->name,
                  (inslibs[i]->flags & LIBFLAG_PRIORITY));
            if (!ISERROR(out) && res != EXIT_SUCCESS)
               out = res;
         }
         lc_free(inslibs);
      }
   }

   return out;
}

/**
 * Creates all branches reaching a moved block, including those for trampolines
 * \param pf The patched file
 * \param mb The moved block
 * */
static void movedblock_createbranches(patchfile_t* pf, movedblock_t* mb)
{
   assert(pf && mb);
   pointer_t* ptr = NULL;
   DBGLVL(1,
         FCTNAMEMSG0("Creating branches to and from "); movedblock_fprint(mb, stderr); STDMSG("\n"));
   //Finding the first instruction in the displaced block
   FOREACH_INQUEUE(mb->patchinsns, itermbpi) {
      if (GET_DATA_T(patchinsn_t*, itermbpi)->patched)
         break;
   }
   assert(itermbpi);
   //Now we have to detect where to add this jump depending on whether we are using a trampoline or not
   if (mb->trampoline) {
      DBGLVL(2,
            FCTNAMEMSG0("The "); movedblock_fprint(mb, stderr); STDMSG(" uses a trampoline in "); movedblock_fprint(mb->trampoline, stderr); STDMSG("\n"));
      //Current block uses a trampoline
      int64_t jmpsaddr;
      //Finding the address at which the instructions branching to the moved code will be inserted
      if (mb->trampoline->newinsns) //The trampoline block already has new code: appending to it
         jmpsaddr = insn_get_end_addr(queue_peek_tail(mb->trampoline->newinsns));
      else
         //New instructions for the trampoline block have not been generated yet: reserving the size for the branch to its moved code
         jmpsaddr = insn_get_addr(
               GET_DATA_T(insn_t*, mb->trampoline->firstinsn))
               + patchfile_getjumpsize(pf, mb->trampoline->jumptype);

      //Generates the instruction(s) for jumping to the displaced code
      queue_t* jmps = patchfile_getjump(pf, mb->jumptype, jmpsaddr, &ptr);
      //Links the pointer to the first instruction of the displaced block
      pointer_set_insn_target(ptr, GET_DATA_T(patchinsn_t*, itermbpi)->patched);

      //Now creating a small jump to the branch instructions
      queue_t* smalljmps = patchfile_getjump(pf, JUMP_TRAMPOLINE,
            insn_get_addr(GET_DATA_T(insn_t*, mb->firstinsn)), &ptr);
      /**\todo TODO (2015-04-15) Cf the todo in the declaration of movedblock_t: we assume all trampolines use a small jump
       * If it is not the case, update the movedblock_t structure and this code here accordingly*/
      //Linking the small jump to the instructions jumping to the displaced code
      pointer_set_insn_target(ptr, queue_peek_head(jmps));

      //Adding the new instructions to the list of instructions replacing the block
      if (mb->newinsns) {
         /*Handling the case where branches to blocks using this block as a trampoline have already been added:
          * the branch instruction to the displaced code is always the first **/
         queue_prepend_and_keep(smalljmps, mb->newinsns);
         lc_free(smalljmps);
      } else
         mb->newinsns = smalljmps;

      //Now appending the queue of instructions jumping to the moved block to the list of instructions replacing the trampoline block
      if (mb->trampoline->newinsns) {
         //Trampoline block alread has instructions
         queue_append(mb->trampoline->newinsns, jmps);
         //Freeing the list of instructions
         //lc_free(jmps);
      } else {
         //Trampoline block has not been handled yet
         mb->trampoline->newinsns = jmps;
      }

   } else {
      DBGLVL(2,
            FCTNAMEMSG("Creating branch at address %#"PRIx64" jumping to the beginning of displaced ", insn_get_addr(GET_DATA_T(insn_t*, mb->firstinsn))); movedblock_fprint(mb, stderr); STDMSG("\n"));
      //Generates the instruction(s) for jumping to the displaced code
      queue_t* jmps = patchfile_getjump(pf, mb->jumptype,
            insn_get_addr(GET_DATA_T(insn_t*, mb->firstinsn)), &ptr);
      //Links the pointer to the first instruction of the displaced block
      pointer_set_insn_target(ptr, GET_DATA_T(patchinsn_t*, itermbpi)->patched);

      //Adding the new instructions to the list of instructions replacing the block
      if (mb->newinsns) {
         /*Handling the case where branches to blocks using this block as a trampoline have already been added:
          * the branch instruction to the displaced code is always the first **/
         queue_prepend_and_keep(jmps, mb->newinsns);
         lc_free(jmps);
      } else
         mb->newinsns = jmps;
   }
}

/**
 * Opens the new file where the patched file will be saved.
 * \param pf Patched file
 * \param newfilename Name of the file to open
 * \return EXIT_SUCCESS if the file could be successfully created, error code otherwise
 * */
static int patchfile_initpatchedfile(patchfile_t* pf, char* newfilename)
{
   if (!pf)
      return ERR_PATCH_NOT_INITIALISED;
   if (!newfilename)
      return ERR_COMMON_FILE_NAME_MISSING;
   return binfile_patch_create_file(pf->patchbin, newfilename);
}

/*
 * Finalises a patching session by building the list of instructions and binary codings, but not writing the file
 * \param pf The patched file
 * \param newfilename The file name under which the patched file must be saved
 * \return EXIT_SUCCESS for success, error code otherwise
 * */
int patchfile_finalise(patchfile_t* pf, char* newfilename)
{
   if (!pf)
      return ERR_PATCH_NOT_INITIALISED;
   int out = EXIT_SUCCESS;
   int res = EXIT_SUCCESS;
   res = patchfile_initpatchedfile(pf, newfilename);
   if (ISERROR(res)) {
      // File name not provided or invalid: creating a default name based on the original name
      const char* suffix = "-madras_patch";
      char dfltname[strlen(asmfile_get_name(pf->afile)) + strlen(suffix) + 1];
      sprintf(dfltname, "%s%s", asmfile_get_name(pf->afile), suffix);
      res = patchfile_initpatchedfile(pf, dfltname);
      if (!ISERROR(res)) {
         WRNMSG(
               "Unable to save patched file with name %s: saving it under name %s\n",
               newfilename, dfltname);
         out = WRN_PATCH_FILE_SAVED_WITH_DEFAULT_NAME;
      } else {
         ERRMSG("Unable to save patched file as %s: aborting patch\n",
               newfilename);
         return res;
      }
   } else {
      UPDATE_ERRORCODE(out, res)
   }
   /**\note (2014-11-06) What we will be doing here (yeah, I could have added this to the TODO above but it could be added to the doc later)
    * (2014-12-17) Updating
    * - Reorder all movedblock by address
    * - For each movedblock
    *   - If block size is 0, discard the block
    *   - Order the modifications by address/type/order of creation (cf old ordering) [insert before/delete/replace/modify]
    *   - For each insn_t in the movedblock
    *     - Store branches pointing to it in a list
    *   - For each modif_t in the block
    *     - Concatenate the patchinsn_t structures associated to the patchmodif_t structure for this modif_t
    *     - Flag the associated instructions as A_PATCHDEL if delete or replace, and as A_PATCHMOD if modify
    *   - Add the return jump instruction
    * - For each modif_t for which movedblock is NULL
    *   - Flag the associated instructions as noted above
    *   - Generate the associated patchinsn_t
    *   - Store branches pointing to associated instructions in a list
    * - For each instruction pointing to a modified instruction
    *   - Create a patchinsn_t of type A_PATCHUPD using this instruction
    *   - Point the new instruction in patchinsn_t either to the new modified/moved instruction or its modification (beware of insert before and delete)
    *>>TO BE HANDLED   - This includes all branches pointing to a modified instruction, especially if an insertion was done before or if the instruction was deleted
    * - Generate the binary code for each movedblock
    * - Invoke the libmbin to create sections for the new code (possibly spreading blocks between sections, at least for fixed and not fixed)
    * - Perform labels modifications
    * - At this point, we may want to create new labels associated to modifications
    * - Process all other modifications (new libraries, new data, etc). Cf patchfile_patch
    *   - Invoke the libmbin to create section for new data
    * - Invoke binfile_patch_finalise (this is elfexe_scnreorder). It will reorder the sections and affect new addresses
    *   (- assert checking that the section for fixed code has not changed address nor size)
    *   - Update code in the new section containing stubs for external functions (done by the libbin)
    * - Recompute all addresses and codes
    * - Generates jumps to moved blocks and trampolines
    * **** Maybe stop at this point and do the rest in another function to allow retrieving information on where modifications are ***
    * - Regenerate binary code for data
    * - Regenerate code for and code sections:
    *   - For each insn_t in original code
    *     - If no flag, keep the original coding
    *     - A_PATCHMOV:
    *       - Retrieve movedblock_t
    *       - Add 1 jump to this block inside the original block
    *       - If movedblock_t contains tr87ampolines, add them
    *       - If there must be some padding, add it until the end of the block, otherwise keep the original code
    *     - A_PATCHUPD/A_PATCHMODIF
    *       - Retrieve associated patchinsn_t
    *       - Copy code of new insn in patchinsn_t
    *       - If followers (padding) also retrieve their code
    *   - Pour chaque movedblock_t
    *     - Compute binary code (ignoring A_PATCHDEL)
    *     - Copy it
    * - Invoke binfile_patch_write
    * - Go home. Well no, check if I'm not forgetting anything from the previous version. Then stop stalling and code the binfile_patch_finalise
    *
    * (2015-03-04) UPDATE - to insert in the algorithm above where relevant
    * - Perform all modifications that need to be done on the binary file <= WARNING, CHECK THAT THERE IS NO DEPENDENCY
    * - Separate the blocks depending on how they are reachable: direct branch, memory reference, or autonomous indirect jump
    * - Invoke the binfile to assign the empty spaces in the binary file to the moved binary sections, and reserve an estimate of the space needed for the new
    *   code and data sections
    * - Update the moved blocks
    *
    *
    *
    *
    * */
   list_t* iteri = NULL;
   queue_t* originbranches = queue_new(); //List storing the instructions pointing to a moved or modified instruction
   queue_t* references = queue_new(); //List storing the data entries pointing to a moved or modified instruction

   //Finalising the modifications of libraries
   res = patchfile_finalise_modiflibs(pf);
   UPDATE_ERRORCODE(out, res)

   //Now computes the size of modified sections referenced by instructions and flags the empty spaces reserved for them as being used
   /**\todo TODO (2015-03-05) What we need to do here :
    * - Compute the size of the sections referenced by instructions that have been modified or added (!madras.plt does not count)
    *   - Adds the size of the new data section, which contains addresses for memrel jumps
    *   - Marks the emptyspaces reserved for those sections as used
    * - Retrieve available space reachable with data references and update the field in patchfile accordingly
    * - Now retrieve the first address reachable with direct jump / or memrel jumps if not
    * - Finalises the movedblocks: reorder them, affect addresses starting from this first address. For each movedblock, check that the first available
    *   address is contiguous
    * - Computes the total sizes for code reachable with direct jumps, memrel jumps, indirect jumps, and data
    * - Invoke binfile_patch_finalise, which will do the following:
    *   - Create the new data section
    *   - Assign modified sections referenced by instructions and the new data section to the available empty spaces
    *   /!\ BEWARE OF SECTIONS THAT CAN'T FIT IN AN INTERVAL
    *   - Create the new code section(s) and assign them to the corresponding available empty spaces reserved for them
    *   - Finalise the reorder, compute segments, etc
    * - Create labels here
    * */
   //Checking if there are addresses used by branch instructions
   if (queue_length(pf->memreladdrs) > 0) {
      //There is at least one address used by a branch instruction to displaced code using a memory relative operand: creating the section containing the addresses
      binscn_t* memrelsscn = binfile_patch_add_data_scn(pf->patchbin, NULL,
            ADDRESS_ERROR, queue_length(pf->memreladdrs) * pf->addrsize);
      /**\todo TODO (2015-05-12) Beware of aligment of such a section. There may be a constraint depending on the architecture.
       * Possibly create a specific function from the interface of the libbin, or retrieve the alignment somewhere from a driver and affect it*/
      //Flags this section as being referenced by instructions in the original code
      binscn_add_attrs(memrelsscn, SCNA_INSREF);
      //Stores the addresses into the section
      load_dataqueue_toscn(memrelsscn, pf->memreladdrs);
   }

   //Forces the finalisation of all modifications
   FOREACH_INQUEUE(pf->modifs, iterm1) {
      if (!(GET_DATA_T(modif_t*, iterm1)->annotate & A_MODIF_FINALISED)) {
         INFOMSG("Forcing finalisation of modification %d\n",
               GET_DATA_T(modif_t*, iterm1)->modif_id);
         patchfile_modif_finalise(pf, GET_DATA_T(modif_t*, iterm1));
      }
   }

   //Reordering movedblock by address (this is purely for comfort when debugging the patched file)
   queue_sort(pf->movedblocks, movedblock_cmporigaddr_qsort);

   //Creating patchinsn_t structures for each movedblock
   FOREACH_INQUEUE(pf->movedblocks, itermb) {
      movedblock_t* mb = GET_DATA_T(movedblock_t*, itermb);
      if ((queue_length(mb->modifs) == 0)
            && (queue_length(mb->trampsites) == 0)) {
         //Basic block has been emptied: we unflag the instructions it contains
         iteri = mb->firstinsn;
         while (iteri && iteri != mb->lastinsn->next) {
            insn_rem_annotate(GET_DATA_T(insn_t*, iteri), A_PATCHMOV);
            iteri = iteri->next;
         }
         DBG(
               FCTNAMEMSG0("Discarding "); movedblock_fprint(mb, stderr); STDMSG(" which is not used for modifications or trampolines\n");)
         continue; //Discarding empty basic blocs
         /**\note (2014-12-18) So far I'm considering that a movedblock_t is computed once and for all and never deleted during the patching process
          * even if at one point it does not contain any modif_t (which allows to avoid having to recompute it if another modif_t is added later on).
          * At this point though we can safely discard it.*/
         /**\todo TODO (2014-12-18) Check if we should delete the movedblock now from the list. This would allow to avoid having to check
          * later on if a movedblock exists before computing its code*/
         /**\todo TODO (2015-05-19) If this block used a trampoline, remove it from the trampoline block and check if this block does not need
          * to be removed because of this (I should factorise the discarding function)*/
      }
      //Orders modifications by respective order of address, type, and id
      queue_sort(mb->modifs, modif_cmp_qsort);
      //Creates the patchinsn_t structures for the block
      movedblock_finalise(pf, mb, originbranches, references);
      /**\todo TODO (2015-03-18) Handle return value of movedblock_finalise in case of an error*/
   }

   /**\todo TODO (2015-03-19) I think I can get rid of the fixed modifs, and use the same mechanism as I did in master.*/
   //Creating patchinsn_t structure for each fixed movedblock
   FOREACH_INQUEUE(pf->fix_movedblocks, iterfmb) {
      /**\todo TODO (2015-03-13) See if I keep handling the fixed modifications like this, or if I reuse the pre-commit mode where
       * each modification can be accessed. If I keep it, fixed modifications would always be added in the space for indirect branches
       * if we are not sure to have enough space for direct branches*/
      movedblock_t* mb = GET_DATA_T(movedblock_t*, iterfmb);
      assert(queue_length(mb->modifs) > 0); //We don't allow for modifs to be deleted from fixed moved blocks
      //Creates the patchinsn_t structures for the block
      movedblock_finalise(pf, mb, originbranches, references);
      //No reorder as the purpose of those blocks is to offer fixed addresses for modifications
   }

   /**\todo TODO (2015-04-16) BEWARE OF THE PLT THAT IS CREATED IN THE LIBTROLL!!!! I HAVE NO CONTROL OVER IT
    * AND IT IS NOT IN THE HASHTABLES OF BRANCHES, I HAVE TO UPDATE ITS ADDRESS AND POINTERS
    * => (2015-05-04) Done below. But not sure it works well*/

   //int dataref_origcode = FALSE; //Stores if an added data is reached from the original code
   //Handles modifications that did not cause the instruction to be moved
   FOREACH_INQUEUE(pf->modifs, iterm) {
      modif_t* modif = GET_DATA_T(modif_t*, iterm);
      if (!(modif->annotate & A_MODIF_ERROR)
            && !(modif->annotate & A_MODIF_CANCEL)
            && !(modif->annotate & A_MODIF_APPLIED)) {
//         if (!insn_check_annotate(GET_DATA_T(insn_t*, modif->modifnode)), A_PATCHMOV) {
         /**\todo TODO (2014-12-22) Check if we test on the modif annotate or the modifnode being flagged as A_PATCHMOV
          * => Warning! We have to flag instructions indeed, but we may have also to handle the instructions whose modification did
          * not cause to be moved, but who had to be moved because belonging to a block moved by another modification. So, using the
          * state of the modif is a good thing to handle modifs that were not linked to a movedblock, but we may be able to do something
          * more clever for those (such as removing the padding for an instruction modified into something smaller)*/

         /**\todo TODO (2015-03-19) I need here to detect if such an instruction references an inserted global variable. If that is
          * the case, the section storing inserted data must be flagged as reachable by memory references, or possibly */
         iteri = modif->modifnode;
         //Flags the instructions targeted by the modification
         while (iteri && iteri != modif->modifnode->next) {
            //Retrieves instructions pointing to the modified instructions
            get_origin_branches(pf->branches, originbranches,
                  GET_DATA_T(insn_t*, iteri));
            //Retrieves data entries referencing the modified instructions
            queue_t* refs = hashtable_lookup_all(asmfile_get_insn_ptrs_by_target_data(pf->afile),
                  GET_DATA_T(insn_t*, iteri));
            if (refs) {
               //Appending refs to the list of data entries
               queue_append(references, refs);
               //lc_free(refs);
               /**\todo TODO (2014-12-18) Once and for all, decide whether queue_append frees the appended queue or not
                * => (2015-05-29) DECIDED.*/
            }
            //Annotates the instruction according to the modification targeting it
            insn_add_annotate(GET_DATA_T(insn_t*, iteri),
                  get_insnannotate_modiftype(modif->type));
            //Creates a patchinsn_t structure for the modified instruction
            patchfile_createpatchinsn(pf, GET_DATA_T(insn_t*, iteri),
                  queue_peek_head(modif->newinsns), NULL);
            /**\todo TODO (2015-01-08) In that particular case, we will have to look up the newinsn in the created patchinsn
             * and its sequence to check whether there are other instructions following it. This todo is mainly here to not forget it,
             * and also to try finding another, more streamlined mechanism, but I'm beginning to be fed up with the handling of patchinsn_t
             * and patchmodif_t. One solution would be to get rid of the patchinsn_t completely and only use patchmodif_t, but it is more
             * memory expensive. The other would be to *always* use this system, even for moved instructions
             * (in that case, patchfile_patchmodif_append would have to be modified)
             * => (2015-05-28) I think I took it into account in patchfile_codescn_genbytes_frominsns, but I don't like it very much.
             * At least I managed to remove the use of the patchmodif_t, which was too heavy*/

            /**\todo TODO (2015-03-19) Detect here if the instruction references a global variable (I'll have to check how I will be coding
             * that in libmadras). If that is so, set dataref_origcode to TRUE.
             * An improvement would be to detect WHICH data are accessed that way, and move them to a different list. This list is then added
             * to a different data section, with the SCNA_INSREF attribute set on it*/
            iteri = iteri->next;
         }
         modif->annotate |= A_MODIF_APPLIED;
      }
   }

   //Now duplicates entries for each data in the binary file referencing a modified instruction
   FOREACH_INQUEUE(references, iterr) {
      data_t* entry = GET_DATA_T(data_t*, iterr);
      //Creates a entry in the patched binary file
      data_t* patchentry = binfile_patch_get_entry_copy(pf->patchbin, entry);
      //Retrieve the patchinsn_t structure associated to the destination
      patchinsn_t* patchref = hashtable_lookup(pf->patchedinsns,
            pointer_get_insn_target(data_get_ref_ptr(entry)));
      assert(patchref);
      //Links the duplicated entry to the patched instruction
      pointer_set_insn_target(data_get_ref_ptr(patchentry), patchref->patched);
      //Stores the data in the table of references
      hashtable_insert(pf->datarefs, patchref->patched, patchentry);
      /**\todo TODO (2015-04-14) Right now I'm accessing this table only there, so there is no need to create
       * a function to factorise accesses like I did for patchfile_setbranch. If it happens that I store data
       * in this table elsewhere, I would need to create a patchfile_setdataref or something like that*/
   }
   queue_free(references, NULL);

   //Now handling global variables modifications
   //Reordering the queue of inserted data depending on their alignment
   queue_sort(pf->modifs_var, modifvar_cmpbyalign_qsort);

   //Queue of globvar_t structures representing global variables to be inserted and referenced by at least one instruction from the original code
   queue_t* newdata_insnref = queue_new();
   //Queue of glogvar_t structures representing global variables to be inserted and not referenced by an instruction from the original code
   queue_t* newdata = queue_new();

   //Inserting the global variables
   FOREACH_INQUEUE(pf->modifs_var, iterv) {
      /**\todo TODO (2014-11-14) Move this into a separate functions that will do the following steps:
       * - Compute the total size of data needed (don't forget to add the newstack if it is here)
       * - Retrieve the address at which we can insert them
       * - Create a temporary queue of globvar to add, ordered by alignment
       * - Extract a list of data to add, inserting new datas if needed to have gaps between variables to obey alignment
       * - This is the data_list queue, whose bytes we will directly inject later into the patched file
       * => (2015-05-11) Mucho more complicatedo, amigo. We need to affect the variables to different empty spaces depending
       * on how from where they are accessed: original code or moved code. I'm rather partial to putting variables accessed
       * only by instructions from a single moved block inside the block itself (I should have a mechanism for handling this,
       * but I need to add special data for alignment). And of course, take the alignment into account
       * => (2015-05-12) Doing that*/
      modifvar_t* modvar = GET_DATA_T(modifvar_t*, iterv);
      if (modvar->type == ADDGLOBVAR) {
         globvar_t* gv = modvar->data.newglobvar;
         //Insertion of new global variable: detecting from where it is referenced
         queue_t* refinsns = hashtable_lookup_all(pf->insnrefs, gv->data);
         queue_t* refblocks = queue_new(); //Queue of moved blocks containing an instruction referencing the data
         FOREACH_INQUEUE(refinsns, iterri) {
            if (!insn_check_annotate(GET_DATA_T(insn_t*, iterri), A_PATCHMOV)
                  && !insn_check_annotate(GET_DATA_T(insn_t*, iterri),
                        A_PATCHNEW)) {
               //The data is referenced by at least one instruction that has not been moved and is not new: it must be reachable from the original code
               DBGMSGLVL(1,
                     "Global variable %s (globvar_%d) is referenced by the instruction at address %#"PRIx64" in the original code\n",
                     gv->name, gv->globvar_id,
                     insn_get_addr(GET_DATA_T(insn_t*, iterri)));
               queue_add_tail(newdata_insnref, gv);
               break;  //No need to go on
            } else {
               //The data is referenced by a moved instruction: retrieving its block
               movedblock_t* mb = hashtable_lookup(pf->movedblocksbyinsns,
                     GET_DATA_T(insn_t*, iterri));
               assert(mb); //I'm not sure if this can legitimately happen. If it does, replace this with an if(mb)
               //Adds the block to the queue of block containing an instruction referencing the data
               queue_add_tail(refblocks, mb);
            }
         }
         if (!iterri && refinsns) {
            //We reached the end of the queue: this means we found no instruction from the original code referencing this data
            if (queue_length(refblocks) == 1) {
               /**\todo TODO (2015-05-13) If I keep the fixed movedblock mechanism, I have to check if the block is fixed and
                * not do this if it is (I can't have it change size afterward)*/
               //There is one single block referencing this data: we will add it to the list of data contained in the block
               DBGLVL(1,
                     FCTNAMEMSG("Global variable %s (globvar_%d) is referenced only by instructions from ", gv->name, gv->globvar_id); movedblock_fprint(GET_DATA_T(movedblock_t*, queue_iterator(refblocks)), stderr); STDMSG("\n");)
               queue_add_tail(
                     GET_DATA_T(movedblock_t*, queue_iterator(refblocks))->localdata,
                     gv);
            } else {
               //The data is referenced by instructions from more than one block: adding it to the list of general data
               queue_add_tail(newdata, gv);
            }
         }
         queue_free(refinsns, NULL);
         queue_free(refblocks, NULL);
      } else {
         assert(0);
         /**\todo (2015-05-12) If this assert fails, it means we added a new type of variable modification and need to handle it.
          * Depending on what this new type is, we may not have anything to do at this stage however (the way I see things, a new
          * type would mostly be about modifying a global variable, so either the modification will impact its content only and we
          * really won't have anything to do at this point, or it will also impact its size and then we will need to move it and
          * update all instructions referencing it accordingly (and it will most probably be in the queue of variables referenced by original code))*/
      }
   }
   /**\todo TODO (2015-05-12) Possible bug: I don't check that variables not referenced from the original code are affected to a section at a distance
    * from the displaced code allowing to reference it. I'm trying to reduce this by having data referenced by a single block be inside that block,
    * but we could have the case of a moved block accessing data shared by various blocks and that has been moved too far from those data to be referenced.
    * For handling this, we could either try to have blocks accessing the same data be grouped together and the data at the end, or update the assembly
    * code referencing these data to use a rebound or an absolute address, if the architecture allows it. This case will probably happen a friday evening.
    * Good luck!
    * */
   //Inserting variables referenced by original code
   if (queue_length(newdata_insnref) > 0) {
      res = patchfile_createdatascns(pf, newdata_insnref, INTERVAL_REFERENCE,
            SCNA_INSREF);
      UPDATE_ERRORCODE(out, res)
      /**\todo (2015-05-13) As of now, I don't need to flag the section as SCNA_INSREF any more, as it is already relocated and we don't
       * need to ask that to the libbin. However I keep it for coherence and maybe I'm wrong and I will end up needing it after all. You never know*/
   }
   queue_free(newdata_insnref, NULL);
   /**\todo TODO (2015-03-20) We will have to insert here some computations by the libbin and the format-specific code for the handling
    * of static patching. In ELF at least, static patching inserts some entries into the .got section, which is referenced by instructions,
    * so this step has be done here before reserving the space for the referenced sections
    * In short, ADD HERE ALL THE STUFF ABOUT THE STATIC PATCHING AND INVOKE A binfile FUNCTION (with underlying driver invocation) TO DO IT*/

   //Reserves space for sections referenced by instructions
   res = patchfile_reserveemptyspaces_refscns(pf);
   if (ISERROR(res)) {
      ERRMSG("Unable to move sections referenced by instructions\n");
      return res;
   }
   UPDATE_ERRORCODE(out, res)

   //Now inserting other variables
   if (queue_length(newdata) > 0) {
      //Resetting all remaining empty spaces to be reserved by no one
      patchfile_reserveemptyspaces(pf, INTERVAL_NOFLAG, TRUE, UINT64_MAX);

      //Now fuse all contiguous spaces containing variables and creates a section for each of them
      res = patchfile_createdatascns(pf, newdata, INTERVAL_NOFLAG, SCNA_NONE);
      UPDATE_ERRORCODE(out, res)
   }
   queue_free(newdata, NULL);

   //Now the empty space necessary to store modified or added sections referenced by original code has been reserved
   DBGMSG0("Reserving all remaining space for moving code sections\n");
   //Updating all remaining space that was reserved for sections reachable by references to reserve them for code
   pf->availsz_codedirect = patchfile_reserveemptyspaces(pf,
         INTERVAL_DIRECTBRANCH, TRUE, UINT64_MAX);
   DBGMSG("New available size for moving code sections is %"PRIu64"\n",
         pf->availsz_codedirect);

   //Now merging all empty spaces
   list_t* iteres = queue_iterator(pf->emptyspaces);
   while (iteres) {
      interval_t* space = GET_DATA_T(interval_t*, iteres);
      if (patcher_interval_getused(space) != INTERVAL_NOFLAG) {
         //Used interval: skipping to the next
         iteres = iteres->next;
         continue;
      }
      while (iteres->next
            && patcher_interval_getreserved(space)
                  == patcher_interval_getreserved(
                        GET_DATA_T(interval_t*, iteres->next))
            && interval_merge(space, GET_DATA_T(interval_t*, iteres->next))) {
         //This interval and the next are reserved for the same object and could be successfully merged: deleting the next one
         patcher_interval_free(queue_remove_elt(pf->emptyspaces, iteres->next));
      }
      //Skipping to the next interval (if there is one remaining)
      iteres = list_getnext(iteres);
   }

   DBGLVL(1,
         FCTNAMEMSG0("Empty intervals now are: \n"); FOREACH_INQUEUE(pf->emptyspaces, __iteres) {STDMSG("\t"); patcher_interval_fprint(GET_DATA_T(interval_t*, __iteres), stderr); STDMSG("\n");})

   //Attempting to find spaces for the moved blocks
   /**\todo TODO (2015-03-19) Split the code from patchfile_movedblocks_finalise to factorise it differently, and move this modification in one
    * of the loops. Or, if we get rid of the fixed moved blocks and invoke patchfile_movedblocks_finalise only once, move this into the function*/
   FOREACH_INQUEUE(pf->movedblocks, iter_findmbsp) {
      movedblock_t* mb = GET_DATA_T(movedblock_t*, iter_findmbsp);
      //First, recompute the size of the block, taking into account its instructions and data
      movedblock_computesize(pf, mb);
      //Resetting the jump type of the block to use the new freed space
      mb->jumptype = patchfile_findjumptype(pf, NULL, FALSE);
      res = movedblock_findspace(pf, mb);
      if (ISERROR(res)) {
         ERRMSG("Unable to relocate block starting at address %#"PRIx64"\n",
               insn_get_addr(GET_DATA_T(insn_t*, mb->firstinsn)));
         /**\todo TODO (2015-03-19) Add some handling here of the movedblock, flagging it at erroneous or something*/
      }
      UPDATE_ERRORCODE(out, res)
   }

   //Reordering movedblock by address (this is purely for comfort when debugging the patched file)
   queue_sort(pf->movedblocks, movedblock_cmporigaddr_qsort);
   /**\todo TODO (2015-06-01) Check if we need to do it twice (we also do it at the beginning of the function) or just once*/

   //Now creates the branch instructions replacing the moved blocks, including those for trampolines
   /**\todo TODO (2015-04-15) I'm doing this in a second pass because it's easier to have all moved blocks being finalised.
    * To be seen whether we could do it in the first pass, but recursively finalising blocks that are either used for trampolines
    * or that use another block as trampoline, but for that I have to be sure that I will not be using the fixed/unfixed thing any more
    * (because I order modifications in the non fixed but not in the fixed blocks)*/
   FOREACH_INQUEUE(pf->movedblocks, itermb2) {
      movedblock_t* mb = GET_DATA_T(movedblock_t*, itermb2);
      movedblock_createbranches(pf, mb);
   }

   //Updates the coding and addresses of moved blocks and creates instructions for returning from them
   patchfile_movedblocks_finalise(pf, pf->movedblocks);
   patchfile_movedblocks_finalise(pf, pf->fix_movedblocks);
   //   //Generates the binary code for the blocks ==> On doit pouvoir faire ça plus tard, j'ai juste besoin de la taille
   //   patchfile_movedblock_gencode(pf, mb);

   //Now generates patchinsn_t structures for all branch instructions targeting a modified instruction
   DBGMSG0(
         "Creating patchinsn_t structure for all branch instructions targeting a modified instruction\n")
   FOREACH_INQUEUE(originbranches, itero) {
      insn_t* originbranch = GET_DATA_T(insn_t*, itero);
      insn_t* origindest = insn_get_branch(originbranch);
//      //Flags the instruction as being updated because of the patched operation
//      insn_add_annotate(originbranch, A_PATCHUPD);
//      //Creates a patchinsn_t structure duplicating the branch instruction
//      patchinsn_t* patchbranch = patchfile_createpatchinsn(pf, originbranch, originbranch, NULL);
      //Retrieve the patchinsn_t structure associated to the destination
      patchinsn_t* patchdest = hashtable_lookup(pf->patchedinsns, origindest);
      assert(patchdest);
      if (!insn_check_annotate(originbranch, A_PATCHMOV)) {
         //Handling the case where this branch has not been moved
         movedblock_t* mb = hashtable_lookup(pf->movedblocksbyinsns,
               origindest);
         if (mb) {
            //The destination belongs to a moved block
            assert(origindest == GET_DATA_T(insn_t*, mb->firstinsn)); //We should not have moved a branch destination in the middle of a block
            //Linking the duplicated branch instruction to the branch jumping to the displaced block
            patchfile_createpatchbranch(pf, originbranch,
                  queue_peek_head(mb->newinsns));
            continue;
            /**\todo TODO (2015-05-13) Firstly, in this particular case, we could simply abstain of creating a patchinsn for the origin
             * branch, as its offset should not change (it would jump to an instruction that replaced its original target). Secondly,
             * we could try to see if the branch could be updated to point to the moved block (i.e. it does not change size), and only
             * create this link if it can't. But for now I'm keeping it to be coherent with everything else and work as before*/
         } //Otherwise the destination has not been moved and we don't need to update it
      } else {
         //Origin branch has been moved: we can update it however we want
         //Checks if there has been insertions before the destination
         queue_t* insertsbefore = hashtable_lookup_all(pf->insnbeforemodifs,
               origindest);
         if (insertsbefore) {
            //There has been insertions before the destination
            queue_sort(insertsbefore, modif_cmp_qsort);
            list_t* itermodifb = queue_iterator(insertsbefore);
            while (itermodifb) {
               modif_t* modifb4 = GET_DATA_T(modif_t*, itermodifb);
               if (!modif_hasbranchupd_restrictions(modifb4)
                     || ((modifb4->flags & PATCHFLAG_INSERT_NO_UPD_FROMFCT)
                           && (!insns_samefct(originbranch, origindest)))
                     || ((modifb4->flags & PATCHFLAG_INSERT_NO_UPD_OUTFCT)
                           && (!insns_samefct(originbranch, origindest)))
                     || ((modifb4->flags & PATCHFLAG_INSERT_NO_UPD_FROMLOOP)
                           && (!insns_sameloop(originbranch, origindest)))) {
                  /*Modification has either no restrictions on the branches,
                   * or branches from the same function must not be updated and the branch belongs to another function,
                   * or branches from others functions must not be updated and the branch belongs to the same function,
                   * or branches from the same loop must not be updated and the branch does not belong to the same loop:
                   * updating branches pointing to the instruction to point to the first added instruction*/
                  patchfile_createpatchbranch(pf, originbranch,
                        queue_peek_head(modifb4->newinsns));
                  break;   //Found where to link the branch: exiting
               }
               itermodifb = itermodifb->next;
            }
            if (itermodifb) {
               //We were able to link the original branch to one of the insertion before: no need to continue with it
               queue_free(insertsbefore, NULL);
               continue;
            } //Otherwise the branch could not be linked to any of the insertions before the instruction
            queue_free(insertsbefore, NULL);
         }  //Otherwise there are no insertions before this instruction
         //Either this instruction has no insertion before it, or the branch could not be linked to either of them
         //Finds the first non-null instruction following the patched instruction
         list_t* iterpidest = patchdest->seq;
         while (iterpidest && !GET_DATA_T(patchinsn_t*, iterpidest)->patched)
            iterpidest = iterpidest->next;
         assert(iterpidest); //At the very least, we could encounter the return branch from the moved block
         //Links the duplicated branch instruction to the patched instruction
         patchfile_createpatchbranch(pf, originbranch,
               GET_DATA_T(patchinsn_t*, iterpidest)->patched);
         /**\todo TODO (2015-04-15) Add a test here to see if the destination is an unconditional branch (not call), and link
          * to the destination if it is. But check if it has to always be the case first*/
      }
   }
   queue_free(originbranches, NULL);
   //uint64_t fix_movedcodesz;

   //Recompute the code and addresses in the moved blocks
   patchfile_movedblocks_updateaddresses(pf, pf->movedblocks);
   patchfile_movedblocks_updateaddresses(pf, pf->fix_movedblocks);

   //Building the queue of intervals containing moved blocks and removing all used intervals from the list
   queue_t* movedspaces = queue_new();
   iteres = queue_iterator(pf->emptyspaces);
   while (iteres) {
      uint8_t usedflag = patcher_interval_getused(
            GET_DATA_T(interval_t*, iteres));
      if (usedflag != INTERVAL_NOFLAG) {
         //This interval has been marked as used: removing it from the list of empty spaces
         list_t* nextiter = iteres->next;
         interval_t* used = queue_remove_elt(pf->emptyspaces, iteres);
         iteres = nextiter; //Resuming the scan at the next element after the one we removed
         if ((usedflag == INTERVAL_DIRECTBRANCH)
               || (usedflag == INTERVAL_INDIRECTBRANCH))
            queue_add_tail(movedspaces, used); //Storing it in the queue of intervals containing moved code
         else
            patcher_interval_free(used); //Deleting the interval (used for storing sections containing references)
      } else
         iteres = iteres->next;
   }
   /**\todo (2015-03-19) I'm using only one queue for both types of intervals as there does not seem to be any need
    * to separate code reached by direct and indirect branches - by design they should not be contiguous and even
    * if they are it should not cause problems. But if it does cause problems on a friday afternoon, we would have
    * to build two different queues.*/

   //Creates new code sections
   if (queue_length(movedspaces) > 0) {
      //There are spaces containing moved code
      //Creating a section for each set of contiguous spaces containing moved blocks
      uint64_t movedcodesz = 0;
      int64_t movedcodeaddr = interval_get_addr(queue_peek_head(movedspaces));
      binscn_t* newscn = binfile_patch_add_code_scn(pf->patchbin, NULL,
            movedcodeaddr, movedcodesz);
      FOREACH_INQUEUE(movedspaces, iterms)
      {
         interval_t* ms = GET_DATA_T(interval_t*, iterms);
         if (iterms->prev
               && (interval_get_addr(ms)
                     > interval_get_end_addr(
                           GET_DATA_T(interval_t*, iterms->prev)))) {
            //The previous interval is not contiguous to the current one
            //Creating a new code section encompassing all previous contiguous intervals
            newscn = binfile_patch_add_code_scn(pf->patchbin, NULL, movedcodeaddr,
                  movedcodesz);
            //Now resetting the address of the beginning of the block of contiguous code and its size
            movedcodeaddr = interval_get_addr(ms);
            movedcodesz = 0;
            //Associating the code to the section
         }
         //Associates the movedblocks moved to this interval with the new section
         queue_t* mbs = interval_get_data(ms);
         FOREACH_INQUEUE(mbs, iterimbs) {
            GET_DATA_T(movedblock_t*, iterimbs)->newscn = newscn;
            hashtable_insert(pf->movedblocksbyscn, newscn,
                  GET_DATA_T(movedblock_t*, iterimbs));
            //Creates labels for the variables stored in the block
            FOREACH_INQUEUE(GET_DATA_T(movedblock_t*, iterimbs)->localdata,
                  itergvmb) {
               patchfile_addlabel_forglobvar(pf,
                     GET_DATA_T(globvar_t*, itergvmb), newscn);
            }
            //Create a label for the block
            patchfile_addlabel_formovedblock(pf,
                  GET_DATA_T(movedblock_t*, iterimbs), newscn);
         }
         //Increasing the size of contiguous code
         movedcodesz += interval_get_size(ms);
         //Updating the section
         binscn_set_size(newscn, movedcodesz);
      }
//      //Creating a section for the last block of contiguous code
//      newscn = binfile_patch_add_code_scn(pf->patchbin, movedcodeaddr, movedcodesz);
   }
   queue_free(movedspaces, patcher_interval_free);
//      //Section containing moved blocks at a fixed address
//      if (queue_length(pf->fix_movedblocks) > 0)
//         binfile_patch_add_code_scn_fixed_addr(pf->patchbin, ((movedblock_t*)queue_peek_head(pf->fix_movedblocks))->newfirstaddr, fix_movedcodesz);
//      /**\todo TODO [DONE] (2014-12-11) The name has to be removed from the binfile_patch_add_code_scn prototype, which will then invoke a driver
//       * function to get the name of the section*/
//      //Section containing moved blocks
//      if (queue_length(pf->movedblocks) > 0)
//         binfile_patch_add_code_scn(pf->patchbin, ((movedblock_t*)queue_peek_head(pf->movedblocks))->newfirstaddr, movedcodesz);
//      /**\todo TODO [DONE] (2014-12-11) The name has to be removed from the binfile_patch_add_code_scn prototype, which will then invoke a driver
//       * function to get the name of the section*/
//      //Generates the code for the new sections => À faire plus tard, là j'ai juste besoin de la taill
//      //patchfile_updatesscncode(pf);

/////////////////Copied from patchfile_patch. To be adapted
   /**\todo TODO (2015-03-05) Here we add a label requests for modifications depending on settings chosen in libmadras. The options we can allow
    * would be: 1) No additional labels (or one at the beginning of each moved section) 2) One label at the beginning of each displaced block
    * (named madras_fctname_addr) 3) One additional label at the beginning of each code insertion (named madras_modif_xx)
    * and at the site of deletions or modification*/

   //Performing the label modification requests (we have to do them now as the instructions are in their final place)
   FOREACH_INQUEUE(pf->modifs_lbl, iterlb) {
      modiflbl_apply(pf, GET_DATA_T(modiflbl_t*, iterlb));
   }
/////////////////// End of code copied from patchfile_patch. To be adapted

   //Finalises the binary file (this will fix the section addresses and reorder)
   res = binfile_patch_finalise(pf->patchbin, pf->emptyspaces);
   if (ISERROR(res)) {
      ERRMSG("Unable to finalise patched file\n");
      return res;
   }
   UPDATE_ERRORCODE(out, res)

   /**\todo TODO (2015-05-05) We could allow binfile_patch_finalise to change the position of some sections (if we reserved
    * sizes too big for the referenced sections and the like). If we do, we would need at this point to recompute the addresses
    * of all movedblocks and the branches used to reach and leave them.
    * This would have to be made in two stages: first, the libbin (and format-specific functions) would relocate the modified
    * binary sections. Then we would check if the there are remaining intervals reachable with references and branches
    * where we could move the code and data section respectively, and update their addresses accordingly. Then invoke the remaining
    * processes from binfile_patch_finalise*/

   /**\todo TODO (2015-03-04) THIS TODO IS IMPORTANT!!!!
    * We also have to create patchinsn_t structure for EVERY data entry that had been duplicated in binfile_patch_get_scn_entrycopy
    * I don't see an easy way to do this apart from scanning each entry of the patched binfile and look up in asmf->datarefs if there is an instruction
    * referencing its original, and creating a patchinsn_t structure with the copy pointing to the new entry. And that would be an horrible bottleneck
    * BUT IT MUST BE DONE
    * => (2015-04-13) Doing it below, but I fear it's a horrible bottleneck*/
   DBGMSG0(
         "Creating patchinsn_t structures for all instructions referencing an entry\n");
   //Creating patchinsn_t structures for all instructions referencing an entry in the binary file (they have all been duplicated during finalise)
   FOREACH_INHASHTABLE(asmfile_get_insn_ptrs_by_target_data(pf->afile), iterh) {
      data_t* copy = binfile_patch_get_entry_copy(pf->patchbin,
            GET_KEY(data_t*, iterh));
      /**\todo TODO (2015-04-14) I have to use here a different function that only returns the copy if it exists (instead of creating
       * it if it does not)
       * => (2015-04-21) Done by tweaking binfile_patch_get_entry_copy so that it does not create the copy if the file has been finalised*/
      if (copy) {
         //Flags the instruction as being updated because of the patched operation
         insn_add_annotate(GET_DATA_T(insn_t*, iterh), A_PATCHUPD);
         //The referenced entry has been duplicated: creating a patchinsn_t referencing the copy
         patchinsn_t* piref = patchfile_createpatchinsn(pf,
               GET_DATA_T(insn_t*, iterh), GET_DATA_T(insn_t*, iterh), NULL);
         /**\todo TODO (2015-04-14) WARNING! I'm creating some patchinsn_t structures here, so I should also duplicate the branches or data
          * referencing them. See how to factorise that, or move this whole block above before we duplicate the structures referencing these instructions*/
         oprnd_t* refop = insn_lookup_ref_oprnd(piref->patched);
         assert(refop && oprnd_get_type(refop) == OT_MEMORY_RELATIVE);
         //Links the copy of the instruction to the copy of the data entry
         pointer_t* ptr = oprnd_get_refptr(refop);
         pointer_set_data_target(ptr, copy);
         /**\todo TODO (2015-04-13) Check if it is interesting to store this in the insnrefs table of the patched file
          * => (2015-04-14) Removing it, and directly updating the pointer. To be restored if we find out we must do that
          * later
          * => (2015-05-04) Now restoring it, so that we can update everything at once afterwards*/
         hashtable_insert(pf->insnrefs, copy, piref->patched);
         //pf->arch->oprnd_updptr(piref->patched, ptr); /**\todo TODO (2015-04-14) Beware of interworking. It should not strike there, but who knows...*/
         //insn_oprnd_updptr(piref->patched, refop);
      }
   }

   /**\todo TODO (2015-04-16) BEWARE OF THE PLT THAT IS CREATED IN THE LIBTROLL!!!! I HAVE NO CONTROL OVER IT
    * AND IT IS NOT IN THE HASHTABLES OF BRANCHES, I HAVE TO UPDATE ITS ADDRESS AND POINTERS
    * => (2015-04-30) Doing it below. Factorise the code I use because I'm pretty sure I used some of it elsewhere.
    * Also, I basically reused insnlist_updaddresses here, so find some way to factorise the code*/
   uint16_t i;

   DBGMSG0(
         "Updating the addresses of all sections not created by the patcher\n");
   //Updates the addresses of all sections that were not created by the patcher (for instance in the libbin)
   for (i = 0; i < binfile_get_nb_load_scns(pf->patchbin); i++) {
      binscn_t* scn = binfile_get_load_scn(pf->patchbin, i);
      if (binscn_patch_get_type(scn) == SCNT_CODE) {
         list_t* firstinsnseq = binscn_patch_get_first_insn_seq(scn);
         if (firstinsnseq
               && insn_get_addr(GET_DATA_T(insn_t*, firstinsnseq))
                     == ADDRESS_ERROR) {
            DBGMSGLVL(1,
                  "Updating addresses of section %s that was not created by the patcher\n",
                  binscn_get_name(scn));
            list_t* iterinscn = firstinsnseq;
            list_t* lastseq = binscn_patch_get_last_insn_seq(scn);
            int64_t addr = binscn_get_addr(scn);
            while (iterinscn != lastseq->next) {
               insn_t* insn = GET_DATA_T(insn_t*, iterinscn);
               insn_set_addr(insn, addr);
               addr += insn_get_bytesize(insn);
               //Detecting branches
               oprnd_t* refop = insn_lookup_ref_oprnd(insn);
               if (refop) {
                  pointer_t* ptr = oprnd_get_refptr(refop);
                  switch (pointer_get_target_type(ptr)) {
                  case TARGET_INSN:
                     DBGMSGLVL(2,
                           "Branch found at address %#"PRIx64" in section %s\n",
                           insn_get_addr(insn), binscn_get_name(scn))
                     ;
                     hashtable_insert(pf->newbranches,
                           pointer_get_insn_target(ptr), insn);
                     break;
                  case TARGET_DATA:
                     DBGMSGLVL(2,
                           "Data reference found at address %#"PRIx64" in section %s\n",
                           insn_get_addr(insn), binscn_get_name(scn))
                     ;
                     hashtable_insert(pf->insnrefs, pointer_get_data_target(ptr),
                           insn);
                     break;
                  }
               }
               iterinscn = iterinscn->next;
            }
         }
      }
   }
   /**\todo TODO (2015-05-04) We may not need hashtables for storing references and branches. Maybe simple queues would be enough.
    * Check when the refactoring is finalised how often we performs lookups into newbranches, datarefs and insnrefs...*/

   //Now updates all pointers
   /**\todo TODO (2015-04-15) Factorise this into a separate function, I have got the feeling I will need it more than once*/
   DBGMSG0("Updating the codings of instructions replacing moved blocks\n");
   //Now updating the coding of the new instructions of all moved blocks (has to be done now because of trampolines)
   FOREACH_INQUEUE(pf->movedblocks, itermb4upd) {
      movedblock_t* mb = GET_DATA_T(movedblock_t*, itermb4upd);
      FOREACH_INQUEUE(mb->newinsns, iterni)
      {
         insn_t* in = GET_DATA_T(insn_t*, iterni);
         oprnd_t* refop = insn_lookup_ref_oprnd(in);
         if (refop) {
            //Update pointer address
            insn_oprnd_updptr(in, refop);
            //Update instruction coding
            upd_assemble_insn(in, pf->asmbldriver, FALSE, NULL);
         }
      }
   }
   //Updating branches
   DBGMSG0("Updating all branches\n");
   {
      FOREACH_INHASHTABLE(pf->newbranches, iterb)
      {
         insn_t* branch = GET_DATA_T(insn_t*, iterb);
         oprnd_t* refop = insn_lookup_ref_oprnd(branch);
         assert(refop && oprnd_get_type(refop) == OT_POINTER);
         //Updates the pointer address
         insn_oprnd_updptr(branch, refop);
         //Updates the instruction coding
         upd_assemble_insn(branch, pf->asmbldriver,
               insn_check_annotate(branch, A_PATCHMOV), NULL);
      }
   }
   //Updating data references
   DBGMSG0("Updating all data referencing instructions\n");
   {
      FOREACH_INHASHTABLE(pf->datarefs, iterd)
      {
         data_t* data = GET_DATA_T(data_t*, iterd);
         pointer_t* ptr = data_get_ref_ptr(data);
         //Updates the pointer address
         pf->arch->oprnd_updptr(NULL, ptr); /**\todo TODO (2015-04-14) Beware of interworking. It should not strike there, but who knows...*/
      }
   }
   //Updating insn references
   DBGMSG0("Updating all instructions referencing data\n");
   {
      FOREACH_INHASHTABLE(pf->insnrefs, iteri)
      {
         insn_t* insnref = GET_DATA_T(insn_t*, iteri);
         oprnd_t* refop = insn_lookup_ref_oprnd(insnref);
         assert(refop && oprnd_get_type(refop) == OT_MEMORY_RELATIVE);
         //Updates the pointer address
         insn_oprnd_updptr(insnref, refop);
         //Updates the instruction coding
         upd_assemble_insn(insnref, pf->asmbldriver, FALSE, NULL);
      }
   }

   /**\todo TODO (2015-04-14) As everywhere else, factorise this by moving the horrible blobs of code below into clean little separate functions,
    * with some pink flowers and unicorns dancing around.*/
   //Regenerates the coding for all code sections and moved blocks
   //First, generates the code for all code and data sections containing instructions
   for (i = 0; i < binfile_get_nb_load_scns(pf->patchbin); i++) {
      binscn_t* scn = binfile_get_load_scn(pf->patchbin, i);
      if (binscn_patch_get_type(scn) == SCNT_CODE) {
         /**\todo TODO (2015-04-14) I have to test the first instruction because so far, the new sections I create for moved code
          * do not contain pointers to the instructions, as the movedblock_t contain patchinsn_t structures and I did not create
          * lists of new instructions. So here I handle existing code sections or code sections that have been added by the libbin
          * (e.g. the .plt for ELF x86_64). In the future, I could actually create list_t containers for the patchinsn_t->patched
          * instructions, and link them to the new code sections
          * => Ok scratch that I will fill the sections first
          * => Or not. It would be horribly memory-consuming. I will write this part first and see if I can fill the bytes
          * of the other code sections without too much trouble.*/
         if (binscn_patch_get_first_insn_seq(scn)) {
            //Section contains instructions: rebuilding its binary code
            res = patchfile_codescn_genbytes_frominsns(pf, scn);
            UPDATE_ERRORCODE(out, res)
         } else {
            //This is one of the sections that contains movedblocks, and to which we have not associated instructions
            patchfile_codescn_genbytes_fromblocks(pf, scn);
         }

      } else if ((binscn_patch_get_type(scn) == SCNT_DATA)
            && (binscn_patch_is_new(scn))) {
         /**\todo TODO (2015-04-14) WARNING! If at some point in the future we allow to modify existing data sections (for instance to
          * change the value of an existing global variable) we have to change the test above and use something else than binscn_patch_is_new.
          * We would probably need to add a new SCNA_x flag signaling that the content of the section is modified, and set it when we modify
          * an existing data_t structure.
          * Also, be careful if we create sections containing tables of instruction addresses (for indirect jumps) to not flag them as
          * SCNT_REFS, or simply remove the test on the section type. Or whatever, I'm fed up. It will never work anyway and it's a gas factory.*/
         //Section is a data section added by a patching operation (otherwise it will be handled by the libbin, at least I hope)
         DBGMSG(
               "Generating binary code for new data section %s from the entries it contains\n",
               binscn_get_name(scn));
         res = binscn_patch_set_data_from_entries(scn);
         if (ISERROR(res)) {
            WRNMSG(
                  "At least one error occurred when generating the code of section %s.\n",
                  binscn_get_name(scn));
         }
         UPDATE_ERRORCODE(out, res)
      }
   }
   //Updates the assembly and binary code of the whole patched file, recomputing addresses and reassembling to take libbin reordering into account
   //patchfile_updatecode(pf);

   return out;
}
/**\todo TODO (2015-04-15) At the end, I have to remove all flags on the instructions from the original file (A_PATCH*)
 * */
