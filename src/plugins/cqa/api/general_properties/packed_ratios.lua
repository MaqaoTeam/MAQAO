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

--- Module cqa.general_properties.packed_ratios
-- Defines the get_packed_ratios function to return the packed (also called
-- vectorization) ratios of a sequence of instructions
module ("cqa.api.general_properties.packed_ratios", package.seeall);

--- Returns vectorization ratio of a sequence of instructions, with details for all instruction types
-- @param mode "INT", "FP" or "INT+FP"
-- @param crp cqa_results_path table (CF data_struct.lua)
-- @return a table containing ratios for all instructions types (all, load, store, add/sub, mul or other)
function get_packed_ratios (mode, crp)
   local cqa_context       = crp.common.context
   local uarch_consts      = cqa_context.uarch_consts;
   local cached_data_cbp   = crp.common._cache ["can_be_packed"];

   local instr_types = {"all", "load", "store", "add-sub", "mul", "other"};

   -- tables, keys are in 'instr_types'
   local nb_packed = {}; -- number of packed instructions
   local nb        = {}; -- number of (scalar or packed) instructions

   local function update_counts (type_name, packed)
      nb [type_name] = nb [type_name] + 1;

      if (packed) then
         nb_packed [type_name] = nb_packed [type_name] + 1;
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
         local name = insn:get_name ();
         local packed = insn:is_packed();

         update_counts ("all", packed);

         if     (insn:is_store  ()) then update_counts ("store"  , packed);
         elseif (insn:is_add_sub()) then update_counts ("add-sub", packed);
         elseif (insn:is_mul    ()) then update_counts ("mul"    , packed);

         elseif (not insn:is_load()) then
            update_counts ("other", packed);
         end

         -- An instruction can be both a load and an arithmetic
         if (insn:is_load ()) then update_counts ("load", packed) end
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
