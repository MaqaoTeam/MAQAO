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

--- Module cqa.stream
-- Defines functions to compute, return or print stream info for a loop
module ("cqa.api.stream", package.seeall)

local insert = table.insert

function get_streams_by_stride (crp, path_streams)
   local ret = { stride_0 = {},
                 stride_1 = {},
                 stride_n = {},
                 unknown_stride = {},
                 indirect = {} }

   -- for each stream detected in this current loop path
   for _,stream in ipairs (path_streams) do
      local unroll_info = stream.__data.info
      if (unroll_info.access_type == "constant" or unroll_info.base == "%RIP") then
         insert (ret.stride_0, stream)

      elseif (unroll_info.access_type == "strided") then
         local load_stride
         local store_stride
         if (type (unroll_info.load) == "table") then load_stride = unroll_info.load.stride  end
         if (type (unroll_info.store)== "table") then store_stride= unroll_info.store.stride end

         local stride
         if (load_stride == nil) then
            stride = store_stride
         elseif (store_stride == nil) then
            stride = load_stride
         elseif (load_stride == store_stride) then
            stride = load_stride
         end

         if (stride == 1) then
            insert (ret.stride_1, stream)

         elseif (stride ~= nil and stride ~= 0) then
            insert (ret.stride_n, stream)

         else
            insert (ret.unknown_stride, stream)
         end

      else -- indirect or unknown access type
         -- TODO: workarounds a bug preventing streams from detecting some constant stride
         local function is_constant (reg_name)
            if (reg_name == nil) then return true end

            local reg_val = string.match (reg_name, "%%(.*)")

            for _,insn in ipairs (crp.insns) do
               -- for each register written by the current instruction
               for _,oprnd in pairs (insn:get_operands()) do
                  if (oprnd.value == reg_val and oprnd.write == true) then
                     return false
                  end
               end
            end

            return true
         end

         if (is_constant (unroll_info.base) and is_constant (unroll_info.index)) then
            insert (ret.stride_0, stream)
         else
            insert (ret.indirect, stream)
         end
      end
   end

   return ret
end
