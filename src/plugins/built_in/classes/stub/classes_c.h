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

#ifndef __CLASSES_H__
#define __CLASSES_H__


#include <lua.h>           // declares lua_*  functions
#include <lauxlib.h>       // declares luaL_* functions
#include "libmcommon.h"

// Internal name of abstract objects
#define GRAPH_NODE     "graph_node"
#define GRAPH_EDGE     "graph_edge"

#define GRAPH "graph"
#define GRAPH_CONNECTED_COMPONENT "graph_connected_component"

// Ease use of LUA objects
typedef struct { graph_node_t *p; } l_graph_node_t;
typedef struct { graph_edge_t *p; } l_graph_edge_t;

typedef struct { graph_t *p; } l_graph_t;
typedef struct { graph_connected_component_t *p; } l_graph_connected_component_t;

//
extern const luaL_reg graph_node_methods[];
extern const luaL_reg graph_node_meta[];

extern const luaL_reg graph_edge_methods[];
extern const luaL_reg graph_edge_meta[];

extern const luaL_reg graph_methods[];
extern const luaL_reg graph_meta[];

extern const luaL_reg graph_connected_component_methods[];
extern const luaL_reg graph_connected_component_meta[];

//
extern l_graph_node_t *create_graph_node(lua_State * L, graph_node_t *node);
extern l_graph_edge_t *create_graph_edge(lua_State * L, graph_edge_t *edge);

extern l_graph_t *create_graph (lua_State * L, graph_t *graph);
extern l_graph_connected_component_t *create_graph_connected_component (lua_State * L, graph_connected_component_t *CC);

#endif
