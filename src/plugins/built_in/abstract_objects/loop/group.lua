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

module("loop.group",package.seeall)

function loop:get_group_lines (filter)
    local str = ""
    local str_opcode = ""
    local str_offset = ""
    local str_address = ""
    local str_sets = ""
    local table = Table:new ()
    local subtable = nil
    local index = 2
    
    if filter == nil then
        filter = 0
    end
        
    table[1] = "size;pattern;addresses;opcodes;offsets;increment status;increment;touched status;touched;touched no overlap;"
    table[1] = table[1].."touched no overlap per iteration;touched sets;"
    for g in self:groups() do 
        subtable = g:totable()
        str_opcode = ""
        str_offset = ""
        str_address = ""
		
		str_sets = "{"
		for i, set in ipairs (subtable.touched_sets) do
			if i ~= 1 then
				str_sets = str_sets..","
			end
			str_sets = str_sets.."{"..string.format("0x%x",set.start)..","..string.format("0x%x",set.stop).."}"
		end
		str_sets = str_sets.."}"
		        
        for i, ins in ipairs (subtable.insns) do  
            str_address = str_address..ins.insn:get_address().." "
            str_opcode  = str_opcode..ins.insn:get_name().." "
            str_offset  = str_offset..ins.offset.." "
        end      
        str_address = string.sub(str_address, 1, -2)
        str_opcode  = string.sub(str_opcode, 1, -2)
        str_offset  = string.sub(str_offset, 1, -2)
        
        str = subtable.size..";"..subtable.pattern..";"..str_address..";"..
            str_opcode..";"..str_offset..";"..
            subtable.increment_status..";"..subtable.increment..";"..
            subtable.memory_status..";"..subtable.number_accessed_bytes..";"..
            subtable.no_overlap_bytes..";"..subtable.head..";"..str_sets..";"

        table[index] = str
        index = index + 1
    end
    return (table)
end
