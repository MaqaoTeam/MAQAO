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

module ("arch", package.seeall)

-- Returns a two-dimensional array containing the names of available architectures and uarch names 
function arch:get_available_uarchs_names()
   local archs = get_archs_list();
   local arch_names = {};
   
   for _, arch in pairs(archs) do
      local uarchs = arch:get_uarchs();
      local uarch_names = {};
      for _, uarch in pairs(uarchs) do
         table.insert(uarch_names, uarch:get_name());      
      end
      arch_names[arch:get_name()] = uarch_names; 
   end
   return arch_names;
end

-- Returns a two-dimensional array containing the names of available architectures and processor names 
function arch:get_available_procs_names()
   local archs = get_archs_list();
   local arch_names = {};
   
   for _, arch in pairs(archs) do
      local uarchs = arch:get_uarchs();
      local proc_names = {};
      for _, uarch in pairs(uarchs) do
         local procs = uarch:get_procs();
         for _, proc in pairs(procs) do
            table.insert(proc_names, proc:get_name());      
         end
      end
      arch_names[arch:get_name()] = proc_names; 
   end
   return arch_names;
end