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

////////////////////////////////////////////////////////////////////////////////
//                                Paths analysis                              //
////////////////////////////////////////////////////////////////////////////////
// The functions of this file are used in the computation of paths contained
// withing a given loop. It is worth noting that if a loop contains a huge
// number of paths, the computation is aborted. It is also aborted if the loop
// doesn't contain exits.

///////////////////////////////////////////////////////////////////////////////
//                                API functions                              //
///////////////////////////////////////////////////////////////////////////////

static void compute_paths(block_t *root_block, int max_paths,
      queue_t **obj_paths)
{
   graph_node_t *root_node = root_block->cfg_node;

   // npaths == max_paths means nb paths >= max_paths. To distinguish: using max_paths+1
   const int npaths = graph_node_get_nb_paths(root_node, max_paths + 1);

   if (npaths <= max_paths) {
      queue_t *paths_nodes = graph_node_compute_paths(root_node);
      queue_t *paths_blocks = queue_new();

      FOREACH_INQUEUE(paths_nodes, iter) {
         array_t *path_nodes = GET_DATA_T(array_t*, iter); /* array of nodes */
         array_t *path_blocks = array_new_with_custom_size
            (array_length (path_nodes));

         FOREACH_INARRAY(path_nodes, iter2) {
            graph_node_t *node = ARRAY_GET_DATA(node, iter2);
            block_t *block = GET_DATA_T(block_t*, node);
            array_add(path_blocks, block);
         }

         array_free(path_nodes, NULL);
         queue_add_tail(paths_blocks, path_blocks);
      }

      queue_free(paths_nodes, NULL);
      *obj_paths = paths_blocks;
   }
}

/*
 * Computes paths for a function and overwrite its 'paths' field
 * Following functions are not considered:
 *  - more than one entry
 *  - paths already computed
 *  - more than FCT_MAX_PATHS paths
 * \param f a function
 */
void lcore_fct_computepaths(fct_t *f)
{
   if (FCT_NB_ENTRIES (f) != 1 || fct_get_paths(f) != NULL)
      return;

   block_t *root_block = queue_peek_head(fct_get_entry_blocks(f));

   compute_paths(root_block, FCT_MAX_PATHS, &(f->paths));
}

static void _remove_edge(graph_edge_t* edge)
{
   graph_node_t* from = edge->from;
   graph_node_t* to = edge->to;

   from->out = list_remove(from->out, edge, NULL);
   to->in = list_remove(to->in, edge, NULL);
}

static void _add_edge(graph_edge_t* edge)
{
   graph_node_t* from = edge->from;
   graph_node_t* to = edge->to;

   from->out = list_add_before(from->out, edge);
   to->in = list_add_before(to->in, edge);
}

/*
 * Removes edges entering and exiting a given loop and put them
 * in a list. Edges are only removed from the corresponding list:
 * for entry block: block->cfg_node->in
 * for exit blocks: block->cfg_node->out
 * \param l a loop
 * \return a list containing removed edges.
 */
static list_t* remove_edges_for_subgraph(loop_t *l)
{
   block_t *root_block = list_getdata(loop_get_entries(l));
   graph_node_t *root_node = root_block->cfg_node;
   list_t* removed_edges = NULL;   // contains all removed edges

   // remove entries
   FOREACH_INLIST(root_node->in, it_edges1) {
      graph_edge_t* ed = GET_DATA_T(graph_edge_t*, it_edges1);
      block_t* from_block = ed->from->data;

      if (from_block->loop == NULL
            || from_block->loop->global_id != l->global_id) {
         removed_edges = list_add_before(removed_edges, ed);
      }
   }

   // remove exits
   if (loop_get_exits(l) != NULL) {
      FOREACH_INLIST(loop_get_exits(l), it_exit)
      {
         block_t* exit_block = GET_DATA_T(block_t*, it_exit);
         graph_node_t *exit_node = exit_block->cfg_node;

         FOREACH_INLIST(exit_node->out, it_edges2)
         {
            graph_edge_t* ed = GET_DATA_T(graph_edge_t*, it_edges2);
            block_t* to_block = ed->to->data;

            if (to_block->loop == NULL
                  || to_block->loop->global_id != l->global_id) {
               removed_edges = list_add_before(removed_edges, ed);
            }
         }
      }
   }

   FOREACH_INLIST(removed_edges, it0) {
      graph_edge_t* ed = GET_DATA_T(graph_edge_t*, it0);
      _remove_edge(ed);
   }

   return removed_edges;
}

/*
 * Restores edges removed by remove_edges_for_subgraph
 * \param removed_edges a list of edges removed by remove_edges_for_subgraph
 */
static void restore_edges_for_subgraph(list_t* removed_edges)
{
   if (removed_edges == NULL)
      return;

   //here, must rebuild the CFG in order to get the original graph
   FOREACH_INLIST(removed_edges, it_ed) {
      graph_edge_t* ed = GET_DATA_T(graph_edge_t*, it_ed);
      _add_edge(ed);
   }

   list_free(removed_edges, NULL);
}

/*
 * Computes paths for a loop and overwrite its 'paths' field
 * Following loops are not considered:
 *  - more than one entry
 *  - paths already computed
 *  - more than LOOP_MAX_PATHS paths
 * \param l a loop
 */
void lcore_loop_computepaths(loop_t *l)
{
   if (LOOP_NB_ENTRIES (l) != 1 || loop_get_paths(l) != NULL)
      return;

   block_t *root_block = list_getdata(loop_get_entries(l));

   //here, must "change" the CFG in order to have a subgraph containing
   //only blocks from the loop l
   list_t* removed_edges = remove_edges_for_subgraph(l);

   compute_paths(root_block, LOOP_MAX_PATHS, &(l->paths));

   //here, must rebuild the CFG in order to get the original graph
   restore_edges_for_subgraph(removed_edges);
}

static void free_paths(queue_t *paths)
{
   if (paths == NULL)
      return;

   /* for each path */
   FOREACH_INQUEUE(paths, path_iter) {
      array_t *path = GET_DATA_T(array_t*, path_iter);

      array_free(path, NULL);
   }

   queue_free(paths, NULL);
}

/*
 * \brief frees the memory allocated for the paths of a function
 * \param fct_t *f a function
 */
void lcore_fct_freepaths(fct_t *f)
{
   free_paths(fct_get_paths(f));
   f->paths = NULL;
}

/*
 * \brief frees the memory allocated for the paths of a loop
 * \param loop_t *l a loop
 */
void lcore_loop_freepaths(loop_t *l)
{
   free_paths(loop_get_paths(l));
   l->paths = NULL;
}

/*
 * \brief returns the number of paths without computing them
 * \param fct_t* f the targeted loop
 */
int lcore_fct_getnpaths(fct_t *f)
{
   if (FCT_NB_ENTRIES (f) == 1) {
      const block_t *root_block = queue_peek_head(fct_get_entry_blocks(f));
      return graph_node_get_nb_paths(root_block->cfg_node, -1);
   }

   DBGMSG("FCT_NB_ENTRIES: %d\n", FCT_NB_ENTRIES (f));

   return -1;
}

/*
 * \brief returns the number of paths without computing them
 * \param loop_t* l the targeted loop
 * \param int take_branch decide whether to take
 * a cycle once encountered or not
 */
int lcore_loop_getnpaths(loop_t *l, int take_branch)
{
   if ((take_branch == FALSE) && (LOOP_NB_ENTRIES (l) == 1)) {
      list_t* removed_edges = remove_edges_for_subgraph(l);
      const block_t *root_block = list_getdata(loop_get_entries(l));
      int nb_path = graph_node_get_nb_paths(root_block->cfg_node, -1);
      restore_edges_for_subgraph(removed_edges);

      return nb_path;
   }

   /*DBGMSG("LOOP_NB_ENTRIES: %d\n", LOOP_NB_ENTRIES (l));
   DBGMSG("LOOP_NB_BLOCKS: %d\n", loop_get_nb_blocks(l));
   int i = 0;
   list_t* entries = loop_get_entries(l);
   for (i=0; i < LOOP_NB_ENTRIES (l); i++) {
      DBGMSG(" - entry n°%d: %lx\n", i, insn_get_addr(block_get_first_insn(list_getdata(entries))));
      entries = list_getnext(entries);
   }

   i = 0;
   queue_t* blocks = loop_get_blocks(l);
   FOREACH_INQUEUE(blocks, block_i) {
      block_t* block = GET_DATA_T(block_t*, block_i);
      DBGMSG(" - block n°%d: %lx\n", i, insn_get_addr(block_get_first_insn(block)));
      i++;
   }*/
      

   return -1;
}

/*
 * \brief check if a block "block" is an ancestor of a
 * "root" block obtained with a direct back edge from
 * "root" to "block"
 * \param block_t *root the block in the tail of the edge
 * \param block_t *block the block in the head of the edge
 */
int lcore_blocks_backedgenodes(block_t *root, block_t *block)
{
   loop_t *parent = root->loop;

   if (parent == NULL)
      return 0;

   list_t *sourceblocks = parent->entries;

   do {
      FOREACH_INLIST(sourceblocks, loopiter) {
         block_t* bl1 = GET_DATA_T(block_t*, loopiter);
         if (bl1->id == block->id)
            return 1;
      }

      if (parent->hierarchy_node != NULL
            && parent->hierarchy_node->parent != NULL) {
         parent = parent->hierarchy_node->parent->data;
         sourceblocks = parent->entries;
      } else
         parent = NULL;
   } while (parent != NULL);

   return 0;
}
