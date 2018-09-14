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

/******************************************************************/
/*                Functions dealing with DWARF                    */
/******************************************************************/
static char api_initialized = 0;

#define CHECK_DAPI_INIT(func_ptr) \
	do { if (api_initialized <= 0) { dwarf_lua_debug(func_ptr, "The API seems not initialized.\n"); return 0;} } while(0)

static void dwarf_lua_debug(int (*func)(struct lua_State *), char *format, ...);

static int l_dwarf_api_init(lua_State *L)
// OK
{
   if (api_initialized)
      dwarf_lua_debug(l_dwarf_api_init,
            "You should call api:finish() before initializing a new instance of DwarfAPI",
            __FUNCTION__);

   char *bin_name = (char *) luaL_checkstring(L, 1);

   create_dapi(L, dwarf_api_init(NULL, bin_name));

   api_initialized++;

   return 1;
}

static int l_dwarf_api_end(lua_State *L)
// OK
{
   CHECK_DAPI_INIT(l_dwarf_api_end);

   da_t *d = luaL_checkudata(L, 1, DWARF_API);

   dwarf_api_end(d->p);

   api_initialized--;

   return 1;
}

static int l_dwarf_api_get(lua_State *L)
// OK
{
   CHECK_DAPI_INIT(l_dwarf_api_get);

   DwarfAPI *api = dwarf_api_get();

   if (api != NULL)
      create_dapi(L, api);
   else {
      printf(
            "Warning (%s) : DwarfAPI must be initialized before calling dwarf_api.get()\n",
            __FUNCTION__);
      lua_pushnil(L);
   }

   return (api != NULL);
}

static int l_dwarf_api_get_function_by_addr(lua_State *L)
// OK
{
   CHECK_DAPI_INIT(l_dwarf_api_get_function_by_addr);

   da_t *d = luaL_checkudata(L, 1, DWARF_API);
   Dwarf_Addr address = luaL_checkinteger(L, 2);

   if (d == NULL)
      return 0;

   DwarfFunction *res = dwarf_api_get_function_by_addr(d->p, address);

   if (res != NULL)
      create_dfunc(L, res);
   else
      lua_pushnil(L);

   return (res != NULL);
}

static int l_dwarf_api_get_function_by_name(lua_State *L)
// OK
{
   CHECK_DAPI_INIT(l_dwarf_api_get_function_by_name);

   da_t *d = luaL_checkudata(L, 1, DWARF_API);
   char *name = (char *) luaL_checkstring(L, 2);

   if (d == NULL || name == NULL)
      return 0;

   DwarfFunction *res = dwarf_api_get_function_by_name(d->p, name);

   if (res != NULL)
      create_dfunc(L, res);
   else
      lua_pushnil(L);

   return (res != NULL);
}

static int l_dwarf_api_get_file_by_name(lua_State *L)
// OK
{
   CHECK_DAPI_INIT(l_dwarf_api_get_file_by_name);

   da_t *d = luaL_checkudata(L, 1, DWARF_API);
   char *filename = (char *) luaL_checkstring(L, 2);

   if (d == NULL || filename == NULL)
      return 0;

   DwarfFile *res = dwarf_api_get_file_by_name(d->p, filename);

   if (res != NULL)
      create_dfile(L, res);
   else
      lua_pushnil(L);

   return (res != NULL);
}

static int l_dwarf_api_get_line(lua_State *L)
{
   CHECK_DAPI_INIT(l_dwarf_api_get_line);

   da_t *d = luaL_checkudata(L, 1, DWARF_API);
   Dwarf_Addr address = luaL_checkinteger(L, 2);
   const char *filename;

   if (d == NULL)
      return 0;

   int res = dwarf_api_get_line(d->p, address, &filename);

   lua_pushinteger(L, res);

   return 1;
}

static int l_dwarf_api_get_lines(lua_State *L)
// TODO
{
   CHECK_DAPI_INIT(l_dwarf_api_get_lines);

   dwarf_lua_debug(l_dwarf_api_get_lines, "Currently not implemented");

   (void) L;
   // unsigned int *res = dwarf_api_get_lines(d->p, addresses, count, &filenames);

   return 1;
}

static int l_dwarf_api_get_verbose(lua_State *L)
// OK
{
   CHECK_DAPI_INIT(l_dwarf_api_get_verbose);

   char verbose = dwarf_api_get_verbose();

   lua_pushinteger(L, verbose);

   return 1;
}

static int l_dwarf_api_set_verbose(lua_State *L)
// OK
{
   CHECK_DAPI_INIT(l_dwarf_api_set_verbose);

   char verbose = luaL_checkinteger(L, 1);

   dwarf_api_set_verbose(verbose);

   return 1;
}

static int _files_iter(lua_State * L)
// OK
{
   list_t **list = lua_touserdata(L, lua_upvalueindex(1));

   if ((list != NULL) && (*list != NULL)) {
      create_dfile(L, list_getdata(*list));
      *list = list_getnext(*list);

      return 1;
   }

   return 0;
}

static int l_dwarf_api_get_files(lua_State * L)
// OK
{
   CHECK_DAPI_INIT(l_dwarf_api_get_files);

   da_t *d = luaL_checkudata(L, 1, DWARF_API);
   queue_t *files = dwarf_api_get_files(d->p);

   if (files != NULL) {
      list_t **list = lua_newuserdata(L, sizeof *list);
      *list = queue_iterator(files);
   }

   else {
      /* This case should never occur */
      lua_pushnil(L);
   }

   lua_pushcclosure(L, _files_iter, 1);

   return 1;
}

static int _containers_iter(lua_State * L)
// OK
{
   list_t **list = lua_touserdata(L, lua_upvalueindex(1));

   if ((list != NULL) && (*list != NULL)) {
      create_dctn(L, list_getdata(*list));
      *list = list_getnext(*list);

      return 1;
   }

   return 0;
}

static int l_dwarf_api_get_containers(lua_State * L)
// OK
{
   CHECK_DAPI_INIT(l_dwarf_api_get_containers);

   da_t *d = luaL_checkudata(L, 1, DWARF_API);
   queue_t *containers = dwarf_api_get_containers(d->p);

   if (containers != NULL) {
      list_t **list = lua_newuserdata(L, sizeof *list);
      *list = queue_iterator(containers);
   }

   else {
      /* This case should never occur */
      lua_pushnil(L);
   }

   lua_pushcclosure(L, _containers_iter, 1);

   return 1;
}

static int l_dwarf_api_get_containers_count(lua_State * L)
{
   CHECK_DAPI_INIT(l_dwarf_api_get_containers_count);
   da_t *d = luaL_checkudata(L, 1, DWARF_API);

   int count = dwarf_api_get_containers_count(d->p);

   lua_pushinteger(L, count);
   return 1;
}

static int l_dwarf_api_get_files_count(lua_State * L)
// OK
{
   CHECK_DAPI_INIT(l_dwarf_api_get_files_count);

   da_t *d = luaL_checkudata(L, 1, DWARF_API);
   int res = dwarf_api_get_files_count(d->p);

   lua_pushinteger(L, res);

   return 1;
}

static int _globals_iter(lua_State * L)
// OK
{
   list_t **list = lua_touserdata(L, lua_upvalueindex(1));

   if ((list != NULL) && (*list != NULL)) {
      create_dglob(L, list_getdata(*list));
      *list = list_getnext(*list);

      return 1;
   }

   return 0;
}

static int l_dwarf_api_get_globals(lua_State * L)
// OK
{
   CHECK_DAPI_INIT(l_dwarf_api_get_globals);

   da_t *d = luaL_checkudata(L, 1, DWARF_API);
   queue_t *globals = dwarf_api_get_globals(d->p);

   if (globals != NULL) {
      list_t **list = lua_newuserdata(L, sizeof *list);
      *list = queue_iterator(globals);
   }

   else {
      /* This case should never occur */
      lua_pushnil(L);
   }

   lua_pushcclosure(L, _globals_iter, 1);

   return 1;
}

static int l_dwarf_api_get_globals_count(lua_State * L)
// OK
{
   CHECK_DAPI_INIT(l_dwarf_api_get_globals_count);

   da_t *d = luaL_checkudata(L, 1, DWARF_API);
   int res = dwarf_api_get_globals_count(d->p);

   lua_pushinteger(L, res);

   return 1;
}

static int l_dwarf_api_get_global_by_address(lua_State * L)
// OK
{
   CHECK_DAPI_INIT(l_dwarf_api_get_global_by_address);

   da_t *d = luaL_checkudata(L, 1, DWARF_API);
   Dwarf_Addr address = luaL_checkinteger(L, 2);

   if (d == NULL)
      return 0;

   DwarfGlobal *res = dwarf_api_get_global_by_address(d->p, address);

   if (res == NULL)
      lua_pushnil(L);
   else
      create_dglob(L, res);

   return (res != NULL);
}

static int l_dwarf_api_get_global_by_name(lua_State * L)
// OK
{
   CHECK_DAPI_INIT(l_dwarf_api_get_global_by_name);

   da_t *d = luaL_checkudata(L, 1, DWARF_API);
   char *name = (char *) luaL_checkstring(L, 2);

   if (d == NULL || name == NULL)
      return 0;

   DwarfGlobal *res = dwarf_api_get_global_by_name(d->p, name);

   if (res == NULL)
      lua_pushnil(L);
   else
      create_dglob(L, res);

   return (res != NULL);
}

static int l_dwarf_file_get_name(lua_State * L)
// OK
{
   CHECK_DAPI_INIT(l_dwarf_file_get_name);

   dfi_t *file_container = luaL_checkudata(L, 1, DWARF_FILE);
   DwarfFile *file = file_container->p;

   char *res = dwarf_file_get_name(file);

   lua_pushstring(L, res);

   return (res != NULL);
}

static int l_dwarf_file_get_dir(lua_State * L)
// OK
{
   CHECK_DAPI_INIT(l_dwarf_file_get_dir);

   dfi_t *file_container = luaL_checkudata(L, 1, DWARF_FILE);
   DwarfFile *file = file_container->p;

   char *res = dwarf_file_get_dir(file);

   lua_pushstring(L, res);

   return (res != NULL);
}

static int l_dwarf_file_get_version(lua_State * L)
// OK
{
   CHECK_DAPI_INIT(l_dwarf_file_get_version);

   dfi_t *file_container = luaL_checkudata(L, 1, DWARF_FILE);
   DwarfFile *file = file_container->p;

   char *res = dwarf_file_get_version(file);

   lua_pushstring(L, res);

   return (res != NULL);
}

static int l_dwarf_file_get_language(lua_State * L)
// OK
{
   CHECK_DAPI_INIT(l_dwarf_file_get_language);

   dfi_t *file_container = luaL_checkudata(L, 1, DWARF_FILE);
   DwarfFile *file = file_container->p;

   char *res = dwarf_file_get_language(file);

   lua_pushstring(L, res);

   return (res != NULL);
}

static int l_dwarf_file_get_vendor(lua_State * L)
// OK
{
   CHECK_DAPI_INIT(l_dwarf_file_get_vendor);

   dfi_t *file_container = luaL_checkudata(L, 1, DWARF_FILE);
   DwarfFile *file = file_container->p;

   char *res = dwarf_file_get_vendor(file);

   lua_pushstring(L, res);

   return (res != NULL);
}

static int _functions_iter(lua_State * L)
// OK
{
   list_t **list = lua_touserdata(L, lua_upvalueindex(1));

   if ((list != NULL) && (*list != NULL)) {
      create_dfunc(L, list_getdata(*list));
      *list = list_getnext(*list);

      return 1;
   }

   return 0;
}

static int l_dwarf_file_get_functions(lua_State * L)
// OK
{
   CHECK_DAPI_INIT(l_dwarf_file_get_functions);

   dfi_t *d = luaL_checkudata(L, 1, DWARF_FILE);
   queue_t *functions = dwarf_file_get_functions(d->p);

   if (functions != NULL) {
      list_t **list = lua_newuserdata(L, sizeof *list);
      *list = queue_iterator(functions);
   }

   else {
      /* This case should never occur */
      lua_pushnil(L);
   }

   lua_pushcclosure(L, _functions_iter, 1);

   return 1;
}

static int l_dwarf_file_get_function_by_addr(lua_State * L)
// OK
{
   CHECK_DAPI_INIT(l_dwarf_file_get_function_by_addr);

   dfi_t *file_container = luaL_checkudata(L, 1, DWARF_FILE);
   DwarfFile *file = file_container->p;

   Dwarf_Addr address = luaL_checkinteger(L, 2);

   if (file == NULL)
      return 0;

   DwarfFunction *res = dwarf_file_get_function_by_addr(file, address);

   if (res == NULL)
      lua_pushnil(L);
   else
      create_dfunc(L, res);

   return (res != NULL);
}

static int l_dwarf_file_get_function_by_name(lua_State * L)
// OK
{
   CHECK_DAPI_INIT(l_dwarf_file_get_function_by_name);

   dfi_t *file_container = luaL_checkudata(L, 1, DWARF_FILE);
   DwarfFile *file = file_container->p;

   char *name = (char *) luaL_checkstring(L, 2);

   if (file == NULL || name == NULL)
      return 0;

   DwarfFunction *res = dwarf_file_get_function_by_name(file, name);

   if (res == NULL)
      lua_pushnil(L);
   else
      create_dfunc(L, res);

   return (res != NULL);
}

static int l_dwarf_file_get_function_count(lua_State * L)
// OK
{
   CHECK_DAPI_INIT(l_dwarf_file_get_function_count);

   dfi_t *file_container = luaL_checkudata(L, 1, DWARF_FILE);
   DwarfFile *file = file_container->p;

   int res = dwarf_file_get_function_count(file);

   lua_pushinteger(L, res);

   return 1;
}

static int l_dwarf_api_debug(lua_State *L)
// OK
{
   CHECK_DAPI_INIT(l_dwarf_api_debug);

   da_t *api_container = luaL_checkudata(L, 1, DWARF_API);
   DwarfAPI *api = api_container->p;

   if (api == NULL)
      return 0;

   dwarf_api_debug(api, stdout);
   return 1;
}

static int l_dwarf_file_debug(lua_State * L)
// OK
{
   CHECK_DAPI_INIT(l_dwarf_file_debug);

   dfi_t *file_container = luaL_checkudata(L, 1, DWARF_FILE);
   DwarfFile *file = file_container->p;

   if (file == NULL)
      return 0;

   dwarf_file_debug(file, stdout);
   return 1;
}

static int l_dwarf_function_get_file(lua_State * L)
// OK
{
   CHECK_DAPI_INIT(l_dwarf_function_get_file);

   dfu_t *func_container = luaL_checkudata(L, 1, DWARF_FUNC);
   DwarfFunction *function = func_container->p;

   if (function == NULL)
      return 0;

   DwarfFile *res = dwarf_function_get_file(function);

   if (res == NULL)
      lua_pushnil(L);
   else
      create_dfile(L, res);

   return 1;
}

static int l_dwarf_function_get_name(lua_State * L)
// OK
{
   CHECK_DAPI_INIT(l_dwarf_function_get_name);

   dfu_t *function_container = luaL_checkudata(L, 1, DWARF_FUNC);
   DwarfFunction *function = function_container->p;

   if (function == NULL)
      return 0;

   char *res = dwarf_function_get_name(function);

   lua_pushstring(L, res);

   return 1;
}

static int l_dwarf_function_get_linkage_name(lua_State * L)
// OK
{
   CHECK_DAPI_INIT(l_dwarf_function_get_linkage_name);

   dfu_t *function_container = luaL_checkudata(L, 1, DWARF_FUNC);
   DwarfFunction *function = function_container->p;

   if (function == NULL)
      return 0;

   char *res = dwarf_function_get_linkage_name(function);

   lua_pushstring(L, res);

   return 1;
}

static int _parameters_iter(lua_State * L)
// OK
{
   list_t **list = lua_touserdata(L, lua_upvalueindex(1));

   if ((list != NULL) && (*list != NULL)) {
      create_dvar(L, list_getdata(*list));
      *list = list_getnext(*list);

      return 1;
   }

   return 0;
}

static int l_dwarf_function_get_parameters(lua_State * L)
// OK
{
   CHECK_DAPI_INIT(l_dwarf_function_get_parameters);

   dfu_t *d = luaL_checkudata(L, 1, DWARF_FUNC);
   queue_t *parameters = dwarf_function_get_parameters(d->p);

   if (parameters != NULL) {
      list_t **list = lua_newuserdata(L, sizeof *list);
      *list = queue_iterator(parameters);
   }

   else {
      /* This case should never occur */
      lua_pushnil(L);
   }

   lua_pushcclosure(L, _parameters_iter, 1);

   return 1;
}

static int _locals_iter(lua_State * L)
// OK
{
   list_t **list = lua_touserdata(L, lua_upvalueindex(1));

   if ((list != NULL) && (*list != NULL)) {
      create_dvar(L, list_getdata(*list));
      *list = list_getnext(*list);

      return 1;
   }

   return 0;
}

static int l_dwarf_function_get_locals(lua_State * L)
// OK
{
   CHECK_DAPI_INIT(l_dwarf_function_get_locals);

   dfu_t *d = luaL_checkudata(L, 1, DWARF_FUNC);
   queue_t *locals = dwarf_function_get_locals(d->p);

   if (locals != NULL) {
      list_t **list = lua_newuserdata(L, sizeof *list);
      *list = queue_iterator(locals);
   }

   else {
      /* This case should never occur */
      lua_pushnil(L);
   }

   lua_pushcclosure(L, _locals_iter, 1);

   return 1;
}

static int l_dwarf_function_get_low_pc(lua_State * L)
// OK
{
   CHECK_DAPI_INIT(l_dwarf_function_get_low_pc);

   dfu_t *d = luaL_checkudata(L, 1, DWARF_FUNC);
   DwarfFunction *function = d->p;

   Dwarf_Addr low_pc;

   if (function == NULL)
      return 0;

   dwarf_function_get_address(function, &low_pc, NULL);

   lua_pushinteger(L, low_pc);

   return 1;
}

static int l_dwarf_function_get_high_pc(lua_State * L)
// OK
{
   CHECK_DAPI_INIT(l_dwarf_function_get_high_pc);

   dfu_t *d = luaL_checkudata(L, 1, DWARF_FUNC);
   DwarfFunction *function = d->p;

   Dwarf_Addr high_pc;

   if (function == NULL)
      return 0;

   dwarf_function_get_address(function, NULL, &high_pc);

   lua_pushinteger(L, high_pc);

   return 1;
}

static int l_dwarf_function_get_line(lua_State * L)
// OK
{
   CHECK_DAPI_INIT(l_dwarf_function_get_line);

   dfu_t *d = luaL_checkudata(L, 1, DWARF_FUNC);
   DwarfFunction *function = d->p;

   unsigned int line_decl;

   if (function == NULL)
      return 0;

   dwarf_function_get_decl(function, &line_decl, NULL);

   lua_pushinteger(L, line_decl);

   return 1;
}

static int l_dwarf_function_get_col(lua_State * L)
// OK
{
   CHECK_DAPI_INIT(l_dwarf_function_get_col);

   dfu_t *d = luaL_checkudata(L, 1, DWARF_FUNC);
   DwarfFunction *function = d->p;

   unsigned int column_decl;

   if (function == NULL)
      return 0;

   dwarf_function_get_decl(function, NULL, &column_decl);

   lua_pushinteger(L, column_decl);

   return 1;
}

static int l_dwarf_function_get_ret_var(lua_State * L)
// OK
{
   CHECK_DAPI_INIT(l_dwarf_function_get_ret_var);

   dfu_t *d = luaL_checkudata(L, 1, DWARF_FUNC);
   DwarfFunction *function = d->p;

   DwarfVar *res = dwarf_function_get_ret_var(function);

   if (res == NULL)
      return 0;

   create_dvar(L, res);

   return 1;
}

static int l_dwarf_function_get_param_count(lua_State * L)
// OK
{
   CHECK_DAPI_INIT(l_dwarf_function_get_param_count);

   dfu_t *d = luaL_checkudata(L, 1, DWARF_FUNC);
   DwarfFunction *function = d->p;

   int res = dwarf_function_get_param_count(function);

   lua_pushinteger(L, res);

   return 1;
}

static int l_dwarf_function_get_local_count(lua_State * L)
// OK
{
   CHECK_DAPI_INIT(l_dwarf_function_get_local_count);

   dfu_t *d = luaL_checkudata(L, 1, DWARF_FUNC);
   DwarfFunction *function = d->p;

   int res = dwarf_function_get_local_count(function);

   lua_pushinteger(L, res);

   return 1;
}

static int l_dwarf_function_debug(lua_State * L)
// OK
{
   CHECK_DAPI_INIT(l_dwarf_function_debug);

   dfu_t *d = luaL_checkudata(L, 1, DWARF_FUNC);
   DwarfFunction *function = d->p;

   dwarf_function_debug(function, stdout);

   return 1;
}

#define l_dwarf_var_is_(x)									 	\
	static int l_dwarf_var_is_##x (lua_State * L) { 			\
		CHECK_DAPI_INIT(l_dwarf_var_is_##x);					\
		dv_t *d = luaL_checkudata(L, 1, DWARF_VAR);				\
		DwarfVar *var = d->p;									\
		if (var == NULL)										\
			return 0; 											\
		lua_pushinteger (L, dwarf_var_is_##x (var)); 			\
		return 1; 												\
	}

// OK
l_dwarf_var_is_(const);
l_dwarf_var_is_(struct);
l_dwarf_var_is_(enum);
l_dwarf_var_is_(inline);
l_dwarf_var_is_(extern);
l_dwarf_var_is_(static);
l_dwarf_var_is_(array);
#undef l_dwarf_var_is_

static int l_dwarf_var_get_name(lua_State * L)
// OK
{
   CHECK_DAPI_INIT(l_dwarf_var_get_name);

   dv_t *var_container = luaL_checkudata(L, 1, DWARF_VAR);
   DwarfVar *var = var_container->p;

   if (var == NULL)
      return 0;

   char *res = dwarf_var_get_name(var);

   if (res == NULL)
      return 0;

   lua_pushstring(L, res);

   return 1;
}

static int l_dwarf_var_get_function(lua_State * L)
// OK
{
   CHECK_DAPI_INIT(l_dwarf_var_get_function);

   dv_t *var_container = luaL_checkudata(L, 1, DWARF_VAR);
   DwarfVar *var = var_container->p;

   if (var == NULL)
      return 0;

   DwarfFunction *res = dwarf_var_get_function(var);

   if (res == NULL)
      return 0;

   create_dfunc(L, res);

   return 1;
}

static int l_dwarf_var_get_type(lua_State * L)
// OK
{
   CHECK_DAPI_INIT(l_dwarf_var_get_type);

   dv_t *var_container = luaL_checkudata(L, 1, DWARF_VAR);
   DwarfVar *var = var_container->p;

   if (var == NULL)
      return 0;

   char *res = dwarf_var_get_type(var);

   if (res == NULL)
      return 0;

   lua_pushstring(L, res);

   return 1;
}

static int l_dwarf_var_get_full_type(lua_State * L)
// OK
{
   CHECK_DAPI_INIT(l_dwarf_var_get_full_type);

   dv_t *var_container = luaL_checkudata(L, 1, DWARF_VAR);
   DwarfVar *var = var_container->p;

   if (var == NULL)
      return 0;

   char *res = dwarf_var_get_full_type(var);

   if (res == NULL)
      return 0;

   lua_pushstring(L, res);

   return 1;
}

static int l_dwarf_var_get_pointer_count(lua_State * L)
// OK
{
   CHECK_DAPI_INIT(l_dwarf_var_get_pointer_count);

   dv_t *var_container = luaL_checkudata(L, 1, DWARF_VAR);
   DwarfVar *var = var_container->p;

   if (var == NULL)
      return 0;

   int res = dwarf_var_get_pointer_count(var);

   lua_pushinteger(L, res);

   return 1;
}

static int l_dwarf_var_get_array_size(lua_State * L)
// OK
{
   CHECK_DAPI_INIT(l_dwarf_var_get_array_size);

   dv_t *var_container = luaL_checkudata(L, 1, DWARF_VAR);
   DwarfVar *var = var_container->p;

   if (var == NULL)
      return 0;

   int res = dwarf_var_get_array_size(var);

   lua_pushinteger(L, res);

   return 1;
}

static int l_dwarf_var_get_byte_size(lua_State * L)
// OK
{
   CHECK_DAPI_INIT(l_dwarf_var_get_byte_size);

   dv_t *var_container = luaL_checkudata(L, 1, DWARF_VAR);
   DwarfVar *var = var_container->p;

   if (var == NULL)
      return 0;

   int res = dwarf_var_get_byte_size(var);

   lua_pushinteger(L, res);

   return 1;
}

static int l_dwarf_var_get_line(lua_State * L)
// OK
{
   CHECK_DAPI_INIT(l_dwarf_var_get_line);

   dv_t *var_container = luaL_checkudata(L, 1, DWARF_VAR);
   DwarfVar *var = var_container->p;

   int line_decl;

   if (var == NULL)
      return 0;

   dwarf_var_get_decl(var, &line_decl, NULL);

   lua_pushinteger(L, line_decl);

   return 1;
}

static int l_dwarf_var_get_col(lua_State * L)
// OK
{
   CHECK_DAPI_INIT(l_dwarf_var_get_col);

   dv_t *var_container = luaL_checkudata(L, 1, DWARF_VAR);
   DwarfVar *var = var_container->p;

   int column_decl;

   if (var == NULL)
      return 0;

   dwarf_var_get_decl(var, NULL, &column_decl);

   lua_pushinteger(L, column_decl);

   return 1;
}

static int _mem_locations_iter(lua_State * L)
// OK
{
   list_t **list = lua_touserdata(L, lua_upvalueindex(1));

   if ((list != NULL) && (*list != NULL)) {
      create_dmemloc(L, list_getdata(*list));
      *list = list_getnext(*list);

      return 1;
   }

   return 0;
}

static int l_dwarf_var_get_mem_locations(lua_State * L)
// OK
{
   CHECK_DAPI_INIT(l_dwarf_var_get_mem_locations);

   dv_t *d = luaL_checkudata(L, 1, DWARF_VAR);
   queue_t *mem_locations = dwarf_var_get_mem_locations(d->p);

   if (mem_locations != NULL) {
      list_t **list = lua_newuserdata(L, sizeof *list);
      *list = queue_iterator(mem_locations);
   }

   else {
      /* This case should never occur */
      lua_pushnil(L);
   }

   lua_pushcclosure(L, _mem_locations_iter, 1);

   return 1;
}

static int l_dwarf_var_get_memlocs_count(lua_State * L)
// OK
{
   CHECK_DAPI_INIT(l_dwarf_var_get_memlocs_count);

   dv_t *var_container = luaL_checkudata(L, 1, DWARF_VAR);
   DwarfVar *var = var_container->p;

   if (var == NULL)
      return 0;

   int res = dwarf_var_get_memlocs_count(var);

   lua_pushinteger(L, res);

   return 1;
}

static int l_dwarf_var_debug(lua_State * L)
// OK
{
   CHECK_DAPI_INIT(l_dwarf_var_debug);

   dv_t *var_container = luaL_checkudata(L, 1, DWARF_VAR);
   DwarfVar *var = var_container->p;

   if (var == NULL)
      return 0;

   dwarf_var_debug(var, stdout);

   return 1;
}

static int l_dwarf_var_get_first_memloc(lua_State * L)
// OK
{
   CHECK_DAPI_INIT(l_dwarf_var_get_first_memloc);

   dv_t *var_container = luaL_checkudata(L, 1, DWARF_VAR);
   DwarfVar *var = var_container->p;

   if (var == NULL)
      return 0;

   DwarfMemLoc *res = dwarf_var_get_first_memloc(var);

   if (res == NULL) {
      lua_pushnil(L);
      return 0;
   }

   create_dmemloc(L, res);

   return 1;
}

static int l_dwarf_var_get_accessibility(lua_State * L)
// OK
{
   CHECK_DAPI_INIT(l_dwarf_var_get_accessibility);

   dv_t *var_container = luaL_checkudata(L, 1, DWARF_VAR);
   DwarfVar *var = var_container->p;

   if (var == NULL)
      return 0;

   char res = dwarf_var_get_accessibility(var);

   lua_pushinteger(L, res);

   return 1;
}

static int l_dwarf_var_get_accessibility_str(lua_State * L)
// OK
{
   CHECK_DAPI_INIT(l_dwarf_var_get_accessibility_str);

   dv_t *var_container = luaL_checkudata(L, 1, DWARF_VAR);
   DwarfVar *var = var_container->p;

   if (var == NULL)
      return 0;

   const char *res = dwarf_var_get_accessibility_str(var);

   lua_pushstring(L, res);

   return 1;
}

static int l_dwarf_global_get_var(lua_State * L)
// OK
{
   CHECK_DAPI_INIT(l_dwarf_global_get_var);

   dg_t *global_c = luaL_checkudata(L, 1, DWARF_GLOB);
   DwarfGlobal *global = global_c->p;

   if (global == NULL)
      return 0;

   DwarfVar *res = dwarf_global_get_var(global);

   if (res == NULL) {
      lua_pushnil(L);
      return 0;
   }

   create_dvar(L, res);

   return 1;
}

static int l_dwarf_global_get_file(lua_State * L)
// OK
{
   CHECK_DAPI_INIT(l_dwarf_global_get_file);

   dg_t *global_c = luaL_checkudata(L, 1, DWARF_GLOB);
   DwarfGlobal *global = global_c->p;

   if (global == NULL) {
      lua_pushnil(L);
      return 0;
   }

   DwarfFile *res = dwarf_global_get_file(global);

   if (res == NULL) {
      lua_pushnil(L);
      return 0;
   }

   create_dfile(L, res);

   return 1;
}

static int l_dwarf_global_debug(lua_State * L)
// OK
{
   CHECK_DAPI_INIT(l_dwarf_global_debug);

   dg_t *global_c = luaL_checkudata(L, 1, DWARF_GLOB);
   DwarfGlobal *global = global_c->p;

   if (global == NULL)
      return 0;

   dwarf_global_debug(global, stdout);

   return 1;
}

static int l_dwarf_memloc_get_type(lua_State * L)
// OK
{
   CHECK_DAPI_INIT(l_dwarf_memloc_get_type);

   dm_t *memloc_container = luaL_checkudata(L, 1, DWARF_MEMLOC);
   DwarfMemLoc *memloc = memloc_container->p;

   if (memloc == NULL)
      return 0;

   int res = dwarf_memloc_get_type(memloc);

   lua_pushinteger(L, res);

   return 1;
}

static int l_dwarf_memloc_get_offset(lua_State * L)
// OK
{
   CHECK_DAPI_INIT(l_dwarf_memloc_get_offset);

   dm_t *memloc_container = luaL_checkudata(L, 1, DWARF_MEMLOC);
   DwarfMemLoc *memloc = memloc_container->p;

   if (memloc == NULL)
      return 0;

   Dwarf_Signed res = dwarf_memloc_get_offset(memloc);

   lua_pushinteger(L, res);

   return 1;
}

static int l_dwarf_memloc_get_address(lua_State * L)
{
   CHECK_DAPI_INIT(l_dwarf_memloc_get_address);

   dm_t *memloc_container = luaL_checkudata(L, 1, DWARF_MEMLOC);
   DwarfMemLoc *memloc = memloc_container->p;

   if (memloc == NULL)
      return 0;

   Dwarf_Addr res = dwarf_memloc_get_address(memloc);

   lua_pushinteger(L, res);

   return 1;
}

static int l_dwarf_memloc_get_low_pc(lua_State * L)
// OK
{
   CHECK_DAPI_INIT(l_dwarf_memloc_get_low_pc);

   dm_t *memloc_container = luaL_checkudata(L, 1, DWARF_MEMLOC);
   DwarfMemLoc *memloc = memloc_container->p;

   Dwarf_Addr low_pc;

   if (memloc == NULL)
      return 0;

   dwarf_memloc_get_range(memloc, &low_pc, NULL);

   lua_pushinteger(L, low_pc);

   return 1;
}

static int l_dwarf_memloc_get_high_pc(lua_State * L)
// OK
{
   CHECK_DAPI_INIT(l_dwarf_memloc_get_high_pc);

   dm_t *memloc_container = luaL_checkudata(L, 1, DWARF_MEMLOC);
   DwarfMemLoc *memloc = memloc_container->p;

   Dwarf_Addr high_pc;

   if (memloc == NULL)
      return 0;

   dwarf_memloc_get_range(memloc, NULL, &high_pc);

   lua_pushinteger(L, high_pc);

   return 1;
}

static int l_dwarf_memloc_get_reg_name(lua_State * L)
// OK
{
   CHECK_DAPI_INIT(l_dwarf_memloc_get_reg_name);

   dm_t *memloc_container = luaL_checkudata(L, 1, DWARF_MEMLOC);
   DwarfMemLoc *memloc = memloc_container->p;

   if (memloc == NULL)
      return 0;

   char *res = dwarf_memloc_get_reg_name(memloc);

   lua_pushstring(L, res);

   return 1;
}

static int l_dwarf_memloc_is_register(lua_State * L)
// OK
{
   CHECK_DAPI_INIT(l_dwarf_memloc_is_register);

   dm_t *memloc_container = luaL_checkudata(L, 1, DWARF_MEMLOC);
   DwarfMemLoc *memloc = memloc_container->p;

   if (memloc == NULL)
      return 0;

   char res = dwarf_memloc_is_register(memloc);

   lua_pushinteger(L, res);

   return 1;
}

static int l_dwarf_memloc_is_address(lua_State * L)
// OK
{
   CHECK_DAPI_INIT(l_dwarf_memloc_is_address);

   dm_t *memloc_container = luaL_checkudata(L, 1, DWARF_MEMLOC);
   DwarfMemLoc *memloc = memloc_container->p;

   if (memloc == NULL)
      return 0;

   char res = dwarf_memloc_is_address(memloc);

   lua_pushinteger(L, res);

   return 1;
}

static int l_dwarf_memloc_get_type_str(lua_State * L)
// OK
{
   CHECK_DAPI_INIT(l_dwarf_memloc_get_type_str);

   dm_t *memloc_container = luaL_checkudata(L, 1, DWARF_MEMLOC);
   DwarfMemLoc *memloc = memloc_container->p;

   if (memloc == NULL)
      return 0;

   const char *res = dwarf_memloc_get_type_str(memloc);

   if (res == NULL)
      return 0;

   lua_pushstring(L, res);

   return 1;
}

static int l_dwarf_memloc_debug(lua_State * L)
{
   CHECK_DAPI_INIT(l_dwarf_memloc_debug);

   dm_t *memloc_container = luaL_checkudata(L, 1, DWARF_MEMLOC);
   DwarfMemLoc *memloc = memloc_container->p;

   if (memloc == NULL)
      return 0;

   dwarf_memloc_debug(memloc, stdout);

   return 1;
}

static int _container_iter(lua_State * L)
// OK
{
   list_t **list = lua_touserdata(L, lua_upvalueindex(1));

   if ((list != NULL) && (*list != NULL)) {
      create_dobj(L, list_getdata(*list));
      *list = list_getnext(*list);

      return 1;
   }

   return 0;
}

static int l_dwarf_container_get_objects(lua_State * L)
{
   CHECK_DAPI_INIT(l_dwarf_container_get_objects);

   dc_t *ctn = luaL_checkudata(L, 1, DWARF_CTN);
   queue_t *members = dwarf_container_get_members(ctn->p);

   if (ctn == NULL) {
      /* This case should never occur */
      lua_pushnil(L);
   }

   else {
      list_t **list = lua_newuserdata(L, sizeof *list);
      *list = queue_iterator(members);
   }

   lua_pushcclosure(L, _container_iter, 1);

   return 1;
}

static int l_dwarf_container_get_objects_count(lua_State * L)
{
   CHECK_DAPI_INIT(l_dwarf_container_get_objects_count);

   dc_t *d = luaL_checkudata(L, 1, DWARF_CTN);
   int res = dwarf_container_get_members_count(d->p);

   lua_pushinteger(L, res);
   return 1;
}

static int l_dwarf_container_get_name(lua_State * L)
{
   CHECK_DAPI_INIT(l_dwarf_container_get_name);

   dc_t *d = luaL_checkudata(L, 1, DWARF_CTN);
   char *name = dwarf_container_get_name(d->p);

   lua_pushstring(L, name);
   return 1;
}

static int l_dwarf_container_get_byte_size(lua_State * L)
{
   CHECK_DAPI_INIT(l_dwarf_container_get_byte_size);

   dc_t *d = luaL_checkudata(L, 1, DWARF_CTN);
   int byte_size = dwarf_container_get_byte_size(d->p);

   lua_pushinteger(L, byte_size);
   return 1;
}

static int l_dwarf_container_get_type(lua_State * L)
{
   CHECK_DAPI_INIT(l_dwarf_container_get_type);
   dc_t *d = luaL_checkudata(L, 1, DWARF_CTN);

   int type = dwarf_container_get_type(d->p);

   lua_pushinteger(L, type);
   return 1;
}

static int l_dwarf_container_get_type_str(lua_State * L)
{
   CHECK_DAPI_INIT(l_dwarf_container_get_type_str);
   dc_t *d = luaL_checkudata(L, 1, DWARF_CTN);

   const char *str = dwarf_container_get_type_str(d->p);

   lua_pushstring(L, str);
   return 1;
}

static int l_dwarf_object_debug(lua_State * L)
{
   CHECK_DAPI_INIT(l_dwarf_object_debug);
   do_t *d = luaL_checkudata(L, 1, DWARF_OBJ);

   dwarf_object_debug(d->p, stdout);

   return 1;
}

static int l_dwarf_container_debug(lua_State * L)
{
   CHECK_DAPI_INIT(l_dwarf_container_debug);
   dc_t *d = luaL_checkudata(L, 1, DWARF_CTN);

   dwarf_container_debug(d->p, stdout);

   return 1;
}

static int l_dwarf_object_get_ctn_type(lua_State * L)
{
   CHECK_DAPI_INIT(l_dwarf_object_get_ctn_type);
   do_t *d = luaL_checkudata(L, 1, DWARF_OBJ);

   int type = dwarf_object_get_ctn_type(d->p);
   lua_pushinteger(L, type);

   return 1;
}

static int l_dwarf_object_get_data(lua_State * L)
{
   CHECK_DAPI_INIT(l_dwarf_object_get_ctn_type);
   do_t *d = luaL_checkudata(L, 1, DWARF_OBJ);
   DwarfObject *o = d->p;
   int type = dwarf_object_get_type(o);
   void *data = dwarf_object_get_data(o);

   switch (type) {
   case DWARF_OBJECT_FUNCTION:
      create_dfunc(L, data);
      break;
   case DWARF_OBJECT_CONTAINER:
      create_dctn(L, data);
      break;
   case DWARF_OBJECT_VARIABLE:
      create_dvar(L, data);
      break;

   default:
      dwarf_lua_debug(l_dwarf_object_get_data,
            "Error: unkown object type! (%d)", type);
      lua_pushnil(L);
      break;
   }

   return 1;
}

/**
 * Bind names from this file to LUA
 * For example, {"foo", "bar"} will be interpreted in the following way:
 * to use bar (defined in this file), call foo in LUA.
 */
 
static const luaL_reg api_methods[] =
{
   {"init",                   l_dwarf_api_init},
   {"finish",                 l_dwarf_api_end},
   {"get",                    l_dwarf_api_get},
   {"get_function_by_addr",   l_dwarf_api_get_function_by_addr},
   {"get_function_by_name",   l_dwarf_api_get_function_by_name},
   {"get_file_by_name",       l_dwarf_api_get_file_by_name},
   {"get_line",               l_dwarf_api_get_line},
   {"get_lines",              l_dwarf_api_get_lines},
   {"get_verbose",            l_dwarf_api_get_verbose},
   {"set_verbose",            l_dwarf_api_set_verbose},
   {"get_files",              l_dwarf_api_get_files},
   {"get_files_count",        l_dwarf_api_get_files_count},
   {"get_globals",            l_dwarf_api_get_globals},
   {"get_globals_count",      l_dwarf_api_get_globals_count},
   {"get_containers",         l_dwarf_api_get_containers},
   {"get_containers_count",   l_dwarf_api_get_containers_count},
   {"get_global_by_address",  l_dwarf_api_get_global_by_address},
   {"get_global_by_name",     l_dwarf_api_get_global_by_name},
   {"debug",                  l_dwarf_api_debug},
   {NULL, NULL}
};

static const luaL_reg dfile_methods[] =
{
   {"get_name",               l_dwarf_file_get_name},
   {"get_dir",                l_dwarf_file_get_dir},
   {"get_version",            l_dwarf_file_get_version},
   {"get_language",           l_dwarf_file_get_language},
   {"get_vendor",             l_dwarf_file_get_vendor},
   {"get_functions",          l_dwarf_file_get_functions},
   {"get_function_by_addr",   l_dwarf_file_get_function_by_addr},
   {"get_function_by_name",   l_dwarf_file_get_function_by_name},
   {"get_function_count",     l_dwarf_file_get_function_count},
   {"debug",                  l_dwarf_file_debug},
   {NULL, NULL}
};

static const luaL_reg dfunc_methods[] =
{
   {"get_file",               l_dwarf_function_get_file},
   {"get_name",               l_dwarf_function_get_name},
   {"get_linkage_name",       l_dwarf_function_get_linkage_name},
   {"get_parameters",         l_dwarf_function_get_parameters},
   {"get_locals",             l_dwarf_function_get_locals},
   {"get_low_pc",             l_dwarf_function_get_low_pc},
   {"get_high_pc",            l_dwarf_function_get_high_pc},
   {"get_line",               l_dwarf_function_get_line},
   {"get_col",                l_dwarf_function_get_col},
   {"get_ret_var",            l_dwarf_function_get_ret_var},
   {"get_param_count",        l_dwarf_function_get_param_count},
   {"get_local_count",        l_dwarf_function_get_local_count},
   {"debug",                  l_dwarf_function_debug},
   {NULL, NULL}
};

static const luaL_reg dvar_methods[] =
{
   {"is_const",            l_dwarf_var_is_const},
   {"is_struct",           l_dwarf_var_is_struct},
   {"is_enum",             l_dwarf_var_is_enum},
   {"is_inline",           l_dwarf_var_is_inline},
   {"is_extern",           l_dwarf_var_is_extern},
   {"is_static",           l_dwarf_var_is_static},
   {"is_array",            l_dwarf_var_is_array},
   {"get_name",            l_dwarf_var_get_name},
   {"get_function",        l_dwarf_var_get_function},
   {"get_type",            l_dwarf_var_get_type},
   {"get_full_type",       l_dwarf_var_get_full_type},
   {"get_pointer_count",   l_dwarf_var_get_pointer_count},
   {"get_array_size",      l_dwarf_var_get_array_size},
   {"get_byte_size",       l_dwarf_var_get_byte_size},
   {"get_line",            l_dwarf_var_get_line},
   {"get_col",             l_dwarf_var_get_col},
   {"get_mem_locations",   l_dwarf_var_get_mem_locations},
   {"get_memlocs_count",   l_dwarf_var_get_memlocs_count},
   {"get_first_memloc",    l_dwarf_var_get_first_memloc},
   {"get_accessibility",   l_dwarf_var_get_accessibility},
   {"get_access_str",      l_dwarf_var_get_accessibility_str},
   {"debug",               l_dwarf_var_debug},
   {NULL, NULL}
};

static const luaL_reg dglobal_methods[] =
{
   {"get_var",             l_dwarf_global_get_var},
   {"get_file",            l_dwarf_global_get_file},
   {"debug",               l_dwarf_global_debug},
   {NULL, NULL}
};

static const luaL_reg dmemloc_methods[] =
{
   {"get_type",            l_dwarf_memloc_get_type},
   {"get_offset",          l_dwarf_memloc_get_offset},
   {"get_address",         l_dwarf_memloc_get_address},
   {"get_low_pc",          l_dwarf_memloc_get_low_pc},
   {"get_high_pc",         l_dwarf_memloc_get_high_pc},
   {"get_reg_name",        l_dwarf_memloc_get_reg_name},
   {"is_register",         l_dwarf_memloc_is_register},
   {"is_address",          l_dwarf_memloc_is_address},
   {"get_type_str",        l_dwarf_memloc_get_type_str},
   {"debug",               l_dwarf_memloc_debug},
   {NULL, NULL}
};

static const luaL_reg dctn_methods[] =
{
   {"get_objects",         l_dwarf_container_get_objects},
   {"get_objects_count",   l_dwarf_container_get_objects_count},
   {"get_name",            l_dwarf_container_get_name},
   {"get_type",            l_dwarf_container_get_type},
   {"get_type_str",        l_dwarf_container_get_type_str},
   {"get_byte_size",       l_dwarf_container_get_byte_size},
   {"debug",               l_dwarf_container_debug},
   {NULL, NULL}
};

static const luaL_reg dobj_methods[] =
{
    {"get_data",                l_dwarf_object_get_data},
    {"get_ctn_type",            l_dwarf_object_get_ctn_type},
    {"debug",                   l_dwarf_object_debug},
    {NULL, NULL}
};

static int dapi_gc(lua_State * L)
{
   (void) L;
   return 0;
}
;

static int dfile_gc(lua_State * L)
{
   (void) L;
   return 0;
}
;

static int dfunc_gc(lua_State * L)
{
   (void) L;
   return 0;
}
;

static int dvar_gc(lua_State * L)
{
   (void) L;
   return 0;
}
;

static int dglobal_gc(lua_State * L)
{
   (void) L;
   return 0;
}
;

static int dmemloc_gc(lua_State * L)
{
   (void) L;
   return 0;
}
;

static int dctn_gc(lua_State * L)
{
   (void) L;
   return 0;
}
;

static int dobj_gc(lua_State * L)
{
   (void) L;
   return 0;
}
;

static const luaL_reg api_meta[] = {
  {"__gc"      , dapi_gc},
  {NULL, NULL}
};

static const luaL_reg dfile_meta[] = {
  {"__gc"      , dfile_gc},
  {NULL, NULL}
};

static const luaL_reg dfunc_meta[] = {
  {"__gc"      , dfunc_gc},
  {NULL, NULL}
};

static const luaL_reg dvar_meta[] = {
  {"__gc"      , dvar_gc},
  {NULL, NULL}
};

static const luaL_reg dglobal_meta[] = {
  {"__gc"      , dglobal_gc},
  {NULL, NULL}
};

static const luaL_reg dmemloc_meta[] = {
  {"__gc"      , dmemloc_gc},
  {NULL, NULL}
};

static const luaL_reg dctn_meta[] = {
  {"__gc"      , dctn_gc},
  {NULL, NULL}
};

static const luaL_reg dobj_meta[] = {
  {"__gc"      , dobj_gc},
  {NULL, NULL}
};


static void dwarf_lua_debug(int (*func)(struct lua_State *), char *format, ...)
{
   static const int methods_size[] = { sizeof(api_methods) / sizeof(luaL_reg)
         - 1, sizeof(dfile_methods) / sizeof(luaL_reg) - 1,
         sizeof(dfunc_methods) / sizeof(luaL_reg) - 1, sizeof(dvar_methods)
               / sizeof(luaL_reg) - 1, sizeof(dmemloc_methods)
               / sizeof(luaL_reg) - 1, sizeof(dglobal_methods)
               / sizeof(luaL_reg) - 1, sizeof(dctn_methods) / sizeof(luaL_reg)
               - 1, sizeof(dobj_methods) / sizeof(luaL_reg) - 1 };

   static const char *methods_name[] = { "api", "file", "func", "var", "memloc",
         "global", "container", "object" };

   static const luaL_reg *methods[] = { api_methods, dfile_methods,
         dfunc_methods, dvar_methods, dmemloc_methods, dglobal_methods,
         dctn_methods, dobj_methods };

   int i, j;

   for (i = 0; i < 8; i++) {
      for (j = 0; j < methods_size[i]; j++) {
         if (methods[i][j].func == func) {
            va_list args;

            fprintf(stderr, "[!] [LUA in %s:%s] ", methods_name[i],
                  methods[i][j].name);

            if (format) {
               va_start(args, format);
               vfprintf(stderr, format, args);
               va_end(args);
            }

            fprintf(stderr, "\n");
            fflush(stderr);
         }
      }
   }
}

