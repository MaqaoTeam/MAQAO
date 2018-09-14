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

---@class Table Class

Table = {};
local TableMeta = {};

--[[ Funtion new
IN: Existing table or emty
OUT: Returns a new Table instance
--]]
function Table:new(existing_table)
	local table = existing_table or {};
	setmetatable(table,TableMeta);
	return table;
end

-----------------------------------------------------------
--#Name : insert
--#Description : inserts an element into current table (table.insert)
-----------------------------------------------------------

function Table:insert(elem)
	if(type(self) ~= "table") then
		return 0;
	end
	table.insert(self,elem);
end

-----------------------------------------------------------
--#Name : get_size
--#Description : returns the number of elements added with 
--               the insert() function or by assignment []
-----------------------------------------------------------
function Table:get_size()
	return Table:count_rows(self);
end

function Table:count_rows(table)
	local count=0;
	if(type(table) ~= "table") then
		return 0;
	end
	for elem in pairs(table)
	do
		count = count + 1;
	end
	return count;
end

-----------------------------------------------------------
--#Name : count_table_rows
--#Description : Count the number of int indexes defined
-----------------------------------------------------------
function Table:count_int_rows(table)
	local count=0;
	if(type(table) ~= "table") then
		return 0;
	end
	for elem in pairs(table)
	do
		if(type(elem) == "number") then
			count = count + 1;
		end
	end
	return count;
end

-----------------------------------------------------------
--#Name : count_table_rows
--#Description : Count the number of string indexes defined
-----------------------------------------------------------
function Table:count_str_rows(table)
	local count=0;
	if(type(table) ~= "table") then
		return 0;
	end
	for elem in pairs(table)
	do
		if(type(elem) == "string") then
			count = count + 1;
		end
	end
	return count;
end

-----------------------------------------------------------
--#Name : count_table_rows
--#Description : Count the number of indexes defined
-----------------------------------------------------------
function Table:count_table_rows(table)
	local count=0;
	if(type(table) ~= "table") then
		return 0;
	end
	for elem in pairs(table)
	do
		if(type(elem) == "table") then
			count = count + 1;
		end
	end
	return count;
end

-----------------------------------------------------------
--#Name : contains
--#Description : Verifies if the table contains the given 
--               element value
-----------------------------------------------------------
function Table:containsv(element)
	for _,v in pairs(self)
	do
		if(v == element) then
			return true;
		end
	end
	return false;
end

-----------------------------------------------------------
--#Name : contains
--#Description : Verifies if the table contains the given
--               key value
-----------------------------------------------------------
function Table:containsk(element)
        for k in pairs(self)
        do
                if(k == element) then
                        return true;
                end
        end
        return false;
end

-----------------------------------------------------------
--#Name : get_index_value
--#Description : Return the index corresponding to
--               the given value, if it exists in the table
-----------------------------------------------------------
function Table:get_index_valueof(element)

	for k,v in pairs(self)
	do
		if(type(element) == "userdata") then
			if(get_userdata_address(element) == get_userdata_address(v)) then
				return k;
			end
		else
		    if(v == element) then
		    	return k;
		    end
		end
	end
	return nil;
end

-----------------------------------------------------------
--#Name : table.index.max
--#Description : returns the greatest index value
-----------------------------------------------------------
function Table:index_max()
	local maxi = 0;
	for index,value in pairs(self)
	do
		if(value > maxi) then
			maxi = value;
		end
	end
	return maxi;
end

-----------------------------------------------------------
--#Name : table.index_add
--#Description : returns the sum of index values
-----------------------------------------------------------
function Table:index_add()
	local add = 0;
	for index in pairs(self)
	do
		add = add + self[index];
	end
	return add;
end

-----------------------------------------------------------
--#Name : sort
--#Description : basic sort (table.sort)
-----------------------------------------------------------

function Table:sort()
        if(type(self) ~= "table") then
                return 0;
        end
        table.sort(self);
end

--TODO : add Table:tostring extra features to Table:serialize and use only Table:serialize in Table:tostring 
--Print Table------------------------------
function Table:tostring(table)
	local table_toprint = nil;
	local name = "Table";
	local processed = {};
	
	if(table ~= nil) then
		table_toprint = table;
	else
		table_toprint = self;
	end

	if(type(table_toprint) ~= "table") then
		print("WARNING : Given object to print is not a table: instead got "..type(table_toprint));
		return nil;
	end

	print(name.." = ".."\n{");
	for k,v in pairs(table_toprint)
	do
		self:__table_tostring(k,v,1,processed);
	end
	print("};");		

	processed = nil;
	
	return "";--Return a string (empty) to be strict 
end

function Table:__table_tostring(name,object,indent,processed)

	if(type(object) == "nil") then
		return;
	end

	if(type(object) == "table" and processed[object]) then
		print(self:get_indent(indent).."[\""..name.."\"] = Ref: "..tostring(object)..";");
		return;
	--Special case to avoid cycling on the special _M entry : metatable entry points on the associated object  
	elseif(type(object) == "table" and name == "_M") then
		print(self:get_indent(indent).."[\"Ref:_M\"];");
		return;
	--Special case to avoid cycling on the special _G entry : global Lua registry object  		
	elseif(type(object) == "table" and name == "_G") then
		print(self:get_indent(indent).."[\"Ref:_G\"];");
		return;		
	else
		processed[object] = true;
	end
	if(type(object) == "number") then
		if(type(name) == "number") then
			print(self:get_indent(indent).."["..name.."] = "..object..";");
		else
			print(self:get_indent(indent).."[\""..tostring(name).."\"] = "..object..";");
		end
	elseif(name == nil) then
			print(self:get_indent(indent).."[\"nil\"] = nil;");
	elseif(type(object) == "boolean") then
		if(type(name) == "number") then
			print(self:get_indent(indent).."["..name.."] = "..tostring(object)..";");
		elseif(type(name) == "userdata") then
			print(self:get_indent(indent).."[\""..tostring(name).."\"] = "..tostring(object)..";");
                else
			print(self:get_indent(indent).."[\""..name.."\"] = "..tostring(object)..";");
		end		
	elseif(type(object) == "userdata" or type(object) == "function") then
		if(type(name) == "number") then
			print(self:get_indent(indent).."["..name.."] = "..tostring(object)..";");
		elseif(type(name) == "userdata") then
			print(self:get_indent(indent).."[\""..tostring(name).."\"] = "..tostring(object)..";");
                else
			print(self:get_indent(indent).."[\""..name.."\"] = "..tostring(object)..";");
		end
	elseif(type(object) == "string") then
		if(type(name) == "number") then
			print(self:get_indent(indent).."["..name,"] = "..string.format("%q",object)..";");
		else
			print(self:get_indent(indent).."[\""..name.."\"] = "..string.format("%q",object)..";");
		end
	elseif(type(object) == "table") then
		if(type(name) == "number") then
			print(self:get_indent(indent).."["..name.."] = ".."\n"..self:get_indent(indent).."{");
		elseif(type(name) == "userdata" or type(name) == "function" or type(name) == "table") then
			print(self:get_indent(indent).."[\""..tostring(name).."\"] = ".."\n"..self:get_indent(indent).."{");
		else
			print(self:get_indent(indent).."[\""..name.."\"] = ".."\n"..self:get_indent(indent).."{");
		end

		indent = indent + 1;
		for k,v in pairs(object)
		do
			self:__table_tostring(k,v,indent,processed);
		end
		indent = indent - 1;
		print(self:get_indent(indent).."};\n");	
	end
end

function Table:get_indent(depth)
	local i;
	local indent = "";

	for i=1,depth,1
	do 
		indent = indent.."\t";
	end
	return indent;
end	

--------.CLONE--------------------------------
function Table:clone(table)
	local clone = Table:new();
        local t;

	if(type(self) ~= "table" or self == Table) then
           if(type(table) ~= "table") then
              return nil;
           else
              t = Table:new(table);
           end
        else
           t = self;
	end

	for name,object in pairs(t)
	do
		self:__table_clone(clone,name,object);
	end
	
	return clone;
end

function Table:__table_clone(clone,name,object)
	if(type(object) == "table") then
		clone[name]=Table:new();
		for k,v in pairs(object)
		do
			self:__table_clone(clone[name],k,v);
		end
	else
		clone[name] = object;
	end
end
--------MERGE--------------------------------
function Table:merge(table2)
	if(type(self) ~= "table" or type(table2) ~= "table") then
		return nil;
	end
	
	for name,object in pairs(table2)
	do
		self:__table_clone(self,name,object);
	end
	--print("printing merge");
	--table.tostring(merge);
end

-------OPAIRS-----------------------------------------------
function __genOrderedIndex(t)
	local orderedIndex = {};
	--eprint(">>__genOrderedIndex param type "..type(t));
	if(t == nil) then
		return orderedIndex;	
	end
	
	for key in pairs(t) do
		if(type(key) == "number") then
			table.insert(orderedIndex,key);
		end
	end
	table.sort(orderedIndex);
	--table.tostring(orderedIndex);
	return orderedIndex;
end

function orderedNext(t, state)
	-- Equivalent of the next function, but returns the keys in the alphabetic
	-- order. We use a temporary ordered key table that is stored in the
	-- table being iterated.

	if(t == nil) then
		--eprint(">orderedNext param type "..type(t).." and state = "..type(state));
		key = nil;
		return;
	end
	
	--print("orderedNext: state = "..tostring(state) )
	if state == nil then
		-- the first time, generate the index
		t.__orderedIndex = __genOrderedIndex(t);
		key = t.__orderedIndex[1];
		--print("FROM STATE : key = "..key..", t[key] = "..t[key]);
		return key, t[key]
	end
	-- fetch the next value
	key = nil
	if(t.__orderedIndex ~= nil) then
		for i = 1,table.getn(t.__orderedIndex) do
			--Find previous key;
			if t.__orderedIndex[i] == state then
				key = t.__orderedIndex[i+1]
			end
		end
	end

	if key then
		--print("FROM key : key = "..key..", t[key] = "..t[key]);
		return key, t[key]
	end
	
	--print("no more value to return, cleanup");
	t.__orderedIndex = nil;

	return;
end

-- Equivalent of the pairs() function on tables. Allows to iterate
-- in (index) order. index must be a number
function opairs(t)
		if(type(t) ~= "table") then
			--print("Passed wrong argument type to opair : Expected table, got "..type(t));
		end
		return orderedNext, t, nil
end

-- Same as opairs() but extends sort to string indexes
function apairs(t)
	local a = {};
	local i = 0;      -- iterator variable

	if(type(t) == "nil") then return function () return end;	end;

	for n in pairs(t) 
	do 
	    table.insert(a, n);
	end
	table.sort(a);
	
	local iter = function ()   -- iterator function
	    i = i + 1;
	    if a[i] == nil then 
		  return nil;
	    else 
		  return a[i], t[a[i]];
	    end
	end
	return iter;
end

function tpairs(t)
	if(type(t) == "nil") then return function () return end;	end;

	local iter = function ()   -- iterator function
		local k,v;
		
		k,v = next(t,k);
		 
		if k then 
			return k, v 
		end
	end
	
	return iter;
end


function Table:is_not_default(name)
	
	if (name == "_G") then
	elseif (name == "os") then
	elseif (name == "string") then
	elseif (name == "math") then
	elseif (name == "package") then
	elseif (name == "debug") then
	elseif (name == "io") then
	elseif (name == "coroutine") then
	elseif (name == "table") then
	elseif (name == "_VERSION") then
	elseif (name == "__size") then
	else
		return true;
	end

	return false;
end

--TODO : takes modifs from Table:tostring() (recursive references handling) and use serialize instead
function Table:serialize_table(name,object,indent)

	if(type(object) == "nil") then
		return;
	end

	if(type(object) == "number" and self:is_not_default(name)) then
		if(type(name) == "number") then
			io.write("["..name.."] = "..object..";\n");
		else
			io.write("[\""..name.."\"] = "..object..";\n");
		end
	elseif(type(object) == "boolean" and self:is_not_default(name)) then
		if(type(name) == "number") then
			io.write("["..name.."] = "..tostring(object)..";\n");
		else
			io.write("[\""..name.."\"] = "..tostring(object)..";\n");
		end		
	elseif(type(object) == "string" and self:is_not_default(name)) then
		if(type(name) == "number") then
			io.write("["..name.."] = "..string.format("%q",object)..";\n");
		else
			io.write("[\""..name.."\"] = "..string.format("%q",object)..";\n");
		end
	elseif(type(object) == "table" and self:is_not_default(name)) then
		local meta = getmetatable(object);
		local txts = "";local txte = "";
--		if(meta ~= nil and meta.name == "Table") then
--			txts = "Table:new(";txte = ")";
--		end		
		if(type(name) == "number") then
			io.write("["..name.."] = ".."\n"..self:get_indent(indent)..txts.."{\n");
		else
			io.write("[\""..name.."\"] = ".."\n"..self:get_indent(indent)..txts.."{\n");
		end

		indent = indent + 1;
		for k,v in pairs(object)
		do
			io.write(self:get_indent(indent));
			self:serialize_table(k,v,indent);
		end
		indent = indent - 1;
		io.write(self:get_indent(indent).."}"..txte..";\n");
	elseif(type(object) == "function" and self:is_not_default(name)) then
		local meta = debug.getinfo(object);
		--Table:tostring(meta);
		if(meta.what == "Lua") then
			io.write("[\""..name.."\"] = "..string.format("loadstring(%q);",string.dump(object)));
		end			
	end
end

function Table:serialize(name,object,string_mode)
	
	if(type(object) == "nil") then
		return;
	else
		local milRT_str_dump = "";
		local write = function(string) milRT_str_dump = milRT_str_dump..string end
	
		if(not string_mode == false) then
			io.write = write
		end
			
		if(type(object) ~= "table") then
			print("WARNING : Given object to print is not a table: instead got "..type(table_toprint));
			return nil;			
		end
		
		local meta = getmetatable(object);
		if(meta ~= nil and meta.name == "Table") then
			io.write(name.." = ".."\nTable:new({\n");
		else
			io.write(name.." = ".."\n{\n");
		end
		for k,v in pairs(object)
		do
			io.write("\t");
			self:serialize_table(k,v,1);
		end
		if(meta ~= nil and meta.name == "Table") then
			io.write("});\n");
		else
			io.write("};\n");
		end
		
		if(not string_mode == false) then
			io.write = io.stdout;

			return milRT_str_dump;
		end
	end
end

--TODO : not working => try using addresses
function Table:name()
	--local addr = tostring(self);
	 
	for name,object in pairs(_G)
	do
		print(name);
		if(object == self) then
				return name;
		end
	end
end

function Table:save_output(output_path,name)

	io.output(output_path);
    self:serialize(name,self);
    io.close();
end

function Table:save_luaState(output_path)

	print("Dumping to "..output_path);
	io.output(output_path);
	for object in pairs(_G)
	do
		self:serialize(object,_G[object]);
	end
	io.close();
end

TableMeta.__tostring = Table.tostring;
TableMeta.__index    = Table;
TableMeta.name	     = "Table";

