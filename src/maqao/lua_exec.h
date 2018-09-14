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

#ifndef __LMAQAO_LUA_EXEC_H__
#define __LMAQAO_LUA_EXEC_H__

/**
 * \file lua_exec.h
 * \brief Provides a mean to execute Lua code from C code
 */

#include "lua.h"

extern lua_State *init_maqao_lua();
extern char * lua_exec_str(lua_State *L, char *buff, int bufferlen,
      char *buffername);
extern int lua_exec(lua_State *L, char *buff, int bufferlen, char *buffername);

#endif
