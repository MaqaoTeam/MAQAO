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
//#define DEFAULT_INIT "___NO_INIT___"
//#define DEFAULT_TXT  "___NO_TXT___"
//#define DEFAULT_FINI "___NO_FINI___"
#define DEFAULT_STRIPSCN "___STRIPED_%.4d___"  /**<Default label name to set for a striped section*/

/**
 * Used only for flow graph analysis
 */
typedef struct current_s {
   asmfile_t *prog; /**<current asmfile*/
   block_t *block; /**<current block*/
   fct_t *function; /**<current function*/
   insn_t *previous; /**<previous instruction*/
   project_t* project; /**<current project*/
   label_t* label; /**<A default label added at the begining of the file if needed*/
   list_t* block_to_move; /**<a list of blocks in madras.code section which have to been moved in another function*/
   hashtable_t* labels_new_block; /**<A hashtable of label where key is an instruction. These labels are added by MAQAO
    to force the creation of a new basic block*/
} current_t;

//-----------------------------------------------------------------------------------
/*
 * Gets the instruction target by a branch
 * \param insn a branch instruction
 * \return the instruction targeted by the branch, else NULL
 */
static inline insn_t *find_branch_target(insn_t *insn)
{
   int i = 0;
   for (i = 0;
         i < insn_get_nb_oprnds(insn)
               && oprnd_is_ptr(insn_get_oprnd(insn, i)) == FALSE; i++) {
   }
   if (i < insn_get_nb_oprnds(insn))
      return pointer_get_insn_target(oprnd_get_ptr(insn_get_oprnd(insn, i)));
   return NULL;
}

/*
 * Moved a block attached to a function in another function
 * \param b a block to moved in another function
 * \param fct the new block function
 */
static void steal_block(block_t* b, fct_t* fct)
{
   if (b == NULL || fct == NULL)
      return;
   if (b->function == fct)
      return;

   fct_t* old = b->function;
   label_t* lbl = insn_get_fctlbl(block_get_first_insn(FCT_ENTRY(fct)));

   // if the stealen block is the first block of the function
   if (queue_peek_head(old->blocks) == b) {
      queue_remove_head(old->blocks);
      block_t* tmp = NULL;

      // get the first next instruction which is not in the same block
      // and set it as the first instruction
      FOREACH_INLIST(block_get_first_insn(b)->sequence, it) {
         insn_t* i = GET_DATA_T(insn_t*, it);
         if (i->block != b) {
            tmp = i->block;
            break;
         }
      }
      old->first_insn = block_get_first_insn(tmp);
      queue_add_tail(fct->blocks, b);
   } else {
      queue_remove(old->blocks, b, NULL);
      queue_add_tail(fct->blocks, b);
   }
   b->function = fct;

   // Checks that all successors are NOT a JUMP to another function
   // ex: JMP printf@plt
   list_t* _to_remove = NULL;
   FOREACH_INLIST(b->cfg_node->out, it_edge) {
      graph_edge_t* ed = GET_DATA_T(graph_edge_t*, it_edge);
      block_t* dst = (block_t*) ed->to->data;

      if (b->function != dst->function
            && strcmp(fct_get_name(dst->function), LABEL_PATCHMOV) != 0) {
         // delete the edge in the cfg
         _to_remove = list_add_before(_to_remove, ed);

         // add an edge in the cg
         graph_add_edge(fct_get_CG_node(b->function),
               fct_get_CG_node(dst->function), NULL);
      }
   }

   FOREACH_INLIST(_to_remove, _it) {
      graph_edge_t* ed = GET_DATA_T(graph_edge_t*, _it);
      graph_remove_edge(ed, NULL);
   }
   list_free(_to_remove, NULL);

   // Update the field fctlbl for each instruction in the block
   FOREACH_INSN_INBLOCK(b, it)
   {
      insn_t* in = GET_DATA_T(insn_t*, it);
      in->fctlbl = lbl;
   }
}

/*
 * Finds the function a patched block belongs to
 * \param b a block
 * \return a function, else NULL
 */
static fct_t* find_stealing_function(block_t* b)
{
   // This function traveres CFG edges while the other block belongs to the same
   // function than b. When it is another function, it is returned

   graph_node_t* node = block_get_CFG_node(b);

   if (node->in != NULL) {
      FOREACH_INLIST(node->in, it)
      {
         graph_edge_t* ed = GET_DATA_T(graph_edge_t*, it);
         block_t* src = (block_t*) ed->from->data;
         if (src->function != b->function)
            return (src->function);
      }
   }

   if (node->out != NULL) {
      FOREACH_INLIST(node->out, it)
      {
         graph_edge_t* ed = GET_DATA_T(graph_edge_t*, it);
         block_t* dst = (block_t*) ed->to->data;
         if (dst->function != b->function)
            return (dst->function);
      }
   }
   return (NULL);
}

static insn_t* change_label_stripped(asmfile_t* asmf, int section,
      insn_t* start_insn, char* strlabel)
{
   label_t* label = NULL;
   label_t* lbl = NULL;
   insn_t* in = NULL;

   // move the cursor (in) at the first .fini instruction
   list_t* seq = start_insn->sequence;
   FOREACH_INLIST(seq, it5) {
      in = GET_DATA_T(insn_t*, it5);
      if ((insn_get_annotate(in) & section))
         break;
   }

   // check if the first .txt instruction has an associated label
   // label != NULL if true
   // lbl is the first label AFTER the first .txt instruction
   FOREACH_INQUEUE(asmf->label_list, it4) {
      lbl = GET_DATA_T(label_t*, it4);

      if (label_get_addr(lbl) == INSN_GET_ADDR(in)) {
         label = lbl;
         break;
      } else if (label_get_addr(lbl) > INSN_GET_ADDR(in))
         break;
   }

   // if no label, a default label is created and associated to each instructions while:
   //    - instructions are in section .fini
   //    - instructions are BEFORE lbl (the first label after the first instruction)
   if (label == NULL) {
      assert(0);
      /**\todo TODO (2015-06-01) This case should never happen anymore, as the disassembler enforces that labels are added at the beginning
       * of sections lacking one. If this assert never triggers for some time (or if we come up a way to test it never does), we could simply
       * remove this whole function and those invoking it. Otherwise, if it can legitimately happen, simply remove the assert*/
      label_t* lbl_ = label_new(strlabel, INSN_GET_ADDR(in), TARGET_INSN, in);
      asmfile_add_label(asmf, lbl_);

      seq = in->sequence;
      FOREACH_INLIST(seq, it4)
      {
         in = GET_DATA_T(insn_t*, it4);
         if ((insn_get_annotate(in) & section)
               == 0|| label_get_addr(lbl) == INSN_GET_ADDR (in))
            break;
         in->fctlbl = lbl_;
      }
   }
   return (in);
}

//-----------------------------------------------------------------------------------
static void init_label_new_blocks(asmfile_t* asmfile, current_t* current)
{
   // Look for all labels whose name is LABEL_NEW_BLOCK and copy them into
   // current->labels_new_block
   queue_t* labels = asmfile_get_labels(asmfile);
   if (labels == NULL)
      return;

   FOREACH_INQUEUE(labels, it)
   {
      label_t* label = GET_DATA_T(label_t*, it);
      if (strcmp(label_get_name(label), LABEL_NEW_BLOCK) == 0) {
         if (label_get_target_type(label) == TARGET_INSN)
            hashtable_insert(current->labels_new_block, label_get_target(label),
                  label);
      }
   }
}
/*
 * Initializes a stripped assembly file.
 * It updates labels of sections .init, .txt and .fini in order to create default
 * functions for these three sections.
 * \param asmf an assembly file
 */
static void init_for_stripped(asmfile_t* asmf)
{
   /**\todo TODO (2015-06-01) I think the case should never happen now that the disassembler enforces the addition
    * of a label at the beginning of sections that lack one, but I'm keeping the code here just to be sure*/
   insn_t* in = queue_peek_head(asmf->insns);
   binfile_t* bf = asmfile_get_binfile(asmf);

   uint16_t i;
   for (i = 0; i < binfile_get_nb_code_scns(bf); i++) {
      binscn_t* scn = binfile_get_code_scn(bf, i);
      int anno = binscn_get_attrs(scn);
      if ((anno & SCNA_STDCODE) && !(anno & SCNA_EXTFCTSTUBS)) {
         char lblbuf[strlen(DEFAULT_STRIPSCN) + 1];
         /**\note Lucky case, it so happens here that the size of DEFAULT_STRIPSCN including the formatting of the string
          * ends up equal to the length of the final string. Don't forget to update the initialisation above if the formatting
          * changes (for instance if the length of the numerical value changes)*/
         sprintf(lblbuf, DEFAULT_STRIPSCN, i);
         in = change_label_stripped(asmf, A_STDCODE,
               GET_DATA_T(insn_t*, binscn_get_first_insn_seq(scn)), lblbuf);
      }
   }
//   in = change_label_stripped (asmf, A_SCTINI, in, DEFAULT_INIT);
//   in = change_label_stripped (asmf, A_SCTTXT, in, DEFAULT_TXT);
//   in = change_label_stripped (asmf, A_SCTFIN, in, DEFAULT_FINI);
}

/*
 * Find block beginning
 * \param p a pointer on an instruction
 * \param user a current_t structure
 */
static void flow_init(void *p, void *user)
{
   current_t *current = (current_t *) user;
   insn_t *insn = (insn_t *) p;
   int anno = insn_get_annotate(insn);

   if (!asmfile_get_parameter(current->prog, PARAM_MODULE_LCORE,
         PARAM_LCORE_FLOW_ANALYZE_ALL_SCNS)) {
      if (!(anno & (A_STDCODE | A_PATCHED)) || (anno & A_EXTFCT))
         return;
   }

   // If current instruction is a JUMP, tag the jump target as a block beginning
   if ((anno & A_JUMP) && !(anno & A_RTRN)) {
      insn_t *r = find_branch_target(insn); /**\todo (2017-11-24) If there is no valid reason that prevents to use insn_get_branch here, use it instead*/
      if (r != NULL) {
         r->annotate |= A_BEGIN_BLOCK;
         DBGMSG("branch from 0x%"PRIx64" to 0x%"PRIx64"\n", INSN_GET_ADDR(insn),
               INSN_GET_ADDR(r));
      }
   }

   // If current instruction has the label LABEL_NEW_BLOCK, tag the instruction as
   // a block beginning
   if (hashtable_lookup(current->labels_new_block, insn) != NULL) {
      insn->annotate |= A_BEGIN_BLOCK;
      DBGMSG("._maqao_new_block label found at 0x%"PRIx64"\n",
            INSN_GET_ADDR(insn));
   }

   if (current->function == NULL
         || ((label_get_name(insn_get_fctlbl(insn)) != NULL)
               && strcmp(fct_get_name(current->function),
                     label_get_name(insn_get_fctlbl(insn))))) {
      current->function = fct_new(current->prog, insn_get_fctlbl(insn), insn);
      DBGMSG("found function %s => 0x%"PRIx64"\n",
            label_get_name(insn_get_fctlbl(insn)), INSN_GET_ADDR(insn));
      insn->annotate |= A_BEGIN_PROC;
   }

   //Detects if the instruction is a call to a handler (exit) function
   if (anno & A_CALL) {
      char** exit_fcts = project_get_exit_fcts(current->project);
      if (exit_fcts != NULL) {
         char* lblname = label_get_name(insn_get_fctlbl(insn_get_branch(insn)));
         if (lblname != NULL) {
            size_t i = 0;
            //Scans the list of exit function names and checks if the branch destination matches one of them
            while (exit_fcts[i] != NULL) {
               char* name = exit_fcts[i];
               size_t j = 0;
               //Guess it's faster that way than using strcmp since we want either identical names or those with an @plt suffix
               while (name[j] != '\0' && lblname[j] != '\0' && name[j] == lblname[j])
                  j++;
               if (name[j] == '\0') {
                  //Match with the exit function name: checking whether we are at the end of the function name
                  if (lblname[j] == '\0') {
                     //Found a match with the function name
                     DBGMSGLVL(1, "Instruction at address %#"PRIx64" is a call to function %s which performs an exit: treated as a RET\n",
                           insn_get_addr(insn), lblname);
                     insn_add_annotate(insn, A_HANDLER_EX);
                     break;   // No need to look further
                  } else if (strcmp(lblname + j, EXT_LBL_SUF) == 0) {
                     //Found a match with the function name in the plt
                     DBGMSGLVL(1, "Instruction at address %#"PRIx64" is a call to dynamic function %s which performs an exit: treated as a RET\n",
                           insn_get_addr(insn), lblname);
                     insn_add_annotate(insn, A_HANDLER_EX);
                     break;   // No need to look further
                  }  // No match
               }
               i++;
            }
         }  // Destination has no label
      }  // No exit functions defined
   } // Not a call
}

static current_t* build_graph_jump(insn_t* insn, current_t* current)
{
   insn_t* r = find_branch_target(insn);
   block_t *b = NULL;
   fct_t* f = NULL;

   if (!current->function) {
      HLTMSG("JUMP not in a function\n");
   }

   if (r != NULL) {
      // jump to another function
      if (insn_get_fctlbl(r)
            && strcmp(label_get_name(insn_get_fctlbl(r)),
                  fct_get_name(current->function)) != 0) {
         DBGMSG("Jumping from function %s to function %s\n",
               fct_get_name(current->function),
               label_get_name(insn_get_fctlbl(r)));
         if (insn_get_fctlbl(r) == NULL)
            return (current);
         f = (fct_t *) hashtable_lookup(current->prog->ht_functions,
               insn_get_fctlbl(r));
         if (f == NULL) {
            f = fct_new(current->prog, insn_get_fctlbl(r), r);
            DBGMSG("Creating function %s\n", label_get_name(insn_get_fctlbl(r)));
            b = block_new(f, r);
            DBGMSG("  with block %d of %s#%s ***\n", block_get_id(b),
                  fct_get_name(f), label_get_name(insn_get_fctlbl(r)));
         }

         // Special case when the current instruction targets a moved block or when
         // a block is targeted by a patched instruction
         // Create an edge in the CFG instead of in the CG. The block in the patched
         // section will be moved a the end of the "main" function
         if ((((insn_get_annotate(insn) & (A_PATCHED))
               && (label_get_type(insn_get_fctlbl(insn)) == LBL_PATCHSCN)))
               || (((insn_get_annotate(r) & (A_PATCHED))
                     && (label_get_type(insn_get_fctlbl(r)) == LBL_PATCHSCN)))) {
            b = block_new(f, r);
            if ((insn_get_annotate(r) & (A_PATCHED))
                  && (label_get_type(insn_get_fctlbl(r)) == LBL_PATCHSCN))
               current->block_to_move = list_add_before(current->block_to_move,
                     b);

            graph_add_edge(block_get_CFG_node(current->block),
                  block_get_CFG_node(b), insn);
         } else {
            if ((insn_get_annotate(r) & A_JUMP)
                  && (insn_get_annotate(find_branch_target(r)) & (A_PATCHED))) {
               DBGMSG("Trampoline detected at address 0x%"PRIx64"\n",
                     INSN_GET_ADDR(r));
               b = block_new(f, r);
               current->block_to_move = list_add_before(current->block_to_move,
                     b);
               graph_add_edge(block_get_CFG_node(current->block),
                     block_get_CFG_node(b), insn);
            } else
               graph_add_edge(fct_get_CG_node(current->function),
                     fct_get_CG_node(f), NULL);
         }
      } else {
         // jump to the same function
         b = block_new(current->function, r);

         assert(
               strcmp(label_get_name(insn_get_fctlbl(r)),
                     fct_get_name(current->function)) == 0);
         DBGMSG("setting jump target: block %d in %s\n", block_get_id(b),
               fct_get_name(current->function));

         // Link the current block to the block attained through the jump
         graph_add_edge(block_get_CFG_node(current->block),
               block_get_CFG_node(b), insn);
      }
   } else {
      DBGMSG0("INDIRECT JUMP NOT CORRECTLY HANDLED\n");
   }
   return (current);
}

static current_t* build_graph_call(insn_t* insn, current_t* current)
{
   insn_t* r = find_branch_target(insn);

   if (!current->function) {
      HLTMSG("CALL not in a function\n");
   }
   // If function found inside the code
   if (r != NULL && insn_get_fctlbl(r) != NULL) {
      fct_t* f = (fct_t *) hashtable_lookup(current->prog->ht_functions,
            insn_get_fctlbl(r));
      if (f == NULL) {
         f = fct_new(current->prog, insn_get_fctlbl(r), r);
         DBGMSG("creating function %s\n", label_get_name(insn_get_fctlbl(r)));

#ifndef NDEBUG
         block_t *b =
#endif
               block_new(f, r);
         assert(
               strcmp(fct_get_name(f), label_get_name(insn_get_fctlbl(r)))
                     == 0);
         DBGMSG("  new block %d of %s#%s\n", block_get_id(b), fct_get_name(f),
               label_get_name(insn_get_fctlbl(r)));
      }
      graph_add_edge(fct_get_CG_node(current->function), fct_get_CG_node(f),
            NULL);
   } else {
      DBGMSG0(
            "INDIRECT CALL OR CALL TO NON-REFERENCED FUNCTION NOT CORRECTLY HANDLED\n");
   }
   return (current);
}

/**
 * Analysis of call graph and control flow graph:
 * - field fctlbl of instructions is assumed to be correctly filed with
 *   the function name and is used to detect function boundaries.
 * The field annotate of instructions must follow the guidelines:
 * - A_RTRN, A_CALL and A_JUMP are assumed to be set according to
 *   instruction semantics. A_CONDITIONAL must be set if control can
 *   pass to following instruction, for A_RTRN and A_JUMP kind of
 *   instructions (case of predicated returns and conditional
 *   jumps). A_CALL is assumed to possibly always return, except if A_HANDLER was set.
 * - A_BEGIN_BLOCK is set to indicate that instruction is the target of
 *   a branch. This is set by function flow_init. Note that
 *   instructions following a branch, call or return are not tagged
 *   with this flag, even though they are first instructions of a
 *   block.
 * \param i a pointer on an instruction
 * \param user a current_t structure
 */
static void build_graph(void *i, void *user)
{
   current_t*current = (current_t *) user;
   insn_t* insn = (insn_t *) i;
   block_t *b = NULL;
   int anno = insn_get_annotate(insn);
   if (!asmfile_get_parameter(current->prog, PARAM_MODULE_LCORE,
         PARAM_LCORE_FLOW_ANALYZE_ALL_SCNS)) {
      if (!(anno & (A_STDCODE | A_PATCHED)) || (anno & A_EXTFCT))
         return;
   }

   // First instruction of a function: this is also a new block
   if (anno & A_BEGIN_PROC) {
      current->function = (fct_t *) hashtable_lookup(
            current->prog->ht_functions, label_get_name(insn_get_fctlbl(insn)));
      current->previous = NULL;
      DBGMSG("*** FUNCTION %s ****\n", label_get_name(insn_get_fctlbl(insn)));
   }

   // If this is the first instruction of a new block, create it and link it to previous block
   if ((anno & (A_BEGIN_BLOCK | A_BEGIN_PROC))
         || (current->previous != NULL
               && (insn_get_annotate(current->previous)
                     & (A_JUMP | A_RTRN | A_CALL)))) {
      b = block_new(current->function, insn);

      DBGMSG("*** New block %d ***\n", block_get_id(b));
      if (current->previous) {
         if ( !(insn_get_annotate(current->previous) & (A_JUMP | A_RTRN | A_HANDLER_EX))
               || (insn_get_annotate(current->previous) & A_CONDITIONAL)) {
            graph_add_edge(block_get_CFG_node(current->block),
                  block_get_CFG_node(b), insn);
            DBGMSG("   link from %d to %d\n", block_get_id(current->block),
                  block_get_id(b));
         } else {
            DBGMSG("  no link between %d and %d\n",
                  block_get_id(current->block), block_get_id(b));
         }
      }
      current->block = b;
   }
   // Exclusive flags: jump, return and call
   switch (anno & (A_JUMP | A_RTRN | A_CALL)) {
   case A_JUMP:
      current = build_graph_jump(insn, current);
      break;

   case A_CALL:
      current = build_graph_call(insn, current);
      break;

   case A_RTRN:
   default:
      break;
   }

   //DBG(insn_printdump(insn,buffer); fprintf(stderr,"%s\n",buffer););
   current->previous = insn;
   if (insn_get_block(insn) == NULL)
      insn->block = current->block;
   add_insn_to_block(insn, current->block);
}

//-----------------------------------------------------------------------------------
/*
 * Builds the control flow graph and the call graph of the asmfile.
 * \param asmfile an existing asmfile
 */
void lcore_analyze_flow(asmfile_t *asmfile)
{
   current_t current;
   if (asmfile != NULL && (asmfile->analyze_flag & DIS_ANALYZE) == 0)
      return; /**\todo add a better error management*/

   current.block = NULL;
   current.function = NULL;
   current.prog = asmfile;
   current.previous = 0;
   current.project = asmfile->project;
   current.label = NULL;
   current.block_to_move = NULL;
   current.labels_new_block = hashtable_new(&direct_hash, &direct_equal);

   fct_t *f = NULL;

   DBGMSG0("computing control flow graph\n");
   DBGLVL(1, FCTNAMEMSG0("List of exit function names is: ");
            char** exit_fcts = project_get_exit_fcts(asmfile_get_project(asmfile));
            if (exit_fcts != NULL) {
               int _dfct = 0;while(exit_fcts[_dfct] != NULL) STDMSG("%s ", exit_fcts[_dfct++]);}
            STDMSG("\n");)
   init_label_new_blocks(asmfile, &current);
   init_for_stripped(asmfile);
   queue_foreach(asmfile_get_insns(asmfile), flow_init, &current);
   queue_foreach(asmfile_get_insns(asmfile), build_graph, &current);

   hashtable_free(current.labels_new_block, NULL, NULL);

   // Code used to analyze a patched binary -----------------------------------
   // current.block_to_move is a list of basic blocks containing blocks located
   // in madras.code section, and targeted by a branch in another function. These
   // blocks will be moved from the madras.code default function to their real function.
   if (current.block_to_move != NULL) {
      FOREACH_INLIST(current.block_to_move, it) {
         block_t* b = GET_DATA_T(block_t*, it);

         fct_t* stealer = find_stealing_function(b);
         DBGMSG(
               "Block %d (0x%"PRIx64") from function %s must go in function %s [0]\n",
               b->global_id, ((insn_t* )b->begin_sequence->data)->address,
               fct_get_name(block_get_fct(b)), fct_get_name(stealer));
         steal_block(b, stealer);
      }

      // Then, there are probably other blocks in the madras.code default function.
      // These blocks will be moved in the function the previous lexicographic block belongs to.
      fct_t* fct_patchmov = NULL;
      FOREACH_INQUEUE(asmfile->functions, it0) {
         f = GET_DATA_T(fct_t*, it0);
         label_t* fctlbl = insn_get_fctlbl(fct_get_first_insn(f)); //Retrieves the label associated to this function
         // Get the patchmov function and remove all blocks in it
         if (label_get_type(fctlbl) == LBL_PATCHSCN) {

            FOREACH_INLIST(f->first_insn->sequence, it_insn) {
               insn_t* in = GET_DATA_T(insn_t*, it_insn);
               if (label_get_type(insn_get_fctlbl(in)) == LBL_PATCHSCN) {
                  assert(in->sequence != NULL && in->sequence->prev != NULL);
                  insn_t* tin = INSN_GET_PREV(in);
                  DBGMSG(
                        "Block %d (0x%"PRIx64") from function %s must go in function %s [2]\n",
                        in->block->global_id, in->address,
                        fct_get_name(block_get_fct(in->block)),
                        fct_get_name(tin->block->function));
                  steal_block(in->block, tin->block->function);
               }
            }
            fct_patchmov = f;
         }
      }
      if ((fct_patchmov != NULL)
            && (queue_length(fct_get_blocks(fct_patchmov)) == 0)) {
         //Freeing the function only if it's empty, in case it contains some blocks that could not be stolen (happens with DynInst)
         queue_remove(asmfile->functions, fct_patchmov, &fct_free);
      }

      // =================================================
      // Some stolen blocks can have wrong edges, remove them
      {
         list_t* ed_to_remove = NULL;
         FOREACH_INQUEUE(asmfile_get_fcts(asmfile), fct_it) {
            fct_t* f = GET_DATA_T(fct_t*, fct_it);
            FOREACH_INQUEUE(f->blocks, it_b)
            {
               block_t* b = GET_DATA_T(block_t*, it_b);
               FOREACH_INLIST(b->cfg_node->in, it_in)
               {
                  graph_edge_t* ed = GET_DATA_T(graph_edge_t*, it_in);
                  block_t* in = ed->from->data;
                  if (in->function != f)
                     ed_to_remove = list_add_before(ed_to_remove, ed);
               }
            }
         }
         FOREACH_INLIST(ed_to_remove, it_ed)
         {
            graph_edge_t* ed = GET_DATA_T(graph_edge_t*, it_ed);
            graph_remove_edge(ed, NULL);
         }
      }
      // End of the code used to analyze a patched binary ------------------------
   }

   DBGMSG0("Indirect branch handling\n");
   // Update the CFG to solve indirect branch
   FOREACH_INQUEUE(asmfile_get_fcts(asmfile), iter) {
      f = GET_DATA_T(fct_t*, iter);
      lcore_solve_using_cmp(f);
   }
   DBGMSG0("CFG updated for indirect branches\n");

   // Update functions to find padding blocks and remove them from the CFG ----
   FOREACH_INQUEUE(asmfile_get_fcts(asmfile), fct_it) {
      fct_t* f = GET_DATA_T(fct_t*, fct_it);
      FOREACH_INQUEUE(fct_get_blocks(f), b_it)
      {
         block_t* b = GET_DATA_T(block_t*, b_it);
         if (block_is_padding(b) == 1) {
            DBGMSG("Block %d is a padding block\n", block_get_id(b));
            while (block_get_CFG_node(b)->out != NULL) {
               list_t* edge_it = b->cfg_node->out;
               DBGMSG("edge deleted from %d to %d\n",
                     ((block_t* )((graph_edge_t* )edge_it->data)->from->data)->global_id,
                     ((block_t* )((graph_edge_t* )edge_it->data)->to->data)->global_id);
               graph_remove_edge((graph_edge_t*) edge_it->data, NULL);
            }
         }
      }
   }
   asmfile->analyze_flag |= CFG_ANALYZE;
}
