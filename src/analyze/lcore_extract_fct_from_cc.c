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

#include <inttypes.h>
#include "libmcore.h"
#include "libmdbg.h"

/**
 * \file
 * This file contains functions used to extract functions from connected components (CCs).
 * It is assumed that "main" connected component (the connected component
 * containing the function label) if the first of the function CC list.
 *
 * Each CC which is not the main CC is changed into a new function. The name of
 * generated functions can have several form:
 *    - <fct>#<adress>
 *       where <fct> is the original function name and <address> the address of
 *       the first instruction of a CC entry>. This is the default form.
 *    - <fct>#omp#region#<nb>
 *       where <fct> is the original function name and <nb> a number (the
 *       position of the CC in the function CCs list). This form is used when
 *       debug data contains a function entry at the address of a CC entry.
 *       This "debug function" name has to contains "__par_region" into its name.
 *    - <fct>#omp#loop#<nb>
 *       where <fct> is the original function name and <nb> a number (the
 *       position of the CC in the function CCs list). This form is used when
 *       debug data contains a function entry at the address of a CC entry.
 *       This "debug function" name has to contains "__par_loop" into its name.
 *
 * After the name generation, the new function structure is created, then
 * the CC CFG is traversed to move blocks and loops from the original function
 * to the new one.
 * When all CCs have been extracted from the function, they are removed from
 * the function CC list.
 *
 * During the extraction, function entries are computed.
 */

/*
 * \struct cntxt_s
 * Structure used to pass several parameters trough a void* pointer
 * in DFS traversal
 */
typedef struct cntxt_s {
   char* flags;    // One flag per block
   fct_t* newf;    // Current function (created from a CC)
   fct_t* orig;    // Original function
} cntxt_t;

/*
 * Function used in graph DFS traversal.
 * Moves the block from a function to another one
 * \param node current CFG node
 * \param context current context
 */
void _DFS_move_block(graph_node_t* node, void* context)
{
   cntxt_t* cntxt = (cntxt_t*) context;
   block_t* b = node->data;

   if (b == NULL)
      return;
   if (cntxt->flags[b->id] == 0) {
      cntxt->flags[b->id] = 1;
      queue_remove(b->function->blocks, b, NULL);
      queue_add_tail(cntxt->newf->blocks, b);
      b->function = cntxt->newf;

      if (b->loop && b->loop->function != cntxt->newf) {
         loop_t* loop = b->loop;
         queue_remove(loop->function->loops, loop, NULL);
         queue_add_tail(cntxt->newf->loops, loop);
         loop->function = cntxt->newf;
      }
   }
}

/*
 * Function called by graph traversal function. Check in a block if there is
 * debug data from DWARF
 * \param graph a CFG node
 * \param user a pointer on a string, used to return the function name if it exist
 */
static void _func_node_look_debug(graph_node_t* g, void* user)
{
   block_t* b = (block_t*) g->data;
   char** ret = (char**) user;

   if (*ret != NULL)
      return;

   insn_t* start = (insn_t*) b->begin_sequence->data;
   insn_t* end = (insn_t*) b->end_sequence->data;
   int64_t dbg_address = -1;
   char* dbg_name = asmfile_has_dbg_function(b->function->asmfile,
         INSN_GET_ADDR(start), INSN_GET_ADDR(end), &dbg_address);
   if (dbg_name != NULL) {
      (*ret) = dbg_name;
      b->function->dbg_addr = dbg_address;
   }
}

/*
 * Extract sub-functions from a function, based on its connected components
 * Extracted functions are added in the asmfile which contains f
 * \param f a function to analyze
 * \param dsuffix distinguish suffix added at the end of created function names
 */
void lcore_function_extract_functions_from_cc(fct_t* f)
{
   asmfile_t* asmf = f->asmfile;
   fct_upd_blocks_id(f);

   // Check if the functions has CCs
   if (queue_length(f->components) < 1) {
      DBGMSG0(
            "WARNING : this function has no connected components => verify that it is nor an empty function\n");
      return;
   } else {
      DBGMSG("INFO : Current function has %d CC(s)\n",
            queue_length(f->components));
   }
   // Now extract functions from CC. For each CC, all its blocks and all its loops
   // are removed from the current function, then added in a new function.
   char ccid = 0;
   cntxt_t cntxt;
   queue_t* new_fcts = queue_new();
   queue_t* not_extracted = queue_new();
   cntxt.orig = f;
   cntxt.flags = lc_malloc0(sizeof(char) * queue_length(f->blocks));
   FOREACH_INQUEUE(f->components, it_cc1) {
      queue_t* cbs = GET_DATA_T(queue_t*, it_cc1);   //list of entries in the CC

      //ccid == 0 means this is the primary CC. In this case, there is nothing
      //to do. Else, a function must be create and blocks must be changed of function
      if (ccid > 0) {
         block_t* entry_block = queue_peek_head(cbs);
         insn_t* entry_insn = block_get_first_insn(entry_block);
         int64_t dbg_address = -1;
         char* dbg_name = asmfile_has_dbg_function(f->asmfile,
               INSN_GET_ADDR(entry_insn), -1, &dbg_address);

         // Special case: if no debug data, iterate over the CC to check if a
         // block has debug data. Use a graph traverse function calling our function
         // for each block
         if (dbg_name == NULL) {
            FOREACH_INQUEUE(cbs, it_en)
            {
               block_t* entry = GET_DATA_T(block_t*, it_en);
               graph_node_DFS(entry->cfg_node, &_func_node_look_debug, NULL, NULL,
                     &dbg_name);

               if (dbg_name != NULL)
                  break;
            }
         }

         char* fnew_name = NULL;
         char* fname = NULL;
         graph_edge_t* to_remove = NULL;

         // No debug data and CCMODE_DEBUG => do not extract the connected component
         // and save it into a list because it will be removed at the end
         if (dbg_name == NULL && f->asmfile->project != NULL
               && f->asmfile->project->cc_mode == CCMODE_DEBUG) {
            queue_add_tail(not_extracted, cbs);
         } else {
            if (f->debug != NULL && f->debug->name != NULL)
               fname = f->debug->name;
            else
               fname = fct_get_name(f);

            // Create the new function name
            if (dbg_name != NULL) {
#ifdef _WIN32
               if (strstr(dbg_name, "__par_region") != NULL)
#else
               if (str_contain(dbg_name,
                     "L_[a-zA-Z0-9_]+_[0-9]+__par_region[0-9]+_[0-9]+_[0-9]+"))
#endif
                     {
                  fnew_name = lc_malloc(
                        (strlen(fname) + strlen("#omp#region#") + 4)
                              * sizeof(char));
                  sprintf(fnew_name, "%s%s%d", fname, "#omp#region#", ccid++);
               }
#ifdef _WIN32
               else if (strstr(dbg_name, "__par_loop") != NULL)
#else
               else if (str_contain(dbg_name,
                     "L_[a-zA-Z0-9_]+_[0-9]+__par_loop[0-9]+_[0-9]+_[0-9]+"))
#endif
                     {
                  fnew_name = lc_malloc(
                        (strlen(fname) + strlen("#omp#loop#") + 4)
                              * sizeof(char));
                  sprintf(fnew_name, "%s%s%d", fname, "#omp#loop#", ccid++);
               } else {
                  fnew_name = lc_malloc((strlen(fname) + 15) * sizeof(char));
                  sprintf(fnew_name, "%s#0x%"PRIx64, fname,
                        INSN_GET_ADDR(entry_insn));
               }
            } else {
               fnew_name = lc_malloc((strlen(fname) + 15) * sizeof(char));
               sprintf(fnew_name, "%s#0x%"PRIx64, fname,
                     INSN_GET_ADDR(entry_insn));
            }
            // Create the label for the new function
            label_t* fnew_namelbl = label_new(fnew_name,
                  INSN_GET_ADDR(entry_insn), TARGET_INSN, entry_insn);
            asmfile_add_label(asmf, fnew_namelbl);

            // Create the new function
            /**\todo (2018-09-05) Use fct_new here, but take account of the fact that this function is supposed to exit if we want to create
             * a function for an instruction whose block is associated to an existing function (which is precisely what we are trying to do here) */
            /**\todo (2014-11-19) Now that we create a label for the new function, would it be interesting to relink the instructions
             * in the new function to the new label ?*/
            fct_t* fnew = (fct_t *) lc_malloc0(sizeof(fct_t));
            queue_add_tail(new_fcts, fnew);
            fnew->namelbl = fnew_namelbl;
            fnew->id = queue_length(asmf->functions); //uniq_id;
            fnew->global_id = asmf->maxid_fct++;
            fnew->asmfile = asmf;
            fnew->blocks = queue_new();
            fnew->loops = queue_new();
            fnew->cg_node = graph_node_new(fnew);
            fnew->entries = queue_new();
            fnew->exits = queue_new();
            fnew->ranges = queue_new();
            fnew->original_function = f;
            fnew->first_insn = entry_insn;
            fnew->components = queue_new();
            fnew->dbg_addr = dbg_address;
            queue_add_head(fnew->components, cbs);
            queue_add_tail(asmf->functions, fnew);

            if (asmf->load_fct_dbg != NULL)
               asmf->load_fct_dbg(fnew);
            if (fnew->demname != NULL)
               lc_free(fnew->demname);
            fnew->demname = lc_strdup(fnew_name);
            // Travserse the CC
            cntxt.newf = fnew;
            FOREACH_INQUEUE(cbs, it_entry) {
               block_t* entry = GET_DATA_T(block_t*, it_entry);
               queue_add_tail(fnew->entries, entry);
               graph_node_DFS(entry->cfg_node, &_DFS_move_block, NULL, NULL, &cntxt);

               //If needed, remove edges from virtual node
               FOREACH_INLIST(entry->cfg_node->in, it_in) {
                  graph_edge_t* ed = GET_DATA_T(graph_edge_t*, it_in);
                  block_t* pred = ed->from->data;
                  if (block_is_virtual(pred))
                     to_remove = ed;
               }
               if (to_remove != NULL)
                  graph_remove_edge(to_remove, NULL);
               to_remove = NULL;
            }

            block_t * vb = (block_t*) lc_malloc0(sizeof(block_t));
            vb->id = queue_length(fnew->blocks);
            vb->global_id = fnew->asmfile->maxid_block++;
            f->asmfile->n_blocks++;
            vb->domination_node = tree_new(vb);
            vb->function = fnew;
            vb->is_padding = -1;
            vb->cfg_node = graph_node_new(vb);
            queue_add_head(fnew->blocks, vb);
            FOREACH_INQUEUE(cbs, it_entry1)
            {
               block_t* entry = GET_DATA_T(block_t*, it_entry1);
               graph_add_edge(vb->cfg_node, entry->cfg_node, NULL);
            }
         }
      }
      ccid++;
   }

   // Remove extracted CC from original function. By construction, the first
   // one belongs to the original function, other ones do not
   while (queue_length(f->components) > 1)
      queue_remove_tail(f->components);
   lc_free(cntxt.flags);
   FOREACH_INQUEUE(not_extracted, it_ne) {
      queue_t* cbs = GET_DATA_T(queue_t*, it_ne);
      queue_add_tail(f->components, cbs);
   }

   // Now iterate over the original function CC to add its entries
   if (queue_length(f->entries) == 0) {
      FOREACH_INQUEUE(f->components, it_cc)
      {
         queue_t* cc = GET_DATA_T(queue_t*, it_cc);
         FOREACH_INQUEUE(cc, it_b)
         {
            block_t* b = GET_DATA_T(block_t*, it_b);
            queue_add_tail(f->entries, b);
         }
      }
   }
   queue_free(new_fcts, NULL);
   queue_free(not_extracted, NULL);

   return;
}

/*
 * Extract sub-functions from all asmfile functions, based on connected components
 * Extracted functions are added in the asmfile which contains f
 * \param f a function to analyze
 * \param dsuffix distinguish suffix added at the end of created function names
 */
void lcore_asmfile_extract_functions_from_cc(asmfile_t* asmf)
{
   if (asmf == NULL)
      return;
   if ((asmf->analyze_flag & COM_ANALYZE) == 0)
      return;

   FOREACH_INQUEUE(asmf->functions, it_f) {
      fct_t* f = GET_DATA_T(fct_t*, it_f);
      lcore_function_extract_functions_from_cc(f);
   }
   asmf->analyze_flag |= EXT_ANALYZE;
}
