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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "libmcommon.h"
#include "libmasm.h"
#include "libmcore.h"

/**
 * \file
 * \section grouping_intro Introduction
 * Grouping analysis is a static analysis used to find memory accesses targeting
 * the same memory area into innermost loops. Basically, these memory areas
 * correspond to arrays.
 *
 * \section grouping_working How does it work ?
 * Grouping analysis is based on the Advanced Data Flow Analysis (ADFA).
 * Following subsections presents the content of functions passed to
 * the ADFA through the driver.
 *
 * \subsection grouping_working_datastructures Data Structures
 * The grouping analysis used a table of groups per loop. Concretely, a
 * hashtable using loops as keys is created. Its data are hashtables using
 * a string as key and a group_t structure as data. This data structure is
 * freed at the end of the analysis, only groups remains allocated until
 * the function deletion.
 * Strings used as keys in subtables represent accessed memory addresses.
 *
 * \subsection grouping_working_init Initialization
 * The initialization step consists in initializing the temporar data structure.
 * The main hashtable is created, then a hashtable is inserted for each innermost
 * loop in the function.
 *
 * \subsection grouping_working_filter Filter Function
 * As grouping analysis focuses on memory accesses, only instructions containing
 * at least one memory access are handled. Moreover, these instructions must belong to an
 * innermost function.
 *
 * \subsection grouping_working_execute Instruction analyzis
 * For each memory operand of the analyzed instruction, an adfa_val_t structure
 * is created from data returned by ADFA. This structure is based on the base
 * register content and on the index register content.
 * Once the adfa_val_t structure is created, a string representing it is created,
 * then the structure is freed. The string will be used as key to link the
 * instruction to its group. If needed, the group is created, then added in the
 * data structure.
 *
 * \subsection grouping_working_progagate Data Propagation
 * For the grouping analysis, there is no data propagation.
 *
 * \subsection grouping_working_results Results
 * As describe in \ref grouping_working_datastructures, generated data structure is
 * temporar. After the analysis of all instructions, each group is saved into its
 * loop.
 *
 *
 *
 *
 *
 * \section grouping_limits Limitations
 * The analysis has some limitations due to the current implementation or some patterns
 * used by compilers to produce assembly code:
 * - Assuming following memory accesses representation:
 *    - 1/ <expr>
 *    - 2/ <expr> + <reg>
 *   with <expr> a common expression for both accesses and <reg> a register. As the
 *   value of <reg> is unknown, both accesses are not considered to belonging to
 *   the same group. This can be false if <reg> value is a small integer (for example
 *   1, 16 or 64).
 *
 * - Assuming following memory accesses representation:
 *    - 1/ 2 * <expr1> + 3 * <expr2>
 *    - 2/ <expr1> + 3 * <expr2> + <expr1>
 *   Both expressions accessed to the same memory address, but current implementation
 *   can not factorize expressions, so both expressions are considered as different.
 * */

// Some functions are declared according to current architectures:
// <arch>_group_isFiltered (group_elem_t* ge, void* user)
// Function used to check if a group element (ge) should be filtered or not
// according to a user value
// \param ge a group element to check
// \param user a user value used during the checking
// \return TRUE if the element can be used, else FALSE if it is filtered

/*
 * Initializes the data structure used for grouping analysis
 * \param f a function to analyze
 * \return an initialized hashtable. Keys are loops, data are hashtables.
 *         Each hashtable data uses string as key and group as data
 */
static void* grouping_init(fct_t* f, adfa_cntxt_t* useless)
{
   (void) useless;
   hashtable_t* groups = hashtable_new(&direct_hash, &direct_equal);
   FOREACH_INQUEUE(f->loops, it_l) {
      loop_t* l = GET_DATA_T(loop_t*, it_l);
      if (loop_is_innermost(l))
         hashtable_insert(groups, l, hashtable_new(&str_hash, &str_equal));
   }
   return (groups);
}

/*
 * Converts a value into a key
 * \param val a value computed by the Advance Data Flow Analysis
 * \param arch current architecture
 * \param buff an allocated string where to save the key
 */
static void grouping_to_key(adfa_val_t* val, arch_t* arch, char* buff,
      int is_mem)
{
   if (val == NULL || arch == NULL || buff == NULL)
      return;
   int _is_mem = (is_mem == 1) ? 1 : val->is_mem;
   if (_is_mem == 0 && val->op != ADFA_OP_ADD && val->op != ADFA_OP_SUB
         && val->op != ADFA_OP_NULL)
      _is_mem = 1;

   if (val->op == ADFA_OP_SQRT)
      sprintf(buff, "%sSQRT(", buff);

   if (val->is_mem)
      sprintf(buff, "%s@[", buff);

   switch (val->type) {
   case ADFA_TYPE_IMM:
      if (_is_mem == 1)
         sprintf(buff, "%s0x%"PRIx64, buff, val->data.imm);
      break;
   case ADFA_TYPE_REG:
      sprintf(buff, "%s%s_%d", buff,
            arch_get_reg_name(arch, val->data.reg->reg->type,
                  val->data.reg->reg->name), val->data.reg->index);
      break;
   case ADFA_TYPE_SONS:
      if (val->op == ADFA_OP_ADD) {
         if (val->data.sons[0])
            grouping_to_key(val->data.sons[0], arch, buff, _is_mem);
         sprintf(buff, "%s+", buff);
         if (val->data.sons[1])
            grouping_to_key(val->data.sons[1], arch, buff, _is_mem);
      } else {
         sprintf(buff, "%s(", buff);
         if (val->data.sons[0])
            grouping_to_key(val->data.sons[0], arch, buff, _is_mem);

         switch (val->op) {
         case ADFA_OP_SUB:
            sprintf(buff, "%s)-(", buff);
            break;
         case ADFA_OP_MUL:
            sprintf(buff, "%s)*(", buff);
            break;
         case ADFA_OP_DIV:
            sprintf(buff, "%s)/(", buff);
            break;
         case ADFA_OP_SL:
            sprintf(buff, "%s)<<(", buff);
            break;
         case ADFA_OP_SR:
            sprintf(buff, "%s)>>(", buff);
            break;
         }
         if (val->data.sons[1])
            grouping_to_key(val->data.sons[1], arch, buff, _is_mem);
         sprintf(buff, "%s)", buff);
      }
      break;
   }
   if (val->is_mem)
      sprintf(buff, "%s]", buff);
   if (val->op == ADFA_OP_SQRT)
      sprintf(buff, "%s)", buff);
}

/*
 * Checks if an instruction should be analyzed to compute groups
 * \param ssain an SSA assembly instruction
 * \param useless not used
 * \return TRUE if the instruction should be analyzed, else FALSE
 */
static int grouping_insn_filter(ssa_insn_t* ssain, void* useless)
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
 * Analyzes an instruction to compute groups
 * \param ssain an SSA assembly instruction
 * \param Rvals hashtable of adfa_val_t indexed by ssa_var_t (values computed
 *        by Advanced Data Flow Analysis)
 * \param pgroups pointer on a hashtable used to store group results
 */
static void grouping_insn_execute(ssa_insn_t* ssain, adfa_val_t* result,
      hashtable_t* Rvals, void* pgroups)
{
   (void) result;
   hashtable_t* allgroups = (hashtable_t*) pgroups;
   int i = 0;
   insn_t* in = ssain->in;

   // Iterates over operands to analysis memory operands
   for (i = 0; i < insn_get_nb_oprnds(in); i++) {
      oprnd_t* op = insn_get_oprnd(in, i);
      if (oprnd_is_mem(op)) {
         // Create the val corresponding to the memory operand ----------------
         // This structure will be used to generate the group key
         reg_t* base = oprnd_get_base(op);
         reg_t* index = oprnd_get_index(op);
         adfa_val_t* val = lc_malloc0(sizeof(adfa_val_t));

         if (base != NULL && index != NULL) {
            val->type = ADFA_TYPE_SONS;
            val->is_mem = FALSE;
            val->op = ADFA_OP_ADD;
            val->data.sons[0] = hashtable_lookup(Rvals, ssain->oprnds[i * 2]);
            val->data.sons[1] = hashtable_lookup(Rvals,
                  ssain->oprnds[i * 2 + 1]);
         } else if (base != NULL && index == NULL) {
            adfa_val_t* val_base = hashtable_lookup(Rvals,
                  ssain->oprnds[i * 2]);

            val->type = val_base->type;
            val->is_mem = val_base->is_mem;
            val->op = val_base->op;
            val->data = val_base->data;
         } else if (base == NULL && index != NULL) {
            adfa_val_t* val_index = hashtable_lookup(Rvals,
                  ssain->oprnds[i * 2 + 1]);

            val->type = val_index->type;
            val->is_mem = val_index->is_mem;
            val->op = val_index->op;
            val->data = val_index->data;
         }

         char pattern = 0;
         // Handle group ------------------------------------------------------
         if (oprnd_is_src(op))
            pattern = 'L';

         if (oprnd_is_dst(op))
            pattern = 'S';

         char key[8192];
         key[0] = '\0';
         grouping_to_key(val, ssain->in->block->function->asmfile->arch, key,
               0);
         lc_free(val);

         group_elem_t* gdat = group_data_new(pattern, in, i);
         hashtable_t* groups = hashtable_lookup(allgroups, in->block->loop);
         group_t* group = hashtable_lookup(groups, key);
         if (group == NULL) {
            int (*group_isFiltered)(group_elem_t*, void*);
            group_isFiltered = NULL;

            group = group_new(key, ssain->in->block->loop, group_isFiltered);
            hashtable_insert(groups, group->key, group);
         }
         group_add_elem(group, gdat);
      }
   }
}

/*
 * Analyzes a function to compute groups
 * \param f a function to analyze
 */
void lcore_fct_analyze_groups(fct_t* f)
{
   if (f == NULL) {
      ERRMSG("Grouping: Input function is NULL");
      return;
   }
   DBGMSG("Analyze groups for %s\n", fct_get_name(f));
   if (f->is_grouping_analyzed == TRUE) {
      return ;
   }
   adfa_driver_t driver;
   driver.init = &grouping_init;
   driver.insn_execute = &grouping_insn_execute;
   driver.insn_filter = &grouping_insn_filter;
   driver.propagate = NULL;
   driver.flags = 0;

   // Compute groups
   adfa_cntxt_t* adfa = ADFA_analyze_function(f, &driver);
   ADFA_free(adfa);
   hashtable_t* allgroups = driver.user_struct;

   // Save groups into the corresponding loop
   FOREACH_INHASHTABLE(allgroups, it_groups) {
      loop_t* loop = GET_KEY(loop, it_groups);
      hashtable_t* groups = GET_DATA_T(hashtable_t*, it_groups);

      FOREACH_INHASHTABLE(groups, it_group) {
         group_t* group = GET_DATA_T(group_t*, it_group);
         loop_add_group(loop, group);
      }
      hashtable_free(groups, NULL, NULL);
   }
   hashtable_free(allgroups, NULL, NULL);

   f->is_grouping_analyzed = TRUE;
}

/*
 * Analyzes a function belonging to an asmfile to compute groups
 * \param asmf an assembly file to analyze
 * \param fctname the function the analyze. If NULL, all functions are analyzed
 */
void lcore_asmf_analyze_groups(asmfile_t* asmf, char* fctname)
{
   int found = FALSE; // Flag turn to TRUE is at least one function has been analyzed

   // Checks inputs -------------------------------------------------
   if (asmf == NULL) {
      ERRMSG("Grouping: Input asmfile is NULL");
      return;
   }
   if (asmf->analyze_flag && (LOO_ANALYZE | DOM_ANALYZE) == 0) {
      ERRMSG(
            "Grouping: Input file for grouping analysis has not been analyzed for loop detection or domination\n");
      return;
   }

   // Iterates over functions to analyze them -----------------------
   FOREACH_INQUEUE(asmf->functions, it_f) {
      fct_t* f = GET_DATA_T(fct_t*, it_f);

      if (fctname == NULL)
         lcore_fct_analyze_groups(f);
      else if (strcmp(fct_get_name(f), fctname) == 0) {
         found = TRUE;
         lcore_fct_analyze_groups(f);
      }
   }
   if (found == FALSE && fctname != NULL) {
      ERRMSG("Grouping: Unknown function (%s)\n", fctname);
   }
}

int group_reg_isvect(group_t* group, reg_t* reg)
{
   return (0);
}
