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

/**
 * \file
 * */

///////////////////////////////////////////////////////////////////////////////
//										Dominance analysis									  //
///////////////////////////////////////////////////////////////////////////////
static void _DFS_postorder(graph_node_t* node, void* user)
{
   queue_t* postorder = (queue_t*) user;
   queue_add_tail(postorder, node->data);
}

static block_t* _intersect(block_t* b1, block_t* b2, block_t** doms,
      int* postorder_index)
{
   block_t* finger1 = b1;
   block_t* finger2 = b2;

   while (postorder_index[finger1->id] != postorder_index[finger2->id]) {
      while (postorder_index[finger1->id] < postorder_index[finger2->id])
         finger1 = doms[finger1->id];
      while (postorder_index[finger2->id] < postorder_index[finger1->id])
         finger2 = doms[finger2->id];
   }
   return (finger1);
}

void _compute_dominance(fct_t* fct)
{
   block_t* start_node = FCT_ENTRY(fct);
   queue_t* postorder = queue_new();
   queue_t* reverse_postorder = queue_new();
   int* postorder_index = lc_malloc(fct_get_nb_blocks(fct) * sizeof(int));
   block_t** doms = lc_malloc(fct_get_nb_blocks(fct) * sizeof(block_t*));
   int i = 0;
   block_t* new_idom = NULL;
   block_t* first_processed_pred = NULL;
   int changed = TRUE;

   // Order nodes in reverse-postorder
   i = 0;
   graph_node_DFS(start_node->cfg_node, NULL, &_DFS_postorder, NULL, postorder);
   FOREACH_INQUEUE(postorder, it_b) {
      block_t* b = GET_DATA_T(block_t*, it_b);
      queue_add_head(reverse_postorder, b);
      postorder_index[b->id] = i++;
   }

   // Initialize the dominator array
   for (i = 0; i < fct_get_nb_blocks(fct); i++)
      doms[i] = NULL;
   doms[start_node->id] = start_node;
   while (changed == TRUE) {
      changed = FALSE;
      FOREACH_INQUEUE(reverse_postorder, it_b)
      {
         block_t* b = GET_DATA_T(block_t*, it_b);
         if (b != start_node) {
            new_idom = NULL;

            // get the first processed predecessor
            FOREACH_INLIST(b->cfg_node->in, it_ed) {
               graph_edge_t* ed = GET_DATA_T(graph_edge_t*, it_ed);
               block_t* tmp = ed->from->data;
               if (doms[tmp->id] != NULL) {
                  first_processed_pred = tmp;
                  break;
               }
            }

            // then iterate over other predecessors
            new_idom = first_processed_pred;
            FOREACH_INLIST(b->cfg_node->in, it_ed0) {
               graph_edge_t* ed = GET_DATA_T(graph_edge_t*, it_ed0);
               block_t* p = ed->from->data;
               if (p != first_processed_pred) {
                  if (doms[p->id] != NULL)
                     new_idom = _intersect(p, new_idom, doms, postorder_index);
               }
            }
            if (doms[b->id] != new_idom) {
               doms[b->id] = new_idom;
               changed = TRUE;
            }
         }
      }
   }

   // Create the dominance tree using computed results
   FOREACH_INQUEUE(fct->blocks, it_b1) {
      block_t* b = GET_DATA_T(block_t*, it_b1);
      block_t* Db = doms[b->id];
      if (block_is_padding(b) == 0 && Db != b)
         tree_insert(Db->domination_node, b->domination_node);
   }
   lc_free(doms);
   lc_free(postorder_index);
   queue_free(reverse_postorder, NULL);
   queue_free(postorder, NULL);
}

/*
 * Builds the immediate dominators of all asmfile blocks.
 * The dominator tree is built as well.
 * \param asmfile an existing asmfile
 */
void lcore_analyze_dominance(asmfile_t *asmfile)
{
   if (asmfile != NULL && (asmfile->analyze_flag & CFG_ANALYZE) == 0)
      return;
   if (asmfile->analyze_flag & DOM_ANALYZE)
      return;

   DBGMSG0("computing domination\n");
   FOREACH_INQUEUE(asmfile_get_fcts(asmfile), iter) {
      fct_t *f = GET_DATA_T(fct_t*, iter);
      _compute_dominance(f);
   }

   asmfile->analyze_flag |= DOM_ANALYZE;
}

///////////////////////////////////////////////////////////////////////////////
//                        Post dominance analysis                            //
///////////////////////////////////////////////////////////////////////////////
static void add_virtual_end(fct_t* f)
{
   // create the virtual node
   block_t* vn = block_new(f, NULL);
   queue_add_tail(f->blocks, vn);
   f->virtual_exit = vn;

   // link exits to it
   FOREACH_INQUEUE(f->blocks, it_b)
   {
      block_t* b = GET_DATA_T(block_t*, it_b);
      if (b->cfg_node->out == NULL && !block_is_padding(b)
            && b != f->virtual_exit) {
         graph_add_edge(b->cfg_node, vn->cfg_node, NULL);
      }
   }
}

static void remove_virtual_end(fct_t* f)
{
   if (f->virtual_exit == NULL)
      return;
   queue_remove_tail(f->blocks);
   block_t* vn = f->virtual_exit;
   f->virtual_exit = NULL;
   block_free(vn);
}

void _compute_post_dominance(fct_t* fct)
{
   queue_t* postorder = queue_new();
   queue_t* reverse_postorder = queue_new();
   int* postorder_index = lc_malloc(fct_get_nb_blocks(fct) * sizeof(int));
   block_t** postdoms = lc_malloc(fct_get_nb_blocks(fct) * sizeof(block_t*));
   int i = 0;
   block_t* start_node = fct->virtual_exit;
   block_t* new_ipostdom = NULL;
   block_t* first_processed_succ = NULL;
   int changed = TRUE;

   fct_upd_blocks_id(fct);

   FOREACH_INQUEUE(fct->blocks, it_b2) {
      block_t* b = GET_DATA_T(block_t*, it_b2);
      b->postdom_node = tree_new(b);
   }

   // Order nodes in reverse-postorder
   i = 0;
   graph_node_BackDFS(start_node->cfg_node, NULL, &_DFS_postorder, NULL, postorder);
   FOREACH_INQUEUE(postorder, it_b) {
      block_t* b = GET_DATA_T(block_t*, it_b);
      queue_add_head(reverse_postorder, b);
      postorder_index[b->id] = i++;
   }

   // Initialize the post-dominator array
   for (i = 0; i < fct_get_nb_blocks(fct); i++)
      postdoms[i] = NULL;
   postdoms[start_node->id] = start_node;
   while (changed == TRUE) {
      changed = FALSE;
      FOREACH_INQUEUE(reverse_postorder, it_b)
      {
         block_t* b = GET_DATA_T(block_t*, it_b);
         if (b != start_node) {
            new_ipostdom = NULL;

            // get the first processed successor
            FOREACH_INLIST(b->cfg_node->out, it_ed) {
               graph_edge_t* ed = GET_DATA_T(graph_edge_t*, it_ed);
               block_t* tmp = ed->to->data;
               if (postdoms[tmp->id] != NULL) {
                  first_processed_succ = tmp;
                  break;
               }
            }
            // then iterate over other successors
            new_ipostdom = first_processed_succ;
            FOREACH_INLIST(b->cfg_node->out, it_ed0) {
               graph_edge_t* ed = GET_DATA_T(graph_edge_t*, it_ed0);
               block_t* p = ed->to->data;
               if (p != first_processed_succ) {
                  if (postdoms[p->id] != NULL)
                     new_ipostdom = _intersect(p, new_ipostdom, postdoms,
                           postorder_index);
               }
            }

            if (postdoms[b->id] != new_ipostdom) {
               postdoms[b->id] = new_ipostdom;
               changed = TRUE;
            }
         }
      }
   }

   // Create the post-dominance tree using computed results
   FOREACH_INQUEUE(fct->blocks, it_b1) {
      block_t* b = GET_DATA_T(block_t*, it_b1);
      block_t* Db = postdoms[b->id];
      if (block_is_padding(b) == 0 && Db != b) {
         tree_insert(Db->postdom_node, b->postdom_node);
      }
   }

   lc_free(postdoms);
   lc_free(postorder_index);
   queue_free(reverse_postorder, NULL);
   queue_free(postorder, NULL);
}

/*
 * Builds the immediate post-dominators of all asmfile blocks.
 * The post-dominator tree is built as well.
 * \param asmfile an existing asmfile
 */
void lcore_analyze_post_dominance(asmfile_t *asmfile)
{
   // CFG not computed
   if (asmfile != NULL && (asmfile->analyze_flag & CFG_ANALYZE) == 0)
      return;
   // Post dominance already computed
   if (asmfile->analyze_flag & PDO_ANALYZE)
      return;

   DBGMSG0("computing post-domination\n");
   FOREACH_INQUEUE(asmfile_get_fcts(asmfile), iter) {
      fct_t *f = GET_DATA_T(fct_t*, iter);
      add_virtual_end(f);
      _compute_post_dominance(f);
      remove_virtual_end(f);
   }
   asmfile->analyze_flag |= PDO_ANALYZE;
}

