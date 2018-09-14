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

---	@class Queue Class

Queue = {};
local QueueMeta = {};

--TableMeta.__tostring = Queue.tostring;
QueueMeta.__index 	 = Queue;
QueueMeta.name		 = "Queue";

---	Creates a queue data structure
--	@return New instance

function Queue:new()
	queue = {};
	queue.head = nil;
	queue.tail = nil; 
	queue.nb_elem = 0;
	setmetatable(queue,QueueMeta);

	return queue;
end

function Queue:add_head(data)
	local temp_head = self.head;
	
	self.head = List:new(data);
	--If Queue empty head = tail
	if(temp_head == nil) then
		self.tail = self.head;
	else
		temp_head.prev = self.head;
	end
	self.head.next = temp_head;	
	self.nb_elem = self.nb_elem + 1;
end

function Queue:del_head()
	local head_temp = self.head;
	local temp_data = self.head.data;
		
	--Removing last element
	if(self.head.next == nil) then
		self.head = nil;
		self.tail = self.head;
	else		
		self.head = self.head.next;
		self.head.prev = nil;
	end
	head_temp:free();
	self.nb_elem = self.nb_elem - 1;
	
	return temp_data;
end

function Queue:pick_head()
	return self.head.data;
end

function Queue:add_tail(data)
	local temp_tail = self.tail;
	
	self.tail = List:new(data);
	--If Queue empty head = tail
	if(temp_tail == nil) then
		self.head = self.tail;
	else
		temp_tail.next = self.tail;	
	end	
	self.tail.prev = temp_tail;
	self.nb_elem = self.nb_elem + 1;	
end

function Queue:del_tail()
	local tail_temp = self.tail;
	local temp_data = self.tail.data;
	
	--Removing last element
	if(self.tail.prev == nil) then
		self.tail = nil;
		self.head = self.tail;
	else		
		self.tail = self.tail.prev;
		self.tail.next = nil;
	end
	tail_temp:free();
	self.nb_elem = self.nb_elem - 1;	
	
	return temp_data;
end

function Queue:pick_tail()
	return self.tail.data;
end

function Queue:length()
	return self.nb_elem;
end

function Queue:traverse()
	self.tail:traverse();
end
