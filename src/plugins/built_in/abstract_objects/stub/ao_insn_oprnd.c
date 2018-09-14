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
/*       Functions related to operands of an instruction          */
/******************************************************************/

/**
 * Returns the name of a register
 * Helper function, internally used by other helper functions and by insn_get_registers_name() and insn_get_operands()
 */
static char *get_reg_name(insn_t *insn, reg_t *reg)
{
   return arch_get_reg_name(insn_get_arch(insn), reg_get_type(reg),
         reg_get_name(reg));
}

int l_insn_get_registers_name(lua_State * L)
{
   insn_t *insn = ((i_t *) luaL_checkudata(L, 1, INSN))->p;
   int i = 1, j;

   lua_newtable(L);

   for (j = 0; j < insn_get_nb_oprnds(insn); j++) {
      oprnd_t *oprnd = insn_get_oprnd(insn, j);

      switch (oprnd_get_type(oprnd)) {
      case OT_REGISTER:
      case OT_REGISTER_INDEXED:
         lua_pushstring(L, get_reg_name(insn, oprnd_get_reg(oprnd)));
         lua_rawseti(L, -2, i++);
         break;

      case OT_MEMORY:
      case OT_MEMORY_RELATIVE:
         if (oprnd_get_seg(oprnd) != NULL) {
            lua_pushstring(L, get_reg_name(insn, oprnd_get_seg(oprnd)));
            lua_rawseti(L, -2, i++);
         }

         if (oprnd_get_base(oprnd) != NULL) {
            lua_pushstring(L, get_reg_name(insn, oprnd_get_base(oprnd)));
            lua_rawseti(L, -2, i++);
         }

         if (oprnd_get_index(oprnd) != NULL) {
            lua_pushstring(L, get_reg_name(insn, oprnd_get_index(oprnd)));
            lua_rawseti(L, -2, i++);
         }
         break;
      default:
         break;  //To avoid compilation warnings
      } /* switch (operand type) */
   } /* For each operand */

   return 1;
}

int l_insn_get_registers_type(lua_State * L)
{
   insn_t *insn = ((i_t *) luaL_checkudata(L, 1, INSN))->p;
   int i = 1, j;

   lua_newtable(L);

   for (j = 0; j < insn_get_nb_oprnds(insn); j++) {
      oprnd_t *oprnd = insn_get_oprnd(insn, j);

      switch (oprnd_get_type(oprnd)) {
      case OT_REGISTER:
      case OT_REGISTER_INDEXED:
         lua_pushinteger(L, reg_get_type(oprnd_get_reg(oprnd)));
         lua_rawseti(L, -2, i++);
         break;

      case OT_MEMORY:
      case OT_MEMORY_RELATIVE:
         if (oprnd_get_seg(oprnd) != NULL) {
            lua_pushinteger(L, reg_get_type(oprnd_get_seg(oprnd)));
            lua_rawseti(L, -2, i++);
         }

         if (oprnd_get_base(oprnd) != NULL) {
            lua_pushinteger(L, reg_get_type(oprnd_get_base(oprnd)));
            lua_rawseti(L, -2, i++);
         }

         if (oprnd_get_index(oprnd) != NULL) {
            lua_pushinteger(L, reg_get_type(oprnd_get_index(oprnd)));
            lua_rawseti(L, -2, i++);
         }
         break;
      default:
         break;  //To avoid compilation warnings
      } /* switch (operand type) */
   } /* For each operand */

   return 1;
}

int l_insn_get_registers_id(lua_State * L)
{
   insn_t *insn = ((i_t *) luaL_checkudata(L, 1, INSN))->p;
   int i = 1, j;

   lua_newtable(L);

   for (j = 0; j < insn_get_nb_oprnds(insn); j++) {
      oprnd_t *oprnd = insn_get_oprnd(insn, j);

      switch (oprnd_get_type(oprnd)) {
      case OT_REGISTER:
      case OT_REGISTER_INDEXED:
         lua_pushnumber(L, __regID(oprnd_get_reg(oprnd), insn_get_arch(insn)));
         lua_rawseti(L, -2, i++);
         break;

      case OT_MEMORY:
      case OT_MEMORY_RELATIVE:
         if (oprnd_get_seg(oprnd) != NULL) {
            lua_pushnumber(L,
                  __regID(oprnd_get_seg(oprnd), insn_get_arch(insn)));
            lua_rawseti(L, -2, i++);
         }

         if (oprnd_get_base(oprnd) != NULL) {
            lua_pushnumber(L,
                  __regID(oprnd_get_base(oprnd), insn_get_arch(insn)));
            lua_rawseti(L, -2, i++);
         }

         if (oprnd_get_index(oprnd) != NULL) {
            lua_pushnumber(L,
                  __regID(oprnd_get_index(oprnd), insn_get_arch(insn)));
            lua_rawseti(L, -2, i++);
         }
         break;
      default:
         break;  //To avoid compilation warnings
      } /* switch (operand type) */
   } /* For each operand */

   return 1;
}

/**
 * Helper functions, internally used by insn_get_operands()
 * Pushes attributes of an operand
 */
static void push_oprnd_value_str(lua_State *L, char *value)
{
   lua_pushliteral(L, "value");
   lua_pushstring(L, value);
   lua_settable(L, -3);
}

static void push_oprnd_value_int(lua_State *L, int64_t value)
{
   lua_pushliteral(L, "value");
   lua_pushinteger(L, value);
   lua_settable(L, -3);
}

static void push_oprnd_type(lua_State *L, uint8_t type)
{
   lua_pushliteral(L, "type");
   lua_pushnumber(L, type);
   lua_settable(L, -3);
}

static void push_oprnd_size(lua_State *L, oprnd_t *oprnd)
{
   lua_pushliteral(L, "size");
   lua_pushnumber(L, oprnd_get_size_value(oprnd));
   lua_settable(L, -3);
}

static void push_oprnd_read(lua_State *L, int is_read)
{
   lua_pushliteral(L, "read");
   lua_pushboolean(L, is_read);
   lua_settable(L, -3);
}

static void push_oprnd_write(lua_State *L, int is_write)
{
   lua_pushliteral(L, "write");
   lua_pushboolean(L, is_write);
   lua_settable(L, -3);
}

static void push_oprnd_typex(lua_State *L, char *typex)
{
   lua_pushliteral(L, "typex");
   lua_pushstring(L, typex);
   lua_settable(L, -3);
}

static void push_oprnd_custom(lua_State *L, const char *str, int val)
{
   lua_pushstring(L, str);
   lua_pushinteger(L, val);
   lua_settable(L, -3);
}

static void push_oprnd_reg(lua_State *L, insn_t *insn, reg_t *reg)
{
   const char reg_type = reg_get_type(reg);

   push_oprnd_custom(L, "reg type", reg_type);
   push_oprnd_custom(L, "name", reg_get_name(reg));
   push_oprnd_custom(L, "family", reg_get_family(reg, insn_get_arch(insn)));
   push_oprnd_value_str(L, get_reg_name(insn, reg));
}

static void push_oprnd_value_mem(lua_State *L, insn_t *insn, oprnd_t *oprnd)
{
   int i = 1;

   lua_pushliteral(L, "value");
   lua_newtable(L);

   if (oprnd_get_seg(oprnd) != NULL) {
      lua_newtable(L);

      push_oprnd_type(L, OT_REGISTER);
      push_oprnd_read(L, TRUE);
      push_oprnd_write(L, FALSE);
      push_oprnd_typex(L, "segment");
      push_oprnd_reg(L, insn, oprnd_get_seg(oprnd));

      lua_rawseti(L, -2, i++);
   }

   if (oprnd_get_offset(oprnd) != 0) {
      lua_newtable(L);

      push_oprnd_type(L, OT_IMMEDIATE);
      push_oprnd_value_int(L, oprnd_get_offset(oprnd));

      lua_rawseti(L, -2, i++);
   }

   if (oprnd_get_base(oprnd) != NULL) {
      lua_newtable(L);

      push_oprnd_type(L, OT_REGISTER);
      push_oprnd_read(L, TRUE);
      push_oprnd_write(L, FALSE);
      push_oprnd_typex(L, "base");
      push_oprnd_reg(L, insn, oprnd_get_base(oprnd));

      lua_rawseti(L, -2, i++);
   }

   if (oprnd_get_index(oprnd) != NULL) {
      lua_newtable(L);

      push_oprnd_type(L, OT_REGISTER);
      push_oprnd_read(L, TRUE);
      push_oprnd_write(L, FALSE);
      push_oprnd_typex(L, "index");
      push_oprnd_reg(L, insn, oprnd_get_index(oprnd));

      lua_rawseti(L, -2, i++);
   }

   if (oprnd_get_scale(oprnd) != 0) {
      lua_newtable(L);

      push_oprnd_type(L, OT_IMMEDIATE);
      push_oprnd_value_int(L, oprnd_get_scale(oprnd));

      lua_rawseti(L, -2, i++);
   }

   lua_settable(L, -3);
}

/*
 * Helper function used by get_registers_rw() and get_operands()
 * Check if at least one operand has an invalid type
 */
static int has_invalid_oprnds(insn_t *insn)
{
   int j;

   for (j = 0; j < insn_get_nb_oprnds(insn); j++) {
      oprnd_t *oprnd = insn_get_oprnd(insn, j);
      const int type = oprnd_get_type(oprnd);

      switch (type) {
      case OT_REGISTER:
      case OT_REGISTER_INDEXED:
      case OT_IMMEDIATE:
      case OT_POINTER:
      case OT_MEMORY:
      case OT_MEMORY_RELATIVE:
         break;

      default:
         fprintf(stderr, "Invalid type in l_insn_get_operands (type = %d)\n",
               oprnd_get_type(oprnd));
         return TRUE;
      }
   }

   return FALSE;
}

/**
 * Retrieves the address pointed by a RIP-based operand. Returns -1 if the instruction has no RIP operand
 * */
int l_insn_get_rip_oprnd_dest(lua_State * L)
{
   insn_t *insn = ((i_t *) luaL_checkudata(L, 1, INSN))->p;
   int isinsn = -1;

   int64_t dest = insn_check_refs(insn, &isinsn);
   if ((dest > SIGNED_ERROR) && (!isinsn))
      lua_pushinteger(L, dest);
   else
      lua_pushinteger(L, -1);

   return 1;
}

int l_insn_get_operands(lua_State *L)
{
   insn_t *insn = ((i_t *) luaL_checkudata(L, 1, INSN))->p;
   int i = 1, j;

   if (has_invalid_oprnds(insn) == TRUE)
      return 0;

   lua_newtable(L); /* operands list */

   for (j = 0; j < insn_get_nb_oprnds(insn); j++) {
      oprnd_t *oprnd = insn_get_oprnd(insn, j);
      const int type = oprnd_get_type(oprnd);

      lua_newtable(L); /* operand itself */

      /* For all operands, type and size are pushed in the table */
      push_oprnd_type(L, type);
      push_oprnd_size(L, oprnd);

      /* Type-dependant data are pushed in the table */
      switch (type) {
      case OT_REGISTER:
      case OT_REGISTER_INDEXED:
         push_oprnd_read(L, oprnd_is_src(oprnd));
         push_oprnd_write(L, oprnd_is_dst(oprnd));
         push_oprnd_reg(L, insn, oprnd_get_reg(oprnd));
         break;

      case OT_IMMEDIATE:
         push_oprnd_value_int(L, oprnd_get_imm(oprnd));
         break;

      case OT_POINTER:
         push_oprnd_value_int(L, oprnd_get_refptr_addr(oprnd));
         break;

      case OT_MEMORY:
      case OT_MEMORY_RELATIVE:
         push_oprnd_read(L, oprnd_is_src(oprnd));
         push_oprnd_write(L, oprnd_is_dst(oprnd));
         push_oprnd_value_mem(L, insn, oprnd);
         break;
      }

      lua_rawseti(L, -2, i++);
   } /* for each operand */

   return 1;
}

int l_insn_get_operand_ptr(lua_State *L)
{
   insn_t *insn = ((i_t *) luaL_checkudata(L, 1, INSN))->p;
   int rank = luaL_checkinteger(L, 2);
   oprnd_t *oprnd;

   if (rank > insn_get_nb_oprnds(insn) || rank < 1)
      return 0;
   oprnd = insn_get_oprnd(insn, rank - 1);
   if (oprnd == NULL)
      return 0;
   lua_pushlightuserdata(L, oprnd);

   return 1;
}

/* Helper function for l_insn_get_registers_rw */
static void push_register_rw(lua_State *L, int key, int rd, int wr,
      insn_t *insn, reg_t *reg)
{
   lua_newtable(L);
   push_oprnd_read(L, rd);
   push_oprnd_write(L, wr);
   push_oprnd_value_str(L, get_reg_name(insn, reg));
   lua_rawseti(L, -2, key);
}

int l_insn_get_registers_rw(lua_State *L)
{
   insn_t *insn = ((i_t *) luaL_checkudata(L, 1, INSN))->p;
   int i = 1, j;

   if (has_invalid_oprnds(insn) == TRUE)
      return 0;

   lua_newtable(L); /* registers list */

   for (j = 0; j < insn_get_nb_oprnds(insn); j++) {
      oprnd_t *oprnd = insn_get_oprnd(insn, j);
      const int type = oprnd_get_type(oprnd);

      if ((type == OT_REGISTER) || (type == OT_REGISTER_INDEXED)
            || ((type == OT_MEMORY || type == OT_MEMORY_RELATIVE)
                  && ((oprnd_get_seg(oprnd) != NULL)
                        || (oprnd_get_base(oprnd) != NULL)
                        || (oprnd_get_index(oprnd) != NULL)))) {

         if (type == OT_REGISTER)
            push_register_rw(L, i++, oprnd_is_src(oprnd), oprnd_is_dst(oprnd),
                  insn, oprnd_get_reg(oprnd));

         else {
            if (oprnd_get_seg(oprnd) != NULL)
               push_register_rw(L, i++, TRUE, FALSE, insn,
                     oprnd_get_seg(oprnd));

            if (oprnd_get_base(oprnd) != NULL)
               push_register_rw(L, i++, TRUE, FALSE, insn,
                     oprnd_get_base(oprnd));

            if (oprnd_get_index(oprnd) != NULL)
               push_register_rw(L, i++, TRUE, FALSE, insn,
                     oprnd_get_index(oprnd));
         }
      } /* if register */
   } /* for each operand */

   return 1;
}

static void push_oprnd_rank(lua_State *L, int rank)
{
   lua_pushliteral(L, "rank");
   lua_pushinteger(L, rank);
   lua_settable(L, -3);
}

int l_insn_get_first_mem_oprnd(lua_State *L)
{
   insn_t *insn = ((i_t *) luaL_checkudata(L, 1, INSN))->p;
   oprnd_t *oprnd = insn_get_first_mem_oprnd(insn);

   if (oprnd == NULL)
      return 0;

   lua_newtable(L);
   push_oprnd_type(L, oprnd_get_type(oprnd));
   push_oprnd_size(L, oprnd);
   push_oprnd_read(L, oprnd_is_src(oprnd));
   push_oprnd_write(L, oprnd_is_dst(oprnd));
   push_oprnd_rank(L, insn_get_first_mem_oprnd_pos(insn));
   push_oprnd_value_mem(L, insn, oprnd);

   return 1;
}

int l_insn_has_src_mem_oprnd(lua_State *L)
{
   insn_t *insn = ((i_t *) luaL_checkudata(L, 1, INSN))->p;
   lua_pushboolean(L, insn_has_src_mem_oprnd(insn));

   return 1;
}

int l_insn_has_dst_mem_oprnd(lua_State *L)
{
   insn_t *insn = ((i_t *) luaL_checkudata(L, 1, INSN))->p;
   lua_pushboolean(L, insn_has_dst_mem_oprnd(insn));

   return 1;
}

int l_insn_get_oprnd_src_index(lua_State *L)
{
   insn_t *insn = ((i_t *) luaL_checkudata(L, 1, INSN))->p;
   lua_pushinteger(L, insn_get_oprnd_src_index(insn));

   return 1;
}

int l_insn_get_oprnd_dst_index(lua_State *L)
{
   insn_t *insn = ((i_t *) luaL_checkudata(L, 1, INSN))->p;
   lua_pushinteger(L, insn_get_oprnd_dst_index(insn));

   return 1;
}

int l_insn_get_nb_oprndss(lua_State * L)
{
   i_t *i = luaL_checkudata(L, 1, INSN);

   lua_pushnumber(L, insn_get_nb_oprnds(i->p));

   return 1;
}

int l_insn_get_oprnd_int(lua_State * L)
{
   i_t *insn = ((i_t *) luaL_checkudata(L, 1, INSN));
   int pos = luaL_checkint(L, 2);
   oprnd_t* oprnd = insn_get_oprnd(insn->p, pos);
   int64_t imm;

   if (oprnd) {
      imm = oprnd_get_imm(oprnd);
      lua_pushinteger(L, imm);
      return 1;
   } else
      return 0;
}

int l_insn_get_oprnd_str(lua_State * L)
{
   i_t *insn = ((i_t *) luaL_checkudata(L, 1, INSN));
   int pos = luaL_checkint(L, 2);

   char out[256];
   oprnd_t* oprnd = insn_get_oprnd(insn->p, pos);
   if (oprnd) {
      oprnd_print(insn->p, oprnd, out, sizeof(out), insn_get_arch(insn->p));
      lua_pushstring(L, out);
      return 1;
   } else
      return 0;
}

int l_insn_get_oprnd_type(lua_State * L)
{
   i_t *insn = ((i_t *) luaL_checkudata(L, 1, INSN));
   int pos = luaL_checkint(L, 2);

   oprnd_t* oprnd = insn_get_oprnd(insn->p, pos);
   if (oprnd) {
      lua_pushnumber(L, oprnd_get_type(oprnd));
      return 1;
   } else
      return 0;
}

int l_insn_is_oprnd_mem(lua_State * L)
{
   i_t *insn = ((i_t *) luaL_checkudata(L, 1, INSN));
   int pos = luaL_checkint(L, 2);

   int ismem = oprnd_is_mem(insn_get_oprnd(insn->p, pos));
   if (ismem)
      lua_pushboolean(L, 1);
   else
      lua_pushboolean(L, 0);
   return 1;
}

int l_insn_is_oprnd_reg(lua_State * L)
{
   i_t *insn = ((i_t *) luaL_checkudata(L, 1, INSN));
   int pos = luaL_checkint(L, 2);

   int isreg = oprnd_is_reg(insn_get_oprnd(insn->p, pos));
   if (isreg)
      lua_pushboolean(L, 1);
   else
      lua_pushboolean(L, 0);
   return 1;
}

int l_insn_is_oprnd_imm(lua_State * L)
{
   i_t *insn = ((i_t *) luaL_checkudata(L, 1, INSN));
   int pos = luaL_checkint(L, 2);

   int isimm = oprnd_is_imm(insn_get_oprnd(insn->p, pos));
   if (isimm)
      lua_pushboolean(L, 1);
   else
      lua_pushboolean(L, 0);
   return 1;
}
