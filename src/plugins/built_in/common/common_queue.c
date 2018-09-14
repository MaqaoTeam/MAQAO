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
/*                Functions dealing with queue                     */
/******************************************************************/

// static int l_queue_add_after (lua_State * L) {
//   /* Get and check arguments */
//   q_t *q = luaL_checkudata (L, 1, CLIST);
//   void *data = lua_touserdata (L, 2);
// 
//   
//   
// }

static int l_queue_new (lua_State *L) 
{
  queue_t* queue = queue_new();
  create_queue (L,queue);

  return 1;
}

static int l_queue_new_fromptr (lua_State *L) 
{  
  void *ptr = lua_touserdata(L,1);
  
  create_queue (L,ptr);

  return 1;
}

static int l_queue_length (lua_State * L) 
{
  q_t *q = luaL_checkudata(L,1,CQUEUE);

  lua_pushinteger(L,queue_length(q->p));

  return 1;
}

static int l_queue_free (lua_State * L) 
{
  q_t *q = luaL_checkudata(L,1,CQUEUE);
 
  if(q->p != NULL) {
    queue_free(q->p,NULL);
  }else{
    WARNING("Can't free a null reference (queue)");
  }

  return 0;
}

static int l_queue_add_head (lua_State * L) 
{
  q_t *q = luaL_checkudata(L,1,CQUEUE);
  void *data = lua_touserdata(L,2);
  
  queue_add_head(q->p,data);
  
  return 1;
}

static int l_queue_add_tail (lua_State * L) 
{
  q_t *q = luaL_checkudata(L,1,CQUEUE);
  void *data = lua_touserdata(L,2);
  
  queue_add_tail(q->p,data);

  return 1;
}

static int l_queue_iter_helper (lua_State *L) 
{
  list_t **list = lua_touserdata (L, lua_upvalueindex (1));

  if (list != NULL && *list != NULL) 
  {
    void *data = list_getdata (*list);

    if (data == NULL) 
    {
      WARNING ("A NULL instruction has been detected, skeeping instruction...");
      return 0;
    }

    lua_pushlightuserdata(L,data);
    *list = list_getnext (*list);

    return 1;
  }

  return 0;
}

static int l_queue_iter (lua_State * L) 
{
  q_t *q = luaL_checkudata (L,1,CQUEUE);
  
  list_t **list = lua_newuserdata (L, sizeof *list);

  *list = q->p->head;
  //*list = b->p->tail;
  
  lua_pushcclosure (L, l_queue_iter_helper, 1);

  return 1;
}

static int l_queue_get_userdataptr (lua_State * L) 
{
  q_t *q = luaL_checkudata (L,1,CQUEUE);
  
  lua_pushlightuserdata(L,q->p);

  return 1;
}

static int l_queue_gc (lua_State *L) 
{
  (void) L;

  return 0;
}

static int l_queue_tostring (lua_State * L) 
{
  q_t *q = luaL_checkudata (L,1,CQUEUE);

  lua_pushfstring (L,"C Queue: %p",q->p);

  return 1;
}

/**
 * Bind names from this file to LUA
 * For example, {"foo", "bar"} will be interpreted in the following way:
 * to use bar (defined in this file), call foo in LUA.
 */
static const luaL_reg queue_methods[] = {
  {"new"              , l_queue_new},
  {"new_fromptr"      , l_queue_new_fromptr},  
  {"get_length"       , l_queue_length},
  {"get_userdataptr"  , l_queue_get_userdataptr},
  {"free"             , l_queue_free},
  {"add_head"         , l_queue_add_head},  
  {"add_tail"         , l_queue_add_tail}, 
  {"iter"             , l_queue_iter},
  {NULL, NULL}
};

static const luaL_reg queue_meta[] = {
  {"__gc"      , l_queue_gc},
  {"__tostring", l_queue_tostring},
  {NULL, NULL}
};
