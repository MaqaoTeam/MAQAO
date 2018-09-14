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

--- Convert an integer into the corresponding column id
-- @param int An integer to convert
-- @return A string corresponding to int column
function XLSX:get_col_from_int (int)
   local col = ""
   local n = math.floor (int)
   
   if n <= 0 then
      return nil
   end
   
   repeat
      c = (n - 1) % 26
      col = string.char (c + string.byte ("A"))..col
      n = math.floor((n - c) / 26)
   until (n <= 0)

   return col
end




function XLSX:get_int_from_col (col)
   if type (col) ~= "string" then
      return -1
   end
   local res = 0
   local l = nil
   for i = 1, col:len () do
      l = col:sub (i, i)
      res = (res * 26) + (string.byte (l) - string.byte ("A") + 1) 
   end
   return res
end




function XLSX:get_coordinates_from_text (text)
   local x = tonumber (XLSX:get_int_from_col (text:match ("^[A-Z]+")))
   local y = tonumber (text:match ("[0-9]+$"))
   return x, y
end




function XLSX:get_text_from_coordinates (x, y)
   return XLSX:get_col_from_int (x)..y
end
