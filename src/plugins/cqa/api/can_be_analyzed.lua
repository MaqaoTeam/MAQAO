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

--- Module cqa.can_be_analyzed
-- Defines the can_be_analyzed function used to test whether a loop can be analyzed by cqa
module ("cqa.api.can_be_analyzed", package.seeall);

require "cqa.consts"

-- Analyzes instructions in a given path
local function check_path_insns (crc, path)
   local compat_uarchs = {}; -- list of uarchs supporting all instructions in the path
   local compat_procs = {}; -- list of procs supporting all instructions in the path
   local blocks = crc.blocks
   local cqa_context = crc.context
   local arch = cqa_context.arch_obj
   local proc = cqa_context.proc

   local jmp_found = false; -- true as soon as at least one jump instruction found in the path
   local _blocks;
   local blocks_type = crc ["blocks type"];
   if (blocks_type ~= "loop" or (blocks:is_innermost() or arch:get_name() == "k1om")) then
      _blocks = path;
   elseif (blocks_type == "loop") then
      local loop_addr = get_userdata_address (blocks);
      _blocks = {};
      for _,block in ipairs (path) do
         if (get_userdata_address (block:get_loop()) == loop_addr) then
            table.insert (_blocks, block);
         end
      end
   else
      print ("unexpected case: check_path_insns");
   end

   -- list of isets supported by the target uarch/proc
   local valid_isets = {};
   if (proc ~= nil) then
      for _, iset in ipairs (proc:get_isets()) do
         valid_isets [iset] = iset;
      end
   else
      local uarch = arch:get_uarch_by_id(cqa_context.uarch)
      for _, iset in ipairs (uarch:get_isets()) do
         valid_isets [iset] = iset;
      end
   end
   local missing_isets = false; -- instruction sets missing in the path
   local path_isets = {};       -- Instruction sets present in the path
   local nb_path_isets = 0;     -- Number of instruction sets present in the path

   -- for each block
   for _,block in ipairs (_blocks) do
      -- Look for a jump instruction in the current block
      if (not jmp_found and blocks_type == "loop") then
         for insn in block:instructions() do
            local fam = insn:get_family();
            if (fam == Consts.FM_CJUMP or
                fam == Consts.FM_JUMP or
                fam == Consts.FM_LOOP) then
               jmp_found = true;
               break;
            end
         end
      end
      -- for each instruction
      for insn in block:instructions() do
         local iset = insn:get_iset();
         if (valid_isets [iset] == nil) then
            -- Instruction set of the instruction is not in the proc/uarch
            missing_isets = true;
         end
         -- Storing the instruction set of the instruction in the list of sets for the path
         if (path_isets[iset] == nil) then
            path_isets[iset] = iset;
            nb_path_isets = nb_path_isets + 1;
         end
         
         if (insn:get_name() == "(bad)") then
            return false, "at least one bad instruction";
         end
      end
   end

   -- If proc/uarch does not support some instructions in the path
   if (missing_isets) then
      -- Finds all the uarchs that support all the isets present in the path
      for _,uarch in ipairs (arch:get_uarchs()) do
         local nb_supported_isets = 0
         for _,iset in ipairs (uarch:get_isets()) do
            if (path_isets [iset] ~= nil and nb_supported_isets < nb_path_isets) then
               nb_supported_isets = nb_supported_isets + 1
            elseif (nb_supported_isets == nb_path_isets) then
               break
            end
         end

         if (nb_supported_isets == nb_path_isets) then
            table.insert (compat_uarchs, uarch)
         end
      end

      -- Finds all the procs that support all the isets present in the path
      for _,proc in ipairs (arch:get_procs()) do
         local nb_supported_isets = 0
         for _,iset in ipairs (proc:get_isets()) do
            if (path_isets [iset] ~= nil and nb_supported_isets < nb_path_isets) then
               nb_supported_isets = nb_supported_isets + 1
            elseif (nb_supported_isets == nb_path_isets) then
               break
            end
         end

         if (nb_supported_isets == nb_path_isets) then
            table.insert (compat_procs, proc)
         end
      end

      return false, "at least one unsupported instruction", compat_uarchs, compat_procs;
   end

   -- If no jump instruction in the loop path
   if (not jmp_found and blocks_type == "loop") then
      return false, "no jump instruction (loop probably badly detected)";
   end

   return true;
end

--- Tests whether a loop can be analyzed by CQA, that is composed of 1 to max_paths paths containing:
--  * no bad instructions
--  * no unsupported instructions (requiring a micro-architecture more recent than target)
--  * at least one jump instruction
-- @param crc cqa_results_common data structure (CF data_struct.lua)
-- @return boolean
-- @return nil or an explanation string if previous return value is false
-- @return nil or the list of compatible uarchs (userdata:uarch values) if previous return value is "at least one unsupported instruction"
-- @return nil or the list of compatible procs (userdata:proc values) if previous return value is "at least one unsupported instruction"
function can_be_analyzed (crc)
   local blocks = crc.blocks
   local cqa_context = crc.context

   if (crc ["blocks type"] == "path") then
      return check_path_insns (crc, blocks);
   end

   if ((not cqa_context.ignore_paths) and (cqa_context.arch ~= "k1om" or (crc ["blocks type"] == "loop" and blocks:is_innermost()))) then
      local nb_paths = blocks:get_nb_paths();
      if (nb_paths == -1) then
         return false, "failed to get the number of paths";
      end

      if (nb_paths > cqa_context.max_paths) then
         crc ["nb paths"] = nb_paths;
         return false, string.format (
            "too many paths (%d paths > max_paths=%d)",
            nb_paths, cqa_context.max_paths);
      end

      -- for each path
      for path in blocks:paths() do
         local r1, r2, r3, r4 = check_path_insns (crc, path);
         if (not r1) then return false, r2, r3, r4 end
      end
      return true;

   else -- k1om and not innermost
      if (not cqa_context.ignore_paths) then
         Debug:warn ("Assuming a loop containing GATHER/SCATTER patterns:...");
         Debug:warn ("... considering only one global path");
      end

      local main_path = {};
      for block in blocks:blocks() do
         table.insert (main_path, block);
      end

      return check_path_insns (crc, main_path);
   end
end
