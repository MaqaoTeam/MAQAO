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

#include "abstract_objects_c.h"
typedef int (*FCT_FILTER)(struct group_elem_s *, void *);

/******************************************************************/
/*                Functions dealing with Group                    */
/******************************************************************/
static int l_group_get_pattern(lua_State * L)
{
   /* Get and check arguments */
   g_t *g = luaL_checkudata(L, 1, GROUP);
   long user = luaL_checkinteger(L, 2);

   lua_pushstring(L, group_get_pattern(g->p, (void *) user));

   return 1;
}

static int l_group_get_size(lua_State * L)
{
   /* Get and check arguments */
   g_t *g = luaL_checkudata(L, 1, GROUP);
   long user = luaL_checkinteger(L, 2);

   lua_pushinteger(L, group_get_size(g->p, (void *) user));

   return 1;
}

static int l_group_get_loop(lua_State * L)
{
   g_t *g = luaL_checkudata(L, 1, GROUP);
   loop_t *loop = group_get_loop(g->p);

   if (loop != NULL) {
      create_loop(L, loop);

      return 1;
   }

   return 0;
}

static int l_group_get_function(lua_State * L)
{
   g_t *g = luaL_checkudata(L, 1, GROUP);
   loop_t *loop = group_get_loop(g->p);
   fct_t* f = loop_get_fct(loop);

   if (f != NULL) {
      create_function(L, f);

      return 1;
   }

   return 0;
}

static int l_group_get_asmfile(lua_State * L)
{
   g_t *g = luaL_checkudata(L, 1, GROUP);
   loop_t *loop = group_get_loop(g->p);
   fct_t* f = loop_get_fct(loop);
   asmfile_t* asmf = fct_get_asmfile(f);

   if (asmf != NULL) {
      create_asmfile(L, asmf);

      return 1;
   }

   return 0;
}

static int l_group_get_project(lua_State * L)
{
   g_t *g = luaL_checkudata(L, 1, GROUP);
   loop_t *loop = group_get_loop(g->p);
   fct_t* f = loop_get_fct(loop);
   asmfile_t* asmf = fct_get_asmfile(f);
   project_t* project = asmfile_get_project(asmf);

   if (project != NULL) {
      create_project(L, project, FALSE);

      return 1;
   }

   return 0;
}

static int l_group_print(lua_State * L)
{
   /* Get and check arguments */
   g_t *g = luaL_checkudata(L, 1, GROUP);
   long user = luaL_checkinteger(L, 2);
   int format = luaL_checkinteger(L, 3);

   group_print(g->p, stdout, (void *) user, format);

   return 1;
}

static int l_group_get_pattern_n(lua_State * L)
{
   /* Get and check arguments */
   g_t *g = luaL_checkudata(L, 1, GROUP);
   int n = luaL_checkinteger(L, 2);
   long user = luaL_checkinteger(L, 3);

   lua_pushinteger(L, group_get_pattern_n(g->p, n, (void*) user));

   return 1;
}

static int l_group_get_insn_n(lua_State * L)
{
   /* Get and check arguments */
   g_t *g = luaL_checkudata(L, 1, GROUP);
   int n = luaL_checkinteger(L, 2);
   long user = luaL_checkinteger(L, 3);

   insn_t* insn = group_get_insn_n(g->p, n, (void*) user);

   if (insn != NULL) {
      create_insn(L, insn);

      return 1;
   }

   return 0;
}

static int l_group_get_offset_n(lua_State * L)
{
   /* Get and check arguments */
   g_t *g = luaL_checkudata(L, 1, GROUP);
   int n = luaL_checkinteger(L, 2);
   long user = luaL_checkinteger(L, 3);

   lua_pushinteger(L, group_get_offset_n(g->p, n, (void*) user));

   return 1;
}

static int l_group_get_span(lua_State * L)
{
   g_t *g = luaL_checkudata(L, 1, GROUP);
   lua_pushinteger(L, group_get_span(g->p));
   return 1;
}

static int l_group_get_head(lua_State * L)
{
   g_t *g = luaL_checkudata(L, 1, GROUP);
   lua_pushinteger(L, group_get_head(g->p));
   return 1;
}

static int l_group_get_increment(lua_State * L)
{
   g_t *g = luaL_checkudata(L, 1, GROUP);
   lua_pushinteger(L, group_get_increment(g->p));
   return 1;
}

static int l_group_get_stride_status(lua_State * L)
{
   g_t *g = luaL_checkudata(L, 1, GROUP);
   char* status = group_get_stride_status(g->p);
   lua_pushstring(L, status);
   return 1;
}

static int l_group_get_memory_status(lua_State * L)
{
   g_t *g = luaL_checkudata(L, 1, GROUP);
   lua_pushstring(L, group_get_memory_status(g->p));
   return 1;
}

static int l_group_get_accessed_memory(lua_State * L)
{
   g_t *g = luaL_checkudata(L, 1, GROUP);
   lua_pushinteger(L, group_get_accessed_memory(g->p));
   return 1;
}

static int l_group_get_accessed_memory_nooverlap(lua_State * L)
{
   g_t *g = luaL_checkudata(L, 1, GROUP);
   lua_pushinteger(L, group_get_accessed_memory_nooverlap(g->p));
   return 1;
}

static int l_group_get_accessed_memory_overlap(lua_State * L)
{
   g_t *g = luaL_checkudata(L, 1, GROUP);
   lua_pushinteger(L, group_get_accessed_memory_overlap(g->p));
   return 1;
}

static int l_group_get_unroll_factor(lua_State * L)
{
   g_t *g = luaL_checkudata(L, 1, GROUP);
   lua_pushinteger(L, group_get_unroll_factor(g->p));
   return 1;
}

static int group_instructions_iter(lua_State *L)
{
   list_t **list = lua_touserdata(L, lua_upvalueindex(1));
   const void *filter = lua_topointer(L, lua_upvalueindex(2));
   long user = lua_tointeger(L, lua_upvalueindex(3));

   if (list != NULL) {
      // Skip filtered elements
      if (filter != NULL) {
         int (*filter_fct)(struct group_elem_s *, void *) = (FCT_FILTER)filter;
         while ((*list != NULL) && (filter_fct (list_getdata (*list), (void *) user) != 1))
         *list = list_getnext (*list);
      }
      if (*list != NULL)
      {
         insn_t *insn = ((group_elem_t *) list_getdata (*list))->insn;
         create_insn (L, insn);
         *list = list_getnext (*list);
         return 1;
      }
   }
   return 0;
}

static int l_group_instructions(lua_State * L)
{
   /* Get and check arguments */
   g_t *g = luaL_checkudata(L, 1, GROUP);
   long user = luaL_checkinteger(L, 2);

   queue_t *gdat = g->p->gdat;

   if (gdat != NULL) {
      list_t **list = lua_newuserdata(L, sizeof *list);

      *list = queue_iterator(gdat);
      lua_pushlightuserdata(L, g->p->filter_fct);
      lua_pushinteger(L, user);
   } else {
      lua_pushnil(L);
      lua_pushnil(L);
      lua_pushnil(L);
   }

   lua_pushcclosure(L, group_instructions_iter, 3);

   return 1;
}

static int group_gc(lua_State * L)
{
   (void) L;
   return 0;
}

static int group_tostring(lua_State * L)
{
   g_t *g = lua_touserdata(L, 1);
   lua_pushfstring(L, "Group: @%p", g->p);
   return 1;
}

void _group_totable(lua_State * L, group_t* group, long user)
{
   DBGMSG0("Convert a group into table for Lua API\n");
   int i = 1, j = 0;
   char* s_status, *m_status;

   lua_newtable(L);

   switch (group->s_status) {
   case SS_NA:
      s_status = SS_MSG_NA;
      break;
   case SS_OK:
      s_status = SS_MSG_OK;
      break;
   case SS_MB:
      s_status = SS_MSG_MB;
      break;
   case SS_VV:
      s_status = SS_MSG_VV;
      break;
   case SS_O:
      s_status = SS_MSG_O;
      break;
   case SS_RIP:
      s_status = SS_MSG_RIP;
      break;
   default:
      s_status = "No status available";
      break;
   }

   switch (group->m_status) {
   case MS_NA:
      m_status = MS_MSG_NA;
      break;
   case MS_OK:
      m_status = MS_MSG_OK;
      break;
   default:
      m_status = "No status available";
      break;
   }

   // set group size
   lua_pushliteral(L, "size");
   lua_pushnumber(L, group_get_size(group, (void*) user));
   lua_settable(L, -3);

   // set group pattern
   lua_pushliteral(L, "pattern");
   lua_pushstring(L, group_get_pattern(group, (void*) user));
   lua_settable(L, -3);

   // set group loop
   lua_pushliteral(L, "loop");
   create_loop(L, group_get_loop(group));
   lua_settable(L, -3);

   // set group span
   lua_pushliteral(L, "span");
   lua_pushnumber(L, group_get_span(group));
   lua_settable(L, -3);

   // set group head
   lua_pushliteral(L, "head");
   lua_pushnumber(L, group_get_head(group));
   lua_settable(L, -3);

   // set stride status
   lua_pushliteral(L, "increment_status");
   lua_pushstring(L, s_status);
   lua_settable(L, -3);

   // set increment
   lua_pushliteral(L, "increment");
   lua_pushnumber(L, group->stride);
   lua_settable(L, -3);

   // set memory status
   lua_pushliteral(L, "memory_status");
   lua_pushstring(L, m_status);
   lua_settable(L, -3);

   // set number of accessed bytes
   lua_pushliteral(L, "number_accessed_bytes");
   lua_pushnumber(L, group->memory_all);
   lua_settable(L, -3);

   // set number of no overlaped accessed bytes
   lua_pushliteral(L, "no_overlap_bytes");
   lua_pushnumber(L, group->memory_nover);
   lua_settable(L, -3);

   // set number of overlaped accessed bytes
   lua_pushliteral(L, "overlap_bytes");
   lua_pushnumber(L, group->memory_overl);
   lua_settable(L, -3);

   // set touched sets
   lua_pushliteral(L, "touched_sets");
   lua_newtable(L);
   i = 1;
   for (j = 0; j < group->nb_sets; j++) {
      lua_newtable(L);

      // start
      lua_pushliteral(L, "start");
      lua_pushnumber(L, group->touched_sets[2 * j]);
      lua_settable(L, -3);

      // stop
      lua_pushliteral(L, "stop");
      lua_pushnumber(L, group->touched_sets[2 * j + 1]);
      lua_settable(L, -3);

      lua_rawseti(L, -2, i++);
   }
   lua_settable(L, -3);

   // set group instructions
   lua_pushliteral(L, "insns");
   lua_newtable(L);
   FOREACH_INQUEUE(group->gdat, it_group) {
      lua_newtable(L);
      group_elem_t* gdat = GET_DATA_T(group_elem_t*, it_group);

      // set the instruction
      lua_pushliteral(L, "insn");
      create_insn(L, gdat->insn);
      lua_settable(L, -3);

      // set the position of the memeory operand
      // (add 1 because lua indexes begin a 1)
      lua_pushliteral(L, "memory_position");
      lua_pushnumber(L, gdat->pos_param + 1);
      lua_settable(L, -3);

      // set the type of access (LOAD / STORE)
      lua_pushliteral(L, "access");
      if (gdat->code == GRP_LOAD)
         lua_pushstring(L, "LOAD");
      else
         lua_pushstring(L, "STORE");
      lua_settable(L, -3);

      // set the offset
      lua_pushliteral(L, "offset");
      lua_pushnumber(L,
            oprnd_get_offset(gdat->insn->oprndtab[gdat->pos_param]));
      lua_settable(L, -3);

      lua_rawseti(L, -2, i++);
   }
   lua_settable(L, -3);
}

static int l_group_totable(lua_State * L)
{
   g_t *g = luaL_checkudata(L, 1, GROUP);
   long user = luaL_optint(L, 2, 0);
   group_t* group = g->p;

   _group_totable(L, group, user);

   return (1);
}

static int l_group_print_in_table(lua_State * L)
{
   g_t *g = luaL_checkudata(L, 1, GROUP);
   void* user = (void*) (long int) luaL_optint(L, 2, 0);
   group_t* group = g->p;
   int pos = 1;

   lua_newtable(L);

   // Get group size
   lua_pushinteger(L, pos++);
   lua_pushnumber(L, group_get_size(group, (void*) user));
   lua_settable(L, -3);

   //print pattern
   lua_pushinteger(L, pos++);
   lua_pushstring(L, group_get_pattern(group, (void*) user));
   lua_settable(L, -3);

   //print addresses
   FOREACH_INQUEUE(group->gdat, gdat_it2) {
      group_elem_t* gdat = GET_DATA_T(group_elem_t*, gdat_it2);
      if (group->filter_fct != NULL && group->filter_fct(gdat, user) == 1) {
         lua_pushinteger(L, pos++);
         lua_pushnumber(L, INSN_GET_ADDR(gdat->insn));
         lua_settable(L, -3);
      }
   }

   //print opcodes
   FOREACH_INQUEUE(group->gdat, gdat_it3) {
      group_elem_t* gdat = GET_DATA_T(group_elem_t*, gdat_it3);
      if (group->filter_fct != NULL && group->filter_fct(gdat, user) == 1) {
         lua_pushinteger(L, pos++);
         lua_pushstring(L, insn_get_opcode(gdat->insn));
         lua_settable(L, -3);
      }
   }

   //print offsets
   FOREACH_INQUEUE(group->gdat, gdat_it4) {
      group_elem_t* gdat = GET_DATA_T(group_elem_t*, gdat_it4);
      if (group->filter_fct != NULL && group->filter_fct(gdat, user) == 1) {
         lua_pushinteger(L, pos++);
         lua_pushnumber(L,
               oprnd_get_offset(insn_get_oprnd(gdat->insn, gdat->pos_param)));
         lua_settable(L, -3);
      }
   }

   lua_pushinteger(L, pos++);
   lua_pushnumber(L, loop_get_id(group->loop));
   lua_settable(L, -3);

   lua_pushinteger(L, pos++);
   lua_pushnumber(L, loop_get_nb_insns(group->loop));
   lua_settable(L, -3);

   lua_pushinteger(L, pos++);
   switch (group->s_status) {
   case SS_NA:
      lua_pushstring(L, SS_MSG_NA);
      break;
   case SS_OK:
      lua_pushstring(L, SS_MSG_OK);
      break;
   case SS_MB:
      lua_pushstring(L, SS_MSG_MB);
      break;
   case SS_VV:
      lua_pushstring(L, SS_MSG_VV);
      break;
   case SS_O:
      lua_pushstring(L, SS_MSG_O);
      break;
   case SS_RIP:
      lua_pushstring(L, SS_MSG_RIP);
      break;
   default:
      lua_pushstring(L, "No status available");
      break;
   }
   lua_settable(L, -3);

   lua_pushinteger(L, pos++);
   lua_pushnumber(L, group->stride);
   lua_settable(L, -3);

   lua_pushinteger(L, pos++);
   switch (group->m_status) {
   case MS_NA:
      lua_pushstring(L, MS_MSG_NA);
      break;
   case MS_OK:
      lua_pushstring(L, MS_MSG_OK);
      break;
   default:
      lua_pushstring(L, "No status available");
      break;
   }
   lua_settable(L, -3);

   lua_pushinteger(L, pos++);
   lua_pushnumber(L, group->memory_all);
   lua_settable(L, -3);

   lua_pushinteger(L, pos++);
   lua_pushnumber(L, group->memory_nover);
   lua_settable(L, -3);

   lua_pushinteger(L, pos++);
   lua_pushnumber(L, group->memory_overl);
   lua_settable(L, -3);

   lua_pushinteger(L, pos++);
   lua_pushnumber(L, group->span);
   lua_settable(L, -3);

   lua_pushinteger(L, pos++);
   lua_pushnumber(L, group->head);
   lua_settable(L, -3);

   lua_pushinteger(L, pos++);
   lua_pushnumber(L, group->unroll_factor);
   lua_settable(L, -3);

   return (1);
}

/**
 * Bind names from this file to LUA
 * For example, {"foo", "bar"} will be interpreted in the following way:
 * to use bar (defined in this file), call foo in LUA.
 */
const luaL_reg group_methods[] = {
   {"get_pattern"                , l_group_get_pattern},
   {"get_size"                   , l_group_get_size},
   {"get_loop"                   , l_group_get_loop},
   {"get_function"               , l_group_get_function},
   {"get_asmfile"                , l_group_get_asmfile},
   {"get_project"                , l_group_get_project},
   {"print"                      , l_group_print},
   {"get_pattern_n"              , l_group_get_pattern_n},
   {"get_insn_n"                 , l_group_get_insn_n},
   {"get_offset_n"               , l_group_get_offset_n},
   {"get_span"                   , l_group_get_span},
   {"get_head"                   , l_group_get_head},
   {"get_increment"              , l_group_get_increment},
   {"get_stride_status"          , l_group_get_stride_status},
   {"get_memory_status"          , l_group_get_memory_status},
   {"get_access_memory"          , l_group_get_accessed_memory},
   {"get_memory_nooverlap"       , l_group_get_accessed_memory_nooverlap},
   {"get_access_memory_overlap"  , l_group_get_accessed_memory_overlap},
   {"get_unroll_factor"          , l_group_get_unroll_factor},
   {"instructions"               , l_group_instructions},
   {"totable"                    , l_group_totable},
   {"print_in_table"             , l_group_print_in_table},
   {NULL, NULL}
};

const luaL_reg group_meta[] = {
   {"__gc"      , group_gc},
   {"__tostring", group_tostring},
   {NULL, NULL}
};
