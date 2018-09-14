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

///////////////////////////////////////////////////////////////////////////////
//                                  project                                  //
///////////////////////////////////////////////////////////////////////////////

/**
 * Initialises the list of exit functions in the project if they were not already set
 * \param project The project
 * */
static void project_init_exit_functions(project_t* project)
{
   char* dflt_exit_fcts_names[] = {DEFAULT_EXIT_FUNCTIONS_NAMES};
   size_t n_dflt_exit_fcts_names = sizeof(dflt_exit_fcts_names) / sizeof(*dflt_exit_fcts_names);

   char** exit_fcts = lc_malloc((n_dflt_exit_fcts_names + 1) * sizeof(char*));
   size_t i;
   for (i = 0; i < n_dflt_exit_fcts_names; i++)
      exit_fcts[i] = lc_strdup(dflt_exit_fcts_names[i]);
   exit_fcts[i] = NULL;
   DBGLVL(1, FCTNAMEMSG0("Initialising list of exit function names to: ");
      int _dfct = 0;while(exit_fcts[_dfct] != NULL) STDMSG("%s ", exit_fcts[_dfct++]);STDMSG("\n");)
   project_set_exit_fcts(project, exit_fcts);
}

/////////////////////////// Constructor/destructor ////////////////////////////

/*
 * Creates a new project
 * \param name name of the project (and file which contains project data)
 * \return a new initialized project
 */
project_t* project_new(char* name)
{
   project_t* new = lc_malloc0(sizeof(*new));

   new->asmfiles = queue_new();
   new->asmfile_table = hashtable_new(str_hash, str_equal);
   new->file = lc_strdup(name);
   new->cc_mode = CCMODE_DEBUG;

   // Initialising list of exit functions (functions names to be used as exits in functions)
   project_init_exit_functions(new);

   return new;
}

/*
 * Duplicates an existing project
 * \param p a project to duplicate
 * \return a copy of p or NULL
 */
project_t* project_dup(project_t* p)
{
   if (p == NULL)
      return NULL;

   project_t* new = lc_malloc0(sizeof(*new));

   new->asmfiles = queue_new();
   new->asmfile_table = hashtable_new(str_hash, str_equal);
   new->file = lc_strdup(p->file);
   new->comp_code = p->comp_code;
   new->lang_code = p->lang_code;
   new->proc = p->proc;
   if (p->uarch_name != NULL)
      new->uarch_name = lc_strdup(p->uarch_name);
   if (p->proc_name != NULL)
      new->proc_name = lc_strdup(p->proc_name);
   new->cc_mode = p->cc_mode;
   memcpy(new->params, p->params, sizeof(p->params)); // sizeof returns expected size for arrays
   // whose size is known at compile time

   // Duplicates the array of exit functions names
   if (p->exit_functions != NULL) {
      size_t i, nb_exits = 0;
      while (p->exit_functions[nb_exits] != NULL) nb_exits++;
      new->exit_functions = lc_malloc(sizeof(char*)*(nb_exits+1));
      for (i = 0; i < nb_exits; i++)
         new->exit_functions[i] = lc_strdup(p->exit_functions[i]);
      new->exit_functions[i] = NULL;
   }


   return new;
}

/*
 * Deletes an existing project
 * \param p a project
 */
void project_free(project_t* p)
{
   if (p == NULL)
      return;

   if (p->exit_functions != NULL) {
      int i = 0;
      while (p->exit_functions[i] != NULL) {
         lc_free(p->exit_functions[i]);
         i++;
      }
      lc_free(p->exit_functions);
   }
   str_free(p->proc_name);
   str_free(p->uarch_name);

   hashtable_free(p->asmfile_table, NULL, NULL);
   str_free(p->file);
   queue_free(p->asmfiles, &asmfile_free);

   lc_free(p);
}

///////////////////////////////// Member getters //////////////////////////////

/*
 * Gets a parameter from a project
 * \param p a project
 * \param module_id a value from params_modules_id_e enum corresponding to a module
 * \param param_id a value from params_<module>_id_e enum corresponding to the option
 * \return the corresponding value or NULL
 */
void* project_get_parameter(project_t* p, int module_id, int param_id)
{
   if (p == NULL
         || module_id >= _NB_PARAM_MODULE || param_id >= _NB_OPT_BY_MODULE)
      return NULL;

   return p->params[module_id][param_id];
}

/*
 * Gets the asmfiles of a project
 * \param p a project
 * \return asmfiles of p
 */
queue_t* project_get_asmfiles(project_t* p)
{
   return (p != NULL) ? p->asmfiles : PTR_ERROR;
}

/*
 * Gets the asmfile table of a project
 * \param p a project
 * \return asmfile table of p
 */
hashtable_t* project_get_asmfile_table(project_t* p)
{
   return (p != NULL) ? p->asmfile_table : PTR_ERROR;
}

/*
 * Gets the processor version of a project
 * \param project a project
 * \return the processor version associated to the project
 */
proc_t* project_get_proc(project_t* project)
{
   if (project == NULL)
      return PTR_ERROR;
   return project->proc;
}

/*
 * Gets the name of a processor version of a project
 * \param project a project
 * \return the name of the processor version associated to the project
 */
char* project_get_proc_name(project_t* project)
{
   if (project == NULL)
      return PTR_ERROR;
   //Returning the name of the processor version associated to the project if it exists
   if (project->proc != NULL)
      return proc_get_name(project->proc);
   //Otherwise, return the name of the processor version associated to the project
   return project->proc_name;
}

/*
 * Gets the uarch name of a project
 * \param p a project
 * \return Name of the uarch associated to the project
 */
char* project_get_uarch_name(project_t* p)
{
   if (p == NULL)
      return PTR_ERROR;
   //Returning the name of the uarch of the processor version associated to the project if it exists
   if (p->proc != NULL)
      return (uarch_get_name(proc_get_uarch(p->proc)));
   //Otherwise, return the name of the uarch associated to the project
   return p->uarch_name;
}

/*
 * Gets the architecture associated to a project
 * \param project A project
 * \return The architecture associated to the project, or NULL if \c project is NULL
 * */
arch_t* project_get_arch(project_t* project)
{
   /**\note (2016-12-14) This function currently extract the architecture from the processor version,
    * if it was filled. If at some point it is discovered that it is not enough, simply add arch to the
    * structure project_t and update this function accordingly*/
   return (uarch_get_arch(proc_get_uarch(project_get_proc(project))));
}

/*
 * Retrieves the identifier of the micro architecture for a project
 * \param project A project
 * \return The identifier of the micro architecture associated to the project
 * */
unsigned int project_get_uarch_id(project_t* project)
{
   return (uarch_get_id(proc_get_uarch(project_get_proc(project))));
}

/*
 * Gets the compiler used for a project
 * \param p a project
 * \return compiler code of p
 */
char project_get_compiler_code(project_t* p)
{
   return (p != NULL) ? p->comp_code : SIGNED_ERROR;
}

/*
 * Gets the language used for a project
 * \param p a project
 * \return language code of p
 */
char project_get_language_code(project_t* p)
{
   return (p != NULL) ? p->lang_code : SIGNED_ERROR;
}

/*
 * Gets the CG depth of a project
 * \param p a project
 * \return depth of p
 */
int project_get_CG_depth(project_t* p)
{
   return (p != NULL) ? p->cg_depth : SIGNED_ERROR;
}

/*
 * Gets the name of a project
 * \param p a project
 * \return p name
 */
char* project_get_name(project_t* p)
{
   return (p != NULL) ? p->file : PTR_ERROR;
}

/*
 * Gets the CC mode of a project (used to extract functions from connected components)
 * \param p a project
 * \return CC mode of p (CCMODE_.*)
 */
char project_get_cc_mode(project_t* p)
{
   return (p != NULL) ? p->cc_mode : SIGNED_ERROR;
}

/*
 * Gets exits functions of a project
 * \param p a project
 * \return exit functions
 */
char** project_get_exit_fcts(project_t* p)
{
   return (p != NULL) ? p->exit_functions : PTR_ERROR;
}

//////////////////////////////// Counter getters //////////////////////////////

/*
 * Gets the number of instructions for a project
 * \param p a project
 * \return the number of instructions of p
 */
int project_get_nb_insns(project_t* p)
{
   int nb = 0;

   FOREACH_INQUEUE(project_get_asmfiles(p), it) {
      asmfile_t* asmf = GET_DATA_T(asmfile_t*, it);
      nb += asmfile_get_nb_insns(asmf);
   }

   return nb;
}

/*
 * Gets the number of blocks for a project
 * \param p a project
 * \return the number of blocks of p
 */
int project_get_nb_blocks(project_t* p)
{
   int nb = 0;

   FOREACH_INQUEUE(project_get_asmfiles(p), it) {
      asmfile_t* asmf = GET_DATA_T(asmfile_t*, it);
      nb += asmfile_get_nb_blocks(asmf);
   }

   return nb;
}

/*
 * Gets the number of blocks for a project, excluding virtual blocks
 * \param p a project
 * \return the number of blocks of p
 */
int project_get_nb_blocks_novirtual(project_t* p)
{
   int nb = 0;

   FOREACH_INQUEUE(project_get_asmfiles(p), it) {
      asmfile_t* asmf = GET_DATA_T(asmfile_t*, it);
      nb += asmfile_get_nb_blocks_novirtual(asmf);
   }

   return nb;
}

/*
 * Gets the number of loops for a project
 * \param p a project
 * \return the number of loops of p
 */
int project_get_nb_loops(project_t* p)
{
   int nb = 0;

   FOREACH_INQUEUE(project_get_asmfiles(p), it) {
      asmfile_t* asmf = GET_DATA_T(asmfile_t*, it);
      nb += asmfile_get_nb_loops(asmf);
   }

   return nb;
}

/*
 * Gets the number of functions for a project
 * \param p a project
 * \return the number of functions of p
 */
int project_get_nb_fcts(project_t* p)
{
   int nb = 0;

   FOREACH_INQUEUE(project_get_asmfiles(p), it) {
      asmfile_t* asmf = GET_DATA_T(asmfile_t*, it);
      nb += asmfile_get_nb_fcts(asmf);
   }

   return nb;
}

/*
 * Gets the number of asmfiles for a project
 * \param p a project
 * \return the number of asmfiles of p
 */
int project_get_nb_asmfiles(project_t* p)
{
   return queue_length(project_get_asmfiles(p));
}

///////////////////////////////// Member setters //////////////////////////////

/*
 * Sets a parameter in a project
 * \param p a project
 * \param module_id a value from params_modules_id_e enum corresponding to a module
 * \param param_id a value from params_<module>_id_e enum corresponding to the option
 * \param value the value of the parameter
 */
void project_add_parameter(project_t* p, int module_id, int param_id,
      void* value)
{
   if (p == NULL
         || module_id >= _NB_PARAM_MODULE || param_id >= _NB_OPT_BY_MODULE)
      return;

   p->params[module_id][param_id] = value;
}

/*
 * Sets the list of exit functions
 * \param p a project
 * \param exits a NULL terminated array containing names of exit functions.
 * The array and the names must have been allocated through lc_malloc and will be freed at the destruction of the project
 * The function names must not contain any extension indicating their potential dynamic origin (e.g. @plt)
 */
void project_set_exit_fcts(project_t* p, char** exits)
{
   if (p == NULL)
      return;
   // Frees existing array if already present
   if (p->exit_functions != NULL) {
      int i = 0;
      while (p->exit_functions[i] != NULL) {
         lc_free(p->exit_functions[i]);
         i++;
      }
      lc_free(p->exit_functions);
   }
   p->exit_functions = exits;
}

/*
 * Appends a list of exit functions to the existing list
 * \param p a project
 * \param exits a NULL terminated array containing names of exit functions
 * The array and the names must have been allocated through lc_malloc and will be freed at the destruction of the project
 * The function names must not contain any extension indicating their potential dynamic origin (e.g. @plt)
 */
void project_add_exit_fcts(project_t* p, char** exits)
{
   if (p == NULL || exits == NULL)
      return;
   // Easy case: no existing list of exit functions
   if (p->exit_functions == NULL) {
      p->exit_functions = exits;
      return;
   }
   // Computes size of existing array
   int n_exit_functions = 0;
   while (p->exit_functions[n_exit_functions] != NULL)
      n_exit_functions++;
   // Computes size of additional array
   int n_exits = 0;
   while (exits[n_exits] != NULL)
      n_exits++;

   p->exit_functions = lc_realloc(p->exit_functions, n_exit_functions + n_exits + 1);
   int i;
   for (i = n_exit_functions; i < (n_exit_functions + n_exits); i++)
      p->exit_functions[i] = exits[i - n_exits];
   p->exit_functions[i] = NULL;
   lc_free(exits);
}

/*
 * Removes a function from the list of exit functions
 * \param p a project
 * \param exit a function name to be removed from the list.
 * */
void project_rem_exit_fct(project_t* p, char* exit)
{
   if (p == NULL || exit == NULL || p->exit_functions == NULL)
      return;
   int i = 0;
   while (p->exit_functions[i] != NULL) {
      if (strcmp(p->exit_functions[i], exit) == 0) {
         //Found the entry to remove
         //Freeing the removed entry
         lc_free(p->exit_functions[i]);
         //Replacing all entries with the one next to it
         while (p->exit_functions[i] != NULL) {
            p->exit_functions[i] = p->exit_functions[i+1];
            i++;
         }
         //Resizing the array
         assert(i > 0);
         if (i == 1) {
            lc_free(p->exit_functions);   //Freeing the array if it has been emptied
            p->exit_functions = NULL;
         } else
            p->exit_functions = lc_realloc(p->exit_functions, i);
         break; //Exiting the main loop
         /**\todo (2017-11-24) Exiting now does not handle the fact of a duplicated entry in the array*/
      }
      i++;
   }
}

/*
 * Sets the value of cc_mode member (supported values are CCMODE_.*)
 * \param p a project
 * \param cc_mode value for cc_mode member
 */
void project_set_ccmode(project_t* p, char cc_mode)
{
   if (p == NULL)
      return;
   p->cc_mode = cc_mode;
}

/*
 * Sets the processor version of a project
 * \param project a project
 * \param proc a processor version
 */
void project_set_proc(project_t* project, proc_t* proc)
{
   if (project == NULL)
      return;
   project->proc = proc;
}

/*
 * Sets the name of a processor version of a project
 * \param project a project
 * \param proc_name the name of the processor version
 */
void project_set_proc_name(project_t* project, char* proc_name)
{
   if (project == NULL)
      return;
   project->proc_name = lc_strdup(proc_name);
}

/*
 * Sets the uarch name of a project
 * \param p a project
 * \param uarch_name The name of a uarch
 */
void project_set_uarch_name(project_t* p, char* uarch_name)
{
   if (p == NULL)
      return;
   p->uarch_name = lc_strdup(uarch_name);
}

/*
 * Sets the compiler used for a project
 * \param p a project
 * \param comp_code a compiler code
 */
void project_set_compiler_code(project_t* p, char comp_code)
{
   if (p == NULL)
      return;
   p->comp_code = comp_code;
}

/*
 * Sets the language used for a project
 * \param p a project
 * \param lang_code a language code
 */
void project_set_language_code(project_t* p, char lang_code)
{
   if (p == NULL)
      return;
   p->lang_code = lang_code;
}

/*
 * Sets the CG depth of a project
 * \param p a project
 * \param cg_depth CG depth
 */
void project_set_CG_depth(project_t* p, int cg_depth)
{
   if (p == NULL)
      return;
   p->cg_depth = cg_depth;
}

/*
 * Sets the name of a project
 * \param p a project
 * \param name a name
 */
void project_set_name(project_t* p, char* name)
{
   if (p == NULL)
      return;
   p->file = name;
}

///////////////////////////////// Other functions /////////////////////////////

/*
 * Adds a file into an existing project
 * \param p an existing project
 * \param filename name of an assembly file to add
 * \return an asmfile structure associated to the given assembly file
 */
asmfile_t* project_add_file(project_t* p, char *filename)
{
   if (p == NULL)
      return NULL;

   asmfile_t *asmfile = hashtable_lookup(p->asmfile_table, filename);
   if (asmfile != NULL)
      return asmfile;

   asmfile = asmfile_new(filename);
   asmfile->project = p;
   asmfile->lang_code = p->lang_code;
   asmfile->comp_code = p->comp_code;
   asmfile->proc = p->proc;

   queue_add_tail(p->asmfiles, asmfile);
   hashtable_insert(p->asmfile_table, filename, asmfile);

   memcpy(asmfile->params, p->params, sizeof(p->params)); // sizeof returns expected size for arrays
   // whose size is known at compile time

   return asmfile;
}

/*
 * Removes a file into an existing project
 * \param p an existing project
 * \param asmfile an asmfile to remove from a project
 * \return 1 or 0 if there is a problem
 */
int project_remove_file(project_t* p, asmfile_t* asmfile)
{
   if (p == NULL || asmfile == NULL)
      return 0;

   queue_remove(p->asmfiles, asmfile, NULL);
   hashtable_remove(p->asmfile_table, asmfile->name);

   //finally free the asmfile
   asmfile_free(asmfile);

   return 1;
}
