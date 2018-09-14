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


--- Module cqa.cqa_results
-- Defines the main CQA library function: cqa:get_cqa_results.
-- These function computes static metrics for a given target and pack them
-- in a table (which is returned).
module ("cqa.api.cqa_results", package.seeall)

require "cqa.consts";
require "cqa.api.data_struct";
require "cqa.api.metrics";

local metrics = cqa.api.metrics

local function do_virtual_unrolling (outer_loop, unroll_factor)
   local outer_loop_addr = get_userdata_address (outer_loop);

   local state = "outer";
   local inner_blocks = {};
   local blocks = {};

   local input_blocks = {};

   -- Sort can no more be applied after transformation => do it now
   for block in outer_loop:blocks() do table.insert (input_blocks, block) end
   table.sort (input_blocks, function(a,b) return (
		     a:get_first_insn():get_address() <
		     b:get_first_insn():get_address()) end)

   for _,block in ipairs (input_blocks) do
      if (get_userdata_address (block:get_loop():get_parent()) == outer_loop_addr) then
	 -- inner loop block

	 -- if switching from outer to inner block
	 if (state == "outer") then
	    state = "inner";
	    for k in ipairs (inner_blocks) do inner_blocks [k] = nil end
	 end
	 
	 table.insert (inner_blocks, block);

      else
	 -- self (outer) loop block

	 -- if switching from inner to outer block
	 if (state == "inner") then
	    state = "outer";
	    for i = 1, unroll_factor do
	       for _,v in ipairs (inner_blocks) do
		  table.insert (blocks, v);
	       end
	    end
	 end

	 table.insert (blocks, block);
      end
   end

   return blocks;
end

--- [Local function, used only by get_loop_cqa_results]
-- Returns exclusive (self) basic blocks for a given loop (excluding inner loops)
-- @param loop a loop
-- @param cqa_context cqa_context table (see consts.lua)
-- @return blocks
local function get_blocks_from_loop (loop, cqa_context)
   if (cqa_context.blocks ~= nil) then
      return cqa_context.blocks (loop);
   elseif (cqa_context.vu ~= nil) then
      return do_virtual_unrolling (loop, cqa_context.vu);
   end

   local blocks = {};

   if (loop:is_innermost() or cqa_context.arch == "k1om") then
      for block in loop:blocks() do
         table.insert (blocks, block);
      end
   else
      local loop_addr = get_userdata_address (loop);
      for block in loop:blocks() do
         if (get_userdata_address (block:get_loop()) == loop_addr) then
            table.insert (blocks, block);
         end
      end
   end

   return blocks;
end

--- [Local function, used only by get_path_cqa_results]
-- Returns exclusive (self) basic blocks for a given path (excluding inner loops)
-- @param loop a loop
-- @param cqa_context cqa_context table (see consts.lua)
-- @param path a path (list of blocks)
-- @return blocks
local function get_blocks_from_path (loop, cqa_context, path)
   if (cqa_context.blocks ~= nil) then
      return cqa_context.blocks (loop);
   elseif (cqa_context.vu ~= nil) then
      return do_virtual_unrolling (loop, cqa_context.vu);
   end

   if (loop:is_innermost() or cqa_context.arch == "k1om") then
      return path;
   end

   local loop_addr = get_userdata_address (loop);
   local blocks = {};

   for _,block in ipairs (path) do
      if (get_userdata_address (block:get_loop()) == loop_addr) then
         table.insert (blocks, block);
      end
   end

   return blocks;
end

local function get_blocks_from_function (fct)
   local blocks = {};

   for block in fct:blocks() do
      table.insert (blocks, block);
   end

   return blocks;
end

-- TODO: improve this
local function scale_induction_stride (insns, scale_factor)
   for k,insn in ipairs (insns) do
      local family = insn:get_family();

      if ((family == Consts.FM_ADD or family == Consts.FM_SUB) and not insn:is_FP()) then
         local prefix, imm, suffix = string.match (insn:get_asm_code(), "(.*)$(0x%x+),(.*)");
         if (imm ~= nil) then
            local new_asm = string.format ("%s$0x%x,%s", prefix, imm * scale_factor, suffix);
            insns[k] = insn:parsenew (new_asm);
         end
      end
   end
end

--- [Local function, used only by get_path_cqa_results]
-- Returns instructions contained in a given path
-- @param path list of basic blocks
-- @param must_be_sorted boolean true if blocks must be sorted (by incr. addr.)
-- @param cqa_context cqa_context table (see consts.lua)
-- @return path instructions
-- @return extra unroll factor (from instruction modifier)
-- @return instructions created by the instruction modifier (allowing to free them)
local function get_path_insns (path, must_be_sorted, cqa_context, streams)
   local unordered_blocks = false;
   local max_addr, first_block, last_block;
   local blocks = {};

   -- for each block
   for _,block in ipairs (path) do
      -- check if the first block is a loop entry
      if (first_block == nil) then
         first_block = block;
         if (not first_block:is_loop_entry()) then
            Debug:warn ("The first block is not a loop entry");
         end
      end

      -- detect the first non contiguous block (considering address in the binary)
      if (not unordered_blocks) then
         if (max_addr == nil or max_addr < block:get_first_insn():get_address()) then
            max_addr = block:get_first_insn():get_address();
         else
            unordered_blocks = true;
            Debug:warn ("Blocks are not ordered by their address");
         end
      end

      -- build a clone of 'path'
      table.insert (blocks, block);

      -- save the last block
      last_block = block;
   end

   if (not last_block:is_loop_exit()) then
      Debug:warn ("The last block is not a loop exit");
   end

   -- When virtual unrolling, sort will be applied before transformation
   if (must_be_sorted and cqa_context.vu == nil) then
      table.sort (blocks, function(a,b) return (
                        a:get_first_insn():get_address() <
                        b:get_first_insn():get_address()) end)
      Debug:warn ("Blocks were reordered");
   end

   -- Build original instructions sequence (needed by insn_modifier)
   local path_insns_orig = {};
   for _,block in ipairs (blocks) do
      for insn in block:instructions() do
         table.insert (path_insns_orig, insn);
      end
   end

   local insn_modifier = cqa_context ["insn_modifier"];
   if (insn_modifier == nil) then return path_insns_orig end

   -- Save internal address of original instructions
   local orig_insns = {}
   for _,insn in ipairs (path_insns_orig) do
      orig_insns [get_userdata_address (insn)] = true;
   end

   -- Build modified instructions sequence (using insn_modifier)
   local path_insns_modif = {};
   local path_extra_unroll_factor = 1;

   local at_least_one_modification_failure = false

   -- Looking for the last CMP instruction
   -- TODO; improve this (run it only if insn_modifier needs it)
   for i = #path_insns_orig, 1, -1 do
      if (path_insns_orig[i]:get_name() == "CMP") then
         cqa_context.cmp_insn = path_insns_orig[i];
         break;
      end
   end

   local is_stride_1 = {};
   if (not cqa_context.insn_modifier_options.force_vec) then
      for _,stream in ipairs (streams or {}) do
         local unroll_info = stream:get_unroll();
         local load_stride, store_stride;
         if (type (unroll_info.load) == "table") then load_stride = unroll_info.load.stride  end
         if (type (unroll_info.store)== "table") then store_stride= unroll_info.store.stride end
         if ((load_stride == 1 and store_stride == nil) or
                (store_stride == 1 and load_stride == nil) or
             (load_stride == 1 and store_stride == 1)) then
            for _,insn in stream:instructions() do is_stride_1 [insn:get_userdata_address()] = true end
         end
      end
   end

   for _,block in ipairs (blocks) do
      for insn in block:instructions() do
         local _is_stride_1;
         if (cqa_context.insn_modifier_options.force_vec) then
            _is_stride_1 = true;
         else
            _is_stride_1 = is_stride_1 [insn:get_userdata_address()]
         end
         local modified_insns, insn_extra_unroll_factor = insn_modifier (insn, path_insns_orig, cqa_context, _is_stride_1);

         for _,v in ipairs (modified_insns) do
            if (v == nil) then at_least_one_modification_failure = true end
            table.insert (path_insns_modif, v);
         end

         if (insn_extra_unroll_factor ~= nil) then
            if (path_extra_unroll_factor < insn_extra_unroll_factor) then
               path_extra_unroll_factor = insn_extra_unroll_factor;
            end
         end
      end
   end
   
   scale_induction_stride (path_insns_modif, path_extra_unroll_factor);

   if (at_least_one_modification_failure) then
      Message:warn (cqa.consts.Errors["INSTRUCTION_MODIFICATION_FAILURE"].str,
                    cqa.consts.Errors["INSTRUCTION_MODIFICATION_FAILURE"].num);
   end

   -- Save internal address of added (temporary) instructions to free them later
   local tmp_insns = {};
   for _,v in ipairs (path_insns_modif) do
      if (orig_insns [get_userdata_address (v)] == nil) then
         table.insert (tmp_insns, v)
      end
   end

   return path_insns_modif, path_extra_unroll_factor, tmp_insns;
end

--- [Local function, used only by get_path_cqa_results]
-- Caches in cqa_results some data that are both expensive and frequently used
-- @param crp cqa_results_path table (see data_struct.lua)
local function cache_data (crp)
   local crc = crp.common
   local arch = crc.context.arch;
   local dispatch_cache = crc._cache ["dispatch"]
   local can_be_packed_cache = crc._cache ["can_be_packed"]

   for i,insn in ipairs (crp.insns) do

      can_be_packed_cache ["INT"   ][insn] = insn:can_be_packed_INT (crp.insns, arch, crc.context.uarch_consts, (i == 1))
      can_be_packed_cache ["FP"    ][insn] = insn:can_be_packed_FP  (arch, crc.context.uarch_consts)
      can_be_packed_cache ["INT+FP"][insn] = can_be_packed_cache ["INT"][insn] or can_be_packed_cache ["FP"][insn]
   end
end

-- TODO: move arch-dependent parts to arch-dependent directories
-- TODO: improve this by considering removing some limitations:
-- * external functions (dyn libs)
-- * loops/paths in followed functions
-- * recursivity support
local function follow_calls (cqa_results_path)
   local insns_new = {};

   -- check if an instruction (by its name) is related to a function call (saving/restoring context or giving back control)
   local function is_fct_overhead (name)
      for _,pattern in ipairs ({"^PUSH%u$", "^POP%u$", "^LEAVE$", "^RET%u?$"}) do
         if (string.find (string.upper (name), pattern) ~= nil) then return true end
      end

      return false
   end

   local mode = cqa_results_path.common.context.follow_calls;

   for _,insn in ipairs (cqa_results_path.insns) do

      if (insn:is_call() and insn:get_branch_target() ~= nil) then
         if (mode == "append") then
            table.insert (insns_new, insn);
         end

         -- follow only calls to a reachable function, excluding recursive calls
         local target_fct = insn:get_branch_target():get_function();
         if (target_fct ~= nil and target_fct ~= insn:get_function()) then
            local str;
            if (mode == "append") then str = "Appending" else str = "Inlining" end
            if (cqa_results_path.warnings ["get_path_cqa_results"] == nil) then
               cqa_results_path.warnings ["get_path_cqa_results"] = {}
            end
            table.insert (cqa_results_path.warnings ["get_path_cqa_results"],
                          string.format (
                             "Following the function called by [%s]:\n"..
                             "%s called function instructions\n",
                             insn:get_asm_code(), str));

            for block in target_fct:blocks() do
               -- Excluding padding blocks, not executed
               if (block:is_padding() == false) then
                  for insn2 in block:instructions() do
                     if (mode == "append" or not is_fct_overhead (insn2:get_name())) then
                        table.insert (insns_new, insn2);
                     end
                  end
               end
            end

         elseif (mode == "inline") then
            table.insert (insns_new, insn);
         end -- target_fct ~= nil

      else -- insn not a call
         table.insert (insns_new, insn);
      end -- is_call
   end -- for each insn

   cqa_results_path.insns = insns_new
end

--- Returns CQA results for a path
-- @param blocks basic blocks corresponding to the path
-- @param path_ID path identifier
-- @param crc cqa_results.common table (see consts.lua), contains data common to all paths of the host loop
-- @return cqa_results.path table (see consts.lua)
function get_path_cqa_results (blocks, path_ID, crc)
   local cqa_context = crc.context;

   local streams = nil;
   if (cqa.api.mod.requires_streams (cqa_context.insn_modifier) and
       crc ["blocks type"] == "loop") then
      streams = crc.blocks:get_streams() [path_ID];
   end
   local insns, extra_unroll_factor, tmp_insns = get_path_insns (blocks, true, cqa_context, streams);

   if (#insns == 0) then return {} end

   local crp = cqa.api.data_struct.new ("cqa_results_path");
   crp.common      = crc;
   crp.blocks      = blocks;
   crp ["path ID"] = path_ID;
   crp.insns       = insns;
   crp.warnings    = {};

   -- TODO: understand in which context this can be true: insn modifier ?
   if (extra_unroll_factor ~= nil) then -- removes warning in debug mode
      crp.extra_unroll_factor = extra_unroll_factor;
   end

   if (cqa_context.follow_calls == nil) then
      -- Warn about call instructions (TODO: think to the best place to move it)
      for _,insn in ipairs (insns) do
         if (insn:is_call()) then
            if (crp.warnings ["get_path_cqa_results"] == nil) then
               crp.warnings ["get_path_cqa_results"] = {}
            end
            table.insert (crp.warnings ["get_path_cqa_results"],
                          "Detected a function call instruction: "..
                          "ignoring called function instructions.\n"..
                          "Rerun with --follow-calls=append to include them to analysis "..
                          " or with --follow-calls=inline to simulate inlining.");
            break;
         end
      end

   elseif (cqa_context.follow_calls == "append" or cqa_context.follow_calls == "inline") then
      follow_calls (crp);
   end

   -- TODO: cache management could probably be optimized (stack of, local var...)
   -- get_path_cqa_results() is recursive (CF push_clean_check): need to save/restore cache

   -- save previous cache state and fill internal cache
   local cbp_data = crp.common._cache.can_be_packed
   local saved_cbp_data = { INT = {}, FP = {}, ["INT+FP"] = {} }
   for elt_type,tab in pairs (cbp_data) do
      for insn,bool in pairs (tab) do
         saved_cbp_data [elt_type][insn] = bool
      end
   end
   cache_data (crp);

   metrics.init_path_metrics (crp)
   metrics.compute_path_group (crp) -- potential recursive calls here

   -- Frees artificial instructions created by the instruction modifier
   if (tmp_insns ~= nil) then
      local cbp_data = crp.common._cache ["can_be_packed"]
      for _,insn in ipairs (tmp_insns) do
         insn:free_parsenew();
         for _,v in pairs (cbp_data) do v[insn] = nil end
      end
   end

   -- restore previous state
   for elt_type,tab in pairs (cbp_data) do
      for insn in pairs (tab) do
         cbp_data [elt_type][insn] = nil
      end
      for insn,bool in pairs (saved_cbp_data [elt_type]) do
         cbp_data [elt_type][insn] = bool
      end
   end

   return crp;
end

--- Returns CQA results for a function, a loop or a path
-- @param blocks a function, a loop or a path (list of blocks)
-- @param cqa_context cqa_context table (see consts.lua)
-- @return cqa_results table (see consts.lua)
function get_cqa_results (blocks, cqa_context)
   if (type (cqa_context ["if_vectorized_options"]) ~= "table") then
      cqa_context ["if_vectorized_options"] = {};
   end
   if (type (cqa_context ["insn_modifier_options"]) ~= "table") then
      cqa_context ["insn_modifier_options"] = {};
   end
   if (cqa_context.memory_level == nil) then
      cqa_context.memory_level = { "L1" };
   end
   if (tonumber (cqa_context.max_paths) == nil) then
      cqa_context.max_paths = cqa.consts.MAX_NB_PATHS;
   end

   local cqa_results = cqa.api.data_struct.new ("cqa_results");
   cqa_results.common = cqa.api.data_struct.new ("cqa_results_common");
   local crc = cqa_results.common
   crc.context  = cqa_context;
   crc.blocks   = blocks;
   crc.warnings = {};

   metrics.init_common_metrics (crc)

   -- Allows to free paths if computed on purpose
   metrics.compute_common_metric ("blocks type", crc)
   local blocks_type = crc ["blocks type"]
   local paths_computed;
   if (blocks_type == "loop" or blocks_type == "function") then
      paths_computed = blocks:are_paths_computed();
   end

   -- Get arch or uarch (from asmfile) if not already done
   local asmfile
   if (blocks_type == "path") then asmfile = blocks[1]:get_asmfile() end
   if (blocks_type == "loop" or blocks_type == "function") then asmfile = blocks:get_asmfile() end
   if (cqa_context.arch == nil) then
      cqa_context.arch = asmfile:get_arch_name();
   end
   if (cqa_context.uarch == nil) then
      cqa_context.uarch = asmfile:get_uarch_id();
   end

   -- Get micro-architecture constants if not already done
   if (cqa_context.uarch_consts == nil) then
      local arch = cqa_context.arch
   end

   if (blocks_type == "loop" and not blocks:is_innermost()) then
      local todo = true

      -- On k1om, gather-scatter loops are processed by CQA as flat instructions
      if (cqa_context.arch == "k1om") then
         local innermost_loops = cqa.api.innermost_loops.get_innermost_loops (blocks:get_function())
         for _,l in ipairs (innermost_loops) do
            if (l:get_id() == blocks:get_id()) then
               todo = false; break
            end
         end
      end

      if (todo) then
         if (crc.warnings ["get_cqa_results"] == nil) then
            crc.warnings ["get_cqa_results"] = {}
         end
         table.insert (crc.warnings ["get_cqa_results"],
                       "Non-innermost loop: analyzing only self part (ignoring child loops).");
      end
   end

   metrics.compute_common_group_before_check (crc);

   -- If static analysis is not possible
   metrics.compute_common_metric ("can be analyzed", crc)
   if (not crc ["can be analyzed"]) then
      return cqa_results;
   end

   crc._cache = {["dispatch"] = {},
                 ["can_be_packed"] = {["INT"] = {},
                                      ["FP"] = {},
                                      ["INT+FP"] = {}}};

   metrics.compute_common_group_after_check (crc);

   cqa_results.paths  = {};

   if (blocks_type == "path") then
      crc ["nb paths"] = 1;
      local cqa_results_path = get_path_cqa_results (blocks, 1, crc);

      table.insert (cqa_results.paths, cqa_results_path);

      return cqa_results;
   end

   if (cqa_context.ignore_paths or (cqa_context.arch == "k1om" and
                                    blocks_type == "loop" and
                                    not blocks:is_innermost ())) then
      crc ["nb paths"] = 1;
      local _blocks;
      if (blocks_type == "loop") then
         _blocks = get_blocks_from_loop (blocks, cqa_context);
      else -- if (blocks_type == "function")
         _blocks = get_blocks_from_function (blocks);
      end
      local cqa_results_path = get_path_cqa_results (_blocks, 1, crc);

      table.insert (cqa_results.paths, cqa_results_path);

      return cqa_results;
   end

   crc ["nb paths"] = blocks:get_nb_paths();

   -- for each path
   local path_ID = 1;
   for path in blocks:paths() do
      local _blocks;
      if (blocks_type == "loop") then
         _blocks = get_blocks_from_path (blocks, cqa_context, path);
      else
         _blocks = path;
      end
      local cqa_results_path = get_path_cqa_results (_blocks, path_ID, crc);
      path_ID = path_ID + 1;

      table.insert (cqa_results.paths, cqa_results_path);
   end

   -- The caller may already have computed paths and relies on them at exit
   if (paths_computed == false) then blocks:free_paths() end

   return cqa_results;
end
