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
#include "libmasm.h"
#include "libmcore.h"

///////////////////////////////////////////////////////////////////////////////
//                                   loop                                    //
///////////////////////////////////////////////////////////////////////////////
/*
 * Create a new empty loop
 * \return a new loop
 */
loop_t* loop_new(block_t* entry)
{
   fct_t* fct = block_get_fct(entry);
   if (fct == NULL)
      return PTR_ERROR;

   loop_t* new = lc_malloc0(sizeof *new);

   new->id = queue_length(fct->loops);
   new->global_id = fct_get_asmfile(fct)->maxid_loop++;
   new->entries = list_add_before(new->entries, entry);
   new->exits = NULL;
   new->function = fct;
   new->hierarchy_node = tree_new(new);
   new->paths = NULL;
   new->groups = NULL;
   new->blocks = queue_new();
   new->nb_insns = 0;

   // Connect new loop to block (entry) and parent function
   queue_add_tail(fct->loops, new);
   if (entry->loop) {
      tree_insert(entry->loop->hierarchy_node, new->hierarchy_node);
   }
   entry->loop = new;

   return new;
}

/*
 * Delete an existing loop and all data it contains
 * \param p a pointer on a loop to free
 */
void loop_free(void* p)
{
   if (p == NULL)
      return;

   loop_t* l = p;

   list_free(l->entries, NULL);
   list_free(l->exits, NULL);
   queue_free(l->blocks, NULL);
   lcore_loop_freepaths (l);
   lc_free(l->hierarchy_node);
   list_free(l->groups, &group_free);

   lc_free(l);
}

/*
 * Retrieves a uniq loop id
 * \param l a loop
 * \return l id or 0 if l is NULL
 */
unsigned int loop_get_id(loop_t* l)
{
   return (l != NULL) ? l->global_id : 0;
}

/*
 * Retrieves a list of all blocks which are l entries
 * \param l a loop
 * \return a list of entries or PTR_ERROR if l is NULL
 */
list_t* loop_get_entries(loop_t* l)
{
   return (l != NULL) ? l->entries : PTR_ERROR;
}

/*
 * Retrieves a list of all blocks which are l exits
 * \param l a loop
 * \return a list of exits or PTR_ERROR if l is NULL
 */
list_t* loop_get_exits(loop_t* l)
{
   return (l != NULL) ? l->exits : PTR_ERROR;
}

/*
 * Retrieves a list of all blocks which are in l
 * \param l a loop
 * \return a list of block or PTR_ERROR if l is NULL
 */
queue_t* loop_get_blocks(loop_t* l)
{
   return (l != NULL) ? l->blocks : PTR_ERROR;
}

/*
 * Retrieves the number of blocks which are in l
 * \param l a loop
 * \return the number of blocks
 */
int loop_get_nb_blocks(loop_t* l)
{
   return queue_length(loop_get_blocks(l));
}

/*
 * Retrieves the number of blocks which are in l, excluding virtual blocks
 * \param l a loop
 * \return the number of blocks
 */
int loop_get_nb_blocks_novirtual(loop_t* l)
{
   int nb = 0;

   FOREACH_INQUEUE(loop_get_blocks(l), it) {
      block_t* block = GET_DATA_T(block_t*, it);
      if (!block_is_virtual(block))
         nb++;
   }

   return nb;
}

/*
 * Retrieves paths of l
 * \param l a loop
 * \return a queue of paths or PTR_ERROR if l is NULL
 */
queue_t* loop_get_paths(loop_t* l)
{
   return (l != NULL) ? l->paths : PTR_ERROR;
}

/*
 * Retrieves a list of all groups which are in l
 * \param l a loop
 * \return a list of groups or PTR_ERROR if l is NULL
 */
list_t* loop_get_groups(loop_t* l)
{
   return (l != NULL) ? l->groups : PTR_ERROR;
}

/*
 * Retrieves a the function l belongs to
 * \param l a loop
 * \return a function or PTR_ERROR if l is NULL
 */
fct_t* loop_get_fct(loop_t* l)
{
   return (l != NULL) ? l->function : PTR_ERROR;
}

/*
 * Retrieves the asmfile containing l
 * \param l a loop
 * \return an asmfile or PTR_ERROR if l is NULL
 */
asmfile_t* loop_get_asmfile(loop_t* l)
{
   fct_t *function = loop_get_fct(l);
   return fct_get_asmfile(function);
}

/*
 * Retrieves the project containing l
 * \param l a loop
 * \return a project or PTR_ERROR if l is NULL
 */
project_t* loop_get_project(loop_t* l)
{
   asmfile_t *asmfile = loop_get_asmfile(l);
   return asmfile_get_project(asmfile);
}

/*
 * Retrieves a the hierarchy node associated to l
 * \param l a loop
 * \return a hierarchy node (tree_t*) or PTR_ERROR if l is NULL
 */
tree_t* loop_get_hierarchy_node(loop_t* l)
{
   return (l != NULL) ? l->hierarchy_node : PTR_ERROR;
}

/*
 * Retrieves the parent node associated to l
 * \param l a loop
 * \return a parent node (tree_t*) or PTR_ERROR if l or loop_get_hierarchy_node (l) is NULL
 */
tree_t* loop_get_parent_node(loop_t* l)
{
   tree_t *hnode = loop_get_hierarchy_node(l);

   return (hnode != NULL) ? tree_get_parent(hnode) : PTR_ERROR;
}

/*
 * Retrieves the children node associated to l
 * \param l a loop
 * \return a children node (tree_t*) or PTR_ERROR if l or loop_get_hierarchy_node (l) is NULL
 */
tree_t* loop_get_children_node(loop_t* l)
{
   tree_t *hnode = loop_get_hierarchy_node(l);

   return (hnode != NULL) ? tree_get_children(hnode) : PTR_ERROR;
}

/*
 * Adds a group in a loop
 * \param l an existing loop
 * \param g an existing group
 */
void loop_add_group(loop_t* l, group_t* g)
{
   if (l != NULL && g != NULL && l == g->loop)
      l->groups = list_add_before(l->groups, (void*) g);
}

/*
 * Gets the number of instructions in a loop
 * \param loop a loop
 * \return number of instructions
 */
int loop_get_nb_insns(loop_t* l)
{
   if (loop_get_blocks(l) == NULL)
      return (0);

   if (l->nb_insns == 0) {
      FOREACH_INQUEUE(loop_get_blocks(l), it)
      {
         block_t* b = GET_DATA_T(block_t*, it);
         l->nb_insns += block_get_size(b);
      }
   }
   return (l->nb_insns);
}

/*
 * Checks if a loop is innermost
 * \param loop a loop
 * \return boolean
 */
int loop_is_innermost(loop_t *loop)
{
   if (loop == NULL)
      return FALSE;

   tree_t *children_node = loop_get_children_node(loop);

   if (children_node == PTR_ERROR) {
      /* TODO: for the moment, an innermost loop can be in this case
       * => return SIGNED_ERROR if dominance not done */
      return TRUE;
   }

   return (children_node == NULL) ? TRUE : FALSE;
}

static int _rec_depth(loop_t *loop, int depth)
{
   loop_t *parent_loop = tree_getdata(loop_get_parent_node(loop));

   return (parent_loop != NULL) ? _rec_depth(parent_loop, depth + 1) : depth;
}

/*
 * Returns depth of a loop
 * \param loop a loop
 * \return depth or SIGNED_ERROR if failure
 */
int loop_get_depth(loop_t *loop)
{
   if (loop == NULL)
      return SIGNED_ERROR;

   return _rec_depth(loop, 0);
}

/*
 * Returns backedges of a loop
 * A backedge E for the loop L is such as E->to is an entry block for L and E->from belongs to L
 * \param loop a loop
 * \return queue of CFG edges
 */
queue_t *loop_get_backedges (loop_t *loop)
{
   queue_t *new = queue_new();

   // For each entry block
   FOREACH_INLIST (loop_get_entries (loop), itb) {
      block_t *block = GET_DATA_T (block_t*, itb);

      // For each predecessor (incoming edge to the CFG node)
      graph_node_t *CFG_node = block_get_CFG_node (block);
      FOREACH_INLIST (CFG_node ? CFG_node->in : NULL, ite) {
         graph_edge_t *edge = GET_DATA_T (graph_edge_t *, ite);

         block_t *pred_block = edge->from->data;
         loop_t *pred_loop = block_get_loop (pred_block);

         if (loop_get_id (pred_loop) == loop_get_id (loop))
            queue_add_tail (new, edge);
      }
   }

   return new;
}

/*
 * Returns backedge instructions of a loop (instructions branching to same loop)
 * \param loop a loop
 * \return queue of instructions
 */
queue_t *loop_get_backedge_insns (loop_t *loop)
{
   queue_t *new = queue_new();

   // For each backedge
   queue_t *backedges = loop_get_backedges (loop);
   FOREACH_INQUEUE (backedges, ite) {

      // Insert to queue last instruction of the related predecessor block
      graph_edge_t *edge = GET_DATA_T (graph_edge_t *, ite);
      block_t *pred = edge->from->data;
      queue_add_tail (new, block_get_last_insn (pred));
   }
   queue_free (backedges, NULL);

   return new;
}

/*
 * Returns the first backedge instruction of a loop
 */
static insn_t *loop_get_first_backedge_insn (loop_t *loop)
{
   queue_t *backedge_insns = loop_get_backedge_insns (loop);
   insn_t *first_insn = queue_peek_head (backedge_insns);
   queue_free (backedge_insns, NULL);

   return first_insn;
}

/*
 * Returns path to the source file defining a loop
 * \warning for some loops instructions are defined in different files (according to insn_get_src_file)
 * In that case returning the one defined in the first instruction (in the first entry block)
 * \param loop a loop
 * \return path to source file (string)
 */
char *loop_get_src_file_path (loop_t *loop)
{
   insn_t *first_insn = loop_get_first_backedge_insn (loop);

   return insn_get_src_file (first_insn);
}

/*
 * Provides first and last source line of a loop
 * \see loop_get_src_file_path
 * \param loop a loop
 * \param min pointer to first line rank
 * \param max pointer to last line rank
 */
void loop_get_src_lines (loop_t *loop, unsigned *min, unsigned *max)
{
   *min = 0;
   *max = 0;

   char *loop_file_path = loop_get_src_file_path (loop);
   if (loop_file_path == NULL) return;

   FOREACH_INQUEUE(loop_get_blocks(loop), itb) {
      block_t* block = GET_DATA_T(block_t*, itb);

      FOREACH_INSN_INBLOCK(block, iti) {
         insn_t *insn = GET_DATA_T(insn_t*, iti);

         char *file_path = insn_get_src_file (insn);
         if (file_path == NULL || strcmp (file_path, loop_file_path) != 0) continue;

         unsigned src_line = insn_get_src_line (insn);
         if (src_line == 0) continue;

         if (*min == 0 || *min > src_line) *min = src_line;
         if (*max == 0 || *max < src_line) *max = src_line;
      }
   }
}

/*
 * Returns source regions for a loop
 * \see blocks_get_src_regions
 */
queue_t *loop_get_src_regions (loop_t *loop)
{
   return blocks_get_src_regions (loop_get_blocks (loop));
}

/*
 * Sets the unique id for a loop
 * \param l a loop
 * \param global_id The new id for the loop
 * \warning USE WITH CAUTION
 */
void loop_set_id(loop_t* l, unsigned int global_id)
{
   if (l != NULL)
      l->global_id = global_id;
}
