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
HTML_table = {}

-- Create the class meta
HTML_tableMeta         = {}
HTML_tableMeta._NAME   = "HTML_table"
HTML_tableMeta.__index = HTML_table
HTML_tableMeta.__id    = 0



local function _rename_id_rec (row)
   row["__id"] = row["id"]
   row["id"]   = nil
   
   if type (row["__children"]) == "table" then
      for _, s_row in ipairs (row["__children"]) do
         _rename_id_rec (s_row)
      end
   end
end



--- Create a HTML_table object
-- @param header Header of the table to display. It is an iterative table with the following format:
--               header[i] = {                        -- Give data about the i-th column
--                             name = <string>,       -- Name in the data table of the i-th column
--                             col_name = <string>,   -- Displayed name of the i-th column
--                             desc = <string>,       -- Description of the column
--                             width = <int>,         -- Column size (percentage of the width)
--                             is_centered = <boolean>,  -- True if the content must be centered, else false
--                             is_optionnal = <boolean>, -- False (default) is the column is always displayed, true
--                                                          if it can be hidden
--                             is_displayed = <boolean>, -- If the column is optionnal, specify if the column is displayed
--                                                       -- at the page loading (true) or not (false, default)
--                             word_break = <boolean>,-- If true, force string to be split if they are too large for the column.
--                                                    -- false is default choice
--               }
-- @param data Data to displayed. It is an iterative table with the following format:
--               data[i] = {
--                  key1 = value1,                 -- key1 is header[1].name
--                  key2 = value2,                 -- key2 is header[2].name
--                  __children = [data],           -- [data] is a table using the same structure than
--                                                 -- the parameter data. It describes childrens of the
--                                                 -- current 
--                  ...
--               }
-- @param _type A string defininf the type of the graph : "fixed", "tree", "paged"
-- @param custom_style An optionnal function called when each cell is generated. The function is called
--                     with parameters i (row id), j (column id), v (value of the cell)
-- @return An initialized HTML_table object or nil
function HTML:create_html_table (header, data, _type, custom_style)
   if type (header) ~= "table" then
      -- TODO error invalid parameter
      return nil
   end
   if type (data) ~= "table" then
      -- TODO error invalid parameter
      return nil
   end

   local htable = {}
   setmetatable (htable, HTML_tableMeta)
  
   htable._id = HTML_tableMeta.__id
   HTML_tableMeta.__id = HTML_tableMeta.__id + 1
  
   htable.data          = data
   htable.header        = header
   htable.type          = _type
   htable.loc_actions   = {}
   htable.col_actions   = {}
   htable.row_actions   = {}
   htable.reg_actions   = nil
   htable.actions       = {}
   htable.action_menus  = {}
   htable.is_action     = false
   htable.is_action_menu= false
   htable.custom_style  = custom_style



   -- Rename 'id' key
   -- -------------------------------------------------------------------------
   -- 'id' is a hidden column used by scripts. It should not been used by the user
   -- In order to prevent issues due to this, header and data are scanned to check
   -- if 'id' is used as key and replaced by '__id'. 'id' will still be displayed
   -- in the html page unless a different col_name value has been defined fot the 
   -- 'id' column
--[[
   local id_index = false
   for i, head in ipairs (htable.header) do
      if head.name == "id" then
         head.name = "__id"
         if head.col_name == nil then
            head.col_name = "id"
         end
         id_index = true
         break;
      end
   end
   if id_index == true then
      for i, row in ipairs (htable.data) do
         _rename_id_rec (row)
      end
   end
--]]
   return htable
end




--- Link an action to the table
-- The part of the action which trigger the action is determnied by parameters line and column.
-- If both are specified, the action is triggered when there is a click on the corresponding cell
-- If only the line is specified, the action is triggered when there is a click on the corresponding row
-- If only the column is specified, the action is triggered when there is a click on the corresponding column
-- If none of them is specified, the action is triggered when there is a click anywher on the table
-- @param html_action An action or a menu to link
-- @param line Index of the row the action is linked to
-- @param column Index of the column the action is linked to
-- @param param An optionnal string which will be used as parameter when the action is call
--              The value is available through <p> in the action url
-- @return true if the action has been linked, else false
function HTML_table:add_action (html_action, line, column, param)
   if type (html_action) ~= "table" then
      return false
   end
   local atype = getmetatable (html_action)._NAME

   -- Add an action to a table ------------------------------------------------
   -- Regular action
   if column == nil and line == nil then

      if self.reg_action == nil then
         self.reg_action = {}
         self.reg_action_p = {}
      end

      if atype == "HTML_action_menu" then
         table.insert (self.reg_action, {action = html_action, param = param, type="m"})
      else
         table.insert (self.reg_action, {action = html_action, param = param, type="a"})
      end

   -- located action
   elseif type (column) == "number"
   and column > 0 
   and type (line) == "number"
   and line > 0 then
      if self.loc_actions[line] == nil then
         self.loc_actions[line] = {}
      end
      if self.loc_actions[line][column] == nil then
         self.loc_actions[line][column] = {}
      end

      if atype == "HTML_action_menu" then
         table.insert (self.loc_actions[line][column], {action = html_action, param = param, type="m"})
      else
         table.insert (self.loc_actions[line][column], {action = html_action, param = param, type="a"})
      end

   -- row action
   elseif (type (column) ~= "number"
   or column <= 0)
   and type (line) == "number"
   and line > 0 then
      if self.row_actions[line] == nil then
         self.row_actions[line] = {}
      end

      if atype == "HTML_action_menu" then
         table.insert (self.row_actions[line], {action = html_action, param = param, type="m"})
      else
         table.insert (self.row_actions[line], {action = html_action, param = param, type="a"})
      end

   -- column action
   elseif type (column) == "number"
   and column > 0 
   and (type (line) ~= "number"
   or line <= 0) then
      if self.col_actions[column] == nil then
         self.col_actions[column] = {}
      end

      if atype == "HTML_action_menu" then
         table.insert (self.col_actions[column], {action = html_action, param = param, type="m"})
      else
         table.insert (self.col_actions[column], {action = html_action, param = param, type="a"})
      end
   else
      return false
   end

   if atype == "HTML_action_menu" then
      self.is_action_menu = true
      self.action_menus[html_action.id] = html_action
   else
      self.is_action = true
      self.actions[html_action.id]      = html_action
   end

   return true
end
