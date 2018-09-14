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

---	@class String Class

String = {};
String.empty = "";

--[[ Function new
IN: Default value for the string or nothing
OUT: Returns a new instance
--]]

function String:new(default_value)
	object = {};
	setmetatable(object,self);
	self.__index = self;
	self.str = String.empty;
	if(type(default_value) == "string" or type(default_value) == "number") then
		self:concat(default_value);
	end	
	return object;
end

--[[ Function get_tokenized_table
IN:	string including a seperator 
	separator token
	Delete the first character of the string before processing
        Delete the last character of the string before processing
OUT:	table containing the string's elements without separators
--]]
function String:get_tokenized_table(string,token,del_first_elem,del_last_elem, table)

	local start,ends;
	local counter = 2;
	local current_element;
	local tokenized_table = table;
	
	if(type(string) ~= "string") then
		return nil;
	end
	if (tokenized_table == nil) then
		tokenized_table = Table:new();
	end
	--print("Received string = "..string.." and token = |"..token.."|");
	if(del_first_elem == true) then
		string = string.sub(string,2);
	end
	if(del_last_elem == true) then
		string = string.sub(string,0,string.len(string)-1);
	end
	--print("With first and last changes : string = "..string);
	start = string.find(string,token);
	if(start == nil) then
		tokenized_table[1] = string;
	else
		current_element = string.sub(string,0,start-1);
		--print(">First token = "..current_element);
		tokenized_table[1] = current_element;
		start = start+1;
		ends = string.find(string,token,start+1);
		while(ends ~= nil)
		do
			current_element = string.sub(string,start,ends-1);
			--print(">Current token = "..current_element);
			tokenized_table[counter] = current_element;
			start = ends + 1;
			ends = string.find(string,token,start);
			counter = counter +1;
		end
		current_element = string.sub(string,start);
		--print(">Last token = "..current_element);
		tokenized_table[counter] = current_element;
	end
	
	return tokenized_table;
end

--[[ Function reversed
OUT: Return the current string reversed
--]]

function String:reversed()
	return string.reverse(self.str);
end

--[[ Function reverse
OUT: Return the current string object reversed
--]]

function String:reverse()
	self.str = string.reverse(self.str);
	return self;	
end

--[[ Function concat
OUT: Return the current string reversed
--]]

function String:concat(strcat)
	if(type(strcat) == "string" or type(strcat) == "number") then
		self.str = self.str..strcat;
	end
end

--[[ Function set
IN: Sets the current string
--]]

function String:set(str_rep)
	if(type(str_rep) == "string") then
		self.str = str_rep;
	elseif(type(str_rep) == "number") then
		self.str = tostring(str_rep);
	end
end

--[[ Function totable
OUT: table representation of the current string (each index = character)
--]]

function String:totable()
	local tab = Table:new();
	
	for i=1,string.len(self.str)
	do
		tab[i] = string.sub(self.str,i,i);
	end
	return tab;
end


--[[ Function tostring
OUT: Prints the current string
--]]

function String:tostring()
	print(self.str);
end

--[[ Function tostring
OUT: Returns the current string
--]]

function String:string()
	return self.str;
end

function String:split(str, pat)
   local t = {}  -- NOTE: use {n = 0} in Lua-5.0
   local fpat = "(.-)" .. pat
   local last_end = 1
   local s, e, cap = str:find(fpat, 1)
   while s do
      if s ~= 1 or cap ~= "" then
	 table.insert(t,cap)
      end
      last_end = e+1
      s, e, cap = str:find(fpat, last_end)
   end
   if last_end <= #str then
      cap = str:sub(last_end)
      table.insert(t, cap)
   end
   return t
end

