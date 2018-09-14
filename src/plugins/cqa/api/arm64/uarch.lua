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

--- Module cqa.arm64.uarch
-- Defines the get_uarch_constants function used to return constants related to a micro-architecture
module ("cqa.api.arm64.uarch", package.seeall)

--- Returns a table describing constants related to the given micro-architecture
-- @param cqa_context cqa_context table
-- @return a table containing constants
function get_uarch_constants (cqa_context)
   local uarch = cqa_context.uarch;
   local ca = Consts[cqa_context.arch];

   local proc_name
   if (cqa_context.proc ~= nil) then
      proc_name = cqa_context.proc:get_name()
   end

   if (uarch == ca.arm64_UARCH_CORTEX_A57) then
      require "cqa.api.arm64.uarch.cortex_a57"
      return cortex_a57.uarch_consts;
   else
      print (string.format ("Error: no CQA uarch constants for uarch = %s\n", get_arch_by_name(cqa_context.arch):get_uarch_by_id(uarch):get_name()))
   end 
end