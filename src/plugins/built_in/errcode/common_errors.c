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
 * \file common_errors.c
 *
 * \date 2016/02/10
 * \brief This file contains functions for handling the error codes defined in MAQAO from Lua
 */

#ifdef _WIN32
#define LUA __declspec(dllexport)
#else
#define LUA
#endif
#include <lua.h>           // declares lua_*  functions
#include <lauxlib.h>       // declares luaL_* functions
#include "libmcommon.h"

/* Retrieves the error message associated to an error code*/
static int l_errcode_tostring (lua_State * L) {
    int errcode = luaL_checkint (L, 1);
    lua_pushfstring (L, errcode_getmsg(errcode));
    return 1;
}

/* Builds an error code from its components */
static int l_errcode_build (lua_State * L) {

  /* Check and get arguments */
  int mod   = luaL_checkint   (L, 1);
  int lvl   = luaL_checkint   (L, 2);
  int code  = luaL_checkint   (L, 3);

  int errcode = errcode_build(mod, lvl, code);
  lua_pushinteger (L, errcode);

  return 1;
}

/** Returns a string corresponding to an error level*/
static int l_errlevel_tostring(lua_State * L) {
   /* Check and get arguments */
   int lvl   = luaL_checkint   (L, 1);
   lua_pushfstring (L, errlevel_getname(lvl));

   return 1;
}

static const luaL_reg errcode_methods[] = {
  /* Builds an error code from its components*/
  {"buildcode"      , l_errcode_build},
  {"levelname"      , l_errlevel_tostring},
  {"errormsg"       , l_errcode_tostring},
  {NULL, NULL}
};

LUA int luaopen_errcode_c (lua_State * L)
{
  DBGMSG0 ("Registering error handling module\n");

  luaL_register     (L, "errcode", errcode_methods);

  return 1;
}

