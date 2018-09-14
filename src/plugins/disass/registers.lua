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

local function set_register(asmfile, registers, reg, value)
end


function update_registers(asmfile,registers,insn)
   -- Look for "MOV x, %reg"
   if insn:get_name() == "MOV" and insn:is_oprnd_reg(1) == true then
      local reg = string.sub(insn:get_oprnd_str(1),2)
      
      if insn:is_oprnd_imm(0) == true then    -- x is an immediate value
         local val = tonumber(string.sub(insn:get_oprnd_str(0),2), 16)
         set_register(asmfile, registers, reg, val)
      elseif insn:is_oprnd_reg(0) == true then -- x is a register
         local reg2 = string.sub(insn:get_oprnd_str(0),2)
         set_register(asmfile, registers, reg, registers[reg2])
      else
         set_register(asmfile, registers, reg, nil)
      end

   -- Look for "XOR x, %reg"
   elseif insn:get_name() == "XOR" and insn:is_oprnd_reg(1) == true then
      local reg = string.sub(insn:get_oprnd_str(1),2)
      
      if registers[reg] ~= nil and insn:is_oprnd_imm(0) == true then    -- x is an immediate value
         local val = tonumber(string.sub(insn:get_oprnd_str(0),2), 16)
         set_register(asmfile, registers, reg, math_xor(val,registers[reg]))
      elseif insn:is_oprnd_reg(0) == true then -- x is a register
         local reg2 = string.sub(insn:get_oprnd_str(0),2)
         if reg == reg2 then -- special case "XOR r, r"
            set_register(asmfile, registers, reg, 0)
         elseif registers[reg] ~= nil and registers[reg2] ~= nil then
            set_register(asmfile, registers, reg, math_xor(registers[reg2],registers[reg]))
         end
      else
         set_register(asmfile, registers, reg, nil)
      end
   
   -- Look for "ADD x, %reg"
   elseif insn:get_name() == "ADD" and insn:is_oprnd_reg(1) == true then
      local reg = string.sub(insn:get_oprnd_str(1),2)
      
      if registers[reg] ~= nil then
         if insn:is_oprnd_imm(0) == true then    -- x is an immediate value
            local val = tonumber(string.sub(insn:get_oprnd_str(0),2), 16)
            set_register(asmfile, registers, reg, val + registers[reg])
         elseif insn:is_oprnd_reg(0) == true then -- x is a register
            local reg2 = string.sub(insn:get_oprnd_str(0),2)
            if registers[reg] ~= nil and registers[reg2] ~= nil then
               set_register(asmfile, registers, reg, registers[reg2] + registers[reg])
            end
         else
            set_register(asmfile, registers, reg, nil)
         end
      end
   
   -- Look for "SUB x, %reg"
   elseif insn:get_name() == "SUB" and insn:is_oprnd_reg(1) == true then
      local reg = string.sub(insn:get_oprnd_str(1),2)
      
      if registers[reg] ~= nil then
         if insn:is_oprnd_imm(0) == true then    -- x is an immediate value
            local val = tonumber(string.sub(insn:get_oprnd_str(0),2), 16)
            set_register(asmfile, registers, reg, registers[reg] - val)
         elseif insn:is_oprnd_reg(0) == true then -- x is a register
            local reg2 = string.sub(insn:get_oprnd_str(0),2)
            if registers[reg] ~= nil and registers[reg2] ~= nil then
               set_register(asmfile, registers, reg, registers[reg] - registers[reg2])
            end
         else
            set_register(asmfile, registers, reg, nil)
         end
      end
   
   -- Look for "MUL x, %reg"
   elseif insn:get_name() == "MUL" and insn:is_oprnd_reg(1) == true then
      local reg = string.sub(insn:get_oprnd_str(1),2)
      
      if registers[reg] ~= nil then
         if insn:is_oprnd_imm(0) == true then    -- x is an immediate value
            local val = tonumber(string.sub(insn:get_oprnd_str(0),2), 16)
            set_register(asmfile, registers, reg, registers[reg] * val)
         elseif insn:is_oprnd_reg(0) == true then -- x is a register
            local reg2 = string.sub(insn:get_oprnd_str(0),2)
            if registers[reg] ~= nil and registers[reg2] ~= nil then
               set_register(asmfile, registers, reg, registers[reg] * registers[reg2])
            end
         else
            set_register(asmfile, registers, reg, nil)
         end
      end
   
   -- Look for "DIV x, %reg"
   elseif insn:get_name() == "DIV" and insn:is_oprnd_reg(1) == true then
      local reg = string.sub(insn:get_oprnd_str(1),2)
      
      if registers[reg] ~= nil then
         if insn:is_oprnd_imm(0) == true then    -- x is an immediate value
            local val = tonumber(string.sub(insn:get_oprnd_str(0),2), 16)
            set_register(asmfile, registers, reg, registers[reg] / val)
         elseif insn:is_oprnd_reg(0) == true then -- x is a register
            local reg2 = string.sub(insn:get_oprnd_str(0),2)
            if registers[reg] ~= nil and registers[reg2] ~= nil then
               set_register(asmfile, registers, reg, registers[reg] / registers[reg2])
            end
         else
            set_register(asmfile, registers, reg, nil)
         end
      end

   -- Unset values of registers involved
   -- FIXME: support implicit registers
   else
      for u,reg in pairs(insn:get_registers_name()) do
         reg = string.sub(reg,2)
         set_register(asmfile, registers, reg, nil)
      end
   end

   return registers
end
