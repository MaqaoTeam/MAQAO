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

---	@class DAG Class 
--	Directed Acyclic Graph

DAG = {};

---	Creates a Directed AG data structure
--	@return A list sorted according to a topological sorting. 
--	Each node (vertex) contains data (finish time)
function DAG:topo_sort()
	local sort = List:New();
	
	return sort;
end

---	Finds the longest path in a DAG
--	@return The longest path in an adjacency list format
--	Each node (vertex) contains data (finish time)
function DAG:longest_path(graph)
	local lpath = Queue:new();
	local v_degree = Table:new();
	local v_value = Table:new();
	local v_updated_from = Table:new();
	local v_to_process = Stack:new();
	local current_vertex = nil;
	local max_vertex_val = 0;local max_vertex_num = 0;
	local graph_info = getmetatable(graph);
	local bis = Table:new(graph);
	
	--Vertices Degree Table Initialization
	for vertex_num,vertex in pairs(graph)
	do
		v_degree[vertex_num] = vertex["in"]:get_size();
		v_value[vertex_num] = 0;
	end	
	--Modified BFS from each root
	for root in pairs(graph_info["roots"])
	do
		v_to_process:push(root);
	end
	while(not v_to_process:is_empty())
	do
		current_vertex = v_to_process:pop();
		--print("current_vertex = "..current_vertex);
		for next_vertex,edge_weight in pairs(graph[current_vertex]["out"])
		do
			local v1 = v_value[next_vertex];
			local v2 = v_value[current_vertex] + edge_weight;
			if(v1 < v2) then
				v_updated_from[next_vertex] = current_vertex;
				v_value[next_vertex] = v2;
			end
			if(max_vertex_val < v_value[next_vertex]) then
				max_vertex_val = v_value[next_vertex];
				max_vertex_num = next_vertex;
			end
			v_degree[next_vertex] = v_degree[next_vertex] - 1;
			if(v_degree[next_vertex] <= 0) then
				v_to_process:push(next_vertex);
			end 
			--print("edge_weight = "..edge_weight.." and next_vertex = "..next_vertex.." and max_val = "..v_value[next_vertex]);
		end
	end	
	
	local in_vertices = graph[max_vertex_num]["in"]:get_size();
	
	while(in_vertices ~= 0)
	do
		lpath:add_head(max_vertex_num);
		max_vertex_num = v_updated_from[max_vertex_num];
		in_vertices = graph[max_vertex_num]["in"]:get_size();
	end
	--Table:new(graph):tostring();
	lpath:add_head(max_vertex_num);	
	
	print("Vertex "..max_vertex_num.." has the highest finish time : "..max_vertex_val);
	
	--v_value:tostring();
	--v_updated_from:tostring();
	return lpath;
end

---	Finds the shortest path in a DAG
--	@return The shortest path in an adjacency list format
function DAG:shortest_path(graph)
--[[
Graph
//Topological sort
//

--]]
end
