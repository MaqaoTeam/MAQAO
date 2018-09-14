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

-- Create the class
XLSX_sheet = {}

-- Create the class meta
XLSX_sheetMeta         = {}
XLSX_sheetMeta._NAME   = "XLSX_sheet"
XLSX_sheetMeta.__index = XLSX_sheet





--- Change the sheet name
-- @param name the new sheet name
function XLSX_sheet:change_name (name)
   self[XLSX.consts.sheet.name] = name
end





--- Add a XLSX_text in the sheet
-- @param text A text to add in the sheet. Either a string or a XLSX_text
--             If <text> is a string, a XLSX_text is created, added and returned
--             Else if <text> is a XLSX_text, it is added and returned
--             Else, nil is returned
-- @param loc_x Location of the cell where to write the text
--             If <loc_x> is a number, it is the column identifier
--             Else if it is a string ([A-Z]+[0-9]+), is it converted as coordinates
--             Else, location is ignored
-- @param loc_y Location of the cell where to write the text
--             If <loc_y> is a number, it is the line identifier
--             Else, location is ignored
-- @note Some examples of callings:
--    add_text ("A text"): Add "A text" on the next cell of the line
--    add_text ("A text", 1, 2): Add "A text" on the cell of the first column (1), second line (2)
--    add_text ("A text", "B5"): Add "A text" on the cell of the second (B) column, fifth line (5)
--    add_text ("A text", "coin"): Add "A text" on the next cell of the line
--    add_text ("A text", 5): Add "A text" on the next cell of the line
-- @return an initialized XLSX_text or nil
function XLSX_sheet:add_text (text, loc_x, loc_y)
   local str = nil
   local text_toadd = nil
   local x = XLSX.consts.sheet.position_x
   local y = XLSX.consts.sheet.position_y

   -- Create or get an XLSX_text object corresponding to text
   if type (text) == "string"
   or type (text) == "number" then
      text_toadd = XLSX_text:new (text)
      str = text
   elseif type (text) == "table" 
   and    text[XLSX.consts.meta.name] == XLSX.consts.meta.text_type then
      text_toadd = text
      str = text[XLSX.consts.text.value]
   else
      return nil
   end
      
   -- -------------------------------------------------------------------------
   -- Handle location if needed
   -- Case (int)loc_x, (int)loc_y 
   if    (type (loc_x) == "number"
      and type (loc_y) == "number"
      and loc_x > 0
      and loc_y > 0)
   or 
      -- Case (string)loc_x
         (type (loc_x) == "string"
      and string.match (loc_x, "^[A-Z]+[0-9]+$")
   ) then
      -- if we are in the case (string)loc_x, convert the string into 
      -- numeric coordinates
      if type (loc_x) == "string"
      and string.match (loc_x, "^[A-Z]+[0-9]+$") then
         loc_x, loc_y = XLSX:get_coordinates_from_text (loc_x)
      end
      
      -- Initialize empty lines if needed
      for j = 1, loc_y do
         if self[XLSX.consts.sheet.cells][j] == nil then
            self[XLSX.consts.sheet.cells][j] = {}
         end
      end
      
      -- Initialize empty cells on the line if needed
      if loc_x > 0 then
         for i = 1, loc_x do
            if self[XLSX.consts.sheet.cells][loc_y][i] == nil then
               self[XLSX.consts.sheet.cells][loc_y][i] = XLSX_text:new ("")
            end
         end
      end
      
      -- Set the XLSX_text in the cell
      self[XLSX.consts.sheet.cells][loc_y][loc_x] = text_toadd

      -- If needed, set new cursor location 
      if loc_y >= self[y] then
         self[x] = loc_x + 1
         self[y] = loc_y
      elseif loc_x >= self[x] then
         self[x] = loc_x + 1
      end
      
   -- -------------------------------------------------------------------------
   -- else simply add the XLSX_text      
   else
      if self[XLSX.consts.sheet.cells][self[y]] == nil then
         self[XLSX.consts.sheet.cells][self[y]] = {}
      end
      
      self[XLSX.consts.sheet.cells][self[y]][self[x]] = text_toadd
      self[x] = self[x] + 1
   end

   -- Update the maximal column number
   if self[x] > self[XLSX.consts.sheet.column_max] then
      self[XLSX.consts.sheet.column_max] = self[x]
   end
   
   -- Update the maximal line number
   if self[y] > self[XLSX.consts.sheet.line_max] then
      self[XLSX.consts.sheet.line_max] = self[y]
   end  
   
   -- Update strings table
   if type (str) == "string" then
      if self[XLSX.consts.sheet.ustrings][str] == nil then
         self[XLSX.consts.sheet.nb_ustrings] = self[XLSX.consts.sheet.nb_ustrings] + 1
         self[XLSX.consts.sheet.ustrings][str] = self[XLSX.consts.sheet.nb_ustrings]
         self[XLSX.consts.sheet.ostrings][self[XLSX.consts.sheet.nb_ustrings]] = str
      end 
      self[XLSX.consts.sheet.nb_strings] = self[XLSX.consts.sheet.nb_strings] + 1
   end 

   return text_toadd
end





--- Add empty cells in the sheet
-- @param size The number of empty cells
-- @param loc_x Location of the cell where to write the text
--             If <loc_x> is a number, it is the column identifier
--             Else if it is a string ([A-Z]+[0-9]+), is it converted as coordinates
--             Else, location is ignored
-- @param loc_y Location of the cell where to write the text
--             If <loc_y> is a number, it is the line identifier
--             Else, location is ignored
-- @return the number of added cells or -1
function XLSX_sheet:add_empty_cells (size, loc_x, loc_y)
   if type (size) ~= "number" then
      return -1
   end
  
   local x = XLSX.consts.sheet.position_x
   local y = XLSX.consts.sheet.position_y

   -- Handle location if needed
   -- Case (int)loc_x, (int)loc_y 
   if    (type (loc_x) == "number"
      and type (loc_y) == "number"
      and loc_x > 0
      and loc_y > 0)
   or 
      -- Case (string)loc_x
         (type (loc_x) == "string"
      and string.match (loc_x, "^[A-Z]+[0-9]+$")
   ) then
      -- if we are in the case (string)loc_x, convert the string into 
      -- numeric coordinates
      if type (loc_x) == "string"
      and string.match (loc_x, "^[A-Z]+[0-9]+$") then
         loc_x, loc_y = XLSX:get_coordinates_from_text (loc_x)
      end
      
   else
      loc_x = self[x]
      loc_y = self[y]
   end
   
   if self[XLSX.consts.sheet.cells][self[y]] == nil then
      self[XLSX.consts.sheet.cells][self[y]] = {}
   end
   
   for i = 1, size do 
      self:add_text ("", loc_x, loc_y)
      loc_x = loc_x + 1
   end
   return size
end





--- Add a merged cell based on nb cells
-- It can be initialized with a string or an XLSX_text using text
-- @param nb The number of cells to merge in order to create the cell
-- @param text A text to add in the cell. Either a string or a XLSX_text
--             If <text> is a string, a XLSX_text is created, added and returned
--             Else if <text> is a XLSX_text, it is added and returned
-- @param loc_x Location of the cell where to write the text
--             If <loc_x> is a number, it is the column identifier
--             Else if it is a string ([A-Z]+[0-9]+), is it converted as coordinates
--             Else, location is ignored
-- @param loc_y Location of the cell where to write the text
--             If <loc_y> is a number, it is the line identifier
--             Else, location is ignored
-- @return The created cell
function XLSX_sheet:merge_cells (nb, text, loc_x, loc_y)
   -- Check parameters
   if type (nb) ~= "number" 
   or nb < 1 then
      return nil
   end
   
   if text ~= nil then
      if  type (text) ~= "string"
      and type (text) ~= "table" then
         return nil
      end
      
      if  type (text) == "table"
      and text[XLSX.consts.meta.name] ~= XLSX.consts.meta.text_type then
         return nil
      end
   end

   -- If nb == 1, just create an return a single cell
   if  nb == 1 
   and text == nil then
      return self:add_text ("", loc_x, loc_y)
   elseif  nb == 1 then
      return self:add_text (text, loc_x, loc_y)
   end
   
   -- Create a first cell using text
   local start_cell = ""
   local stop_cell  = ""

   -- Handle location if needed
   -- Case (int)loc_x, (int)loc_y 
   if    (type (loc_x) == "number"
      and type (loc_y) == "number"
      and loc_x > 0
      and loc_y > 0)
   or 
      -- Case (string)loc_x
         (type (loc_x) == "string"
      and string.match (loc_x, "^[A-Z]+[0-9]+$")
   ) then
      -- if we are in the case (string)loc_x, convert the string into 
      -- numeric coordinates
      if type (loc_x) == "string"
      and string.match (loc_x, "^[A-Z]+[0-9]+$") then
         loc_x, loc_y = XLSX:get_coordinates_from_text (loc_x)
      end
      
      -- At this point, loc_x and loc_y contains both a integer greater than 0
      start_cell = XLSX:get_col_from_int (loc_x)..loc_y
      stop_cell  = XLSX:get_col_from_int (loc_x + nb - 1)..loc_y      
   else
      start_cell = XLSX:get_col_from_int (self[XLSX.consts.sheet.position_x])..self[XLSX.consts.sheet.position_y]
      stop_cell  = XLSX:get_col_from_int (self[XLSX.consts.sheet.position_x] + nb - 1)..self[XLSX.consts.sheet.position_y]

      loc_x = self[XLSX.consts.sheet.position_x]
      loc_y = self[XLSX.consts.sheet.position_y]
   end
   local returned_cell =  self:add_text (text, loc_x, loc_y)
   
   -- Then create nb - 1 empty cells
   self:add_empty_cells (nb - 1, loc_x + 1, loc_y)
   
   -- Update the sheet
   table.insert (self[XLSX.consts.sheet.mergedCells], start_cell..":"..stop_cell)
   
   return returned_cell
end





--- Add a hyperlink in the sheet to another sheet
-- @param text A text to add in the cell. Either a string or a XLSX_text
--             If <text> is a string, a XLSX_text is created, added and returned
--             Else if <text> is a XLSX_text, it is added and returned
-- @param dst An existing XLSX_sheet used as target or a string representing a target file
--             If it is a string, it must have the following format: <file>#<sheet>
--             where <file> is the file name and <sheet> is a sheet name in the file
--             If <file> or <sheet> are wrong, the link will be created but will be useless
-- @param loc_x Location of the cell where to write the text
--             If <loc_x> is a number, it is the column identifier
--             Else if it is a string ([A-Z]+[0-9]+), is it converted as coordinates
--             Else, location is ignored
-- @param loc_y Location of the cell where to write the text
--             If <loc_y> is a number, it is the line identifier
--             Else, location is ignored
-- @return The created cell
function XLSX_sheet:add_hyperlink (text, dst, dst_loc, loc_x, loc_y)
   if  type (dst) ~= "string"
   and type (dst) ~= "table" 
   or text == nil
   or text == "" then
      return nil
   elseif type (dst) == "table"
   and    dst[XLSX.consts.meta.name] ~= XLSX.consts.meta.sheet_type then
      return nil
   end
   
   if  type (dst) == "string" 
   and not string.match (dst, ".+#.+") then
      return nil
   end

   local x = XLSX.consts.sheet.position_x
   local y = XLSX.consts.sheet.position_y

   -- Handle location if needed
   -- Case (int)loc_x, (int)loc_y 
   if    (type (loc_x) == "number"
      and type (loc_y) == "number"
      and loc_x > 0
      and loc_y > 0)
   or 
      -- Case (string)loc_x
         (type (loc_x) == "string"
      and string.match (loc_x, "^[A-Z]+[0-9]+$")
   ) then
      -- if we are in the case (string)loc_x, convert the string into 
      -- numeric coordinates
      if type (loc_x) == "string"
      and string.match (loc_x, "^[A-Z]+[0-9]+$") then
         loc_x, loc_y = XLSX:get_coordinates_from_text (loc_x)
      end
      
   else
      loc_x = self[x]
      loc_y = self[y]
   end

   local cell = self:add_text (text, loc_x, loc_y)
   if cell == nil then
      return nil
   end
   
   local link = {}
   link[XLSX.consts.hyperlink.ref]      = XLSX:get_col_from_int (loc_x)..loc_y
   link[XLSX.consts.hyperlink.display]  = cell[XLSX.consts.text.value]
   link[XLSX.consts.hyperlink.dst_cell] = "A1"

   if  type (dst_loc) == "string"
   and string.match (dst_loc, "^[A-Z]+%d+$") ~= nil then
      link[XLSX.consts.hyperlink.dst_cell] = dst_loc
   end
   
   if  type (dst) == "table" then
      link[XLSX.consts.hyperlink.dst]      = dst
   elseif  type (dst) == "string" then
      link[XLSX.consts.hyperlink.r_file]   = string.gsub(dst, "#.+", "")
      link[XLSX.consts.hyperlink.r_sheet]  = string.gsub(dst, ".+#", "")
      self[XLSX.consts.sheet.is_remote_hyperlinks] = true
   end
   table.insert (self[XLSX.consts.sheet.hyperlinks], link)
   
   return cell
end





--- Go the a new line
function XLSX_sheet:new_line ()
   -- If the current line is empty, add a empty cell
   if self[XLSX.consts.sheet.position_x] == 1 then
      self:add_empty_cells (1)
   end

   self[XLSX.consts.sheet.position_y] = self[XLSX.consts.sheet.position_y] + 1
   self[XLSX.consts.sheet.position_x] = 1
   
   if self[XLSX.consts.sheet.position_y] > self[XLSX.consts.sheet.line_max] then
      self[XLSX.consts.sheet.line_max] = self[XLSX.consts.sheet.position_y]
   end
end





---
-- @param loc_x Location of the cell where to write the text
--             If <loc_x> is a number, it is the column identifier
--             Else if it is a string ([A-Z]+[0-9]+), is it converted as coordinates
--             Else, location is wrong
-- @param loc_y Location of the cell where to write the text
--             If <loc_y> is a number, it is the line identifier
--             Else, location is wrong
-- @return The cell at given location or nil if the location is wrong
function XLSX_sheet:get_cell (loc_x, loc_y)
   -- Case (int)loc_x, (int)loc_y 
   if    (type (loc_x) == "number"
      and type (loc_y) == "number"
      and loc_x > 0
      and loc_y > 0)
   or 
      -- Case (string)loc_x
         (type (loc_x) == "string"
      and string.match (loc_x, "^[A-Z]+[0-9]+$")
   ) then
      -- if we are in the case (string)loc_x, convert the string into 
      -- numeric coordinates
      if type (loc_x) == "string"
      and string.match (loc_x, "^[A-Z]+[0-9]+$") then
         loc_x, loc_y = XLSX:get_coordinates_from_text (loc_x)
      end
      
      if self[XLSX.consts.sheet.cells][loc_y][loc_x] == nil then 
         return self:add_text ("", loc_x, loc_y)
      else
         return self[XLSX.consts.sheet.cells][loc_y][loc_x]
      end
      
   -- invalid location
   else
      return nil
   end
end





--- Get current location
-- @return current x location (number), current y location (number)
function XLSX_sheet:get_location ()
   return self[XLSX.consts.sheet.position_x], self[XLSX.consts.sheet.position_y]
end





--- Add a XLSX_table in the sheet
-- @param table A table to add in the sheet
-- @param loc_x Location of the cell where to write the text
--             If <loc_x> is a number, it is the column identifier
--             Else if it is a string ([A-Z]+[0-9]+), is it converted as coordinates
--             Else, location is ignored
-- @param loc_y Location of the cell where to write the text
--             If <loc_y> is a number, it is the line identifier
--             Else, location is ignored
function XLSX_sheet:add_table (table, loc_x, loc_y)
   if type (table) == "table" 
   and    table[XLSX.consts.meta.name] ~= XLSX.consts.meta.table_type then
      -- error
      return
   end
   if type (table) ~= "table"  then
      -- error
      return
   end
   local text = nil
   
   
   -- Handle location if needed
   -- Case (int)loc_x, (int)loc_y 
   if    (type (loc_x) == "number"
      and type (loc_y) == "number"
      and loc_x > 0
      and loc_y > 0)
   or 
      -- Case (string)loc_x
         (type (loc_x) == "string"
      and string.match (loc_x, "^[A-Z]+[0-9]+$")
   ) then
      -- if we are in the case (string)loc_x, convert the string into 
      -- numeric coordinates
      if type (loc_x) == "string"
      and string.match (loc_x, "^[A-Z]+[0-9]+$") then
         loc_x, loc_y = XLSX:get_coordinates_from_text (loc_x)
      else
         -- nothing to do, we want loc_x and loc_y as positive integers
      end
   else
      loc_x = self[XLSX.consts.sheet.position_x]
      loc_y = self[XLSX.consts.sheet.position_y]
   end
   

   -- -------------------------------------------------------------------------      
   -- Add the table name
   text = XLSX_text:new (table[XLSX.consts.table.name])
   
   -- Colors
   text:set_background (table[XLSX.consts.table.name_style][XLSX.consts.text.background])
   text:set_color (table[XLSX.consts.table.name_style][XLSX.consts.text.color])
   
   -- Font style
   if table[XLSX.consts.table.name_style][XLSX.consts.text.bold] == "1" then
      text:set_bold (true)
   end
   if table[XLSX.consts.table.name_style][XLSX.consts.text.italic] == "1" then
      text:set_italic (true)
   end
   if table[XLSX.consts.table.name_style][XLSX.consts.text.strike] == "1" then
      text:set_strike (true)
   end
   if  table[XLSX.consts.table.name_style][XLSX.consts.text.underline] ~= nil 
   and table[XLSX.consts.table.name_style][XLSX.consts.text.underline] ~= "none" then
      text:set_underline (true)
   end
   text:set_size (table[XLSX.consts.table.name_style][XLSX.consts.text.size])
   text:set_font (table[XLSX.consts.table.name_style][XLSX.consts.text.font])
   text:set_alignment (table[XLSX.consts.table.name_style][XLSX.consts.text.alignment])
   self:merge_cells (3, text, loc_x, loc_y)
   
   -- Add the table header
   table[XLSX.consts.table.y_head_loc] = loc_y + 2
   table[XLSX.consts.table.x_head_loc] = loc_x
   for i, val in ipairs (table[XLSX.consts.table.header]) do
      text = XLSX_text:new (val)
      
      -- Borders: Top, bottom and right
      text:set_border (XLSX.consts.borders.all, XLSX.consts.borders.thin)
      
      -- Colors
      text:set_background (table[XLSX.consts.table.head_style][XLSX.consts.text.background])
      text:set_color (table[XLSX.consts.table.head_style][XLSX.consts.text.color])
      
      -- Font style
      if table[XLSX.consts.table.head_style][XLSX.consts.text.bold] == "1" then
         text:set_bold (true)
      end
      if table[XLSX.consts.table.head_style][XLSX.consts.text.italic] == "1" then
         text:set_italic (true)
      end
      if table[XLSX.consts.table.head_style][XLSX.consts.text.strike] == "1" then
         text:set_strike (true)
      end
      if  table[XLSX.consts.table.head_style][XLSX.consts.text.underline] ~= nil 
      and table[XLSX.consts.table.head_style][XLSX.consts.text.underline] ~= "none" then
         text:set_underline (true)
      end
      text:set_size (table[XLSX.consts.table.head_style][XLSX.consts.text.size])
      text:set_font (table[XLSX.consts.table.head_style][XLSX.consts.text.font])
      text:set_alignment (table[XLSX.consts.table.head_style][XLSX.consts.text.alignment])

      self:add_text (text, loc_x + i - 1, loc_y + 2)
   end
   
   -- Add the table data
   for i, line in ipairs (table[XLSX.consts.table.data]) do
      for j, val in ipairs (table[XLSX.consts.table.data][i]) do
         text = XLSX_text:new (val)
         
         -- Borders: Bottom and right
         text:set_border (XLSX.consts.borders.all, XLSX.consts.borders.thin)
      
         -- Colors
         text:set_background (table[XLSX.consts.table.data_style][XLSX.consts.text.background])
         text:set_color (table[XLSX.consts.table.data_style][XLSX.consts.text.color])
         
         -- Font style
         if table[XLSX.consts.table.data_style][XLSX.consts.text.bold] == "1" then
            text:set_bold (true)
         end
         if table[XLSX.consts.table.data_style][XLSX.consts.text.italic] == "1" then
            text:set_italic (true)
         end
         if table[XLSX.consts.table.data_style][XLSX.consts.text.strike] == "1" then
            text:set_strike (true)
         end
         if  table[XLSX.consts.table.data_style][XLSX.consts.text.underline] ~= nil 
         and table[XLSX.consts.table.data_style][XLSX.consts.text.underline] ~= "none" then
            text:set_underline (true)
         end
         text:set_size (table[XLSX.consts.table.data_style][XLSX.consts.text.size])
         text:set_font (table[XLSX.consts.table.data_style][XLSX.consts.text.font])
         text:set_alignment (table[XLSX.consts.table.data_style][XLSX.consts.text.alignment])
         
         self:add_text (text, loc_x + j - 1, loc_y + 2 + i)
      end
   end
end





--- Add a XLSX_graph in the sheet
-- @param graph A graph to add in the sheet
-- @param loc_x Location of the cell where to write the text
--             If <loc_x> is a number, it is the column identifier
--             Else if it is a string ([A-Z]+[0-9]+), is it converted as coordinates
--             Else, location is ignored
-- @param loc_y Location of the cell where to write the text
--             If <loc_y> is a number, it is the line identifier
--             Else, location is ignored
function XLSX_sheet:add_graph (graph, loc_x, loc_y)
   if type (graph) ~= "table" 
   or graph[XLSX.consts.meta.name] ~= XLSX.consts.meta.graph_type then
      return
   end
   table.insert (self[XLSX.consts.sheet.graphs], graph)
   
   -- Handle location if needed
   -- Case (int)loc_x, (int)loc_y 
   if    (type (loc_x) == "number"
      and type (loc_y) == "number"
      and loc_x > 0
      and loc_y > 0)
   or 
      -- Case (string)loc_x
         (type (loc_x) == "string"
      and string.match (loc_x, "^[A-Z]+[0-9]+$")
   ) then
      -- if we are in the case (string)loc_x, convert the string into 
      -- numeric coordinates
      if type (loc_x) == "string"
      and string.match (loc_x, "^[A-Z]+[0-9]+$") then
         loc_x, loc_y = XLSX:get_coordinates_from_text (loc_x)
      else
         -- nothing to do, we want loc_x and loc_y as positive integers
      end
   else
      self:new_line ()
      self:new_line ()

      loc_x = self[XLSX.consts.sheet.position_x]
      loc_y = self[XLSX.consts.sheet.position_y]      
   end
   
   -- Update graph location in the sheet
   graph[XLSX.consts.graph.y_top] = loc_y
   graph[XLSX.consts.graph.x_top] = loc_x
   
   -- Update sheet's cells to keep empty space
   if loc_y + graph[XLSX.consts.graph.size_h_c] + 1 > self[XLSX.consts.sheet.line_max] then
      for i = 1, (loc_y + graph[XLSX.consts.graph.size_h_c] + 1) - self[XLSX.consts.sheet.line_max] do
         self:new_line ()
      end   
   end
end





--- Change columns size
-- @param start First column index to change
-- @param stop Last column index to change
-- @param size The new column size (in cm) given in a string
function XLSX_sheet:set_columns_size (start, stop, size)
   if type (start) ~= "number"
   or type (stop) ~= "number" then
      return
   end
   if stop < start then
      return
   end

   for i = 1, start do
      if self[XLSX.consts.sheet.colum_size][i] == nil then
         self[XLSX.consts.sheet.colum_size][i] = XLSX.consts.size.col_width
      end
   end
   for i = start, stop do
      self[XLSX.consts.sheet.colum_size][i] = size
   end
end





--- Change rows size
-- @param start First row index to change
-- @param stop Last row index to change
-- @param size The new row size (in cm) given in a string
function XLSX_sheet:set_rows_size (start, stop, size)
   if type (start) ~= "number"
   or type (stop) ~= "number" then
      return
   end
   if stop < start then
      return
   end

   for i = 1, start do
      if self[XLSX.consts.sheet.row_size][i] == nil then
         self[XLSX.consts.sheet.row_size][i] = XLSX.consts.size.row_height
      end
   end
   for i = start, stop do
      self[XLSX.consts.sheet.row_size][i] = size
   end
end





--- Insert on comment on a given cell
-- @param text Text to set in the comment. Use \n as line delimiter
-- @param loc_x Location of the cell where to write the text
--             If <loc_x> is a number, it is the column identifier
--             Else if it is a string ([A-Z]+[0-9]+), is it converted as coordinates
--             Else, location is ignored
-- @param loc_y Location of the cell where to write the text
--             If <loc_y> is a number, it is the line identifier
--             Else, location is ignored
-- @return the created comment or nil
function XLSX_sheet:add_comment (text, loc_x, loc_y)
   if type (text) ~= "string" then
      return nil
   end

   -- Handle location if needed
   -- Case (int)loc_x, (int)loc_y 
   if    (type (loc_x) == "number"
      and type (loc_y) == "number"
      and loc_x > 0
      and loc_y > 0)
   or 
      -- Case (string)loc_x
         (type (loc_x) == "string"
      and string.match (loc_x, "^[A-Z]+[0-9]+$")
   ) then
      -- if we are in the case (string)loc_x, convert the string into 
      -- numeric coordinates
      if type (loc_x) == "string"
      and string.match (loc_x, "^[A-Z]+[0-9]+$") then
         loc_x, loc_y = XLSX:get_coordinates_from_text (loc_x)
      else
         -- nothing to do, we want loc_x and loc_y as positive integers
      end
   else
      loc_x = self[XLSX.consts.sheet.position_x]
      loc_y = self[XLSX.consts.sheet.position_y]

      -- If location is not given, it is assumed the comment must be linked to
      -- the previous added cell so x should be decreased by one.
      if loc_x > 1 then
         loc_x = loc_x - 1
      end
   end
   -- At this point, loc_x and loc_y contains the comment location
      
   local comment = XLSX_comment:new (text, loc_x, loc_y, table.getn (self[XLSX.consts.sheet.comments]) + 1)   
   table.insert (self[XLSX.consts.sheet.comments], comment)
   
   return comment
end




--- Fix the first sheet raw
-- @param is_fixed a boolean used to set if the first raw must be fixed (true) or not (false)
-- @param nb if defined, specified how many raws should be fixed (default is 1)
function XLSX_sheet:fix_first_raw (is_fixed, nb)
   if self == nil then
      return 
   end
   
   if is_fixed == true then
      if  type (nb) == "number"
      and nb > 0 then
         self[XLSX.consts.sheet.is_raw_fixed] = nb
      elseif nb == 0 then
         self[XLSX.consts.sheet.is_raw_fixed] = nil
      else
         self[XLSX.consts.sheet.is_raw_fixed] = 1
      end
   elseif is_fixed == false
   or     is_fixed == nil then
      self[XLSX.consts.sheet.is_raw_fixed] = nil
   end
end
