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

--- Module cqa.csv
-- Defines functions used to generate the cqa report as a CSV file
module ("cqa.api.csv", package.seeall)

require "cqa.consts";
require "cqa.api.metrics";


-- TODO: try to reuse this
local function get_CSV_scheme (cqa_context)
   local CSV_scheme = {}
   local common_metrics = cqa.api.metrics.common_metrics
   local path_metrics = cqa.api.metrics.path_metrics

   -- for each requested metric (in order)
   for _,metric_key in ipairs (cqa_context.requested_metrics) do
      local metric
      if (common_metrics [metric_key] ~= nil) then
         metric = common_metrics [metric_key]
      elseif (path_metrics [metric_key] ~= nil) then
         metric = path_metrics [metric_key]
      end

      if (string.find (metric_key, "^%[.+%]") ~= nil) then
         metric_key = string.match (metric_key, "^%[.*%] (.*)")
      end

      local metric_args
      if (type (metric.args) == "function") then
         metric_args = metric.args (cqa_context)
      elseif (type (metric.args) == "table") then
         metric_args = metric.args
      end

      if (metric_args ~= nil and #metric_args == 2) then
         for _,arg1 in ipairs (metric_args[1]) do
            for _,arg2 in ipairs (metric_args[2]) do
               table.insert (CSV_scheme, {
                                header = string.format (metric.CSV_header, arg1, arg2),
                                cr_key = { metric_key, arg1, arg2 } })
            end
         end

      elseif (metric_args ~= nil and #metric_args == 1) then
         for _,arg1 in ipairs (metric_args[1]) do
            table.insert (CSV_scheme, {
                             header = string.format (metric.CSV_header, arg1),
                             cr_key = { metric_key, arg1 } })
         end

      else
         table.insert (CSV_scheme, {
                          header = metric.CSV_header,
                          cr_key = { metric_key } })
      end
   end

   return CSV_scheme
end


--- Inits the CSV file
-- Creates a CSV file and print as first line column headers
-- The name is generated from function_name
-- @param function_name Name of the function
-- @param cqa_context cqa_context table used for generating further CQA results and that must contain at least the arch field
-- @return descriptor on the file
function init_csv (csv_name, cqa_context)
   -- open file
   local csv_file = io.open (csv_name, "w");

   -- test if opened
   if (csv_file == nil) then
      Message:display (cqa.consts.Errors["CANNOT_WRITE_CSV"],
                       lfs.currentdir(),        -- current directory
                       package.config:sub(1,1), -- system directory separator
                       csv_name);
   end

   -- Get micro-architecture constants if not already done
   if (cqa_context.uarch_consts == nil) then
      local arch = cqa_context.arch
      cqa_context.uarch_consts = cqa.api[arch].uarch.get_uarch_constants (cqa_context);
   end
   local CSV_scheme = get_CSV_scheme (cqa_context)

   local t = {}
   for k,v in ipairs (CSV_scheme) do t[k] = v.header end
   csv_file:write (table.concat (t, ";")..";\n") -- TODO: remove trailing ; (legacy)

   return csv_file;
end

local function get_val (cr_key, crc, crp)
   local val = crc [cr_key[1]]
   if (val == nil and crp ~= nil) then val = crp [cr_key[1]] end

   if (#cr_key == 2) then
      if (type(val) ~= "table") then return "NA" end
      val = val [cr_key[2]]

   elseif (#cr_key == 3) then
      if (type(val) ~= "table") then return "NA" end
      val = val [cr_key[2]]

      if (type(val) ~= "table") then return "NA" end
      val = val [cr_key[3]]
   end

   if (val == nil) then return "NA" end

   return tostring (val)
end

-- Pushes to the output CSV file static analyis results for a loop
-- @param cqa_results Table containing results from static analysis
-- @param csv_file Descriptor of the CSV output file
function push_to_csv (cqa_results, csv_file)
   if (csv_file == nil) then return end

   local CSV_scheme = get_CSV_scheme (cqa_results.common.context)
  
   if (not cqa_results.common ["can be analyzed"]) then
      local t = {}
      for k,v in ipairs (CSV_scheme) do
         t[k] = get_val (v.cr_key, cqa_results.common, nil)
      end

      csv_file:write (table.concat (t, ";")..";\n"); -- TODO: remove trailing ; (legacy)
      return
   end

   for _,cqa_results_path in ipairs (cqa_results.paths) do
      local t = {}
      for k,v in ipairs (CSV_scheme) do
         t[k] = get_val (v.cr_key, cqa_results.common, cqa_results_path)
      end

      csv_file:write (table.concat (t, ";")..";\n"); -- TODO: remove trailing ; (legacy)
   end
end

--- Dumps CSV table as a lua table, to get a flattened view of metrics/values
-- Returns a table
-- @param cqa_results cqa:get_cqa_results return value
-- @param cqa_context cqa_context table passed to cqa:get_cqa_results
-- @return table { <path 1>, <path 2>... }
-- with <path i> = { [<metric 1 CSV header>] = <metric 1 value>, etc for other metrics }
function dump_csv (cqa_results, cqa_context)
   local dump = {};
   local CSV_scheme = get_CSV_scheme (cqa_context)

   if (not cqa_results.common ["can be analyzed"]) then
      t = {}
      for _,v in ipairs (CSV_scheme) do
         t [v.header] = get_val (v.cr_key, cqa_results.common, nil)
      end
      dump [1] = t

      return dump
   end

   for path_ID,cqa_results_path in ipairs (cqa_results.paths) do
      t = {}
      for _,v in ipairs (CSV_scheme) do
         t [v.header] = get_val (v.cr_key, cqa_results.common, cqa_results_path)
      end
      dump [path_ID] = t
   end

   return dump
end
