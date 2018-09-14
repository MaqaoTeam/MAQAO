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

--- Module cqa.output.txt
-- Defines the print_reports function used to print on stdout CQA reports
module ("cqa.output.txt", package.seeall)

require "cqa.output.txt_html_shared"
require "cqa.api.reports_shared";
local is_intel = cqa.api.reports_shared.is_intel;
local is_gnu   = cqa.api.reports_shared.is_gnu;

require "cqa.api.reports";

-- Prints the 'str' string and underlines it using the 'chr' character
local function print_header (str, chr)
   print (str);
   print (string.rep (chr, #str));
end

-- Variables controling the print_section function
local level_idx;
local old_level;

-- Prints a section header with the corresponding indentation level
-- Numerotation is automatically managed.
local function print_section (title, level)
   -- Computes the section context from the previous one
   if (old_level == nil) then
      level = 1;
      level_idx = {1};
   elseif (old_level >= level) then
      level_idx [level] = level_idx [level] + 1;
   else
      for l = old_level+1, level do
         level_idx [l] = 1;
      end
   end

   -- Prints the section title
   local section = tostring (level_idx [1]);
   for l = 2, level do
      section = section.."."..level_idx [l];
   end
   print_header ("Section "..section..": "..title, "=");
   print ("");

   -- Save the indentation level
   old_level = level;
end

local function print_path (reports, confidence)
   for _,confidence_level in ipairs (confidence) do
      for _,report in ipairs (reports [confidence_level]) do
         print_header (report.title, "-");
         print (report.txt);
         if (report.details   ) then print (report.details   ) end
         if (report.workaround) then print ("Workaround(s):\n"..report.workaround) end
         print ("");
      end
   end
end

-- Prints the part of the static analysis report which is common to source and binary loops
local function print_cmn (cqa_results)
   local cqa_context = cqa_results.common.context;

   if (cqa_context.for_host) then
      -- cqa_context.freq = tonumber (
      --   string.match (project:get_cpu_frequency(), "(%d+%.%d+)%s*GHz")
      --);
   end

   local reports = cqa:get_reports (cqa_results, cqa_context.tags);

   for _,v in ipairs (reports.common.header) do print (v) end
   if (reports.paths[1] ~= nil) then
      for _,v in ipairs (reports.paths[1].header) do print (v) end
   end
   print ("");

   if (not cqa_results.common ["can be analyzed"]) then return end

   -- for each path
   for path_id,reports_path in ipairs (reports.paths) do
      if (#reports.paths > 1) then print_section ("Path #"..path_id, 4) end
      print_path (reports_path, cqa_context.confidence);
   end
end

local function get_compiler_optimization_report (cqa_results)
   if (cqa_results == nil) then return end

   local fct = cqa_results.common ["function"];
   local compiler_string = fct:get_compiler();
   if (compiler_string == nil) then return end

   if (is_intel (compiler_string)) then
      local cqa_context = cqa_results.common.context;
      for _,log_file_name in ipairs (cqa_context.opr or {}) do
         local parsed = tools.icc_parse (fct:get_asmfile(), log_file_name);
         if (parsed ~= nil) then
            local min = string.match (cqa_results.common["src line min-max"], "%d+")
            min = tonumber (min)

            local report = ""
            for _,v in ipairs (parsed) do
               if (string.find (v.ids, "loop") ~= nil and v.content ~= nil) then
                  for k2,v2 in ipairs (v.content.loops) do
                     if (string.find (cqa_results.common["src file"], v2.file.."$") ~= nil and
                         (min - tonumber (v2.line)) <= 2) then
                        report = report .. string.format ("%d: %s\n", k2, table.concat (v2.content, ","))
                     end
                  end
               end
            end

            if (#report > 0) then return report end
         end
      end
   end
end

-- Prints the static analysis report for a source loop (in fct-loops mode, without "requested loope")
local function print_src (last_line, src_loop, cqa_results)
   print_section ("Source loop ending at line "..last_line, 2);

   print_header ("Composition and unrolling", "-");

   -- Prints composition
   io.write ("It is composed of the ");

   if (#src_loop ["loop IDs"] == 1) then
      print ("loop "..src_loop["loop IDs"][1]);
   else
      print ("following loops [ID (first-last source line)]:");

      table.sort (src_loop ["loop IDs"]); -- makes output determinist

      for _,id in ipairs (src_loop ["loop IDs"]) do
         local src_lines
         if (cqa_results [id] ~= nil) then
            src_lines = cqa_results [id].common ["src line min-max"]
         else
            src_lines = "?-"..last_line
         end

         print (string.format (" - %d (%s)", id, src_lines));
      end
   end -- if one bin loop in the src loop

   -- Prints unrolling
   if (src_loop ["unroll info"] ~= nil) then
      io.write ("and is "..src_loop ["unroll info"]);
      if (string.find (src_loop ["unroll info"], "unrolled by") ~= nil) then
         io.write (" (including vectorization)");
      end
      io.write (".");
   end
   io.write ("\n");

   -- Prints compiler optimization report, if any
   local opt_report = get_compiler_optimization_report (cqa_results[src_loop ["loop IDs"][1]]);
   if (opt_report ~= nil) then
      io.write ("\n");
      print_header ("Compiler optimization report", "-");
      print (opt_report);
   end

   -- Prints details about binary loops composing the source loop
   if (src_loop ["unroll info"] ~= nil and
       string.find (src_loop ["unroll info"], "^unrolled") ~= nil) then
      table.sort (src_loop.main); -- makes output determinist

      print ("");
      print ("The following loops are considered as:");
      print (" - unrolled and/or vectorized main: "..table.concat (src_loop.main, ", "));
      if (#src_loop.intermediate > 0) then
         table.sort (src_loop.intermediate); -- makes output determinist
         print (" - unrolled and/or vectorized peel or tail: "..table.concat (src_loop.intermediate, ", "));
      end
      if (#src_loop.peel_or_tail > 0) then
         table.sort (src_loop.peel_or_tail); -- makes output determinist
         print (" - peel or tail: "..table.concat (src_loop.peel_or_tail, ", "));
      end

      print ("The analysis will be displayed for the unrolled and/or vectorized loops: "..table.concat (src_loop.main, ", "));

      for _,id in pairs (src_loop.main) do
         print ("");
         print_section ("Binary (unrolled and/or vectorized) loop #"..id, 3);
         print_cmn (cqa_results [id]);
      end

   else -- if not unrolled, multi-versionned or unrolled with no peel/tail code
      if (#src_loop ["loop IDs"] > 1) then
         print ("The analysis will be displayed for all loops");
      end

      for _,id in ipairs (src_loop ["loop IDs"]) do
         print ("");
         print_section ("Binary loop #"..id, 3);
         print_cmn (cqa_results [id]);
      end
   end
end

--- Prints on the standard output static analysis results
-- @param fct a function
-- @param cqa_results static analysis results for innermost loops of fct
function print_reports (fct, cqa_results, requested_loops)
   local params = {}

   params.print_fct_name = function ()
      print_section ("Function: "..params.fct_name, 1)
   end

   params.print_print_no_results = function ()
      print ("No results to display in this function.")
   end

   params.print_debug_data_string = function (crc)
      local str = cqa.api.reports.get_debug_data_string (crc)
      if (str) then print (str) end
   end

   params.print_code_specialization = function (str)
      if (str) then print (str) end
   end

   params.print_no_loops = function ()
      print_cmn (cqa_results)
      print ("");
   end

   params.print_src_file_path = function (src_file_path)
      if (src_file_path ~= nil) then
         print ("These loops are supposed to be defined in: "..src_file_path);
      end
      print ("");
   end

   params.print_src = print_src

   params.print_bin_in_src = function (cqa_results, id, is_already_printed)
      print ("");
      print_section ("Binary loop #"..id, 2);
      print_cmn (cqa_results [id]);

      is_already_printed [id] = true;
   end

   params.print_bin_loops = function (bin_loops, is_already_printed)
      -- If at least one binary loop not printed before
      local loops_to_print = {}
      for _,v in ipairs (bin_loops       or {}) do loops_to_print[v] = true end
      for _,v in ipairs (requested_loops or {}) do loops_to_print[v] = true end
      for v in pairs (loops_to_print) do
         if (is_already_printed [v] or cqa_results[v] == nil) then
            loops_to_print[v] = nil
         end
      end

      if (next (loops_to_print) ~= nil) then
         print_section ("Binary loops in the function named "..params.fct_name, 2)

         for v in pairs (loops_to_print) do
            print_section ("Binary loop #"..v, 3);
            print_cmn (cqa_results[v]);
         end

         print ("");
      end
   end

   params.print_src_end = function () print ("") end

   params.print_function_end = function ()
      print ("All innermost loops were analyzed.");
      print ("");
   end

   params.tags = {
      list_start = "",
      list_stop = "",
      list_elem_start = " - ",
      list_elem_stop = "\n",
      sublist_elem_start = "  * ",
      sublist_elem_stop = "\n",

      ordered_list_elem_start = " %d) ",
      ordered_sublist_elem_start = "  %d) ",

      lt = "<",
      gt = ">",
      amp = "&",

      table_start = "",
      table_stop  = "",
      table_header_start = "",
      table_header_stop  = "",
      table_elem_start = "",
      table_elem_stop  = "",
      table_row_start = "",
      table_row_stop  = "\n",
   }

   cqa.output.txt_html_shared.print_reports (fct, cqa_results, requested_loops, params)
end
