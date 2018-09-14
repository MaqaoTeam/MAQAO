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
 * \file asmblutils.c
 * \brief Contains utility functions for the assembler
 *
 * \date 26 sept. 2011
 */
#include "asmblutils.h"

/**
 * Structure for storing internal variables to the assembly. Currently only used for storing the sizes of the arrays of variables
 * used by the matching functions, but could be extended in future updates;
 * */
typedef struct asmblcontext_s {
   uint16_t n_vars; /**<Size of the array containing variables (nonterminals)*/
   uint16_t n_vals; /**<Size of the array containing tokens (terminals)*/
} asmblcontext_t;

static bitvector_t* revaction_encode(revaction_t* action, void* input,
      bitvector_t* cod, asmblcontext_t* ac);

/**
 * Creates a new structure storing the internal variables for the assembly.
 * \param n_vars Size of the array containing variables (nonterminals)
 * \param n_vals Size of the array containing tokens (terminals)
 * \return A new structure
 * */
static asmblcontext_t* asmblcontext_new(unsigned int n_vars,
      unsigned int n_vals)
{
   asmblcontext_t* ac = lc_malloc(sizeof(asmblcontext_t));
   ac->n_vars = n_vars;
   ac->n_vals = n_vals;
   return ac;
}

/**
 * Frees a structure storing the internal variables for the assembly
 * \param The structure to free
 * */
static void asmblcontext_free(asmblcontext_t* ac)
{
   lc_free(ac);
}

/**
 * Performs the encoding of a reverse symbol depending on its type: if it contains encoding rules for a symbol (its var_id member is not null),
 * performs a sequence of actions for matching the input and returns the result of the first non-NULL one. Otherwise, it is an upward symbol,
 * meaning we are completing the code of a partially encoded input, and we perform the whole sequence of actions it contain.
 * \param revsymbol The structure holding the details about the symbol we want to encode or complete
 * \param input Input for the matching functions
 * \param cod Coding of the already encoded symbol
 * \param ac Structure containing the internal variables for the assembly
 * \return If \c revsymbol is a regular symbol, the result (coding) of the first matching function whose result was not NULL (match found)
 * or NULL if none did. If \c revsymbol is an upward symbol, the completed coding of the input.
 * */
static bitvector_t* revsymbol_encode(revsymbol_t* revsymbol, void* input,
      bitvector_t* cod, asmblcontext_t* ac)
{
   /**\todo There may be some way to factorise even more the code of this function, especially if we clean up a bit the code of the first case*/
   int i;
   bitvector_t* val = NULL;
   if (revsymbol->var_id == 0) { //Encoding an upward symbol: we execute all the actions it contains
      bitvector_t *coding = cod;
      for (i = 0; i < revsymbol->n_actions; i++) {
         if ((val == NULL)
               && ((val = revaction_encode(revsymbol->actions[i], input, coding,
                     ac)) != NULL)) {
            DBGMSG("Function returned bitvector %p:", val);
            DBG(
                  char buf[256]={0};bitvector_hexprint(val, buf, sizeof(buf), " ");fprintf(stderr,"%s\n",buf););
            if (coding != val) {
               if (coding != NULL)
                  bitvector_free(coding);
               coding = val;
            }
         }
         val = NULL;
      }
      return coding;
   } else { //Encoding a regular symbol: we execute all the actions it contains until one is successful
      for (i = 0; i < revsymbol->n_actions; i++) {
         if (((val = revaction_encode(revsymbol->actions[i], input, NULL, ac))
               != NULL)) {
            break;
         }
      }
      return val;
   }
}

/**
 * Performs a reverse semantic action on an input.
 * \param action The structure containing the details about the reverse semantic action
 * \param input The data to encode
 * \param cod Partial coding of this input. Must not be NULL for reverse semantic actions. This is used for actions
 * associated to symbols that can be expanded into symbols containing instructions, for grammars where the template
 * symbol is not directly expanded into expression whose semantic actions represent the building of instructions.
 * \param ac Structure containing the internal variables for the assembly
 * \return The coding for this input or NULL if it could not be encoded
 * */
static bitvector_t* revaction_encode(revaction_t* action, void* input,
      bitvector_t* cod, asmblcontext_t* ac)
{
   if (!action)
      return NULL;
   //Declares arrays for the variables used by the action
   void* vars[ac->n_vars];
   memset(vars, 0, sizeof(vars));
   paramcoding_t vals[ac->n_vals];
   memset(vals, 0, sizeof(vals));
   uint32_t i;
   for (i = 0; i < action->n_revsyms; i++) {
      if (action->revsyms[i]->type == REVSYMPART_T_TOKEN)
         vals[action->revsyms[i]->data.token->var_id].length =
               action->revsyms[i]->data.token->size;
   }
   /**\todo The memset on the arrays everytime is inefficient. However, we actually need variables not used by the matcher function
    * to be set as NULL before invoking revsymbol_encode. We have to test to see whether setting those to NULL only in \ref insnsym_encode
    * would work, but it may cause problems if some variables are used by more than one action and a subsequent test happens to use
    * a variable (element in the array) that was set by another action at a different value. Conversely, this could help for some
    * memoisation.*/
   //Upward reverse action: initialises the input for the symbol containing instructions with the partial coding
   if (action->mainvar_id > 0)
      vars[action->mainvar_id] = cod;
   //Invokes the reverse semantic action to check if it matches with the input and fill the variables
   int ismatch = action->matcher_main(input, vars, vals);
   if (ismatch) { //Match successful. The variables in vars and vals have been updated
      //Initialising the output
      bitvector_t* out = bitvector_new(0);
      //Scans the list of symbols composing the binary expression for this action
      for (i = 0; i < action->n_revsyms; i++) {
         bitvector_t* part;
         switch (action->revsyms[i]->type) {
         case REVSYMPART_T_BINARY:
            //Constant binary value
            bitvector_append(out, action->revsyms[i]->data.constant);
            break;
         case REVSYMPART_T_TOKEN:
            //Numerical value that was updated in the vals array by the matcher function
            bitvector_appendvalue(out,
                  vals[action->revsyms[i]->data.token->var_id].value,
                  action->revsyms[i]->data.token->size,
                  action->revsyms[i]->data.token->endian);
            break;
         case REVSYMPART_T_DEFINE:
            /*Numerical value that was updated in the vals array by the matcher function, and corresponds to a fixed symbol in the binary
             * built from a define (macro), but which is used in a macro. We will compare the output against a reference value*/
            //Builds a bitvector from the value returned by the matcher function
            part = bitvector_new_from_value(
                  vals[action->revsyms[i]->data.define->var_id].value,
                  action->revsyms[i]->data.define->endian,
                  action->revsyms[i]->data.define->size);
            if (bitvector_equal(part,
                  action->revsyms[i]->data.define->constant)) {
               //The value returned is what we expect: we append it to the binary coding
               bitvector_append(out, action->revsyms[i]->data.define->constant);
               bitvector_free(part);
            } else {
               //The value is not what we expect: this is an encoding error
               bitvector_free(part);
               bitvector_free(out);
               return NULL;
            }
            break;
         case REVSYMPART_T_SYMBOL:
            //Other symbol to encode, using as input the value updated in the vars array by the matcher function
            part = revsymbol_encode(action->revsyms[i]->data.symbol,
                  vars[action->revsyms[i]->data.symbol->var_id], NULL, ac);
            if (part == NULL) {
               //Failed to encode this symbol: reverse action failed
               bitvector_free(out);
               return NULL;
            } else {
               //Encoding successful
               bitvector_append(out, part);
               bitvector_free(part);
            }
            break;
         case REVSYMPART_T_INPUT:
            //Element is the symbol containing instructions which we are expanding
            bitvector_append(out, vars[action->mainvar_id]);
            break;
         }
      }
      if (action->revsymup != NULL) {
         //The symbol this action encodes contains additional actions leading up to the template. We execute them
         out = revsymbol_encode(action->revsymup, input, out, ac);
      }
      return out;
   } else
      return NULL;
}

/*
 * Encodes an input (instruction)
 * \param insnsymbol Structure containing the list of possible encodings for this instruction
 * \param input Pointer to the instruction to encode
 * \param n_vars Size of the array containing variables (nonterminals)
 * \param n_vals Size of the array containing tokens (terminals)
 * \return The binary coding for the instruction, or NULL if it could not be encoded
 * */
bitvector_t* insnsym_encode(revsymbol_t* insnsymbol, void* input,
      unsigned int n_vars, unsigned int n_vals)
{
   /**\todo The asmblcontext_t could be created in the driver to avoid creating it at each assembled instruction*/
   bitvector_t *val = NULL;
   asmblcontext_t* ac = asmblcontext_new(n_vars, n_vals);
   val = revsymbol_encode(insnsymbol, input, NULL, ac);
   asmblcontext_free(ac);
   return val;
}
