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


module ("analyze", package.seeall)

require "analyze.list_functions"
require "analyze.list_loops"
require "analyze.list_instructions"

--- Prints the grouping analysis help
-- @return An initialized Help object
function analyze:init_help ()
   local help = Help:new()
   help:set_name ("maqao-analyze")
   help:set_usage ("maqao analyze -lf|-ll|-li|-g <binary> [options]")
   help:set_description ("This module displays the results of the static analysis performed on the binary.")
   
   help:add_option ("list-functions", "lf", nil, false, 
               "List all functions in the binary.")

   help:add_option ("list-loops", "ll", nil, false, 
               "List all loops per function in the binary.")

   help:add_option ("list-instructions", "li", nil, false,
               "List all instructions per function and loop in the binary.")

   help:add_separator ("Grouping analysis")
   help:add_option ("grouping",       "g", nil, false, "Run grouping analysis. Type maqao analyze -g --help for more detailed help")

   help:add_separator ("Filtering results")
   help:add_option ("fct", nil, "<function>", false, 
               "Filter results using <function>, which is interpreted as a Lua regular expression.\n"..
               "The regular expression format is available in the Lua 5.1 documentation.\n"..
               "For --list-functions, this will restrict the output to the functions with a matching name.\n"..
               "For --list-loops and --list-instructions, this will restrict the output to respectively\n"..
               "the loops and instructions contained in a function with a matching name.")
   help:add_option ("loop-ids", nil, "<vals>", false,
               "Filter results using <vals>, expected as a list of loop identifiers separated by commas (',').\n"..
               "For --list-loops, this will restrict the output to the loops whose identifier is in the list.\n"..
               "For --list-instructions, this will restrict the output to the instructions\n"..
               "contained in a loop whose identifier is in the list.")
   help:add_option ("loop-depth", nil, "<val>", false,
               "Filter results to only get loops with a given hierarchy level.\n"..
               "<.br>If <val> is an integer, only loops with a depth of <val> are displayed.\n"..
               "<.br>If <val> is \"innermost\", \"in-between\", or \"outermost\", only <val> loops\n"..
               "will be displayed.")

   help:add_separator ("Output display")
   help:add_option ("show-hierarchy", nil, "<show>", false,
               "Allows to display the results following loop hierarchy instead of as a flat list\n"..
               "It is only used when listing loops or instructions.",
               {{name = "on", desc = "Enabling", default=true}, {name = "off", desc = "Disabling"}})
   help:add_option ("show-extra-info", nil, "<show>", false,
               "Display extra information: file and source lines if available, and assembly ranges for functions, loops and blocks.",
               {{name = "on", desc = "Enabling"}, {name = "off", desc = "Disabling", default=true}})
   help:add_option ("show-empty-functions", nil, "<show>", false,
               "Displays a function name even if it does not contain a matching loop.\n"..
               "This option is ignored when listing instructions.",
               {{name = "on", desc = "Enabling", default=true}, {name = "off", desc = "Disabling"}})

   Utils:load_common_help_options (help)

   return help
end

-- Sets a boolean option into the configuration from the arguments
local function set_boolean_option(config, args, option, default)
   if args[option] ~= nil and args[option] ~= "" then
      if args[option] == "off" then
         config[option] = "off"
      else
         config[option] = "on"
      end
   else
      config[option] = default
   end
end

local function create_config(args)
   local config = {}

   config["bin"] = args.bin

   -- List mode
   if args["lf"] ~= nil or args["list-functions"] ~= nil then
      config["list"] = "functions"
   elseif args["ll"] ~= nil or args["list-loops"] ~= nil then
      config["list"] = "loops"
   elseif args["li"] ~= nil or args["list-instructions"] ~= nil then
      config["list"] = "instructions"
   elseif args["g"] ~= nil or args["grouping"] ~= nil then
      config["list"] = "grouping"
   end

   -- Filters
   config["fct"]        = args["fct"]

   local loop_filter_ids = nil
   local loop_filter_depth_name = nil
   local loop_filter_depth_level = nil
   if args["loop-ids"] ~= nil and args["loop-ids"] ~= "" then
      local found_valid_filter = false
      loop_filter_ids = {}
      for lid in string.gmatch(args["loop-ids"], "[^,]+") do
         if lid == nil then
            Message:warning("Ignoring empty value in loop-ids filter")
         elseif tonumber(lid) == nil then
            Message:warning("Ignoring invalid loop-ids filter value "..lid.." (number expected).")
         else
            table.insert(loop_filter_ids, tonumber(lid))
            found_valid_filter = true   --At least one of the values in the filter is valid
         end
      end
      if found_valid_filter == false then
         Message:warning("Invalid value for loop-ids: list of numerical identifiers expected. Ignoring filter")
         loop_filter_ids = nil
      end
   end

   if args["loop-depth"] ~= nil and args["loop-depth"] ~= "" then
      if args["loop-depth"] == "innermost" or args["loop-depth"] == "outermost" then
         loop_filter_depth_name = args["loop-depth"]
      elseif string.match (args["loop-depth"], "^[0-9]+$") then
         loop_filter_depth_level = tonumber(args["loop-depth"])
      else
         Message:warning("Invalid value for loop-depth: only innermost, outermost, or <depth level> accepted. Ignoring filter.")
      end
   end
   if loop_filter_ids ~= nil or loop_filter_depth_name ~= nil or loop_filter_depth_level ~= nil then
      config["loop-filters"] = {}
      config["loop-filters"]["ids"]           = loop_filter_ids
      config["loop-filters"]["depth-name"]    = loop_filter_depth_name
      config["loop-filters"]["depth-level"]   = loop_filter_depth_level
   end

   -- Display
   set_boolean_option(config, args, "show-hierarchy",        "on")
   set_boolean_option(config, args, "show-extra-info",       "off")
   set_boolean_option(config, args, "show-empty-functions",  "on")

   -- Help & version
   config["help"]    = args["h"] or args["help"]
   config["version"] = args["v"] or args["version"]

   config["arch"] = args["arch"]

   return config
end

-- Checks that a function matches with the filters specified in arguments
function analyze:fct_matches(config, fct)
   local fct_filter = config["fct"]

   if fct_filter == nil or fct_filter == "" then
      return true -- No filter
   end

   local name    = fct:get_name() or ""
   local demname = fct:get_demname() or ""

   if string.match (name, fct_filter) or string.match(demname, fct_filter) then
      return true
   else
      return false
   end
end

-- Checks that a loop matches with the filters specified in arguments
function analyze:loop_matches(config, loop)
   
   if config["loop-filters"] == nil then
      return true -- No filters
   end

   local lids_filter          = config["loop-filters"]["ids"]
   local depth_name_filter    = config["loop-filters"]["depth-name"]
   local depth_level_filter   = config["loop-filters"]["depth-level"]

   -- Checks depth name filter
   if (depth_name_filter == "innermost" and loop:is_innermost ())
   or (depth_name_filter == "outermost" and loop:is_outermost ()) then
      return true
   end

   -- Check depth level filter
   if depth_level_filter == loop:get_depth() then
      return true
   end
   
   -- Checks loop id filter
   if (lids_filter ~= nil) then
      for _, lid in pairs(lids_filter) do
         if lid == loop:get_id() then
            return true
         end
      end
   end
   -- At least one filter is defined and none of them returned true: it is not a match
   return false
end

local function open_asmfile(config, aproject)
   local binary_name = config["bin"]
   
   if binary_name == nil then
      Message:print ("No input file name provided");
      Message:print ("\nUSAGE --------------------------------------------");
      Message:print ("\tmaqao analyze --list-"..config["list"].." <binary or formatted assembly file>\n");
      os.exit (1);
   end

   local asmfile;
   local filetype;
   if (string.find (string.lower (binary_name), "%.s$") ~= nil) then
      -- TODO: handle properly args.arch in MAQAO core
      asmfile = aproject:load_txtfile (binary_name, config.arch, aproject:get_uarch_name());
      filetype = "assembly";
   else
      asmfile = aproject:load (binary_name, aproject:get_uarch_name());
      filetype = "binary";
   end
   if (asmfile == nil) then
      Message:error("Cannot analyze the "..filetype.." file "..binary_name);
      os.exit (1);
   end
   config["asmfile"] = asmfile
end

function analyze:analyze_launch (args, project)

   local config = create_config(args)
   
   -- Runs grouping
   if config["list"] == "grouping" then
      grouping:grouping_launch (config, project)

   -- Prints help
   elseif config["help"] ~= nil then
      local help = analyze:init_help();
      help:print_help (true, args.verbose);

   -- Prints version
   elseif config["version"] ~= nil then
      local help = analyze:init_help();
      help:print_version (true, args.verbose);

   -- Prints functions
   elseif config["list"] == "functions" then
      open_asmfile(config, project)
      analyze:list_functions(config)

   -- Prints loops
   elseif config["list"] == "loops" then
      open_asmfile(config, project)
      analyze:list_loops(config)

   -- Prints instructions
   elseif config["list"] == "instructions" then
      open_asmfile(config, project)
      analyze:list_instructions(config)

   -- No valid parameter given: printing help
   else
      local help = analyze:init_help();
      help:print_help (true, args.verbose);
   end

end
