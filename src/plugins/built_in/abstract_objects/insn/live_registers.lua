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

module ("insn.live_registers", package.seeall)

--- Return the list of registers to save and restore for the instrumentation 
-- of the instruction insn.
-- @param insn instruction pointer which is instrumented
-- @param pos specify if the call must be inserted before or after "insn"
function insn:get_live_registers(pos) 

   local insn = self

	-- TODO Use a table with the pointer as index
	local b = insn:get_block();
	local a = b:get_asmfile();
	local registers_table = a:get_arch_registers();
	local live_registers = {};
	local func = b:get_function();

	if (b:is_padding()) then
		-- It is a padding block (Should not happen)
		return nil;
	else
		local block_exits = 0;
		local internal_calls = 0;
		
		-- Checking that there is no indirect branch
		-- This test is made because of the live analysis which cannot work with such elements
		for k, i in pairs(func:get_exitsi()) do
		   -- Checking for indirect branch
			if ( i:is_exit_potential() or ( (i:is_call() or i:is_exit_early()) and (i:get_label_name() == nil) ) ) then 
				block_exits = block_exits + 1; 
			end
		end
		
		if (block_exits > 0 ) then
			-- There is at least 1 indirect branch
			Debug:info("live analysis aborted due to the presence of an indirect branch");
			return nil;
		end
			
		-- Live registers for a loop
		if (b:is_loop_entry() and not (insn:is_call()) and not (insn:is_exit_early()) 
				and (insn:get_address() == b:get_first_insn():get_address())) then
			local loop = b:get_loop();
			local nb_reg = func:analyze_live_registers(true);
			
			-- Iterating on each block entry
			for i,b2 in pairs(loop:get_entries()) do
						
				-- Living registers between blocks
				local block_live_registers = func:get_live_registers(b2,nb_reg);
				if (block_live_registers == nil) then 
					-- Live analysis failed
					Debug:info("live analysis failed");
					return nil; 
				end
				for k,v in pairs(block_live_registers) do
					if (registers_table[k] == 0 and 
					(v == Consts.IN_FLAG or v == Consts.OUT_FLAG or v == (Consts.IN_FLAG+Consts.OUT_FLAG))) then
						local found = false;
						for k2,v2 in pairs(live_registers) do
							if (a:get_register_from_id(k-1) == v2) then found = true; break; end
						end
						if (found == false) then table.insert(live_registers,a:get_register_from_id(k-1)); end	
					end	
				end
				
				-- Living registers in the block
				block_live_registers = b:get_defined_registers(insn);
				for k,v in pairs(block_live_registers) do
					local found = false;
					for k2,v2 in pairs(live_registers) do
						if (a:get_register_from_id(v) == v2) then found = true; break; end
					end
					if (found == false) then table.insert(live_registers,a:get_register_from_id(v)); end
				end
			end

		-- Live registers for other cases
		else
			-- The instruction is a call: 
			-- we include arguments registers and/or return registers
			if (insn:is_call() or insn:is_exit_early()) then
			
				-- Living registers between blocks
				local nb_reg = func:analyze_live_registers(true);
				local block_regs = func:get_live_registers(b,nb_reg);
				if (block_regs == nil) then 
					-- Live analysis failed
					Debug:warn("live analysis failed");
					return nil; 
				end
				for k,v in pairs(block_regs) do
					-- Warning: it is based on the fact that a call is always the last instruction of a block.
					-- Instrumentation before the call, we need IN & OUT registers
					if (pos == mil.POSITIONT.BEFORE) then
						if (registers_table[k] == 0 and 
						(v == Consts.IN_FLAG or v == Consts.OUT_FLAG or v == (Consts.IN_FLAG+Consts.OUT_FLAG))) then
							local found = false;
							for k2,v2 in pairs(live_registers) do
								if (a:get_register_from_id(k-1) == v2) then found = true; break; end
							end
							if (found == false) then table.insert(live_registers,a:get_register_from_id(k-1)); end
						end
					-- Instrumentation after the call, we need OUT registers
					elseif (pos == mil.POSITIONT.AFTER) then
						if (registers_table[k] == 0 and 
						(v == Consts.OUT_FLAG or v == (Consts.IN_FLAG+Consts.OUT_FLAG))) then
							local found = false;
							for k2,v2 in pairs(live_registers) do
								if (a:get_register_from_id(k-1) == v2) then found = true; break; end
							end
							if (found == false) then table.insert(live_registers,a:get_register_from_id(k-1)); end
						end
					end
				end
				
				-- Instrumentation before the call, we add ABI argument registers
				if (pos == mil.POSITIONT.BEFORE) then
					local arg_regs = a:get_arg_registers();
					for k,v in pairs(arg_regs) do
						local found = false;
						for k2,v2 in pairs(live_registers) do
							if (v == v2) then found = true; break; end
						end
						if (found == false) then table.insert(live_registers,v); end				
					end
				-- Instrumentation after the call, we add ABI return registers and arguments registers
				elseif (pos == mil.POSITIONT.AFTER) then
					local ret_regs = a:get_ret_registers();
					for k,v in pairs(ret_regs) do
						local found = false;
						for k2,v2 in pairs(live_registers) do
							if (v == v2) then found = true; break; end
						end
						if (found == false) then table.insert(live_registers,v); end				
					end
				end
				
				-- Living registers in the block
				if (pos == mil.POSITIONT.BEFORE) then
					block_regs = b:get_defined_registers(insn);
				elseif ((pos == mil.POSITIONT.AFTER) and (insn:get_next() ~= nil)) then
					block_regs = b:get_defined_registers(insn:get_next());
				else 
					return nil;
				end
				
				for k,v in pairs(block_regs) do
					local found = false;
					for k2,v2 in pairs(live_registers) do
						if (a:get_register_from_id(v) == v2) then found = true; break; end
					end
					if (found == false) then table.insert(live_registers,a:get_register_from_id(v)); end
				end
				
			-- Instruction is the first instruction of the function and we insert the call before 
			elseif ((insn:get_address() == b:get_first_insn():get_address()) and (func:get_entry():get_id() == b:get_id()) 
					and (pos == mil.POSITIONT.BEFORE)) then
			
				local arg_regs = a:get_arg_registers();
								
				for k,v in pairs(arg_regs) do
					local found = false;
					for k2,v2 in pairs(live_registers) do
						if (v == v2) then found = true; break; end
					end
					if (found == false) then table.insert(live_registers,v); end				
				end
									
			-- Instruction is the last instruction of the block
			elseif (insn:get_address() == b:get_last_insn():get_address() and pos == mil.POSITIONT.AFTER) then
								
				-- Living registers between blocks
				local nb_reg = func:analyze_live_registers(true);
				local block_regs = func:get_live_registers(b,nb_reg);
				if (block_regs == nil) then 
					-- Live analysis failed
					return nil; 
				end
				for k,v in pairs(block_regs) do
					-- Instrumentation after the last instruction, we need OUT registers
					if ((registers_table[k] == 0) and 
					((v == Consts.OUT_FLAG) or (v == (Consts.IN_FLAG+Consts.OUT_FLAG)))) then
						local found = false;
						for k2,v2 in pairs(live_registers) do
							if (a:get_register_from_id(k-1) == v2) then found = true; break; end
						end
						if (found == false) then 
							table.insert(live_registers,a:get_register_from_id(k-1)); 
						end
					end
				end
										
			-- Instruction is the first instruction of the block
			elseif (insn:get_address() == b:get_first_insn():get_address() and pos == mil.POSITIONT.BEFORE) then
						
				-- Living registers between blocks
				local nb_reg = func:analyze_live_registers(true);
				local block_regs = func:get_live_registers(b,nb_reg);
				if (block_regs == nil) then 
					-- Live analysis failed
					return nil; 
				end
				
				for k,v in pairs(block_regs) do
					-- Instrumentation before the first instruction, we need IN registers
					if ((registers_table[k] == 0) and 
					((v == Consts.IN_FLAG) or (v == (Consts.IN_FLAG+Consts.OUT_FLAG)))) then
						local found = false;
						for k2,v2 in pairs(live_registers) do
							if (a:get_register_from_id(k-1) == v2) then found = true; break; end
						end
						if (found == false) then table.insert(live_registers,a:get_register_from_id(k-1)); end
					end
				end
									
			-- The instruction is elsewhere
			else
			
				-- Living registers between blocks
				local nb_reg = func:analyze_live_registers(true);
				local block_regs = func:get_live_registers(b,nb_reg);
				if (block_regs == nil) then 
					-- Live analysis failed
					return nil; 
				end
								
				for k,v in pairs(block_regs) do
					if (registers_table[k] == 0 and 
					(v == Consts.IN_FLAG or v == (Consts.IN_FLAG+Consts.OUT_FLAG))) then
						local found = false;
						for k2,v2 in pairs(live_registers) do
							if (a:get_register_from_id(k-1) == v2) then found = true; break; end
						end
						if (found == false) then table.insert(live_registers,a:get_register_from_id(k-1)); end
					end
				end
							
				-- Living registers in the block
				if (pos == mil.POSITIONT.BEFORE) then
					block_regs = b:get_defined_registers(insn);
				elseif ((pos == mil.POSITIONT.AFTER) and (insn:get_next() ~= nil)) then
					block_regs = b:get_defined_registers(insn:get_next());
				else 
					return nil;
				end
									
				for k,v in pairs(block_regs) do
					local found = false;
					for k2,v2 in pairs(live_registers) do
						if (a:get_register_from_id(v) == v2) then found = true; break; end
					end
					if (found == false) then table.insert(live_registers,a:get_register_from_id(v)); end
				end
				
			end
			
		end		
	end
	return live_registers;
end

