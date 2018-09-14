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

function patcher_init(bin,trace,stack,shift_size)

   if stack == nil then
      stack = MDSAPI.STACK_SHIFT
   end

   if shift_size == nil then
      shift_size = 512
   end

   local p = project.new("a");
   local a = p:load(bin,0);
   local m = madras.new(bin)

   if trace then
      m:traceon()
   else
      m:traceoff()
   end
   
   m:modifs_init(stack,shift_size)

   return
      { project = p
      , binary_path = bin
      , asmfile = a
      , madras = m
      , between = {} -- insertions between blocks
      }
end

function patcher_trace(patch,enable)
   local m = patch.madras
   if enable then
      m:traceon()
   else
      m:traceoff()
   end
end

-- :: Patch -> [Addr -> Pos -> IO Modif] -> Addr -> Pos -> IO Modif
local function fusion_modifs(patch, fs)
   local m = patch.madras

   local retf = function(addr,pos)
      local last = nil
      for _,f in pairs(fs) do
         local modif = f(addr,pos)
         if modif == nil then
            Message:critical("One of the patchs between two blocks returns nil instead of a Madras patch. Please fix this.")
         end
         if last ~= nil then
            m:modif_setnext(last,modif,0)
         end
         last = modif
      end
      return last
   end
   return retf
end

function patcher_commit(patch,outname)
   local m = patch.madras

   -- Perform insertions between blocks
   for b1,bs in pairs(patch.between) do
      -- Fusion functions at the same insertion point
      local nbs = {}
      for b2,fs in pairs(bs) do
         nbs[b2] = fusion_modifs(patch,fs)
      end
      -- Perform insertion
      patcher.insert_between_effective(patch,b1,nbs)
   end

   return m:modifs_commit(outname)
end
