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

local function encode(str)
   return str:gsub("&","&amp;"):gsub(">", "&gt;"):gsub("<", "&lt;"):gsub("\"", "&quot;")
end

local function print_header(config,asmfile)
   config.printf(
[[<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<asmfile>
]])
end

local function print_footer(config,asmfile)
   config.printf(
[[
</asmfile>
]])
end

local function print_fct_header(config,fct,name)
   local addr = fct:get_first_insn():get_address()

   config.printf([[<function id="%d" offset="%x" label="%s">]], fct:get_id(), addr, encode(name))
end

local function print_fct_body_header(config,fct)
end

local function print_fct_footer(config,fct)
   config.printf("</function>")
end

local function print_line_begin(config,file,line)
   config.printf([[<source file="%s" line="%d"/>]], file,line)
end

local function print_line_end(config,file,line)
end

local function print_block_begin(config,block,label,isBeginLoop,comment)
   config.printf([[<block id="%d" label="%s" comment="%s">]], block:get_id(), label, comment)
end

local function print_block_end(config,block)
   config.printf([[</block>]])
end

local function print_inline_begin(config,fct)
end

local function print_inline_end(config,fct)
end

local function print_insn(config,fct,i,comment,class)
   config.printf([[<insn offset="%x" raw="%s" asm="%s" comment="%s" class="%d"/>]],
      i:get_address(), trim(i:get_coding()), encode(i:get_asm_code():lower()), encode(comment), i:get_class())
end

local function print_loop_begin(config,loop,insn)
   config.printf([[<loop id="%d">]],loop:get_id())
end

local function print_loop_end(config,loop,insn)
   config.printf([[</loop>]])
end

pp_xml = {
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

