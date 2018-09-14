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

--- Module cqa.innermost_loops
-- Defines the get_innermost_loops function which returns innermost loops in a function
-- For the k1om architecture, ignore loops infered by GATHER/SCATTER instructions
module ("cqa.api.innermost_loops", package.seeall)

require "cqa.api.k1om.unroll";

--- Returns innermost loops in the 'fct' function, assuming an architecture
-- @param fct function
-- @return table of loops
function get_innermost_loops (fct)
   local function cmp_loops (a, b)
      local _,a_srcl = a:get_src_lines();
      local _,b_srcl = b:get_src_lines();

      if (a_srcl == b_srcl) then
         local a_addr = a:get_first_entry():get_first_insn():get_address();
         local b_addr = b:get_first_entry():get_first_insn():get_address();

         return a_addr < b_addr;
      end

      return a_srcl < b_srcl;
   end

   local loops = {};

   -- Fill the loops table with innermost loops
   if (fct:get_asmfile():get_arch_name() ~= "k1om") then
      for l in fct:innermost_loops() do
         table.insert (loops, l);
      end

   else
      local already_inserted = {};

      for l in fct:innermost_loops() do
         -- In k1om, gather/scatter instructions are coupled with a mask conditional
         -- branch, creating an innermost loop. To ignore it, we take the parent loop
         if (cqa.api.k1om.unroll.is_k1om_gather_scatter (l)) then
            local parent_loop = l:get_parent();

            if (parent_loop ~= nil) then
               local parent_loop_id = parent_loop:get_id();

               -- parent_loop can contain more than one gather/scatter instruction
               -- Prevent multiple insertion
               if (already_inserted [parent_loop_id] == nil) then
                  table.insert (loops, parent_loop);
                  already_inserted [parent_loop_id] = true;
               end
            end
         else
            table.insert (loops, l);
         end
      end
   end

   -- Sort loops by increasing end source line and then by increasing address
   table.sort (loops, cmp_loops);

   return loops;
end
