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

----------------------------------------
-- COUNTER
----------------------------------------

-- Increment a counter (in a global variable)
function insert_counter_inc(patch, addr, pos, counter)
   local code = asm_code(asm_safe({}, {"INC %VAR%"}))

   local gvars = Table:new()
   gvars:insert(counter)
   return insert_asm(patch, addr, pos, gvars, code)
end

-- Decrement a counter (in a global variable)
function insert_counter_dec(patch, addr, pos, counter)
   local code = asm_code(asm_safe({}, {"DEC %VAR%"}))

   local gvars = Table:new()
   gvars:insert(counter)
   return insert_asm(patch, addr, pos, gvars, code)
end

function insert_counter_inc_between(patch,source,target,counter)
   return patcher.insert_between(patch,source,target,
      function (addr,pos)
         return patcher.insert_counter_inc(patch,addr,pos,counter)
      end
   )
end

function insert_counter_dec_between(patch,source,target,counter)
   return patcher.insert_between(patch,source,target,
      function (addr,pos)
         return patcher.insert_counter_dec(patch,addr,pos,counter)
      end
   )
end
