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

-- Constants
InsertBefore = 0
InsertAfter = 1

-- Insert ASM code
function insert_asm(patch, addr, pos, vars, asm)

   -- we replace occurences of %VAR% with
   --    0(%RIP) for global variables
   --    %FS:0(%RIP) for TLS variables

   local asm2 = asm
   local gvars = {}
   local tlsvars = {}

   for _,g in ipairs(vars) do
      if g.typ == "gvar" then
         asm2 = string.gsub(asm2, "%%VAR%%", "0(%%RIP)", 1)
         table.insert(gvars,g.value)
      elseif g.typ == "tlsvar" then
         asm2 = string.gsub(asm2, "%%VAR%%", "%%FS:0(%%RIP)", 1)
         table.insert(tlsvars,g.value)
      else
         Message:critical("Invalid variable type")
      end
   end

   local m = patch.madras
   return m:insnlist_add(asm2, addr, pos, gvars, tlsvars)
end

function merge_codes(codes)
   local ret = {}
   for _,c in ipairs(codes) do
      for _,i in ipairs(c) do
         table.insert(ret,i)
      end
   end
   return ret
end

-- Prepare the asm code in the given array for Madras
function asm_code(code)
   return table.concat(code, "\n")
end

-- Save and restore the given registers
--
-- Warning: no alignement check
function asm_save_regs(regs,code)
   local saves = {}
   local restores = {}
   for _,reg in ipairs(regs) do
      table.insert(saves,"PUSH %" .. reg)
      table.insert(restores,1, "POP %" .. reg)
   end
   local cs = {saves,code,restores}
   return merge_codes(cs)
end

-- Reserve some stack space
function asm_save_stack(code)
   local prefix = 
      { "LEA -0x200(%RSP),%RSP"
      , "PUSHFQ"
      }
   local suffix = 
      { "POPFQ"
      , "LEA 0x200(%RSP),%RSP"
      }
   local cs = {prefix,code,suffix}
   return merge_codes(cs)
end

-- Wrap the given code with the bare minimum safety code
function asm_safe(regs,code)
   return asm_save_stack(asm_save_regs(regs,code))
end

----------------------------------------
-- FUNCTION CALL
----------------------------------------

-- Insert a function call parameter
--
-- Use arg* helpers to create parameters!
--
function insert_arg(patch, arg)
   local m = patch.madras

   if arg.typ == "gvar" then
      local addrOrVal = arg.pointer and "a" or "q"
      m:fctcall_addparam_from_gvar(arg.value, nil, addrOrVal)
   elseif arg.typ == "tlsvar" then
      local addrOrVal = arg.pointer and "a" or "q"
      m:fctcall_addparam_from_tlsvar(arg.value, nil, addrOrVal)
   elseif arg.typ == "imm" then
      m:fctcall_addparam_imm(arg.value)
   elseif arg.typ == "immdouble" then
      m:fctcall_addparam_immdouble(arg.value)
   elseif arg.typ == "reg" then
      m:fctcall_addparam_reg(arg.value)
   elseif arg.typ == "str" then
      m:fctcall_addparam_from_str(arg.value)
   else
      Message:critical("Invalid argument type")
   end
end

-- Parameter: pointer to a global variable
function argPointerTo(var)
   return { typ   = var.typ
          , value = var.value
          , pointer = true
          }
end

-- Parameter: value of a global variable
function argValueOf(var)
   return { typ   = var.typ
          , value = var.value
          , pointer = false
          }
end

-- Parameter: immediate value
function argImmediate(v)
   return { typ   = "imm"
          , value = v
          }
end

-- Parameter: immediate double value
function argImmediateDouble(v)
   return { typ   = "immdouble"
          , value = v
          }
end

-- Parameter: register (by name)
function argReg(reg)
   return { typ   = "reg"
          , value = reg
          }
end

-- Parameter: string value
function argString(v)
   return { typ   = "str"
          , value = v
          }
end

-- Insert a function call
function insert_call(patch,lib,name,addr,pos,retval,args)
   local m = patch.madras
   local modif = m:fctcall_new(name, lib, addr, pos, 0)

   for _,arg in pairs(args) do
      insert_arg(patch,arg)
   end

   if retval ~= nil then
      m:fctcall_addreturnval(retval)
   end

   return modif
end

----------------------------------------
-- FUNCTION CALL : LIBC
----------------------------------------

-- Insert a call to the libc
function insert_call_libc(p,name,addr,pos,retval,args)
   return insert_call(p,"libc.so.6",name,addr,pos,retval,args)
end

-- Insert a call to printf
function insert_printf(p,addr,pos,retval,args)
   return insert_call_libc(p,"printf",addr,pos,retval,args)
end

-- Insert a call to fprintf
function insert_fprintf(p,addr,pos,retval,args)
   return insert_call_libc(p,"fprintf",addr,pos,retval,args)
end

-- Insert a call to fopen
--
-- * path and mode are strings
-- * outfile is returned as first return value
function insert_fopen(p,addr,pos,path,mode)
   local path_var = patcher.add_string_gvar(p, path)
   local mode_var = patcher.add_string_gvar(p, mode)
   local outfile = patcher.add_int64_gvar(p, 0)

   local modif = insert_call_libc(p,"fopen",addr,pos,outfile,
      { argPointerTo(path_var)
      , argPointerTo(mode_var)
      })

   return outfile,modif
end

-- Insert a call to fclose
function insert_fclose(p,addr,pos,retval,fp)
   return insert_call_libc(p,"fclose",addr,pos,retval,
      { argValueOf(fp)
      })
end
