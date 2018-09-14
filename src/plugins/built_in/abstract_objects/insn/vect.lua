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

--- Defines functions related to instructions
-- vector (SIMD) instruction set instructions (at present, SSE and AVX)
module ("insn.vect", package.seeall)

local cannot_be_packed_data = {}; -- memoization to speed up cannot_be_packed

--- Checks whether an instruction cannot be packed
-- since contributing to loop control or address calculation
-- @param insn instruction to check
-- @param insns sequence of instructions containing insn
-- @param reset_memoized_data memoized data will be reset if true
-- @return boolean
local function cannot_be_packed (insn, insns, reset_memoized_data)
   if (insns == nil) then return false end

   -- Instructions in the LEA family are assumed to be part of address computation
   if (insn:get_family() == Consts.FM_LEA) then return true end

   -- Instructions reading/writing from/to partial legacy registers are probably non vectorizable
   for _,oprnd_cur in pairs (insn:get_operands()) do
      if (oprnd_cur.type == Consts.OT_REGISTER and oprnd_cur.size < 32) then
         return true
      end
   end

   local cbpd = cannot_be_packed_data;

   -- If no memoized data for cmp_insn, computes it
   if (cbpd.cmp_insn == nil) then
      -- Looking for the last CMP instruction
      for i = #insns, 1, -1 do
	 if (insns[i]:get_family() == Consts.FM_CMP) then
	    cbpd.cmp_insn = insns[i];
	    break;
	 end
      end
   end
   local cmp_insn = cbpd.cmp_insn;

   -- If no CMP instruction found
   if (cmp_insn == nil) then return false end

   -- If the last CMP instruction is the input instruction
   if (cmp_insn:get_userdata_address() ==
       insn    :get_userdata_address()) then
      return true;
   end

   -- Checking whether the input instruction is modifying a register:
   -- * read by the last CMP instruction
   -- * or present in a memory operand of any instruction

   -- If no memoized data for matching_registers, computes it
   if (cbpd.matching_registers == nil) then
      cbpd.matching_registers = {};

      -- enqueue registers read by the last CMP instruction
      for _,oprnd_CMP in pairs (cmp_insn:get_operands()) do
	 if (oprnd_CMP ["type"] == Consts.OT_REGISTER) then
            if (cbpd.matching_registers [oprnd_CMP ["family"]] == nil) then
               cbpd.matching_registers [oprnd_CMP ["family"]] = {}
            end
	    cbpd.matching_registers [oprnd_CMP ["family"]] [oprnd_CMP ["name"]] = true;
	 end
      end

      -- enqueue registers present in memory operands
      for _,i in pairs (insns) do
	 local mem_oprnd = i:get_first_mem_oprnd();
	 if (mem_oprnd ~= nil) then
	    for _,sub_oprnd in pairs (mem_oprnd ["value"]) do
	       if (sub_oprnd ["type"] == Consts.OT_REGISTER) then
                  if (cbpd.matching_registers [sub_oprnd ["family"]] == nil) then
                     cbpd.matching_registers [sub_oprnd ["family"]] = {}
                  end
		  cbpd.matching_registers [sub_oprnd ["family"]] [sub_oprnd ["name"]] = true;
	       end
	    end
	 end
      end
   end
   local matching_registers = cbpd.matching_registers;

   -- for each register written by the current instruction
   for _,oprnd_cur in pairs (insn:get_operands()) do
      if (oprnd_cur ["type" ] == Consts.OT_REGISTER and
   	  oprnd_cur ["write"] == true) then

	 -- if matching an address calculation register
	 if (matching_registers [oprnd_cur ["family"]] ~= nil and
             matching_registers [oprnd_cur ["family"]] [oprnd_cur ["name"]] == true) then
	    return true;
	 end
      end
   end

   return false;
end

function insn:can_be_packed_INT (insns, arch, uarch_consts, reset_memoized_data)
   if (reset_memoized_data) then
      cannot_be_packed_data.cmp_insn           = nil;
      cannot_be_packed_data.matching_registers = nil;
   end

   if (arch == "k1om") then return self:is_SIMD_NOT_FP() end

   -- If the current instruction is FP
   if (self:is_FP()) then return false end

   -- If the current instruction is packed integer
   if (self:is_SIMD_NOT_FP()) then return true end

   -- From here, we can suppose that the current instruction is general purpose

   -- If the current instruction is part of loop control or address calculation (not vectorizable)
   if (cannot_be_packed (self, insns, reset_memoized_data)) then return false end

   -- Some general purpose instructions can not be packed.
   -- To avoid checking each instruction, we use their class.
   local class = self:get_class();
   if ((class == Consts.C_CONTROL) or
       (self:is_struct_or_str()) or
       (class == Consts.C_OTHER) or
       (class == Consts.C_MOVE and
	string.find (self:get_name(), "MOV") == nil)) then
      return false;
   end

   return true;
end

-- TODO: put local as soon as "main function has more than 200 local variables" issue fixed
function insn:can_be_packed_FP (arch, uarch_consts)
   local FP_element_size = self:get_input_element_size();
   local can_FP_size_be_packed = false;

   if (uarch_consts["Vectorizable FP sizes"] ~= nil and uarch_consts["Vectorizable FP sizes"][FP_element_size] ~= nil) then
      can_FP_size_be_packed = uarch_consts["Vectorizable FP sizes"][FP_element_size];
   end

   return (self:is_SIMD_FP() or (self:is_FP() and can_FP_size_be_packed));
end

--- Checks whether an instruction can be packed (or is already packed)
-- @param mode "INT", "FP" or "INT+FP", allows to consider as packable only INT or FP instructions
-- @param insns instructions sequence, to ignore loop control instructions
-- @param uarch_consts Micro-architecture properties
-- @return boolean
function insn:can_be_packed (mode, insns, uarch_consts)
   if (mode == "INT") then return self:can_be_packed_INT (insns, arch, uarch_consts) end
   if (mode == "FP" ) then return self:can_be_packed_FP  (arch, uarch_consts       ) end

   if (mode == "INT+FP") then
      return (self:can_be_packed_INT (insns, arch, uarch_consts) or
	      self:can_be_packed_FP  (arch, uarch_consts           ));
   end

   Debug:error ("insn:can_be_packed: invalid mode value (valid: \"INT\", \"FP\" and \"INT+FP\"");
end
