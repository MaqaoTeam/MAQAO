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

--- Module cqa.api.reports
-- Defines the get_reports function used to generate reports for functions, loops or paths
module ("cqa.api.reports", package.seeall)

require "cqa.api.arm64.reports";
require "cqa.api.reports_shared";

local find = string.find;
local format = string.format;
local insert = table.insert;
local concat = table.concat;

local rs = cqa.api.reports_shared
local is_intel = rs.is_intel;
local is_gnu   = rs.is_gnu;
local get_min_max_string = rs.get_min_max_string
local get_workaround_string = rs.get_workaround_string

-- A report is a table with following fields:
--  * title     : title
--  * txt       : content
--  * details   : extra content [optional]
--  * workaround: workarounds [optional]

function get_fma_report (cqa_results)
   local cqa_context = cqa_results.common.context

   if (not cqa_context.uarch_consts ["Supports FMA"]) then return nil end

   local FP_ops = cqa_results ["FP ops"]
   local has_both_ADD_MUL = (FP_ops ["add-sub"] > 0 and FP_ops ["mul"] > 0)
   local nb_FMAs = FP_ops ["fma"]

   if (not has_both_ADD_MUL and nb_FMAs == 0) then return nil end

   local txt_buf = {}
   local wa_str

   if (nb_FMAs > 0) then
      txt_buf [1] = format ("Detected %d FMA (fused multiply-add) operations.", nb_FMAs)
   end

   if (has_both_ADD_MUL) then
      insert (txt_buf, "Presence of both ADD/SUB and MUL operations.")

      local wa_buf = {}

      insert (wa_buf, "Try to change order in which elements are evaluated (using parentheses) in arithmetic expressions containing both ADD/SUB and MUL operations to enable your compiler to generate FMA instructions wherever possible.\n" ..
              "For instance a + b*c is a valid FMA (MUL then ADD).\n" ..
              "However (a+b)* c cannot be translated into an FMA (ADD then MUL).")

      wa_str = get_workaround_string (wa_buf, cqa_context.tags)
   end

   return { title = "FMA",
            txt   = concat (txt_buf, "\n"),
            workaround = wa_str }
end

-- Returns assembly code report
local function get_asm_code_report (cqa_results)
   local crc = cqa_results.common
   local addr_str = format ("In the binary file, the address of the %s is: %s\n",
                            crc["blocks type"],
                            crc ["addr"])

   local asm_code

   local arch = cqa_results.common.context.arch
   -- WARNING: instructions are not reordered (for example by increasing address)
   local buf = {}
   for i, insn in ipairs (cqa_results.insns) do
      buf[i] = insn:get_asm_code()
   end

   collectgarbage ("collect") -- Luajit: avoids "out of memory" at table.concat
   asm_code = concat (buf, "\n")

   return {title = "ASM code",
           txt   = addr_str.."\n"..asm_code}
end

-- Returns general properties report
local function get_gen_prop_report (cqa_results)
   local arch = cqa_results.common.context.arch;

   local rows = {}

   local function append (metric, fmt)
      if (cqa_results [metric] == nil) then return end
      insert (rows, { metric, format (fmt or "%d", cqa_results [metric]) })
   end

   append ("nb instructions")
   append ("loop length")
   append ("nb stack references")

   return { title = "General properties",
            txt = rs.print_array_to_string (rows, cqa_results.common.context.tags, false) }
end

-- Returns front-end report
local function get_front_end_report (cqa_results)
   local arch = cqa_results.common.context.arch;

   return {title = "Front-end",
           txt   = cqa.api[arch].reports.get_front_end_string (
              cqa_results)};
end

-- Returns cycles summary report
local function get_cycles_summary_report (cqa_results)
   local rows = {}

   -- Front-end
   if (cqa_results ["cycles front end"] ~= nil) then
      insert (rows, { "Front-end", format ("%.2f", cqa_results ["cycles front end"]) })
   end

   local crc = cqa_results.common

   -- Back-end (execution ports and DIV/SQRT)
   local arch = crc.context.arch;

   -- RecMII (critical path, data dependencies)
   if (crc ["RecMII"] ~= nil) then
      insert (rows, { "Data deps.", get_min_max_string (crc ["RecMII"]) })
   end

   -- #PRAGMA_NOSTATIC MEM_PROJ
   if (cqa_results ["cycles memory"] ~= nil) then
      for _,lvl in ipairs (crc.context.memory_level) do
         if (cqa_results ["cycles memory"][lvl] ~= nil) then
            local cycles_mem = cqa_results ["cycles memory"][lvl]
            if (cycles_mem.auto ~= nil) then
               insert (rows, { format ("%s-auto", lvl),
                               format ("%.2f-%.2f", cycles_mem.auto.min, cycles_mem.auto.max) })
            end
            for _,stride in ipairs ({"stride1", "ind_stride1", "ind_random"}) do
               if (cycles_mem[stride] ~= nil) then
                  insert (rows, { format ("%s-%s", lvl, stride),
                                  format ("%.2f-%.2f", cycles_mem[stride].min, cycles_mem[stride].max) })
               end
            end
         end
      end
   end
   -- #PRAGMA_STATIC

   for _,lvl in ipairs (crc.context.memory_level) do
      -- Maximum of all previous stages (cycles)
      if (cqa_results.cycles [lvl] ~= nil) then
         insert (rows, { format ("Overall %s", lvl),
                         get_min_max_string (cqa_results.cycles [lvl]) })
      end
   end

   return { title = "Cycles summary",
            txt = rs.print_array_to_string (rows, crc.context.tags, false) }
end

-- Returns vectorization ratios (expert) report
-- Breakdown is given per mode (INT, FP, INT+FP) and then per family (load, store, add...)
local function get_expert_vec_ratio_report (cqa_results)
   local modes;
   if (cqa_results ["packed ratio INT"]["all"] ~= "NA" and
       cqa_results ["packed ratio FP"]["all"] ~= "NA") then
      modes = {"INT", "FP", "INT+FP"};
   else
      modes = {"INT+FP"};
   end

   local report = {title = "Vectorization ratios", txt = ""};

   for _,mode in ipairs (modes) do
      if (#modes == 3) then report.txt = report.txt .. mode .. "\n" end

      local packed_ratio
      if (mode == "INT" or mode == "FP") then
         packed_ratio = cqa_results ["packed ratio "..mode]
      else
         packed_ratio = cqa_results ["packed ratio"]
      end

      if (packed_ratio["all"] ~= "NA") then
         local rows = { { "all", format ("%d%%", packed_ratio["all"]) } }

         for i,v in ipairs {"load", "store", "mul", "add-sub", "other"} do
            if (packed_ratio[v] ~= "NA") then
               rows [i+1] = { v, format ("%d%%", packed_ratio[v]) }
            else
               rows [i+1] = { v, format ("NA (no %s vectorizable/vectorized instructions)", v) }
            end
         end

         local tags = cqa_results.common.context.tags
         report.txt = report.txt .. rs.print_array_to_string (rows, tags, false)
      else
         report.txt = report.txt .. "No vectorizable/vectorized instructions\n";
      end
   end

   return report;
end

-- Returns the location string (position in source and binary code)
local function get_location_string (crc)
   local location;
   local blocks_type = crc["blocks type"];

   if (crc ["src regions"] ~= nil and
       #crc ["src regions"] > 1) then
      local tags = crc.context.tags

      local buf = {}
      for i,v in ipairs (crc ["src regions"]) do
         buf[i] = tags.list_elem_start .. v .. tags.list_elem_stop
      end

      location = format ("The %s is defined in:\n%s%s%s\n", blocks_type,
                         tags.list_start, concat (buf), tags.list_stop);

   elseif (crc ["src file"        ] ~= nil and
           crc ["src line min-max"] ~= nil) then
      location = format ("The %s is defined in %s:%s.\n",
			 blocks_type,
                         crc ["src file"],
                         crc ["src line min-max"]);
   else
      location = "";
   end

   return location;
end

local function get_unroll_string (crc)
   local unroll_info = crc ["unroll info"]
   if (unroll_info == nil) then return nil end

   if (find (unroll_info, "unrolled by") ~= nil) then
      unroll_info = unroll_info .. " (including vectorization)"
   end

   if (crc ["unroll loop type"] == nil) then
      return format ("The related source loop is %s.", unroll_info)
   end

   return format ("It is %s loop of related source loop which is %s.",
                  crc ["unroll loop type"], unroll_info)
end

-- Returns a string giving computational resource usage
local function get_comp_usage_string (cqa_results)
   local cqa_context = cqa_results.common.context;

   -- Returns peak FP computation performance (FP operations per cycle) which is the product of
   -- FP instructions per cycle (ILP) and FP operations per instruction (SIMD)
   local function get_peak ()
      local arch = cqa_context.arch;
      local n = 1;
      local uarch_consts = cqa_context.uarch_consts;

      -- SIMD
      local FP_vec_size; -- number of FP elements in a vector register
      FP_vec_size = uarch_consts ["vector size in bytes"];

      if (cqa_results.dominant_element_size ~= nil) then
         n = FP_vec_size / cqa_results.dominant_element_size;
      end

      -- ILP
      n = n * 4; -- 2 FMA units

      return n;
   end

   local max_FP_ops_per_cycle = get_peak();
   local freq = cqa_context.freq;

   -- Returns ratio between used and peak performance
   local buf = {}
   for rank,lvl in ipairs (cqa_context.memory_level) do
      local FP_ops_per_cycle = cqa_results ["FP operations per cycle"][lvl].min or 0;
      if (cqa_context.arch == "arm64") then
         FP_ops_per_cycle = cqa_results ["normalized FP operations per cycle"][lvl].min or 0;
      end

      local header = ""
      if (#(cqa_context.memory_level) > 1) then
	 header = "In "..lvl.." "
      end

      if (freq ~= nil) then
	 insert (buf, format ("%s%d%% of peak computational performance is used (%.2f out of %.2f FLOP per cycle (%.2f GFLOPS @ %.2fGHz))", header, (100 * FP_ops_per_cycle) / max_FP_ops_per_cycle, FP_ops_per_cycle, max_FP_ops_per_cycle, FP_ops_per_cycle * freq, freq));
      else
	 insert (buf, format ("%s%d%% of peak computational performance is used (%.2f out of %.2f FLOP per cycle (GFLOPS @ 1GHz))", header, (100 * FP_ops_per_cycle) / max_FP_ops_per_cycle, FP_ops_per_cycle, max_FP_ops_per_cycle));
      end
   end

   return concat (buf, "\n")
end

-- Prints cycles and load and store units usage.
local function get_cycles_and_mem_report (cqa_results)
   local report = {title = "Cycles and memory resources usage"};

   if (tonumber (cqa_results ["nb masked instructions"]) ~= nil and
       tonumber (cqa_results ["nb masked instructions"]) > 0) then
      report.txt = "Detected masked instructions: assuming all mask elements are active.\n";
   else
      report.txt = "";
   end

   local cqa_context = cqa_results.common.context;
   local tags = cqa_context.tags
   local arch = cqa_context.arch;
   local event;
   if (cqa_results.common ["blocks type"] == "loop") then
      event = "iteration of the binary loop";
   elseif (cqa_results.common ["blocks type"] == "function") then
      event = "call to the function";
   else
      event = "execution of the path";
   end

   -- Prints cycles in each memory level
   for _,lvl in ipairs (cqa_context.memory_level) do
      local memory_level_string;
      if (find (lvl, "^L%d$") ~= nil) then memory_level_string = "the "..lvl.." cache";
      else memory_level_string = lvl;
      end

      report.txt = report.txt .. format ("Assuming all data fit into %s, each %s takes %.2f cycles. At this rate:\n", memory_level_string, event, cqa_results.cycles [lvl].max) .. tags.list_start;

      -- Prints the load/store units usage
      for _,v in pairs ({"load", "stor"}) do
	 local bytes_per_cycle = cqa_results ["bytes "..v.."ed per cycle"][lvl].min;

	 if (bytes_per_cycle > 0) then
	    local max_bytes_per_cycle;

	    -- Compute peak memory performance (bytes per cycle)
            local uarch_consts = cqa_context.uarch_consts;   
            if (v == "load") then                                        
               max_bytes_per_cycle = uarch_consts ["load unit native width"] *
                  uarch_consts ["nb load units"];                                     
            elseif (v == "stor") then                                         
               max_bytes_per_cycle = uarch_consts ["store unit native width"] *
                  uarch_consts ["nb store units"];                          
            end

	    local txt = "load"; if (v == "stor") then txt = "store" end

	    local freq = cqa_context.freq;
	    if (freq ~= nil) then
	       report.txt = report.txt .. format ("%s%d%% of peak %s performance is reached (%.2f out of %.2f bytes %sed per cycle (%.2f GB/s @ %.2fGHz))%s",
                                                  tags.list_elem_start,
						  (100 * bytes_per_cycle) / max_bytes_per_cycle,
						  txt, bytes_per_cycle, max_bytes_per_cycle, v,
						  bytes_per_cycle * freq, freq,
                                                  tags.list_elem_stop);
	    else
	       report.txt = report.txt .. format ("%s%d%% of peak %s performance is reached (%.2f out of %.2f bytes %sed per cycle (GB/s @ 1GHz))%s",
                                                  tags.list_elem_start,
						  (100 * bytes_per_cycle) / max_bytes_per_cycle,
						  txt, bytes_per_cycle, max_bytes_per_cycle, v,
                                                  tags.list_elem_stop);
	    end
	 end
      end

      report.txt = report.txt .. tags.list_stop
   end

   return report;
end

-- Finds the most represented instruction pattern (x87, SS, ...)
-- TODO: improve (no nil but 0-value entries in pattern)
local function get_dominant_pattern (cqa_results)
   local patterns;
   local arch = cqa_results.common.context.arch;
      patterns = {"INTD", "INTQ", "PS", "PD"};

   local dom_pattern;

   if (cqa_results ["pattern mth"] == nil) then
      return nil;
   end

   for _,pattern in pairs (patterns) do
      local nb_insns = cqa_results ["pattern mth"][pattern];

      if (nb_insns ~= nil and nb_insns > 0 and (dom_pattern == nil or nb_insns > cqa_results ["pattern mth"][dom_pattern])) then
         dom_pattern = pattern;
      end
   end

   return dom_pattern;
end

-- Returns the type and the size of the element corresponding to the dominant pattern
local function get_dominant_element (cqa_results)
   local dom_pattern = get_dominant_pattern (cqa_results);

   if (dom_pattern == nil) then return end

   if (dom_pattern == "x87") then return "x87 FP" end

   local arch = cqa_results.common.context.arch;
end

local function get_peel_tail_overhead_report (cqa_results)
   local crc = cqa_results.common
   local src_loop = crc ["src loop"];

   -- applies only on peel/tail loops of unrolled/vectorized source loops
   if (src_loop == nil or
       find (src_loop ["unroll info"], "^unrolled") == nil or
       crc ["is main/unrolled"] ~= false) then
      return nil;
   end

   local prof_gen;
   local prof_use;
   local intel_loop_count = "";
   local spe_ex = "";

   local fct = crc ["function"];
   if (fct:has_debug_data()) then
      local lang_code = fct:get_language();
      local comp_str = fct:get_compiler();

      if (is_intel (comp_str)) then
         prof_gen = "prof-gen";
         prof_use = "prof-use";
         if (lang_code == Consts.LANG_C or
             lang_code == Consts.LANG_CPP) then
            intel_loop_count = "insert #pragma loop_count min(n),max(n),avg(n) at top of your loop";
         elseif (lang_code == Consts.LANG_FORTRAN) then
            intel_loop_count = "insert !DIR$ LOOP COUNT MAX(n),MIN(n),AVG(n) at top of your loop";
         end
      elseif (is_gnu (comp_str)) then
         prof_gen = "fprofile-generate";
         prof_use = "fprofile-use";
      end

      if (lang_code == Consts.LANG_C or
          lang_code == Consts.LANG_CPP) then
         local tags_lt = crc.context.tags.lt
         spe_ex = format (" For instance, replace for (i=0; i%sn; i++) foo(i) with:\n"..
            "switch (n) {\n"..
         "  case (4): for (i=0; i%s4; i++) foo(i); break;\n"..
         "  case (6): for (i=0; i%s6; i++) foo(i); break;\n"..
         "  default : for (i=0; i%sn; i++) foo(i); break;\n"..
            "}", tags_lt, tags_lt, tags_lt, tags_lt)
      elseif (lang_code == Consts.LANG_FORTRAN) then
         spe_ex = " For instance, replace do i=1,n foo(i) with:\n"..
            "select case (n)\n"..
         "  case (4): do i=1,4 foo(i)\n"..
         "  case (6): do i=1,6 foo(i)\n"..
         "  default : do i=1,n foo(i)\n"..
         "end select"
      end
   end

   local wa_buf
   if (prof_gen and prof_use) then
      wa_buf = { "recompile with -"..prof_gen..", execute and recompile with -"..prof_use.." (profile-guided optimization)" }
   else
      wa_buf = { "use profile-guided optimization flags of your compiler" }
   end
   if (#intel_loop_count > 0) then
      insert (wa_buf, intel_loop_count)
   end
   insert (wa_buf, { header = "hardcode most frequent values of loop bounds by adding specialized paths.",
                     list = { spe_ex } })

   return {title = "Unrolling/vectorization cost",
           txt = "This loop is peel/tail of a unrolled/vectorized loop. If its cost is not negligible compared to the main (unrolled/vectorized) loop, unrolling/vectorization is counterproductive due to low trip count.",
           details = "The more iterations the main loop is processing, the higher the trip count must be to amortize peel/tail overhead.",
           workaround = get_workaround_string (wa_buf, crc.context.tags)
   }
end

local function get_vec_workaround_string (cqa_results)
   local wa_buf = {}

   local packed_ratio_all = cqa_results ["packed ratio"]["all"];
   local crc = cqa_results.common
   local blocks_type = crc ["blocks type"];
   local comp_str = crc.comp_str
   local tags_gt = crc.context.tags.gt

   if (tonumber (packed_ratio_all) ~= nil and packed_ratio_all < 80) then
      local wa_tune_compiler = { header    = "Try another compiler or update/tune your current one",
                                 list_type = "unordered" }

      local wa_unit_stride = { list_type = "unordered",
                               list = {} }

      local lang_code = crc.lang_code
      if (blocks_type == "loop" or block_type == "path") then
         if (is_intel (comp_str)) then
            wa_tune_compiler.list = { "use the vec-report option to understand why your loop was not vectorized. If \"existence of vector dependences\", try the IVDEP directive. If, using IVDEP, \"vectorization possible but seems inefficient\", try the VECTOR ALWAYS directive." }
         elseif (is_gnu (comp_str)) then
            -- Vectorization enabled by ftree-vectorize, included in O3, itself included in Ofast
            local vec = (find (comp_str, "Ofast") ~= nil or
                         find (comp_str, "O3") ~= nil or
                         find (comp_str, "ftree%-vectorize") ~= nil)

            -- FP operations reordering enabled by fassociative-math, included in funsafe-math-optimization,
            -- itself included in ffast-math, itself included in Ofast
            local ass = (find (comp_str, "Ofast") ~= nil or
                         find (comp_str, "ffast%-math") ~= nil or
                         find (comp_str, "funsafe%-math%-optimizations") ~= nil or
                         find (comp_str, "fassociative%-math") ~= nil)

            if (vec and not ass) then
               wa_tune_compiler.list = { "recompile with fassociative-math (included in Ofast or ffast-math) to extend loop vectorization to FP reductions." }
            elseif (not vec and ass) then
               wa_tune_compiler.list = { "recompile with ftree-vectorize (included in O3) to enable loop vectorization (including FP reductions)." }
            elseif (not vec and not ass) then
               wa_tune_compiler.list = { "recompile with ftree-vectorize (included in O3) to enable loop vectorization and with fassociative-math (included in Ofast or ffast-math) to extend vectorization to FP reductions." }
            end
         end

         -- Storage order
         local storage_order_str = "If your arrays have 2 or more dimensions, check whether elements are accessed contiguously and, otherwise, try to permute loops accordingly"
         if (lang_code == Consts.LANG_C) then
            storage_order_str = storage_order_str .. format (":\nC storage order is row-major: for(i) for(j) a[j][i] = b[j][i]; (slow, non stride 1) =%s for(i) for(j) a[i][j] = b[i][j]; (fast, stride 1)", tags_gt)
         elseif (lang_code == Consts.LANG_FORTRAN) then
            storage_order_str = storage_order_str .. format (":\nFortran storage order is column-major: do i do j a(i,j) = b(i,j) (slow, non stride 1) =%s do i do j a(j,i) = b(i,j) (fast, stride 1)", tags_gt)
         end
         wa_unit_stride.header = "Remove inter-iterations dependences from your loop and make it unit-stride"
         insert (wa_unit_stride.list, storage_order_str)

      else
         wa_unit_stride.header = "Make array accesses unit-stride"
      end -- loop or path

      -- AoS => SoA
      local aos_soa_str = "If your "..blocks_type.." streams arrays of structures (AoS), try to use structures of arrays instead (SoA)"
      if (lang_code == Consts.LANG_C) then
         aos_soa_str = aos_soa_str .. format (":\nfor(i) a[i].x = b[i].x; (slow, non stride 1) =%s for(i) a.x[i] = b.x[i]; (fast, stride 1)", tags_gt);
      elseif (lang_code == Consts.LANG_FORTRAN) then
         aos_soa_str = aos_soa_str .. format (":\ndo i a(i)%%x = b(i)%%x (slow, non stride 1) =%s do i a%%x(i) = b%%x(i) (fast, stride 1)", tags_gt);
      end
      insert (wa_unit_stride.list, aos_soa_str)

      insert (wa_buf, wa_tune_compiler)
      insert (wa_buf, wa_unit_stride)

   else
      local arch = crc.context.arch

      if (is_intel (comp_str) and (blocks_type == "loop" or blocks_type == "path")) then
         insert (wa_buf, "Use the LOOP COUNT directive")
      end
   end

   return get_workaround_string (wa_buf, crc.context.tags)
end

local function has_vec_gain (cqa_results)
   local crc = cqa_results.common

   -- Peel/tail loop
   if (crc ["src loop"] ~= nil and
          find (crc ["src loop"]["unroll info"], "^unrolled") ~= nil and
       crc ["is main/unrolled"] == false) then
      return false
   end

   local lvl = crc.context.memory_level[1];
   -- TODO: think to iterate over all memory levels
   if (cqa_results ["cycles L1 if fully vectorized"] == nil or
       cqa_results ["cycles L1 if fully vectorized"].max >= cqa_results.cycles [lvl].max) then
      return false
   end

   return true
end

-- Prints whether a loop is fully, partially or not vectorized
local function get_vec_report (cqa_results)
   local arch = cqa_results.common.context.arch;
   local blocks_type = cqa_results.common ["blocks type"]

   local vect_iset;
   vect_iset = "VPU" 

   local report = {title = "Vectorization"};

   local packed_ratio_all = cqa_results ["packed ratio"]["all"];
   local vec_eff_ratio_all
   if (cqa_results ["vec eff ratio"] ~= nil) then
      vec_eff_ratio_all = tonumber (cqa_results ["vec eff ratio"]["all"]);
   end

   local scal_ver = "process only one data element in vector registers"
   local vect_ver = "process two or more data elements in vector registers"
   local avg_insns = "average across all "..vect_iset.." instructions"

   local fully = ""
   if (packed_ratio_all == 100) then
      local cqa_context = cqa_results.common.context
      local ivo = cqa_context.if_vectorized_options;
      local vec_size = cqa_context.uarch_consts ["vector size in bytes"]
      if (ivo ~= nil and ivo.force_sse) then vec_size = 16 end

      if (vec_eff_ratio_all == 25 and vec_size == 64) then
         report.txt = format ("Your %s is vectorized, but using only 128 out of 512 bits (SSE/AVX-128 instructions on AVX-512 processors).\n", blocks_type);
      elseif (vec_eff_ratio_all == 50 and vec_size == 64) then
         report.txt = format ("Your %s is vectorized, but using only 256 out of 512 bits (AVX/AVX2 instructions on AVX-512 processors).\n", blocks_type);
      elseif (vec_eff_ratio_all == 50 and vec_size == 32) then
         report.txt = format ("Your %s is vectorized, but using only 128 out of 256 bits (SSE/AVX-128 instructions on AVX/AVX2 processors).\n", blocks_type);
      else
         report.txt = format ("Your %s is vectorized, but using %d%% register length (%s).\n", blocks_type, vec_eff_ratio_all, avg_insns);
      end
      fully = "fully "
      report.details = format ("All %s instructions are used in vector version (%s).\n", vect_iset, vect_ver)

   elseif (packed_ratio_all == 0) then
      report.txt = format ("Your %s is not vectorized.\n", blocks_type);
      if (vec_eff_ratio_all == 12.5 or vec_eff_ratio_all == 25 or vec_eff_ratio_all == 50) then
         report.txt = report.txt .. format ("%d data elements could be processed at once in vector registers.\n", 100 / vec_eff_ratio_all);
      elseif (tonumber (vec_eff_ratio_all) ~= nil) then
         report.txt = report.txt .. format ("Only %d%% of vector register length is used (%s).\n", vec_eff_ratio_all, avg_insns);
      end
      report.details = format ("All %s instructions are used in scalar version (%s).\n", vect_iset, scal_ver)

   elseif (packed_ratio_all == "NA") then
      report.txt = format ("Your %s is not vectorized.\n", blocks_type);
      report.details = format ("No %s instruction: only general purpose registers are used and data elements are processed one at a time.\n", vect_iset)

   elseif ((packed_ratio_all > 0) and (packed_ratio_all < 100)) then
      if (rs.false_vectorization (cqa_results) == true) then
         report.txt = format ("Your %s is probably not vectorized.\n", blocks_type);
         report.details = format ("Store and arithmetical %s instructions are used in scalar version (%s).\n", vect_iset, scal_ver)
      else
         if (packed_ratio_all >= 90) then
            report.txt = format ("Your %s is largely vectorized.\n", blocks_type)
         else
            report.txt = format ("Your %s is partially vectorized.\n", blocks_type)
         end

         local tags = cqa_results.common.context.tags
         report.details = format ("%d%% of %s instructions are used in vector version (%s):\n", packed_ratio_all, vect_iset, vect_ver) .. tags.list_start

         -- Prints the breakdown for not fully vectorized instructions
         local full_name = { ["load"   ] = "loads",
                             ["store"  ] = "stores",
                             ["add-sub"] = "addition or subtraction instructions",
                             ["mul"    ] = "multiply instructions",
                             ["other"  ] = "instructions that are not load, store, addition, subtraction nor multiply instructions" }

         local crpr = cqa_results ["packed ratio"]
	 for _,insn_type in ipairs ({"load", "store", "add-sub", "mul", "other"}) do
            local pr = crpr [insn_type]
            if (pr ~= "NA" and pr < 100) then
               report.details = report.details .. format ("%s%d%% of %s %s are used in vector version.%s", tags.list_elem_start, pr, vect_iset, full_name [insn_type], tags.list_elem_stop);
	    end
	 end

         report.details = report.details .. tags.list_stop;
         fully = "fully "
      end -- if loop is really partially vectorized

      if (tonumber (vec_eff_ratio_all) ~= nil) then
         local only = ""; if (vec_eff_ratio_all < 80) then only = "Only " end
         report.txt = report.txt .. format ("%s%d%% of vector register length is used (%s).\n", only, vec_eff_ratio_all, avg_insns);
      end
   end -- switch (packed_ratio_all)

   if (has_vec_gain (cqa_results)) then
      local lvl = cqa_results.common.context.memory_level[1];
      report.txt = report.txt .. format ("By %svectorizing your %s, you can lower the cost of an iteration from %.2f to %.2f cycles (%.2fx speedup).", fully, blocks_type, cqa_results.cycles [lvl].max, cqa_results ["cycles L1 if fully vectorized"].max, cqa_results.cycles [lvl].max / cqa_results ["cycles L1 if fully vectorized"].max);
      report.details = (report.details or "") .. format ("Since your execution units are vector units, only a %svectorized %s can use their full power.\n", fully, blocks_type);
      report.workaround = get_vec_workaround_string (cqa_results)
   end
   
   return report;
end

-- Returns the report about the type of processed elements
-- and the instruction set of instructions processing them
-- Only arithmetic and math instructions are considered
local function get_pattern_report (cqa_results)
   local report = {title = "Type of elements and instruction set"};

   local patterns = cqa_results ["pattern mth"] or {};

   local nb_patterns = 0
   for _,nb in pairs (patterns) do
      if (nb > 0) then nb_patterns = nb_patterns + 1 end
   end

   if (nb_patterns == 0) then
      local blocks_type = cqa_results.common ["blocks type"]
      report.txt = "No instructions are processing arithmetic or math operations on FP elements. This "..blocks_type.." is probably writing/copying data or processing integer elements.";
      return report;
   end

   if (patterns.x87 > 0) then
      report.txt = format ("%d x87 instructions are processing arithmetic or math operations on FP elements in scalar mode (one at a time).\n", patterns.x87);
   else
      report.txt = "";
   end

   local arch = cqa_results.common.context.arch;

   return report;
end

-- Returns report about arithmetic intensity (FP operations / bytes loaded+stored)
-- TODO: use new metric !
local function get_arith_intensity_report (cqa_results)
   local nb_FP_ops = cqa_results ["nb total FP operations"];
   local bytes_loaded = cqa_results ["bytes loaded"];
   local bytes_stored = cqa_results ["bytes stored"];

   if (nb_FP_ops > 0 and (bytes_loaded + bytes_stored) > 0) then
      return {title = "Arithmetic intensity",
              txt   = format ("Arithmetic intensity is %.2f FP operations per loaded or stored byte.", nb_FP_ops / (bytes_loaded + bytes_stored))};
   end

   return nil;
end

-- Returns a string about FP operations (breakdown of FP operations per family: add, mul...)
local function get_FP_ops_string (cqa_results)
   local nb_FP_ops = cqa_results ["nb total FP operations"];
   local blocks_type = cqa_results.common ["blocks type"]

   if (nb_FP_ops == 0) then
      return "The binary "..blocks_type.." does not contain any FP arithmetical operations.\n";
   end

   local tags = cqa_results.common.context.tags
   local buf = { "The binary "..blocks_type.." is composed of "..nb_FP_ops.." FP arithmetical operations:\n",
                 tags.list_start }

   local t = { ["add-sub"] = "addition or subtraction",
               ["mul"    ] = "multiply",
               ["div"    ] = "divide",
               ["rcp"    ] = "fast reciprocal",
               ["sqrt"   ] = "square root",
               ["rsqrt"  ] = "fast square root reciprocal" }
   
   for _,op_type in ipairs ({"add-sub","mul","div","rcp","sqrt","rsqrt"}) do
      local nb = cqa_results ["FP ops"][op_type]
      if (op_type == "add-sub" or op_type == "mul") then
         nb = nb + (cqa_results ["FP ops"]["fma"] or 0)
      end
      if (nb > 0) then
         insert (buf, format ("%s%d: %s%s", tags.list_elem_start, nb, t[op_type], tags.list_elem_stop))
      end
   end

   insert (buf, tags.list_stop)
   return concat (buf)
end

-- Updates the number of loaded and stored bytes report
local function get_nb_loaded_stored_bytes_string (cqa_results)
   local blocks_type = cqa_results.common ["blocks type"]

   if (cqa_results ["bytes loaded"] + cqa_results ["bytes stored"] == 0) then
      return "The binary "..blocks_type.." does not load or store any data.";
   end

   local buf = {}
   for _,v in pairs ({"prefetch", "load", "stor"}) do
      local nb_bytes = cqa_results ["bytes "..v.."ed"];

      if (nb_bytes > 0) then
         local detail = ""
         if (cqa_results.dominant_element_size ~= nil) then
            detail = format (" (%d %s elements)", nb_bytes / cqa_results.dominant_element_size, cqa_results.dominant_element);
         end
         
         insert (buf, format ("The binary "..blocks_type.." is %sing %d bytes%s.", v, nb_bytes, detail))
      end
   end

   return concat (buf, "\n")
end

-- Prints the number of FP operations and loaded/stored bytes allowing the user
-- to do matching between the source and the binary loop
local function get_src_bin_matching (cqa_results)
   local blocks_type = cqa_results.common ["blocks type"]

   return {title = "Matching between your "..blocks_type.." (in the source code) and the binary "..blocks_type.."",
           txt = get_FP_ops_string (cqa_results)..get_nb_loaded_stored_bytes_string (cqa_results)};
end

function get_conversion_insns_report (cqa_results)
   local cvt_insns = cqa_results ["conversion instructions"];
   if ((cvt_insns == nil) or (next (cvt_insns) == nil)) then return nil end

   local tmp;
   local lang_code = cqa_results.common.lang_code;
   if (lang_code == Consts.LANG_C or
       lang_code == Consts.LANG_CPP) then
      tmp = " In C/C++, FP constants are double precision by default and must be suffixed by 'f' to make them single precision.";
   elseif (lang_code == Consts.LANG_FORTRAN) then
      tmp = " In Fortran, constants are single precision by default and must be suffixed by 'D0' to make them double precision.";
   else
      tmp = "";
   end

   return {title      = "Conversion instructions",
           txt        = "Detected expensive conversion instructions (typically from/to single/double precision).",
           details    = cqa.api.reports_shared.get_insn_breakdown_string (cvt_insns, cqa_results.common.context.tags),
           workaround = "Use double instead of single precision only when/where needed by numerical stability and avoid mixing data with different types. In particular, check if the type of constants is the same as array elements."..tmp};
end

-- TODO: improve this: use metrics, factor code with MEM_PROJ, consider "grouping" implementation etc.
function get_streams_stride_report (cqa_results)
   local streams = cqa_results ["streams stride"]
   if (streams == nil) then return nil end

   if (#(streams.unknown_stride) == 0 and
       #(streams.stride_n) == 0 and
       #(streams.indirect) == 0) then
      return nil
   end

   -- Forge details and workaround strings
   local tags = cqa_results.common.context.tags
   local dt_buf, wa_buf = {}, {}
   if (#(streams.unknown_stride) > 0 or #(streams.stride_n) > 0) then
      if (#(streams.unknown_stride) > 0) then
         insert (dt_buf, format ("%sConstant unknown stride: %d occurrence(s)%s",
                                 tags.list_elem_start, #(streams.unknown_stride),
                                 tags.list_elem_stop))
      end
      if (#(streams.stride_n) > 0) then
         insert (dt_buf, format ("%sConstant non-unit stride: %d occurrence(s)%s",
                                 tags.list_elem_start, #(streams.stride_n),
                                 tags.list_elem_stop))
      end
      insert (wa_buf, "Try to reorganize arrays of structures to structures of arrays")
      insert (wa_buf, "Consider to permute loops (see vectorization gain report)")
   end
   if (#(streams.indirect) > 0) then
      insert (dt_buf, format ("%sIrregular (variable stride) or indirect: %d occurrence(s)%s",
                           tags.list_elem_start, #(streams.indirect),
                           tags.list_elem_stop))
      insert (wa_buf, "Try to remove indirect accesses. If applicable, precompute elements out of the innermost loop.")
   end

   return { title   = "Slow data structures access",
            txt     = "Detected data structures (typically arrays) that cannot be efficiently read/written",
            details = tags.list_start .. concat (dt_buf) .. tags.list_stop ..
               "Non-unit stride (uncontiguous) accesses are not efficiently using data caches\n",
            workaround = get_workaround_string (wa_buf, tags) }
end

function get_gather_scatter_insns_report (cqa_results)
   local gs_insns = cqa_results ["gather/scatter instructions"];
   if ((gs_insns == nil) or (next (gs_insns) == nil)) then return nil end

   return {title      = "Gather/scatter instructions",
           txt        = "Detected gather/scatter instructions (typically caused by indirect accesses).",
           details    = cqa.api.reports_shared.get_insn_breakdown_string (gs_insns, cqa_results.common.context.tags),
           workaround = "Try to simplify your code and/or replace indirect accesses with unit-stride ones."};
end

local function get_warnings_string (cqa_results)
   local crw = cqa_results.warnings
   if (next (crw) == nil) then return nil end

   -- Makes output deterministic (allow non regression check)
   local ordered_keys = {}
   for k in pairs (crw) do insert (ordered_keys, k) end
   table.sort (ordered_keys)

   local warnings = {}
   if (Debug:is_enabled()) then
      for i,k in ipairs (ordered_keys) do
         warnings[i] = { header = k, list = crw[k] }
      end
   else
      for _,k in ipairs (ordered_keys) do
         for _,warning in ipairs (crw[k]) do
            insert (warnings, warning)
         end
      end
   end

   local tags = (cqa_results.common or cqa_results).context.tags
   return "Warnings:\n".. get_workaround_string (warnings, tags);
end

local function get_unroll_opportunity_report (cqa_results)
   local src_loop = cqa_results.common ["src loop"];
   if (src_loop == nil) then return nil end

   local src_unroll_info = src_loop ["unroll info"];
   if (src_unroll_info == nil) then return nil end

   -- if source loop already unrolled
   if (find (src_unroll_info, "^unrolled") ~= nil) then return nil end

   local small_body = (cqa_results ["nb instructions"] < 8);
   local memory_bound;
   local dispatch_cycles = cqa_results.dispatch.cycles
   local cqa_context = cqa_results.common.context
   local arch = cqa_context.arch

   if (small_body == false and (memory_bound == nil or memory_bound < 0.5)) then return nil end

   local report = { title = "Unroll opportunity" };

   local wa_buf = { "Unroll your loop if trip count is significantly higher than target unroll factor" }

   if (small_body == true) then
      report.txt = "Loop body is too small to efficiently use resources.";
   else
      if (memory_bound >= 1) then
	 report.txt = "Loop is data access bound.";
      else
	 report.txt = "Loop is potentially data access bound.";
      end
      insert (wa_buf, " and if some data references are common to consecutive iterations");
   end

   insert (wa_buf, ". This can be done manually.");

   local fct = cqa_results.common ["function"];
   if (fct:has_debug_data()) then
      cqa_results.lang_code = fct:get_language();

      local comp_str = fct:get_compiler();
      if (is_gnu (comp_str) or is_intel (comp_str)) then
	 if (is_gnu (comp_str)) then
	    insert (wa_buf, " Or by recompiling with -funroll-loops");
	    if (tonumber (string.match (comp_str, "%d+%.%d+")) >= 5.0) then
	       insert (wa_buf, " and/or -floop-unroll-and-jam");
	    end
	 else -- Intel
	    insert (wa_buf, " Or by combining O2/O3 with the UNROLL (resp. UNROLL_AND_JAM) directive on top of the inner (resp. surrounding) loop. You can enforce an unroll factor: e.g. UNROLL(4)");
	 end
	 insert (wa_buf, ".");
      end
   end

   report.workaround = concat (wa_buf)

   return report;
end

local function get_path_reports (reports, cqa_results)
   reports.gain      = {};
   reports.potential = {};
   reports.hint      = {};
   reports.expert    = {};
   reports.brief     = {};

   local arch = cqa_results.common.context.arch;
   local caogr;
   caogr = cqa.api[arch].reports;

   cqa_results.dominant_element, cqa_results.dominant_element_size =
   get_dominant_element (cqa_results);

   -- Header reports
   insert (reports.header, get_warnings_string   (cqa_results));
   insert (reports.header, get_comp_usage_string (cqa_results));

   -- Used to generate reports only for requested confidence levels
   local confidence_levels = cqa_results.common.context.confidence;
   if (type (confidence_levels) ~= "table") then confidence_levels = {} end

   -- for each requested confidence level
   for _,v in ipairs (confidence_levels) do

   -- Expert reports
      if (v == "expert") then
         insert (reports.expert, get_asm_code_report (cqa_results));
         insert (reports.expert, get_gen_prop_report (cqa_results));
         insert (reports.expert, get_front_end_report (cqa_results));
         insert (reports.expert, get_cycles_summary_report (cqa_results, arch));
         insert (reports.expert, get_expert_vec_ratio_report (cqa_results));
         insert (reports.expert, get_cycles_and_mem_report (cqa_results));

      -- Gain reports
      elseif (v == "gain") then
         insert (reports.gain, get_peel_tail_overhead_report (cqa_results));
         insert (reports.gain, get_vec_report (cqa_results));

      -- Potential reports
      elseif (v == "potential") then
         insert (reports.potential, get_fma_report (cqa_results));

      -- Hint reports
      elseif (v == "hint") then
         insert (reports.hint, caogr.get_vector_unaligned_report (cqa_results));
         insert (reports.hint, get_gather_scatter_insns_report (cqa_results));
         insert (reports.hint, get_conversion_insns_report (cqa_results));
         insert (reports.hint, get_pattern_report (cqa_results));
         insert (reports.hint, get_src_bin_matching (cqa_results));
         insert (reports.hint, get_arith_intensity_report (cqa_results));
         insert (reports.hint, get_unroll_opportunity_report (cqa_results));

      end -- switch on the current confidence level
   end -- ipairs (confidence_levels)
end

-- TODO: move this to metrics
local function set_compiler_and_language (crc)
   -- Prevents this function from being called multiple times on a given cqa_results table
   if (crc.comp_and_lang_done == true) then return end
   crc.comp_and_lang_done = true

   local fct = crc ["function"];

   if (fct:has_debug_data()) then
      crc.lang_code = fct:get_language();

      local comp_str = fct:get_compiler();
      if (is_intel (comp_str) or is_gnu (comp_str)) then
         crc.comp_str = comp_str;

         -- Save compiler options
         -- For the moment, supporting only binaries compiled with Intel compilers using the -sox flag
         if (is_intel (comp_str)) then
            fct:get_asmfile():analyze_compile_options();
            local comp_opts = fct:get_compile_options();
            if (comp_opts ~= nil) then
               crc.comp_opts = string.match (comp_opts, "([^:]*)$");
            end
         end
      end
   end
end

function get_debug_data_string (crc)
   if (not crc.comp_and_lang_done) then set_compiler_and_language (crc) end

   local fct = crc ["function"]

   if (fct:has_debug_data()) then
      if (is_intel (crc.comp_str) and crc.comp_opts == nil) then
         return "Recompile with -sox to prevent CQA from suggesting already used flags";
      end
      if (not is_gnu (crc.comp_str) and not is_intel (crc.comp_str)) then
         return "Unknown compiler: skipping compiler-specific hints.\n";   
      end
      return
   end

   local tags = crc.context.tags
   return "Found no debug data for this function.\n" ..
   "Without them, cannot print source file/lines and customize reports for used compiler/language.\n" ..
   "With GNU or Intel compilers, please recompile with -g.\n" ..
   "With an Intel compiler you must explicitly specify an optimization level " ..
   "and you can use -sox compilation flag to allow CQA to retrieve more information on used flags.\n"..
   "Alternatively, try to:\n" ..tags.list_start..
   tags.list_elem_start.."recompile with -debug noinline-debug-info (if using Intel compiler 13)"..tags.list_elem_stop..
   tags.list_elem_start.."analyze the caller function (possible inlining)"..tags.list_elem_stop..
   tags.list_stop;
end

function get_requested_metrics (report_names)
   local requested_metrics = {
      header = {
         "function", "addr", "blocks type",
         "src file", "src line min-max", "src regions",
         "FP operations per cycle"
      },

      gain = {
         "nb AVX-SSE transitions", -- AVX-SSE transitions report
         "cycles if clean", "arm64] cycles", -- if clean report
         "is main/unrolled", -- peel/tail overhead report
         "packed ratio INT", "packed ratio FP", "packed ratio", "vec eff ratio", -- vectorization report
         "cycles L1 if fully vectorized", -- vectorization gain report
         "bottlenecks", "cycles if hitting next bottleneck" -- execution units bottlenecks report
      },

      potential = {
         "FP operations per cycle", -- FMA report
         "cycles if no deps", "[arm64] cycles", -- data-dependencies report
         "nb masked instructions" -- masked instructions report
      },

      hint = {
         -- expensive instructions reports
         "gather/scatter instructions", "conversion instructions",
         "complex instructions",

         -- source-binary matching and arithmetic intensity reports
         "nb total FP operations", "FP ops",
         "bytes prefetched", "bytes loaded", "bytes stored",

         "nb instructions", "dispatch", "cycles div sqrt", -- unroll opportunity
         "streams stride nb", -- streams stride report
      },

      expert = {
         -- general properties report
         "nb instructions", "nb uops", "loop length",
         "nb stack references",
         "ADD-SUB / MUL ratio",
         "nb pure loads", "nb impl loads", "nb stores",

         -- front-end report
         "assumed macro fusion",
         "fit into loop buffer", "fit into uop cache",
         "[arm64] cycles front end",

         -- back-end report
         "dispatch",
         "cycles div sqrt",
         "RecMII",

         -- cycles summary report
         "cycles dispatch", "[arm64] cycles",
         "cycles memory",

         -- #PRAGMA_NOSTATIC UFS
         -- UFS report
         "cycles UFS", "UFS stall cycles", "UFS resource full",
         -- #PRAGMA_STATIC

         -- expert vectorization reports
         "packed ratio INT", "packed ratio FP", "packed ratio",
         "vec eff ratio INT", "vec eff ratio FP", "vec eff ratio",

         -- cycles and memory report
         "nb masked instructions", "bytes loaded per cycle", "bytes stored per cycle",

         -- Front-end bottlenecks report
         "bottlenecks", "cycles if hitting next bottleneck"
      },

      brief = {
         "cycles L1 if fully vectorized",
         "[arm64] cycles",
         "cycles if clean",
         "cycles if no deps",
         "bottlenecks", "cycles if hitting next bottleneck" -- bottlenecks report
      },
   }

   local at_least_one_valid = false
   for _,report_name in ipairs (report_names) do
      if (requested_metrics [report_name] ~= nil) then
         at_least_one_valid = true
      end
   end
   if (not at_least_one_valid) then return end

   local t = { "function name", "addr", "src file", "src line min-max",
               "id", "can be analyzed", "nb paths", "unroll info", "unroll loop type" }
   for _,report_name in ipairs (report_names) do
      for _,metric_name in ipairs (requested_metrics [report_name]) do
         insert (t, metric_name)
      end
   end

   return t
end

-- Returns the processor effectively used (autodetected or forced) for analysis
local function get_proc_string (proc)
   local proc_name = proc:get_display_name()
   local uarch = proc:get_uarch()
   if (find (proc_name, "based on .* microarchitecture") ~= nil) then
      return format ("Target processor is: %s (%s architecture).",
                     proc_name, uarch:get_arch():get_name())
   else
      return format ("Target processor is: %s (%s/%s micro-architecture).",
                     proc_name, uarch:get_arch():get_name(), uarch:get_display_name())
   end
end

-- Returns a table containing lines to display before any report
function get_reports_header (cqa_context)
   local header = {}

   if (cqa_context.proc == nil and cqa_context.uarch_name ~= nil) then
      local uarch = cqa_context.arch_obj:get_uarch_by_id (cqa_context.uarch)
      insert (header, format ("Target micro-architecture is: %s (run maqao --list-procs to review related processors).", uarch:get_display_name()))
   elseif (cqa_context.proc ~= nil) then
      local proc_str = get_proc_string (cqa_context.proc)
      if (proc_str ~= nil) then insert (header, proc_str) end
   end

   return header
end

--- Returns a table containing some CQA reports for a binary loop, function or path
-- Only requested confidence levels (defined in the context common to all paths) will be considered
-- @param cqa_results table returned by cqa:get_cqa_results()
function get_reports (cqa_results)
   if (cqa_results == nil) then return end

   local reports = {common = {}, paths = {}};

   local crc = cqa_results.common
   local cqa_context = crc.context
   local blocks_type = crc ["blocks type"]

   reports.common.header = {};
   insert (reports.common.header, get_location_string (crc));

   local unroll_string = get_unroll_string (crc)
   if (unroll_string ~= nil) then insert (reports.common.header, unroll_string) end

   if (blocks_type == "path") then
      insert (reports.common.header, "Reports generation assumes your path is from a loop");
   end
   insert (reports.common.header, get_warnings_string (crc));

   if (not crc ["can be analyzed"]) then
      local msg;
      if (crc ["why cannot be analyzed"] == "at least one unsupported instruction") then
	 local fct = crc ["function"];

         local arch_spe_option = "for it";
         local compiler_string = fct:get_compiler();
         if (compiler_string ~= nil) then
            if     (is_intel (compiler_string)) then arch_spe_option = "with -xHost"
            elseif (is_gnu   (compiler_string)) then arch_spe_option = "with -march=native"
            end
         end

         if (cqa_context.proc ~= nil) then

            -- Builds the array of proc names
            local compatible_procs_names = {};
            for proc_rank, proc in ipairs (crc ["compatible procs"]) do
               compatible_procs_names [proc_rank] = proc:get_name();
            end

            msg = format ("At least one instruction cannot execute on the processor model targeted for analysis (%s) but requires either of {%s}. Recompile your application %s or rerun analysis with proc={%s}",
                          cqa_context.proc:get_name(),
                          concat (compatible_procs_names, ", "),
                          arch_spe_option,
                          concat (compatible_procs_names, ", "));            
         else

            -- Builds the array of uarch names
            local compatible_uarchs_names = {};
            for uarch_rank, uarch in ipairs (crc ["compatible uarchs"]) do
               compatible_uarchs_names[uarch_rank] = uarch:get_name();
            end
            -- Retrieves the name of the target uarch
            local arch = get_arch_by_name(cqa_context.arch);
            local target_uarch = arch:get_uarch_by_id(cqa_context.uarch); 

            msg = format ("At least one instruction cannot execute on the microarchitecture targeted for analysis (%s) but requires either of {%s}. Recompile your application %s or rerun analysis with uarch={%s}",
                          target_uarch:get_name(),
                          concat (compatible_uarchs_names, ", "),
                          arch_spe_option,
                          concat (compatible_uarchs_names, ", "));
         end
      else
         msg = crc ["why cannot be analyzed"];
         if (find (msg, "too many paths") ~= nil) then
            msg = msg .. format (". Rerun with -max_paths=%d", crc ["nb paths"])
         elseif (msg == "failed to get the number of paths") then
            msg = msg .. ". Rerun with --ignore-paths/-igp to ignore execution paths "..
               "(i.e consider all instructions in a single path yet with no real existence)."
         end
      end

      insert (reports.common.header, format ("This %s cannot be analyzed: %s.\n", blocks_type, msg));

      if (find (msg, "too many paths") ~= nil and blocks_type == "function") then
	 if (crc.blocks:get_nloops() > 0) then
	    insert (reports.common.header, "This function contains loops. If profiling identifies them as cold, consider removing them by full-unrolling. Otherwise analyze/optimize them.\n");
	 else
	    insert (reports.common.header, "Try to simplify control and/or increase the maximum number of paths per function/loop through the related CQA option.\n");
	 end
      end

      return reports;
   end

   set_compiler_and_language (crc)

   local nb_paths = crc ["nb paths"];
   reports.common.nb_paths = nb_paths;

   if (nb_paths >= 2) then
      local tags = cqa_context.tags
      if (nb_paths == 2) then
         insert (reports.common.header,
                 format ("The structure of this %s is probably %sif then [else] end%s.\n",
                         blocks_type, tags.lt, tags.gt));
      else
         insert (reports.common.header, "This "..blocks_type.." has "..nb_paths.." execution paths.\n");
      end

      insert (reports.common.header,
              "The presence of multiple execution paths is typically the main/first bottleneck.\n"..
              "Try to simplify control inside "..blocks_type..": ideally, try to remove all conditional expressions, for example by (if applicable):\n"..tags.list_start..
              tags.list_elem_start.."hoisting them (moving them outside the "..blocks_type..")"..tags.list_elem_stop..
              tags.list_elem_start.."turning them into conditional moves, MIN or MAX"..tags.list_elem_stop..
              tags.list_stop.."\n");

      local lang_code = crc.lang_code;
      if (lang_code == Consts.LANG_C or
          lang_code == Consts.LANG_CPP) then
         insert (reports.common.header, format ("Ex: if (x%s0) x=0 =%s x = (x%s0 ? 0 : x) (or MAX(0,x) after defining the corresponding macro)\n", tags.lt, tags.gt, tags.lt));
      elseif (lang_code == Consts.LANG_FORTRAN) then
         insert (reports.common.header, format ("Ex: if (x%s0) x=0 =%s x = max(0,x) (Fortran instrinsic procedure)\n", tags.lt, tags.gt));
      end
   end

   -- for each path
   for _,cqa_results_path in pairs (cqa_results.paths) do
      local reports_path = {header = {}};
      get_path_reports (reports_path, cqa_results_path);
      insert (reports.paths, reports_path);
   end

   return reports;
end
