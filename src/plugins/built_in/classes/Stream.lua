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

---@class Table Stream

Stream = {};
local StreamMeta = {};

Stream.register_families = {
  IP   = {"IP","EIP","RIP"},
  A    = {"AH","AL","AX","EAX","RAX"},
  B    = {"BH","BL","BX","EBX","RBX"},
  C    = {"CH","CL","CX","ECX","RCX"},
  D    = {"DH","DL","DX","EDX","RDX"},
  SP   = {"SPL","SP","ESP","RSP"},
  SI   = {"SIL","SI","ESI","RSI"},
  DI   = {"DIL","DI","EDI","RDI"},
  BP   = {"BPL","BP","EBP","RBP"},
  R8   = {"R8B","R8W","R8D","R8"},
  R9   = {"R9B","R9W","R9D","R9"},
  R10  = {"R10B","R10W","R10D","R10"},
  R11  = {"R11B","R11W","R11D","R11"},
  R12  = {"R12B","R12W","R12D","R12"},
  R13  = {"R13B","R13W","R13D","R13"},
  R14  = {"R14B","R14W","R14D","R14"},
  R15  = {"R15B","R15W","R15D","R15"},
  xMM0 = {"XMM0","YMM0","ZMM0"},
  xMM1 = {"XMM1","YMM1","ZMM1"},
  xMM2 = {"XMM2","YMM2","ZMM2"},
  xMM3 = {"XMM3","YMM3","ZMM3"},
  xMM4 = {"XMM4","YMM4","ZMM4"},
  xMM5 = {"XMM5","YMM5","ZMM5"},
  xMM6 = {"XMM6","YMM6","ZMM6"},
  xMM7 = {"XMM7","YMM7","ZMM7"},
  xMM8 = {"XMM8","YMM8","ZMM8"},
  xMM9 = {"XMM9","YMM9","ZMM9"},
  xMM10 = {"XMM10","YMM10","ZMM10"},
  xMM11 = {"XMM11","YMM11","ZMM11"},
  xMM12 = {"XMM12","YMM12","ZMM12"},
  xMM13 = {"XMM13","YMM13","ZMM13"},
  xMM14 = {"XMM14","YMM14","ZMM14"},
  xMM15 = {"XMM15","YMM15","ZMM15"}
}
Stream.register_families_rev = {};

function Stream:new(stream_table,parent,insns)
   local obj = {};

   setmetatable(obj,StreamMeta);
   obj.__data   = stream_table;
   obj.__parent = parent;
   Stream:__update_pattern(stream_table);
   Stream:__compute_unroll(stream_table); 
   Stream:__compute_stride(obj.__parent, obj.__data,insns);
   
   return obj;
end

function Stream:tostring()
   Table:tostring(self.__data);
   return "";
end

function Stream.__iter(data,position)
   position = position + 1;
   local value = data[position];
   if value then
      return position, value;
   end 
end

function Stream:instructions(data)
   if(data == nil) then
      data = self.__data.insns;
   end

   return self.__iter, data, 0;
end

function Stream:get_unroll()
   return self.__data.info;
end

--TODO this function should move to loop to avoid multiple calls
function Stream:__compute_stride(parent,data,insns)
   local stride;
   --stride is only meaningful if parent object is a loop
   if(parent ~= nil) then
      local l = parent;
      local meta = getmetatable(l);
      if(meta._NAME == "loop") then
         local inducs, invars;
         if (insns == nil) then
            inducs, invars = l:get_inductions();
         else
            inducs, invars = l:get_inductions_from_insnlist(insns);
         end
         local info  = data.info;
         local ival, bi, invar;
         local fbase  = self.register_families_rev[info.base]
         local findex = self.register_families_rev[info.index]; 
         local is_base_inv, is_index_inv;

	     --Verify if stream is invariant
         if(invars.reg[info.base] ~= nil) then is_base_inv = true; end
         if(invars.reg[info.index] ~= nil) then is_index_inv = true; end

         if(info.base ~= nil and info.index ~= nil) then
            if(is_base_inv == true and is_index_inv == true) then
              --print("reg ",info.base, "is invariant")
              --print("reg ",info.index, "is invariant")
              invar = true;
            end
         elseif(info.base ~= nil) then
            if(is_base_inv == true) then
               invar = true;
            end            
         else--info.index ~= nil
            if(is_index_inv == true) then
               invar = true;
            end            
         end   
        if(invar == true) then
              info.access_type = "constant";
              return;
         end 
         --Verify if stream is driven by inductions
         --We consider that only the base or the index can change exclusively
         --hence stopping reg search after matching at least base or index
         --TODO look for cases where both base and index have an associated induction
        for reg,val in pairs(inducs) do
           local freg   = self.register_families_rev[reg];
           --print("induc,finduc,fbase,findex",reg,freg,fbase,findex);
           if(freg ~= nil and (freg == fbase or freg == findex)) then
              info.access_type = "strided";
              --Don't set stride info if progression is unknown
              if(val.valtype == 'Invar' or val.val == nil) then return end;
              ival = math.abs(val.val);
              if(freg == fbase) then
                 bi = "base";
              end
              if(freg == findex) then
                 bi = "index";
              end            
              break;--found match
           end
        end

         if(info.load ~= nil) then
            local bytes = info.load.bytes;
           
            if(bi == "base") then
               info.load.stride = ival / bytes; 
            elseif(bi == "index") then
               info.load.stride = (ival * info.scale) / bytes; 
            end
         end
         if(info.store ~= nil) then
            local bytes = info.store.bytes;
            if(bi == "base") then
               info.store.stride = ival / bytes; 
            elseif(bi == "index") then
               info.store.stride = (ival * info.scale) / bytes; 
            end
         end        
      end
   end
end

function Stream:get_pattern(inorder,compressed,without_digit)
   local p = self.__data.info.pattern;

   if(inorder == true) then
      p = self.__data.info.pattern_inorder;
   end
   if(compressed ~= true) then 
      return p;
   end

   local tcurr,tnext;  
   local similar = 1;
   local pc = "";

   for i = 1, #p do
      local tcurr = p:sub(i,i);
      local tnext = p:sub(i+1,i+1);
      
      if(tcurr == tnext) then
         similar = similar + 1;
      else
         --Print last different token
         if(similar > 1 and without_digit ~= true) then
            pc = pc..tcurr..similar;
         else
            pc = pc..tcurr;
         end
         similar = 1;         
      end
   end   

   return pc;
end

function Stream:__update_pattern(stream)
   if(stream.load ~= nil and #stream.load > 0) then
      stream.info.pattern = stream.info.pattern..string.rep("L",#stream.load);
   end
   if(stream.store ~= nil and #stream.store > 0) then
      stream.info.pattern = stream.info.pattern..string.rep("S",#stream.store);
   end
end

function Stream:__compute_unroll_set(set)
   local iter = 1;
   local icurr,inext;
   local ucontiguous = true;
   local uinorder    = true;
   local ufactor     = 1;
   local nmin        = set[iter].n;
   local nmax        = nmin;
   local bytes       = set[iter].size;
   --Iterate on all instructions and find unroll factor if any
   while(set[iter+1] ~= nil) do
      local icurr = set[iter];
      local inext = set[iter+1];
      
      bytes = bytes + set[iter].size;      
      --offset of 2 consecutive instructions equals prev insn size
      if(inext.offset - icurr.offset ~= icurr.size) then
         unroll_contiguous = false;
      else
         ufactor = ufactor + 1;
      end
      if(inext.n ~= icurr.n + 1) then
         unroll_inorder = false;
      end
      
      if(icurr.n < nmin) then 
         nmin = icurr.n;
      end
      if(icurr.n > nmax) then 
         nmax = icurr.n;
      end      
      iter = iter + 1;
   end  

   return {unroll_contiguous = ucontiguous,
           unroll_inorder = uinorder, 
           unroll_factor = ufactor,
           nmin = nmin,
           nmax = nmax,
           bytes = bytes};
end

function Stream:__compute_unroll(stream)
   local resl,ress;
   local function sort_by_offset(a,b)
      if(a.offset < b.offset) then
         return true;
      else
         return false;
      end
   end
   
   stream.info.intersect = false;
   if(stream.load ~= nil) then
      table.sort(stream.load,sort_by_offset);
      resl = Stream:__compute_unroll_set(stream.load);
      stream.info.load = resl;
      if(stream.load[1].i_ptr:is_FP()) then
         stream.info.load.is_fp = true;
         stream.info.is_fp = true;
      end
      stream.info.unroll_factor = resl.unroll_factor;
   end
   if(stream.store ~= nil) then
      table.sort(stream.store,sort_by_offset);
      ress = Stream:__compute_unroll_set(stream.store);
      stream.info.store = ress;
      --Use first store to berify instruction type
      if(stream.store[1].i_ptr:is_FP()) then
         stream.info.store.is_fp = true;
         stream.info.is_fp = true;
      end
      stream.info.unroll_factor = ress.unroll_factor
   end
   if(resl ~= nil and ress ~= nil) then
      if(resl.unroll_factor == ress.unroll_factor) then
         stream.info.unroll_factor = resl.unroll_factor;
      else
         stream.info.unroll_factor = nil;
      end   
      if(resl.nmax > ress.nmin) then
         stream.info.intersect = true;
      end
      --Check load/store symmetry
      if(stream.load[1].offset == stream.store[1].offset) then
         stream.info.symmetry = true;
      end
      if(stream.load[1].i_ptr:is_FP() and stream.store[1].i_ptr:is_FP()) then
         stream.info.is_fp = true;
      else
         stream.info.is_fp = nil;
      end      
   end
end

--TODO Create/Add this function in the c stub. Temporarily here.
function Stream:__get_oprnd_decomp(oprnd)
    local rslt = {--offset base index scale
        offset = 0,
    }

    for k,sub_op in ipairs(oprnd) do
      if(k == 1 and sub_op.typex == nil) then
          rslt.offset = sub_op.value;
      end
      if(sub_op.typex == "base" or sub_op.typex == "index") then
          base_or_index_found = true;
          if(sub_op.typex == "base") then
            rslt.base  = sub_op.value;
          elseif(sub_op.typex == "index") then
            rslt.index = sub_op.value;
          end
      end
      if(base_or_index_found == true and sub_op.typex == nil) then
          rslt.scale = sub_op.value;
      end
    end
    return rslt;
end

StreamMeta.__tostring = Stream.tostring;
StreamMeta.__index    = Stream;
StreamMeta.name       = "Stream";

--Autogen Stream.register_families_rev
for name,f in pairs(Stream.register_families) do
  Stream.register_families_rev["%"..name] = name;
  for _,r in ipairs(f) do
      Stream.register_families_rev["%"..r] = name;
  end
end
