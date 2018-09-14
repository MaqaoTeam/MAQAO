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

#include "libmcore.h"

/*
 * \struct order_s
 * \brief structure holding DFS tree traversal informations
 */
typedef struct order_s {
   int Dfn; /**<*/
   short int traversed; /**<*/
} order_t;

/*
 * \struct global_s
 * contains all global variables
 */
typedef struct global_s {
   order_t *order; /**<*/
   list_t *Bstack; /**<*/
   list_t* remove_from_stack; /**<*/
} global_t;

/*
 * Removes an element from a list without deleting it
 * \param list list_t** the list from which the element will be removed
 * \param current list_t* the element to be removed
 * \return the removed element
 */
list_t *list_remove_element(list_t **list, list_t *current)
{
   list_t *prev, *next;
   if (*list == current)
      *list = (*list)->next;
   if (current != NULL) {
      prev = ((list_t *) current)->prev;
      next = ((list_t *) current)->next;
      if (prev != NULL)
         prev->next = next;
      if (next != NULL)
         next->prev = prev;
   }
   return *list;
}

/*
 * Reorders the hierarchy of loops(ie: the loop nesting forest). This is done in case
 * of overlapping unstructuredness or irreducibility unstructuredness. Note the reorder walks through
 * all the loop nest.
 * \param b block_t* block from which the loop to reorder is to be extracted.
 * \param h block_t* the entry of the second loop to be reordered
 */
void reorderHierarchy(block_t *b, block_t *h, global_t* global)
{
   block_t *ih = NULL;
   if (b == h || h == NULL)
      return;
   block_t *cur1 = b, *cur2 = h;
   DBGMSGLVL(3, "Reordering from block [%#"PRIx64" - %#"PRIx64"] to block [%#"PRIx64" - %#"PRIx64"]\n"
         , block_get_first_insn_addr(b), block_get_last_insn_addr(b), block_get_first_insn_addr(h), block_get_last_insn_addr(h));
   while (1) {
      //----------------test of the while -------------------------------------
      //The difference with the algorithm on the paper is pure implementational 
      //separation of the case where the block itself is a loop header  
      if (cur1->loop) {
         if ((block_t *) cur1->loop->entries->data != cur1)
            ih = (block_t *) cur1->loop->entries->data;
         else if (cur1->loop->hierarchy_node->parent)
            ih =
                  (block_t *) ((loop_t *) cur1->loop->hierarchy_node->parent->data)->entries->data;
         else
            break;
      } else
         break;

      if (ih == cur2)
         return;
      if (global->order[block_get_id(ih)].Dfn
            < global->order[block_get_id(cur2)].Dfn) {
         if ((block_t *) cur1->loop->entries->data != cur1) {
            DBGMSGLVL(2, "Moving block [%#"PRIx64" - %#"PRIx64"] from loop %d to loop %d\n"
                  , block_get_first_insn_addr(cur1), block_get_last_insn_addr(cur2), loop_get_id(cur1->loop), loop_get_id(cur2->loop));
            cur1->loop = cur2->loop;
         }
         //-----------updating the hierarchy node------------------------------
         if (cur2->loop->hierarchy_node->parent)
            tree_remove_child(cur2->loop->hierarchy_node->parent,
                  cur2->loop->hierarchy_node);
         tree_insert(ih->loop->hierarchy_node, cur2->loop->hierarchy_node);
         DBGMSGLVL(2, "Moving loop %d under loop %d\n", loop_get_id(cur2->loop), loop_get_id(GET_DATA_T(loop_t*, ih->loop->hierarchy_node)));
         cur1 = cur2;
         cur2 = ih;
      } else
         cur1 = ih;
   }

   if (!cur1->loop)
      cur1->loop = cur2->loop;
   else
      tree_insert(cur2->loop->hierarchy_node, cur1->loop->hierarchy_node);
}

/*
 * The algorithm for loop identification and loop nesting construction
 * \param root block_t* the root of the sub tree from which the algorithm will operate
 * \param NextDfn
 * \return block_t* the header of the root's loop (if it exists)
 */
block_t *loop_constructor(block_t *root, int* NextDfn, global_t* global)
{
   global->Bstack = list_add_before(global->Bstack, root); // Works as a stack push
   global->remove_from_stack = list_add_before(global->remove_from_stack,
         global->Bstack);
   (*NextDfn) += 1;
   global->order[block_get_id(root)].Dfn = *NextDfn;
   global->order[block_get_id(root)].traversed = 1;
   loop_t *loop = NULL;
   list_t *l = root->cfg_node->out;

   //------ FIRST STEP ---------------------------------------------------------
   FOREACH_INLIST_REVERT(l, succiter) {
      block_t *b = GET_DATA_T(block_t*, GET_DATA_T(graph_edge_t*, succiter)->to);
      if (!global->order[block_get_id(b)].traversed) {
         // CASE (A) : new block
         DBGMSGLVL(2, "Block %d has not been analysed yet: building loops starting from it\n", block_get_id(b));
         block_t *nh = loop_constructor(b, NextDfn, global); // THE RECURSIVE CALL
         reorderHierarchy(root, nh, global);
      } else {
         if (global->order[block_get_id(b)].Dfn > 0) {
            // CASE (B) : b in DFSP(b0): MARK b AS A LOOP HEADER
            if ((!b->loop)
                  || ((b->loop) && ((block_t *) b->loop->entries->data != b))) {
               loop = loop_new(b);
               DBGMSGLVL(2, "Created new loop %d with block %d as header\n", loop_get_id(loop), block_get_id(b));
            }
            reorderHierarchy(root, b, global);
         } else if ((b->loop != NULL)
               && (((block_t *) b->loop->entries->data != b)
                     || b->loop->hierarchy_node->parent)) {
            // CASE (C) is omitted : b->loop == NULL //
            block_t *h = NULL;

            if ((block_t *) b->loop->entries->data != b)
               h = (block_t *) b->loop->entries->data;
            else
               h =
                     (block_t *) ((loop_t *) b->loop->hierarchy_node->parent->data)->entries->data;
            DBGMSGLVL(2, "Block %d belongs to loop %d. Block preceding in hierarchy is %d\n", block_get_id(b), loop_get_id(b->loop), block_get_id(h));
            if (global->order[block_get_id(h)].Dfn > 0)
               // CASE (D) : h in DFSP(b0)
               reorderHierarchy(root, h, global);
            else {
               // CASE (E) : h not in DFSP(b0): MARK b AS A RE-ENTRY
               loop_t *llp = NULL;

               if (list_lookup(h->loop->entries, b) == NULL)
                  // adding the re-entry
                  list_add_after(h->loop->entries, b);
               DBGMSG("Block %d is a re-entry for loop %d\n", block_get_id(b), loop_get_id(b->loop));
               if (h->loop->hierarchy_node->parent)
                  llp = (loop_t *) h->loop->hierarchy_node->parent->data;
               if (llp) {
                  while (1) {
                     if (global->order[block_get_id(
                           (block_t *) llp->entries->data)].Dfn > 0) {
                        reorderHierarchy(root, (block_t *) llp->entries->data,
                              global);
                        break;
                     }
                     if (list_lookup(llp->entries, b) == NULL)
                        //adding the re-entry
                        list_add_after(llp->entries, b);
                     if (llp->hierarchy_node->parent)
                        llp = (loop_t *) llp->hierarchy_node->parent->data;
                     else
                        break;
                  }
               }
            }
         }
      }
   }

   if (root->loop == NULL) {
      global->order[block_get_id(root)].Dfn = 0;
      return (NULL);
   }

   if (root == (block_t *) root->loop->entries->data) {
      loop = root->loop;
      list_t *current = list_lookup(global->Bstack, root); // Locating the loop root in the stack
      while (current != NULL) {
         block_t* block = (block_t *) current->data;
         //if the current block contains an exit
         insn_t *linsn = block_get_last_insn(block);
         asmfile_t * a = fct_get_asmfile(block_get_fct(block));
         if (a->project != NULL && a->project->exit_functions != NULL) {
            if (linsn != NULL
                  && insn_check_annotate(linsn,
                        A_CALL) && oprnd_is_ptr (insn_get_oprnd (linsn, 0)) == TRUE
                        && pointer_get_insn_target (oprnd_get_ptr(insn_get_oprnd (linsn, 0))) != NULL) {
               // check the target function is in the handler function list
               fct_t* target = block_get_fct(
                     insn_get_block(
                           pointer_get_insn_target(
                                 oprnd_get_ptr(insn_get_oprnd(linsn, 0)))));
               int i = 0;

               while (a->project->exit_functions[i] != NULL) {
                  if (strcmp(fct_get_name(target),
                        a->project->exit_functions[i]) == 0) {
                     if (list_lookup(loop->exits, block) == NULL)
                        loop->exits = list_add_before(loop->exits, block);
                     block->is_loop_exit = 1;
                     break;
                  }
                  i++;
               }
            }
         }
         if (block->loop != NULL) {
            // Case where the block is not part of an inner loop -> determine if it is an exit
            if (block->loop == loop) {
               queue_add_head(loop->blocks, block); // adding the block to the loop
               if (block != root)
                  list_remove_element(&(global->Bstack), current); // removing the block from the stack
               list_t *l = block->cfg_node->out;
               // Verify if the the current block has an output edge leading outside of the current loop
               // If such a case is found => mark block as exit and break
               FOREACH_INLIST(l, blockiter)
               {
                  block_t *b = GET_DATA_T(block_t*,
                        (GET_DATA_T(graph_edge_t*, blockiter))->to);
                  //block not in loop
                  if (b->loop == NULL) {
                     if (list_lookup(loop->exits, block) == NULL)
                        loop->exits = list_add_before(loop->exits, block);
                     block->is_loop_exit = 1;
                     break;
                  }
                  //block belonging to another loop
                  else if (b->loop != loop) {
                     if (b->loop->hierarchy_node->parent) {
                        if (!tree_is_ancestor(loop->hierarchy_node,
                              b->loop->hierarchy_node)) {
                           if (list_lookup(loop->exits, block) == NULL)
                              loop->exits = list_add_before(loop->exits, block);
                           block->is_loop_exit = 1;
                           break;
                        }
                     } else {
                        if (list_lookup(loop->exits, block) == NULL)
                           loop->exits = list_add_before(loop->exits, block);
                        block->is_loop_exit = 1;
                     }
                  }
               }
            }
            // Case where the block is the entry of an inner loop add its blocks to the current one, and determine if its an exit
            else if (((block_t *) block->loop->entries->data == block)) {
               if ((tree_is_ancestor(loop->hierarchy_node,
                     block->loop->hierarchy_node))) {
                  //the added block is a loop entry, so blocks of inner loop are added to the outer one
                  loop_t *nestedLoop = block->loop;
                  FOREACH_INQUEUE(nestedLoop->blocks, loopiter) {
                     block_t *iter = GET_DATA_T(block_t*, loopiter);
                     queue_add_head(loop->blocks, iter);
                     list_t *l = iter->cfg_node->out;
                     FOREACH_INLIST(l, blockiter)
                     {
                        block_t *b = GET_DATA_T(block_t*,
                              (GET_DATA_T(graph_edge_t*, blockiter))->to);
                        if (b->loop) {
                           if ((b->loop != loop)
                                 && ((b->loop->hierarchy_node->parent
                                       && !tree_is_ancestor(
                                             loop->hierarchy_node,
                                             b->loop->hierarchy_node))
                                       || (!b->loop->hierarchy_node->parent))) {
                              if (list_lookup(loop->exits, iter) == NULL)
                                 loop->exits = list_add_before(loop->exits,
                                       iter);
                              iter->is_loop_exit = 1;
                              break;
                           }
                        } else {
                           if (list_lookup(loop->exits, iter) == NULL)
                              loop->exits = list_add_before(loop->exits, iter);
                           iter->is_loop_exit = 1;
                           break;
                        }
                     }
                  }
                  list_remove_element(&(global->Bstack), current);
               }
            }
         }
         if (current != NULL)
            current = current->prev;  // go upward in the stack
      }  //end while (current != NULL)
   }

   global->order[block_get_id(root)].Dfn = 0;
   if ((block_t *) root->loop->entries->data != root)
      return (block_t *) root->loop->entries->data;
   if (root->loop->hierarchy_node->parent)
      return (block_t *) ((loop_t *) root->loop->hierarchy_node->parent->data)->entries->data;
   return NULL;
}

/*
 * \brief Initialize the structures and launches the loop detection algorithm
 */
static void build_loops(fct_t *f, global_t* global)
{
   block_t *root = NULL;
   list_t *initheads = NULL;
   int Nblocks = f->asmfile->n_blocks;
   int NextDfn = 0;
   int validFunc = 0;
   if (Nblocks == 0)
      return;

   // Call the function which constructs the initial list of heads of connected components
   // these represent a subset of the fine list of head and enables the application of 
   // loops analysis since the connected components analysis is done after it.
   initheads = lcore_collect_init_heads(f);
   DBGLVL(1, FCTNAMEMSG("Function %s has the following heads of collected components:\n",  fct_get_name(f));
            list_t* dbgiter = initheads;
            while (dbgiter != NULL) {
               fprintf(stderr, "%d [%#"PRIx64" - %#"PRIx64"]\n",
                     block_get_id(GET_DATA_T(block_t*, dbgiter)), block_get_first_insn_addr(GET_DATA_T(block_t*, dbgiter)),
                     block_get_last_insn_addr(GET_DATA_T(block_t*, dbgiter)));
               dbgiter = list_getnext(dbgiter);
            })
   //------ Initialization of the structure order ----------------------------
   global->order = lc_malloc0((Nblocks + 1) * sizeof(order_t));

   // Apply the algorithm to each block without predecessors (potential connected component)
   FOREACH_INLIST(initheads, blockiter) {
      root = GET_DATA_T(block_t*, blockiter);
      if ((root != NULL) && (root->cfg_node != NULL)) {
         validFunc = 1;
         DBGMSGLVL(1, "Building loops starting from block %d\n", block_get_id(root));
         loop_constructor(root, &NextDfn, global);
      }
   }
   if (!validFunc)
      WRNMSG("Function %s has a NULL entry block %p \n", fct_get_name(f),
            f->blocks);

   list_free(initheads, NULL);
   list_free(global->remove_from_stack, &lc_free);
   global->remove_from_stack = NULL;
   global->Bstack = NULL;
   lc_free(global->order);
}

/**
 * \brief After loop detection we need to verify if there are no remaining loops which are actually a connected
 * component's entry and not inserted into the connected components' entries list. This phase needs the loops hierarchy.
 * \param asmfile The asmfile of the binary which contains the functions to analyze
 */
static void lcore_loop_find_orphan_CC(asmfile_t *asmfile)
{
   fct_t *f = NULL;

   DBGMSG0(
         "Looking for potentiel connected components not added to the connected components' entries list \n");
   FOREACH_INQUEUE(asmfile->functions, funciter)
   {
      f = GET_DATA_T(fct_t*, funciter);
      // If the function has a virtual block 
      if (FCT_ENTRY(f)->begin_sequence != NULL)
         continue;
      /*TODO: use simpler algo => if loop entry has no other predecessors than blocks in the same loop
       then add it to the the connected components' entries list */
      FOREACH_INQUEUE(f->loops, loopiter)
      {
         loop_t *l = GET_DATA_T(loop_t*, loopiter);
         FOREACH_INLIST(l->entries, blockiter)
         {
            block_t *b = GET_DATA_T(block_t*, blockiter);
            // Check if there are some input edges
            if (b->cfg_node->in == NULL)
               continue;

            int attach_toentry = 0;
            int already_attached = 0;
            int count = 0, linked = 0;

            FOREACH_INLIST(b->cfg_node->in, initer) {
               count++;
               block_t *inblock = (block_t*) ((graph_edge_t *) GET_DATA_T(block_t*,
                     initer))->from->data;
               //inblock is the virtual node
               if (inblock->begin_sequence == NULL)
                  already_attached = 1;
               //not a self pointer (one block loop)
               else if (block_get_id(inblock) != block_get_id(b)) {
                  loop_t *hloop = inblock->loop;

                  while (hloop != NULL) {
                     if (hloop != b->loop) {
                        if (hloop->hierarchy_node->parent)
                           hloop =
                                 (loop_t *) hloop->hierarchy_node->parent->data;
                        else
                           break;
                     } else {
                        linked++;
                        break;
                     }
                  }
               }
            }
            if (linked == count)
               attach_toentry = 1;
            if (attach_toentry && !already_attached) {
               graph_add_edge(FCT_ENTRY(f)->cfg_node, b->cfg_node, NULL);
               DBGMSG("ADDED EDGE FROM %u -> %u\n", block_get_id(FCT_ENTRY(f)),
                     block_get_id(b));
               break;
            }
         }
      }
   }
}


int _fix_loop_entries (asmfile_t *asmfile)
{
   int nb_added_entries = 0;

   DBGMSG0("Fixing loops entries\n");

   // Analyze all functions
   FOREACH_INQUEUE(asmfile->functions, funciter)
   {
      fct_t* f = GET_DATA_T(fct_t*, funciter);

      // Analyse all loops
      FOREACH_INQUEUE(f->loops, loopiter)
      {
         loop_t* l = GET_DATA_T(loop_t*, loopiter);

         // Analyze all blocks in the loop
         FOREACH_INQUEUE(l->blocks, blockiter)
         {
            block_t* b = GET_DATA_T (block_t*, blockiter);
            graph_node_t* cfg_b = block_get_CFG_node (b);

            // For each predecessor in the CFG, check if it is in the loop
            FOREACH_INLIST (cfg_b->in, initer)
            {
               graph_edge_t* edge      = GET_DATA_T (graph_edge_t*, initer);
               graph_node_t* cfg_pred = edge->from;
               block_t* pred     = cfg_pred->data;
               char is_in_loop   = FALSE;

               FOREACH_INQUEUE(l->blocks, blockiter2)
               {
                  block_t* b = GET_DATA_T (block_t*, blockiter2);
                  if (block_get_id (pred) == block_get_id (b))
                  {
                     is_in_loop = TRUE;
                     break;
                  }
               }

               // If yes, continue
               if (is_in_loop == TRUE)
                  continue;

               // Check if it is in the entry list
               if (list_lookup(loop_get_entries (l), b) == NULL)
               {
                  // If not, add it in the entry list
                  l->entries = list_add_before(l->entries, b);
                  nb_added_entries = nb_added_entries + 1;
               }
            }
         }
      }
   }
   return (nb_added_entries);
}



/*
 * Launches the loop detection analysis for all functions
 * \param asmfile a valid asmfile
 */
void lcore_analyze_loops(asmfile_t *asmfile)
{
   fct_t *f;
   if (asmfile == NULL || (asmfile->analyze_flag & CFG_ANALYZE) == 0) {
      ERRMSG("Control Flow should be analyzed before computing loops\n");
      return;
   }

   global_t global;
   global.order = NULL;
   global.Bstack = NULL;
   global.remove_from_stack = NULL;

   DBGMSG0("computing loops\n");
   FOREACH_INQUEUE(asmfile->functions, iter) {
      f = GET_DATA_T(fct_t*, iter);
      DBGMSG("Analyzing loops of function %s\n", fct_get_name(f));
      build_loops(f, &global);
   }
   asmfile->analyze_flag |= LOO_ANALYZE;
   //Special case where an independent loop is not recognized as a CC
   lcore_loop_find_orphan_CC(asmfile);

   // fix loop entries
   // In some cases, some loop entries are not detected.
   // As some of our analysis require loops have only one entry, it is
   // necessary than loop entries are corrects
   _fix_loop_entries (asmfile);
}
