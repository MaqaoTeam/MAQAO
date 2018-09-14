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
   This file defines stub (hook, wrapper...) functions to allow using C MAQAO functions in LUA for the following classes:
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

/* LUA specific includes */
#include <lua.h>     /* declares lua_*  functions */
#include <lauxlib.h> /* declares luaL_* functions */

/* MAQAO specific includes */
#include "libmmaqao.h"
#include "classes_c.h"

/**
 * The 2 following functions create the LUA version of the classes.
 * It is not easy to factor them because of the second argument of lua_newuserdata,
 * that must be determined statically for performance reasons
 * TODO: factor these functions without significantly impacting performance
 */

l_graph_node_t *create_graph_node(lua_State * L, graph_node_t *node)
{
   l_graph_node_t *ptr = lua_newuserdata(L, sizeof *ptr);

   luaL_getmetatable(L, GRAPH_NODE);
   lua_setmetatable(L, -2);
   ptr->p = node;

   return ptr;
}

l_graph_edge_t *create_graph_edge(lua_State * L, graph_edge_t *edge)
{
   l_graph_edge_t *ptr = lua_newuserdata(L, sizeof *ptr);

   luaL_getmetatable(L, GRAPH_EDGE);
   lua_setmetatable(L, -2);
   ptr->p = edge;

   return ptr;
}

l_graph_connected_component_t *create_graph_connected_component(lua_State * L, graph_connected_component_t *cc)
{
   l_graph_connected_component_t *ptr = lua_newuserdata(L, sizeof *ptr);

   luaL_getmetatable(L, GRAPH_CONNECTED_COMPONENT);
   lua_setmetatable(L, -2);
   ptr->p = cc;

   return ptr;
}

l_graph_t *create_graph (lua_State * L, graph_t *graph)
{
   l_graph_t *ptr = lua_newuserdata(L, sizeof *ptr);

   luaL_getmetatable(L, GRAPH);
   lua_setmetatable(L, -2);
   ptr->p = graph;

   return ptr;   
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
      {graph_node_methods, graph_node_meta, GRAPH_NODE},
      {graph_edge_methods, graph_edge_meta, GRAPH_EDGE},
      {graph_methods, graph_meta, GRAPH},
      {graph_connected_component_methods, graph_connected_component_meta, GRAPH_CONNECTED_COMPONENT},
      {NULL, NULL, NULL}
};

int luaopen_classes_c(lua_State * L)
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

   return 1;
}
