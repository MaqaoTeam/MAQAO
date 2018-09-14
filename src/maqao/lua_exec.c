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

#include <stdio.h>
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "libmcommon.h"

#define MAQAO_ERROR_ROOT_STR "MAQAO> "

extern void luaL_openlibs(lua_State *L);

/**
 * Displays the execution stack trace at the current lua state point using the lua traceback function
 * \param L  A valid lua state
 * */
int lua_exec_traceback(lua_State *L)
{
   lua_getfield(L, LUA_GLOBALSINDEX, "debug");
   lua_getfield(L, -1, "traceback");
   lua_pushvalue(L, 1);
   lua_pushinteger(L, 2);
   lua_call(L, 2, 1);
   fprintf(stderr, "%s%s\n", MAQAO_ERROR_ROOT_STR, lua_tostring(L, -1));

   return 1;
}

/**
 * Displays the execution stack trace at the current lua state point
 * \param L  A valid lua state
 * */
void lua_exec_stackdump(lua_State* L)
{
   int i;
   int top = lua_gettop(L);

   printf("total in stack %d\n", top);

   for (i = 1; i <= top; i++) { /* repeat for each level */
      int t = lua_type(L, i);
      switch (t) {
      case LUA_TSTRING: /* strings */
         printf("string: '%s'\n", lua_tostring(L, i));
         break;
      case LUA_TBOOLEAN: /* booleans */
         printf("boolean %s\n", lua_toboolean(L, i) ? "true" : "false");
         break;
      case LUA_TNUMBER: /* numbers */
         printf("number: %g\n", lua_tonumber(L, i));
         break;
      default: /* other values */
         printf("%s\n", lua_typename(L, t));
         break;
      }
      printf("  "); /* put a separator */
   }
   printf("\n"); /* end the listing */
}

/**
 * Loads and executes a Lua code chunk contained in a text buffer
 * \param L  A valid lua state
 * \param buff Text buffer containing the Lua chunk to execute
 * \param bufferlen Text buffer length
 * \param buffername  Chunk name, used for debug information and error messages. 
 * \return An error string
 * */
char *lua_exec_str(lua_State *L, char *buff, int bufferlen, char *buffername)
{
   char *str_error = NULL;
   int error = 0;
   int bufflen = 0;

   if (L == NULL) {
      str_error =
            lc_strdup(
                  "Invalid internal interpreter context : Impossible to execute your query");
      return str_error;
   }

   if (bufferlen > 0)
      bufflen = bufferlen;
   else if (buff != NULL)
      bufflen = strlen(buff);

   //Only use this output in debug mode
   //lua_exec_traceback(L);
   error = luaL_loadbuffer(L, buff, bufflen, buffername)
         || lua_pcall(L, 0, 0, 0);
   if (error) {
      int size = 0;
      if (error == LUA_ERRSYNTAX)
         fprintf(stderr, "%sSyntax error during pre-compilation\n",
               MAQAO_ERROR_ROOT_STR);
      if (error == LUA_ERRMEM)
         fprintf(stderr, "%sMemory allocation error\n", MAQAO_ERROR_ROOT_STR);
      const char* string_error = lua_tostring(L, -1);
      size = strlen(string_error) + strlen(MAQAO_ERROR_ROOT_STR) + 2;
      str_error = (char *) lc_malloc(size);
      lc_sprintf(str_error, size, "%s%s\n", MAQAO_ERROR_ROOT_STR, string_error);
      //Only use this output in debug mode
      //fprintf(stderr,"Size = %d, Buffer = %s\n",bufferlen,buff);
      //lua_exec_stackdump(L);
      lua_pop(L, 1);

      return str_error;
   }

   return str_error;
}

/**
 * Loads and executes a Lua code chunk contained in a text buffer
 * \param L  A valid lua state
 * \param buff Text buffer containing the Lua chunk to execute
 * \param bufferlen Text buffer length
 * \param buffername  Chunk name, used for debug information and error messages. 
 * \return An error number
 * */
int lua_exec(lua_State *L, char *buff, int bufferlen, char *buffername)
{
   int error = 0;
   int lua_error;
   int bufflen = 0;

   if (L == NULL)
      return ERR_LUAEXE_MISSING_LUA_STATE;

   if (bufferlen > 0)
      bufflen = bufferlen;
   else if (buff != NULL) //Guess size of string literal
      bufflen = strlen(buff);
   else
      return ERR_LUAEXE_MISSING_LUA_CHUNK;

   //Only use this output in debug mode
   //lua_exec_traceback(L);
   error = luaL_loadbuffer(L, buff, bufflen, buffername);
   if (error) {
      if (error == LUA_ERRSYNTAX) //Syntax error during pre-compilation
         return ERR_LUAEXE_PRECOMP_SYNTAX_ERROR;
      else if (error == LUA_ERRMEM) //Memory allocation error
         return ERR_LUAEXE_PRECOMP_MEMORY_ALLOCATION;
   } else {
      error = lua_pcall(L, 0, 0, 0);
      if (error) {
         if (error == LUA_ERRRUN) //A runtime error
         {
            if (lua_isstring(L, -1)) {
               DBGMSG("Error message from runtime: %s\n", lua_tostring (L,-1));
               lua_error = atoi(lua_tostring(L, -1));
               if (lua_error)
                  return lua_error;
               else
                  return ERR_LUAEXE_RUNTIME_ERROR;
            }
            return ERR_LUAEXE_UNKNOWN_RUNTIME_ERROR;
         } else if (error == LUA_ERRMEM) //Memory allocation error. For such errors, Lua does not call the error handler function.
            return ERR_LUAEXE_MEMORY_ALLOCATION;
         else if (error == LUA_ERRERR) //Error while running the error handler function.
            return ERR_LUAEXE_ERROR_HANDLER;
      }
   }

   return 0;
}

/**
 * Initializes a Lua state, MAQAO modules and environment
 * \return An initialized lua state
 * */
lua_State *init_maqao_lua()
{
   lua_State *context = (lua_State *) luaL_newstate();
   char* decodestr = NULL;
   char* lua_msg = NULL;
   //Load Lua default modules along with MAQAO modules (stub part written ic C/C++)
   luaL_openlibs(context);
   //Load  MAQAO Lua utility classes
#include "lua_modules.lua.b64.c"
   decodestr = decode(lua_modules, lua_modules_size);
   lua_msg = lua_exec_str(context, decodestr, lua_modules_size, lua_modules_name);
   if (lua_msg != NULL) {
      STDMSG("%s", lua_msg);
      lc_free(lua_msg);
   }
   lc_free(decodestr);
   //Load lua modules (lua_modules.h is automatically generated given the existing modules and inclusion/exclusion lists)
#include "lua_modules.h"
   //Set arguments
   lua_msg = lua_exec_str(context, "arg = {}", 0, "init_args");
   if (lua_msg != NULL) {
      STDMSG("%s", lua_msg);
      lc_free(lua_msg);
   }

   //Inject embedded libraries metadata into Lua context	
   lua_getglobal(context, "_G");
   lua_pushstring(context, "____static_binary_mode____");
   lua_pushboolean(context, 1);

#include "lua_list_modules.lua.b64.c"
   decodestr = decode(lua_list_modules, lua_list_modules_size);
   lua_msg = lua_exec_str(context, decodestr, lua_list_modules_size,
               lua_list_modules_name);
   if (lua_msg != NULL) {
      STDMSG("%s", lua_msg);
      lc_free(lua_msg);
   }
   lc_free(decodestr);
   //Start MAQAO Lua entry point
   //MESSAGE("%s", lua_exec_str(context,buff,0,"modules_list"));

   return context;
}
