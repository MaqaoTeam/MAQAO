---
--  Copyright (C) 2004 - 2018 Université de Versailles Saint-Quentin-en-Yvelines (UVSQ)
--
-- This file is part of MAQAO.
--
-- MAQAO is free software; you can redistribute it and/or
--  modify it under the terms of the GNU Lesser General Public License
--  as published by the Free Software Foundation; either version 3
--  of the License, or (at your option) any later version.
--
--  This program is distributed in the hope that it will be useful,
--  but WITHOUT ANY WARRANTY; without even the implied warranty of
--  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
--  GNU Lesser General Public License for more details.
--
--  You should have received a copy of the GNU Lesser General Public License
--  along with this program; if not, write to the Free Software
--  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
---

--- Module mod
-- Defines some instruction modifiers
module ("cqa.api.mod", package.seeall)

require "cqa.api.general_properties.vec_eff_ratios";

-- TODO: think to another format for insn modifiers: insns/cqa_context not always used...

-- Returns true if an instruction is involved in loop control
local function is_loop_ctrl (insn, cmp_insn)
   local fam = insn:get_family();

   -- if conditional branch
   if (fam == Consts.FM_CJUMP or fam == Consts.FM_LOOP) then return true end

   -- If no CMP instruction found
   if (cmp_insn == nil) then return false end

   -- If the last CMP instruction is the input instruction
   if (cmp_insn:get_userdata_address() ==
       insn    :get_userdata_address()) then
      return true;
   end

   -- for each register written by the current instruction
   for _,oprnd_cur in pairs (insn:get_operands()) do
      if (oprnd_cur ["type" ] == Consts.OT_REGISTER and
          oprnd_cur ["write"] == true) then

         -- for each register read by the last CMP instruction
         for _,oprnd_CMP in pairs (cmp_insn:get_operands()) do
            if (oprnd_CMP ["type"] == Consts.OT_REGISTER) then

               -- if matching
               if (oprnd_cur ["value"] == oprnd_CMP ["value"]) then return true end
            end
         end
      end
   end

   return false;
end

-- Returns true if an instruction is SSE/AVX or implements loop control
function vector_iset (insn, insns, cqa_context)
   -- if uses SIMD instruction set or registers
   if (insn:is_SIMD()) then return {insn} end

   -- if MOVSX or MOVSXD
   if (string.find (insn:get_name(), "^MOVSX") ~= nil) then return {insn} end

   if (is_loop_ctrl (insn, cqa_context.cmp_insn)) then return {insn} end

   return {};
end

-- Returns true if an instruction is SSE/AVX FP or implements loop control
function FP (insn, insns, cqa_context)
   if (insn:is_SIMD_FP()) then return {insn} end

   if (is_loop_ctrl (insn, cqa_context.cmp_insn)) then return {insn} end

   return {};
end

-- Returns true if an instruction is FP arithmetic or implements loop control
function FP_arith (insn, insns, cqa_context)

   if (insn:is_SIMD_FP() and insn:is_arith()) then

      return {insn};
   end

   if (is_loop_ctrl (insn, cqa_context.cmp_insn)) then return {insn} end

   return {};
end

local function cannot_be_packed (insn, insns, cqa_context)
   if (string.find (insn:get_name(), "MOVSX") ~= nil) then return true end

   local arch = cqa_context.arch;
   if (not insn:can_be_packed_INT (insns, arch, cqa_context.uarch_consts, true) and
       not insn:can_be_packed_FP (arch, cqa_context.uarch_consts)) then
      return true;
   end

   return false;
end

-- common for fully_vec and mem_vec
-- TODO: remove this (directly use cqa_context.insn_modifier_options
local function get_params (cqa_context)
   local params = {};

   local options = cqa_context.insn_modifier_options;
   if     (options.vector_aligned  ) then params.aligned = true;
   elseif (options.vector_unaligned) then params.aligned = false;
   end

   if (options.force_sse) then params.sse = true end

   return params;
end

local function get_extra_unroll_factor (insn, cqa_context, force_sse)
   local vec_eff = cqa.api.general_properties.vec_eff_ratios.get_vec_eff (insn, cqa_context);

   if (vec_eff == nil) then return nil end

   local arch = cqa_context.arch;
   return 1 / vec_eff 

end

function fully_vec (insn, insns, cqa_context, is_stride_1)
   -- forward (return unmodified) non vectorizable instructions
   if (cannot_be_packed (insn, insns, cqa_context)) then return {insn} end

   local params = get_params (cqa_context);

end

function compute_vec (insn, insns, cqa_context)
   -- forward (return unmodified) non vectorizable instructions
   if (cannot_be_packed (insn, insns, cqa_context)) then return {insn} end

   local params = get_params (cqa_context);

end

function mem_vec (insn, insns, cqa_context, is_stride_1)
   -- forward (return unmodified) non vectorizable instructions
   if (cannot_be_packed (insn, insns, cqa_context)) then return {insn} end

   local params = get_params (cqa_context);

end

function requires_streams (func)
   return (func == fully_vec or func == mem_vec)
end
