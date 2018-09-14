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

--- Module cqa.api.arm64.reports
-- Defines the get_*_reports functions used to get k1om specific reports for loop paths
module ("cqa.api.arm64.reports", package.seeall)

require "cqa.api.reports_shared";

local rs = cqa.api.reports_shared;

local find   = string.find;
local format = string.format;
local insert = table.insert;
local concat = table.concat;

function get_front_end_string (cqa_results)
   local cqa_context = cqa_results.common.context;
   local uarch_consts = cqa_context.uarch_consts;

   local str = "";

   -- /loop buffer and uop cache
   if (uarch_consts ["uop cache capacity"] == nil) then
      local fit_into_loop_buffer = cqa_results ["fit into loop buffer"];
      if (fit_into_loop_buffer == true) then
         str = str .. "FIT INTO THE LOOP BUFFER\n";
      elseif (fit_into_loop_buffer == false) then
         str = str .. "DOES NOT FIT INTO THE LOOP BUFFER\n";
      end

   else
      local fit_into_uop_cache = cqa_results ["fit into uop cache"];
      if (fit_into_uop_cache == true) then
         str = str .. "FIT IN UOP CACHE\n";
      elseif (fit_into_uop_cache == false) then
         str = str .. "DOES NOT FIT IN UOP CACHE\n";
      end
   end

   local rows = {}

   local function append (metric, longest_metric_length)
      local cycles = cqa_results ["[arm64] cycles "..metric];
      if (cycles ~= nil) then
         insert (rows, { metric, format ("%.2f cycles", cycles) });
      end
   end

   for _,v in ipairs {"front end"} do
      append (v, #"micro-operation queue");
   end

   return str .. rs.print_array_to_string (rows, cqa_context.tags, false);
end

-- Returns back-end report
function get_back_end_report (cqa_results)
   local report = {title = "Back-end", txt = ""};
   local cqa_context = cqa_results.common.context;
   local nb_ports = cqa_context.uarch_consts ["nb execution ports"];

   -- Dispatch
   local dispatch_array = {}

   -- Dispatch: header line
   local first_row = { string.rep (" ", math.max (#"uops", #"cycles")) }
   for i = 0, nb_ports-1 do
      first_row [i+2] = format ("P%d", i)
   end
   dispatch_array [1] = first_row

   -- Dispatch: uops and cycles
   for k,v in ipairs ({"uops", "cycles"}) do
      local row = { v }
      local dispatch_entry = cqa_results.dispatch [v]
      for i = 0, nb_ports-1 do
         row [i+2] = format ("%.2f", dispatch_entry [i])
      end
      dispatch_array [k+1] = row
   end

   local rows = {}

   -- DIV/SQRT
   local cycles_div_sqrt = cqa_results ["cycles div sqrt"];
   rows [1] = { "Cycles executing div or sqrt instructions" }
   if (cycles_div_sqrt ~= nil and cycles_div_sqrt.min > 0) then
      rows [1][2] = get_min_max_string (cycles_div_sqrt)
   else
      rows [1][2] = "NA"
   end

   -- Data dependencies
   if (cqa_results.common ["RecMII"] ~= nil) then
      insert (rows, { "Longest recurrence chain latency (RecMII)",
                      get_min_max_string (cqa_results.common ["RecMII"]) })
   end

   report.txt = rs.print_array_to_string (dispatch_array, cqa_context.tags, true) .. "\n" ..
      rs.print_array_to_string (rows, cqa_context.tags, false)

   return report;
end

function get_brief_report (cqa_results)
   local txt = "";

   local lvl = cqa_results.common.context.memory_level;

   -- Vectorization gain
   if (tonumber (cqa_results ["cycles L1 if fully vectorized"]) ~= nil and
       cqa_results ["cycles L1 if fully vectorized"].max < cqa_results.cycles [lvl].max) then
      txt = format ("Potential gain by full vectorization: %.2f\n",
                    cqa_results.cycles [lvl].max /
                    cqa_results ["cycles L1 if fully vectorized"].max);
   end

   -- Clean gain (scalar integer instructions)
   local cycles_if_clean = cqa_results ["cycles if clean"][lvl];
   if (cycles_if_clean ~= nil and too_much_int_insns (cqa_results, cycles_if_clean)) then
      txt = format ("Potential gain by simplifying address computation: %.2f\n",
                    cqa_results.cycles [lvl].max / cycles_if_clean.max);
   end

   -- Data dependencies gain
   local cycles_nodeps = cqa_results ["cycles if no deps"];
   if (cycles_nodeps ~= nil) then cycles_nodeps = cycles_nodeps [lvl] end
   if (cycles_nodeps ~= nil and cycles_nodeps.max < cqa_results.cycles [lvl].max) then
      txt = txt .. format ("Potential gain by hiding inter-iteration latency: %.2f\n",
                           cqa_results.cycles [lvl].max / cycles_nodeps.max);
   end

   -- Bottlenecks gain
   local btn_report = get_btn_report (cqa_results);
   local btn_txt = string.gsub (btn_report.txt, "\n(By removing)", "%1");
   local speedup = string.match (btn_txt, "(%d+%.%d%d)x speedup");
   if (speedup ~= nil) then
      txt = txt .. string.gsub (btn_txt, "(By removing .* speedup%))",
                                "Potential gain by removing these bottlenecks: "..speedup);
   end

   -- If no potential gain
   if (string.len (txt) == 0) then
      txt = "Detected no potential gain";
   end

   return { title = "Brief", txt = txt };
end

function get_vec_gain_report (cqa_results)
   local crc = cqa_results.common

   -- Peel/tail loop
   if (crc ["src loop"] ~= nil and
       find (crc ["src loop"]["unroll info"], "^unrolled") ~= nil and
       crc ["is main/unrolled"] == false) then
      return nil
   end

   local lvl = crc.context.memory_level[1];
   -- TODO: think to iterate over all memory levels
   if (cqa_results ["cycles L1 if fully vectorized"] == nil or
       cqa_results ["cycles L1 if fully vectorized"].max >= cqa_results.cycles [lvl].max) then
      return nil;
   end

   local report = {title = "Vectorization"};
   local blocks_type = crc ["blocks type"];
   local packed_ratio_all = cqa_results ["packed ratio"]["all"];

   local fully = ""
   if (packed_ratio_all == 0) then
      report.txt = "Your "..blocks_type.." is NOT VECTORIZED and could benefit from vectorization.\n"
   elseif (rs.false_vectorization (cqa_results) == true) then
      report.txt = "Your "..blocks_type.." is probably NOT VECTORIZED and could benefit from vectorization.\n"
   else
      report.txt = "Your "..blocks_type.." is PARTIALLY VECTORIZED and could benefit from full vectorization.\n"
      fully = "fully "
   end

   report.txt = report.txt .. format ("By %svectorizing your %s, you can lower the cost of an iteration from %.2f to %.2f cycles (%.2fx speedup).", fully, blocks_type, cqa_results.cycles [lvl].max, cqa_results ["cycles L1 if fully vectorized"].max, cqa_results.cycles [lvl].max / cqa_results ["cycles L1 if fully vectorized"].max);
   report.details = format ("Since your execution units are vector units, only a %svectorized %s can use their full power.\n", fully, blocks_type);


   local comp_str = crc.comp_str;
   local lang_code = crc.lang_code;

   local wa_buf = {}

   if (tonumber (packed_ratio_all) ~= nil and packed_ratio_all < 80) then
      local wa_tune_compiler = { header    = "Try another compiler or update/tune your current one",
                                 list_type = "unordered" }

      local wa_unit_stride = { list_type = "unordered",
                               list = {} }

      if (blocks_type == "loop" or block_type == "path") then
         if (is_gnu (comp_str)) then
            wa_tune_compiler.list = { "if not already done, recompile with O3 to enable the compiler vectorizer. In case of reduction loop, use Ofast instead of O3 or add ffast-math." }
         end

         -- Storage order
         local storage_order_str = "If your arrays have 2 or more dimensions, check whether elements are accessed contiguously and, otherwise, try to permute loops accordingly"
         if (lang_code == Consts.LANG_C) then
            storage_order_str = storage_order_str .. ":\nC storage order is row-major: for(i) for(j) a[j][i] = b[j][i]; (slow, non stride 1) => for(i) for(j) a[i][j] = b[i][j]; (fast, stride 1)"
         elseif (lang_code == Consts.LANG_FORTRAN) then
            storage_order_str = storage_order_str .. ":\nFortran storage order is column-major: do i do j a(i,j) = b(i,j) (slow, non stride 1) => do i do j a(j,i) = b(i,j) (fast, stride 1)"
         end
         wa_unit_stride.header = "Remove inter-iterations dependences from your loop and make it unit-stride"
         insert (wa_unit_stride.list, storage_order_str)

      else
         wa_unit_stride.header = "Make array accesses unit-stride"
      end -- loop or path

      -- AoS => SoA
      local aos_soa_str = "If your "..blocks_type.." streams arrays of structures (AoS), try to use structures of arrays instead (SoA)"
      if (lang_code == Consts.LANG_C) then
         aos_soa_str = aos_soa_str .. ":\nfor(i) a[i].x = b[i].x; (slow, non stride 1) => for(i) a.x[i] = b.x[i]; (fast, stride 1)";
      elseif (lang_code == Consts.LANG_FORTRAN) then
         aos_soa_str = aos_soa_str .. ":\ndo i a(i)%x = b(i)%x (slow, non stride 1) => do i a%x(i) = b%x(i) (fast, stride 1)";
      end
      insert (wa_unit_stride.list, aos_soa_str)

      insert (wa_buf, wa_tune_compiler)
      insert (wa_buf, wa_unit_stride)

   else
      local code_spe_wa = get_code_specialization_workaround (cqa_results)
      if (code_spe_wa ~= nil) then insert (wa_buf, code_spe_wa) end
      insert (wa_buf, get_align_vectors_string (cqa_results))
   end

   report.workaround = get_workaround_string (wa_buf, crc.context.tags)
   return report;
end