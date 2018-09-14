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
 * \file This file defines functions related to oriented graph objects
 * */

/*
 * A graph is roughly a set of connected components.
 * A connected component is roughly a set of nodes and edges.
 * A node is the basic building block of a graph. It has a generic payload. Nodes are referenced by edges.
 * An edge links (by references) a source node to a destination node. It has a generic payload.
 * Outside this file, you should call only graph-related functions to create/modify a graph.
 *
 * Typical graph usage:
 *  graph_t *graph = graph_new(); // An empty graph is created
 *  node[0] = graph_add_new_node (graph, node_data[0]); // A node is created and inserted in the graph
 *  node[1] = graph_add_new_node (graph, node_data[1]); // Etc. for other nodes
 *  graph_add_new_edge (graph, node[0], node[1], edge_data[0]); // An edge is created and inserted in the graph. It connects node[0] to node[1]
 *  // recursive traversal from entry nodes via graph_node_DFS/BFS or iterative scan of nodes/edges
 *  graph_free (graph, NULL, NULL); // graph is freed (including edges and nodes themselves)
 *
 * Graph details:
 *  - related functions are named graph_*
 *  - data type is graph_t, fields are:
 *  - node2cc:
 *    + access via get_node2cc()
 *    + set (hashtable_t *) of (key=node, value=connected component) pairs
 *    + provides a unified view of nodes from any connected component,
 *    + allows to lookup (in O(1) cost) a given node and its connected component
 *  - edge2cc: similar 'node2cc'
 *  - connected_components:
 *    + access via get_connected_components()
 *    + set (queue_t *) of connected components
 *
 * Connected component details:
 *  - related functions are named graph_connected_component_*
 *  - data type is graph_connected_component_t, fields are:
 *  - entry_nodes:
 *    + access via get_entry_nodes()
 *    + represents source nodes for traversals, entry nodes have no predecessors (except via backedges)
 *    + entirely under user control (since what is "entry" is context dependent...)
 *    + subset of connected component nodes
 *    + set (hashtable_t *) (key=node, value=node payload) pairs
 *  - nodes:
 *    + access via get_nodes()
 *    + connected components nodes
 *    + same structure as entry_nodes
 *    + allows to lookup (in O(1)) a given node
 *  - edges: similar 'nodes'
 *
 * Node details:
 *  - related functions are named graph_node_*
 *  - data type is graph_node_t, fields are:
 *  - data: payload (void *), access via get/set_data
 *  - in : set (list_t *) of incoming edges, access via get/set_incoming_edges
 *  - out: set (list_t *) of outgoing edges, access via get/set_outgoing_edges
 *  - predecessor/successor nodes can be returned by get/set_successors/predecessors
 *
 * Edge details:
 *  - functions are named graph_edge_*
 *  - data type is graph_edge_t, fields are:
 *  - data: payload (void *), access via get/set_data
 *  - from: source      node (graph_node_t *), access via get/set_src_node
 *  - to  : destination node (graph_node_t *), access via get/set_dst_node
 */

#include "libmcommon.h"

#define GRAPH_MAX_PATHS 1000000 /* Default maximum number of paths counted by graph_node_get_nb_paths */

///////////////////////////////////////////////////////////////////////////////
//                              graph functions                              //
///////////////////////////////////////////////////////////////////////////////

/* Basic accessors */
void   *graph_node_get_data           (graph_node_t *node) { return node ? node->data : PTR_ERROR; }
list_t *graph_node_get_incoming_edges (graph_node_t *node) { return node ? node->in   : PTR_ERROR; }
list_t *graph_node_get_outgoing_edges (graph_node_t *node) { return node ? node->out  : PTR_ERROR; }

void         *graph_edge_get_data     (graph_edge_t *edge) { return edge ? edge->data : PTR_ERROR; }
graph_node_t *graph_edge_get_src_node (graph_edge_t *edge) { return edge ? edge->from : PTR_ERROR; }
graph_node_t *graph_edge_get_dst_node (graph_edge_t *edge) { return edge ? edge->to   : PTR_ERROR; }

/* Basic setters */
void graph_node_set_data (graph_node_t *node, void *data)
{
   if (!node) return;
   node->data = data;
}

void graph_node_set_incoming_edges (graph_node_t *node, list_t *edges)
{
   if (!node) return;
   node->in = edges;
}

void graph_node_set_outgoing_edges (graph_node_t *node, list_t *edges)
{
   if (!node) return;
   node->out = edges;
}

void graph_edge_set_data (graph_edge_t *edge, void *data)
{
   if (!edge) return;
   edge->data = data;
}

void graph_edge_set_src_node (graph_edge_t *edge, graph_node_t *node)
{
   if (!edge) return;
   edge->from = node;
}

void graph_edge_set_dst_node (graph_edge_t *edge, graph_node_t *node)
{
   if (!edge) return;
   edge->to = node;
}

/*
 * Creates a graph node
 * \param data element of the node
 * \return a new graph node
 */
graph_node_t *graph_node_new(void *data)
{
   DBGMSG("Create a new graph node [data = %p]\n", data);
   graph_node_t *new = lc_malloc(sizeof *new);

   new->data = data;
   new->in = new->out = NULL;

   return new;
}

/*
 * TODO: rename into graph_edge_new 
 * Adds an edge between two existing graph nodes
 * \param from source node
 * \param to destination node
 * \param data element of the edge
 * \return a new edge
 */
graph_edge_t *graph_add_edge(graph_node_t *from, graph_node_t *to, void *data)
{
   graph_edge_t *new = lc_malloc(sizeof *new);

   /* Connect the new edge to nodes */
   new->from = from;
   new->to = to;

   new->data = data;

   /* Connect nodes to the new edge */
   from->out = list_add_before(from->out, new);
   to->in = list_add_before(to->in, new);

   return new;
}

/*
 * TODO: rename into graph_edge_new_uniq 
 * Wrapper for graph_add_edge function. Adds an edge only if the edge does not already exist
 * \param from source node
 * \param to destination node
 * \param data an user data
 * \return 1 if the edge is added, else 0
 */
int graph_add_uniq_edge(graph_node_t* from, graph_node_t* to, void* data)
{
   if (!from || !to)
      return 0;

   FOREACH_INLIST(from->out, it) {
      graph_edge_t *tmp = GET_DATA_T(graph_edge_t*, it);

      if (tmp->to == to)
         return 0;
   }

   graph_add_edge(from, to, data);
   return 1;
}

/*
 * Looks for an edge from \e from to \e to, with value equals to \e data
 * \param from source node
 * \param to destination node
 * \param data data of the looked for edge. If NULL, this field is not used for comparison
 * \return an edge if found, else NULL
 */
graph_edge_t *graph_lookup_edge(graph_node_t *from, graph_node_t *to, void *data)
{
   if (!from || !to)
      return NULL;

   FOREACH_INLIST(from->out, it) {
      graph_edge_t* tmp = GET_DATA_T(graph_edge_t*, it);

      if ((tmp->to == to) && (data == NULL || tmp->data == data))
         return tmp;
   }

   return NULL;
}

/*
 * Removes and frees an edge
 * \param edge an edge to free
 * \param f a function used to free an element, with the following prototype:
 *      void < my_function > (void* data); where:
 *                      - data is the element to free
 */
void graph_remove_edge(graph_edge_t *edge, void (*f)(void*))
{
   graph_node_t* from = edge->from;
   graph_node_t* to = edge->to;

   /* Removes the edge from source and destination nodes */
   from->out = list_remove(from->out, edge, NULL);
   to->in = list_remove(to->in, edge, NULL);

   /* Free data if an appropriate routine is provided */
   if (f)
      f(edge->data);

   /* Free edge */
   lc_free(edge);
}

/*
 * Traverses a graph using standard Breadth First Search (BFS) algorithm from a source node.
 * Only nodes accessible from the source node will be explored.
 * \param root a graph (root node)
 * \param func_node a function to execute on a node, with the following prototype:
 *      void < my_function > (graph_node_t* data, void* un); where:
 *                      - data is the element
 *                      - un is a user data
 * \param func_edge a function to execute on an edge, with the following prototype:
 *      void < my_function > (graph_node_t *from, graph_node_t *to); where:
 *                      - from is the begining of an edge
 *                      - to is the end of an edge
 * \param un_data passed to func_node
 */
void graph_node_BFS(graph_node_t *root, void (*func_node)(graph_node_t *, void*),
      void (*func_edge)(graph_node_t *, graph_node_t *), void* un_data)
{
   hashtable_t *mark = hashtable_new(direct_hash, direct_equal);
   queue_t *queue = queue_new();

   /* Insert (enqueue and mark) the root element */
   queue_add_tail(queue, root); /* enqueue */
   hashtable_insert(mark, root, root); /* mark */

   while (!queue_is_empty(queue)) {
      graph_node_t *current_node = queue_remove_head(queue); /* dequeue */

      if (func_node)
         func_node(current_node, un_data);

      FOREACH_INLIST(current_node->out, it)
      {
         graph_edge_t *edge = GET_DATA_T(graph_edge_t*, it);
         graph_node_t *node = edge->to;

         /* If not already visited/inserted */
         if (hashtable_lookup(mark, node) == NULL) {
            hashtable_insert(mark, node, node); /* mark */
            queue_add_tail(queue, node); /* enqueue */
         }

         if (func_edge)
            func_edge(current_node, node);
      }
   }

   queue_free(queue, NULL);
   hashtable_free(mark, NULL, NULL);
}

/* Directly used by set_color and get_color.
 * - GRAY: partially discovered
 * - BLACK: fully discovered
 */
typedef enum {
   GRAY = 1, BLACK
} DFS_color_t;

/** Helper function called by DFS and graph_node_DFS to set the color of a node */
static void set_color(hashtable_t *color_table, const graph_node_t *node,
      DFS_color_t color)
{
   hashtable_insert(color_table, (void *) node, (void *) color);
}

/** Helper function called by DFS and graph_node_DFS to get the color of a node */
static DFS_color_t get_color(hashtable_t *color_table, const graph_node_t *node)
{
   return (long int) hashtable_lookup(color_table, (void *) node);
}

/** Recursive function called by graph_node_DFS to visit a node */
static void DFS(hashtable_t *color_table, graph_node_t *node,
      void (*func_node_before)(graph_node_t *, void *),
      void (*func_node_after)(graph_node_t *, void *),
      void (*func_edge)(graph_edge_t *, void *), void *user_data)
{

   /* Colors the node to GRAY (partially discovered) */
   set_color(color_table, node, GRAY);

   /* Before visiting the node, calls the corresponding function */
   if (func_node_before != NULL)
      func_node_before(node, user_data);

   /* For each child of node */
   FOREACH_INLIST(node->out, e) {
      graph_edge_t *cur_edge = GET_DATA_T(graph_edge_t*, e);
      graph_node_t *child = cur_edge->to;

      /* At this point, it is the first time that cur_edge is encountered */

      /* Before traversing the current edge, calls the corresponding function */
      if (func_edge != NULL)
         func_edge(cur_edge, user_data);

      /* If child not yet discovered, visits it */
      const DFS_color_t color = get_color(color_table, child);
      if ((color != GRAY) && (color != BLACK))
         DFS(color_table, child, func_node_before, func_node_after, func_edge,
               user_data);
   }

   /* Color the node as BLACK (fully discovered) */
   set_color(color_table, node, BLACK);

   /* After visiting the node, calls the corresponding function */
   if (func_node_after != NULL)
      func_node_after(node, user_data);
}

/*
 * Traverses a graph using standard Depth First Search (DFS) algorithm from a source node.
 * Only nodes accessible from the source node will be explored.
 * \param root a graph (root node)
 * \param func_node_before function called before exploring children of a node
 * \param func_node_after function called after exploring children of a node
 * \param func_edge function called before traversing an edge
 * \param user_data pointer passed as parameter to the three previous functions
 */
void graph_node_DFS(graph_node_t *root, void (*func_node_before)(graph_node_t *, void *),
      void (*func_node_after)(graph_node_t *, void *),
      void (*func_edge)(graph_edge_t *, void *), void *user_data)
{

   /* Creates an associative table to color nodes */
   hashtable_t *color_table = hashtable_new(direct_hash, direct_equal);

   /* Visits all nodes from the root node */
   DFS(color_table, root, func_node_before, func_node_after, func_edge,
         user_data);

   /* Frees memory allocated for color_table */
   hashtable_free(color_table, NULL, NULL);
}

/** Recursive function called by graph_node_BackDFS to visit a node */
static void BackDFS(hashtable_t *color_table, graph_node_t *node,
      void (*func_node_before)(graph_node_t *, void *),
      void (*func_node_after)(graph_node_t *, void *),
      void (*func_edge)(graph_edge_t *, void *), void *user_data)
{
   /* Colors the node to GRAY (partially discovered) */
   set_color(color_table, node, GRAY);

   /* Before visiting the node, calls the corresponding function */
   if (func_node_before != NULL)
      func_node_before(node, user_data);

   /* For each child of node */
   FOREACH_INLIST(node->in, e) {
      graph_edge_t *cur_edge = GET_DATA_T(graph_edge_t*, e);
      graph_node_t *child = cur_edge->from;

      /* At this point, it is the first time that cur_edge is encountered */

      /* Before traversing the current edge, calls the corresponding function */
      if (func_edge != NULL)
         func_edge(cur_edge, user_data);

      /* If child not yet discovered, visits it */
      const DFS_color_t color = get_color(color_table, child);
      if ((color != GRAY) && (color != BLACK))
         BackDFS(color_table, child, func_node_before, func_node_after,
               func_edge, user_data);
   }

   /* Color the node as BLACK (fully discovered) */
   set_color(color_table, node, BLACK);

   /* After visiting the node, calls the corresponding function */
   if (func_node_after != NULL)
      func_node_after(node, user_data);
}

/*
 * Traverses a graph using standard Back Depth First Search (DFS) algorithm from a source node.
 * Only nodes accessible from the source node will be explored.
 * \param root a graph (root node)
 * \param func_node_before function called before exploring children of a node
 * \param func_node_after function called after exploring children of a node
 * \param func_edge function called before traversing an edge
 * \param user_data pointer passed as parameter to the three previous functions
 */
void graph_node_BackDFS(graph_node_t *root, void (*func_node_before)(graph_node_t *, void *),
      void (*func_node_after)(graph_node_t *, void *),
      void (*func_edge)(graph_edge_t *, void *), void *user_data)
{

   /* Creates an associative table to color nodes */
   hashtable_t *color_table = hashtable_new(direct_hash, direct_equal);

   /* Visits all nodes from the root node */
   BackDFS(color_table, root, func_node_before, func_node_after, func_edge,
         user_data);

   /* Frees memory allocated for color_table */
   hashtable_free(color_table, NULL, NULL);
}

/** Helper function called by graph_node_get_accessible_nodes that enqueues a node */
static void add_node_to_array(graph_node_t *node, void *user)
{
   array_add((array_t *) user, node);
}

/*
 * Returns nodes accessible from a root node, using a DFS.
 * \param root a graph (root node)
 * \return set of nodes (as dynamic array)
 */
array_t *graph_node_get_accessible_nodes(const graph_node_t *root)
{
   array_t *nodes = array_new();

   graph_node_DFS((graph_node_t *) root, add_node_to_array, NULL, NULL, nodes);

   return nodes;
}

/** Hashtable used by graph_node_topological_sort, to save DFS end time for each node */
static hashtable_t *end_time_table;

/** Helper function passed to graph_node_DFS when called by graph_node_topological_sort */
static void inc_time(graph_node_t *node, void *user_data)
{
   (void) node;
   (*((int *) user_data))++;
}

/** Helper function passed to graph_node_DFS when called by graph_node_topological_sort */
static void update_end_time_table(graph_node_t *node, void *user_data)
{
   ptrdiff_t time = *((int *) user_data);

   hashtable_insert(end_time_table, node, (void *) time);
   inc_time(node, user_data);
}

/** Helper function passed to array_sort when called by graph_node_topological_sort */
static int cmp_nodes(const void *a, const void *b)
{
   ptrdiff_t end_time_a = (ptrdiff_t) hashtable_lookup(end_time_table,
         *((void **) a));
   ptrdiff_t end_time_b = (ptrdiff_t) hashtable_lookup(end_time_table,
         *((void **) b));

   return end_time_b - end_time_a;
}

/*
 * Topologically sorts a graph from a root node.
 * \param root a graph (root node)
 * \return sorted set of nodes (as dynamic array)
 */
array_t *graph_node_topological_sort(const graph_node_t *root)
{
   int time = 0; /* Incremented before and after discovering a node during DFS */

   /* Creates an hashtable to associate an end time to each node */
   end_time_table = hashtable_new(direct_hash, direct_equal);

   /* Explores the graph from the root node using DFS and, during exploration,
    * keep at each time each node were explored */
   graph_node_DFS((graph_node_t *) root, inc_time, update_end_time_table, NULL, &time);

   /* Gets the list of nodes accessible from the root node */
   array_t *nodes = graph_node_get_accessible_nodes(root);

   /* Sorts the list */
   array_sort(nodes, cmp_nodes);

   /* Frees memory allocated for end_time_table */
   hashtable_free(end_time_table, NULL, NULL);

   return nodes;
}

struct DFS_container_s {
   hashtable_t* backedges_table;
   hashtable_t* color_table;
};

/** Helper function passed to graph_node_DFS when called by graph_node_get_backedges_table
 * to update a backedges table */
static void update_backedges_table(graph_edge_t *edge, void *user_data)
{
   struct DFS_container_s* container = user_data;
   hashtable_t *backedges_table = container->backedges_table;

   if (get_color(container->color_table, edge->to) == GRAY)
      hashtable_insert(backedges_table, edge, (void *) 1);
}

/*
 * Returns a table that can be passed to graph_is_backedge_from_table
 * \param root a graph (root node)
 * \return a table for graph_is_backedge_from_table
 */
hashtable_t *graph_node_get_backedges_table(const graph_node_t *root)
{
   /* Creates an associative table to color nodes */
   hashtable_t *color_table = hashtable_new(direct_hash, direct_equal);

   struct DFS_container_s container;
   container.backedges_table = hashtable_new(direct_hash, direct_equal);
   container.color_table = color_table;

   /* Visits all nodes from the root node */
   DFS(color_table, (graph_node_t*) root, NULL, NULL, update_backedges_table,
         &container);

   /* Frees memory allocated for color_table */
   hashtable_free(color_table, NULL, NULL);

   return container.backedges_table;
}

/*
 * Checks whether an edge is a backedge, provided a table returned by graph_node_get_backedges_table.
 * Use this function to check at least two or three edges into a graph. In case of only one edge, graph_is_backedge_from_graph_node is simpler to use.
 * \param edge an edge
 * \param bet an object returned by graph_node_get_backedges_table
 * \return TRUE or FALSE
 */
int graph_is_backedge_from_table(const graph_edge_t *edge, const hashtable_t *bet)
{
   return (hashtable_lookup((hashtable_t *) bet, edge) != NULL) ? TRUE : FALSE;
}

/*
 * Checks whether an edge is a backedge, provided a graph (a root node).
 * Use this function to check only one or two edges into a graph. In other cases, use graph_node_get_backedges_table.
 * \param edge an edge
 * \param root a graph (root node)
 * \return TRUE or FALSE
 */
int graph_is_backedge_from_graph_node(const graph_edge_t *edge, const graph_node_t *root)
{
   hashtable_t *bet = graph_node_get_backedges_table(root);

   int ret = graph_is_backedge_from_table(edge, bet);

   hashtable_free(bet, NULL, NULL);

   return ret;
}

/** Helper function that returns the number of non-back edges of a node */
/* TODO: return a queue of non-back edges instead and test if perf improvements */
/* In update_pred_paths, edges are scanned 1+nb_paths times */
static int get_nb_children(const graph_node_t *node, const hashtable_t *bet)
{
   int nb_children = 0;

   FOREACH_INLIST(node->out, e) {
      const graph_edge_t *cur_edge = GET_DATA_T(graph_edge_t*, e);

      /* Ignoring backedges */
      if (!graph_is_backedge_from_table(cur_edge, bet))
         nb_children++;
   }

   return nb_children;
}

/**
 * Helper function, called by graph_node_compute_paths.
 * Updates paths ending with a given node
 * \param node a graph node
 * \param paths current set of paths built by graph_node_compute_paths
 * \param bet backedges hashtable, allows to use graph_is_backedge_from_table
 */
static void update_pred_paths(const graph_node_t *node, queue_t *paths,
      const hashtable_t *bet)
{
   array_t *paths_to_remove = array_new();
   array_t *paths_to_add = array_new();

   /* Counts the number of children (non-back edges) */
   const int nb_children = get_nb_children(node, bet);

   /* For each path */
   FOREACH_INQUEUE(paths, p) {
      array_t *cur_path = GET_DATA_T(array_t *, p);
      graph_node_t *last_node = array_get_last_elt (cur_path);

      /* We only consider paths ending with the current node */
      if (last_node != node)
         continue;

      /* If only one child, appends it to the current path */
      if (nb_children == 1) {
         /* For each output edge */
         FOREACH_INLIST(node->out, e)
         {
            const graph_edge_t *cur_edge = GET_DATA_T(graph_edge_t*, e);

            if (!graph_is_backedge_from_table(cur_edge, bet)) {
               graph_node_t *child_node = cur_edge->to;

               array_add(cur_path, child_node);
            }
         }
      }

      /* If more than one children */
      else if (nb_children > 1) {
         /* For each output edge */
         FOREACH_INLIST(node->out, e) {
            const graph_edge_t *cur_edge = GET_DATA_T(graph_edge_t*, e);

            if (!graph_is_backedge_from_table(cur_edge, bet)) {
               graph_node_t *child_node = cur_edge->to;

               /* Adds a new path appended with the child node */
               array_t *copy = array_dup(cur_path);
               array_add(copy, child_node);
               array_add(paths_to_add, copy);
            }
         }

         /* Adds the current path to the list of paths to remove */
         array_add(paths_to_remove, p);
      } /* else if (nb_children > 1) */
   } /* for each path */

   /* Remove from 'paths' the paths to remove */
   FOREACH_INARRAY(paths_to_remove, p2) {
      list_t *cur_data = ARRAY_GET_DATA(cur_data, p2);
      array_t *cur_path = queue_remove_elt(paths, cur_data);
      array_free(cur_path, NULL);
   }

   array_free(paths_to_remove, NULL);

   /* Add into 'paths' the paths to add */
   FOREACH_INARRAY(paths_to_add, p3) {
      array_t *cur_path = ARRAY_GET_DATA(cur_path, p3);

      queue_add_tail(paths, cur_path);
   }

   array_free(paths_to_add, NULL);
}

/*
 * Computes and returns paths (as a queue) in a graph, starting from a source node.
 * Warning: The number of paths is not verified before computing them. It is up to the user
 * to call graph_node_get_nb_paths before and decide if it is worth calling graph_node_compute_paths.
 * \param root a graph (root node)
 * \return queue of paths starting from root
 */
queue_t *graph_node_compute_paths(const graph_node_t *root)
{
   /* Creates a first path containing the root node */
   array_t *root_path = array_new();
   array_add(root_path, (void *) root);

   /* Creates an empty set of paths and inserts root_path into it */
   queue_t *paths = queue_new();
   queue_add_tail(paths, root_path);

   /* Topologically sorts nodes */
   array_t *sorted_nodes = graph_node_topological_sort(root);

   /* Registers backedge in an hashtable for a fast checks */
   hashtable_t *bet = graph_node_get_backedges_table(root);

   /* For each node, by topological order */
   FOREACH_INARRAY(sorted_nodes, n) {
      const graph_node_t *cur_node = ARRAY_GET_DATA(cur_node, n);

      /* Updates paths ending with cur_node */
      update_pred_paths(cur_node, paths, bet);
   }

   /* Frees memory allocated for sorted_nodes_queue and bet) */
   array_free(sorted_nodes, NULL);
   hashtable_free(bet, NULL, NULL);

   return paths;
}

/*
 * Frees the memory allocated for given paths
 * \param paths paths to free
 */
void graph_free_paths(queue_t *paths)
{
   FOREACH_INQUEUE(paths, path_iter) {
      array_t *path = GET_DATA_T(array_t*, path_iter);

      array_free(path, NULL);
   }

   queue_free(paths, NULL);
}

/**
 * Helper function that updates the number of paths defined in graph_node_get_nb_paths.
 * Recursive. Originally called from graph_node_get_nb_paths.
 */
static void update_nb_paths(const graph_node_t *node, const hashtable_t *bet,
      int *nb_paths, int max_paths)
{
   if (*nb_paths >= max_paths)
      return;

   /* Counts the number of times we arrived to a leaf node (ignoring backedges) */
   if (get_nb_children(node, bet) == 0) {
      *nb_paths = *nb_paths + 1;
      return;
   }

   FOREACH_INLIST(node->out, e)
   {
      const graph_edge_t *cur_edge = GET_DATA_T(graph_edge_t*, e);

      if (!graph_is_backedge_from_table(cur_edge, bet))
         update_nb_paths(cur_edge->to, bet, nb_paths, max_paths);
   }
}

/*
 * Gets the number of paths in a graph starting from a given source (root) node.
 * Paths are not computed (faster than first computing paths and count them).
 * \param root a graph (root node)
 * \param max_paths maximum number of paths that the user accepts to compute. A negative or zero value is equivalent to MAX_PATHS
 * \return paths number
 */
int graph_node_get_nb_paths(const graph_node_t *root, int max_paths)
{
   if (max_paths <= 0)
      max_paths = GRAPH_MAX_PATHS;

   int nb_paths = 0;

   /* Registers backedge in an hashtable for a fast checks */
   hashtable_t *bet = graph_node_get_backedges_table(root);

   /* Updates the number of paths for the root node */
   update_nb_paths(root, bet, &nb_paths, max_paths);

   /* Frees memory allocated for bet */
   hashtable_free(bet, NULL, NULL);

   return nb_paths;
}

/*
 * Removes and frees a node and frees all edges linked to it
 * \see graph_free_from_nodes to easily and quickly free an entire graph (connected component) or, better, use new interface (based on graph_connected_component_t)
 * \param node a node to remove
 * \param f_node a function used to free an element of a node, with the following prototype:
 *      void < my_function > (void* data); where:
 *                      - data is the element to free
 * \param f_edge a function used to free an element of an edge, with the following prototype:
 *      void < my_function > (void* data); where:
 *                      - data is the element to free
 */
void graph_node_free(graph_node_t* node, void (*f_node)(void*), void (*f_edge)(void*))
{
   /* Remove/free input edges (and the list referencing them) */
   while (node->in != NULL)
      graph_remove_edge(node->in->data, f_edge);
   list_free(node->in, NULL);

   /* Idem for output edges */
   while (node->out != NULL)
      graph_remove_edge(node->out->data, f_edge);
   list_free(node->out, NULL);

   /* Remove the node itself */
   if (f_node != NULL)
      f_node(node->data);
   lc_free(node);
}

static void _graph_free_from_nodes(array_t *nodes, hashtable_t *nodes_to_free, hashtable_t *edges_to_free) {
   FOREACH_INARRAY (nodes, node_iter) {
      graph_node_t *node = ARRAY_GET_DATA (node, node_iter);

      /* If node not yet visited */
      if (hashtable_lookup(nodes_to_free, node) == NULL) {
         /* mark node: must be freed */
         hashtable_insert(nodes_to_free, node, node);

         /* mark incoming edges: must be freed */
         FOREACH_INLIST(node->in, edge_in_iter) {
            graph_edge_t *edge = GET_DATA_T(graph_edge_t*, edge_in_iter);
            if (hashtable_lookup(edges_to_free, edge) == NULL)
               hashtable_insert(edges_to_free, edge, edge);
         }

         /* mark outgoing edges: must be freed */
         FOREACH_INLIST(node->out, edge_out_iter) {
            graph_edge_t *edge = GET_DATA_T(graph_edge_t*, edge_out_iter);
            if (hashtable_lookup(edges_to_free, edge) == NULL)
               hashtable_insert(edges_to_free, edge, edge);
         }
      }
   }
}

/* Used by graph_free_DFS and graph_connected_component_free */
static void _free_node(void *key, void *data, void *user)
{
   /* Remove unused parameter warnings */
   (void) data;

   graph_node_t *node = key;
   void (*f_node) (void*) = user;

   list_free (node->in , NULL);
   list_free (node->out, NULL);

   if (f_node != NULL) f_node (node->data);
   lc_free (node);
}

/* Used by graph_free_DFS and graph_connected_component_free */
static void _free_edge(void *key, void *data, void *user)
{
   /* Remove unused parameter warnings */
   (void) data;

   graph_edge_t *edge = key;
   void (*f_edge) (void*) = user;

   if (f_edge != NULL) f_edge (edge->data);
   lc_free (edge);
}

/*
 * TODO: use graph_free or graph_connected_component_free instead
 * Frees an entire graph (connected component) defined by its nodes.
 * This function is fast since graph_node_free is bypassed (hashtables used to mark nodes and edges)
 * \param nodes queue of graph nodes
 * \see graph_node_free for f_node and f_edge
 */
void graph_free_from_nodes(array_t *nodes, void (*f_node)(void*), void (*f_edge)(void*))
{
   /* Create hashtables to avoid freeing multiple times nodes and edges */
   hashtable_t *nodes_to_free = hashtable_new (direct_hash, direct_equal);
   hashtable_t *edges_to_free = hashtable_new (direct_hash, direct_equal);

   _graph_free_from_nodes (nodes, nodes_to_free, edges_to_free);

   /* Now we can safely free nodes and edges */
   hashtable_foreach(nodes_to_free, _free_node, f_node);
   hashtable_free(nodes_to_free, NULL, NULL);

   hashtable_foreach(edges_to_free, _free_edge, f_edge);
   hashtable_free(edges_to_free, NULL, NULL);
}

/*
 * Checks whether a graph is consistent
 * More precisely, check consistence between nodes (referencing edges) and edges (referencing nodes).
 * Should be
 * \param root a graph (root node)
 * \return FALSE or TRUE
 */
int graph_node_is_consistent(const graph_node_t *root)
{
   array_t *accessible_nodes = graph_node_get_accessible_nodes(root);

   FOREACH_INARRAY(accessible_nodes, node_iter) {
      graph_node_t *node = ARRAY_GET_DATA (node, node_iter);

      /* If a node 'node' references an incoming edge 'edge', 'edge'->to must equals 'node' */
      FOREACH_INLIST(node->in, edge_to_iter) {
         graph_edge_t *edge = GET_DATA_T(graph_edge_t*, edge_to_iter);

         if (edge->to != node) {
            array_free(accessible_nodes, NULL);
            return FALSE;
         }
      }

      /* If a node 'node' references an outcoming edge 'edge', 'edge'->from must equals 'node' */
      FOREACH_INLIST(node->out, edge_from_iter)
      {
         graph_edge_t *edge = GET_DATA_T(graph_edge_t*, edge_from_iter);

         if (edge->from != node) {
            array_free(accessible_nodes, NULL);
            return FALSE;
         }
      }
   }

   array_free(accessible_nodes, NULL);
   return TRUE;
}

/*
 * Returns predecessors of a node
 * \param node a node
 * \return array of nodes or PTR_ERROR
 */
array_t *graph_node_get_predecessors (graph_node_t *node)
{
   if (node == NULL) return PTR_ERROR;

   list_t *incoming_edges = graph_node_get_incoming_edges (node);
   const int nb_pred = list_length (incoming_edges);
   array_t *new = array_new_with_custom_size (nb_pred);

   FOREACH_INLIST(incoming_edges, iter) {
      graph_node_t *pred_node = GET_DATA_T (graph_edge_t *, iter)->from;
      array_add (new, pred_node);
   }

   return new;
}

/*
 * Returns successors of a node
 * \param node a node
 * \return array of nodes or PTR_ERROR
 */
array_t *graph_node_get_successors (graph_node_t *node)
{
   if (node == NULL) return PTR_ERROR;

   list_t *outgoing_edges = graph_node_get_outgoing_edges (node);
   const int nb_succ = list_length (outgoing_edges);
   array_t *new = array_new_with_custom_size (nb_succ);

   FOREACH_INLIST(outgoing_edges, iter) {
      graph_node_t *succ_node = GET_DATA_T (graph_edge_t *, iter)->to;
      array_add (new, succ_node);
   }

   return new;
}

///////////////////////////////////////////////////////////////////////////////
//               new graph functions: using connected components             //
///////////////////////////////////////////////////////////////////////////////

/*
 * Creates a graph connected component
 * \return new graph connected component
 */
static graph_connected_component_t *graph_connected_component_new ()
{
   graph_connected_component_t *new = lc_malloc (sizeof *new);
   if (!new) return PTR_ERROR;

   new->entry_nodes = hashtable_new (direct_hash, direct_equal);
   new->nodes       = hashtable_new (direct_hash, direct_equal);
   new->edges       = hashtable_new (direct_hash, direct_equal);

   return new;
}

/*
 * Returns entry nodes from a graph connected component
 * \param cc connected component
 * \return array of entry nodes
 */
hashtable_t *graph_connected_component_get_entry_nodes (graph_connected_component_t *cc)
{
   return cc ? cc->entry_nodes : PTR_ERROR;
}

/*
 * Returns nodes from a graph connected component
 * \param cc connected component
 * \return hashtable of nodes
 */
hashtable_t *graph_connected_component_get_nodes (graph_connected_component_t *cc)
{
   return cc ? cc->nodes : PTR_ERROR;
}

/*
 * Returns edges from a graph connected component
 * \param cc connected component
 * \return hashtable of edges
 */
hashtable_t *graph_connected_component_get_edges (graph_connected_component_t *cc)
{
   return cc ? cc->edges : PTR_ERROR;
}

/*
 * Adds an already existing node into a connected component
 * \param cc connected component
 * \param node node to add
 */
static void graph_connected_component_add_node (graph_connected_component_t *cc, graph_node_t *node)
{
   if (!cc || !node) return;

   hashtable_t *nodes = graph_connected_component_get_nodes (cc);
   if (hashtable_lookup (nodes, node)) return; // node already present

   hashtable_insert (nodes, node, GET_DATA(node));
}

/*
 * Adds an already existing edge into a connected component
 * \note no check: nodes referenced by edge are considered as belonging to cc
 * \param cc connected component
 * \param edge edge to add
 */
static void graph_connected_component_add_edge (graph_connected_component_t *cc, graph_edge_t *edge)
{
   if (!cc || !edge) return;

   hashtable_t *edges = graph_connected_component_get_edges (cc);
   if (hashtable_lookup (edges, edge)) return; // edge already existing

   hashtable_insert (edges, edge, GET_DATA(edge));
}

/*
 * Merges two graph connected components: copy data from cc2 to cc1
 * \note nothing is deleted in cc2 + no check on nodes/edges (operation is supposed to be consistent)
 * \param cc1 destination connected component
 * \param cc2 source connected component
 */
static void graph_connected_component_merge (graph_connected_component_t *cc1, graph_connected_component_t *cc2)
{
   if (!cc1 || !cc2) return;

   // Merge entry nodes
   hashtable_t *cc1_entry_nodes = graph_connected_component_get_entry_nodes (cc1);
   hashtable_t *cc2_entry_nodes = graph_connected_component_get_entry_nodes (cc2);
   hashtable_copy (cc1_entry_nodes, cc2_entry_nodes);

   // Merge nodes
   hashtable_t *cc1_nodes = graph_connected_component_get_nodes (cc1);
   hashtable_t *cc2_nodes = graph_connected_component_get_nodes (cc2);
   hashtable_copy (cc1_nodes, cc2_nodes);

   // Merge edges
   hashtable_t *cc1_edges = graph_connected_component_get_edges (cc1);
   hashtable_t *cc2_edges = graph_connected_component_get_edges (cc2);
   hashtable_copy (cc1_edges, cc2_edges);
}

/*
 * Frees a graph connected component (but nodes/edges themselves are preserved)
 * \param cc connected component to free
 */
static void graph_connected_component_free (graph_connected_component_t *cc)
{
   if (!cc) return;

   // Free entry nodes table (not nodes themselves)
   hashtable_t *entry_nodes = graph_connected_component_get_entry_nodes (cc);
   hashtable_free (entry_nodes, NULL, NULL);

   // Free nodes table (not nodes themselves)
   hashtable_t *nodes = graph_connected_component_get_nodes (cc);
   hashtable_free (nodes, NULL, NULL);

   // Free edges table (not edges themselves)
   hashtable_t *edges = graph_connected_component_get_edges (cc);
   hashtable_free (edges, NULL, NULL);

   lc_free (cc);
}

/*
 * Creates a graph (set of connected components)
 * \return new graph or PTR_ERROR in case of failure
 */
graph_t *graph_new ()
{
   graph_t *new = lc_malloc (sizeof *new);
   if (!new) return PTR_ERROR;

   new->connected_components = queue_new ();
   new->node2cc = hashtable_new (direct_hash, direct_equal);
   new->edge2cc = hashtable_new (direct_hash, direct_equal);

   return new;
}

/*
 * Returns connected components of a graph
 * \param graph a graph
 * \return queue of connected components
 */
queue_t *graph_get_connected_components (graph_t *graph)
{
   return graph ? graph->connected_components : PTR_ERROR;
}

/*
 * Returns node to connected component index of a graph
 * \param graph a graph
 * \return node to cc index (hashtable of (node,cc) pairs)
 */
hashtable_t *graph_get_node2cc (graph_t *graph)
{
   return graph ? graph->node2cc : PTR_ERROR;
}

/*
 * Returns edge to connected component index of a graph
 * \param graph a graph
 * \return edge to cc index (hashtable of (edge,cc) pairs)
 */
hashtable_t *graph_get_edge2cc (graph_t *graph)
{
   return graph ? graph->edge2cc : PTR_ERROR;
}

/*
 * Creates a node from input data and inserts it into a graph
 * \param graph graph to fill
 * \param data data to save in new node
 * \return new node
 */
graph_node_t *graph_add_new_node (graph_t *graph, void *data)
{
   if (!graph) return PTR_ERROR;

   graph_node_t *new_node = graph_node_new (data);
   if (!new_node) return PTR_ERROR;

   graph_connected_component_t *new_cc = graph_connected_component_new ();
   if (!new_cc) return PTR_ERROR;

   graph_connected_component_add_node (new_cc, new_node);

   queue_add_tail (graph_get_connected_components (graph), new_cc);
   hashtable_insert (graph_get_node2cc (graph), new_node, new_cc);

   return new_node;
}

/*
 * Creates an edge from input nodes (first one being origin) and inserts it into a graph
 * \param graph graph to fill
 * \param n1 origin node
 * \param n2 destination node
 * \param data data to save in new edge
 * \return new edge
 */
graph_edge_t *graph_add_new_edge (graph_t *graph, graph_node_t *n1, graph_node_t *n2, void *data)
{
   if (!graph || !n1 || !n2) return PTR_ERROR;

   hashtable_t *node2cc = graph_get_node2cc (graph);
   graph_connected_component_t *cc1 = hashtable_lookup (node2cc, n1);
   graph_connected_component_t *cc2 = hashtable_lookup (node2cc, n2);
   if (!cc1 || !cc2) {
      fprintf (stderr, "graph_add_edge_in_graph: one or both nodes are not in the graph\n");
      return PTR_ERROR;
   }

   graph_edge_t *new_edge = graph_add_edge (n1, n2, data);
   if (!new_edge) return PTR_ERROR;

   // If edge crosses 2 connected components, need to merge them (append in first, remove second one)
   if (cc1 != cc2) {
      queue_remove (graph_get_connected_components (graph), cc2, NULL);
      graph_connected_component_merge (cc1, cc2);
      {
         FOREACH_INHASHTABLE (graph_get_node2cc (graph), node2cc_iter) {
            if (node2cc_iter->data == cc2)
               node2cc_iter->data = cc1;
         }
      }
      {
         FOREACH_INHASHTABLE (graph_get_edge2cc (graph), edge2cc_iter) {
            if (edge2cc_iter->data == cc2)
               edge2cc_iter->data = cc1;
         }
      } 
      graph_connected_component_free (cc2);
   }

   graph_connected_component_add_edge (cc1, new_edge);
   hashtable_insert (graph_get_edge2cc (graph), new_edge, cc1);

   return new_edge;
}

/*
 * Wrapper for graph_add_new_edge function. Adds an edge only if the edge does not already exist
 * \param n1 origin node
 * \param n2 destination node
 * \param data user data
 * \return 1 if the edge is added, else 0
 */
int graph_add_new_edge_uniq (graph_t *graph, graph_node_t *n1, graph_node_t *n2, void *data)
{
   if (!n1 || !n2) return 0;

   FOREACH_INLIST (n1->out, it) {
      graph_edge_t *tmp = GET_DATA_T(graph_edge_t*, it);
      if (tmp->to == n2) return 0;
   }

   graph_add_new_edge (graph, n1, n2, data);
   return 1;
}

/*
 * Frees a graph (set of connected components
 * \param graph graph to free
 * \param f_node function used to free a node
 * \param f_edge function used to free an edge
 * \see graph_node_free for f_node and f_edge
 */
void graph_free (graph_t *graph, void (*f_node)(void*), void (*f_edge)(void*))
{
   if (!graph) return;

   queue_t *connected_components = graph_get_connected_components (graph);
   FOREACH_INQUEUE (connected_components, cc_iter) {
      graph_connected_component_t *cc = GET_DATA_T (graph_connected_component_t *, cc_iter);

      // Frees nodes
      hashtable_t *nodes = graph_connected_component_get_nodes (cc);
      hashtable_foreach (nodes, _free_node, f_node);

      // Frees edges
      hashtable_t *edges = graph_connected_component_get_edges (cc);
      hashtable_foreach (edges, _free_edge, f_edge);

      graph_connected_component_free (cc);
   }

   queue_free (connected_components, NULL);
   hashtable_free (graph_get_node2cc (graph), NULL, NULL);
   hashtable_free (graph_get_edge2cc (graph), NULL, NULL);
   lc_free (graph);
}

void graph_for_each_path (graph_t *graph, int max_paths,
                          void (*fct) (array_t *, void *), void *data)
{
   /* For each connected component */
   queue_t *connected_components = graph_get_connected_components (graph);
   FOREACH_INQUEUE (connected_components, cc_iter) {
      graph_connected_component_t *cc = GET_DATA_T (graph_connected_component_t *, cc_iter);

      /* For each entry node */
      FOREACH_INHASHTABLE(graph_connected_component_get_entry_nodes(cc), entry_node_iter) {
         graph_node_t *entry_node = GET_KEY(graph_node_t*, entry_node_iter);

         if (graph_node_get_nb_paths(entry_node, max_paths) >= max_paths)
            continue;

         /* For each path */
         queue_t *paths = graph_node_compute_paths(entry_node);
         FOREACH_INQUEUE(paths, paths_iter) {
            array_t *path = GET_DATA_T(array_t*, paths_iter);
            fct (path, data);
         }
         queue_free (paths, NULL);
      } /* entry node */
   } /* cc */
}

array_t *graph_cycle_get_edges (queue_t *cycle, boolean_t (*ignore_edge) (const graph_edge_t *edge))
{
   array_t *edges = array_new();

   /* For each node */
   FOREACH_INQUEUE(cycle, cycle_iter) {
      graph_node_t *cur_node = GET_DATA_T(graph_node_t*, cycle_iter);
      graph_node_t *nxt_node =
            list_getnext(cycle_iter) ?
                  GET_DATA_T(graph_node_t*, list_getnext(cycle_iter)) :
                  queue_peek_head(cycle);

      /* Looking for the first compatible edge (skipping edges that must be ignored) */
      FOREACH_INLIST(cur_node->out, out_iter)
      { /* For each edge exiting cur_node */
         graph_edge_t *edge = GET_DATA_T(graph_edge_t*, out_iter);

         if (edge->to != nxt_node)
            continue; /* not in the cycle */

         if (ignore_edge (edge)) continue;

         array_add (edges, edge);
         break;
      }
   }

   return edges;
}

/*
 * Returns path cycles. If path = A->B->C and C->B, returns B->C
 */
static array_t *get_cycles (array_t *path, boolean_t (*ignore_edge) (const graph_edge_t *edge))
{
   array_t *cycles = array_new();
   hashtable_t *ranks = hashtable_new(direct_hash, direct_equal);

   /* Associate each node to its rank */
   long rank = 1;
   FOREACH_INARRAY(path, path_iter1) {    
      hashtable_insert(ranks, *path_iter1, (void *) rank++);
   }

   long dst_rank = 1;
   /* For each node in the path */
   FOREACH_INARRAY(path, path_iter2) {
      graph_node_t *node = ARRAY_GET_DATA(node, path_iter2);

      /* For each edge arriving to node */
      FOREACH_INLIST(node->in, in_iter) {
         graph_edge_t *edge = GET_DATA_T(graph_edge_t*, in_iter);

         if (ignore_edge (edge)) continue;

         long src_rank = (long) hashtable_lookup(ranks, edge->from);

         /* If coming from a path node */
         if (src_rank && src_rank >= dst_rank) {
            /* Add a new cycle */
            queue_t *cycle = queue_new();
            rank = 1;
            FOREACH_INARRAY(path, path_iter3) {
               if (rank > src_rank)
                  break;
               if (rank >= dst_rank)
                  queue_add_tail(cycle, *path_iter3);
               rank++;
            }
            array_add(cycles, cycle);
         } /* if coming from a path node */
      } /* for each incoming edge */

      dst_rank++;
   } /* for each node */

   hashtable_free(ranks, NULL, NULL);

   return cycles;
}

typedef struct {
   boolean_t (*ignore_edge) (const graph_edge_t *edge);
   void (*cycle_fct) (queue_t *cycle, void *);
   void *cycle_data;
} _upd_path_data_t;

static void _upd_path (array_t *path, void *_data)
{
   _upd_path_data_t data = *((_upd_path_data_t *) _data);

   /* For each cycle */
   array_t *cycles = get_cycles (path, data.ignore_edge);
   FOREACH_INARRAY(cycles, cycles_iter) {
      queue_t *cycle = ARRAY_GET_DATA (cycle, cycles_iter);
      data.cycle_fct (cycle, data.cycle_data);
   }
   array_free (cycles, NULL);
}

void graph_for_each_cycle (graph_t *graph, int max_paths,
                           boolean_t (*ignore_edge) (const graph_edge_t *edge),
                           void (*fct) (queue_t *cycle, void *), void *data)
{
   _upd_path_data_t upd_path_data = { ignore_edge, fct, data };
   graph_for_each_path (graph, max_paths, _upd_path, &upd_path_data);
}

static float _get_path_len (array_t *path, float (*get_edge_weight) (graph_edge_t *))
{
   int sum = 0;

   int i;
   /* For each edge in the path, from node to node */
   for (i=0; i < array_length (path)-1; i++) {
      graph_node_t *cur_node = ARRAY_ELT_AT_POS (path, i);   // current node
      graph_node_t *nxt_node = ARRAY_ELT_AT_POS (path, i+1); // next node
      graph_edge_t *edge = NULL; // from cur_node to nxt_node

      /* Look for the first edge linking cur_node to nxt_node */
      FOREACH_INLIST(cur_node->out, out_iter) {
         edge = GET_DATA_T(graph_edge_t*, out_iter);
         if (edge->to == nxt_node) break;
      }

      sum += get_edge_weight (edge);
   }

   return sum;
}

/*
 * Updates critical paths for a graph
 * Passed to the graph_for_each_path function that will invoke on each path
 * \param path a path (array of nodes)
 * \param data parameters (cast to graph_update_critical_paths_data_t *)
 */
void graph_update_critical_paths (array_t *path, void *data)
{
   graph_update_critical_paths_data_t *crit_paths = data;

   float len = _get_path_len (path, crit_paths->get_edge_weight);

   // Update critical paths list
   if (crit_paths->max_length < len) {
      array_flush (crit_paths->paths, NULL);
      array_add (crit_paths->paths, path);
      crit_paths->max_length = len;
   } else if (crit_paths->max_length == len) {
      array_add (crit_paths->paths, path);
   }
}

/*
 * Returns critical paths for a graph
 * \param graph a graph
 * \param max_paths maximum number of paths to consider from an entry node
 * \param crit_paths pointer to an array, set to critical paths
 * \param get_edge_weight function to return the weight of an edge (providing genericity)
 */
void graph_get_critical_paths (graph_t *graph, int max_paths,
                               array_t **crit_paths,
                               float (*get_edge_weight) (graph_edge_t *))
{
   graph_update_critical_paths_data_t crit_paths_data = { 0.0f, array_new(), get_edge_weight };
   graph_for_each_path (graph, max_paths, graph_update_critical_paths, &crit_paths_data);

   *crit_paths = crit_paths_data.paths;
}
