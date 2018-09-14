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

--- Module cqa.general_properties.used_registers
-- Defines the get_used_registers function used to return
-- the number of registers used by a sequence of instructions
module ("cqa.api.general_properties.used_registers", package.seeall);

--- Returns the number of registers used by a sequence of instructions
-- and whose name matches with a prefix (e.g. "XMM")
-- @param insns table referencing instructions
-- @param prefix pattern string to match with register name (case sensitive)
-- @return number of used registers
function get_used_registers (insns, prefix)
   local used = Table:new();

   -- for each instruction
   for _,insn in ipairs (insns) do
      local registers = insn:get_registers_name();

      if (registers ~= nil) then
         for _,reg in pairs (registers) do
            local matches_with_prefix =
            (string.find (reg, "^"..prefix) ~= nil);

            if (matches_with_prefix and (used [reg] == nil)) then
               used [reg] = true; -- any value different from nil could fit here
            end
         end

      else
         Debug:info ("No register operand in the instruction ["..insn:get_asm_code().."]");
      end
   end

   return used:get_size();
end
