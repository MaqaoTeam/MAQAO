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

module("fct.debug",package.seeall)

--- Returns the compiler (name + version) used to compile a function
-- @return compiler string
function fct:get_compiler ()
   local comp = self:get_compiler_short ()
   local vers = self:get_compiler_version ()
   
   if comp == nil then
      return nil
   else
      if vers ~= nil then
         local ret = comp.." "..vers
         return ret
      else
	 return comp
      end
   end 
end


--- Compute inlining for a given function
-- @param debug_asm an array returned by asmfile:get_function_debug (). If nil
--                  the function 'get_function_debug' is called into this function
-- @return a table containg inline compuation results. The table has for key
-- inlined functions IDs and for values subtables with the following strucure:
-- <ul>
--   <li>nb: Number of instructions coming from the inlined function</li>
--   <li>percent: percentage of inlined instruction from the inlined function</li>
--   <li>fct: The function</li>
-- </ul>
function fct:get_inline (debug_asm)
   local srcl = nil
   local srcline = nil
   local srcfile = nil
   local res = Table:new()
   local total_insn = nil
   
   -- if debug_asm is nil, comptute it (to avoid because to computation will
   -- be do at each call of this function
   if debug_asm == nil then
      local asmfile = self:get_asmfile ()
      -- in this case, inlining can not be analyzed, so an empty table is returned
      if asmfile == nil then
         return {}
      end
      debug_asm = asmfile:get_function_debug ()
   end
      
   -- iterate over each instruction whose function debug data are available
   -- and check if instruction source line is inside function boundaries
   --print (self:get_id().."   "..self:get_name())
   res = Table:new()
   if debug_asm[self:get_id()] ~= nil then
      for block in self:blocks () do
         for insn in block:instructions () do
            -- Retrieve debug data for current instruction
            srcline = insn:get_src_line ()
            srcfile = insn:get_src_file_path ()
            
            if srcline < debug_asm[self:get_id()]["start"]
            or    (srcline > debug_asm[self:get_id()]["stop"]
               and debug_asm[self:get_id()]["stop"] ~= -1) then
               -- if here, this means the instruction is not in function boundaries
               -- look for the inlined function (it should be in the same source file)
               for i, entry in pairs(debug_asm) do                   
                  if srcline > debug_asm[entry.fct:get_id()]["start"]
                  and srcfile == debug_asm[entry.fct:get_id()]["file"]
                  and    (srcline < debug_asm[entry.fct:get_id()]["stop"]
                     or debug_asm[entry.fct:get_id()]["stop"] == -1) then
                     
                     -- if no entry created for the found function, add it
                     if (res[entry.fct:get_id()] == nil) then
                        res[entry.fct:get_id()] = {nb=0, fct=entry.fct, percent=0}
                     end
                     res[entry.fct:get_id()].nb = res[entry.fct:get_id()].nb + 1
                  end  
               end
            end
         end         
      end
   end
   
   -- at this point, all instructions of the function has been traversed
   -- so percentage of inlined instructions can be computed
   total_insn = self:get_ninsns()
   for i, entry in pairs (res) do
      entry.percent = ((entry.nb * 100) / total_insn)
   end  
   return res
end

function fct:info_summary()
  local func  = self;
  local finsn = func:get_first_insn();
  local linsn;
  local fname = mil:fct_main_attribute(func);  
  local fsrcf = finsn:get_src_file_path();
  local fct_start = finsn:get_src_line();
  local fct_stop  = fct_start;
  local fct_info_summary;

  for b in func:blocks() do
    for insn in b:instructions() do
      if(insn:get_src_line() < fct_start) then
	fct_start = insn:get_src_line();
      end
      if(insn:get_src_line() > fct_stop) then
	fct_stop = insn:get_src_line();
      end												
    end
  end
  if(fsrcf == nil) then
	  fsrcf = "";
  end
  if(fct_start == fct_stop) then
	  fct_info_summary = fname.." [{"..fsrcf.."} {"..fct_start..",0}]";
  else
	  fct_info_summary = fname.." [{"..fsrcf.."} {"..fct_start..",0}-{"..fct_stop..",0}]";
  end

  return fct_info_summary;
end
