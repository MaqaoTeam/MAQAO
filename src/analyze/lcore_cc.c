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

/**\file lcore_cc.c
 * */

#include <inttypes.h>

#include "libmcore.h"

///////////////////////////////////////////////////////////////////////////////
//                                Connected Components analysis               //
///////////////////////////////////////////////////////////////////////////////
/*
 * connected compoenents labeling analysis
 * The anaysis is based on a DFS search as described
 * by Tarjan in his paper:
 * "Efficient Alorithms for Graph Manipulation"(1972)
 * The algorithm was slightly modified to be able to record all the entries to
 * a component which correspond to blocks without any predecessor .
 **/

/**
 * \brief performs a dfs traversal with the rigth block marking. Each block b if
 * not traversed is marked as traversed (1), if it is a component head without
 * predecessor (3) it remains the same and if it is a head of component with
 * predecessors (2), the address of its first instruction is compared with the
 * one of the current head, if it is smaller than put the block as head of the
 * component and mark it as (2).
 * \param root the root block from which the traversal begins
 * \param cinfo an array holding traversal informations each block of the function
 * \param defaulthead the current head of the connected component
 * \param initheads the list of initial heads collected so far
 * \return list_t* the list of initial heads
 **/
static list_t *lcore_dfs_mark(block_t *root, int *cinfo, block_t *defaulthead,
      list_t *initheads)
{
   FOREACH_INLIST(root->cfg_node->out, liter) {
      graph_edge_t* ed = GET_DATA_T(graph_edge_t*, liter);
      block_t *b = (block_t*) (ed->to->data);

      // Case where the default head has predecessors and is in the initlist : look to the addresses of the first instruction
      // of each block and place the one with the smallest address in the inithead list
      if ((cinfo[b->id] == 0)
            || ((cinfo[defaulthead->id] == 2) && (cinfo[b->id] == 2))) {
         if (b->begin_sequence == NULL)
            continue;

         insn_t *ins1 = GET_DATA_T(insn_t*, b->begin_sequence);
         insn_t *ins2 = GET_DATA_T(insn_t*, defaulthead->begin_sequence);
         int notvisited = 0;

         if (cinfo[b->id] == 0)
            notvisited = 1;

         if (INSN_GET_ADDR(ins1) < INSN_GET_ADDR(ins2)) {
            initheads = list_remove(initheads, defaulthead, NULL);
            cinfo[defaulthead->id] = 1;
            initheads = list_add_before(initheads, b);
            defaulthead = b;
            cinfo[b->id] = 2;
         } else
            cinfo[b->id] = 1;

         if (notvisited)
            initheads = lcore_dfs_mark(b, cinfo, defaulthead, initheads);
      }

      // Case where the defaulthead has no predecessors : if the current block was in the initlist it should be removed
      else if ((cinfo[defaulthead->id] == 3) && (cinfo[b->id] == 2)) {
         initheads = list_remove(initheads, b, NULL);
         cinfo[b->id] = 1;
      }
   }
   return initheads;
}

/*
 * \brief Creates the list of initial heads of connected components. As a function may
 * contain more than just one component(especially in the case of openMP programs).
 * Hence, component headers are mandatory to cover all the function's elements within
 * the different analyses such as loop analysis.
 * \param fct_t* the target function
 * \return list_t* the list of initial heads of components
 **/
list_t *lcore_collect_init_heads(fct_t *f)
{
   fct_upd_blocks_id(f);
   DBGMSG("Collecting connected components head in function %s\n", fct_get_name(f));
   int i;
   list_t* initheads = NULL;
   int *cinfos = (int *) lc_malloc(sizeof(int) * f->blocks->length);

   for (i = 0; (unsigned int) i < f->blocks->length; i++)
      cinfos[i] = 0;

   //Process until stability
   FOREACH_INQUEUE(f->blocks, blockiter) {
      block_t *b1 = GET_DATA_T(block_t*, blockiter);
      if (b1->begin_sequence != NULL) {
         if (cinfos[b1->id] == 0) {
            if (b1->cfg_node->in == NULL)
               cinfos[b1->id] = 3;
            else
               cinfos[b1->id] = 2;
            initheads = list_add_before(initheads, b1);
            initheads = lcore_dfs_mark(b1, cinfos, b1, initheads);
         }
      }
   }
   lc_free(cinfos);
   return initheads;
}

typedef struct cntxt_s {
   fct_t* f;
   queue_t* current_CC;   //queue of block. Each block is an entry of current CC
   queue_t** bflags;       //array of queue, one per block
   queue_t* CC_to_remove;  //array of queue to remove, one per CC

   // Index on CCs to speedup lookup
   hashtable_t* current_CC_ht;   //hashtable of block. Each block is an entry of current CC
   hashtable_t** bflags_ht;       //array of hashtable, one per block
} cntxt_t;

/**
 * Checks if an edge is a backedge
 * \param edge an edge to check
 * \return TRUE if edge is a backedge, else FALSE
 */
static int _edge_isbackedge(graph_edge_t* edge)
{
   block_t* bfrom = ((block_t*) edge->from->data);
   block_t* to = ((block_t*) edge->to->data);

   if (to->loop == bfrom->loop) {
      loop_t* loop = bfrom->loop;
      list_t* entries = loop_get_entries(loop);
      FOREACH_INLIST(entries, it_entry)
      {
         block_t* entry = GET_DATA_T(block_t*, it_entry);
         if (entry->global_id == to->global_id)
            return (TRUE);
      }
   }
   return (FALSE);
}

/**
 * Checks if a block is a connected component entry
 * \param b a basic block
 * \return TRUE if b is a CC entry, else FALSE
 */
static int _block_is_CC_entry(block_t* b)
{
   if (block_is_padding(b) || block_is_virtual(b))
      return (FALSE);

   FOREACH_INLIST(b->cfg_node->in, it_in) {
      graph_edge_t* ed = GET_DATA_T(graph_edge_t*, it_in);
      block_t* pred = ed->from->data;

      if (block_is_virtual(pred) == FALSE && block_is_padding(pred) == FALSE
            && _edge_isbackedge(ed) == FALSE) {
         return (FALSE);
      }
   }
   return (TRUE);
}

/**
 * Function call for each block in the DFS traversal
 * If block CC not set, set it
 * Else merge current CC with block CC
 * \param node current node
 * \param context current context
 */
static void _DFS_func(graph_node_t* node, void* context)
{
   cntxt_t* cntxt = (cntxt_t*) context;
   block_t* b = node->data;

   if (cntxt->bflags[b->id] == NULL) {
      cntxt->bflags[b->id] = cntxt->current_CC;
      cntxt->bflags_ht[b->id] = cntxt->current_CC_ht;
   } else {
      block_t *CC_entry = queue_peek_head(cntxt->current_CC);

      //Means current block has been traversed twice
      //Add current CC entry into the CC saved in cntxt->bflags[b->id]
      if (hashtable_lookup(cntxt->bflags_ht[b->id], CC_entry) == NULL) {
         queue_add_tail(cntxt->bflags[b->id], CC_entry);
         hashtable_insert (cntxt->bflags_ht[b->id], CC_entry, CC_entry);
      }
      if (queue_lookup(cntxt->CC_to_remove, &direct_equal,
            cntxt->current_CC) == NULL)
         queue_add_head(cntxt->CC_to_remove, cntxt->current_CC);
   }
}

/**
 * Function call for each block in the DFS traversal for main CC
 * Just set the block CC
 * \param node current node
 * \param context current context
 */
static void _DFS_main(graph_node_t* node, void* context)
{
   cntxt_t* cntxt = (cntxt_t*) context;
   block_t* b = node->data;
   cntxt->bflags[b->id] = cntxt->current_CC;
   cntxt->bflags_ht[b->id] = cntxt->current_CC_ht;
}

/**
 * Custom graph traverse function. Both successors and predecessors are used to
 * traverse the graph
 * \param node node to traverse
 * \param cntxt current context
 * \param marks used to mark traversed blocks
 * \param func function call on each traversed block
 */
static void __traverse_CFG(graph_node_t* node, cntxt_t* cntxt, hashtable_t* marks,
      void (*func)(graph_node_t *, void *))
{
   hashtable_insert(marks, node, node);

   // Function on node
   if (func != NULL)
      func(node, cntxt);

   // Traverse predecessors
   FOREACH_INLIST(node->in, it_in) {
      graph_edge_t* ed = GET_DATA_T(graph_edge_t*, it_in);
      graph_node_t* pred = ed->from;
      block_t* b = pred->data;
      if (!block_is_virtual(b) && hashtable_lookup(marks, pred) == NULL)
         __traverse_CFG(pred, cntxt, marks, func);
   }

   // Traverse successors
   FOREACH_INLIST(node->out, it_out)
   {
      graph_edge_t* ed = GET_DATA_T(graph_edge_t*, it_out);
      graph_node_t* succ = ed->to;
      block_t* b = succ->data;
      if (!block_is_virtual(b) && hashtable_lookup(marks, succ) == NULL)
         __traverse_CFG(succ, cntxt, marks, func);
   }
}

/**
 * Analyzes functions to get connected compontents (CC) heads.
 * \param asmfile an existing asmfile
 */
void lcore_analyze_connected_components(asmfile_t *asmfile)
{
   if ((asmfile->analyze_flag & CFG_ANALYZE) == 0)
      return;
   DBGMSG0("Compute connected components\n");
   // For a given function, iterate over its blocks
   // If a non virtual block as no predecessors in the CFG or if
   // its predecessors are backedges, then it is a connected component entry
   //
   // For each CC entry, traverse the CFG (following predecessors and successors)
   // and add each traversed block in the CC. If a block belongs to several CC,
   // these CC are merged into a single one with multiple entries

   // Recycled structures
   hashtable_t* marks = hashtable_new(&direct_hash, &direct_equal);

   // Iterate over asmfile functions
   FOREACH_INQUEUE(asmfile->functions, it_func) {
      fct_t* func = GET_DATA_T(fct_t*, it_func);
      if (func->components != NULL)
         break;
      insn_t* finsn = fct_get_first_insn(func);
      block_t* entryblock = insn_get_block(finsn);
      func->components = queue_new();
      fct_upd_blocks_id(func);

      // List CC heads for current function -----------------------------------
      FOREACH_INQUEUE(func->blocks, it_b) {
         block_t* b = GET_DATA_T(block_t*, it_b);

         if (_block_is_CC_entry(b) || b->global_id == entryblock->global_id) {
            queue_t* cc = queue_new();
            queue_add_tail(cc, b);
            if (b->global_id == entryblock->global_id)
               queue_add_head(func->components, cc);
            else
               queue_add_tail(func->components, cc);
         }
      }

      // Look for CC with multiple entries ------------------------------------
      if (queue_length(func->components) > 1) {
         int pos = 0;
         cntxt_t cntxt;
         cntxt.f = func;
         cntxt.current_CC = NULL;
         cntxt.current_CC_ht = NULL;
         cntxt.bflags = lc_malloc0(
               queue_length(func->blocks) * sizeof(queue_t*));
         cntxt.bflags_ht = lc_malloc0(
               queue_length(func->blocks) * sizeof(hashtable_t*));
         cntxt.CC_to_remove = queue_new();
         queue_t *CC_ht_to_remove = queue_new();

         FOREACH_INQUEUE(func->components, it_cc) {
            queue_t* cc = GET_DATA_T(queue_t*, it_cc);
            block_t* b = queue_peek_head(cc);
            cntxt.current_CC = cc;
            cntxt.current_CC_ht = hashtable_new (&direct_hash, &direct_equal);
            queue_add_tail (CC_ht_to_remove, cntxt.current_CC_ht);
            FOREACH_INQUEUE(cc, it_bl) {
               block_t *block = GET_DATA_T(block_t*, it_bl);
               hashtable_insert (cntxt.current_CC_ht, block, block);
            }

            if (pos == 0) {  // If first CC (the main one), just marks blocks
               __traverse_CFG(b->cfg_node, &cntxt, marks, &_DFS_main);
            } else { // Else, check for if traversed block if it has already been traversed
                     // (during another CC analysis)
                     // If yes, merge current CC with the CC containing the block
               __traverse_CFG(b->cfg_node, &cntxt, marks, &_DFS_func);
            }
            hashtable_flush (marks, NULL, NULL);
            pos++;
         }

         // Remove merged CC from function CCs --------------------------------
         FOREACH_INQUEUE(cntxt.CC_to_remove, it_cc1) {
            queue_t* cc = GET_DATA_T(queue_t*, it_cc1);
            queue_remove(func->components, cc, NULL);
            queue_free(cc, NULL);
         }
         queue_free(cntxt.CC_to_remove, NULL);

         FOREACH_INQUEUE(CC_ht_to_remove, it_cc2) {
            hashtable_t* cc_ht = GET_DATA_T(hashtable_t*, it_cc2);
            hashtable_free(cc_ht, NULL, NULL);
         }
         queue_free(CC_ht_to_remove, NULL);

         lc_free(cntxt.bflags);
         lc_free(cntxt.bflags_ht);
      }
   }

   // recycled structures
   hashtable_free (marks, NULL, NULL);

   asmfile->analyze_flag |= COM_ANALYZE;
}
