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
 * \file ao_arch.c
 *
 * \date 30 nov. 2016
 */


#include "abstract_objects_c.h"
#include "arch.h"
#include "libmasm.h"

/******************************************************************/
/*                Functions dealing with arch                     */
/******************************************************************/

static int l_arch_get_endianness(lua_State* L)
{
   l_arch_t *l_arch = luaL_checkudata(L, 1, ARCH);
   lua_pushinteger(L, arch_get_endianness(l_arch->p));
   return 1;
}

static int l_arch_get_name(lua_State* L)
{
   l_arch_t *l_arch = luaL_checkudata(L, 1, ARCH);
   lua_pushstring(L, arch_get_name(l_arch->p));
   return 1;
}

static int l_arch_get_code(lua_State* L)
{
   l_arch_t *l_arch = luaL_checkudata(L, 1, ARCH);
   lua_pushinteger(L, arch_get_code(l_arch->p));
   return 1;
}

static int l_arch_get_nb_isets(lua_State* L)
{
   l_arch_t *l_arch = luaL_checkudata(L, 1, ARCH);
   lua_pushinteger(L, arch_get_nb_isets(l_arch->p));
   return 1;
}

static int l_arch_get_iset_name(lua_State* L)
{
   l_arch_t *l_arch = luaL_checkudata(L, 1, ARCH);
   unsigned int iset = luaL_checkinteger(L, 2);
   lua_pushstring(L, arch_get_iset_name(l_arch->p, iset));
   return 1;
}

static int l_arch_get_uarch_by_id(lua_State* L)
{
   l_arch_t *l_arch = luaL_checkudata(L, 1, ARCH);
   uint16_t uarch_id = luaL_checkinteger(L, 2);
   uarch_t* uarch = arch_get_uarch_by_id(l_arch->p, uarch_id);
   if (uarch != NULL) {
      create_uarch(L, uarch);
      return 1;
   }
   return 0;
}

static int l_arch_get_uarch_by_name(lua_State* L)
{
   l_arch_t *l_arch = luaL_checkudata(L, 1, ARCH);
   const char* uarch_name = luaL_checkstring(L, 2);
   uarch_t* uarch = arch_get_uarch_by_name(l_arch->p, (char*)uarch_name);
   if (uarch != NULL) {
      create_uarch(L, uarch);
      return 1;
   }
   return 0;
}

static int l_arch_get_proc_by_id(lua_State* L)
{
   l_arch_t *l_arch = luaL_checkudata(L, 1, ARCH);
   uint16_t proc_id = luaL_checkinteger(L, 2);
   proc_t* proc = arch_get_proc_by_id(l_arch->p, proc_id);
   if (proc != NULL) {
      create_proc(L, proc);
      return 1;
   }
   return 0;
}

static int l_arch_get_proc_by_name(lua_State* L)
{
   l_arch_t *l_arch = luaL_checkudata(L, 1, ARCH);
   const char* proc_name = luaL_checkstring(L, 2);
   proc_t* proc = arch_get_proc_by_name(l_arch->p, (char*)proc_name);
   if (proc != NULL) {
      create_proc(L, proc);
      return 1;
   }
   return 0;
}

static int l_arch_get_uarch_default_proc(lua_State* L)
{
   l_arch_t *l_arch = luaL_checkudata(L, 1, ARCH);
   l_uarch_t* l_uarch = luaL_checkudata(L, 2, UARCH);
   proc_t* proc = arch_get_uarch_default_proc(l_arch->p, l_uarch->p);
   if (proc != NULL) {
      create_proc(L, proc);
      return 1;
   }
   return 0;
}

static int l_arch_get_procs(lua_State* L)
{
   l_arch_t *l_arch = luaL_checkudata(L, 1, ARCH);
   proc_t** procs = arch_get_procs(l_arch->p);
   if (procs != NULL) {
      uint16_t i, j = 1;
      lua_newtable(L);
      for (i = 1; i < arch_get_nb_procs(l_arch->p); i++) {
         //Starting at 1 as the first proc in an arch is always NULL (<arch>_PROC_NONE)
         if (procs[i] != NULL) {
            create_proc(L, procs[i]);
            lua_rawseti(L, -2, j++);
         }
      }
   } else {
      lua_pushnil(L);
   }
   return 1;
}

static int l_arch_get_procs_from_iset(lua_State* L)
{
   l_arch_t *l_arch = luaL_checkudata(L, 1, ARCH);
   unsigned int iset = luaL_checkinteger(L, 2);
   uint16_t nb_procs;
   proc_t** procs = arch_get_procs_from_iset(l_arch->p, iset, &nb_procs);
   if (procs != NULL) {
      uint16_t i;
      lua_newtable(L);
      for (i = 0; i < nb_procs; i++) {
         create_proc(L, procs[i]);
         lua_rawseti(L, -2, i + 1);
      }
      lc_free(procs);
   } else {
      lua_pushnil(L);
   }
   return 1;
}

static int l_arch_get_uarchs(lua_State* L)
{
   l_arch_t *l_arch = luaL_checkudata(L, 1, ARCH);
   uarch_t** uarchs = arch_get_uarchs(l_arch->p);
   if (uarchs != NULL) {
      uint16_t i, j = 1;
      lua_newtable(L);
      for (i = 1; i < arch_get_nb_uarchs(l_arch->p); i++) {
         //Starting at 1 as the first uarch in an arch is always NULL (<arch>_UARCH_NONE)
         if (uarchs[i] != NULL) {
            create_uarch(L, uarchs[i]);
            lua_rawseti(L, -2, j++);
         }
      }
   } else {
      lua_pushnil(L);
   }
   return 1;
}

static int l_arch_get_uarchs_from_iset(lua_State* L)
{
   l_arch_t *l_arch = luaL_checkudata(L, 1, ARCH);
   unsigned int iset = luaL_checkinteger(L, 2);
   uint16_t nb_uarchs;
   uarch_t** uarchs = arch_get_uarchs_from_iset(l_arch->p, iset, &nb_uarchs);
   if (uarchs != NULL) {
      uint16_t i;
      lua_newtable(L);
      for (i = 0; i < nb_uarchs; i++) {
         create_uarch(L, uarchs[i]);
         lua_rawseti(L, -2, i + 1);
      }
      lc_free(uarchs);
   } else {
      lua_pushnil(L);
   }
   return 1;
}


static int arch_gc(lua_State * L)
{
   (void) L;
   return 0;
}

static int arch_tostring(lua_State * L)
{
   l_arch_t *r = luaL_checkudata(L, 1, ARCH);
   lua_pushfstring(L, "Arch: %s", arch_get_name(r->p));
   return 1;
}

/**
 * Bind names from this file to LUA
 * \see previous occurrence of this message
 */
const luaL_reg arch_methods[] = {
   {"get_endianness"          , l_arch_get_endianness},
   {"get_name"                , l_arch_get_name},
   {"get_code"                , l_arch_get_code},
   {"get_nb_isets"            , l_arch_get_nb_isets},
   {"get_iset_name"           , l_arch_get_iset_name},
   {"get_uarch_by_id"         , l_arch_get_uarch_by_id},
   {"get_uarch_by_name"       , l_arch_get_uarch_by_name},
   {"get_proc_by_id"          , l_arch_get_proc_by_id},
   {"get_proc_by_name"        , l_arch_get_proc_by_name},
   {"get_uarch_default_proc"  , l_arch_get_uarch_default_proc},
   {"get_procs"               , l_arch_get_procs},
   {"get_uarchs"              , l_arch_get_uarchs},
   {"get_procs_from_iset"     , l_arch_get_procs_from_iset},
   {"get_uarchs_from_iset"    , l_arch_get_uarchs_from_iset},
   {NULL, NULL}
};

const luaL_reg arch_meta[] = {
   {"__gc"      , arch_gc},
   {"__tostring", arch_tostring},
   {NULL, NULL}
};


/******************************************************************/
/*                Functions dealing with uarch                    */
/******************************************************************/

static int l_uarch_get_arch(lua_State* L)
{
   l_uarch_t *l_uarch = luaL_checkudata(L, 1, UARCH);
   arch_t *arch = uarch_get_arch(l_uarch->p);
   if (arch != NULL) {
      create_arch(L, arch);
      return 1;
   }
   return 0;
}

static int l_uarch_get_display_name(lua_State* L)
{
   l_uarch_t *l_uarch = luaL_checkudata(L, 1, UARCH);
   lua_pushstring(L, uarch_get_display_name(l_uarch->p));
   return 1;
}

static int l_uarch_get_name(lua_State* L)
{
   l_uarch_t *l_uarch = luaL_checkudata(L, 1, UARCH);
   lua_pushstring(L, uarch_get_name(l_uarch->p));
   return 1;
}

static int l_uarch_get_alias(lua_State* L)
{
   l_uarch_t *l_uarch = luaL_checkudata(L, 1, UARCH);
   lua_pushstring(L, uarch_get_alias(l_uarch->p));
   return 1;
}

static int l_uarch_get_procs(lua_State* L)
{
   l_uarch_t *l_uarch = luaL_checkudata(L, 1, UARCH);
   proc_t** procs = uarch_get_procs(l_uarch->p);
   if (procs != NULL) {
      uint16_t i;
      lua_newtable(L);
      for (i = 0; i < uarch_get_nb_procs(l_uarch->p); i++) {
         create_proc(L, procs[i]);
         lua_rawseti(L, -2, i + 1);
      }
   } else {
      lua_pushnil(L);
   }
   return 1;
}

static int l_uarch_get_id(lua_State* L)
{
   l_uarch_t *l_uarch = luaL_checkudata(L, 1, UARCH);
   lua_pushinteger(L, uarch_get_id(l_uarch->p));
   return 1;
}

static int l_uarch_get_isets(lua_State* L)
{
   l_uarch_t *l_uarch = luaL_checkudata(L, 1, UARCH);
   uint16_t i, nb_isets;
   uint8_t* isets = uarch_get_isets(l_uarch->p, &nb_isets);
   lua_newtable(L);
   for (i = 0; i < nb_isets; i++) {
      lua_pushinteger(L, isets[i]);
      lua_rawseti(L, -2, i + 1);
   }
   lc_free(isets);
   return 1;
}

static int uarch_gc(lua_State * L)
{
   (void) L;
   return 0;
}

static int uarch_tostring(lua_State * L)
{
   l_uarch_t *r = luaL_checkudata(L, 1, UARCH);
   lua_pushfstring(L, "Uarch: %s", uarch_get_name(r->p));
   return 1;
}

/**
 * Bind names from this file to LUA
 * \see previous occurrence of this message
 */
const luaL_reg uarch_methods[] = {
   {"get_arch"          , l_uarch_get_arch},
   {"get_display_name"  , l_uarch_get_display_name},
   {"get_name"          , l_uarch_get_name},
   {"get_alias"         , l_uarch_get_alias},
   {"get_procs"         , l_uarch_get_procs},
   {"get_id"            , l_uarch_get_id},
   {"get_isets"         , l_uarch_get_isets},
   {NULL, NULL}
};

const luaL_reg uarch_meta[] = {
   {"__gc"      , uarch_gc},
   {"__tostring", uarch_tostring},
   {NULL, NULL}
};

/******************************************************************/
/*                Functions dealing with proc                     */
/******************************************************************/


static int l_proc_get_uarch(lua_State* L)
{
   l_proc_t *l_proc = luaL_checkudata(L, 1, PROC);
   uarch_t *uarch = proc_get_uarch(l_proc->p);
   if (uarch != NULL) {
      create_uarch(L, uarch);
      return 1;
   }
   return 0;
}

static int l_proc_get_name(lua_State* L)
{
   l_proc_t *l_proc = luaL_checkudata(L, 1, PROC);
   lua_pushstring(L, proc_get_name(l_proc->p));
   return 1;
}

static int l_proc_get_display_name(lua_State* L)
{
   l_proc_t *l_proc = luaL_checkudata(L, 1, PROC);
   lua_pushstring(L, proc_get_display_name(l_proc->p));
   return 1;
}

/**\todo (2016-11-30) Write this function taking into account the fact that the cpuid code
 * may be stored as a different structure depending on the architecture. The (commented)
 * code below assumes it was stored on an int64_t, which is the case for the Intel architectures
 * as of now, but may have to change in the future*/
//static int l_proc_get_cpuid_code(lua_State* L)
//{
//   l_proc_t *l_proc = luaL_checkudata(L, 1, PROC);
//   void* code = proc_get_cpuid_code(l_proc->p);
//   if (code != NULL) {
//      lua_pushinteger(L, *(int64_t*)code);
//      return 1;
//   } else
//      return 0;
//}

static int l_proc_get_isets(lua_State* L)
{
   l_proc_t *l_proc = luaL_checkudata(L, 1, PROC);
   uint16_t i;
   uint8_t* isets = proc_get_isets(l_proc->p);
   lua_newtable(L);
   for (i = 0; i < proc_get_nb_isets(l_proc->p); i++) {
      lua_pushinteger(L, isets[i]);
      lua_rawseti(L, -2, i + 1);
   }
   return 1;
}

static int l_proc_get_id(lua_State* L)
{
   l_proc_t *l_proc = luaL_checkudata(L, 1, PROC);
   lua_pushinteger(L, proc_get_id(l_proc->p));
   return 1;
}

static int proc_gc(lua_State * L)
{
   (void) L;
   return 0;
}

static int proc_tostring(lua_State * L)
{
   l_proc_t *r = luaL_checkudata(L, 1, PROC);
   lua_pushfstring(L, "Proc: %s", proc_get_name(r->p));
   return 1;
}

/**
 * Bind names from this file to LUA
 * \see previous occurrence of this message
 */
const luaL_reg proc_methods[] = {
   {"get_uarch"          , l_proc_get_uarch},
   {"get_name"           , l_proc_get_name},
   {"get_display_name"   , l_proc_get_display_name},
//   {"get_cpuid_code"     , l_proc_get_cpuid_code},
   {"get_isets"          , l_proc_get_isets},
   {"get_id"             , l_proc_get_id},
   {NULL, NULL}
};

const luaL_reg proc_meta[] = {
   {"__gc"      , proc_gc},
   {"__tostring", proc_tostring},
   {NULL, NULL}
};
