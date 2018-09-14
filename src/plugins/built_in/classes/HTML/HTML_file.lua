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
HTML_file = {}

-- Create the class meta
HTML_fileMeta         = {}
HTML_fileMeta._NAME   = "HTML_file"
HTML_fileMeta.__index = HTML_file
HTML_fileMeta.__id   = 0


-- ------------------------------------------------------------------------- --
--                              PRIVATE FUNCTIONS                            --
-- ------------------------------------------------------------------------- --

--- [PRIVATE] Create a new HTML_file object
-- @param name A string representing the file path to open.
-- @return An HTML_file object or nil if there is a problem
function HTML_file:new (name)
   local file = io.open (name, "w")
   if file == nil then
      return nil
   end
   
   local hfile = {}
   setmetatable (hfile, HTML_fileMeta)
   hfile._id = HTML_fileMeta.__id
   HTML_fileMeta.__id = HTML_fileMeta.__id + 1
   
   hfile.file               = file
   hfile.name               = name
   hfile.content_width      = 950   -- TODO use this constant to generate CSS
   hfile.nb_accordions      = 1
   hfile.accordion_display  = {}
   hfile.accordion_ids      = {}
   hfile.is_acc_active      = false
   hfile.is_acc_not_active  = false

   return hfile
end



--- [PRIVATE] Close an HTML_file
-- @return Values returned by io.close
function HTML_file:close ()
   return self.file:close ()
end





-- ------------------------------------------------------------------------- --
--                               PUBLIC FUNCTIONS                            --
-- ------------------------------------------------------------------------- --

--- Write some content in an HTML_file object
-- @param txt A string to write
-- @return Values returned by io.write
function HTML_file:write (txt)
   return self.file:write (txt)
end


--- Insert an HTML_table object in an HTML_file object
-- @param htable An HTML_table object to insert
-- @param opt_param An optionnal parameter whose valeu depends on the table type
--         if tree table: A boolean indicating if the table must be fully expendend (true) or not (false, default)
-- @return true if the table is inserted, else false
function HTML_file:insert_html_table (htable, opt_param)
   if type (htable) ~= "table" then
      return false
   end

   if htable.type == "tree" then
      return HTML:_html_display_tree_table (self, htable, opt_param)

   elseif htable.type == "fixed" then
      return HTML:_html_display_fixed_table (self, htable)

   elseif htable.type == "paged" then
      return HTML:_html_display_paged_table (self, htable)
   end
end



--- Insert an HTML_chart object in an HTML_file object
-- @param hchart An HTML_chart object to insert
-- @return true if the chart is inserted, else false
function HTML_file:insert_chart (hchart)
   if type (hchart) ~= "table" then
      return false
   end
   
   -- Scan the header to get the chart type (pie / other)
   -- -------------------------------------------------------------------------
   -- A pie chart must have only one serie whose type is "pie".
   -- If a chart has several 'pie' series or use 'pie' mixed with 
   -- either 'bar' or 'line', there is an error
   -- Other cases are considered as a mix of 'bar' and 'lines'
   local _type = "other"
   for i, head in ipairs (hchart.header.type) do
      if head == "pie" then
         if i > 1 then
            -- Error: Several series in a pie chart, not supported
            return false 
         end
         _type = "pie"
      end
   end

   if _type == "pie" then
      HTML:_html_display_pie_chart (self, hchart)
   elseif _type == "other" then
      if  type (hchart.header) == "table" 
      and hchart.header.zoom == true then
         HTML:_html_display_zoomable_bar_chart (self, hchart)
      else
         HTML:_html_display_bar_and_line_chart (self, hchart)
      end
   end

   return true
end
