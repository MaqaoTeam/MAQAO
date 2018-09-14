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

--- Defines getters for extensions (number of fused uops,
-- latency, reciprocal throughput and uops groups)
module ("insn.ext", package.seeall)

local function get_bypass (insn, key)
   if (__insn_ext_bypass == nil) then return end

   local ieb_entry = __insn_ext_bypass [insn:get_name()];
   if (ieb_entry == nil) then return end

   local ieb_subentry = ieb_entry[key];
   if (ieb_subentry == nil) then
      local dst_oprnd_size;
      for _,oprnd in pairs (insn:get_operands() or nil) do
         if (oprnd.write) then
            if (ieb_entry[oprnd.size] ~= nil) then
               ieb_rcpt_entry = ieb_entry[oprnd.size][key];
            end

            break;
         end
      end
   end

   if (ieb_subentry ~= nil) then
      return ieb_subentry;
   end
end

--- Get the minimum and maximum number of fused micro-operations of an instruction
-- @param dispatch The dispatch of an instruction (provided for performances reasons)
-- @return minimum number of fused uops
-- @return maximum number of fused uops
function insn:get_nb_fused_uops (dispatch, uarch)
   local t = get_bypass (self, "nb_uops");
   if     (type (t) == "table" ) then return t.min, t.max;
   elseif (type (t) == "number") then return t, t;
   end

   if (dispatch == nil) then dispatch = self:get_dispatch() end
   if (dispatch == nil) then return end

   local min = dispatch ["nb_uops"]["min"];
   local max = dispatch ["nb_uops"]["max"];

   return min, max;
end

--- Get the minimum and maximum latency of an instruction
-- @param dispatch The dispatch of an instruction (provided for performances reasons)
-- @return minimum reciprocal latency
-- @return maximum reciprocal latency
function insn:get_latency (dispatch, uarch)
   local t = get_bypass (self, "latency");
   if     (type (t) == "table" ) then return t.min, t.max;
   elseif (type (t) == "number") then return t, t;
   end

   if (dispatch == nil) then dispatch = self:get_dispatch() end
   if (dispatch == nil) then return end

   local min = dispatch ["latency"]["min"];
   local max = dispatch ["latency"]["max"];

   return min, max;
end

--- Get the reciprocal throughput of an instruction
-- @param dispatch The dispatch of an instruction (provided for performances reasons)
-- @return string representing reciprocal throughput (for example "17-23")
function insn:get_reciprocal_throughput (dispatch, uarch)
   local t = get_bypass (self, "rcpt");
   if     (type (t) == "table" ) then return t.min, t.max;
   elseif (type (t) == "number") then return t, t;
   end

   if (dispatch == nil) then dispatch = self:get_dispatch() end
   if (dispatch == nil) then return end

   local min = dispatch ["recip_throughput"]["min"];
   local max = dispatch ["recip_throughput"]["max"];

   return min, max;
end

--- Get the micro-operations groups of an instruction
-- @param dispatch The dispatch of an instruction (provided for performances reasons)
-- @return uops groups (table)
function insn:get_uops_groups (dispatch, uarch)
   local t = get_bypass (self, "uops_groups");
   if (type (t) == "table") then
      local ret = {};
      for k,v in pairs (t) do
         ret[k] = { nb_uops = v[1], units = v[2] };
      end
      return ret;
   end

   if (dispatch == nil) then dispatch = self:get_dispatch() end
   if (dispatch == nil) then return end

   return dispatch ["uops_groups"];
end
