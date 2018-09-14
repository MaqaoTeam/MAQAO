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

/* CF prepare_sampling_display.c
 * Defines a hierarchical structure to save nodes/processes/threads results
 */

#ifndef __PREPARE_SAMPLING_DISPLAY_H__
#define __PREPARE_SAMPLING_DISPLAY_H__

#include <stdint.h>
#include "libmcommon.h" // array_t...
#include "avltree.h" // SinfoFunc/Loop, avlTree_t
#include "binary_format.h" // lprof_loop_t...

// lua_State
#include <lua.h>
#include <lualib.h>

/* Tree-structured data used to display samples
 * Global context => nodes => processes => threads */

// Global (shared by all nodes) context, references nodes
typedef struct {
   const char *exp_path;
   array_t *nodes;

   // Parameters
   boolean_t display_functions;
   boolean_t display_loops;
   unsigned callchain_filter;
   const char *hwc_mode; // "maqao_events", "timer" or "maqao_custom"
   float cpu_freq; // CPU nominal frequency (grep GHz /proc/cpuinfo)
   float ref_freq; // reference frequency: reference cycles per second
   unsigned sampling_period;
   boolean_t show_sample_val;
   boolean_t ext_mode;

   // Executable data
   // TODO: consider this at process level
   char lprof_version [MAQAO_LPROF_VERSION_SIZE];
   char *exe_name;
   uint32_t nb_exe_fcts, nb_exe_loops;
   lprof_fct_t *exe_fcts;
   lprof_loop_t *exe_loops;

   // Misc.
   float base_clk;
   char *ev_list;
   unsigned events_per_group;
   hashtable_t *libc_fct_to_cat;
   const char *lecLibs; // libraries specified in "-lec" option
   hashtable_t *libsExtraCat; // key = library name, value = category ID
   unsigned int nbExtraCat; // number of elements in "libsExtraCat" hashtable
} sampling_display_context_t;

// Nodes, references processes
typedef struct {
   char *name;
   unsigned rank; // discover order in experiment directory, starts at 0
   sampling_display_context_t *parent_context;

   array_t *processes;

   SinfoFunc *unknown_fcts; // virtual function

   // Addresses for functions/loops in executable
   avlTree_t *exe_fcts_tree;
   avlTree_t *exe_loops_tree;

   // Libraries data
   uint32_t nb_libs;
   lprof_libraries_info_t libsInfo;

   // Addresses for functions/loops in libraries
   avlTree_t **libs_fcts_tree;
   avlTree_t **libs_loops_tree;

   // Addresses for system functions
   avlTree_t *sys_fcts_tree;
} lprof_node_t;

// Process, references threads
typedef struct {
   long pid;
   unsigned map_rank; // rank in libs[].start/stopMapAddress[]
   lprof_node_t *parent_node;

   array_t *threads;

   hashtable_t *is_library;
   uint64_t exe_offset;

   hashtable_t *ip2fct_cache; // cache to reduce calls to search_fct_from_address
} lprof_process_t;

// Thread (leaf)
typedef struct {
   long tid;
   unsigned rank; // discover order in samples.lprof (process-relative), starts at 0
   lprof_process_t *parent_process;

   hashtable_t *fcts; // table [<function name>] = lprof function (SinfoFunc)
   hashtable_t *loops; // table [<thread ID .. loop ID>] = lprof loop [SinfoLoop)

   uint64_t *events_nb; // as many elements as events/group
   unsigned *categories;
   unsigned *libc_categories;
} lprof_thread_t;

// Refactored function to prepare sampling display, deprecating processing_parallel_new
void prepare_sampling_display (sampling_display_context_t *context);

// Push to LUA stack l_lprof_prepare_sampling_display outputs
int push_outputs (lua_State *L, sampling_display_context_t *context);

/* Release memory allocated for the context */
void free_context (sampling_display_context_t *context);

#endif // __PREPARE_SAMPLING_DISPLAY_H__
