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

function trim(s)
   return s:match'^%s*(.*%S)' or ''
end

function insn_inlined_from(insn)
   if insn == nil then
      return nil,0
   end

   if insn:get_asmfile() == nil then
      return nil,0
   end

   local addr = insn:get_address()

   for fct in insn:get_asmfile():functions() do
      if fct:get_id() ~= insn:get_function():get_id() then
         for k,range in pairs(fct:get_debug_ranges()) do
            if addr >= range.start and addr <= range.stop then
               return fct,k
            end
         end
      end
   end
   
   return nil,0
end

--- Indicate if an instruction comes from an inlining
function insn_is_inlined(insn)
   local orig,k = insn_inlined_from(insn)

   return (orig ~= nil)
end

function fct_set_insert(xs,x)
   if xs[x:get_name()] == nil then
      xs[x:get_name()] = x
   end
end

function fct_expected_name(config,fct)
   local fct_name = fct:get_name()
   if config.demangle and fct:get_demname() ~= nil then
      fct_name = fct:get_demname()
   end
   return fct_name
end

--- Try to find the best block label (using symbols, etc.)
function block_label_get(config,b)
   local label = string.format("%d", b:get_id())

   if b:get_function() and b:get_function():get_first_insn() 
      and b:get_function():get_first_insn():get_address() == b:get_first_insn():get_address() then
      label = string.format("%d (function %s)", b:get_id(), b:get_function():get_name())
   elseif b:is_loop_entry() then
      label = string.format("%d (loop %d)", b:get_id(), b:get_loop():get_id())
   end

   return label
end
