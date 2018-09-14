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

module("loop.mt",package.seeall)
--[[
Cache system for loops. 
When calling loop:get_<name>(), if the function is not defined, 
then it calls the first time the function self:compute_<name>. 
The value returned is cached in an array and returned. 
Following calls will directly return the cached value.

This mechanism is transparent for all other functions that never use
the array loop used for cache.
--]]
cache = {
	__index = function(t,k)
		if (string.find(k,"get_")==1) then
			local name=string.sub(k,5)
			if (name=="profile") then return function() return nil end end
			return 	function(l)
				local id = l:get_id()
				if(loop.cache == nil) then loop.cache = {} end
				local hash = l:get_function():get_asmfile():get_hash();
				if(loop.cache[hash] == nil) then loop.cache[hash] = {} end
				if(loop.cache[hash][id] == nil) then loop.cache[hash][id] = {} end
				if(loop.cache[hash][id][name] == nil) then
					if(t["compute_"..name] ~= nil) then
						--print("calling compute_"..name)
						loop.cache[hash][id][name] = t["compute_"..name](l)
					else
						return nil;
					end
				end
				return loop.cache[hash][id][name];
			end
		end
	end
}

setmetatable(loop,cache)

