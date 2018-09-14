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

--- Module cqa.src_loop_grouping
-- Defines the cqa:get_fct_src_loops function returning source loops in a given function
module ("cqa.api.src_loop_grouping", package.seeall)

require "cqa.consts"
require "cqa.api.innermost_loops"

-- Merges source loops whose line number differs from only one.
-- To merge two source loops a and b, (i) the identifiers of binary loops of b
-- are appended to a and (ii) b is removed
local function merge_source_loops (src_loops, min_lines)
   local can_merge = {};

   -- Merges source loops that can merge
   local function merge ()
      for _,v in pairs (can_merge) do
         local l1 = src_loops [v[1]];
         local l2 = src_loops [v[2]];

         -- Merging a loop that were already merged seems too dangerous
         if (l1 ~= nil and l2 ~= nil) then
            Message:info (string.format ("Assuming lines %d and %d correspond to the same source loop",
                                         v[1], v[2]));

            -- Copies binary loops IDs from l2 to l1
            for _,id in ipairs (l2 ["loop IDs"]) do
               table.insert (src_loops [v[1]] ["loop IDs"], id);
               for _,bin_loop in ipairs (l2 ["loops"]) do
                  if (bin_loop:get_id() == id) then
                     table.insert (src_loops [v[1]] ["loops"], bin_loop)
                     break
                  end
               end
            end

            -- Removes l2, now merged into l1
            src_loops [v[2]] = nil;
         end
      end
   end

   -- Finds which source loops can be merged

   -- First case: differ from only one line
   for l1 in pairs (src_loops) do
      for l2 in pairs (src_loops) do
         if (math.abs (l2-l1) == 1) then
            table.insert (can_merge, {l1, l2});
            break;
         end
      end
   end

   if (cqa.consts.LOOP_MERGE_DISTANCE == 1) then
      merge ();
      return;
   end

   -- Second case: differ from 2 to cqa.consts.LOOP_MERGE_DISTANCE lines but,
   -- for all binary loops, has the same min and CMP line
   -- This case was seen in icc 12.x
   for l1,v1 in pairs (src_loops) do
      for l2,v2 in pairs (src_loops) do
         if (l2-l1 >= 2 and l2-l1 <= cqa.consts.LOOP_MERGE_DISTANCE) then
            local first_min_line;

            -- Concatenate binary loops of l1 and l2
            local ID_list = {};
            for _,v in ipairs (v1 ["loop IDs"]) do table.insert (ID_list, v) end
            for _,v in ipairs (v2 ["loop IDs"]) do table.insert (ID_list, v) end

            -- for each binary loop of l1 and l2
            for _,id in ipairs (ID_list) do
               local min_line = min_lines [id];
               if (first_min_line == nil) then
                  first_min_line = min_line;
               elseif (first_min_line == min_line) then
                  table.insert (can_merge, {l1, l2});
                  break;
               end
            end
         end
      end
   end

   merge ();
end

--- Groups binary loops by their last source line
-- @param loops binary loops to group
-- @return source loops grouped by their last source line
-- @return binary loops (list of identifiers)
local function group_loops_by_src_lines (loops)
   local src_loops = {}; -- source loops
   local bin_loops = {}; -- binary loops

   local min_lines = {}; -- source lines for each binary loop, needed by merge_source_loops

   for _,l in pairs (loops) do
      local id = l:get_id();

      local min, max = l:get_src_lines();
      min_lines [id] = min;

      if (max > 0) then
         -- If first loop of a source loop
         if (src_loops [max] == nil) then
            src_loops [max] = {
               ["loop IDs"] = {},
               ["loops"] = {} }
         end

         -- Insert the loop in the corresponding source loop
         table.insert (src_loops [max] ["loop IDs"], id);
         table.insert (src_loops [max] ["loops"], l);
      else
         table.insert (bin_loops, id);
      end -- if max > 0
   end -- for each loop

   if (cqa.consts.LOOP_MERGE_DISTANCE > 0) then
      merge_source_loops (src_loops, min_lines);
   end

   return src_loops, bin_loops;
end

--- Returns source loops for a given function
-- @param fct a function
-- @return source and binary loops, see group_loops_by_src_lines
function get_fct_src_loops (fct)
   local innermost_loops = cqa.api.innermost_loops.get_innermost_loops (fct)
   return group_loops_by_src_lines (innermost_loops)
end
