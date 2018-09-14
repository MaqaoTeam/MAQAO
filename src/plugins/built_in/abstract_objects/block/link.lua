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

-- Characterization of the link between two blocks
LinkType = 
   { LinkNone   = { id = 0, name = "None" }
   , LinkDirect = { id = 1, name = "Direct" }
   , LinkJump   = { id = 2, name = "Unconditional jump" }
   , LinkJcc    = { id = 3, name = "Conditional jump" }
   }

-- Return the link type with the target block
function block:get_link_type(target)
   local last_insn = self:get_last_insn()
   local first_insn = target:get_first_insn()

   if last_insn:get_next():get_address() == first_insn:get_address() then
      return LinkType.LinkDirect

   elseif last_insn:is_branch() then
      local target_block = last_insn:get_branch_target():get_block()
      if target_block:get_id() == target:get_id() then
         if last_insn:is_branch_cond() then
            return LinkType.LinkJcc
         else
            return LinkType.LinkJump
         end
      end
   else
      return LinkType.LinkNone
   end
end

