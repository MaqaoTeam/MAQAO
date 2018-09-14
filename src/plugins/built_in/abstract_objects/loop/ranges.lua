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

module("loop.ranges",package.seeall)


function sort_blocks(block_one,block_two)
	for insn in block_one:instructions() do
	        insn_start_one = insn:get_address();
	        break;
	    end;
	for insn in block_two:instructions() do
	        insn_start_two = insn:get_address();
	        break;
	    end;

return insn_start_one < insn_start_two;
end


function loop:get_ranges()

	local start = -1;
	local stop = -1;
	local block,insn
	
	local loop_ranges = Table:new();
	local blocks_ranges = Table:new();	

		
	for block in self:blocks() do
		-- only add blocks that belong only to this loop
		if (block:get_loop():get_id() == self:get_id()) then
			blocks_ranges:insert(block);
		end
	end
	--Sort block by address
	table.sort(blocks_ranges,sort_blocks)
	
	--if you want to print the sorted table
--	for _,block in pairs(blocks_ranges) do
--		for insn in block:instructions() do
--			print (block);
--			print(block:get_first_insn():get_address());	
--			print(block:get_last_insn():get_address());
--		end	
--	end
	
	local insn_start;
	local insn_stop;
	

	for _,block in pairs(blocks_ranges) do
		--initialization for a new bloc entry
		if (insn_start == nil) then
			insn_start = block:get_first_insn();
			insn_stop = block:get_last_insn();
		else
			--Debug:info ("NEXT INSTRUCTION : "..insn_stop:get_address().." -> "..
			--insn_stop:get_next():get_address().."?=? "..block:get_first_insn():get_address());
			if ( insn_stop:get_next():get_address() ~= block:get_first_insn():get_address()) 
			then -- those two blocks are not continue
				range = {start=insn_start:get_address(),stop=insn_stop:get_address()};
				loop_ranges:insert(range);
				insn_start = block:get_first_insn();
				insn_stop = block:get_last_insn();
			else -- those two blocks are continue
				insn_stop = block:get_last_insn();
			end
		end
	end
	
	-- We have this case if all blocks are continue
	-- or if we finish with only one block 
	if (insn_start ~= nil) then	
		range = {start=insn_start:get_address(),stop=insn_stop:get_address()};
		loop_ranges:insert(range);
	end
return loop_ranges;
end

