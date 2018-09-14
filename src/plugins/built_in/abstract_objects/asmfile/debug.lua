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

module("asmfile.debug",package.seeall)

--- Compute function start source line and function stop source line
-- @return a table whose key is function ids and values are subtable
-- with the following structure:
-- <ul>
--   <li>start: Function start source line</li>
--   <li>stop: Function stop source line. -1 if the value can be computed 
--             (last function of the source file)</li>
--   <li>fct: The function</li>
--   <li>file: Function source file string</li>
-- </ul>
-- @warning Only functions with debug data are saved into the table
function asmfile:get_function_debug ()
   local ret = Table:new ()
   local insn = nil;
   local srcline = nil;
   
   -- list all functions whose debug data are available
   for fct in self:functions () do
      insn = fct:get_first_insn ()
      srcline = insn:get_src_line ()
      srcfile = insn:get_src_file_path ()
      
      if srcline > 0 then
         ret:insert ({start=srcline, stop=0, fct=fct, file=srcfile})    
      end
   end
   
   -- sort the function
   table.sort(ret, function (a, b) return a["start"] < b["start"] end)
   
   -- compute "stop" field. -1 means that no end has been found (=> last element)
   local size = table.getn(ret)
   local off = 1
   for i,_ in pairs (ret) do
      off = 1
      while i + off <= size and ret[i + off].file ~= ret[i].file do
         off = off + 1
      end
      
      if i + off <= size then
         ret[i].stop = ret[i + off].start - 1
      else
         ret[i].stop = -1
      end 
   end
   
   -- create a new table with function as key and {start, stop, file, fct} as value
   local ret2 = Table:new()
   for i,entry in pairs (ret) do
      ret2[entry.fct:get_id(entry.fct)] = ({start=entry.start, stop=entry.stop, file=entry.file, fct=entry.fct})
   end
   return ret2
end
