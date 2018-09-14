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

module("fct.multiple_loops",package.seeall)

function fct:get_multiple_loops()
   local multiple = Table:new()
   local loop
   local t,l
   for loop in self:loops() do
		if (loop:get_children()==nil) then
			l=loop:get_src_line()
	 		if (multiple[l]==nil) then multiple[l]={} end
	 		t=multiple[l]
	 		t[#t+1]=loop
      	end
   end
   return multiple
end
