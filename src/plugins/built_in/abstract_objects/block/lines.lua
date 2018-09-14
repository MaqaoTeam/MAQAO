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


module("block.lines",package.seeall)

-- DEPRECATED, use block:get_src_lines instead
function block:get_src_line()
   Message:warn ("block:get_src_line is deprecated and will be removed. Use block:get_src_lines instead (default return values are 0 instead of -1).\n");

   local src_start = -1;
   local src_end = -1;
   local insn

   for insn in self:instructions() do
      local linesrc = insn:get_src_line();

      -- Case where no source line is specified
      if(linesrc > 0) then
         if ((src_start == -1) or (src_end == -1)) then
            if (src_start == -1) then
               src_start = linesrc;
            end
            if (src_end == -1) then
               src_end = linesrc;
            end
         else
            src_start = math.min(src_start,linesrc);
            src_end = math.max(src_end,linesrc);
         end
      end
   end

   --[[
   print("=> Found line "..src_start);
   print("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n");
   print("=> Found line "..src_end);
   print("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n");
   --]]
   return src_start,src_end;
end



function block:get_asm_addresses()
   local src_start = -1;
   local src_end = -1;
   local insn

   for insn in self:instructions() do
      local linesrc = insn:get_address();
      if ((src_start == -1) or (src_end == -1)) then
         if (src_start == -1) then
            src_start = linesrc;
         end
         if (src_end == -1) then
            src_end = linesrc;
         end
      else
         src_start = math.min(src_start,linesrc);
         src_end = math.max(src_end,linesrc);
      end
   end

   --[[
   print("=> Found address "..src_start);
   print("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n");
   print("=> Found address "..src_end);
   print("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n");
   --]]
   return src_start,src_end;
end








