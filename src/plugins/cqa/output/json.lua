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

-- This file defines the get_json_metrics_list function writing to a JSON file a human readable listing of available metrics
module ("cqa.output.json", package.seeall)

require "cqa.api.metrics"

function get_json_metrics_list ()

   local function get_CSV_scheme (metric_key, metric)
      local CSV_scheme = {}

      if (#metric.args == 2) then
         for _,arg1 in ipairs (metric.args[1]) do
            for _,arg2 in ipairs (metric.args[2]) do
               table.insert (CSV_scheme, {
                                header = string.format (metric.CSV_header, arg1, arg2),
                                desc   = string.format (metric.desc, arg1, arg2) })
            end
         end

      elseif (metric.args ~= nil and #metric.args == 1) then
         for _,arg1 in ipairs (metric.args[1]) do
            table.insert (CSV_scheme, {
                             header = string.format (metric.CSV_header, arg1),
                             desc   = string.format (metric.desc, arg1) })
         end
      end

      return CSV_scheme
   end

   local function get_metric_string (metric_key, metric)
      local concat = table.concat
      local format = string.format

      local metric_args = "NA"
      if (type (metric.args) == "table") then
         local metric_args_tab = {}
         for i,v in ipairs (metric.args) do
            metric_args_tab [i] = format ("{ %s }", concat (v, ", "))
         end
         metric_args = concat (metric_args_tab, ",\n")
      elseif (type (metric.args) == "function") then
         metric_args = "<function>"
      end

      local metric_deps = "NA"
      if (metric.deps ~= nil) then metric_deps = concat (metric.deps, ", ") end

      local metric_arch = "NA"
      if (metric.arch ~= nil) then metric_arch = concat (metric.arch, ", ") end

      -- TODO: improve this
      local metric_exp = "NA"
      if (type (metric.args) == "table" and metric.CSV_header ~= nil) then
         local CSV_scheme = get_CSV_scheme (metric_key, metric)
         local metric_args_tab = {}
         for i,v in ipairs (CSV_scheme) do
            metric_args_tab [i] = format ("(%s, %s)", v.header, v.desc)
         end
         metric_exp = string.format ("{ %s }", concat (metric_args_tab, ", "))
      end

      local t = {
         format ("\t\t\"%s\": {"                 , metric_key),
         format ("\t\t\t\"args\": \"%s\","       , metric_args),
         format ("\t\t\t\"CSV_header\": \"%s\"," , metric.CSV_header or "NA"),
         format ("\t\t\t\"lua_type\": \"%s\","   , metric.lua_type),
         format ("\t\t\t\"description\": \"%s\"" , metric.desc),
         format ("\t\t\t\"exp. metrics\": \"%s\"", metric_exp),
         format ("\t\t\t\"dependencies\": \"%s\"", metric_deps),
         format ("\t\t\t\"arch\":  \"%s\""       , metric_arch),
         format ("\t\t}")
      }
      return table.concat (t, "\n")
   end

   -- open file
   local json_file = io.open ("metrics.json", "w")
   json_file:write ("{\n")

   -- Common to all execution paths
   json_file:write ("\t\"common\": {\n")
   local buf_tab = {}
   for metric_key,metric in pairs (cqa.api.metrics.common_metrics) do
      table.insert (buf_tab, get_metric_string (metric_key, metric))
   end
   json_file:write (table.concat (buf_tab, ",\n").."\n")
   json_file:write ("\t},\n")

   -- Specific to execution paths
   json_file:write ("\t\"path\": {\n")
   buf_tab = {}
   for metric_key,metric in pairs (cqa.api.metrics.path_metrics) do
      table.insert (buf_tab, get_metric_string (metric_key, metric))
   end
   json_file:write (table.concat (buf_tab, ",\n").."\n")
   json_file:write ("\t}\n")

   -- close file
   json_file:write ("}\n")
   json_file:close()
end
