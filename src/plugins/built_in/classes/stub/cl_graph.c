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

/* TODO: move this into builtin/classes/stub (stub directory + related infrastructure is to create) */

#include "libmcore.h"
#include "classes_c.h"
#include "abstract_objects_c.h"

static int l_graph_node_get_predecessors(lua_State * L)
{
   l_graph_node_t *g = luaL_checkudata(L, 1, GRAPH_NODE);
   array_t *predecessors = graph_node_get_predecessors (g->p);

   if (predecessors != NULL) {
      int i = 1;
      lua_newtable(L);

      FOREACH_INARRAY (predecessors, iter) {
         graph_node_t *pred_node = ARRAY_GET_DATA(graph_node_t*, iter);

         create_graph_node(L, pred_node);
         lua_rawseti(L, -2, i++);
      }
      array_free (predecessors, NULL);

      return 1;
   }

   return 0;
}

static int l_graph_node_get_successors(lua_State * L)
{
   l_graph_node_t *g = luaL_checkudata(L, 1, GRAPH_NODE);
   array_t *successors = graph_node_get_successors (g->p);

   if (successors != NULL) {
      int i = 1;
      lua_newtable(L);

      FOREACH_INARRAY (successors, iter) {
         graph_node_t *succ_node = ARRAY_GET_DATA(graph_node_t*, iter);

         create_graph_node(L, succ_node);
         lua_rawseti(L, -2, i++);
      }
      array_free (successors, NULL);

      return 1;
   }

   return 0;
}

static int l_graph_node_get_incoming_edges(lua_State * L)
{
   l_graph_node_t *g = luaL_checkudata(L, 1, GRAPH_NODE);
   list_t *incoming_edges = graph_node_get_incoming_edges (g->p);

   if (incoming_edges != NULL) {
      int i = 1;
      lua_newtable(L);

      FOREACH_INLIST (incoming_edges, iter) {
         graph_edge_t *edge = GET_DATA_T(graph_edge_t*, iter);

         create_graph_edge(L, edge);
         lua_rawseti(L, -2, i++);
      }

      return 1;
   }

   return 0;
}

static int l_graph_node_get_outgoing_edges(lua_State * L)
{
   l_graph_node_t *g = luaL_checkudata(L, 1, GRAPH_NODE);
   list_t *outgoing_edges = graph_node_get_outgoing_edges (g->p);

   if (outgoing_edges != NULL) {
      int i = 1;
      lua_newtable(L);

      FOREACH_INLIST (outgoing_edges, iter) {
         graph_edge_t *edge = GET_DATA_T(graph_edge_t*, iter);

         create_graph_edge(L, edge);
         lua_rawseti(L, -2, i++);
      }

      return 1;
   }

   return 0;
}

static int l_graph_node_get_block(lua_State * L)
{
   l_graph_node_t *g = luaL_checkudata(L, 1, GRAPH_NODE);
   block_t *block = graph_node_get_data (g->p);

   if (block != NULL) {
      create_block (L, block);
      return 1;
   }

   return 0;
}

static int l_graph_node_get_insn(lua_State * L)
{
   l_graph_node_t *g = luaL_checkudata(L, 1, GRAPH_NODE);
   insn_t *insn = graph_node_get_data (g->p);

   if (insn != NULL) {
      create_insn (L, insn);
      return 1;
   }

   return 0;
}

static int l_graph_edge_get_src_node(lua_State * L)
{
   l_graph_edge_t *e = luaL_checkudata(L, 1, GRAPH_EDGE);
   graph_node_t *node = graph_edge_get_src_node (e->p);

   if (node != NULL) {
      create_graph_node (L, node);
      return 1;
   }

   return 0;
}

static int l_graph_edge_get_dst_node(lua_State * L)
{
   l_graph_edge_t *e = luaL_checkudata(L, 1, GRAPH_EDGE);
   graph_node_t *node = graph_edge_get_dst_node (e->p);

   if (node != NULL) {
      create_graph_node (L, node);
      return 1;
   }

   return 0;
}

static int l_graph_edge_get_data_dependence(lua_State * L)
{
   l_graph_edge_t *e = luaL_checkudata(L, 1, GRAPH_EDGE);
   data_dependence_t *data_dep = graph_edge_get_data (e->p);

   if (data_dep != NULL) {
      lua_newtable(L);

      lua_pushliteral(L, "latency min");
      lua_pushnumber(L, data_dep->latency.min);
      lua_settable(L, -3);

      lua_pushliteral(L, "latency max");
      lua_pushnumber(L, data_dep->latency.max);
      lua_settable(L, -3);

      lua_pushliteral(L, "distance");
      lua_pushnumber(L, data_dep->distance);
      lua_settable(L, -3);

      lua_pushliteral(L, "kind");
      lua_pushstring(L, data_dep->kind);
      lua_settable(L, -3);

      return 1;
   }

   return 0;
}

static void push_nodes (lua_State *L, hashtable_t *nodes)
{
   lua_newtable (L);

   int i = 1;
   FOREACH_INHASHTABLE(nodes, node_iter) {
      graph_node_t *node = GET_KEY (graph_node_t *, node_iter);
      create_graph_node (L, node);
      lua_rawseti (L, -2, i++);
   }
}

static void push_edges (lua_State *L, hashtable_t *edges)
{
   lua_newtable (L);

   int i = 1;
   FOREACH_INHASHTABLE(edges, edge_iter) {
      graph_edge_t *edge = GET_KEY (graph_edge_t *, edge_iter);
      create_graph_edge (L, edge);
      lua_rawseti (L, -2, i++);
   }
}

static int l_graph_connected_component_get_entry_nodes(lua_State *L)
{
   l_graph_connected_component_t *cc = luaL_checkudata(L, 1, GRAPH_CONNECTED_COMPONENT);
   if (cc) {
      hashtable_t *entry_nodes = graph_connected_component_get_entry_nodes (cc->p);
      push_nodes (L, entry_nodes);
      return 1;
   }

   return 0;
}

static int l_graph_connected_component_get_nodes(lua_State *L)
{
   l_graph_connected_component_t *cc = luaL_checkudata(L, 1, GRAPH_CONNECTED_COMPONENT);
   if (cc) {
      hashtable_t *nodes = graph_connected_component_get_nodes (cc->p);
      push_nodes (L, nodes);
      return 1;
   }

   return 0;
}

static int l_graph_connected_component_get_edges(lua_State *L)
{
   l_graph_connected_component_t *cc = luaL_checkudata(L, 1, GRAPH_CONNECTED_COMPONENT);
   if (cc) {
      hashtable_t *edges = graph_connected_component_get_edges (cc->p);
      push_edges (L, edges);
      return 1;
   }

   return 0;
}

static int l_graph_new (lua_State * L)
{
   graph_t *new = graph_new ();

   if (new) {
      create_graph (L, new);
      return 1;
   }

   return 0;
}

static void push_node2cc (lua_State *L, hashtable_t *node2cc)
{
   lua_newtable (L);
   FOREACH_INHASHTABLE(node2cc, node2cc_iter) {
      graph_node_t *node = GET_KEY (graph_node_t *, node2cc_iter);
      graph_connected_component_t *cc = GET_DATA_T (graph_connected_component_t *, node2cc_iter);
      create_graph_node (L, node);
      create_graph_connected_component (L, cc);
      lua_settable (L, -3);
   }
}

static void push_edge2cc (lua_State *L, hashtable_t *edge2cc)
{
   lua_newtable (L);
   FOREACH_INHASHTABLE(edge2cc, edge2cc_iter) {
      graph_edge_t *edge = GET_KEY (graph_edge_t *, edge2cc_iter);
      graph_connected_component_t *cc = GET_DATA_T (graph_connected_component_t *, edge2cc_iter);
      create_graph_edge (L, edge);
      create_graph_connected_component (L, cc);
      lua_settable (L, -3);
   }
}

static void push_connected_components (lua_State *L, queue_t *CCs)
{
   lua_newtable (L);

   int i = 1;
   FOREACH_INQUEUE(CCs, CC_iter) {
      graph_connected_component_t *CC = GET_DATA_T (graph_connected_component_t *, CC_iter);
      create_graph_connected_component (L, CC);
      lua_rawseti (L, -2, i++);
   }
}

static int l_graph_get_node2cc(lua_State * L)
{
   l_graph_t *g = luaL_checkudata(L, 1, GRAPH);
   if (g) {
      hashtable_t *node2cc = graph_get_node2cc (g->p);
      push_node2cc (L, node2cc);
      return 1;
   }

   return 0;
}

static int l_graph_get_edge2cc(lua_State * L)
{
   l_graph_t *g = luaL_checkudata(L, 1, GRAPH);
   if (g) {
      hashtable_t *edge2cc = graph_get_edge2cc (g->p);
      push_edge2cc (L, edge2cc);
      return 1;
   }

   return 0;
}

static int l_graph_get_connected_components(lua_State * L)
{
   l_graph_t *g = luaL_checkudata(L, 1, GRAPH);
   if (g) {
      queue_t *connected_components = graph_get_connected_components (g->p);
      push_connected_components (L, connected_components);
      return 1;
   }

   return 0;
}

static int l_graph_add_new_node (lua_State * L)
{
   l_graph_t *g = luaL_checkudata(L, 1, GRAPH);
   void *udata = lua_touserdata(L, 2);

   graph_node_t *new_node = graph_add_new_node (g->p, udata ? ((i_t *) udata)->p : NULL);

   if (new_node) {
      create_graph_node (L, new_node);
      return 1;
   }

   return 0;
}

static int l_graph_add_new_edge (lua_State * L)
{
   l_graph_t       *g = luaL_checkudata(L, 1, GRAPH);
   l_graph_node_t *n1 = luaL_checkudata(L, 2, GRAPH_NODE);
   l_graph_node_t *n2 = luaL_checkudata(L, 3, GRAPH_NODE);
   void        *udata = lua_touserdata (L, 4);

   graph_edge_t *new_edge = graph_add_new_edge (g->p, n1->p, n2->p, udata ? ((i_t *) udata)->p : NULL);

   if (new_edge) {
      create_graph_edge (L, new_edge);
      return 1;
   }

   return 0;
}

static int l_graph_free (lua_State * L)
{
   l_graph_t *g = luaL_checkudata(L, 1, GRAPH);
   graph_free (g->p, NULL, NULL);

   return 0;
}

static int graph_node_gc(lua_State * L)
{
   (void) L;

   return 0;
}

static int graph_node_tostring(lua_State * L)
{
   l_graph_node_t *g = lua_touserdata(L, 1);

   lua_pushfstring(L, "Graph node: %p", g->p);

   return 1;
}

static int graph_edge_gc(lua_State * L)
{
   (void) L;

   return 0;
}

static int graph_edge_tostring(lua_State * L)
{
   l_graph_edge_t *e = lua_touserdata(L, 1);

   lua_pushfstring(L, "Graph edge: %p", e->p);

   return 1;
}

static int graph_gc(lua_State * L)
{
   (void) L;

   return 0;
}

static int graph_tostring(lua_State * L)
{
   l_graph_t *g = lua_touserdata(L, 1);

   lua_pushfstring(L, "Graph: %p", g->p);

   return 1;
}

static int graph_connected_component_gc(lua_State * L)
{
   (void) L;

   return 0;
}

static int graph_connected_component_tostring(lua_State * L)
{
   l_graph_connected_component_t *cc = lua_touserdata(L, 1);

   lua_pushfstring(L, "Graph connected component: %p", cc->p);

   return 1;
}

static int l_DDG_get_RecMII(lua_State* L)
{
   l_graph_t *g = luaL_checkudata(L, 1, GRAPH);

   int max_paths = -1;
   if (lua_type(L, 2) == LUA_TNUMBER)
      max_paths = luaL_checkinteger(L, 2);

   float min, max;
   get_RecMII(g->p, max_paths, &min, &max);

   lua_pushnumber(L, min);
   lua_pushnumber(L, max);

   return 2;
}

static void _push_DDG_paths (lua_State* L, array_t *array) {
   lua_newtable(L); int i = 1;
   FOREACH_INARRAY(array, array_iter) {
      lua_newtable(L); int j = 1;
      array_t *path = ARRAY_GET_DATA (path, array_iter);
      FOREACH_INARRAY(path, path_iter) {
         graph_node_t *node = ARRAY_GET_DATA (node, path_iter);
         insn_t *insn = graph_node_get_data (node);
         create_insn (L, insn);
         lua_rawseti (L, -2, j++);
      }
      lua_rawseti (L, -2, i++);
   }
   array_free (array, NULL);
}

static int l_DDG_get_critical_paths(lua_State* L)
{
   l_graph_t *g = luaL_checkudata(L, 1, GRAPH);

   int max_paths = -1;
   if (lua_type(L, 2) == LUA_TNUMBER)
      max_paths = luaL_checkinteger(L, 2);

   array_t *min, *max; // critical paths considering minimum/maximum latency values
   lcore_ddg_get_critical_paths (g->p, max_paths, &min, &max);

   _push_DDG_paths (L, min); // min latency
   _push_DDG_paths (L, max); // max latency

   return 2;
}

static int l_DDG_free (lua_State* L)
{
   l_graph_t *g = luaL_checkudata(L, 1, GRAPH);
   lcore_freeddg (g->p);

   return 0;
}

const luaL_reg graph_node_methods[] = {
  {"get_predecessors"     , l_graph_node_get_predecessors},
  {"get_successors"       , l_graph_node_get_successors},
  {"get_incoming_edges"   , l_graph_node_get_incoming_edges},
  {"get_outgoing_edges"   , l_graph_node_get_outgoing_edges},
  {"get_block"            , l_graph_node_get_block},
  {"get_insn"             , l_graph_node_get_insn},
  {NULL, NULL}
};

const luaL_reg graph_edge_methods[] = {
  {"get_src_node", l_graph_edge_get_src_node},
  {"get_dst_node", l_graph_edge_get_dst_node},
  {"get_data_dependence" , l_graph_edge_get_data_dependence},
  {NULL, NULL}
};

const luaL_reg graph_methods[] = {
   {"new", l_graph_new},
   {"get_node2cc", l_graph_get_node2cc},
   {"get_edge2cc", l_graph_get_edge2cc},
   {"get_connected_components", l_graph_get_connected_components},
   {"add_new_node", l_graph_add_new_node},
   {"add_new_edge", l_graph_add_new_edge},
   {"DDG_get_RecMII", l_DDG_get_RecMII},
   {"DDG_get_critical_paths", l_DDG_get_critical_paths},
   {"DDG_free", l_DDG_free},
   {"free", l_graph_free},
  {NULL, NULL}
};

const luaL_reg graph_connected_component_methods[] = {
   {"get_entry_nodes", l_graph_connected_component_get_entry_nodes},
   {"get_nodes", l_graph_connected_component_get_nodes},
   {"get_edges", l_graph_connected_component_get_edges},
  {NULL, NULL}
};

const luaL_reg graph_node_meta[] = {
  {"__gc"      , graph_node_gc},
  {"__tostring", graph_node_tostring},
  {NULL, NULL}
};

const luaL_reg graph_edge_meta[] = {
  {"__gc"      , graph_edge_gc},
  {"__tostring", graph_edge_tostring},
  {NULL, NULL}
};

const luaL_reg graph_meta[] = {
  {"__gc"      , graph_gc},
  {"__tostring", graph_tostring},
  {NULL, NULL}
};

const luaL_reg graph_connected_component_meta[] = {
  {"__gc"      , graph_connected_component_gc},
  {"__tostring", graph_connected_component_tostring},
  {NULL, NULL}
};
