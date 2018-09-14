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

#include "libextends.h"
#include "libmcore.h"
#include "assembler.h"
#include "libmdisass.h"
#include "abstract_objects_c.h"

extern int l_insn_get_registers_name(lua_State * L);
extern int l_insn_get_registers_type(lua_State * L);
extern int l_insn_get_registers_id(lua_State * L);
extern int l_insn_get_operands(lua_State *L);
extern int l_insn_get_operand_ptr(lua_State *L);
extern int l_insn_get_registers_rw(lua_State *L);
extern int l_insn_get_first_mem_oprnd(lua_State *L);
extern int l_insn_has_src_mem_oprnd(lua_State *L);
extern int l_insn_has_dst_mem_oprnd(lua_State *L);
extern int l_insn_get_oprnd_src_index(lua_State *L);
extern int l_insn_get_oprnd_dst_index(lua_State *L);
extern int l_insn_get_nb_oprndss(lua_State * L);
extern int l_insn_get_oprnd_int(lua_State * L);
extern int l_insn_get_oprnd_str(lua_State * L);
extern int l_insn_get_oprnd_type(lua_State * L);
extern int l_insn_get_rip_oprnd_dest(lua_State * L);
extern int l_insn_is_oprnd_mem(lua_State * L);
extern int l_insn_is_oprnd_reg(lua_State * L);
extern int l_insn_is_oprnd_imm(lua_State * L);

extern arch_t* getarch_byname(char* archname);

/******************************************************************/
/*                Functions dealing with Insn                     */
/******************************************************************/

static int l_insn_get_label_name(lua_State * L)
{
   i_t *i = luaL_checkudata(L, 1, INSN);
   char *name = label_get_name(insn_get_fctlbl(insn_get_branch(i->p)));

   if (name != NULL) {
      lua_pushstring(L, name);

      return 1;
   }

   return 0;
}

static int l_insn_get_project(lua_State * L)
{
   i_t *i = luaL_checkudata(L, 1, INSN);
   project_t *project = insn_get_project(i->p);

   if (project != NULL) {
      create_project(L, project, FALSE);

      return 1;
   }

   return 0;
}

static int l_insn_get_asmfile(lua_State * L)
{
   i_t *i = luaL_checkudata(L, 1, INSN);
   asmfile_t *asmfile = insn_get_asmfile(i->p);

   if (asmfile != NULL) {
      create_asmfile(L, asmfile);

      return 1;
   }

   return 0;
}

static int l_insn_get_function(lua_State * L)
{
   i_t *i = luaL_checkudata(L, 1, INSN);
   fct_t *function = insn_get_fct(i->p);

   if (function != NULL) {
      create_function(L, function);

      return 1;
   }

   return 0;
}

static int l_insn_get_loop(lua_State * L)
{
   i_t *i = luaL_checkudata(L, 1, INSN);
   loop_t *loop = insn_get_loop(i->p);

   if (loop != NULL) {
      create_loop(L, loop);

      return 1;
   }

   return 0;
}

static int l_insn_get_block(lua_State * L)
{
   i_t *i = luaL_checkudata(L, 1, INSN);

   create_block(L, insn_get_block(i->p));

   return 1;
}

static void link_insn_to_asmfile(insn_t *insn, asmfile_t *asmf)
{
   fct_t *fct = lc_malloc0(sizeof *fct);
   fct->asmfile = asmf;

   block_t *block = lc_malloc0(sizeof *block);
   block->function = fct;

   insn->block = block;
}

/* Creates an instruction from an ASM code. Architecture is retrieved from any instruction */
static int l_insn_parsenew(lua_State * L)
{
   i_t *i = luaL_checkudata(L, 1, INSN);
   const char *asm_code = luaL_checkstring(L, 2);

   arch_t *arch = insn_get_arch(i->p);
   queue_t **insns = (queue_t**) lc_malloc0(sizeof(queue_t*));
   int assembling_status = assemble_strlist_forarch((char *) asm_code, arch,
         NULL, insns);

   if ((queue_length(*insns) == 0) || (assembling_status != EXIT_SUCCESS)) {
      queue_free(*insns, arch->insn_free);
      lc_free(insns);
      lua_pushnil(L);
      return 1;
   }

   int bin_stream_size;
   unsigned char *bin_stream = insnlist_getcoding(*insns, &bin_stream_size,
         NULL, NULL);
   queue_free(*insns, arch->insn_free);
   asmfile_t *asmfile = asmfile_new("foo");
   proc_t *proc = asmfile_get_proc(insn_get_asmfile(i->p));
   asmfile_set_proc(asmfile, proc);
   int ret = stream_disassemble(asmfile, bin_stream, bin_stream_size,
         insn_get_addr(i->p), arch, NULL);
   lc_free(bin_stream);
   if (ISERROR(ret)) {
      asmfile_free(asmfile);
      lua_pushnil(L);
      return 1;
   }

   *insns = asmfile_get_insns(asmfile);

   insn_t *new_insn = queue_peek_head(*insns);
   link_insn_to_asmfile(new_insn, asmfile);

   create_insn(L, new_insn);

   lc_free(insns);

   return 1;
}

static int l_insn_parsenew_fromscratch(lua_State * L)
{
   const char *asm_code = luaL_checkstring(L, 1);
   const char *arch_name = luaL_checkstring(L, 2);

   arch_t *arch = getarch_byname((char *) arch_name);
   insn_t *insn = insn_parsenew((char *) asm_code, arch);

   create_insn(L, insn);

   return 1;
}

/* Frees an instruction created by insn:parsenew */
static int l_insn_free_parsenew(lua_State * L)
{
   i_t *i = luaL_checkudata(L, 1, INSN);
   if (!i)
      return 0;

   insn_t *insn = i->p;
   if (!insn)
      return 0;

   asmfile_t *asmfile = insn->block->function->asmfile;

   lc_free(insn->block->function);
   lc_free(insn->block);
   asmfile_free(asmfile); /* frees the instruction itself */

   return 0;
}

static int l_insn_get_groups(lua_State * L)
{
   i_t *i = luaL_checkudata(L, 1, INSN);
   list_t *groups = insn_get_groups(i->p);
   if (groups == NULL)
      return 0;

   int cpt = 1;
   lua_newtable(L);

   FOREACH_INLIST(groups, it_g) {
      group_t* group = GET_DATA_T(group_t*, it_g);
      create_group(L, group);
      lua_rawseti(L, -2, cpt++);
   }

   return 1;
}

static int l_insn_get_first_group(lua_State * L)
{
   i_t *i = luaL_checkudata(L, 1, INSN);
   group_t *group = insn_get_first_group(i->p);

   if (group != NULL) {
      create_group(L, group);
      return 1;
   }

   return 0;
}

static int insngroups_iter(lua_State * L)
{
   list_t **list = lua_touserdata(L, lua_upvalueindex(1));

   if ((list != NULL) && (*list != NULL)) {
      group_t *g = list_getdata(*list);
      create_group(L, g);
      *list = list_getnext(*list);
      return 1;
   }

   return 0;
}

static int l_insn_groups(lua_State * L)
{
   i_t *i = luaL_checkudata(L, 1, INSN);
   list_t *groups = insn_get_groups(i->p);

   if (groups != NULL) {
      list_t **list = lua_newuserdata(L, sizeof *list);
      *list = groups;
   } else
      lua_pushnil(L);
   lua_pushcclosure(L, insngroups_iter, 1);

   return 1;
}

static int l_insn_get_address(lua_State * L)
{
   i_t *i = luaL_checkudata(L, 1, INSN);

   lua_pushinteger(L, insn_get_addr(i->p));

   return 1;
}

static int l_insn_get_name(lua_State * L)
{
   i_t *i = luaL_checkudata(L, 1, INSN);
   char *name = insn_get_opcode(i->p);

   if (name != NULL) {
      lua_pushstring(L, name);

      return 1;
   }

   return 0;
}

static int l_insn_get_src_line(lua_State * L)
{
   i_t *i = luaL_checkudata(L, 1, INSN);
   lua_pushinteger(L, insn_get_src_line(i->p));
   return 1;
}

static int l_insn_get_src_column(lua_State * L)
{
   i_t *i = luaL_checkudata(L, 1, INSN);
   lua_pushinteger(L, insn_get_src_col(i->p));
   return 1;
}

static int l_insn_get_src_file_path(lua_State * L)
{
   i_t *i = luaL_checkudata(L, 1, INSN);
   char *srcfile = insn_get_src_file(i->p);

   if (srcfile != NULL)
      lua_pushstring(L, srcfile);
   else
      lua_pushnil(L);

   return 1;
}

static int l_insn_get_class(lua_State *L)
{
   insn_t *insn = ((i_t *) luaL_checkudata(L, 1, INSN))->p;

   lua_pushinteger(L, insn_get_class(insn));

   return 1;
}

static int l_insn_get_element_size(lua_State *L)
{
   i_t *i = luaL_checkudata(L, 1, INSN);

   lua_pushinteger(L, insn_get_input_element_size(i->p));

   return 1;
}

static int l_insn_get_input_element_size(lua_State *L)
{
   i_t *i = luaL_checkudata(L, 1, INSN);

   lua_pushinteger(L, insn_get_input_element_size(i->p));

   return 1;
}

static int l_insn_get_output_element_size(lua_State *L)
{
   i_t *i = luaL_checkudata(L, 1, INSN);

   lua_pushinteger(L, insn_get_output_element_size(i->p));

   return 1;
}

static int l_insn_get_element_bits(lua_State *L)
{
   i_t *i = luaL_checkudata(L, 1, INSN);

   lua_pushinteger(L, datasz_getvalue(insn_get_input_element_size(i->p)));

   return 1;
}

static int l_insn_get_input_element_bits(lua_State *L)
{
   i_t *i = luaL_checkudata(L, 1, INSN);

   lua_pushinteger(L, datasz_getvalue(insn_get_input_element_size(i->p)));

   return 1;
}

static int l_insn_get_output_element_bits(lua_State *L)
{
   i_t *i = luaL_checkudata(L, 1, INSN);

   lua_pushinteger(L, datasz_getvalue(insn_get_output_element_size(i->p)));

   return 1;
}

static int l_insn_get_element_type(lua_State *L)
{
   insn_t *insn = ((i_t *) luaL_checkudata(L, 1, INSN))->p;

   lua_pushinteger(L, insn_get_input_element_type(insn));

   return 1;
}

static int l_insn_get_input_element_type(lua_State *L)
{
   insn_t *insn = ((i_t *) luaL_checkudata(L, 1, INSN))->p;

   lua_pushinteger(L, insn_get_input_element_type(insn));

   return 1;
}

static int l_insn_get_output_element_type(lua_State *L)
{
   insn_t *insn = ((i_t *) luaL_checkudata(L, 1, INSN))->p;

   lua_pushinteger(L, insn_get_output_element_type(insn));

   return 1;
}

static int l_insn_get_family(lua_State * L)
{
   insn_t *insn = ((i_t *) luaL_checkudata(L, 1, INSN))->p;

   lua_pushinteger(L, insn_get_family(insn));

   return 1;
}

static int l_insn_is_SIMD(lua_State *L)
{
   insn_t *insn = ((i_t *) luaL_checkudata(L, 1, INSN))->p;
   lua_pushboolean(L, insn_is_SIMD(insn));

   return 1;
}

static int l_insn_is_INT(lua_State *L)
{
   insn_t *insn = ((i_t *) luaL_checkudata(L, 1, INSN))->p;
   lua_pushboolean(L, insn_is_INT(insn));

   return 1;
}

static int l_insn_is_SIMD_INT(lua_State *L)
{
   insn_t *insn = ((i_t *) luaL_checkudata(L, 1, INSN))->p;
   lua_pushboolean(L, insn_is_SIMD_INT(insn));

   return 1;
}

static int l_insn_is_FP(lua_State *L)
{
   insn_t *insn = ((i_t *) luaL_checkudata(L, 1, INSN))->p;
   lua_pushboolean(L, insn_is_FP(insn));

   return 1;
}

static int l_insn_is_struct_or_str(lua_State *L)
{
   insn_t *insn = ((i_t *) luaL_checkudata(L, 1, INSN))->p;
   lua_pushboolean(L, insn_is_struct_or_str(insn));

   return 1;
}

static int l_insn_is_single_prec(lua_State *L)
{
   insn_t *insn = ((i_t *) luaL_checkudata(L, 1, INSN))->p;
   lua_pushboolean(L, insn_is_single_prec(insn));

   return 1;
}

static int l_insn_is_double_prec(lua_State *L)
{
   insn_t *insn = ((i_t *) luaL_checkudata(L, 1, INSN))->p;
   lua_pushboolean(L, insn_is_double_prec(insn));

   return 1;
}

static int l_insn_is_prefetch(lua_State *L)
{
   insn_t *insn = ((i_t *) luaL_checkudata(L, 1, INSN))->p;
   lua_pushboolean(L, insn_is_prefetch(insn));

   return 1;
}

static int l_insn_is_SIMD_FP(lua_State *L)
{
   insn_t *insn = ((i_t *) luaL_checkudata(L, 1, INSN))->p;
   lua_pushboolean(L, insn_is_SIMD_FP(insn));

   return 1;
}

static int l_insn_is_SIMD_NOT_FP(lua_State *L)
{
   insn_t *insn = ((i_t *) luaL_checkudata(L, 1, INSN))->p;
   lua_pushboolean(L, insn_is_SIMD_NOT_FP(insn));

   return 1;
}

static int l_insn_get_SIMD_width(lua_State *L)
{
   insn_t *insn = ((i_t *) luaL_checkudata(L, 1, INSN))->p;
   lua_pushinteger(L, insn_get_SIMD_width(insn));

   return 1;
}

static int l_insn_is_packed(lua_State *L)
{
   insn_t *insn = ((i_t *) luaL_checkudata(L, 1, INSN))->p;
   lua_pushboolean(L, insn_is_packed(insn));

   return 1;
}

static int l_insn_get_read_size(lua_State *L)
{
   insn_t *insn = ((i_t *) luaL_checkudata(L, 1, INSN))->p;

   lua_pushinteger(L, insn_get_read_size(insn));

   return 1;
}

static int l_insn_get_read_bits(lua_State *L)
{
   insn_t *insn = ((i_t *) luaL_checkudata(L, 1, INSN))->p;

   lua_pushinteger(L, datasz_getvalue(insn_get_read_size(insn)));

   return 1;
}

/**
 * Helper function, internally used for l_insn_get_dispatch()
 */
static void push_uint16_min_max(lua_State *L, uint16_min_max_t m)
{
   lua_newtable(L);

   lua_pushliteral(L, "min");
   lua_pushnumber(L, m.min);
   lua_settable(L, -3);

   lua_pushliteral(L, "max");
   lua_pushnumber(L, m.max);
   lua_settable(L, -3);

   lua_settable(L, -3);
}

/**
 * Helper function, internally used for l_insn_get_dispatch()
 */
static void push_float_min_max(lua_State *L, float_min_max_t m)
{
   lua_newtable(L);

   lua_pushliteral(L, "min");
   lua_pushnumber(L, m.min);
   lua_settable(L, -3);

   lua_pushliteral(L, "max");
   lua_pushnumber(L, m.max);
   lua_settable(L, -3);

   lua_settable(L, -3);
}

/**
 * Helper function, internally used for push_uops_groups ()
 */
static void push_units(lua_State *L, uops_group_t ug)
{
   int i;

   lua_newtable(L);

   /* For each port/unit of the current uops group */
   for (i = 0; i < ug.nb_units; i++) {
      lua_pushnumber(L, i + 1);
      lua_pushnumber(L, ug.units[i]);
      lua_settable(L, -3);
   }

   lua_settable(L, -3);
}

/**
 * Helper function, internally used for l_insn_get_dispatch()
 */
static void push_uops_groups(lua_State *L, intel_ooo_t *ext)
{
   int i;

   /* Create a table for the uops groups entry */
   lua_newtable(L);

   /* For each uops group */
   for (i = 0; i < ext->nb_uops_groups; i++) {
      uops_group_t ug = ext->uops_groups[i];

      lua_pushnumber(L, i + 1); /* key */
      lua_newtable(L); /* value (table) */

      /* Push the "nb_uops" entry */
      lua_pushliteral(L, "nb_uops"); /* key */
      lua_pushnumber(L, ug.nb_uops); /* value (integer) */
      lua_settable(L, -3);

      /* Push the "units" entry */
      lua_pushliteral(L, "units"); /* key */
      push_units(L, ug); /* value (table) */
      lua_settable(L, -3);
   }

   lua_settable(L, -3);
}

static int l_insn_get_dispatch(lua_State * L)
{
   i_t *i = luaL_checkudata(L, 1, INSN);

#if defined (_ARCHDEF_arm64)

   if (arch_get_code(insn_get_arch(i->p)) == ARCH_arm64)
   {

      arm64_ooo_t *ext = insn_get_ext(i->p);

      if (ext == NULL)
         return 0;

      /* Create a table for extension */
      lua_newtable(L);

      /* Push the "nb_uops" entry */
      lua_pushliteral(L, "nb_uops"); /* key */
      lua_pushnumber(L, ext->nb_uops); /* value */
      lua_settable(L, -3);

      /* Push the "dispatch" entry */
      lua_pushliteral(L, "dispatch"); /* key */

      /* Create a table for the uops entries */
      lua_newtable(L);

      /* For each uop */
      int i;
      for (i = 0; i < ext->nb_uops; i++) {
         uop_dispatch_t dispatch = ext->dispatch[i];

         lua_pushnumber(L, i + 1); /* key */
         lua_newtable(L); /* value (table) */

         /* Push the "port" entry */
         lua_pushliteral(L, "F0"); /* key */
         lua_pushnumber(L, dispatch.F0); /* value (integer) */
         lua_settable(L, -3);

         lua_pushliteral(L, "F1"); /* key */
         lua_pushnumber(L, dispatch.F1); /* value (integer) */
         lua_settable(L, -3);

         lua_pushliteral(L, "I0"); /* key */
         lua_pushnumber(L, dispatch.I0); /* value (integer) */
         lua_settable(L, -3);

         lua_pushliteral(L, "I1"); /* key */
         lua_pushnumber(L, dispatch.I1); /* value (integer) */
         lua_settable(L, -3);

         lua_pushliteral(L, "M"); /* key */
         lua_pushnumber(L, dispatch.M); /* value (integer) */
         lua_settable(L, -3);

         lua_pushliteral(L, "L"); /* key */
         lua_pushnumber(L, dispatch.L); /* value (integer) */
         lua_settable(L, -3);

         lua_pushliteral(L, "S"); /* key */
         lua_pushnumber(L, dispatch.S); /* value (integer) */
         lua_settable(L, -3);

         lua_pushliteral(L, "B"); /* key */
         lua_pushnumber(L, dispatch.B); /* value (integer) */
         lua_settable(L, -3);

         lua_settable(L, -3);
      }

      lua_settable(L, -3);

      /* Push the "latency" entry */
      lua_pushliteral(L, "latency"); /* key */
      push_float_min_max(L, ext->latency); /* value (table) */

      /* Push the "lf_latency" (late forwarding) entry */
      lua_pushliteral(L, "lf_latency"); /* key */
      push_float_min_max(L, ext->lf_latency); /* value (table) */

      /* Push the "throughput" (throughput) entry */
      lua_pushliteral(L, "throughput"); /* key */
      push_float_min_max(L, ext->throughput); /* value (table) */

      return 1;

   }

#endif

   return 0;

}

static int l_insn_get_bitsize(lua_State * L)
{
   i_t *i = luaL_checkudata(L, 1, INSN);

   lua_pushnumber(L, insn_get_size(i->p));

   return 1;
}

static int l_insn_get_coding(lua_State *L)
{
   char out[64];
   i_t *i = luaL_checkudata(L, 1, INSN);

   bitvector_hexprint(insn_get_coding(i->p), out, sizeof(out), " ");
   lua_pushstring(L, out);

   return 1;
}

static int l_insn_get_prev(lua_State *L)
{
   i_t *i = luaL_checkudata(L, 1, INSN);
   insn_t *prev = insn_get_prev(i->p);

   if (prev != NULL) {
      create_insn(L, prev);

      return 1;
   }

   return 0;
}

static int l_insn_get_next(lua_State *L)
{
   i_t *i = luaL_checkudata(L, 1, INSN);
   insn_t *next = insn_get_next(i->p);

   if (next != NULL) {
      create_insn(L, next);

      return 1;
   }

   return 0;
}

static int l_insn_is_exit_helper(lua_State *L, int annotation)
{
   i_t *insn = luaL_checkudata(L, 1, INSN);

   if (insn_get_annotate(insn->p) & annotation)
      lua_pushboolean(L, 1);
   else
      lua_pushboolean(L, 0);

   return 1;
}

static int l_insn_is_exit(lua_State * L)
{
   return l_insn_is_exit_helper(L, A_EX);
}

static int l_insn_is_exit_potential(lua_State * L)
{
   return l_insn_is_exit_helper(L, A_POTENTIAL_EX);
}

static int l_insn_is_exit_natural(lua_State * L)
{
   return l_insn_is_exit_helper(L, A_NATURAL_EX);
}

static int l_insn_is_exit_handler(lua_State * L)
{
   return l_insn_is_exit_helper(L, A_HANDLER_EX);
}

static int l_insn_is_exit_early(lua_State * L)
{
   return l_insn_is_exit_helper(L, A_EARLY_EX);
}

static int l_insn_is_branch(lua_State * L)
{
   insn_t *insn = ((i_t *) luaL_checkudata(L, 1, INSN))->p;
   lua_pushboolean(L, insn_is_jump(insn));

   return 1;
}

static int l_insn_is_branch_cond(lua_State * L)
{
   insn_t *insn = ((i_t *) luaL_checkudata(L, 1, INSN))->p;
   lua_pushboolean(L, insn_is_cond_jump(insn));

   return 1;
}

static int l_insn_is_branch_uncond(lua_State * L)
{
   insn_t *insn = ((i_t *) luaL_checkudata(L, 1, INSN))->p;
   lua_pushboolean(L, insn_is_uncond_jump(insn));

   return 1;
}

static int l_insn_is_call(lua_State * L)
{
   i_t *i = luaL_checkudata(L, 1, INSN);

   lua_pushboolean(L, insn_get_annotate(i->p) & A_CALL);

   return 1;
}

static int l_insn_is_return(lua_State * L)
{
   i_t *i = luaL_checkudata(L, 1, INSN);

   lua_pushboolean(L, insn_get_annotate(i->p) & A_RTRN);

   return 1;
}

static int l_insn_is_patchmov(lua_State * L)
{
   i_t *i = luaL_checkudata(L, 1, INSN);

   lua_pushboolean(L, insn_get_annotate(i->p) & A_PATCHMOV);

   return 1;
}

static int l_insn_is_patchnew(lua_State * L)
{
   i_t *i = luaL_checkudata(L, 1, INSN);

   lua_pushboolean(L, insn_get_annotate(i->p) & A_PATCHNEW);

   return 1;
}

static int l_insn_get_iset(lua_State *L)
{
   i_t *i = luaL_checkudata(L, 1, INSN);

   lua_pushnumber(L, insn_get_iset(i->p));

   return 1;
}

static int l_insn_get_branch_target(lua_State *L)
{
   insn_t *insn = ((i_t *) luaL_checkudata(L, 1, INSN))->p;
   insn_t *target;

   if (!(insn_get_annotate(insn) & A_JUMP)
         && !(insn_get_annotate(insn) & A_CALL))
      return 0;

   target = insn_get_branch(insn);

   if (target != NULL) {
      create_insn(L, target);

      return 1;
   }

   return 0;
}

static int l_insn_get_asm_code(lua_State * L)
{
   char buffer[4096];
   insn_t *insn = ((i_t *) lua_touserdata(L, 1))->p;

   insn_print(insn, buffer, sizeof(buffer));
   lua_pushstring(L, buffer);

   return 1;
}

static int l_insn_is_load(lua_State * L)
{
   insn_t *insn = ((i_t *) luaL_checkudata(L, 1, INSN))->p;
   lua_pushboolean(L, insn_is_load(insn));

   return 1;
}

static int l_insn_is_store(lua_State * L)
{
   insn_t *insn = ((i_t *) luaL_checkudata(L, 1, INSN))->p;
   lua_pushboolean(L, insn_is_store(insn));

   return 1;
}

static int l_insn_is_add_sub(lua_State * L)
{
   insn_t *insn = ((i_t *) luaL_checkudata(L, 1, INSN))->p;
   lua_pushboolean(L, insn_is_add_sub(insn));

   return 1;
}

static int l_insn_is_mul(lua_State * L)
{
   insn_t *insn = ((i_t *) luaL_checkudata(L, 1, INSN))->p;
   lua_pushboolean(L, insn_is_mul(insn));

   return 1;
}

static int l_insn_is_fma(lua_State * L)
{
   insn_t *insn = ((i_t *) luaL_checkudata(L, 1, INSN))->p;
   lua_pushboolean(L, insn_is_fma(insn));

   return 1;
}

static int l_insn_is_div(lua_State * L)
{
   insn_t *insn = ((i_t *) luaL_checkudata(L, 1, INSN))->p;
   lua_pushboolean(L, insn_is_div(insn));

   return 1;
}

static int l_insn_is_rcp(lua_State * L)
{
   insn_t *insn = ((i_t *) luaL_checkudata(L, 1, INSN))->p;
   lua_pushboolean(L, insn_is_rcp(insn));

   return 1;
}

static int l_insn_is_sqrt(lua_State * L)
{
   insn_t *insn = ((i_t *) luaL_checkudata(L, 1, INSN))->p;
   lua_pushboolean(L, insn_is_sqrt(insn));

   return 1;
}

static int l_insn_is_rsqrt(lua_State * L)
{
   insn_t *insn = ((i_t *) luaL_checkudata(L, 1, INSN))->p;
   lua_pushboolean(L, insn_is_rsqrt(insn));

   return 1;
}

static int l_insn_is_arith(lua_State * L)
{
   insn_t *insn = ((i_t *) luaL_checkudata(L, 1, INSN))->p;
   lua_pushboolean(L, insn_is_arith(insn));

   return 1;
}

static int insn_gc(lua_State * L)
{
   (void) L;

   return 0;
}

static int insn_tostring(lua_State * L)
{
   char buffer[4096];
   insn_t *insn = ((i_t *) lua_touserdata(L, 1))->p;

   insn_print(insn, buffer, sizeof(buffer));
   lua_pushfstring(L, "Insn: %s", buffer);

   return 1;
}

const luaL_reg insn_methods[] = {
  /* Getters on parent abstract objects */
   {"get_project"              , l_insn_get_project},
   {"get_asmfile"              , l_insn_get_asmfile},
   {"get_function"             , l_insn_get_function},
   {"get_loop"                 , l_insn_get_loop},
   {"get_block"                , l_insn_get_block},

   /* Getters/testers on attributes */
   {"get_address"              , l_insn_get_address},
   {"get_src_line"             , l_insn_get_src_line},
   {"get_src_column"           , l_insn_get_src_column},
   {"get_src_file_path"        , l_insn_get_src_file_path},
   {"get_coding"               , l_insn_get_coding},
   {"get_iset"                 , l_insn_get_iset},
   {"get_name"                 , l_insn_get_name},
   {"get_asm_code"             , l_insn_get_asm_code},
   {"get_bitsize"              , l_insn_get_bitsize},
   {"get_dispatch"             , l_insn_get_dispatch},

   {"get_class"                , l_insn_get_class},
   {"get_element_size"         , l_insn_get_element_size},
   {"get_input_element_size"   , l_insn_get_input_element_size},
   {"get_output_element_size"  , l_insn_get_output_element_size},
   {"get_element_bits"         , l_insn_get_element_bits},
   {"get_input_element_bits"   , l_insn_get_input_element_bits},
   {"get_output_element_bits"  , l_insn_get_output_element_bits},
   {"get_element_type"         , l_insn_get_element_type},
   {"get_input_element_type"   , l_insn_get_input_element_type},
   {"get_output_element_type"  , l_insn_get_output_element_type},
   {"get_family"               , l_insn_get_family},
   {"is_SIMD"                  , l_insn_is_SIMD},
   {"is_INT"                   , l_insn_is_INT},
   {"is_SIMD_INT"              , l_insn_is_SIMD_INT},
   {"is_FP"                    , l_insn_is_FP},
   {"is_struct_or_str"         , l_insn_is_struct_or_str},
   {"is_single_prec"           , l_insn_is_single_prec},
   {"is_double_prec"           , l_insn_is_double_prec},
   {"is_prefetch"              , l_insn_is_prefetch},
   {"is_load"                  , l_insn_is_load},
   {"is_store"                 , l_insn_is_store},
   {"is_SIMD_FP"               , l_insn_is_SIMD_FP},
   {"is_SIMD_NOT_FP"           , l_insn_is_SIMD_NOT_FP},
   {"get_SIMD_width"           , l_insn_get_SIMD_width},
   {"is_packed"                , l_insn_is_packed},
   {"get_read_bits"            , l_insn_get_read_bits},
   {"get_read_size"            , l_insn_get_read_size},

   {"parsenew"                , l_insn_parsenew},
   {"parsenew_fromscratch"    , l_insn_parsenew_fromscratch},
   {"free_parsenew"           , l_insn_free_parsenew},
   {"is_branch"               , l_insn_is_branch},
   {"is_branch_cond"          , l_insn_is_branch_cond},
   {"is_branch_uncond"        , l_insn_is_branch_uncond},
   {"is_call"                 , l_insn_is_call},
   {"is_return"               , l_insn_is_return},
   {"get_branch_target"       , l_insn_get_branch_target},
   {"get_groups"              , l_insn_get_groups},
   {"groups"                  , l_insn_groups},
   {"get_first_group"         , l_insn_get_first_group},
   {"get_label_name"          , l_insn_get_label_name},

   {"get_prev"    		      , l_insn_get_prev},
   {"get_next"    		      , l_insn_get_next},
   {"is_exit"                 , l_insn_is_exit},
   {"is_exit_natural"         , l_insn_is_exit_natural},
   {"is_exit_early"           , l_insn_is_exit_early},
   {"is_exit_potential"       , l_insn_is_exit_potential},
   {"is_exit_handler"         , l_insn_is_exit_handler},

   {"is_patched"              , l_insn_is_patchmov},
   {"is_patch_added"          , l_insn_is_patchnew},

   /* Getters/testers on operands (defined in ao_insn_oprnd.c) */
   {"get_noprnds"             , l_insn_get_nb_oprndss},
   {"get_registers_name"      , l_insn_get_registers_name},
   {"get_registers_type"      , l_insn_get_registers_type},
   {"get_registers_id"        , l_insn_get_registers_id},
   {"get_registers_rw"        , l_insn_get_registers_rw},
   {"get_operands"            , l_insn_get_operands},
   {"get_operand_ptr"         , l_insn_get_operand_ptr},
   {"get_first_mem_oprnd"     , l_insn_get_first_mem_oprnd},
   {"has_src_mem_oprnd"       , l_insn_has_src_mem_oprnd},
   {"has_dst_mem_oprnd"       , l_insn_has_dst_mem_oprnd},
   {"get_operand_src_index"   , l_insn_get_oprnd_src_index},
   {"get_operand_dest_index"  , l_insn_get_oprnd_dst_index},
   {"get_oprnd_str"           , l_insn_get_oprnd_str},
   {"get_oprnd_int"           , l_insn_get_oprnd_int},
   {"get_oprnd_type"          , l_insn_get_oprnd_type},
   {"get_rip_oprnd_dest"      , l_insn_get_rip_oprnd_dest},
   {"is_oprnd_mem"            , l_insn_is_oprnd_mem},
   {"is_oprnd_reg"            , l_insn_is_oprnd_reg},
   {"is_oprnd_imm"            , l_insn_is_oprnd_imm},

   /* Testers for arithmetical properties */
   {"is_add_sub", l_insn_is_add_sub},
   {"is_mul"    , l_insn_is_mul},
   {"is_fma"    , l_insn_is_fma},
   {"is_div"    , l_insn_is_div},
   {"is_rcp"    , l_insn_is_rcp},
   {"is_sqrt"   , l_insn_is_sqrt},
   {"is_rsqrt"  , l_insn_is_rsqrt},
   {"is_arith"  , l_insn_is_arith},

   {NULL, NULL}
};

const luaL_reg insn_meta[] = {
  {"__gc"      , insn_gc},
  {"__tostring", insn_tostring},
  {NULL, NULL}
};
