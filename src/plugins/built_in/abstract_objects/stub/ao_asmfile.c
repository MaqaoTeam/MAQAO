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
#include "libmcore.h"
//#include "libmtroll.h"
#include "libmdbg.h"
#include "arch.h"

/******************************************************************/
/*                Functions dealing with asmfile                  */
/******************************************************************/

static int l_asmfile_get_project(lua_State * L)
{
   a_t *a = luaL_checkudata(L, 1, ASMFILE);
   project_t *project = asmfile_get_project(a->p);
   if (project != NULL) {
      create_project(L, project, FALSE);
      return 1;
   }
   return 0;
}

// TODO: think to rename to get_arch_code
static int l_asmfile_get_arch(lua_State * L)
{
   a_t *a = luaL_checkudata(L, 1, ASMFILE);

   lua_pushinteger(L, asmfile_get_arch_code(a->p));
   return 1;
}

/**\todo (2016-11-30) See above, rename get_arch into get_arch_code and get_arch_obj into get_arch*/
static int l_asmfile_get_arch_obj(lua_State * L)
{
   a_t *a = luaL_checkudata(L, 1, ASMFILE);
   arch_t* arch = asmfile_get_arch(a->p);
   if (arch != NULL) {
      create_arch(L, arch);
      return 1;
   }
   return 0;
}

static int l_asmfile_get_arch_registers(lua_State * L)
{
   a_t *a = luaL_checkudata(L, 1, ASMFILE);
   arch_t* arch = asmfile_get_arch(a->p);
   int (*_regID)(reg_t*, arch_t*) = &__regID;

   if (arch->code == ARCH_arm64) {
#ifdef _ARCHDEF_arm64
      _regID = &arm64_cs_regID;
#else
      return 0;
#endif
   }

   lua_newtable(L);
   if (arch != NULL) {
      int i = 0, j = 0;
      for (i = 0; i < lcore_get_nb_registers(arch); i++) {
         lua_pushinteger(L, -1);
         lua_rawseti(L, -2, (i + 1));
      }
      for (i = 0; i < arch->nb_type_registers; i++) {
         for (j = 0; j < arch->nb_names_registers; j++) {
            if (arch->regs[i][j] != NULL) {
               lua_pushinteger(L, 0);
               lua_rawseti(L, -2, _regID(arch->regs[i][j], arch));
            }
         }
      }
      return 1;
   }
   return 0;
}

static int l_asmfile_get_arg_registers(lua_State * L)
{
   asmfile_t* asmfile = ((a_t *) luaL_checkudata(L, 1, ASMFILE))->p;
   arch_t* arch = asmfile->arch;
   if (asmfile != NULL) {
      int i;
      lua_newtable(L);

      for (i = 0; i < arch->nb_arg_regs; i++) {
         lua_pushlightuserdata(L, arch->arg_regs[i]);
         lua_rawseti(L, -2, (i + 1));
      }
      return 1;
   }
   return 0;
}

static int l_asmfile_get_ret_registers(lua_State * L)
{
   asmfile_t* asmfile = ((a_t *) luaL_checkudata(L, 1, ASMFILE))->p;
   arch_t* arch = asmfile->arch;
   if (asmfile != NULL) {
      int i;
      lua_newtable(L);
      for (i = 0; i < arch->nb_return_regs; i++) {
         lua_pushlightuserdata(L, arch->return_regs[i]);
         lua_rawseti(L, -2, (i + 1));
      }
      return 1;
   }
   return 0;
}

static int l_asmfile_get_register_from_id(lua_State * L)
{
   asmfile_t* asmfile = ((a_t *) luaL_checkudata(L, 1, ASMFILE))->p;
   int id = luaL_checkinteger(L, 2);
   arch_t* arch = asmfile->arch;
   reg_t* reg = NULL;

   if (arch->code == ARCH_arm64) {
#ifdef _ARCHDEF_arm64
      reg = arm64_cs_IDreg(id,asmfile->arch);
#else
      return 0;
#endif
   } 

   if (reg != NULL) {
      lua_pushlightuserdata(L, reg);
      return 1;
   }

   return 0;
}

static int l_asmfile_get_register_name(lua_State * L)
{
   asmfile_t* asmfile = ((a_t *) luaL_checkudata(L, 1, ASMFILE))->p;
   reg_t* reg = lua_touserdata(L, 2);
   if (reg != NULL) {
      lua_pushstring(L,
            arch_get_reg_name(asmfile->arch, reg_get_type(reg),
                  reg_get_name(reg)));
      return 1;
   }
   return 0;
}

static int l_asmfile_get_register_name_from_id(lua_State * L)
{
   asmfile_t* asmfile = ((a_t *) luaL_checkudata(L, 1, ASMFILE))->p;
   int id = luaL_checkinteger(L, 2);
   arch_t* arch = asmfile->arch;
   reg_t* reg = NULL;

   if (arch->code == ARCH_arm64) {
#ifdef _ARCHDEF_arm64
      reg = arm64_cs_IDreg(id,asmfile->arch);
#else
      return 0;
#endif
   } 

   if (reg != NULL) {
      lua_pushstring(L,
            arch_get_reg_name(asmfile->arch, reg_get_type(reg),
                  reg_get_name(reg)));
      return 1;
   }
   return 0;
}

static int l_asmfile_get_register_fam(lua_State * L)
{
   asmfile_t* asmfile = ((a_t *) luaL_checkudata(L, 1, ASMFILE))->p;
   reg_t* reg = lua_touserdata(L, 2);
   if (reg != NULL) {
      lua_pushinteger(L,
            asmfile->arch->reg_families[(unsigned char) reg_get_type(reg)]);
      return 1;
   }
   return 0;
}

static int l_asmfile_get_register_fam_from_id(lua_State * L)
{
   asmfile_t* asmfile = ((a_t *) luaL_checkudata(L, 1, ASMFILE))->p;
   int id = luaL_checkinteger(L, 2);
   arch_t* arch = asmfile->arch;
   reg_t* reg = NULL;

   if (arch->code == ARCH_arm64) {
#ifdef _ARCHDEF_arm64
      reg = arm64_cs_IDreg(id,asmfile->arch);
#else
      return 0;
#endif
   } 

   if (reg != NULL) {
      lua_pushinteger(L,
            asmfile->arch->reg_families[(unsigned char) reg_get_type(reg)]);
      return 1;
   }
   return 0;
}

static int l_asmfile_get_arch_families(lua_State * L)
{
   a_t *a = luaL_checkudata(L, 1, ASMFILE);
   arch_t* archi = asmfile_get_arch(a->p);
   lua_newtable(L);
   if (archi != NULL) {
      int i = 0;

      for (i = 0; i < archi->nb_type_registers; i++) {
         lua_pushinteger(L, archi->reg_families[i]);
         lua_rawseti(L, -2, (i + 1));
      }
      return 1;
   }
   return 0;
}

static int l_asmfile_get_name(lua_State * L)
{
   a_t *a = luaL_checkudata(L, 1, ASMFILE);
   lua_pushstring(L, asmfile_get_name(a->p));
   return 1;
}

static int l_asmfile_get_arch_name(lua_State * L)
{
   a_t *a = luaL_checkudata(L, 1, ASMFILE);
   const char *arch_name = asmfile_get_arch_name(a->p);
   if (arch_name != NULL) {
      lua_pushstring(L, arch_name);
      return 1;
   }
   return 0;
}

static int l_asmfile_get_proc(lua_State * L)
{
   a_t *a = luaL_checkudata(L, 1, ASMFILE);
   proc_t* proc = asmfile_get_proc(a->p);
   if (proc != NULL) {
      create_proc(L, proc);
      return 1;
   }
   return 0;
}

static int l_asmfile_get_uarch_id(lua_State * L)
{
   a_t *a = luaL_checkudata(L, 1, ASMFILE);
   lua_pushinteger(L, uarch_get_id(proc_get_uarch(asmfile_get_proc(a->p))));
   return 1;
}

static int l_asmfile_get_uarch_name(lua_State * L)
{
   a_t *a = luaL_checkudata(L, 1, ASMFILE);

   if (a != NULL) {
      lua_pushstring(L, asmfile_get_uarch_name(a->p));
      return 1;
   }
   return 0;
}

static int l_asmfile_get_hash(lua_State * L)
{
   a_t *a = luaL_checkudata(L, 1, ASMFILE);
   /* file_hash calls exit if error */
   lua_pushinteger(L, file_hash(asmfile_get_name(a->p)));
   return 1;
}

static int l_asmfile_get_nb_fcts(lua_State * L)
{
   a_t *a = luaL_checkudata(L, 1, ASMFILE);
   lua_pushinteger(L, asmfile_get_nb_fcts(a->p));
   return 1;
}

static int l_asmfile_get_nb_loops(lua_State * L)
{
   a_t *a = luaL_checkudata(L, 1, ASMFILE);
   lua_pushinteger(L, asmfile_get_nb_loops(a->p));
   return 1;
}

static int l_asmfile_get_nb_blocks(lua_State * L)
{
   a_t *a = luaL_checkudata(L, 1, ASMFILE);
   lua_pushinteger(L, asmfile_get_nb_blocks_novirtual(a->p));
   return 1;
}

static int l_asmfile_get_nb_insns(lua_State * L)
{
   a_t *a = luaL_checkudata(L, 1, ASMFILE);
   lua_pushinteger(L, asmfile_get_nb_insns(a->p));
   return 1;
}

static int l_asmfile_compute_post_dominance(lua_State* L)
{
   a_t *a = luaL_checkudata(L, 1, ASMFILE);
   lcore_analyze_post_dominance(a->p);
   return (0);
}

/**
 * This function is internally used by functions()
 */
static int functions_iter(lua_State * L)
{
   list_t **list = lua_touserdata(L, lua_upvalueindex(1));
   if ((list != NULL) && (*list != NULL)) {
      create_function(L, list_getdata(*list));
      *list = list_getnext(*list);
      return 1;
   }
   return 0;
}

static int l_asmfile_functions(lua_State * L)
{
   a_t *a = luaL_checkudata(L, 1, ASMFILE);
   queue_t *functions = asmfile_get_fcts(a->p);
   if (functions != NULL) {
      list_t **list = lua_newuserdata(L, sizeof *list);
      *list = queue_iterator(functions);
   } else {
      /* This case should never occur, even with an empty asmfile */
      lua_pushnil(L);
   }
   lua_pushcclosure(L, functions_iter, 1);
   return 1;
}

static int l_asmfile_get_fct_labels(lua_State * L)
{
   unsigned int nbFunctions = 0;
   a_t *asmfile = luaL_checkudata(L, 1, ASMFILE);
   label_t** labels = asmfile_get_fct_labels(asmfile->p, &nbFunctions);
   unsigned int i = 0;
   unsigned idx = 1;

   if (labels != NULL) {
      //Table A
      lua_newtable(L);
      for (i = 0; i < nbFunctions; i++) {
         lua_pushnumber(L, idx++);
         //Table B
         lua_newtable(L);
         lua_pushliteral(L, "fct_name");
         lua_pushstring(L, labels[i]->name);
         //B["fct_name"] = labels[i]->name
         lua_settable(L, -3);
         lua_pushliteral(L, "start_addr");
         lua_pushinteger(L, labels[i]->address);
         //B["start_addr"] = labels[i]->address
         lua_settable(L, -3);
         //A[idx] = B
         lua_settable(L, -3);
      }
      return 1;
   }
   return 0;
}

static int l_asmfile_get_last_label(lua_State * L)
{
   a_t *asmfile = luaL_checkudata(L, 1, ASMFILE);
   int64_t addr = luaL_checkinteger(L, 2);
   list_t** lastcontainer = NULL; //FIXME:handle this argument
   label_t* lbl;
   lbl = asmfile_get_last_label(asmfile->p, addr, lastcontainer);
   lua_pushlightuserdata(L, lbl);

   return 1;
}

static int l_label_get_name(lua_State * L)
{
   a_t *asmfile = luaL_checkudata(L, 1, ASMFILE);
   (void) asmfile;
   label_t* lbl = lua_touserdata(L, 2);
   char* name = label_get_name(lbl);
   lua_pushstring(L, name);
   return 1;
}

static int l_label_get_addr(lua_State * L)
{
   a_t *asmfile = luaL_checkudata(L, 1, ASMFILE);
   (void) asmfile;
   label_t* lbl = lua_touserdata(L, 2);
   int64_t addr = label_get_addr(lbl);
   lua_pushinteger(L, addr);
   return 1;
}

/**
 * Checks if a label is of type patched
 * */
static int l_label_ispatched(lua_State * L)
{
   a_t *asmfile = luaL_checkudata(L, 1, ASMFILE);
   (void) asmfile;
   label_t* lbl = lua_touserdata(L, 2);
   lua_pushboolean(L, (label_get_type(lbl) == LBL_PATCHSCN));
   return 1;
}

/**
 * This function is internally used by fct_plt_iter()
 */
static int fct_plt_iter(lua_State * L)
{
   list_t** list = lua_touserdata(L, lua_upvalueindex(1));
   if ((list != NULL) && (*list != NULL)) {
      fct_t* function = list_getdata(*list);
      create_function(L, function);
      *list = list_getnext(*list);

      return 1;
   }
   return 0;
}

static int l_asmfile_get_fct_plt(lua_State * L)
{
   a_t *a = luaL_checkudata(L, 1, ASMFILE);
   list_t *plt_functions = asmfile_get_fct_plt(a->p);
   if (plt_functions != NULL) {
      list_t** list = lua_newuserdata(L, sizeof *list);
      *list = plt_functions;
   } else {
      /* This case should never occur, even with an empty asmfile */
      lua_pushnil(L);
   }
   lua_pushcclosure(L, fct_plt_iter, 1);
   return 1;
}

static int l_asmfile_get_insn_by_addr(lua_State *L)
{
   a_t *a = luaL_checkudata(L, 1, ASMFILE);
   int64_t addr = luaL_checkinteger(L, 2);
   insn_t* insn = asmfile_get_insn_by_addr(a->p, addr);
   if (insn != NULL)
      create_insn(L, insn);
   else
      lua_pushnil(L);
   return 1;
}

static int l_asmfile_analyze_compile_options(lua_State * L)
{
   a_t *asmfile = luaL_checkudata(L, 1, ASMFILE);
   char* opts = NULL;

   if (asmfile != NULL)
	   opts = asmfile_get_compile_options(asmfile->p);

   if (opts == NULL)
      lua_pushstring(L, "");
   else
      lua_pushstring(L, opts);

   return (1);
}

static int l_asmfile_set_option(lua_State * L)
{
   a_t *asmfile = luaL_checkudata(L, 1, ASMFILE);
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
   asmfile_add_parameter(asmfile->p, module_id, param_id, value);
   return (0);
}

static int l_asmfile_get_boolean_option(lua_State * L)
{
   a_t *asmfile = luaL_checkudata(L, 1, ASMFILE);
   int module_id = luaL_checkinteger(L, 2);
   int param_id = luaL_checkinteger(L, 3);
   void* value = asmfile_get_parameter(asmfile->p, module_id, param_id);

   if (value == NULL)
      lua_pushboolean(L, FALSE);
   else
      lua_pushboolean(L, (long int) value);
   return (1);
}

static int l_asmfile_get_int_option(lua_State * L)
{
   a_t *asmfile = luaL_checkudata(L, 1, ASMFILE);
   int module_id = luaL_checkinteger(L, 2);
   int param_id = luaL_checkinteger(L, 3);
   void* value = asmfile_get_parameter(asmfile->p, module_id, param_id);

   if (value == NULL)
      lua_pushinteger(L, 0);
   else
      lua_pushinteger(L, (long int) value);
   return (1);
}

static int l_asmfile_get_string_option(lua_State * L)
{
   a_t *asmfile = luaL_checkudata(L, 1, ASMFILE);
   int module_id = luaL_checkinteger(L, 2);
   int param_id = luaL_checkinteger(L, 3);
   void* value = asmfile_get_parameter(asmfile->p, module_id, param_id);

   if (value == NULL)
      lua_pushnil(L);
   else
      lua_pushstring(L, (char*) value);
   return (1);
}

static int l_asmfile_get_string_from_file(lua_State* L)
{
   a_t *asmfile = luaL_checkudata(L, 1, ASMFILE);
   int64_t addr = luaL_checkinteger(L, 2);

   if (asmfile == NULL)
      lua_pushnil(L);
   else {
      binfile_t* bf = asmfile_get_binfile(asmfile->p);
      binscn_t* scn = binfile_lookup_scn_span_addr(bf, addr);
      int len = 0;
      int64_t startaddr = binscn_get_addr(scn);
      unsigned char* bytes = binscn_get_data_at_offset(scn,
            addr - (int64_t) startaddr);
      lua_pushstring(L, (char*) &(bytes[addr - startaddr]));
   }
   return (1);
}

static int asmfile_gc(lua_State * L)
{
   (void) L;
   return 0;
}

static int asmfile_tostring(lua_State * L)
{
   a_t *a = luaL_checkudata(L, 1, ASMFILE);
   lua_pushfstring(L, "Asmfile: %s", asmfile_get_name(a->p));
   return 1;
}

/**
 * Bind names from this file to LUA
 * \see previous occurence of this message
 */
const luaL_reg asmfile_methods[] = {
   {"get_project"                , l_asmfile_get_project},
   {"get_name"                   , l_asmfile_get_name},
   {"get_arch"                   , l_asmfile_get_arch},
   {"get_arch_obj"               , l_asmfile_get_arch_obj},
   {"get_arch_name"              , l_asmfile_get_arch_name},
   {"get_proc"                   , l_asmfile_get_proc},
   {"get_uarch_id"               , l_asmfile_get_uarch_id},
   {"get_uarch_name"             , l_asmfile_get_uarch_name},
   {"get_hash"                   , l_asmfile_get_hash},
   {"get_nfunctions"             , l_asmfile_get_nb_fcts},
   {"get_nloops"                 , l_asmfile_get_nb_loops},
   {"get_nblocks"                , l_asmfile_get_nb_blocks},
   {"get_ninsns"                 , l_asmfile_get_nb_insns},
   {"compute_post_dominance"     , l_asmfile_compute_post_dominance},
   {"functions"                  , l_asmfile_functions},
   {"get_fct_labels"             , l_asmfile_get_fct_labels},
   {"get_lastlabel"              , l_asmfile_get_last_label},
   {"label_get_name"             , l_label_get_name},
   {"label_get_addr"             , l_label_get_addr},
   {"label_ispatched"            , l_label_ispatched},
   {"get_fct_plt"                , l_asmfile_get_fct_plt},
   {"getinsn_byaddress"          , l_asmfile_get_insn_by_addr},
   {"get_arg_registers"          , l_asmfile_get_arg_registers},
   {"get_ret_registers"          , l_asmfile_get_ret_registers},
   {"get_arch_registers"         , l_asmfile_get_arch_registers},
   {"get_arch_families"          , l_asmfile_get_arch_families},
   {"get_register_name_from_id"  , l_asmfile_get_register_name_from_id},
   {"get_register_fam_from_id"   , l_asmfile_get_register_fam_from_id},
   {"get_register_name"          , l_asmfile_get_register_name},
   {"get_register_fam"           , l_asmfile_get_register_fam},
   {"get_register_from_id"       , l_asmfile_get_register_from_id},
   {"analyze_compile_options"    , l_asmfile_analyze_compile_options},
   {"set_option"                 , l_asmfile_set_option},
   {"get_boolean_option"         , l_asmfile_get_boolean_option},
   {"get_int_option"             , l_asmfile_get_int_option},
   {"get_string_option"          , l_asmfile_get_string_option},
   {"get_string_from_file"       , l_asmfile_get_string_from_file},
   {NULL, NULL}
};

const luaL_reg asmfile_meta[] = {
   {"__gc"      , asmfile_gc},
   {"__tostring", asmfile_tostring},
   {NULL, NULL}
};
