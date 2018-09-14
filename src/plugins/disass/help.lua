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

module ("disass.help", package.seeall)

function disass:init_help()
   local help = Help:new();
   help:set_name ("disass");
   help:set_version("0.0.1");
   help:set_usage("maqao disass [OPTION]... objfile");
   help:set_description("Use this module to disassemble a binary file");
   help:add_option("hide-source-lines", nil, nil, nil, "Hide source file and line", nil);
   help:add_option("hide-function-headers", "", nil, nil, "Hide function headers", nil);
   help:add_option("hide-labels", "", nil, nil, "Hide block labels", nil);
   help:add_option("hide-loops", "", nil, nil, "Hide loops", nil);
   help:add_option("hide-raw", "", nil, nil, "Hide raw hexadecimal instruction coding", nil);
   help:add_option("hide-offsets", "", nil, nil, "Hide instruction offsets", nil);
   help:add_option("hide-classes", "", nil, nil, "Hide instruction classes", nil);
   help:add_option("hide-asm", "", nil, nil, "Hide instruction ASM code", nil);
   help:add_option("hide-comments", "", nil, nil, "Hide comments", nil);
   help:add_option("hide-inlining", "", nil, nil, "Hide inlining display", nil);
   help:add_option("disable-dynamic-highlight", "", nil, nil, "Disable dynamic instruction highlighting (HTML output only)", nil);
   help:add_option("output-format", "of", "[shell|html|xml]", false, "Select output format", nil);
   help:add_option("disable-demangling", "", nil, nil, "Disable decoding (demangling) of low-level symbol names into user-level names.", nil);
   help:add_option("start-address", "", "address", false, "Start displaying data at the specified address.", nil);
   help:add_option("stop-address", "", "address", false, "Stop displaying data at the specified address.", nil);
   help:add_option("function-filter", "ff", "pattern", false, "Only display functions whose names match the given pattern", nil);
   help:add_option("loop-id", "lid", "ID", false, "Only display instructions of the given loop", nil);
   help:add_option("function", "f", "label", false, "Select a function to display", nil);
   help:add_option("function-inbound-deps", "fi", nil, nil, "Display inbound dependencies of the selected function", nil);
   help:add_option("function-outbound-deps", "fo", nil, nil, "Display outbound dependencies of the selected function", nil);
   Utils:load_common_help_options (help)
   return help;
end
