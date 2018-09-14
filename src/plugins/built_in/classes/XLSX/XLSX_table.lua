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
XLSX_table = {}

-- Create the class meta
XLSX_tableMeta         = {}
XLSX_tableMeta._NAME   = "XLSX_table"
XLSX_tableMeta.__index = XLSX_table



--- Create a new XLSX_table structure
-- @param name Name of table
-- @param data A 2D array where:
--             - The first line (index 1) contains column names
--             - The first column contains line names
--             An exemple:
--             Series | exp1 | exp2 
--             -------+------+------
--             serie1 | v11  | v12
--             serie2 | v21  | v22
--             serie3 | v31  | v32
-- @param orientation Table orientation in the file (element from
--        XLSX.consts.table.orientations). Default value is
--        XLSX.consts.table.orientations.columns and displays
--        the table as shown on data variable structure.
--        XLSX.consts.table.orientations.lines displays the 
--        table with the following structure (for same data 
--        variable structure)
--        Series | serie1 | serie2 | serie3
--        -------+--------+--------+--------
--         exp1  |  v11   |  v21   |  v31
--         exp2  |  v12   |  v22   |  v32
-- @return an initialized XLSX_table or nil
function XLSX_table:new (name, data, orientation)
   local t = {}
	setmetatable (t, XLSX_tableMeta)

   t[XLSX.consts.meta.name]        = XLSX.consts.meta.table_type
   t[XLSX.consts.table.y_head_loc] = 0
   if type (name) == "string" then
      t[XLSX.consts.table.name]       = name
   elseif type (name) == number then 
      t[XLSX.consts.table.name]       = name..""
   else
      t[XLSX.consts.table.name]       = ""
   end
   if orientation == XLSX.consts.table.orientations.columns
   or orientation == XLSX.consts.table.orientations.lines  then
      t[XLSX.consts.table.orientation] = orientation
   elseif orientation == nil then
      t[XLSX.consts.table.orientation] = XLSX.consts.table.orientations.columns
   else
      Message:error ("Unvalid value for table orientation: "..orientation)
      return nil
   end   
   
   if t[XLSX.consts.table.orientation] == XLSX.consts.table.orientations.columns then
      t[XLSX.consts.table.header]     = {}
      for j, val in ipairs (data[1]) do
         t[XLSX.consts.table.header][j] = val
      end
   
      t[XLSX.consts.table.data]       = {}
      for i = 2, table.getn (data) do
         t[XLSX.consts.table.data][i - 1] = {}
         for j = 1, table.getn (data[i]) do
            t[XLSX.consts.table.data][i - 1][j] = data[i][j]
         end
      end
   elseif t[XLSX.consts.table.orientation] == XLSX.consts.table.orientations.lines then
      t[XLSX.consts.table.header]     = {}
      for i, val in ipairs (data) do
         t[XLSX.consts.table.header][i] = data[i][1]
      end
      
      t[XLSX.consts.table.data]       = {}            
      for j = 2, table.getn (data[1]) do
         t[XLSX.consts.table.data][j - 1] = {}
         for i = 1, table.getn (data) do
            t[XLSX.consts.table.data][j - 1][i] = data[i][j]
         end
      end
      
   end
   
   -- Table name style
   t[XLSX.consts.table.name_style] = {}
   t[XLSX.consts.table.name_style][XLSX.consts.text.bold]       = "1"
   t[XLSX.consts.table.name_style][XLSX.consts.text.underline]  = "none"
   t[XLSX.consts.table.name_style][XLSX.consts.text.color]      = XLSX.consts.colors.white
   t[XLSX.consts.table.name_style][XLSX.consts.text.size]       = 12
   t[XLSX.consts.table.name_style][XLSX.consts.text.background] = "00112132"
   t[XLSX.consts.table.name_style][XLSX.consts.text.italic]     = "0"
   t[XLSX.consts.table.name_style][XLSX.consts.text.strike]     = "0"
   t[XLSX.consts.table.name_style][XLSX.consts.text.font]       = "Arial"
   t[XLSX.consts.table.name_style][XLSX.consts.text.alignment]  = XLSX.consts.align.default

   -- Table header style
   t[XLSX.consts.table.head_style] = {}
   t[XLSX.consts.table.head_style][XLSX.consts.text.bold]       = "1"
   t[XLSX.consts.table.head_style][XLSX.consts.text.underline]  = "none"
   t[XLSX.consts.table.head_style][XLSX.consts.text.color]      = XLSX.consts.colors.white
   t[XLSX.consts.table.head_style][XLSX.consts.text.size]       = 12
   t[XLSX.consts.table.head_style][XLSX.consts.text.background] = "00364560"
   t[XLSX.consts.table.head_style][XLSX.consts.text.italic]     = "0"
   t[XLSX.consts.table.head_style][XLSX.consts.text.strike]     = "0"
   t[XLSX.consts.table.head_style][XLSX.consts.text.font]       = "Arial"
   t[XLSX.consts.table.head_style][XLSX.consts.text.alignment]  = XLSX.consts.align.center
   
   -- Table data style
   t[XLSX.consts.table.data_style] = {}
   t[XLSX.consts.table.data_style][XLSX.consts.text.bold]       = "0"
   t[XLSX.consts.table.data_style][XLSX.consts.text.underline]  = "none"
   t[XLSX.consts.table.data_style][XLSX.consts.text.color]      = XLSX.consts.colors.black
   t[XLSX.consts.table.data_style][XLSX.consts.text.size]       = 12
   t[XLSX.consts.table.data_style][XLSX.consts.text.background] = ""  
   t[XLSX.consts.table.data_style][XLSX.consts.text.italic]     = "0"
   t[XLSX.consts.table.data_style][XLSX.consts.text.strike]     = "0"
   t[XLSX.consts.table.data_style][XLSX.consts.text.font]       = "Arial"
   t[XLSX.consts.table.data_style][XLSX.consts.text.alignment]  = XLSX.consts.align.center

   return t
end


---
--
--
function XLSX_table:duplicate (src_tab)
   local src_tab = nil

   return dst_tab
end
