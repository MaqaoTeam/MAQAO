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
   See LUADOC documentation of this file
   This file defines stub (hook, wrapper...) functions to allow using C MAQAO functions in LUA for the following abstract objects:
   - project (set of asmfiles)
   - asmfile (disassembled binary file)
   - fct (function)
   - loop
   - group (set of instructions)
   - block (basic block, sequence of instructions)
   - insn (instruction)
   - dwarf (debugging information)
   - graph, edge
 */

#include <stdlib.h>
#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif
#include <stdint.h> /* int64_t... */
#include <string.h> /* strcmp... */
#include <math.h>   /* log2 */
#include <inttypes.h>
#include <stdio.h>

#include <sys/ioctl.h>




/* LUA specific includes */
#include <lua.h>     /* declares lua_*  functions */
#include <lauxlib.h> /* declares luaL_* functions */

/* MAQAO specific includes */
#include "libmmaqao.h"
#include "abstract_objects_c.h"
#include "archinterface.h"

/**
 * The 7 following functions create the LUA version of the abstract objects.
 * It is not easy to factor them because of the second argument of lua_newuserdata,
 * that must be determined statically for performance reasons
 * TODO: factor these functions without significantly impacting performance
 */

p_t *create_project(lua_State * L, project_t *project, int must_be_freed)
{
   p_t *ptr = lua_newuserdata(L, sizeof *ptr);

   luaL_getmetatable(L, PROJECT);
   lua_setmetatable(L, -2);
   ptr->p = project;
   ptr->must_be_freed = must_be_freed;
   ptr->uarch_name = NULL;

   return ptr;
}

a_t *create_asmfile(lua_State * L, asmfile_t *asmfile)
{
   a_t *ptr = lua_newuserdata(L, sizeof *ptr);

   luaL_getmetatable(L, ASMFILE);
   lua_setmetatable(L, -2);
   ptr->p = asmfile;

   return ptr;
}

l_arch_t *create_arch(lua_State * L, arch_t *arch)
{
   l_arch_t *ptr = lua_newuserdata(L, sizeof *ptr);

   luaL_getmetatable(L, ARCH);
   lua_setmetatable(L, -2);
   ptr->p = arch;

   return ptr;
}

l_uarch_t *create_uarch(lua_State * L, uarch_t *uarch)
{
   l_uarch_t *ptr = lua_newuserdata(L, sizeof *ptr);

   luaL_getmetatable(L, UARCH);
   lua_setmetatable(L, -2);
   ptr->p = uarch;

   return ptr;
}

l_proc_t *create_proc(lua_State * L, proc_t *proc)
{
   l_proc_t *ptr = lua_newuserdata(L, sizeof *ptr);

   luaL_getmetatable(L, PROC);
   lua_setmetatable(L, -2);
   ptr->p = proc;

   return ptr;
}

f_t *create_function(lua_State * L, fct_t *function)
{
   f_t *ptr = lua_newuserdata(L, sizeof *ptr);

   luaL_getmetatable(L, FUNCTION);
   lua_setmetatable(L, -2);
   ptr->p = function;

   return ptr;
}

l_t *create_loop(lua_State * L, loop_t *loop)
{
   l_t *ptr = lua_newuserdata(L, sizeof *ptr);

   luaL_getmetatable(L, LOOP);
   lua_setmetatable(L, -2);
   ptr->p = loop;

   return ptr;
}

g_t *create_group(lua_State * L, group_t *group)
{
   g_t *ptr = lua_newuserdata(L, sizeof *ptr);

   luaL_getmetatable(L, GROUP);
   lua_setmetatable(L, -2);
   ptr->p = group;

   return ptr;
}

b_t *create_block(lua_State * L, block_t *block)
{
   b_t *ptr = lua_newuserdata(L, sizeof *ptr);

   luaL_getmetatable(L, BLOCK);
   lua_setmetatable(L, -2);
   ptr->p = block;

   return ptr;
}

i_t *create_insn(lua_State * L, insn_t *insn)
{
   i_t *ptr = lua_newuserdata(L, sizeof *ptr);

   luaL_getmetatable(L, INSN);
   lua_setmetatable(L, -2);
   ptr->p = insn;

   return ptr;
}

static int get_userdata_address(lua_State *L)
{
   void *ptr = lua_touserdata(L, 1);

   if (ptr != NULL) {
      /* For the cast, any type is OK as soon as it is a structure with a 'p' pointer field */
      lua_pushinteger(L, (lua_Integer) ((i_t *) ptr)->p);

      return 1;
   }

   return 0;
}

/* TODO: move this to plugins/built_in/classes stubs */
static int get_host_uarch(lua_State *L)
{
   /**\todo (2016-12-05) Rewrite this function and all functions invoking it to make full use of the
    * new uarch_t and proc_t structures. Right now this version ensures compatibility with what existed
    * before, so it only returns identifiers.*/
   proc_t* proc = utils_get_proc_host();
   uarch_t* uarch = proc_get_uarch(proc);

   if (uarch != NULL) {
      lua_pushinteger(L, uarch_get_id(uarch));
      lua_pushinteger(L, arch_get_code(uarch_get_arch(uarch)));
   } else {
      lua_pushnil(L);
      lua_pushnil(L);
   }

   return 2;
}

static int get_host_proc(lua_State *L)
{
   proc_t* proc = utils_get_proc_host();
   if (proc != NULL) {
      create_proc(L, proc);
   } else {
      lua_pushnil(L);
   }
   return 1;
}

static int get_arch_by_name(lua_State* L)
{
   char* arch_name = (char*)luaL_checkstring(L, 1);
   arch_t* arch = getarch_byname(arch_name);
   if (arch != NULL) {
      create_arch(L, arch);
      return 1;
   }
   return 0;
}

static int get_file_arch(lua_State* L)
{
   char* file_name = (char*)luaL_checkstring(L, 1);
   arch_t* arch = file_get_arch(file_name);
   if (arch != NULL) {
      create_arch(L, arch);
      return 1;
   }
   return 0;
}

static int get_archs_list(lua_State* L)
{
   int i = 1;
   arch_t** arch = MAQAO_archs;;
   lua_newtable(L);
   if (*arch != NULL) {
      while (*arch != NULL) {
         create_arch(L, *arch++);
         lua_rawseti(L, -2, i++);
      }
      return 1;
   } else
      return 0;
}

static int demangle_string(lua_State *L)
{
   const char* str = luaL_checkstring(L, 1);
   char* demstr = fct_demangle(str, COMP_ERR, LANG_ERR);

   if (demstr == NULL)
      lua_pushnil(L);
   else
      lua_pushstring(L, demstr);
   return (1);
}

/** Returns a formatted timestamp, including the microseconds*/
static int gen_timestamp(lua_State *L)
{
   char str[256];
   int res = generate_timestamp(str, 256);
   if (res == EXIT_SUCCESS)
      lua_pushstring(L, str);
   else
      lua_pushnil(L);
   return 1;
}

static int maqao_sleep (lua_State *L)
{
   int nb = luaL_checkinteger(L, 1);
   if (nb > 0)
#ifdef _WIN32
      Sleep (nb);
#else
      sleep (nb);
#endif

   return 0;
}

static int maqao_wait_SIGINT (lua_State *L)
{
   int ret = -1;
   int delay = luaL_checkinteger (L, 1);
   if (delay > 0)
#ifdef _WIN32
   {
      size_t len = strlen ("ping -n  127.0.0.1 >nul") + strlen ("99999") + 1;
      char cmd [len];
      snprintf (cmd, len, "ping -n %d 127.0.0.1 >nul", delay);
      ret = system (cmd);
   }
#else
   {
      size_t len = strlen ("sleep ") + strlen ("99999") + 1;
      char cmd [len];
      snprintf (cmd, len, "sleep %d", delay);
      ret = system (cmd);
   }
#endif

   lua_pushinteger(L, ret);
   return 1;
}

static int maqao_isatty (lua_State *L)
{
   luaL_checktype(L, 1, LUA_TUSERDATA);
   FILE* f = *(FILE**) luaL_checkudata (L, 1, "FILE*");

   lua_pushinteger(L, isatty (fileno (f)));

   return 1;
}

static int maqao_get_term_size (lua_State *L)
{
   struct winsize w;
   ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

   lua_pushinteger(L, w.ws_col);
   lua_pushinteger(L, w.ws_row);

   return 2;
}

static int adapt_text_length (lua_State *L)
{
   const char* txt    = luaL_checkstring(L, 1);
   char* text         = strdup (txt);
   int max_size       = luaL_checkint(L, 2);
   int i              = 0;
   int cpt            = 0;
   int prev           = 0;

   while (text[i] != '\0')
   {
      if (text[i] == '\n')
      {
         cpt = 0;
         i++;
         continue;
      }
      if (cpt >= max_size)
      {
         int j = i;
         while (j >= 0 && j > prev && text[j] != ' ')
            j --;
         if (j > 0 && text[j] == ' ')
         {
            if (cpt == max_size)
            {
               text[j] = '\n';
               cpt = i - j;
               prev = j;
            }
            else
            {
               cpt = i % max_size;
            }
         }
      }
      cpt ++;
      i ++;
   }

   lua_pushstring(L, text);
   return 1;
}


void ao_init_help(lua_State *L, help_t* help)
{
   if (help == NULL)
      return;
   int i = 1;

   lua_newtable(L);

   lua_pushliteral(L, "_name");
   if (help->program != NULL)
      lua_pushstring(L, help->program);
   else
      lua_pushliteral(L, "");
   lua_settable(L, -3);

   lua_pushliteral(L, "_usage");
   if (help->usage != NULL)
      lua_pushstring(L, help->usage);
   else
      lua_pushliteral(L, "");
   lua_settable(L, -3);

   lua_pushliteral(L, "_description");
   if (help->description != NULL)
      lua_pushstring(L, help->description);
   else
      lua_pushliteral(L, "");
   lua_settable(L, -3);

   lua_pushliteral(L, "_copyright");
   if (help->copyright != NULL)
      lua_pushstring(L, help->copyright);
   else
      lua_pushliteral(L, "");
   lua_settable(L, -3);

   lua_pushliteral(L, "_bug");
   if (help->bugs != NULL)
      lua_pushstring(L, help->bugs);
   else
      lua_pushliteral(L, "");
   lua_settable(L, -3);

   lua_pushliteral(L, "_date");
   if (help->date != NULL)
      lua_pushstring(L, help->date);
   else
      lua_pushliteral(L, "");
   lua_settable(L, -3);

   lua_pushliteral(L, "_version");
   if (help->version != NULL)
      lua_pushstring(L, help->version);
   else
      lua_pushliteral(L, "");
   lua_settable(L, -3);

   lua_pushliteral(L, "_build");
   if (help->build != NULL)
      lua_pushstring(L, help->build);
   else
      lua_pushliteral(L, "");
   lua_settable(L, -3);

   lua_pushliteral(L, "_authors");
   lua_newtable(L);
   if (help->author != NULL) {
      lua_pushstring(L, help->author);
      lua_rawseti(L, -2, 1);
   }
   lua_settable(L, -3);

   lua_pushliteral(L, "_options");
   lua_newtable(L);
   if (help->options != NULL) {
      for (i = 0; i < help->size_options; i++) {
         option_t* opt = help->options[i];
         lua_newtable(L);

         if (opt->type == _HELPTYPE_OPT) {
            lua_pushliteral(L, "short");
            if (opt->shortname != NULL)
               lua_pushstring(L, opt->shortname);
            else
               lua_pushliteral(L, "");
            lua_settable(L, -3);

            lua_pushliteral(L, "long");
            if (opt->longname != NULL)
               lua_pushstring(L, opt->longname);
            else
               lua_pushliteral(L, "");
            lua_settable(L, -3);

            lua_pushliteral(L, "desc");
            if (opt->desc != NULL)
               lua_pushstring(L, opt->desc);
            else
               lua_pushliteral(L, "");
            lua_settable(L, -3);

            lua_pushliteral(L, "arg");
            if (opt->arg != NULL)
               lua_pushstring(L, opt->arg);
            else
               lua_pushliteral(L, "");
            lua_settable(L, -3);

            lua_pushliteral(L, "is_opt");
            lua_pushboolean(L, opt->is_arg_opt);
            lua_settable(L, -3);
         } else if (opt->type == _HELPTYPE_SEP) {
            lua_pushliteral(L, "name");
            lua_pushstring(L, opt->longname);
            lua_settable(L, -3);

            lua_pushliteral(L, "is_sep");
            lua_pushboolean(L, TRUE);
            lua_settable(L, -3);
         }
         lua_rawseti(L, -2, i + 1);
      }
   }
   lua_settable(L, -3);

   lua_pushliteral(L, "_examples");
   lua_newtable(L);
   if (help->examples != NULL) {
      for (i = 0; i < help->size_examples; i++) {
         lua_newtable(L);
         lua_pushliteral(L, "cmd");
         lua_pushstring(L, help->examples[i][_EXAMPLE_CMD]);
         lua_settable(L, -3);
         lua_pushliteral(L, "desc");
         lua_pushstring(L, help->examples[i][_EXAMPLE_DESC]);
         lua_settable(L, -3);
         lua_rawseti(L, -2, i + 1);
      }
   }
   lua_settable(L, -3);
}



/******************************************************************/
/*                Library Creation                                */
/******************************************************************/

typedef struct {
   const luaL_reg *methods;
   const luaL_reg *meta;
   const char     *id;
} bib_t;

static const bib_t bibs[] = {
      {project_methods , project_meta , PROJECT},
      {asmfile_methods , asmfile_meta , ASMFILE},
      {arch_methods    , arch_meta    , ARCH},
      {uarch_methods   , uarch_meta   , UARCH},
      {proc_methods    , proc_meta    , PROC},
      {function_methods, function_meta, FUNCTION},
      {loop_methods    , loop_meta    , LOOP},
      {block_methods   , block_meta   , BLOCK},
      {insn_methods    , insn_meta    , INSN},
      {group_methods   , group_meta   , GROUP},
      {NULL, NULL, NULL}
};

int luaopen_abstract_objects_c(lua_State * L)
{
   const bib_t *b;

   for (b = bibs; b->id != NULL; b++) {
      luaL_register(L, b->id, b->methods);
      luaL_newmetatable(L, b->id);
      luaL_register(L, 0, b->meta);

      lua_pushliteral(L, "__index");
      lua_pushvalue(L, -3);
      lua_rawset(L, -3);

      lua_pushliteral(L, "__metatable");
      lua_pushvalue(L, -3);
      lua_rawset(L, -3);

      lua_pop(L, 1);
   }

   lua_pushcfunction(L, get_userdata_address);
   lua_setglobal(L, "get_userdata_address");

   lua_pushcfunction(L, get_host_uarch);
   lua_setglobal(L, "get_host_uarch");

   lua_pushcfunction(L, get_host_proc);
   lua_setglobal(L, "get_host_proc");

   lua_pushcfunction(L, get_file_arch);
   lua_setglobal(L, "get_file_arch");

   lua_pushcfunction(L, get_arch_by_name);
   lua_setglobal(L, "get_arch_by_name");

   lua_pushcfunction(L, get_archs_list);
   lua_setglobal(L, "get_archs_list");

   lua_pushcfunction(L, gen_timestamp);
   lua_setglobal(L, "gen_timestamp");

   lua_pushcfunction(L, demangle_string);
   lua_setglobal(L, "demangle_string");

   lua_pushcfunction(L, maqao_sleep);
   lua_setglobal(L, "maqao_sleep");

   lua_pushcfunction(L, maqao_wait_SIGINT);
   lua_setglobal(L, "maqao_wait_SIGINT");

   lua_pushcfunction(L, maqao_isatty);
   lua_setglobal(L, "maqao_isatty");

   lua_pushcfunction(L, maqao_get_term_size);
   lua_setglobal(L, "maqao_get_term_size");

   lua_pushcfunction(L, adapt_text_length);
   lua_setglobal(L, "adapt_text_length");

   return 1;
}
