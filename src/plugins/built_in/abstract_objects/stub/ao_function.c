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

#include <stdlib.h>
#include "libmcore.h"
#include "DwarfLight.h"
#include "libmdbg.h"
#include "abstract_objects_c.h"

/******************************************************************/
/*                Functions dealing with Function                 */
/******************************************************************/

static int l_function_get_project(lua_State * L)
{
   f_t *f = luaL_checkudata(L, 1, FUNCTION);
   project_t *project = fct_get_project(f->p);
   if (project != NULL) {
      create_project(L, project, FALSE);
      return 1;
   }
   return 0;
}

static int l_function_get_asmfile(lua_State * L)
{
   f_t *f = luaL_checkudata(L, 1, FUNCTION);
   asmfile_t *asmfile = fct_get_asmfile(f->p);
   if (asmfile != NULL) {
      create_asmfile(L, asmfile);
      return 1;
   }
   return 0;
}

static int l_function_has_debug_data(lua_State * L)
{
   f_t *f = luaL_checkudata(L, 1, FUNCTION);
   lua_pushboolean(L, fct_has_debug_data(f->p));
   return 1;
}

static int l_function_get_src_file_name(lua_State * L)
{
   f_t *f = luaL_checkudata(L, 1, FUNCTION);
   char *srcfile_name = fct_get_src_file(f->p);
   if (srcfile_name != NULL)
      lua_pushstring(L, srcfile_name);
   else
      lua_pushnil(L);
   return 1;
}

static int l_function_get_src_file_path(lua_State * L)
{
   f_t *f = luaL_checkudata(L, 1, FUNCTION);
   char *srcfile_path = fct_get_src_file_path(f->p);
   if (srcfile_path != NULL) {
      lua_pushstring(L, srcfile_path);
      lc_free(srcfile_path);
   } else
      lua_pushnil(L);
   return 1;
}

static int l_function_get_compiler_short(lua_State * L)
{
   f_t *f = luaL_checkudata(L, 1, FUNCTION);
   char *compiler_str = fct_get_compiler(f->p);
   if (compiler_str != NULL)
      lua_pushstring(L, compiler_str);
   else
      lua_pushnil(L);
   return 1;
}

static int l_function_get_compiler_version(lua_State * L)
{
   f_t *f = luaL_checkudata(L, 1, FUNCTION);
   char *version_str = fct_get_version(f->p);
   if (version_str != NULL)
      lua_pushstring(L, version_str);
   else
      lua_pushnil(L);
   return 1;
}

static int l_function_get_language(lua_State * L)
{
   f_t *f = luaL_checkudata(L, 1, FUNCTION);
   char lang = fct_get_language_code(f->p);
   if (lang != LANG_ERR)
      lua_pushinteger(L, (int) lang);
   else
      lua_pushnil(L);
   return 1;
}

static int l_function_get_producer(lua_State * L)
{
   f_t *f = luaL_checkudata(L, 1, FUNCTION);
   char *prod = fct_getproducer(f->p);
   if (prod != NULL)
      lua_pushstring(L, prod);
   else
      lua_pushnil(L);
   return 1;
}

static int l_function_get_dir(lua_State * L)
{
   f_t *f = luaL_checkudata(L, 1, FUNCTION);
   char *dir = fct_getdir(f->p);
   if (dir != NULL)
      lua_pushstring(L, dir);
   else
      lua_pushnil(L);
   return 1;
}

static int l_function_get_name(lua_State * L)
{
   f_t *f = luaL_checkudata(L, 1, FUNCTION);
   char* name = fct_get_name(f->p);
   if (name != NULL)
      lua_pushstring(L, name);
   else
      lua_pushnil(L);
   return 1;
}

static int l_function_get_demname(lua_State * L)
{
   f_t *f = luaL_checkudata(L, 1, FUNCTION);
   char* name = fct_get_demname(f->p);
   if (name != NULL)
      lua_pushstring(L, name);
   else
      lua_pushnil(L);
   return 1;
}

static int l_function_get_decl_line(lua_State * L)
{
   f_t *f = luaL_checkudata(L, 1, FUNCTION);
   lua_pushinteger(L, fct_get_decl_line(f->p));
   return 1;
}

static int l_function_get_id(lua_State * L)
{
   f_t *f = luaL_checkudata(L, 1, FUNCTION);
   lua_pushinteger(L, fct_get_id(f->p));
   return 1;
}

static int l_function_get_nloops(lua_State * L)
{
   f_t *f = luaL_checkudata(L, 1, FUNCTION);
   lua_pushinteger(L, fct_get_nb_loops(f->p));
   return 1;
}

static int l_function_get_nblocks(lua_State * L)
{
   f_t *f = luaL_checkudata(L, 1, FUNCTION);
   lua_pushinteger(L, fct_get_nb_blocks_novirtual(f->p));
   return 1;
}

static int l_function_get_npaddingblocks(lua_State * L)
{
   f_t *f = luaL_checkudata(L, 1, FUNCTION);
   lua_pushinteger(L, queue_length(fct_get_padding_blocks(f->p)));
   return 1;
}

static int l_function_get_ninsns(lua_State * L)
{
   f_t *f = luaL_checkudata(L, 1, FUNCTION);
   lua_pushinteger(L, fct_get_nb_insns(f->p));
   return 1;
}

static int l_function_get_entry(lua_State * L)
{
   f_t *f = luaL_checkudata(L, 1, FUNCTION);
   block_t *block = fct_get_main_entry(f->p);
   if (block != NULL) {
      create_block(L, block);
      return 1;
   }
   return 0;
}

static int l_function_get_entriesb(lua_State * L)
{
   f_t *f = luaL_checkudata(L, 1, FUNCTION);
   queue_t *entriesb = fct_get_entry_blocks(f->p);
   int i = 1;
   if (entriesb != NULL) {
      lua_newtable(L);
      FOREACH_INQUEUE(entriesb, entries_iter) {
         lua_pushnumber(L, i++);
         create_block(L, GET_DATA_T(block_t *, entries_iter));
         lua_settable(L, -3);
      }
      return 1;
   }
   return 0;
}

static int l_function_get_entriesi(lua_State * L)
{
   f_t *f = luaL_checkudata(L, 1, FUNCTION);
   queue_t *entriesi = fct_get_entry_insns(f->p);
   int i = 1;
   if (entriesi != NULL) {
      lua_newtable(L);
      FOREACH_INQUEUE(entriesi, entriesi_iter) {
         lua_pushnumber(L, i++);
         create_insn(L, GET_DATA_T(insn_t *, entriesi_iter));
         lua_settable(L, -3);
      }
      return 1;
   }
   return 0;
}

static int l_function_get_exitsb(lua_State * L)
{
   f_t *f = luaL_checkudata(L, 1, FUNCTION);
   queue_t *exitsb = fct_get_exit_blocks(f->p);
   int i = 1;
   if (exitsb != NULL) {
      lua_newtable(L);
      FOREACH_INQUEUE(exitsb, exitsb_iter) {
         lua_pushnumber(L, i++);
         create_block(L, GET_DATA_T(block_t *, exitsb_iter));
         lua_settable(L, -3);
      }
      return 1;
   }
   return 0;
}

static int l_function_get_exitsi(lua_State * L)
{
   f_t *f = luaL_checkudata(L, 1, FUNCTION);
   queue_t *exitsi = fct_get_exit_insns(f->p);
   int i = 1;
   if (exitsi != NULL) {
      lua_newtable(L);
      FOREACH_INQUEUE(exitsi, exitsi_iter) {
         lua_pushnumber(L, i++);
         create_insn(L, GET_DATA_T(insn_t *, exitsi_iter));
         lua_settable(L, -3);
      }
      return 1;
   }
   return 0;
}

static int l_function_get_ranges(lua_State * L)
{
   f_t *f = luaL_checkudata(L, 1, FUNCTION);
   queue_t *ranges = f->p->ranges;
   int i = 1;
   if (ranges != NULL) {
      lua_newtable(L);
      FOREACH_INQUEUE(ranges, ranges_iter) {
         fct_range_t *range = GET_DATA_T(fct_range_t*, ranges_iter);
         insn_t *tinsn;
         lua_pushnumber(L, i++);
         lua_newtable(L);
         tinsn = fct_range_getstart(range);
         lua_pushliteral(L, "start");
         lua_pushinteger(L, insn_get_addr(tinsn));
         lua_settable(L, -3);
         lua_pushliteral(L, "start_insn");
         create_insn(L, tinsn);
         lua_settable(L, -3);
         tinsn = fct_range_getstop(range);
         lua_pushliteral(L, "stop");
         lua_pushinteger(L, insn_get_addr(tinsn));
         lua_settable(L, -3);
         lua_pushliteral(L, "stop_insn");
         create_insn(L, tinsn);
         lua_settable(L, -3);
         lua_settable(L, -3);
      }
      return 1;
   }
   return 0;
}

static int l_function_get_debug_ranges(lua_State * L)
{
   f_t *f = luaL_checkudata(L, 1, FUNCTION);
   queue_t *ranges = fct_get_ranges(f->p);
   int i = 1;
   if (ranges != NULL) {
      lua_newtable(L);
      FOREACH_INQUEUE(ranges, ranges_iter) {
         fct_range_t *range = GET_DATA_T(fct_range_t*, ranges_iter);
         insn_t *tinsn;
         lua_pushnumber(L, i++);
         lua_newtable(L);
         tinsn = fct_range_getstart(range);
         lua_pushliteral(L, "start");
         lua_pushinteger(L, insn_get_addr(tinsn));
         lua_settable(L, -3);
         lua_pushliteral(L, "start_insn");
         create_insn(L, tinsn);
         lua_settable(L, -3);
         tinsn = fct_range_getstop(range);
         lua_pushliteral(L, "stop");
         lua_pushinteger(L, insn_get_addr(tinsn));
         lua_settable(L, -3);
         lua_pushliteral(L, "stop_insn");
         create_insn(L, tinsn);
         lua_settable(L, -3);
         lua_settable(L, -3);
      }
      return 1;
   }
   return 0;
}

static int l_function_get_first_insn(lua_State * L)
{
   f_t *f = luaL_checkudata(L, 1, FUNCTION);
   insn_t *first_insn = fct_get_first_insn(f->p);
   if (first_insn != NULL) {
      create_insn(L, first_insn);
      return 1;
   }
   return 0;
}

static int l_function_get_CFG_file_path(lua_State * L)
{
   f_t *f = luaL_checkudata(L, 1, FUNCTION);
   char *cfg = lcore_print_function_cfg(f->p);
   if (cfg != NULL) {
      lua_pushstring(L, cfg);

      return 1;
   }
   return 0;
}

static int l_function_get_cc(lua_State * L)
{
   int i = 1, j = 1;
   f_t *f = luaL_checkudata(L, 1, FUNCTION);
   /* Create a table for all CCs */
   lua_newtable(L);
   /* For each connex component */
   FOREACH_INQUEUE(fct_get_components(f->p), qiter) {
      list_t *headlist = GET_DATA_T(list_t*, qiter);
      lua_pushnumber(L, i++);
      /* Create a table for each CC */
      lua_newtable(L);
      /* For each component entry */
      FOREACH_INLIST(headlist, liter) {
         lua_pushnumber(L, j++);
         create_block(L, GET_DATA_T(block_t *, liter));
         lua_settable(L, -3);
      }
      lua_settable(L, -3);
   }

   return 1;
}

static int l_function_get_nb_cc(lua_State * L)
{
   f_t *f = luaL_checkudata(L, 1, FUNCTION);
   lua_pushnumber(L, CFG_NB_CC(f->p));
   return 1;
}

static int l_function_is_external(lua_State * L)
{
   f_t *f = luaL_checkudata(L, 1, FUNCTION);
   int isext = fct_is_external_stub(f->p);
   if (isext)
      lua_pushboolean(L, 1);
   else
      lua_pushboolean(L, 0);
   return 1;
}

/**
 Helper function, internally used by loops_iter() and l_loop_is_innermost()
 */
int loop_is_dominant(loop_t *loop)
{
   if (loop_get_children_node(loop) != NULL)
      return TRUE;
   return FALSE;
}

/**
 This function is internally used by _loops()
 */
static int loops_iter(lua_State * L)
{
   list_t **list = lua_touserdata(L, lua_upvalueindex(1));
   int innermost = lua_toboolean(L, lua_upvalueindex(2));
   if (list != NULL) {
      if (innermost == TRUE) {
         /* Skip non innermost loops */
         while ((*list != NULL) && loop_is_dominant(list_getdata(*list)))
            *list = list_getnext(*list);
      }
      if (*list != NULL) {
         create_loop(L, list_getdata(*list));
         *list = list_getnext(*list);
         return 1;
      }
   }
   return 0;
}

static int _loops(lua_State * L, int only_innermost)
{
   f_t *f = luaL_checkudata(L, 1, FUNCTION);
   queue_t *loops = fct_get_loops(f->p);
   if (loops != NULL) {
      list_t **list = lua_newuserdata(L, sizeof *list);
      *list = queue_iterator(loops);
      lua_pushboolean(L, only_innermost);
   } else {
      /* This case should never occur, even with a loop-free function */
      lua_pushnil(L);
      lua_pushnil(L);
   }
   lua_pushcclosure(L, loops_iter, 2);
   return 1;
}

static int l_function_loops(lua_State * L)
{
   return _loops(L, FALSE);
}

static int l_function_innermost_loops(lua_State * L)
{
   return _loops(L, TRUE);
}

/**
 this function is internally used by l_function_blocks, l_loop_blocks, l_loop_entries and l_loop_exits
 */
int blocks_iter(lua_State * L)
{
   list_t **list = lua_touserdata(L, lua_upvalueindex(1));
   if ((list != NULL) && (*list != NULL)) {
      if (block_is_virtual(list_getdata(*list))) {
         *list = list_getnext(*list);
      }
      if ((list != NULL) && (*list != NULL)) {
         create_block(L, list_getdata(*list));
         *list = list_getnext(*list);
         return 1;
      }
   }
   return 0;
}

static int l_function_blocks(lua_State * L)
{
   f_t *f = luaL_checkudata(L, 1, FUNCTION);
   queue_t *blocks = fct_get_blocks(f->p);
   if (blocks != NULL) {
      list_t **list = lua_newuserdata(L, sizeof *list);
      *list = queue_iterator(blocks);
   } else {
      /* This case should never occur, even with an empty function */
      lua_pushnil(L);
   }
   lua_pushcclosure(L, blocks_iter, 1);
   return 1;
}

static int l_function_get_nb_paths(lua_State * L)
{
   f_t *f = luaL_checkudata(L, 1, FUNCTION);

   lua_pushinteger(L, lcore_fct_getnpaths(f->p));

   return 1;
}

/**
 this function is internally used by l_loop_paths
 */
static int paths_iter(lua_State * L)
{
   list_t **list = lua_touserdata(L, lua_upvalueindex(1));

   if ((list != NULL) && (*list != NULL)) {
      int i = 1;

      lua_newtable(L);

      array_t *path = list_getdata(*list);
      FOREACH_INARRAY(path, iter) {
         create_block(L, *iter);
         lua_rawseti(L, -2, i++);
      }

      *list = list_getnext(*list);

      return 1;
   }

   return 0;
}

static int l_function_paths(lua_State * L)
{
   f_t *f = luaL_checkudata(L, 1, FUNCTION);
   queue_t *paths = fct_get_paths(f->p);

   if (paths == NULL) {
      lcore_fct_computepaths(f->p);
      paths = fct_get_paths(f->p);
   }

   if (paths != NULL) {
      list_t **list = lua_newuserdata(L, sizeof *list);
      *list = queue_iterator(paths);
   } else
      lua_pushnil(L);

   lua_pushcclosure(L, paths_iter, 1);

   return 1;
}

static int l_function_are_paths_computed(lua_State *L)
{
   f_t *f = luaL_checkudata(L, 1, FUNCTION);
   queue_t *paths = fct_get_paths(f->p);

   lua_pushboolean(L, (paths != NULL));

   return 1;
}

static int l_function_free_paths(lua_State *L)
{
   f_t *f = luaL_checkudata(L, 1, FUNCTION);

   lcore_fct_freepaths(f->p);

   return 0;
}

/**
 this function is internally used by l_function_padding_blocks
 */
static int padding_blocks_iter(lua_State * L)
{
   list_t **list = lua_touserdata(L, lua_upvalueindex(1));
   if ((list != NULL) && (*list != NULL)) {
      create_block(L, list_getdata(*list));
      *list = list_getnext(*list);
      return 1;
   }
   return 0;
}

static int l_function_padding_blocks(lua_State * L)
{
   f_t *f = luaL_checkudata(L, 1, FUNCTION);
   if (f == NULL)
      return 0;
   queue_t *blocks = fct_get_padding_blocks(f->p);
   if (blocks != NULL) {
      list_t **list = lua_newuserdata(L, sizeof *list);
      *list = queue_iterator(blocks);
   } else {
      /* This case should never occur, even with an empty function */
      lua_pushnil(L);
   }
   lua_pushcclosure(L, padding_blocks_iter, 1);
   return 1;
}

/* TODO: should be moved elsewhere */
static int l_function_analyze_grouping(lua_State *L)
{
   f_t *f = luaL_checkudata(L, 1, FUNCTION);
   lcore_fct_analyze_groups(f->p);
   return 0;
}

static int l_function_analyze_grouping_extend(lua_State *L)
{
   f_t *f = luaL_checkudata(L, 1, FUNCTION);
   long user = luaL_checkinteger(L, 2);
   lcore_group_stride(f->p);
   lcore_group_memory(f->p, (void*) user);
   return 0;
}

static int l_fct_get_predecessors(lua_State * L)
{
   f_t *b = luaL_checkudata(L, 1, FUNCTION);
   graph_node_t *CG_node = fct_get_CG_node(b->p);
   if ((CG_node != NULL) && (CG_node->in != NULL)) {
      int i = 1;
      lua_newtable(L);
      FOREACH_INLIST(CG_node->in, iter) {
         fct_t *fct = GET_DATA_T (graph_edge_t *, iter)->from->data;
         create_function(L, fct);
         lua_rawseti(L, -2, i++);
      }
      return 1;
   }
   return 0;
}

/**
 this function is internally used by l_fct_predecessors()
 */
static int fct_predecessor_iter(lua_State * L)
{
   list_t **list = lua_touserdata(L, lua_upvalueindex(1));
   if ((list != NULL) && (*list != NULL)) {
      graph_edge_t *edge = list_getdata(*list);
      create_function(L, edge->from->data);
      *list = list_getnext(*list);
      return 1;
   }
   return 0;
}

static int l_fct_predecessors(lua_State * L)
{
   f_t *b = luaL_checkudata(L, 1, FUNCTION);
   graph_node_t *CG_node = fct_get_CG_node(b->p);
   if ((CG_node != NULL) && (CG_node->in != NULL)) {
      list_t **list = lua_newuserdata(L, sizeof *list);
      *list = CG_node->in;
   } else
      lua_pushnil(L);
   lua_pushcclosure(L, fct_predecessor_iter, 1);
   return 1;
}

static int l_fct_get_successors(lua_State * L)
{
   f_t *b = luaL_checkudata(L, 1, FUNCTION);
   graph_node_t *CG_node = fct_get_CG_node(b->p);
   if ((CG_node != NULL) && (CG_node->out != NULL)) {
      int i = 1;
      lua_newtable(L);
      FOREACH_INLIST(CG_node->out, iter) {
         fct_t *fct = GET_DATA_T (graph_edge_t *, iter)->to->data;
         create_function(L, fct);
         lua_rawseti(L, -2, i++);
      }
      return 1;
   }
   return 0;
}

/**
 this function is internally used by l_fct_successors()
 */
static int fct_successor_iter(lua_State * L)
{
   list_t **list = lua_touserdata(L, lua_upvalueindex(1));
   if ((list != NULL) && (*list != NULL)) {
      graph_edge_t *edge = list_getdata(*list);
      create_function(L, edge->to->data);
      *list = list_getnext(*list);
      return 1;
   }
   return 0;
}

static int l_fct_successors(lua_State * L)
{
   f_t *b = luaL_checkudata(L, 1, FUNCTION);
   graph_node_t *CG_node = fct_get_CG_node(b->p);
   if ((CG_node != NULL) && (CG_node->out != NULL)) {
      list_t **list = lua_newuserdata(L, sizeof *list);
      *list = CG_node->out;
   } else
      lua_pushnil(L);
   lua_pushcclosure(L, fct_successor_iter, 1);
   return 1;
}

static int function_gc(lua_State * L)
{
   (void) L;
   return 0;
}

static int function_tostring(lua_State * L)
{
   f_t *f = lua_touserdata(L, 1);
   lua_pushfstring(L, "Function: %s (%d)", fct_get_name(f->p),
         fct_get_id(f->p));
   return 1;
}

static int l_function_get_original_function(lua_State * L)
{
   f_t *f = luaL_checkudata(L, 1, FUNCTION);
   fct_t *original = fct_get_original_function(f->p);
   if (original != NULL) {
      create_function(L, original);
      return 1;
   }
   return 0;
}

static int l_function_get_return_var(lua_State * L)
{
   f_t *f = luaL_checkudata(L, 1, FUNCTION);
   switch (f->p->asmfile->debug->format) {
   case DBG_FORMAT_DWARF: {
      if (f->p->debug == NULL) {
         lua_pushnil(L);
         return (1);
      }
      DwarfVar* var = dwarf_function_get_returned_var(f->p->debug->data);
      if (dwarf_var_get_full_type(var) != NULL)
         lua_pushstring(L, dwarf_var_get_full_type(var));
      else
         lua_pushnil(L);
   }
      break;
   default:
      lua_pushnil(L);
      return (1);
      break;
   }
   return (1);
}

static int l_function_get_parameters(lua_State * L)
{
   f_t *f = luaL_checkudata(L, 1, FUNCTION);
   switch (f->p->asmfile->debug->format) {
   case DBG_FORMAT_DWARF: {
      if (f->p->debug == NULL) {
         lua_pushnil(L);
         return (1);
      }
      queue_t* params = dwarf_function_get_parameters(f->p->debug->data);
      int i = 1;
      lua_newtable(L);
      FOREACH_INQUEUE(params, it_params)
      {
         DwarfVar* var = GET_DATA_T(DwarfVar*, it_params);
         lua_newtable(L);
         lua_pushliteral(L, "type");
         if (dwarf_var_get_full_type(var) != NULL)
            lua_pushstring(L, dwarf_var_get_full_type(var));
         else
            lua_pushstring(L, "");
         lua_settable(L, -3);
         lua_pushliteral(L, "name");
         lua_pushstring(L, dwarf_var_get_name(var));
         lua_settable(L, -3);
         DwarfMemLoc* loc = dwarf_var_get_location(var);
         if (loc != NULL) {
            lua_pushliteral(L, "location");
            lua_newtable(L);
            switch (dwarf_memloc_get_type(loc)) {
            case DWARF_REG: {
               reg_t* reg = dwarf_memloc_get_register(loc);
               lua_pushliteral(L, "reg");
               if (reg != NULL) {
                  lua_pushstring(L,
                        arch_get_reg_name(f->p->asmfile->arch, reg->type,
                              reg->name));
               } else {
                  char* name = lc_malloc(5 * sizeof(char));
                  sprintf(name, "r%d", dwarf_memloc_get_register_index(loc));
               }
               lua_settable(L, -3);
               break;
            }
            case DWARF_ADDR:
               lua_pushliteral(L, "address");
               lua_pushnumber(L, dwarf_memloc_get_address(loc));
               lua_settable(L, -3);
               break;
            case DWARF_BREG:
            case DWARF_FBREG: {
               lua_pushliteral(L, "offset");
               lua_pushnumber(L, dwarf_memloc_get_offset(loc));
               lua_settable(L, -3);
               reg_t* reg = dwarf_memloc_get_register(loc);
               lua_pushliteral(L, "index");
               if (reg != NULL) {
                  lua_pushstring(L,
                        arch_get_reg_name(f->p->asmfile->arch, reg->type,
                              reg->name));
               } else {
                  char* name = lc_malloc(5 * sizeof(char));
                  sprintf(name, "r%d", dwarf_memloc_get_register_index(loc));
               }
               lua_settable(L, -3);
               break;
            }
            case DWARF_FBREG_TBRES:
               lua_pushliteral(L, "offset");
               lua_pushnumber(L, dwarf_memloc_get_offset(loc));
               lua_settable(L, -3);
               break;
            }
            lua_settable(L, -3);
         }
         lua_rawseti(L, -2, i++);
      }
   }
      break;
   default:
      lua_pushnil(L);
      return (1);
      break;
   }
   return (1);
}

static int l_function_analyze_polytopes(lua_State * L)
{
   f_t *f = luaL_checkudata(L, 1, FUNCTION);
   if (f != NULL)
      lcore_fct_analyze_polytopes(f->p);
   return (0);
}

static int l_function_analyze_live_registers(lua_State * L)
{
   f_t *f = luaL_checkudata(L, 1, FUNCTION);
   int mode = FALSE;

   if (lua_isnoneornil (L,2) == 0)
      mode = lua_toboolean(L, 2);

   if (f != NULL) {
      int nb_reg;
      lcore_compute_live_registers(f->p, &nb_reg, mode);
      lua_pushinteger(L, nb_reg);
   }
   return (1);
}

static int l_function_get_live_registers(lua_State * L)
{
   f_t *f = luaL_checkudata(L, 1, FUNCTION);
   b_t *b = luaL_checkudata(L, 2, BLOCK);
   int nb_registers = luaL_checkinteger(L, 3);
   if (f != NULL && b != NULL) {
      int i;
      unsigned int id = b->p->id;
      lua_newtable(L);
      for (i = 0; i < nb_registers; i++) {
         lua_pushinteger(L, f->p->live_registers[id][i]);
         lua_rawseti(L, -2, (i + 1));
      }
      return 1;
   }
   return 0;
}

static int l_function_free_live_analysis(lua_State * L)
{
   f_t *f = luaL_checkudata(L, 1, FUNCTION);
   if (f != NULL)
      lcore_free_live_registers(f->p);
   return (1);
}

static int l_function_get_polytopes(lua_State * L)
{
   f_t *f = luaL_checkudata(L, 1, FUNCTION);
   arch_t* arch = f->p->asmfile->arch;
   lua_newtable(L);
   queue_t** polytopes = lcore_get_polytopes_from_fct(f->p);
   if (polytopes == NULL)
      return (1);
   FOREACH_INQUEUE(f->p->loops, it_loop) {
      loop_t* loop = GET_DATA_T(loop_t*, it_loop);
      if (polytopes[loop->id] != NULL) {
         FOREACH_INQUEUE(polytopes[loop->id], it_polytope)
         {
            polytope_t* polytope = GET_DATA_T(polytope_t*, it_polytope);
            /*char* str = polytope_tostring (polytope);
             printf ("%s\n", str);
             lc_free (str);*/
            lua_pushnumber(L, polytope->ssain->in->address);
            lua_newtable(L);

            // expression ----------------------------
            lua_pushliteral(L, "expression");
            lua_pushstring(L, polytope->acces_str);
            lua_settable(L, -3);

            // computed ------------------------------
            lua_pushliteral(L, "computed");
            lua_pushboolean(L, polytope->computed);
            lua_settable(L, -3);

            // expression code -----------------------
            lua_pushliteral(L, "expression_code");
            char* tmp = polytope_toLuagraph(polytope);
            lua_pushstring(L, tmp);
            lua_settable(L, -3);
            lc_free(tmp);

            // level ---------------------------------
            lua_pushliteral(L, "level");
            lua_pushnumber(L, polytope->level);
            lua_settable(L, -3);

            // registers -----------------------------
            int i = 1;
            lua_pushliteral(L, "registers");
            lua_newtable(L);
            FOREACH_INQUEUE(polytope->registers, it_reg) {
               ssa_var_t* reg = GET_DATA_T(ssa_var_t*, it_reg);
               int64_t addr = 0;
               char tmp[15];
               memset(&tmp[0], '\0', 15 * sizeof(char));
               sprintf(tmp, "%s_%d",
                     arch_get_reg_name(arch, reg->reg->type, reg->reg->name),
                     reg->index);
               if (reg->insn != NULL && reg->insn->in != NULL)
                  addr = reg->insn->in->address;
               else if (reg->insn != NULL && reg->insn->in == NULL)
                  addr =
                        ((insn_t*) reg->insn->ssab->block->begin_sequence->data)->address;
               else if (reg->insn == NULL)
                  addr =
                        ((insn_t*) ((block_t*) queue_peek_head(f->p->entries))->begin_sequence->data)->address;
               lua_newtable(L);
               lua_pushliteral(L, "reg");
               lua_pushstring(L, tmp);
               lua_settable(L, -3);
               lua_pushliteral(L, "address");
               lua_pushnumber(L, addr);
               lua_settable(L, -3);
               lua_pushliteral(L, "str");
               lua_pushstring(L,
                     arch_get_reg_name(arch, reg->reg->type, reg->reg->name));
               lua_settable(L, -3);
               lua_pushliteral(L, "id");
               lua_pushnumber(L, reg->index);
               lua_settable(L, -3);
               lua_rawseti(L, -2, i++);
            }
            lua_settable(L, -3);
            // induction_reg --------------------
            if (polytope->induction != NULL
                  && polytope->induction->add->type == IND_NODE_IMM
                  && polytope->induction->mul->type == IND_NODE_IMM) {
               // as induction detection depends on stop_bound_reg and as
               // stop_bound_reg depends on operands, this is always not NULL
               ssa_var_t* var = polytope->stop_bound_insn->oprnds[2];
               lua_pushliteral(L, "induction_register");
               lua_newtable(L);
               lua_pushliteral(L, "str");
               lua_pushstring(L,
                     arch_get_reg_name(arch, var->reg->type, var->reg->name));
               lua_settable(L, -3);
               lua_pushliteral(L, "id");
               lua_pushnumber(L, var->index);
               lua_settable(L, -3);
               lua_pushliteral(L, "val");
               lua_pushnumber(L, polytope->induction->add->data.imm);
               lua_settable(L, -3);
               lua_settable(L, -3);
            }
            // stop_bound_reg -------------------
            if (polytope->stop_bound_insn != NULL) {
               ssa_var_t* var = polytope->stop_bound_insn->oprnds[2];
               lua_pushliteral(L, "stop_bound_register");
               lua_newtable(L);
               lua_pushliteral(L, "str");
               lua_pushstring(L,
                     arch_get_reg_name(arch, var->reg->type, var->reg->name));
               lua_settable(L, -3);
               lua_pushliteral(L, "id");
               lua_pushnumber(L, var->index);
               lua_settable(L, -3);
               lua_pushliteral(L, "val");
               lua_pushnumber(L,
                     oprnd_get_imm(
                           insn_get_oprnd(polytope->stop_bound_insn->in, 0)));
               lua_settable(L, -3);
               lua_settable(L, -3);
            }
            // start_bound_reg ------------------
            if (polytope->start_bound_insn != NULL) {
               ssa_var_t* var = polytope->start_bound_insn->output[0];
               char tmp[256];
               tmp[0] = '\0';
               polytope_val_tostring(polytope->start_bound_val, arch, tmp);
               lua_pushliteral(L, "start_bound_register");
               lua_newtable(L);
               lua_pushliteral(L, "str");
               lua_pushstring(L,
                     arch_get_reg_name(arch, var->reg->type, var->reg->name));
               lua_settable(L, -3);
               lua_pushliteral(L, "id");
               lua_pushnumber(L, var->index);
               lua_settable(L, -3);
               lua_pushliteral(L, "val");
               lua_pushstring(L, tmp);
               lua_settable(L, -3);
               lua_settable(L, -3);
            }
            lua_settable(L, -3);
         }
      }
   }
   return (1);
}

static int l_function_get_debug_variables(lua_State * L)
{
   f_t *f = luaL_checkudata(L, 1, FUNCTION);
   lua_newtable(L);
   if (f->p->debug == NULL || f->p->debug->data == NULL)
      return (1);
   int i = 1;
   DwarfFunction *func = f->p->debug->data;
   arch_t* arch = f->p->asmfile->arch;
   queue_t* vars = dwarf_function_get_local_variables(func);
   FOREACH_INQUEUE(vars, it_vars) {
      DwarfVar* var = GET_DATA_T(DwarfVar*, it_vars);
      lua_newtable(L);
      lua_pushliteral(L, "name");
      lua_pushstring(L, dwarf_var_get_name(var));
      lua_settable(L, -3);
      lua_pushliteral(L, "type");
      lua_pushstring(L, dwarf_var_get_full_type(var));
      lua_settable(L, -3);
      lua_pushliteral(L, "line");
      lua_pushnumber(L, dwarf_var_get_source_line(var));
      lua_settable(L, -3);
      lua_pushliteral(L, "column");
      lua_pushnumber(L, dwarf_var_get_source_column(var));
      lua_settable(L, -3);
      lua_pushliteral(L, "location");
      lua_newtable(L);
      DwarfMemLoc* loc = dwarf_var_get_location(var);
      if (loc != NULL) {
         switch (dwarf_memloc_get_type(loc)) {
         case DWARF_REG:
            if (dwarf_memloc_get_register(loc) != NULL) {
               reg_t* reg = dwarf_memloc_get_register(loc);
               lua_pushliteral(L, "reg");
               lua_pushstring(L, arch_get_reg_name(arch, reg->type, reg->name));
               lua_settable(L, -3);
            }
            break;
         case DWARF_BREG:
         case DWARF_FBREG:
            if (dwarf_memloc_get_register(loc) != NULL) {
               reg_t* reg = dwarf_memloc_get_register(loc);
               lua_pushliteral(L, "reg");
               lua_pushstring(L, arch_get_reg_name(arch, reg->type, reg->name));
               lua_settable(L, -3);
               lua_pushliteral(L, "offset");
               lua_pushnumber(L, dwarf_memloc_get_offset(loc));
               lua_settable(L, -3);
            }
            break;
         case DWARF_ADDR:
            lua_pushliteral(L, "address");
            lua_pushnumber(L, dwarf_memloc_get_address(loc));
            lua_settable(L, -3);
            break;
         case DWARF_FBREG_TBRES:
            lua_pushliteral(L, "offset");
            lua_pushnumber(L, dwarf_memloc_get_offset(loc));
            lua_settable(L, -3);
            break;
         default:
            break;
         }
      }
      lua_settable(L, -3);
      lua_rawseti(L, -2, i++);
   }
   return (1);
}

static int l_function_get_compile_options(lua_State * L)
{
   f_t *f = luaL_checkudata(L, 1, FUNCTION);
   char *opts = fct_get_compile_options(f->p);
   if (opts != NULL)
      lua_pushstring(L, opts);
   else
      lua_pushnil(L);
   return 1;
}

static int l_function_get_src_lines(lua_State * L)
{
   f_t *f = luaL_checkudata(L, 1, FUNCTION);

   unsigned min, max;
   fct_get_src_lines (f->p, &min, &max);
   lua_pushnumber (L, min);
   lua_pushnumber (L, max);

   return 2;
}

static int l_function_get_src_regions(lua_State * L)
{
   f_t *f = luaL_checkudata(L, 1, FUNCTION);

   lua_newtable(L);

   int i = 1;
   queue_t *src_regions = fct_get_src_regions (f->p);
   FOREACH_INQUEUE (src_regions, it_reg) {
      char *region = GET_DATA_T(char *, it_reg);
      lua_pushnumber(L, i++);
      lua_pushstring(L, region);
      lua_settable(L, -3);
   }
   queue_free (src_regions, NULL);

   return 1;
}

static int l_function_get_DDG(lua_State* L)
{
   f_t *f = luaL_checkudata(L, 1, FUNCTION);
   graph_t *DDG = lcore_fct_getddg(f->p);

   if (DDG != NULL) {
      create_graph (L, DDG);
      return 1;
   }

   return 0;
}

static int l_function_get_DDG_file_path(lua_State * L)
{
   f_t *f = luaL_checkudata(L, 1, FUNCTION);
   char *dotfile_name = lcore_print_fct_ddg(f->p);

   if (dotfile_name != NULL) {
      lua_pushstring(L, dotfile_name);
      lcore_print_fct_ddg_paths(f->p);

      return 1;
   }

   return 0;
}

/**
 * Bind names from this file to LUA
 * For example, {"foo", "bar"} will be interpreted in the following way:
 * to use bar (defined in this file), call foo in LUA.
 */
const luaL_reg function_methods[] = {
   {"get_project"             , l_function_get_project},
   {"get_asmfile"             , l_function_get_asmfile},
   {"has_debug_data"          , l_function_has_debug_data},
   {"get_src_file_name"       , l_function_get_src_file_name},
   {"get_src_file_path"       , l_function_get_src_file_path},
   {"get_compiler_short"      , l_function_get_compiler_short},
   {"get_compiler_version"    , l_function_get_compiler_version},
   {"get_language"            , l_function_get_language},
   {"get_producer"            , l_function_get_producer},
   {"get_decl_line"           ,l_function_get_decl_line},
   {"get_dir"                 , l_function_get_dir},
   {"get_name"                , l_function_get_name},
   {"get_demname"             , l_function_get_demname},
   {"get_id"                  , l_function_get_id},
   {"get_nloops"              , l_function_get_nloops},
   {"get_nblocks"             , l_function_get_nblocks},
   {"get_npaddingblocks"      , l_function_get_npaddingblocks},
   {"get_ninsns"              , l_function_get_ninsns},
   {"get_entry"               , l_function_get_entry},
   {"get_entriesb"            , l_function_get_entriesb},
   {"get_entriesi"            , l_function_get_entriesi},
   {"get_exitsb"              , l_function_get_exitsb},
   {"get_exitsi"              , l_function_get_exitsi},
   {"get_ranges"              , l_function_get_ranges},
   {"get_debug_ranges"        , l_function_get_debug_ranges},
   {"get_first_block"         , l_function_get_entry},
   {"get_first_insn"          , l_function_get_first_insn},
   {"get_CFG_file_path"       , l_function_get_CFG_file_path},
   {"get_CC"                  , l_function_get_cc},
   {"get_nCCs"                , l_function_get_nb_cc},
   {"analyze_groups"          , l_function_analyze_grouping},
   {"analyze_groups_extend"   , l_function_analyze_grouping_extend},
   {"analyze_live_registers"  , l_function_analyze_live_registers},
   {"get_live_registers"      , l_function_get_live_registers},
   {"free_live_analysis"      , l_function_free_live_analysis},
   {"loops"                   , l_function_loops},
   {"blocks"                  , l_function_blocks},
   {"get_nb_paths"            , l_function_get_nb_paths},
   {"paths"                   , l_function_paths},
   {"are_paths_computed"      , l_function_are_paths_computed},
   {"free_paths"              , l_function_free_paths},
   {"padding_blocks"          , l_function_padding_blocks},
   {"innermost_loops"         , l_function_innermost_loops},
   {"get_predecessors"        , l_fct_get_predecessors},
   {"get_successors"          , l_fct_get_successors},
   {"predecessors"            , l_fct_predecessors},
   {"successors"              , l_fct_successors},
   {"get_original_function"   , l_function_get_original_function},
   {"get_return_var"          , l_function_get_return_var},
   {"get_parameters"          , l_function_get_parameters},
   {"analyze_polytopes"       , l_function_analyze_polytopes},
   {"get_polytopes"           , l_function_get_polytopes},
   {"get_debug_variables"     , l_function_get_debug_variables},
   {"get_compile_options"     , l_function_get_compile_options},
   {"is_external_stub"        , l_function_is_external},
   {"get_src_lines"           , l_function_get_src_lines},
   {"get_src_regions"         , l_function_get_src_regions},
   {"get_DDG"                 , l_function_get_DDG},
   {"get_DDG_file_path"       , l_function_get_DDG_file_path},
   {NULL, NULL}
};

const luaL_reg function_meta[] = {
   {"__gc"      , function_gc},
   {"__tostring", function_tostring},
   {NULL, NULL}
};
