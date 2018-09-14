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

-- Return program entry point (addr)
function program_entry(asmfile)
   for fct in asmfile:functions() do
      if fct:get_name() == "_init" then
         return fct:get_first_insn():get_address()
      end
   end

   return nil
end

-- Return program exit point (addr)
function program_exit(asmfile)
   for fct in asmfile:functions() do
      if fct:get_name() == "_fini" then
         local lastinsn_addr = 0;
         for b in fct:blocks() do
            if (b:get_last_insn():get_address() >  lastinsn_addr) then
               lastinsn_addr = b:get_last_insn():get_address();
            end
         end
         return lastinsn_addr
      end
   end

   return nil
end
