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
#include "libmdbg.h"

///////////////////////////////////////////////////////////////////////////////
//                                  functions                                //
///////////////////////////////////////////////////////////////////////////////
/*
 * Create a new empty function
 * \param asmf assembly file containing the function
 * \param label Label representing the name of the function (must be not null)
 * \param insn first instruction of the function
 * \return a new function, or PTR_ERROR if \e name is NULL
 */
fct_t* fct_new(asmfile_t* asmf, label_t* label, insn_t* insn)
{
   if (label == NULL || insn == NULL || asmf == NULL)
      return NULL;

   fct_t *ft = block_get_fct(insn->block);
   if (ft != NULL)
      return ft;

   assert(asmf->functions != NULL);

   ft = hashtable_lookup(asmf->ht_functions, label->name);
   if (ft != NULL)
      return ft;

   fct_t* new = lc_malloc0(sizeof(*new));
   new->namelbl = label;
   new->id = queue_length(asmf->functions); //uniq_id;
   new->global_id = asmf->maxid_fct++;
   new->asmfile = asmf;
   new->blocks = queue_new();
   new->loops = queue_new();
   new->cg_node = graph_node_new(new);
   new->first_insn = insn;
   new->entries = queue_new();
   new->exits = queue_new();
   new->ranges = queue_new();
   new->dbg_addr = -1;
   new->padding_blocks = queue_new();
   new->is_grouping_analyzed = FALSE;

   /**\todo this is done to remove plt functions from maqao data. Find something to free them*/
   hashtable_insert(asmf->ht_functions, label->name, new);
   if (label_get_type(label) != LBL_EXTFUNCTION)
      queue_add_tail(asmf->functions, new);
   else
      asmf->plt_fct = list_add_before(asmf->plt_fct, new);

   char* fct_name = lc_strdup(label->name);
   if (strstr(fct_name, EXT_LBL_SUF))
      fct_name[strlen(fct_name) - strlen(EXT_LBL_SUF)] = '\0';
   new->demname = fct_demangle(fct_name, asmf->comp_code, asmf->lang_code);
   lc_free(fct_name);

   if (asmf->load_fct_dbg != NULL)
      asmf->load_fct_dbg(new);

   return new;
}

static void _fct_free(void* p, boolean_t free_cg_node)
{
   if (p == NULL)
      return;

   fct_t* f = p;

   asmfile_t* asmf = fct_get_asmfile(f);
   if (asmf != NULL) {
      if (asmf->free_ssa != NULL)
         asmf->free_ssa(f);
      if (asmf->free_polytopes != NULL)
         asmf->free_polytopes(f);
      if (asmf->free_live_registers != NULL)
         asmf->free_live_registers(f);
   }

   queue_free(f->blocks, &block_free);
   if (free_cg_node == TRUE) graph_node_free(f->cg_node, NULL, NULL);
   queue_free(f->loops, &loop_free);
   queue_free(f->entries, NULL);
   queue_free(f->exits, NULL);
   queue_free(f->ranges, &fct_range_free);
   queue_free(f->padding_blocks, NULL);

   FOREACH_INQUEUE(f->components, it) {
      queue_t* l = GET_DATA_T(queue_t*, it);
      queue_free(l, NULL);
   }
   queue_free(f->components, NULL);

   lc_free(f->demname);
   lc_free(f);
}

/*
 * Delete an existing functions and all data it contains
 * \param p a pointer on a function to free
 */
void fct_free(void* p) { _fct_free(p, TRUE); }

/*
 * Delete an existing functions and all data it contains except cg_node
 * Allow dramatical improvement of asmfile_free (all CG can be destroyed at once)
 * \param p a pointer on a function to free
 */
void fct_free_except_cg_node(void* p) { _fct_free(p, FALSE); }

/*
 * Retrieves the connected components
 * \param f a function
 * \return a queue of connected components (lists of blocks) or NULL if f is NULL
 */
queue_t* fct_get_components(fct_t *f)
{
   return (f != NULL) ? f->components : NULL;
}

/*
 * Retrieves the original function (if CC of a real function)
 * \param f a function
 * \return a function or NULL if f is NULL
 */
fct_t* fct_get_original_function(fct_t* f)
{
   return (f != NULL) ? f->original_function : PTR_ERROR;
}

/*
 * Retrieves a uniq ID associated to a function
 * \param f a function
 * \return a uniq ID or 0 if f is NULL
 */
unsigned int fct_get_id(fct_t* f)
{
   return (f != NULL) ? f->global_id : 0;
}

/*
 * Retrieves a function name
 * \param f a function
 * \return f name or PTR_ERROR if f is NULL
 */
char* fct_get_name(fct_t* f)
{
   return (f != NULL) ? label_get_name(f->namelbl) : PTR_ERROR;
}

/*
 * Retrieves the label representing a function name
 * \param f a function
 * \return The label representing the function name or PTR_ERROR if f is NULL
 */
label_t* fct_get_lblname(fct_t* f)
{
   return (f != NULL) ? f->namelbl : PTR_ERROR;
}

/*
 * Retrieves a list of blocks which belongs to f
 * \param f a function
 * \return f a list of block NULL if f is NULL
 */
queue_t* fct_get_blocks(fct_t* f)
{
   return (f != NULL) ? f->blocks : PTR_ERROR;
}

/*
 * Retrieves a list of padding blocks which belongs to f
 * \param f a function
 * \return f a list of padding block NULL if f is NULL
 */
queue_t* fct_get_padding_blocks(fct_t* f)
{
   return (f != NULL) ? f->padding_blocks : PTR_ERROR;
}

/*
 * Retrieves paths of f
 * \param f a function
 * \return a queue of paths or PTR_ERROR if f is NULL
 */
queue_t* fct_get_paths(fct_t* f)
{
   return (f != NULL) ? f->paths : PTR_ERROR;
}

/*
 * Retrieves a CG node associated to a function
 * \param f a function
 * \return f CG node or PTR_ERROR if f is NULL
 */
graph_node_t* fct_get_CG_node(fct_t* f)
{
   return (f != NULL) ? f->cg_node : PTR_ERROR;
}

/*
 * Retrieves a list of loops which belongs to f
 * \param f a function
 * \return f a list of loops NULL if f is NULL
 */
queue_t* fct_get_loops(fct_t* f)
{
   return (f != NULL) ? f->loops : PTR_ERROR;
}

/*
 * Retrieves an asmfile which contains f
 * \param f a function
 * \return f an asmfile which contains f or PTR_ERROR if f is NULL
 */
asmfile_t* fct_get_asmfile(fct_t* f)
{
   return (f != NULL) ? f->asmfile : PTR_ERROR;
}

/*
 * Retrieves a project which contains f
 * \param f a function
 * \return f a project which contains f or PTR_ERROR if f is NULL
 */
project_t* fct_get_project(fct_t* f)
{
   asmfile_t *asmfile = fct_get_asmfile(f);
   return asmfile_get_project(asmfile);
}

/*
 * Iterate over the first head of each connected component of a function
 * \param f a function
 * return list_t* list of the first header of each component
 */
list_t *fct_get_CC_heads(fct_t *f)
{
   list_t *headers = NULL;

   FOREACH_INQUEUE(fct_get_components(f), it) {
      list_t *list = GET_DATA_T(list_t*, it);
      block_t *block = GET_DATA_T(block_t*, list);
      headers = list_add_before(headers, block);
   }
   return headers;
}

/*
 * Gets the number of loops for a function
 * \param f a function
 * \return the number of loops of f
 */
int fct_get_nb_loops(fct_t* f)
{
   return queue_length(fct_get_loops(f));
}

/*
 * Gets the number of blocks for a function
 * \param f a function
 * \return the number of blocks of f
 */
int fct_get_nb_blocks(fct_t* f)
{
   return queue_length(fct_get_blocks(f));
}

/*
 * Gets the number of blocks for a function, excluding virtual blocks
 * \param f a function
 * \return the number of blocks of f
 */
int fct_get_nb_blocks_novirtual(fct_t* f)
{
   int nb = 0;

   FOREACH_INQUEUE(fct_get_blocks(f), it) {
      block_t* block = GET_DATA_T(block_t*, it);
      if (!block_is_virtual(block))
         nb++;
   }

   return nb;
}

/*
 * Gets the number of instructions for a function
 * \param f a function
 * \return the number of instructions of f
 */
int fct_get_nb_insns(fct_t* f)
{
   if (f == NULL)
      return 0;

   if (f->nb_insns == 0) {
      FOREACH_INQUEUE(f->blocks, it)
      {
         block_t* block = GET_DATA_T(block_t*, it);
         f->nb_insns += block_get_size(block);
      }
   }

   return f->nb_insns;
}

/*
 * Retrieves the first instruction of f
 * \param f a function
 * \return the first instruction of f or PTR_ERROR if f is NULL
 */
insn_t *fct_get_first_insn(fct_t* f)
{
   return (f != NULL) ? f->first_insn : PTR_ERROR;
}

/*
 * Retrieves a demangled function name
 * \param f a function
 * \return f demangled name
 */
char* fct_get_demname(fct_t* f)
{
   if (f == NULL)
      return NULL;

   if (f->demname != NULL)
      return (f->demname);

   if (f->debug != NULL)
      return (f->debug->name);

   return NULL;
}

/**
 * Retrieves the debug data of a function
 * @param f The function
 * @return Its debug data or NULL if the function is NULL or has no debug data
 */
static dbg_fct_t* fct_getdebug(fct_t* f)
{
   return (f != NULL) ? f->debug : NULL;
}

/*
 * Checks whether a function has debug data
 * \param f a function
 * \return TRUE or FALSE
 */
int fct_has_debug_data(fct_t* f)
{
   return (fct_getdebug(f) != NULL) ? TRUE : FALSE;
}

/*
 * Return the source file name of the function
 * \param f a function
 * \return the source file name or NULL if not available
 */
char* fct_get_src_file(fct_t* f)
{
   dbg_fct_t* debug = fct_getdebug(f);
   return (debug != NULL) ? debug->file : NULL;
}

/*
 * Return the full path (directory + name) to the source file of the function
 * \param f a function
 * \return the source file path or NULL if directory or name not available
 */
char* fct_get_src_file_path(fct_t* f)
{
   char* dir = fct_getdir(f);
   if (dir == NULL)
      return NULL;

   char* name = fct_get_src_file(f);
   if (name == NULL)
      return NULL;

#ifdef _WIN32
   char sep = '\\';
#else
   char sep = '/';
#endif

   // allocates and fills a string from concatenation of dir + sep + name
   const size_t dir_len = strlen(dir);
   const size_t nam_len = strlen(name);
   char *new = malloc(dir_len + nam_len + 2);
   strcpy(new, dir);
   new[dir_len] = sep;
   strcpy(new + dir_len + 1, name);

   return new;
}

/*
 * Provides first and last source line of a function
 * \see fct_get_src_file_path
 * \param fct a function
 * \param min pointer to first line rank
 * \param max pointer to last line rank
 */
void fct_get_src_lines (fct_t *fct, unsigned *min, unsigned *max)
{
   *min = 0;
   *max = 0;

   char *fct_file_path = fct_get_src_file_path (fct);
   if (fct_file_path == NULL) return;

   FOREACH_INQUEUE(fct_get_blocks(fct), itb) {
      block_t* block = GET_DATA_T(block_t*, itb);

      FOREACH_INSN_INBLOCK(block, iti) {
         insn_t *insn = GET_DATA_T(insn_t*, iti);

         char *file_path = insn_get_src_file (insn);
         if (file_path == NULL || strcmp (file_path, fct_file_path) != 0) continue;

         unsigned src_line = insn_get_src_line (insn);
         if (src_line == 0) continue;

         if (*min == 0 || *min > src_line) *min = src_line;
         if (*max == 0 || *max < src_line) *max = src_line;
      }
   }
}

/*
 * Returns source regions for a function
 * \see blocks_get_src_regions
 */
queue_t *fct_get_src_regions (fct_t *fct)
{
   return blocks_get_src_regions (fct_get_blocks (fct));
}

/*
 * Return the compiler used to compile the function
 * \param f a function
 * \return the compiler name or NULL if not available
 */
char* fct_get_compiler(fct_t* f)
{
   dbg_fct_t* debug = fct_getdebug(f);
   return (debug != NULL) ? debug->compiler : NULL;
}

/*
 * Return the compiler version used to compile the function
 * \param f a function
 * \return the compiler version or NULL if not available
 */
char* fct_get_version(fct_t* f)
{
   dbg_fct_t* debug = fct_getdebug(f);
   return (debug != NULL) ? debug->version : NULL;
}

/*
 * Return the source line where the function was declared
 * \param f a function
 * \return 0 if error, else the line where the function was declared
 */
int fct_get_decl_line(fct_t* f)
{
   dbg_fct_t* debug = fct_getdebug(f);
   return (debug != NULL) ? debug->decl_line : 0;
}

/*
 * Return the source language of the function
 * \param f a function
 * \return the source language or NULL if not available
 */
char* fct_get_language(fct_t* f)
{
   dbg_fct_t* debug = fct_getdebug(f);
   return (debug != NULL) ? debug->language : NULL;
}

/*
 * Return the source language code
 * \param f a function
 * \return a DEM_LANG_ code
 */
char fct_get_language_code(fct_t* f)
{
   dbg_fct_t* debug = fct_getdebug(f);
   return (debug != NULL) ? debug->lang_code : LANG_ERR;
}

/*
 * Updates all blocks local identifiers
 * \param f a function
 */
void fct_upd_blocks_id(fct_t* f)
{
   int id = 0;
   FOREACH_INQUEUE(fct_get_blocks(f), it)
   {
      block_t* b = GET_DATA_T(block_t*, it);
      b->id = id++;
   }
}

/*
 * Updates all loops local identifiers
 * \param f a function
 */
void fct_upd_loops_id(fct_t* f)
{
   int id = 0;
   FOREACH_INQUEUE(fct_get_loops(f), it)
   {
      loop_t* l = GET_DATA_T(loop_t*, it);
      l->id = id++;
   }
}

/*
 * Returns the list of entry blocks for a function
 * \param f a function
 * \return the list of entry blocks or NULL
 */
queue_t* fct_get_entry_blocks(fct_t* f)
{
   return (f != NULL) ? f->entries : NULL;
}

/*
 * Returns the list of exit blocks for a function
 * \param f a function
 * \return the list of exit blocks or NULL
 */
queue_t* fct_get_exit_blocks(fct_t* f)
{
   return (f != NULL) ? f->exits : NULL;
}

/*
 * Returns the list of entry instructions for a function. The return queue
 * should be free using queue_free (<returned queue>, NULL)
 * \param f a function
 * \return the list of entry instructions or NULL
 */
queue_t* fct_get_entry_insns(fct_t* f)
{
   if (f == NULL)
      return NULL;

   queue_t* ret = queue_new();
   FOREACH_INQUEUE(f->entries, it) {
      block_t* b = GET_DATA_T(block_t*, it);
      queue_add_tail(ret, block_get_first_insn(b));
   }
   return ret;
}

/*
 * Returns the list of exit instructions for a function. The return queue
 * should be free using queue_free (<returned queue>, NULL)
 * \param f a function
 * \return the list of exit instructions or NULL
 */
queue_t* fct_get_exit_insns(fct_t* f)
{
   if (f == NULL)
      return NULL;

   queue_t* ret = queue_new();
   FOREACH_INQUEUE(f->exits, it) {
      block_t* b = GET_DATA_T(block_t*, it);
      queue_add_tail(ret, block_get_last_insn(b));
   }
   return ret;
}

/*
 * Returns the function main entry (block containing the instruction which is at
 * the function label)
 * \param f a function
 * \return a basic block wich is the main entry or NULL if there is a problem
 */
block_t* fct_get_main_entry(fct_t* f)
{
   return queue_peek_head(fct_get_entry_blocks(f));
}

/*
 * Checks if a function is an external stub
 * \param f a function
 * \return TRUE if the label containing the function name is of type LBL_EXTFUNCTION, FALSE otherwise
 * */
int fct_is_external_stub(fct_t* f)
{
   label_t *namelbl = fct_get_lblname(f);
   return (namelbl != NULL) ? (namelbl->type == LBL_EXTFUNCTION) : FALSE;
}

/*
 * Creates a new range from limits
 * \param start first instruction of the range
 * \param stop last instruction of the range
 * \return an initialized range
 */
fct_range_t* fct_range_new(insn_t* start, insn_t* stop)
{
   fct_range_t* range = lc_malloc(sizeof(*range));
   range->start = start;
   range->stop = stop;
   range->type = RANGE_ORIGINAL;

   return range;
}

/*
 * Frees an existing range
 * \param p a pointer on a fct_range_t structure
 */
void fct_range_free(void* p)
{
   lc_free(p);
}

/*
 * Returns the list of ranges for a function
 * \param f a function
 * \return the list of ranges or NULL
 */
queue_t* fct_getranges(fct_t *f)
{
   return (f != NULL) ? f->ranges : NULL;
}

/*
 * Returns the first instruction of a range
 * \param range a fct_range_t structure
 * \return the first instruction of the range, or NULL
 */
insn_t* fct_range_getstart(fct_range_t* range)
{
   return (range != NULL) ? range->start : NULL;
}

/*
 * Returns the last instruction of a range
 * \param range a fct_range_t structure
 * \return the last instruction of the range, or NULL
 */
insn_t* fct_range_getstop(fct_range_t* range)
{
   return (range != NULL) ? range->stop : NULL;
}
