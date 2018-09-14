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

--- Module cqa.api.arm64.back_end
-- Defines functions related to the front-end of the processor and, to be more precise,
-- corresponds to the in-order stages from instruction fetch to register read.
module ("cqa.api.arm64.back_end", package.seeall)

--- Does back-end analysis for a sequence of instructions and push results into cqa_results
--- This function should set the following entries:
---    - "[arm64] cycles back end"
-- @param cqa_env
function do_back_end_analysis (cqa_env)
   local cqa_context    = cqa_env.common.context;
   local arch_consts    = Consts[cqa_context.arch];
   local uarch          = cqa_context.uarch;
   local uarch_consts   = cqa_context.uarch_consts;

   local cycles_min = 0;
   local cycles_max = 0;

   for _,port in pairs(cqa_env["[arm64] uops min"]) do
      if (port > cycles_min) then 
         cycles_min = port;
      end
   end

   for _,port in pairs(cqa_env["[arm64] uops max"]) do
      if (port > cycles_max) then 
         cycles_max = port;
      end
   end

   print("back_end: "..cycles_min.." cycles min, "..cycles_max.." cycles max");

   cqa_env["[arm64] cycles back end"] = { min = cycles_min, max = cycles_max};
end