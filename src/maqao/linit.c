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
 ** $Id: linit.c,v 1.14.1.1 2007/12/27 13:02:25 roberto Exp $
 ** Initialization of libraries for lua.c
 ** See Copyright Notice in lua.h
 */

#define linit_c
#define LUA_LIB

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

extern int luaopen_abstract_objects_c(lua_State * L);
extern int luaopen_classes_c(lua_State * L);
extern int luaopen_errcode_c(lua_State * L);
extern int luaopen_madras(lua_State * L);
extern int luaopen_fs(lua_State * L);
extern int luaopen_lfs(lua_State * L);
#ifndef _WIN32
extern int luaopen_fcgx(lua_State * L);
#endif
extern int luaopen_bitops(lua_State * L);
#include "lua_decl_stub.h"
extern int luaopen_common_c(lua_State * L);

static const luaL_Reg lualibs[] = { { "", luaopen_base }, { LUA_LOADLIBNAME,
      luaopen_package }, { LUA_TABLIBNAME, luaopen_table }, { LUA_IOLIBNAME,
      luaopen_io }, { LUA_OSLIBNAME, luaopen_os }, { LUA_STRLIBNAME,
      luaopen_string }, { LUA_MATHLIBNAME, luaopen_math }, { LUA_DBLIBNAME,
      luaopen_debug }, { "", luaopen_abstract_objects_c }, { "", luaopen_classes_c }, { "",
      luaopen_errcode_c }, { "", luaopen_bitops }, { "", luaopen_madras }, { "",
      luaopen_fs }, { "", luaopen_lfs },
#ifndef _WIN32
      { "", luaopen_fcgx },
#endif
      { "", luaopen_common_c },
#include "lua_list_stub.h"
      { NULL, NULL } };

LUALIB_API void luaL_openlibs(lua_State *L)
{
   const luaL_Reg *lib = lualibs;

   for (; lib->func; lib++) {
      lua_pushcfunction(L, lib->func);
      lua_pushstring(L, lib->name);
      lua_call(L, 1, 0);
   }
}

