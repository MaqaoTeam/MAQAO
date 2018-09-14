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

/******************************************************************/
/*                Functions dealing with list                     */
/******************************************************************/

// static int l_list_add_after (lua_State * L) {
//   /* Get and check arguments */
//   l_t *l = luaL_checkudata (L, 1, CLIST);
//   void *data = lua_touserdata (L, 2);
// 
//   
//   
// }

static int l_list_new (lua_State * L) 
{
  list_t* list = (list_t *)malloc(sizeof(list_t));
  create_list (L,list);

  return 1;
}

static int l_list_get_data(lua_State * L) 
{
  l_t *l = luaL_checkudata (L,1,CLIST);

  lua_pushlightuserdata(L,l->p->data);

  return 1;
}

static int l_list_gc (lua_State * L) 
{
  (void) L;

  return 0;
}

static int l_list_tostring (lua_State * L) 
{
  l_t *l = luaL_checkudata (L,1,CLIST);

  lua_pushfstring (L, "C List: %p", l->p);

  return 1;
}

/**
 * Bind names from this file to LUA
 * For example, {"foo", "bar"} will be interpreted in the following way:
 * to use bar (defined in this file), call foo in LUA.
 */
static const luaL_reg list_methods[] = {
  {"new"              , l_list_new},
  {"get_data"         , l_list_get_data},  
  {NULL, NULL}
};

static const luaL_reg list_meta[] = {
  {"__gc"      , l_list_gc},
  {"__tostring", l_list_tostring},
  {NULL, NULL}
};
