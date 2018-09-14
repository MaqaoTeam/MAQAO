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

#ifndef __ABSTRACT_OBJECTS_H__
#define __ABSTRACT_OBJECTS_H__

#include <lua.h>           // declares lua_*  functions
#include <lauxlib.h>       // declares luaL_* functions
#include "libmasm.h"
#include "classes_c.h"

// Internal name of abstract objects
#define PROJECT   "project"
#define ASMFILE   "asmfile"
#define ARCH      "arch"
#define UARCH     "uarch"
#define PROC      "proc"
#define FUNCTION  "fct"
#define LOOP      "loop"
#define GROUP     "group"
#define BLOCK     "block"
#define INSN      "insn"

/**\todo (2016-11-30) For better clarity, rename all these objects p_t, a_t, ... into l_project_t, l_asmfile_t, etc*/
// Ease use of LUA objects
typedef struct {
   project_t *p;
   int must_be_freed;   // TRUE if project_free must be called on p during garbage-collection
   char* uarch_name;           // assumed common for all asmfiles
} p_t;

typedef struct {
   asmfile_t* p;
} a_t;

typedef struct {
   arch_t* p;
} l_arch_t;

typedef struct {
   uarch_t* p;
} l_uarch_t;

typedef struct {
   proc_t* p;
} l_proc_t;

typedef struct {
   fct_t* p;
} f_t;

typedef struct {
   loop_t* p;
} l_t;

typedef struct {
   group_t *p;
} g_t;

typedef struct {
   block_t *p;
} b_t;

typedef struct {
   insn_t *p;
} i_t;

//
extern const luaL_reg project_methods[];
extern const luaL_reg project_meta[];

extern const luaL_reg asmfile_methods[];
extern const luaL_reg asmfile_meta[];

extern const luaL_reg arch_methods[];
extern const luaL_reg arch_meta[];

extern const luaL_reg uarch_methods[];
extern const luaL_reg uarch_meta[];

extern const luaL_reg proc_methods[];
extern const luaL_reg proc_meta[];

extern const luaL_reg function_methods[];
extern const luaL_reg function_meta[];

extern const luaL_reg loop_methods[];
extern const luaL_reg loop_meta[];

extern const luaL_reg insn_methods[];
extern const luaL_reg insn_meta[];

extern const luaL_reg block_methods[];
extern const luaL_reg block_meta[];

extern const luaL_reg group_methods[];
extern const luaL_reg group_meta[];

//
extern p_t *create_project(lua_State * L, project_t *project, int must_be_freed);
extern a_t *create_asmfile(lua_State * L, asmfile_t *asmfile);
extern l_arch_t *create_arch(lua_State * L, arch_t *arch);
extern l_uarch_t *create_uarch(lua_State * L, uarch_t *uarch);
extern l_proc_t *create_proc(lua_State * L, proc_t *proc);
extern f_t *create_function(lua_State * L, fct_t *function);
extern l_t *create_loop(lua_State * L, loop_t *loop);
extern g_t *create_group(lua_State * L, group_t *group);
extern b_t *create_block(lua_State * L, block_t *block);
extern i_t *create_insn(lua_State * L, insn_t *insn);

extern int blocks_iter(lua_State * L);
extern int loop_is_dominant(loop_t *loop);
extern void _group_totable(lua_State * L, group_t* group, long user);

#endif
