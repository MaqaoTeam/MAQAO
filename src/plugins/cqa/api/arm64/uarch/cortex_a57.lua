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

--- Module cqa.arm64.uarch.cortex_a57
-- Defines micro-architectural constants for ARM Cortex-A57 processors
module ("cqa.api.arm64.uarch.cortex_a57", package.seeall)

uarch_consts = {
   -- FRONT END
   -- Fetch (in & out: instructions)
   ["fetch queue entries"]       = 32,
   ["insns fetched per cycle"]   = 3,
   -- Decode (in: instruction, out: µops)
   ["uops decoded per cycle"]    = 3,
   -- Loop buffer
   ["nb loop buffer entries"]    = 32,
   -- Register renaming
   ["uops renaming per cycle"]   = 3,
   -- Dispatch (in & out: µops)
   ["uops dispatched per cycle"] = 3,

   -- BACK END
   -- Issue ports
   ["ports queues entries"]      = 8,
   ["nb executions units"]       = 8,
   -- Units
   units = {
      [0] = "Load",
      [1] = "Store",
      [2] = "Multi-cycle integer",
      [3] = "FP 0",
      [4] = "FP 1",
      [5] = "Branch",
      [6] = "Integer ALU 1",
      [7] = "Integer ALU 2",
   },
   -- Memory
   ["nb load units"] = 1;
   ["nb store units"] = 1; 
   ["load unit native width"] = 16;
   ["store unit native width"] = 16;

   -- PROPERTIES
   -- Vector size
   ["vector size in bytes"] = 16,
   ["INT vector size in bytes"] = 16,
   ["Vectorizable FP sizes"] = {
      [Consts.DATASZ_8b]     = false,
      [Consts.DATASZ_16b]    = true,
      [Consts.DATASZ_32b]    = true,
      [Consts.DATASZ_64b]    = true,
      [Consts.DATASZ_128b]   = false,
   },

   -- #PRAGMA_NOSTATIC UFS
   -- For UFS: resources size
   ["Is supported by UFS"] = false,
   -- #PRAGMA_STATIC
}
