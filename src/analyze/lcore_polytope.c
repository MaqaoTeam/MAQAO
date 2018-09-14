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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "libmcommon.h"
#include "libmasm.h"
#include "libmcore.h"

typedef struct poly_context_s poly_context_t;

extern char* polytope_tostring(polytope_t* polytope);

/*
 * \struct poly_context_s
 * Context for polytope analysis
 */
struct poly_context_s {
   arch_t* arch; /**< Current architecture*/
   fct_t* f; /**< Current function*/
   queue_t** polytopes; /**< Array of queue of polytopes. Array
    indexed by function loop ids*/
   queue_t* adfa_to_free; /**< A queue of adfa_val_t structure to free*/
   ind_context_t* inductions; /**< Results on induction analysis*/
   adfa_cntxt_t* adfa; /**< ADFA context*/
};

/*
 * Creates an adfa_val_t structure from memory operand parts
 * \param to_free a queue of adfa_val_t structure used to list elements to remove
 * \param offset offset of the memory operand
 * \param base adfa_val_t structure corresponding to base register (can be NULL)
 * \param index adfa_val_t structure corresponding to index register (can be NULL)
 * \param scale scale of the memory operand
 * \return an adfa_val_t structure representing the memory operand
 */
static adfa_val_t* __memory_to_val(queue_t* to_free, int64_t offset,
      adfa_val_t* base, adfa_val_t* index, int scale)
{
   adfa_val_t* val = NULL;
   adfa_val_t* valOff = lc_malloc(sizeof(adfa_val_t));
   queue_add_head(to_free, valOff);
   valOff->is_mem = FALSE;
   valOff->op = ADFA_OP_NULL;
   valOff->type = ADFA_TYPE_IMM;
   valOff->data.imm = offset;

   adfa_val_t* valright = NULL;
   // Index register is not NULL
   if (index != NULL) {
      // rigth = index * scale
      adfa_val_t* valscale = lc_malloc(sizeof(adfa_val_t));
      queue_add_head(to_free, valscale);
      valscale->is_mem = FALSE;
      valscale->op = ADFA_OP_NULL;
      valscale->type = ADFA_TYPE_IMM;
      valscale->data.imm = scale;

      valright = lc_malloc(sizeof(adfa_val_t));
      queue_add_head(to_free, valright);
      valright->is_mem = FALSE;
      valright->op = ADFA_OP_MUL;
      valright->type = ADFA_TYPE_SONS;
      valright->data.sons[0] = index;
      valright->data.sons[1] = valscale;
   }

   // Base register and index register are not NULL
   if (base != NULL && valright != NULL) {  // left = off + base
      adfa_val_t* valleft = lc_malloc(sizeof(adfa_val_t));
      queue_add_head(to_free, valleft);
      valleft->is_mem = FALSE;
      valleft->op = ADFA_OP_ADD;
      valleft->type = ADFA_TYPE_SONS;
      valleft->data.sons[0] = valOff;
      valleft->data.sons[1] = base;

      // then val  = left + right
      val = lc_malloc(sizeof(adfa_val_t));
      val->is_mem = FALSE;
      val->op = ADFA_OP_ADD;
      val->type = ADFA_TYPE_SONS;
      val->data.sons[0] = valleft;
      val->data.sons[1] = valright;
   }
   // else Base register is not NULL but index register is NULL
   else if (base != NULL && valright == NULL) {  // val = off + base
      val = lc_malloc(sizeof(adfa_val_t));
      val->is_mem = FALSE;
      val->op = ADFA_OP_ADD;
      val->type = ADFA_TYPE_SONS;
      val->data.sons[0] = valOff;
      val->data.sons[1] = base;
   }
   // else Base register is NULL but index register is not NULL
   else {  // val = off + right
      val = lc_malloc(sizeof(adfa_val_t));
      val->is_mem = FALSE;
      val->op = ADFA_OP_ADD;
      val->type = ADFA_TYPE_SONS;
      val->data.sons[0] = valOff;
      val->data.sons[1] = valright;
   }
   return (val);
}

/*
 * Prints a value into a string
 * \param val a value to print
 * \param arch current architecture
 * \param str the string where to print
 */
void polytope_val_tostring(adfa_val_t* val, arch_t* arch, char* str)
{
   if (val == NULL || arch == NULL || str == NULL)
      return;
   int sifht_val = 0;

   if (val->is_mem)
      sprintf(str, "%s@[", str);

   switch (val->type) {
   case ADFA_TYPE_IMM:
      sprintf(str, "%s0x%"PRIx64, str, val->data.imm);
      break;
   case ADFA_TYPE_REG:
      sprintf(str, "%s%s_%d", str,
            arch_get_reg_name(arch, val->data.reg->reg->type,
                  val->data.reg->reg->name), val->data.reg->index);
      break;
   case ADFA_TYPE_SONS:
      sprintf(str, "%s(", str);
      // print left
      if (val->data.sons[0])
         polytope_val_tostring(val->data.sons[0], arch, str);

      if (val->data.sons[1] != NULL && val->data.sons[1]->type == ADFA_TYPE_IMM
            && (val->op == ADFA_OP_SL || val->op == ADFA_OP_SR))
         sifht_val = pow(2, val->data.sons[1]->data.imm);

      // print operator
      switch (val->op) {
      case ADFA_OP_ADD:
         sprintf(str, "%s + ", str);
         break;
      case ADFA_OP_SUB:
         sprintf(str, "%s - ", str);
         break;
      case ADFA_OP_MUL:
         sprintf(str, "%s * ", str);
         break;
      case ADFA_OP_DIV:
         sprintf(str, "%s / ", str);
         break;
      case ADFA_OP_SL:
         if (sifht_val == 0)
            sprintf(str, "%s * 2 ^ ", str);
         else
            sprintf(str, "%s * 0x%x", str, sifht_val);
         break;
      case ADFA_OP_SR:
         if (sifht_val == 0)
            sprintf(str, "%s / 2 ^ ", str);
         else
            sprintf(str, "%s / 0x%x", str, sifht_val);
         break;
      }

      // print rigth
      if (val->data.sons[1] && sifht_val == 0)
         polytope_val_tostring(val->data.sons[1], arch, str);
      sprintf(str, "%s)", str);
      break;
   }
   if (val->is_mem)
      sprintf(str, "%s]", str);
}

/*
 * Lists registers used into a polytope and saves them into the structure
 * \param polytope a polytope to analyze
 * \param val an element in the access member
 */
void _polytope_list_registers(polytope_t* polytope, adfa_val_t* val)
{
   if (polytope == NULL || val == NULL)
      return;

   if (val->type == ADFA_TYPE_REG) {
      if (queue_lookup(polytope->registers, &ssa_var_equal,
            val->data.reg) == NULL)
         queue_add_tail(polytope->registers, val->data.reg);
   } else if (val->type == ADFA_TYPE_SONS) {
      if (val->data.sons[0])
         _polytope_list_registers(polytope, val->data.sons[0]);
      if (val->data.sons[1])
         _polytope_list_registers(polytope, val->data.sons[1]);
   }
}

/*
 * Looks for the stop bound of current loop for a polytope
 * \param loop current loop
 * \param f current function
 * \param polytope a polytope to analyze
 */
static void _polytope_lookfor_stop(loop_t* loop, fct_t* f, polytope_t* polytope)
{
   if (list_length(loop->exits) == 1) {
      block_t* bex = list_getdata(loop->exits);
      ssa_block_t* ssabex = fct_get_ssa(f)[bex->id]; //((ssa_block_t**)f->ssa)[bex->id];
      FOREACH_INQUEUE_REVERSE(ssabex->first_insn, it_in)
      {
         ssa_insn_t* ssain = GET_DATA_T(ssa_insn_t*, it_in);
         insn_t* in = ssain->in;
         unsigned short family = insn_get_family(in);

         if (in == NULL)
            break;;

         // Checks the instruction looks like CMP <imm>, <reg>
         if (family == FM_CMP && insn_get_nb_oprnds(in) == 2
               && oprnd_is_imm(insn_get_oprnd(in, 0)) == TRUE
               && oprnd_is_reg(insn_get_oprnd(in, 1)) == TRUE) {
            polytope->stop_bound_insn = ssain;
            break;
         } else if (family == FM_CMP)
            break;
      }
   }
}

/*
 * Looks for the induction variable for a polytope
 * \param polytope a polytope to analyze
 * \param ind_context induction context
 */
static void _polytope_lookfor_induction(polytope_t* polytope,
      ind_context_t* ind_context)
{
   if (polytope->stop_bound_insn != NULL) {
      ssa_var_t* reg = polytope->stop_bound_insn->oprnds[2];
      polytope->induction = hashtable_lookup(ind_context->derived_induction,
            reg);
   }
}

/*
 * Looks for the start bound for a polytope
 * \param polytope a polytope to analyze
 */
static void _polytope_lookfor_start(polytope_t* polytope, hashtable_t* Rvals,
      adfa_cntxt_t* adfa_cntxt)
{
   if (polytope->stop_bound_insn != NULL && polytope->induction != NULL) {
      ssa_var_t* reg = polytope->induction->family;
      ssa_insn_t* reg_def = reg->insn;
      // If reg_def is a phi function with 2 operands (the induction
      // increment and the initial value, try to analyze the initial
      // value to find the initial state of the induction register.
      if (reg_def != NULL && reg_def->in == NULL) {
         int nb_oprnd = 1;
         while (reg_def->oprnds[nb_oprnd] != NULL)
            nb_oprnd++;

         if (nb_oprnd <= 2) {
            ssa_var_t* var = NULL;
            if (nb_oprnd == 1)
               var = reg_def->oprnds[0];
            else if (nb_oprnd == 2) {
               if (ssa_var_equal(reg_def->oprnds[1],
                     polytope->stop_bound_insn->oprnds[1]))
                  var = reg_def->oprnds[0];
               else
                  var = reg_def->oprnds[1];
            }
            // At this point, var is NULL (exit) or it contains the register
            // defined before the phifunction
            adfa_val_t* res = hashtable_lookup(Rvals, var);
            if (res == NULL)
               res = ADFA_analyze_insn(var->insn, adfa_cntxt);
            if (res != NULL) {
               polytope->start_bound_insn = var->insn;
               polytope->start_bound_val = res;
            }
         }
      }
      // else, checks if the value  of the register is an immediate
      // value or not
      else {
      }
   }
}

/*
 * Checks if a polytope can be comptued or should be instrumented
 * \param polytope a polytope to analyze
 * \param ind_context induction results
 */
static void _polytope_checkif_computed(polytope_t* polytope,
      ind_context_t* ind_context)
{
   FOREACH_INQUEUE(polytope->registers, it_r) {
      ssa_var_t* reg = GET_DATA_T(ssa_var_t*, it_r);

      // if  reg is not an induction
      // and reg is defined into the loop
      if (hashtable_lookup(ind_context->derived_induction, reg) == NULL
            && reg->insn != NULL
            && loop_is_innermost(reg->insn->ssab->block->loop))
         return;
   }
   polytope->computed = TRUE;
}

/*
 * Initializes the data structure used for polytopes analysis
 * \param f a function to analyze
 * \return an initialized poly_context_t structure
 */
static void* polytope_init(fct_t* f, adfa_cntxt_t* adfa)
{
   int i = 0;
   poly_context_t* cntxt = lc_malloc(sizeof(poly_context_t));
   cntxt->arch = f->asmfile->arch;
   cntxt->f = f;
   cntxt->inductions = lcore_compute_function_induction(f);
   ;
   cntxt->polytopes = lc_malloc0(queue_length(f->loops) * sizeof(queue_t*));
   cntxt->adfa = adfa;
   cntxt->adfa_to_free = queue_new();

   for (i = 0; i < queue_length(f->loops); i++)
      cntxt->polytopes[i] = queue_new();
   return (cntxt);
}

/*
 * Checks if an instruction should be analyzed
 * \param ssain an SSA assembly instruction
 * \param useless not used
 * \return TRUE if the instruction should be analyzed, else FALSE
 */
static int polytope_insn_filter(ssa_insn_t* ssain, void* useless)
{
   (void) useless;
   insn_t* in = ssain->in;
   block_t* b = in->block;
   loop_t* l = b->loop;
   int i = 0;
   unsigned short family = insn_get_family(in);

   if (l == NULL)
      return (FALSE);
   if (loop_is_innermost(l) == FALSE)
      return (FALSE);

   if (family == FM_LEA || family == FM_CALL || family == FM_NOP)
      return (FALSE);

   for (i = 0; i < insn_get_nb_oprnds(in); i++) {
      oprnd_t* op = insn_get_oprnd(in, i);
      if (oprnd_is_mem(op))
         return (TRUE);
   }
   return (FALSE);
}

/*
 * Analyzes an instruction to compute polytopes
 * \param ssain an SSA assembly instruction
 * \param Rvals hashtable of adfa_val_t indexed by ssa_var_t (values computed
 *        by Advanced Data Flow Analysis)
 * \param pcntxt pointer on a poly_context_t structure
 */
static void polytope_insn_execute(ssa_insn_t* ssain, adfa_val_t* result,
      hashtable_t* Rvals, void* pcntxt)
{
   (void) result;
   if (ssain->in == NULL)
      return;
   poly_context_t* cntxt = (poly_context_t*) pcntxt;
   queue_t* polytopes = cntxt->polytopes[ssain->in->block->loop->id];
   int i = 0;
   insn_t* in = ssain->in;
   loop_t* loop = ssain->in->block->loop;
   fct_t* current_fct = loop->function;
   arch_t* arch = current_fct->asmfile->arch;
   int lvl = 0;
   tree_t* tree = loop->hierarchy_node;

   // Loop level computation
   while (tree != NULL) {
      tree = tree->parent;
      lvl++;
   }

   // Iterates over operands to analysis memory operands
   for (i = 0; i < insn_get_nb_oprnds(in); i++) {
      oprnd_t* op = insn_get_oprnd(in, i);
      if (oprnd_is_mem(op)) {
         reg_t* rbase = oprnd_get_base(op);
         reg_t* rindex = oprnd_get_index(op);
         adfa_val_t* base = NULL;
         adfa_val_t* index = NULL;

         if (rbase != NULL)
            base = hashtable_lookup(Rvals, ssain->oprnds[i * 2]);
         if (rindex != NULL)
            index = hashtable_lookup(Rvals, ssain->oprnds[i * 2 + 1]);

         polytope_t* polytope = lc_malloc0(sizeof(polytope_t));
         queue_add_tail(polytopes, polytope);
         polytope->computed = FALSE;
         polytope->ssain = ssain;
         polytope->f = current_fct;
         polytope->level = lvl;
         polytope->registers = queue_new();
         polytope->acces = __memory_to_val(cntxt->adfa_to_free,
               oprnd_get_offset(op), base, index, oprnd_get_scale(op));
         _polytope_list_registers(polytope, polytope->acces);

         // As acces structure will be partially freed at the end of the function,
         // it is saved here in a string
         char str[2048];
         str[0] = '\0';
         polytope_val_tostring(polytope->acces, arch, str);
         polytope->acces_str = lc_strdup(str);

         _polytope_lookfor_stop(loop, current_fct, polytope);
         _polytope_lookfor_induction(polytope, cntxt->inductions);
         _polytope_lookfor_start(polytope, Rvals, cntxt->adfa);
         _polytope_checkif_computed(polytope, cntxt->inductions);
      }
   }
}

/*
 * Compute polytopes for a function
 * \param f a function to analyze
 */
void lcore_fct_analyze_polytopes(fct_t* f)
{
   if (f == NULL) {
      ERRMSG("Grouping: Input function is NULL");
      return;
   }
   if (f->polytopes != NULL)
      return;
   DBGMSG("Analyze function %s\n", fct_get_name(f))
   adfa_driver_t driver;
   driver.init = &polytope_init;
   driver.insn_execute = &polytope_insn_execute;
   driver.insn_filter = &polytope_insn_filter;
   driver.propagate = NULL;
   driver.flags = ADFA_NO_UNRESOLVED_SHIFT;

   ADFA_analyze_function(f, &driver);
   poly_context_t* cntxt = driver.user_struct;
   f->polytopes = cntxt;
   f->asmfile->free_polytopes = &lcore_free_polytopes;
}

static void _polytope_free(void* ppolytope)
{
   polytope_t* polytope = (polytope_t*) ppolytope;
   if (polytope == NULL)
      return;
   lc_free(polytope->acces);
   lc_free(polytope->acces_str);
   queue_free(polytope->registers, NULL);
   lc_free(polytope);
}

/*
 * Frees polytopes computed for a function
 * \param f a function
 */
void lcore_free_polytopes(fct_t* f)
{
   if (f->polytopes == NULL)
      return;
   poly_context_t* cntxt = (poly_context_t*) f->polytopes;
   int i = 0;

   lcore_free_induction(cntxt->inductions);
   cntxt->inductions = NULL;
   for (i = 0; i < queue_length(f->loops); i++)
      queue_free(cntxt->polytopes[i], &_polytope_free);
   lc_free(cntxt->polytopes);
   queue_free(cntxt->adfa_to_free, &lc_free);

   ADFA_free(cntxt->adfa);
   lc_free(f->polytopes);
   f->polytopes = NULL;
}

/*
 * Gets polytopes from a function (the function should be analyzed before)
 * \param f a function
 * \return an array of queue of polytopes. One queue per function's loop id
 */
queue_t** lcore_get_polytopes_from_fct(fct_t* f)
{
   if (f == NULL)
      return (NULL);
   if (f->polytopes == NULL)
      return (NULL);
   return (((polytope_context_t*) f->polytopes)->polytopes);
}

/*
 * Prints a value into a string
 * \param val a value to print
 * \param arch current architecture
 * \param str the string where to print
 */
int _polytope_toLuagraph_node(adfa_val_t* val, arch_t* arch, char* str, int id)
{
   int ret_id = id;
   int left_id = -1;
   int rigth_id = -1;
   if (val == NULL || arch == NULL || str == NULL)
      return ret_id;
   int shift_val = 0;

   if (val->is_mem)
      return (ret_id);

   switch (val->type) {
   case ADFA_TYPE_IMM:
      sprintf(str, "%s  graph:add_node(%d, \"0x%"PRIx64"\");\n", str, id,
            val->data.imm);
      return (id);
      break;
   case ADFA_TYPE_REG:
      sprintf(str, "%s  graph:add_node(%d, \"%s_%d\");\n", str, id,
            arch_get_reg_name(arch, val->data.reg->reg->type,
                  val->data.reg->reg->name), val->data.reg->index);
      return (id);
      break;
   case ADFA_TYPE_SONS:
      if (val->data.sons[1] != NULL && val->data.sons[1]->type == ADFA_TYPE_IMM
            && (val->op == ADFA_OP_SL || val->op == ADFA_OP_SR))
         shift_val = pow(2, val->data.sons[1]->data.imm);

      if (val->data.sons[0]) {
         ret_id = _polytope_toLuagraph_node(val->data.sons[0], arch, str, id);
         left_id = ret_id;
         ret_id += 1;
      }

      if (val->data.sons[1] && shift_val == 0) {
         ret_id = _polytope_toLuagraph_node(val->data.sons[1], arch, str,
               ret_id);
         rigth_id = ret_id;
         ret_id += 1;
      }

      // print operator
      switch (val->op) {
      case ADFA_OP_ADD:
         sprintf(str, "%s  graph:add_node(%d, \"+\");\n", str, ret_id);
         break;
      case ADFA_OP_SUB:
         sprintf(str, "%s  graph:add_node(%d, \"-\");\n", str, ret_id);
         break;
      case ADFA_OP_MUL:
         sprintf(str, "%s  graph:add_node(%d, \"*\");\n", str, ret_id);
         break;
      case ADFA_OP_DIV:
         sprintf(str, "%s  graph:add_node(%d, \"\\\");\n", str, ret_id);
         break;
      case ADFA_OP_SL:
         if (shift_val == 0)
            sprintf(str, "%s  graph:add_node(%d, \"2 ^ \");\n", str, id);
         else {
            sprintf(str, "%s  graph:add_node(%d, \"0x%x\");\n", str, ret_id,
                  shift_val);
            rigth_id = ret_id;
            ret_id += 1;
            sprintf(str, "%s  graph:add_node(%d, \"*\");\n", str, ret_id);
            sprintf(str, "%s  graph:add_edge(%d, %d, \"\");\n", str, ret_id,
                  rigth_id);
         }
         break;
      case ADFA_OP_SR:
         if (shift_val == 0)
            sprintf(str, "%s  graph:add_node(%d, \"2 ^ \");\n", str, id);
         else {
            sprintf(str, "%s  graph:add_node(%d, \"0x%x\");\n", str, ret_id,
                  shift_val);
            rigth_id = ret_id;
            ret_id += 1;
            sprintf(str, "%s  graph:add_node(%d, \"\\\");\n", str, ret_id);
            sprintf(str, "%s  graph:add_edge(%d, %d, \"\");\n", str, ret_id,
                  rigth_id);
         }
         break;
      }
      if (left_id != -1)
         sprintf(str, "%s  graph:add_edge(%d, %d, \"\");\n", str, ret_id,
               left_id);

      if (rigth_id != -1)
         sprintf(str, "%s  graph:add_edge(%d, %d, \"\");\n", str, ret_id,
               rigth_id);
      break;
   }

   return (ret_id);
}

/*
 * Generate the Lua code used to generate a graph representing the expression
 * \param polytope a polytope to analyze
 * \return a string to free
 */
char* polytope_toLuagraph(polytope_t* polytope)
{
   char buff[4096];
   sprintf(buff, "local function _create_graph ()\n"
         "  local graph = Graph:new();\n");

   _polytope_toLuagraph_node(polytope->acces, polytope->f->asmfile->arch, buff,
         1);

   sprintf(buff, "%s  graph:set_node_root(1);\n"
         "  return graph;\n"
         "end\n", buff);
   return (lc_strdup(buff));
}

/*
 * Creates a string (must be freed using lc_free) of a given polytope
 * \param polytope a polytope to change into a string
 * \return an allocated string (must be freed when not used anymore)
 */
char* polytope_tostring(polytope_t* polytope)
{
   char* expression_code = polytope_toLuagraph(polytope);

   char buff[8192];
   fct_t* f = polytope->f;
   arch_t* arch = f->asmfile->arch;

   sprintf(buff, "[0x%"PRIx64"] = {\n"
   "  expression = \"%s\",\n"
   "  computed = %s,\n"
   "  expression_code = \"%s\",\n"
   "  registers = {\n", polytope->ssain->in->address, polytope->acces_str,
         (polytope->computed == TRUE) ? "TRUE" : "FALSE", expression_code);
   lc_free(expression_code);

   // registers ------------------------
   int i = 1;
   int nb_regs = queue_length(polytope->registers);
   FOREACH_INQUEUE(polytope->registers, it_reg) {
      ssa_var_t* reg = GET_DATA_T(ssa_var_t*, it_reg);
      int64_t addr = 0;
      if (reg->insn != NULL && reg->insn->in != NULL)
         addr = reg->insn->in->address;
      else if (reg->insn != NULL && reg->insn->in == NULL)
         addr =
               ((insn_t*) reg->insn->ssab->block->begin_sequence->data)->address;
      else if (reg->insn == NULL)
         addr =
               ((insn_t*) ((block_t*) queue_peek_head(f->entries))->begin_sequence->data)->address;

      sprintf(buff,
            "%s    {reg=\"%s_%d\", address=\"0x%"PRIx64"\", str=\"%s\", id=\"%d\"}%s\n",
            buff, arch_get_reg_name(arch, reg->reg->type, reg->reg->name),
            reg->index, addr,
            arch_get_reg_name(arch, reg->reg->type, reg->reg->name), reg->index,
            (i == nb_regs) ? "" : ",");
      i++;
   }
   sprintf(buff, "%s  },\n"
         "  level = \"%d\"\n", buff, polytope->level);

   // induction_reg --------------------
   if (polytope->induction != NULL
         && polytope->induction->add->type == IND_NODE_IMM
         && polytope->induction->mul->type == IND_NODE_IMM) {
      // as induction detection depends on stop_bound_reg and as
      // stop_bound_reg depends on operands, this is always not NULL
      ssa_var_t* var = polytope->stop_bound_insn->oprnds[2];
      sprintf(buff,
            "%s  ,induction_reg = {str = \"%s\", id = \"%d\", val = \"%d\"}\n",
            buff, arch_get_reg_name(arch, var->reg->type, var->reg->name),
            var->index, polytope->induction->add->data.imm);
   }

   // stop_bound_reg -------------------
   if (polytope->stop_bound_insn != NULL) {
      ssa_var_t* var = polytope->stop_bound_insn->oprnds[2];
      sprintf(buff,
            "%s  ,stop_bound_reg = {str = \"%s\", id = \"%d\", val = \"0x%"PRIx64"\"}\n",
            buff, arch_get_reg_name(arch, var->reg->type, var->reg->name),
            var->index,
            oprnd_get_imm(insn_get_oprnd(polytope->stop_bound_insn->in, 0)));
   }

   // start_bound_reg ------------------
   if (polytope->start_bound_insn != NULL) {
      ssa_var_t* var = polytope->start_bound_insn->output[0];
      sprintf(buff,
            "%s  ,start_bound_reg = {str = \"%s\", id = \"%d\", val = \"", buff,
            arch_get_reg_name(arch, var->reg->type, var->reg->name),
            var->index);
      polytope_val_tostring(polytope->start_bound_val, arch, buff);
      sprintf(buff, "%s\"}\n", buff);
   }
   sprintf(buff, "%s}", buff);
   return (lc_strdup(buff));
}
