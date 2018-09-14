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

/*
 * Defines functions shared between generate_metafile.c and deprecated.c
 * Remark: old code, never refactored
 */

#include <stdlib.h> // qsort
#include <sys/types.h> // stat
#include <sys/stat.h> // stat
#include <unistd.h> // stat and readlink

#include "libmadras.h" // madras_get_file_dynamic_libraries
#include "libmcommon.h" // lc_calloc...
#include "libmasm.h" // block_t...
#include "binary_format.h" // lprof_loop_t...

/* Compares 2 blocks by their address (of first instruction)
 * Allow qsort to sort blocks by increasing address */
static int cmp_block (const void *b1, const void *b2)
{
   const block_t* block1 = *((block_t**)b1);
   const block_t* block2 = *((block_t**)b2);
   const insn_t*  insn1 = NULL;
   const insn_t*  insn2 = NULL;

   if (block1->begin_sequence != NULL)
      insn1 = block1->begin_sequence->data;
   else
      return 0;
   if (block1->begin_sequence != NULL)
      insn2 = block2->begin_sequence->data;
   else
      return 1;

   return (insn1->address-insn2->address);
}

/* Sets block ranges info to a lprof loop (lprof_loop_t) from basic blocks
 * Ranges are contiguous blocks sequences */
unsigned loop_get_ranges(loop_t* l, lprof_loop_t* lprofLoop)
{
   unsigned blockIdx = 0;
   unsigned nbBlocks = l->blocks->length;
   block_t** blockRanges = lc_calloc( nbBlocks,sizeof(*blockRanges));
   FOREACH_INQUEUE (l->blocks,biter)
   {
      block_t* b = GET_DATA_T(block_t*,biter);
      if ( block_get_loop(b)->global_id == l->global_id)
      {
         blockRanges[blockIdx] = b;
         blockIdx++;
      }

   }
   nbBlocks = blockIdx;
   lprofLoop->nbBlocks = nbBlocks;
   lprofLoop->blockIds = lc_malloc (nbBlocks * sizeof(*lprofLoop->blockIds));

   //fprintf(stderr,"NB BLOCKS = %u\n", nbBlocks);
   qsort (blockRanges, nbBlocks, sizeof (*blockRanges), cmp_block);

   uint64_t startAddr, stopAddr;
   insn_t* startInsn, *stopInsn, *nextStartInsn, *nextStopInsn, *nextInsn;
   unsigned partIdx = 0;

   lprofLoop->startAddress = NULL;
   lprofLoop->stopAddress  = NULL;
   if (nbBlocks >= 1)
   {

      startInsn = blockRanges[0]->begin_sequence->data;
      stopInsn  = blockRanges[0]->end_sequence->data;
      startAddr = startInsn->address;
      stopAddr = stopInsn->address;

      lprofLoop->blockIds[0].startAddress  = startAddr;
      lprofLoop->blockIds[0].stopAddress   = stopAddr;
      lprofLoop->blockIds[0].blockId      = blockRanges[0]->global_id;

      for (blockIdx =1; blockIdx < nbBlocks; blockIdx++)
      {
         lprofLoop->blockIds[blockIdx].startAddress  = startAddr;
         lprofLoop->blockIds[blockIdx].stopAddress   = stopAddr;
         lprofLoop->blockIds[blockIdx].blockId      = blockRanges[blockIdx]->global_id;

         nextStartInsn = blockRanges[blockIdx]->begin_sequence->data;
         nextStopInsn = blockRanges[blockIdx]->end_sequence->data;
         nextInsn = GET_DATA_T(insn_t*, (insn_get_sequence(stopInsn))->next);
         if (nextInsn->address != nextStartInsn->address)
         {
            //fprintf(stderr,"NOT CONTIGUOUS\n");
            //fprintf(stderr,"RANGE CREATION : %p - %p\n",startAddr,stopAddr);
            //TODO : CHECK REALLOC
            lprofLoop->startAddress = lc_realloc(lprofLoop->startAddress, (partIdx+1) * sizeof (uint64_t));
            lprofLoop->stopAddress  = lc_realloc(lprofLoop->stopAddress,  (partIdx+1) * sizeof(uint64_t));
            *(lprofLoop->startAddress +partIdx) = startAddr;
            *(lprofLoop->stopAddress + partIdx)  = stopAddr;

            startInsn = nextStartInsn;
            stopInsn =  nextStopInsn;
            startAddr = startInsn->address;
            stopAddr = stopInsn->address;
            partIdx++;
         }
         else
         {
            //fprintf(stderr,"CONTIGUOUS\n");
            stopInsn =  nextStopInsn;
            stopAddr = stopInsn->address;
         }
      }
      //fprintf(stderr,"FINAL RANGE CREATION : %p - %p\n",startAddr,stopAddr);
      //TODO : CHECK REALLOC
      lprofLoop->startAddress = lc_realloc(lprofLoop->startAddress, (partIdx+1) * sizeof (uint64_t));
      lprofLoop->stopAddress  = lc_realloc(lprofLoop->stopAddress,  (partIdx+1) * sizeof(uint64_t));
      *(lprofLoop->startAddress +partIdx) = startAddr;
      *(lprofLoop->stopAddress + partIdx)  = stopAddr;
      partIdx++;
   }
   //fprintf(stderr,"NB PARTS = %u\n",partIdx);
   free(blockRanges);
   lprofLoop->nbParts = partIdx;
   return partIdx;
}

static int loop_is_outermost (loop_t* loop)
{
   assert (loop != NULL);
   if (loop_get_parent_node (loop) == NULL)
      return TRUE;
   return FALSE;
}

/* Returns loop hierarchical level:
 * - SINGLE_LOOP (no parent + no children)
 * - INNERMOST_LOOP (parent + no children)
 * - OUTERMOST_LOOP (no parent + children)
 * - INBETWEEN_LOOP (parent + children) */
int get_loop_level (loop_t* loop)
{
   assert (loop != NULL);
   if (loop_is_innermost(loop) == TRUE && loop_is_outermost(loop))
      return SINGLE_LOOP;
   if (loop_is_innermost(loop) == TRUE)
      return INNERMOST_LOOP;
   if (loop_is_outermost(loop) == TRUE)
      return OUTERMOST_LOOP;

   //LAST CASE
   return INBETWEEN_LOOP;
}

/* Returns children loops, as an array of IDs */
unsigned loop_get_children (loop_t* l, uint32_t** childrenList)
{
   unsigned nbChildren = 0;
   tree_t *children = loop_get_children_node (l);
   *childrenList = NULL;

   if (children != NULL)
   {
      tree_t *iter;

      for (iter = children; iter != NULL; iter = iter->next)
      {
         loop_t *loop = tree_getdata (iter);
         //TODO:check lc_realloc with tmp ptr
         *childrenList = lc_realloc (*childrenList, (nbChildren+1) * sizeof(uint32_t));
         *(*childrenList + nbChildren) = loop_get_id(loop);
         nbChildren++;
      }
   }
   return nbChildren;
}

// From LD_LIBRARY_PATH, retrieve in which directory a given dynamic library will be loaded
static char *find_library_from_env (const char *lib)
{
   // Get LD_LIBRARY_PATH value
   char *ld_lib_str = lc_strdup (getenv ("LD_LIBRARY_PATH"));

   // Iterate over LD_LIBRARY_PATH paths
   char *sub_ld_lib = NULL;
   while ((sub_ld_lib = strtok (sub_ld_lib ? NULL : ld_lib_str, ":")) != NULL) {
      // Build full path (path + basename)
      char *file_path = lc_malloc (strlen (sub_ld_lib) + strlen (lib) + 2);
      sprintf (file_path, "%s/%s", sub_ld_lib, lib);

      struct stat st_buf;
      if (stat (file_path, &st_buf) == 0) { // If exists
         char *ret = lc_strdup (sub_ld_lib);
         lc_free (ld_lib_str);
         lc_free (file_path);
         return ret; // Return path in which lib were found
      }
      
      lc_free (file_path);
   }

   lc_free (ld_lib_str);
   return NULL;
} 

// Return target (basename) for symlink path/name
static char *get_target (const char *path, const char *name)
{
   // Build full path (path + basename)
   char *full_path = lc_malloc (strlen (path) + strlen (name) + 2);
   sprintf (full_path, "%s/%s", path, name);

   // Check if symbolic link
   struct stat st_buf;
   if (lstat (full_path, &st_buf) == -1) {
      lc_free (full_path);
      return NULL;
   }

   // Get target name
   char *target = lc_malloc (st_buf.st_size + 1);
   ssize_t r = readlink (full_path, target, st_buf.st_size + 1);
   lc_free (full_path);
   if (r == -1) {
      lc_free (target);
      return NULL;
   }

   // Write final '\0' (not written by readlink)
   target [r] = '\0';

   return target;
}

// Returns physical (target) to symbolic (link) index
hashtable_t *get_phy2sym (const char *exe_name)
{
   hashtable_t *phy2sym = hashtable_new (str_hash, str_equal);

   // Get dynamic libraries referenced by executable (as read by ldd)
   queue_t *dyn_libs = madras_get_file_dynamic_libraries ((char *) exe_name);

   // For each referenced dynamic library
   FOREACH_INQUEUE (dyn_libs, dyn_libs_iter) {
      // Get effective path from which it will be loaded
      const char *dyn_lib = GET_DATA (dyn_libs_iter);
      char *path = find_library_from_env (dyn_lib);
      if (!path) continue;

      // Get target name (if symbolic link)
      char *target = get_target (path, dyn_lib);
      if (!target) continue;
      
      // Build full physical name (path + basename)
      char *phy_name = lc_malloc (strlen (path) + strlen (target) + 2);
      sprintf (phy_name, "%s/%s", path, target);
      lc_free (target);

      // Build full symbolic name (path + basename)
      char *sym_name = lc_malloc (strlen (path) + strlen (dyn_lib) + 2);
      sprintf (sym_name, "%s/%s", path, dyn_lib);

      // Insert (key=phy_name, val=sym_name) to index
      hashtable_insert (phy2sym, phy_name, sym_name);

      lc_free (path);
   }
   queue_free (dyn_libs, lc_free);

   return phy2sym;
}
