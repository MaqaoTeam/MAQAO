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

-- Prints a loop
local function print_loop (config, loop, indent, fct_info, fct_info_printed)

   -- Checks that the loop is not filtered out
   if analyze:loop_matches(config, loop) == false then
      return fct_info_printed
   end

   local loop_details = string.rep(" ", 41).." | "..string.rep(" ", indent)..loop:get_id ().."  [depth = "..loop:get_depth().."]"

   if config["show-extra-info"] == "on" then
      local sstart, sstop = loop:get_src_lines()
      local astart, astop = loop:get_asm_addresses()
      local src_file_path = loop:get_src_file_path();

      if (src_file_path ~= "" and src_file_path ~= nil) then
         loop_details = loop_details.."  [src: "..src_file_path..":"..sstart.."-"..sstop.."]"
      end
      loop_details = loop_details.."  ["..string.format("asm: 0x%x;0x%x", astart, astop).."]"
   end
   if fct_info_printed == false then
      print(fct_info)
      fct_info_printed = true
   end
   print(loop_details)
   return fct_info_printed
end

-- Prints the children loops of a given loop
local function print_loop_children (config, parent_loop, fct_info, fct_info_printed)

   fct_info_printed = print_loop(config, parent_loop, parent_loop:get_depth(), fct_info, fct_info_printed)
   
   for loop in parent_loop:children() do
      if (loop:get_depth() == (parent_loop:get_depth() + 1)) then
         fct_info_printed = print_loop_children(config, loop, fct_info, fct_info_printed)
      end
   end
   
   return fct_info_printed
end

-- Displays the list of loops
function analyze:list_loops (config)

   local asmfile = config["asmfile"]

   print (" Function name (in file)                  | Loops")
   print ("------------------------------------------+--------------------------------")

   for fct in asmfile:functions() do
      local name    = fct:get_name() or ""
      local demname = fct:get_demname() or ""

      -- print the list of functions
      if analyze:fct_matches(config, fct) then
         local fct_info = " "..name..string.rep(" ", 40 - string.len (name)).." |"
         local fct_info_printed = false
         if config["show-empty-functions"] == "on" then
            print(fct_info)
            fct_info_printed = true
         end

         if config["show-hierarchy"] == "on" then
            -- Printing the list of loops
            for loop in fct:loops () do
               if (loop:is_outermost ()) then
                  fct_info_printed = print_loop_children(config, loop, fct_info, fct_info_printed)
               end
            end
         else
            for loop in fct:loops () do
               fct_info_printed = print_loop(config, loop, 0, fct_info, fct_info_printed)
            end
         end
      end
   end
   print ("------------------------------------------+--------------------------------")
end
