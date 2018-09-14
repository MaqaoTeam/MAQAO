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

--- Module cqa.general_properties.vec_eff_ratios
-- Defines the get_vec_eff_ratios function to return the vector efficiency
-- ratios of a sequence of instructions (average vector length usage)
module ("cqa.api.general_properties.vec_eff_ratios", package.seeall);

function get_vec_eff (insn, cqa_context)
   -- Compute the largest supported vector size (in bytes)
   local reg_size = cqa_context.uarch_consts ["vector size in bytes"]; -- FP
   if (not insn:is_FP() and insn:get_name() ~= "VPTEST") then
      reg_size = cqa_context.uarch_consts ["INT vector size in bytes"]; -- INT
   end
   local ivo = cqa_context.if_vectorized_options;
   if (ivo ~= nil and ivo.force_sse) then reg_size = 16 end -- force SSE

   -- Compute the used vector size (in bytes)
   local used_size = insn:get_read_bits();
   if (used_size == 0) then used_size = insn:get_element_bits() end
   if (used_size == 0) then
      Debug:warn ("Cannot guess size of elements for ["..insn:get_asm_code().."]")
      return 1;
   else
      return (used_size / 8) / reg_size;
   end
end

--- Returns vector efficiency ratio of a sequence of instructions, with details for all instruction types
-- @param mode "INT", "FP" or "INT+FP"
-- @param crp cqa_results_path table (CF data_struct.lua)
-- @return a table containing ratios for all instructions types (all, load, store, add/sub, mul or other)
function get_vec_eff_ratios (mode, crp)
   local cqa_context       = crp.common.context
   local uarch_consts      = cqa_context.uarch_consts;
   local cached_data_cbp = crp.common._cache ["can_be_packed"];

   local instr_types = {"all", "load", "store", "add-sub", "mul", "other"};

   -- tables, keys are in 'instr_types'
   local nb_packed = {}; -- number of packed instructions
   local nb        = {}; -- number of (scalar or packed) instructions

   local function update_counts (type_name, packed)
      nb [type_name] = nb [type_name] + 1;

      if (packed > 0) then
         nb_packed [type_name] = nb_packed [type_name] + packed;
      end
   end

   for _,v in pairs (instr_types) do
      nb_packed [v] = 0;
      nb        [v] = 0;
   end

   -- for each SSE or AVX instruction
   for _,insn in ipairs (crp.insns) do
      local can_be_packed;

      if (cached_data_cbp ~= nil) then
	 can_be_packed = cached_data_cbp [mode][insn];
      else
	 can_be_packed = insn:can_be_packed (mode, crp.insns, uarch_consts)
      end

      if (can_be_packed) then
         local vec_eff = get_vec_eff (insn, crp.common.context);

	 if (vec_eff ~= nil) then
	    local name = insn:get_name ();

	    update_counts ("all", vec_eff);

	    if     (insn:is_store  ()) then update_counts ("store"  , vec_eff);
	    elseif (insn:is_add_sub()) then update_counts ("add-sub", vec_eff);
	    elseif (insn:is_mul    ()) then update_counts ("mul"    , vec_eff);

	    elseif (not insn:is_load() or (string.find (name, "MOV") == nil)) then
	       update_counts ("other", vec_eff);
	    end

	    -- An instruction can be both a load and an arithmetic
	    if (insn:is_load ()) then update_counts ("load", vec_eff) end
	 end -- if vec_eff ~= nil
      end -- if can be packed
   end

   -- Compute vectorization ratios from the number of instructions
   local ratios = {};

   for _,v in pairs (instr_types) do
      if (nb [v] == 0) then
         ratios [v] = "NA";
      else
         ratios [v] = (nb_packed [v] * 100) / nb [v];
      end
   end

   return ratios;
end
