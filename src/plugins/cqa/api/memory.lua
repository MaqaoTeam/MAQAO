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

-- #PRAGMA_NOSTATIC MEM_PROJ

--- Module cqa.memory
-- Defines the get_memory_cycles function used to return cycles spent in memory levels
module ("cqa.api.memory", package.seeall)

require "cqa.consts"

local function is_present_in_DB (DB_key, lvl)
   return (type (__ubench_cycles_pattern [DB_key]) == "table" and
              __ubench_cycles_pattern [DB_key][lvl] ~= nil)
end

local function get_2_streams_groups (DB_key_template, DB_key_mask, lvl, stream_stride, stream_groups, set1, set2)
   local key1_template, key2_template = string.match (DB_key_template, DB_key_mask)

   -- 2 streams can be grouped if present in related set(s) and have same stride info (e.g. stride 1...)
   local function can_group (set1_stream_IDs, set2_stream_IDs)
      if (set1 ~= set2) then
         return (#set1_stream_IDs >= 1 and #set2_stream_IDs >= 1 and
                    stream_stride [set1_stream_IDs[1]].stride_info ==
                    stream_stride [set2_stream_IDs[1]].stride_info)
      else
         return (#set1_stream_IDs >= 2 and
                    stream_stride [set1_stream_IDs[1]].stride_info ==
                    stream_stride [set1_stream_IDs[2]].stride_info)
      end
   end

   -- for each existing base key ("split_256_F_U_4", "32_F_U_1"...) in first set
   for base_key,set1_stream_IDs in pairs (set1) do
      -- to generate keys for DB or sets we need some canonical representation (without heading "split")
      local nosplit_base_key = string.match (base_key, "split_(.*)") or string.match (base_key, "_(.*)")

      -- Using canonical key, forges keys for sets and DB
      local set1_key = string.format (key1_template, nosplit_base_key)
      local set2_key = string.format (key2_template, nosplit_base_key)
      local DB_key = string.format (DB_key_template, nosplit_base_key, nosplit_base_key, nosplit_base_key)

      local set2_stream_IDs = set2[set2_key]

      -- If existing data in DB, try to make groups of 2 compatible streams
      if (base_key == set1_key and set2_stream_IDs ~= nil and is_present_in_DB (DB_key, lvl)) then

         -- While can find 2 compatible streams, 1 in first set and 1 in second
         while (can_group (set1_stream_IDs, set2_stream_IDs)) do
            Debug:info (string.format ("Matched stream group: %s", DB_key))

            local second_stream_ID = set2_stream_IDs [1]
            if (set1 == set2) then second_stream_ID = set1_stream_IDs [2] end

            -- Insert 2-streams groups into stream_groups
            table.insert (stream_groups, { DB_key = DB_key,
                                           stride_info = stream_stride [set1_stream_IDs [1]].stride_info,
                                           stream_IDs = { set1_stream_IDs [1],
                                                          second_stream_ID
                                           }})
            
            -- Remove corresponding streams
            table.remove (set1_stream_IDs, 1)
            table.remove (set2_stream_IDs, 1)
         end
      end
   end
end

-- Supported: XYZ, XXX or XXY
-- Similar get_2_streams_groups
local function get_3_streams_groups (DB_key_template, DB_key_mask, lvl, stream_stride, stream_groups, set1, set2, set3)
   local key1_template, key2_template, key3_template = string.match (DB_key_template, DB_key_mask)

   local function get_stream_IDs (set1_stream_IDs, set2_stream_IDs, set3_stream_IDs)
      if (set1 == set2 and set1 == set3) then -- XXX
         return { set1_stream_IDs[1], set1_stream_IDs[2], set1_stream_IDs[3] }
      elseif (set1 == set2) then --XXY
         return { set1_stream_IDs[1], set1_stream_IDs[2], set3_stream_IDs[1] }
      elseif (set2 ~= set3) then -- XYZ
         return { set1_stream_IDs[1], set2_stream_IDs[1], set3_stream_IDs[1] }
      end      
   end

   local function can_group (set1_stream_IDs, set2_stream_IDs, set3_stream_IDs)
      local function have_compatible_stride ()
         local sid = get_stream_IDs (set1_stream_IDs, set2_stream_IDs, set3_stream_IDs)
         return (stream_stride [sid[1]].stride_info == stream_stride [sid[2]].stride_info and
                 stream_stride [sid[1]].stride_info == stream_stride [sid[3]].stride_info)
      end

      if (set1 == set2 and set1 == set3) then -- XXX
         return (#set1_stream_IDs >= 3 and have_compatible_stride())
      elseif (set1 == set2) then -- XXY
         return (#set1_stream_IDs >= 2 and #set3_stream_IDs >= 1 and have_compatible_stride())
      elseif (set2 ~= set3) then -- XYZ
         return (#set1_stream_IDs >= 1 and #set2_stream_IDs >= 1 and #set3_stream_IDs >= 1 and have_compatible_stride())
      end
   end

   for base_key,set1_stream_IDs in pairs (set1) do
      local nosplit_base_key = string.match (base_key, "split_(.*)") or string.match (base_key, "_(.*)")
      local set1_key = string.format (key1_template, nosplit_base_key)
      local set2_key = string.format (key2_template, nosplit_base_key)
      local set3_key = string.format (key3_template, nosplit_base_key)
      local DB_key = string.format (DB_key_template, nosplit_base_key, nosplit_base_key, nosplit_base_key)

      local set2_stream_IDs = set2[set2_key]
      local set3_stream_IDs = set3[set3_key]

      if (base_key == set1_key and set2_stream_IDs ~= nil and set3_stream_IDs ~= nil and
          is_present_in_DB (DB_key, lvl)) then

         while (can_group (set1_stream_IDs, set2_stream_IDs, set3_stream_IDs)) do
            Debug:info (string.format ("Matched stream group: %s", DB_key))

            table.insert (stream_groups, { DB_key = DB_key,
                                           stride_info = stream_stride [set1_stream_IDs [1]].stride_info,
                                           stream_IDs = get_stream_IDs (set1_stream_IDs, set2_stream_IDs, set3_stream_IDs)
                                           })

            -- remove corresponding streams
            table.remove (set1_stream_IDs, 1)
            table.remove (set2_stream_IDs, 1)
            table.remove (set3_stream_IDs, 1)
         end
      end
   end
end

local function get_stream_groups (DB_key_2_stream_ID, lvl, stream_stride)
   local stream_groups = {}
   local DB_key_templates

   -- L+LS like "1_L_32_F_U_4-2_L_32_F_U_4-2_S_32_F_U_4"
   DB_key_templates = {"1_Lsplit_%s-2_L_%s-2_S_%s",
                       "1_L_%s-2_L_%s-2_S_%s"}
   for _,DB_key_template in ipairs (DB_key_templates) do
      get_2_streams_groups (DB_key_template, "1_L(.*)%-2_L(.*)%-2_S", lvl, stream_stride, stream_groups,
                            DB_key_2_stream_ID.L, DB_key_2_stream_ID.LS)
   end

   -- LLS
   DB_key_templates = {"1_Lsplit_%s-2_L_%s-3_S_%s",
                       "1_Lsplit_%s-2_Lsplit_%s-3_S_%s",
                       "1_L_%s-2_L_%s-3_Ssplit_%s",
                       "1_Lsplit_%s-2_Lsplit_%s-3_Ssplit_%s",
                       "1_Lsplit_%s-2_L_%s-3_Ssplit_%s",
                       "1_L_%s-2_L_%s-3_S_%s"}
   for _,DB_key_template in ipairs (DB_key_templates) do
      get_3_streams_groups (DB_key_template, "1_L(.*)%-2_L(.*)%-3_S(.*)", lvl, stream_stride, stream_groups,
                            DB_key_2_stream_ID.L, DB_key_2_stream_ID.L, DB_key_2_stream_ID.S)
   end

   -- L+S (independent) like "1_L_32_F_U_4-2_S_32_F_U_4"
   DB_key_templates = {"1_Lsplit_%s-2_S_%s",
                       "1_L_%s-2_Ssplit_%s",
                       "1_Lsplit_%s-2_Ssplit_%s",
                       "1_L_%s-2_S_%s"}
   for _,DB_key_template in ipairs (DB_key_templates) do
      get_2_streams_groups (DB_key_template, "1_L(.*)%-2_S(.*)", lvl, stream_stride, stream_groups,
                            DB_key_2_stream_ID.L, DB_key_2_stream_ID.S)
   end

   -- LL
   DB_key_templates = {"1_Lsplit_%s-2_L_%s",
                       "1_L_%s-2_Lsplit_%s",
                       "1_Lsplit_%s-2_Lsplit_%s",
                       "1_L_%s-2_L_%s"}
   for _,DB_key_template in ipairs (DB_key_templates) do
      get_2_streams_groups (DB_key_template, "1_L(.*)%-2_L(.*)", lvl, stream_stride, stream_groups,
                            DB_key_2_stream_ID.L, DB_key_2_stream_ID.L)
   end

   return stream_groups
end

--- Returns a standard replacement instruction to any instruction using a memory operand
-- Example: ADDSS (mem),reg => MOVSS
-- @return opcode (string)
local function get_mem_equiv_name (insn, mem_oprnd_size)
   return "MOV";
end

--- Checks whether an instruction is loading index registers in next instructions
-- That is if it modifies a register used as index in a memory operand appearing after the target instruction
local function is_loading_index_reg (insns, target_insn)

   local function is_reading_reg (insn, reg)
      local mem_oprnd = insn:get_first_mem_oprnd();
      if (mem_oprnd ~= nil) then
         local mem_oprnd_val = mem_oprnd ["value"];
         for _,sub_oprnd in ipairs (mem_oprnd_val) do
            if (sub_oprnd ["typex"] == "index" and sub_oprnd ["value"] == reg) then
               return true;
            end
         end
      end
      return false;
   end

   for _,v in pairs (target_insn:get_operands() or {}) do
      -- for each written register (should have only one)
      if (v["write"] == true and v["type"] == MDSAPI.REGISTER) then
         local after_target = false;
         for _,insn in ipairs (insns) do
            -- for each instruction executed after target_insn
            if (after_target == true and is_reading_reg (insn, v ["value"])) then return true end

            -- Instructions are ordered: just need to switch state after hitting target_insn
            if (insn == target_insn) then after_target = true end
         end
      end
   end

   return false;
end

--- Checks whether an entry is a valid min-max entry, that is { min = <number>, max = <number> }
local function check_min_max_entry (entry, entry_name)
   if (type (entry) ~= "table" or (type (entry.min) ~= "number" or type (entry.max) ~= "number")) then
      Message:display (cqa.consts.Errors ["INVALID_CQA_TAB_INVALID_MIN_MAX_ENTRY"], entry_name);
   end
end

--- Accumulates cycles from min-max entries targeted by input parameters
-- Rougly, compute output = output + input
-- @param input input table, containing one or more min-max entries
-- @param input_name string to display in case of check failure
-- @param output output table
-- @param mode "one to one", "one to all", "all to all" drive function to handle input/output
local function accumulate_cycles (input, input_name, output, mode)
   local function acc (input, output)
      output.min = output.min + input.min;
      output.max = output.max + input.max;
   end

   if (mode == "one to one") then
      check_min_max_entry (input, input_name);
      acc (input, output);

   elseif (mode == "one to all") then
      check_min_max_entry (input, input_name);
      for k in pairs (output) do acc (input, output[k]) end

   elseif (mode == "all to all") then
      for k in pairs (input) do
         check_min_max_entry (input[k],input_name.."["..k.."]");
         if (output[k] == nil) then output[k] = { min = 0, max = 0 } end
         acc (input[k], output[k]);
      end
   end
end

local function accumulate_cycles_insns (table_of_roles, role, lvl, entry_insn_name, mem_oprnd, cycles, forgotten_cycles)
   local input = table_of_roles [role][lvl];
   local input_name = "["..entry_insn_name.."]["..mem_oprnd ["size"].."]["..role.."]["..lvl.."]";

   accumulate_cycles (input, input_name, cycles[role], "all to all")
   if (next (input) == "stride1" and next (input, "stride1") == nil) then
      accumulate_cycles (input.stride1, input_name.."[stride1]", forgotten_cycles[role], "one to one")
   end
end

--- Accumulate cycles over all instructions, at instruction granularity
local function get_memory_cycles_insns (cycles, forgotten_cycles, insns, lvl)

   local function is_valid (insn)
      -- LEA is not reading/writing memory even if it has a true memory operand
      if (insn:get_family() == Consts.FM_LEA) then return false end

      local mem_oprnd = insn:get_first_mem_oprnd();

      -- Reject instructions with no memory operand or no memory operand size
      if (mem_oprnd == nil or mem_oprnd ["size"] == nil) then return false end

      -- All instructions (except GATHER, for the moment) should have a non zero memory operand size
      if (insn:get_family() ~= Consts.FM_GATHER and mem_oprnd ["size"] == 0) then return false end

      -- Instructions loading index registers are already taken into account in indirect access microbenchmarks
      -- In other words, when considering MOVSXD EAX,RBX; MOVSS (RSI,RBX),XMM1, MOVSS indir_* cycles were already
      -- measured with an extra instruction used to load index values
      if (is_loading_index_reg (insns, insn)) then return false end

      return true;
   end

   -- Filters out non-memory instructions or instructions with no/invalid memory operand size,
   -- that will not contribute to "memory" cycles
   local insns_filtered = {};
   for _,insn in ipairs (insns) do
      if (is_valid (insn)) then table.insert (insns_filtered, insn) end
   end

   for _,insn in ipairs (insns_filtered) do
      local mem_oprnd = insn:get_first_mem_oprnd();
      local entry_insn_name = insn:get_name(); -- key in the microbench table
      local table_of_sizes = __ubench_cycles [entry_insn_name];

      -- if current instruction not found in the database, try with a standard replacement (ADDSS => MOVSS)
      if (table_of_sizes == nil) then
         entry_insn_name = get_mem_equiv_name (insn, mem_oprnd ["size"]);
         table_of_sizes = __ubench_cycles [entry_insn_name];
      end

      if (table_of_sizes == nil) then
         -- if cannot match to anything in the database
         local str;
         if (entry_insn_name == insn:get_name()) then
            str = entry_insn_name;
         else
            str = string.format ("%s (or memory-equivalent: %s)", insn:get_name(), entry_insn_name);
         end
         Message:warn (string.format ("No entry for %s in ubench table: ignoring related cycles", str));

      elseif (type (table_of_sizes) ~= "table") then
         -- there is something matching but is not in expected format
         Message:display (cqa.consts.Errors ["INVALID_CQA_TAB_INVALID_INSN_ENTRY"], entry_insn_name);

      else
         -- normal case: something found and in correct format

         -- TODO: remove this after MAQAO fix
         --- Returns memory operand size for GATHER instructions
         -- For some of them, MAQAO returns a 0 size for memory operand
         local function get_mem_oprnd_size ()
	    if (insn:get_family() ~= Consts.FM_GATHER) then return mem_oprnd ["size"] end

            -- GATHER instructions: retrieves memory operand size
            local oprnds = insn:get_operands();
            for _,oprnd in pairs (oprnds or {}) do
               if (oprnd.write == true) then
                  -- Can store only 2 64-bits index values in a 128-bits register => can gather only 2 values
                  -- If 32 bits values, can gather only 64 bits
                  if ((string.find (entry_insn_name, "QD$" ) ~= nil or
                          string.find (entry_insn_name, "QPS$") ~= nil) and
                      oprnd.size == 128) then
                     return 64;
                  end

                  -- General case: memory operand size = output register size
                  return oprnd.size;
               end
            end
         end

         local mem_oprnd_size = get_mem_oprnd_size ();
         local table_of_roles = table_of_sizes [mem_oprnd_size];

	 if (type (mem_oprnd_size) ~= "number") then
	    Message:warn (string.format ("Unknown memory operand size for [%s]: ignoring related cycles",
					 insn:get_asm_code()));

         elseif (table_of_roles == nil) then
            -- If size not found in the database (for example, 32/64 bits available for MOV but requesting 8 bits)
            Message:warn (string.format ("No entry for %s size %d in ubench table: ignoring related cycles",
                                         entry_insn_name, mem_oprnd_size));

         elseif (type (table_of_roles) ~= "table") then
            -- If available but invalid
            Message:display (cqa.consts.Errors ["INVALID_CQA_TAB_INVALID_SIZE_ENTRY"],
                             entry_insn_name, mem_oprnd_size);

         else
            -- General case: present and valid
            if (insn:is_load ()) then
               accumulate_cycles_insns (table_of_roles, "src", lvl, entry_insn_name, mem_oprnd, cycles, forgotten_cycles);
               Debug:info (string.format ("Accumulate src cycles from instruction %s", insn:get_asm_code()))
            end

            if (insn:is_store ()) then
               accumulate_cycles_insns (table_of_roles, "dst", lvl, entry_insn_name, mem_oprnd, cycles, forgotten_cycles);
               Debug:info (string.format ("Accumulate dst cycles from instruction %s", insn:get_asm_code()))
            end
         end
      end
   end
end

-- TODO: improve
--- Returns the database key (string) corresponding to a stream
local function get_DB_key (stream)
   -- look for a memory operand size constant along all instructions in the stream
   local mem_oprnd_size;
   for _,insn in stream:instructions() do
      local mem_oprnd = insn:get_first_mem_oprnd();
      if (mem_oprnd ["size"] ~= nil and mem_oprnd ["size"] ~= 0) then
         if (mem_oprnd_size == nil) then
            mem_oprnd_size = mem_oprnd ["size"];
         elseif (mem_oprnd_size ~= mem_oprnd ["size"]) then
            return nil; -- at least 2 different memory operand sizes
         end
      end
   end

   -- unknown memory operand size => cannot match it
   if (mem_oprnd_size == nil or mem_oprnd_size == 0) then return nil end

   local unroll_info = stream:get_unroll();
   local unroll_factor = unroll_info.unroll_factor;

   -- unknown unroll factor => cannot match it
   if (unroll_factor == nil) then return nil end

   local compressed_format = stream:get_pattern (false, true, true);
   if (compressed_format == "LS") then
      -- Combined load+store patterns (typical case: updating [e.g scaling] values)

      -- non-temporal stores (data assumed vector-aligned)
      if (string.find (stream.__data.store[1].i_ptr:get_name(), "MOVNT") ~= nil) then
         return string.format ("1_L_%d_F_A_%d-1_Svmovntps_%d_F_A_%d",
                               mem_oprnd_size, unroll_factor,
                               mem_oprnd_size, unroll_factor);
      end

      -- general case (data assumed vector-unaligned)
      return string.format ("1_L_%d_F_U_%d-1_S_%d_F_U_%d",
                            mem_oprnd_size, unroll_factor,
                            mem_oprnd_size, unroll_factor);

   elseif (compressed_format == "L" or compressed_format == "S") then
      -- Pure load or pure store patterns

      -- split128, used to load/store vector-unaligned 256-bits data in two 128-bits chunks
      -- viewed by the MAQAO stream-detector as unrolling
      if (unroll_factor >= 2 and mem_oprnd_size == 128) then
         if (compressed_format == "L") then
            local insn_entry_1 = stream.__data.load[1];
            local insn_entry_2 = stream.__data.load[2];

            if (string.find (insn_entry_1.i_ptr:get_name(), "MOVUP") ~= nil and insn_entry_1.size == 16 and
                string.find (insn_entry_2.i_ptr:get_name(), "VINSERT[FI]128") ~= nil) then
               return string.format ("1_Lsplit_256_F_U_%d", unroll_factor/2),
	              string.format ("1_L_128_F_U_%d", unroll_factor);
            end
         else -- compressed_format == "S"
            local insn_entry_1 = stream.__data.store[1];
            local insn_entry_2 = stream.__data.store[2];

            if (string.find (insn_entry_1.i_ptr:get_name(), "MOVUP") ~= nil and insn_entry_1.size == 16 and
                string.find (insn_entry_2.i_ptr:get_name(), "VEXTRACT[FI]128") ~= nil) then
               return string.format ("1_Ssplit_256_F_U_%d", unroll_factor/2),
		      string.format ("1_S_128_F_U_%d", unroll_factor);
            end
         end
      end

      -- non-temporal stores (data assumed vector-aligned)
      if (compressed_format == "S" and string.find (stream.__data.store[1].i_ptr:get_name(), "MOVNT") ~= nil) then
         return string.format ("1_Svmovntps_%d_F_A_%d",
                               mem_oprnd_size, unroll_factor);
      end

      -- general case (data assumed vector-aligned)
      return string.format ("1_%s_%d_F_U_%d",
                            compressed_format, mem_oprnd_size, unroll_factor);
   end
end

local function get_insns_min_max_entry (insns, input, sublevel, lvl)
   local cycles = { src = { stride1 = { min = 0, max = 0 }},
                    dst = { stride1 = { min = 0, max = 0 }}};
   local forgotten_cycles = { src = { min = 0, max = 0 },
                              dst = { min = 0, max = 0 }};
   get_memory_cycles_insns (cycles, forgotten_cycles, insns, lvl);

   local cy_src = cycles.src [sublevel];
   local cy_dst = cycles.dst [sublevel];

   if (cy_src == nil and cy_dst == nil) then
      if (type (input) == "table") then
         return input.stride1;
      else
         return { min = 0, max = 0 };
      end
   elseif (cy_src == nil) then
      cy_src = cycles.src.stride1;
   elseif (cy_dst == nil) then
      cy_dst = cycles.dst.stride1;
   end

   local fc_src = forgotten_cycles.src;
   local fc_dst = forgotten_cycles.dst;

   return { min = cy_src.min + fc_src.min + cy_dst.min + fc_dst.min,
            max = cy_src.max + fc_src.max + cy_dst.max + fc_dst.max };
end

local function get_stream_group_min_max_entry (stream_group, path_streams, input, sublevel, lvl)
   if (input ~= nil and input[sublevel] ~= nil) then return input[sublevel] end;

   local insns = {};
   for _,stream_ID in ipairs (stream_group.stream_IDs) do
      for _,v in path_streams[stream_ID]:instructions() do table.insert (insns, v) end
   end

   return get_insns_min_max_entry (insns, input, sublevel, lvl);
end

local function get_stream_min_max_entry (stream, input, sublevel, lvl)
   if (input ~= nil and input[sublevel] ~= nil) then return input[sublevel] end;

   local insns = {};
   for _,v in stream:instructions() do table.insert (insns, v) end

   return get_insns_min_max_entry (insns, input, sublevel, lvl);
end

local function get_memory_cycles_patterns (cycles, forgotten_cycles, cqa_results, lvl, loop_is_unit_stride)
   -- Get streams corresponding to current path
   local path_streams
   if (cqa_results.common.context.insn_modifier == nil) then
      path_streams = cqa_results.common.blocks:get_streams() [cqa_results ["path ID"]];
   else
      path_streams = cqa_results.common.blocks:get_streams_from_insnlist (cqa_results.insns);
   end

   -- a loop is considered stride 1 (unit stride) if all streams are stride 1
   for _,stream in ipairs (path_streams) do
      local unroll_info = stream:get_unroll();
      if ((type (unroll_info.load ) == "table" and unroll_info.load.stride  ~= 1) or
          (type (unroll_info.store) == "table" and unroll_info.store.stride ~= 1)) then
         loop_is_unit_stride = false
         break
      end
   end

   local DB_key_2_stream_ID = { L = {}, S = {}, LS = {} }
   local stream_stride = {}
   
   local function tag_stream (stream_ID, DB_key, stride_info, input, input_name)
      if (DB_key ~= nil) then
         local t, base_key
         if (string.find (DB_key, "^1_L.*%-1_S") ~= nil) then
            t = DB_key_2_stream_ID.LS
            base_key = string.match (DB_key, "^1_L(.*)%-")
         elseif (string.find (DB_key, "^1_L") ~= nil) then
            t = DB_key_2_stream_ID.L
            base_key = string.match (DB_key, "^1_L(.*)")
         elseif (string.find (DB_key, "^1_S") ~= nil) then
            t = DB_key_2_stream_ID.S
            base_key = string.match (DB_key, "^1_S(.*)")
         end
   
         if (t [base_key] == nil) then
            t [base_key] = { stream_ID }
         else
            table.insert (t [base_key], stream_ID)
         end
      end

      stream_stride [stream_ID] = { stride_info = stride_info, input = input, input_name = input_name }
   end

   local in_a_stream = {}

   -- for each stream detected in this current loop path
   for stream_ID,stream in ipairs (path_streams) do
      local unroll_info = stream:get_unroll();
      local compressed_format  = stream:get_pattern (false, true, true);
      local DB_key;
      local input;
      local input_name = "virtual input";

      -- if easy to match to our current database
      if (unroll_info.is_fp == true and (
             (compressed_format == "LS" and unroll_info.symmetry == true) or
             compressed_format == "L" or compressed_format == "S")) then
         DB_key_pref, DB_key_alt = get_DB_key (stream);
         Debug:info (string.format ("Matched: pref=%s alt=%s", DB_key_pref or "", DB_key_alt or ""));

         -- if an entry matches in the database
         if (__ubench_cycles_pattern [DB_key_pref] ~= nil) then
	    DB_key = DB_key_pref;
	 elseif (__ubench_cycles_pattern [DB_key_alt] ~= nil) then
            -- backtrack to alternative pattern projections, for this stream only
            Debug:info (string.format ("BACKTRACK/NOT IN BD: FOUND ALTERNATIVE: pref=%s alt=%s", DB_key_pref or "", DB_key_alt or ""));

	    DB_key = DB_key_alt;
	 else
            -- backtrack to instruction-based projections, for this stream only
            Debug:info (string.format ("BACKTRACK/NOT IN BD: pref=%s alt=%s", DB_key_pref or "", DB_key_alt or ""));
         end

	 if (DB_key ~= nil) then
	    input = __ubench_cycles_pattern [DB_key][lvl];
            input_name = "["..DB_key.."]["..lvl.."]";
	 end
      else
         -- backtrack to instruction-based projections, for this stream only
         Debug:info (string.format ("BACKTRACK/TOO COMPLEX: is_fp=%s, compressed_format=%s",
                                    tostring (unroll_info.is_fp), compressed_format));
      end

      if (unroll_info.access_type == "constant" or unroll_info.base == "%RIP") then
	 -- considering L1, stride 1
	 if (__ubench_cycles_pattern [DB_key] ~= nil) then
	    input = __ubench_cycles_pattern [DB_key].L1;
	    input_name = "["..DB_key.."][L1]";
	 end

         tag_stream (stream_ID, DB_key, "constant", input, input_name)
         Debug:info (string.format ("Considering stream #%d (%s) as constant", stream_ID, DB_key or "no DB entry"))

      elseif (unroll_info.access_type == "strided") then
	 local load_stride  = "undef";
	 local store_stride = "undef";
	 if (type (unroll_info.load) == "table") then load_stride = unroll_info.load.stride  end
	 if (type (unroll_info.store)== "table") then store_stride= unroll_info.store.stride end

	 if ((store_stride ~= "undef" or load_stride ~= 1) and
             (load_stride ~= "undef" or store_stride ~= 1) and
             (load_stride ~= 1 or store_stride ~= 1)) then
	    Debug:info ("Strided pattern with unknown stride: assuming stride 1");
	 end
	    
	 -- considering stride 1
         if (type (input) == "table" and input.stride1 == nil) then
            Message:display (cqa.consts.Errors ["INVALID_CQA_TAB_INVALID_RAM_ENTRY"], input_name);
         end

         tag_stream (stream_ID, DB_key, "assumed stride 1", input, input_name)
         Debug:info (string.format ("Considering stream #%d (%s) as stride 1", stream_ID, DB_key or "no DB entry"))

      elseif (unroll_info.access_type == "indirect") then
	 -- considering ind_stride1 for lower_bound and ind_random for upper_bound
         if (input == "table" and input.stride1 == nil) then
            Message:display (cqa.consts.Errors ["INVALID_CQA_TAB_INVALID_RAM_ENTRY"], input_name);
         end

         tag_stream (stream_ID, DB_key, "indirect", input, input_name)
         Debug:info (string.format ("Considering stream #%d (%s) as indirect", stream_ID, DB_key or "no DB entry"))

      else -- unknown access type
	 -- considering stride1 for lower_bound and ind_random for upper_bound
         if (input == "table" and input.stride1 == nil) then
            Message:display (cqa.consts.Errors ["INVALID_CQA_TAB_INVALID_RAM_ENTRY"], input_name);
         end

         tag_stream (stream_ID, DB_key, "unknown", input, input_name)
         Debug:info (string.format ("Considering stream #%d (%s) as unknown", stream_ID, DB_key or "no DB entry"))
      end

      for _,v in stream:instructions() do in_a_stream [get_userdata_address (v)] = true end
   end

   local stream_groups = get_stream_groups (DB_key_2_stream_ID, lvl, stream_stride)
   local is_in_a_group = {}

   -- For each stream group
   for stream_group_ID,stream_group in ipairs (stream_groups) do
      local DB_key = stream_group.DB_key
      local input = __ubench_cycles_pattern [DB_key][lvl];
      local input_name = "["..DB_key.."]["..lvl.."]";

      for _,stream_ID in ipairs (stream_group.stream_IDs) do
         is_in_a_group [stream_ID] = true
      end

      if (stream_group.stride_info == "constant") then
	 -- considering L1, stride 1
	 if (__ubench_cycles_pattern [DB_key] ~= nil) then
	    input = __ubench_cycles_pattern [DB_key].L1;
	    input_name = "["..DB_key.."][L1]";
	 end

         local input_L1_stride1 = get_stream_group_min_max_entry (stream_group, path_streams, input, "stride1", "L1");
         accumulate_cycles (input_L1_stride1, input_name, cycles.pat, "one to all");
         Debug:info (string.format ("Accumulate cycles from stream group #%d (%s) as constant", stream_group_ID, table.concat (stream_group.stream_IDs, ",")))

      elseif (stream_group.stride_info == "assumed stride 1") then
         local input_stride1 = get_stream_group_min_max_entry (stream_group, path_streams, input, "stride1", lvl);
         accumulate_cycles (input_stride1, input_name.."[stride1]", cycles.pat, "one to all");
         Debug:info (string.format ("Accumulate cycles from stream group #%d (%s) as stride1", stream_group_ID, table.concat (stream_group.stream_IDs, ",")))

      elseif (stream_group.stride_info == "indirect") then
         local input_ind_stride1 = get_stream_group_min_max_entry (stream_group, path_streams, input, "ind_stride1", lvl);
         local input_ind_random  = get_stream_group_min_max_entry (stream_group, path_streams, input, "ind_random" , lvl);

         accumulate_cycles (input_ind_stride1, input_name.."[ind_stride1]", cycles.pat.lower_bound, "one to one");
         accumulate_cycles (input_ind_random , input_name.."[ind_random]" , cycles.pat.upper_bound, "one to one");
         Debug:info (string.format ("Accumulate cycles from stream group #%d (%s) as indirect", stream_group_ID, table.concat (stream_group.stream_IDs, ",")))
      else
         local input_stride1    = get_stream_group_min_max_entry (stream_group, path_streams, input, "stride1"   , lvl);
         local input_ind_random = get_stream_group_min_max_entry (stream_group, path_streams, input, "ind_random", lvl);

         accumulate_cycles (input_stride1   , input_name.."[stride1]"   , cycles.pat.lower_bound, "one to one");
         accumulate_cycles (input_ind_random, input_name.."[ind_random]", cycles.pat.upper_bound, "one to one");
         Debug:info (string.format ("Accumulate cycles from stream group #%d (%s) as unknwown", stream_group_ID, table.concat (stream_group.stream_IDs, ",")))
      end
   end

   local out_of_any_group_streams = {}
   for stream_ID in ipairs (path_streams) do
      if (not is_in_a_group [stream_ID]) then table.insert (out_of_any_group_streams, stream_ID) end
   end

   -- for each individual stream (out of any group)
   for _,stream_ID in ipairs (out_of_any_group_streams) do
      local stream = path_streams [stream_ID]
      local stride_info = stream_stride[stream_ID].stride_info
      local input = stream_stride[stream_ID].input
      local input_name = stream_stride[stream_ID].input_name

      if (stride_info == "constant") then
         local input_L1_stride1 = get_stream_min_max_entry (stream, input, "stride1", "L1");
         accumulate_cycles (input_L1_stride1, input_name, cycles.pat, "one to all");
         Debug:info (string.format ("Accumulate cycles from stream #%d (%s) as constant", stream_ID, base_key))

      elseif (stride_info == "assumed stride 1") then
         local input_stride1 = get_stream_min_max_entry (stream, input, "stride1", lvl);
         accumulate_cycles (input_stride1, input_name.."[stride1]", cycles.pat, "one to all");
         Debug:info (string.format ("Accumulate cycles from stream #%d as stride 1", stream_ID, base_key))

      elseif (stride_info == "indirect") then
         local input_ind_stride1 = get_stream_min_max_entry (stream, input, "ind_stride1", lvl);
         local input_ind_random  = get_stream_min_max_entry (stream, input, "ind_random" , lvl);

         accumulate_cycles (input_ind_stride1, input_name.."[ind_stride1]", cycles.pat.lower_bound, "one to one");
         accumulate_cycles (input_ind_random , input_name.."[ind_random]" , cycles.pat.upper_bound, "one to one");    
         Debug:info (string.format ("Accumulate cycles from stream #%d as indirect", stream_ID, base_key))
      else
         local input_stride1    = get_stream_min_max_entry (stream, input, "stride1"   , lvl);
         local input_ind_random = get_stream_min_max_entry (stream, input, "ind_random", lvl);

         accumulate_cycles (input_stride1   , input_name.."[stride1]"   , cycles.pat.lower_bound, "one to one");
         accumulate_cycles (input_ind_random, input_name.."[ind_random]", cycles.pat.upper_bound, "one to one");            
         Debug:info (string.format ("Accumulate cycles from stream #%d as unknown", stream_ID, base_key))
      end
   end

   -- for instructions not handled in streams, backtrack to instruction granularity
   local leftover_insns = {};
   for _,insn in ipairs (cqa_results.insns) do
      if (in_a_stream [get_userdata_address (insn)] == nil) then table.insert (leftover_insns, insn) end
   end
   get_memory_cycles_insns (cycles, forgotten_cycles, leftover_insns, lvl);
end

--- Returns cycles spent in the memory subsystem
-- @param cqa_results table returned by get_cqa_results
-- @param lvl memory level string: "L1", "L2", "L3" or "RAM"
-- @param files path to files generated by microbench, first one being for instructions and second one, for patterns
-- @return minimum and maximum cycles spent in the target memory level. Can be numbers or tables of numbers
function get_memory_cycles (cqa_results, lvl, files)
   local cycles, forgotten_cycles;
   local loop_is_unit_stride = true;

   cycles = { src = { stride1 = { min = 0, max = 0 }},
              dst = { stride1 = { min = 0, max = 0 }},
              pat = { lower_bound = { min = 0, max = 0 },
                      upper_bound = { min = 0, max = 0 }}};
   forgotten_cycles = { src = { min = 0, max = 0 },
                        dst = { min = 0, max = 0 }};

   if (cqa_results.common ["blocks type"] == "loop" and
       type (__ubench_cycles_pattern) == "table") then
      -- if streams detected by MAQAO and related database provided
      get_memory_cycles_patterns (cycles, forgotten_cycles, cqa_results, lvl, loop_is_unit_stride);
   else
      -- No streams detected or no patterns database => instruction granularity
      get_memory_cycles_insns (cycles, forgotten_cycles, cqa_results.insns, lvl);
   end

   local function get_cycles (min_max_key, auto_key, pattern_cycles, max_or_sum)
      local t = {};

      -- Set bounds from load instructions contributions
      for k,v in pairs (cycles.src) do
         t[k] = v [min_max_key];
         if (k ~= "stride1") then
            t[k] = t[k] + forgotten_cycles.src [min_max_key];
         end
      end

      -- Accumulate from store instructions contributions
      -- In L1, "instructions" bandwidth considered dedicated for loads and stores => can go in parallel
      for k,v in pairs (cycles.dst) do
         local sum = v [min_max_key];
         if (k ~= "stride1") then sum = sum + forgotten_cycles.dst [min_max_key] end

         if (t[k] == nil) then
            t[k] = sum;
         elseif (lvl == "L1") then
            t[k] = math.max (t[k], sum)
         else
            t[k] = t[k] + sum
         end
      end

      -- Accumulate from patterns contributions
      for k in pairs (t) do
         t[k] = t[k] + pattern_cycles [min_max_key];
      end

      t.auto = t[auto_key];

      return t;
   end

   local lower_bound = get_cycles ("min", "stride1", cycles.pat.lower_bound, lvl);
   local upper_bound;
   if (loop_is_unit_stride) then
      upper_bound = get_cycles ("max", "stride1", cycles.pat.upper_bound, lvl);
   else
      upper_bound = get_cycles ("max", "ind_random", cycles.pat.upper_bound, lvl);
   end

   return lower_bound, upper_bound;
end

-- #PRAGMA_STATIC
