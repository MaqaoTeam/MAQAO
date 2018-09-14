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

module ("cqa.api.data_struct", package.seeall)

-- TODO: support "table of X" types

-- Defines keys that can be used to access cqa_context and cqa_results tables
keys = {

   cqa_context = {
      -- architecture: "arm64"
      arch = { type = "string" },
      
      -- micro-architecture: Consts.<arch>.UARCH_*
      uarch = { type = "number" },

      -- micro-architecture constants returned by <arch>/get_uarch_constants()
      uarch_consts = { type = "table" },

      -- confidence levels list: "gain", "potential", "hint" and "expert"
      confidence = { type = "table:string" },

      -- analysis will be done for the host machine: true/false
      for_host = { type = "boolean" },

      -- host machine frequency in GHz
      freq = { type = "number" },

      -- function to select blocks in a loop, defined in <cqa_root>/user.lua
      blocks = { type = "function" },

      -- action when hitting a CALL: "append", "inline"
      follow_calls = { type = "string" },

      -- memory level string ("L1", "L2", "L3" or "RAM")
      memory_level = { type = "string" },

      -- memory level filepath string: path to a microbench table (when enabled)
      memory_level_filepath = { type = "string" },

      -- function to modify instructions before analysis, defined in <cqa_root>/mod.lua
      insn_modifier = { type = "function" },

      -- options to pass to insn_modifier: "vector_aligned", "vector_unaligned", "int_novec", "force_sse"
      insn_modifier_options = { type = "table:string" },

      -- TODO: move this to something like _cache
      -- table containing cached data: non-memory execution ports 
      _nomem_uops = { type = "table" },

      -- TODO: move this to something like _cache
      -- table containing cached data: output of cqa:group_loops_by_src_lines
      _src_line_groups = { type = "table" },

      -- boolean true if min DIV/SQRT latency assumed = max
      -- allow to speedup analysis
      ignore_min_DIV_SQRT_latency = { type = "boolean" },

      -- options to use when computing "if vectorized" metrics: "force_sse", "int_novec"
      if_vectorized_options = { type = "table:string" },

      -- paths will be ignored (assuming one global path): true/false
      ignore_paths = { type = "boolean" },

      -- virtual unroll factor
      vu = { type =  "number" },

      -- instruction extensions bypass
      ieb = { type = "string" },

      cmp_insn = { type = "userdata" },

      -- optimization report files
      opr = { type = "table:string" },

      -- micro-architecture model
      um = { type = "string" },

      -- requested metric names
      requested_metrics = { type = "table:string" },

      -- #PRAGMA_NOSTATIC UFS
      -- UFS options
      ufs_params = { type =  "table" },
      -- #PRAGMA_STATIC
   },

   cqa_results = {
      -- reference to the cqa_results_common subtable
      common = { type = "table" },

      -- (list of) references to cqa_results_path subtables
      paths  = { type = "table" },
   },

   cqa_results_common = {
      -- reference to the related context
      context  = { type = "table" },

      -- source loop info
      ["src loop"] = { type = "table" },

      -- reference to the related "blocks" object
      blocks = { type = { "userdata", "table:block"} },

      -- warnings
      warnings = { type = "table:string" },

      -- table containing cached data
      _cache = { type = "table" },

      -- table containing hidden data related to metrics computation
      _metrics = { type = "table" },

      -- language: Consts.LANG_*, returned by fct:get_language()
      lang_code = { type = "number" },

      -- compiler string, returned by fct:get_compiler()
      comp_str = { type = "string" },

      -- compiler options, returned by fct:get_compile_options()
      comp_opts = { type = "string" },
   },

   cqa_results_path = {
      -- reference to the cqa_results_common subtable
      common = { type = "table" },

      -- related blocks
      blocks = { type = "table:block" },

      -- related instructions
      insns = { type = "table:insn" },

      -- warnings
      warnings = { type = "table:string" },

      -- source language
      lang_code = { type = "number" },

      -- dominant (most frequently used) element type + size in bytes
      dominant_element      = { type = "string" }, -- ex: "single precision FP"
      dominant_element_size = { type = "number" },
   }
}

local function check_table_and_key (table, key)
   if (key == nil) then
      -- TODO: remove traceback once integrated into Debug/Message
      Message:critical ("cannot get value from a nil key\n"..debug.traceback());
   end

   if (keys [table.type] [key] == nil) then
      local found = false;
      for k in pairs (keys [table.type]) do
         if (string.find (key, k) ~= nil) then
            found = true;
            break;
         end
      end
      if (not found) then
         -- TODO: remove traceback once integrated into Debug/Message
         Message:critical (key.." is not a valid key for a "..table.type.." table\n"..debug.traceback());
      end
   end
end

local function get_invalid_type_msg (value, expected_type)
   local value_type = type (value);

   if (value_type ~= "table" and value_type ~= expected_type) then
      if (expected_type ~= "number" or value ~= "NA") then
	 -- TODO: remove traceback once integrated into Debug/Message
	 return ": Storing a value with type "..value_type..
	    " (expected type: "..expected_type..")\n"..debug.traceback();
      end

      return;
   end

   if (value_type == "table") then
      local expected_nested_type = string.match (expected_type, "table:(%w+)");
      if (expected_nested_type ~= nil) then
	 for _,nested_value in pairs (value) do
	    local nested_value_type = type (nested_value);
	    if (nested_value_type == "userdata") then
	       nested_value_type = getmetatable (nested_value)["_NAME"];
	    end
	    if (nested_value_type ~= expected_nested_type) then
	       -- TODO: remove traceback once integrated into Debug/Message
	       return ": Storing a value in a table with type "..nested_value_type..
		  " (expected type: "..expected_nested_type..")\n"..debug.traceback();
	    end
	 end
      end
   end
end

local function check_value (table, key, value)
   if (value == nil) then
      -- TODO: remove traceback once integrated into Debug/Message
      Debug:warn ("storing as key "..key.." a nil value into a "..table.type.." table\n"..debug.traceback());
      return;
   end

   local matching_entry = keys [table.type] [key];
   if (matching_entry == nil) then
      for k,v in pairs (keys [table.type]) do
         if (string.find (key, k) ~= nil) then
            matching_entry = v;
            break;
         end
      end
   end

   if (matching_entry == nil) then return end

   local expected_type = matching_entry.type;

   local msg;
   if (type (expected_type) == "table") then
      for _,t in ipairs (expected_type) do
	 msg = get_invalid_type_msg (value, t);
	 if (msg == nil) then
	    return;
	 end
      end
   else
      msg = get_invalid_type_msg (value, expected_type);
   end

   if (msg ~= nil) then Message:critical(key..": "..msg) end
end

-- CF http://www.lua.org/pil/13.4.4.html
function new (str)
   -- TODO: reenable this... if compatible with new metrics computation engine
   if (true) then return {} end
   --if (not Debug:is_enabled()) then return {} end

   if (not keys [str]) then
      local valid_types = {};
      for k in pairs (keys) do table.insert (valid_types, k) end
      -- TODO: remove traceback once integrated into Debug/Message
      Message:critical (str.." invalid for creating a new cqa_context or cqa_results table. Valid: "..table.concat (valid_types, ',').."\n"..debug.traceback());
   end

   t = {}   -- original table (created somewhere)
   
   -- keep a private access to original table
   local _t = t
   
   -- create proxy
   t = { __orig = _t, type = str }
   
   -- create metatable
   local mt = {
      __index = function (t,k)
		   --print("*access to element " .. tostring(k))
		   check_table_and_key (t, k)
		   return _t[k]   -- access the original table
		end,
      
      __newindex = function (t,k,v)
		      --print("*update of element " .. tostring(k) ..
		      -- " to " .. tostring(v))
		      check_table_and_key (t, k)
		      check_value (t, k, v)
		      _t[k] = v   -- update original table
		   end
   }

   return setmetatable(t, mt)
end
