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

--- Module cqa.general_properties.moved_bytes
-- Defines the get_moved_bytes function used to return the number
-- of bytes moved (loaded or stored) by a sequence of instructions
module ("cqa.api.general_properties.moved_bytes", package.seeall)

--- [Local function, used only by get_moved_bytes]
-- Returns the number of bytes moved (from/to memory) by an instruction
-- @param insn instruction
-- @return number of bytes
local function instr_moved_bytes (insn)
   if (insn:is_prefetch()) then return 64 end -- 64B cachelines

   local mem_oprnd = insn:get_first_mem_oprnd ();

   if (mem_oprnd ~= nil) then return mem_oprnd ["size"] / 8 end

   Debug:warn ("The number of bytes loaded/stored by the instruction ["..
          insn:get_asm_code().."] is unknown");

   return 0;
end

--- [Local function, used only by get_moved_bytes]
-- Returns the memory type (prefetch, load or store) of an instruction, if it is a prefetch, a load or a store
-- @param insn Instruction from which we want to get the memory type
-- @return "prefetch" (resp. "load", "store") if the instruction is a prefetch (resp. a load, store)
local function get_memory_type (insn)
   if (insn:is_store    ()) then return "store"    end
   if (insn:is_prefetch ()) then return "prefetch" end
   if (insn:is_load     ()) then return "load"     end
end

--- Returns the number of bytes moved (loaded or stored) by a sequence of instructions
-- @param insns table referencing instructions
-- @param weight table containing weight (scaling factor) for each instruction
-- @return number of bytes
function get_moved_bytes (insns, weight)
   local moved_bytes = {["prefetch"] = 0, ["load"] = 0, ["store"] = 0};

   -- for each instruction
   for _,insn in ipairs (insns) do
      local memory_type = get_memory_type (insn);

      if (memory_type ~= nil) then
	 local scale;
	 if (weight == nil) then scale = 1 else scale = weight [insn] end

         moved_bytes [memory_type] =
         moved_bytes [memory_type] + scale * instr_moved_bytes (insn);
      end
   end

   return moved_bytes;
end
