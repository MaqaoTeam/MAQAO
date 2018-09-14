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

#include "libmmaqao.h"
#include "libmcore.h"
#include "uarch_detector.h"

#include "abstract_objects_c.h"

/******************************************************************/
/*                Functions dealing with project                  */
/******************************************************************/

static int l_project_new(lua_State * L)
{
   char *project_name = (char *) luaL_checkstring(L, 1);

   create_project(L, project_new(project_name), TRUE);

   return 1;
}

static int l_project_duplicate(lua_State * L)
{
   p_t* p = luaL_checkudata(L, 1, PROJECT);
   create_project(L, project_dup(p->p), TRUE);
   return 1;
}

static int l_project_set_uarch_name(lua_State * L)
{
   p_t *p = luaL_checkudata(L, 1, PROJECT);
   char* uarch_name = (char*) luaL_checkstring(L, 2);

   project_set_uarch_name(p->p, uarch_name);

   return 0;
}

static int l_project_set_proc(lua_State * L)
{
   p_t *p = luaL_checkudata(L, 1, PROJECT);
   l_proc_t* lproc = luaL_checkudata(L, 2, PROC);

   project_set_proc(p->p, lproc->p);

   return 0;
}

static int l_project_set_proc_name(lua_State * L)
{
   p_t *p = luaL_checkudata(L, 1, PROJECT);
   char* proc_name = (char*) luaL_checkstring(L, 2);

   project_set_proc_name(p->p, proc_name);

   return 0;
}

static int l_project_set_compiler_code(lua_State * L)
{
   p_t *p = luaL_checkudata(L, 1, PROJECT);
   char comp_code = luaL_checkinteger(L, 2);

   project_set_compiler_code(p->p, comp_code);

   return 0;
}

static int l_project_set_language_code(lua_State * L)
{
   p_t *p = luaL_checkudata(L, 1, PROJECT);
   char lang_code = luaL_checkinteger(L, 2);

   project_set_language_code(p->p, lang_code);

   return 0;
}

static int l_project_set_ccmode(lua_State * L)
{
   p_t *p = luaL_checkudata(L, 1, PROJECT);
   long ccmode = luaL_checkinteger(L, 2);
   if (p != NULL) {
      project_set_ccmode(p->p, (char) ccmode);
   }
   return 0;
}

static int l_project_load(lua_State * L)
{
   /* Get and check arguments */
   p_t *p = luaL_checkudata(L, 1, PROJECT);
   char *asmfile_name = (char *) luaL_checkstring(L, 2);
   char *uarch_name = (char*) luaL_optstring(L, 3, NULL);

   asmfile_t *asmfile = project_load_file(p->p, asmfile_name, uarch_name);
   //asmfile = project_load_file (p->p, asmfile_name, &(p->p->uarch));   // FROM DECAN

   if (asmfile != NULL)
      create_asmfile(L, asmfile);
   else
      lua_pushnil(L);

   return 1;
}

static int l_project_load_asm(lua_State * L)
{
   /* Get and check arguments */
   p_t *p = luaL_checkudata(L, 1, PROJECT);
   char *asmfile_name = (char *) luaL_checkstring(L, 2);
   char *arch_name = (char *) luaL_checkstring(L, 3);
   char *uarch_name = (char*) luaL_optstring(L, 4, NULL);

   asmfile_t *asmfile = project_load_asm_file(p->p, asmfile_name, arch_name,
         uarch_name);
   //asmfile = project_load_file (p->p, asmfile_name, &(p->p->uarch));   // FROM DECAN

   if (asmfile != NULL)
      create_asmfile(L, asmfile);
   else
      lua_pushnil(L);

   return 1;
}

static int l_project_load_txtfile(lua_State * L)
{
   /* Get and check arguments */
   p_t *p = luaL_checkudata(L, 1, PROJECT);
   char *asmfile_name = (char *) luaL_checkstring(L, 2);
   char *arch_name = (char *) luaL_checkstring(L, 3);
   char *uarch_name = (char*) luaL_optstring(L, 4, NULL);

   asmfile_t *asmfile = project_load_txtfile(p->p, asmfile_name, NULL,
         arch_name, uarch_name, NULL);

   if (asmfile != NULL)
      create_asmfile(L, asmfile);
   else
      lua_pushnil(L);

   return 1;
}

static int l_project_parse(lua_State * L)
{
   /* Get and check arguments */
   p_t *p = luaL_checkudata(L, 1, PROJECT);
   char *asmfile_name = (char *) luaL_checkstring(L, 2);
   p->uarch_name = (char*) luaL_checkstring(L, 3);

   asmfile_t *asmfile = project_parse_file(p->p, asmfile_name, p->uarch_name);

   if (asmfile != NULL) {
      create_asmfile(L, asmfile);

      return 1;
   }

   return 0;
}

static int l_project_remove_file(lua_State * L)
{
   /* Get and check arguments */
   p_t *p = luaL_checkudata(L, 1, PROJECT);
   a_t *a = luaL_checkudata(L, 2, ASMFILE);

   lua_pushinteger(L, project_remove_file(p->p, a->p));

   return 1;
}

static int l_project_free(lua_State * L)
{
   /* Get and check arguments */
   p_t *p = luaL_checkudata(L, 1, PROJECT);

   project_free(p->p);
   p->must_be_freed = FALSE;

   return 0;
}

static int l_project_get_name(lua_State * L)
{
   p_t *p = luaL_checkudata(L, 1, PROJECT);

   lua_pushstring(L, project_get_name(p->p));

   return 1;
}

static int l_project_get_nb_asmfiles(lua_State * L)
{
   p_t *p = luaL_checkudata(L, 1, PROJECT);

   lua_pushinteger(L, project_get_nb_asmfiles(p->p));

   return 1;
}

static int l_project_get_nfunctions(lua_State * L)
{
   p_t *p = luaL_checkudata(L, 1, PROJECT);

   lua_pushinteger(L, project_get_nb_fcts(p->p));

   return 1;
}

static int l_project_get_nb_loops(lua_State * L)
{
   p_t *p = luaL_checkudata(L, 1, PROJECT);

   lua_pushinteger(L, project_get_nb_loops(p->p));

   return 1;
}

static int l_project_get_nb_blocks(lua_State * L)
{
   p_t *p = luaL_checkudata(L, 1, PROJECT);
   lua_pushinteger(L, project_get_nb_blocks_novirtual(p->p));

   return 1;
}

static int l_project_get_nb_insns(lua_State * L)
{
   p_t *p = luaL_checkudata(L, 1, PROJECT);

   lua_pushinteger(L, project_get_nb_insns(p->p));

   return 1;
}

static int l_project_get_CG_file_path(lua_State * L)
{
   p_t *p = luaL_checkudata(L, 1, PROJECT);

   char *cg = lcore_print_cg(p->p);

   if (cg != NULL) {
      lua_pushstring(L, cg);

      return 1;
   }

   return 0;
}

/**
 * This function is internally used by l_project_asmfiles()
 * \param None
 * \return Nothing
 */
static int asmfiles_iter(lua_State * L)
{
   list_t **list = lua_touserdata(L, lua_upvalueindex(1));

   if ((list != NULL) && (*list != NULL)) {
      create_asmfile(L, list_getdata(*list));
      *list = list_getnext(*list);

      return 1;
   }

   return 0;
}

static int l_project_asmfiles(lua_State * L)
{
   p_t *p = luaL_checkudata(L, 1, PROJECT);
   queue_t *asmfiles = project_get_asmfiles(p->p);

   if (asmfiles != NULL) {
      list_t **list = lua_newuserdata(L, sizeof *list);

      *list = queue_iterator(asmfiles);
   } else {
      /* This case should never occur, even with an empty project */
      STDMSG("No valid asmfile\n");

      lua_pushnil(L);
   }

   lua_pushcclosure(L, asmfiles_iter, 1);

   return 1;
}

static int l_project_get_first_asmfile(lua_State * L)
{
   p_t *p = luaL_checkudata(L, 1, PROJECT);
   queue_t *asmfiles = project_get_asmfiles(p->p);

   if (asmfiles != NULL) {
      create_asmfile(L, list_getdata(queue_iterator(asmfiles)));

      return 1;
   }

   STDMSG("No valid asmfile\n");

   return 0;
}

static int l_project_get_uarch_id(lua_State * L)
{
   p_t *p = luaL_checkudata(L, 1, PROJECT);

   if (p != NULL) {
      lua_pushinteger(L, uarch_get_id(proc_get_uarch(project_get_proc(p->p))));
      return 1;
   }
   return 0;
}

static int l_project_get_uarch_name(lua_State * L)
{
   p_t *p = luaL_checkudata(L, 1, PROJECT);

   if (p != NULL) {
      lua_pushstring(L, project_get_uarch_name(p->p));
      return 1;
   }
   return 0;
}

static int l_project_get_arch(lua_State * L)
{
   p_t *p = luaL_checkudata(L, 1, PROJECT);
   arch_t* arch = project_get_arch(p->p);
   if (arch != NULL) {
      create_arch(L, arch);
      return 1;
   }
   return 0;
}

static int l_project_set_exits(lua_State * L)
{
   p_t *p = luaL_checkudata(L, 1, PROJECT);
   int size = 0;
   char** exits = NULL;
   int i = 0;

   // Get table size
   lua_pushnil(L);
   while (lua_next(L, -2) != 0) {
      size++;
      lua_pop(L, 1);
   }
   if (size == 0)
      return (0);

   // Get table content and generate the array
   exits = lc_malloc((size + 1) * sizeof(char*));
   i = 0;
   lua_pushnil(L);
   while (lua_next(L, -2) != 0) {
      exits[i++] = lc_strdup(luaL_checkstring(L, -1));
      lua_pop(L, 1);
   }
   exits[i] = NULL;
   project_set_exit_fcts(p->p, exits);
   return (0);
}

static int l_project_add_exits(lua_State * L)
{
   p_t *p = luaL_checkudata(L, 1, PROJECT);
   int size = 0;
   char** exits = NULL;
   int i = 0;

   // Get table size
   lua_pushnil(L);
   while (lua_next(L, -2) != 0) {
      size++;
      lua_pop(L, 1);
   }
   if (size == 0)
      return (0);

   // Get table content and generate the array
   exits = lc_malloc((size + 1) * sizeof(char*));
   i = 0;
   lua_pushnil(L);
   while (lua_next(L, -2) != 0) {
      exits[i++] = lc_strdup(luaL_checkstring(L, -1));
      lua_pop(L, 1);
   }
   exits[i] = NULL;
   project_add_exit_fcts(p->p, exits);
   return (0);
}

static int l_project_rem_exit(lua_State * L)
{
   p_t *p = luaL_checkudata(L, 1, PROJECT);
   char *exit = (char *) luaL_checkstring(L, 2);
   project_rem_exit_fct(p->p, exit);
   return 0;
}

static int l_project_set_option(lua_State * L)
{
   p_t *p = luaL_checkudata(L, 1, PROJECT);
   int module_id = luaL_checkinteger(L, 2);
   int param_id = luaL_checkinteger(L, 3);
   void* value = NULL;

   if (lua_isboolean(L, 4))
      value = (void*) (size_t) lua_toboolean(L, 4);
   else if (lua_isnoneornil(L, 4))
      value = NULL;
   else if (lua_isnumber(L, 4))
      value = (void*) luaL_checkinteger(L, 4);
   else if (lua_isstring(L, 4))
      value = (void*) luaL_checkstring(L, 4);
   project_add_parameter(p->p, module_id, param_id, value);

   return (0);
}

static int l_project_get_boolean_option(lua_State * L)
{
   p_t *p = luaL_checkudata(L, 1, PROJECT);
   int module_id = luaL_checkinteger(L, 2);
   int param_id = luaL_checkinteger(L, 3);
   void* value = project_get_parameter(p->p, module_id, param_id);

   if (value == NULL)
      lua_pushboolean(L, FALSE);
   else
      lua_pushboolean(L, (long int) value);
   return (1);
}

static int l_project_get_int_option(lua_State * L)
{
   p_t *p = luaL_checkudata(L, 1, PROJECT);
   int module_id = luaL_checkinteger(L, 2);
   int param_id = luaL_checkinteger(L, 3);
   void* value = project_get_parameter(p->p, module_id, param_id);

   if (value == NULL)
      lua_pushinteger(L, 0);
   else
      lua_pushinteger(L, (long int) value);
   return (1);
}

static int l_project_get_string_option(lua_State * L)
{
   p_t *p = luaL_checkudata(L, 1, PROJECT);
   int module_id = luaL_checkinteger(L, 2);
   int param_id = luaL_checkinteger(L, 3);
   void* value = project_get_parameter(p->p, module_id, param_id);

   if (value == NULL)
      lua_pushnil(L);
   else
      lua_pushstring(L, (char*) value);
   return (1);
}

static int l_project_init_proc(lua_State * L)
{
   p_t* p = luaL_checkudata(L, 1, PROJECT);
   char *file_name = (char *) luaL_optstring(L, 2, NULL);
   char *arch_name = (char *) luaL_optstring(L, 3, NULL);
   char *uarch_name = (char *) luaL_optstring(L, 4, NULL);
   char *proc_name = (char *) luaL_optstring(L, 5, NULL);

   int status = project_init_proc(p->p, file_name, arch_name, uarch_name, proc_name);

   lua_pushinteger(L, status);

   return 1;
}

static int project_gc(lua_State * L)
{
   p_t *p = luaL_checkudata(L, 1, PROJECT);

   if (p->must_be_freed == TRUE)
      project_free(p->p);

   return 0;
}

static int project_tostring(lua_State * L)
{
   p_t *p = luaL_checkudata(L, 1, PROJECT);

   lua_pushfstring(L, "Project: %s", p->p->file);

   return 1;
}

static int l_is_iset_supported_by_host(lua_State * L)
{
   int iset = luaL_checkinteger(L, 1);

   lua_pushboolean(L,
         (iset > 0) ? utils_is_iset_supported_by_host(iset) : FALSE);

   return 1;
}

static int l_as_flag_to_iset(lua_State * L)
{
   const char* flag_name = luaL_checkstring(L, 1);

   lua_pushinteger(L, utils_as_flag_to_iset(flag_name));

   return 1;
}

static int l_get_cpu_frequency(lua_State * L)
{
   lua_pushstring(L, utils_get_cpu_frequency());
   return 1;
}

static void push_str(lua_State *L, const char *key, const char *str)
{
   lua_pushstring(L, key);
   lua_pushstring(L, str);
   lua_settable(L, -3);
}

static void push_num(lua_State *L, const char *key, int num)
{
   lua_pushstring(L, key);
   lua_pushinteger(L, num);
   lua_settable(L, -3);
}

static void push_bool(lua_State *L, const char *key, int num)
{
   lua_pushstring(L, key);
   if (num == -1)
      lua_pushnil(L);
   else
      lua_pushboolean(L, num);
   lua_settable(L, -3);
}

static int l_get_cache_info(lua_State * L)
{
   udc_cache_entries_t entries;
   int ret = utils_set_cache_info(&entries);
   if (ret == -1) {
      lua_pushnil(L);
      return 1;
   }

   lua_newtable(L);

   int i;
   for (i = 0; i < entries.index_entry_nb; i++) {
      udc_index_entry_t *cur_entry = &(entries.index[i]);
      lua_newtable(L);

      // Allocation policy
      switch (cur_entry->allocation_policy) {
      case UDC_WR_ALLOC:
         push_str(L, "allocation_policy", "WriteAllocate");
         break;
      case UDC_RD_ALLOC:
         push_str(L, "allocation_policy", "ReadAllocate");
         break;
      case UDC_RW_ALLOC:
         push_str(L, "allocation_policy", "ReadWriteAllocate");
         break;
      default:
         ;
      }

      // Type
      switch (cur_entry->type) {
      case UDC_DATA:
         push_str(L, "type", "Data");
         break;
      case UDC_INSTRUCTION:
         push_str(L, "type", "Instruction");
         break;
      case UDC_UNIFIED:
         push_str(L, "type", "Unified");
         break;
      default:
         ;
      }

      // Write policy
      switch (cur_entry->write_policy) {
      case UDC_WRITE_THROUGH:
         push_str(L, "write_policy", "WriteThrough");
         break;
      case UDC_WRITE_BACK:
         push_str(L, "write_policy", "WriteBack");
         break;
      default:
         ;
      }

      push_num(L, "coherency_line_size", cur_entry->coherency_line_size);
      push_num(L, "level", cur_entry->level);
      push_num(L, "number_of_sets", cur_entry->number_of_sets);
      push_num(L, "physical_line_partition",
            cur_entry->physical_line_partition);
      push_str(L, "shared_cpu_list", cur_entry->shared_cpu_list);
      push_bool(L, "is_core_private", cur_entry->is_core_private);
      push_str(L, "shared_cpu_map", cur_entry->shared_cpu_map);
      push_num(L, "size", cur_entry->size);
      push_num(L, "ways_of_associativity", cur_entry->ways_of_associativity);

      lua_rawseti(L, -2, i + 1);
   }

   return 1;
}

static int l_get_data_cache_size(lua_State * L)
{
   udc_cache_entries_t entries;
   int ret = utils_set_cache_info(&entries);
   if (ret == -1) {
      lua_pushnil(L);
      return 1;
   }

   uint8_t level = luaL_checkinteger(L, 2);
   lua_pushinteger(L, utils_get_data_cache_size(&entries, level));

   return 1;
}

static int l_get_data_cache_nb_levels(lua_State * L)
{
   udc_cache_entries_t entries;
   int ret = utils_set_cache_info(&entries);
   if (ret == -1) {
      lua_pushnil(L);
      return 1;
   }

   lua_pushinteger(L, utils_get_data_cache_nb_levels(&entries));

   return 1;
}

static int l_get_nb_sockets(lua_State * L)
{
   lua_pushinteger(L, utils_get_nb_sockets());
   return 1;
}

/**
 * Bind names from this file to LUA
 * For example, {"foo", "bar"} will be interpreted in the following way:
 * to use bar (defined in this file), call foo in LUA.
 */
const luaL_reg project_methods[] = {
   {"new"                        , l_project_new},
   {"duplicate"                  , l_project_duplicate},
   {"load"                       , l_project_load},
   {"load_asm"                   , l_project_load_asm},
   {"load_txtfile"               , l_project_load_txtfile},
   {"parse"                      , l_project_parse},
   {"free"                       , l_project_free},
   {"remove_file"                , l_project_remove_file},
   {"get_name"                   , l_project_get_name},
   {"get_nasmfiles"              , l_project_get_nb_asmfiles},
   {"get_nfunctions"             , l_project_get_nfunctions},
   {"get_nloops"                 , l_project_get_nb_loops},
   {"get_nblocks"                , l_project_get_nb_blocks},
   {"get_ninsns"                 , l_project_get_nb_insns},
   {"get_first_asmfile"          , l_project_get_first_asmfile},
   {"get_CG_file_path"           , l_project_get_CG_file_path},
   {"get_uarch_id"               , l_project_get_uarch_id},
   {"get_uarch_name"             , l_project_get_uarch_name},
   {"get_arch"                   , l_project_get_arch},
   {"set_proc"                   , l_project_set_proc},
   {"set_proc_name"              , l_project_set_proc_name},
   {"set_uarch_name"             , l_project_set_uarch_name},
   {"set_compiler_code"          , l_project_set_compiler_code},
   {"set_language_code"          , l_project_set_language_code},
   {"set_exits"                  , l_project_set_exits},
   {"add_exits"                  , l_project_add_exits},
   {"rem_exit"                   , l_project_rem_exit},
   {"set_ccmode"                 , l_project_set_ccmode},
   {"asmfiles"                   , l_project_asmfiles},
   {"set_option"                 , l_project_set_option},
   {"get_boolean_option"         , l_project_get_boolean_option},
   {"get_int_option"             , l_project_get_int_option},
   {"get_string_option"          , l_project_get_string_option},
   {"init_proc"                  , l_project_init_proc},
   {"is_iset_supported_by_host"  , l_is_iset_supported_by_host},
   {"as_flag_to_iset"            , l_as_flag_to_iset},
   {"get_cpu_frequency"          , l_get_cpu_frequency},
   {"get_cache_info"             , l_get_cache_info},
   {"get_data_cache_size"        , l_get_data_cache_size},
   {"get_data_cache_nb_levels"   , l_get_data_cache_nb_levels},
   {"get_nb_sockets"             , l_get_nb_sockets},
   {NULL, NULL}
};

const luaL_reg project_meta[] = {
   {"__gc"      , project_gc},
   {"__tostring", project_tostring},
   {NULL, NULL}
};
