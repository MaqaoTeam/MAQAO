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

#include "libmcore.h"
#include "arch.h"
#include "abstract_objects_c.h"

/******************************************************************/
/*                Functions dealing with Block                    */
/******************************************************************/

static int l_block_get_project(lua_State * L)
{
   b_t *b = luaL_checkudata(L, 1, BLOCK);
   project_t *project = block_get_project(b->p);

   if (project != NULL) {
      create_project(L, project, FALSE);

      return 1;
   }

   return 0;
}

static int l_block_get_asmfile(lua_State * L)
{
   b_t *b = luaL_checkudata(L, 1, BLOCK);
   asmfile_t *asmfile = block_get_asmfile(b->p);

   if (asmfile != NULL) {
      create_asmfile(L, asmfile);

      return 1;
   }

   return 0;
}

static int l_block_get_function(lua_State * L)
{
   b_t *b = luaL_checkudata(L, 1, BLOCK);
   fct_t *function = block_get_fct(b->p);

   if (function != NULL) {
      create_function(L, function);

      return 1;
   }

   return 0;
}

static int l_block_get_loop(lua_State * L)
{
   b_t *b = luaL_checkudata(L, 1, BLOCK);
   loop_t *loop = block_get_loop(b->p);

   if (loop != NULL) {
      create_loop(L, loop);

      return 1;
   }

   return 0;
}

static int l_block_get_id(lua_State * L)
{
   b_t *b = luaL_checkudata(L, 1, BLOCK);

   lua_pushinteger(L, block_get_id(b->p));

   return 1;
}

static int l_block_is_loop_entry(lua_State * L)
{
   b_t *b = luaL_checkudata(L, 1, BLOCK);

   if (block_is_loop_entry(b->p) == TRUE)
      lua_pushboolean(L, 1);
   else
      lua_pushboolean(L, 0);

   return 1;
}

static int l_block_is_loop_exit(lua_State * L)
{
   b_t *b = luaL_checkudata(L, 1, BLOCK);

   if (block_is_loop_exit(b->p) == TRUE)
      lua_pushboolean(L, 1);
   else
      lua_pushboolean(L, 0);

   return 1;
}

static int l_block_is_function_exit(lua_State * L)
{
   b_t *b = luaL_checkudata(L, 1, BLOCK);
   lua_pushboolean(L, block_is_function_exit(b->p));

   return 1;
}

static int l_block_get_imm_dominator(lua_State * L)
{
   b_t *b = luaL_checkudata(L, 1, BLOCK);
   tree_t *parent = block_get_dominant_parent(b->p);

   if (parent != NULL) {
      create_block(L, tree_getdata(parent));

      return 1;
   }

   return 0;
}

static int l_block_get_imm_dominated(lua_State * L)
{
   b_t *b = luaL_checkudata(L, 1, BLOCK);
   tree_t *children = block_get_dominant_children(b->p);

   if (children != NULL) {
      tree_t *iter;

      lua_newtable(L);

      for (iter = children; iter != NULL; iter = iter->next) {
         block_t *block = tree_getdata(iter);

         create_block(L, block);
         lua_rawseti(L, -2, block_get_id(block));
      }

      return 1;
   }

   return 0;
}

static int l_block_get_imm_postdominator(lua_State * L)
{
   b_t *b = luaL_checkudata(L, 1, BLOCK);
   tree_t *parent = block_get_post_dominant_parent(b->p);

   if (parent != NULL) {
      create_block(L, tree_getdata(parent));

      return 1;
   }

   return 0;
}

static int l_block_get_imm_postdominated(lua_State * L)
{
   b_t *b = luaL_checkudata(L, 1, BLOCK);
   tree_t *children = block_get_post_dominant_children(b->p);

   if (children != NULL) {
      tree_t *iter;

      lua_newtable(L);

      for (iter = children; iter != NULL; iter = iter->next) {
         block_t *block = tree_getdata(iter);

         create_block(L, block);
         lua_rawseti(L, -2, block_get_id(block));
      }

      return 1;
   }

   return 0;
}

static int l_block_get_first_insn(lua_State * L)
{
   b_t *b = luaL_checkudata(L, 1, BLOCK);
   insn_t *first_insn = block_get_first_insn(b->p);

   if (first_insn != NULL) {
      create_insn(L, first_insn);

      return 1;
   }

   return 0;
}

static int l_block_get_last_insn(lua_State * L)
{
   b_t *b = luaL_checkudata(L, 1, BLOCK);
   insn_t *last_insn = block_get_last_insn(b->p);

   if (last_insn != NULL) {
      create_insn(L, last_insn);

      return 1;
   }

   return 0;
}

static int l_block_is_back_edge_origin(lua_State * L)
{
   b_t *src = luaL_checkudata(L, 1, BLOCK);
   b_t *dst = luaL_checkudata(L, 2, BLOCK);

   if (lcore_blocks_backedgenodes(src->p, dst->p))
      lua_pushboolean(L, 1);
   else
      lua_pushboolean(L, 0);

   return 1;
}

static int l_block_is_padding(lua_State * L)
{
   b_t *src = luaL_checkudata(L, 1, BLOCK);

   if (block_is_padding(src->p))
      lua_pushboolean(L, 1);
   else
      lua_pushboolean(L, 0);

   return 1;
}

static int l_block_is_virtual(lua_State * L)
{
   b_t *src = luaL_checkudata(L, 1, BLOCK);

   if (block_is_virtual(src->p))
      lua_pushboolean(L, 1);
   else
      lua_pushboolean(L, 0);

   return 1;
}

static int l_block_get_defined_registers(lua_State * L)
{
   block_t *block = ((b_t *) luaL_checkudata(L, 1, BLOCK))->p;
   insn_t *insn = ((i_t *) luaL_checkudata(L, 2, INSN))->p;

   fct_t *f = block_get_fct(block);
   arch_t* arch = f->asmfile->arch;
   insn_t* in = NULL;
   oprnd_t* op = NULL;
   reg_t* V = NULL;
   reg_t** implicits = NULL;
   int (*_regID)(reg_t*, arch_t*) = &__regID;
   int nb_implicits = 0;

   if (arch->code == ARCH_arm64) {
#ifdef _ARCHDEF_arm64
      _regID = &arm64_cs_regID;
#else
      return 0;
#endif
   } 

   int j = 1, i;
   lua_newtable(L);

   //Iterate over instructions in the block
   FOREACH_INSN_INBLOCK(block, it_in) {
      in = GET_DATA_T(insn_t*, it_in);
      if (in == insn)
         break;

      // -------------------------------------------------------------------
      // Use: Iterate over operands to get registers defined
      for (i = 0; i < insn_get_nb_oprnds(in); i++) {
         op = insn_get_oprnd(in, i);
         if (oprnd_is_dst(op) && oprnd_is_reg(op)) {
            V = oprnd_get_reg(op);

            lua_pushinteger(L, _regID(V, arch));
            lua_rawseti(L, -2, (j++));
         }
      }

      // Handle calls to external functions. Based on the AMD64 System V ABI.
      if (((insn_get_annotate(in) & A_CALL) != 0)) {
         for (i = 0; i < arch->nb_return_regs; i++) {
            V = arch->return_regs[i];
            lua_pushinteger(L, _regID(V, arch));
            lua_rawseti(L, -2, (j++));
         }
      }

      implicits = f->asmfile->arch->get_implicite_dst(f->asmfile->arch,
            insn_get_opcode_code(in), &nb_implicits);
      for (i = 0; i < nb_implicits; i++) {
         V = implicits[i];
         lua_pushinteger(L, _regID(V, arch));
         lua_rawseti(L, -2, (j++));
      }
      if (implicits != NULL)
         lc_free(implicits);
   }

   return 1;
}

static int l_block_get_predecessors(lua_State * L)
{
   b_t *b = luaL_checkudata(L, 1, BLOCK);
   queue_t *predecessors = block_get_predecessors (b->p);

   if (predecessors != NULL) {
      int i = 1;
      lua_newtable(L);

      FOREACH_INQUEUE (predecessors, iter) {
         block_t *block = GET_DATA_T(block_t*, iter);

         create_block(L, block);
         lua_rawseti(L, -2, i++);
      }
      queue_free (predecessors, NULL);

      return 1;
   }

   return 0;
}

/**
 this function is internally used by l_block_predecessors()
 */
static int block_predecessor_iter(lua_State * L)
{
   queue_t **predecessors = lua_touserdata(L, lua_upvalueindex(1));
   list_t **list = lua_touserdata(L, lua_upvalueindex(2));

   if ((list != NULL) && (*list != NULL)) {
      block_t *block = list_getdata(*list);

      create_block(L, block);
      *list = list_getnext(*list);

      return 1;
   }

   queue_free (*predecessors, NULL);

   return 0;
}

static int l_block_predecessors(lua_State * L)
{
   b_t *b = luaL_checkudata(L, 1, BLOCK);
   queue_t *predecessors = block_get_predecessors(b->p);

   if (predecessors != NULL) {
      queue_t **queue = lua_newuserdata(L, sizeof *queue);
      list_t **list = lua_newuserdata(L, sizeof *list);

      *queue = predecessors;
      *list = queue_iterator (predecessors);
   } else
      lua_pushnil(L);

   lua_pushcclosure(L, block_predecessor_iter, 2);

   return 1;
}

static int l_block_get_successors(lua_State * L)
{
   b_t *b = luaL_checkudata(L, 1, BLOCK);
   queue_t *successors = block_get_successors (b->p);

   if (successors != NULL) {
      int i = 1;
      lua_newtable(L);

      FOREACH_INQUEUE (successors, iter) {
         block_t *block = GET_DATA_T(block_t*, iter);

         create_block(L, block);
         lua_rawseti(L, -2, i++);
      }
      queue_free (successors, NULL);

      return 1;
   }

   return 0;
}

/**
 this function is internally used by l_block_successors()
 */
static int block_successor_iter(lua_State * L)
{
   queue_t **successors = lua_touserdata(L, lua_upvalueindex(1));
   list_t **list = lua_touserdata(L, lua_upvalueindex(2));

   if ((list != NULL) && (*list != NULL)) {
      block_t *block = list_getdata(*list);

      create_block(L, block);
      *list = list_getnext(*list);

      return 1;
   }

   queue_free (*successors, NULL);

   return 0;
}

static int l_block_successors(lua_State * L)
{
   b_t *b = luaL_checkudata(L, 1, BLOCK);
   queue_t *successors = block_get_successors(b->p);

   if (successors != NULL) {
      queue_t **queue = lua_newuserdata(L, sizeof *queue);
      list_t **list = lua_newuserdata(L, sizeof *list);

      *queue = successors;
      *list = queue_iterator (successors);
   } else
      lua_pushnil(L);

   lua_pushcclosure(L, block_successor_iter, 2);

   return 1;
}

/*
 * The instruction list is only created for an asmfile.
 * It is then not possible to iterate over the end of the list.
 * The over variable is here to stop when the last instruction of the block is found.
 */
static int instructions_iter(lua_State *L)
{
   list_t **list = lua_touserdata(L, lua_upvalueindex(1));
   int over = lua_tointeger(L, lua_upvalueindex(2));

   if ((over == FALSE) && (list != NULL) && (*list != NULL)) {
      insn_t *insn = list_getdata(*list);

      if (insn == NULL) {
         STDMSG("A NULL instruction has been detected, skeeping instruction...");

         return 0;
      }

      /* If the last instruction of the block was found */
      if (insn == block_get_last_insn(insn_get_block(insn))) {
         lua_pushnumber(L, 1); /* Push 1 (TRUE) to stop at next iteration */
         lua_replace(L, lua_upvalueindex(2));
      }

      create_insn(L, insn);
      *list = list_getnext(*list);

      return 1;
   }

   return 0;
}

static int l_block_instructions(lua_State * L)
{
   b_t *b = luaL_checkudata(L, 1, BLOCK);
   list_t **list = lua_newuserdata(L, sizeof *list);

   *list = insn_get_sequence(block_get_first_insn(b->p));
   lua_pushinteger(L, 0);
   lua_pushcclosure(L, instructions_iter, 2);

   return 1;
}

static int l_block_get_src_file_path(lua_State * L)
{
   b_t *b = luaL_checkudata(L, 1, BLOCK);

   const char *file_path = block_get_src_file_path (b->p);
   if (file_path != NULL) {
      lua_pushstring (L, file_path);
      return 1;
   }

   return 0;
}

static int l_block_get_src_lines(lua_State * L)
{
   b_t *b = luaL_checkudata(L, 1, BLOCK);

   unsigned min, max;
   block_get_src_lines (b->p, &min, &max);
   lua_pushnumber (L, min);
   lua_pushnumber (L, max);

   return 2;
}

static int l_block_get_src_regions(lua_State * L)
{
   b_t *b = luaL_checkudata(L, 1, BLOCK);

   lua_newtable(L);

   int i = 1;
   queue_t *src_regions = block_get_src_regions (b->p);
   FOREACH_INQUEUE (src_regions, it_reg) {
      char *region = GET_DATA_T(char *, it_reg);
      lua_pushnumber(L, i++);
      lua_pushstring(L, region);
      lua_settable(L, -3);
   }
   queue_free (src_regions, NULL);

   return 1;
}

static int l_block_get_DDG(lua_State* L)
{
   b_t *b = luaL_checkudata(L, 1, BLOCK);
   graph_t *DDG = lcore_block_getddg(b->p);

   if (DDG != NULL) {
      create_graph (L, DDG);
      return 1;
   }

   return 0;
}

static int l_block_get_DDG_file_path(lua_State * L)
{
   b_t *b = luaL_checkudata(L, 1, BLOCK);
   char *dotfile_name = lcore_print_block_ddg(b->p);

   if (dotfile_name != NULL) {
      lua_pushstring(L, dotfile_name);
      return 1;
   }

   return 0;
}

static int block_gc(lua_State * L)
{
   (void) L;

   return 0;
}

static int block_tostring(lua_State * L)
{
   b_t *b = lua_touserdata(L, 1);

   lua_pushfstring(L, "Block: %d", block_get_id(b->p));

   return 1;
}

const luaL_reg block_methods[] = {
  {"get_project"          , l_block_get_project},
  {"get_asmfile"          , l_block_get_asmfile},
  {"get_function"         , l_block_get_function},
  {"get_loop"             , l_block_get_loop},
  {"get_id"               , l_block_get_id},
  {"get_imm_dominator"    , l_block_get_imm_dominator},
  {"get_imm_dominated"    , l_block_get_imm_dominated},
  {"get_imm_postdominator", l_block_get_imm_postdominator},
  {"get_imm_postdominated", l_block_get_imm_postdominated},
  {"get_predecessors"     , l_block_get_predecessors},
  {"get_successors"       , l_block_get_successors},
  {"get_first_insn"       , l_block_get_first_insn},
  {"get_last_insn"        , l_block_get_last_insn},
  {"get_defined_registers", l_block_get_defined_registers},
  {"is_back_edge_origin"  , l_block_is_back_edge_origin},
  {"is_loop_entry"        , l_block_is_loop_entry},
  {"is_loop_exit"         , l_block_is_loop_exit},
  {"is_function_exit"     , l_block_is_function_exit},
  {"is_padding"           , l_block_is_padding},
  {"is_virtual"           , l_block_is_virtual},
  {"predecessors"         , l_block_predecessors},
  {"successors"           , l_block_successors},
  {"instructions"         , l_block_instructions},
  {"get_src_file_path"    , l_block_get_src_file_path},
  {"get_src_lines"        , l_block_get_src_lines},
  {"get_src_regions"      , l_block_get_src_regions},
  {"get_DDG"              , l_block_get_DDG},
  {"get_DDG_file_path"    , l_block_get_DDG_file_path},
  {NULL, NULL}
};

const luaL_reg block_meta[] = {
  {"__gc"      , block_gc},
  {"__tostring", block_tostring},
  {NULL, NULL}
};
