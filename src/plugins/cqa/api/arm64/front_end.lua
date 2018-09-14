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

--- Module cqa.api.arm64.front_end
-- Defines functions related to the front-end of the processor and, to be more precise,
-- corresponds to the in-order stages from instruction fetch to register read.
module ("cqa.api.arm64.front_end", package.seeall)

--require "cqa.api.arm64.front_end.fetch";
--require "cqa.api.arm64.front_end.decode";
--require "cqa.api.arm64.front_end.rename";
--require "cqa.api.arm64.front_end.dispatch";

--- Does front-end analysis for a sequence of instructions and push results into cqa_results
--- This function should set the following entries:
---    - "[arm64] cycles front end"
-- @param cqa_env
function do_front_end_analysis (cqa_env)
   local cqa_context    = cqa_env.common.context;
   local arch_consts    = Consts[cqa_context.arch];
   local uarch          = cqa_context.uarch;
   local uarch_consts   = cqa_context.uarch_consts;

   local front_end_cycles = 0;

   -- WARNING Assuming that the loop buffer has no limitation of code size 
   -- (most likely true, instructions having a fixed length it would make no sense to specify a uops capacity if it was not the case)
   -- If we are analyzing a loop and it fits in the loop buffer then it means the fetch and decode cannot be limiting factors.
   -- Else we need to take into account fetching and decoding
   if (false) then

      -- To avoid losing propagation effets of a stall we use as base the maximum input and we increase it every time 
      -- a stall should happens
      front_end_cycles = cqa_results["nb instructions"] / uarch_consts["insns fetched per cycle"]; 

      -- If an instruction generates more uops than the throughput of the decoder then we have to wait 1 / decoder's wide (uops) extra cycle per extra uop.
      -- WARNING Assuming that there is a buffer between fetch and decode to avoid starvation when decoding
      for _,insn in ipairs (crp.insns) do
         --if (insn:)
      end

   end

   -- On the Juno Board it appears that some mechanisms prevent issuing uops of next iteration
   -- with the branch of the current iteration. It is the same as rounding up the cycles required by dispatch
   cycles = math.ceil(#cqa_env.insns / 3.0);

   print("frond_end: "..cycles.." cycles");

   cqa_env["[arm64] cycles front end"] = cycles;
end