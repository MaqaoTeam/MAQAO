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

--- Module cqa.general_properties.stack_ref_nb_x86
-- Defines the get_stack_ref_nb_x86 function used to return
-- the number of stack references used by a sequence of instructions
module ("cqa.api.general_properties.stack_ref_nb_x86", package.seeall);

--- Returns the number of stack references, relative to the stack or frame pointer
-- x86: the stack pointer is in %[ER]SP and the frame pointer, in %[ER]BP
-- A stack reference is accounted with the following formats:
--  * (%[ER][SB]P)
--  * offset(%[ER][SB]P)
--  * [CDEFGS]S:(%[ER][SB]P)
--  * [CDEFGS]S:offset(%[ER][SB]P)
-- @param insns table referencing instructions
-- @return number of references
function get_stack_ref_nb_x86 (insns)
   local offsets = {};
   local stack_ref_insns = {};

   -- for each instruction
   for _,insn in ipairs (insns) do
      local mem_oprnd = insn:get_first_mem_oprnd();

      -- if found a memory operand
      if (mem_oprnd ~= nil) then
         local mem_oprnd_val = mem_oprnd ["value"];

         -- for each memory sub-operand
         for idx,sub_oprnd in ipairs (mem_oprnd_val) do

            -- if found a base register at the last position (no index and no scale)
            if (sub_oprnd ["typex"] == "base" and mem_oprnd_val [idx+1] == nil) then
               for _,reg_pattern in pairs ({"[ER]SP$", "[ER]BP$"}) do
                  if (string.find (sub_oprnd ["value"], reg_pattern) ~= nil) then
		     table.insert (stack_ref_insns, insn);

                     local offset = 0;

                     if (idx > 1 and mem_oprnd_val[idx-1]["type"] == Consts.OT_IMMEDIATE) then
                        offset = mem_oprnd_val[idx-1]["value"];
                     end

                     if (offsets [reg_pattern] == nil) then offsets [reg_pattern] = Table:new() end

                     if (offsets [reg_pattern][offset] == nil) then
                        offsets [reg_pattern][offset] = true; -- any non-nil value
                     end

                     break;
                  end
               end

               break;
            end -- if found a base register at the last position
         end -- for each memory sub-operand
      end -- if found a memory operand
   end -- for each instruction

   local nb = 0;

   if (offsets ["[ER]SP$"] ~= nil) then nb = nb + offsets ["[ER]SP$"]:get_size() end
   if (offsets ["[ER]BP$"] ~= nil) then nb = nb + offsets ["[ER]BP$"]:get_size() end

   return nb, stack_ref_insns;
end
