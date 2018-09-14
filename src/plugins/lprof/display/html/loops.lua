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

module("lprof.display.html",package.seeall)

-- lid_to_time = loops_id_to_time[hostname][pid][tid] : predefined => time % for a given lid
--     loops_child = loop_children : predefined => for each loop id provides its children and src info
--     loops_hierarchy_time : defined => cumulated (current + children) time for each loop node

--The goal is to fill data array (loop_nodes, outer_aggregate_time, id) in order to be able to print
--the loop hierarchy as a flattened array (javascript array used for display)
function _l_print_loop_children_html(loops_child,lid_to_time,lid,depth,
                                     depth_withtime,data,loop_level,parent_id,loops_hierarchy_time)
   local has_children = false;
   local loop_time;

   --The algo uses loops_hierarchy_time to prune loop branches that have no time recorded (dynamic/executed loop tree)
   if(loops_hierarchy_time[lid] == nil and
      (tonumber(lid_to_time[lid]) == nil or tonumber(lid_to_time[lid]) == 0))
   then
      return;
   end
   if(loops_hierarchy_time[lid] == 0) then
      return;
   end

   if(type(loops_child[lid].children) == "table" and
      Table.get_size(loops_child[lid].children) > 0) then
      --print(Table.get_size(loops_child[lid].children));
      has_children = true;
   end
   local is_leaf = 'false';

   if(has_children == false) then
      is_leaf = 'true';
   end

   if(lid_to_time[lid] ~= nil) then
      loop_time = tonumber(lid_to_time[lid]);
   else
      loop_time = 0.00;
   end

   depth_withtime = depth_withtime + 1;
   data.loop_nodes:insert({id = data.id, colspan = 0,lid = lid,
                                       src_file = loops_child[lid].src_file,
                                       src_line_start = loops_child[lid].src_line_start,
                                       src_line_end = loops_child[lid].src_line_end,
                                       time = loop_time, level = loop_level,
                                       parent = parent_id, is_leaf = is_leaf, expanded = "false"});
   parent_id = data.id;
   data.id   = data.id + 1;
   data.outer_aggregate_time = data.outer_aggregate_time + loop_time;

   if(has_children == false) then
      return;
   end
   for child_lid in pairs(loops_child[lid].children) do
         if(child_lid ~= lid) then
            _l_print_loop_children_html(loops_child,lid_to_time,child_lid,
                                        depth+1,depth_withtime,data,
                                        loop_level+1,parent_id,loops_hierarchy_time);
         end
   end
end

--The goal is to fill loops_hierarchy_time array which build the actual hierarchy and associated timings
function _l_traverse_loop_children(loops_hierarchy_time,loops_child,lid_to_time,plid,lid,visited)
   local has_children = false;
   local loop_time;

   if(visited[lid] == nil) then
      visited[lid] = true;
   else
      --Message:critical("Detected a cycle in loop hierarchy info. Metadata must be corrupted.");
      return;
   end

   if(loops_child[lid] == nil) then
      Message:warn("Loop "..lid.." has no siblings info.");
   else
      if(type(loops_child[lid].children) == "table" and
         Table.get_size(loops_child[lid].children) > 0) then
         has_children = true;
      end
   end

   if(lid_to_time[lid] ~= nil) then
      loop_time = tonumber(lid_to_time[lid]);
   else
      loop_time = 0.00;
   end

   if(has_children == true) then
      for child_lid in pairs(loops_child[lid].children) do
         _l_traverse_loop_children(loops_hierarchy_time,loops_child,lid_to_time,lid,child_lid,visited);
      end

   end
   --Cumulated parent time = direct children time (loop_time) + cumulated children hierarchy time (loops_hierarchy_time[lid])
   if(loops_hierarchy_time[plid] == nil) then
      loops_hierarchy_time[plid] = loop_time;
   else
      loops_hierarchy_time[plid] = loops_hierarchy_time[plid] + loop_time;
   end
   if(has_children == true) then
      --print('> LID '..lid..' adding '..loop_time..' to '..plid,loops_hierarchy_time[lid]);
      --print('> LID '..lid..' adding '..loop_time..' + '..loops_hierarchy_time[lid]..' to '..plid);
      loops_hierarchy_time[plid] = loops_hierarchy_time[plid] + loops_hierarchy_time[lid];
   else
      --print('LID '..lid..' adding '..loop_time..' to '..plid);
   end
end

function _l_print_hf_loop_children_html(lid,loops_child,hf_loops,data,depth,loop_level,parent_id)
   local has_children = false;
   local median, stddev;
   
   if(type(loops_child[lid].children) == "table" and
      Table.get_size(loops_child[lid].children) > 0) then
      --print(Table.get_size(loops_child[lid].children));
      has_children = true;
   end
   local is_leaf = 'false';

   if(has_children == false) then
      is_leaf = 'true';
   end

   if(hf_loops[lid] ~= nil) then
      median = tonumber(hf_loops[lid].median);
      stddev = tonumber(hf_loops[lid].stddev);
   else
      median = 0.00;
      stddev = 0.00;
   end

   --depth_withtime = depth_withtime + 1;
   data.loop_nodes:insert({id = data.id, colspan = 0,lid = lid,
                           src_file = loops_child[lid].src_file,
                           src_line_start = loops_child[lid].src_line_start,
                           src_line_end = loops_child[lid].src_line_end,
                           median = median, stddev = stddev, 
                           level = loop_level, parent = parent_id, is_leaf = is_leaf, 
                           expanded = "false"});
   parent_id = data.id;
   data.id   = data.id + 1;

   if(has_children == false) then
      return;
   end
   for child_lid in pairs(loops_child[lid].children) do
         if(child_lid ~= lid) then
            _l_print_hf_loop_children_html(child_lid,loops_child,hf_loops,data,
                                           depth+1,loop_level+1,parent_id);
         end
   end    
end
