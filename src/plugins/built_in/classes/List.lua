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

---	@class List Class

List = {};
local ListMeta = {};

--ListMeta.__tostring = List.tostring;
ListMeta.__index 	 = List;
ListMeta.name		 = "List";
---	Creates a list data structure
--	@return New instance
function List:new(data)
	list = {};
	list.data = data;
	list.prev = nil; 
	list.next = nil; 
	setmetatable(list,ListMeta);

	return list;
end

function List:size()
	return self.nb_elem;
end

function List:add_head(data)	
	local old_self = self;	
	
	self = List:new(data);
	old_self.next = self; 
	self.prev = old_self;	
	
	return self;
end

function List:del_head(data)	
	local old_self = self;	
	
	self = self.prev;
	old_self:free();
	
	return self;
end

function List:rfree()
	local lprec = self.prev;
	local lnext = self.next;
	local tmp;
	
	self:free();
	while(lprev ~= nil)
	do
		tmp = lprec.prev;
		lprev:free();
		lprev = tmp;
	end		
	while(lnext ~= nil)
	do
		tmp = lnext.next;
		lnext:free();
		lnext = tmp;		
	end		
end

function List:free()
	self.data = nil;
	self.prev = nil; 
	self.next = nil; 
	self = nil;
end

function List:traverse()
	local start = self;
	
	while(start ~= nil)
	do
		print(start.data);
		start = start.prev;
	end
end

function List:size()
	local start = self;
	local size = 0;
	
	while(start ~= nil)
	do
		size = size + 1;
		start = start.prev;
	end
	
	return size;
end
