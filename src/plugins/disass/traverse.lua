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

require "disass.registers"

local function get_loops(insn)
   local loops = {}
   local loop = insn:get_loop()
   while loop ~= nil do
      table.insert(loops, 1, loop)
      loop = loop:get_parent()
   end
   return loops
end

function fct_traverse(asmfile,fct,callbacks,start,stop)

   -- Instruction iterator
   local insn_iter = function ()
      -- Some blocks of the function may not be contiguous (especially blocks
      -- added during instrumentation). Hence we find these blocks and traverse
      -- them
      local visited_blocks = {}

      -- Return true if the given instruction is valid
      local is_valid = function (i)
         return not (i == nil
            or (start ~= nil and i:get_address() < start) -- Check that i is in start/stop range
            or (stop ~= nil and i:get_address() > stop)
            or (i:get_function() == nil or i:get_function():get_id() ~= fct:get_id()) -- Check that i belongs to the function
            )
      end

      -- Return next valid insn
      local get_next = function (i)
         
         -- Very next instruction
         if is_valid(i:get_next()) then
            return i:get_next()
         end

         -- Next non contiguous block
         for b in fct:blocks() do
            if not visited_blocks[b:get_id()] then
               return b:get_first_insn()
            end
         end

         return nil
      end

      -- Return next valid instruction
      local get_next_valid = function (i)
         local insn = i
         while (insn ~= nil and not is_valid(insn)) do
            insn = get_next(insn)
         end
         return insn
      end

      local insn = get_next_valid(fct:get_first_insn())

      return function () 
         local ret = insn
         if insn ~= nil then
            visited_blocks[ret:get_block():get_id()] = true
            insn = get_next_valid(get_next(insn))
         end
         return ret
      end
   end

   local insns = insn_iter()

   local insn = insns()

   local prev_inlining_fn,prev_inlining_order = nil,nil
   local inlined_fn,inlined_order = insn_inlined_from(insn)
   local prev_loops,loops = {}, get_loops(insn)
   local prev_block,block = nil, insn:get_block()
   local prev_src_file, src_file = nil, insn:get_src_file_path()
   local prev_src_line, src_line = nil, insn:get_src_line()
   local registers = {}

   if callbacks["function_begin"] then
      callbacks["function_begin"](fct,insn)
   end

   while insn ~= nil do

      -- Inlining
      if callbacks["inlining_begin"] then
         if inlining_fn ~= nil and
            (prev_inlining_fn == nil or
             prev_inlining_fn:get_id() ~= inlining_fn:get_id() or
             prev_inlining_order ~= inlining_order) then
            callbacks["inlining_begin"](inlining_fn,insn)
         end
      end

      -- Loop
      local is_begin_loop = false
      for i=1,#loops do
         if prev_loops[i] == nil or prev_loops[i]:get_id() ~= loops[i]:get_id() then
            if callbacks["loop_begin"] then
               callbacks["loop_begin"](loops[i], insn)
            end
            is_begin_loop = true
         end
      end

      -- Block
      local is_begin_block = prev_block == nil or prev_block:get_id() ~= block:get_id()
      if callbacks["block_begin"] then
         if is_begin_block then
            callbacks["block_begin"](block,insn,is_begin_loop)
         end
      end

      -- Line
      if callbacks["line_begin"] then
         if src_file~= -1 and (prev_src_file ~= src_file or prev_src_line ~= src_line) then
            callbacks["line_begin"](src_file,src_line,insn)
         end
      end

      -- Register values
      if is_begin_block then
         registers = {}
      end
      update_registers(asmfile,registers,insn)

      -- Instruction
      if callbacks["insn"] then
         callbacks["insn"](insn,registers)
      end

      if insn:is_call() then
         registers = {}
      end

      ------- Lookahead -------
      insn = insns()

      local next_src_file = -1
      local next_src_line = -1
      local next_block = nil
      local next_loops = {}
      local next_inlining_fn,next_inlining_order = insn_inlined_from(insn) 
      if insn ~= nil then
         next_src_file = insn:get_src_file_path()
         next_src_line = insn:get_src_line()
         next_block = insn:get_block()
         next_loops = get_loops(insn)
      end

      -- Line
      if callbacks["line_end"] then
         if src_file~= -1 and (next_src_file ~= src_file or next_src_line ~= src_line) then
            callbacks["line_end"](src_file,src_line,insn)
         end
      end

      -- Block
      if callbacks["block_end"] then
         if next_block == nil or next_block:get_id() ~= block:get_id() then
            callbacks["block_end"](block,insn)
         end
      end
      
      -- Loop
      if callbacks["loop_end"] then
         for i=#loops,1,-1 do
            if next_loops[i] == nil or next_loops[i]:get_id() ~= loops[i]:get_id() then
               callbacks["loop_end"](loops[i], insn)
            end
         end
      end

      -- Inlining
      if callbacks["inlining_end"] then
         if inlining_fn ~= nil and
            (next_inlining_fn == nil or
             next_inlining_fn:get_id() ~= inlining_fn:get_id() or
             next_inlining_order ~= inlining_order) then
            callbacks["inlining_end"](inlining_fn, insn)
         end
      end

      ----- Reuse already computed lookead values -----
      
      -- Line
      prev_src_file = src_file
      prev_src_line = src_line
      src_file = next_src_file
      src_line = next_src_line
      
      -- Block
      prev_block = block
      block = next_block

      -- Loop
      prev_loops = loops
      loops = next_loops

      -- Inlining
      prev_inlining_fn = inlining_fn
      prev_inlining_order = inlining_order

      inlining_fn = next_inlining_fn
      inlining_order = next_inlining_order
   end

   if callbacks["function_end"] then
      callbacks["function_end"](fct,insn)
   end
end
