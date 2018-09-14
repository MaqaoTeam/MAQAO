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

module("block.insn",package.seeall)

--- Checks if a block contains an instruction using its name
-- @param insnname name of an instruction to look for
-- @return 1 if the instruction is found
function block:has_instruction(insnname)
   for insn in self:instructions () do
      local mne = insn:get_name()
      if (mne == insnname) then return 1 end
   end
end

--- Gets the number of bytes of a block
-- @return the number of bytes
function block:get_nbytes()
   local nb_bytes = 0;
	
   for insn in self:instructions() do
      nb_bytes = nb_bytes + insn:get_bitsize()/(64/8);
   end
   
   return nb_bytes;
end

--- Gets the number of instructions of a block
-- @return the number of instructions
function block:get_ninsns()
   local ninsns = 0;
   
   for insn in self:instructions() do
      ninsns = ninsns + 1;
   end
   
   return ninsns;
end