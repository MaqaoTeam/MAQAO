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

-- This file defines functions used to display (HTML, TXT, CSV) results from an experiment directory
-- Main functions:
--  - [TXT]  print_sampling_txt
--  - [TXT]  print_sampling_full/summary_view
--  - [HTML] prepare_sampling_display_html
--  - [HTML] remark: HTML display itself (print_sampling_html) is defined in html.lua

module("lprof.display",package.seeall)

require "lprof.utils" -- get_opt, is_opt_set

BOUNDARY_CHAR   = "#";
SIDE_CHAR       = "#";
COLUMN_CHAR     = "|";

function set_opts_from_cmdline (args, options)
   local function is_opt_set (alias1, alias2)
      return lprof.utils.is_opt_set (args, alias1, alias2)
   end
   local function get_opt (alias1, alias2)
      return lprof.utils.get_opt (args, alias1, alias2)
   end

   -- Display functions/loops
   if (is_opt_set ("display-functions", "df")) then
      options.display_type = lprof.INFO_TYPE_FUNC -- "SFX"
   elseif (is_opt_set ("display-loops", "dl")) then
      options.display_type = lprof.INFO_TYPE_LOOP -- "SLX"
   elseif (get_opt ("output-format", "of") == "html") then
      options.display_type = lprof.INFO_TYPE_FUNC -- "SFX"
   end

   -- Output format, prefix and path
   options.output_format = get_opt ("output-format", "of") or "txt"
   options.output_prefix = get_opt ("output-prefix") or ""
   local op = get_opt ("output-path", "op")
   if (op ~= nil) then
      options.output_path = lprof:check_output_path_format (op)
   else
      options.output_path = ""
   end

   -- Show raw values for counters (number of samples instead of events)
   options.show_samples_value = lprof.utils.is_feature_set (
      args, "show-samples-value", "ssv", false)

   -- Extended mode (extra columns)
   -- TODO: remove or improve:
   --  - options.extended_mode has to be false for expected output
   --  - not referenced in help
   --  - not or no more maintained
   options.extended_mode = is_opt_set ("extended-mode", "em")
   
   -- Cumulative threshold
   local ct = get_opt ("cumulative-threshold", "ct")
   if (ct == nil) then
      options.cumulative_threshold = 100
   else
      ct = tonumber (ct)
      if (ct ~= nil and ct >= 0 and ct <= 100) then
         options.cumulative_threshold = ct
      else
         Message:error ("[MAQAO] Option cumulative-threshold must be a number between 0 and 100.")
         os.exit()
      end
   end

   -- Categorization
   local cat = get_opt ("categorization", "c") -- DEPRECATED
   if (cat ~= nil) then
      Message:warn ("[MAQAO] categorization/c is deprecated: use category-view/cv.")
   end
   local cv = (cat or get_opt ("category-view", "cv"))
   if (cv == "full") then
      options.categorization = lprof.CATEGORIZATION_FULL
   elseif (cv == "summary") then
      options.categorization = lprof.CATEGORIZATION_SUMMARY
   elseif (cv == "node") then
      options.categorization = lprof.CATEGORIZATION_NODE
   elseif (cv == "process") then
      options.categorization = lprof.CATEGORIZATION_PROCESS
   elseif (cv == "thread") then
      options.categorization = lprof.CATEGORIZATION_THREAD
   elseif (cv == nil) then
      options.categorization = lprof.CATEGORIZATION_SUMMARY -- default
   else
      Message:error ("[MAQAO] Unknown category-view option value <"..cv..">")
      Message:error ("[MAQAO] Available category-view option values are: full, summary, node, process, thread")
      os.exit()
   end

   -- View mode (thread-level or aggregated, useful for CSV/TXT output formats)
   local view = get_opt ("view", "v") -- DEPRECATED
   if (view ~= nil) then
      Message:warn ("[MAQAO] view/v is deprecated: use display-by-threads/dt.")
   end
   if (view == "full" or is_opt_set ("display-by-threads", "dt")) then
      options.view = "full"
   --elseif (view == "process" or is_opt_set ("display-by-process", "dp")) then
   --   options.view = "process"
   else
      options.view = "summary"
   end

   -- Callchains output + usage for refining categorization
   local cc = get_opt ("callchain", "cc")
   if (cc == true or cc == "exe") then
      if (cc == true) then
         Message:warn ("[MAQAO] no value for callchain/cc is deprecated: use callchain=exe.")
      end
      options.display_callchains = true
      options.callchain_filter = lprof.CALLCHAIN_FILTER_UP_TO_BINARY
   elseif (is_opt_set ("callchain-lib", "ccl") or cc == "lib") then
      if (is_opt_set ("callchain-lib", "ccl")) then
         Message:warn ("[MAQAO] callchain-lib/ccl is deprecated: use callchain=lib.")
      end
      options.display_callchains = true
      options.callchain_filter = lprof.CALLCHAIN_FILTER_UP_TO_LIBRARY
   elseif (is_opt_set ("callchain-all", "cca") or cc == "all") then
      if (is_opt_set ("callchain-all", "cca")) then
         Message:warn ("[MAQAO] callchain-all/cca is deprecated: use callchain=all.")
      end
      options.display_callchains = true
      options.callchain_filter = lprof.CALLCHAIN_FILTER_UP_TO_SYSTEM
   elseif (is_opt_set ("callchain-off", "cco") or cc == "off") then
      if (is_opt_set ("callchain-off", "cco")) then
         Message:warn ("[MAQAO] callchain-off/cco is deprecated: use callchain=off.")
      end
      options.display_callchains = false
      options.callchain_filter = lprof.CALLCHAIN_FILTER_IGNORE_ALL
   else --default
      -- TODO: try to remove options.display_callchains
      -- TODO: why not lprof.CALLCHAIN_FILTER_IGNORE_ALL ?
      options.display_callchains = false
      options.callchain_filter = lprof.CALLCHAIN_FILTER_UP_TO_BINARY
   end
   if (options.display_callchains == true) then options.view = "full" end

   -- Callchains coverage throttling
   local cvf = get_opt ("callchain-value-filter", "cvf") -- DEPRECATED
   if (cvf ~= nil) then
      Message:warn ("[MAQAO] callchain-value-filter/cvf is deprecated: use callchain-weight-filter/cwf.")
   end
   local cwf = (cvf or get_opt ("callchain-weight-filter", "cwf"))
   if (cwf == nil) then
      options.callchain_value_filter = lprof.CALLCHAIN_DEFAULT_VALUE_FILTER
   else
      cwf = tonumber (cwf)
      if (cwf ~= nil and cwf >= 0 and cwf <= 100) then
         options.callchain_value_filter = cwf
      else
         Message:error ("[MAQAO] Option callchain-(value/weight)-filter must be a number between 0 and 100.")
         os.exit()
      end
   end

   options["libraries_extra_categories"] = get_opt("libraries-extra-categories", "lec") or "off"
end

-- cf. AVLTREE.H ENUMERATOR
--INFO_TYPE_FUNC = 1;
--INFO_TYPE_LOOP = 2;
--INFO_TYPE_EXT_LIB = 3;

-- TODO: move this upper in MAQAO to enable reuse by another modules... (in plugins/built_in ?)
function lprof:get_median(array)
   if(type(array) ~= "table") then
      Message:error("Cannot compute median value of a nil table");
      return;
   end
   table.sort(array);
   if(#array == 0) then
      Message:error("Cannot compute median value of an empty table");
      return;
   elseif(#array == 1) then
      return array[1];
   elseif(#array % 2 == 0) then
      local left_center = #array/2;
      return (array[left_center] + array[left_center+1]) / 2;
   else
      --since index start at 1 we need to virtually add one element
      return array[(#array+1)/2];
   end
end

-- TODO: CF get_median
function lprof:get_mean(array)
   local sum = 0;
   if(type(array) ~= "table") then
      Message:error("Cannot compute mean value of a nil table");
      return;
   elseif(#array == 0) then
      Message:error("Cannot compute mean value of an empty table");
      return;
   end

   for _,v in ipairs(array) do
      sum = sum + v;
   end

   return Math:round(sum / #array,2);
end

-- TODO: CF get_median
function lprof:get_standard_deviation(array)
   if(type(array) ~= "table") then
      Message:error("Cannot compute standard deviation value of a nil table");
      return;
   elseif(#array == 0) then
      Message:error("Cannot compute standard deviation value of an empty table");
      return;
   end

   if(#array == 1) then return 0 end

   local sum  = 0;
   local mean = lprof:get_mean(array);

   if(mean == 0) then
      return 0;
   end

   for _,v in ipairs(array) do
      --print(v,mean,v - mean,(v - mean)^2)
      sum = sum + (v - mean)^2;
   end
   if(sum == 0) then
      return 0;
   end

   return Math:round(math.ceil(sum) / (#array-1),2);
end

--return the width of the frame
local function get_width_size (column_names,alignment,col_marge)
   width_size = 0;
   for i,col_name in ipairs(column_names) do
      --"+ 2*column_marge": it's the spaces added on each side of a column value: "  exmpl  "
      marge_left = math.floor((alignment[i]-string.len(col_name)+2*col_marge) / 2);
      marge_right = math.ceil((alignment[i]-string.len(col_name)+2*col_marge) / 2);
      -- Add the left marge spaces + the length of the column name
      -- + the right marge spaces + the COLUMN separator (the +1)
      width_size = width_size + (string.len(col_name)+marge_left+marge_right+1);
   end
   --Add the size of the final SIDE separator (on the right side)
   width_size = width_size +1;
   return width_size;
end

-- For the TXT display format
-- Print a boundary to delimit a text array in a terminal
function lprof:print_boundary (column_names,alignment,col_marge)
   width_size = get_width_size(column_names,alignment,col_marge);
   io.stdout:write(string.rep(BOUNDARY_CHAR,width_size));
   io.stdout:write("\n");
end

-- Print in the top center of the frame the corresponding Thread Id
function lprof:print_thread_id (thread_id, time, column_names, alignment, col_marge)
   width_size = get_width_size(column_names,alignment,col_marge)
   time_in_second = string.format("%.2f",time);
   marge_to_center = width_size/2 - (string.len("Thread #"..thread_id.." - "..time_in_second.." second(s)")/2);
   io.stdout:write(string.rep(" ",marge_to_center).."Thread #"..thread_id.." - "..time_in_second.." second(s)".."\n");
end

-- Print in the top center of the frame the corresponding NODE (hostname)
function lprof:print_hostname (hostname,extra_info,column_names,alignment,col_marge)
   width_size = get_width_size(column_names,alignment,col_marge)

   local string_to_display;
   -- Without adding any info
   if (extra_info == nil) then
      string_to_display = string.upper(tostring(hostname));
   else -- Adding some info
      string_to_display = string.upper(tostring(hostname)).." - "..extra_info;
   end

   marge_to_center = width_size/2 - (string.len(string_to_display)/2);
   io.stdout:write(string.rep(" ",marge_to_center)..string_to_display.."\n");
end

-- For the TXT display format
-- Print the column header (column name + separator)
function lprof:print_column_header (column_names,alignment,col_marge,thread_id)

   lprof:print_boundary(column_names, alignment, col_marge);
   io.stdout:write(SIDE_CHAR);
   for i,col_name in ipairs(column_names) do
      -- To center the column name :
      -- Alignment [i] = The longest string length for the i-th column
      -- col_marge : marge on each side of the colomn's value
      marge_left = math.floor((alignment[i]-string.len(col_name)+2*col_marge) / 2);
      marge_right = math.ceil((alignment[i]-string.len(col_name)+2*col_marge) / 2);
      io.stdout:write(string.rep(" ",marge_right)..col_name..string.rep(" ",marge_left));
      if (i < #column_names) then
         io.stdout:write(COLUMN_CHAR)
      else
         io.stdout:write(SIDE_CHAR)
      end
   end

   io.stdout:write("\n");
   lprof:print_boundary(column_names,alignment,col_marge);
end

-- For the TXT display format
-- We search for each column,the longest
-- string character in each column.
-- It permits to compute, later, the number of spaces
-- needed to align each value of a column.
-- UPD: Use lprof.FCT_NAME_STR_MAX_SIZE as maximal size for any column
-- If a column should be bigger, its content will be truncated during the txt display
function lprof:search_column_alignment (column_names,data)
   local alignment = {};
   --Initialisation with the column name length
   for i,col_name in ipairs(column_names) do
      alignment[i]= string.len(col_name)
   end

   -- Search the longest string character in each column
   -- Store it in the alignment[j-th] index corresponding to the j-th column
   for i,row in ipairs(data) do
      for j,value in ipairs (row) do
         local str_size = string.len(tostring(value))
         if (alignment[j] ~= nil and str_size ~= nil) then
            if str_size > lprof.FCT_NAME_STR_MAX_SIZE then
               alignment[j] = math.max(alignment[j],lprof.FCT_NAME_STR_MAX_SIZE)
            else
               alignment[j] = math.max(alignment[j],str_size)
            end
         end
      end
   end

   return alignment;
end

--For the TXT display format
-- Print one line of an array
function lprof:print_line(data,alignment,col_marge)
   local col_align;
   io.stdout:write(SIDE_CHAR);
   for i,value in ipairs(data) do
      -- Compute the number of spaces needed to align the current length value with the longest one.
      col_align = alignment[i] - string.len(value)
      -- Create one column entry : Marge + Value + Align + Marge + Separator
      -- Truncate the text (value) to display
      local _value = string.sub(value, 1,lprof.FCT_NAME_STR_MAX_SIZE)
      io.stdout:write(string.rep(" ",col_marge).._value..string.rep(" ",col_align)..string.rep(" ",col_marge));

      --To dectect the last column
      if (i < #data) then --not the last one : print the delimitation character.
         io.stdout:write(COLUMN_CHAR)
      else
         io.stdout:write(SIDE_CHAR) -- last column so we print the side character.
      end

   end
   io.stdout:write("\n");
end


--For the TXT display format
-- Print one line of an array
local function print_callchain(callchain, percentage, spaces_alignment,col_marge)
   local col_align = 0;
   io.stdout:write(SIDE_CHAR);
   -- 6 characters
   if (percentage == "100.00" ) then
      spaces_alignment = spaces_alignment - (string.len(callchain) + 1) + col_marge - 1;
   else
      -- 5 characters
      spaces_alignment = spaces_alignment - string.len(callchain) + col_marge - 1;
   end
   -- Create one column entry : Marge + Value + Align + Marge + Separator
   io.stdout:write("   "..string.rep(" ",col_marge)..percentage.."% : "..callchain..string.rep(" ",spaces_alignment)..string.rep(" ",col_marge));
   io.stdout:write(SIDE_CHAR) -- last column so we print the side character.
   --end
   io.stdout:write("\n");
end

-- Export any 2D lua Table into a CSV file format
local function export_to_csv (file_name,array,separator,column_names)

   if (file_name ~= nil) then
      local file,error_msg = io.open(file_name..".csv","w");
      if (file ~= nil) then
         src_info_idx = 0;
         if (column_names ~= nil) then
            for i,name in ipairs (column_names) do
               file:write(name..separator);
            end
            file:write("\n");
         end
         for i,row in ipairs (array) do
            for j,value in ipairs(row) do
               if (value ~= "-1") then
                  file:write(value..separator);
               else
                  file:write(""..separator);
               end
            end
            file:write("\n");
         end
         Message:info("The CSV file "..file_name..".csv has been generated");
         file:close();
      else
         Message:info("Impossible to create CSV file: "..error_msg);
      end
   else
      Debug:info("File name missing");
   end
end

function print_display_commands_array (experiment_path)

   local column_names = {lprof.strings.COLUMN_LEVEL, lprof.strings.COLUMN_REPORT, lprof.strings.COLUMN_COMMAND};
   local info = {
      -- Level                                  Report                                          Command
      {lprof.strings.LEVEL_FUNCTIONS,      lprof.strings.REPORT_SUMMARY,  lprof.strings.MAQAO_LPROF_COMMAND.." "..lprof.strings.OPT_DF.." "..lprof.strings.OPT_XP..experiment_path},
      {lprof.strings.LEVEL_FUNCTIONS,      lprof.strings.REPORT_COMPLETE, lprof.strings.MAQAO_LPROF_COMMAND.." "..lprof.strings.OPT_DF.." "..lprof.strings.OPT_XP..experiment_path.." "..lprof.strings.OPT_DT.." "..lprof.strings.OPT_CV.."full"},
      {lprof.strings.LEVEL_LOOPS,          lprof.strings.REPORT_SUMMARY,  lprof.strings.MAQAO_LPROF_COMMAND.." "..lprof.strings.OPT_DL.." "..lprof.strings.OPT_XP..experiment_path},
      {lprof.strings.LEVEL_LOOPS,          lprof.strings.REPORT_COMPLETE, lprof.strings.MAQAO_LPROF_COMMAND.." "..lprof.strings.OPT_DL.." "..lprof.strings.OPT_XP..experiment_path.." "..lprof.strings.OPT_DT},
      {lprof.strings.LEVEL_FCTS_AND_LOOPS, lprof.strings.REPORT_HTML,     lprof.strings.MAQAO_LPROF_COMMAND.."     "                           ..lprof.strings.OPT_XP..experiment_path.." "..lprof.strings.OPT_OF.."html"},
      --{"add",                                "a new",                           "command!"}
   };

   print ( lprof.strings.MAQAO_TAG.." "..lprof.strings.MSG_INFO_CMD );

   local alignment = lprof:search_column_alignment (column_names,info);
   lprof:print_column_header (column_names,alignment,2,"");
   for i=1,table.getn(info)  do
      lprof:print_line(info[i],alignment,2);
   end
   lprof:print_boundary(column_names,alignment,2);
end

-- For some binaries (e.g compiled with Intel compilers on PGO mode) some functions are not disassembled
-- by default and then considered as unknown. Using --lcore-flow-all allows MAQAO to see them
local function warn_high_unknown_functions (t)
   local nb_threads = 0
   local avg_thread_percent = 0

   for _,node in pairs (t.nodes) do
      for _,ps in pairs (node.processes) do
         for _,thread in pairs (ps.threads) do
            local unknown_fcts = thread.functions ["Unknown functions"]

            if (unknown_fcts ~= nil) then
               local tokens = String:get_tokenized_table (unknown_fcts ["display string"], ";")
               local thread_percent = tonumber (tokens[4])

               if (thread_percent ~= nil) then
                  avg_thread_percent = avg_thread_percent + tonumber (tokens[4])
                  nb_threads = nb_threads + 1
               end
            end
         end
      end
   end

   if (avg_thread_percent / nb_threads > 20) then
      Message:warn ("High unknown functions coverage: try to reprofile with --lcore-flow-all.")
   end
end

-- no more needed after deprecated code removal
local function get_pre_refactoring_tables (t, display_functions, display_loops, context)
   local node_name = {}
   local pid = {}
   local tid = {}
   local thread_time = {}
   local ev_list = t["events list"]
   local thread_fcts = {}
   local callchains = {}
   local is_library = {}
   local outer_loops = {}
   local categories = {}
   local libc_cats = {}
   local thread_loops = {}
   local loop_time = {}
   local exe_loops = {}

   if (display_functions) then
      for exe_fct_name, exe_fct in pairs (t ["executable functions"]) do
         outer_loops [exe_fct_name] = exe_fct ["outermost loops"]
      end
   end

   if (display_loops) then
      for exe_loop_id, exe_loop in pairs (t ["executable loops"]) do
         exe_loops [exe_loop_id] = exe_loop
      end
   end

   for node_key, node in pairs (t.nodes) do
      local node_rank = node.rank + 1
      node_name   [node_rank] = node_key
      pid         [node_rank] = {}
      tid         [node_rank] = {}
      thread_time [node_rank] = {}
      thread_fcts [node_rank] = {}
      callchains  [node_rank] = {}
      is_library  [node_rank] = {}
      categories  [node_rank] = {}
      libc_cats   [node_rank] = {}
      thread_loops[node_rank] = {}
      loop_time   [node_rank] = {}

      for ps_key, ps in pairs (node.processes) do
         local ps_rank = ps.rank + 1
         pid         [node_rank][ps_rank] = tostring(ps_key)
         tid         [node_rank][ps_rank] = {}
         thread_time [node_rank][ps_rank] = {}
         thread_fcts [node_rank][ps_rank] = {}
         callchains  [node_rank][ps_rank] = {}
         is_library  [node_rank][ps_rank] = ps.is_library
         categories  [node_rank][ps_rank] = {}
         libc_cats   [node_rank][ps_rank] = {}
         thread_loops[node_rank][ps_rank] = {}
         loop_time   [node_rank][ps_rank] = {}

         for thread_key, thread in pairs (ps.threads) do
            local thread_rank = thread.rank + 1
            tid         [node_rank][ps_rank][thread_rank] = thread_key
            thread_time [node_rank][ps_rank][thread_rank] = thread ["time in seconds"]

            if (display_functions) then
               thread_fcts [node_rank][ps_rank][thread_rank] = {}
               callchains  [node_rank][ps_rank][thread_rank] = {}
               categories  [node_rank][ps_rank][thread_rank] = thread ["categories"]
               libc_cats   [node_rank][ps_rank][thread_rank] = thread ["libc categories"]

               for fct_name, fct in pairs (thread.functions) do
                  if (fct ["display string"] ~= "N/A") then
                     table.insert (thread_fcts [node_rank][ps_rank][thread_rank],
                                   fct ["display string"])
                  end
                  callchains  [node_rank][ps_rank][thread_rank][fct_name] =
                     fct.callchains
               end
            end

            if (display_loops) then
               thread_loops [node_rank][ps_rank][thread_rank] = {}
               loop_time    [node_rank][ps_rank][thread_rank] = {}

               for module_name, module_loops in pairs (thread.loops) do
                  for loop_id, loop in pairs (module_loops) do
                     if (loop ["display string"] ~= "N/A") then
                        table.insert (thread_loops [node_rank][ps_rank][thread_rank],
                                      loop ["display string"])
                     end
                     loop_time [node_rank][ps_rank][thread_rank][loop_id] =
                        loop ["thread time percent"]
                  end
               end
            end
         end
      end
   end

   if (display_functions) then warn_high_unknown_functions (t) end

   context.binary_name           = t["executable name"]
   context.hostnameIdxToHostname = node_name
   context.pidIdxToPid           = pid
   context.thread_idx_to_thread_id = tid
   context.time_per_thread       = thread_time
   context.hwc_list              = ev_list
   context.results_array         = thread_fcts
   context.callchains            = callchains
   context.is_library            = is_library
   context.outermost_loops       = outer_loops
   context.samples_category      = categories
   context.libc_category         = libc_cats
   context.results_loops_array   = thread_loops
   context.loop_id_to_time       = loop_time -- probably no more used/maintained...
   context.loops_child           = exe_loops
end

-- Get time info (in second):
-- walltime             : Walltime of your traced application
-- time_per_node        : Maximum time spent in each node
-- time_average_node    : Average time spent in nodes
-- time_per_process     : Maximum time spent in each process
-- time_average_process : Average time spent in processes
-- time_average_thread  : Average time spent in threads
local function get_time_info (context)

   local walltime = 0;
   local time_per_thread = context.time_per_thread
   local time_per_node = {};
   local time_per_process = {};
   local time_average_node = 0;
   local time_average_process = {};
   local time_average_thread = {};

   for nodeIdx,_ in ipairs(time_per_thread) do
      time_per_node[nodeIdx] = 0;

      time_average_process[nodeIdx] = 0;
      time_per_process[nodeIdx]     = {};
      time_average_thread[nodeIdx] = {};

      for pidIdx,_ in ipairs(time_per_thread[nodeIdx]) do
         time_per_process[nodeIdx][pidIdx] = 0;
         time_average_thread[nodeIdx][pidIdx] = 0;

         threadIdx = 1;
         -- TODO : ~ Dirty, try to find a better solution
         -- Using a while loop here to "play" with the threadIdx.
         -- We can remove some threads from the array during
         -- a loop turn so the index
         while threadIdx <= #time_per_thread[nodeIdx][pidIdx]  do
            -- Thread is correct ( > 0 s) ?
            local threadTime = time_per_thread[nodeIdx][pidIdx][threadIdx];
            if ( threadTime ~= 0 ) then
               -- Thread max time = walltime
               if (walltime < threadTime ) then
                  walltime = threadTime;
               end
               -- Thread max time attach to a process = Time of this process
               if ( time_per_process[nodeIdx][pidIdx] < threadTime ) then
                  time_per_process[nodeIdx][pidIdx] = threadTime;
               end
               -- Add thread time to compute the average thread time
               time_average_thread[nodeIdx][pidIdx] = time_average_thread[nodeIdx][pidIdx] + threadTime;
               -- Get the next element
               threadIdx = threadIdx + 1;
            else -- Some threads have been create/kill instantly so we need to remove them
               table.remove(context.time_per_thread[nodeIdx][pidIdx],threadIdx);
               table.remove(context.samples_category[nodeIdx][pidIdx],threadIdx);
               if (context.results_array) then
                  table.remove(context.results_array[nodeIdx][pidIdx],threadIdx);
               end
               if (context.results_loops_array) then
                  table.remove(context.results_loops_array[nodeIdx][pidIdx],threadIdx);
               end
               table.remove(context.callchains[nodeIdx][pidIdx],threadIdx);
               table.remove(context.thread_idx_to_thread_id[nodeIdx][pidIdx],threadIdx)
               -- The next elements is now at the current threadIdx...
            end
         end -- THREAD

         -- To compute thread average, we divide by the number of threads
         time_average_thread[nodeIdx][pidIdx] = string.format("%.2f",time_average_thread[nodeIdx][pidIdx]/ #time_per_thread[nodeIdx][pidIdx])
         -- Add process time to compute the average process time
         time_average_process[nodeIdx] = time_average_process[nodeIdx] + time_per_process[nodeIdx][pidIdx];
         -- Process max time on a node = Time of this node
         if ( time_per_node[nodeIdx] < time_per_process[nodeIdx][pidIdx] ) then
            time_per_node[nodeIdx] = time_per_process[nodeIdx][pidIdx];
         end

      end -- PROCESS
      -- Add node time to compute the average node time
      time_average_node = time_average_node + time_per_node[nodeIdx];
      -- To compute process average, we divide by the number of process
      time_average_process[nodeIdx] = string.format("%.2f",time_average_process[nodeIdx]/ #time_per_process[nodeIdx])

   end -- NODE

   -- To compute node average, we divide by the number of node
   time_average_node = string.format("%.2f",time_average_node/ #time_per_node);

   walltime = string.format("%.2f",walltime);

   return walltime, time_per_node, time_average_node, time_per_process, time_average_process, time_average_thread;

end

local function update_categories_in_post_refactoring_table (context)
   local category_headers = lprof:split (lprof.CATEGORIZATION_COLUMN_NAMES, ",")
   local lec = context.options ["libraries_extra_categories"]
   if (lec ~= "on" and lec ~= "off") then
      for lib_name in string.gmatch (lec, "[^,]+") do
         category_headers [#category_header + 1] = lib_name.." (%)"
      end
   end

   local prt = context.post_refactoring_table

   -- average across all nodes
   local cat_summary = context.categorization_summary[1]
   prt.categories = {}
   for i = 2, #cat_summary do
      prt.categories [category_headers[i-1]] = cat_summary[i]
   end
   
   -- for each host/node
   for host_rank = 1, #context.cat_per_host-1 do
      local hostname = context.hostnameIdxToHostname [host_rank]
      local host = prt.nodes [hostname]
      host.categories = {}
      local cat_per_host = context.cat_per_host [host_rank]
      for i = 2, #cat_per_host do
         host.categories[category_headers[i-1]] = cat_per_host[i]
      end

      -- for each process
      for process_rank = 1, #context.cat_per_process [host_rank]-1 do
         local process_id = tonumber (context.pidIdxToPid [host_rank][process_rank])
         local process = host.processes [process_id]
         process.categories = {}
         local cat_per_process = context.cat_per_process [host_rank][process_rank]
         for i = 2, #cat_per_process do
            process.categories[category_headers[i-1]] = cat_per_process[i]
         end

         -- for each thread
         for thread_rank = 1, #context.cat_per_thread [host_rank][process_rank]-1 do
            local thread_id = context.thread_idx_to_thread_id [host_rank][process_rank][thread_rank]
            local thread = process.threads [thread_id]
            local cat_per_thread = context.cat_per_thread [host_rank][process_rank][thread_rank]
            for i = 2, #cat_per_thread do
               thread.categories[category_headers[i-1]] = cat_per_thread[i]
            end
         end
      end
   end
end

-- Shared by lprof:prepare_sampling_display_html_display and print_samples_categorisation (TXT)
local function get_samples_categorisation (context)

   local samples_category        = context.samples_category
   
   local walltime, time_per_node, time_average_node, time_per_process, time_average_process,
   time_average_thread = get_time_info (context);

   local nbLecLibs = 0 -- number of libraries in "-lec" options
   local nbExtraCatT = 0 -- number of extra categories at thread level
   local nbExtraCatPNG = 0 -- idem at process/node/global level
   if((context.options["libraries_extra_categories"] ~= "on") and (context.options["libraries_extra_categories"] ~= "off")) then
      nbLecLibs = table.getn(lprof:split(context.options["libraries_extra_categories"], ","))
      -- thread level: "+1" because of "TOTAL" category (= total number of samples)
      nbExtraCatT = nbLecLibs + 1
      -- process/node/global level: no "TOTAL" category
      nbExtraCatPNG = nbLecLibs
   end

   -- Categorization array : summary / per node / per process / per thread
   -- Extra info for the two first column of CategoryIdx : Node name or PID or Thread ID (column 1) , Time (column 2)
   -- Then categorization as follow ( column 3 to 12 ) : Binary, MPI, OpenMP, Math, System, Pthread, IO, String, Memory, Others
   local categorization_summary = {{"TOTAL",walltime,0,0,0,0,0,0,0,0,0,0}};
   -- add elements corresponding to extra categories (if any)
   for i=1, nbLecLibs do
      table.insert(categorization_summary[1], 0)
   end
   local cat_per_node     = {}; -- [NodeIdx] [CategoryIdx]
   local cat_per_process  = {}; -- [NodeIdx] [ProcessIdx] [CategoryIdx]
   local cat_per_thread   = {}; -- [NodeIdx] [ProcessIdx] [ThreadIdx] [CategoryIdx]
   local total_per_node = 0;

   for nodeIdx, nodeData in ipairs (samples_category) do
      cat_per_thread[nodeIdx] = {};
      cat_per_process[nodeIdx] = {};
      cat_per_node[nodeIdx] = {0,0,0,0,0,0,0,0,0,0,0,0};
      -- add elements corresponding to extra categories (if any)
      for i=1, nbLecLibs do
         table.insert(cat_per_node[nodeIdx], 0)
      end
      cat_per_node[nodeIdx][lprof.INFO_CATEGORY] = context.hostnameIdxToHostname[nodeIdx];
      cat_per_node[nodeIdx][lprof.TIME_CATEGORY] = string.format("%.2f",time_per_node[nodeIdx]);
      local total_per_process = 0;

      for processIdx, processData in ipairs (nodeData) do
         cat_per_thread[nodeIdx][processIdx] = {};
         cat_per_process[nodeIdx][processIdx]    = {0,0,0,0,0,0,0,0,0,0,0,0};
         -- add elements corresponding to extra categories (if any)
         for i=1, nbLecLibs do
            table.insert(cat_per_process[nodeIdx][processIdx], 0)
         end
         cat_per_process[nodeIdx][processIdx][lprof.INFO_CATEGORY] = context.pidIdxToPid[nodeIdx][processIdx];
         cat_per_process[nodeIdx][processIdx][lprof.TIME_CATEGORY] = string.format("%.2f",time_per_process[nodeIdx][processIdx]);
         local nbThreads = #samples_category[nodeIdx][processIdx];
         local total_per_thread = 0;

         for threadIdx, threadData in ipairs (processData) do
            cat_per_thread[nodeIdx][processIdx][threadIdx] = {};
            cat_per_thread[nodeIdx][processIdx][threadIdx][lprof.INFO_CATEGORY] = context.thread_idx_to_thread_id[nodeIdx][processIdx][threadIdx];
            cat_per_thread[nodeIdx][processIdx][threadIdx][lprof.TIME_CATEGORY] = string.format("%.2f",context.time_per_thread[nodeIdx][processIdx][threadIdx]);

            -- Add samples for each category to each level ( thread / process / node / summary )
            for idx = lprof.SAMPLES_CATEGORY_BIN, (lprof.SAMPLES_CATEGORY_NB_CATEGORIES + nbExtraCatT) do
               if (idx ~= lprof.SAMPLES_CATEGORY_TOTAL) then -- skip "TOTAL" category (not a real one)
                  local percentage = threadData [idx] * 100 / threadData[lprof.SAMPLES_CATEGORY_TOTAL];
                  -- For cat_per_... array the index is idx+2 because we have INFO_CATEGORY and TIME_CATEGORY at idx 1 and idx 2;
                  if (idx < lprof.SAMPLES_CATEGORY_TOTAL) then
                     cat_per_thread[nodeIdx][processIdx][threadIdx][idx+2] = string.format("%.2f",percentage);
                     cat_per_process[nodeIdx][processIdx][idx+2]           = cat_per_process[nodeIdx][processIdx][idx+2] + threadData [idx];
                     cat_per_node[nodeIdx][idx+2]                          = cat_per_node[nodeIdx][idx+2] + threadData[idx];
                     categorization_summary[1][idx+2]                      = categorization_summary[1][idx+2] + threadData[idx];
                  else -- ignored index (lprof.SAMPLES_CATEGORY_TOTAL) reduces shift to match columns (+2 --> +1)
                     cat_per_thread[nodeIdx][processIdx][threadIdx][idx+1] = string.format("%.2f",percentage);
                     cat_per_process[nodeIdx][processIdx][idx+1]           = cat_per_process[nodeIdx][processIdx][idx+1] + threadData [idx];
                     cat_per_node[nodeIdx][idx+1]                          = cat_per_node[nodeIdx][idx+1] + threadData[idx];
                     categorization_summary[1][idx+1]                      = categorization_summary[1][idx+1] + threadData[idx];
                  end
               end
            end
            total_per_thread = total_per_thread + threadData[lprof.SAMPLES_CATEGORY_TOTAL];
            total_per_process = total_per_process + threadData[lprof.SAMPLES_CATEGORY_TOTAL];
            total_per_node = total_per_node + threadData[lprof.SAMPLES_CATEGORY_TOTAL];
         end -- Thread

         local nbThreads = #samples_category[nodeIdx][processIdx]
         -- Add Average line for threads
         cat_per_thread[nodeIdx][processIdx][nbThreads+1] = {0,0,0,0,0,0,0,0,0,0,0,0};
         -- add elements corresponding to extra categories (if any)
         for i=1, nbLecLibs do
            table.insert(cat_per_thread[nodeIdx][processIdx][nbThreads+1], 0)
         end
         cat_per_thread[nodeIdx][processIdx][nbThreads+1][lprof.INFO_CATEGORY] = "AVERAGE";
         cat_per_thread[nodeIdx][processIdx][nbThreads+1][lprof.TIME_CATEGORY] = time_average_thread[nodeIdx][processIdx];

         -- Compute data at Process Level
         for idx = lprof.SAMPLES_CATEGORY_BIN, (lprof.SAMPLES_CATEGORY_NB_CATEGORIES + nbExtraCatPNG) do
            -- For cat_per_... array the index is idx+2 because we have INFO_CATEGORY and TIME_CATEGORY at idx 1 and idx 2;
            -- Compute the average of all threads of processIdx (in percentage)
            local percentage = cat_per_process[nodeIdx][processIdx][idx+2] * 100 / total_per_thread;
            -- Fill in the Average column for threads
            cat_per_thread[nodeIdx][processIdx][nbThreads+1][idx+2] = string.format("%.2f",percentage);
            -- Fill in the category (idx+2) column for the processIdx
            cat_per_process[nodeIdx][processIdx][idx+2] = string.format("%.2f",percentage);
         end
      end -- Process

      local nbPids = #samples_category[nodeIdx];
      -- Add Average line for processes
      cat_per_process[nodeIdx][nbPids+1] = {"AVERAGE",time_average_process[nodeIdx],0,0,0,0,0,0,0,0,0,0};
      -- add elements corresponding to extra categories (if any)
      for i=1, nbLecLibs do
         table.insert(cat_per_process[nodeIdx][nbPids+1], 0)
      end
      cat_per_process[nodeIdx][nbPids+1][lprof.INFO_CATEGORY] = "AVERAGE";
      cat_per_process[nodeIdx][nbPids+1][lprof.TIME_CATEGORY] = time_average_process[nodeIdx];

      -- Compute data at Node Level
      for idx = lprof.SAMPLES_CATEGORY_BIN, (lprof.SAMPLES_CATEGORY_NB_CATEGORIES + nbExtraCatPNG) do
         -- For cat_per_... array the index is idx+2 because we have INFO_CATEGORY and TIME_CATEGORY at idx 1 and idx 2;
         -- Compute the average of all processes of nodeIdx (in percentage)
         local percentage = cat_per_node[nodeIdx][idx+2] * 100 / total_per_process;
         -- Fill in the Average column for processes
         cat_per_process[nodeIdx][nbPids+1][idx+2] = string.format("%.2f",percentage);
         cat_per_node[nodeIdx][idx+2] = string.format("%.2f",percentage);
      end

   end -- Node

   local nbNodes = #samples_category;
   -- Add Average line for nodes
   cat_per_node[nbNodes+1] = {"AVERAGE",walltime,0,0,0,0,0,0,0,0,0,0};
   -- add elements corresponding to extra categories (if any)
   for i=1, nbLecLibs do
      table.insert(cat_per_node[nbNodes+1], 0)
   end

   -- Compute data at Summary Level
   for idx = lprof.SAMPLES_CATEGORY_BIN, (lprof.SAMPLES_CATEGORY_NB_CATEGORIES + nbExtraCatPNG) do
      -- For cat_per_... array the index is idx+2 because we have INFO_CATEGORY and TIME_CATEGORY at idx 1 and idx 2;
      -- Compute the average of all nodes (in percentage)
      local percentage = categorization_summary[1][idx+2] * 100 / total_per_node;
      -- Fill in the Average column for nodes
      cat_per_node[nbNodes+1][idx+2]= string.format("%.2f", percentage );
      categorization_summary[1][idx+2] = string.format("%.2f", percentage );
   end

   context.categorization_summary = categorization_summary
   context.cat_per_thread  = cat_per_thread
   context.cat_per_process = cat_per_process
   context.cat_per_host    = cat_per_node

   if (context.post_refactoring_table ~= nil) then
      update_categories_in_post_refactoring_table (context)
   end
end

function prepare_sampling_display_html (context)
   local options = context.options
   local absolute_pidIdxToPid

   local display_functions = true
   local display_loops     = true
   context.post_refactoring_table = lprof.prepare_sampling_display (
      options["experiment_path"],
      display_functions, display_loops,
      options["callchain_filter"],
      options["hwc_mode"],
      options["cpu_frequency"],
      options["ref_frequency"],
      options["sampling_period"],
      options["show_samples_value"],
      options["extended_mode"],
      options["libraries_extra_categories"]);

   get_pre_refactoring_tables (context.post_refactoring_table, display_functions, display_loops, context);

   context.absolute_pidIdxToPid = {}
   local pid_idx = 1
   for hostname, node in pairs (context.post_refactoring_table.nodes) do
      context.rslt_files [hostname] = {}
      for pid, process in pairs (node.processes) do
         context.rslt_files [hostname] [tostring(pid)] = "unused !"
         context.absolute_pidIdxToPid [pid_idx] = tostring(pid)
         pid_idx = pid_idx + 1
      end
   end

   for hostIdx,host in ipairs(context.results_array) do
      for pidIdx,pid in ipairs(host) do
         for threadIdx,thread_info in ipairs (pid) do
            for line_id, line in ipairs (thread_info) do
               context.results_array[hostIdx][pidIdx][threadIdx][line_id] = lprof:split(line,";");
            end
         end
      end
   end

   for hostIdx,host in ipairs(context.results_loops_array) do
      for pidIdx,pid in ipairs(host) do
         for threadIdx,thread_info in ipairs (pid) do
            for line_id, line in ipairs (thread_info) do
               context.results_loops_array[hostIdx][pidIdx][threadIdx][line_id] = lprof:split(line,";");
            end
         end
      end
   end

   -- local pidIdx=1;
   -- local hostnameIdx = 1;
   -- local walltime={};
   -- local walltime_per_thread = {};
   -- for hostname ,pid_directory in pairs (context.rslt_files) do
   --    walltime[hostnameIdx] = {};
   --    walltime_per_thread[hostnameIdx] = {};
   --    for curpid, rslt_files in pairs (pid_directory)do
   --       --walltimeFile = assert(io.open(options["experiment_path"].."/"..hostname.."/"..curpid.."/walltime", "r"));
   --       --walltime[hostnameIdx][pidIdx] = walltimeFile:read("*number");
   --       --walltimeFile:close();
   --       walltime[hostnameIdx][pidIdx] = 0;

   --       walltime_per_thread[hostnameIdx][pidIdx] = {};
   --       for thread_idx,thread_id in pairs(context.thread_idx_to_thread_id[hostnameIdx][pidIdx]) do
   --          local walltimeFile = io.open(options["experiment_path"].."/"..hostname.."/"..curpid.."/"..thread_id..".walltime", "r");
   --          if (walltimeFile ~= nil) then
   --             curWalltime = walltimeFile:read("*number");
   --             walltime_per_thread[hostnameIdx][pidIdx][thread_id] = curWalltime;
   --             walltimeFile:close()
   --          end
   --          --walltime_per_thread[hostnameIdx][pidIdx][thread_id] = 0;
   --       end
   --       pidIdx = pidIdx +1
   --    end
   --    pidIdx = 1;
   --    hostnameIdx = hostnameIdx +1
   -- end

   get_samples_categorisation (context);
end

-- Called only by print_sampling_txt
local function print_samples_categorisation (context)
   local options = context.options
   local column_marge = 2;
   local output_path = nil;

   local cat_per_thread = context.cat_per_thread
   local cat_per_pid    = context.cat_per_process
   local cat_per_host   = context.cat_per_host

   local categoriesToRemove = {} -- contains indexes of standard categories not to print (i.e. w/ value = 0%)
   local columnHeadersExtraCat = lprof.CATEGORIZATION_COLUMN_NAMES
   -- add extra headers to categorization arrays (if any)
   if((options["libraries_extra_categories"] ~= "on") and (options["libraries_extra_categories"] ~= "off")) then
      for _, libraryName in ipairs(lprof:split(options["libraries_extra_categories"], ",")) do
         columnHeadersExtraCat = columnHeadersExtraCat..","..libraryName.." (%)"
      end
   end
   --RESUME ALL NODES
   if ( options["categorization"] == lprof.CATEGORIZATION_FULL or options["categorization"] == lprof.CATEGORIZATION_SUMMARY ) then
      -- First column name is empty
      local column_names = lprof:split(" ,"..columnHeadersExtraCat,",");

      -- search for indexes of categories to remove
      for idxCategory=lprof.NB_CATEGORIES, 1, -1 do
         if (tonumber(context.categorization_summary[1][idxCategory]) == 0) then
            table.remove(context.categorization_summary[1], idxCategory)
	    table.remove(column_names, idxCategory)
	    table.insert(categoriesToRemove, idxCategory)
	 end
      end
      -- remove categories at host level
      for _, host in ipairs(cat_per_host) do
         for _, category in ipairs(categoriesToRemove) do
            table.remove(host, category)
	 end
      end
      -- remove categories at process level
      for idxHost, _ in ipairs(cat_per_pid) do
         for _, process in ipairs(cat_per_pid[idxHost]) do
            for _, category in ipairs(categoriesToRemove) do
               table.remove(process, category)
	    end
	 end
      end
      -- remove categories at thread level
      for idxHost, _ in ipairs(cat_per_thread) do
         for idxProcess, _ in ipairs(cat_per_thread[idxHost]) do
            for _, thread in ipairs(cat_per_thread[idxHost][idxProcess]) do
               for _, category in ipairs(categoriesToRemove) do
                  table.remove(thread, category)
               end
	    end
	 end
      end

      if (options["output_format"] == "txt") then
         local alignment = lprof:search_column_alignment(column_names,context.categorization_summary);
         lprof:print_hostname ("CATEGORIZATION SUMMARY", nil, column_names, alignment, column_marge);
         lprof:print_column_header(column_names,alignment,column_marge,0);
         lprof:print_line(context.categorization_summary[1],alignment,column_marge);
         lprof:print_boundary(column_names,alignment,column_marge);
      elseif (options["output_format"] == "csv") then
         output_path = options["output_path"]..options["output_prefix"].."lprof_"..lprof.output_file_names.cat_summary.."_"..fs.basename(options["experiment_path"]);
         export_to_csv(output_path,context.categorization_summary,";",column_names);
      end
   end

   --PER NODE
   if ( options["categorization"] == lprof.CATEGORIZATION_FULL or options["categorization"] == lprof.CATEGORIZATION_NODE ) then
      local column_names = lprof:split("NODE,"..columnHeadersExtraCat,",");
      for _, category in ipairs(categoriesToRemove) do -- remove 0% categories
         table.remove(column_names, category)
      end

      if (options["output_format"] == "txt") then
         local alignment = lprof:search_column_alignment(column_names,cat_per_host);
         lprof:print_hostname ("NODES", "CATEGORIZATION", column_names, alignment, column_marge);
         lprof:print_column_header(column_names,alignment,column_marge,0);
         for nodeIdx,hostData in ipairs (cat_per_host) do
            lprof:print_line(hostData,alignment,column_marge);
         end
         lprof:print_boundary(column_names,alignment,column_marge);
      elseif (options["output_format"] == "csv") then
         output_path = options["output_path"]..options["output_prefix"].."lprof_"..lprof.output_file_names.cat_node.."_"..fs.basename(options["experiment_path"]);
         export_to_csv(output_path,cat_per_host,";",column_names);
      end
   end

   --PER PROCESS
   if ( options["categorization"] == lprof.CATEGORIZATION_FULL or options["categorization"] == lprof.CATEGORIZATION_PROCESS ) then
      local column_names = lprof:split("PROCESS ID,"..columnHeadersExtraCat,",");
      for _, category in ipairs(categoriesToRemove) do -- remove 0% categories
         table.remove(column_names, category)
      end

      if (options["output_format"] == "txt") then
         for nodeIdx,_ in ipairs (cat_per_pid) do
            local alignment = lprof:search_column_alignment(column_names,cat_per_pid[nodeIdx]);
            lprof:print_hostname ( context.hostnameIdxToHostname[nodeIdx], "CATEGORIZATION", column_names, alignment, column_marge);
            lprof:print_column_header(column_names,alignment,column_marge,0);
            for processIdx,data in ipairs(cat_per_pid[nodeIdx]) do
               lprof:print_line(data,alignment,column_marge);
            end
            lprof:print_boundary(column_names,alignment,column_marge);
         end
      elseif (options["output_format"] == "csv") then
         for nodeIdx,_ in ipairs (cat_per_pid) do
            output_path = options["output_path"]..options["output_prefix"].."lprof_"..lprof.output_file_names.cat_process.."_"..context.hostnameIdxToHostname[nodeIdx].."_xp_"..fs.basename(options["experiment_path"]);
            export_to_csv(output_path,cat_per_pid[nodeIdx],";",column_names);
         end
      end
   end

   --PER THREAD
   if ( options["categorization"] == lprof.CATEGORIZATION_FULL or options["categorization"] == lprof.CATEGORIZATION_THREAD ) then
      local column_names = lprof:split("THREAD ID,"..columnHeadersExtraCat,",");
      for _, category in ipairs(categoriesToRemove) do -- remove 0% categories
         table.remove(column_names, category)
      end

      if (options["output_format"] == "txt") then
         for nodeIdx,_ in ipairs (cat_per_thread) do
            for processIdx,_ in ipairs(cat_per_thread[nodeIdx]) do
               local alignment = lprof:search_column_alignment(column_names,cat_per_thread[nodeIdx][processIdx]);
               lprof:print_hostname ("PROCESS #"..context.pidIdxToPid[nodeIdx][processIdx], "CATEGORIZATION", column_names, alignment, column_marge);
               lprof:print_column_header(column_names,alignment,column_marge,0);
               for threadIdx,data in ipairs(cat_per_thread[nodeIdx][processIdx]) do
                  lprof:print_line(data,alignment,column_marge);
               end
               lprof:print_boundary(column_names,alignment,column_marge);
            end
         end
      elseif (options["output_format"] == "csv") then
         for nodeIdx,_ in ipairs (cat_per_thread) do
            for processIdx,_ in ipairs(cat_per_thread[nodeIdx]) do
               output_path = options["output_path"]..options["output_prefix"].."lprof_"..lprof.output_file_names.cat_thread.."_"..context.pidIdxToPid[nodeIdx][processIdx]..lprof.output_file_names.from_node.."_"..context.hostnameIdxToHostname[nodeIdx].."_xp_"..fs.basename(options["experiment_path"]);
               export_to_csv(output_path,cat_per_thread[nodeIdx][processIdx],";",column_names);
            end
         end
      end
   end

end

local function print_sampling_summary_view (context, display_type)
   local options = context.options
   local time_per_thread = context.time_per_thread
   
   -- We index the data by function name and not by process hierarchy (host->process->thread)
   local resume_array = {};

   -- These next two arrays permit to create an array indexed from 1 to n (easy to sort etc.)
   local fct_info_to_idx = {}; -- "Hashtable" to get the corresponding index in resume_array
   local idx_to_fct_info = {}; -- "Hastable" to get the corresponding function name(+src info)
   local fct_idx = 1;

   -- These next two arrays permit to create an array indexed from 1 to n (easy to sort etc.)
   local loop_id_to_idx = {}; -- "Hashtable" to get the corresponding index in resume_array
   local idx_to_loop_id = {}; -- "Hastable" to get the corresponding loop id
   local loop_idx = 1;

   local total_walltime = 0;
   local column_names = nil;

   local MODULE_IDX = nil;

   -- Select the average idx in the summary array.
   -- It's two different indexes for fct and loop summary view
   local avgIdx = nil;
   if (display_type == lprof.INFO_TYPE_FUNC) then
      avgIdx = lprof.SUMMARY_FCT_AVERAGE_IDX;
   else
      avgIdx = lprof.SUMMARY_LOOP_AVERAGE_IDX;
   end

   ---------------- DECLARATION OF EMBEDDED FUNCTIONS ----------------

   -- Sort the times of the same function/loop (each threads has his own time)
   function sort_threads_by_time (t1,t2)
      local time_t1 = t1[lprof.RESUME_TIME_SECOND_IDX];
      local time_t2 = t2[lprof.RESUME_TIME_SECOND_IDX];

      return  time_t1 > time_t2;
   end

   -- Sort functions/loops by time
   function sort_by_time (a,b)
      -- a[1],b[1] are the maximum time of these functions (already sorted with the function sort_threads_by_time).
      local time_a = a[1][lprof.RESUME_TIME_SECOND_IDX];
      local time_b = b[1][lprof.RESUME_TIME_SECOND_IDX];

      return  time_a > time_b;
   end

   -- Sort functions by time average
   function sort_by_time_average (a,b)
      return tonumber(a[avgIdx]) > tonumber(b[avgIdx]);
   end

   -- Create a fct object which contains :
   -- [1]=hostIdx,  [2]=pidIdx,   [3]=threadIdx, [4]=time(s),
   -- [5]=time(%),  [6]=fctInfo,  [7]=nbSamples, [8]=module
   function new_fct (data,hostIdx,pidIdx,threadIdx)
      --TODO Do not truncate the function name here. Instead, update the display to truncate if the string is
      --     too long
      --fct_name          = string.sub(data[lprof.FCT_NAME_IDX], 1,lprof.FCT_NAME_STR_MAX_SIZE);
      fct_name          = data[lprof.FCT_NAME_IDX]

      fct_src_info      = data[lprof.FCT_SRC_INFO_IDX];
      if (fct_src_info == "-1") then
         fct_src_info = "";
      end

      fct_info          = data[lprof.FCT_NAME_IDX];
      split_time_sample = lprof:split(data[lprof.FCT_TIME_PERCENT_IDX],"(");
      fct_time_percent  = split_time_sample[1];
      if (options["show_samples_value"]) then
         fct_samples = lprof:split(split_time_sample[2],")")[1];
      else
         fct_samples = 0;
      end
      fct_time_second   = data[lprof.FCT_TIME_SECOND_IDX];
      module            = data[MODULE_IDX];
      if (fct_info_to_idx[fct_info] == nil) then
         fct_info_to_idx[fct_info] = fct_idx;
         idx_to_fct_info[fct_idx] = fct_info;
         resume_array[fct_idx] = {};
         fct_idx = fct_idx + 1;
      end
      -- Host, PID, TID, Time (sec), Time (%), Fct Name, Source Info, Module
      local fct = {true, true, true, true, true, true, true,true,true};
      fct[lprof.RESUME_HOST_IDX]         = context.hostnameIdxToHostname[hostIdx];
      fct[lprof.RESUME_PID_IDX]          = context.pidIdxToPid[hostIdx][pidIdx];
      fct[lprof.RESUME_TID_IDX]          = context.thread_idx_to_thread_id[hostIdx][pidIdx][threadIdx];
      fct[lprof.RESUME_TIME_SECOND_IDX]  = tonumber(fct_time_second);
      fct[lprof.RESUME_TIME_PERCENT_IDX] = tonumber(fct_time_percent);
      fct[lprof.RESUME_FCT_NAME_IDX]     = fct_name;
      fct[lprof.RESUME_FCT_SRC_INFO_IDX] = fct_src_info;
      fct[lprof.RESUME_FCT_SAMPLES_IDX]  = fct_samples;
      fct[lprof.RESUME_FCT_MODULE_IDX]   = module;

      return fct;
   end

   -- Create a loop object which contains :
   -- [1]=hostIdx,  [2]=pidIdx,   [3]=threadIdx, [4]=time(s),   [5]=time(%),
   -- [6]=loopId,   [7]=srcInfos, [8]=level,     [9]=nbSamples, [9]=moduleName
   function new_loop (data,hostIdx,pidIdx,threadIdx)
      loop_id             = data[lprof.LOOP_ID_IDX];
      --fct_name            = string.sub(data[lprof.LOOP_FCT_INFO_NAME_IDX],1,lprof.FCT_NAME_STR_MAX_SIZE);
      fct_name            = data[lprof.LOOP_FCT_INFO_NAME_IDX];
      src_line_info       = data[lprof.LOOP_SRC_LINE_INFO_IDX];
      if (src_line_info == "-1") then
         src_line_info = "";
      end
      loop_level          = data[lprof.LOOP_LEVEL_IDX];
      time_and_nb_samples = lprof:split(data[lprof.LOOP_TIME_PERCENT_IDX],"(");
      loop_time_percent   = time_and_nb_samples[1];
      if (options["show_samples_value"]) then
         loop_samples = lprof:split(time_and_nb_samples[2],")")[1];
      else
         loop_samples = 0;
      end
      loop_time_second = data[lprof.LOOP_TIME_SECOND_IDX];
      module           = data[MODULE_IDX];
      if (loop_id_to_idx[module..":"..loop_id] == nil) then
         loop_id_to_idx[module..":"..loop_id] = loop_idx;
         idx_to_loop_id[loop_idx] = module..":"..loop_id;
         resume_array[loop_idx] = {};
         loop_idx = loop_idx + 1;
      end
      -- Host, PID, TID, Time (sec), Time (%), loop id, fct name ,src infos, loop level, samples, , module
      local loop = {true,true,true,true,true,true,true,true,true,true,true};
      loop[lprof.RESUME_HOST_IDX]           = context.hostnameIdxToHostname[hostIdx];
      loop[lprof.RESUME_PID_IDX]            = context.pidIdxToPid[hostIdx][pidIdx];
      loop[lprof.RESUME_TID_IDX]            = context.thread_idx_to_thread_id[hostIdx][pidIdx][threadIdx];
      loop[lprof.RESUME_TIME_SECOND_IDX]    = tonumber(loop_time_second);
      loop[lprof.RESUME_TIME_PERCENT_IDX]   = tonumber(loop_time_percent);
      loop[lprof.RESUME_LOOP_ID]            = loop_id_to_idx[module..":"..loop_id];
      loop[lprof.RESUME_LOOP_FCT_NAME_IDX]  = fct_name;
      loop[lprof.RESUME_LOOP_SRC_INFO_IDX]  = src_line_info;
      loop[lprof.RESUME_LOOP_LEVEL_IDX]     = loop_level;
      loop[lprof.RESUME_LOOP_SAMPLES_IDX]   = loop_samples;
      loop[lprof.RESUME_LOOP_MODULE_IDX]    = module;
      return loop;
   end

   -- Create an object which contains all the info needed to print the fct summary view
   -- fct summary view : [1]   = Function Name
   --                    [2]   = Source Info,
   --                    [3]   = Time Average(%),
   --                    [4]   = Time Min(s) [TID],
   --                    [5]   = Time Max(s)[TID],
   --                    [6]   = Time Average(s),
   --                    [7]   = Samples Min-Max, NOT REQUIRED
   --                    [7/8] = Module
   function new_fct_summary_entry (fcts,idx,idx_to_fct_info,average,nbThreads,total_walltime)
      local fct_summary_data = {true,true,true,true,true,true,true};
      fct_summary_data[1] = fcts[1][lprof.RESUME_FCT_NAME_IDX];
      fct_summary_data[2] = fcts[1][lprof.RESUME_FCT_MODULE_IDX];
      fct_summary_data[3] = fcts[1][lprof.RESUME_FCT_SRC_INFO_IDX];
      --[[ Median calculation (over total #threads) if needed in the future
      local fctTimes = {}
      local fctNbOccurrences = #fcts
      -- Add measured times
      for i=1, fctNbOccurrences do
         table.insert(fctTimes, fcts[i][lprof.RESUME_TIME_SECOND_IDX])
      end
      -- Add 0 in case #measurements < #threads
      for i=(fctNbOccurrences+1), nbThreads do
         table.insert(fctTimes, 0)
      end
      -- Compute median time
      local fctMedianTime = lprof:get_median(fctTimes)
      -- "fctMedianTime" can now be added to array "fct_summary_data"
      --]]
      fct_summary_data[4] = string.format("%.02f",total_time[idx]*100/total_walltime);
      fct_summary_data[5] = string.format("%.02f",fcts[#fcts][lprof.RESUME_TIME_SECOND_IDX]).." ["..fcts[#fcts][lprof.RESUME_TID_IDX].."]";
      fct_summary_data[6] = string.format("%.02f",fcts[1][lprof.RESUME_TIME_SECOND_IDX]).." ["..fcts[1][lprof.RESUME_TID_IDX].."]";
      --fct_summary_data[7] = string.format("%.02f",total_time[idx]/nb_threads_assigned[idx]);
      fct_summary_data[7] = string.format("%.02f",total_time[idx]/nbThreads);
      if (options["show_samples_value"]) then
         fct_summary_data[8] = fcts[#fcts][lprof.RESUME_FCT_SAMPLES_IDX].." - "..fcts[1][lprof.RESUME_FCT_SAMPLES_IDX];
      end
      return fct_summary_data;
   end

   -- Create an object which contains all the info needed to print the loop summary view
   -- loop summary view : [1]    = Loop ID
   --                     [2]    = Module
   --                     [3]    = Fct Name,
   --                     [4]    = Source infos,
   --                     [5]    = Level,
   --                     [6]    = Time Average (%),
   --                     [7]    = Time Min(s) [TID],
   --                     [8]    = Time Max(s)[TID],
   --                     [9]    = Time Average(s),
   --                     [10]   = (Samples Min-Max), NOT REQUIRED
   function new_loop_summary_entry (loops,idx,idx_to_loop_id,average,nbThreads,total_walltime)
      local loop_summary_data = {true,true,true,true,true,true,true,true,true};
      local lid = string.gsub (idx_to_loop_id[loops[1][lprof.RESUME_LOOP_ID]], ".*:", "")
      loop_summary_data[1] = lid;
      loop_summary_data[2] = loops[1][lprof.RESUME_LOOP_MODULE_IDX];
      loop_summary_data[3] = loops[1][lprof.RESUME_LOOP_FCT_NAME_IDX];
      loop_summary_data[4] = loops[1][lprof.RESUME_LOOP_SRC_INFO_IDX];
      loop_summary_data[5] = loops[1][lprof.RESUME_LOOP_LEVEL_IDX];
      --[[ Median calculation (over total #threads) if needed in the future
      local loopTimes = {}
      local loopNbOccurrences = #loops
      -- Add measured times
      for i=1, loopNbOccurrences do
         table.insert(loopTimes, loops[i][lprof.RESUME_TIME_SECOND_IDX])
      end
      -- Add 0 in case #measurements < #threads
      for i=(loopNbOccurrences+1), nbThreads do
         table.insert(loopTimes, 0)
      end
      -- Compute median time
      local loopMedianTime = lprof:get_median(loopTimes)
      -- "loopMedianTime" can now be added to array "loop_summary_data"
      --]]
      loop_summary_data[6] = string.format("%.02f",total_time[idx]*100/total_walltime);
      loop_summary_data[7] = string.format("%.02f",loops[#loops][lprof.RESUME_TIME_SECOND_IDX]).." ["..loops[#loops][lprof.RESUME_TID_IDX].."]";
      loop_summary_data[8] = string.format("%.02f",loops[1][lprof.RESUME_TIME_SECOND_IDX]).." ["..loops[1][lprof.RESUME_TID_IDX].."]";
      --loop_summary_data[9] = string.format("%.02f",total_time[idx]/nb_threads_assigned[idx]);
      loop_summary_data[9] = string.format("%.02f",total_time[idx]/nbThreads);
      if (options["show_samples_value"]) then
         loop_summary_data[10] = loops[#loops][lprof.RESUME_LOOP_SAMPLES_IDX].." - "..loops[1][lprof.RESUME_LOOP_SAMPLES_IDX];
      end
      return loop_summary_data;
   end

   ---------------- END OF DECLARATION OF EMBEDDED FUNCTIONS ----------------

   local nbThreads = 0

   --CREATION OF THE RESUME ARRAY
   --resume_array[Functions Name (1..n)][Function data for each threads (1..t)]
   local results_array
   if     (display_type == lprof.INFO_TYPE_FUNC) then results_array = context.results_array
   elseif (display_type == lprof.INFO_TYPE_LOOP) then results_array = context.results_loops_array
   end
   for hostIdx,host in ipairs(results_array) do
      for pidIdx,pid in ipairs(host) do
         for threadIdx,thread in ipairs (pid) do
            nbThreads = nbThreads + 1
            for dataIdx, data in ipairs (thread) do
               data = lprof:split(data,";");
               -- DISPLAY FCTS SUMMARY
               if (display_type == lprof.INFO_TYPE_FUNC) then
                  -- SELECT MODULE IDX
                  if (options["hwc_mode"] == "maqao_events") then
                     MODULE_IDX = lprof.FCT_MODULE_IDX;
                  elseif (options["hwc_mode"] == "timer") then
                     MODULE_IDX = lprof.TIMER_FCT_MODULE_IDX;
                  end
                  local fct = new_fct(data,hostIdx,pidIdx,threadIdx);
                  table.insert (resume_array[fct_info_to_idx[fct_info]], fct);
               else -- DISPLAY LOOPS SUMMARY
                  if (options["hwc_mode"] == "maqao_events") then
                     MODULE_IDX = lprof.LOOP_MODULE_IDX;
                  elseif (options["hwc_mode"] == "timer") then
                     MODULE_IDX = lprof.TIMER_LOOP_MODULE_IDX;
                  end
                  local loop = new_loop(data,hostIdx,pidIdx,threadIdx);

                  table.insert (resume_array[loop_id_to_idx[module..":"..loop_id]], loop);
               end
            end
         end
      end
   end


   -- Add the walltime of each thread (in second)
   for hostIdx,_ in ipairs(time_per_thread) do
      for pidIdx,_ in ipairs(time_per_thread[hostIdx]) do
         for threadIdx,_ in ipairs(time_per_thread[hostIdx][pidIdx]) do
            total_walltime = total_walltime + time_per_thread[hostIdx][pidIdx][threadIdx];
         end
      end
   end

   --SORT FOR THE FUNCTION/LOOP THE TIME OF EACH THREADS
   for idx ,_ in ipairs (resume_array) do
      table.sort (resume_array[idx],sort_threads_by_time);
   end

   --SORT THE FUNCTIONS/LOOPS BY TIME
   table.sort(resume_array,sort_by_time)

   --SUM TIME (in sec) SPENT FOR EACH FUNCTION/LOOP IN EACH THREADS
   total_time = {};
   -- We store the number of threads in which the function/loop appears
   nb_threads_assigned = {};
   for idx,fcts in pairs (resume_array) do
      sum = 0;
      for _,fct_data in ipairs (fcts) do
         sum = sum + fct_data[lprof.RESUME_TIME_SECOND_IDX];
      end
      table.insert(total_time,sum);
      table.insert(nb_threads_assigned,#fcts);
   end


   --CREATE THE SUMMARY ARRAY (FCTS/LOOPS)
   summary_array = {};
   for idx,data in pairs (resume_array) do
      if (display_type == lprof.INFO_TYPE_FUNC) then --FCT
         local fct_summary_data = new_fct_summary_entry(data,idx,idx_to_fct_info,average,nbThreads,total_walltime);
         table.insert(summary_array,fct_summary_data); --ADD ONE ENTRY
      else -- LOOP
         local loop_summary_data = new_loop_summary_entry(data,idx,idx_to_loop_id,average,nbThreads,total_walltime);
         table.insert(summary_array,loop_summary_data); --ADD ONE ENTRY
      end
   end

   --SORT THE ARRAY BY THE AVERAGE OF TIME TAKEN BY A FUNCTION/LOOP
   -- MAX TO MIN TIME AVERAGE
   table.sort(summary_array,sort_by_time_average);

   -- SELECT THE COLUMN NAMES
   local column_marge = 2;
   if (options["show_samples_value"]) then
      if (display_type == lprof.INFO_TYPE_FUNC) then --FCT
         column_names = lprof:split(lprof.SUMMARY_COLUMN_NAMES_FCT_SSV,",");
      else -- LOOP
         column_names = lprof:split(lprof.SUMMARY_COLUMN_NAMES_LOOP_SSV,",");
      end
   else -- NO SAMPLES COLUMN
      if (display_type == lprof.INFO_TYPE_FUNC) then -- FCT
         column_names = lprof:split(lprof.SUMMARY_COLUMN_NAMES_FCT,",");
      else -- LOOP
         column_names = lprof:split(lprof.SUMMARY_COLUMN_NAMES_LOOP,",");
      end
   end


   if (options["output_format"] == "txt") then
      --local extra_info = "WALLTIME "..string.format("%.2f",max_walltime).."(s)";
      local extra_info = nil;
      --DISPLAY THE FUNCTIONS/LOOPS SUMMARY VIEW
      alignment = lprof:search_column_alignment(column_names,summary_array);
      lprof:print_hostname ("HOTSPOTS SUMMARY",nil, column_names, alignment, column_marge);
      lprof:print_column_header(column_names,alignment,column_marge,0);
      local currentCumulativeCoverage = 0
      for idx,data in ipairs(summary_array) do
         if (currentCumulativeCoverage > options["cumulative_threshold"]) then
            break;
         else
            if (display_type == lprof.INFO_TYPE_FUNC) then
               currentCumulativeCoverage = currentCumulativeCoverage + tonumber(data[4])
            else
               currentCumulativeCoverage = currentCumulativeCoverage + tonumber(data[6])
            end
            lprof:print_line(data,alignment,column_marge);
         end
      end
      lprof:print_boundary(column_names,alignment,column_marge);
   elseif ( options["output_format"] == "csv" ) then
      local output_format;
      if ( display_type == lprof.INFO_TYPE_FUNC ) then
         output_format = lprof.output_file_names.functions_summary;
      elseif ( display_type == lprof.INFO_TYPE_LOOP ) then
         output_format = lprof.output_file_names.loops_summary;
      end
      local output_path = options["output_path"]..options["output_prefix"].."lprof_"..output_format.."_xp_"..fs.basename(options["experiment_path"]);
      export_to_csv(output_path,summary_array,";",column_names);
   end

end

local function print_sampling_full_view (context, display_type)
   local results_array
   if     (display_type == lprof.INFO_TYPE_FUNC) then results_array = context.results_array
   elseif (display_type == lprof.INFO_TYPE_LOOP) then results_array = context.results_loops_array
   end
   local options = context.options

   local current_cumulative_value = 0;
   local cumulative_threshold_max;
   local idx = 1;
   local column_names = "";

   function sort_loop_fct (a,b)
      for idColumn,value in ipairs(a) do
         if idColumn== lprof.LOOP_TIME_PERCENT_IDX then
            time = lprof:split(value,"(");
            k1=time[1];
            break;
         end;
      end;
      for idColumn,value in ipairs(b) do
         if idColumn==lprof.LOOP_TIME_PERCENT_IDX then
            time = lprof:split(value,"(");
            k2=time[1];
            break;
         end;
      end;

      -- if k1 or k2 is a column name!
      if (tonumber(k1) == nil ) then
         return true
      else
         if (tonumber(k2) == nil ) then
            return false
         end
      end

      return tonumber(k1) > tonumber(k2);
   end

   function sort_fct (a,b)
      for idColumn,value in ipairs(a) do
         if idColumn==lprof.FCT_TIME_PERCENT_IDX then
            time = lprof:split(value,"(")
            k1=time[1];
            break;
         end
      end
      for idColumn,value in ipairs(b) do
         if idColumn==lprof.FCT_TIME_PERCENT_IDX then
            time = lprof:split(value,"(")
            k2=time[1];
            break;
         end
      end

      -- if k1 or k2 is a column name!
      if (tonumber(k1) == nil ) then
         return true
      else
         if (tonumber(k2) == nil ) then
            return false
         end
      end

      return tonumber(k1) > tonumber(k2);
   end

   for hostIdx,host in ipairs(results_array) do
      for pidIdx,pid in ipairs(host) do
         for threadIdx,thread_info in ipairs (pid) do
            for line_id, line in ipairs (thread_info) do
               results_array[hostIdx][pidIdx][threadIdx][line_id] = lprof:split(line,";");
               -- Cut the "Function Name" column : FCT_NAME_STR_MAX_SIZE max characters
               --results_array[hostIdx][pidIdx][threadIdx][line_id][lprof.FCT_NAME_IDX] =
               --  string.sub(results_array[hostIdx][pidIdx][threadIdx][line_id][1],lprof.FCT_NAME_IDX,lprof.FCT_NAME_STR_MAX_SIZE);
               if ( display_type == lprof.INFO_TYPE_FUNC ) then
                  if (results_array[hostIdx][pidIdx][threadIdx][line_id][lprof.FCT_SRC_INFO_IDX] == "-1") then
                     results_array[hostIdx][pidIdx][threadIdx][line_id][lprof.FCT_SRC_INFO_IDX] = "";
                  end
               else
                  if (results_array[hostIdx][pidIdx][threadIdx][line_id][lprof.LOOP_SRC_LINE_INFO_IDX] == "-1") then
                     results_array[hostIdx][pidIdx][threadIdx][line_id][lprof.LOOP_SRC_LINE_INFO_IDX] = "";
                  end
               end
            end
         end
      end
   end


   --TODO: Sort the table (on one of the event sampled)
   --idx = 1;

   --local alignment;
   for hostIdx,host in ipairs(results_array) do
      for pidIdx,pid in ipairs(host) do
         for threadIdx,output_display in ipairs(pid) do
            if (table.getn(output_display) >= 1) then
               -- Thread should be removed in function get_time_info
               -- So we need to check if it still exists
               if (context.time_per_thread[hostIdx][pidIdx][idx] ~= nil) then
                  --DISPLAY LOOP
                  column_names = lprof:select_column_names(display_type,options,context.hwc_list);
                  if (display_type == lprof.INFO_TYPE_LOOP) then
                     table.sort(output_display, sort_loop_fct);
                  else
                     --DISPLAY FUNCTION
                     table.sort(output_display, sort_fct);
                  end

                  if (options["output_format"] == "txt") then
                     --array that store the spaces neccesary to align the columns
                     alignment = lprof:search_column_alignment(column_names,output_display);
                     --Spaces added on each side of a column value
                     local column_marge = 2;
                     local extra_info = "PROCESS #"..context.pidIdxToPid[hostIdx][pidIdx];
                     lprof:print_hostname (context.hostnameIdxToHostname[hostIdx], extra_info, column_names, alignment, column_marge);
                     lprof:print_thread_id (context.thread_idx_to_thread_id[hostIdx][pidIdx][idx], context.time_per_thread[hostIdx][pidIdx][idx],column_names, alignment, column_marge);
                     --The display part himself
                     lprof:print_column_header(column_names,alignment,column_marge,thread_id);
                     -- print each lines (infos are stored from 2 to N)
                     for i=1,table.getn(output_display)  do
                        if (current_cumulative_value > options["cumulative_threshold"]) then
                           break;
                        else
                           -- Get the time percentage : format = XX.X (#NB_SAMPLES)
                           -- So we need to extract the XX.X%
                           if (display_type == lprof.INFO_TYPE_LOOP) then
                              time_percentage = lprof:split(output_display[i][lprof.LOOP_TIME_PERCENT_IDX]," ")[1];
                           else
                              time_percentage = lprof:split(output_display[i][lprof.FCT_TIME_PERCENT_IDX]," ")[1];
                           end
                           current_cumulative_value = current_cumulative_value + time_percentage;
                           lprof:print_line(output_display[i], alignment, column_marge);

                           if (display_type == lprof.INFO_TYPE_FUNC and options["display_callchains"] == true) then
                              --callchainKey is the key (the function name) for the callchains array to retrieve
                              --the corresponding callchains for one function.
                              callchainKey = output_display[i][lprof.FCT_NAME_IDX];
                              if (context.callchains[hostIdx][pidIdx][threadIdx][callchainKey] ~= nil) then
                                 local callchains_percentage = {};
                                 local callchains_string = {};
                                 local callchains_info = {};
                                 local idx_cc = 1;
                                 for cc_string, cc_percentage in pairs (context.callchains[hostIdx][pidIdx][threadIdx][callchainKey]) do
                                    callchains_info[idx_cc] = {};
                                    callchains_info[idx_cc].percentage  = cc_percentage;
                                    callchains_info[idx_cc].string = cc_string;
                                    idx_cc = idx_cc + 1;
                                 end

                                 table.sort(callchains_info, function(a,b) return a.percentage > b.percentage end);

                                 -- Compute spaces needed to add after the callchain
                                 local spaces = 0;
                                 for _,value in ipairs (alignment) do
                                    spaces = spaces + value + column_marge;
                                 end

                                 for cc_idx, callchain in pairs (callchains_info) do
                                    if ( callchain.percentage > options["callchain_value_filter"]  ) then
                                       print_callchain(callchain.string ,string.format("%05.2f",callchain.percentage),spaces,column_marge);
                                    end
                                 end
                              end
                           end
                        end
                     end
                     lprof:print_boundary(column_names,alignment,column_marge);

                  elseif (options["output_format"] == "csv") then
                     local output_format;
                     if ( options.display_type == lprof.INFO_TYPE_FUNC ) then
                        output_format = lprof.output_file_names.functions;
                     elseif ( options.display_type == lprof.INFO_TYPE_LOOP ) then
                        output_format = lprof.output_file_names.loops;
                     end
                     local output_path = options["output_path"]..options["output_prefix"].."lprof_"..output_format.."_xp_"..fs.basename(options["experiment_path"]).."_H_"..context.hostnameIdxToHostname[hostIdx].."_P_"..context.pidIdxToPid[hostIdx][pidIdx].."_T_"..context.thread_idx_to_thread_id[hostIdx][pidIdx][threadIdx];
                     export_to_csv(output_path,output_display,";",column_names);
                  end
               end
            end
            idx = idx + 1;
            current_cumulative_value = 0;
         end
         idx = 1;
      end
   end
end

local function print_sampling_txt (context)
   local options = context.options

   local display_type = options.display_type;

   if (options["hwc_mode"] ~= "custom_events" and options["view"] == "summary") then
      if (display_type == lprof.INFO_TYPE_FUNC) then
         print_samples_categorisation (context);
         print(); -- ADD A SPACE BETWEEN THESE TWO ARRAYS
      end
      print_sampling_summary_view(context, display_type);
   -- elseif (options["hwc_mode"] == "custom_events" or options["view"] == "full") then
   elseif (options["hwc_mode"] == "custom_events" or options["view"] == "full" or options["view"] == "process") then
      if (display_type == lprof.INFO_TYPE_FUNC) then
         print_samples_categorisation (context);
         print(); -- ADD A SPACE BETWEEN THESE TWO ARRAYS
      end
      -- /!\ process view currently only available w/ default hardware counters (i.e. default events)
      --if ((options["hwc_mode"] == "maqao_events") and (options["view"] == "process")) then
      --   lprof:print_sampling_process_view(context, display_type)
      --else
         print_sampling_full_view (context, display_type)
      --end
   else
      Message:error("[MAQAO] View option is unknown!");
      Message:error("[MAQAO] Available views : summary,full");
      -- Message:error("[MAQAO] Available views : summary, process, full");
   end

end

function lprof:select_column_names (display_type,options,hwc_list)

   local column_names = nil;
   -- DISPLAY LOOP
   if (display_type == lprof.INFO_TYPE_LOOP) then
      if (options["hwc_mode"] == "maqao_events") then
         if (options["extended_mode"] == false) then
            column_names = lprof:split(lprof.FULL_COLUMN_NAMES_LOOP,",");
         else
            column_names = lprof:split(lprof.FULL_COLUMN_NAMES_LOOP..",L1D:REPLACEMENT,L2_LINES_IN:ANY,ARITH:FPU_DIV",",");
         end
      elseif (options["hwc_mode"] == "custom_events") then
         columns = lprof.CUSTOM_FULL_COLUMN_NAMES_LOOP..hwc_list;
         column_names = lprof:split(columns,",");
      elseif (options["hwc_mode"] == "timer") then
         column_names = lprof:split(lprof.TIMER_FULL_COLUMN_NAMES_LOOP,",");
      end
   else
      --DISPLAY FUNCTION
      if (options["hwc_mode"] == "maqao_events") then
         if (options["extended_mode"] == false) then
            column_names = lprof:split(lprof.FULL_COLUMN_NAMES_FCT,",");
         else
            column_names = lprof:split(lprof.FULL_COLUMN_NAMES_FCT..",L1D:REPLACEMENT,L2_LINES_IN:ANY,ARITH:FPU_DIV",",");
         end
      elseif (options["hwc_mode"] == "custom_events") then
         columns = lprof.CUSTOM_FULL_COLUMN_NAMES_FCT..hwc_list;
         column_names = lprof:split(columns,",");
      elseif (options["hwc_mode"] == "timer") then
         column_names = lprof:split(lprof.TIMER_FULL_COLUMN_NAMES_FCT,",");
      end
   end

   return column_names;
end

-- **************************************************************************
-- * WARNING: not 100% reliable (and so disabled) since process != MPI rank *
-- **************************************************************************
-- prints profiling results at process level
local function print_sampling_process_view (context, display_type)
   local results_array
   if     (display_type == lprof.INFO_TYPE_FUNC) then
      results_array = context.results_array
   elseif (display_type == lprof.INFO_TYPE_LOOP) then
      results_array = context.results_loops_array
   end
   local options = context.options

   local cumulative_threshold_max;
   local idx = 1;
   --local column_names = "";
   local columnHeaders = {}

   function sort_by_descending_coverage(region1, region2)
      -- sort functions
      if (display_type == lprof.INFO_TYPE_FUNC) then
         for idx, value in ipairs(region1) do
            if (idx == lprof.FCT_TIME_PERCENT_IDX) then
               coverage1 = lprof:split(value, "(")[1]
            end
         end
         for idx, value in ipairs(region2) do
            if (idx == lprof.FCT_TIME_PERCENT_IDX) then
               coverage2 = lprof:split(value, "(")[1]
            end
         end
      -- sort loops
      elseif (display_type == lprof.INFO_TYPE_LOOP) then
         for idx, value in ipairs(region1) do
            if (idx == lprof.LOOP_TIME_PERCENT_IDX) then
               coverage1 = lprof:split(value, "(")[1]
            end
         end
         for idx, value in ipairs(region2) do
            if (idx == lprof.LOOP_TIME_PERCENT_IDX) then
               coverage2 = lprof:split(value, "(")[1]
            end
         end
      end

      return (tonumber(coverage1) > tonumber(coverage2))
   end
   
   function print_min_max_thread_time (tid, tTime, minXORmax, columnHeaders, alignment, columnMargin)
      local widthSize = get_width_size(columnHeaders, alignment, columnMargin)
      local timeInfo = "Time "..minXORmax..": "..string.format("%.2f", tTime).." second(s) - Thread #"..tid
      local marginToCenter = widthSize/2 - string.len(timeInfo)/2;
      io.stdout:write(string.rep(" ", marginToCenter)..timeInfo.."\n");
   end

   -- *********************************************************
   -- * split lines containing threads' data to create tables *
   -- *********************************************************

   -- each line is like:
   -- [fct] <fct_name>;<module>;<src_info>;<time(%)>;<time(s)>;<CPI>
   -- [loop] <loop_id>;<module>;<fct>;<src_info>;<level>;<time(%)>;<time(s)>;<CPI>
   -- before: thread = {csv lines w/ separator = ";"}
   -- after: thread = {Lua tables}
   for hIdx, host in ipairs(results_array) do
      for pIdx, process in ipairs(host) do
         for tIdx, thread in ipairs (process) do
            for lineIdx, line in ipairs (thread) do
               results_array[hIdx][pIdx][tIdx][lineIdx] = lprof:split(line,";");
               -- <src_info> may be missing (i.e. valued -1)
               -- if so, to be replaced by empty string
               if ( display_type == lprof.INFO_TYPE_FUNC ) then
                  if (results_array[hIdx][pIdx][tIdx][lineIdx][lprof.FCT_SRC_INFO_IDX] == "-1") then
                     results_array[hIdx][pIdx][tIdx][lineIdx][lprof.FCT_SRC_INFO_IDX] = "";
                  end
               else
                  if (results_array[hIdx][pIdx][tIdx][lineIdx][lprof.LOOP_SRC_LINE_INFO_IDX] == "-1") then
                     results_array[hIdx][pIdx][tIdx][lineIdx][lprof.LOOP_SRC_LINE_INFO_IDX] = "";
                  end
               end
            end
         end
      end
   end

   -- **************************************************
   -- * get the number of hosts, processes and threads *
   -- **************************************************

   --[[
   local nbNodes, nbProcesses, nbThreads = 0, 0, 0
   for _, host in ipairs(results_array) do
      nbNodes = nbNodes + 1
      for _, process in ipairs(host) do
         nbProcesses = nbProcesses + 1
         for _, thread in ipairs(processes) do
            nbThreads = nbThreads + 1
         end
      end
   end
   --]]


   --TODO: Sort the table (on one of the event sampled)
   --idx = 1;

   --local alignment;
   for hIdx, host in ipairs(results_array) do
      for pIdx, process in ipairs(host) do
         local pTime = 0
         --local pThreadsTimes = {}
         local tidMinTime = context.thread_idx_to_thread_id[hIdx][pIdx][idx]
         local tMinTime = context.time_per_thread[hIdx][pIdx][idx]
         local tidMaxTime = context.thread_idx_to_thread_id[hIdx][pIdx][idx]
         local tMaxTime = context.time_per_thread[hIdx][pIdx][idx]
         local tmp_pRegions = {}
         local pRegions = {}
         local currentCumulativeCoverage = 0
         for tIdx,output_display in ipairs(process) do
            local TID = context.thread_idx_to_thread_id[hIdx][pIdx][idx]
            if (table.getn(output_display) >= 1) then
               -- Thread should be removed in function get_time_info
               -- So we need to check if it still exists
               if (context.time_per_thread[hIdx][pIdx][idx] ~= nil) then
                  -- update time of current process
                  pTime = pTime + context.time_per_thread[hIdx][pIdx][idx]
                  -- add current thread time to set of times of current process
                  --table.insert(pThreadsTimes, context.time_per_thread[hIdx][pIdx][idx])
                  -- update min/max thread time/id if required
                  if (context.time_per_thread[hIdx][pIdx][idx] < tMinTime) then
                     tMinTime = context.time_per_thread[hIdx][pIdx][idx]
                     tidMinTime = context.thread_idx_to_thread_id[hIdx][pIdx][idx]
                  elseif (context.time_per_thread[hIdx][pIdx][idx] > tMaxTime) then
                     tMaxTime = context.time_per_thread[hIdx][pIdx][idx]
                     tidMaxTime = context.thread_idx_to_thread_id[hIdx][pIdx][idx]
                  end
                  --column_names = lprof:select_column_names(display_type,options,context.hwc_list);
                  if (display_type == lprof.INFO_TYPE_FUNC) then
                     table.sort(output_display, sort_by_descending_coverage);

                     -- ********************************************
                     -- * sum time for each fct of current process *
                     -- ********************************************

                     if (options["hwc_mode"] == "maqao_events") then
                        for _, fct in ipairs(output_display) do
                           if (tmp_pRegions[fct[lprof.FCT_NAME_IDX]] == nil) then -- new fct within current process
                              tmp_pRegions[fct[lprof.FCT_NAME_IDX]] = {}
                              tmp_pRegions[fct[lprof.FCT_NAME_IDX]][lprof.FCT_NAME_IDX] = fct[lprof.FCT_NAME_IDX]
                              tmp_pRegions[fct[lprof.FCT_NAME_IDX]][lprof.FCT_MODULE_IDX] = fct[lprof.FCT_MODULE_IDX]
                              tmp_pRegions[fct[lprof.FCT_NAME_IDX]][lprof.FCT_SRC_INFO_IDX] = fct[lprof.FCT_SRC_INFO_IDX]
                              -- coverage is calculated using time in seconds, that's why there's "%_idx = s_idx"
                              tmp_pRegions[fct[lprof.FCT_NAME_IDX]][lprof.FCT_TIME_PERCENT_IDX] = tonumber(fct[lprof.FCT_TIME_SECOND_IDX])
                              -- minimum/maximum time among threads of current process (default = 1st thread)
                              -- TODO: add two more constants for these indexes?
                              tmp_pRegions[fct[lprof.FCT_NAME_IDX]][5] = fct[lprof.FCT_TIME_SECOND_IDX].." ["..TID.."]"
                              tmp_pRegions[fct[lprof.FCT_NAME_IDX]][6] = fct[lprof.FCT_TIME_SECOND_IDX].." ["..TID.."]"
                           else -- fct already known within current process 
                              tmp_pRegions[fct[lprof.FCT_NAME_IDX]][lprof.FCT_TIME_PERCENT_IDX] = tmp_pRegions[fct[lprof.FCT_NAME_IDX]][lprof.FCT_TIME_PERCENT_IDX] + tonumber(fct[lprof.FCT_TIME_SECOND_IDX])
                              -- update minimum/maximum time if required
                              if (tonumber(fct[lprof.FCT_TIME_SECOND_IDX]) < tonumber(lprof:split(tmp_pRegions[fct[lprof.FCT_NAME_IDX]][5], " ")[1]))  then
                                 tmp_pRegions[fct[lprof.FCT_NAME_IDX]][5] = fct[lprof.FCT_TIME_SECOND_IDX].." ["..TID.."]"
                              end
                              if (tonumber(fct[lprof.FCT_TIME_SECOND_IDX]) > tonumber(lprof:split(tmp_pRegions[fct[lprof.FCT_NAME_IDX]][6], " ")[1]))  then
                                 tmp_pRegions[fct[lprof.FCT_NAME_IDX]][6] = fct[lprof.FCT_TIME_SECOND_IDX].." ["..TID.."]"
                              end
                           end
                        end

                        -- **********************
                        -- * calculate coverage *
                        -- **********************

                        if (tIdx == table.getn(process)) then -- last thread of current process
                           for _, fct in pairs(tmp_pRegions) do
                              fct[lprof.FCT_TIME_PERCENT_IDX] = string.format("%.2f", fct[lprof.FCT_TIME_PERCENT_IDX]*100/pTime)
                              table.insert(pRegions, fct) -- switch to indexed table (sortable)
                           end
                           tmp_pRegions = nil -- free memory
                           table.sort(pRegions, sort_by_descending_coverage)
                           --[[for idxFct, fct in ipairs(pRegions) do
                              print("\nFUNCTION #"..idxFct)
                              for i=1, table.getn(fct) do
                                 print(fct[i])
                              end
                           end--]]
                        end
                        columnHeaders = lprof:split(lprof.PROCESS_VIEW_HEADERS_FCT, ",")
                     end
                  else
                     table.sort(output_display, sort_by_descending_coverage);

                     -- *********************************************
                     -- * sum time for each loop of current process *
                     -- *********************************************

                     if (options["hwc_mode"] == "maqao_events") then
                        for _, loop in ipairs(output_display) do
                           if (tmp_pRegions[loop[lprof.LOOP_ID_IDX]] == nil) then -- new loop within current process
                              tmp_pRegions[loop[lprof.LOOP_ID_IDX]] = {}
                              tmp_pRegions[loop[lprof.LOOP_ID_IDX]][lprof.LOOP_ID_IDX] = loop[lprof.LOOP_ID_IDX]
                              tmp_pRegions[loop[lprof.LOOP_ID_IDX]][lprof.LOOP_MODULE_IDX] = loop[lprof.LOOP_MODULE_IDX]
                              tmp_pRegions[loop[lprof.LOOP_ID_IDX]][lprof.LOOP_FCT_INFO_NAME_IDX] = loop[lprof.LOOP_FCT_INFO_NAME_IDX]
                              tmp_pRegions[loop[lprof.LOOP_ID_IDX]][lprof.LOOP_SRC_LINE_INFO_IDX] = loop[lprof.LOOP_SRC_LINE_INFO_IDX]
                              tmp_pRegions[loop[lprof.LOOP_ID_IDX]][lprof.LOOP_LEVEL_IDX] = loop[lprof.LOOP_LEVEL_IDX]
                              -- coverage is calculated using time in seconds, that's why there's "%_idx = s_idx"
                              tmp_pRegions[loop[lprof.LOOP_ID_IDX]][lprof.LOOP_TIME_PERCENT_IDX] = tonumber(loop[lprof.LOOP_TIME_SECOND_IDX])
                              -- minimum/maximum time among threads of current process (default = 1st thread)
                              -- TODO: add two more constants for these indexes?
                              tmp_pRegions[loop[lprof.LOOP_ID_IDX]][7] = loop[lprof.LOOP_TIME_SECOND_IDX].." ["..TID.."]"
                              tmp_pRegions[loop[lprof.LOOP_ID_IDX]][8] = loop[lprof.LOOP_TIME_SECOND_IDX].." ["..TID.."]"
                           else  -- loop already known within current process
                              tmp_pRegions[loop[lprof.LOOP_ID_IDX]][lprof.LOOP_TIME_PERCENT_IDX] = tmp_pRegions[loop[lprof.LOOP_ID_IDX]][lprof.LOOP_TIME_PERCENT_IDX] + tonumber(loop[lprof.LOOP_TIME_SECOND_IDX])
                              -- update minimum/maximum time if required
                              if (tonumber(loop[lprof.LOOP_TIME_SECOND_IDX]) < tonumber(lprof:split(tmp_pRegions[loop[lprof.LOOP_ID_IDX]][7], " ")[1])) then
                                 tmp_pRegions[loop[lprof.LOOP_ID_IDX]][7] = loop[lprof.LOOP_TIME_SECOND_IDX].." ["..TID.."]"
                              end
                              if (tonumber(loop[lprof.LOOP_TIME_SECOND_IDX]) > tonumber(lprof:split(tmp_pRegions[loop[lprof.LOOP_ID_IDX]][8], " ")[1])) then
                                 tmp_pRegions[loop[lprof.LOOP_ID_IDX]][8] = loop[lprof.LOOP_TIME_SECOND_IDX].." ["..TID.."]"
                              end
                           end
                        end
                        
                        -- **********************
                        -- * calculate coverage *
                        -- **********************

                        if (tIdx == table.getn(process)) then -- last thread of current process
                           for _, loop in pairs(tmp_pRegions) do
                              loop[lprof.LOOP_TIME_PERCENT_IDX] = string.format("%.2f", loop[lprof.LOOP_TIME_PERCENT_IDX]*100/pTime)
                              table.insert(pRegions, loop) -- switch to indexed table (sortable)
                           end
                           tmp_pRegions = nil -- free memory
                           table.sort(pRegions, sort_by_descending_coverage)
                           --[[for idxLoop, loop in ipairs(pRegions) do
                              print("\nLOOP #"..idxLoop)
                              for i=1, table.getn(loop) do
                                 print(loop[i])
                              end
                           end--]]
                        end
                        columnHeaders = lprof:split(lprof.PROCESS_VIEW_HEADERS_LOOP, ",")
                     end
                  end
               end
            end
            -- sort threads times for current process
            --[[if (tIdx == table.getn(process)) then -- last thread of current process
               table.sort(pThreadsTimes)
               for idxTime, time in ipairs(pThreadsTimes) do
                  print("Time #"..idxTime.." = "..time)
               end
            end--]]
            idx = idx + 1;
         end -- tIdx
         idx = 1;
                     
         if (options["output_format"] == "txt") then
            local columnMargin = 2
            local alignment = lprof:search_column_alignment(columnHeaders, pRegions)
            local extraInfo = "PROCESS #"..context.pidIdxToPid[hIdx][pIdx]
            lprof:print_hostname(context.hostnameIdxToHostname[hIdx], extraInfo, columnHeaders, alignment, columnMargin)
            print_min_max_thread_time(tidMinTime, tMinTime, "Min", columnHeaders, alignment, columnMargin)
            print_min_max_thread_time(tidMaxTime, tMaxTime, "Max", columnHeaders, alignment, columnMargin)
            lprof:print_column_header(columnHeaders, alignment, columnMargin)
            for _, region in ipairs(pRegions) do
               if (currentCumulativeCoverage > options["cumulative_threshold"]) then
                  break
               else
                  if (display_type == lprof.INFO_TYPE_FUNC) then
                     currentCumulativeCoverage = currentCumulativeCoverage + region[lprof.FCT_TIME_PERCENT_IDX]
                  else
                     currentCumulativeCoverage = currentCumulativeCoverage + region[lprof.LOOP_TIME_PERCENT_IDX]
                  end
                  lprof:print_line(region, alignment, columnMargin)
               end
            end
            lprof:print_boundary(columnHeaders, alignment, columnMargin)
            -- TODO: add callchains
         elseif (options["output_format"] == "csv") then
            local outputFormat = ""
            local outputPath = ""
            if (display_type == lprof.INFO_TYPE_FUNC) then -- functions
               outputFormat = lprof.output_file_names.functions_process 
            else -- loops
               outputFormat = lprof.output_file_names.loops_process 
            end
            outputPath = options["output_path"]..options["output_prefix"].."lprof_"..outputFormat.."_xp_"..fs.basename(options["experiment_path"]).."_H_"..context.hostnameIdxToHostname[hIdx].."_P_"..context.pidIdxToPid[hIdx][pIdx]
            export_to_csv(outputPath, pRegions, ";", columnHeaders)
         end
      end -- pIdx
   end -- hIdx
end



local function prepare_sampling_display_txt (context)
   local options = context.options
   local display_type = options.display_type;

   local display_functions = (display_type == lprof.INFO_TYPE_FUNC)
   local display_loops     = (display_type == lprof.INFO_TYPE_LOOP)
   context.post_refactoring_table = lprof.prepare_sampling_display (
      options["experiment_path"],
      display_functions, display_loops,
      options["callchain_filter"],
      options["hwc_mode"],
      options["cpu_frequency"],
      options["ref_frequency"],
      options["sampling_period"],
      options["show_samples_value"],
      options["extended_mode"],
      options["libraries_extra_categories"]);

   get_pre_refactoring_tables (context.post_refactoring_table, display_functions, display_loops, context);

   if (display_type == lprof.INFO_TYPE_FUNC) then get_samples_categorisation (context) end
end

-- Post-process data collected in experiment directory
function prepare_sampling_display (options)

   local type_file_name = options.experiment_path.."/info.lua"
   local type_file = io.open (type_file_name, "r")
   if (type_file == nil) then
      Message:critical ("[MAQAO] Cannot read "..type_file_name)
   end
   
   local line = type_file:read()
   local sampling_info = lprof:split (line, ":")
   if (sampling_info == nil) then
      Message:error ("[MAQAO] File info.lua seems to be empty!")
   elseif (#sampling_info == 4) then
      Message:warn ("[MAQAO] File info.lua seems to be generated with lprof 2.1 or lower. "..
                       "Conversion of time events to seconds will be wrong. Please rerun with lprof >= 2.2")
   end

   local context = {
      rslt_files = Table:new(),
      options = options
   }

   options["hwc_mode"     ] = sampling_info[2];
   options.sampling_period  = sampling_info[3];
   options["cpu_frequency"] = sampling_info[4];
   options["ref_frequency"] = sampling_info[5];

   if (options["sampling_period"] == nil) then
      Message:info("Bad format for the info.lua file (sampling period is missing)");
      Message:info("Default period has been chosen.");
      if (sampling_info[1] ~= "TX") then
         options["sampling_period"] = lprof.DEFAULT_SAMPLING_PERIOD;
      else
         options["sampling_period"] = lprof.TIMER_DEFAULT_SAMPLING_PERIOD;
      end
   end

   if (options["output_format"] ~= "html") then
      prepare_sampling_display_txt (context)

   else
      if (options["hwc_mode"] == "maqao_events" or options["hwc_mode"] == "timer") then

         prepare_sampling_display_html (context);

         context.pid_idx_to_pid = context.absolute_pidIdxToPid

         -- LOADING CQA INFO (no more used)
         --dofile(options["experiment_path"].."/cqa_info.lua");
      else
         Message:info("[MAQAO] Html format is not yet available with custom mode (-hwc option)");
      end
   end

   return context
end

-- Defines the second lprof step: display results "collected" in an "experiment directory"
-- Prints data returned by prepare_sampling_display()
function print_sampling (context)
   if (context.options["output_format"] ~= "html") then
      print_sampling_txt (context);
   else
      require "lprof.display.html";
      lprof.display.html.print_sampling_html( context.options["experiment_path"],
                                              context.hostnameIdxToHostname,
                                              context.results_array, context.callchains,
                                              context.loops_child, context.outermost_loops,
                                              context.loop_id_to_time, context.is_library,
                                              context.cat_per_host, context.binary_name,
                                              context.thread_idx_to_thread_id,
                                              context.pid_idx_to_pid
      );
   end
end
