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

-- Initialize timers (warmup RDTSC) and return a gvar containing the timer probe cost
--
-- Require libvprof
function insert_init_timer(patch, addr, pos, sync)

   local v = patcher.add_int64_gvar(patch, 0)

   patcher.insert_call(patch, "libvprof.so", "vprof_timer_warmup", addr, pos
      , nil
      , { patcher.argImmediate(sync and 1 or 0)
        , patcher.argPointerTo(v)
        })

   return v
end


-- Start a timer (in a global variable)
--
-- @sync@ indicates if barriers should be inserted (i.e. CPUID)
function insert_start_timer(patch, addr, pos, timer, sync)
   local code = {}

   if sync then
      code = asm_code(asm_safe({"RAX","RBX","RCX","RDX"},
         { "CPUID"
         , "RDTSC"
         , "SAL $0x20, %RDX"
         , "OR %RDX, %RAX"
         , "MOV %RAX, %VAR%"
         }))
   else
      code = asm_code(asm_safe({"RAX","RDX"},
         { "RDTSC"
         , "SAL $0x20, %RDX"
         , "OR %RDX, %RAX"
         , "MOV %RAX, %VAR%"
         }))
   end

   local gvars = Table:new()
   gvars:insert(timer)
   return insert_asm(patch, addr, pos, gvars, code)
end

-- Stop a timer (in a global variable)
--
-- Correction value is given by insert_init_timer
--
-- @sync@ indicates if barriers should be inserted (i.e. CPUID, RDTSCP)
function insert_stop_timer(patch, addr, pos, timer, sync, correction)
   local code = {}

   if sync then
      code = asm_code(asm_safe({"RAX","RBX","RCX","RDX"},
         { "RDTSCP"
         , "SAL $0x20, %RDX"
         , "OR %RDX, %RAX"
         , "MOV %VAR%,%RDX"
         , "SUB %RDX, %RAX"
         , "MOV %VAR%,%RDX"
         , "SUB %RDX, %RAX"
         , "MOV %RAX, %VAR%"
         , "CPUID"
         }))
   else
      code = asm_code(asm_safe({"RAX","RDX"},
         { "RDTSC"
         , "SAL $0x20, %RDX"
         , "OR %RDX, %RAX"
         , "MOV %VAR%,%RDX"
         , "SUB %RDX, %RAX"
         , "MOV %VAR%,%RDX"
         , "SUB %RDX, %RAX"
         , "MOV %RAX, %VAR%"
         }))
   end

   local gvars = Table:new()
   gvars:insert(timer)
   gvars:insert(correction)
   gvars:insert(timer)
   return insert_asm(patch, addr, pos, gvars, code)
end
