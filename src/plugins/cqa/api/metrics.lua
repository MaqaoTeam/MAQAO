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

-- This file defines CQA built-in metrics
--
-- Metrics are splitted in two sets:
--  * common_metrics (common to all execution paths)
--  * path_metrics (specific to execution paths)
--
-- A metric is a class (in sense of object-oriented programming) with following attributes/methods:
--  * args (optional, table metrics only): table listing keys to display (mostly useful to generate CSV headers).
--    For metrics with 1 hierarchical level format is { { key_1, key_2 etc. } }
--    For metrics with 2 hierarchical (3 or more not supported) format is { { <keys for level 1> }, { <keys for level 2> } }
--
--  * CSV_header (optional): string or function (taking as input a cqa_context and returning a string) defining the name of the related CSV column(s). For table metrics, CSV_header will be serialized and "expanded" via string.format from "args"
--
--  * desc (optional): string documenting briefly the metric. Used to document source code and displayed in JSON listing. Can be expanded/serialized from "args"
--
--  * lua_type: like "desc", used to document source code and displayed in JSON listing
--
--  * arch (optional): table listing supported architectures, for example { "arm64" }. If nil/undefined, related metric supposed compatible with any architecture
--
--  * deps (optional): table listing name of metrics that must be computed before computing the present one. If nil/undefined, related metric supposed having no dependency (for example: number of instructions)
--
--  * compute (optional): function taking a cqa_results.common or a cqa_results.paths[i] table as input. Defines how metric will be computed (and related value stored into the cqa_results.X table) from dependent metrics. Metrics with nil/undefined "compute" are typically already computed by another metric. Such "virtual" metrics are useful to customize a CSV header
--
-- Metrics are computed in cqa:get_cqa_results using compute_common_group*() and compute_path_group() functions
-- Root functions to compute a metric are compute_common_metric() and compute_path_metric(). Computation is on demand: only metrics listed in cqa_context.requested_metrics will be computed

module ("cqa.api.metrics", package.seeall)

require "cqa.api.can_be_analyzed"
require "cqa.api.src_loop_grouping";
require "cqa.api.unroll";
require "cqa.api.stream";

-- #PRAGMA_NOSTATIC MEM_PROJ
require "cqa.api.memory"
-- #PRAGMA_STATIC

require "cqa.api.general_properties.moved_bytes"
require "cqa.api.general_properties.used_registers"
require "cqa.api.general_properties.packed_ratios"
require "cqa.api.general_properties.vec_eff_ratios"

require "cqa.api.arm64.front_end"
require "cqa.api.arm64.back_end"
require "cqa.api.arm64.general_properties.uops"
require "cqa.api.arm64.uarch"

local arm64 = { "arm64" }

local gp = cqa.api.general_properties

local min_max_args = { { "min", "max" } }

common_metrics = {
   ["function" ] = {
      lua_type = "userdata:fct",
      deps = { "blocks type" },
      compute = function (crc)
         local blocks = crc.blocks
         local blocks_type = crc ["blocks type"]
         if     (blocks_type == "path"    ) then crc ["function"] = blocks[1]:get_function()
         elseif (blocks_type == "function") then crc ["function"] = blocks
         elseif (blocks_type == "loop"    ) then crc ["function"] = blocks:get_function() end
      end
   },

   ["function name"] = {
      CSV_header = "Function name",
      desc = "Name of the function in the binary",
      lua_type = "string",
      deps = { "function" },
      compute = function (crc)
         crc ["function name"] = crc ["function"]:get_name()
      end
   },

   ["blocks type"] = {
      CSV_header = "Analysis target type",
      desc = "Type of analyzis target: path, loop or function",
      lua_type = "string",
      compute = function (crc)
         local blocks = crc.blocks
         if (type (blocks) == "table") then
            -- a CQA path is a table of lua userdata, each one being a block (MAQAO abstract object)
            if (getmetatable (blocks[1])["_NAME"] == "block") then crc ["blocks type"] = "path" end

         elseif (type (blocks) == "userdata") then
            local meta_name = getmetatable (blocks)["_NAME"];

            if     (meta_name == "fct" ) then crc ["blocks type"] = "function"
            elseif (meta_name == "loop") then crc ["blocks type"] = "loop" end
         end
      end
   },

   ["first insn"] = {
      lua_type = "userdata:insn",
      deps = { "blocks type" },
      compute = function (crc)
         local entry_block
         if (crc ["blocks type"] == "loop") then
            entry_block = crc.blocks:get_first_entry()
         elseif (crc ["blocks type"] == "function") then
            entry_block = crc.blocks
         else
            entry_block = crc.blocks[1]
         end
         crc ["first insn"] = entry_block:get_first_insn()
      end
   },

   ["src file"] = {
      CSV_header = "Source file",
      desc = "Path to the source file defining the analysis target",
      lua_type = "string",
      deps = { "first insn", "blocks type" },
      compute = function (crc)
         if (crc ["blocks type"] == "loop") then
            crc ["src file"] = crc.blocks:get_src_file_path()
         else
            crc ["src file"] = crc ["first insn"]:get_src_file_path()
         end
      end
   },

   ["src line min-max"] = {
      CSV_header = "Source lines",
      desc = "First and last line in the source file",
      lua_type = "string",
      deps = { "blocks type" },
      compute = function (crc)
         local src_min, src_max;
         local blocks_type = crc ["blocks type"];
         if (blocks_type == "loop" or blocks_type == "function") then
            src_min, src_max = crc.blocks:get_src_lines();
         else -- TODO
            src_min, src_max = -1, -1;
         end
         crc ["src line min-max"] = src_min.."-"..src_max;
      end
   },

   ["src regions"] = {
      desc = "source regions: returned by block/loop:get_src_regions()",
      lua_type = "table of string",
      deps = { "blocks type" },
      compute = function (crc)
         local blocks_type = crc ["blocks type"];
         if (blocks_type == "loop" or blocks_type == "function") then
            crc ["src regions"] = crc.blocks:get_src_regions();
         end
      end
   },

   ["addr"] = {
      CSV_header = "Address in the binary",
      desc = "Address of the first instruction of the loop in the binary",
      lua_type = "string",
      deps = { "first insn" },
      compute = function (crc)
         crc ["addr"] = string.format ("%x", crc ["first insn"]:get_address());
      end
   },

   ["id"] = {
      CSV_header = "ID",
      desc = "MAQAO identifier",
      lua_type = "number",
      deps = { "blocks type" },
      compute = function (crc)
         local blocks_type = crc ["blocks type"]
         if (blocks_type == "loop" or blocks_type == "function") then
            crc ["id"] = crc.blocks:get_id();
         end
      end
   },

   ["can be analyzed"] = {
      CSV_header = "Can be analyzed",
      desc = "true if can be analyzed by the MAQAO loop static analyzer",
      lua_type = "boolean",
      deps = { "blocks type" },
      compute = function (crc)
         local can_be_analyzed, why_cannot_be_analyzed, compat_uarchs, compat_procs =
            cqa.api.can_be_analyzed.can_be_analyzed (crc);

         crc ["can be analyzed"] = can_be_analyzed;
         if (not can_be_analyzed) then
            crc ["why cannot be analyzed"] = why_cannot_be_analyzed;
            crc ["compatible uarchs"] = compat_uarchs;
            crc ["compatible procs"] = compat_procs;
         end
      end
   },

   ["why cannot be analyzed"] = {
      CSV_header = "Why cannot be analyzed",
      desc = "string explaining why an object cannot be analyzed",
      lua_type = "string",
      deps = { "can be analyzed" },
      compute = nil -- computed by can be analyzed
   },

   ["compatible uarchs"] = {
      desc = "Micro-architectures supporting all related instructions",
      lua_type = "table of userdata:uarch",
      deps = { "can be analyzed" },
      compute = nil -- computed by can be analyzed
   },

   ["compatible procs"] = {
      desc = "Processor models supporting all related instructions",
      lua_type = "table of userdata:proc",
      deps = { "can be analyzed" },
      compute = nil -- computed by can be analyzed
   },

   ["RecMII"] = {
      args = min_max_args,
      CSV_header = "RecMII (cycles): %s",
      desc = "RecMII (Recurrence Minimum Initiation Interval, cycles spent in the longest dependency chain)",
      lua_type = "number",
      arch = { "arm64" }, 
      deps = { "blocks type" },
      compute = function (crc)
         if (crc ["blocks type"] == "loop") then
            local DDG = crc.blocks:get_DDG()
            local min, max = DDG:DDG_get_RecMII()
            DDG:DDG_free()
            crc ["RecMII"] = { min = min, max = max }
         end
      end
   },

   ["nb paths"] = {
      CSV_header = "Nb paths",
      desc = "Number of execution paths",
      lua_type = "number",
      compute = nil -- managed by get_cqa_results
   },

   ["src loop"] = {
      desc = "Source loop",
      lua_type = "table",
      deps = { "function", "blocks type", "id" },
      compute = function (crc)
         if (crc ["blocks type"] ~= "loop") then return end

         if (crc.context._src_line_groups == nil) then
            crc.context._src_line_groups = {}
         end

         local src_line_groups = crc.context._src_line_groups
         local fct_id = crc ["function"]:get_id()

         -- Not yet computed / in cache
         if (src_line_groups [fct_id] == nil) then
            -- Get source loops in related function
            local src_loops, bin_loops = cqa:get_fct_src_loops (crc ["function"])

            -- Lookup for source loop containing present loop
            local bin2src = {}
            for _, src_loop in pairs (src_loops) do
               for _,lid in ipairs (src_loop ["loop IDs"]) do
                  bin2src [lid] = src_loop
               end
            end

            src_line_groups [fct_id] = {
               src = src_loops,
               bin = bin_loops,
               bin2src = bin2src }
         end

         crc ["src loop"] = src_line_groups [fct_id].bin2src [crc.id]
      end
   },

   ["unroll info"] = {
      -- SOURCE LEVEL
      CSV_header = "Unroll info (src loop)",
      desc = "Source loop unroll information",
      lua_type = "string",
      deps = { "src loop" },
      compute = function (crc)
         local src_loop = crc ["src loop"]
         if (src_loop == nil) then return end

         -- Not yet computed / in cache
         if (src_loop ["unroll info"] == nil) then
            -- Get cqa results for binary loops in source loop, required for unroll info
            local cqa_context = {
               arch  = crc.context.arch,
               uarch = crc.context.uarch,
               uao = crc.context.uao,
               proc = crc.context.proc,
               arch_obj = crc.context.arch_obj,
               max_paths = crc.context.max_paths,
               requested_metrics = {
                  "can be analyzed", "FP ops",
                  "bytes loaded", "bytes stored",
                  "bytes stack ref" }
            }
            if (crc.context.arch == "k1om") then
               for _,v in ipairs ({"first insn", "packed ratio", "[k1om] pattern"}) do
                  table.insert (cqa_context.requested_metrics, v)
               end
            end

            local cqa_results = {}
            for _,l in ipairs (src_loop ["loops"]) do
               cqa_results [l:get_id()] = cqa:get_cqa_results (l, cqa_context)
            end

            -- Computes unroll info from cqa_results
            cqa.api.unroll.compute_src_unroll_factor (src_loop, cqa_results)
            cqa.api.unroll.add_unroll_factor_in_src_loop (src_loop, cqa_results, crc.context.arch)

            -- Gathers in source loop binary-level unroll info
            src_loop ["unroll data"] = {}
            for _,metric in ipairs ({ "is main/unrolled", "unroll factor",
                                      "unroll loop type"}) do
               local t = {}
               for _,lid in ipairs (src_loop ["loop IDs"]) do
                  t[lid] = cqa_results [lid].common [metric]
               end
               src_loop ["unroll data"] [metric] = t
            end
         end

         crc ["unroll info"] = src_loop ["unroll info"]
         crc ["unroll confidence level"] = src_loop ["unroll confidence level"]
      end
   },

   ["unroll confidence level"] = {
      -- SOURCE LEVEL
      CSV_header = "Unroll confidence level (src loop)",
      desc = "Confidence level about unroll information",
      lua_type = "string",
      deps = { "unroll info" },
      compute = nil -- computed with unroll info
   },

   ["is main/unrolled"] = {
      CSV_header = "Is main/unrolled",
      desc = "false if another loop processing more elements per iteration exists",
      lua_type = "boolean",
      deps = { "src loop", "unroll info", "id" },
      compute = function (crc)
         if (crc ["src loop"] ~= nil) then
            crc ["is main/unrolled"] =
               crc ["src loop"]["unroll data"]["is main/unrolled"][crc.id]
         end
      end
   },

   ["unroll factor"] = {
      CSV_header = "Unroll factor",
      desc = "Unroll factor compared to the smallest binary loop",
      lua_type = "number",
      deps = { "src loop", "unroll info", "id" },
      compute = function (crc)
         if (crc ["src loop"] ~= nil) then
            crc ["unroll factor"] =
               crc ["src loop"]["unroll data"]["unroll factor"][crc.id]
         end
      end
   },

   ["unroll loop type"] = {
      CSV_header = "Unroll loop type",
      desc = "Unroll type of the loop (\"vectorized peel/tail\", \"peel/tail\", \"main\")",
      lua_type = "string",
      deps = { "src loop", "unroll info", "id" },
      compute = function (crc)
         if (crc ["src loop"] ~= nil) then
            crc ["unroll loop type"] =
               crc ["src loop"]["unroll data"]["unroll loop type"][crc.id]
         end
      end
   },

}

local function get_mem_lvl_args (cqa_context)
   return { cqa_context.memory_level, { "min", "max" } }
end

path_metrics = {

   ["path ID"] = {
      CSV_header = "Path ID",
      desc = "MAQAO path identifier",
      lua_type = "number",
      compute = nil -- managed by get_cqa_results
   },

   ["extra_unroll_factor"] = {
      CSV_header = "Extra unroll factor",
      desc = "When using a vectorizer instructions modifier, unroll factor compared to the unmodified code",
      lua_type = "number",
   },

   ["nb instructions"] = {
      CSV_header = "Nb instr.",
      desc = "Number of instructions",
      lua_type = "number",
      compute = function (crp)
         crp ["nb instructions"] = #crp.insns
      end
   },

   ["loop length"] = {
      CSV_header = "Size (bytes)",
      desc = "Binary code size in bytes",
      lua_type = "number",
      compute = function (crp)
         local l = 0
         local warn_str

         for _,insn in ipairs (crp.insns) do
            local size = insn:get_bitsize () -- size in bits

            if (size ~= nil and size > 0) then
               l = l + size
            else
               warn_str = (warn_str or "") ..
               "Unknown or invalid size for the instruction ["..insn:get_asm_code().."]\n"
            end
         end

         crp ["loop length"] = l / 8
         crp.warnings ["get_length"] = warn_str
      end
   },

   ["gather/scatter instructions"] = {
      desc = "Gather/scatter instructions",
      lua_type = "table (string,number)",
      compute = function (crp)
         local gs_insns = {};
         local find = string.find;

         for _,insn in ipairs (crp.insns) do
            local name = insn:get_name();

            if (find (name, "GATHER") ~= nil or find (name, "SCATTER") ~= nil) then
               if (gs_insns [name] == nil) then
                  gs_insns [name] = 1;
               else
                  gs_insns [name] = gs_insns [name] + 1;
               end
            end
         end

         crp ["gather/scatter instructions"] = gs_insns;
      end
   },

   ["conversion instructions"] = {
      desc = "Conversion instructions",
      lua_type = "table (string,number)",
      compute = function (crp)
         local cvt_insns = {};
         local find = string.find

         for _,insn in ipairs (crp.insns) do
            local name = insn:get_name();

            if (find (name, "^V?CVT") ~= nil) then
               if (cvt_insns [name] == nil) then
                  cvt_insns [name] = 1;
               else
                  cvt_insns [name] = cvt_insns [name] + 1;
               end
            end
         end

         crp ["conversion instructions"] = cvt_insns;
      end
   },

   ["nb masked instructions"] = {
      CSV_header = "Nb masked insns",
      desc = "Number of masked instructions (excluding GATHER and SCATTER instructions)",
      lua_type = "number",
      compute = function (crp)
         local nb = 0;
         local find = string.find;

         for _,insn in ipairs (crp.insns) do
            local asm = insn:get_asm_code();

            if (find (asm, "GATHER" ) == nil and
                find (asm, "SCATTER") == nil and
                find (asm, "{%%K%d}") ~= nil) then
               nb = nb + 1;
            end
         end

         crp ["nb masked instructions"] = nb;
      end
   },
   
   ["FP ops"] = {
      args = { {"add-sub", "mul", "fma", "div", "sqrt", "rcp", "rsqrt"} },
      CSV_header = "Nb FLOP: %s",
      desc = "Table of floating-point operations stats like { ADD = 2, MUL = 8 }. By definition, packed SIMD instructions process more than 1 operation",
      lua_type = "number",
      compute = function (crp)
         local fam2key = { [Consts.FM_ADD ] = "add-sub", [Consts.FM_SUB  ] = "add-sub",
                           [Consts.FM_MUL ] = "mul"    , [Consts.FM_FMA  ] = "fma",
                           [Consts.FM_DIV ] = "div"    , [Consts.FM_RCP  ] = "rcp",
                           [Consts.FM_SQRT] = "sqrt"   , [Consts.FM_RSQRT] = "rsqrt",
                           [Consts.FM_FMS ] = "fma" }

         local FP_ops            = {};
         local normalized_FP_ops = {};
         for _,v in ipairs ({"add-sub", "mul", "fma", "div", "rcp", "sqrt", "rsqrt"}) do
            FP_ops [v]              = 0;
            normalized_FP_ops [v]   = 0;
         end

         for _,insn in ipairs (crp.insns) do
            if (insn:is_FP()) then
               local key = fam2key [insn:get_family()]
               if (key ~= nil) then
                  local nb_ops            = insn:get_SIMD_width ();
                  local normalized_nb_ops = nb_ops / (64 / insn:get_input_element_bits());
                  FP_ops [key]            = FP_ops [key] + nb_ops;
                  normalized_FP_ops [key] = normalized_FP_ops [key] + normalized_nb_ops;
               end
            end
         end

         crp ["FP ops"]             = FP_ops;
         crp ["normalized FP ops"]  = normalized_FP_ops;
      end
   },

   ["nb total FP operations"] = {
      CSV_header = "Nb FLOP",
      desc = "Number of FP operations (FMA counts as 2)",
      lua_type = "number",
      deps = { "FP ops" },
      compute = function (crp)
         local sum = 0;
         local normalized_sum = 0;
         for _,v in pairs (crp ["FP ops"]) do
            sum = sum + v
         end
         for _,v in pairs (crp ["normalized FP ops"]) do
            normalized_sum = normalized_sum + v
         end

         crp ["nb total FP operations"]            = sum + crp ["FP ops"]["fma"];
         crp ["normalized nb total FP operations"] = normalized_sum + crp ["normalized FP ops"]["fma"];
      end
   },

   ["packed ratio INT"] = {
      args = {{"all", "load", "store", "add-sub", "mul", "other"}},
      CSV_header = "Vec. ratio (%%): %s (INT)",
      desc = "Vectorization ratio of instructions processing integer elements",
      lua_type = "number or NA",
      compute = function (crp)
         crp ["packed ratio INT"] = gp.packed_ratios.get_packed_ratios ("INT", crp)
      end
   },

   ["packed ratio FP"] = {
      args = {{"all", "load", "store", "add-sub", "mul", "other"}},
      CSV_header = "Vec. ratio (%%): %s (FP)",
      desc = "Vectorization ratio of %s instructions processing FP elements",
      lua_type = "number or NA",
      compute = function (crp)
         crp ["packed ratio FP"] = gp.packed_ratios.get_packed_ratios ("FP", crp)
      end
   },

   ["packed ratio"] = {
      args = {{"all", "load", "store", "add-sub", "mul", "other"}},
      CSV_header = "Vec. ratio (%%): %s",
      desc = "Vectorization ratio (proportion of vectorizable instructions that was vectorized) of %s instructions processing FP or integer elements",
      lua_type = "number or NA",
      compute = function (crp)
         crp ["packed ratio"] = gp.packed_ratios.get_packed_ratios ("INT+FP", crp)
      end
   },
   
   ["vec eff ratio INT"] = {
      args = {{"all", "load", "store", "add-sub", "mul", "other"}},
      CSV_header = "Vec. eff. ratio (%%): %s (INT)",
      desc = "Vector efficiency ratio (average proportion of used vector length) of %s instructions processing integer elements",
      lua_type = "number or NA",
      arch = { "arm64" },
      compute = function (crp)
         crp ["vec eff ratio INT"] = gp.vec_eff_ratios.get_vec_eff_ratios ("INT", crp)
      end
   },

   ["vec eff ratio FP"] = {
      args = {{"all", "load", "store", "add-sub", "mul", "other"}},
      CSV_header = "Vec. eff. ratio (%%): %s (FP)",
      desc = "Vector efficiency ratio (average proportion of used vector length) of %s instructions processing FP elements",
      lua_type = "number or NA",
      arch = { "arm64" },
      compute = function (crp)
         crp ["vec eff ratio FP"] = gp.vec_eff_ratios.get_vec_eff_ratios ("FP", crp)
      end
   },

   ["vec eff ratio"] = {
      args = {{"all", "load", "store", "add-sub", "mul", "other"}},
      CSV_header = "Vec. eff. ratio (%%): %s",
      desc = "Vector efficiency ratio (average proportion of used vector length) of %s instructions processing FP or integer elements",
      lua_type = "number or NA",
      arch = { "arm64" },
      compute = function (crp)
         crp ["vec eff ratio"] = gp.vec_eff_ratios.get_vec_eff_ratios ("INT+FP", crp)
      end
   },

   ["ADD-SUB / MUL ratio"] = {
      CSV_header = "Ratio ADD-SUB / MUL",
      desc = "Ratio between FP ADD-SUB and MUL instructions number",
      lua_type = "number",
      compute = function (crp)
         local nb_add_sub = 0
         local nb_mul = 0

         for _,insn in ipairs (crp.insns) do
            if (insn:is_FP()) then
               if (insn:is_add_sub()) then
                  nb_add_sub = nb_add_sub + 1;
               elseif (insn:is_mul()) then
                  nb_mul = nb_mul + 1;
               end
            end
         end

         if (nb_add_sub > 0 and nb_mul > 0) then crp ["ADD-SUB / MUL ratio"] = nb_add_sub / nb_mul end
      end
   },

   ["nb stores"] = {
      CSV_header = "Nb stores",
      desc = "Number of stores",
      lua_type = "number",
      compute = function (crp)
         local n = 0
         for _,insn in ipairs (crp.insns) do
            if (insn:is_store()) then n = n + 1 end
         end

         crp ["nb stores"] = n
      end
   },
   
   ["bytes prefetched"] = {
      CSV_header = "Bytes prefetched",
      desc = "Number of bytes prefetched",
      lua_type = "number",
      compute = function (crp)
         local sum = 0
         for _,insn in ipairs (crp.insns) do
            if (insn:is_prefetch()) then sum = sum + 64 end -- 64B cachelines
         end
         crp ["bytes prefetched"] = sum
      end
   },

   ["bytes loaded"] = {
      CSV_header = "Bytes loaded",
      desc = "Number of bytes loaded",
      lua_type = "number",
      compute = function (crp)
         local sum = 0
         for _,insn in ipairs (crp.insns) do
            if (insn:is_load()) then
               local mem_oprnd = insn:get_first_mem_oprnd ();
               sum = sum + mem_oprnd ["size"] / 8
            end
         end
         crp ["bytes loaded"] = sum
      end
   },

   ["bytes stored"] = {
      CSV_header = "Bytes stored",
      desc = "Number of bytes stored",
      lua_type = "number",
      compute = function (crp)
         local sum = 0
         for _,insn in ipairs (crp.insns) do
            if (insn:is_store()) then
               local mem_oprnd = insn:get_first_mem_oprnd ();
               sum = sum + mem_oprnd ["size"] / 8
            end
         end
         crp ["bytes stored"] = sum
      end
   },

   ["bytes loaded or stored"] = {
      CSV_header = "Bytes loaded or stored",
      desc = "Number of bytes loaded or stored",
      lua_type = "number",
      deps = { "bytes loaded", "bytes stored" },
      compute = function (crp)
         crp ["bytes loaded or stored"] = crp ["bytes loaded"] + crp ["bytes stored"]
      end
   },

   ["arithmetic intensity"] = {
      CSV_header = "Arith. intensity (FLOP / ld+st bytes)",
      desc = "Arithmetic intensity, that is ratio between FP operations and loaded/stored bytes",
      lua_type = "number",
      deps = { "nb total FP operations", "bytes loaded or stored" },
      compute = function (crp)
         if (crp ["bytes loaded or stored"] > 0) then
            crp ["arithmetic intensity"] = crp ["nb total FP operations"] / crp ["bytes loaded or stored"]
         else
            crp ["arithmetic intensity extra info"] = "no loaded or stored bytes"
         end
      end
   },

   ["arithmetic intensity extra info"] = {
      CSV_header = "Arith. intensity extra info",
      desc = "Arithmetic intensity extra information (if nil)",
      lua_type = "string",
      deps = { "arithmetic intensity" },
      compute = nil -- computed by "arithmetic intensity"
   },

   ["[arm64] uops"] = {
      CSV_header = "Micro-operations",
      desc = "Micro-operations generated by the instructions",
      lua_type = "number",
      arch = arm64,
      compute = cqa.api.arm64.general_properties.uops.get_characteristics
   },

   ["[arm64] cycles front end"] = {
      CSV_header = "Front-end (cycles)",
      desc = "Number of cycles to stream instructions through the whole front-end",
      lua_type = "number",
      arch = arm64,
      deps = { "nb instructions", "[arm64] uops"},
      compute = cqa.api.arm64.front_end.do_front_end_analysis
   },

   ["[arm64] cycles back end"] = {
      CSV_header = "Back-end (cycles)",
      desc = "Number of cycles to stream instructions through the back-end",
      lua_type = "number",
      arch = arm64,
      deps = { "nb instructions", "[arm64] uops"},
      compute = cqa.api.arm64.back_end.do_back_end_analysis
   },


   ["cycles div sqrt"] = {
      args = min_max_args,
      CSV_header = "Nb cycles DIV/SQRT: %s",
      desc = "Cycles spent in DIV/SQRT unit",
      lua_type = "number",
      arch = { "arm64" },
      compute = function (crp)
         local uarch = crp.common.context.uarch
         local cached_dispatch = crp.common._cache.dispatch
         local min = 0
         local max = 0

         for _,insn in ipairs (crp.insns) do
            if (insn:is_div() or insn:is_sqrt()) then
               local rmin, rmax;
               if (cached_dispatch ~= nil) then
                  rmin, rmax = insn:get_reciprocal_throughput (cached_dispatch [insn:get_asm_code()], uarch);
               else
                  rmin, rmax = insn:get_reciprocal_throughput (nil, uarch);
               end

               if (rmin ~= nil) then
                  min = min + rmin;
                  max = max + rmax;
               end
            end
         end

         crp ["cycles div sqrt"] = { min = min, max = max }
      end
   },

   ["[arm64] cycles"] = {
      args = get_mem_lvl_args,
      CSV_header = "[%s] Nb cycles: %s",
      desc = "Loop throughput (number of cycles per iteration) if all data are in %s",
      lua_type = "number",
      arch = arm64,
      deps = { "[arm64] cycles front end", "[arm64] cycles back end" },
      compute = function (crp)
         local cycles_front_end = crp ["[arm64] cycles front end"]
         local cycles_back_end = crp ["[arm64] cycles back end"]
         local cycles_recmii = crp.common ["RecMII"] or { min = 0, max = 0}

         crp.cycles = {}
         local cqa_context = crp.common.context
         for _,lvl in ipairs (cqa_context.memory_level) do
            crp.cycles [lvl] = { 
               min = math.max (cycles_front_end, cycles_back_end.min, cycles_recmii.min),
               max = math.max (cycles_front_end, cycles_back_end.max, cycles_recmii.max), 
            }
            print(lvl.." min: "..crp.cycles [lvl].min)
            print(lvl.." max: "..crp.cycles [lvl].max)
         end
      end
   },

   ["bytes prefetched per cycle"] = {
      args = get_mem_lvl_args,
      CSV_header = "[%s] Bytes prefetched / cycle: %s",
      desc = "Bytes prefetched per cycle if all data are in %s",
      lua_type = "number",
      deps = { "bytes prefetched", "[arm64] cycles" },
      compute = function (crp)
         crp ["bytes prefetched per cycle"] = {}

         local cqa_context = crp.common.context
         for _,lvl in ipairs (cqa_context.memory_level) do
            local cycles = crp.cycles [lvl]
            crp ["bytes prefetched per cycle"][lvl] = {
               min = crp ["bytes prefetched"] / cycles.max,
               max = crp ["bytes prefetched"] / cycles.min,
            }
         end
      end
   },

   ["bytes loaded per cycle"] = {
      args = get_mem_lvl_args,
      CSV_header = "[%s] Bytes loaded / cycle: %s",
      desc = "Bytes loaded per cycle if all data are in %s",
      lua_type = "number",
      deps = { "bytes loaded", "[arm64] cycles" },
      compute = function (crp)
         crp ["bytes loaded per cycle"] = {}

         local cqa_context = crp.common.context
         for _,lvl in ipairs (cqa_context.memory_level) do
            local cycles = crp.cycles [lvl]
            crp ["bytes loaded per cycle"][lvl] = {
               min = crp ["bytes loaded"] / cycles.max,
               max = crp ["bytes loaded"] / cycles.min,
            }
         end
      end
   },

   ["bytes stored per cycle"] = {
      args = get_mem_lvl_args,
      CSV_header = "[%s] Bytes stored / cycle: %s",
      desc = "Bytes stored per cycle if all data are in %s",
      lua_type = "number",
      deps = { "bytes stored", "[arm64] cycles" },
      compute = function (crp)
         crp ["bytes stored per cycle"] = {}

         local cqa_context = crp.common.context
         for _,lvl in ipairs (cqa_context.memory_level) do
            local cycles = crp.cycles [lvl]
            crp ["bytes stored per cycle"][lvl] = {
               min = crp ["bytes stored"] / cycles.max,
               max = crp ["bytes stored"] / cycles.min,
            }
         end
      end
   },

   ["bytes loaded or stored per cycle"] = {
      args = get_mem_lvl_args,
      CSV_header = "[%s] Bytes ld+st / cycle: %s",
      desc = "Bytes loaded or stored (sum of loaded and stored bytes) per cycle if all data are in %s",
      lua_type = "number",
      deps = { "bytes loaded or stored", "[arm64] cycles" },
      compute = function (crp)
         crp ["bytes loaded or stored per cycle"] = {}

         local cqa_context = crp.common.context
         for _,lvl in ipairs (cqa_context.memory_level) do
            local cycles = crp.cycles [lvl]
            crp ["bytes loaded or stored per cycle"][lvl] = {
               min = crp ["bytes loaded or stored"] / cycles.max,
               max = crp ["bytes loaded or stored"] / cycles.min,
            }
         end
      end
   },

   ["instructions per cycle"] = {
      args = get_mem_lvl_args,
      CSV_header = "[%s] IPC: %s",
      desc = "Instructions per cycle if all data are in %s",
      lua_type = "number",
      deps = { "nb instructions", "[arm64] cycles" },
      compute = function (crp)
         crp ["instructions per cycle"] = {}

         local cqa_context = crp.common.context
         for _,lvl in ipairs (cqa_context.memory_level) do
            local cycles = crp.cycles [lvl]
            crp ["instructions per cycle"][lvl] = {
               min = crp ["nb instructions"] / cycles.max,
               max = crp ["nb instructions"] / cycles.min,
            }
         end
      end
   },

   ["FP operations per cycle"] = {
      args = get_mem_lvl_args,
      CSV_header = "[%s] FLOP/cycle: %s",
      desc = "FLOPs per cycle if all data are in %s",
      lua_type = "number",
      deps = { "nb total FP operations", "[arm64] cycles" },
      compute = function (crp)
         crp ["FP operations per cycle"] = {}
         crp ["normalized FP operations per cycle"] = {}

         local cqa_context = crp.common.context
         for _,lvl in ipairs (cqa_context.memory_level) do
            local cycles = crp.cycles [lvl]
            crp ["FP operations per cycle"][lvl] = {
               min = crp ["nb total FP operations"] / cycles.max,
               max = crp ["nb total FP operations"] / cycles.min,
            }
            crp ["normalized FP operations per cycle"][lvl] = {
               min = crp ["normalized nb total FP operations"] / cycles.max,
               max = crp ["normalized nb total FP operations"] / cycles.min,
            }
         end
      end
   },

   ["cycles if optimized"] = {
      args = get_mem_lvl_args,
      CSV_header = "[%s] Nb cycles if optimized: %s",
      desc = "Cycles if all detected issues are fixed (cleaned, vectorized, inter-iteration deps removed and first bottleneck removed)",
      lua_type = "number",
      arch = arm64,
      deps = { "cycles if clean", "cycles L1 if fully vectorized",
               "cycles if no deps", "cycles if hitting next bottleneck",
               "[arm64] cycles" },
      compute = function (crp)
         crp ["cycles if optimized"] = {}

         local cqa_context = crp.common.context
         for _,lvl in ipairs (cqa_context.memory_level) do
            crp ["cycles if optimized"][lvl] = {
               min = crad.get_cycles_if_optimized (crp, "min"),
               max = crad.get_cycles_if_optimized (crp, "max"),
            }
         end
      end
   },

}

-- Subset of common_metrics to consider before checking target can be analyzed
common_metrics_before_check = { "function name", "src file", "src line min-max", "addr", "id", "src loop" };

local function check_common_metric (metric_key)
   local metric = common_metrics [metric_key]
   if (metric == nil or type (metric) ~= "table") then
      Message:critical ("Unknown or invalid common metric: "..debug.traceback())
   end
end

local function check_path_metric (metric_key)
   local metric = path_metrics [metric_key]
   if (metric == nil or type (metric) ~= "table") then
      Message:critical ("Unknown or invalid path metric: "..debug.traceback())
   end
end

local function check_cqa_results_common (obj)
   if (type (obj) ~= "table" or obj.context == nil) then
      Message:critical ("Expecting a cqa_results_common table"..debug.traceback())
   end
end

local function check_cqa_results_path (obj)
   if (type (obj) ~= "table" or obj.insns == nil) then
      Message:critical ("Expecting a cqa_results_path table"..debug.traceback())
   end
end

function init_common_metrics (crc)
   if (Debug:is_enabled()) then check_cqa_results_common (crc) end

   crc._metrics = {
      is_visited = {},
      is_required = {}
   }

   local is_required = crc._metrics.is_required
   for _,v in ipairs (crc.context.requested_metrics) do
      if (common_metrics [v] ~= nil) then
         is_required [v] = true
      end
   end
end

function init_path_metrics (crp)
   if (Debug:is_enabled()) then check_cqa_results_path (crp) end
   
   crp._metrics = {
      is_visited = {},
      is_required = {}
   }

   local is_required = crp._metrics.is_required
   for _,v in ipairs (crp.common.context.requested_metrics) do
      if (path_metrics [v] ~= nil) then
         is_required [v] = true
      end
   end   
end

local function is_arch_compatible (arch, arch_set)
   if (arch_set == nil) then return true end

   for _,v in ipairs (arch_set) do
      if (arch == v) then return true end
   end

   return false
end

function compute_common_metric (metric_key, crc)
   if (Debug:is_enabled()) then
      check_common_metric (metric_key)
      check_cqa_results_common (crc)
   end

   local metric = common_metrics [metric_key]

   -- If not compatible with target architecture, do nothing and do not retry
   if (not is_arch_compatible (crc.context.arch, metric.arch)) then
      crc._metrics.is_visited [metric_key] = true
      return
   end

   -- If already computed
   if (crc._metrics.is_visited [metric_key]) then return end

   -- Compute all dependent metrics
   for _,dep in ipairs (metric.deps or {}) do
      compute_common_metric (dep, crc)
   end

   if (metric.compute ~= nil) then metric.compute (crc) end
   crc._metrics.is_visited [metric_key] = true
end

function compute_path_metric (metric_key, crp)
   if (Debug:is_enabled()) then
      check_path_metric (metric_key)
      check_cqa_results_path (crp)
   end

   local metric = path_metrics [metric_key]

   -- If not compatible with target architecture, do nothing and do not retry
   if (not is_arch_compatible (crp.common.context.arch, metric.arch)) then
      crp._metrics.is_visited [metric_key] = true
      return
   end

   -- If already computed
   if (crp._metrics.is_visited [metric_key]) then
      return
   end

   for _,dep in ipairs (metric.deps or {}) do
      if (common_metrics [dep] ~= nil) then compute_common_metric (dep, crp.common)
      elseif (path_metrics [dep] ~= nil) then compute_path_metric (dep, crp) end
   end

   if (metric.compute ~= nil) then metric.compute (crp) end
   crp._metrics.is_visited [metric_key] = true
end

local function compute_common_group (crc, group)
   local is_required = crc._metrics.is_required
   local _common_metrics = {}
   for _,v in ipairs (group) do
      _common_metrics [v] = true
   end

   for metric_key in pairs (_common_metrics) do
      if (is_required [metric_key]) then
         compute_common_metric (metric_key, crc)
      end 
   end
end

function compute_common_group_before_check (crc)
   compute_common_group (crc, common_metrics_before_check)
end

function compute_common_group_after_check (crc)
   local is_before = {}
   for _,metric_name in ipairs (common_metrics_before_check) do
      is_before [metric_name] = true
   end

   local common_metrics_after_check = {}
   for metric_name in pairs (common_metrics) do
      if (not is_before [metric_name]) then
         table.insert (common_metrics_after_check, metric_name)
      end
   end
   compute_common_group (crc, common_metrics_after_check)
end

function compute_path_group (crp)
   local is_required = crp._metrics.is_required

   for metric_key in pairs (path_metrics) do
      if (is_required [metric_key]) then
         compute_path_metric (metric_key, crp)
      end
   end
end

function clear_context_cache (cqa_context, fct)
   for fid in pairs (cqa_context._src_line_groups or {}) do
      if (fct == nil or fct:get_id() == fid) then
         cqa_context._src_line_groups [fid] = nil
      end
   end
end

function add_user_metrics (user_metrics)
   if (type(user_metrics.common_metrics) == "table") then
      for k,v in pairs (user_metrics.common_metrics) do
         common_metrics [k] = v
      end
   end
   if (type(user_metrics.path_metrics) == "table") then
      for k,v in pairs (user_metrics.path_metrics) do
         path_metrics [k] = v
      end
   end
   if (type(user_metrics.common_metrics_before_check) == "table") then
      for _,v in ipairs (user_metrics.common_metrics_before_check) do
         table.insert (common_metrics_before_check, v)
      end
   end
end
