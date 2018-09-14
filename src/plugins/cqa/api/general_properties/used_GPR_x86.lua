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

--- Module cqa.general_properties.used_GPR_x86
-- Defines the get_used_GPR_x86 function used to return
-- the number of general-purpose registers used by a sequence of instructions
module ("cqa.api.general_properties.used_GPR_x86", package.seeall);

--- Returns register operands of an instruction
-- @param insn an instruction
-- @return table of register operands (such as elt ["type"] = Consts.OT_REGISTER)
local function get_registers (insn)
   local operands = insn:get_operands();

   if (operands == nil) then return end

   local regs = {};

   for _,v in pairs (operands) do
      if (v["type"] == Consts.OT_REGISTER) then
         table.insert (regs, v);
      elseif (v["type"] == Consts.OT_MEMORY) then
         for _,v2 in pairs (v["value"]) do
            if (v2["type"] == Consts.OT_REGISTER) then
               table.insert (regs, v2);
            end
         end
      end
   end

   return regs;
end

-- Checks whether a register is included into another one already inserted into used_table
-- @param reg a register (table)
-- @param used_table table of already inserted registers
-- @return boolean
local function is_included (reg, used_table)
   for _,reg2 in pairs (used_table) do
      if ((reg ["name"  ] == reg2 ["name"]) and
          (reg ["family"] == reg2 ["family"])) then
         return true;
      end
   end

   return false;
end

--- Returns the number of x86 general-purpose (non-segment) registers
-- used by a sequence of instructions
-- @param insns table referencing instructions
-- @param arch architecture string ("x86-64" or "k1om")
-- @return number of used registers
function get_used_GPR_x86 (insns, arch)
   local used = Table:new();

   -- for each instruction
   for _,insn in ipairs (insns) do
      local registers = get_registers(insn);

      if (registers ~= nil) then
         for _,reg in pairs (registers) do
            if (insn:reg_is_GPR (reg, arch) and (used [reg["value"]] == nil) and not is_included (reg, used)) then
               used [reg["value"]] = reg;
            end
         end
      end
   end

   return used:get_size();
end
