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


module ("analyze", package.seeall)

-- Function used to sort instructions
local function insn_cmp(insn1, insn2)
   return insn1:get_address() < insn2:get_address()
end

-- Function used to check if an instruction belongs to a given loop
local function block_is_in_loop(config, block)
   local block_loop = block:get_loop()   -- Retrieves the innermost loop containing the instruction
   while block_loop ~= nil do
      if analyze:loop_matches(config, block_loop) then
         return true
      end
      block_loop = block_loop:get_parent()
   end
   return false
end

-- Retrieves the name of the function to which an instruction belongs
local function insn_get_fct_name(insn)
   local insn_fct = insn:get_block():get_function()
   return insn_fct:get_demname() or insn_fct:get_name()
end

-- Displays the list of instructions
function analyze:list_instructions (config)
   local asmfile = config["asmfile"]

   -- Loading instructions from the file (restricted to function and/or loop if requested) into an array
   local insns = {}
   for f in asmfile:functions() do
      if analyze:fct_matches(config, f) then
         -- This function is not filtered out
         for b in f:blocks() do
            if config["loop-filters"] == nil or block_is_in_loop(config, b) then
               -- The block belongs to one of the requested loops or none were requested
               for i in b:instructions() do
                  -- Loads the instructions in an array
                  table.insert(insns, i)
               end
            end
         end
      end
   end
   -- Sorts the instructions by address
   table.sort(insns, insn_cmp)

   -- Prints the list
   local func = ""
   local loop = ""
   local prefix = " "
   for _, insn in ipairs(insns) do
      -- New function: prints its name
      if insn_get_fct_name(insn) ~= func then
         func = insn_get_fct_name(insn)
         print("<"..func..">:")
      end
      local insn_loop = insn:get_block():get_loop()
      if insn_loop == nil then
         -- Instruction is not in a loop: resetting loop id and indentation
         loop = ""
         prefix = " "
      elseif insn_loop:get_id() ~= loop then
         -- New loop: updating loop id and indentation
         loop = insn:get_block():get_loop():get_id()
         local indent
         if config["show-hierarchy"] == "on" then
            indent = string.rep(" ", 2*(insn_loop:get_depth() + 1))
         else
            indent = " "
         end
         -- Printing loop id
         prefix = indent.."("..loop..")"
      end
      local insn_str = string.format(prefix.."%#x", insn:get_address())..string.gsub(insn:tostring(), "Insn", "")
      if config["show-extra-info"] == "on" then
         -- Source info requested: adding it after the instruction
         local insn_src_file = insn:get_src_file_path()
         local insn_src_line = insn:get_src_line()
         if insn_src_file ~= nil and insn_src_file ~= "" then
            insn_str = insn_str.."\t[src: "..insn_src_file..":"..insn_src_line.."]"
         end
      end
      print(insn_str)
   end
end