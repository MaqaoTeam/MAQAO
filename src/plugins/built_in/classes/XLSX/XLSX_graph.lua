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
XLSX_graph = {}

-- Create the class meta
XLSX_graphMeta         = {}
XLSX_graphMeta._NAME   = "XLSX_graph"
XLSX_graphMeta.__index = XLSX_graph




---
--
--
--
function XLSX_graph:_save (rootpath, idx, sheet)
   if self[XLSX.consts.graph.type] == XLSX.consts.graph_types.line then
      XLSX:_create_line_chart (rootpath, idx, sheet, self)
   elseif self[XLSX.consts.graph.type] == XLSX.consts.graph_types.bar then
      XLSX:_create_bar_chart (rootpath, idx, sheet, self)
   elseif self[XLSX.consts.graph.type] == XLSX.consts.graph_types.bar_and_lines then
      XLSX:_create_bar_and_line_chart (rootpath, idx, sheet, self)
   elseif self[XLSX.consts.graph.type] == XLSX.consts.graph_types.pie then
      XLSX:_create_pie_chart (rootpath, idx, sheet, self)
   end
end





--- Create a new graph
-- @param table
-- @param graph_type if set, a value from XLSX.consts.graph_types describing
-- the graph type. Default value is XLSX.consts.graph_types.line
-- @return 
function XLSX_graph:new (table, graph_type)
   if table == nil then
      return nil
   end
   if  type (text) == "table"
   and table[XLSX.consts.meta.name] ~= XLSX.consts.meta.table_type then
      return nil
   end
   local g = {}
   setmetatable (g, XLSX_graphMeta)

   g[XLSX.consts.meta.name]        = XLSX.consts.meta.graph_type
   g[XLSX.consts.graph.table]      = table
   g[XLSX.consts.graph.x_top]      = 0
   g[XLSX.consts.graph.y_top]      = 0
   g[XLSX.consts.graph.size_h_c]   = 20
   g[XLSX.consts.graph.size_w_c]   = 8
   g[XLSX.consts.graph.secondary]  = {}
   g[XLSX.consts.graph.lines_ser]  = {}
   g[XLSX.consts.graph.primary_name]   = ""
   g[XLSX.consts.graph.secondary_name] = ""
   g[XLSX.consts.graph.grouped]    = false
   g[XLSX.consts.graph.custom_colors]  = {}
   
   if  graph_type ~= nil
   and XLSX.consts.graph_types[graph_type] ~= nil then
      g[XLSX.consts.graph.type] = graph_type
   else
      g[XLSX.consts.graph.type] = XLSX.consts.graph_types.line
   end
   
   return g
end





--- Set the graph height (default value is 20)
-- @param nb_col The new graph height (in number of column)
function XLSX_graph:set_height (nb_col)
   if self == nil then
      return
   end
   if nb_col < 1 then
      return
   end
   
   self[XLSX.consts.graph.size_h_c] = nb_col
end





--- Set the graph width (default value is 8)
-- @param nb_col The new graph width (in number of column)
function XLSX_graph:set_width (nb_col)
   if self == nil then
      return
   end
   if nb_col < 1 then
      return
   end
   
   self[XLSX.consts.graph.size_w_c] = nb_col
end





--- Set the minimal value of the y axis (default is handled by the reading sowftware)
-- @param value a number used as minimal value for the y axis or nil if the default behavior should be used
function XLSX_graph:set_y_axis_min (value)
   if self == nil then
      return
   end
   
   if type (value) == "number"
   or vaue == nil then
      self[XLSX.consts.graph.y_axis_min] = value
   end
end




--- Set a custom color for a specific chart series
-- @param id Series identifier (number, starting at 1)
-- @param color custom color to use instead of the default color
function XLSX_graph:set_color (id, color)
   if self == nil then
      return
   end
   if type (id) ~= "number"
   or id < 1 then
      return
   end
   if  type (color) ~= "string"
   and color ~= nil then
      return
   end 

   -- If needed, filter the string to remove the two first characters
   local _color = color
   if string.len(_color) == 8 then
      _color = string.gsub (_color, "^[0fF][0fF]", "")
   end

   self[XLSX.consts.graph.custom_colors][id] = _color
end



-- ----------------------------------------------------- --
--    A set of functions used for bar_and_line charts    --
-- ----------------------------------------------------- --
function XLSX_graph:set_primary_scale (id)
   if self == nil then
      return
   end
   -- Check if the entry id is in the secondary table and remove it
   self[XLSX.consts.graph.secondary][id] = nil
end





function XLSX_graph:set_secondary_scale (id)
   if self == nil then
      return
   end
   -- Add entry id in the secondary table
   self[XLSX.consts.graph.secondary][id] = true
end





function XLSX_graph:set_line_serie (id)
   if self == nil then
      return
   end
   -- Add entry id in the lines_ser table
   self[XLSX.consts.graph.lines_ser][id] = true
end





function XLSX_graph:set_primary_axis_name (name)
   if self == nil then
      return
   end
   if name == nil
   or type (name) ~= "string" then
      self[XLSX.consts.graph.primary_name] = ""
   end
   self[XLSX.consts.graph.primary_name] = name
end





function XLSX_graph:set_secondary_axis_name (name)
   if self == nil then
      return
   end
   if name == nil
   or type (name) ~= "string" then
      self[XLSX.consts.graph.secondary_name] = ""
   end
   self[XLSX.consts.graph.secondary_name] = name
end





function XLSX_graph:set_bar_stacked (bool)
   if self == nil then
      return
   end
   if name == nil
   or type (name) ~= "boolean" then
      self[XLSX.consts.graph.grouped] = false
   end
   self[XLSX.consts.graph.grouped] = bool
end