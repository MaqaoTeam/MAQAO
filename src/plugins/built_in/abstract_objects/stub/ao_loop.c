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
#include "abstract_objects_c.h"

/******************************************************************/
/*                Functions dealing with Loop                     */
/******************************************************************/

static int l_loop_get_project(lua_State * L)
{
   l_t *l = luaL_checkudata(L, 1, LOOP);
   project_t *project = loop_get_project(l->p);

   if (project != NULL) {
      create_project(L, project, FALSE);

      return 1;
   }

   return 0;
}

static int l_loop_get_asmfile(lua_State * L)
{
   l_t *l = luaL_checkudata(L, 1, LOOP);
   asmfile_t *asmfile = loop_get_asmfile(l->p);

   if (asmfile != NULL) {
      create_asmfile(L, asmfile);

      return 1;
   }

   return 0;
}

static int l_loop_get_function(lua_State * L)
{
   l_t *l = luaL_checkudata(L, 1, LOOP);
   fct_t *function = loop_get_fct(l->p);

   if (function != NULL) {
      create_function(L, function);

      return 1;
   }

   return 0;
}

static int l_loop_get_id(lua_State * L)
{
   l_t *l = luaL_checkudata(L, 1, LOOP);

   lua_pushinteger(L, loop_get_id(l->p));

   return 1;
}

static int l_loop_get_parent(lua_State * L)
{
   l_t *l = luaL_checkudata(L, 1, LOOP);
   tree_t *parent = loop_get_parent_node(l->p);

   if (parent != NULL) {
      create_loop(L, tree_getdata(parent));

      return 1;
   }

   return 0;
}

static int l_loop_get_children(lua_State * L)
{
   l_t *l = luaL_checkudata(L, 1, LOOP);
   tree_t *children = loop_get_children_node(l->p);

   if (children != NULL) {
      tree_t *iter;

      lua_newtable(L);

      for (iter = children; iter != NULL; iter = iter->next) {
         loop_t *loop = tree_getdata(iter);

         create_loop(L, loop);
         lua_rawseti(L, -2, loop_get_id(loop));
      }

      return 1;
   }

   return 0;
}

/**
 this function is internally used by l_loop_children
 */
static int loop_children_iter(lua_State * L)
{
   tree_t **tree = lua_touserdata(L, lua_upvalueindex(1));

   if ((tree != NULL) && (*tree != NULL)) {
      create_loop(L, tree_getdata(*tree));
      *tree = (*tree)->next;

      return 1;
   }

   return 0;
}

static int l_loop_children(lua_State * L)
{
   l_t *l = luaL_checkudata(L, 1, LOOP);
   tree_t *children = loop_get_children_node(l->p);

   if (children != NULL) {
      tree_t **tree = lua_newuserdata(L, sizeof *tree);

      *tree = children;
   } else
      lua_pushnil(L);

   lua_pushcclosure(L, loop_children_iter, 1);

   return 1;
}

static int l_loop_get_first_entry(lua_State * L)
{
   l_t *l = luaL_checkudata(L, 1, LOOP);

   create_block(L, list_getdata(loop_get_entries(l->p)));

   return 1;
}

static int l_loop_get_entries(lua_State * L)
{
   l_t *l = luaL_checkudata(L, 1, LOOP);

   lua_newtable(L);

   FOREACH_INLIST(loop_get_entries(l->p), iter) {
      block_t *block = GET_DATA_T(block_t*, iter);

      create_block(L, block);
      lua_rawseti(L, -2, block_get_id(block));
   }

   return 1;
}

static int l_loop_get_exits(lua_State * L)
{
   l_t *l = luaL_checkudata(L, 1, LOOP);

   lua_newtable(L);

   FOREACH_INLIST(loop_get_exits(l->p), iter) {
      block_t *block = GET_DATA_T(block_t*, iter);

      create_block(L, block);
      lua_rawseti(L, -2, block_get_id(block));
   }

   return 1;
}

static int l_loop_get_groups(lua_State * L)
{
   l_t *l = luaL_checkudata(L, 1, LOOP);
   int i = 1;

   lua_newtable(L);

   FOREACH_INLIST(loop_get_groups(l->p), iter) {
      create_group(L, GET_DATA_T(group_t *, iter));
      lua_rawseti(L, -2, i++);
   }

   return 1;
}

static int l_loop_blocks(lua_State * L)
{
   l_t *l = luaL_checkudata(L, 1, LOOP);
   queue_t *blocks = loop_get_blocks(l->p);

   if (blocks != NULL) {
      list_t **list = lua_newuserdata(L, sizeof *list);

      *list = queue_iterator(blocks);
   } else {
      /* This case should never occur, even with an empty loop */

      lua_pushnil(L);
   }

   lua_pushcclosure(L, blocks_iter, 1);

   return 1;
}

static int l_loop_get_nb_paths(lua_State * L)
{
   l_t *l = luaL_checkudata(L, 1, LOOP);

   lua_pushinteger(L, lcore_loop_getnpaths(l->p, FALSE));

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

static int l_loop_paths(lua_State * L)
{
   l_t *l = luaL_checkudata(L, 1, LOOP);
   queue_t *paths = loop_get_paths(l->p);

   if (paths == NULL) {
      lcore_loop_computepaths(l->p);
      paths = loop_get_paths(l->p);
   }

   if (paths != NULL) {
      list_t **list = lua_newuserdata(L, sizeof *list);
      *list = queue_iterator(paths);
   } else
      lua_pushnil(L);

   lua_pushcclosure(L, paths_iter, 1);

   return 1;
}

static int l_loop_are_paths_computed(lua_State *L)
{
   l_t *l = luaL_checkudata(L, 1, LOOP);
   queue_t *paths = loop_get_paths(l->p);

   lua_pushboolean(L, (paths != NULL));

   return 1;
}

static int l_loop_free_paths(lua_State *L)
{
   l_t *l = luaL_checkudata(L, 1, LOOP);

   lcore_loop_freepaths(l->p);

   return 0;
}

static int l_loop_get_first_path(lua_State * L)
{
   l_t *l = luaL_checkudata(L, 1, LOOP);
   queue_t *paths = loop_get_paths(l->p);

   if (paths == NULL) {
      lcore_loop_computepaths(l->p);
      paths = loop_get_paths(l->p);
   }

   if (paths != NULL) {
      int i = 1;
      array_t *first_path = queue_peek_head(paths);

      lua_newtable(L);

      FOREACH_INARRAY(first_path, iter) {
         create_block(L, *iter);
         lua_rawseti(L, -2, i++);
      }

      return 1;
   }

   return 0;
}

static int l_loop_get_nentries(lua_State * L)
{
   l_t *l = luaL_checkudata(L, 1, LOOP);

   lua_pushinteger(L, LOOP_NB_ENTRIES(l->p));

   return 1;
}

static int l_loop_get_nexits(lua_State * L)
{
   l_t *l = luaL_checkudata(L, 1, LOOP);

   lua_pushinteger(L, LOOP_NB_EXITS(l->p));

   return 1;
}

static int l_loop_get_nblocks(lua_State * L)
{
   l_t *l = luaL_checkudata(L, 1, LOOP);

   lua_pushinteger(L, loop_get_nb_blocks_novirtual(l->p));

   return 1;
}

static int l_loop_get_ninsns(lua_State * L)
{
   l_t *l = luaL_checkudata(L, 1, LOOP);

   lua_pushinteger(L, loop_get_nb_insns(l->p));

   return 1;
}

static int l_loop_is_innermost(lua_State *L)
{
   l_t *l = luaL_checkudata(L, 1, LOOP);

   if (loop_is_dominant(l->p) == FALSE)
      lua_pushboolean(L, 1);
   else
      lua_pushboolean(L, 0);

   return 1;
}

static int loop_is_dominant_ancestor(loop_t *loop)
{
   if (loop_get_parent_node(loop) == NULL)
      return TRUE;
   return FALSE;
}

static int l_loop_is_outermost(lua_State *L)
{
   l_t *l = luaL_checkudata(L, 1, LOOP);

   if (loop_is_dominant_ancestor(l->p) == TRUE)
      lua_pushboolean(L, 1);
   else
      lua_pushboolean(L, 0);

   return 1;
}

/**
 this function is internally used by loop_groups
 */
static int loop_groups_iter(lua_State * L)
{
   list_t **list = lua_touserdata(L, lua_upvalueindex(1));

   if ((list != NULL) && (*list != NULL)) {
      create_group(L, list_getdata(*list));
      *list = list_getnext(*list);

      return 1;
   }

   return 0;
}

static int l_loop_groups(lua_State * L)
{
   l_t *l = luaL_checkudata(L, 1, LOOP);
   list_t *groups = loop_get_groups(l->p);

   if (groups != NULL) {
      list_t **list = lua_newuserdata(L, sizeof *list);

      *list = groups;
   } else
      lua_pushnil(L);

   lua_pushcclosure(L, loop_groups_iter, 1);

   return 1;
}

static int l_loop_get_groups_totable(lua_State* L)
{
   l_t *l = luaL_checkudata(L, 1, LOOP);
   list_t *groups = loop_get_groups(l->p);
   int i = 1;

   lua_newtable(L);
   FOREACH_INLIST(groups, it_groups) {
      group_t* group = GET_DATA_T(group_t*, it_groups);
      _group_totable(L, group, 0);
      lua_rawseti(L, -2, i++);
   }
   return 1;
}

static int l_loop_has_groups(lua_State* L)
{
   l_t *l = luaL_checkudata(L, 1, LOOP);
   list_t *groups = loop_get_groups(l->p);

   if (groups != NULL)
      lua_pushboolean(L, 1);
   else
      lua_pushboolean(L, 0);

   return 1;
}

static int l_loop_get_DDG(lua_State* L)
{
   l_t *l = luaL_checkudata(L, 1, LOOP);
   graph_t *DDG = lcore_loop_getddg(l->p);

   if (DDG != NULL) {
      create_graph (L, DDG);
      return 1;
   }

   return 0;
}

static int l_loop_get_DDG_file_path(lua_State * L)
{
   l_t *l = luaL_checkudata(L, 1, LOOP);
   char *dotfile_name = lcore_print_loop_ddg(l->p);

   if (dotfile_name != NULL) {
      lua_pushstring(L, dotfile_name);
      lcore_print_loop_ddg_paths(l->p);

      return 1;
   }

   return 0;
}

static int l_loop_get_depth(lua_State * L)
{
   l_t *l = luaL_checkudata(L, 1, LOOP);

   int depth = loop_get_depth(l->p);
   if (depth != SIGNED_ERROR) {
      lua_pushinteger(L, depth);
      return 1;
   }

   return 0;
}

static int l_loop_get_src_file_path(lua_State * L)
{
   l_t *l = luaL_checkudata(L, 1, LOOP);

   const char *file_path = loop_get_src_file_path (l->p);
   if (file_path != NULL) {
      lua_pushstring (L, file_path);
      return 1;
   }

   return 0;
}

static int l_loop_get_src_lines(lua_State * L)
{
   l_t *l = luaL_checkudata(L, 1, LOOP);

   unsigned min, max;
   loop_get_src_lines (l->p, &min, &max);
   lua_pushnumber (L, min);
   lua_pushnumber (L, max);

   return 2;
}

static int l_loop_get_src_regions(lua_State * L)
{
   l_t *l = luaL_checkudata(L, 1, LOOP);

   lua_newtable(L);

   int i = 1;
   queue_t *src_regions = loop_get_src_regions (l->p);
   FOREACH_INQUEUE (src_regions, it_reg) {
      char *region = GET_DATA_T(char *, it_reg);
      lua_pushnumber(L, i++);
      lua_pushstring(L, region);
      lua_settable(L, -3);
   }
   queue_free (src_regions, NULL);

   return 1;
}

//static void setfield (lua_State * L, const char *index, uint64_t value) {
//   lua_pushstring(L, index);
//   lua_pushnumber(L, (double)value);
//   lua_settable(L, -3);
//}

static void setfield_str(lua_State * L, const char *index, const char * value)
{
   lua_pushstring(L, index);
   lua_pushstring(L, value);
   lua_settable(L, -3);
}

static int l_loop_get_pattern(lua_State * L)
{
   l_t *l = luaL_checkudata(L, 1, LOOP);
   loop_pattern pat = maqao_loop_pattern_detect(l->p);
   if (pat == NULL) {
      lua_pushnil(L);
      return 1;
   }

   switch (pat->type) {
   case Repeat:
      lua_newtable(L);
      setfield_str(L, "type", "repeat");
      break;
   case MultiRepeat:
      lua_newtable(L);
      setfield_str(L, "type", "multirepeat");
      break;
   case While:
      lua_newtable(L);
      setfield_str(L, "type", "while");
      break;
   default:
      lua_pushnil(L);
   }

   free(pat);
   return 1;
}

static int l_loop_get_polytopes(lua_State * L)
{
   l_t *l = luaL_checkudata(L, 1, LOOP);
   arch_t* arch = l->p->function->asmfile->arch;

   lua_newtable(L);
   queue_t** polytopes = lcore_get_polytopes_from_fct(l->p->function);
   if (polytopes == NULL) {
      return (1);
   }
   if (polytopes[l->p->id] != NULL) {
      FOREACH_INQUEUE(polytopes[l->p->id], it_polytope)
      {
         polytope_t* polytope = GET_DATA_T(polytope_t*, it_polytope);
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
               addr = ((insn_t*) ((block_t*) queue_peek_head(
                     l->p->function->entries))->begin_sequence->data)->address;

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
   return (1);
}

static int loop_gc(lua_State * L)
{
   (void) L;

   return 0;
}

static int loop_tostring(lua_State * L)
{
   l_t *l = lua_touserdata(L, 1);

   lua_pushfstring(L, "Loop: %d", loop_get_id(l->p));

   return 1;
}

/** Try to find an instruction in loop to put instrumentation code (inc [mem]) */
insn_t * find_loop_iter_instru_insn(loop_t *l)
{

   block_t * b;
   insn_t * i = NULL;
   loop_pattern pat = maqao_loop_pattern_detect(l);
   if (pat == NULL)
      return NULL;

   switch (pat->type) {
   case While:
      b = pat->pattern_while.entry_exit;
      i = block_find_flag_overriding_insn_inc(b);
      break;
   case Repeat:
      b = pat->pattern_repeat.exit;
      i = block_find_flag_overriding_insn_inc(b);
      break;
   case MultiRepeat:
      b = pat->pattern_multirepeat.entry;
      i = block_find_flag_overriding_insn_inc(b);
      break;
   }

   free(pat);
   return i;
}

static int l_loop_get_iter_insn(lua_State * L)
{
   l_t *l = lua_touserdata(L, 1);

   insn_t *i = find_loop_iter_instru_insn(l->p);

   if (i != NULL) {
      create_insn(L, i);

      return 1;
   }

   return 0;
}

/**
 * Bind names from this file to LUA
 * For example, {"foo", "bar"} will be interpreted in the following way:
 * to use bar (defined in this file), call foo in LUA.
 */
const luaL_reg loop_methods[] = {
   {"get_project"          , l_loop_get_project},
   {"get_asmfile"          , l_loop_get_asmfile},
   {"get_function"         , l_loop_get_function},
   {"get_id"               , l_loop_get_id},
   {"get_parent"           , l_loop_get_parent},
   {"get_children"         , l_loop_get_children},
   {"get_first_entry"      , l_loop_get_first_entry},
   {"get_nentries"         , l_loop_get_nentries},
   {"get_nexits"           , l_loop_get_nexits},
   {"get_nblocks"          , l_loop_get_nblocks},
   {"get_ninsns"           , l_loop_get_ninsns},
   {"has_groups"           , l_loop_has_groups},
   {"get_groups"           , l_loop_get_groups},
   {"get_groups_totable"   , l_loop_get_groups_totable},
   {"get_entries"          , l_loop_get_entries},
   {"get_exits"            , l_loop_get_exits},
   {"is_innermost"         , l_loop_is_innermost},
   {"is_outermost"         , l_loop_is_outermost},
   {"get_first_path"       , l_loop_get_first_path},
   {"blocks"               , l_loop_blocks},
   {"children"             , l_loop_children},
   {"groups"               , l_loop_groups},
   {"get_nb_paths"         , l_loop_get_nb_paths},
   {"paths"                , l_loop_paths},
   {"are_paths_computed"   , l_loop_are_paths_computed},
   {"free_paths"           , l_loop_free_paths},
   {"get_DDG"              , l_loop_get_DDG},
   {"get_DDG_file_path"    , l_loop_get_DDG_file_path},
   {"get_polytopes"        , l_loop_get_polytopes},
   {"get_depth"            , l_loop_get_depth},
   {"get_pattern"          , l_loop_get_pattern},
   {"get_iter_insn"        , l_loop_get_iter_insn},
   {"get_src_file_path"    , l_loop_get_src_file_path},
   {"get_src_lines"        , l_loop_get_src_lines},
   {"get_src_regions"      , l_loop_get_src_regions},
   {NULL, NULL}
};

const luaL_reg loop_meta[] = {
   {"__gc"      , loop_gc},
   {"__tostring", loop_tostring},
   {NULL, NULL}
};
