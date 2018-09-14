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

-- Force block relocation
function force_block_relocation(patch,block)
   local m = patch.madras
   local i = block:get_first_insn()
   if i ~= nil then
      m:relocate_insn(i:get_address())
   end
end

-- Force loop relocation
function force_loop_relocation(patch,loop)
   for b in loop:blocks() do
      patcher.force_block_relocation(patch,b)
   end
end


-- Perform a virtual insertion between two blocks
-- The effective insertion will be done at commit
--
-- Patcher -> Block -> Block -> (Addr -> Pos -> IO Modif) -> IO ()
function insert_between(patch,source,target,f)

   -- LUA distinguishes between block objects referencing the same C block...
   -- Hence we need to loop to find the block, instead of directly indexing
   -- with the block

   local b1 = nil
   local b2 = nil

   for s,bs in pairs(patch.between) do
      if s:get_id() == source:get_id() then
         b1 = s
         for t,_ in pairs(bs) do
            if t:get_id() == target:get_id() then
               b2 = t
            end
         end
      end
   end

   if b1 == nil then
      b1 = source
   end

   if b2 == nil then
      b2 = target
   end

   if patch.between[b1] == nil then
      patch.between[b1] = {}
   end

   if patch.between[b1][b2] == nil then
      patch.between[b1][b2] = {}
   end

   table.insert(patch.between[b1][b2], f)
end

-- Perform insertions between a block and its successors
-- Patcher -> Block -> [(Block,Addr->Pos->IO Modif)] -> IO ()
function insert_between_effective(patch,source,targets)
   local m = patch.madras

   local succ_natural = nil
   local succ_jmp = nil
   local succ_jcc = nil

   -- Check that blocks are related and get successors
   for target,_ in pairs(targets) do
      local link = source:get_link_type(target)
      if link == nil or link.id == 0 then
         Message:critical("Cannot insert code between two unrelated blocks")
      end

      if link.id == 1 then
         succ_natural = target
      elseif link.id == 2 then
         succ_jmp = target
      elseif link.id == 3 then
         succ_jcc = target
      end
   end
      
   if succ_natural ~= nil and succ_jmp == nil and succ_jcc == nil then
      -- We insert at the end of the block
      local addr = source:get_last_insn():get_address()
      local f = targets[succ_natural]
      f(addr,1)

   elseif succ_natural == nil and succ_jmp ~= nil and succ_jcc == nil then
      -- We insert before the jump
      local addr = source:get_last_insn():get_address()
      local f = targets[succ_jmp]
      f(addr,0)

   elseif succ_natural == nil and succ_jmp == nil and succ_jcc ~= nil then
      -- We only patch the conditional jump link
      local jcc = source:get_last_insn()
      local addr = jcc:get_address()
      local jncc,cond = m:get_oppositebranch(0, jcc)
      local f = targets[succ_jcc]

      if jncc ~= nil then
         -- We transform 
         --    Jcc X
         -- Z:
         -- into
         --    Jncc Z
         --    inserted code
         --    JMP X
         -- Z:
         local x = jcc:get_branch_target():get_address()
         local z = jcc:get_next():get_address()
         m:linkbranch_toaddr(jncc,z)
         m:add_insn(jncc, addr, patcher.InsertBefore)
         local modif = f(addr,patcher.InsertBefore)
         m:delete_insns(1, addr)
         m:modif_setnext(modif,nil,x)
      elseif cond ~= nil then
         -- We add conditional instructions before the jump
         local modif = f(addr,0)
         m:modif_addcond(modif,cond,0)
      else
         Message:critical("Cannot instrument conditional jump")
      end

   elseif succ_natural ~= nil and succ_jmp == nil and succ_jcc ~= nil then
      -- We patch both the conditional jump link and the natural flow link
      local jcc = source:get_last_insn()
      local addr = jcc:get_address()
      local jncc,cond = m:get_oppositebranch(0, jcc)
      local fjcc = targets[succ_jcc]
      local fnat = targets[succ_natural]

      if jncc ~= nil then
         -- We transform 
         --    Jcc X
         -- Z:
         -- into
         --    Jncc K
         --    inserted code fjcc
         -- K: Jcc X
         --    insert code fnat
         -- Z:
         local x = jcc:get_branch_target():get_address()
         local z = jcc:get_next():get_address()
         -- insert (reverse) conditional code
         m:linkbranch_toaddr(jncc,addr)
         m:add_insn(jncc, addr, patcher.InsertBefore)
         local modif = fjcc(addr,patcher.InsertBefore)
         -- insert natural flow code
         fnat(addr,patcher.InsertAfter)
      else
         Message:critical("Cannot instrument conditional jump")
      end
   else
      Message:critical("Cannot instrument link between blocks")
   end
end

-- Insert code between two blocks
function insert_code_between(patch,source,target,gvars,code)
   local f = function(addr,pos)
      return patcher.insert_asm(patch,addr,pos,gvars,code)
   end
   return insert_between(patch,source,target,f)
end


