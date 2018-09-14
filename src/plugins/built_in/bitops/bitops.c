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

#ifdef _WIN32
#define LUA __declspec(dllexport)
#else
#define LUA
#endif

#include <stdlib.h>
#include <stdint.h> /* int64_t... */
#include <string.h> /* strcmp... */
#include <math.h>   /* log2 */

/* LUA specific includes */
#include <lua.h>     /* declares lua_*  functions */
#include <lauxlib.h> /* declares luaL_* functions */
/******************************************************************/
/*   Functions extending math (will be obsolete with LUA 5.2)     */
/******************************************************************/

/* TODO: on 32 bits machines, lua_Integer is 32 bits */
static int math_or (lua_State * L) {
  lua_Integer a = luaL_checkinteger (L, 1);
  lua_Integer b = luaL_checkinteger (L, 2);

  lua_pushinteger (L, a | b);

  return 1;
}

static int math_and (lua_State * L) {
  lua_Integer a = luaL_checkinteger (L, 1);
  lua_Integer b = luaL_checkinteger (L, 2);

  lua_pushinteger (L, a & b);

  return 1;
}

static int math_xor (lua_State * L) {
  lua_Integer a = luaL_checkinteger (L, 1);
  lua_Integer b = luaL_checkinteger (L, 2);

  lua_pushinteger (L, a ^ b);

  return 1;
}

static int math_not (lua_State * L) {
  lua_Integer a = luaL_checkinteger (L, 1);

  lua_pushinteger (L, ~a);

  return 1;
}

static int math_log2 (lua_State * L) {
  double x = lua_tonumber (L, -1);

  lua_pushnumber (L, log2 (x));

  return 1;
}

static int math_rshift (lua_State *L) {
  lua_Integer val   = luaL_checkinteger (L, 1);
  lua_Integer shift = luaL_checkinteger (L, 2);

  lua_pushinteger (L, val >> shift);

  return 1;
}

static int math_lshift (lua_State *L) {
  lua_Integer val   = luaL_checkinteger (L, 1);
  lua_Integer shift = luaL_checkinteger (L, 2);

  lua_pushinteger (L, val << shift);

  return 1;
}

/******************************************************************/
/*                Library Creation                                */
/******************************************************************/

LUA int luaopen_bitops (lua_State * L)
{
  lua_pushcfunction (L, math_or);
  lua_setglobal     (L, "math_or");

  lua_pushcfunction (L, math_and);
  lua_setglobal     (L, "math_and");

  lua_pushcfunction (L, math_xor);
  lua_setglobal     (L, "math_xor");

  lua_pushcfunction (L, math_not);
  lua_setglobal     (L, "math_not");

  lua_pushcfunction (L, math_log2);
  lua_setglobal     (L, "math_log2");

  lua_pushcfunction (L, math_lshift);
  lua_setglobal     (L, "math_lshift");

  lua_pushcfunction (L, math_rshift);
  lua_setglobal     (L, "math_rshift");

  return 1;
}
