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

module ("disass", package.seeall)

require "disass.help"
require "disass.util"
require "disass.pp_shell"
require "disass.pp_html"
require "disass.pp_xml"
require "disass.traverse"
require "disass.calls"

function disass:disass_launch (args, aproject)
   local help = disass:init_help()

   if args.v == true or args.version == true then
      help:print_version (false, args.verbose)
      os.exit (0)
   end

   if args.h == true or args.help == true then
      help:print_help (false, args.verbose)
      os.exit (0)
   end

   if args.bin == nil then
      print ("No binary name provided");
      print ("\nUSAGE --------------------------------------------");
      print ("\tmaqao disass <binary>\n");
      os.exit (1);
   end

   local asmfile = aproject:load(args.bin, aproject:get_uarch_name());
   if (asmfile == nil) then
      print(string.format("Cannot disassemble the binary \"%s\"", args.bin))
      os.exit (1);
   end

   local config = 
      { printer_name            = args["of"] or args["output_format"]
      , printer                 = pp_sh
      , printf                  = function(fmt,...) return print(string.format(fmt,...)) end
      , show_source_lines       = not args["hide-source-lines"]
      , show_function_headers   = not args["hide-function-headers"]
      , show_loops              = not args["hide-loops"]
      , show_blocks             = not args["hide-labels"]
      , show_raw                = not args["hide-raw"]
      , show_offset             = not args["hide-offsets"]
      , show_asm                = not args["hide-asm"]
      , show_class              = not args["hide-classes"]
      , show_comment            = not args["hide-comments"]
      , show_inlining           = not args["hide-inlining"]
      , demangle                = not args["disable-demangling"]
      , dynamic_highlight       = not args["disable-dynamic-highlight"]
      , start_address           = nil
      , stop_address            = nil
      , function_filter_pattern = args["ff"] or args["function-filter"]
      , loop_id                 = args["loop-id"] or args["lid"]
      , function_filter         = function(fct) return true end
      , function_select         = args["f"] or args["function"]
      , function_outbound       = args["fo"] or args["function-outbound-deps"]
      , function_inbound        = args["fi"] or args["function-inbound-deps"]
      , functions               = {}
      }

   -- Selected printer (shell, html...)
   if config.printer_name == "shell" then
      config.printer = pp_sh
   elseif config.printer_name == "html" then
      config.printer = pp_html
   elseif config.printer_name == "xml" then
      config.printer = pp_xml
   end

   -- Start and stop addresses
   if args["start-address"] ~= nil then
      config["start_address"] = tonumber(args["start-address"], 16)
   end

   if args["stop-address"] ~= nil then
      config["stop_address"] = tonumber(args["stop-address"], 16)
   end
   --
   -- Function filtering (matching on name)
   if config.function_filter_pattern ~= nil then
      config["function_filter"] = function(fct) 
         return fct_expected_name(config,fct):match(config.function_filter_pattern)
      end
   end
   
   -- Function filtering (matching on loop_id)
   if config.loop_id ~= nil then
      local f = function(fct) 
         for l in fct:loops() do
            if l:get_id() == tonumber(config.loop_id) then
               return true
            end
         end
         return false
      end
      if config["function_filter"] ~= nil then
         local g = config["function_filter"]
         config["function_filter"] = function (x)
            return (g(x) and f(x))
         end
      else
         config["function_filter"] = f
      end
   end

   -- Function selected
   if config.function_select ~= nil then
      for fct in asmfile:functions() do
         if fct_expected_name(config,fct) == config.function_select then
            if config.function_filter(fct) then
               fct_set_insert(config.functions, fct)
            end
            if config.function_outbound then
               for s in fct:successors() do
                  if config.function_filter(s) then
                     fct_set_insert(config.functions, s)
                  end
               end
            end
            if config.function_inbound then
               for s in fct:predecessors() do
                  if config.function_filter(s) then
                     fct_set_insert(config.functions, s)
                  end
               end
            end
            break
         end
      end
   else
      for s in asmfile:functions() do
         if config.function_filter(s) then
            table.insert(config.functions, s)
         end
      end
   end

   function is_in_loop(i,lid)
      local l = i:get_loop()
      while l~= nil do
         if l:get_id() == lid then
            return true
         end
         l = l:get_parent()
      end

      return false
   end

   function insn_filter(i)
      return config.loop_id == nil or is_in_loop(i, tonumber(config.loop_id))
   end


   -- Traversal callbacks
   local callbacks = {}

   callbacks["function_begin"] = function(fct,i)
      if not insn_filter(i) then
         return
      end
      if config.show_function_headers then
         config.printer.print_fct_header(config,fct,fct_expected_name(config,fct))
      end
      config.printer.print_fct_body_header(config,fct)
   end

   callbacks["function_end"] = function(fct,i)
      if not insn_filter(i) then
         return
      end
      config.printer.print_fct_footer(config,fct)
   end
      
   if config.show_inlining then
      callbacks["inlining_begin"] = function(fct,i)
         if not insn_filter(i) then
            return
         end
         config.printer.print_inline_begin(config, fct)
      end
      callbacks["inlining_end"] = function(fct,i)
         if not insn_filter(i) then
            return
         end
         config.printer.print_inline_end(config, fct)
      end
   end

   if config.show_loops then
      callbacks["loop_begin"] = function(loop,i)
         if not insn_filter(i) then
            return
         end
         config.printer.print_loop_begin(config, loop, i)
      end

      callbacks["loop_end"] = function(loop,i)
         if not insn_filter(i) then
            return
         end
         config.printer.print_loop_end(config, loop, i)
      end
   end

   if config.show_blocks then
      callbacks["block_begin"] = function(block,i,isBeginLoop)
         if not insn_filter(i) then
            return
         end
         config.printer.print_block_begin(config, block, block_label_get(config,block),isBeginLoop,"")
      end

      callbacks["block_end"] = function(block,i)
         if not insn_filter(i) then
            return
         end
         config.printer.print_block_end(config, block)
      end
   end

   if config.show_source_lines then
      callbacks["line_begin"] = function(srcFile,srcLine,i)
         if not insn_filter(i) then
            return
         end
         config.printer.print_line_begin(config,srcFile,srcLine)
      end

      callbacks["line_end"] = function(srcFile,srcLine,i)
         if not insn_filter(i) then
            return
         end
         config.printer.print_line_end(config,srcFile,srcLine)
      end
   end

   callbacks["insn"] = function(i,registers)
      if not insn_filter(i) then
         return
      end
      local icomment = ""
      if i:is_branch() or i:is_call() then
         if i:get_branch_target() and i:get_branch_target():get_block() then
            local block = i:get_branch_target():get_block()
            local block_label = block_label_get(config,block)
            local fct_label = fct_expected_name(config,block:get_function())

            local branchstr = ""
            local sym = "→"
            if i:is_branch() then
               if i:get_address() > i:get_branch_target():get_address() then
                  sym = "↰"
               else 
                  sym = "↲"
               end
            else
               sym = "⇄"
            end
            if block:get_first_insn():get_address() ~= block:get_function():get_first_insn():get_address() then
               if block:get_function():get_id() ~= i:get_function():get_id() then
                  branchstr = string.format([[%s label %s in function %s]], sym, block_label, fct_label)   
               else
                  branchstr = string.format([[%s label %s]], sym, block_label)
               end
            elseif string.sub(fct_label, -4) == Consts.EXT_LBL_SUF then
               local fct_name = string.sub(fct_label, 1, -string.len(Consts.EXT_LBL_SUF)-1)
               local callstr = call_str(config,asmfile,fct_name,registers)
               branchstr = string.format([[%s %s]], sym, callstr)
            else
               branchstr = string.format([[%s %s]], sym, fct_label)
            end
                                 
            if icomment == "" then
               icomment = branchstr
            else
               icomment = string.format("%s, %s", icomment, branchstr)
            end
         end
      end
      iclass_names = {
         [Consts.C_OTHER] = "other",
         [Consts.C_ARITH] = "arith",
         [Consts.C_CONV] = "conv",
         [Consts.C_CRYPTO] = "crypto",
         [Consts.C_CONTROL] = "control",
         [Consts.C_LOGIC] = "logic",
         [Consts.C_MATH] = "math",
         [Consts.C_MOVE] = "move",
         [Consts.C_APP_SPECIFIC] = "app"
      }

      config.printer.print_insn(config,fct,i,icomment,iclass_names[i:get_class()])
   end

   -- Traverse object

   config.printer.print_header(config,asmfile)

   for k,fct in pairs(config.functions) do
      fct_traverse(asmfile,fct,callbacks,config.start_address,config.stop_address)
   end

   config.printer.print_footer(config,asmfile)
end
