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

module("loop.registers",package.seeall)

function loop:get_cumulative_itc()
   local p=self:get_profile()
   local ch={}
   if (p~=nil) then
      local itc_tot
      local h
      itc_tot=p.settings.itc_count
      local c=0
      local i=1
      for _,h in pairs(p.histogram) do
	 c=c+h.itc*h.weight
	 if (h.lc>0 or i==1) then 
	    ch[h.lc+1]=c/itc_tot
	    --print(h.lc.." "..ch[h.lc+1])
	 end
	 i=i+1
      end
      return ch
   end
end
