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

/*
 * Defines functions to prepare display from raw data collected in experiment directory
 * To sum up, results (%time and seconds) will be aggregated per function/loops at thread level
 */

#include <stdint.h>
#include <string.h> // strcmp...
#include <libgen.h> // basename
#include <unistd.h> // access
#include "utils.h" // for_each_directory...
#include "libmcommon.h" // hashtable_t...
#include "list_libc.h" // load_libc_functions
#include "binary_format.h" // get_lprof_header...
#include "IP_events_format.h" // raw_IP_events_t...
#include "prepare_sampling_display_shared.h" // handle_callchains...
#include "prepare_sampling_display.h" // lprof_context_t...

// Create a thread and insert it to parent process
static void insert_thread_to_process (long tid, lprof_process_t *process, unsigned events_per_group, unsigned int nbExtraCat)
{
   // Create thread
   lprof_thread_t *t = lc_malloc (sizeof *t);
   t->tid             = tid;
   t->rank            = array_length (process->threads);
   t->parent_process  = process;
   t->fcts            = hashtable_new (str_hash, str_equal);
   t->loops           = hashtable_new (str_hash, str_equal);
   t->events_nb       = lc_calloc (events_per_group  , sizeof t->events_nb[0]);
   t->categories      = lc_calloc (NB_CATEGORIES+nbExtraCat, sizeof(t->categories[0]));
   t->libc_categories = lc_calloc (LIBC_NB_CATEGORIES, sizeof t->libc_categories[0]);

   // Insert new thread to parent process
   array_add (process->threads, t);
}

// To call via for_each_directory_in_directory
// Create a process and insert it to parent node
static void insert_process_to_node (const char *node_path, const char *process_id, void *data)
{
   (void) node_path; // Silent unused warning

   // Ignoring invalid name entries (expecting positive decimal number)
   char *endptr; if (strtol (process_id, &endptr, 10) < 1 || *endptr != '\0') return;

   lprof_node_t *node = data;

   // Create process
   lprof_process_t *p = lc_malloc0 (sizeof *p);
   p->pid         = atol (process_id);
   p->parent_node = node;
   p->threads     = array_new(); // array of threads
   p->is_library  = hashtable_new (int_hash, int_equal);
   p->ip2fct_cache= hashtable_new (direct_hash, direct_equal);
   
   sampling_display_context_t *context = node->parent_context;
   char process_path [strlen (context->exp_path) + strlen (node->name) + strlen (process_id) + 3];
   sprintf (process_path, "%s/%s/%s", context->exp_path, node->name, process_id);
   p->exe_offset  = get_exe_offset (process_path, context->lprof_version);

   // Insert new process to parent node
   array_add (node->processes, p);
}

static hashtable_t *get_map_rank_for_node_processes (const char *node_path)
{
   FILE *fp = fopen_in_directory (node_path, "processes_index.lua", "r");
   if (!fp) return NULL;

   hashtable_t *pid2rnk = hashtable_new (direct_hash, direct_equal);

   // For each line in the file
   char line[1024];
   while (fgets (line, sizeof line, fp) != NULL) {
      uint64_t pid, rnk;
      if (sscanf (line, "pidToPidIdx[\"%"PRIu64"\"] = %"PRIu64"", &pid, &rnk) == 2)
         hashtable_insert (pid2rnk, (void *) pid, (void *) rnk);
   }

   fclose (fp);
   return pid2rnk;
}

// To call via for_each_directory_in_directory
// Create a node and insert it to parent context
static void insert_node_to_context (const char *exp_path, const char *node_name, void *context_data)
{
   // Ignore html directory which is not a node
   if (strcmp (node_name, "html") == 0) return;

   // Get node path ("<exp_path>/<node_name>")
   char *node_path = lc_malloc (strlen (exp_path) + strlen (node_name) + 2);
   sprintf (node_path, "%s/%s", exp_path, node_name);

   // Ignore corrupted/invalid node directories
   char index_name [strlen (node_path) + strlen ("/processes_index.lua") + 1];
   sprintf (index_name, "%s/%s", node_path, "processes_index.lua");
   if (access (index_name, F_OK) == -1) {
      WRNMSG ("Ignoring %s node directory: processes_index.lua not found\n", node_name);
      lc_free (node_path);
      return;
   }
   FILE *fp = fopen_in_directory (node_path, "processes_index.lua", "r");
   if (!fp) {
      DBGMSG ("Ignoring %s node directory: cannot read processes_index.lua\n", node_name);
      lc_free (node_path);
      return;
   }
   fclose (fp);

   sampling_display_context_t *context = context_data;

   // Create node
   lprof_node_t *n = lc_malloc0 (sizeof *n);
   n->name           = strdup (node_name);
   n->rank           = array_length (context->nodes);
   n->parent_context = context;
   n->processes      = array_new(); // array of processes

   // Insert new node to parent context
   array_add (context->nodes, n);

   // Insert related processes (to this node)
   for_each_directory_in_directory (node_path, insert_process_to_node, n);
   hashtable_t *pid2rnk = get_map_rank_for_node_processes (node_path);
   lc_free (node_path);

   // Set map_rank field for all node processes
   FOREACH_INARRAY (n->processes, ps_iter) {
      lprof_process_t *p = ARRAY_GET_DATA (NULL, ps_iter);
      p->map_rank = (uint64_t) hashtable_lookup (pid2rnk, (void*)(uint64_t) p->pid) - 1;
   }
   hashtable_free (pid2rnk, NULL, NULL);

   // Create virtual function to gather unknown functions
   const unsigned nb_processes = array_length (n->processes);
   SinfoFunc *f = lc_malloc (sizeof *f);
   f->name            = "Unknown functions";
   f->src_file        = NULL;
   f->src_line        = -1;
   f->hwcInfo         = lc_calloc (nb_processes, sizeof f->hwcInfo[0]);
   f->callChainsInfo  = lc_calloc (nb_processes, sizeof f->callChainsInfo[0]);
   f->totalCallChains = lc_calloc (nb_processes, sizeof f->totalCallChains[0]);
   f->libraryIdx      = -1;
   n->unknown_fcts = f;
}

// Create nodes and processes corresponding to exp_path and insert them to context
static void insert_nodes_and_processes_to_context (sampling_display_context_t *context)
{
   context->nodes = array_new(); // array of lprof_node_t elements

   // For each node directory in the experiment directory
   for_each_directory_in_directory (context->exp_path, insert_node_to_context, context);
}

// Loads executable metadata from exp_path/binary.lprof
static void load_exe_metadata (const char *exp_path, lprof_header_t *lprof_header,
                               lprof_binary_info_header_t *exe_metadata_header,
                               lprof_binary_info_t *exe_metadata)
{
   FILE *fp = fopen_in_directory (exp_path, "binary.lprof", "r");
   if (!fp) {
      HLTMSG ("Cannot load executable metadata from %s\n", exp_path);
      exit (-1);
   }

   // Read lprof header (including lprof version...)
   get_lprof_header (fp, lprof_header);

   // Get number of functions and loops
   get_bin_info_header (fp, exe_metadata_header);

   // Write metadata to exe_metadata
   get_bin_info (fp, &(exe_metadata_header->nbFunctions), &(exe_metadata_header->nbLoops), exe_metadata);

   fclose (fp);
}

// Loads libraries metadata from exp_path/node_name/libs/*.lprof
static void load_lib_metadata (const char *libs_path, const char *file_name,
                               lprof_libraries_info_header_t *lib_metadata_header,
                               lprof_libraries_info_t *lib_metadata)
{
   FILE *fp = fopen_in_directory (libs_path, file_name, "r");
   if (!fp) {
      ERRMSG ("Cannot load libraries metadata from %s\n", libs_path);
      memset (lib_metadata_header, 0, sizeof *lib_metadata_header);
      lib_metadata_header->nbLibraries = 0;
      return;
   }

   // Skip header (unused)
   lprof_header_t lprof_header;
   get_lprof_header (fp, &lprof_header);

   // Get number of functions and loops
   get_libs_info_header (fp, lib_metadata_header);

   // Write metadata to lib_metadata
   get_libs_info (fp, &(lib_metadata_header->nbLibraries), lib_metadata);

   fclose (fp);
}

// Returns AVL tree corresponding to functions read from metadata
static avlTree_t *get_fcts_tree (unsigned nb_fcts, lprof_fct_t *fcts, unsigned nb_processes)
{
   unsigned i, j;
   avlTree_t *fcts_tree = NULL;

   for (i=0; i < nb_fcts; i++) {
      for (j=0; j < fcts[i].nbParts; j++) {
         SinfoFunc *fct = function_to_infoFunc (&fcts[i], j, nb_processes);
         fcts_tree = insert (fcts[i].startAddress[j], fct, fcts_tree);
      }
   }

   return fcts_tree;
}

// Returns AVL tree corresponding to loops read from metadata
static avlTree_t *get_loops_tree (unsigned nb_loops, lprof_loop_t *loops, unsigned nb_processes)
{
   unsigned i, j;
   avlTree_t *loops_tree = NULL;

   for (i=0; i < nb_loops; i++) {
      for (j=0; j < loops[i].nbParts; j++) {
         SinfoLoop *loop = lprof_loop_to_infoLoop (&loops[i], j, nb_processes);
         loops_tree = insert (loops[i].startAddress[j], loop, loops_tree);
      }
   }

   return loops_tree;
}

// Used by load_system_maps
typedef struct { uint64_t addr; char *name; } map_symbol_t;

static int compar_map_symbols (const void *p1, const void *p2)
{
   const map_symbol_t *s1 = *((map_symbol_t *const *) p1);
   const map_symbol_t *s2 = *((map_symbol_t *const *) p2);
   return s1->addr - s2->addr;
}

static void insert_last_symbol (avlTree_t **sys_tree, uint64_t addr, unsigned nb_processes)
{
   SinfoFunc *fct = lc_malloc (sizeof *fct);
   fct->name            = lc_strdup ("unknown kernel region");
   fct->src_file        = NULL;
   fct->src_line        = -1;
   fct->hwcInfo         = lc_calloc (nb_processes, sizeof fct->hwcInfo[0]);
   fct->callChainsInfo  = lc_calloc (nb_processes, sizeof fct->callChainsInfo[0]);
   fct->totalCallChains = lc_calloc (nb_processes, sizeof fct->totalCallChains[0]);
   fct->libraryIdx      = -2; //  -2 == SYSTEM CALL

   // CF AMD64 (x86-64) "canonical form addresses"
   fct->start           = (addr == 0) ? 0xFFFF800000000000 : addr;
   fct->stop            = UINT64_MAX;

   *sys_tree = insert (addr, fct, *sys_tree);
}

// Converts system maps file (node_path/system_map) to a tree of system functions (indexed by address ranges)
static avlTree_t *load_system_maps (const char *node_path, unsigned nb_processes)
{
   FILE *fp = fopen_in_directory (node_path, "system_map", "r");
   if (!fp) return NULL;

   array_t *map_symbols = array_new();

   // Load symbols (address and function name) to an array of structures
   char line [1024];
   while (fgets (line, sizeof line, fp) != NULL) {
      uint64_t addr; char name[500];
      int ret = sscanf (line, "%"PRIx64" %*[tT] %s", &addr, name);
      if (ret == 2) {
         map_symbol_t *ms = lc_malloc (sizeof *ms);
         ms->addr = addr;
         ms->name = lc_strdup (name);
         array_add (map_symbols, ms);
      }
   }

   fclose (fp);

   if (array_length (map_symbols) == 0) return NULL;
  
   // Sort symbols by increasing address
   array_sort (map_symbols, compar_map_symbols);

   avlTree_t *sys_tree = NULL;
   map_symbol_t *prv = NULL; // previous symbol
   FOREACH_INARRAY (map_symbols, ms_iter) {
      map_symbol_t *cur = ARRAY_GET_DATA (NULL, ms_iter); // current symbol

      // Inserting previous function
      // Stop address for a function can be deduced only from next function start address
      if (prv != NULL && prv->addr < cur->addr) {
         SinfoFunc *fct = lc_malloc (sizeof *fct);
         fct->name            = prv->name;
         fct->start           = prv->addr; // current function start addr. is previous function stop addr.
         fct->stop            = cur->addr - 1;
         fct->src_file        = NULL;
         fct->src_line        = -1;
         fct->hwcInfo         = lc_calloc (nb_processes, sizeof fct->hwcInfo[0]);
         fct->callChainsInfo  = lc_calloc (nb_processes, sizeof fct->callChainsInfo[0]);
         fct->totalCallChains = lc_calloc (nb_processes, sizeof fct->totalCallChains[0]);
         fct->libraryIdx      = -2; //  -2 == SYSTEM CALL
         DBGMSGLVL(1, "%s [%"PRIx64"- %"PRIx64"]\n", fct->name, fct->start, fct->stop);
         sys_tree = insert (prv->addr, fct, sys_tree);
      } else if (prv != NULL)
         lc_free (prv->name);

      prv = cur;
   }
   insert_last_symbol (&sys_tree, prv->addr, nb_processes);

   array_free (map_symbols, lc_free);

   return sys_tree;
}

// Pushes (as a table) to the LUA stack data related to a function coming from application executable
static void push_exe_fct (lua_State *L, lprof_fct_t *fct)
{
   lua_newtable (L); // function data

   // Outermost loops
   lua_pushstring (L, "outermost loops"); // key
   lua_newtable (L);                      // value: outermost loops table
   unsigned i;
   for (i=0; i<fct->nbOutermostLoops; i++) {
      lua_pushnumber (L, fct->outermostLoopsList [i]); // key: loop ID
      lua_pushboolean (L, TRUE);                       // value: boolean true
      lua_rawset (L, -3);                              // set key+value for level-2 table (loops)
   }
   lua_rawset (L, -3); // set outermost loops key to "outermost loops"
}

// Pushes (as a table) to the LUA stack data related to a loop coming from application executable
static void push_exe_loop (lua_State *L, lprof_loop_t *loop)
{
   lua_newtable (L); // loop data

   // Children loops : e.g. { [42] = true, [48] = true }
   lua_pushstring (L, "children"); // key
   lua_newtable (L);               // value: table with (ID,true) pairs
   unsigned i;
   for (i=0; i<loop->nbChildren; i++) {
      lua_pushnumber (L, loop->childrenList [i]); // key
      lua_pushboolean (L, TRUE);                  // value
      lua_rawset (L, -3);                         // set key+value for children loops table
   }
   lua_rawset (L, -3); // set children table key to "children"

   // Source file
   lua_pushstring (L, "src_file");               // key
   lua_pushstring (L, basename (loop->srcFile)); // value
   lua_rawset (L, -3);                           // set value key to "src_file"

   // Start source line
   lua_pushstring (L, "src_line_start");    // key
   lua_pushnumber (L, loop->srcStartLine);  // value
   lua_rawset (L, -3);                      // set value key to "src_line_start"

   // End source source line
   lua_pushstring (L, "src_line_end");    // key
   lua_pushnumber (L, loop->srcStopLine); // value
   lua_rawset (L, -3);                    // set value key to "src_line_end"
}

// To pass to push_node_process_thread_table: push list of hot functions
static char *get_fct_line (const SinfoFunc *fct, const lprof_thread_t *thread)
{
   const lprof_process_t *process = thread->parent_process;
   const lprof_node_t *node = process->parent_node;
   const sampling_display_context_t *context = node->parent_context;

   if (strcmp (context->hwc_mode, "maqao_events") == 0)
      return create_fct_line ((SinfoFunc *) fct, process->map_rank, thread->rank, context->events_per_group, context->sampling_period, context->cpu_freq, context->ref_freq, thread->events_nb, context->show_sample_val, context->ext_mode, node->libsInfo, context->exe_name);
   else
      return create_fct_line_custom ((SinfoFunc *) fct, process->map_rank, thread->rank, context->events_per_group, thread->events_nb, context->show_sample_val, node->libsInfo, context->exe_name);
}

// Create a string summarizing data for the current loop hotspot
static char *get_loop_line (const SinfoLoop *loop, const lprof_thread_t *thread)
{
   const lprof_process_t *process = thread->parent_process;
   const lprof_node_t *node = process->parent_node;
   const sampling_display_context_t *context = node->parent_context;

   if (strcmp (context->hwc_mode, "maqao_events") == 0)
      return create_loop_line ((SinfoLoop *) loop, process->map_rank, thread->rank, context->events_per_group, context->sampling_period, context->cpu_freq, context->ref_freq, thread->events_nb, context->show_sample_val, context->ext_mode, node->libsInfo, context->exe_name);
   else
      return create_loop_line_custom ((SinfoLoop *) loop, process->map_rank, thread->rank, context->events_per_group, thread->events_nb, context->show_sample_val, node->libsInfo, context->exe_name);
}

// Pushes to the LUA stack (as a table) data related to callchains for a given function
static void push_callchains (lua_State *L, const lprof_thread_t *thread, const SinfoFunc *fct)
{
   const unsigned process_rank = thread->parent_process->map_rank;

   const hashtable_t *callchains = fct->callChainsInfo [process_rank] [thread->rank];
   if (callchains == NULL) { lua_pushnil (L); return; }

   const float total = fct->totalCallChains [process_rank] [thread->rank];
   if (total == 0.0f) { lua_pushnil (L); return; }

   lua_newtable (L); // list of callchains
   FOREACH_INHASHTABLE (callchains, callchain_iter) {
      char *callchain         = GET_KEY    (char *    , callchain_iter);
      unsigned *nbOccurrences = GET_DATA_T (unsigned *, callchain_iter);

      const float percentage = *nbOccurrences * 100.0f / total;
      if (percentage > 0.0f) {
         lua_pushstring (L, callchain);  // key (callchain itself, e.g. "main->foo")
         lua_pushnumber (L, percentage); // value (callchain weight)
         lua_rawset (L, -3); // set key+value as pair in list of callchains
      }
   }
}

// Pushes (as a table) to the LUA stack data related to a function for a given thread
static void push_thread_fct (lua_State *L, const lprof_thread_t *thread, const SinfoFunc *fct)
{
   lua_newtable (L); // value: function data (table)

   // Display (pre-formatted) string
   lua_pushstring (L, "display string");
   char *line = get_fct_line (fct, thread);
   if (line != NULL) {
      lua_pushstring (L, line);
      free (line);
   } else
      lua_pushstring (L, "N/A");
   lua_rawset (L, -3); // set key to "display string"

   // Callchains
   lua_pushstring (L, "callchains");
   push_callchains (L, thread, fct);
   lua_rawset (L, -3); // set key to "callchains"
}

// Pushes (as a table) to the LUA stack data related to a loop for a given thread
static void push_thread_loop (lua_State *L, const lprof_thread_t *thread, const SinfoLoop *loop)
{
   lua_newtable (L); // value: loop data (table)

   // Display (pre-formatted) string
   lua_pushstring (L, "display string"); // key
   char *line = get_loop_line (loop, thread);
   if (line != NULL) {
      lua_pushstring (L, line);
      free (line);
   } else
      lua_pushstring (L, "N/A");
   lua_rawset (L, -3); // set key to "display string"

   // Thread time percent
   lua_pushstring (L, "thread time percent"); // key
   const uint64_t thread_cycles = thread->events_nb [0];
   if (thread_cycles > 0) {
      const uint32_t loop_cycles = loop->hwcInfo [thread->parent_process->map_rank][thread->rank][0];
      lua_pushnumber (L, loop_cycles * 100.0f / thread_cycles);
   } else
      lua_pushnil (L);
   lua_rawset (L, -3); // set key to "thread time percent"
}

/* Pushes a thread object (as a table) on the LUA stack
 * This table contains all thread-level data/results
 */
static void push_thread (lua_State *L, const lprof_thread_t *thread,
                         boolean_t display_functions, boolean_t display_loops)
{
   unsigned i;
   lua_newtable (L); // thread data

   // Rank (in parent process)
   lua_pushstring (L, "rank");       // key
   lua_pushnumber (L, thread->rank); // value
   lua_rawset (L, -3);

   // Functions
   if (display_functions) {
      lua_pushstring (L, "functions"); // key
      lua_newtable (L);                // value: table of functions
      FOREACH_INHASHTABLE (thread->fcts, fct_iter) {
         SinfoFunc *fct = GET_DATA_T (SinfoFunc *, fct_iter);
         lua_pushstring (L, fct->name);    // key
         push_thread_fct (L, thread, fct); // value: function data (table)
         lua_rawset (L, -3);               // set function data table key
      }
      lua_rawset (L, -3); // set table of functions key to "functions"
   }

   // Loops
   if (display_loops) {
      lua_pushstring (L, "loops"); // key
      lua_newtable (L);            // value: table of modules
      FOREACH_INHASHTABLE (thread->loops, module) {
         const char *module_name = GET_KEY (char *, module);
         const hashtable_t *module_loops = GET_DATA_T (hashtable_t *, module);
         
         lua_pushstring (L, module_name); // key
         lua_newtable (L); // value: table of loops (in current module)
         FOREACH_INHASHTABLE (module_loops, loop_iter) {
            SinfoLoop *loop = GET_DATA_T (SinfoLoop *, loop_iter);
            lua_pushnumber (L, loop->loop_id);  // key
            push_thread_loop (L, thread, loop); // value: loop data (table)
            lua_rawset (L, -3);                 // set loop data table key
         }
         lua_rawset (L, -3); // set table of loops key to module name
      }
      lua_rawset (L, -3); // set table of modules key to "loops"
   }

   // Time in seconds
   lua_pushstring (L, "time in seconds"); // key
   const sampling_display_context_t *context = thread->parent_process->parent_node->parent_context;
   if (strcmp (context->hwc_mode, "maqao_events") == 0 ||
       strcmp (context->hwc_mode, "maqao_custom") == 0)
      lua_pushnumber (L, (thread->events_nb[0] * context->sampling_period) / context->ref_freq);
   else
      lua_pushnumber (L, thread->events_nb[0] * 0.001f);
   lua_rawset (L, -3); // set key to "time in seconds"

   // Categories
   if (display_functions) {
      // Categories
      lua_pushstring (L, "categories");
      lua_newtable (L);
      for (i=0; i<(NB_CATEGORIES+context->nbExtraCat); i++) {
         lua_pushnumber (L, thread->categories [i]);
         lua_rawseti (L, -2, i+1); // set key for pushed number
      }
      lua_rawset (L, -3); // set key to "categories"

      // Libc categories
      lua_pushstring (L, "libc categories");
      lua_newtable (L);
      for (i=0; i<LIBC_NB_CATEGORIES-1; i++) {
         const unsigned tot = thread->libc_categories [LIBC_TOTAL_CATEGORY];
         lua_pushnumber (L, tot > 0 ? thread->libc_categories [i] * 100.0f / tot : 0.0f);
         lua_rawseti (L, -2, i+1); // set key for pushed number
      }
      lua_rawset (L, -3); // set key to "libc categories"
   }
}

/* Pushes a process object (as a table) on the LUA stack
 * This table contains all process-level data/results (including children = threads)
 */
static void push_process (lua_State *L, const lprof_process_t *process)
{
   const boolean_t display_functions = process->parent_node->parent_context->display_functions;
   const boolean_t display_loops     = process->parent_node->parent_context->display_loops;

   lua_newtable (L); // process data

   // Rank (according to <node>/processes_index.lua)
   lua_pushstring (L, "rank");            // key
   lua_pushnumber (L, process->map_rank); // value
   lua_rawset (L, -3);

   // Process libraries
   if (display_functions) {
      lua_pushstring (L, "is_library");
      lua_newtable (L); // list of libraries: { ["foo"] = true, ["bar"] = true... }
      FOREACH_INHASHTABLE (process->is_library, fct_iter) {
         SinfoFunc *fct = GET_DATA_T (SinfoFunc *, fct_iter);
         char *fct_name = strndup (fct->name, 75);
         lua_pushstring (L, fct_name); // key
         lua_pushboolean (L, TRUE);    // value
         lua_rawset (L, -3);           // set key+value for list of libraries
         free (fct_name);
      }
      lua_rawset (L, -3); // set library list key to "is_library"
   }

   // Threads
   lua_pushstring (L, "threads"); // key
   lua_newtable (L);              // value: table of threads
   FOREACH_INARRAY (process->threads, thread_iter) {
      const lprof_thread_t *thread = ARRAY_GET_DATA (NULL, thread_iter);
      lua_pushnumber (L, thread->tid);                           // key
      push_thread (L, thread, display_functions, display_loops); // value
      lua_rawset (L, -3);
   }
   lua_rawset (L, -3); // set table of threads key to "threads"
}

/* Pushes a node object (as a table) on the LUA stack
 * This table contains all node-level data/results (including children=processes)
 */
static void push_node (lua_State *L, const lprof_node_t *node)
{
   lua_newtable (L); // node data

   // Rank (in parent context)
   lua_pushstring (L, "rank");     // key
   lua_pushnumber (L, node->rank); // value
   lua_rawset (L, -3);

   // Processes
   lua_pushstring (L, "processes"); // key
   lua_newtable (L);               // value: table of processes
   FOREACH_INARRAY (node->processes, process_iter) {
      const lprof_process_t *process = ARRAY_GET_DATA (NULL, process_iter);
      lua_pushnumber (L, process->pid); // key
      push_process (L, process);        // value
      lua_rawset (L, -3);
   }
   lua_rawset (L, -3); // set table of processes key to "processes"
}

/* Push on the LUA stack a 3D (node, process, thread) table.
 * Each element is pushed by a generic function passed as parameter */
static void push_context (lua_State *L, const sampling_display_context_t *context)
{
   unsigned i;
   lua_newtable (L); // context data

   // Executable name
   lua_pushstring (L, "executable name"); // key
   lua_pushstring (L, context->exe_name); // value
   lua_rawset (L, -3);

   // HW events list
   lua_pushstring (L, "events list");    // key
   lua_pushstring (L, context->ev_list); // value
   lua_rawset (L, -3);

   if (context->display_functions) {
      // Executable (application) functions
      lua_pushstring (L, "executable functions");
      lua_newtable (L); // table of functions
      for (i=0; i<context->nb_exe_fcts; i++) {
         lprof_fct_t *fct = &(context->exe_fcts [i]);
         lua_pushstring (L, fct->name); // key
         push_exe_fct (L, fct);         // value
         lua_rawset (L, -3); // set function data key
      }
      lua_rawset (L, -3); // set table of functions key to "executable functions"
   }

   if (context->display_loops) {
      // Executable (application) loops
      lua_pushstring (L, "executable loops");
      lua_newtable (L); // table of loops
      for (i=0; i<context->nb_exe_loops; i++) {
         lprof_loop_t *loop = &(context->exe_loops [i]);
         lua_pushnumber (L, loop->id); // key
         push_exe_loop (L, loop);      // value
         lua_rawset (L, -3); // set loop data key
      }
      lua_rawset (L, -3); // set table of loops key to "executable loops"
   }

   // Nodes
   lua_pushstring (L, "nodes"); // key
   lua_newtable (L);            // value: table of nodes
   FOREACH_INARRAY (context->nodes, node_iter) {
      const lprof_node_t *node = ARRAY_GET_DATA (NULL, node_iter);
      lua_pushstring (L, node->name); // key
      push_node (L, node);            // value
      lua_rawset (L, -3);
   }
   lua_rawset (L, -3); // set table of nodes key to "nodes"
}

// Push to LUA stack l_lprof_prepare_sampling_display outputs
int push_outputs (lua_State *L, sampling_display_context_t *context)
{
   push_context (L, context);

   return 1;
}

/* In libraries, look for the object (function/loop) containing a given address
 * (such as obj.start_addr <= addr <= obj.end_addr)
 */
static avlTree_t *search_obj_in_libraries (uint64_t addr, avlTree_t **lib_trees,
                                           const lprof_process_t *process,
                                           int display_type, int *lib_rank)
{
   if (addr <= 0x3000000) return NULL;

   unsigned i;
   const lprof_node_t *node = process->parent_node;
   lprof_library_t *libs = node->libsInfo.libraries;

   // For each library in the current node
   for (i=0; i < node->nb_libs; i++) {

      if (lib_trees[i] == NULL) continue; // skip libraries with no ranges info

      // Skip non-matching libraries
      if (addr >= libs[i].startMapAddress[process->map_rank] &&
          addr <= libs[i].stopMapAddress [process->map_rank]) {

         // libc/libld are mapped between 0x3000000000 and 0x4000000000: no need to offset
         if (addr <= 0x3000000000 || addr >= 0x4000000000)
            addr -= libs[i].startMapAddress[process->map_rank];

         avlTree_t *found = search_address (addr, lib_trees[i], display_type);
         *lib_rank = i;
         return found;
      }
   }

   return NULL;
}

/* Returns the object (function or loop), if any, related to the address for a given process
 * Common part to search_fct_from_addr and search_loop_from_addr => CF these functions
 */
static avlTree_t *search_obj (uint64_t addr, const lprof_process_t *process,
                              avlTree_t *exe_tree, avlTree_t *sys_tree, avlTree_t **lib_trees,
                              int display_type, int *lib_rank)
{
   // Search in executable functions/loops
   avlTree_t *found = search_address (addr - process->exe_offset, exe_tree, display_type);
   if (found != NULL) { *lib_rank = -1; return found; }

   // If not found in executable, search in libraries functions/loops
   found = search_obj_in_libraries (addr, lib_trees, process, display_type, lib_rank);
   if (found != NULL) return found;

   // If not found in libraries, search in system (kernel) functions
   if (sys_tree != NULL) {
      found = search_address (addr, sys_tree, display_type);
      if (found != NULL) *lib_rank = -2;
   }

   return found;
}

/* Returns the function (SinfoFunc), if any, related to the address for a given process
 * Function will be searched in executable (in user code and then in system code) and then in libraries
 */
static SinfoFunc *search_fct_from_addr (uint64_t addr, const lprof_process_t *process)
{
   SinfoFunc *cached = hashtable_lookup (process->ip2fct_cache, (void *) addr);
   if (cached) return cached;

   const lprof_node_t *node = process->parent_node;

   int lib_rank = -1;
   avlTree_t *found = search_obj (addr, process,
                                  node->exe_fcts_tree,
                                  node->sys_fcts_tree,
                                  node->libs_fcts_tree,
                                  PERF_FUNC, &lib_rank);

   if (found != NULL) {
      SinfoFunc *fct = found->value;
      fct->libraryIdx = lib_rank;
      hashtable_insert (process->ip2fct_cache, (void *) addr, fct);
      return fct;
   }

   hashtable_insert (process->ip2fct_cache, (void *) addr, process->parent_node->unknown_fcts);
   return process->parent_node->unknown_fcts;
}

/* Returns the loop (SinfoLoop), if any, related to the address for a given process
 * Loop will be searched in executable (in user code only, system code excluded) and then in libraries
 */
static SinfoLoop *search_loop_from_addr (uint64_t addr, const lprof_process_t *process)
{
   const lprof_node_t *node = process->parent_node;

   int lib_rank = -1;
   avlTree_t *found = search_obj (addr, process,
                                  node->exe_loops_tree,
                                  NULL,
                                  node->libs_loops_tree,
                                  PERF_LOOP, &lib_rank);

   if (found != NULL) {
      SinfoLoop *loop = found->value;
      loop->libraryIdx = lib_rank;
      return loop;
   }

   return NULL;
}

// Remark: does not cover 100% cases
static int select_fct_category (const char *fct_name)
{
   // MPI: Check if starts with MPI or xMPI (upper or lower case)
   const char *MPI_pos = strstr (fct_name, "MPI");
   if (!MPI_pos) MPI_pos = strstr (fct_name, "mpi");
   if (MPI_pos && (MPI_pos - fct_name < 2)) {
      DBGMSG ("Assuming %s as MPI\n", fct_name);
      return MPI_CATEGORY;
   }

   // MPI: Check if starts with PMI or xPMI (upper or lower case)
   const char *PMI_pos = strstr (fct_name, "PMI");
   if (!PMI_pos) PMI_pos = strstr (fct_name, "pmi");
   if (PMI_pos && (PMI_pos - fct_name < 2)) {
      DBGMSG ("Assuming %s as MPI\n", fct_name);
      return MPI_CATEGORY;
   }

   // Intel OpenMP: Check if starts with kmp or __kmp (lower case)
   const char *kmp_pos = strstr (fct_name, "__kmp");
   if (!kmp_pos) kmp_pos = strstr (fct_name, "kmp");
   if (kmp_pos == fct_name) {
      DBGMSG ("Assuming %s as OpenMP\n", fct_name);
      return OMP_CATEGORY;
   }

   // GNU OpenMP: Checks if starts with gomp or GOMP (lower or upper case)
   const char *gomp_pos = strstr (fct_name, "gomp");
   if (!gomp_pos) gomp_pos = strstr (fct_name, "GOMP");
   if (gomp_pos == fct_name) {
      DBGMSG ("Assuming %s as OpenMP\n", fct_name);
      return OMP_CATEGORY;
   }

   // MPC: Checks if starts with ompmpc
   const char *mpcomp_pos = strstr (fct_name, "mpcomp");
   if (!mpcomp_pos) mpcomp_pos = strstr (fct_name, "mpcomp");
   if (mpcomp_pos == fct_name) {
      DBGMSG ("Assuming %s as OpenMP\n", fct_name);
      return OMP_CATEGORY;
   }

   return -1;
}

// From IP callchains, returns the inferred category
static int get_category_from_callchains (const raw_IP_events_t *IP_events, lprof_thread_t *thread,
                                         const SinfoFunc *target_fct)
{
   // No callchains: nothing to do
   if (IP_events->nb_callchains == 0 || IP_events->callchains == NULL) return -1;

   unsigned i, j;
   const lprof_process_t *process = thread->parent_process;
   lprof_library_t *libraries = process->parent_node->libsInfo.libraries;

   for (i=0; i < IP_events->nb_callchains; i++) {
      const IP_callchain_t *callchain = &(IP_events->callchains[i]);

      for (j=0; j < callchain->nb_IPs; j++) {
         uint64_t ip = callchain->IPs[j];
         const SinfoFunc *fct = search_fct_from_addr (ip, process);

         if (!fct || fct == process->parent_node->unknown_fcts) {
            DBGMSGLVL (1, "[CALLCHAIN] ADDRESS %#"PRIx64" IS UNKNOWN\n", ip);
            continue;
         }
         
         // Skip target function
         if (strcmp (target_fct->name, fct->name) == 0) continue;

         // Check if current function called from a runtime
         if (fct->libraryIdx > -1) {
            int cat_ID = select_category (libraries [fct->libraryIdx].name, fct->name, NULL);
            if (cat_ID == MPI_CATEGORY || cat_ID == OMP_CATEGORY || cat_ID == PTHREAD_CATEGORY)
               return cat_ID;
         }
         int fct_name_cat = select_fct_category (fct->name);
         if (fct_name_cat == MPI_CATEGORY || fct_name_cat == OMP_CATEGORY)
            return fct_name_cat;
      }
   }
   
   return -1;   
}

// Returns (writes to a buffer) the string representation of a callchain
static void get_callchain_string (const IP_callchain_t *callchain, const lprof_thread_t *thread,
                                  const SinfoFunc *target_fct, char **buf, size_t *buf_len)
{
   lprof_process_t *const process = thread->parent_process;
   const unsigned callchain_filter = process->parent_node->parent_context->callchain_filter;

   unsigned i;
   unsigned nb_recursions = 1;
   const SinfoFunc *prv_fct = NULL;
   unsigned sample_type = 0;
   *buf[0] = '\0';
   
   for (i=0; i < callchain->nb_IPs; i++) {
      uint64_t ip = callchain->IPs[i];
      const SinfoFunc *fct = search_fct_from_addr (ip, process);

      if (!fct || fct == process->parent_node->unknown_fcts) {
         DBGMSGLVL (1, "[CALLCHAIN] ADDRESS %#"PRIx64" IS UNKNOWN\n", ip);
         continue;
      }
         
      // Skip target function
      if (strcmp (target_fct->name, fct->name) == 0) continue;
         
      // Excludes functions that need to be filtered out
      if (fct->libraryIdx == -1)
         sample_type = SAMPLE_TYPE_BINARY;
      else if (fct->libraryIdx == -2)
         sample_type = SAMPLE_TYPE_SYSTEM;
      else
         sample_type = SAMPLE_TYPE_LIBRARY;
      if (sample_type > callchain_filter) continue;

      // IP in same function as previous IP: increment recursion counter
      if (fct == prv_fct) {
         nb_recursions++;
         continue;
      }

      prv_fct = fct;

      // Appends function name to previous ones in buffer
      if (*buf[0] == '\0') {
         sprintf (*buf, "%.50s", fct->name);
      }
      else if (nb_recursions > 1) {
         size_t required_size = strlen (*buf) + strlen (" [x999999] <-- ") + 50 + 1;
         if (required_size > *buf_len) {
            *buf = lc_realloc (*buf, required_size);
            *buf_len = required_size;
         }
         sprintf (*buf, "%s [x%u] <-- %.50s", *buf, nb_recursions, fct->name);
      }
      else {
         size_t required_size = strlen (*buf) + strlen (" <-- ") + 50 + 1;
         if (required_size > *buf_len) {
            *buf = lc_realloc (*buf, required_size);
            *buf_len = required_size;
         }
         sprintf (*buf, "%s <-- %.50s", *buf, fct->name);
      }

      nb_recursions = 1;
   }

   // If last function from the callchain is recursive, we need to handle it here!
   if (nb_recursions > 1 && sample_type <= callchain_filter) {
      size_t required_size = strlen (*buf) + strlen (" [x999999]") + 1;
      if (required_size > *buf_len) {
         *buf = lc_realloc (*buf, required_size);
         *buf_len = required_size;
      }
      sprintf (*buf, "%s [x%u]", *buf, nb_recursions);
   }
}

// Inserts into the "callChainsInfo" hashtable data related to a given IP
static void insert_callChainsInfo (const raw_IP_events_t *IP_events, const lprof_thread_t *thread,
                                   const SinfoFunc *target_fct)
{
   if (IP_events->nb_callchains == 0 || IP_events->callchains == NULL) return; // no callchains

   lprof_process_t *const process = thread->parent_process;
   const unsigned callchain_filter = process->parent_node->parent_context->callchain_filter;
   if (callchain_filter == CALLCHAIN_FILTER_IGNORE_ALL) return; // callchains display disabled

   hashtable_t *callchains = target_fct->callChainsInfo [process->map_rank][thread->rank];
   if (callchains == NULL) {
      callchains = hashtable_new (str_hash, str_equal);
      target_fct->callChainsInfo [process->map_rank][thread->rank] = callchains;
   }

   unsigned i;
   size_t buf_len = 1000;
   char *buf = lc_malloc (buf_len);

   for (i=0; i < IP_events->nb_callchains; i++) {
      const IP_callchain_t *callchain = &(IP_events->callchains[i]);
      get_callchain_string (callchain, thread, target_fct, &buf, &buf_len);

      // Insertion in hashtable
      if (buf[0] != '\0') {
         uint64_t *nb_occurrences = hashtable_lookup (callchains, buf);
         if (nb_occurrences == NULL) {
            nb_occurrences = lc_malloc (sizeof *nb_occurrences);
            *nb_occurrences = callchain->nb_hits;
            hashtable_insert (callchains, lc_strdup (buf), nb_occurrences);
         } else {
            *nb_occurrences += callchain->nb_hits;
         }
         target_fct->totalCallChains [process->map_rank][thread->rank] += callchain->nb_hits;
         DBGMSGLVL (1, "CALLCHAIN : <%s> : OCCURRENCES = %"PRIu64"\n", buf, *nb_occurrences);
      }
   }

   lc_free (buf);
}

// From a sample, increment related category histograms for a function in a given thread
static void update_categories (const raw_IP_events_t *IP_events, lprof_thread_t *thread, SinfoFunc *fct)
{
   const lprof_process_t *process = thread->parent_process;
   const lprof_node_t    *node    = process->parent_node;
   const sampling_display_context_t *context = node->parent_context;
   const lprof_library_t *libs = node->libsInfo.libraries;

   unsigned cat_ID;
   if (fct->libraryIdx == -1) {
      int fct_cat = select_fct_category (fct->name);
      cat_ID = fct_cat != -1 ? fct_cat : BIN_CATEGORY;
   }
   else /* system or library */ {
      int callchain_cat_ID = get_category_from_callchains (IP_events, thread, fct);
      if (callchain_cat_ID != -1) /* system or library with better category found */
         cat_ID = callchain_cat_ID;
      else if (fct->libraryIdx >= 0) /* library (with no better category found) */
      {
         // search in libraries specified in "-lec" option (if any)
         cat_ID = (uint64_t) hashtable_lookup(context->libsExtraCat, strrchr(libs[fct->libraryIdx].name, '/')+1);
         if(!cat_ID) // avoid wrongly reassigning current sample to another category
            cat_ID = select_category (libs [fct->libraryIdx].name, fct->name, context->libc_fct_to_cat);
         if (cat_ID == OTHERS_CATEGORY) {
            int fct_cat = select_fct_category (fct->name);
            cat_ID = fct_cat != -1 ? fct_cat : OTHERS_CATEGORY;
         }
      }
      else /* system (with no better category found) */
         cat_ID = SYSTEM_CATEGORY;
   }

   thread->categories [cat_ID]         += IP_events->eventsNb[0];
   thread->categories [TOTAL_CATEGORY] += IP_events->eventsNb[0];
   
   // libc special case (don't understand...)
   if (fct->libraryIdx < 0) return;

   const char *lib_name = libs [fct->libraryIdx].name;
   if (strstr (lib_name, "libc.") || strstr (lib_name, "libc-")) {
      unsigned libc_cat_ID = (uint64_t) hashtable_lookup (context->libc_fct_to_cat, fct->name);
      if (libc_cat_ID > 0)
         thread->libc_categories [libc_cat_ID] += 1;
      else
         thread->libc_categories [LIBC_UNKNOWN_FCT] += 1;
      thread->libc_categories [LIBC_TOTAL_CATEGORY] += 1;
   }
}

// Increments/updates results of the function "hit" by a given sample
static void map_IP_to_function (lprof_thread_t *thread, const raw_IP_events_t *IP_events,
                                unsigned nb_threads, unsigned HW_evts_per_grp)
{
   const lprof_process_t *process = thread->parent_process;
   unsigned i;

   // Get related function part (start-end addresses range)
   SinfoFunc *fct_part = search_fct_from_addr (IP_events->ip, process);
   if (!fct_part || fct_part == process->parent_node->unknown_fcts) {
      fct_part = process->parent_node->unknown_fcts;
      for (i=0; i<HW_evts_per_grp; i++)
         fct_part->hwcInfo [process->map_rank][thread->rank][i] += IP_events->eventsNb[i];
      return;
   }

   // Insert to process library functions (if library and not already inserted)
   if (fct_part->libraryIdx >= 0 && hashtable_lookup (process->is_library, fct_part) == NULL)
      hashtable_insert (process->is_library, fct_part, fct_part);

   // Insert to process functions (if not already inserted: one per name+module)
   char *key = lc_malloc (strlen (fct_part->name) + strlen ("4000000000") + 1);
   sprintf (key, "%s%d", fct_part->name, fct_part->libraryIdx);
   SinfoFunc *f = hashtable_lookup (thread->fcts, key);
   if (f == NULL) {
      hashtable_insert (thread->fcts, key, fct_part);
      // key will be freed at hashtable_free
   } else {
      fct_part = f;
      lc_free (key);
   }

   // Increment events for current thread
   const sampling_display_context_t *context = process->parent_node->parent_context;
   init_sinfo_func_hwc (fct_part, process->map_rank, nb_threads, context->events_per_group);
   for (i=0; i<HW_evts_per_grp; i++)
      fct_part->hwcInfo [process->map_rank][thread->rank][i] += IP_events->eventsNb[i];

   // Update thread->categories/libc_categories (use callchains to refine categorization)
   update_categories (IP_events, thread, fct_part);

   // Allows callchains display
   insert_callChainsInfo (IP_events, thread, fct_part);
}

// Increments/updates results of the loop "hit" by a given sample
static void map_IP_to_loop (lprof_thread_t *thread, const raw_IP_events_t *IP_events,
                            unsigned nb_threads, unsigned HW_evts_per_grp)
{
   const lprof_process_t *process = thread->parent_process;
   unsigned i;

   // Get related loop part (start-end addresses range)
   SinfoLoop *loop_part = search_loop_from_addr (IP_events->ip, process);
   if (loop_part == NULL) return;

   // Insert to thread loops (if not already inserted: one per ID+module)
   lprof_node_t *node = process->parent_node;
   char *module_name;
   if (loop_part->libraryIdx > -1)
      module_name = node->libsInfo.libraries [loop_part->libraryIdx].name;
   else if (loop_part->libraryIdx == -2)
      module_name = "SYSTEM CALL";
   else
      module_name = node->parent_context->exe_name;

   hashtable_t *module_loops = hashtable_lookup (thread->loops, module_name);
   if (module_loops == NULL) {
      module_loops = hashtable_new (direct_hash, direct_equal);
      hashtable_insert (module_loops, (void *)(uint64_t) loop_part->loop_id, loop_part);
      hashtable_insert (thread->loops, module_name, module_loops);
   } else {
      SinfoLoop *l = hashtable_lookup (module_loops, (void *)(uint64_t) loop_part->loop_id);
      if (l == NULL) {
         hashtable_insert (module_loops, (void *)(uint64_t) loop_part->loop_id, loop_part);
      } else {
         loop_part = l;
      }
   }

   // Increment events for current thread
   const sampling_display_context_t *context = process->parent_node->parent_context;
   init_sinfo_loop_hwc (loop_part, process->map_rank, nb_threads, context->events_per_group);
   for (i=0; i<HW_evts_per_grp; i++)
      loop_part->hwcInfo [process->map_rank][thread->rank][i] += IP_events->eventsNb[i];
}

// Map (from instruction addresses) samples to executable/libraries functions/loops
static void map_process_samples_to_hotspots (const char *node_path, lprof_process_t *process)
{
   const lprof_node_t *node = process->parent_node;
   sampling_display_context_t *context = node->parent_context;

   // Load process samples from <process path>/IP_events.lprof
   char *const process_path = lc_malloc (strlen (node_path) + strlen ("999999") + 2);
   sprintf (process_path, "%s/%ld", node_path, process->pid);
   FILE *fp = fopen_in_directory (process_path, "IP_events.lprof", "rb");
   lc_free (process_path);
   if (!fp) {
      HLTMSG ("Cannot load events for %s\n", process_path);
      exit (-1);
   }
   TID_events_header_t TID_events_header;
   if (read_TID_events_header (fp, &TID_events_header) != 0) {
      ERRMSG ("Cannot read TID events header\n");
      fclose (fp);
      return;
   }
   unsigned evts_per_grp = TID_events_header.HW_evts_per_grp;
   if (!context->ev_list) {
      context->ev_list = strdup (TID_events_header.HW_evts_list);
      context->events_per_group = evts_per_grp;
   }

   // Initialize unknown functions data for this process
   init_sinfo_func_hwc (node->unknown_fcts, process->map_rank,
                        TID_events_header.nb_threads, evts_per_grp);
   raw_IP_events_t *IP_events = raw_IP_events_new (context->events_per_group);

   /* For each thread */
   unsigned thr_rank;
   for (thr_rank=0; thr_rank < TID_events_header.nb_threads; thr_rank++) {
      // Read header (description of IP events for this thread)
      uint64_t tid; unsigned IP_events_nb;
      if (read_IP_events_header (fp, &tid, &IP_events_nb) != 0) {
         ERRMSG ("Cannot read IP events header\n");
         break;
      }

      insert_thread_to_process (tid, process, evts_per_grp, context->nbExtraCat);
      lprof_thread_t *thread = array_get_last_elt (process->threads);
      hashtable_insert (thread->fcts, lc_strdup ("UNKNOWN FCTS"), node->unknown_fcts);
      memset (thread->events_nb, 0, evts_per_grp * sizeof thread->events_nb[0]);

      unsigned ip_rank;
      for (ip_rank=0; ip_rank < IP_events_nb; ip_rank++) {
         if (read_IP_events (fp, IP_events, evts_per_grp) != 0) {
            ERRMSG ("Cannot read IP events\n");
            fclose (fp);
            return;
         }

         // Increment nb occurrences
         unsigned i;
         for (i=0; i<evts_per_grp; i++)
            thread->events_nb [i] += IP_events->eventsNb[i];

         if (context->display_functions)
            map_IP_to_function (thread, IP_events, TID_events_header.nb_threads, evts_per_grp);

         if (context->display_loops)
            map_IP_to_loop (thread, IP_events, TID_events_header.nb_threads, evts_per_grp);
      }
   } // for each event

   fclose (fp);

   free_TID_events_header (&TID_events_header);
   raw_IP_events_free (IP_events);
}

static void free_thread (lprof_thread_t *thread)
{
   hashtable_free (thread->fcts, NULL, NULL);
   lc_free (thread->events_nb);
   lc_free (thread->categories);
   lc_free (thread->libc_categories);

   // Loops
   FOREACH_INHASHTABLE (thread->loops, module) {
      hashtable_t *module_loops = GET_DATA_T (hashtable_t *, module);
      hashtable_free (module_loops, NULL, NULL);
   }
   hashtable_free (thread->loops, NULL, NULL);
}

static void free_process (lprof_process_t *process)
{
   FOREACH_INARRAY (process->threads, thread_iter) {
      lprof_thread_t *const thread = ARRAY_GET_DATA (NULL, thread_iter);
      free_thread (thread);
   }
   array_free (process->threads, NULL);

   hashtable_free (process->is_library, NULL, NULL);
   hashtable_free (process->ip2fct_cache, NULL, NULL);
}

static void free_node (lprof_node_t *node)
{
   FOREACH_INARRAY (node->processes, process_iter) {
      lprof_process_t *const process = ARRAY_GET_DATA (NULL, process_iter);
      free_process (process);
   }
   array_free (node->processes, NULL);

   lc_free (node->unknown_fcts);
   lc_free (node->libs_fcts_tree);
   lc_free (node->libs_loops_tree);
}

/* Release memory allocated for the context.
 * Since it is hierarchical, all children will be "recursively" released */
void free_context (sampling_display_context_t *context)
{
   FOREACH_INARRAY (context->nodes, node_iter) {
      lprof_node_t *const node = ARRAY_GET_DATA (NULL, node_iter);
      free_node (node);
   }
   array_free (context->nodes, NULL);

   hashtable_free (context->libc_fct_to_cat, NULL, NULL);
}

static void add_pid (const char *host_path, const char *process_name, void *pids)
{
   (void) host_path; // Silent unused warning

   // Ignoring invalid name entries (expecting positive decimal number)
   char *endptr; if (strtol (process_name, &endptr, 10) < 1 || *endptr != '\0') return;
   array_add (pids, lc_strdup (process_name));
}

// Write to processes_index.lua pid value to rank mapping
// TODO: will probably be no more used with rewritten code...
static void write_processes_index (const char *exp_path, const char *hostname, void *data)
{
   (void) data; // silent unused parameter warning

   char *host_path = lc_malloc (strlen (exp_path) + strlen (hostname) + 2);
   sprintf (host_path, "%s/%s", exp_path, hostname);

   array_t *pids = array_new();
   for_each_directory_in_directory (host_path, add_pid, pids);

   if (array_length (pids) == 0) {
      lc_free (host_path);
      return;
   }

   // Open a file named "processes_index.lua"
   FILE *fp = fopen_in_directory (host_path, "processes_index.lua", "w");
   if (!fp) return;
   fprintf (fp, "pidToPidIdx = {};\n");

   // For each process
   unsigned pid_id = 1;
   FOREACH_INARRAY (pids, pids_iter) {
      const char *pid = ARRAY_GET_DATA (NULL, pids_iter);
      fprintf (fp, "pidToPidIdx[\"%s\"] = %u;\n", pid, pid_id++);
   }
   array_free (pids, lc_free);

   fclose (fp);
}

static void add_lib (const char *libs_path, const char *file_name, void *data)
{
   lprof_libraries_info_header_t metadata_header;
   lprof_libraries_info_t metadata;
   load_lib_metadata (libs_path, file_name, &metadata_header, &metadata);

   array_add ((array_t *) data, metadata.libraries);
}

static void add_process_ranges (const char *node_path, const char *process_id, void *data)
{
   // Ignoring invalid name entries (expecting positive decimal number)
   char *endptr; if (strtol (process_id, &endptr, 10) < 1 || *endptr != '\0') return;

   char process_path [strlen (node_path) + strlen (process_id) + 2];
   sprintf (process_path, "%s/%s", node_path, process_id);
   FILE *fp = fopen_in_directory (process_path, "lib_ranges.lprof", "rb");
   if (!fp) return;

   hashtable_t *process_ranges = hashtable_new (str_hash, str_equal);
   size_t nb_records;
   size_t name_len;
   while ((nb_records = fread (&name_len, sizeof name_len, 1, fp)) == 1) {
      // Read name
      char name [name_len+1];
      if (fread (name, name_len, 1, fp) != 1) break;
      name [name_len] = '\0';

      // Read start/stop addresses
      uint64_t start_addr, stop_addr;
      if (fread (&start_addr, sizeof start_addr, 1, fp) != 1) break;
      if (fread (&stop_addr , sizeof stop_addr , 1, fp) != 1) break;

      lib_range_t *lib_range = lc_malloc (sizeof *lib_range);
      lib_range->name = lc_strdup (name);
      lib_range->startMapAddress = start_addr;
      lib_range->stopMapAddress  = stop_addr;
      hashtable_insert (process_ranges, lib_range->name, lib_range);
   }

   hashtable_insert ((hashtable_t *) data, lc_strdup (process_id), process_ranges);
}

static void load_node_libs (sampling_display_context_t *context, lprof_node_t *node, const char *node_path)
{
   // Load structure of libraries
   array_t *libs = array_new();
   char libs_path [strlen (node_path) + strlen ("/libs") + 1];
   sprintf (libs_path, "%s/libs", node_path);
   for_each_file_in_directory (libs_path, add_lib, libs);

   // Load processes ranges in libraries
   hashtable_t *node_ranges = hashtable_new (str_hash, str_equal);
   for_each_directory_in_directory (node_path, add_process_ranges, node_ranges);
   
   node->nb_libs = array_length (libs);
   node->libs_fcts_tree = context->display_functions ?
      lc_malloc (node->nb_libs * sizeof node->libs_fcts_tree[0]) : NULL;
   node->libs_loops_tree = context->display_loops ?
      lc_malloc (node->nb_libs * sizeof node->libs_loops_tree[0]) : NULL;

   lprof_library_t *concat_libs = lc_malloc (node->nb_libs * sizeof concat_libs[0]);

   // Complete libraries address ranges and trees
   unsigned lib_rank = 0;
   FOREACH_INARRAY (libs, libs_iter) {
      lprof_library_t *lib = ARRAY_GET_DATA (lprof_library_t *, libs_iter);

      lib->nbProcesses = array_length (node->processes);

      // Set ranges
      lib->startMapAddress = lc_realloc (lib->startMapAddress, lib->nbProcesses * sizeof lib->startMapAddress[0]);
      lib->stopMapAddress  = lc_realloc (lib->stopMapAddress , lib->nbProcesses * sizeof lib->stopMapAddress [0]);

      unsigned process_rank = 0;
      FOREACH_INARRAY (node->processes, process_iter) {
         lprof_process_t *process = ARRAY_GET_DATA (NULL, process_iter);

         // Get address ranges for current process
         char process_id [strlen ("999999") + 1];
         sprintf (process_id, "%ld", process->pid);
         const hashtable_t *ranges = hashtable_lookup (node_ranges, process_id);
         if (ranges != NULL) {
            FOREACH_INHASHTABLE (ranges, range_iter) {
               const char *name = GET_KEY (char *, range_iter);
               if (strcmp (lib->name, name) == 0) {
                  lib_range_t *lib_range = GET_DATA_T (lib_range_t *, range_iter);
                  lib->startMapAddress [process_rank] = lib_range->startMapAddress;
                  lib->stopMapAddress  [process_rank] = lib_range->stopMapAddress;
               }
            }
         }
         process_rank++;
      }

      // Set trees
      if (context->display_functions)
         node->libs_fcts_tree [lib_rank] = get_fcts_tree (lib->nbFunctions, lib->fctsInfo, lib->nbProcesses);
      if (context->display_loops)
         node->libs_loops_tree[lib_rank] = get_loops_tree (lib->nbLoops, lib->loopsInfo, lib->nbProcesses);

      // Save updated library
      concat_libs [lib_rank] = *lib;
      lc_free (lib);

      lib_rank++;
   }
   array_free (libs, NULL);

   // Free node_ranges
   FOREACH_INHASHTABLE (node_ranges, nr_iter) {
      hashtable_t *process_ranges = GET_DATA_T (hashtable_t *, nr_iter);
      FOREACH_INHASHTABLE (process_ranges, pr_iter) {
         lib_range_t *lib_range = GET_DATA_T (lib_range_t *, pr_iter);
         lc_free (lib_range->name);
         lc_free (lib_range);
      }
      hashtable_free (process_ranges, NULL, NULL);
   }
   hashtable_free (node_ranges, NULL, lc_free);
   
   node->libsInfo.libraries = concat_libs;
}

// Refactored function to prepare sampling display, deprecating processing_parallel_new
void prepare_sampling_display (sampling_display_context_t *context) {
   // Load executable metadata (from exp_path/binary.lprof)
   lprof_header_t lprof_header; lprof_binary_info_header_t exe_metadata_header; lprof_binary_info_t exe_metadata;
   load_exe_metadata (context->exp_path, &lprof_header, &exe_metadata_header, &exe_metadata);
   strcpy (context->lprof_version, lprof_header.version);
   context->exe_name     = exe_metadata_header.binName;
   context->nb_exe_fcts  = exe_metadata_header.nbFunctions;
   context->nb_exe_loops = exe_metadata_header.nbLoops;
   context->exe_fcts     = exe_metadata.functions;
   context->exe_loops    = exe_metadata.loops;

   // For all nodes, write processes index (list of PIDs)
   for_each_directory_in_directory (context->exp_path, write_processes_index, NULL);

   // Get directory structure
   insert_nodes_and_processes_to_context (context);

   // Load libc functions categories
   context->libc_fct_to_cat = hashtable_new (str_hash, str_equal);
   load_libc_functions (context->libc_fct_to_cat);

   // create hashtable of libraries to consider as extra categories (if any)
   context->libsExtraCat = hashtable_new(str_hash, str_equal);
   int extraCatID = NB_CATEGORIES; // = 11: 0-9 used for "standard" categories, 10 is "total" (special) category
   char *lecLibs = lc_strdup(context->lecLibs); // context->lecLibs might be needed elsewhere: work w/ a copy
   char *libToken = strtok(lecLibs, ",");
   while(libToken != NULL)
   {
      // insert (key=libToken, value=extraCatID) into hashtable
      hashtable_insert(context->libsExtraCat, lc_strdup(libToken), (void *)(int64_t) extraCatID);
      // get next token (i.e. lib) + update extraCatID
      libToken = strtok(NULL, ",");
      extraCatID++;
   }
   lc_free(lecLibs);
   // get number of extra categories
   context->nbExtraCat = hashtable_size(context->libsExtraCat);

   // Load executable ranges and libraries metadata and ranges
   // For each node
   FOREACH_INARRAY (context->nodes, node_iter) {
      lprof_node_t *const node = ARRAY_GET_DATA (NULL, node_iter);

      char *const node_path = lc_malloc (strlen (context->exp_path) + strlen (node->name) + 2);
      sprintf (node_path, "%s/%s", context->exp_path, node->name);
      const unsigned nb_processes = array_length (node->processes);

      // Get ranges trees for executable
      if (context->display_functions)
         node->exe_fcts_tree = get_fcts_tree (context->nb_exe_fcts, exe_metadata.functions,
                                              nb_processes);
      if (context->display_loops)
         node->exe_loops_tree = get_loops_tree (context->nb_exe_loops, exe_metadata.loops,
                                                nb_processes);

      // Load libraries metadata for this node (from node_path/libs/*.lprof)
      load_node_libs (context, node, node_path);

      // Get ranges trees for system calls (from node_path/system_map)
      node->sys_fcts_tree = load_system_maps (node_path, nb_processes);

      // For each process, map samples to hotspots
      FOREACH_INARRAY (node->processes, processes_iter) {
         lprof_process_t *const process = ARRAY_GET_DATA (NULL, processes_iter);

         map_process_samples_to_hotspots (node_path, process);
      }

      // Free hotspots trees
      destroy (node->exe_fcts_tree);
      destroy (node->exe_loops_tree);
      destroy (node->sys_fcts_tree);
      unsigned i;
      for (i=0; i < node->nb_libs; i++) {
         if (context->display_functions) destroy (node->libs_fcts_tree  [i]);
         if (context->display_loops    ) destroy (node->libs_loops_tree [i]);
      }

      lc_free (node_path);
   } // for each node
}
