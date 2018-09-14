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

local function print_header(config,asmfile)
end

local function print_footer(config,asmfile)
end

local function print_fct_header(config,fct,name)
   local addr = fct:get_first_insn():get_address()

   local fid = fct:get_id()
   if fid ~= 0 then
      config.printf("\n")
   end
   config.printf("%016x <%s>:", addr, name)
end

local function print_fct_body_header(config,fct)
end

local function print_fct_footer(config,fct)
end

local function print_line_begin(config,file,line)
   config.printf("%s:%d", file,line)
end

local function print_line_end(config,file,line)
end

local function print_block_begin(config,block,label,isBeginLoop,comment)

   if not isBeginLoop and block:is_loop_entry() then
      local loop = block:get_loop()
      comment = comment .. string.format("Loop %d (entry)", loop:get_id())
   end

   if config.show_comment and comment then
      config.printf("%s:\t%s", label, comment)
   else
      config.printf("%s:", label)
   end
end

local function print_block_end(config,block)
end

local function print_insn(config,fct,i,comment,class)
   local offstr = ""
   local rawstr = ""
   local asmstr = ""
   local cmtstr = ""
   local classstr = ""

   if config.show_offset then
      local addr = i:get_address()
      offstr = string.format("%x:\t", addr)
   end

   if config.show_raw then
      local raw = trim(i:get_coding())
      rawstr = string.format("%-28s\t",raw)
   end

   if config.show_asm then
      local asm = i:get_asm_code():lower()
      asmstr = string.format("%s",asm)
   end

   if config.show_comment and comment ~= "" then
      cmtstr = string.format("\t; %s", comment)
   end

   if config.show_class then
      class = string.format("(%s)",class)
      classstr = string.format("%-8s\t", class)
   end

   config.printf("  %s%s%s%s%s",offstr,rawstr,classstr,asmstr,cmtstr)
end

local function print_inline_begin(config,fct)
   config.printf([[; ------- Inlined from function "%s"]], fct:get_name())
end

local function print_inline_end(config,fct)
   config.printf([[; ------- End of inlining]])
end

local function print_loop_begin(config,loop,insn)
   config.printf([[; ------- Loop %d entry]], loop:get_id())
end

local function print_loop_end(config,loop,insn)
   config.printf([[; ------- Loop %d exit]], loop:get_id())
end


pp_sh = {
   print_header = print_header,
   print_footer = print_footer,
   print_fct_header = print_fct_header,
   print_fct_body_header = print_fct_body_header,
   print_fct_footer = print_fct_footer,
   print_line_begin = print_line_begin,
   print_line_end = print_line_end,
   print_block_begin = print_block_begin,
   print_block_end = print_block_end,
   print_insn = print_insn,
   print_inline_begin = print_inline_begin,
   print_inline_end = print_inline_end,
   print_loop_begin = print_loop_begin,
   print_loop_end = print_loop_end
}

