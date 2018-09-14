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
 * Defines LUA to C wrappers to expose lprof core functions (in C) to LUA (lprof tool or API)
 * TODO: this file should define only wrappers, few C lines to get parameters from the LUA stack
 * and forward them to a C function defined elsewhere...
 * TODO: remove duplicate includes
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "libmasm.h" // project_t, proc_get_uarch...
#include "sampling_engine.h"
#include "sampling_engine_shared.h" // SAMPLING_ENGINE_PTRACE...

#include "generate_metafile.h"
#include "prepare_sampling_display.h"
#include "abstract_objects_c.h" // p_t, PROJECT="project"

#include "deprecated_shared.h"

//Declaring micro-architecture identifiers
#include "arch.h" 

/* Entry point for the "collect" step of lprof in sampling mode:
 * "timer" => (new) sampling using system timers
 * "sampling" => deprecated sampling engine, ptrace-based
 * "sampling inherit" => (new) inherit-based sampling engine
 * "sampling ptrace" => refactored ptrace-based sampling engine
 */
static int l_lprof_launch (lua_State * L)
{
   //MAIN
   const char* bin = luaL_checkstring(L,1);
   const char* output_path = luaL_checkstring(L,2);
   const int sampling_period = lua_tonumber(L,3);
   const char* hwc_list = luaL_checkstring(L,4);
   const int user_guided  = lua_tonumber(L,5);
   const int backtrace_mode = lua_tonumber(L,6);
   const char* mode = luaL_checkstring(L,7);
   const char* cpu_list = luaL_checkstring(L,8);
   const char* mpi_target = luaL_checkstring(L,9);
   const int nb_sampler_threads = lua_tonumber(L,10);
   const boolean_t verbose = lua_toboolean(L,11);
   const int finalize_signal = lua_tonumber(L,12);
   const int max_buf_MB = lua_tonumber(L,13);
   const int files_buf_MB = lua_tonumber(L,14);
   const int max_files_MB = lua_tonumber(L,15);
   returnInfo_t retInfo;

   if (strcmp (mode, "sampling inherit") == 0)
      retInfo = sample (bin, output_path, sampling_period,
                        hwc_list, user_guided, backtrace_mode,
                        strlen (cpu_list) ? cpu_list : NULL,
                        strlen (mpi_target) ? mpi_target : NULL,
                        nb_sampler_threads, SAMPLING_ENGINE_INHERIT, TRUE, finalize_signal, verbose, max_buf_MB, files_buf_MB, max_files_MB);
   else if (strcmp (mode, "sampling ptrace sync") == 0)
      retInfo = sample (bin, output_path, sampling_period,
                        hwc_list, user_guided, backtrace_mode,
                        strlen (cpu_list) ? cpu_list : NULL,
                        strlen (mpi_target) ? mpi_target : NULL,
                        nb_sampler_threads, SAMPLING_ENGINE_PTRACE, TRUE, finalize_signal, verbose, max_buf_MB, files_buf_MB, max_files_MB);
   else if (strcmp (mode, "sampling ptrace async") == 0)
      retInfo = sample (bin, output_path, sampling_period,
                        hwc_list, user_guided, backtrace_mode,
                        strlen (cpu_list) ? cpu_list : NULL,
                        strlen (mpi_target) ? mpi_target : NULL,
                        nb_sampler_threads, SAMPLING_ENGINE_PTRACE, FALSE, finalize_signal, verbose, max_buf_MB, files_buf_MB, max_files_MB);
   else if (strcmp (mode, "sampling timers") == 0)
      retInfo = sample (bin, output_path, sampling_period,
                        hwc_list, user_guided, backtrace_mode,
                        strlen (cpu_list) ? cpu_list : NULL,
                        strlen (mpi_target) ? mpi_target : NULL,
                        nb_sampler_threads, SAMPLING_ENGINE_TIMERS, FALSE, finalize_signal, verbose, max_buf_MB, files_buf_MB, max_files_MB);

   lua_pushnumber(L,retInfo.pid);
   lua_pushstring(L,retInfo.hostname);
   return 2;
}

/* Returns the reference frequency, that is number of reference cycles per second */
static float get_ref_freq (unsigned arch, unsigned uarch)
{
   switch (arch) {
   }

   return 0.0f;
}

// TODO: improve this with refactoring
static int l_lprof_get_reference_frequency (lua_State * L)
{
   proc_t *proc = utils_get_proc_host (NULL);
   uarch_t *uarch = proc_get_uarch (proc);
   int arch_code = arch_get_code (uarch_get_arch (uarch));
   int uarch_id = uarch_get_id (uarch);

   lua_pushnumber (L, get_ref_freq (arch_code, uarch_id));

   return 1;
}

static int l_lprof_gc(lua_State *L) {
   (void)L;
   return 0;
}

static int l_lprof_tostring(lua_State *L) {
   lua_pushfstring(L,"Lprof Library Object");
   return 1;
}

/* Writes metafiles in binary format (new, refactored implementation)
 * TODO: move most of code to a function named generate_metafile defined in generate_metafile.c
 */
static int l_lprof_generate_metafile_binformat_new (lua_State *L)
{
   // Get arguments from LUA
   const char *exp_path    = lua_tostring    (L, 1);
   const char *host_path   = lua_tostring    (L, 2);
   const pid_t pid         = luaL_checknumber(L, 3);
   const char *exe_name    = lua_tostring    (L, 4);
   const char *disass_list = lua_tostring    (L, 5);
   const p_t  *proj        = luaL_checkudata (L, 6, PROJECT);

   generate_metafile_binformat_new (exp_path, host_path, pid, exe_name, disass_list, proj->p);

   return 0;
}

// Refactored function to prepare sampling display, deprecating processing_parallel_new
static int l_lprof_prepare_sampling_display (lua_State *L)
{
   sampling_display_context_t context;
   memset (&context, 0, sizeof (context));

   // Get arguments from LUA stack
   context.exp_path          = lua_tostring     (L, 1); // experiment path
   context.display_functions = lua_toboolean    (L, 2); // display functions
   context.display_loops     = lua_toboolean    (L, 3); // display loops
   context.callchain_filter  = luaL_checknumber (L, 4); // allow to bypass cc-based categorization
   context.hwc_mode          = luaL_checkstring (L, 5); // "maqao_events", "timer" or "custom"
   context.cpu_freq          = luaL_checknumber (L, 6);
   context.ref_freq          = luaL_checknumber (L, 7);
   context.sampling_period   = luaL_checknumber (L, 8);
   context.show_sample_val   = lua_toboolean    (L, 9);
   context.ext_mode          = lua_toboolean    (L, 10);
   context.lecLibs           = luaL_checkstring (L, 11); // libraries to consider as extra categories

   prepare_sampling_display (&context);

   // Push outputs to LUA stack
   int outputs_nb = push_outputs (L, &context);

   // Free memory
   free_context (&context);

   return outputs_nb;
}

static const luaL_reg lprof_methods[] = {
   { "launch",l_lprof_launch },
   { "get_reference_frequency",l_lprof_get_reference_frequency },
   { "prepare_sampling_display",l_lprof_prepare_sampling_display },
   { "generate_metafile_binformat_new", l_lprof_generate_metafile_binformat_new },
   { NULL,NULL }
};

static const luaL_reg lprof_meta[] = {
   { "__gc", l_lprof_gc},
   { "__tostring",l_lprof_tostring},
   {NULL,NULL}
};

/***************************************************************\
|*       Library registration                                   *|
\*_____________________________________________________________*/

typedef struct
{
   const luaL_reg *methods;
   const luaL_reg *meta;
   const char *id;
} bib_t;

static const bib_t bibs[] = {
   {lprof_methods, lprof_meta, "lprof"},
   {NULL, NULL, NULL}
};

int luaopen_lprof_c(lua_State *L)
{
   const bib_t *b;

   DBGMSG0("Registering LPROF module\n");
   for (b = bibs; b->id != NULL; b++)
   {
      luaL_register (L, b->id, b->methods);
      luaL_newmetatable (L, b->id);
      luaL_register (L, 0, b->meta);
      lua_pushliteral (L, "__index");
      lua_pushvalue (L, -3);
      lua_rawset (L, -3);
      lua_pushliteral (L, "__metatable");
      lua_pushvalue (L, -3);
      lua_rawset (L, -3);
      lua_pop (L, 1);
   }

   return 1;
}
