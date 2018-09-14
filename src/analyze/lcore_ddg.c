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
 * \brief This file defines functions and structs related to DDG (Data Dependency Graph)
 * */

#include "libmcore.h"
#include "arch.h"

/* Contains information needed to insert an edge in a DDG */
typedef struct {
   insn_t *src; /* source instruction */
   insn_t *dst; /* destination instruction */
   char kind[4]; /* "RAW", "WAR" or "WAW" */
   int distance; /* 0/1 for current/previous iteration */
} ddg_edge_t;

/* Contains context related to a DDG */
typedef struct {
   graph_t *ddg;
   array_t *edges;
   arch_t *arch; /* architecture */

   /* rd/wrreg2insn allows fast access to instructions reading/writing a given register
    * from its "family" and "name" (ex: XMM7 and YMM7 target the same register) */
   hashtable_t *rdreg2insn; /* (read    register, array of instructions) pairs */
   hashtable_t *wrreg2insn; /* (written register, array of instructions) pairs */

   /* insn_rank links each instruction to its rank (first instruction has rank 1),
    * allowing to know if an instruction is executed before another one */
   hashtable_t *insn_rank; /* (instruction, rank [long int]) pairs */

   /* insn2node allow fast access to DDG nodes */
   hashtable_t *insn2node; /* (instruction, node [graph_node_t *]) pairs */
} ddg_context_t;

/**************************************************************************************/
/*                 FUNCTIONS RELATED TO lcore_loop[path]_getddg[_ext]                 */
/**************************************************************************************/

/**
 *  Checks whether an instruction breaks dependencies (and is register to register)
 *  \param insn The instruction
 * \TODO move this arch-dependent function elsewhere */
static int breaks_dependency(insn_t *insn)
{
   /* If opcode not matching */
   char *opcode = insn_get_opcode(insn);
   if (strcmp(opcode, "SUB") && /* not exact matches SUB */
   !strstr(opcode, "SUBP") && /* not matches V?SUBP[SD] */
   !strstr(opcode, "PSUB") && /* not matches V?PSUB[BWDQ] */
   !strstr(opcode, "XOR") && /* not matches V?PXOR and V?XORP[SD] */
   !strstr(opcode, "PCMPEQ")) /* not matches V?PCMPEQ[BWDQ] */
      return FALSE;

   /* Gets operands */
   oprnd_t **oprnds = insn_get_oprnds(insn);

   /* If format not matching (not register to register) */
   if (!oprnd_is_reg(oprnds[0]) || !oprnd_is_reg(oprnds[1]))
      return FALSE;

   /* If used with two identical source registers */
   reg_t *reg1 = oprnd_get_reg(oprnds[0]);
   reg_t *reg2 = oprnd_get_reg(oprnds[1]);
   if (reg_get_type(reg1) == reg_get_type(reg2)
         && reg_get_name(reg1) == reg_get_name(reg2))
      return TRUE;

   return FALSE;
}

static void insert_reg2insn(hashtable_t *ht, void *reg_key, insn_t *insn)
{
   array_t *insns = hashtable_lookup(ht, reg_key);

   if (!insns) {
      insns = array_new();
      hashtable_insert(ht, reg_key, insns);
   }

   array_add(insns, insn);
}

/**
 * Inserts a new (register, instruction) pair in rdreg2insn and/or wrreg2insn hashtables
 * \param ctxt DDG context
 * \param oprnd operand (memory or register)
 * \param reg register extracted from oprnd
 * \param insn instruction reading/writing reg
 */
static void update_hashtables(ddg_context_t ctxt, oprnd_t *oprnd, reg_t *reg,
      insn_t *insn)
{
   if (!reg)
      return;

   /* A register key is the concatenation of its family and name (CF <arch>_arch.h) */
   void *reg_key = (void *) ((long) (reg_get_family(reg, ctxt.arch) * 256
         + reg_get_name(reg)));

   /* In a (register to register) dependency-breaking instruction */
   if (breaks_dependency(insn)) {
      /* Ignore register reads */
      if (oprnd_is_dst(oprnd))
         insert_reg2insn(ctxt.wrreg2insn, reg_key, insn);

      return;
   }

   /* If read (register in a memory operand or source register) */
   if (oprnd_is_mem(oprnd) || oprnd_is_src(oprnd))
      insert_reg2insn(ctxt.rdreg2insn, reg_key, insn);

   /* If written (destination register) */
   if (!oprnd_is_mem(oprnd) && oprnd_is_dst(oprnd))
      insert_reg2insn(ctxt.wrreg2insn, reg_key, insn);
}

/*
 * Fills rdreg2insn, wrreg2insn and insn_rank hashtables from all path instructions
 */
static void fill_DDG_data(ddg_context_t *ctxt, array_t *insns)
{
   ctxt->rdreg2insn = hashtable_new(direct_hash, direct_equal);
   ctxt->wrreg2insn = hashtable_new(direct_hash, direct_equal);
   ctxt->insn_rank = hashtable_new(direct_hash, direct_equal);

   long rank = 1;

   /* For each instruction */
   FOREACH_INARRAY(insns, insns_iter)
   {
      insn_t *insn = ARRAY_GET_DATA(insn, insns_iter);

      /* Save instruction rank */
      hashtable_insert(ctxt->insn_rank, insn, (void *) rank++);

      /* For each operand */
      int i;
      oprnd_t **oprnds = insn_get_oprnds(insn);
      for (i = 0; i < insn_get_nb_oprnds(insn); i++) {
         oprnd_t *oprnd = oprnds[i];

         if (oprnd_is_reg(oprnd))
            update_hashtables(*ctxt, oprnd, oprnd_get_reg(oprnd), insn);
         else if (oprnd_is_mem(oprnd)) {
            update_hashtables(*ctxt, oprnd, oprnd_get_base(oprnd), insn);
            update_hashtables(*ctxt, oprnd, oprnd_get_index(oprnd), insn);
         }
      } /* for each operand */
   } /* for each instruction */
}

/**
 * Connects src to dst DDG nodes with an edge representing the data dependency
 * \param ddg DDG graph
 * \param src source node
 * \param dst destination node
 * \param kind CF ddg_edge_t
 * \param distance CF ddg_edge_t
 */
static void connect_nodes(graph_t *ddg, graph_node_t *src, graph_node_t *dst, char *kind, int distance)
{
   data_dependence_t *data_dep = lc_malloc(sizeof *data_dep);

   /* Fills data_dep fields (excepted latency) */
   data_dep->distance = distance;
   lc_strncpy(data_dep->kind, sizeof(data_dep->kind), kind, 4);

   /* Connects src to dst nodes with data_dep as data */
   graph_edge_t *edge = graph_add_new_edge(ddg, src, dst, data_dep);

   graph_connected_component_t *cc = hashtable_lookup (graph_get_edge2cc (ddg), edge);
   hashtable_t *entry_nodes = graph_connected_component_get_entry_nodes (cc);
   if (data_dep->distance == 0) hashtable_remove (entry_nodes, graph_edge_get_dst_node (edge));
}

/* From an instruction, creates a DDG node and update related structures accordingly */
static graph_node_t *insert_node(ddg_context_t ctxt, insn_t *insn)
{
   graph_node_t *node = graph_add_new_node(ctxt.ddg, insn);
   hashtable_insert(ctxt.insn2node, insn, node);

   graph_connected_component_t *cc = hashtable_lookup (graph_get_node2cc (ctxt.ddg), node);
   hashtable_t *entry_nodes = graph_connected_component_get_entry_nodes (cc);
   hashtable_insert (entry_nodes, node, insn);


   return node;
}

/** Inserts in a DDG a new data dependency from a source to a destination instruction
 * Nodes are created if not yet inserted and connected. Connected components are
 * created and merged if src and dst belong to different CCs.
 * \param ctxt DDG context
 * \param src source instruction
 * \param dst destination instruction
 * \param kind CF ddg_edge_t
 * \param distance CF ddg_edge_t
 */
static void insert_in_DDG(ddg_context_t ctxt, insn_t *src, insn_t *dst,
      char *kind, int distance)
{
   graph_node_t *src_node = hashtable_lookup(ctxt.insn2node, src);
   graph_node_t *dst_node = hashtable_lookup(ctxt.insn2node, dst);

   if (src != dst) {
      if (!src_node) src_node = insert_node (ctxt, src);
      if (!dst_node) dst_node = insert_node (ctxt, dst);
   } else if (!src_node)
      src_node = dst_node = insert_node (ctxt, src);

   connect_nodes (ctxt.ddg, src_node, dst_node, kind, distance);
}

/* Inserts in a flat structure a new data dependency from a source to a destination
 * instruction. Used to aggregate dependencies from multiple paths to ease a global
 * DDG creation */
static void insert_in_edges(array_t *edges, insn_t *src, insn_t *dst,
      char *kind, int distance)
{
   ddg_edge_t *ddg_edge = lc_malloc(sizeof *ddg_edge);

   /* Fills ddg_edge fields */
   ddg_edge->src = src;
   ddg_edge->dst = dst;
   lc_strncpy(ddg_edge->kind, sizeof(ddg_edge->kind), kind, 4);
   ddg_edge->distance = distance;

   /* Appends ddg_edge to other edges */
   array_add(edges, ddg_edge);
}

/* Inserts a new data dependency from a source to a destination instruction in a DDG or a flat structure depending on the context */
static void insert_in_DDG_or_edges(ddg_context_t ctxt, insn_t *src, insn_t *dst,
      char *kind, int distance)
{
   if (ctxt.edges)
      insert_in_edges(ctxt.edges, src, dst, kind, distance);
   else
      insert_in_DDG(ctxt, src, dst, kind, distance);
}

/* Inserts RAW (Read After Write) or WAW (Write After Write) dependencies in the DDG */
static void insert_RAW_or_WAW(ddg_context_t ctxt, insn_t *dst_insn,
      void *reg_key, char *kind)
{
   const long dst_insn_rank = (long) hashtable_lookup(ctxt.insn_rank, dst_insn);
   array_t *src_insns = hashtable_lookup(ctxt.wrreg2insn, reg_key);

   if (!src_insns)
      return;

   /* Looking for the nearest instruction in the same loop iteration */
   /* For each instruction writing <reg_key>, from the last to the first executed */
   FOREACH_INARRAY_REVERSE(src_insns, src_insns_iter) {
      insn_t *src_insn = ARRAY_GET_DATA(src_insn, src_insns_iter);

      /* skips previous iteration */
      long src_insn_rank = (long) hashtable_lookup(ctxt.insn_rank, src_insn);
      if (src_insn_rank >= dst_insn_rank)
         continue;

      insert_in_DDG_or_edges(ctxt, src_insn, dst_insn, kind, 0);

      return;
   }

   /* Nearest instruction in the previous loop iteration */
   insn_t *src_insn = array_get_last_elt(src_insns);
   insert_in_DDG_or_edges(ctxt, src_insn, dst_insn, kind, 1);
}

/* Inserts RAW (Read After Write) dependencies in the DDG
 * The nearest instruction writing a register matching reg_key cuts the dependency chain.
 * This instruction is searched in the same loop iteration and then in the previous one */
static void insert_RAW(ddg_context_t ctxt, insn_t *dst_insn, void *reg_key)
{
   insert_RAW_or_WAW(ctxt, dst_insn, reg_key, "RAW");
}

/* Inserts WAR (Write After Read) dependencies in the DDG
 * Contrary to other dependencies, all matching instructions are retained */
static void insert_WAR(ddg_context_t ctxt, insn_t *dst_insn, void *reg_key)
{
   const long dst_insn_rank = (long) hashtable_lookup(ctxt.insn_rank, dst_insn);
   array_t *src_insns = hashtable_lookup(ctxt.rdreg2insn, reg_key);

   if (!src_insns)
      return;

   /* Looking for all matching instructions */
   FOREACH_INARRAY_REVERSE(src_insns, src_insns_iter)
   {
      insn_t *src_insn = ARRAY_GET_DATA(src_insn, src_insns_iter);
      long src_insn_rank = (long) hashtable_lookup(ctxt.insn_rank, src_insn);

      if (src_insn_rank >= dst_insn_rank) /* previous iteration */
         insert_in_DDG_or_edges(ctxt, src_insn, dst_insn, "WAR", 1);
      else
         /* current iteration */
         insert_in_DDG_or_edges(ctxt, src_insn, dst_insn, "WAR", 0);
   }
}

/* Insert WAW (Write After Write) dependencies in the DDG
 * The nearest instruction writing a register matching reg_key cuts the dependency chain.
 * This instruction is searched in the same loop iteration and then in the previous one */
static void insert_WAW(ddg_context_t ctxt, insn_t *dst_insn, void *reg_key)
{
   insert_RAW_or_WAW(ctxt, dst_insn, reg_key, "WAW");
}

/**
 * Used by build_DDG
 * @param data
 */
static void free_insns(void *data)
{
   array_free(data, NULL);
}

/**
 * Builds DDG for a sequence of instructions (as a graph or a list of edges)
 * \param insns dynamic array of instructions
 * \param ddg   NULL or empty DDG
 * \param edges NULL or set of edges (as dynamic array) to insert in a future ddg
 * \param only_RAW TRUE for considering only RAW dependencies, FALSE for WAW and WAR too
 */
static void build_DDG(array_t *insns, graph_t *ddg, array_t *edges, int only_RAW)
{
   ddg_context_t ctxt;
   ctxt.ddg = ddg;
   ctxt.edges = edges;
   ctxt.arch = insn_get_arch (array_get_first_elt (insns));
   ctxt.insn2node = NULL;

   /* Allocate/fill related hashtables */
   fill_DDG_data(&ctxt, insns);
   if (!edges) {
      ctxt.insn2node = hashtable_new(direct_hash, direct_equal);
   } else {
      ctxt.insn2node = NULL;
   } /* get rid of compiler warnings */

   /* For each (read register, instruction) pair */
   FOREACH_INHASHTABLE(ctxt.rdreg2insn, rdreg2insn_iter) {
      void *reg_key = GET_KEY(reg_key, rdreg2insn_iter);
      array_t *insns = GET_DATA_T(array_t*, rdreg2insn_iter);

      FOREACH_INARRAY(insns, insns_rdreg_iter)
      {
         insn_t *insn = ARRAY_GET_DATA(insn, insns_rdreg_iter);

         insert_RAW(ctxt, insn, reg_key);
      }
   }

   if (only_RAW == FALSE) {
      /* For each (written register, instruction) pair */
      FOREACH_INHASHTABLE(ctxt.wrreg2insn, wrreg2insn_iter)
      {
         void *reg_key = GET_KEY(reg_key, wrreg2insn_iter);
         array_t *insns = GET_DATA_T(array_t*, wrreg2insn_iter);

         FOREACH_INARRAY(insns, insns_wrreg_iter)
         {
            insn_t *insn = ARRAY_GET_DATA(insn, insns_wrreg_iter);

            insert_WAR(ctxt, insn, reg_key);
            insert_WAW(ctxt, insn, reg_key);
         }
      }
   }

   /* Finalize/free data structures */
   hashtable_free(ctxt.rdreg2insn, free_insns, NULL);
   hashtable_free(ctxt.wrreg2insn, free_insns, NULL);
   hashtable_free(ctxt.insn_rank, NULL, NULL);

   if (!edges) {
      hashtable_free(ctxt.insn2node, NULL, NULL);
   }
}

static array_t *get_path_insns (array_t *path)
{
   /* Get number of instructions */
   int nb_insns = 0;
   FOREACH_INARRAY(path, path_iter1) {
      block_t *block = ARRAY_GET_DATA(block, path_iter1);
      nb_insns += block_get_size (block);
   }

   array_t *insns = array_new_with_custom_size (nb_insns);

   /* Insert instructions from path to insns */
   FOREACH_INARRAY(path, path_iter2) {
      block_t *block = ARRAY_GET_DATA(block, path_iter2);

      FOREACH_INSN_INBLOCK(block, block_iter) {
         insn_t *insn = GET_DATA_T(insn_t*, block_iter);
         array_add (insns, insn);
      }
   }

   return insns;
}

#ifdef _ARCHDEF_arm64
extern DDG_latency_t arm64_get_DDG_latency (insn_t *src, insn_t *dst);
#endif

static DDG_latency_t get_0_latency (insn_t *src, insn_t *dst)
{
   (void) src;
   (void) dst;

   DDG_latency_t lat = {0, 0};
   return lat;
}

static get_DDG_latency_t get_default_latency (arch_t *arch)
{
   char arch_code = arch_get_code (arch);

#ifdef _ARCHDEF_arm64
   if (arch_code == ARCH_arm64)
      return arm64_get_DDG_latency;
#endif

   return get_0_latency;
}

static graph_t *get_DDG(array_t *insns, int only_RAW)
{
   graph_t *ddg = graph_new();

   build_DDG(insns, ddg, NULL, only_RAW);

   insn_t *first_insn = array_get_first_elt (insns);
   arch_t *arch = insn_get_arch (first_insn);
   lcore_set_ddg_latency (ddg, get_default_latency (arch));

   return ddg;
}

static graph_t *get_path_DDG(array_t *path, int only_RAW)
{
   array_t *insns = get_path_insns (path);
   graph_t *ddg = get_DDG(insns, only_RAW); 
   array_free (insns, NULL);

   return ddg;
}

/*
 * Gets function/loop paths. If not previously computed (by another MAQAO function), compute them
 * and informs the caller about that via the 'already_computed' parameter
 * */
static queue_t *get_obj_paths(void *obj, int *already_computed,
                              queue_t* (*get_paths)(void *),
                              void (*compute_paths)(void *))
{
   queue_t *paths = get_paths(obj);

   if (paths == NULL) {
      *already_computed = FALSE;
      compute_paths(obj);
      paths = get_paths(obj);
   } else
      *already_computed = TRUE;

   return paths;
}

/* CF lcore_fctpath_getddg and build_DDG */
static queue_t *objpath_getddg(void *obj, int only_RAW,
                               queue_t* (*get_paths)(void *),
                               void (*compute_paths)(void *),
                               void (*free_paths)(void *),
                               arch_t *arch)
{
   int paths_already_computed;
   queue_t *paths = get_obj_paths(obj, &paths_already_computed, get_paths, compute_paths);
   queue_t *ddg_allpaths = queue_new();

   /* For each path */
   FOREACH_INQUEUE(paths, paths_iter) {
      array_t *path = GET_DATA_T(array_t*, paths_iter);
      graph_t *ddg = get_path_DDG(path, only_RAW);
      lcore_set_ddg_latency (ddg, get_default_latency (arch));
      queue_add_tail(ddg_allpaths, ddg);
   }

   /* Free paths if was computed on purpose */
   if (paths_already_computed == FALSE)
      free_paths(obj);

   return ddg_allpaths;
}

/* CF lcore_fct_getddg and build_DDG */
static graph_t *obj_getddg(void *obj, int only_RAW,
                           queue_t* (*get_paths)(void *),
                           void (*compute_paths)(void *),
                           void (*free_paths)(void *),
                           arch_t *arch)
{
   graph_t *obj_ddg = graph_new();
   int paths_already_computed;
   queue_t *paths = get_obj_paths(obj, &paths_already_computed, get_paths, compute_paths);

   /* If only one path, directly build+get DDG */
   if (queue_length(paths) == 1) {
      array_t *path = list_getdata(queue_iterator(paths)); /* first path */
      array_t *insns = get_path_insns (path);
      build_DDG(insns, obj_ddg, NULL, only_RAW);
      array_free (insns, NULL);
   }

   /* Get DDG edges for each path and, from them, build a global DDG */
   else {
      array_t *ddg_edges = array_new();

      /* Get DDG edges for each path */
      FOREACH_INQUEUE(paths, paths_iter) {
         array_t *path = GET_DATA_T(array_t*, paths_iter);
         array_t *insns = get_path_insns (path);
         build_DDG(insns, NULL, ddg_edges, only_RAW);
         array_free (insns, NULL);
      }

      /* Allocate temporary data structures */
      ddg_context_t ctxt;
      ctxt.ddg = obj_ddg;
      ctxt.insn2node = hashtable_new(direct_hash, direct_equal);

      /* Insert DDG edges in obj_ddg */
      FOREACH_INARRAY(ddg_edges, ddg_edges_iter) {
         ddg_edge_t *ddg_edge = ARRAY_GET_DATA(ddg_edge, ddg_edges_iter);
         insert_in_DDG(ctxt, ddg_edge->src, ddg_edge->dst, ddg_edge->kind,
               ddg_edge->distance);
      }

      /* Free temporary data structures */
      array_free(ddg_edges, NULL);
      hashtable_free(ctxt.insn2node, NULL, NULL);
   }

   /* Free paths if was computed on purpose */
   if (paths_already_computed == FALSE)
      free_paths(obj);

   lcore_set_ddg_latency (obj_ddg, get_default_latency (arch));

   return obj_ddg;
}

/***************************************************************************************************
 *                                        Specific to functions                                    *
 ***************************************************************************************************/

static queue_t *_fct_get_paths (void *f) { return fct_get_paths   (f); }
static void fct_compute_paths  (void *f) { lcore_fct_computepaths (f); }
static void fct_free_paths     (void *f) { lcore_fct_freepaths    (f); }


/* CF lcore_fctpath_getddg and build_DDG */
static queue_t *fctpath_getddg(fct_t *fct, int only_RAW)
{
   arch_t *arch = asmfile_get_arch (fct_get_asmfile (fct));
   return objpath_getddg (fct, only_RAW, _fct_get_paths, fct_compute_paths, fct_free_paths, arch);
}

static graph_t *fct_getddg(fct_t *fct, int only_RAW)
{
   arch_t *arch = asmfile_get_arch (fct_get_asmfile (fct));
   return obj_getddg(fct, only_RAW, _fct_get_paths, fct_compute_paths, fct_free_paths, arch);
}

/*
 * Returns DDG for all paths of a function, with only RAW dependencies
 * \param fct function
 * \return queue of DDGs (CF lcore_fct_getddg)
 */
queue_t *lcore_fctpath_getddg(fct_t *fct)
{
   return fctpath_getddg(fct, TRUE);
}

/* Idem lcore_fctpath_getddg with WAW and WAR */
queue_t *lcore_fctpath_getddg_ext(fct_t *fct)
{
   return fctpath_getddg(fct, FALSE);
}

/*
 * Returns DDG for a function, with only RAW dependencies
 * If multipaths function, path DDGs are merged into a single DDG
 * \param fct function
 * \return DDG */
graph_t *lcore_fct_getddg(fct_t *fct)
{
   return fct_getddg(fct, TRUE);
}

/* Idem lcore_fct_getddg with WAW and WAR */
graph_t *lcore_fct_getddg_ext(fct_t *fct)
{
   return fct_getddg(fct, FALSE);
}

/***************************************************************************************************
 *                                          Specific to loops                                      *
 ***************************************************************************************************/

static queue_t *_loop_get_paths (void *l) { return loop_get_paths   (l); }
static void loop_compute_paths  (void *l) { lcore_loop_computepaths (l); }
static void loop_free_paths     (void *l) { lcore_loop_freepaths    (l); }

/* CF lcore_looppath_getddg and build_DDG */
static queue_t *looppath_getddg(loop_t *loop, int only_RAW)
{
   arch_t *arch = asmfile_get_arch (loop_get_asmfile (loop));
   return objpath_getddg (loop, only_RAW, _loop_get_paths, loop_compute_paths, loop_free_paths, arch);
}

/* CF lcore_loop_getddg and build_DDG */
static graph_t *loop_getddg(loop_t *loop, int only_RAW)
{
   arch_t *arch = asmfile_get_arch (loop_get_asmfile (loop));
   return obj_getddg(loop, only_RAW, _loop_get_paths, loop_compute_paths, loop_free_paths, arch);
}

/*
 * Returns DDG for all paths of a loop, with only RAW dependencies
 * \param loop loop
 * \return queue of DDGs (CF lcore_loop_getddg)
 */
queue_t *lcore_looppath_getddg(loop_t *loop)
{
   return looppath_getddg(loop, TRUE);
}

/* Idem lcore_looppath_getddg with WAW and WAR */
queue_t *lcore_looppath_getddg_ext(loop_t *loop)
{
   return looppath_getddg(loop, FALSE);
}

/*
 * Returns DDG for a loop, with only RAW dependencies
 * If multipaths loop, path DDGs are merged into a single DDG
 * \param loop loop
 * \return DDG (queue of CCs (Connected Components), each CC being a queue of entry nodes) */
graph_t *lcore_loop_getddg(loop_t *loop)
{
   return loop_getddg(loop, TRUE);
}

/* Idem lcore_loop_getddg with WAW and WAR */
graph_t *lcore_loop_getddg_ext(loop_t *loop)
{
   return loop_getddg(loop, FALSE);
}

/***************************************************************************************************
 *                                Specific to paths (array of blocks)                              *
 ***************************************************************************************************/

/*
 * Returns DDG for a path (array of blocks), with only RAW dependencies
 * \param path array of blocks
 * \return DDG
 */
graph_t *lcore_path_getddg(array_t *path)
{
   return get_path_DDG(path, TRUE);
}

/* Idem lcore_path_getddg with WAW and WAR */
graph_t *lcore_path_getddg_ext(array_t *path)
{
   return get_path_DDG(path, FALSE);
}

/***************************************************************************************************
 *                                        Specific to block                                        *
 ***************************************************************************************************/

static graph_t *get_block_DDG(block_t *block, int only_RAW)
{
   array_t *insns = array_new_with_custom_size (block_get_size (block));
   FOREACH_INSN_INBLOCK (block, block_iter) {
      insn_t *insn = GET_DATA_T (insn_t*, block_iter);
      array_add (insns, insn);
   }
   graph_t *ddg = get_DDG(insns, only_RAW);
   array_free (insns, NULL);

   return ddg;
}

/*
 * Returns DDG for a block, with only RAW dependencies
 * \param block a block
 * \return DDG
 */
graph_t *lcore_block_getddg(block_t *block)
{
   return get_block_DDG(block, TRUE);
}

/* Idem lcore_block_getddg with WAW and WAR */
graph_t *lcore_block_getddg_ext(block_t *block)
{
   return get_block_DDG(block, FALSE);
}

/***************************************************************************************************
 *                                   Specific to instruction list                                  *
 ***************************************************************************************************/

/*
 * Returns DDG for a sequence (array) of instructions (array of blocks), with only RAW dependencies
 * \param insns array of instructions
 * \return queue of DDGs (CF lcore_loop_getddg)
 */
graph_t *lcore_getddg(array_t *insns)
{
   return get_DDG(insns, TRUE);
}

/* Idem lcore_getddg with WAW and WAR */
graph_t *lcore_getddg_ext(array_t *insns)
{
   return get_DDG(insns, FALSE);
}

/*
 * Sets latency information in the DDG
 * \param DDG a DDG (graph)
 * \param get_latency function that returns latency from source and destination instructions
 */
void lcore_set_ddg_latency(graph_t *DDG, get_DDG_latency_t get_latency)
{
   if (get_latency == NULL)
      get_latency = get_0_latency;

   /* For each connected component */
   FOREACH_INQUEUE(graph_get_connected_components (DDG), cc_iter) {
      graph_connected_component_t *cc = GET_DATA_T(graph_connected_component_t*, cc_iter);

      /* For each edge */
      FOREACH_INHASHTABLE(graph_connected_component_get_edges (cc), edge_iter) {
         graph_edge_t *edge = GET_KEY(graph_edge_t *, edge_iter);

         graph_node_t *src_node = graph_edge_get_src_node (edge);
         graph_node_t *dst_node = graph_edge_get_dst_node (edge);
         data_dependence_t *data_dep = graph_edge_get_data (edge);
         insn_t *src_insn = graph_node_get_data (src_node);
         insn_t *dst_insn = graph_node_get_data (dst_node);

         data_dep->latency = get_latency (src_insn, dst_insn);
      }
   }
}

/**************************************************************************************/
/*                          FUNCTIONS RELATED TO get_RecMII                           */
/**************************************************************************************/

static boolean_t ignore_non_RAW (const graph_edge_t *edge)
{
   /* Considering only RAW dependencies */
   data_dependence_t *data_dep = edge->data;
   return strcmp(data_dep->kind, "RAW") ? TRUE : FALSE;
}

typedef struct {
   float min; // maximum of minimum (best -case) latency values
   float max; // maximum of maximum (worst-case) latency values
} _upd_cycle_data_t;

static void _upd_cycle (queue_t *cycle, void *_data)
{
   int sum_min_latency = 0;
   int sum_max_latency = 0;
   int sum_distance = 0;

   /* For each edge */
   array_t *edges = graph_cycle_get_edges (cycle, ignore_non_RAW);
   FOREACH_INARRAY(edges, edge_iter) {
      graph_edge_t *edge = ARRAY_GET_DATA(edge, edge_iter);

      data_dependence_t *data_dep = edge->data;
      sum_min_latency += data_dep->latency.min;
      sum_max_latency += data_dep->latency.max;
      sum_distance += data_dep->distance;
   }
   array_free (edges, NULL);

   if (sum_distance == 0) return;

   float min = (float) sum_min_latency / sum_distance;
   float max = (float) sum_max_latency / sum_distance;

   _upd_cycle_data_t *graph_max = _data;
   // Warning: graph_max->min/max are both maximum values
   if (graph_max->min < min) graph_max->min = min;
   if (graph_max->max < max) graph_max->max = max;
}

/** Default maximum number of paths to compute in RecMII from a DDG entry node */
#define DDG_MAX_PATHS 1000

/*
 * Returns a loop RecMII (longest latency chain) from its DDG
 * \param ddg loop DDG
 * \param max_paths maximum number of paths to compute for each CC. If negative value or zero, equivalent to DDG_MAX_PATHS
 * \param min RecMII using min latency
 * \param max RecMII using max latency
 */
void get_RecMII(graph_t *ddg, int max_paths, float *min, float *max)
{
   if (max_paths <= 0)
      max_paths = DDG_MAX_PATHS;

   _upd_cycle_data_t upd_cycle_data = { 0.0f, 0.0f };
   graph_for_each_cycle (ddg, max_paths, ignore_non_RAW,
                         _upd_cycle, &upd_cycle_data);

   *min = upd_cycle_data.min;
   *max = upd_cycle_data.max;
}

typedef struct { graph_update_critical_paths_data_t min, max; } _ddg_upd_crit_paths_data_t;

static float _get_min_lat (graph_edge_t *edge)
{
   data_dependence_t *data_dep = graph_edge_get_data (edge);
   return data_dep->latency.min;
}

static float _get_max_lat (graph_edge_t *edge)
{
   data_dependence_t *data_dep = graph_edge_get_data (edge);
   return data_dep->latency.max;
}

static void _ddg_upd_crit_paths (array_t *path, void *data)
{
   _ddg_upd_crit_paths_data_t *crit_paths = data;
   graph_update_critical_paths (path, &(crit_paths->min));
   graph_update_critical_paths (path, &(crit_paths->max));
}

/*
 * Returns critical paths for a DDG
 * \param ddg DDG (a graph)
 * \param max_paths maximum number of paths to consider from an entry node
 * \param min_lat_crit_paths pointer to an array, set to critical paths considering minimum latency values
 * \param max_lat_crit_paths idem min_lat_crit_paths considering maximum latencies
 */
void lcore_ddg_get_critical_paths (graph_t *ddg, int max_paths,
                                   array_t **min_lat_crit_paths,
                                   array_t **max_lat_crit_paths)
{
   if (max_paths <= 0)
      max_paths = DDG_MAX_PATHS;

   _ddg_upd_crit_paths_data_t crit_paths_data = { { 0.0f, array_new(), _get_min_lat },
                                                  { 0.0f, array_new(), _get_max_lat } };
   graph_for_each_path (ddg, max_paths, _ddg_upd_crit_paths, &crit_paths_data);

   *min_lat_crit_paths = crit_paths_data.min.paths;
   *max_lat_crit_paths = crit_paths_data.max.paths;
}

/**************************************************************************************/
/*                        FUNCTIONS RELATED TO lcore_freeddg                          */
/**************************************************************************************/

/*
 * Frees memory allocated for a DDG
 * \param ddg DDG as returned by lcore_*_getddg[_ext]
 */
void lcore_freeddg (graph_t *ddg)
{
   graph_free (ddg, NULL, lc_free);
}
