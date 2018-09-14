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
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include "fcgiapp.h"

static int
l_FCGX_OpenSocket (lua_State * L)
{
  size_t len;
  const char *addr = lua_tolstring (L, 1, &len);
  int socket;
  socket = FCGX_OpenSocket (addr, 10);
  if (socket < 0 || FCGX_Init ())
    {
      lua_pushnil (L);
    }
  else
    lua_pushinteger (L, socket);
  return 1;
}

static int
l_FCGX_InitRequest (lua_State * L)
{
  int socket = lua_tointeger (L, 1);
  FCGX_Request *request =
    (FCGX_Request *) lua_newuserdata (L, sizeof (FCGX_Request));
  FCGX_InitRequest (request, socket, 0);
  return 1;
}

static int
l_FCGX_Accept_r (lua_State * L)
{
  FCGX_Request *request = (FCGX_Request *) lua_touserdata (L, 1);
  if (FCGX_Accept_r (request) == 0)
    lua_pushinteger (L, 0);
  else
    lua_pushinteger (L, -1);
  return 1;
}

static int
l_FCGX_GetParam (lua_State * L)
{
  FCGX_Request *request = (FCGX_Request *) lua_touserdata (L, 1);
  lua_pushstring (L, FCGX_GetParam ("QUERY_STRING", request->envp));
  return 1;
}
static int
l_FCGX_GetLine (lua_State * L)
{
  char buffer[1024];
  FCGX_Request *request = (FCGX_Request *) lua_touserdata (L, 1);
  lua_pushstring (L, FCGX_GetLine (buffer, 1024, request->in));
  return 1;
}
static int
l_FCGX_FPrintF (lua_State * L)
{
  FCGX_Request *request = (FCGX_Request *) lua_touserdata (L, 1);
  const char *s = lua_tostring (L, 2);
  FCGX_PutS (s, request->out);
  return 0;
}
static const struct luaL_Reg fcgx[] = {
  {"print", l_FCGX_FPrintF},
  {"getparam", l_FCGX_GetParam},
  {"getline", l_FCGX_GetLine},
  {"accept_r", l_FCGX_Accept_r},
  {"initrequest", l_FCGX_InitRequest},
  {"opensocket", l_FCGX_OpenSocket},
  {NULL, NULL}
};


int
luaopen_fcgx (lua_State * L)
{
  luaL_register (L, "fcgx", fcgx);
  return 1;
}
