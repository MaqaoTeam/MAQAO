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



-- ///////////////////////////////////////////////////////////////////////// --
--                               PRIVATE FUNCTIONS                           --
-- ///////////////////////////////////////////////////////////////////////// --

local function __handle_level (_obj, lvl)
   if _obj.numbered ~= true then
      return ""
   end

   if lvl == 1 then
      return _obj.header_1.."  -  "
   elseif lvl == 2 then
      return _obj.header_1..".".._obj.header_2.."  -  "
   elseif lvl == 3 then
      return _obj.header_1..".".._obj.header_2..".".._obj.header_3.."  -  "
   elseif lvl == 4 then
      return _obj.header_1..".".._obj.header_2..".".._obj.header_3..".".._obj.header_4.."  -  "
   elseif lvl == 5 then
      return _obj.header_1..".".._obj.header_2..".".._obj.header_3..".".._obj.header_4..".".._obj.header_5.."  -  "
   else
      return ""
   end

   return ""
end



local function __display_header_box (_obj, text, c, lvl)
   local x = maqao_get_term_size()
   local str_line = "+"..string.rep (c, x - 2).."+"
   local str_text = ""

   if text == nil then
      text = __handle_level (_obj, lvl)
   else
      text = __handle_level (_obj, lvl)..text   
   end

   if text == nil then
      str_text = "+"..string.rep (" ", x - 2).."+"
   else
      local nb_space_l = math.floor (((x - 2) - string.len (text)) / 2)
      local nb_space_r = nb_space_l
      local _text = text

      if nb_space_r + nb_space_l + string.len (text) + 2 < x then
         nb_space_r = nb_space_r + 1
      end

      if maqao_isatty (io.stdout) == 1 then
         _text = "\27[1m".._text.."\27[0m"
      end

      str_text = "+"..string.rep (" ", nb_space_l).._text..string.rep (" ", nb_space_r).."+"
   end

   print ("")
   print (str_line)
   print (str_text)
   print (str_line)
   print ("")
end


local function __display_header_line (_obj, text, c, bold, lvl)
   local x = maqao_get_term_size()
   local str_line = "  "..string.rep (c, x - 12)
   local str_text = ""
   local _bold = bold

   if _bold == nil then
      _bold = true
   end

   if text == nil then
      str_text = ""
   else
      local _text = text
      if  _bold == true
      and maqao_isatty (io.stdout) == 1 then
         _text = "\27[1m"..text.."\27[0m"
      end

      str_text = "      "..__handle_level (_obj, lvl).._text
   end

   print ("")
   print (str_text)
   print (str_line)
   print ("")

end





-- ///////////////////////////////////////////////////////////////////////// --
--                                PUBLIC FUNCTIONS                           --
-- ///////////////////////////////////////////////////////////////////////// --

--- Enable / disable the numbering of headers
-- @param bool A boolean telling if the numbering must be enabled (true) or disabled (false)
function TEXT:set_numbered_headers (bool)
   if type (bool) == "boolean" then
       self.numbered = bool
   elseif bool == nil then
      self.numbered = true
   else
      Message:error ("Function TEXT:set_numbered_headers waits for a boolean parameter")
   end
end


--- Display first level header on the standard output
-- @param text Name of the header
function TEXT:display_header_1 (text)
   self.header_1 = self.header_1 + 1
   self.header_2 = 0
   self.header_3 = 0
   self.header_4 = 0
   self.header_5 = 0

   print ("")
   __display_header_box (self, text, "=", 1)
end


--- Display second level header on the standard output
-- @param text Name of the header
function TEXT:display_header_2 (text)
   self.header_2 = self.header_2 + 1
   self.header_3 = 0
   self.header_4 = 0
   self.header_5 = 0

   __display_header_box (self, text, "-", 2)
end


--- Display third level header on the standard output
-- @param text Name of the header
function TEXT:display_header_3 (text)
   self.header_3 = self.header_3 + 1
   self.header_4 = 0
   self.header_5 = 0

   __display_header_line (self, text, "=", true, 3)
end


--- Display fourth level header on the standard output
-- @param text Name of the header
function TEXT:display_header_4 (text)
   self.header_4 = self.header_4 + 1
   self.header_5 = 0

   __display_header_line (self, text, "-", true, 4)
end


--- Display fifth level header on the standard output
-- @param text Name of the header
function TEXT:display_header_5 (text)
   self.header_5 = self.header_5 + 1
   
   __display_header_line (self, text, "-", false, 5)
end


--- Display a table on the standard output
--@param header A table detailling the table header. Each entry is the name of a column
--@param data Table data. A 2D table where data[i][j] is the element at the row i, column j
--@param sizes An optionnal table describing proportions of columns. If not specified, all columns 
--             have the same size. If specified, each entry must be a number representing a
--             percentage of the table width the column must have
function TEXT:display_table (header, data, sizes)
   local str = ""
   local col_sizes = {}
   local max_x = maqao_get_term_size()
   local sum_x_header = 2
   local sum_x_sizes  = 0

   max_x = max_x - 4      -- -4 for spaces on left and right

   -- Compute columns sizes
   for _, head in pairs (header) do
      sum_x_header = sum_x_header + string.len (head) + 3
   end

   if type (sizes) == "table" then
      for _, s in pairs (sizes) do
         sum_x_sizes = sum_x_sizes + s
      end

      if table.getn (sizes) ~= table.getn (header) then
         Message:error ("In table displaying, sum of all given sizes is greater than 100")
         sum_x_sizes = 101
      end
   end

   -- Headers larger than the terminal => do not add empty space
   if sum_x_header >= max_x
   or sum_x_sizes > 100 then
      for _, head in pairs (header) do
         table.insert (col_sizes, string.len (head))
      end

   else
      -- Proportions given by the user
      if type (sizes) == "table" then
         local res = max_x
         for _, s in pairs (sizes) do
            res = res - math.floor (((max_x * s) / 100))
         end

         for i, head in pairs (header) do
            if i < res then
               table.insert (col_sizes, math.floor (((max_x * sizes[i]) / 100))+ 1)
            else
               table.insert (col_sizes, math.floor (((max_x * sizes[i]) / 100)))
            end
         end

      -- Nothing given by the user, split the table in equal parts
      else
         local def = math.floor (max_x / table.getn (header))
         local res = max_x - (def * table.getn (header))

         for i, head in pairs (header) do
            if i < res then
               table.insert (col_sizes, def + 1)
            else
               table.insert (col_sizes, def)
            end
         end
      end

      -- Equilibrate columns size according to header sizes (the column should be as large as the header)
      local oversize = 0
      for i, head in pairs (header) do
         if string.len (head) + 3 > col_sizes [i] then
            col_sizes [i] = string.len (head) + 3
            oversize = oversize + (col_sizes [i] - string.len (head))
         end
      end

      local change = true
      while oversize > 0  and change == true do
         change = false
         for i, head in pairs (header) do
            if oversize > 0 then
               if col_sizes [i] > string.len (head) + 4 then
                  col_sizes [i] = col_sizes [i] - 1
                  oversize = oversize - 1
                  change = true
               end
            end
         end
      end
   end

   -- Display the table
   str = "  "
   for i, head in pairs (header) do
      local _head = head
      if string.len (head) < (col_sizes [i] - 3) then
         _head = _head..string.rep (" ", col_sizes [i] - 3 - string.len (head))
      end

      if maqao_isatty (io.stdout) == 1 then
         str = str.." \27[1m".._head.."\27[0m |"
      else
         str = str.." ".._head.." |"
      end
   end
   print (str)

   str = "  "
   for _, s in pairs (col_sizes) do
      str = str..string.rep ("-", s - 1).."+"
   end
   print (str)

   for _, row in pairs (data) do
      str = "  "
      for i, val in pairs (row) do
         local _val = nil
         if type (val) == "string" then
            if string.len (val) > col_sizes [i] - 3 then
               _val = string.sub (val, 1, col_sizes [i] - 6).."..."
            else
               _val = string.sub (val, 1, col_sizes [i] - 3)
            end
         elseif type (val) == "number" then
            _val = string.sub (tostring (val), 1, col_sizes [i] - 3)
         elseif val == nil then
            _val = ""
         else

         end

         if _val ~= nil and string.len (_val) < (col_sizes [i] - 3) then
            _val = _val..string.rep (" ", col_sizes [i] - 3 - string.len (_val))
         end
         if _val ~= nil then
            str = str.." ".._val.." |"
         end
      end
      print (str)
   end
end



--- Display some text on the standard output, adapting lines breaks to the terminal size
-- @param text Some text to display
function TEXT:display_text (text)
   local max_x = maqao_get_term_size()

   local text2 = adapt_text_length (text, max_x)
   print (text2)
   text2 = nil
end


