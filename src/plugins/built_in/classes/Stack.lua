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

---	@class Stack Class
--	Stack operations : pop, push, size, 

Stack = {};

---	Creates a new Stack object
--	@return New Stack object
function Stack:new()
	stack = {};	
	stack.nb_elem = 0;
	stack.queue = Queue:new();
	setmetatable(stack,self);
	self.__index = self;
	return stack;
end

---	Pushes a given value onto the stack
--	@param val Value to push
function Stack:push(val)
	self.queue:add_head(val);
	self.nb_elem = self.nb_elem + 1;
end

---	Pops top value from the stack
--	@return Value on top of stack
function Stack:pop()
	self.nb_elem = self.nb_elem -1;
	return self.queue:del_head();
end

---	Gets value from top of the stack without poping it
--	@return Value on top of stack
function Stack:top()
	return self.queue:pick_head();
end

---	Gets the stack's size
--	@return Stack size
function Stack:size()
	return self.nb_elem;
end

---	Tests if the stack is empty or not
--	@return True or false
function Stack:is_empty()
	if(self.nb_elem > 0) then
		return false;
	else
		return true;
	end
end
