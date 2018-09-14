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
   This file defines stub (hook, wrapper...) functions to allow using C MAQAO functions in LUA for the following libcommon data structures:
   - list
   - queue
   - tree
   - graph
   - hashtable
   - bitvector
*/

#include <stdlib.h>
#include <stdint.h> /* int64_t... */
#include <string.h> /* strcmp... */

/* LUA specific includes */
#include <lua.h>     /* declares lua_*  functions */
#include <lauxlib.h> /* declares luaL_* functions */

/* MAQAO specific includes */
#include "libmcommon.h"

/* Internal name of data structures from the C libcommon */
#define CFCT      "cfct"
#define CLIST      "clist"
#define CQUEUE     "cqueue"
#define CTREE      "ctree"
#define CGRAPH     "cgraph"
#define CHASHTABLE "chashtable"
#define CBITVECTOR "cbitvector"

/* Ease use of LUA objects */

typedef struct { list_t      *p; } l_t;
typedef struct { queue_t     *p; } q_t;
typedef struct { tree_t      *p; } t_t;
typedef struct { graph_node_t     *p; } g_t;
typedef struct { hashtable_t *p; } h_t;
typedef struct { bitvector_t *p; } b_t;

/**
 * The 6 following functions create the LUA version of the data structures.
 * It is not easy to factor them because of the second argument of lua_newuserdata,
 * that must be determined statically for performance reasons
 * TODO: factor these functions without significantly impacting performance
 */
/*
static l_t *create_list (lua_State * L, list_t *list) {
  l_t *ptr = lua_newuserdata (L, sizeof *ptr);

  luaL_getmetatable (L, CLIST);
  lua_setmetatable (L, -2);

  ptr->p = list;

  return ptr;
}

static q_t *create_queue (lua_State * L, queue_t *queue) {
  q_t *ptr = lua_newuserdata (L, sizeof *ptr);

  luaL_getmetatable (L, CQUEUE);
  lua_setmetatable (L, -2);

  ptr->p = queue;

  return ptr;
}

static t_t *create_tree (lua_State * L, tree_t *tree) {
  t_t *ptr = lua_newuserdata (L, sizeof *ptr);

  luaL_getmetatable (L, CTREE);
  lua_setmetatable (L, -2);

  ptr->p = tree;

  return ptr;
}
*/
/*
static g_t *create_graph (lua_State * L, graph_node_t *graph) {
  g_t *ptr = lua_newuserdata (L, sizeof *ptr);

  luaL_getmetatable (L, CGRAPH);
  lua_setmetatable (L, -2);

  ptr->p = graph;

  return ptr;
}
*/
/*
static h_t *create_hashtable (lua_State * L, hashtable_t *hashtable) {
  h_t *ptr = lua_newuserdata (L, sizeof *ptr);

  luaL_getmetatable (L, CHASHTABLE);
  lua_setmetatable (L, -2);

  ptr->p = hashtable;

  return ptr;
}
*/
/*
static b_t *create_bitvector (lua_State * L, bitvector_t *bitvector) {
  b_t *ptr = lua_newuserdata (L, sizeof *ptr);

  luaL_getmetatable (L, CBITVECTOR);
  lua_setmetatable (L, -2);

  ptr->p = bitvector;

  return ptr;
}
*/
/*
#include "common_list.c"
#include "common_queue.c"
*/
/******************************************************************/
/*                Library Creation                                */
/******************************************************************/

typedef struct {
  const luaL_reg *methods;
  const luaL_reg *meta;
  const char     *id;
} bib_t;

static const bib_t bibs[] = {
//  {list_methods, list_meta, CLIST},
//  {queue_methods, queue_meta, CQUEUE},
//   {tree_methods, tree_meta, CTREE},
//   {graph_methods, graph_meta, CGRAPH},
//   {hashtable_methods, hashtable_meta, CHASHTABLE},
//   {bitvector_methods, bitvector_meta, CBITVECTOR},
  {NULL, NULL, NULL}
};

int luaopen_common_c (lua_State * L) 
{
  const bib_t *b;

  for (b = bibs; b->id != NULL; b++) {
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
