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

///////////////////////////////////////////////////////////////////////////////
//                                  block                                    //
///////////////////////////////////////////////////////////////////////////////
/*
 * Create a new empty block
 * \return a new block
 */
block_t* block_new(fct_t* fct, insn_t* insn)
{
   DBGMSG(
         "Creating new block in function %s beginning at instruction @ %#"PRIx64"\n",
         fct_get_name(fct), INSN_GET_ADDR(insn));

   if (fct == NULL)
      return PTR_ERROR;

   if (insn_get_block(insn) != NULL) {
      block_t* blk = insn->block;
      DBGMSG("Assertion: %s (%p) == %s (%p)\n", fct_get_name(blk->function),
            blk->function, fct_get_name(fct), fct);
      assert(blk->function == fct);
      return blk;
   }

   block_t *new = lc_malloc0(sizeof *new);

   new->id = queue_length(fct->blocks); //uniq_id;
   new->global_id = fct->asmfile->maxid_block++;
   DBGMSG("\tNew block has id %d\n", new->global_id);
   fct->asmfile->n_blocks++;

   if (insn != NULL)
      queue_add_tail(fct->blocks, new);

   new->domination_node = tree_new(new);
   new->postdom_node = NULL;
   new->function = fct;
   new->loop = NULL;
   new->is_loop_exit = 0;
   new->is_padding = -1;
   new->cfg_node = graph_node_new(new);

   if (insn != NULL) {
      new->begin_sequence = insn->sequence;
      new->end_sequence = insn->sequence;
      insn->block = new;
   } else {
      // virtual block case
      new->begin_sequence = NULL;
      new->end_sequence = NULL;
   }

   return new;
}

/*
 * Delete an existing block
 * \param p a pointer on a block to free
 */
void block_free(void* p)
{
   if (p == NULL)
      return;

   block_t* blk = p;

   lc_free(blk->domination_node);
   lc_free(blk->postdom_node);
   graph_node_free(blk->cfg_node, NULL, NULL);

   lc_free(blk);
}

/*
 * Retrieves the cell containing first instruction in instructions sequence
 * \param b a block
 * \return first instruction cell
 */
list_t* block_get_begin_sequence(block_t* b)
{
   return (b != NULL) ? b->begin_sequence : PTR_ERROR;
}

/*
 * Retrieves the cell containing last instruction in instructions sequence
 * \param b a block
 * \return last instruction cell
 */
list_t* block_get_end_sequence(block_t* b)
{
   return (b != NULL) ? b->end_sequence : PTR_ERROR;
}

/*
 * Retrieves block instructions
 * \param b a block
 * \return list of instructions
 */
list_t* block_get_insns(block_t* b)
{
   return block_get_begin_sequence(b);
}

/*
 * Retrieves a block id uniq for each block
 * \param b a block
 * \return b id, or SIGNED_ERROR is b is NULL
 */
unsigned int block_get_id(block_t* b)
{
   return (b != NULL) ? b->global_id : 0;
}

/*
 * Retrieves the first instruction of a block
 * \param b a block
 * \return the first instruction of b, or PTR_ERROR if b is NULL
 */
insn_t* block_get_first_insn(block_t* b)
{
   return list_getdata(block_get_begin_sequence(b));
}

/*
 * Retrieves the address of the first instruction of a block
 * \param b a block
 * \return the address of the first instruction of b, or SIGNED_ERROR if b is NULL
 */
int64_t block_get_first_insn_addr(block_t* b)
{
   return insn_get_addr(block_get_first_insn(b));
}

/*
 * Retrieves the last instruction of a block
 * \param b a block
 * \return the last instruction of b, or PTR_ERROR if b is NULL
 */
insn_t* block_get_last_insn(block_t* b)
{
   return list_getdata(block_get_end_sequence(b));
}

/*
 * Retrieves the address of the last instruction of a block
 * \param b a block
 * \return the address of the last instruction of b, or SIGNED_ERROR if b is NULL
 */
int64_t block_get_last_insn_addr(block_t* b)
{
   return insn_get_addr(block_get_last_insn(b));
}

/*
 * Retrieves the function the block belongs to
 * \param b a block
 * \return the function which contains b or PTR_ERROR if b is NULL
 */
fct_t* block_get_fct(block_t* b)
{
   return (b != NULL) ? b->function : PTR_ERROR;
}

/*
 * Retrieves the innmermost loop the block belongs to
 * \param b a block
 * \return the innermost loop which contains b or PTR_ERROR if b is NULL
 */
loop_t* block_get_loop(block_t* b)
{
   return (b != NULL) ? b->loop : PTR_ERROR;
}

/*
 * Retrieves the asmfile the block belongs to
 * \param b a block
 * \return the asmfile which contains b or PTR_ERROR if b is NULL
 */
asmfile_t* block_get_asmfile(block_t* b)
{
   fct_t *function = block_get_fct(b);
   return fct_get_asmfile(function);
}

/*
 * Retrieves the project the block belongs to
 * \param b a block
 * \return the project which contains b or PTR_ERROR if b is NULL
 */
project_t* block_get_project(block_t* b)
{
   asmfile_t *asmfile = block_get_asmfile(b);
   return asmfile_get_project(asmfile);
}

/*
 * Retrieves the block CFG node
 * \param b a block
 * \return the block CFG node or PTR_ERROR if b is NULL
 */
graph_node_t* block_get_CFG_node(block_t* b)
{
   return (b != NULL) ? b->cfg_node : PTR_ERROR;
}

/*
 * Returns predecessors (in the CFG) of a block
 * \param b a block
 * \return queue of blocks or PTR_ERROR if no CFG node
 */
queue_t * block_get_predecessors(block_t* b)
{
   graph_node_t *CFG_node = block_get_CFG_node (b);
   if (CFG_node == NULL) return PTR_ERROR;

   queue_t *new = queue_new();

   FOREACH_INLIST(CFG_node->in, iter) {
      block_t *block = GET_DATA_T (graph_edge_t *, iter)->from->data;
      queue_add_tail (new, block);
   }

   return new;
}

/*
 * Returns successors (in the CFG) of a block
 * \param b a block
 * \return queue of blocks or PTR_ERROR if no CFG node
 */
queue_t * block_get_successors(block_t* b)
{
   graph_node_t *CFG_node = block_get_CFG_node (b);
   if (CFG_node == NULL) return PTR_ERROR;

   queue_t *new = queue_new();

   FOREACH_INLIST(CFG_node->out, iter) {
      block_t *block = GET_DATA_T (graph_edge_t *, iter)->to->data;
      queue_add_tail (new, block);
   }

   return new;
}

/*
 * Retrieves the block domination node
 * \param b a block
 * \return the domination CFG node or PTR_ERROR if b is NULL
 */
tree_t* block_get_domination_node(block_t* b)
{
   return (b != NULL) ? b->domination_node : PTR_ERROR;
}

/*
 * Retrieves the block post domination node
 * \param b a block
 * \return the post domination CFG node or PTR_ERROR if b is NULL
 */
tree_t* block_get_postdom_node(block_t* b)
{
   return (b != NULL) ? b->postdom_node : PTR_ERROR;
}

/*
 * Check if a block is a function exit
 * \param b a block
 * \return TRUE if b is a function exit, else FALSE
 */
boolean_t block_is_function_exit(block_t* b)
{
   //Iterate over instructions in the block
   FOREACH_INSN_INBLOCK(b, it) {
      insn_t *insn = GET_DATA_T(insn_t*, it);

      if (insn_get_annotate(insn) & A_EX)
         return TRUE;
   }

   return FALSE;
}

/*
 * Check if a block is a loop entry
 * \param b a block
 * \return TRUE if b is a loop entry, else FALSE
 */
char block_is_loop_entry(block_t* b)
{
   return block_is_entry_of_loop(b, block_get_loop(b));
}

/*
 * Check if a block is a loop exit
 * \param b a block
 * \return TRUE if b is a loop exit, else FALSE
 */
char block_is_loop_exit(block_t* b)
{
   return (b != NULL) ? b->is_loop_exit : FALSE;
}

/*
 * Checks if a block is virtual (= it has no instructions)
 * \param b a block to test
 * \return TRUE if the block is virtual, else FALSE
 */
int block_is_virtual(block_t* b)
{
   if (b == NULL)
      return FALSE;

   return (b->begin_sequence == NULL) ? TRUE : FALSE;
}

/*
 * \brief tells if a given block is an entry of a given loop
 * \param block_t *block the potential entry
 * \param loop_t* loop the loop that contains the list of entries of the test
 * \return TRUE if the block an entry, FALSE if not
 */
int block_is_entry_of_loop(block_t *block, loop_t *loop)
{
   if (block == NULL)
      return FALSE;

   FOREACH_INLIST(loop_get_entries(loop), it) {
      block_t *b1 = GET_DATA_T(block_t*, it);
      if (block->global_id == b1->global_id)
         return TRUE;
   }
   return FALSE;
}

/*
 * \brief tells if a given block is an exit of a given loop
 * \param block_t *block the potential exit
 * \param loop_t* loop the loop that contains the list of exits ofthe test
 * \return TRUE if the block an exit, FALSE if not
 */
int block_is_exit_of_loop(block_t *block, loop_t *loop)
{
   if (block == NULL)
      return FALSE;

   FOREACH_INLIST(loop_get_exits(loop), it) {
      block_t *b1 = GET_DATA_T(block_t*, it);
      if (block->global_id == b1->global_id)
         return TRUE;
   }
   return FALSE;
}

/*
 * Gets the number of instructions in a block
 * \param b a block
 * \return the number of instructions in block b
 */
int block_get_size(block_t* b)
{
   if (b == NULL)
      return 0;

   int cpt = 0;
   FOREACH_INSN_INBLOCK(b, it) {
      cpt++;
   }
   return cpt;
}

/*
 * Retrieves the parent node associated to b in the dominant tree
 * \param b a block
 * \return a parent node (tree_t*) or PTR_ERROR if b or block_get_domination_node (b) is NULL
 */
tree_t* block_get_dominant_parent(block_t* b)
{
   tree_t *dnode = block_get_domination_node(b);
   return (dnode != NULL) ? tree_get_parent(dnode) : PTR_ERROR;
}

/*
 * Retrieves the children node associated to b in the dominant tree
 * \param b a block
 * \return a children node (tree_t*) or PTR_ERROR if b or block_get_domination_node (b) is NULL
 */
tree_t* block_get_dominant_children(block_t* b)
{
   tree_t *dnode = block_get_domination_node(b);
   return (dnode != NULL) ? tree_get_children(dnode) : PTR_ERROR;
}

/*
 * Retrieves the parent node associated to b in the post dominant tree
 * \param b a block
 * \return a parent node (tree_t*) or PTR_ERROR if b or block_get_postdom_node (b) is NULL
 */
tree_t* block_get_post_dominant_parent(block_t* b)
{
   tree_t *dnode = block_get_postdom_node(b);
   return (dnode != NULL) ? tree_get_parent(dnode) : PTR_ERROR;
}

/*
 * Retrieves the children node associated to b in the post dominant tree
 * \param b a block
 * \return a children node (tree_t*) or PTR_ERROR if b or block_get_postdom_node (b) is NULL
 */
tree_t* block_get_post_dominant_children(block_t* b)
{
   tree_t *dnode = block_get_postdom_node(b);
   return (dnode != NULL) ? tree_get_children(dnode) : PTR_ERROR;
}

/**
 * Used by block_is_padding
 * Returns register found in operand at a given position
 */
static reg_t* insn_get_reg(insn_t* insn, int pos)
{
   oprnd_t *oprnd = insn_get_oprnd(insn, pos);

   if (oprnd_is_reg(oprnd) == TRUE)
      return oprnd_get_reg(oprnd);

   if (oprnd_is_mem(oprnd) == TRUE)
      return oprnd_get_base(oprnd);

   return NULL;
}

/*
 * Check if a bloc contains padding: only NOP, MOV and LEA, with the same register (ex: MOV EAX,EAX     LEA 0x0(RAX),RAX)
 * \param b a basic block
 * \return 1 if it is padding, else 0
 */
int block_is_padding(block_t* b)
{
   if (b == NULL)
      return 0;

   // if padding already computed, do not need to do it again
   if (b->is_padding != -1)
      return (b->is_padding);

   // if it contains the first instruction of the function, it is not padding
   if (block_get_first_insn(b) == fct_get_first_insn(b->function)) {
      b->is_padding = 0;
      return 0;
   }

   // if block has predecessors, it is not padding
   if (block_get_CFG_node(b)->in != NULL) {
      b->is_padding = 0;
      return 0;
   }

   int res = 1;

   FOREACH_INSN_INBLOCK(b, it1) {
      insn_t* insn = GET_DATA_T(insn_t*, it1);
      unsigned short fam = insn_get_family(insn);

      // undefined family
      if (fam == FM_UNDEF) {
         res = 0;
         break;
      }

      // family not used to padd
      if ((fam != FM_NOP) && (fam != FM_LEA) && (fam != FM_MOV)
            && (fam != FM_XCHG)) {
         res = 0;
         break;
      }

      // LEA, MOV or XCHG: potentially used to pad if used with same register
      if (fam != FM_NOP && (insn_get_reg(insn, 0) != insn_get_reg(insn, 1))) {
         res = 0;
         break;
      }
   }

   if (res == 1) {
      b->is_padding = 1;
      queue_add_tail(fct_get_padding_blocks(b->function), b);
      return 1;
   }

   // To handle blocks added by madras during patch
   res = 1;
   FOREACH_INSN_INBLOCK(b, it2) {
      insn_t* insn = GET_DATA_T(insn_t*, it2);
      unsigned short fam = insn_get_family(insn);

      if (fam != FM_NOP) {
         insn_t* last_insn = block_get_last_insn(b);
         if ((last_insn != NULL)
               && (INSN_GET_ADDR (insn) == INSN_GET_ADDR(last_insn))
               && ((insn->annotate & A_JUMP) != 0)
               && (insn_get_branch(insn) != NULL
                     && (long) insn_get_branch(insn) > 0)
               && ((insn_get_branch(insn)->annotate & A_PATCHMOV) != 0)) {
         } else {
            res = 0;
            break;
         }
      }
   }

   b->is_padding = res;
   if (res == 1)
      queue_add_tail(fct_get_padding_blocks(b->function), b);

   return res;
}

/**
 * Used by block_is_*_exit
 * Check if last instruction is an exit of a given type
 */
static int block_is_exit(block_t* b, unsigned int type)
{
   insn_t* last_insn = block_get_last_insn(b);
   return (last_insn != NULL) ? insn_check_annotate(last_insn, type) : FALSE;
}

/*
 * Checks if the block is a natural exit block
 * \param b a basic block
 * \return TRUE if b is a natural exit, else FALSE
 */
int block_is_natural_exit(block_t* b)
{
   return block_is_exit(b, A_NATURAL_EX);
}

/*
 * Checks if the block is a potential exit block
 * \param b a basic block
 * \return TRUE if b is a potential exit, else FALSE
 */
int block_is_potential_exit(block_t* b)
{
   return block_is_exit(b, A_POTENTIAL_EX);
}

/*
 * Checks if the block is a early exit block
 * \param b a basic block
 * \return TRUE if b is a early exit, else FALSE
 */
int block_is_early_exit(block_t* b)
{
   return block_is_exit(b, A_EARLY_EX);
}

/*
 * Checks if the block is a handler exit block
 * \param b a basic block
 * \return TRUE if b is a handler exit, else FALSE
 */
int block_is_handler_exit(block_t* b)
{
   return block_is_exit(b, A_HANDLER_EX);
}

/*
 * Recursively checks if parent dominates child
 * \param parent a basic bloc
 * \param child a basic bloc
 * \return TRUE if parent dominates child, else FALSE
 */
int block_is_dominated(block_t* parent, block_t* child)
{
   tree_t *parent_domnode = block_get_domination_node(parent);
   tree_t *child_domnode = block_get_domination_node(child);

   if (parent_domnode == NULL || child_domnode == NULL)
      return FALSE;

   if (child_domnode->parent == parent_domnode)
      return TRUE;
   else if (child_domnode->parent == NULL)
      return FALSE;
   else
      return block_is_dominated(parent, child_domnode->parent->data);
}

/**
 * Find an instruction in the block that overwrites the
 * flags modified by the instruction corresponding to the
 * given opcode. Return NULL otherwise
 * */
insn_t * block_find_flag_overriding_insn(block_t *b, int opcode)
{
   uint8_t fs, set, cleared, def, undef;
   opcode_altered_flags(opcode, NULL, &set, &cleared, &def, &undef);
   fs = set | cleared | def | undef;

   FOREACH_INLIST(block_get_insns(b), it_b) {
      insn_t * i = GET_DATA_T(insn_t*, it_b);
      if (insn_flags_override_test(i->opcode, fs)) {
         return i;
      }
   }

   return NULL;
}

/**
 * Find an instruction in the block that overwrites the
 * flags modified by the INC instruction
 * */
insn_t * block_find_flag_overriding_insn_inc(block_t *b)
{
   return block_find_flag_overriding_insn(b, insn_inc_opcode());
}

/*
 * Returns path to the source file defining a block
 * \warning for some blocks instructions are defined in different files (according to insn_get_src_file)
 * In that case reference file is the one defined in the first instruction
 * \param block a block
 * \return path to source file (string)
 */
char *block_get_src_file_path (block_t *block)
{
   insn_t *first_insn = block_get_first_insn (block);

   return insn_get_src_file (first_insn);
}

/*
 * Provides first and last source line of a block
 * \see block_get_src_file_path
 * \param block a block
 * \param min pointer to first line rank
 * \param max pointer to last line rank
 */
void block_get_src_lines (block_t *block, unsigned *min, unsigned *max)
{
   *min = 0;
   *max = 0;

   char *block_file_path = block_get_src_file_path (block);
   if (block_file_path == NULL) return;

   FOREACH_INSN_INBLOCK(block, iti) {
      insn_t *insn = GET_DATA_T(insn_t*, iti);

      char *file_path = insn_get_src_file (insn);
      if (file_path == NULL || strcmp (file_path, block_file_path) != 0) continue;

      unsigned src_line = insn_get_src_line (insn);
      if (src_line == 0) continue;

      if (*min == 0 || *min > src_line) *min = src_line;
      if (*max == 0 || *max < src_line) *max = src_line;
   }
}

/*
 * Returns source regions for a set of blocks as a queue of strings.
 * Source regions are formatted as "<file>: <start_line>-<end_line>"
 * \param blocks queue of blocks
 * \return queue of strings
 */
queue_t *blocks_get_src_regions (queue_t *blocks) {
   queue_t *ret = queue_new();

   hashtable_t *index = hashtable_new (&str_hash, &str_equal);

   FOREACH_INQUEUE(blocks, it_b) {
      block_t *block = GET_DATA_T(block_t *, it_b);

      FOREACH_INSN_INBLOCK(block, iti) {
         insn_t *insn = GET_DATA_T(insn_t*, iti);

         char *file_path = insn_get_src_file (insn);
         if (file_path == NULL) continue;

         unsigned src_line = insn_get_src_line (insn);
         if (src_line == 0) continue;

         list_t *src_lines = hashtable_lookup (index, file_path);
         if (src_lines == NULL) {
            src_lines = list_new ((void *) (unsigned long) src_line);
            hashtable_insert (index, file_path, src_lines);
         } else {
            src_lines = list_add_after (src_lines, (void *) (unsigned long) src_line);
         }
      }
   }

   FOREACH_INHASHTABLE(index,it_hn) {
      char   *src_file  = GET_KEY(char *, it_hn);
      list_t *src_lines = GET_DATA_T(list_t *, it_hn);

      int min = -1, max = -1;
      FOREACH_INLIST (src_lines, it_line) {
         unsigned line = GET_DATA_T(unsigned long, it_line);
         if (min == -1 || min > line) min = line;
         if (max == -1 || max < line) max = line;         
      }
      list_free (src_lines, NULL);

      char *str = lc_malloc (strlen (src_file)+20);
      lc_sprintf (str, strlen (src_file)+20, "%s: %u-%u", src_file, min, max);
      queue_add_tail (ret, str);
   }

   hashtable_free (index, NULL, NULL);

   return ret;
}

/*
 * Returns source regions for a block
 * \see blocks_get_src_regions
 */
queue_t *block_get_src_regions (block_t *block) {
   queue_t *blocks = queue_new ();
   queue_add_tail (blocks, block);

   queue_t *ret = blocks_get_src_regions (blocks);
   queue_free (blocks, NULL);

   return ret;
}

/*
 * Sets a block unique id
 * \param b a block
 * \param global_id The new id for b
 * \warning USE WITH CAUTION
 */
void block_set_id(block_t* b, unsigned int global_id)
{
   if (b != NULL)
      b->global_id = global_id;
}

/*
 * Compares two blocks based on the addresses of their first instruction
 * \param b1 First block
 * \param b2 Second block
 * \return positive integer if the address of the first instruction of b1 is higher than the one of b2,
 * negative if it is lower, and 0 if both are equal
 */
int block_cmpbyaddr_qsort(const void* b1, const void* b2)
{
   block_t* block1 = *((block_t**) b1);
   block_t* block2 = *((block_t**) b2);
   int64_t addr1 = insn_get_addr(block_get_first_insn(block1));
   int64_t addr2 = insn_get_addr(block_get_first_insn(block2));

   return addr1 - addr2;
}
