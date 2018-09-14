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

--- Module cqa.api
-- Defines API functions, visible from other modules via the "cqa:" prefix/namespace
module ("cqa.api", package.seeall)

require "cqa.api.metrics"
require "cqa.api.cqa_results"
require "cqa.api.csv"
require "cqa.api.src_loop_grouping"
require "cqa.api.reports"

--- Adds external/user metrics to CQA
-- @note built-in metrics can be listed via maqao cqa -lm (writing a metrics.json file)
-- @param user_metrics (following specifications described in api/metrics.lua)
function cqa:add_user_metrics (user_metrics)
   cqa.api.metrics.add_user_metrics (user_metrics)
end

--- Returns CQA results for a function, a loop or a path
-- cqa_context should be reused (to benefit from cached data)
-- for all objects in the same binary file but not accross different files
-- @param blocks a function, a loop or a path (list of blocks)
-- @param cqa_context cqa_context table (see data_struct.lua)
-- @return cqa_results table (see consts.lua)
function cqa:get_cqa_results (blocks, cqa_context)
   return cqa.api.cqa_results.get_cqa_results (blocks, cqa_context)
end

--- Clears cache located in the cqa_context table
-- This cache is managed (created/filled/used) by the CQA metrics engine
-- to speedup source-level metrics (like unroll factor) computation
-- It contains source-loop level data of functions containing analyzed loops
-- This function is useful when many functions are touched (in same bin. file)
-- and you can group loops by function
-- @param cqa_context cqa_context table (see data_struct.lua)
-- @param fct function for which cached data must be cleared
function cqa:clear_context_cache (cqa_context, fct)
   cqa.api.metrics.clear_context_cache (cqa_context, fct)
end

--- Inits the CSV file
-- Creates a CSV file and print as first line column headers
-- The name is generated from function_name
-- @param function_name Name of the function
-- @param cqa_context cqa_context table used for generating further CQA results and that must contain at least the arch field
-- @return descriptor on the file
function cqa:init_csv (csv_name, cqa_context)
   return cqa.api.csv.init_csv (csv_name, cqa_context)
end

--- Pushes to a CSV file data from a cqa_results table
-- @param cqa_results cqa:get_cqa_results return value
-- @param csv_file Descriptor of the CSV output file
function cqa:push_to_csv (cqa_results, csv_file)
   cqa.api.csv.push_to_csv (cqa_results, csv_file)
end

--- Dumps CSV table as a lua table, to get a flattened view of metrics/values
-- Returns a table
-- @param cqa_results cqa:get_cqa_results return value
-- @param cqa_context cqa_context table passed to cqa:get_cqa_results
-- @return table { <path 1>, <path 2>... }
-- with <path i> = { [<metric 1 CSV header>] = <metric 1 value>, etc for other metrics }
function cqa:dump_csv (cqa_results, cqa_context)
   return cqa.api.csv.dump_csv (cqa_results, cqa_context)
end

--- Returns source loops for a given function
-- @note use only if you don't need other metrics (otherwise use cqa:get_cqa_results)
-- @param fct a function
-- @return source and binary loops, see group_loops_by_src_lines
function cqa:get_fct_src_loops (fct)
   return cqa.api.src_loop_grouping.get_fct_src_loops (fct)
end

--- Returns list of metric names needed to get reports in given categories
-- @param report_names list of categories: "header", "gain", "potential",
-- "hint", "expert", "brief"
-- @return list of requested metric names, e.g {"cycles decoding", "nb instructions"...}
function cqa:get_requested_metrics_for_reports (report_names)
   return cqa.api.reports.get_requested_metrics (report_names)
end

--- Returns a table containing some CQA reports for a binary loop, function or path
-- Only requested confidence levels (defined in the context common to all paths) will be considered
-- @param cqa_results table returned by cqa:get_cqa_results()
-- @param tags table defining text or html tags to format lists, tables etc.
function cqa:get_reports (cqa_results, tags)
   cqa_results.common.context.tags = tags;
   return cqa.api.reports.get_reports (cqa_results)
end
