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

--[[
Consts.OT_REGISTER = 0  --oprnd is a register
Consts.OT_MEMORY = 1  --oprnd is a memory address
Consts.OT_IMMEDIATE = 2  --oprnd is an immediate value
Consts.OT_POINTER = 3  --oprnd is a pointer to another instruction
Consts.OT_MEMORY_RELATIVE = 4  --oprnd is a memory address referencing an address in the file
Consts.OT_IMMEDIATE_ADDRESS = 5  --oprnd is an immediate value representing an address
Consts.OT_REGISTER_INDEXED = 6  --oprnd is an element inside a register
Consts.OT_UNKNOWN = 7
--]] 
module("loop.stream",package.seeall)

local function find_streams(streams,parent,insns)
   local streams_t = Table:new();
   for hkey,stream in pairs(streams) do
      local s = Stream:new(stream,parent,insns);
      streams_t:insert(s);
   end

   return streams_t;
end

local function add_insn (i, i_cpt, streams)
   local i_type = "NA";
   --print(i);
   if (i:is_load()) then 
      i_type = "load";
   elseif (i:is_store()) then
      i_type = "store";
   end

   if ((i_type == "load" or i_type == "store") and i_cpt >= 0) then
      local offset,base,index,scale;
      local base_or_index_found,hkey;
      
      op     = i:get_first_mem_oprnd();
      offset = 0;
      hkey   = "";
      for k,sub_op in ipairs(op.value) do
         if(k == 1 and sub_op.typex == nil) then
            offset = sub_op.value;
         end
         if(sub_op.typex == "base" or sub_op.typex == "index") then
            base_or_index_found = true;
            if(sub_op.typex == "base") then
               base  = sub_op.value;
            elseif(sub_op.typex == "index") then
               index = sub_op.value;
            end
         end
         if(base_or_index_found == true and sub_op.typex == nil) then
            scale = sub_op.value;
         end
      end
      --print("Offset:",offset,"Base:",base,"Index:",index,"Scale:",scale,"Size:",op.size); 
      --Prepare hase key to store stream info
      hkey = hkey.."(";
      if(base ~= nil) then
         hkey = hkey..base..",";
      end
      if(index ~= nil) then
         hkey = hkey..index..",";
      end
      if(scale ~= nil) then
         hkey = hkey..scale;
      end
      hkey = hkey..")";
      --Insert data
      if(streams[hkey] == nil) then
         streams[hkey] = Table:new({insns = Table:new()});
      end
      if (streams[hkey][i_type] == nil) then
         streams[hkey][i_type] = Table:new();
      end
      if (streams[hkey]["info"] == nil) then
         local b,i;
         if(base ~= nil)  then b = '%'..base; end
         if(index ~= nil) then i = '%'..index; end
         streams[hkey]["info"] = Table:new({pattern_inorder = "", pattern = "", 
                                            base = b, index = i, scale = scale});
      end
      streams[hkey].insns:insert(i);
      streams[hkey][i_type]:insert({offset = offset, i_ptr = i, n = i_cpt, size = op.size / 8});
      --Info data
      if(i_type == "load") then
         streams[hkey].info.pattern_inorder = streams[hkey].info.pattern_inorder..'L';
      else
         streams[hkey].info.pattern_inorder = streams[hkey].info.pattern_inorder..'S';
      end
      
   end
end

function loop:get_streams()
   local streams_t = Table:new();
   
   for p in self:paths() do
      local streams    = Table:new();
      local regs_state = Table:new();
      local i_cpt      = 0;
      --print("path ",p);
      for _,b in ipairs(p) do 
         for i in b:instructions() do 
            add_insn (i, i_cpt, streams);
            i_cpt = i_cpt + 1;
         end--of instructions
      end--of blocks()
      streams_t:insert(find_streams(streams,self));
   end--of paths()   
   
   return streams_t;
end

function loop:get_streams_from_insnlist(insns)
   local streams    = Table:new();
   local i_cpt      = 0;

   for _,i in ipairs (insns) do 
      add_insn (i, i_cpt, streams);
      i_cpt = i_cpt + 1;
   end--of instructions
   
   return find_streams(streams,self,insns);
end
