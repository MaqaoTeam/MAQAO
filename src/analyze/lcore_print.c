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
 * \file
 * */

#include <sys/stat.h>
#include "libmcore.h"

///////////////////////////////////////////////////////////////////////////////
//                             DDG printing                                  //
///////////////////////////////////////////////////////////////////////////////

/*
 * \struct fileandlist_t
 * Used to pass user data to graph_node_DFS
 */
typedef struct {
   FILE * file; /**<dot file descriptor*/
   queue_t * list; /**<list of already printed nodes*/
} fileandlist_t;

/*
 * Prints a DDG node and its input edges (passed to graph_node_DFS)
 * \param node node to print
 * \param user data passed by graph_node_DFS
 */
static void print_DDG_node(graph_node_t *node, void *user)
{
   fileandlist_t *fileandlist = user;

   /* Retrieve node data */
   insn_t *insn = node->data;
   int64_t insn_addr = insn->address;
   FILE *dotfile = fileandlist->file;
   queue_t *list = fileandlist->list;

   /* Check if the node was already printed */
   FOREACH_INQUEUE(list, list_iter) {
      insn_t *node = GET_DATA_T(insn_t*, list_iter);

      if (node->address == insn_addr)
         return;
   }

   /* If the node was not already printed... */

   /* Print the node ... */
   char insn_asmcode[100];
   insn_asmcode[0] = '\0';
   if (insn != NULL && insn->block != NULL && insn->block->function != NULL)
      insn_print(insn, insn_asmcode, sizeof(insn_asmcode));
   fprintf(dotfile, "%ld[label=\"%ld : %s\"];\n", insn_addr, insn_addr,
         insn_asmcode);

   /* ... and each input edge */
   FOREACH_INLIST(node->in, in_iter) {
      graph_edge_t *edge = GET_DATA_T(graph_edge_t*, in_iter);
      data_dependence_t * data_dependence = edge->data;
      insn_t *src_insn = ((graph_node_t *) edge->from)->data;

      if (data_dependence->latency.min == data_dependence->latency.max)
         fprintf(dotfile, "\"%ld\"->\"%ld\"[label=\"%s_lat=%hu_dist=%d\"]; \n",
                 src_insn->address, insn_addr, data_dependence->kind,
                 data_dependence->latency.min, data_dependence->distance);
      else
         fprintf(dotfile, "\"%ld\"->\"%ld\"[label=\"%s_lat=%hu-%hu_dist=%d\"]; \n",
                 src_insn->address, insn_addr, data_dependence->kind,
                 data_dependence->latency.min, data_dependence->latency.max,
                 data_dependence->distance);
   }

   /* Add the node to the list of already printed nodes */
   queue_add_tail(list, insn);
}

/*
 * Prints all graph nodes and their input edges (recursively calls print_graph_node)
 * \param dotfile dot file used to print the graph
 * \param graph (set of connected components)
 * \param print_graph_node function to print individual node
 */
static void print_all_graph_nodes (FILE *dotfile, graph_t *graph, void (*print_graph_node) (graph_node_t *node, void *user))
{
   fileandlist_t new_fileandlist;
   new_fileandlist.list = queue_new();
   new_fileandlist.file = dotfile;

   /* For each connected component (CC) */
   FOREACH_INQUEUE(graph_get_connected_components (graph), cc_iter) {
      graph_connected_component_t *CC = GET_DATA_T(graph_connected_component_t *, cc_iter);

      /* For each entry node (in the current CC) */
      FOREACH_INHASHTABLE(graph_connected_component_get_entry_nodes(CC), entry_node_iter) {
         graph_node_t *node = GET_KEY(graph_node_t*, entry_node_iter);

         graph_node_DFS (node, print_graph_node, NULL, NULL, &new_fileandlist);
      }
   }

   queue_free (new_fileandlist.list, NULL);
}

/*
 * Prints a graph to a dot file and returns the path to this file.
 * \param graph graph (set of connected components)
 * \param filename dot file base name. Full name is GRAPHS_PATH + filename + ".dot"
 * \param print_graph_node function to print individual node
 * \return dot file path (string)
 */
char *lcore_print_graph (graph_t *graph, char *filename, void (*print_graph_node) (graph_node_t *node, void *user))
{
   /* Builds the dot file name from input filename */
   char *dotfile_name = str_new (512);
   sprintf (dotfile_name, "%s/%s.dot", GRAPHS_PATH, filename);

   /* Opens the dot file in write-only mode */
   FILE *dotfile = fopen (dotfile_name, "w");
   if (dotfile == NULL) {
      fprintf (stderr, "Cannot write to %s/%s.dot\n", GRAPHS_PATH, filename);
      return NULL;
   }

   /* Writes dot format data into output file */
   fprintf (dotfile, "digraph %s {\n", filename);
   print_all_graph_nodes (dotfile, graph, print_graph_node);
   fprintf (dotfile, "}");

   fclose (dotfile);

   /* PNG export (debug) */
   /* char cmd[512]; */
   /* sprintf (cmd, "dot %s/%s_DDG.dot -Tpng -o %s/%s_DDG.png", */
   /* 	     GRAPHS_PATH, filename, GRAPHS_PATH, filename); */
   /* system (cmd); */

   return dotfile_name;
}

/*
 * For each path of an object (function, loop), prints the DDG to a dot file
 * \param DDGs queue of DDGs (one per path)
 * \param type "function" or "loop"
 * \param global_id function/loop global ID
 */
void lcore_print_DDG_paths (queue_t *DDGs, char *type, int global_id)
{
   unsigned path_id = 1;

   /* For each DDG/path */
   FOREACH_INQUEUE (DDGs, iter_path) {
      graph_t *DDG = GET_DATA_T (graph_t*, iter_path);

      char *filename = str_new (512);
      sprintf (filename, "%s_%d_path_%d_DDG", type, global_id, path_id);
      lcore_print_graph (DDG, filename, print_DDG_node);
      str_free (filename);

      path_id++;
   }
}

/*
 * Prints the DDG to a dot file, all paths being merged (paths are ignored)
 * \param DDG DDG
 * \param type "function", "loop" or "block"
 * \param global_id function/loop/block global ID
 */
char *lcore_print_DDG_merged_paths (graph_t *DDG, char *type, int global_id)
{
   char *filename = str_new (512);
   sprintf (filename, "%s_%d_DDG", type, global_id);
   char *ret = lcore_print_graph (DDG, filename, print_DDG_node);
   str_free (filename);

   return ret;
}

/*
 * For each path of a function, prints the DDG to a dot file
 * \param fct a function
 */
void lcore_print_fct_ddg_paths (fct_t *fct)
{
   lcore_print_DDG_paths (lcore_fctpath_getddg (fct), "fct", fct_get_id (fct));
}

/*
 * Prints a function's DDG to a dot file (different paths are merged)
 * and returns the path to this file
 * \param fct a function
 * \return dot file path (string)
 */
char *lcore_print_fct_ddg (fct_t *fct)
{
   return lcore_print_DDG_merged_paths (lcore_fct_getddg (fct), "fct", fct_get_id (fct));
}

/*
 * For each path of a loop, prints the DDG to a dot file
 * \param loop a loop
 */
void lcore_print_loop_ddg_paths (loop_t *loop)
{
   lcore_print_DDG_paths (lcore_looppath_getddg (loop), "loop", loop_get_id (loop));
}

/*
 * Prints a loop's DDG to a dot file (different paths are merged)
 * and returns the path to this file
 * \param loop a loop
 * \return dot file path (string)
 */
char *lcore_print_loop_ddg (loop_t *loop)
{
   return lcore_print_DDG_merged_paths (lcore_loop_getddg (loop), "loop", loop_get_id (loop));
}

/*
 * Prints a block's DDG to a dot file and returns the path to this file
 * \param block a block
 * \return dot file path (string)
 */
char *lcore_print_block_ddg (block_t *block)
{
   return lcore_print_DDG_merged_paths (lcore_block_getddg (block), "block", block_get_id (block));
}

///////////////////////////////////////////////////////////////////////////////
//                             CFG printing                                  //
///////////////////////////////////////////////////////////////////////////////

/*
 * Prints a node in function CFG
 * \param b a basic block
 * \param file a file where to print the node
 * \param f current function
 */
static void print_cfg_node(block_t *b, FILE *file, fct_t *f)
{
   if (block_get_fct(b) != f) {
      DBGMSG("block %d not in function %s\n", block_get_id(b), fct_get_name(f));
      return;
   }

   fprintf(file, "%d[label=\"", block_get_id(b));

   if (block_get_loop(b)) {
      loop_t *loop = block_get_loop(b);

      if (list_lookup(loop_get_entries(loop), b) || block_is_loop_exit(b)) {
         if (list_lookup(loop_get_entries(loop), b))
            fprintf(file, "LOOPENTRY: %d\\l", loop_get_id(loop));
         if (block_is_loop_exit(b))
            fprintf(file, "LOOPEXIT: %d\\l", loop_get_id(loop));
      } else
         fprintf(file, "LOOP: %d\\l", loop_get_id(loop));

      fprintf(file, " (%d)", block_get_id(b));
   }

   else
      fprintf(file, "%d", block_get_id(b));

   fprintf(file, "\"];\n");
}

/*
 * Prints an edge in function CFG
 * \param e an edge
 * \param file a file where to print the node
 */
static void print_cfg_edge(graph_edge_t *e, FILE *file)
{
   block_t *from = e->from->data;
   block_t *to = e->to->data;

   fprintf(file, "%d->%d;\n", block_get_id(from), block_get_id(to));
}

/*
 * Prints a function CFG into a file
 * \param f a function
 * \return a string with the file name where the CFG has been printed
 */
char* lcore_print_function_cfg(fct_t *f)
{
   if (f == NULL)
      return NULL;

   unsigned long h = add_hash(file_hash(asmfile_get_name(fct_get_asmfile(f))),
         fct_get_name(f));

   DBGMSG("printing cfg %scfg_%lu.dot\n", GRAPHS_PATH, h);

   char *filename = str_new(256);
   sprintf(filename, "%scfg_%lu.dot", GRAPHS_PATH, h);
   struct stat st;
   if (!stat(filename, &st))
      return filename;

   FILE *file = fopen(filename, "w");
   if (file == NULL)
      return NULL;

   fprintf(file, "digraph \"%s\" {\n", fct_get_name(f));

   FOREACH_INQUEUE(fct_get_blocks(f), iter) {
      block_t *current = GET_DATA_T(block_t*, iter);
      if (block_is_padding(current) == 0) {
         print_cfg_node(current, file, f);

         FOREACH_INLIST(block_get_CFG_node(current)->in, iter2)
         {
            graph_edge_t *edge = GET_DATA_T(graph_edge_t*, iter2);
            print_cfg_edge(edge, file);
         }
      }
   }
   fprintf(file, "}\n");
   fclose(file);

   return filename;
}

///////////////////////////////////////////////////////////////////////////////
//                       Domination tree printing                            //
///////////////////////////////////////////////////////////////////////////////

/*
 * Prints a domination node
 * \param node a node to print
 * \param user the file where to print the node
 * \return 0
 */
static int print_domination_node(tree_t *node, void *user)
{
   FILE *file = user;

   if (tree_hasparent(node))
      fprintf(file, "%d->%d[color=grey];\n",
            ((block_t *) node->parent->data)->global_id,
            ((block_t *) node->data)->global_id);

   return FALSE;
}

/*
 * Prints a function domination tree into a file
 * \param f a function
 * \return a string with the file name where the domination tree has been printed
 */
char * lcore_print_function_dominance(fct_t *f)
{
   if (f == NULL)
      return NULL;

   unsigned long h = add_hash(file_hash(asmfile_get_name(fct_get_asmfile(f))),
         fct_get_name(f));

   STDMSG("printing cfg %sdom_%lu.dot\n", GRAPHS_PATH, h);

   char *filename = str_new(256);
   sprintf(filename, "%sdom_%lu.dot", GRAPHS_PATH, h);
   struct stat st;
   if (!stat(filename, &st))
      return filename;

   FILE *file = fopen(filename, "w");
   if (file == NULL)
      return NULL;

   fprintf(file, "digraph \"%s\" {\n", fct_get_name(f));

   block_t *current = FCT_ENTRY(f);
   if (current != NULL) {
      tree_t *domination_root = block_get_domination_node(FCT_ENTRY(f));
      assert(domination_root != NULL);
      tree_traverse(domination_root, print_domination_node, file);
   }

   fprintf(file, "}\n");
   fclose(file);

   return filename;
}

///////////////////////////////////////////////////////////////////////////////
//                            CG printing                                    //
///////////////////////////////////////////////////////////////////////////////
/*
 * Print a CG node into a file
 * \param p a pointer on a function
 * \param user a pointer on a file where to print the node
 */
static void print_cg_node(void *p, void *user)
{
   if (p == NULL)
      return;

   fct_t *f = p;
   FILE *file = user;

   fprintf(file, "%d[label=\"%s.%s\"];\n", fct_get_id(f),
         getBasename(f->asmfile->name), fct_get_name(f));

   FOREACH_INLIST(fct_get_CG_node(f)->out, i)
   {
      graph_edge_t *e = GET_DATA_T(graph_edge_t*, i);
      fct_t *b = GET_DATA_T(fct_t*, e->to);
      fprintf(file, "%d->%d;\n", fct_get_id(f), fct_get_id(b));
   }
}

/*
 * Prints a function CG into a file
 * \param project an existing project
 * \return a string with the file name where the CG has been printed
 */
char* lcore_print_cg(project_t* project)
{
   if (project == NULL)
      return NULL;

   unsigned long h = 0;

   FOREACH_INQUEUE(project->asmfiles, iter) {
      asmfile_t* asmf = GET_DATA_T(asmfile_t*, iter);
      h = add_hash(h, asmf->name);
   }

   DBGMSG("printing cg %scg_%lu.dot\n", GRAPHS_PATH, h);

   char *filename = str_new(256);
   sprintf(filename, "%scg_%lu.dot", GRAPHS_PATH, h);

   DBGMSG("%s\n", filename);

   struct stat st;
   if (!stat(filename, &st))
      return filename;

   FILE *file = fopen(filename, "w");
   if (file == NULL)
      return NULL;

   fprintf(file, "digraph cg {\n");

   FOREACH_INQUEUE(project->asmfiles, it0) {
      asmfile_t* af = GET_DATA_T(asmfile_t*, it0);

      FOREACH_INQUEUE(af->functions, it) {
         fct_t* f = GET_DATA_T(fct_t*, it);
         print_cg_node(f, file);
      }

      if (af->plt_fct != NULL) {
         FOREACH_INLIST(af->plt_fct, it0)
         {
            fct_t* f = GET_DATA_T(fct_t*, it0);
            fprintf(file, "%d[label=\"%s.%s\"];\n", fct_get_id(f),
                  getBasename(f->asmfile->name), fct_get_name(f));
         }
      }
   }

   fprintf(file, "}\n");
   fclose(file);

   return filename;
}

///////////////////////////////////////////////////////////////////////////////
//                              loop printing                                //
///////////////////////////////////////////////////////////////////////////////

/*
 * Prints the loops of a function
 * \param f fct_t* the function whose loops will be printed
 */
void lcore_print_function_loops(fct_t *f)
{
   printf("digraph loops {\n");

   FOREACH_INQUEUE(fct_get_loops(f), iter) {
      loop_t *loop = GET_DATA_T(loop_t*, iter);
      printf("%ud;\n", block_get_id((block_t *) loop->entries->data));
   }

   printf("}\n");
}

///////////////////////////////////////////////////////////////////////////////
//                    Post-Domination tree printing                          //
///////////////////////////////////////////////////////////////////////////////
/*
 * Prints a function post domination tree into a file
 * \param f a function
 * \return a string with the file name where the post domination tree has been printed
 */
char * lcore_print_function_post_dominance(fct_t *f)
{
   if (f == NULL)
      return NULL;
   if (f->asmfile != NULL && (f->asmfile->analyze_flag & PDO_ANALYZE) == 0)
      return NULL;

   unsigned long h = add_hash(file_hash(asmfile_get_name(fct_get_asmfile(f))),
         fct_get_name(f));
   DBGMSG("printing post dominance %spostdom_%lu.dot\n", GRAPHS_PATH, h);

   char *filename = str_new(256);
   sprintf(filename, "%spostdom_%lu.dot", GRAPHS_PATH, h);
   struct stat st;
   if (!stat(filename, &st))
      return filename;

   FILE *file = fopen(filename, "w");
   if (file == NULL)
      return NULL;

   fprintf(file, "digraph \"%s\" {\n", fct_get_name(f));

   FOREACH_INQUEUE(f->blocks, it_b) {
      block_t* b = GET_DATA_T(block_t*, it_b);
      if (b->postdom_node != NULL && tree_hasparent(b->postdom_node)
            && !block_is_padding(b)) {
         fprintf(file, "%d->%d[color=grey];\n",
               ((block_t*) b->postdom_node->parent->data)->global_id,
               b->global_id);
      }
   }
   fprintf(file, "}\n");
   fclose(file);

   return filename;
}
