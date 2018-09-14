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

---	@class Graph Class

Graph = {};
Graph.ROOT 		  = 0x01;
Graph.PSEUDO_ROOT = 0x02;
GraphMeta = {};
--GraphMeta.__tostring= Graph.tostring;
GraphMeta.__index 	= Graph;
GraphMeta.name			= "Graph";

---	Creates a list data structure
--	@return New instance
function Graph:new(data)
	local graph  = {};
	graph.nodes = Table:new();
	graph.roots = Table:new();
	graph.leaves = Table:new();
	setmetatable(graph,GraphMeta);
	return graph;
end

function Graph:add_node(id,data,force)
	if(self.nodes[id] == nil or force == true) then
		self.nodes[id] = nil;
		self.nodes[id] = Table:new();
		self.nodes[id]["data"] = data;
		self.nodes[id]["in"]   = Table:new(); 
		self.nodes[id]["out"]  = Table:new();	
	else	
		self.nodes[id]["data"] = data;
	end
end

function Graph:set_node_root(id)
	if(self.nodes[id] ~= nil) then
		self.roots[id] = self.nodes[id];
	end
end

function Graph:set_node_leaf(id)
	if(self.nodes[id] ~= nil) then
		self.leaves[id] = self.nodes[id];
	end
end

function Graph:add_edge(srcid,destid,data)

	if(self.nodes[srcid] == nil) then
		self:add_node(srcid,nil);
	end
	if(self.nodes[destid] == nil) then
		self:add_node(destid,nil);
	end
	if(self.nodes[srcid]["out"] ~= nil) then
		self.nodes[srcid]["out"][destid] = data;
	end
	if(self.nodes[destid]["in"] ~= nil) then
		self.nodes[destid]["in"][srcid] = data;
	end
end

function Graph:bfs(nodeid,fct,userdata,__visit)
	local queue = Queue:new();
	local tmp_nodeid;
	local node;

	if(__visit == nil) then
			__visit = Table:new();
	end
	queue:add_tail(nodeid);
	
	while(queue:length() > 0) do
		tmp_nodeid = queue:del_head();
		node = self.nodes[tmp_nodeid];
		if(node["data"] == nil) then
			print("WARNING : Node "..nodeid.." has no data information");
		end			
		fct(node,userdata);
		
		for vid in pairs(node["out"]) do
			--not visited yet
			if(not __visit[vid] == true) then
				__visit[vid] = true;
				queue:add_tail(vid);
			end
		end
	end
end

function Graph:backbfs(nodeid,fct,userdata,__visit)
	local queue = Queue:new();
	local tmp_nodeid;
	local node;

	if(__visit == nil) then
			__visit = Table:new();
	end
	if(nodeid == nil) then
		for leafid in pairs(self.leaves) do
			queue:add_tail(leafid);
		end				
	else	
		queue:add_tail(nodeid);
	end
	
	while(queue:length() > 0) do
		tmp_nodeid = queue:del_head();
		node = self.nodes[tmp_nodeid];
		if(node["data"] == nil) then
			print("WARNING : Node "..nodeid.." has no data information");
		end			
		fct(node,userdata);

		for vid in pairs(node["in"]) do
			--not visited yet
			if(not __visit[vid] == true) then
				__visit[vid] = true;
				queue:add_tail(vid);
			end
		end	
	end
end

function Graph:dfs(nodeid,fct,userdata,visit)
	local node;

	if(visit == nil) then
		visit = Table:new();
	end
	visit[nodeid] = true;
	node = self.nodes[nodeid];
	
	if(node["data"] == nil) then
		print("WARNING : Node "..nodeid.." has no data information");
	end
	fct(node,userdata);

	for vid in pairs(node["out"])	do
		if(not visit[vid] == true) then			
			self:dfs(vid,fct,nil,visit);
		end
	end
end

function Graph:backdfs(nodeid,fct,userdata,visit)
	local node;

	if(visit == nil) then
		visit = Table:new();
	end
	visit[nodeid] = true;
	node = self.nodes[nodeid];
	if(node["data"] == nil) then
		print("WARNING : Node "..nodeid.." has no data information");
	end
	fct(node,userdata);

	for vid in pairs(node["in"]) do
		if(not visit[vid] == true) then			
			self:backdfs(vid,fct,nil,visit);
		end
	end	
end

function Graph:longest_path(graph)
	for root,root_type in pairs(graph["roots"])
	do
		if(root_type == Graph.ROOT) then
			--modified dfs
			graph[vertex]["visited"] = visited;
			for _,vertex_out in pairs(graph[vertex]["out"])
			do
				if(graph[vertex_out]["visited"] == nil or graph[vertex_out]["visited"] ~= visited) then
					print("Visiting vertex "..vertex_out);
					self:dfs_traverse(graph,vertex_out,visited);
				end
			end
			
		end
	end	
end
