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

--- Module cqa, related to the CQA tool
-- Defines the cqa_launch function which is called when the cqa module is invoked from the command line
module ("cqa", package.seeall)

-- TODO: try to factorize analyze_requested_loops and analyze_all_innermost_loops

require "cqa.help";
require "cqa.consts";
require "cqa.output.json";
require "cqa.output.txt";
require "cqa.output.html";
require "cqa.output.csv";

require "cqa.api.data_struct";
require "cqa.api.innermost_loops";
require "cqa.api";
require "cqa.api.mod";


-- #PRAGMA_NOSTATIC MEM_PROJ
local function load_cqa_tab (files)
   -- Loads database files: first one for instructions and second one for patterns
   local file_path_insn;
   local file_path_pattern;

   local cpt = 1;
   for file_path in string.gmatch (files, "[^,]+") do
      if (cpt > 2) then break end

      if (cpt == 1) then
         file_path_insn = file_path;
      else
         file_path_pattern = file_path;
      end

      local fp, err = io.open (file_path);
      if (fp == nil and cpt == 1) then
         -- critical: will exit
         Message:display (cqa.consts.Errors["CANNOT_OPEN_INSN_CQA_TAB_FILE"], file_path, err);
      elseif (fp == nil) then
         Message:display (cqa.consts.Errors["CANNOT_OPEN_PATTERN_CQA_TAB_FILE"], file_path, err);
      else
         io.close (fp);
         dofile (file_path);
      end

      cpt = cpt + 1;
   end

   -- if cannot find expected tables in loaded files
   if (type (__ubench_cycles) ~= "table") then
      -- critical: will exit
      Message:display (cqa.consts.Errors["CANNOT_FIND_INSN_CQA_TAB"], file_path_insn);
   elseif (file_path_pattern ~= nil and type (__ubench_cycles_pattern) ~= "table") then
      -- not critical: will continue
      Message:display (cqa.consts.Errors["CANNOT_FIND_PATTERN_CQA_TAB"], file_path_pattern);
   end
end

local function get_all_memory_levels ()
   -- If found
   if (type (__ubench_cycles_memory_levels) == "table") then
      return __ubench_cycles_memory_levels
   end

   -- not critical: will continue
   Message:display (cqa.consts.Errors["CANNOT_READ_MEMORY_LEVELS_LIST"])

   local _,first_insn = next (__ubench_cycles)
   local _,first_size = next (first_insn)
   local _,first_drct = next (first_size)

   local mem_lvls = {}
   for k in pairs (first_drct) do
      table.insert (mem_lvls, k)
   end

   return mem_lvls
end
-- #PRAGMA_STATIC

--- [local function, used only by cqa_launch]
-- Checks whether a function must be analyzed
-- @param fct a function
-- @param functions_list string listing functions to analyze. The comma is used as separator
-- @param pattern_found table to mark found patterns
-- @return boolean
local function to_analyze (fct, functions_list, pattern_found)
   if (functions_list == nil) then return false end

   -- If not nil, fct is a connected component related to another function and typicaly corresponds to an OpenMP region
   local original_function = fct:get_original_function();

   -- First look for a user pattern matching with the demangled name
   local dem_name;
   if (original_function == nil) then
      dem_name = fct:get_demname(); -- demangled name (as in the source code)
   else
      dem_name = original_function:get_demname();
   end

   if (dem_name ~= nil) then
      for pattern in string.gmatch (functions_list, "[^,]+") do
         if (string.find (dem_name, pattern) ~= nil) then

            if (original_function ~= nil) then
               Message:info (string.format ("%s is related to %s\n", fct:get_demname(), dem_name));
            end

            -- Mark the pattern as found
            pattern_found [pattern] = true;

            return true;
         end
      end
   end

   -- If not previously found, look for a user pattern matching with the mangled/binary name
   local bin_name;
   if (original_function == nil) then
      bin_name = fct:get_name(); -- name of the corresponding label in the binary code
   else
      bin_name = original_function:get_name();
   end

   for pattern in string.gmatch (functions_list, "[^,]+") do
      if (string.find (bin_name, pattern) ~= nil) then
         Debug:warn ("Found a function whose binary name matches but demangled name does not. Your binary was probably compiled without -g or you directly entered the binary name.");

         if (original_function ~= nil) then
            Message:info (string.format ("%s is related to %s\n", fct:get_name(), bin_name));
         end

         -- Mark the pattern as found
         pattern_found [pattern] = true;

         return true;
      end
   end

   return false;
end

local of, op; -- options living only in this module (CQA tool)

-- Table containing options and data common to all analyzed loops
-- Supposed not modified by functions taking it as parameter (cqa:get_cqa_results...)
local cqa_context;

local function analyze_blocks (f, blocks, loops_csv_file, html_reports)

   local cqa_results = cqa:get_cqa_results (blocks, cqa_context);

   -- Push results on a CSV file and/or the standard output
   if (of == "csv") then
      -- CSV file
      local csv_file;
      if (loops_csv_file == nil) then
         csv_file = cqa:init_csv (f:get_name()..".csv", cqa_context);
      else
         csv_file = loops_csv_file;
      end

      if (csv_file ~= nil) then
         cqa:push_to_csv (cqa_results, csv_file, {});

         if (loops_csv_file == nil) then
            csv_file:close();
         end
      end
   elseif (of == "txt") then
      cqa.output.txt.print_reports (f, cqa_results);
   elseif (of == "html") then
      cqa.output.html.print_reports (f, cqa_results, html_reports);
   end

   cqa:clear_context_cache (cqa_context, f)
end

--- [Local function, used only by cqa_launch]
-- Analyzes all innermost loops of the 'f' function
-- @param f function
-- @param loops_csv_file loops.csv descriptor, dedicated to requested loops
-- @param html_reports HTML reports table, common for all functions
local function analyze_all_innermost_loops (f, loops_csv_file, html_reports)
   local innermost_loops = cqa.api.innermost_loops.get_innermost_loops (f);

   if (#innermost_loops == 0) then
      Message:info ("No innermost loops in the function "..f:get_name());
      return;
   end

   local cqa_results = {};
   for _,l in ipairs (innermost_loops) do
      cqa_results [l:get_id()] = cqa:get_cqa_results (l, cqa_context)
   end

   -- Push results on a CSV file and/or the standard output
   if (of == "csv") then
      -- CSV file
      local csv_file;
      if (loops_csv_file == nil) then
         csv_file = cqa:init_csv (f:get_name()..".csv", cqa_context);
      else
         csv_file = loops_csv_file;
      end

      if (csv_file ~= nil) then
         for _,l in ipairs (innermost_loops) do
            cqa:push_to_csv (cqa_results [l:get_id()], csv_file);
         end

         if (loops_csv_file == nil) then
            csv_file:close();
         end
      end
   elseif (of == "txt") then
      cqa.output.txt.print_reports (f, cqa_results);
   elseif (of == "html") then
      cqa.output.html.print_reports (f, cqa_results, html_reports, nil);
   end

   cqa:clear_context_cache (cqa_context, f)
end

--- [Local function, used only by cqa_launch]
-- Analyzes loops in the 'f' function (if requested ID)
-- @param f function
-- @param loop_found table to mark found loops
-- @param loops_csv_file loops.csv descriptor, dedicated to requested loops
-- @param is_requested_loop table referencing loops to analyze
-- @param html_reports HTML reports table, common for all functions
local function analyze_requested_loops (f, loop_found, loops_csv_file, is_requested_loop, html_reports)
   local at_least_one = false; -- true if at least one requested loop in f

   -- Check if at least one requested loop in f
   for l in f:loops() do
      if (is_requested_loop [l:get_id()]) then
         at_least_one = true;
         break;
      end

      local first_insn = l:get_first_entry():get_first_insn();
      local addr = string.format ("0x%x", first_insn:get_address());
      if (is_requested_loop [addr]) then
         at_least_one = true;
         break;
      end
   end

   -- If no requested loop to analyze, return
   if (not at_least_one) then return end

   local requested_loops = {};
   local cqa_results = {};

   --for each loop
   for l in f:loops() do
      local lid = l:get_id();
      local first_insn = l:get_first_entry():get_first_insn();
      local addr = string.format ("0x%x", first_insn:get_address());

      -- Analyze the current innermost loop if its ID was requested
      if (is_requested_loop [lid] or is_requested_loop [addr]) then
         cqa_results [lid] = cqa:get_cqa_results (l, cqa_context);

         local sr = cqa_results [lid];
         if (is_requested_loop [lid]) then
            loop_found [lid] = true;
         else
            loop_found [addr] = true;
         end

         table.insert (requested_loops, lid);

         -- Push results on a CSV file and/or the standard output
         if (loops_csv_file ~= nil) then
            cqa:push_to_csv (sr, loops_csv_file);
         end
      end -- if found matching loop
   end --for each innermost loop

   -- If no requested loop to analyze, return
   if (next (requested_loops) == nil) then return end

   if (of == "txt") then
      cqa.output.txt.print_reports (f, cqa_results, requested_loops);
   elseif (of == "html") then
      cqa.output.html.print_reports (f, cqa_results, html_reports, requested_loops);
   end

   cqa:clear_context_cache (cqa_context, f)
end

--- Entry point of the cqa module/tool
-- @param args Arguments from batch server
-- @param proj the MAQAO project
function cqa:cqa_launch (args, proj)

   --------------------------------------------------------------------------------
   --                         Command line checking                              --
   --------------------------------------------------------------------------------

   -- Retrieves the architecture 

   local function get_CQA_unsupported_uarchs ()
      local not_excluded_uarchs = {}
      --[[ Example: {
         -- #PRAGMA_NOSTATIC CQA_FOO --
         FOO = true,
         -- #PRAGMA_STATIC
         }
      --]]

      local is_excluded = {}
      for _,uarch in ipairs (cqa.consts.potentially_excluded_uarchs) do
         if (not_excluded_uarchs [uarch]) then
            is_excluded [uarch] = false
         else
            is_excluded [uarch] = true
         end
      end

      return is_excluded
   end

   local function get_valid_uarchs (arch)
      local is_excluded = get_CQA_unsupported_uarchs()
      local valid_uarchs = {}

      for _,uarch in ipairs (arch:get_uarchs()) do
         local uarch_name = uarch:get_name()
         if (is_excluded [uarch_name] == nil or is_excluded [uarch_name] == false) then
            table.insert (valid_uarchs, uarch_name)
         end
      end

      return table.concat (valid_uarchs, ", ")
   end

   -- For options with 2 aliases, returns the corresponding value
   local function get_opt (alias1, alias2)
      if (args [alias1] ~= nil) then return args [alias1] end
      return args [alias2];
   end

   -- For options with 2 aliases, checks whether it is set
   local function is_opt_set (alias1, alias2)
      return (args [alias1] ~= nil or args [alias2] ~= nil);
   end

   -- If help was requested, print the detailed usage
   if (is_opt_set ("help", "h")) then
      local help = cqa:init_help ()
      help:print_help (true, args.verbose);
      os.exit (0)
   end

   -- If version was requested, print the detailed version
   if (is_opt_set ("version", "v")) then
      local help = init_help ()
      help:print_version (true, args.verbose);
      os.exit (0)
   end

   -- Check if the user just wants to return the JSON file
   if (is_opt_set ("list-metrics", "lm")) then
      cqa.output.json.get_json_metrics_list ();
      os.exit (0)
   end

   -- Check if fct used, now deprecated
   if (args.fct ~= nil) then
      Message:warn ("\'fct\' was renamed to \'fct-loops\'. Old syntax is now deprecated.\n");
      args["fct-loops"] = args.fct;
      args.fct = nil;
   end

   cqa_context = cqa.api.data_struct.new ("cqa_context");

   -- [ADVANCED FEATURE] Custom micro-architecture model
   local um = get_opt ("uarch-model", "um");
   if (um ~= nil) then
      local f = io.open (um, "r");
      if (f == nil) then
         Message:display (cqa.consts.Errors["CANNOT_OPEN_UARCH_MODEL_FILE"], um);
      else
         dofile (um)
         cqa_context.uarch_consts = __user_uarch_consts;
         io.close (f);
      end
   end

   -- Check whether all mandatory options are present
   if ((args.bin == nil) or (not is_opt_set ("fct-loops", "fl") and
                             not is_opt_set ("fct-body", "f") and
                             not is_opt_set ("loop", "l") and
                             not is_opt_set ("path", "p"))) then
      
	     Message:display (cqa.consts.Errors["MISSING_MANDATORY_OPTIONS"]);

   end

   -- Check whether only one option is specified between fct-loops, fct-body, loop, path
   local nb_present = 0;
   if (is_opt_set ("fct-loops", "fl")) then nb_present = nb_present + 1 end
   if (is_opt_set ("fct-body" , "f" )) then nb_present = nb_present + 1 end
   if (is_opt_set ("loop"     , "l" )) then nb_present = nb_present + 1 end
   if (is_opt_set ("path"     , "p" )) then nb_present = nb_present + 1 end
   if (nb_present > 1) then
      Message:display (cqa.consts.Errors["MORE_THAN_ONE_OBJECT_TYPE"]);
   end

   -- Check if the binary exists and can be read
   local f, err = io.open (args.bin);
   if (f == nil) then
      Message:display (cqa.consts.Errors["CANNOT_OPEN_BINARY"], err);
   else
      io.close (f);
   end
   
   cqa_context.for_host = (args.uarch == nil and args.proc == nil);
   cqa_context.if_vectorized_options = {};

   local ivo = get_opt ("if-vectorized-options", "ivo");
   if (ivo ~= nil) then
      for pattern in string.gmatch (ivo, "[^,]+") do
         cqa_context.if_vectorized_options[pattern] = true;
      end
   end

   local im = get_opt ("instructions-modifier", "im");
   if (im ~= nil) then
      im = string.gsub (string.lower (im), "fp", "FP");

      cqa_context.insn_modifier = cqa.api.mod[im];
      if (cqa_context.insn_modifier == nil) then
         local avail = {};
         for k in pairs (cqa.api.mod) do
            -- exclude internal pairs (_NAME, _PACKAGE)
            if (string.find (k, "^_") == nil) then
               table.insert (avail, string.upper (k))
            end
         end
         Message:display (cqa.consts.Errors["INVALID_INSTRUCTION_MODIFIER"], table.concat (avail, ','));
      end

      cqa_context.insn_modifier_options = {};
      local imo = get_opt ("instructions-modifier-options", "imo");
      if (imo ~= nil) then
         for pattern in string.gmatch (imo, "[^,]+") do
            cqa_context.insn_modifier_options[pattern] = true;
         end
      end
   end

   of = get_opt ("output-format", "of");
   if (of ~= nil and of ~= "txt" and of ~= "csv" and of ~= "html") then
      Message:display (cqa.consts.Errors["INVALID_OUTPUT_FORMAT"], of);
   end

   op = get_opt ("output-path", "op");

   -- If a CSV output file is specified (name ending with .csv)
   if (op ~= nil and string.find (op, "%.csv$") ~= nil) then
      if (of == nil) then of = "csv"
      elseif (of == "txt" or of == "html") then
         Message:display(cqa.consts.Errors["UNEXPECTED_CSV_OUTPUT_PATH"]);
      end
   elseif (of == nil) then
      of = "txt";
   end

   -- Get and check confidence levels (drives reports generation/display)
   if (of == "csv") then
      cqa_context.confidence = {};
   elseif (of == "html") then
      cqa_context.confidence = {"gain", "potential", "hint", "expert"};
   elseif (of == "txt") then
      local conf = get_opt ("confidence-levels", "conf");

      if (conf == nil) then
         cqa_context.confidence = {"gain", "potential"};
      elseif (conf == "all") then
         cqa_context.confidence = {"gain", "potential", "hint", "expert"};
      else
         cqa_context.confidence = {};
         for pattern in string.gmatch (conf, "[^,]+") do
            if (pattern == "gain" or pattern == "potential" or
                pattern == "hint" or pattern == "expert" or pattern == "brief") then
               table.insert (cqa_context.confidence, pattern);
            else
               Message:display(cqa.consts.Errors["INVALID_CONFIDENCE_LEVEL"], pattern);
            end
         end
      end
   end

   local opr = get_opt ("opt-report", "opr");
   if (opr ~= nil) then
      cqa_context.opr = {};
      for pattern in string.gmatch (opr, "[^,]+") do
         table.insert (cqa_context.opr, pattern);
      end
   end

   -- [ADVANCED FEATURE] Override internal constants default values
   local dist = tonumber (get_opt ("loop-distance", "dist"));
   if (dist ~= nil) then cqa.consts.LOOP_MERGE_DISTANCE = dist end

   local max_paths = tonumber (get_opt ("max-paths-nb", "max_paths"));
   cqa_context.max_paths = max_paths or cqa.consts.MAX_NB_PATHS

   -- [ADVANCED FEATURE] For multiple paths loops, consider all blocks as belonging to a unique path
   cqa_context.ignore_paths = is_opt_set ("ignore-paths", "igp");

   -- [ADVANCED FEATURE] Follow calls
   local follow_calls = get_opt ("follow-calls", "fc");
   if (follow_calls ~= nil) then
      cqa_context.follow_calls = follow_calls;
      Message:info ("follow-calls used. Interpret results carefully:");
      Message:info (" - instructions in called functions are not taken into account for RecMII");
      Message:info (" - paths are ignored inside called functions");
   end

   -- [ADVANCED FEATURE] Use of block selection function (must be defined in loop_blocks_selection_functions.lua in the working directory)
   local sb = get_opt ("select-blocks", "sb");
   if (sb ~= nil) then
      local f = io.open ("loop_blocks_selection_functions.lua", "r");
      if (f == nil) then
         Message:display (cqa.consts.Errors["CANNOT_OPEN_BLOCKS_SELECTION_FILE"]);
      else
         io.close (f);
      end

      dofile ("loop_blocks_selection_functions.lua");

      cqa_context.blocks = loadstring ("return "..sb.."(...)");
   end

   -- [ADVANCED FEATURE] Virtual unrolling: simulates complete unrolling of innermost
   -- loops in a parent loop
   local vu = get_opt ("virtual-unrolling", "vu");
   if (vu ~= nil) then
      cqa_context.vu = tonumber (vu);
   end

   -- [ADVANCED FEATURE] Instructions extensions bypass (must be defined in user.lua in the working directory)
   local ieb = get_opt ("insn-ext-bypass", "ieb");
   if (ieb ~= nil) then
      local f = io.open (ieb, "r");
      if (f == nil) then
         Message:display (cqa.consts.Errors["CANNOT_OPEN_INSN_EXT_BYPASS_FILE"]);
      else
         io.close (f);
      end

      dofile (ieb);
   end

   cqa_context.memory_level = { "L1" }

-- #PRAGMA_NOSTATIC MEM_PROJ
   -- [ADVANCED FEATURE] Use of microbenchmarks data to project cycles to L2/L3/RAM
   local mlf = get_opt ("memory-level-filepath", "mlf");
   if (mlf ~= nil) then load_cqa_tab (mlf) end
   cqa_context.memory_level_filepath = mlf;

   local ml = get_opt ("memory-level", "ml");
   if (ml == "all") then
      cqa_context.memory_level = get_all_memory_levels()
   elseif (ml ~= nil) then
      cqa_context.memory_level = {};
      for memory_level in string.gmatch (ml, "[^,]+") do
         table.insert (cqa_context.memory_level, memory_level);
      end      
   end

   if (ml ~= nil and mlf == nil) then
      print ("ubench file required: add mlf/memory-level-filepath flag")
      os.exit(-1);
   end
-- #PRAGMA_STATIC

   -- [ADVANCED FEATURE] Use of user data to intercept/modify each CSV output line
   local ud = get_opt ("user-data", "ud");
   if (ud ~= nil and of == "csv") then
      local f, err = io.open (ud);
      if (f == nil) then
	 Message:display (cqa.consts.Errors["CANNOT_OPEN_USER_DATA_FILE"], ud, err);
      else
	 io.close (f);
	 dofile (ud);

	 -- Abort execution in case of bad table format
	 if (type(__cqa_user_data) ~= "table") then
	    Message:display (cqa.consts.Errors["INVALID_USER_DATA"], "__cqa_user_data must be a table");
	 end
	 if (type(__cqa_user_data.requested_metrics) ~= "table") then
	    Message:display (cqa.consts.Errors["INVALID_USER_DATA"], "__cqa_user_data.requested_metrics must be a table");
	 end
         cqa:add_user_metrics (__cqa_user_data)
	 cqa_context.user_data = __cqa_user_data;
      end
   elseif (ud ~= nil and of ~= "csv") then
      Message:warn ("In this release, user data has only effect on CSV output")
   end

   -- [ADVANCED FEATURE] Data access stride analysis report
   -- TODO: remove this (enable this by default) as soon as more reliable
   cqa_context.sr = get_opt ("enable-stride-report", "sr")

   -- Load the binary and add it into 'proj'
   local bin
   if (string.find (string.lower (args.bin), "%.s$") ~= nil) then
      local is_valid, valid_arch_name = {}, {}
      for i,arch in ipairs (get_archs_list()) do
         is_valid [arch:get_name()] = true
         valid_arch_name [i] = arch:get_name()
      end

      if (is_valid [args.arch]) then
         bin = proj:load_txtfile (args.bin, args.arch);
      elseif (args.arch == nil) then
         Message:display (cqa.consts.Errors["MISSING_ARCH"], table.concat (valid_arch_name, ", "))
      else
         Message:display (cqa.consts.Errors["INVALID_ARCH"], table.concat (valid_arch_name, ", "))
      end
   else
      bin = proj:load (args.bin);
   end
   if (bin == nil) then
      Message:display (cqa.consts.Errors["CANNOT_LOAD_BINARY"], args.bin);
   end

   -- Get the architecture (from the binary)
   cqa_context.arch = bin:get_arch_name();
   cqa_context.arch_obj = bin:get_arch_obj();

   -- Get the micro-architecture (from the binary)
   cqa_context.uarch = bin:get_uarch_id();
   cqa_context.uarch_name = bin:get_uarch_name();

--TODO (2017-01-13) Do we really need this???
   if (cqa_context.uarch == Consts.UARCH_NONE) then
      local tmp_arch;
      if args.arch ~= nil then
         tmp_arch = get_arch_by_name(args.arch);
      end
      if tmp_arch == nil then
         tmp_arch = get_file_arch(args.bin) or proj:get_arch();
      end
      print("Unsupported CPU detected. Please re-run with uarch={"..get_valid_uarchs (tmp_arch).."}");
      os.exit(0);
   end

   if (args.proc ~= nil or args.uarch == nil) then
      cqa_context.proc = bin:get_proc();
   end

   -- Check the micro-architecture (check if CQA-blacklisted)
   local arch_name = get_arch_by_name (cqa_context.arch)
   local CQA_unsupported_uarchs = get_CQA_unsupported_uarchs()
   if (CQA_unsupported_uarchs [cqa_context.uarch_name]) then
      local valid_uarchs = get_valid_uarchs (arch_name)
      if (cqa_context.proc ~= nil) then
         Message:display (cqa.consts.Errors["UNSUPPORTED_PROC_UARCH"], args.proc, valid_uarchs);
      else
         Message:display (cqa.consts.Errors["UNSUPPORTED_UARCH"], args.uarch, valid_uarchs);
      end
   end   

   -- Set metrics to compute
   local requested_metrics = {}
   if (of == "csv" and cqa_context.user_data == nil) then
      for _,v in ipairs (cqa.output.csv.CSV) do
         table.insert (requested_metrics, v)
      end

      local arch_key
      arch_key = "arm64"

      for _,metric in ipairs (cqa.output.csv.CSV_arch_dep [arch_key]) do
         table.insert (requested_metrics, metric)
      end

   elseif (of == "csv" and cqa_context.user_data ~= nil) then
      for _,v in ipairs (cqa_context.user_data.requested_metrics) do
         table.insert (requested_metrics, v)
      end

   elseif (of == "txt") then
      local report_names = { "header" }
      if (type (cqa_context.confidence) == "table") then
         for i,v in ipairs (cqa_context.confidence) do
            report_names [i+1] = v
         end
      end
      requested_metrics = cqa:get_requested_metrics_for_reports (report_names)
      table.insert (requested_metrics, "src loop")

   elseif (of == "html") then
      local report_names = {"header", "gain", "potential", "hint", "expert"}
      requested_metrics = cqa:get_requested_metrics_for_reports (report_names)
      table.insert (requested_metrics, "src loop")
   end
   cqa_context.requested_metrics = requested_metrics


   --------------------------------------------------------------------------------
   --                                Binary analysis                             --
   --------------------------------------------------------------------------------

   local pattern_found = {}; -- used to mark found functions patterns
   local loop_found    = {}; -- used to mark found loops
   local loops_csv_file;     -- loops.csv descriptor, dedicated to requested loops
   local html_reports  = Table:new();

   local is_requested_loop;
   if (is_opt_set ("loop", "l")) then
      is_requested_loop = {};
      for lid in string.gmatch (get_opt ("loop", "l"), "[^,]+") do
         if (string.find (lid, "^0x") == nil) then
            if (tonumber (lid) ~= nil) then
               is_requested_loop [tonumber (lid)] = true;
            else
               Message:warn (string.format ("[Analysis skipped] Invalid loop identifier: %s. Expects decimal number or hexadecimal address prefixed with 0x", lid))
            end
         else -- address (hexa)
            local addr = string.match (lid, "^0x(.*)")
            if (#addr > 0 and string.find (addr, "%X") == nil) then -- %X matches non hexadecimal characters (complement of %x)
               is_requested_loop [string.lower(lid)] = true;
            elseif (#addr > 0) then
               Message:info (string.format ("[Analysis skipped] Invalid loop address: %s. Contains non hexadecimal digits", lid))
            else
               Message:info ("[Analysis skipped] Unspecified/empty loop address. Expected example: 0x18cd0")
            end
         end
      end
   end

   if (of == "csv" and (is_opt_set ("loop", "l") or op ~= nil)) then
      if (op == nil) then op = "loops.csv" end
      loops_csv_file = cqa:init_csv (op, cqa_context);

   elseif (of == "html") then
      if (op == nil) then
         op = "cqa_html"
         if (fs.exists (op) == false) then
            os.execute ("mkdir cqa_html")
         else
            Message:warn ("Already existing default output directory (cqa_html): overwritten.")
            os.execute ("rm -rf cqa_html/*")
         end
      elseif (fs.exists (op) == false) then
         os.execute ("mkdir "..op);
      elseif (op == lfs.currentdir()) then
         Message:critical ("Dumping HTML files directly to current directory is not allowed.\n"..
                           "Please specify another directory or the name of a directory to be created in current directory).")
      else
         Message:critical (string.format ("Already existing directory: %s. Please specify another or remove.", op))
      end
   end

   local fct_mode = {name="", val=""};
   if (is_opt_set ("fct-loops", "fl")) then
      fct_mode.name = "fct-loops";
      fct_mode.val  = get_opt ("fct-loops", "fl");
   elseif (is_opt_set ("fct-body", "f")) then
      fct_mode.name = "fct-body";
      fct_mode.val  = get_opt ("fct-body", "f");
   end

   local is_requested_block;
   if (is_opt_set ("path", "p")) then
      is_requested_block = {};
      for pattern in string.gmatch (get_opt ("path", "p"), "[^,]+") do
         is_requested_block [pattern] = true;
      end
   end

   local function get_path (f, is_requested_path)
      local block_from_id = {};
      for block in f:blocks() do
         block_from_id [block:get_id()] = block;
      end

      local path = {};
      for block_id in pairs (is_requested_block) do
         local block = block_from_id [tonumber(block_id)];
         if (block == nil) then return {} end
         table.insert (path, block);
      end

      return path;
   end

   local header = cqa.api.reports.get_reports_header (cqa_context, bin)
   if (of == "txt") then
      print (table.concat (header, "\n") .. "\n")
   end

   -- for each function in the binary to analyze
   for f in bin:functions() do
      -- if f matches with a function pattern
      if (fct_mode.name == "fct-loops" and to_analyze (f, fct_mode.val, pattern_found)) then
         analyze_all_innermost_loops (f, loops_csv_file, html_reports);
      elseif (fct_mode.name == "fct-body" and to_analyze (f, fct_mode.val, pattern_found)) then
         analyze_blocks (f, f, loops_csv_file, html_reports);
      elseif (is_opt_set ("loop", "l")) then
         analyze_requested_loops (f, loop_found, loops_csv_file, is_requested_loop, html_reports);
      elseif (is_opt_set ("path", "p")) then
         local path = get_path (f, is_requested_block);

         if (next (path) ~= nil) then
            analyze_blocks (f, path, loops_csv_file, html_reports);
         end
      end
   end -- for each function

   if (of == "html") then
      local mode;
      if (is_opt_set ("fct-loops", "fl")) then
         mode = "fct-loops";
      elseif (is_opt_set ("loop", "l")) then
         mode = "loop";
      elseif (is_opt_set ("fct-body", "f")) then
         mode = "fct-body";
      elseif (is_opt_set ("path", "p")) then
         mode = "path";
      end
      cqa.output.html.print_reports_exit (html_reports, op, mode, header);
      Message:info ("Done. Open "..op.."/index.html in your browser to view GUI.");

   elseif (of == "txt" and get_opt ("confidence-levels", "conf") == nil) then
      Message:info ("Rerun CQA with conf=hint,expert to display more advanced reports"..
                       " or conf=all to display them with default reports.")
   end

   --------------------------------------------------------------------------------
   --                       Not found functions/loops printing                   --
   --------------------------------------------------------------------------------

   -- if the user wanted to analyze functions
   if (is_opt_set ("fct-body", "f") or is_opt_set ("fct-loops", "fl")) then
      -- Print patterns matching no functions
      -- for each function pattern
      for pattern in string.gmatch (get_opt ("fct-body", "f") or get_opt ("fct-loops", "fl"), "[^,]+") do
         -- if found no function matching this pattern, print the pattern
         if (pattern_found [pattern] == nil) then
            Message:info ("No function matches the pattern "..pattern.." in the binary "..args.bin);
         end
      end

      -- if the user wanted to analyze loops
   elseif (is_opt_set ("loop", "l")) then
      -- Print IDs of not found (requested) loops
      -- for each requested loop ID
      for lid in pairs (is_requested_loop) do
         -- if found no such loop, print the ID
         if (loop_found [lid] == nil) then
            Message:info ("No loop "..lid.." in the binary "..args.bin);
         end
      end
   end

   -- Close opened files
   if (loops_csv_file ~= nil) then
      loops_csv_file:close();
   end

   proj:remove_file(bin);
   
end
