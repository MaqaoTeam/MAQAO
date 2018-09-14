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

---	Module dealing with dominance<br>
--	This module provides you with functions
--	that allows you to retrieve dominance information
--	about blocks

module("block.dominance",package.seeall)

---	This function is used to find an instruction 
--	given a block and a register name string
--	Note: If some instructions use registers for assignments
--	then the return value will be nil; ex : reg_name = some_reg
--	@param block pointer
--	@param register name string<br> 
--	NOTE: On some architectures a register can have alternate
--	names. Be sure to provide the "real" name and not the alternate one. 
--	@return an instruction object else nil
function block:dom_get_insn_by_reg(reg)

	local parent;
	
	if(type(self) ~= "userdata") then
		print("You submited an incorrect block information");
		return nil;
	end
	if(type(reg) ~= "string") then
		print("Verify the register parameter : it must be a string");
		return nil;
	end
	parent = self:get_imm_dominator();
	if(parent ~= nil) 
	then
		while(parent ~= nil)
		do	
			for ins in parent:instructions()
			do
				local reg_names = ins:get_registers_name();
				for _,reg_name in pairs(reg_names)
				do
					if (string.match(reg_name,reg) ~= nil) then
						return ins;
					end
				end
			end
			parent = parent:get_imm_dominator();
		end
	end
	
	return nil;
end

---	This function is used to find an instruction 
--	given a block and a mnemonic string
--	@param block pointer
--	@param mnemonic string  
--	@return an instruction object else nil
function block:dom_get_insn_by_mnemo(pmnemo)

	local parent;
	local son;
		
	if(type(self) ~= "userdata") then
		print("You submited an incorrect block information");
		return nil;
	end
	if(type(pmnemo) ~= "string") then
		print("Verify the mnemonic parameter : it must be a string");
		return nil;
	end
	parent = self:get_imm_dominator();
	if(parent ~= nil) 
	then
		while(parent ~= nil)
		do	
			for ins in parent:instructions()
			do
				local mnemo = ins:get_name();
				if(type(mnemo) == "string") then
					if (string.match(pmnemo,mnemo) ~= nil) then
						return ins;
					end
				end
			end
			parent = parent:get_imm_dominator();
		end
	end
	
	return nil;

end
