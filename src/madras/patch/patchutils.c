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
 * \file patchutils.c
 * \brief This file contains helper functions for manipulating the structures used by libmpatch
 * */

#include <inttypes.h>
#include "libmpatch.h"
#include "patchutils.h"

///////////////NEW FUNCTIONS FOR PATCHER REFACTORING
//////////////END OF PATCHER REFACTORING FUNCTIONS
/*
 * Identifies a library type from its name.
 * \param libname Name of the library
 * \return The type of the library, as a constant defined in the enum libtypes. A library whose name ends with .a is
 * considered to be static, while a library whose name ends with .so is considered dynamic. Otherwise UNDEF_LIBRARY is returned
 * */
int getlibtype_byname(char* libname)
{
   int out = UNDEF_LIBRARY;
   if (libname == NULL) {
      //error
      return out;
   }

#ifdef _WIN32
   if (strstr(libname, ".dll") != NULL) {
#else
   if (str_contain(libname, "(\\.so(\\.[0-9.]*)?)$") == 1) {
#endif
      // dynamic library. The name ends with .so or something likes .so.1.2.42
      DBGMSG("Library %s is dynamic\n", libname);
      out = DYNAMIC_LIBRARY;
#ifdef _WIN32
   } else if (strstr(libname, ".lib") != NULL) {
#else
   } else if (str_contain(libname, "(\\.a)$") == 1) {
#endif
      // static library. The name ends with .a
      DBGMSG("Library %s is static\n", libname);
      out = STATIC_LIBRARY;
   } else {
      // error
      DBGMSG("Library %s is of undefined type\n", libname);
      out = UNDEF_LIBRARY;
   }
   return out;
}

/**
 * Returns the string representation of the logical symbol for the type of a condition
 * \param condtype Type of the condition
 * \return String representation of the symbol corresponding to the type of the condition
 * */
static char* condtype_strvalue(int condtype)
{
   switch (condtype) {
   case COND_AND:
      return "&&";
   case COND_OR:
      return "||";
   case COND_EQUAL:
      return "==";
   case COND_NEQUAL:
      return "!=";
   case COND_LESS:
      return "<";
   case COND_GREATER:
      return ">";
   case COND_EQUALLESS:
      return "<=";
   case COND_EQUALGREATER:
      return ">=";
   default:
      return "?";
   }
}

/*
 * Prints a condition to a string
 * \param pf A pointer to the MADRAS structure
 * \param cond The condition
 * \param str The string to print to
 * \param size The size of the string to print to
 * \param arch The target architecture the condition is defined for (assembly operands will be printed as "(null)" if not set)
 * */
void cond_print(cond_t* cond, char* str, size_t size, arch_t* arch)
{
   if ((!cond) || (!str))
      return;

   int char_limit = (size_t) str + size;

   lc_sprintf(str, char_limit - (size_t) str, "(");
   str++;

   if (cond->type < COND_LAST_LOGICAL) {
      cond_print(cond->cond1, str, size, arch);
      while (*str != '\0')
         str++;
      lc_sprintf(str, char_limit - (size_t) str, "%s",
            condtype_strvalue(cond->type));
      while (*str != '\0')
         str++;
      cond_print(cond->cond2, str, size, arch);
      while (*str != '\0')
         str++;
   } else {
      lc_sprintf(str, char_limit - (size_t) str, "\"");
      while (*str != '\0')
         str++;
      oprnd_print(NULL, cond->condop, str, size, arch);
      while (*str != '\0')
         str++;
      lc_sprintf(str, char_limit - (size_t) str, "\"");
      while (*str != '\0')
         str++;
      lc_sprintf(str, char_limit - (size_t) str, "%s",
            condtype_strvalue(cond->type));
      while (*str != '\0')
         str++;
      lc_sprintf(str, char_limit - (size_t) str, "%#"PRIx64, cond->condval);
      while (*str != '\0')
         str++;
   }
   lc_sprintf(str, char_limit - (size_t) str, ")");
   str++;
}

/*
 * Creates a new insfct_t object with the given parameters
 * \param funcname Name of the function to insert
 * \param parameters List of operands to use as parameters for the function
 * \param nparams Size of \e parameters
 * \param optparam Array of options for each parameters
 * \param wrap Flag specifying if the function call must be wrapped with instructions to save/restore the context
 * \return Pointer to the new insfct_t structure
 * */
insfct_t* insfct_new(char* funcname, oprnd_t** parameters, int nparams,
      char* optparam, reg_t** reglist, int nreg)
{
   insfct_t* ifct = NULL;
   ifct = (insfct_t*) lc_malloc0(sizeof(insfct_t));

   ifct->funcname = lc_strdup(funcname);
   ifct->parameters = parameters;
   ifct->nparams = nparams;
   ifct->optparam = optparam;
   ifct->reglist = reglist;
   ifct->nreg = nreg;
   return ifct;
}

#if NOT_USED
/*
 * Creates a new insert_t object with the given parameters
 * \param after Flag specifying if the insertion must be made after the address or before
 * \param insert A pointer to a insfct if the insertion is a function call insert, NULL otherwise
 * \param insns List of instructions to insert if this is not a function call insert
 * \return Pointer to the new insert_t structure
 * */
insert_t* insert_new(char after, insfct_t* insert, queue_t* insns)
{
   insert_t* ins = (insert_t*) lc_malloc0(sizeof(insert_t));

   ins->after = after;
   ins->insnlist = insns;
   ins->fct = insert;
   return ins;
}

/*
 * Creates a new replace_t structure
 * \param dels List of replaced instructions
 * \param ni Number of instructions
 * \param fillver Version of the filler to use
 * \return Pointer to the new replace_t structure
 * \todo Update this function to take into account the current way replace_t is used
 * */
replace_t* replace_new (queue_t* dels) {
{
   replace_t* out = (replace_t*) lc_malloc(sizeof(replace_t));

   out->deleted = dels;
//      out->ninsn = ni;
//      out->fillerver = fillver;
   return out;
}

/*
 * Creates a new delete_t structure
 * \param ni Number of instructions to delete
 * \return Pointer to the new delete_t structure
 * */
delete_t* delete_new () {
{
   delete_t* out = (delete_t*) lc_malloc(sizeof(delete_t));

//      out->ninsn = ni;
   out->deleted = NULL;
   return out;
}
#endif
/*
 * Creates a new modiflbl_t structure
 * \param addr Address where the label must be inserted of modified
 * \param lblname Name of the label to add or modify
 * \param lbltype Type of the label
 * \param linkednode Node containing the instruction to link the label to
 * \param oldname Old name of the label for renaming (not implemented)
 * \param type Type of the label modification to perform
 * \return Pointer to the new modiflbl_t structure or NULL if an error occurred
 * */
modiflbl_t* modiflbl_new(int64_t addr, char* lblname, int lbltype,
      list_t* linkednode, char* oldname, int type)
{
   (void) oldname;//Removing compilation warnings as long as renaming is not implemented
   modiflbl_t* out = NULL;

   switch (type) {
   case NEWLABEL:
      if ((lblname != NULL) && ((addr >= 0) || (linkednode != NULL))) {
         out = lc_malloc0(sizeof(modiflbl_t));
         out->addr = addr;
         out->lblname = lc_strdup(lblname);
         out->lbltype = lbltype;
         out->linkednode = linkednode;
         out->oldname = NULL;
         out->type = type;
      } else {
         ERRMSG(
               "New label name missing or invalid location. Label will not be added\n");
      }
      break;
   case RENAMELABEL:
      if ((lblname != NULL) && (oldname != NULL)) {
         out = lc_malloc0(sizeof(modiflbl_t));
         out->addr = 0;
         out->lblname = lc_strdup(lblname);
         out->lbltype = lbltype;
         out->linkednode = NULL;
         out->oldname = lc_strdup(oldname);
         out->type = type;
      } else {
         ERRMSG(
               "Missing old label name or new label name for label renaming.\n");
      }
      break;
   default:
      ERRMSG("Label modification type not implemented\n")
      ;
   }
   return out;
}

/*
 * Creates a new \ref insnmodify_s structure
 * \param newopcode New name of the opcode for the modified instruction
 * \param newparams Array of new parameters for the modified instruction
 * \param n_newparams Size of \e newparams
 * \param withpadding Flag specifying if a padding must be added if the new instruction has a smaller coding
 * \return A new \ref insnmodify_s structure with the given parameters
 * */
insnmodify_t* insnmodify_new(char* newopcode, oprnd_t** newparams,
      int n_newparams, int withpadding)
{
   insnmodify_t* out = (insnmodify_t*) lc_malloc(sizeof(insnmodify_t));

   if (out) {
      if (newopcode)
         out->newopcode = lc_strdup(newopcode);
      else
         out->newopcode = newopcode;
      out->newparams = newparams;
      out->n_newparams = n_newparams;
      out->withpadding = withpadding;
   }
   return out;
}

/*
 * Creates a new condition
 * \param pf Structure containing the patched file
 * \param condtype Type of the condition
 * \param oprnd Operand whose value is needed for the comparison
 * \param condval Value to compare the operand to (for comparison conditions)
 * \param cond1 Sub-condition to use (for logical conditions)
 * \param cond2 Sub-condition to use (for logical conditions)
 * \return A pointer to the new condition, or NULL if an error occurred
 * In this case, the last error code in \c patchf will be updated
 * */
cond_t* cond_new(void* patchf, int condtype, oprnd_t* oprnd, int64_t condval,
      cond_t* cond1, cond_t* cond2)
{

   patchfile_t *pf = (patchfile_t*) patchf;
   cond_t* out = NULL;
   if ((condtype > COND_VOID) && (condtype < N_CONDTYPES)) {
      if ((oprnd != NULL) && (condtype > COND_LAST_LOGICAL)) {
         out = (cond_t*) lc_malloc0(sizeof(cond_t));
         out->condop = oprnd_copy_generic(oprnd);
         /**\todo (2017-07-26) Assuming here we will not need the potential arch-specific extensions of the operand
          * for the conditions. Otherwise, we will need to somehow pass the architecture into which the operand is defined
          * in order to be able to call the architecture specific version of oprnd_copy*/
         out->condval = condval;
         out->type = condtype;
         if (pf)
            out->cond_id = pf->current_cond_id++;
      } else if ((cond1 != NULL) && (cond2 != NULL)
            && (condtype < COND_LAST_LOGICAL)) {
         out = (cond_t*) lc_malloc0(sizeof(cond_t));
         out->cond1 = cond1;
         out->cond2 = cond2;
         out->cond1->parent = out;
         out->cond2->parent = out;
         out->type = condtype;
         if (pf)
            out->cond_id = pf->current_cond_id++;
      } else {
         ERRMSG(
               "Mismatch between condition type and arguments (%s). Condition will not be added\n",
               ((condtype < COND_LAST_LOGICAL) ?
                     "logical condition with numerical arguments" :
                     "conditional condition with logical arguments"));
         patchfile_set_last_error_code(pf,
               ERR_PATCH_CONDITION_ARGUMENTS_MISMATCH);
      }
   } else {
      ERRMSG("Unrecognized condition type. Condition will not be added\n");
      patchfile_set_last_error_code(pf, ERR_PATCH_CONDITION_TYPE_UNKNOWN);
   }
   DBGMSG(
         "Created new condition %p of type %d with operand %p (copied as %p), value %#"PRIx64", cond1 %p and cond2 %p\n",
         out, condtype, oprnd, out->condop, condval, cond1, cond2);
   return out;

}

/**
 * Parses the string representation of a condition and returns the corresponding string
 * \param patchf Structure containing the patched file (set as a void* to avoid circular dependecies)
 * \param strcond String representation of the condition. The syntax is the same for C: && and || are used respectively for AND and OR logical operators,
 * and <, >, <=, >=, == and != are used for comparison operators. For this function, logical operators can only been used between two conditions, and
 * comparison operators between an assembly operand and a numerical value. Assembly operands must be written between quotes (\").
 * \param pos Pointer to the index into the string at which the parsing starts. If not NULL, will be updated to contain the
 * index at which the operand ends (operand separator or end of string). Should be NULL when invoking this function from outside of itself.
 * \return A pointer to the condition represented by the string, or NULL if an error occurred
 * In this case, the last error code in \c patchf will be updated
 * */
static cond_t* cond_parsenew(void* patchf, char* strcond, int* pos)
{
   patchfile_t* pf = (patchfile_t*) patchf;
   int c;
   int type = COND_VOID;
   cond_t* out = NULL;
   if (pos != NULL)
      c = *pos;
   else
      c = 0;

   //Skips spaces
   while (strcond[c] == ' ')
      c++;
   if (strcond[c] == '(') {
      //We reached the beginning of the condition
      c++;
      //Skips spaces
      while (strcond[c] == ' ')
         c++;
      //Encountering a nested condition
      if (strcond[c] == '(') {
         //Parses the first condition
         cond_t* cond1 = cond_parsenew(pf, strcond, &c);
         if (cond1 == NULL)
            return NULL;
         //Skips spaces
         while (strcond[c] == ' ')
            c++;
         //Decodes the logical operator
         if ((strcond[c] == '&') && (strcond[c + 1] == '&'))
            type = COND_AND;
         else if ((strcond[c] == '|') && (strcond[c + 1] == '|'))
            type = COND_OR;
         else {
            ERRMSG(
                  "Parsing condition %s: unsupported logical operator '%c' at index %d\n",
                  strcond, strcond[c], c);
            cond_free(cond1);
            patchfile_set_last_error_code(pf, ERR_PATCH_CONDITION_PARSE_ERROR);
            return NULL;
         }
         c += 2;
         //Skips spaces
         while (strcond[c] == ' ')
            c++;
         //Parses the second condition
         cond_t* cond2 = cond_parsenew(pf, strcond, &c);
         if (cond2 == NULL) {
            cond_free(cond1);
            return NULL;
         }
         out = cond_new(pf, type, NULL, 0, cond1, cond2);
      } else if (strcond[c] == '\"') {
         c++;
         oprnd_t* op1 = oprnd_parsenew(strcond, &c, asmfile_get_arch(pf->afile));
         if (op1 == NULL)
            return NULL;
         if (strcond[c] != '\"') {
            ERRMSG("Parsing condition %s: missing closing quote at index %d\n",
                  strcond, c);
            oprnd_free(op1);
            patchfile_set_last_error_code(pf, ERR_PATCH_CONDITION_PARSE_ERROR);
            return NULL;
         }
         c++;
         while (strcond[c] == ' ')
            c++;
         if ((strcond[c] == '=') && (strcond[c + 1] == '=')) {
            type = COND_EQUAL;
            c += 2;
         } else if ((strcond[c] == '!') && (strcond[c + 1] == '=')) {
            type = COND_NEQUAL;
            c += 2;
         } else if (strcond[c] == '<')
            if (strcond[c + 1] == '=') {
               type = COND_EQUALLESS;
               c += 2;
            } else {
               type = COND_LESS;
               c++;
            }
         else if (strcond[c] == '>')
            if (strcond[c + 1] == '=') {
               type = COND_EQUALGREATER;
               c += 2;
            } else {
               type = COND_GREATER;
               c++;
            }
         else {
            ERRMSG(
                  "Parsing condition %s: unsupported logical operator '%c' at index %d\n",
                  strcond, strcond[c], c);
            oprnd_free(op1);
            patchfile_set_last_error_code(pf, ERR_PATCH_CONDITION_PARSE_ERROR);
            return NULL;
         }
         //Skips spaces
         while (strcond[c] == ' ')
            c++;
         int64_t val;
         if (parse_number__(strcond, &c, &val) != EXIT_SUCCESS) {
            ERRMSG(
                  "Parsing condition %s: invalid value used in comparison at index %d\n",
                  strcond, c);
            oprnd_free(op1);
            patchfile_set_last_error_code(pf, ERR_PATCH_CONDITION_PARSE_ERROR);
            return NULL;
         }
         //Skips spaces
         while (strcond[c] == ' ')
            c++;
         out = cond_new(pf, type, op1, val, NULL, NULL);
         oprnd_free(op1);
      } else {
         ERRMSG(
               "Unable to parse condition %s. Unexpected character '%c' at index %d\n",
               strcond, strcond[c], c);
         patchfile_set_last_error_code(pf, ERR_PATCH_CONDITION_PARSE_ERROR);
         return NULL;
      }
      //Skips spaces
      while (strcond[c] == ' ')
         c++;
      if (strcond[c] != ')') {
         ERRMSG(
               "Parsing condition %s: missing closing parenthesis at index %d\n",
               strcond, c);
         cond_free(out);
         patchfile_set_last_error_code(pf, ERR_PATCH_CONDITION_PARSE_ERROR);
         return NULL;
      }
      c++; //Skipping ending parenthesis
   } else {
      ERRMSG(
            "Unable to parse condition %s. Expected character '(' at index %d\n",
            strcond, c);
      patchfile_set_last_error_code(pf, ERR_PATCH_CONDITION_PARSE_ERROR);
      return NULL;
   }
   if (pos)
      *pos = c;
   else if ((unsigned int) c != strlen(strcond)) {
      ERRMSG(
            "Condition contains additional characters after index %d (\"%s\")\n",
            c, strcond + c);
      cond_free(out);
      patchfile_set_last_error_code(pf, ERR_PATCH_CONDITION_PARSE_ERROR);
      out = NULL;
   }
   return out;
}

/*
 * Adds a condition to a modification request
 * \param patchf Structure containing the patched file
 * \param modif The modification to add a condition to
 * \param cond The condition to add
 * \param strcond String representation of the condition. Will be used if \c cond is NULL
 * \param condtype If an existing condition was already present for this insertion, the new condition will be logically added to the existing one using
 * this type. If set to 0, COND_AND will be used
 * \param gvars Array of global variables to use in the condition. NOT USED IN THE CURRENT VERSION
 * \return EXIT_SUCCESS if successful, error code otherwise
 * */
int modif_addcond(void* patchf, modif_t* modif, cond_t* cond, char* strcond,
      int condtype, globvar_t** gvars)
{
   /**\todo Implement the use of conditions on other modifications*/
   /**\todo Allow to reference global variables in a condition*/
   /**\todo Handle the case where the operand is RIP-based*/

   patchfile_t* pf = (patchfile_t*) patchf;
   cond_t* condition;
   (void) gvars; /**\todo Implement the use of global variables*/

   if (modif == NULL) {
      ERRMSG("Unable to add condition to modification (modification is NULL)\n");
      return ERR_PATCH_MISSING_MODIF_STRUCTURE;
   }
   if (cond == NULL && strcond == NULL) {
      ERRMSG("Unable to add condition to modification %d (condition is NULL)\n",
            modif->modif_id);
      return ERR_PATCH_CONDITION_MISSING;
   }

   if (modif->type != MODTYPE_INSERT) {
      WRNMSG(
            "Conditions on non-insertion modifications are not supported in this version\n");
      return ERR_PATCH_CONDITION_UNSUPPORTED_MODIF_TYPE;
   }
   if (cond != NULL)
      condition = cond;
   else {
      condition = cond_parsenew(pf, strcond, NULL);
      if (condition == NULL) {
         ERRMSG(
               "Unable to parse condition %s. Condition will not be added to modification %d\n",
               strcond, MODIF_ID(modif));
         int err = patchfile_get_last_error_code(pf);
         if (err == EXIT_SUCCESS)
            return ERR_PATCH_CONDITION_PARSE_ERROR;
         else
            return err;
      }
      DBG(
            char condstr[512];cond_print(condition, condstr, sizeof(condstr), asmfile_get_arch(pf->afile));DBGMSG("String %s was parsed as condition %s\n",strcond,condstr));
   }

   if (modif->condition != NULL) {
      int type;
      type = (
            ((condtype > COND_VOID) && (condtype < COND_LAST_LOGICAL)) ?
                  condtype : COND_AND);
      modif->condition = cond_new(pf, type, NULL, 0, modif->condition,
            condition);
   } else {
      modif->condition = condition;
   }
   DBGMSG("Adding conditions %p with type %d to insertion modification %p\n",
         condition, modif->condition->type, modif);

   return EXIT_SUCCESS;

}

/*
 * Adds a request to a new global variable insertion into the file
 * \param pf Structure containing the patched file
 * \param name Name of the global variable
 * \param type Type of the variable. Currently this is redundant with the value in modvar_t
 * \param size The size in bytes of the global variable
 * \param value A pointer to the value of the global variable (if NULL, will be filled with 0)
 * \return A pointer to the object defining the global variable
 * */
globvar_t* globvar_new(void* patchf, char* name, int type, int size,
      void* value)
{
   /**\todo TODO (2014-11-12) Allow to set the type of the global variable (string, value, etc)
    * Also allow to set the alignment of the variable*/
   if (!patchf)
      return NULL;
   patchfile_t* pf = (patchfile_t*) patchf;
   globvar_t* out = (globvar_t*) lc_malloc0(sizeof(globvar_t));

   out->type = type;
   out->data = data_new_raw(size, value);
//   out->size = size;
//   if ( value != NULL) {
//      out->value = lc_malloc(size);
//      memcpy(out->value,value,size);
//   } else {
//      out->value = value;
//   }
//   if(pf)
   out->globvar_id = pf->current_globvar_id++;
   //Creating a name for the variable name if it does not have one
   if (!name) {
      char buf[16];
      sprintf(buf, "globvar_%d", out->globvar_id);
      out->name = lc_strdup(buf);
   } else
      out->name = lc_strdup(name);
   //And the offset should be initialised to 0 because of malloc0
   return out;
}

/*
 * Adds a request to a new tls variable insertion into the file
 * \param pf Structure containing the patched file
 * \param type Type of the variable. Currently this is redundant with the value in modvar_t
 * \param size The size in bytes of the tls variable
 * \param value A pointer to the value of the tls variable (if NULL, will be filled with 0)
 * \return A pointer to the object defining the tls variable
 * */
tlsvar_t* tlsvar_new(void* patchf, int type, int size, void* value)
{
   patchfile_t* pf = (patchfile_t*) patchf;
   tlsvar_t* out = (tlsvar_t*) lc_malloc0(sizeof(tlsvar_t));

   out->type = type;
   out->size = size;
   if (value != NULL) {
      out->value = lc_malloc(size);
      memcpy(out->value, value, size);
   } else {
      out->value = value;
   }
   if (pf)
      out->tlsvar_id = pf->current_tlsvar_id++;

   return out;
}

/**
 * Creates a new inslib_t structure (details for the insertion of a new library)
 * \param pf Pointer to the madras file handler
 * \param libname Name of the new library
 * \param filedesc Descriptor of the file containing the library (not mandatory, set to 0 if not to be used)
 * \param n_disassembler Pointer to a function allowing to disassemble a file possibly containing multiple binary files
 * \return Pointer to the new inslib_t structure or NULL if an error occurred. In this case, the last error code in \c pf will be updated
 * */
static inslib_t* inslib_new(patchfile_t* pf, char* libname, int filedesc,
      int (*n_disassembler)(asmfile_t*, asmfile_t***, int))
{
   inslib_t* out = NULL;
   int libtype;

   if (pf == NULL || libname == NULL) {
      //error
      return NULL;
   }

   // Identifies the type of library from its name
   libtype = getlibtype_byname(libname);
   if (libtype != UNDEF_LIBRARY) {
      asmfile_t* asmlib;
      out = lc_malloc0(sizeof(inslib_t));
      out->name = lc_strdup(libname);
      out->type = libtype;
      switch (libtype) {
      case STATIC_LIBRARY:
         // Disassembling the static library
         asmlib = asmfile_new(libname);
         asmfile_set_proc(asmlib, asmfile_get_proc(pf->afile));
         /**\todo Separate the case of libraries added because they contain a function and the others.
          * The latters don't need to be disassembled, only parsed. Also, some update to the patcher may
          * remove the need of disassembling those containing functions as well (we could only add an ELF link instead
          * of a branchaddr)*/
         out->n_files = n_disassembler(asmlib, &(out->files), filedesc);
         if (out->n_files <= 0) {
            // An error occurred during disassembly: file will not be added
            /**\todo Handle the case where n_files is 0 and asmlib was correctly disassembled (would be the case for a .o
             * file, not currently accepted anyway)*/
            ERRMSG(
                  "File %s was not found, could not be properly disassembled, or is not a static library. Library will not be added\n",
                  libname);
            int errcode = asmfile_get_last_error_code(asmlib);
            if (errcode != EXIT_SUCCESS)
               patchfile_set_last_error_code(pf, errcode);
            else
               patchfile_set_last_error_code(pf, ERR_COMMON_FILE_INVALID);
            lc_free(out);
            out = NULL;
         }
         break;
      case DYNAMIC_LIBRARY:
         // Dynamic library: not much more to do (in the future, we may also check such libraries)
         out->n_files = 0;
         out->files = NULL;
         break;
      }
   } else {
      ERRMSG(
            "Unable to identify type of library %s from its name (must end with .a or .so). Library will not be added\n",
            libname);
      patchfile_set_last_error_code(pf, ERR_BINARY_LIBRARY_TYPE_UNDEFINED);
   }
   return out;
}

/*
 * Returns the labels defined in an inserted library.
 * \param modlib The \ref modiflib_t structure describing an library insertion
 * \param lbltbl Hashtable (indexed on label names) to be filled with the labels in the library.
 * Must be initialised (using str_hash and str_equal for hashing and comparison functions) or set to NULL if it is not needed.
 * It is assumed at least either \c lbltbl or \c lblqueue is not NULL
 * \param lblqueue Queue to be filled with the labels in the library.
 * Must be initialised or set to NULL if it is not needed. It is assumed at least either \c lbltbl or \c lblqueue is not NULL
 * \return EXIT_SUCCESS if the operation went successfully, error code otherwise (e.g. \c modlib is not a library insertion)
 * */
int modiflib_getlabels(modiflib_t* modlib, hashtable_t* lbltbl,
      queue_t* lblqueue)
{
   if (!modlib)
      return ERR_PATCH_MISSING_MODIF_STRUCTURE;
   if (modlib->type != ADDLIB)
      return ERR_PATCH_WRONG_MODIF_TYPE;
   if ((!lbltbl) && (!lblqueue))
      return EXIT_SUCCESS; //Nothing to be done

   inslib_t* inslib = modlib->data.inslib;
   int i;

   for (i = 0; i < inslib->n_files; i++) {
      FOREACH_INQUEUE(asmfile_get_labels(inslib->files[i]), iter)
      {
         if (lbltbl)
            hashtable_insert(lbltbl, label_get_name(GET_DATA_T(label_t*, iter)),
                  GET_DATA_T(label_t*, iter));
         if (lblqueue)
            queue_add_tail(lblqueue, GET_DATA_T(label_t*, iter));
      }
   }

   return EXIT_SUCCESS;
}

/*
 * Adds the library \e extlibname as a mandatory external library
 * Returns a pointer to the insertion object for the library if the operation succeeded, NULL otherwise
 * We use this function for coherence with the traces
 * \param patchf Pointer to the madras structure
 * \param extlibname Name of the external library
 * \param filedesc Descriptor of the file containing the library (not mandatory, set to 0 if not to be used)
 * \param n_disassembler Pointer to a function allowing to disassemble a file possibly containing multiple binary files
 * \return Pointer to the inslib_t structure containing the details about the insertion or NULL if an error occurred.
 * In this case, the last error code in \c patchf will be updated
 * */
modiflib_t* add_extlib(void* patchf, char* extlibname, int filedesc,
      int (*n_disassembler)(asmfile_t*, asmfile_t***, int))
{
   /**\todo [DONE] This function (and all those that use it) could not be moved to libmpatch because of the use of the disassembler
    * Find some way to remove this dependency => Use a function pointer*/
   patchfile_t* pf = (patchfile_t*) patchf;
   inslib_t* inslib = NULL;
   modiflib_t* modlib = NULL;
   if (pf == NULL || extlibname == NULL) {
      patchfile_set_last_error_code(pf, ERR_COMMON_PARAMETER_MISSING);
      return NULL;
   }

   //Looks for the name of the library in the list of requests
   list_t* iter = queue_iterator(pf->modifs_lib);
   while (iter != NULL) {
      modiflib_t* modiflib = GET_DATA_T(modiflib_t*, iter);
      if ((modiflib->type == ADDLIB)
            && (str_equal(modiflib->data.inslib->name, extlibname)))
         break;
      iter = iter->next;
   }
   if (iter != NULL) {
      //A library insertion request with this name was already found: we return a pointer to the insertion object
      modlib = GET_DATA_T(modiflib_t*, iter);
   } else {
      inslib = inslib_new(pf, extlibname, filedesc, n_disassembler);
      if (inslib != NULL) {
         modlib = modiflib_add(pf, ADDLIB, inslib);
      }
   }
   return modlib;
}

/*
 * Adds a reference between an instruction and a global variable
 * */
static int insngvar_add(void* patchf, insn_t* insn, globvar_t* gvar)
{
   patchfile_t* pf = (patchfile_t*) patchf;
   if ((pf == NULL) || (insn == NULL) || (gvar == NULL))
      return ERR_COMMON_PARAMETER_MISSING;

   insnvar_t* invar = (insnvar_t*) lc_malloc0(sizeof(insnvar_t));
   invar->insn = insn;
   invar->var.gvar = gvar;
   invar->type = GLOBAL_VAR;

   queue_add_tail(pf->insnvars, invar);
   return EXIT_SUCCESS;
}

/*
 * Adds a reference between an instruction and a tls variable
 * */
static int insntlsvar_add(void* patchf, insn_t* insn, tlsvar_t* tlsvar)
{
   patchfile_t* pf = (patchfile_t*) patchf;
   if ((pf == NULL) || (insn == NULL) || (tlsvar == NULL))
      return ERR_COMMON_PARAMETER_MISSING;

   insnvar_t* invar = (insnvar_t*) lc_malloc0(sizeof(insnvar_t));
   invar->insn = insn;
   invar->var.tlsvar = tlsvar;
   invar->type = TLS_VAR;

   queue_add_tail(pf->insnvars, invar);
   return EXIT_SUCCESS;
}

/*
 * Adds an instruction list insertion request
 * \param pf File
 * \param insnq Instruction list
 * \param addr Address.
 * \param node Where to insert the list
 * \param pos Like everywhere else, use your brain
 * \param linkedvars Linked global variables
 * \return A pointer to the modification object containing the insertion to perform or NULL if \c pf is NULL.
 * If an error occurred, the last error code in \c patchf will be updated
 * */
modif_t* insert_newlist(void* patchf, queue_t* insnq, int64_t addr,
      list_t* node, modifpos_t pos, globvar_t** linkedgvars,
      tlsvar_t** linkedtlsvars)
{
   /**\todo [DONE] This function (and all that depends upon it) could not be moved to libmpatch because of the TRACE functions needing
    * \c ed. Find a way to remove this dependency*/
   patchfile_t *pf = (patchfile_t*) patchf;
   if (pf == NULL)
      return NULL;
   if (pos != MODIFPOS_BEFORE && pos != MODIFPOS_AFTER) {
      ERRMSG(
            "[INTERNAL] Requested insertion with invalid position with regard to the given address (%#"PRIx64"). Aborting\n",
            addr);
   }

   //insert_t* inslist=NULL;
   modif_t* mod = NULL;
   if ((queue_length(insnq) > 0)
         && (linkedgvars != NULL || linkedtlsvars != NULL)) {
      //Generating the array of global variables linked to the instructions
      int n_gv = 0, n_tlsv = 0, i = 0;
      FOREACH_INQUEUE(insnq, iter)
      {
//         int isinsn;
         /*Instruction is a reference to a data structure*/
         if (linkedgvars[n_gv] != NULL) {
            //Retrieving the corresponding associated global variable and linking it to the instruction
            //insnvar_add(pf,GET_DATA_T(insn_t*,iter),linkedvars[n_gv]);*
//         int64_t dest;
            /**\todo TODO (2015-05-11) Now using the data_t structures to link an instruction to a global variable
             * The current convention used is that any RIP-based instruction that is not linked to an existing data_t
             * structure is assumed to be a reference to a new global variable. This may evolve over time as the API
             * is refactored, and will need to be updated accordingly in the documentation, but so far I keep it for
             * implementing the refactoring without needing too much updates to the API.*/
            oprnd_t* refop = insn_lookup_ref_oprnd(GET_DATA_T(insn_t*, iter));
            pointer_t* ptr = oprnd_get_memrel_pointer(refop);
            if (ptr && !pointer_get_data_target(ptr)) {
               //The instruction contains a reference to a global variable
               /**\todo The test above is to make sure that we did not pick up a legitimate RIP-based instruction in the added list
                * (when it was duplicated from the original code and is an instruction pointing to a variable),
                * but one that is really 0(%RIP). It has many shortcomings, like assuming that a code could not contain a 0(%rip) address
                * and of course heavily relying on the way insn_check_refs identifies such an instruction. So we really have to find
                * another way to identify instructions to be linked to global variables in inserted lists.
                * => (2015-05-11) Updating this, as explained above. I'm keeping all this until the API itself has been refactored to make
                * all of this clearer*/
//               oprnd_t* itop = NULL, *memory = NULL; int j = 0;
//               while ((itop = insn_get_oprnd(GET_DATA_T(insn_t*,iter), j++)))
//                  if (oprnd_is_mem(itop)) { memory = itop; break; }
//               if (oprnd_getseg(memory) != PTR_ERROR) {
//                  /*Instruction is a reference to a tls data structure*/
//                  if (linkedtlsvars != NULL && linkedtlsvars[n_tlsv] != NULL) {
//                     //Retrieving the corresponding associated tls variable
//                     insntlsvar_add(pf,GET_DATA_T(insn_t*,iter),linkedtlsvars[n_tlsv]);
//                     n_tlsv++;
//                  } else {
//                     //Corresponding tls variable is NULL
//                     char buf[256];
//                     insn_print(GET_DATA_T(insn_t*,iter), buf, pf->afile);
//                     MESSAGE("ERRMSG: Instruction %s must be linked to a tls variable, but it is missing from the array. "
//                           "No further linking will be performed. The patched file may probably fail\n",buf);
//                     break;
//                  }
//               } else {
               /*Instruction is a reference to a data structure*/
               if (linkedgvars != NULL && linkedgvars[n_gv] != NULL) {
                  //Retrieving the corresponding associated global variable
                  insngvar_add(pf, GET_DATA_T(insn_t*, iter),
                        linkedgvars[n_gv]);
                  pointer_set_data_target(ptr, linkedgvars[n_gv]->data);
                  n_gv++;
               } else {
                  //Corresponding global variable is NULL
                  char buf[256];
                  insn_print(GET_DATA_T(insn_t*, iter), buf, sizeof(buf));
                  ERRMSG(
                        "Instruction %s must be linked to a global variable, but it is missing from the array. " "No further linking will be performed. The patched file may probably fail\n",
                        buf);
                  pf->last_error_code = ERR_PATCH_REFERENCED_GLOBVAR_MISSING;
                  break;
               }
//               }
            }
         }

         i++;

         /*Marks all added instructions as new in the code. This will allow to differentiate them from the original ones*/
         insn_add_annotate(GET_DATA_T(insn_t*, iter), A_PATCHNEW);
      }
   }

   //Creating the insertion request and adding it to the list of requests
//   inslist = insert_new(after,NULL,insnq);
   mod = modif_add(pf, addr, node, MODTYPE_INSERT, pos);
   if (mod) {
      mod->newinsns = insnq;
   }
   return mod;
}

/**
 * Creates a new code modification object
 * \param patchf Structure containing the patched file (set as a void* to avoid circular dependencies)
 * \param addr Address at which the modification will take place
 * \param modifnode Pointer to the node containing the instruction at which the modification will take place
 * \param type Type of the modification
 * \param after Set to TRUE if the modification is to be inserted after the given instruction, FALSE if it must be inserted before.
 * This parameter is ignored for modifications
 * \param mod Pointer to the object containing the types modification
 * \return A pointer to the new modification structure
 * */
static modif_t* modif_new(void* patchf, int64_t addr, list_t* modifnode,
      int type, modifpos_t pos)
{
   patchfile_t* pf = (patchfile_t*) patchf;
   modif_t* modif = NULL;
   modif = (modif_t*) lc_malloc0(sizeof(modif_t));
   modif->addr = addr;
   modif->type = type;
   modif->modifnode = modifnode;
   modif->stackshift = pf->stackshift;
   modif->flags = pf->flags;
   modif->position = pos;
//   switch(type) {
//   case MODTYPE_INSERT:
//      modif->data.insert = (insert_t*)mod;
//   break;
//   case MODTYPE_REPLACE:
//      modif->data.replace = (replace_t*)mod;
//   break;
//   case MODTYPE_DELETE:
//      modif->data.delete = (delete_t*)mod;
//   break;
//   case MODTYPE_MODIFY:
//      modif->data.insnmodify = (insnmodify_t*)mod;
//      /**\todo REFONTE: Récupérer le code de insnmodify_apply et patchfile_patch_replaceinsns pour
//       * vérifier si la taille de l'instruction modifiée doit changer. Si oui, on lance le processus
//       * de création d'un movedblock*/
//   break;
//   case MODTYPE_RELOCATE:
//      //No modification performed, but we will consider this as an instruction modification anyway
//      modif->data.insnmodify = (insnmodify_t*)mod;
//   break;
//   }
   /**\todo REFONTE: Rechercher le bloc autour de cette adresse (on se base ici sur la taille définie au niveau du patchfile)
    * Appeler movedblock_new et l'ajouter à patchfile
    * => (2015-05-07) Ooooold Todo. Done elsewhere. To be removed*/
   modif->modif_id = pf->current_modif_id++;
   DBGMSG(
         "Created new modif %p with type %d, address %#"PRIx64", modifnode %p and position %d\n",
         modif, type, addr, modifnode, pos);
   return modif;
}

/**
 * Compares two modifications depending on a flag.
 * \param modif1 First modif
 * \param modif2 Second modif
 * \return -1 if \c modif1 is flagged with \c flag and not \c modif2,
 * 1 if \c modif2 is flagged with \c flag and not \c modif1, and 0 if both or neither are flagged with \c flag
 * */
static int modif_cmp_flags(modif_t* modif1, modif_t* modif2, int flag)
{
   assert(modif1 && modif2);
   if ((modif1->flags & flag) && !(modif2->flags & flag))
      return -1;  //modif1 is flagged and not modif2
   else if (!(modif1->flags & flag) && (modif2->flags & flag))
      return 1;   //modif2 is flagged and not modif1
   else
      return 0;   //Both or neither are flagged
}

/*
 * Compares two modifications depending on their addresses, type, position, and order of insertion
 * \param m1 First modification
 * \param m2 Second modification
 * \return -1 if m1 is less than m2, 0 if m1 equals m2, 1 if m1 is greater than m2
 * The ordering obeys the following rules, in order of priority:
 * - Order of addresses (m1->addr < m2->addr => m1 < m2)
 * - Types: MODTYPE_INSERT+before < MODTYPE_MODIFY < MODTYPE_REPLACE < MODTYPE_DELETE < MODTYPE_INSERT+after
 * - The address at which m1 takes place is strictly less than the address at which m2 takes place
 * - Between two insertions at the same address and the same position after/before the address:
 *   - m1 < m2 if m1 is flagged for not updating branches leading to it while m2 is not
 *   - m1 < m2 if m1 has a lower modif_id than m2 (it was requested before m2)
 * */
int modif_cmp_qsort(const void* m1, const void* m2)
{
   /**\note About the tie being broken on modif_id for insertions at the same address and position: This is a bit of a hack.
    * The thing is, multiple insertions at the same address would not actually be performed in the same order:
    * insertion before an address would end up being performed in the same order they were requested in,
    * but insertions after will be performed in the reverse order.
    * [ Here is why: suppose I insert my code c after instruction x. I will have x;c. Now I have to insert c1 after
    * instruction x. Then I will have x;c1;c, since I always insert AFTER x, but the patcher has no way of knowing
    * that c is something that was inserted after x (it could have been for instance something to insert before the
    * instruction after x). For the same reason, code inserted before will be inserted in the right order ]
    * So we will cheat, and order the list of insertions so that all insertions before the same address will
    * be ordered into the same order they were requested, while insertions after the same address will be
    * ordered into the reverse order they were requested in. Therefore, insertions at the same address & position
    * will always be effectively performed in the same order they were requested in.
    * ===> (2015-05-20) It has changed now that the refactoring processes modifications sequentially, so there is
    * not need to distinguish between the cases before and after. I think*/
   /**\todo TODO (2015-05-20) Rewrite or remove the commentary above if we indeed don't need the hack anymore*/
   /**\todo (2014-12-18) Rewrite to get a shorter code. */
   modif_t* modif1 = *(modif_t**) m1;
   modif_t* modif2 = *(modif_t**) m2;

   if (modif1->addr < modif2->addr)
      return -1; //modif1 is at a strictly inferior address than modif2: it will be set first
   else if (modif1->addr > modif2->addr)
      return 1; //modif1 is at a strictly superior address than modif2: it will be set last
   else {
      //Both modifications are at the same address
      if ((modif1->type == MODTYPE_INSERT)
            && (modif2->type == MODTYPE_INSERT)) {
         //Both modifications are insertions
         if (modif1->position < modif2->position)
            return -1; //modif1 is set before and modif2 after: modif1 will come first
         else if (modif1->position > modif2->position)
            return 1; //modif1 is set after and modif2 before: modif1 will come last
         else {
//         else if ( (modif1->data.insert->after == FALSE) && (modif2->data.insert->after == FALSE) ) {
            //Both modifications are set at the same position: ordering them by update flags
            int cmpflag;
//            //Checking if one has the flag for not updating branch destination
//            cmpflag = modif_cmp_flags(modif1, modif2, PATCHFLAG_BRANCH_NO_UPD_DST);
//            if (cmpflag)
//               return cmpflag;   //One is flagged and not the other: ordering is done
            //Checking if one has the flag for not updating branches from the same function
            cmpflag = modif_cmp_flags(modif1, modif2,
                  PATCHFLAG_INSERT_NO_UPD_FROMFCT);
            if (cmpflag)
               return cmpflag; //One is flagged and not the other: ordering is done
            //Checking if one has the flag for not updating branches from another function
            cmpflag = modif_cmp_flags(modif1, modif2,
                  PATCHFLAG_INSERT_NO_UPD_OUTFCT);
            if (cmpflag)
               return cmpflag; //One is flagged and not the other: ordering is done
            //Checking if one has the flag for not updating branches from the same loop
            cmpflag = modif_cmp_flags(modif1, modif2,
                  PATCHFLAG_INSERT_NO_UPD_FROMLOOP);
            if (cmpflag)
               return cmpflag; //One is flagged and not the other: ordering is done

            //Both modification have the same update flag(s): ordering by increasing modif_id (so that those requested first will be inserted first)
            if (modif1->modif_id < modif2->modif_id)
               return -1;  //modif1 was requested before modif2
            else if (modif1->modif_id > modif2->modif_id)
               return 1;   //modif1 was requested after modif2
            else
               return 0; //Should never happen (modif_id should be unique)
//         } else if ( (modif1->data.insert->after == TRUE) && (modif2->data.insert->after == TRUE) ) {
//            //Both modifications are set after: ordering by decreasing modif_id (so that those requested first will be inserted last and executed first)
//            if (modif1->modif_id > modif2->modif_id)
//               return -1;  //modif1 was requested before modif2
//            else if (modif1->modif_id < modif2->modif_id)
//               return 1;  //modif1 was requested after modif2
//            else
//               return 0; //Should never happen (modif_id should be unique)
         }
      } else if (modif1->type == MODTYPE_INSERT) {
         //modif1 is an insertion (and modif2 is not)
         if (modif1->position == MODIFPOS_BEFORE) {
            //modif1 is an insertion before: it is always inferior
            return -1;
         } else if (modif1->position == MODIFPOS_AFTER) {
            //modif1 is an insertion after: it is always superior
            return 1;
         } else
            return 0; //Should never happen, it's after or before, but it makes the compiler happy
      } else if (modif2->type == MODTYPE_INSERT) {
         //modif2 is an insertion (and modif1 is not)
         if (modif1->position == MODIFPOS_BEFORE) {
            //modif2 is an insertion before: it is always inferior
            return 1;
         } else if (modif1->position == MODIFPOS_AFTER) {
            //modif2 is an insertion after: it is always superior
            return -1;
         } else
            return 0; //Should never happen, it's after or before, but it makes the compiler happy
      } else {
         //Neither modification is an insertion: using the order of the type identifier
         if (modif1->type < modif2->type)
            return -1;
         else if (modif1->type > modif2->type)
            return 1;
         else
            return 0; //This would not make much sense, as it would mean there was two deletions, replacement or modifications at the same address
      }
   }
}

/*
 * Adds the modification request in a list of modifications
 * The list is ordered with the lowest insertion addresses at the beginning
 * \param pf Structure containing the patched file
 * \param addr The address at which the modification must take place
 * \param modifnode List object containing the instruction at \e addr
 * \param type The type of modification
 * \param pos The position of the modification with regard to the given instruction or address
 * \return A pointer to the structure representing the modification
 * */
modif_t* modif_add(void* patchf, int64_t addr, list_t* modifnode,
      modiftype_t type, modifpos_t pos)
{
   patchfile_t* pf = (patchfile_t*) patchf;
   if (pf == NULL)
      return NULL;

   modif_t* modif = NULL;
   if (addr == ADDRESS_ERROR) {
      if (!modifnode) {
         ERRMSG(
               "Requested modification has no valid address: modification will not be added\n");
         return NULL;
      }
      addr = insn_get_addr(GET_DATA_T(insn_t*, modifnode));
   }
   if (addr == 0 && !modifnode)
      pos = MODIFPOS_FLOATING;

   DBGMSGLVL(1, "Adding modif request of type %d around address %#"PRIx64"\n",
         type, addr);
   //Creates the modification object
   modif = modif_new(pf, addr, modifnode, type, pos);
   queue_add_tail(pf->modifs, modif);
   /**\todo TODO (2014-10-28) Check whether we still need to order modification requests. We may only need to order those before/after a
    * given address, or simply to order the corresponding movedblocks*/
   return modif;
}

/*
 * Checks whether a modification is processed.
 * \param modif The modification
 * \return TRUE if the modification exists and has been processed, FALSE otherwise
 * */
int modif_isprocessed(modif_t* modif)
{
   /**\todo TODO (2014-10-28) Since I'm making this up as I go along, the annotation used to check if the modification has been processed may change
    * Basically this happens once the corresponding movedblock has been created and enqueued. This is basically the sole reason for this getter*/
   return (modif && (modif->annotate & A_MODIF_PROCESSED)) ? TRUE : FALSE;
}

/*
 * Checks whether a modification is fixed. A fixed modification can not be removed nor marked as not fixed.
 * \param modif The modification
 * \return TRUE if the modification exists, is flagged as fixed and has been processed, FALSE otherwise
 * */
int modif_isfixed(modif_t* modif)
{
   return
         (modif_isprocessed(modif) && (modif->flags & PATCHFLAG_MODIF_FIXED)) ?
               TRUE : FALSE;
}

/*
 * Removes a modification request from the list of modifications
 * \param patchf Structure containing the patched file
 * \param modif The modification. It will be freed if the removal was successful.
 * \return TRUE if the modification could be successfully removed, FALSE if the modification is fixed, NULL, or not found as a pending modification
 * */
int modif_remove(void* patchf, modif_t* modif)
{
   if (!patchf || !modif)
      return FALSE;

   patchfile_t* pf = (patchfile_t*) patchf;
   //First checking whether the modification is marked as fixed
   if (modif_isfixed(modif)) {
      WRNMSG(
            "Unable to remove modification %d: modification is flagged as fixed and has been committed\n",
            modif->modif_id);
      return FALSE;
   }

   modif->annotate |= A_MODIF_CANCEL;
   if (modif->movedblock) {
//      modif->movedblock->size -= modif->patchmodif->size;
      /**\todo TODO (2015-01-16) I removed the size from the members of movedblock_t, but we still need to keep somewhere the end address
       * of movedblocks for future insertions
       * =>(2015-03-03) And guess what, I'm restoring the size to the movedblock. Although I may not need it after all. What counts is the available
       * size reachable by direct branches.*/
      queue_remove(modif->movedblock->modifs, modif, NULL);
      //Updating the size of the block
      modif->movedblock->newsize -= modif->size;
      //Updating the available size for moved code reachable with a direct branch
      if (modif->movedblock->jumptype == JUMP_DIRECT)
         pf->availsz_codedirect += modif->size;
   }
   /**\todo TODO (2014-11-18) We have a lot of checks to do here (not ordered):
    * - MAYBE Flag the modification as deleted instead of actually removing it
    * - Check if modification had predecessors and a fixed address, raise a warning if yes and exit
    * - If the modif had not been committed, simply remove it
    * - Remove the modification from the modifications from its moved block. Check if the block has any remaining modification
    *    - If so, invoke removal of the block (this implies unflagging instructions)
    * (more to be added I think about them)
    * => (2014-12-17) Updated function, comments above probably don't apply anymore, but you never known, check if it's the case
    * */
   return TRUE;
}

/*
 * Adds the library modification request in a list of library modifications
 * \param pf Structure containing the patched file
 * \param type The type of modification
 * \param mod The modification to add (its type must be in accordance with \e type)
 * \return A pointer to the structure representing the modification
 * */
modiflib_t* modiflib_add(void* patchf, int type, void* mod)
{
   patchfile_t* pf = (patchfile_t*) patchf;
   if (pf == NULL)
      return NULL;

   modiflib_t* modif = (modiflib_t*) lc_malloc0(sizeof(modiflib_t));
   modif->type = type;
   modif->modiflib_id = pf->current_modiflib_id++;
   switch (type) {
   case ADDLIB:
      modif->data.inslib = mod;
      break;
   case RENAMELIB:
      modif->data.rename = mod;
      break;
   }
   queue_add_tail(pf->modifs_lib, modif);
   return modif;
}

/*
 * Adds the variable modification request in a list of variable modifications
 * \param pf Structure containing the patched file
 * \param type The type of modification
 * \param mod The modification to add (its type must be in accordance with /c type)
 * */
void modifvars_add(void* patchf, int type, void* mod)
{
   patchfile_t* pf = (patchfile_t*) patchf;
   if (pf == NULL)
      return;

   modifvar_t* modif = (modifvar_t*) lc_malloc0(sizeof(modifvar_t));
   modif->type = type;
   switch (type) {
   case NOUPDATE:
   case ADDGLOBVAR:
      modif->data.newglobvar = (globvar_t*) mod;
      break;
   case ADDTLSVAR:
      modif->data.newtlsvar = (tlsvar_t*) mod;
      break;
   }
   queue_add_tail(pf->modifs_var, modif);
}

/*
 * Compares two variable modification requests by alignment
 * \param m1 First modification
 * \param m2 Second modification
 * \return -1 if m1 has a higher alignment than m2, 1 if m2 has a higher alignment than m1, 0 if both have the
 * same alignment. So far we assume all modifications are variable insertions
 * */
int modifvar_cmpbyalign_qsort(const void* m1, const void* m2)
{
   modifvar_t* modif1 = *(modifvar_t**) m1;
   modifvar_t* modif2 = *(modifvar_t**) m2;

   assert(modif1->type == ADDGLOBVAR && modif2->type == ADDGLOBVAR);
   /**\todo (2015-05-12) If the above assert fails, it means we added a new type of variable modification request
    * (probably modifying an existing variable). Update the code below accordingly to take the new type into account*/

   if (modif1->data.newglobvar->align > modif2->data.newglobvar->align)
      return -1;
   else if (modif1->data.newglobvar->align < modif2->data.newglobvar->align)
      return 1;
   else
      return 0;
}

/*
 * Creates a new structure storing the data needed to generate the code associated to a condition
 * \param nconds The number of comparisons used in the condition
 * \return A new insertcond structure
 * */
insertconds_t* insertconds_new(int nconds)
{
   insertconds_t* insertconds = NULL;
   if (nconds > 0) {
      insertconds = lc_malloc0(sizeof(insertconds_t));
      insertconds->condoprnds = (oprnd_t**) lc_malloc0(
            sizeof(oprnd_t*) * nconds);
      insertconds->condtypes = (char*) lc_malloc0(sizeof(char) * nconds);
      insertconds->condvals = (int64_t*) lc_malloc0(sizeof(int64_t) * nconds);
      insertconds->conddst = (int*) lc_malloc0(sizeof(int) * nconds);
   }
   return insertconds;
}

/*
 * Frees a structure storing the data needed to generate the code associated to a condition
 * \param The insertcond structure to free
 * */
void insertconds_free(insertconds_t* insertconds)
{
   if ((insertconds != NULL) && (insertconds->nconds > 0)) {
      lc_free(insertconds->condoprnds);
      lc_free(insertconds->condtypes);
      lc_free(insertconds->condvals);
      lc_free(insertconds->conddst);
      lc_free(insertconds);
   }
}

/*
 * Frees a condition and all sub-conditions
 * \param cond Pointer to the condition to free
 * */
void cond_free(cond_t* cond)
{
   DBGMSG("Freeing condition %p\n", cond);
   if (cond != NULL) {
      if (cond->condop != NULL)
         oprnd_free(cond->condop);
      cond->condop = NULL;
      if (cond->cond1 != NULL)
         cond_free(cond->cond1);
      cond->cond1 = NULL;
      if (cond->cond2 != NULL)
         cond_free(cond->cond2);
      cond->cond2 = NULL;
      insertconds_free(cond->insertconds);
      lc_free(cond);
   }
}

/*
 * Creates a new insertfunc_t structure
 * \param funcname Name of the function
 * \param functype Type of the function
 * \param libname Library where the function is defined
 * \return New structure
 * */
insertfunc_t* insertfunc_new(char* funcname, unsigned functype, char* libname)
{
   insertfunc_t* out = lc_malloc0(sizeof(*out));
   out->name = funcname;
   out->type = functype;
   out->libname = libname;
   return out;
}

/*
 * Frees and insertfunc_t structure
 * \param i The structure to free
 * */
void insertfunc_free(void* i)
{
   insertfunc_t* insertfunc = i;
   if (!insertfunc)
      return;
   pointer_free(insertfunc->fctptr);
   lc_free(insertfunc);
}

/*
 * Frees an insertion function request
 * \param ifc The insertion function request structure
 * \bug This function does not frees the allocated oprnd_t structures (as they can be pointers
 * to parameters in the instruction list)
 * */
void insfct_free(void* ifc)
{
   if (!ifc)
      return;
   insfct_t *ifct = (insfct_t*) ifc;
   int i;

   for (i = 0; i < ifct->nparams; i++) {
      oprnd_free(ifct->parameters[i]);
   }

   lc_free(ifct->funcname);
   lc_free(ifct->parameters);
   lc_free(ifct->optparam);
   lc_free(ifct->paramvars);
   lc_free(ifct);
}

#if NOT_USED_ANYMORE
/*
 * Frees an insertion request
 * \param ins The insertion request structure
 * */
void insert_free(void *ins)
{
   if (!ins)
   return;

   insert_t* inser = (insert_t*) ins;
   if (inser->fct)
   insfct_free(inser->fct);
   //lc_free(inser->insnlist);
   /**\bug If we appended the queue to another one (modif next), this will cause a seg fault as the queue as already been
    * freed and is appended anyway. We have to retrieve the modif flag to check for A_MODIF_ATTACHED
    * \todo TODO (2015-05-27) Removing this as it should be freed when the patchmodif is created, but it's not quite
    * streamlined that way. Find something to reduce the number of structures per modifications*/
   lc_free(inser);
}

/*
 * Frees a deletion request
 * \param d The deletion request structure
 * */
void delete_free(void* d)
{
   if (!d)
   return;

   delete_t* del = (delete_t*) d;
   if (del->deleted) {
      queue_free(del->deleted, insn_free);
   }
   lc_free(del);
}

/*
 * Frees an instruction replacement request
 * \param r The replacement request structure
 * \todo Like all uses of replace_t, this has to be updated when the actual use of this structure is clarified
 * */
void replace_free(void* r)
{
   if (!r)
   return;

   replace_t* rep = (replace_t*) r;
   if (rep->deleted) {
      queue_free(rep->deleted, insn_free);
   }
   lc_free(rep);
}
#endif
/*
 * Frees a library insertion request
 * \param inslib Pointer to the library insertion request
 * */
void inslib_free(inslib_t* inslib)
{
   if (inslib != NULL) {
      if (inslib->n_files > 0) {
         // Frees the asmfiles of an archive library
         int i;
         for (i = 0; i < inslib->n_files; i++) {
            asmfile_free(inslib->files[i]);
         }
         lc_free(inslib->files);
      }
      lc_free(inslib->name);
      lc_free(inslib);
   }
}

/*
 * Frees an instruction modification request
 * \param imod The modification request structure
 * */
void insnmodify_free(void *imod)
{
   if (!imod)
      return;

   int i;
   insnmodify_t* insmod = (insnmodify_t*) imod;

   lc_free(insmod->newopcode);
   for (i = 0; i < insmod->n_newparams; i++)
      oprnd_free(insmod->newparams[i]);
   lc_free(insmod->newparams);
   lc_free(insmod);
}

/*
 * Frees a global variable structure
 * \param gv The structure to free
 * */
void globvar_free(globvar_t* gv)
{
   if (gv == NULL)
      return;
//      lc_free(gv->value);
   //data_free(gv->data);
   /**\todo TODO (2015-05-19) Removing the freeing of gv->data as it is done when freeing the binary section where the data
    * has been stored. If the case could happen where a globvar is not affected to a section, we would need to have a flag to
    * set when we do, so that we know if we need to delete the data here or not*/
   lc_free(gv->name);
   lc_free(gv);
}

/*
 * Frees a variable modification request
 * \param vmod Pointer to the structure holding the details about the variable modification request
 * */
void modifvar_free(void* vmod)
{
   if (vmod != NULL) {
      modifvar_t* modifvar = (modifvar_t*) vmod;
      switch (modifvar->type) {
      case NOUPDATE:
      case ADDGLOBVAR:
         globvar_free(modifvar->data.newglobvar);
         break;
      }
      lc_free(modifvar);
   }
}

/*
 * Frees a library modification request
 * \param lmod Pointer to the structure holding the details about the library modification request
 * */
void modiflib_free(void* lmod)
{
   if (lmod != NULL) {
      modiflib_t* modiflib = (modiflib_t*) lmod;
      switch (modiflib->type) {
      case RENAMELIB:
         lc_free(modiflib->data.rename);
         break;
      case ADDLIB:
         inslib_free(modiflib->data.inslib);
         break;
      }
      lc_free(modiflib);
   }
}

/*
 * Frees a label modification request
 * \param lblmod Pointer to the structure holding the details about the label modification request
 * */
void modiflbl_free(void* lblmod)
{
   if (lblmod != NULL) {
      modiflbl_t* modiflbl = (modiflbl_t*) lblmod;
      lc_free(modiflbl->lblname);
      lc_free(modiflbl);
   }
}

/*
 * Creates a new patchmodif_t structure
 * \param firstinsnseq Node containing the first instruction modified by this modification
 * \param lastinsnseq Node containing the last instruction modified by this modification
 * \param newinsns Queue of insn_t associated to this modification (can be NULL for deletion)
 * \param position Where the modification takes place with regard to the original instruction(s)
 * \param size Size in byte of the modification
 * */
patchmodif_t* patchmodif_new(list_t* firstinsnseq, list_t* lastinsnseq,
      queue_t* newinsns, uint8_t position, int64_t size)
{
   //assert(firstinsnseq && lastinsnseq);
   patchmodif_t* out = lc_malloc(sizeof(*out));
   out->firstinsnseq = firstinsnseq;
   out->lastinsnseq = lastinsnseq;
   out->newinsns = newinsns;
   /**\todo TODO (2014-12-15) The newinsns queue should contain patchinsn_t structures (for easier upates later on) returned by the
    * architecture specific driver for the patcher.
    * => (2014-12-18) Done
    * => (2015-01-06) Changed AGAIN to insn_t structures. I want to associate a insn_t to a patchinsn_t using a hashtable, and so for that
    * I need to do it only when I'm sure I'll keep the patchinsn_t structures I created, so that's when I finalise everything.*/
   out->position = position;
   out->size = size;
   return out;
}

/*
 * Frees a patchmodif_t structure
 * \param pm The patchmodif structure
 * */
void patchmodif_free(patchmodif_t* pm)
{
   if (!pm)
      return;
   if (pm->newinsns)
      queue_free(pm->newinsns, NULL);
   /**\todo TODO (2015-05-27) Maybe add a test here to check whether the modification has been applied or not
    * to know if we need to delete the instructions inside (use insn_free in the queue_free)*/
   lc_free(pm);
}

/*
 * Frees an code modification request (this includes
 * \param cmod Pointer to the structure holding the details about the instruction modification request
 * */
void modif_free(void* cmod)
{
   if (!cmod)
      return;
   modif_t* modifcode = (modif_t*) cmod;
   DBGMSG("Freeing modif %d (%p)\n", modifcode->modif_id, modifcode);
//   switch( modifcode->type) {
//   case MODTYPE_INSERT:	//Code insertion
//      insert_free(modifcode->data.insert);
//   break;
//   case MODTYPE_REPLACE:	//Replacement of whole instructions by others
//      replace_free(modifcode->data.replace);
//   break;
//   case MODTYPE_MODIFY:	//Modification of an instruction
//      insnmodify_free(modifcode->data.insnmodify);
//   break;
//   case MODTYPE_DELETE:	//Code deletions
//      delete_free(modifcode->data.delete);
//   break;
//   }
   if (modifcode->insnmodify)
      insnmodify_free(modifcode->insnmodify);
   if (modifcode->fct)
      insfct_free(modifcode->fct);
   insn_free(modifcode->paddinginsn);
   if (modifcode->linkedcondmodifs)
      queue_free(modifcode->linkedcondmodifs, NULL);
   /**\todo TODO (2014-11-18) WARNING! See how we handle those modifications with the same condition, did we copy the cond_t over all of them or simply
    * used the same one. If so, we have to avoid freeing it multiple time (we would enforce that the head of the queue is the one with the original condition)*/
   cond_free(modifcode->condition);
   if (modifcode->previousmodifs)
      queue_free(modifcode->previousmodifs, NULL);
   if (modifcode->position != MODIFPOS_FLOATING)
      queue_free(modifcode->newinsns, NULL);
//   if (modifcode->patchmodif)
//      patchmodif_free(modifcode->patchmodif);

   lc_free(cmod);
}

/*
 * Adds a parameter to a function call request
 * \param ifct Pointer to the structure containing the details about the function call request
 * \param oprnd Pointer to an operand structure describing the operand to use as parameter
 * \param opt Option about this operand
 * \return EXIT_SUCCESS success, error code otherwise
 * */
int fctcall_add_param(insfct_t* ifct, oprnd_t* oprnd, char opt)
{
   if (ifct == NULL) {
      //error
      return ERR_COMMON_PARAMETER_MISSING;
   }

   /**\todo Update error code to conform with the norm for error handling*/
   ifct->parameters = (oprnd_t**) lc_realloc(ifct->parameters,
         sizeof(oprnd_t*) * (ifct->nparams + 1));
   ifct->parameters[ifct->nparams] = oprnd;
   //Adds the option to the parameters' options
   ifct->optparam = (char*) lc_realloc(ifct->optparam,
         sizeof(char) * (ifct->nparams + 1));
   ifct->optparam[ifct->nparams] = opt;

   //Reserves a space in the array for global variables used by the function call
   ifct->paramvars = (globvar_t**) lc_realloc(ifct->paramvars,
         sizeof(globvar_t*) * (ifct->nparams + 1));
   ifct->paramvars[ifct->nparams] = NULL;

   ifct->paramtlsvars = (tlsvar_t**) lc_realloc(ifct->paramtlsvars,
         sizeof(tlsvar_t*) * (ifct->nparams + 1));
   ifct->paramtlsvars[ifct->nparams] = NULL;

   ifct->nparams++;
   return EXIT_SUCCESS;
}

