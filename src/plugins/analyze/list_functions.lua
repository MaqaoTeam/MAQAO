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

-- Displays the list of functions
function analyze:list_functions (config)
   local asmfile = config["asmfile"]
   
   print (" Function name (in file)                  | Function name (in source)")
   print ("------------------------------------------+------------------------------------")

   for fct in asmfile:functions() do
      local name    = fct:get_name() or ""
      local demname = fct:get_demname() or ""

      -- print the list of functions
      if analyze:fct_matches(config, fct) then
         local fct_info = " "..name..string.rep(" ", 40 - string.len (name)).." | "..demname
         if config["show-extra-info"] == "on" then
            local sstart, sstop = fct:get_src_lines()
            local src_file_path = fct:get_src_file_path();
            if (src_file_path ~= "" and src_file_path ~= nil) then
               fct_info = fct_info.."  [src: "..src_file_path..":"..sstart.."-"..sstop.."]"
            end
         end
         print(fct_info)
      end
   end
   print ("------------------------------------------+------------------------------------")
end
