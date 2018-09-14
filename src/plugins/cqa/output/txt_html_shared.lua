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

--- Module cqa.output.txt_html_shared
-- Defines the get_reports function used to generate reports for functions, loops or paths
module ("cqa.output.txt_html_shared", package.seeall)

require "cqa.api.reports_shared"
local is_intel = cqa.api.reports_shared.is_intel;
local is_gnu   = cqa.api.reports_shared.is_gnu;

local function get_code_specialization_string (fct)
   local comp_str = fct:get_compiler()
   if (not is_intel (comp_str) and not is_gnu (comp_str)) then return end

   local comp_opts
   if (is_intel (comp_str)) then
      fct:get_asmfile():analyze_compile_options()
      comp_opts = fct:get_compile_options()
      if (comp_opts ~= nil) then comp_opts = string.match (comp_opts, "([^:]*)$") end
   end

   local spe_flags = {}
   if (comp_opts ~= nil) then
      -- Ex: -axCORE-AVX2, -xHost, -fast (including -xHost)
      for _,regex in ipairs ({"%-a?x%S+", "%-fast", "%-march=%S+"}) do
         for flag in string.gmatch (comp_opts, regex) do table.insert (spe_flags, flag) end
      end
   elseif (is_gnu (comp_str)) then
      -- Ex: -march=native or -march=haswell
      for flag in string.gmatch (comp_str, "%-march=%S+") do table.insert (spe_flags, flag) end
   end

   if (#spe_flags == 0) then return end

   if (is_gnu (comp_str)) then
      str = "Code for this function has been specialized for %s. For execution on another machine, recompile on it or with explicit target (example for a Haswell machine: use -march=haswell, see compiler manual for full list)."
   else -- Intel
      str = "Code for this function has been specialized for %s. For execution on another machine, recompile on it or with explicit target (example for a Haswell machine: use -xCORE-AVX2, see compiler manual for full list)."
   end

   -- Check if code specialized for compiler-host processor
   for _,v in ipairs (spe_flags) do
      v = string.lower (v)
      if (string.find (v, "march=native") ~= nil or
          string.find (v, "xhost") ~= nil or
          string.find (v, "%-fast") ~= nil) then
         return string.format (str, "the machine where it has been compiled")
      end
   end

   local arch_obj = fct:get_asmfile():get_arch_obj()
   local arch_name = arch_obj:get_name()

   -- Check if code already specialized for target uarch (explicit)
   if (#comp_targets > 0) then
      return string.format (str, cqa.api.reports_shared.get_uarchs_str (comp_targets, arch_obj))
   end
end

-- Returns requested loops present in a given source loop
local function get_bin_in_src (bin_loops, src_loop)
   local loops = {}

   for _,bin_loop in pairs (bin_loops) do
      for _,bin_loop_in_src_loop in pairs (src_loop ["loop IDs"]) do
         if (bin_loop == bin_loop_in_src_loop) then
            table.insert (loops, bin_loop)
         end
      end
   end

   return loops;
end

--- Prints static analysis results for txt or html outputs
-- @param fct a function
-- @param cqa_results static analysis results for innermost loops of fct
function print_reports (fct, cqa_results, requested_loops, params)
   params.fct_name = fct:get_demname() or fct:get_name()

   params.print_fct_name ()

   if (cqa_results == nil) then
      if (params.print_no_results) then params.print_no_results () end
      return
   end

   local crc = cqa_results.common
   if (crc == nil) then
      -- cqa_results is an array of cqa_results belonging to several loops
      local _, first_cqa_results = next (cqa_results)
      crc = first_cqa_results.common
   end

   crc.context.tags = params.tags

   params.print_debug_data_string (crc)
   params.print_code_specialization (get_code_specialization_string (fct))

   local src_line_groups = crc.context._src_line_groups
   local src_loops
   local bin_loops
   if (src_line_groups ~= nil) then
      src_line_groups = src_line_groups [fct:get_id()]
      if (src_line_groups ~= nil) then
         src_loops = src_line_groups.src
         bin_loops = src_line_groups.bin
      end
   end

   if (src_loops == nil and bin_loops == nil and requested_loops == nil) then
      params.print_no_loops ();
      return;
   end

   local is_already_printed = {}

   -- If at least one source loop
   if (src_loops ~= nil and next (src_loops) ~= nil) then
      local src_file_path
      if (fct:has_debug_data ()) then
         src_file_path = fct:get_src_file_path ();
      end

      local src_file_paths = Table:new()
      for _,src_loop in pairs (src_loops) do
         for _,id in ipairs (src_loop ["loop IDs"]) do
            if (cqa_results [id] ~= nil) then
               src_file_paths [cqa_results [id].common ["src file"]] = true;
            end
         end
      end
      if (src_file_paths:get_size() == 1) then
         src_file_path = next (src_file_paths);
      else
         if (src_file_paths:get_size() > 1) then
            Debug:warn ("Source loops are defined in different files...");
         else
            Debug:warn ("Source loops are defined in unknown files...");
         end

         Debug:warn ("considering the file defining the common function.")
      end

      params.print_src_file_path (src_file_path)

      -- Sort source loops by increasing end line
      local src_loops_keys = {};
      for k in pairs (src_loops) do table.insert (src_loops_keys, k) end
      table.sort (src_loops_keys);

      -- Print source loops or requested loops in source loops
      for _,last_line in ipairs (src_loops_keys) do
         local src_loop = src_loops [last_line]
         if (requested_loops == nil) then
            params.print_src (last_line, src_loop, cqa_results, src_file_path);
         else
            for _,id in ipairs (get_bin_in_src (requested_loops, src_loop)) do
               params.print_bin_in_src (cqa_results, id, is_already_printed)
            end
         end
      end

      if (params.print_src_end) then params.print_src_end () end
   end

   params.print_bin_loops (requested_loops or bin_loops, is_already_printed)

   if (params.print_function_end) then params.print_function_end () end
end
