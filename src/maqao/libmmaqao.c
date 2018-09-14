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

#include "libmmaqao.h"
#include "libmdisass.h"
#include "assembler.h"
#include "libmasm.h"
#include "libmcore.h"
#include "libmdbg.h"
#include "uarch_detector.h"
#include "archinterface.h"

#include <time.h>
#include <inttypes.h>

//#define _MAQAO_TIMER_

static void f_node_before(graph_node_t * node, void * pflags)
{
   char* flags = (char*) pflags;
   block_t* b = ((block_t*) node->data);

   if (b != NULL)
      flags[b->id] = 1;
}

/**
 * Analyses an asmfile loaded into a project
 * \param project An existing project
 * \param asmfile An asmfile containing a disassembled file (it DIS_ANALYZE flag must be set)
 * \param uarch_name The name of the micro-architecture for which to analyze the file
 * */
static void analyze_disassembled_file(project_t* project, asmfile_t* asmfile)
{
   assert(project && asmfile);

#ifdef _MAQAO_TIMER_
   clock_t t1, t2;
   t1 = clock();
#endif
   asmfile->unload_dbg = &asmfile_unload_dbg;
   DBGMSG0("debug data loading ...\n");
   asmfile->load_fct_dbg = &asmfile_load_fct_dbg;
   asmfile_load_dbg(asmfile);
#ifdef _MAQAO_TIMER_
   t2 = clock();
   printf ("debug data loading ...[%.2f s]\n", (float) (t2-t1) / CLOCKS_PER_SEC);
   t1 = clock();
#endif
   DBGMSG0("flow analysing ...\n");
   lcore_analyze_flow(asmfile);
#ifdef _MAQAO_TIMER_
   t2 = clock();
   printf ("flow analysing ...[%.2f s]\n", (float) (t2-t1) / CLOCKS_PER_SEC);
   t1 = clock();
#endif
   DBGMSG0("loop analysing ...\n");
   lcore_analyze_loops(asmfile);
#ifdef _MAQAO_TIMER_
   t2 = clock();
   printf ("loop analysing ...[%.2f s]\n", (float) (t2-t1) / CLOCKS_PER_SEC);
   t1 = clock();
#endif
   DBGMSG0("connected components analysing ...\n");
   lcore_analyze_connected_components(asmfile);
#ifdef _MAQAO_TIMER_
   t2 = clock();
   printf ("connected components analysing ...[%.2f s]\n", (float) (t2-t1) / CLOCKS_PER_SEC);
#endif
   if (project->cc_mode != CCMODE_OFF) {
#ifdef _MAQAO_TIMER_
      t1 = clock();
#endif
      DBGMSG0("extract functions from connected components ...\n");
      lcore_asmfile_extract_functions_from_cc(asmfile);
#ifdef _MAQAO_TIMER_
      t2 = clock();
      printf ("extract functions from connected components ...[%.2f s]\n", (float) (t2-t1) / CLOCKS_PER_SEC);
#endif
   }
#ifdef _MAQAO_TIMER_
   t1 = clock();
#endif
   // At this point, data structures (functions, loops) are not modified, so
   // ids for loops and blocks can be updated
   DBGMSG0("update ids ...\n");
   FOREACH_INQUEUE(asmfile->functions, it__f) {
      fct_t* f = GET_DATA_T(fct_t*, it__f);
      //*
      //if (queue_length(f->components) > 1)
      {
         //If needed, add a virtual node at the beginning of the function
         //it will be removed at the end of the analysis
         block_t* virtual = lc_malloc(sizeof(block_t));
         virtual->id = 0;
         virtual->global_id = f->asmfile->n_blocks;
         f->asmfile->n_blocks += 1;
         virtual->begin_sequence = NULL;
         virtual->end_sequence = NULL;
         virtual->function = f;
         virtual->loop = NULL;
         virtual->cfg_node = graph_node_new(virtual);
         virtual->domination_node = tree_new(virtual);
         virtual->postdom_node = NULL;
         virtual->is_loop_exit = 0;
         virtual->is_padding = 0;

         // First step: add an edge from the virtual node to all CC entries
         // who don't have any predecessors
         FOREACH_INQUEUE(f->components, it_cc) {
            queue_t* cc = GET_DATA_T(queue_t*, it_cc);
            FOREACH_INQUEUE(cc, it_en)
            {
               block_t* b = GET_DATA_T(block_t*, it_en);
               graph_add_edge(virtual->cfg_node, b->cfg_node, NULL);
               DBGMSG("Add edge from virtual node %d to CC entry %d\n",
                     virtual->global_id, b->global_id)
            }
         }

         // Second step: add an edge from the virtual node to all blocks
         // who don't have any predecessors
         FOREACH_INQUEUE(f->blocks, it_b) {
            block_t* b = GET_DATA_T(block_t*, it_b);
            if (b->cfg_node->in == NULL) {
               graph_add_edge(virtual->cfg_node, b->cfg_node, NULL);
               DBGMSG("Add edge from virtual node %d to block %d\n",
                     virtual->global_id, b->global_id)
            }
         }

         // Third step: check that all blocks are "linked" to the virtual block
         char* flags = lc_malloc0(f->asmfile->n_blocks * sizeof(char));
         graph_node_DFS(virtual->cfg_node, &f_node_before, NULL, NULL, flags);

         FOREACH_INQUEUE(f->blocks, it_b0) {
            block_t* b = GET_DATA_T(block_t*, it_b0);

            // If the test is true, this means the block has not been traversed.
            // link it to the virtual node.
            if (flags[b->id] == 0 && block_is_padding(b) != TRUE) {
               graph_node_DFS(b->cfg_node, &f_node_before, NULL, NULL, flags);
               graph_add_edge(virtual->cfg_node, b->cfg_node, NULL);
            }
         }
         lc_free(flags);
         queue_add_head(f->blocks, virtual);
      }             //*/
      fct_upd_loops_id(f);
      fct_upd_blocks_id(f);
   }
#ifdef _MAQAO_TIMER_
   t2 = clock();
   printf ("update ids ...[%.2f s]\n", (float) (t2-t1) / CLOCKS_PER_SEC);
   t1 = clock();
#endif
   DBGMSG0("end of functions analysing ...\n");
   asmfile_detect_end_of_functions(asmfile);
#ifdef _MAQAO_TIMER_
   t2 = clock();
   printf ("detect end of functions ...[%.2f s]\n", (float) (t2-t1) / CLOCKS_PER_SEC);
   t1 = clock();
#endif
   DBGMSG0("functions ranges analysing ...\n");
   asmfile_detect_ranges(asmfile);
#ifdef _MAQAO_TIMER_
   t2 = clock();
   printf ("functions ranges analysing ...[%.2f s]\n", (float) (t2-t1) / CLOCKS_PER_SEC);
   t1 = clock();
#endif
   DBGMSG0("dominance analysing ...\n");
   lcore_analyze_dominance(asmfile);
#ifdef _MAQAO_TIMER_
   t2 = clock();
   printf ("dominance analysing ...[%.2f s]\n", (float) (t2-t1) / CLOCKS_PER_SEC);
   t1 = clock();
#endif
   asmfile_update_counters(asmfile);
#ifdef _MAQAO_TIMER_
   t2 = clock();
   printf ("update counters [%.2f s]\n", (float) (t2-t1) / CLOCKS_PER_SEC);
#endif

   // End of Testing ======================================
   DBGMSG0("loading done\n");

}

/**
 * Retrieves the architecture from its name or from the code stored in a binary file
 * \param arch_name Name of the architecture
 * \param file_name Path the file
 * \return Structure describing the architecture with the given name, or the architecture of the binary file \c file_name
 * if \c arch_name is NULL, or NULL if it could not be found by either ways
 * */
static arch_t* get_arch(char* arch_name, char* file_name)
{
   arch_t* arch = getarch_byname(arch_name);
   if (arch == NULL)
      arch = file_get_arch(file_name);
   return arch;
}

/**
 * Retrieves the processor version for a given architecture based on its name or that of its micro-architecture, or from the host
 * if both names are NULL
 * \param proc Return parameter. Will be updated containt a pointer to the structure describing the processor version,
 * or to NULL if none could be found
 * \param arch The architecture
 * \param uarch_name The name of the micro-architecture
 * \param proc_name The name of the processor version
 * \return EXIT_SUCCESS or error code if the processor version could not be successfully deduced from the parameters
 * */
static int get_proc(proc_t** proc, arch_t* arch, char* uarch_name, char* proc_name)
{
   assert(proc != NULL);   //Static function, we know what we are doing
   if (uarch_name == NULL && proc_name == NULL) {
      // If neither uarch nor proc is given, look for it using CPUID
      proc_t* proc_host = utils_get_proc_host();
      if (arch == NULL || uarch_get_arch(proc_get_uarch(proc_host)) == arch) {
         //Host has the same architecture as the file
         *proc = proc_host;
         return (*proc != NULL)?EXIT_SUCCESS:ERR_MAQAO_UNABLE_TO_DETECT_PROC_HOST;
      } else {
         //Host does not have the same architecture as the file (cross-analysis)
         *proc = NULL;
         return ERR_MAQAO_MISSING_UARCH_OR_PROC;
         /**\todo (2017-01-13) Decide if this can be set as an error or a warning - we may not need it*/
      }
   } else if (arch != NULL) {
      if (proc_name != NULL) {
         // Trying to identify the processor version from the architecture
         *proc = arch_get_proc_by_name(arch, proc_name);
         return (*proc != NULL)?EXIT_SUCCESS:ERR_LIBASM_PROC_NAME_INVALID;
      } else { //uarch_name != NULL
         *proc = arch_get_uarch_default_proc(arch,
               arch_get_uarch_by_name(arch, uarch_name));
         return (*proc != NULL)?EXIT_SUCCESS:ERR_LIBASM_UARCH_NAME_INVALID;
      }
      assert(FALSE); //Whatever Eclipse says, we should not reach this point
   } else {
      *proc = NULL;
      return ERR_LIBASM_ARCH_MISSING;
   }
}

/*
 * Sets the information relative to a processor version in a project.
 * If neither uarch_name nor proc_name are set, the function will attempt retrieving the processor version from the host
 * If either one is set, the function will attempt retrieving the architecture from the file, and deduce the processor version
 * from the name. If the architecture can not be deduced from the file (or filename is NULL), the values will be stored in the project
 * \param project The project
 * \param filename The name of a file associated to the project
 * \param arch_name The name of the architecture used in the project
 * \param uarch_name The name of the micro-architecture to associate to the project if \c proc_name is NULL
 * \param proc_name The name of the processor version to associate to the project
 * \return EXIT_SUCCESS or error code if the processor version could not be successfully deduced from the parameters
 * */
int project_init_proc(project_t* project, char* file_name, char* arch_name, char* uarch_name, char* proc_name)
{
   /**\todo (2017-01-13) proc_name should come first in the list of arguments, because it will always be used if present,
    * but it is last for coherence with the Lua functions, where it will be last for retrocompatibility
    * To be updated later when it is implemented everywhere*/
   if (project == NULL)
      return ERR_LIBASM_MISSING_PROJECT;
   arch_t* arch = NULL;
   proc_t* proc = NULL;

   //Retrieving the architecture from its name or the file
   arch = get_arch(arch_name, file_name);

   // Tries to retrieve the processor version from its name or the host
   int status = get_proc(&proc, arch, uarch_name, proc_name);

   //If the processor version was found, setting it in the host
   if (status == EXIT_SUCCESS)
      project_set_proc(project, proc);
   else if (status != ERR_LIBASM_ARCH_MISSING) {
      return status;
   } else {
      // arch was not given as parameter and can't be deduced from the binary
      if (proc_name != NULL) {
         // Stores the name of the processor version in the structure for later resolution
         project_set_proc_name (project, proc_name);
      } else if (uarch_name != NULL) {
         // Stores the name of the uarch in the structure for later resolution
         project_set_uarch_name (project, uarch_name);
      }
      /**\todo (2017-01-13) Check whether we would need to return a warning here*/
   }
   return EXIT_SUCCESS;
}

/**
 * Sets the information relative to a processor version in an asmfile.
 * If neither uarch_name nor proc_name are set, the function will attempt retrieving the processor version from the project.
 * If not available, it will attempt to retrieve them from the host.
 * If either one is set, the function will attempt retrieving the architecture from the file, and deduce the processor version
 * from the name. If the architecture can not be deduced from the file (or filename is NULL), an error will be returned
 * \param asmfile The asmfile
 * \param arch_name The name of the architecture used in the project
 * \param uarch_name The name of the micro-architecture  to associate to the project if \c proc_name is NULL
 * \param proc_name The name of the processor version to associate to the project
 * \return EXIT_SUCCESS or error code if the processor version could not be successfully deduced from the parameters
 * */
static int asmfile_init_proc(asmfile_t* asmfile, char* arch_name, char* uarch_name, char* proc_name)
{
   /**\todo (2017-01-13) proc_name should come first in the list of arguments, because it will always be used if present,
    * but it is last for coherence with the Lua functions, where it will be last for retrocompatibility
    * To be updated later when it is implemented everywhere*/
  if (asmfile == NULL)
      return ERR_LIBASM_MISSING_ASMFILE;
   project_t* project = asmfile_get_project(asmfile);
   arch_t* arch = NULL;
   proc_t* proc = NULL;
   int status = EXIT_SUCCESS;

   //Retrieving the architecture from its name or the file
   arch = get_arch(arch_name, asmfile_get_name(asmfile));

   if (uarch_name == NULL && proc_name == NULL) {
      //No micro architecture or processor version names given: try retrieving those from the project if it is the same architecture
      if (arch == project_get_arch(project))
         proc = project_get_proc(project);
      //If the project does not have a processor version try retrieving one from its micro architecture or processor version names or the host
      if (proc == NULL) {
         status = get_proc(&proc, arch, project_get_uarch_name(project), project_get_proc_name(project));
      }
   } else {
      //Retrieving the micro architecture or processor version from the given name
      status = get_proc(&proc, arch, uarch_name, proc_name);
   }
   asmfile_set_proc(asmfile, proc);
   return status;
}

/*
 * Adds and analyzes an asmfile into a project
 * \param project an existing project
 * \param filename name of the assembly file to load and analyze
 * \param uarch_name Name of the micro-architecture against which the project must be analysed
 * \return a new asmfile structure
 */
asmfile_t* project_load_file(project_t *project, char *filename, char *uarch_name)
{
#ifdef _MAQAO_TIMER_
   clock_t t1, t2;
#endif
   asmfile_t *asmfile;
   int res = EXIT_SUCCESS;
   DBGMSG("project %s\n", project->file);
   /* TODO: check for read permissions */
   if (!filename || !fileExist(filename)) {
      /* Important info: use MESSAGE instead of DBGMSG */
      STDMSG("Cannot open binary file (invalid name or not found).\n");
      return NULL;
   }

   DBGMSG("loading asm %s in project %p ...\n", filename, project);
#ifdef _MAQAO_TIMER_
   t1 = clock();
#endif

   asmfile = hashtable_lookup(project->asmfile_table, filename);
   if (asmfile != NULL)
      return asmfile;

   asmfile = project_add_file(project, filename);
   res = asmfile_init_proc(asmfile, NULL, uarch_name, NULL);
   if (ISERROR(res))
      asmfile_set_last_error_code(asmfile, res);
   /**\todo (2017-01-13) Decide whether we exit with an error here or go on (it was the previous behaviour, there was no test on the uarch_name)*/

#ifdef _MAQAO_TIMER_
   t2 = clock();
   printf("loading asm %s in project %p ...[%.2f s]\n",filename, project, (float) (t2-t1) / CLOCKS_PER_SEC);
   DBGMSG0("disassembling ...\n");
   t1 = clock();
#endif
   asmfile_add_parameter(asmfile, PARAM_MODULE_DISASS, PARAM_DISASS_OPTIONS,
         (void*) DISASS_OPTIONS_FULLDISASS);
   res = asmfile_disassemble(asmfile);
   if (ISERROR(res)) {  // The binary has not been analyzed => error.
      project_remove_file(project, asmfile);
      return (NULL);
   }

#ifdef _MAQAO_TIMER_
   t2 = clock();
   printf ("disassembling ...[%.2f s]\n", (float) (t2-t1) / CLOCKS_PER_SEC);
#endif

   //Performs all analyses on the file
   analyze_disassembled_file(project, asmfile);

   return asmfile;
}

/*
 * Adds and analyzes an asmfile into a project
 * \param project an existing project
 * \param filename name of the assembly file to load and analyze
 * \param archname Name of the architecture
 * \param uarch_name Name of the micto architecture
 * \return a new asmfile structure
 */
asmfile_t* project_load_asm_file(project_t *project, char *filename,
      char* archname, char *uarch_name)
{
#ifdef _MAQAO_TIMER_
   clock_t t1, t2;
#endif
   asmfile_t *asmfile;
   int res = EXIT_SUCCESS;
   DBGMSG("project %s\n", project->file);
   /* TODO: check for read permissions */
   if (!filename || !fileExist(filename)) {
      /* Important info: use MESSAGE instead of DBGMSG */
      ERRMSG("Cannot open text file %s (invalid name or not found).\n",
            filename);
      return NULL;
   }
   if (!archname) {
      ERRMSG("Missing architecture name for reading file %s.\n", filename);
      return NULL;
   }

   DBGMSG("loading asm %s in project %p ...\n", filename, project);
#ifdef _MAQAO_TIMER_
   t1 = clock();
#endif

   asmfile = project_add_file(project, filename);
   res = asmfile_init_proc(asmfile, archname, uarch_name, NULL);
   if (ISERROR(res))
      asmfile_set_last_error_code(asmfile, res);
   /**\todo (2017-01-13) Decide whether we exit with an error here or go on (it was the previous behaviour, there was no test on the uarch_name)*/

#ifdef _MAQAO_TIMER_
   t2 = clock();
   printf("loading asm %s in project %p ...[%.2f s]\n",filename, project, (float) (t2-t1) / CLOCKS_PER_SEC);
   DBGMSG0("disassembling ...\n");
   t1 = clock();
#endif
   int bytelen = 0;
   unsigned char* bytes = assemble_asm_file(asmfile, archname, &bytelen);

   if (!bytes) {
      //Unable to assemble the file => error
      project_remove_file(project, asmfile);
      return (NULL);
   }
   //Adds a label at the beginning of the file
   label_t* mainlbl = label_new("main", 0, TARGET_INSN, NULL);
   label_set_type(mainlbl, LBL_FUNCTION);
   asmfile_add_label_unsorted(asmfile, mainlbl);
   asmfile_upd_labels(asmfile);

   //Deleting the instructions from the file
   if (asmfile_get_arch(asmfile)) {
      queue_flush(asmfile_get_insns(asmfile),
            asmfile_get_arch(asmfile)->insn_free);
   }

   res = stream_disassemble(asmfile, bytes, bytelen, 0, NULL, archname);
   if (ISERROR(res)) {  // The binary has not been analyzed => error.
      project_remove_file(project, asmfile);
      return (NULL);
   }
   lc_free(bytes);

#ifdef _MAQAO_TIMER_
   t2 = clock();
   printf ("disassembling ...[%.2f s]\n", (float) (t2-t1) / CLOCKS_PER_SEC);
#endif

   //Performs all analyses on the file
   analyze_disassembled_file(project, asmfile);

   return asmfile;
}

/**The following functions ensure that loop and block ids in a file loaded from a formatted assembly file using project_load_txtfile are
 * updated to be those from the file**/
/**\todo (2016-04-01) The following functions and structures should be moved to a different file, but I have no idea which*/
/**
 * Structure storing the characteristics of a block from the text file
 * */
typedef struct txtblock_s {
   uint32_t bid;
   int64_t addr;
} txtblock_t;

/**
 * Structure storing the characteristics of a loop from the text file
 * */
typedef struct txtloop_s {
   uint32_t lid; /**<Loop identifier*/
   txtblock_t** blocks; /**<Array of blocks in the loop*/
   uint32_t n_blocks; /**<Number of blocks in the loop*/
} txtloop_t;

/**
 * Creates a new structure representing a block from the text file
 * @param bid Block identifier
 * @param addr Address of the first instruction of the block
 * @return new structure representing a block
 */
static txtblock_t* txtblock_new(uint32_t bid, int64_t addr)
{
   txtblock_t* txtblock = lc_malloc(sizeof(*txtblock));
   txtblock->bid = bid;
   txtblock->addr = addr;
   return txtblock;
}

/**
 * Frees a structure representing a block from the text file
 * @param txtblock The structure to free
 */
static void txtblock_free(txtblock_t* txtblock)
{
   assert(txtblock);
   lc_free(txtblock);
}

/**
 * Function comparing two structures representing blocks from the text file based on the addresses of their first instruction
 * \param tb1 First block
 * \param tb2 Second block
 * \return 1 if the address of tb1 is higher than the address of tb2, 0 if they are equal and -1 otherwise
 * */
static int txtblock_cmp_qsort(const void* tb1, const void* tb2)
{
   assert(tb1 && tb2);
   txtblock_t* txtblock1 = *((txtblock_t**) tb1);
   txtblock_t* txtblock2 = *((txtblock_t**) tb2);

   if (txtblock1->addr > txtblock2->addr)
      return 1;
   else if (txtblock1->addr < txtblock2->addr)
      return -1;
   else
      return 0;
}

/**
 * Comparison function used for a binary search of structures representing blocks in the text file based on the addresses of their first instruction
 * @param a Pointer to an address
 * @param b Pointer to a structure representing a block in the text file
 * @return -1 if a is lower than the address is lower than the address of the block, 0 if both are equal, -1 otherwise
 */
static int txtblock_cmpaddr_bsearch(const void* a, const void* b)
{
   assert(a && b);
   int64_t addr = *(int64_t*) a;
   txtblock_t* txtblock = *(txtblock_t**) b;
   if (addr < txtblock->addr)
      return -1;
   else if (addr > txtblock->addr)
      return 1;
   else
      return 0;
}

/**
 * Comparison function used for a binary search of structures representing blocks in the text file based on their identifier
 * @param i Pointer to an identifier
 * @param b Pointer to a structure representing a block in the text file
 * @return -1 if a is lower than the identifier is lower than the identifier of the block, 0 if both are equal, -1 otherwise
 */
static int txtblock_cmpid_bsearch(const void* i, const void* b)
{
   assert(i && b);
   uint32_t bid = *(int64_t*) i;
   txtblock_t* txtblock = *(txtblock_t**) b;
   if (bid < txtblock->bid)
      return -1;
   else if (bid > txtblock->bid)
      return 1;
   else
      return 0;
}

/**
 * Creates a new structure representing a loop from the text file
 * @param lid Loop identifier
 * @return New structure representing a loop
 */
static txtloop_t* txtloop_new(uint32_t lid)
{
   txtloop_t* txtloop = lc_malloc0(sizeof(*txtloop));
   txtloop->lid = lid;
   return txtloop;
}

/**
 * Adds a block to a structure representing a loop from a text files
 * @param txtloop The structure representing the loop
 * @param txtblock The structure representing the block
 */
static void txtloop_addblock(txtloop_t* txtloop, txtblock_t* txtblock)
{
   assert(txtloop && txtblock);
   ADD_CELL_TO_ARRAY(txtloop->blocks, txtloop->n_blocks, txtblock);
}

/**
 * Frees a structure representing a loop from the text file
 * @param txtloop The structure to free
 */
static void txtloop_free(void* tl)
{
   txtloop_t* txtloop = (txtloop_t*) tl;
   assert(txtloop);
   lc_free(txtloop->blocks);
   lc_free(txtloop);
}

/**
 * Frees a structure representing a loop from the text file, including the blocks it contains
 * @param txtloop The structure to free
 */
static void txtloop_free_all(void* tl)
{
   txtloop_t* txtloop = (txtloop_t*) tl;
   assert(txtloop);
   uint32_t i;
   for (i = 0; i < txtloop->n_blocks; i++)
      txtblock_free(txtloop->blocks[i]);
   txtloop_free(txtloop);
}

/**
 * Finalises a loop from the text file and stores it into a hashtable, or frees it if it does not contain any blocks
 * @param txtloop The loop
 * @param loops_map The hashtable containing loops from the text file
 */
static void txtloop_finalise(txtloop_t* txtloop, hashtable_t* loops_map)
{
   //Stores the loop in the hashtable, indexed on the address of the first block
   if (txtloop->n_blocks > 0) {
      //Orders the blocks in the loop by starting address
      qsort(txtloop->blocks, txtloop->n_blocks, sizeof(*txtloop->blocks),
            txtblock_cmp_qsort);
      //Stores the loop in the hashtable
      hashtable_insert(loops_map, &(txtloop->blocks[0]->addr), txtloop);
   } else {
      //Loop is empty: discarding it
      DBGMSG("Ignoring loop id %u as it does not contain any block\n",
            txtloop->lid);
      txtloop_free(txtloop);
   }
}

/**
 * Updates the identifiers of a series of loops.
 * @param loops Queue of loops from MAQAO analysis
 * @param loops_map Hashtable of loops present in the text file, indexed by the lowest address of their blocks
 * @param max_loopid Pointer to the maximum identifier of the loops ids
 */
static void update_loop_ids(queue_t* loops, hashtable_t* loops_map,
      uint32_t* max_loopid)
{
   assert(loops && loops_map && max_loopid);
   uint32_t i;
   FOREACH_INQUEUE(loops, loopiter)
   {
      loop_t *loop = GET_DATA_T(loop_t*, loopiter);
      queue_t* loopblocks = loop_get_blocks(loop);
      uint32_t n_lblocks;
      //Storing the blocks of the loop into an array
      QUEUE_TO_ARRAY(block_t*, loopblocks, lblocks, n_lblocks);
      /**\todo (2016-04-01) Duplicating the list of blocks brings some overhead, but it allows not to change the
       * order of blocks in the loop. Also, it's easier to access. Change this if it becomes too heavy for the memory*/
      if (n_lblocks == 0)
         continue;   //No blocks in the loop
      //Ordering the block based on the addresses of their first instruction
      qsort(lblocks, n_lblocks, sizeof(*lblocks), block_cmpbyaddr_qsort);
      DBGLVL(3,
            DBGMSG("Binary file contains loop %u composed of blocks beginning at addresses %#"PRIx64, loop_get_id(loop), block_get_first_insn_addr(lblocks[0])); for (i = 1; i < n_lblocks; i++) fprintf(stderr, ", %#"PRIx64, block_get_first_insn_addr(lblocks[i])); fprintf(stderr, "\n");)
      //Now looking up all loops from the text file with the same first address as this loop
      int64_t lblock_addr = block_get_first_insn_addr(lblocks[0]);
      queue_t* txtloops = hashtable_lookup_all(loops_map, &lblock_addr);
      FOREACH_INQUEUE(txtloops, tliter) {
         txtloop_t* txtloop = GET_DATA_T(txtloop_t*, tliter);
         if (n_lblocks != txtloop->n_blocks)
            continue; //Not the same number of blocks in the loop: this is not the loop you are looking for
         //Checking if each block of the loop from the text file has the same first addresses as those from MAQAO
         for (i = 0; i < n_lblocks; i++) {
            if (block_get_first_insn_addr(lblocks[i])
                  != txtloop->blocks[i]->addr)
               break; //Found a block with a different address: not the right loop
         }
         if (i == n_lblocks) {
            //The list of blocks are identical: we have found the right loop from the text
            DBGMSGLVL(1, "Reassigning identifiers: Loop %d => %d\n",
                  loop_get_id(loop), txtloop->lid);
            loop_set_id(loop, txtloop->lid);
            break;   //Stopping here, no need to go further
         }
      }
      if (tliter == NULL) {
         (*max_loopid)++;
         DBGMSGLVL(1,
               "Reassigning identifiers: Loop %d => %d (no equivalent found in the text file)\n",
               loop_get_id(loop), *max_loopid);
         loop_set_id(loop, *max_loopid);
      }
      queue_free(txtloops, NULL);
   }
}

/**
 * Replaces MAQAO block and loop ids with the ones specified in the input file
 * @param asmfile The analysed file
 * @param txtfile The text file from which it was build
 * @param fieldnames The names of the fields in the file
 * \todo (2016-04-01) Move this someplace else
 */
static void update_loop_block_ids(asmfile_t* asmfile, txtfile_t* txtfile,
      const asm_txt_fields_t *fieldnames)
{
   assert(asmfile && txtfile && fieldnames);
   assert(asmfile_test_analyze(asmfile, LOO_ANALYZE));
   /**\todo (2016-03-21) CHECK THAT BLOCK_FIRST_INSN_ADDR, LOOPF_ID and BLOCKF_ID exist in this file and leave if not*/

   uint32_t i;
   hashtable_t *loops_map = hashtable_new(int64p_hash, int64p_equal);
   uint32_t max_loopid = 0;

   //Retrieves the formatted file from which the asmfile was built
   fieldnames = asmfile_get_txtfile_field_names(asmfile);
   uint32_t n_blocks, n_loops;

   //First, identify the scope over which block identifiers are declared
   char* bid_scope = NULL;
   uint32_t n_filescns;
   txtscn_t** filescns = txtfile_getsections_bytype(txtfile,
         fieldnames->scnfile, &n_filescns);
   if (n_filescns > 0) {
      if (n_filescns > 1) {
         assert(filescns && filescns[0]);
         WRNMSG(
               "Multiple sections characterising the file found: keeping values from line %u\n",
               txtscn_getline(filescns[0]));
      }
      txtfield_t* bid_scope_field = txtscn_getfield(filescns[0],
            fieldnames->filefieldnames[TXTFILEF_BLOCKID_SCOPE]);
      bid_scope = txtfield_gettxt(bid_scope_field);
      lc_free(filescns);
   }
   if (str_equal(bid_scope, fieldnames->scnloops)) {
      DBGMSG0("Block identifiers are unique inside a loop\n");
      //Block ids are unique inside a given loop
      //Scanning all sections to get the loops and blocks following them
      i = 0;
      txtscn_t* scn = NULL;
      char* scntype = NULL;
      while (i < txtfile_getn_sections(txtfile)) {
         scn = txtfile_getsection(txtfile, i);
         scntype = txtscn_gettype(scn);
         if (str_equal(scntype, fieldnames->scnloops)) {
            //Section represents a loop
            //Retrieves in the section the field representing its id
            txtfield_t* field_loop_id = txtscn_getfield(scn,
                  fieldnames->loopfieldnames[LOOPF_ID]);
            //Retrieves the value stored in the field
            uint32_t loop_id = txtfield_getnum(field_loop_id);
            if (loop_id > max_loopid)
               max_loopid = loop_id;   //Tracking the highest loop id
            //Creates a new text loop object
            txtloop_t* txtloop = txtloop_new(loop_id);
            DBGMSGLVL(1, "Found loop %d at line %u in the file\n", loop_id,
                  txtscn_getline(scn));
            //Scanning all sections following it to detect blocks
            i++;
            while (i < txtfile_getn_sections(txtfile)) {
               txtscn_t* bscn = txtfile_getsection(txtfile, i);
               char* bscntype = txtscn_gettype(bscn);
               if (str_equal(bscntype, fieldnames->scnloops)) {
                  //Reached the next loop: stopping now
                  break;//Returning to analyse loops
               } else if (str_equal(bscntype, fieldnames->scnblocks)) {
                  //Section is a block belonging to the loop
                  //Retrieves the address of the block
                  int64_t addr =
                        txtfield_getnum(
                              txtscn_getfield(bscn,
                                    fieldnames->blockfieldnames[BLOCKF_FIRST_INSN_ADDR]));
                  //Creates a block structure with an invalid id and the address
                  txtblock_t* txtblock = txtblock_new(UINT32_MAX, addr);
                  //Associating the block to the loop
                  txtloop_addblock(txtloop, txtblock);
                  DBGMSGLVL(2, "Block at address %#"PRIx64" is in loop %d\n",
                        txtblock->addr, loop_id);
               } //Otherwise the section represents something else and we are not interested in it
               i++;
            }
            //Either reached the next loop section or the last section: finalising the current loop
            txtloop_finalise(txtloop, loops_map);
         } else {
            //Found another section than a loop: simply skipping to the next section
            i++;
         }
      }  //Completed the scan of sections

      //Now scans all loops in the file and tries to match them with those from the file
      fct_t *f;
      FOREACH_INQUEUE(asmfile->functions, funciter) {
         f = GET_DATA_T(fct_t*, funciter);
         // If the function has a virtual block
         //      if (FCT_ENTRY(f)->begin_sequence != NULL)
         //         continue;

         //Updates loop identifiers
         update_loop_ids(fct_get_loops(f), loops_map, &max_loopid);
      }
      hashtable_free(loops_map, txtloop_free_all, NULL);
   } else {
      DBGMSG0("Block identifiers are unique inside the full file\n");
      //Block ids are unique inside the whole file
      if (bid_scope && strcmp(bid_scope, fieldnames->scnfile) != 0) {
         //Printing a warning if the scope is defined and not equal to file
         WRNMSG(
               "Scope for block ids %s unsupported: assuming block identifiers are unique among the whole file\n",
               bid_scope);
      }
      //Retrieves all sections whose type name was defined in fieldnames->scnblocks, sorted over the field representing their id
      txtscn_t** blocks = txtfile_getsections_bytype_sorted(txtfile,
            fieldnames->scnblocks, &n_blocks,
            fieldnames->blockfieldnames[BLOCKF_ID]);

      //Builds an array of structures representing all the blocks in the text file
      txtblock_t* txtblocks[n_blocks];
      uint32_t n_txtblocks = 0;
      for (i = 0; i < n_blocks; i++) {
         uint32_t bid = txtfield_getnum(
               txtscn_getfield(blocks[i],
                     fieldnames->blockfieldnames[BLOCKF_ID]));
         int64_t addr = txtfield_getnum(
               txtscn_getfield(blocks[i],
                     fieldnames->blockfieldnames[BLOCKF_FIRST_INSN_ADDR]));
         //Detecting duplicated blocks and ignoring them
         if (n_txtblocks > 0 && bid == txtblocks[n_txtblocks - 1]->bid) {
            if (addr == txtblocks[n_txtblocks - 1]->addr) {
               INFOMSG("Block %u is declared twice\n", bid);
            } else {
               WRNMSG(
                     "Blocks at addresses %#"PRIx64" and %#"PRIx64" have the same identifier %u: ignoring block at address %#"PRIx64"\n",
                     txtblocks[n_txtblocks - 1]->addr, addr, bid, addr);
            }
            continue;
         }
         txtblocks[n_txtblocks++] = txtblock_new(bid, addr);
         DBGMSGLVL(2, "Found block %d at address %#"PRIx64" in the text file\n",
               txtblocks[n_txtblocks - 1]->bid,
               txtblocks[n_txtblocks - 1]->addr);
      }
      //Stores the highest identifier
      uint32_t max_txtbid = txtblocks[n_txtblocks - 1]->bid;

      //Free the array of text sections representing blocks
      if (blocks)
         lc_free(blocks);

      //Retrieves all sections whose type name was defined in fieldnames->scnloops
      txtscn_t** loops = txtfile_getsections_bytype(txtfile,
            fieldnames->scnloops, &n_loops);

      //Builds a hashtable of structures representing the loops in the text file, indexed on the address of their first block
      for (i = 0; i < n_loops; i++) {
         uint32_t j;
         //Retrieves in the section the field whose name is defined in fieldnames->loopfieldnames[LOOPF_ID]
         txtfield_t* field_loop_id = txtscn_getfield(loops[i],
               fieldnames->loopfieldnames[LOOPF_ID]);
         //Retrieves the value stored in the field
         int loop_id = txtfield_getnum(field_loop_id);
         if (loop_id > max_loopid)
            max_loopid = loop_id;   //Tracking the highest loop id
         //Retrieves in the section the field list whose name is defined in fieldnames->loopfieldnames[LOOPF_BLOCKS]
         uint16_t n_lblocks_ids;
         txtfield_t** lblocks_ids = txtscn_getfieldlist(loops[i],
               fieldnames->loopfieldnames[LOOPF_BLOCKS], &n_lblocks_ids);
         //Creates a new text loop object
         txtloop_t* txtloop = txtloop_new(loop_id);

         //Scans the retrieved list of fields
         for (j = 0; j < n_lblocks_ids; j++) {
            //Retrieves the value stored in the field
            int lblock_id = txtfield_getnum(lblocks_ids[j]);
            //Looks up a section representing a block and with the same ID
            txtblock_t** txtblock = bsearch(&lblock_id, txtblocks, n_txtblocks,
                  sizeof(*txtblocks), txtblock_cmpid_bsearch);
            //Check if such a block was found
            if (txtblock != NULL) {
               //Stores the block in the loop
               DBGMSGLVL(2, "Block %d at address %#"PRIx64" is in loop %d\n",
                     lblock_id, (*txtblock)->addr, loop_id);
               txtloop_addblock(txtloop, *txtblock);
            } else {
               DBGMSG("Block %d found in loop %d from file %s not found\n",
                     lblock_id, loop_id, txtfile_getname(txtfile));
            }
         }
         //Either reached the next loop section or the last section: finalising the current loop
         txtloop_finalise(txtloop, loops_map);

         if (lblocks_ids)
            lc_free(lblocks_ids);
      }
      //Now sorts the array of blocks by address
      qsort(txtblocks, n_txtblocks, sizeof(*txtblocks), txtblock_cmp_qsort);

      //Freeing the arrays of text sections representing loops
      if (loops)
         lc_free(loops);

      //Now scans all loops in the file and tries to match them with those from the file
      fct_t *f;
      FOREACH_INQUEUE(asmfile->functions, funciter) {
         f = GET_DATA_T(fct_t*, funciter);
         // If the function has a virtual block
         //      if (FCT_ENTRY(f)->begin_sequence != NULL)
         //         continue;
         //Updates identifiers of all blocks in the function
         FOREACH_INQUEUE(fct_get_blocks(f), blockiter) {
            block_t* block = GET_DATA_T(block_t*, blockiter);
            int64_t blockaddr = block_get_first_insn_addr(block);
            //Looks up in the array of blocks from the text file for a block with the same starting address
            txtblock_t** txtblock = bsearch(&blockaddr, txtblocks, n_txtblocks,
                  sizeof(*txtblocks), txtblock_cmpaddr_bsearch);
            uint32_t newbid;
            if (txtblock) {
               //Found a text block at this address: updates the identifier of the analysed block with the identifier from the file
               newbid = (*txtblock)->bid;
            } else {
               //Block not found in the text file: updating its identifier to be higher that the max id found in the file
               newbid = ++max_txtbid;
               DBGMSGLVL(2,
                     "Block with id %d and address %#"PRIx64" not found in the text file\n",
                     block_get_id(block), blockaddr);
            }
            DBGMSGLVL(1, "Reassigning identifiers: Block %d => %d\n",
                  block_get_id(block), newbid);
            block_set_id(block, newbid);
         }

         //Updates loop identifiers
         update_loop_ids(fct_get_loops(f), loops_map, &max_loopid);
      }
      for (i = 0; i < n_txtblocks; i++)
         txtblock_free(txtblocks[i]);
      hashtable_free(loops_map, txtloop_free, NULL);
   }
}

/*
 * Adds and analyzes a formatted assembly text file into a project
 * \param project an existing project
 * \param filename Name of the formatted assembly file to load and analyze
 * \param content Content of the formatted assembly file to load and analyze. Will be used if \c filename is NULL
 * \param archname Name of the architecture
 * \param user Description of the micro architecture
 * \param fieldnames Names of the fields and sections containing informations about the assembly elements
 * \return A new asmfile structure
 */
asmfile_t* project_load_txtfile(project_t *project, char *filename,
      char* content, char* archname, char *uarch_name,
      const asm_txt_fields_t *fieldnames)
{
#ifdef _MAQAO_TIMER_
   clock_t t1, t2;
#endif
   asmfile_t *asmfile;
   txtfile_t* txtfile;
   asm_txt_fields_t fields;
   DBGMSG("project %s\n", project->file);

   if (filename)
      txtfile = txtfile_open(filename);
   else if (content)
      txtfile = txtfile_load(content);
   else {
      ERRMSG("Unable to load text file: name and content are NULL\n");
      return NULL;
   }

   if (!archname) {
      ERRMSG("Missing architecture name for reading file %s.\n", filename);
      return NULL;
   }

   int retcode = txtfile_parse(txtfile);
   if (ISERROR(retcode)) {
      //Unable to parse text file
      ERRMSG("Unable to parse text file %s (error at line %u: %s)\n", filename,
            txtfile_getcurrentline(txtfile), errcode_getmsg(retcode));
      return (NULL);
   }

   DBGMSG("loading asm %s in project %p ...\n", filename, project);
#ifdef _MAQAO_TIMER_
   t1 = clock();
#endif

   asmfile = project_add_file(project, filename);
   retcode = asmfile_init_proc(asmfile, archname, uarch_name, NULL);
   if (ISERROR(retcode))
      asmfile_set_last_error_code(asmfile, retcode);
   /**\todo (2017-01-13) Decide whether we exit with an error here or go on (it was the previous behaviour, there was no test on the uarch_name)*/


   //Declaring the names of fields in the file if they were not set
   if (fieldnames == NULL) {
      //WARNING("Missing declaration of field names: assuming default values\n");
      DBGMSG0(
            "Warning: Missing declaration of field names: assuming default values\n");
      memset(&fields, 0, sizeof(fields));
      fields.scninsns = "body";
      fields.scnbrchlbls = "block";
      fields.scnblocks = "block";
      fields.scnloops = "loop";
      fields.scnfctlbls = "function";
      fields.scnarch = "arch";
      fields.scnfile = "file";

      fields.insnfieldnames[INSNF_FULL_ASSEMBLY] = "assembly";
      fields.insnfieldnames[INSNF_ADDRESS] = "address";
      fields.insnfieldnames[INSNF_DBG_SRCLINE] = "line";
      fields.insnfieldnames[INSNF_DBG_SRCFILE] = "file";
      fields.blockfieldnames[BLOCKF_FIRST_INSN_ADDR] = "address";
      fields.blockfieldnames[BLOCKF_ID] = "bid";
      fields.labelfieldnames[LBLF_NAME] = "name";
      fields.labelfieldnames[LBLF_ADDRESS] = "address";
      fields.loopfieldnames[LOOPF_BLOCKS] = "blocks";
      fields.loopfieldnames[LOOPF_ID] = "lid";
      fields.archfieldnames[ARCHF_NAME] = "arch";
      fields.filefieldnames[TXTFILEF_BLOCKID_SCOPE] = "bid_scope";
   } else {
      //Checking that the names of mandatory fields in the file are properly declared
      if (fieldnames->insnfieldnames[INSNF_FULL_ASSEMBLY] == NULL) {
         ERRMSG(
               "Missing name for field representing the assembly code of an instruction\n");
         return NULL;
      }
      if (fieldnames->scninsns == NULL) {
         ERRMSG(
               "Missing name for section containing the assembly code of instructions\n");
         return NULL;
      }
      fields = *fieldnames;
   }

#ifdef _MAQAO_TIMER_
   t2 = clock();
   printf("loading asm %s in project %p ...[%.2f s]\n",filename, project, (float) (t2-t1) / CLOCKS_PER_SEC);
   DBGMSG0("disassembling ...\n");
   t1 = clock();
#endif
   retcode = asmfile_assemble_fromtxtfile(asmfile, archname, txtfile, fields);
   if (ISERROR(retcode)) {
      //Unable to assemble the file => error
      project_remove_file(project, asmfile);
      return (NULL);
   }
   retcode = asmfile_disassemble_existing(asmfile);
   if (ISERROR(retcode)) { // The binary has not been analyzed => error.
      project_remove_file(project, asmfile);
      return (NULL);
   }
   //Performs all analyses on the file
   analyze_disassembled_file(project, asmfile);

   //Updates all block and loop identifiers in the analyzed files to be those from the text file
   update_loop_block_ids(asmfile, txtfile, &fields);

   return asmfile;
}

/*
 * Parses a binary and fills an asmfile object
 * \param project an existing project
 * \param filename name of the assembly file to load and analyze
 * \param uarch_name The name of the micro-architecture for which to analyze the file
 * \return a new asmfile structure
 */
asmfile_t* project_parse_file(project_t *project, char *filename, char *uarch_name)
{
   asmfile_t *asmfile;
   DBGMSG("project %s\n", project->file);
   if (!filename || !fileExist(filename)) {
      DBGMSG0("Cannot open binary file (invalid name or not found).\n");
      return NULL;
   }

   DBGMSG("loading asm %s in project %p ...\n", filename, project);
   asmfile = project_add_file(project, filename);
   int res = asmfile_init_proc(asmfile, NULL, uarch_name, NULL);
   if (ISERROR(res))
      asmfile_set_last_error_code(asmfile, res);
   /**\todo (2017-01-13) Decide whether we exit with an error here or go on (it was the previous behaviour, there was no test on the uarch_name)*/
   /**\todo (2017-01-13) Also, seriously, why do we need the uarch_name here???*/

   DBGMSG0("Reading elf ...\n");
   asmfile_add_parameter(asmfile, PARAM_MODULE_DISASS, PARAM_DISASS_OPTIONS,
         (void*) DISASS_OPTIONS_PARSEONLY);
   asmfile_disassemble(asmfile);
   DBGMSG0("parsing done\n");

   return asmfile;
}
