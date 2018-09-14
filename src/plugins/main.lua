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

module ("utils", package.seeall)

require "images_db"
require "abstract_objects_c"
require "classes_c"
require "errcode_c";
require "fcgx";
require "fs";
require "lfs";
require "madras";
require "bitops";

Utils.images = maqao_images or {}


--- Return the absolute path of the file calling this function
function Utils:get_file_path ()
   local debug_path = debug.getinfo(2, "Sl").source
   debug_path = string.gsub(debug_path, "^@", "")
   debug_path = string.gsub(debug_path, "[a-zA-Z0-9_]+.lua", "")
   return (debug_path)
end




--- This function prints the list of available modules with there alias and location
-- then exit with the code 0
function Utils:list_modules (args, display)
   local custom_paths = os.getenv(Consts.ENVVAR_MODULES_PATH)

   -- List modules and alias of built-in modules
   -- As modules are registered into utils._modules for the static binary
   -- but not for the dynamic one, a test has to be done to know where to
   -- find "built-in" modules (modules saved in the static binary)
   if Table.get_size(utils._modules) == 1 then
      Utils:scan_dir (Consts.MAQAO_PATH.."/src/plugins/", utils._modules)
   end

   -- List modules and alias of user modules (scan directories given
   -- in Consts.ENVVAR_MODULES_PATH
   if custom_paths ~= nil and custom_paths ~= "" then
      for i, path in pairs(String:split(custom_paths, ":")) do 
         Utils:scan_dir (path, utils._modules)
      end
   end 

   -- current directory
   Utils:scan_dir (os.getenv("PWD"), utils._modules)

   if display == false then
      return
   end
   
   -- Print modules then exit
   print ("") 
   print ("  MODULE              ALIAS               LOCATION");
   print ("==============================================================================");
   for i, module in pairs(utils._modules) do
      if module.s_alias == nil then
         print ("  "..module.name..string.rep(" ", 20 - string.len (module.name))..string.rep(" ", 20)..module.path)
      else
         for i, alias in ipairs (module.s_alias) do
            if i == 1 then
               print ("  "..module.name..string.rep(" ", 20 - string.len (module.name))..
                     alias..string.rep(" ", 20 - string.len (alias))..module.path)
            else
               print ("  "..string.rep(" ", 20)..alias..string.rep(" ", 20 - string.len (alias))..module.path)
            end
         end
      end
      
      -- Print long aliases if needed
      if module.l_alias ~= nil then  
         for key, data in pairs (module.l_alias) do
            print ("  "..string.rep(" ", 20)..key..string.rep(" ", 20 - string.len (key))..data)
         end
      end                   
   end

   os.exit (0);
end

local function _utils_list_procs ()
   local table_proc = {}
   for _, arch in pairs(get_archs_list()) do
      print (string.format ("For architecture %s:", arch:get_name()))

      for i, uarch in ipairs (arch:get_uarchs()) do
         local proc_list = {}
         for i,proc in ipairs (uarch:get_procs()) do
            proc_list [i] = proc:get_name();
         end

         print (string.format ("  For micro-architecture %s: %s\n",
                               uarch:get_name(), table.concat (proc_list, ", ")))
      end
   end
end


--- Generate an empty module skeleton 
local function _utils_module_skeleton (args)
   local name = ""

   -- Check if the name is set
   if (args.mod == "" or args.mod == nil)
   and (args.bin == "" or args.bin == nil) then
      print ("Name of the module to create can not been empty.\n"..
      "Specify the module name in the command line with [mod=]<val>")
      return
   end

   if args.mod ~= "" and args.mod ~= nil then
      name = args.mod
   else
      name = args.bin
   end

   -- Check if a module with the same name already exists
   -- List modules
   local custom_paths = os.getenv(Consts.ENVVAR_MODULES_PATH)
   if Table.get_size(utils._modules) == 1 then
      Utils:scan_dir (Consts.MAQAO_PATH.."/src/plugins/", utils._modules)
   end
   if custom_paths ~= nil and custom_paths ~= "" then
      Utils:scan_dir (custom_paths, utils._modules)
   end
   -- Then check if a module with the same name exists
   for _, mod in pairs (utils._modules) do
      if mod.name == name then
         print ("A module with the same name already exist.\nYour module can not been created")
         return
      end
   end

   -- Handle the output path
   local output_path = nil
   local output = ""
   local custom_output = ""
   if args.output ~= nil then
      output_path = args.output.."/"
   else
      output_path = os.getenv("PWD")
   end
   output = output_path.."/"..name

   -- Create the directory
   local res
   local error
   res, error = lfs.mkdir (output) 
   if res ~= true then
      print ("Directory "..output.." can not been created: "..error)
      return
   end

   local code = nil
   local filename = nil
   local file_out = nil

   --Create and generate the help file
   code = "module (\""..name..".help\", package.seeall)\n\n"..
   "function "..name..":init_help()\n"..
   "   local help = Help:new();\n"..
   "   help:set_name (\""..name.."\");\n"..
   "   -- Add your code here\n"..
   "   Utils:load_common_help_options (help)\n"..
   "   return help;\n"..
   "end\n"
   filename = output.."/help.lua"
   file_out = io.open(filename, "w");
   file_out:write (code)
   file_out:close ()

   -- Create and generate the file content
   code = "module (\""..name.."\", package.seeall)\n\n"..
   "require (\""..name..".help\")\n\n"..
   "--- Entry point of the module "..name.."\n"..
   "--@param args A table of parameters\n"..
   "--@param aproject An existing project\n"..
   "function "..name..":"..name.."_launch (args, aproject)\n".. 
   "   local help = "..name..":init_help()\n"..
   "   -- Print version if -v or --version are passed in parameters\n"..
   "   if args.v == true or args.version == true then\n"..
   "      help:print_version (false, args.verbose)\n"..
   "   end\n\n"..
   "   -- Print help if -h or --help are passed in parameters\n"..
   "   if args.h == true or args.help == true then\n"..
   "      help:print_help (false, args.verbose)\n"..
   "   end\n\n"..
   "   -- If help or version have been displayed, exit\n"..
   "   if args.h == true or args.help == true\n"..
   "   or args.v == true or args.version == true then\n"..
   "      os.exit (0)\n"..
   "   end\n"..
   "	--Insert your code here\n"..	       
   "end\n"	
   filename = output.."/"..name..".lua"
   file_out = io.open(filename, "w");
   file_out:write (code)
   file_out:close ()

   print ("\nSkeleton for module "..name.." has been created in\n"..output.."\n")
end




--- Generate the manpage using wiki syntax
-- @param args Table of arguments
-- @param module name of the module used as input. If nil, then MAQAO man page is generated
local function _utils_generate_wiki (args, module)
   -- Get the Help object ----------------------------------------------------
   local module_help = nil
   local code = ""

   if module == nil then   -- Get help for maqao command
      module_help = loadstring ("return Utils:main_load_help ()")()
   else                    -- Get help for a maqao module
      module_help = loadstring ("return "..module..":init_help()")()
   end
   if module_help == nil then -- Check error
      printf (module..":init_help return nil. Be sure your function is implemented and return\n"..
      "a Help object.")
      os.exit (1)
   end

   -- Generate the code of the manpage ---------------------------------------
   -- Header
   print ("====== Man Page ======")
   print ("Generated using MAQAO - "..module_help._date.." - "..module_help._version)
   print ("")

   -- Usage
   print ("===== Synopsis =====")
   print (module_help._usage)
   print ("")

   -- Description
   print ("===== Description =====")
   print (module_help._description)
   print ("")

   -- Options
   print ("===== Options =====")
   local str = ""
   for i, opt in pairs (module_help._options) do
      if  opt.is_sep ~= true
      and opt.is_text ~= true then
         str = ""
         -- Short option
         if opt.short ~= nil and opt.short ~= "" then
            str = str.."''-"..opt.short.."''"
         end

         -- Long option
         if opt.long ~= nil then
            if str ~= "" then
               str = str..", "
            end

            -- Special case for -- options
            if opt.long == "" then
               str = str.."''--"

               -- single word (without -- at the begining)
            elseif opt.long:find("^<") ~= nil then
               str = str..opt.long
               -- regular long option (--<opt>)
            else
               str = str.."''--"..opt.long
            end

            -- Add the argument if needed
            if opt.arg ~= nil then
               if opt.is_opt == true then
                  str = str.."[=//"..opt.arg.."//]"
               else
                  str = str.."=//"..opt.arg.."//"
               end
            end
            str = str.."''\\\\"
            print (str)

            -- Option description
            local desc = string.gsub (opt.desc, "\nExample:", "\n  Example:")
            print (desc)

            -- List of values
            if opt.values ~= nil then
               print ("Available values are:")
               local is_desc = false

               -- scan values to check if one of them has a description
               for i, val in ipairs (opt.values) do
                  if val.desc ~= nil then
                     is_desc = true
                  end
               end

               -- is_desc = true => at least one option has a description, so
               -- print one value per line
               if is_desc == true then
                  for i, val in ipairs (opt.values) do
                     str = "  * //"..val.name.."//"
                     if val.default == true then
                        str = str.." (default)"
                     end
                     if val.desc ~= nil then
                        str = str ..": "..string.gsub (val.desc, "\n", " ")
                     end
                     print (str)
                  end


               -- else, is_desc = false => print all values in the same line
               else
                  str = ""
                  for i, val in ipairs (opt.values) do
                     if i == 1 then
                        str = str..val.name
                     else
                        str = str..", "..val.name
                     end

                     -- if it is the default value
                     if val.default == true then
                        str = str.." (default)"
                     end
                  end
                  print (str)
               end
            end
            print ("")
         end

         -- Separator
      elseif opt.is_sep == true then
         print ("==== "..opt.name.." ====")
         -- Text
      elseif opt.is_text == true then
         print ("\n"..string.gsub (opt.name, "\n", " ").."\n")
      end
   end

   -- Examples
   if table.getn (module_help._examples) > 0 then
      print ("===== EXAMPLES =====")
      print ("")
      for i, example in pairs (module_help._examples) do
         local desc = example.desc:gsub("\n", " ")
         print ("  "..example.cmd)
         print (desc)
         print ("")
      end
   end


   os.exit (0)
end




--- Generate the manpage of the module
-- @param args Table of arguments
-- @param module name of the module used as input. If nil, then MAQAO man page is generated
local function _utils_generate_man (args, module)
   -- Get the Help object ----------------------------------------------------
   local module_help = nil
   local br   = "<.br>"
   local br_n = "\n.br\n"

   if module == nil then   -- Get help for maqao command
      module_help = loadstring ("return Utils:main_load_help ()")()
   else                    -- Get help for a maqao module
      module_help = loadstring ("return "..module..":init_help()")()
   end
   if module_help == nil then	-- Check error
      printf (module..":init_help return nil. Be sure your function is implemented and return\n"..
      "a Help object.")
      os.exit (1)	
   end

   -- Get the name of the manpage --------------------------------------------
   local man_name = nil
   if module == nil then
      man_name = "maqao"
   elseif module == "grouping" then
      man_name = "maqao-analyze-grouping"
   else
      man_name = "maqao-"..module
   end

   -- Open manpage file ------------------------------------------------------
   local filepath = "."
   if args.output ~= nil and args.output ~= "" then
      filepath = args.output
   end

   -- Generate the code of the manpage ---------------------------------------
   local filename = filepath.."/"..man_name..".1"
   local file_out = io.open(filename, "w");
   local code = ""
   -- Header
   code = code..".\\\" File generated using by MAQAO.\n"
   code = code..".TH "..man_name:upper().." \"1\" \""..module_help._date.."\" \""..man_name:upper().." "..module_help._version.."\" \"User Commands\"\n"

   -- Title
   code = code..".SH NAME\n"
   code = code..man_name.." \\- manual page for "..man_name:gsub ("-", " ").." module.\n"

   -- Usage
   code = code..".SH SYNOPSIS\n"
   code = code..module_help._usage:gsub ("\n", ""):gsub (br, br_n).."\n"

   -- Description
   code = code..".SH DESCRIPTION\n"
   code = code..module_help._description:gsub ("\n", ""):gsub (br, br_n).."\n"

   -- Options
   code = code..".SH OPTIONS\n"
   local str = ""
   for i, opt in pairs (module_help._options) do
      if  opt.is_sep ~= true
      and opt.is_text ~= true then
         str = ""
         -- Short option
         if opt.short ~= nil and opt.short ~= "" then
            str = str.."\\fB\\-"..opt.short.."\\fR"
         end

         -- Long option
         if opt.long ~= nil then
            if str ~= "" then
               str = str..", "
            end

            -- Special case for -- options
            if opt.long == "" then
               str = str.."\\fB\\-\\-\\fR"

               -- single word (without -- at the begining)
            elseif opt.long:find("^<") ~= nil then
               str = str.."\\fB"..opt.long.."\\fR"
               -- regular long option (--<opt>)
            else
               str = str.."\\fB\\-\\-"..opt.long.."\\fR"
            end

            -- Add the argument if needed
            if opt.arg ~= nil then
               if opt.is_opt == true then
                  str = str.."[\\="..opt.arg.."]"
               else
                  str = str.."\\="..opt.arg
               end
            end

            -- Option description
            str = str.."\n"..opt.desc:gsub ("\n", " "):gsub (br, br_n)
            
            -- List of values
            if opt.values ~= nil then
               str = str.." Available values are: \n"
               local is_desc = false

               -- scan values to check if one of them has a description
               for i, val in ipairs (opt.values) do
                  if val.desc ~= nil then 
                     is_desc = true
                  end
               end
               
               -- is_desc = true => at least one option has a description, so 
               -- print one value per line
               if is_desc == true then
                  for i, val in ipairs (opt.values) do
                     str = str..".TP 20 \n\\fB       "..val.name.."\\fR "
                     if val.default == true then
                        str = str.." (default)"
                     end 
                     if val.desc ~= nil then
                        str = str .."\n"..val.desc:gsub ("\n", " ").."\n"
                     end
                  end
                  str = str..".\n.SH \"\""        
                  

               -- else, is_desc = false => print all values in the same line
               else
                  for i, val in ipairs (opt.values) do
                     if i == 1 then
                        str = str..val.name
                     else
                        str = str..", "..val.name                     
                     end

                     -- if it is the default value
                     if val.default == true then
                        str = str.." (default)"
                     end
                  end
                  str = str..".\n"        
               end
            end
         end

         if str ~= "" then
            code = code..".TP\n"..str.."\n"
         end
         -- Separator
      elseif opt.is_sep == true then
         code = code..".SH \"    "..opt.name:upper ().."\"\n"
         -- Text
      elseif opt.is_text == true then
         code = code..".TP\n"..string.gsub (opt.name, "\n", " "):gsub (br, br_n).."\n"
      end
   end

   -- Examples
   if table.getn (module_help._examples) > 0 then
      code = code..".SH EXAMPLES\n"
      for i, example in pairs (module_help._examples) do
         code = code..".TP\n"..example.cmd.."\n"..example.desc:gsub("\n", " "):gsub(br, br_n).."\n"
      end
   end

   -- Author
   code = code..".SH AUTHOR\n"
   local list_authors = "Written by "
   if table.getn(module_help._authors) == 0 then 
      list_authors = list_authors.."Nobody"
   else
      for i, str in pairs (module_help._authors) do
         if i > 1 then
            list_authors = list_authors..", "
         end
         list_authors = list_authors..str
      end
   end
   list_authors = list_authors.."."
   code = code..list_authors.."\n"

   -- Bugs
   code = code..".SH \"REPORTING BUGS\"\n"
   code = code.."Report bugs to <"..module_help._bug..">.\n"

   -- Copyrigth
   code = code..".SH COPYRIGHT\n"
   code = code..module_help._copyright.."\n"

   -- List of other help pages
   code = code..".SH \"SEE ALSO\"\n"
   if Table.get_size(utils._modules) == 1 then
      Utils:scan_dir (Consts.MAQAO_PATH.."/src/plugins/", utils._modules)
   end
   local custom_paths = os.getenv(Consts.ENVVAR_MODULES_PATH)
   if custom_paths ~= nil and custom_paths ~= "" then
      for i, path in pairs(String:split(custom_paths, ":")) do 
         Utils:scan_dir (path, utils._modules)
      end
   end 

   str = ""
   if module ~= nil then
      str = str.."maqao(1)"
   end
   for i, mod in pairs (utils._modules) do
      if mod.name ~= module then
         if str ~= "" then
            str = str..", "
         end

         if mod.name == "grouping" then
            str = str.."maqao-analyze-grouping(1)"			
         else
            str = str.."maqao-"..mod.name.."(1)"
         end
      end		
   end
   code = code..str.."\n"

   -- Print the code
   file_out:write (code)
   file_out:close ()
   os.exit (0)
end




--- Generate a bash file used to handle completion for MAQAO
local function _utils_generate_complete (args)
   if Consts.is_UNIX == false then
      print ("Feature available only for UNIX systems")
      os.exit (1)
   end

   -- Handle the output path
   local output = os.getenv("HOME")
   if output ~= nil or output ~= "" then
      output = output.."/.maqao_complete.sh"
   else
      print ("File name can not been created: $HOME is empty")
      os.exit (1)
   end
   local complete_path = output

   -- Open the file
   print ("Open "..output.." to generate complete code ...")
   file_out = io.open(output, "w");
   if file_out == nil then 
      print ("File ["..output.."] can not been created")
      os.exit (1)	
   end

   -- Code to handle analyze module: As it is used to call submodules (grouping)
   -- the code must we done manually
   -- #PRAGMA_NOSTATIC
   -- Load grouping module (for dynamic version only)
   if fs.exists (Consts.MAQAO_PATH.."/src/plugins/grouping/grouping.lua") then
      dofile (Consts.MAQAO_PATH.."/src/plugins/grouping/grouping.lua")
   end	
   if fs.exists (Consts.MAQAO_PATH.."/src/plugins/analyze/analyze.lua") then
      dofile (Consts.MAQAO_PATH.."/src/plugins/analyze/analyze.lua")
   end	
   -- #PRAGMA_STATIC

   local string = 
   -- Code for Grouping module
   "# Handle completion for maqao analyze --grouping command\n"..
   "function _mymaqao_grouping_()\n"..
   "{\n"..
   "   case \"$line\" in\n"..
   "      ## default\n"..
   "      *)\n"..
   "         COMPREPLY=($( compgen -W '"
   local module_help = grouping:init_help()
   for _, opt in pairs(module_help._options) do
      if opt.long ~= nil and opt.is_sep ~= true then
         string = string.." --"..opt.long
      end
   end
   string = string .. 
   "' -- \"$word\" ));\n"..
   "      return 0;"..
   "      ;;\n"..
   "   esac\n"..
   "}\n"..
   -- Code for analyze module
   "# Handle completion for maqao analyze command\n"..
   "function _mymaqao_analyze_()\n"..
   "{\n"..
   "   case \"$line\" in\n"..
   "      *--grouping*)\n"..
   "         _mymaqao_grouping_\n"..
   "         return 0;\n"..
   "      ;;\n"..
   "      *--list-functions* | *--help* | *--version*)\n"..
   "         return 0;\n"..
   "      ;;\n"..
   "      ## default\n"..
   "      *)\n"..
   "         COMPREPLY=($( compgen -W '"
   local module_help = analyze:init_help()
   for _, opt in pairs(module_help._options) do
      if opt.long ~= nil and opt.is_sep ~= true then
          string = string.." --"..opt.long
      end
   end
   string = string .. 
   "' -- \"$word\" ));\n"..
   "      return 0;"..
   "      ;;\n"..
   "   esac\n"..
   "}\n\n"   
   file_out:write (string)

   -- Iterate over modules to generate function handling each module
   -- Some modules are excluded because they are done manually
   for _, module in pairs (utils._modules) do
      -- header
      if  module.name ~= "analyze"
      and module.name ~= "grouping" then
         local string = "# Handle completion for maqao "..module.name.." command\n"..
         "function _mymaqao_"..module.name.."_()\n"..
         "{\n"..
         "   case \"$line\" in\n"..
         "      ## default\n"..
         "      *)\n"..
         "         COMPREPLY=($( compgen -W '"

         -- list of long options                  
         local module_help = loadstring ("return "..module.name..":init_help()")()

         for _, opt in pairs(module_help._options) do
            if opt.long ~= nil and opt.is_sep ~= true then
               string = string.." --"..opt.long
            end
         end

         -- tail
         string = string ..
         "' -- \"$word\" ));\n"..
         "      return 0;"..
         "      ;;\n"..
         "   esac\n"..
         "}\n\n"

         file_out:write (string)
      end
   end

   -- Generate the main function
   local string = "function _maqao_()\n"..
   "{\n"..
   "   local word=${COMP_WORDS[COMP_CWORD]}\n"..
   "   local line=${COMP_LINE}\n\n"..
   "   # If the last word contains a / char, list files\n"..
   "   case \"$word\" in\n"..
   "      */*)\n"..
   "         if [ \"$COMP_CWORD\" == \"1\" ]; then\n"..
   "             COMPREPLY=($( compgen -f -X \"!*.lua\" -- \"$word\" ));\n"..
   "         else\n"..
   "             COMPREPLY=($( compgen -f -X \"$word\" -- \"$word\" ));\n"..
   "         fi\n"..
   "         return 0;\n"..
   "      ;;\n"..
   "      *module*)\n"..
   "         if [ \"$COMP_CWORD\" == \"1\" ]; then\n"..
   "            COMPREPLY=($( compgen -W '"
   for _, module in pairs (utils._modules) do
      string = string.." module="..module.name
   end
   string = string..
   "' -X \"$word\" -- \"$word\" ));\n"..
   "            return 0;\n"..
   "         fi\n"..
   "      ;;\n"..
   "   esac\n\n"..
   "   case \"${COMP_WORDS[1]}\" in\n"

   for _, module in pairs (utils._modules) do
      string = string..      
      "      \"module\\="..module.name.."\""
      if module.s_alias ~= nil then
         for _, alias in ipairs (module.s_alias) do 
            string = string.." | "..alias
         end
      end
      string = string..")\n"..
      "         _mymaqao_"..module.name.."_\n"..
      "         return 0;\n"..      
      "      ;;\n"
   end   
   string = string..
   "      --module-skeleton)\n"..
   "         COMPREPLY=($( compgen -W 'mod output' -X \"$word\" -- \"$word\" ));\n"..
   "         return 0;\n"..
   "      ;;\n"..
   "      --list-modules | --help | --version)\n"..
   "         return 0;\n"..
   "      ;;\n"..
   "      ## default\n"..
   "      *)\n"..
   "         COMPREPLY=($( compgen -W '"
   -- list modules
   for _, module in pairs (utils._modules) do
      if module.s_alias ~= nil then
         for _, alias in ipairs (module.s_alias) do
            string = string.." "..alias
         end
      end
   end

   -- list options
   module_help = Utils:main_load_help ()
   for _, module in pairs (utils._modules) do
      string = string.." \"module="..module.name.."\""
   end
   
   for _, opt in pairs(module_help._options) do
      if opt.long ~= nil and opt.is_sep ~= true then
         if opt.long:sub(1,1) ~= "<" 
         and opt.long ~= "" then
            string = string.." \"--"..opt.long.."\""
         end
      end
   end
   string = string.."' -X \"$word\" -- \"$word\" ));\n"..
   "         return 0;\n"..
   "      ;;\n"..
   "      esac\n"..
   "}\n"

   file_out:write (string)
   file_out:close ()
   print ("   => complete code generated")

   -- If needed and possible, update .bashrc to add commands used to load 
   -- .maqao_complete.sh
   output = os.getenv("HOME").."/.bashrc"
   print ("Check in "..output.." if MAQAO complete file is loaded ...")
   file_out = io.open(output, "r");
   if file_out == nil then 
      print ("File ["..output.."] can not been opened")
      os.exit (1)	
   end

   -- Look for a piece of code loading maqao_complete.sh
   local found = false

   for line in file_out:lines() do
      -- If the code is not found, add it at the end
      if found == false and string.match(line, utils.complete_start) ~= nil then
         found = true
      end
   end
   file_out:close ()

   -- if bashrc file has not to been updated
   if args["no-bashrc"] == true then
      string = "complete -F _maqao_ maqao madras maqaodev\n"..
      "source "..complete_path

      print ("   => --no-bashrc option used. .bashrc file will not be modified")
      print ("      run the following bash code to load completion for MAQAO:\n")
      print ("# --------------------------------------------\n")
      print (string)
      print ("\n# --------------------------------------------")

      -- nothing to do
   elseif found == true then 
      print ("   => complete function is loaded")

      -- add the file loading at the end
   else
      print ("   => complete function is not loaded")
      print ("   => update "..output.." to load the complete function")		
      file_out = io.open(output, "a");
      if file_out == nil then 
         print ("File ["..output.."] can not been used")
         os.exit (1)	
      end

      string = "\n"..utils.complete_start.."\n"..
      "# DO NOT ADD CODE BETWEEN "..utils.complete_start.." AND "..utils.complete_stop.." lines\n"..
      "complete -d -X '.[^./]*' -F _maqao_ maqao madras maqaodev\n"..
      "source "..complete_path.."\n"..
      utils.complete_stop.."\n"

      file_out:write (string)
      file_out:close ()
   end	
end




local function _utils_unload_complete (args)
   -- Look if .bashrc contains utils.complete_start and utils.complete_stop
   --	-> no:  exit
   --  -> yes: look for the source line to get the complete file location.
   local output = os.getenv("HOME").."/.bashrc"
   print ("Check in "..output.." if MAQAO complete file is loaded ...")
   local file_out = io.open(output, "r");
   if file_out == nil then 
      print ("File ["..output.."] can not been opened")
      os.exit (1)	
   end

   -- Look for a piece of code loading maqao_complete.sh
   local found = false
   local bashrc_to_update = false
   local filename = ""

   for line in file_out:lines() do
      -- Look for the start flag
      if found == false and string.match(line, "^"..utils.complete_start) ~= nil then
         found = true
         bashrc_to_update = true

         -- Look for the stop flag
      elseif found == true and string.match(line, "^"..utils.complete_stop) ~= nil then
         found = false

         -- Look for the source command
      elseif found == true and string.match(line, "source ") ~= nil then
         filename = string.gsub (line, "source ", "")
      end
   end

   if bashrc_to_update == true then
      print ("   => "..output.." loads maqao complete file")
   else
      print ("   => "..output.." does not load maqao complete file")
   end

   if filename ~= "" then
      print ("   => maqao complete file is "..filename)
   else
      print ("   => there is no maqao complete file")
   end
   file_out:close ()

   -- if a file is sourced, removed if
   if filename ~= "" then
      print ("Remove "..filename)
      os.remove (filename)
   end

   -- if bashrc loads a file, update it to not handle maqao complete anymore
   if bashrc_to_update == true then
      print ("Update  "..output.." to not load maqao complete code")
      local bashrc = io.open(output, "r");
      local bashrc_tmp = io.open(output.."_tmp", "w");	
      local is_maqao_complete = false

      for line in bashrc:lines() do
         -- Look for the start flag
         if is_maqao_complete == false and string.match(line, "^"..utils.complete_start) ~= nil then
            is_maqao_complete = true

            -- Look for the stop flag
         elseif is_maqao_complete == true and string.match(line, "^"..utils.complete_stop) ~= nil then
            is_maqao_complete = false

         elseif is_maqao_complete == false then
            bashrc_tmp:write (line.."\n")
         end
      end
      bashrc:close ()		
      bashrc_tmp:close ()
      os.remove (output)
      os.rename (output.."_tmp", output)
   end
end




-- Check in a table of modules (modules) if the module to run exists
-- and run it with parameters (all args (utils.__args) and the  
-- current project (aproject))
local function _utils_checkandrun (aproject, modules)
   for i, module in pairs(modules) do
      -- module=<>
      if (utils.__args.module ~= nil and utils.__args.module == module.name) then
         if utils.__args["generate-man"] then
            _utils_generate_man (utils.__args, module.name)
         elseif utils.__args["generate-wiki"] then
            _utils_generate_wiki (utils.__args, module.name)
         end

         line = "return "..module.name..":"..module.name.."_launch(...)"
         loadstring (line)(utils.__args, aproject)
         return (true)

         -- <module>
      elseif (utils.__args._alias ~= nil and utils.__args._alias == module.name) then
         if utils.__args["generate-man"] then
            _utils_generate_man (utils.__args, module.name)
         elseif utils.__args["generate-wiki"] then
            _utils_generate_wiki (utils.__args, module.name)
         end
         line = "return "..module.name..":"..module.name.."_launch(...)"
         loadstring (line)(utils.__args, aproject)
         return (true)
         
         -- <alias> in a list of aliases
      elseif module.s_alias ~= nil then 
         for i, s_alias in ipairs (module.s_alias) do 
            if utils.__args._alias == s_alias then
               if utils.__args["generate-man"] then
                  _utils_generate_man (utils.__args, module.name)
               elseif utils.__args["generate-wiki"] then
                  _utils_generate_wiki (utils.__args, module.name)
               end
               line = "return "..module.name..":"..module.name.."_launch(...)"
               loadstring (line)(utils.__args, aproject)
               return (true)
            end         
         end

         -- <long alias>
      elseif utils.__args._alias ~= nil then
         if module.l_alias ~= nil then
            for key, data in pairs (module.l_alias) do
               if key == utils.__args._alias then
                  -- Convert the string into a table by splitting on spaces.
                  local largs = String:split(data, " ")
                  local args = Utils:get_args(largs)

                  for arg, val in pairs(args) do
                     if utils.__args[arg] ~= nil
                     and utils.__args[arg] ~= val then
                        Message:warn("Option "..arg.." has been overridden by alias "..key.." with value "..tostring(val))
                     end
                     utils.__args[arg] = val;
                  end
                  if utils.__args["generate-man"] then
                     _utils_generate_man (utils.__args, module.name)
                  elseif utils.__args["generate-wiki"] then
                     _utils_generate_wiki (utils.__args, module.name)
                  end

                  line = "return "..module.name..":"..module.name.."_launch(...)"
                  loadstring (line)(utils.__args, aproject)
                  return (true)
               end
            end
         end
      end
   end
   return (false)
end




local function _utils_run_module (aproject)
   local module_list = {};
   local custom_paths = os.getenv(Consts.ENVVAR_MODULES_PATH)
   local found = false

   -- **** list modules
   -- => return a table with {module, alias, path}
   -- #PRAGMA_NOSTATIC
   -- built-in modules
   Utils:scan_dir (Consts.MAQAO_PATH.."/src/plugins/", module_list, "0")
   -- #PRAGMA_STATIC

   -- user modules
   if custom_paths ~= nil and custom_paths ~= "" then
      for i, path in pairs(String:split(custom_paths, ":")) do 
         Utils:scan_dir (path, module_list, "1")
      end
   end 

   -- current directory
   Utils:scan_dir (os.getenv("PWD"), module_list, "1")


   -- **** dofiles
   -- #PRAGMA_NOSTATIC
   -- built-in modules
   for i, module in pairs(module_list) do
      if module.user == "0" then
         dofile (module.path.."/"..module.name.."/"..module.name..".lua")
      end
   end
   -- #PRAGMA_STATIC

   -- user modules
   for i, module in pairs(module_list) do
      if module.user == "1" then
         package.path=package.path..";"..module.path.."/?.lua"
         dofile (module.path.."/"..module.name.."/"..module.name..".lua")
      end
   end

   -- **** Some checks on data
   -- The module (utils.__args.module) or the alias module (utils.__args.alias) have to be 
   -- in utils.__modules (table to create) containing modules, alias and paths
   -- If found, run the module

   -- Check in static modules
   if found ~= true then
      found = _utils_checkandrun (aproject, utils._modules)
   end

   -- Check in user modules
   if found ~= true then
      found = _utils_checkandrun (aproject, module_list)
   end

   -- If module not found, display an error
   if found ~= true then
      if utils.__args.module ~= nil then
         print ("Module \""..utils.__args.module.."\" not found")
      elseif utils.__args._alias then
         print ("Alias \""..utils.__args._alias.."\" not found")
      end
      os.exit (1)
   end
end





-- Gets the code corresponding to a compiler string
local function compiler_to_code (comp)
   if comp == true or comp == false then
      comp = nil
   end

   if     (comp == nil         ) then return (Consts.COMP_ERR)
   elseif (comp == "GNU"       ) then return (Consts.COMP_GNU)
   elseif (comp == "Intel"     ) then return (Consts.COMP_INTEL)
   else
      print (comp .. " is not a valid compiler");
      print ("Valid compilers: " .. table.concat (utils.avail_compiler_list, " "));
      os.exit (-1);
   end
end




-- Gets the code corresponding to a language string
local function language_to_code (lang)
   if lang == true or lang == false then
      lang = nil
   end

   if     (lang == nil       ) then return (Consts.LANG_ERR)
   elseif (lang == "c"       ) then return (Consts.LANG_C)
   elseif (lang == "c++"     ) then return (Consts.LANG_CPP)
   elseif (lang == "fortran" ) then return (Consts.LANG_FORTRAN)
   else
      print (lang .. " is not a valid language");
      print ("Valid languages: " .. table.concat (utils.avail_language_list, " "));
      os.exit (-1);
   end
end




-- Get the code corresponding to a cc_mode string
local function ccmode_to_code (ccmode_s, ccmode_l)
   local ccmode = nil
   if ccmode_l ~= nil then
      ccmode = ccmode_l
   else
      ccmode = ccmode_s
   end

   if     ccmode == nil           then return (Consts.CCMODE_DEBUG)
   elseif ccmode == "off"         then return (Consts.CCMODE_OFF)
   elseif ccmode == "debug_based" then return (Consts.CCMODE_DEBUG)
   elseif ccmode == "all"         then return (Consts.CCMODE_ALL)
   else
      print (ccmode .. " is not a valid value");
      print ("Valid values: off, debug_based, all");
      os.exit (-1);
   end
end




--Creates a project from parameters
function init_project (args)
   local comp,lang,ccmode;
   local p_name = args.module 
   if p_name == nil then
      p_name = args._alias
   end
   local proj = project.new (p_name)

   if (proj == nil) then
      print ("Cannot create a MAQAO project named \""..p_name.."\"")
      os.exit (-1);
   end

   -- Sets the processor version from the input parameters or the binary
   proj:init_proc_infos(args.bin, args.arch, args.uarch, args.proc);
   
   -- check some parameters
   if (args["enable-debug-vars"] == true) then
      proj:set_option (Consts.PARAM_MODULE_DEBUG, Consts.PARAM_DEBUG_ENABLE_VARS, true);
   end
   if (args["disable-debug"] == true) then
      proj:set_option (Consts.PARAM_MODULE_DEBUG, Consts.PARAM_DEBUG_DISABLE_DEBUG, true);
   end
   if (args["lcore-flow-all"] == true) then
      proj:set_option (Consts.PARAM_MODULE_LCORE, Consts.PARAM_LCORE_FLOW_ANALYZE_ALL_SCNS, true);
   end
   
   proj:set_compiler_code (compiler_to_code (args.compiler))
   proj:set_language_code (language_to_code (args.language))
   proj:set_ccmode        (  ccmode_to_code (args.ifr, args["interleaved-functions-recognition"]))
   return (proj)
end

-- ############################################################################
-- ############################################################################
--
--          Entry point of the program into the LUA part of MAQAO
--
-- ############################################################################
-- ############################################################################
__debug_output__ = true;
utils.__args              = Utils:get_args(arg);
utils.avail_language_list = {"c", "c++", "fortran"};
utils.avail_compiler_list = {"GNU", "Intel"};
utils.complete_start      = "#maqao_complete start"
utils.complete_stop       = "#maqao_complete stop"

-- Enable Debug mode
if utils.__args.debug == nil
and utils.__args.dbg   == nil then
   Debug:disable()
else
   local dbg = nil
   if     utils.__args.debug ~= nil then
      dbg = tonumber (utils.__args.debug)
   elseif utils.__args.dbg   ~= nil then
      dbg = tonumber (utils.__args.dbg)
   end
   
   if dbg == nil then
      Debug:disable()
      Message:warn("Invalid debug level.")
   elseif dbg == 0 then
      Debug:enable(false)
   elseif dbg > 0 then
      Debug:enable(true)
   else 
      Debug:disable()
      Message:warn("Invalid debug level.")
   end
end
Utils:load ()

if utils._modules == nil then
   -- This table is created in static mode.
   -- It contains the list of built-in available modules
   -- In dynamic mode, the table is empty.
   utils._modules = {{name = "madras", s_alias = {"madras"}, path = "built-in", user = 0, is_stub = "false", l_alias = {}}}
end

-- if the commands runned a custom lua script, run it
if (utils.__args.lua_script ~= false) then
   if (fs.exists (utils.__args.lua_script)) then
      os.exit (dofile (utils.__args.lua_script));
   else
      print ("File "..utils.__args.lua_script.." cannot be found")
      os.exit (1)
   end

   -- run madras
elseif (utils.__args.module == "madras" or utils.__args._alias == "madras") then
   if utils.__args["generate-man"] then
      _utils_generate_man (utils.__args, "madras")
   elseif utils.__args["generate-wiki"] then
      _utils_generate_wiki (utils.__args, "madras")
   else
      os.execute ("madras "..utils.__args["c_module_opts"])
   end
   os.exit (0)

-- project management
elseif (utils.__args.module == "create-project" or utils.__args._alias == "create-project") then
   
   local project_name = utils.__args["bin"]
   local main_bin = utils.__args["main-bin"]

   if main_bin == nil then
      Message:critical("You must specify the main binary file with --main-bin=BIN.")
   end

   print("Creating project \""..project_name.."\"...")
   project:create(project_name, main_bin)
   print("Done.")


-- if there is a module (module=<...>) 
-- or an alias (first parameter is a single word)
elseif (utils.__args.module ~= nil or utils.__args._alias ~= nil) then
   local aproject = init_project (utils.__args)
   _utils_run_module (aproject)

   -- if there is a -h or --help
elseif ((utils.__args.help) or (utils.__args.h)) then
   local help = Utils:main_load_help ()
   help:print_help ()
   os.exit (0)

   -- if there is -v or --version
elseif ((utils.__args.version) or (utils.__args.v)) then
   local help = Utils:main_load_help ()
   help:print_version (true, utils.__args.verbose)
   os.exit (0)

   -- if there is --list-modules
elseif utils.__args["list-modules"] then
   Utils:list_modules (utils.__args)

   -- if there is --list-procs
elseif utils.__args["list-procs"] then
   _utils_list_procs ()

   -- if there is --module-skeleton
elseif utils.__args["module-skeleton"] then
   _utils_module_skeleton (utils.__args)

   -- if there is --generate-complete
elseif utils.__args["generate-complete"] then
   _utils_generate_complete (utils.__args)

   -- if there is --unload-complete
elseif utils.__args["unload-complete"] then
   _utils_unload_complete (utils.__args)

   -- a manpage must be generated
elseif utils.__args["generate-man"] then
   _utils_generate_man (utils.__args, nil)
elseif utils.__args["generate-wiki"] then
   _utils_generate_wiki (utils.__args, nil)
else
   print ("\nNo command provided. Use maqao -h to get more help\n");
end
