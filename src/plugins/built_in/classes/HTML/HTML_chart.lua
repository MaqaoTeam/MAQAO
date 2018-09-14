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
HTML_chart = {}

-- Create the class meta
HTML_chartMeta         = {}
HTML_chartMeta._NAME   = "HTML_chart"
HTML_chartMeta.__index = HTML_chart
HTML_chartMeta.__id    = 0



--- Create a new HTML_chart
-- @param header A table describing options needed to display the chart with the following format:
--               options = {
--                  series = {"serie 1 name"     , "serie 2 name"    , ...},   -- Each entry is a serie's name
--                  type   = {"bar"|"line"|"pie" , "bar"|"line"|"pie", ...},   -- Each entry indicates if the serie must be displayed
--                                                                             -- as a bar or a line
--                  yaxis  = {    1 | 2          ,     1 | 2         , ...},   -- Each entry indicates if the serie must used the primary
--                                                                             -- Y-axis scale or the secondary Y-axis scale
--                  y_min  = <number>,                                         -- Minimal value for primary Y-axis
--                  y2_min = <number>,                                         -- Minimal value for secondary Y-axis
--                  is_point_labels   = <boolean>,                             -- true is all points labels must be displayed, else false
--                  x_name = <string>,                                         -- Name of the X-axis, displayed if filled
--                  y_name = <string>,                                         -- Name of the primary Y-axis, displayed if filled
--                  y2_name = <string>,                                        -- Name of the secondary Y-axis, displayed if filled
--                  zoom = <boolean>,                                          -- If true, create a chart the can zoomable. It can be used
--                                                                             -- only with a single bar serie.
--                  legend = "n", "s", "e", "w", "none"                        -- Location of the legend
--               }
-- @param data A table describing all data. It is an iterative table where each entry
--             describes a serie. Each serie is a table where each entry is 
--             {key = <string>, value = <number>}
-- @example To create a pie chart:
--          hchart = HTML:create_html_chart ({
--            series   = {"Characterization"},
--            type     = {"pie"},
--            yaxis  = {1},
--          },
--          {{
--             {key = "Application", value = 95},
--             {key = "MPI", value = 0},
--             {key = "OpenMP", value = 0},
--             {key = "Math", value = 0},
--             {key = "System", value = 5},
--             {key = "Pthread", value = 0},
--             {key = "IO", value = 0},
--             {key = "String manipulation", value = 0},
--             {key = "Memory operations", value = 0},
--             {key = "Others", value = 0},
--          }})
-- @example To create a chart mixing lines and bars on different axis:
--          hchart = HTML:create_html_chart ({
--             series   = {"Nb loop", "Coverage", "Cumul Coverage"},
--             type     = {"bar", "bar", "line"},
--             yaxis  = {1, 2, 2},
--          },
--          {{
--             {key = "8+", value = 1},
--             {key = "8-4", value = 0},
--             {key = "4-2", value = 2},
--             {key = "2-1", value = 4},
--             {key = "1-0.5", value = 9},
--             {key = "> 0.5", value = 4},
--          },
--          {
--             {key = "8+", value = 36.4},
--             {key = "8-4", value = 0},
--             {key = "4-2", value = 6.6},
--             {key = "2-1", value = 5},
--             {key = "1-0.5", value = 4},
--             {key = "> 0.5", value = 0.3},
--          },
--          {
--             {key = "8+", value = 36.4},
--             {key = "8-4", value = 36.4},
--             {key = "4-2", value = 36.4 + 6.6},
--             {key = "2-1", value = 36.4 + 6.6 + 5},
--             {key = "1-0.5", value = 36.4 + 6.6 + 5 + 4},
--             {key = "> 0.5", value = 36.4 + 6.6 + 5 + 4 + 0.3},
--          },
--          })
function HTML:create_html_chart (header, data)
   local hchart = {}
   setmetatable (hchart, HTML_chartMeta)
   hchart._id = HTML_chartMeta.__id
   HTML_chartMeta.__id = HTML_chartMeta.__id + 1
   
   hchart.header        = header
   hchart.data          = data
   hchart.loc_actions   = {}
   hchart.col_actions   = {}
   hchart.row_actions   = {}
   hchart.reg_actions   = nil
   hchart.is_action     = false
   
   return hchart
end



function HTML_chart:add_action (html_action, serie, elem)
   if type (html_action) ~= "table" then
      return false
   end

   -- Add an action to a table ------------------------------------------------
   -- Regular action
   if elem == nil and serie == nil then
      self.reg_action = html_action
      self.is_action = true

   -- located action
   elseif type (elem) == "number"
   and elem > 0 
   and type (serie) == "number"
   and line > 0 then
      if self.loc_actions[serie] == nil then
         self.loc_actions[serie] = {}
      end
      self.loc_actions[serie][elem] = html_action
      self.is_action = true

   -- row action
   elseif (type (elem) ~= "number"
   or elem <= 0)
   and type (serie) == "number"
   and serie > 0 then
      self.row_actions[serie] = html_action
      self.is_action = true

   -- column action
   elseif type (elem) == "number"
   and elem > 0 
   and (type (serie) ~= "number"
   or serie <= 0) then
      self.col_actions[elem] = html_action
      self.is_action = true
      
   else
      return false
   end

   return true
end