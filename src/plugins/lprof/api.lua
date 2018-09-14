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

module ("lprof.api", package.seeall)

require "lprof.collect"
require "lprof.display"

--- Loads a lprof csv file containing a loop based profiling into a table
-- @param filename Name of the lprof file to load
-- @return A table with the following structure:
--      tab = {
--          {loop_id=<int>,            -- Loop identifier
--           function_name=<string>,   -- Name of the function the loop belongs to
--           source_info=<string>,     -- Source location of the loop
--           level=<string>,           -- Level of the loop (Single, Innermost, InBetween, Outermost)
--           time_p=<number>,          -- Time spent by the loop (in percentage of the total execution time)
--           time_s=<number>,          -- Time spent by the loop (in seconds)
--           cpi_ratio=<number>,       -- Cycle Per Instruction ratio
--           module=<string>},         -- Module the loop belongs to (for Fortran codes)
--          {...},
--      }
--         or nil if there is a problem
function lprof:load_lprof_loop_CSV_file (filename)
   if type (filename) ~= "string" then
      -- Invalid parameter
      return nil
   end
   if fs.exists (filename) == false then
      -- File does not exist
      return nil
   end

   local file = io.open (filename, "r")
   if file == nil then
      -- File can not be openned
      return nil
   end

   local results = {}
   local i = 0
   for line in file:lines() do
      if i == 0 then
         -- First line contains only the legend
      else
         local tmp = String:split (line, ";")
         results[i] = {
            loop_id = tonumber (tmp[1]),
            module = tmp[2],
            function_name = tmp[3],
            source_info = tmp[4],
            level = tmp[5],
            time_p = tonumber (tmp[6]),
            time_s = tonumber (tmp[7]),
            cpi_ratio = tonumber (tmp[8]),
         }
      end
      i = i + 1
   end
   file:close ()
   return results
end





--- Loads a lprof csv file containing a loop based profiling into a table
-- @param filename Name of the lprof file to load
-- @return A table with the following structure:
--      tab = {
--          {loop_id=<int>,            -- Loop identifier
--           function_name=<string>,   -- Name of the function the loop belongs to
--           source_info=<string>,     -- Source location of the loop
--           level=<string>,           -- Level of the loop (Single, Innermost, InBetween, Outermost)
--           time_p=<number>,          -- Time spent by the loop (in percentage of the total execution time)
--           time_s_min=<number>,      -- Minimal time spent by the loop (in seconds)
--           time_s_max=<number>,      -- Maximal time spent by the loop (in seconds)
--           time_s_avg=<number>,      -- Average time spent by the loop (in seconds)
--           module=<string>},         -- Module the loop belongs to (for Fortran codes)
--          {...},
--      }
--         or nil if there is a problem
function lprof:load_lprof_loop_summary_CSV_file (filename)
   if type (filename) ~= "string" then
      -- Invalid parameter
      return nil
   end
   if fs.exists (filename) == false then
      -- File does not exist
      return nil
   end

   local file = io.open (filename, "r")
   if file == nil then
      -- File can not be openned
      return nil
   end

   local results = {}
   local i = 0
   for line in file:lines() do
      if i == 0 then
         -- First line contains only the legend
      else
         local tmp = String:split (line, ";")
         results[i] = {
            loop_id = tonumber (tmp[1]),
            module = tmp[2],
            function_name = tmp[3],
            source_info = tmp[4],
            level = tmp[5],
            time_p = tonumber (tmp[6]),
            time_s_min = tmp[7],
            time_s_avg = tmp[9],
            time_s_max = tmp[8],
            tid_time_s_min = tmp[7],
            tid_time_s_avg = tmp[9],
            tid_time_s_max = tmp[8],
         }

         results[i].time_s_min = string.gsub (results[i].time_s_min, "%s.*", "")
         results[i].time_s_max = string.gsub (results[i].time_s_max, "%s.*", "")
         results[i].time_s_avg = string.gsub (results[i].time_s_avg, "%s.*", "")
         results[i].time_s_min = tonumber (results[i].time_s_min)
         results[i].time_s_max = tonumber (results[i].time_s_max)
         results[i].time_s_avg = tonumber (results[i].time_s_avg)

         results[i].tid_time_s_min = string.gsub (results[i].tid_time_s_min, ".*%[", "")
         results[i].tid_time_s_max = string.gsub (results[i].tid_time_s_max, ".*%[", "")
         results[i].tid_time_s_avg = string.gsub (results[i].tid_time_s_avg, ".*%[", "")
         results[i].tid_time_s_min = string.gsub (results[i].tid_time_s_min, "%].*", "")
         results[i].tid_time_s_max = string.gsub (results[i].tid_time_s_max, "%].*", "")
         results[i].tid_time_s_avg = string.gsub (results[i].tid_time_s_avg, "%].*", "")
         results[i].tid_time_s_min = tonumber (results[i].tid_time_s_min)
         results[i].tid_time_s_max = tonumber (results[i].tid_time_s_max)
         results[i].tid_time_s_avg = tonumber (results[i].tid_time_s_avg)

      end
      i = i + 1
   end
   file:close ()
   return results
end




--- Loads a lprof csv file containing a loop based profiling into a table
-- @param filename Name of the lprof file to load
-- @return A table with the following structure:
--      tab = {
--          {loop_id=<int>,            -- Loop identifier
--           function_name=<string>,   -- Name of the function the loop belongs to
--           source_info=<string>,     -- Source location of the loop
--           level=<string>,           -- Level of the loop (Single, Innermost, InBetween, Outermost)
--           time_p=<number>,          -- Time spent by the loop (in percentage of the total execution time)
--           time_s_min=<number>,      -- Minimal time spent by the loop (in seconds)
--           time_s_max=<number>,      -- Maximal time spent by the loop (in seconds)
--           time_s_avg=<number>,      -- Average time spent by the loop (in seconds)
--           module=<string>},         -- Module the loop belongs to (for Fortran codes)
--          {...},
--      }
--         or nil if there is a problem
function lprof:load_lprof_functions_summary_CSV_file (filename)
   if type (filename) ~= "string" then
      -- Invalid parameter
      return nil
   end
   if fs.exists (filename) == false then
      -- File does not exist
      return nil
   end

   local file = io.open (filename, "r")
   if file == nil then
      -- File can not be openned
      return nil
   end

   local results = {}
   local i = 0
   for line in file:lines() do
      if i == 0 then
         -- First line contains only the legend
      else
         local tmp = String:split (line, ";")
         results[i] = {
            function_name = tmp[1],
            module = tmp[2],
            source_info = tmp[3],
            time_p = tonumber (tmp[4]),
            time_s_min = tmp[5],
            time_s_avg = tmp[7],
            time_s_max = tmp[6],
            tid_time_s_min = tmp[5],
            tid_time_s_avg = tmp[7],
            tid_time_s_max = tmp[6],
         }
         results[i].time_s_min = string.gsub (results[i].time_s_min, "%s.*", "")
         results[i].time_s_max = string.gsub (results[i].time_s_max, "%s.*", "")
         results[i].time_s_avg = string.gsub (results[i].time_s_avg, "%s.*", "")
         results[i].time_s_min = tonumber (results[i].time_s_min)
         results[i].time_s_max = tonumber (results[i].time_s_max)
         results[i].time_s_avg = tonumber (results[i].time_s_avg)

         results[i].tid_time_s_min = string.gsub (results[i].tid_time_s_min, ".*%[", "")
         results[i].tid_time_s_max = string.gsub (results[i].tid_time_s_max, ".*%[", "")
         results[i].tid_time_s_avg = string.gsub (results[i].tid_time_s_avg, ".*%[", "")
         results[i].tid_time_s_min = string.gsub (results[i].tid_time_s_min, "%].*", "")
         results[i].tid_time_s_max = string.gsub (results[i].tid_time_s_max, "%].*", "")
         results[i].tid_time_s_avg = string.gsub (results[i].tid_time_s_avg, "%].*", "")
         results[i].tid_time_s_min = tonumber (results[i].tid_time_s_min)
         results[i].tid_time_s_max = tonumber (results[i].tid_time_s_max)
         results[i].tid_time_s_avg = tonumber (results[i].tid_time_s_avg)
      end
      i = i + 1
   end
   file:close ()
   return results
end










--- Loads a lprof csv file containing a function based profiling into a table
-- @param filename Name of the lprof file to load
-- @return A table with the following structure:
--      tab = {
--          {
--           function_name=<string>,   -- Name of the function
--           source_info=<string>,     -- Source location of the function
--           time_p=<number>,          -- Time spent by the function (in percentage of the total execution time)
--           time_s=<number>,          -- Time spent by the function (in seconds)
--           cpi_ratio=<number>,       -- Cycle Per Instruction ratio
--           module=<string>},         -- Module the function belongs to (for Fortran codes)
--          {...},
--      }
--         or nil if there is a problem
function lprof:load_lprof_functions_CSV_file (filename)
   if type (filename) ~= "string" then
      -- Invalid parameter
      return nil
   end
   if fs.exists (filename) == false then
      -- File does not exist
      return nil
   end

   local file = io.open (filename, "r")
   if file == nil then
      -- File can not be openned
      return nil
   end

   local results = {}
   local i = 0
   for line in file:lines() do
      if i == 0 then
         -- First line contains only the legend
      else
         local tmp = String:split (line, ";")
         results[i] = {
            function_name = tmp[1],
            module = tmp[2],
            source_info = tmp[3],
            time_p = tonumber (tmp[4]),
            time_s = tonumber (tmp[5]),
            cpi_ratio = tonumber (tmp[6]),
         }
      end
      i = i + 1
   end
   file:close ()
   return results
end


function lprof:load_categorization_CSV_file (filename)
   if type (filename) ~= "string" then
      -- Invalid parameter
      return nil
   end
   if fs.exists (filename) == false then
      -- File does not exist
      return nil
   end

   local file = io.open (filename, "r")
   if file == nil then
      -- File can not be openned
      return nil
   end

   return Utils:load_CSV (filename, ';')
end


function lprof:load_detailed_categorization (input_dir, prefix)
   if prefix == nil then
      prefix = ""
   end

   local exp_name               = nil
   local nodes                  = nil
   local node_to_idx            = {}
   local idx_to_node            = {}

   local processes              = {}
   local pid_to_idx             = {}
   local idx_to_pid             = {}

   local threads                = {}
   local tid_to_idx             = {}
   local idx_to_tid             = {}

   -- Get the node summary file
   -- --------------------------------------------------------
   -- Looking for the file lprof_cat_per_node_<...>.csv
   local input_dir_content = fs.readdir (input_dir)
   for _, f in pairs (input_dir_content) do
      if string.match (f.name, "^"..prefix.."lprof_cat_per_node_.*%.csv$") ~= nil then
         exp_name = string.gsub (f.name, "^"..prefix.."lprof_cat_per_node_", "")
         exp_name = string.gsub (exp_name, "%.csv$", "")
         nodes    = Utils:load_CSV (input_dir.."/"..f.name, ';')

         for i, line in ipairs (nodes) do
            node_to_idx[line["NODE"]] = i
            idx_to_node[i]            = line["NODE"]
         end
      end
   end


   -- Get files for each node
   -- --------------------------------------------------------
   -- Looking for files lprof_cat_per_process_from_node_<...>.csv
   for idx, node in ipairs (idx_to_node) do
      local fname = input_dir.."/"..prefix.."lprof_cat_per_process_from_node_"..node.."_xp_"..exp_name..".csv"
      if fs.exists (fname) == true then
         processes[idx] = Utils:load_CSV (fname, ';')
         pid_to_idx[idx] = {}
         idx_to_pid[idx] = {}
         for i, process in ipairs (processes[idx]) do
            pid_to_idx[idx][process["PROCESS ID"]] = i
            idx_to_pid[idx][i]                     = process["PROCESS ID"]
         end
      end
   end


   -- Get files for each process
   -- --------------------------------------------------------
   -- Looking for files lprof_cat_per_thread_from_process_<...>.csv
   for nidx, node in ipairs (idx_to_node) do
      threads[nidx]    = {}
      tid_to_idx[nidx] = {}
      idx_to_tid[nidx] = {}

      if idx_to_pid[nidx] ~= nil then
         for pidx, process in ipairs (idx_to_pid[nidx]) do
            local fname = input_dir.."/"..prefix.."lprof_cat_per_thread_from_process_"..process.."_from_node_"..node.."_xp_"..exp_name..".csv"
            if fs.exists (fname) == true then
               threads[nidx][pidx] = Utils:load_CSV (fname, ';')
               tid_to_idx[nidx][pidx] = {}
               idx_to_tid[nidx][pidx] = {}
               for i, thread in ipairs (threads[nidx][pidx]) do
                  tid_to_idx[nidx][pidx][thread["THREAD ID"]] = i
                  idx_to_tid[nidx][pidx][i]                   = thread["THREAD ID"]
               end
            end
         end
      end
   end

   return {nodes = nodes, processes = processes, threads = threads,
           nid_to_idx = node_to_idx, idx_to_nid = idx_to_node,
           pid_to_idx = pid_to_idx , idx_to_pid = idx_to_pid,
           tid_to_idx = tid_to_idx , idx_to_tid = idx_to_tid}
end

-- API function to return a valid options table to pass to lprof:collect()
-- Typical use of this function:
--  local custom_options = lprof:get_default_collect_options()
--  custom_options.field = custom_value
-- @return options table
function lprof:get_default_collect_options()
   return {
      -- Sampling engine
      -- Possible values: "SX2", "SX3", "TX"
      sampling_engine = "SX3", -- default, ptrace-based
      -- "SX2": alternative, inherit-based. CLI: --use-alternative-engine (hidden)
      -- "TX": OS timers. CLI: --use-OS-timers

      -- Custom counters list
      hwc_list = "", -- CLI: -hwc/--hardware-counters

      -- CPU set (SX2/SX3 engines only)
      cpu_list = "", -- CLI: -cpu/--cpu-list

      -- MPI target (SX2 engine only)
      mpi_target = "", -- CLI: -mt/--mpi-target

      -- Sampler threads (multithreaded sampling)
      sampler_threads = 1, -- CLI: -st/--sampler-threads
      -- 0: corresponds to st=nprocs

      -- Sampling period (inverse of sampling rate/frequency)
      sampling_period = lprof.DEFAULT_SAMPLING_PERIOD, -- CLI: --sampling-rate
      -- lprof.XSMALL_SAMPLING_PERIOD: sampling-rate = "highest" (SX2/3)
      -- lprof.SMALL_SAMPLING_PERIOD : sampling-rate = "high"    (SX2/3)
      -- lprof.LARGE_SAMPLING_PERIOD : sampling_rate = "low"     (SX2/3)
      -- lprof.TIMER_XSMALL_SAMPLING_PERIOD : sampling-rate = "highest" (TX)
      -- lprof.TIMER_SMALL_SAMPLING_PERIOD  : sampling-rate = "high"    (TX)
      -- lprof.TIMER_LARGE_SAMPLING_PERIOD  : sampling-rate = "low"     (TX)
      -- lprof.TIMER_DEFAULT_SAMPLING_PERIOD: TX default

      -- Syncronous tracer (undocumented, SX2 only)
      ["sync tracer"] = false, -- CLI: 

      -- Finalization signal
      finalize_signal = -1, -- CLI: -fs/--finalize-signal

      -- Library structure analysis (disassembling to get loops)
      library_debug_info = "", -- CLI: -ldi/--library-debug-info

      -- User guided: start after X seconds or pause/resume counters at CTRL+Z
      -- CLI: -ug/--user-guided
      user_guided = -1,
      -- 42: ug=42
      -- 0 : ug=on (keyboard driven)

      -- Kernel detection bypassing
      kernel_detection = true, -- CLI: --detect-kernel

      -- Sampling profile (ready-to-use counters lists)
      sampling_profile = "SX3", -- CLI: -p/--profile

      -- Backtrace mode (method to extract callchains)
      -- CLI: -btm/--backtrace-mode
      backtrace_mode = lprof.BACKTRACE_MODE_CALL,
      -- lprof.BACKTRACE_MODE_STACK: libunwind. CLI: btm=stack
      -- lprof.BACKTRACE_MODE_BRANCH: LBR. CLI: btm=branch
      -- lprof.BACKTRACE_MODE_OFF: no callchain collection. CLI: btm=off

      -- Disabling of check for already existing experiment directory
      check_directory = true, -- CLI: -cd/--check-directory

      -- Size limitation for memory buffers and temporary files
      max_buf_MB   = 1024, -- CLI: --maximum-buffer-megabytes
      files_buf_MB = 20  , -- CLI: --tmpfiles-buffer-megabytes
      max_files_MB = 1024, -- CLI: --maximum-tmpfiles-megabytes

      -- Parallel runs
      mpi_cmd       = nil, -- CLI: --mc/--mpi-command
      batch_script  = nil, -- CLI: --bs/--batch-script
      batch_command = nil, -- CLI: --bc/--batch-command
   }
end

local function check_collect_options (options)
   if (options.max_files_MB < options.files_buf_MB) then
      Message:error (
         "maximum-tmpfiles-megabytes cannot be lower than tmpfiles-buffer-megabytes.\n"..
         string.format ("Setting to same value as tmpfiles-buffer-megabytes (%u MB)", options.files_buf_MB)
      )
      options.max_files_MB = options.files_buf_MB
   end
end

-- API function to profile an application with lprof
-- Produces an experiment directory that can be used as input by lprof:post_process
-- @param application_cmd application command (string), e.g "toto.exe size=10000 input=graph.in"
-- @param experiment_path path to experiment directory to create (string)
-- @param maqao_path path to the MAQAO executable on parallel nodes (like args ["_maqao_"])
-- @param options options table (if nil, default one will be used)
-- @see get_default_options() helper function
function lprof:do_collect (application_cmd, experiment_path, maqao_path, options)
   if (application_cmd == nil) then
      Message:critical ("Cannot profile: missing application command line")
   end
   if (experiment_path == nil) then
      Message:critical ("Cannot profile: missing path to experiment directory")
   end

   if (options == nil) then
      options = lprof:get_default_collect_options()
   else
      check_collect_options (options)
   end
   options.bin             = application_cmd
   options.experiment_path = experiment_path
   options.maqao_path      = maqao_path

   local proj = project.new ("lprof")
   if (options ["disable-debug"] == true) then
      proj:set_option (Consts.PARAM_MODULE_DWARF, Consts.PARAM_DWARF_DISABLE_DEBUG, true);
   end
   if (options ["lcore-flow-all"] == true) then
      proj:set_option (Consts.PARAM_MODULE_LCORE, Consts.PARAM_LCORE_FLOW_ANALYZE_ALL_SCNS, true);
   end

   lprof.collect.collect (options, proj)
end

function lprof:get_default_post_process_options (display_mode)
   local opts = {
      -- Show raw values for counters (number of samples instead of events)
      show_samples_value = false, -- CLI: -ssv/--show-samples-value    

      -- Callchain analysis/display scope
      callchain_filter = lprof.CALLCHAIN_FILTER_UP_TO_BINARY, -- CLI: -cc/--callchain

      -- Consider specified libraries as extra categories
      libraries_extra_categories = "off", -- CLI: -lec/--libraries-extra-categories
   }

   if (display_mode == "html") then
      -- CLI: of=html
      opts.display_type = lprof.INFO_TYPE_FUNC
      opts.output_format = "html" -- CLI: -of/--output-format
   elseif (display_mode == "txt/functions") then
      -- CLI: -df (of=txt)
      opts.display_type = lprof.INFO_TYPE_FUNC
      opts.output_format = "txt"
   elseif (display_mode == "txt/loops") then
      -- CLI: -dl (of=txt)
      opts.display_type = lprof.INFO_TYPE_LOOP
      opts.output_format = "txt"
   end

   return opts
end

-- API function to post-process data collected by lprof:collect in experiment directory
-- @param options options table (if nil, default one will be used)
-- @return results table (self explanatory)
function lprof:post_process (experiment_path, display_mode, options)
   if (options == nil) then
      options = lprof:get_default_post_process_options (display_mode)
   end
   options.experiment_path = experiment_path
   local context = lprof.display.prepare_sampling_display (options)
   return context.post_refactoring_table
end

function lprof:collect_and_post_process (application_cmd, experiment_path, maqao_path, options)
   Message:info ("Native single step is WIP")
   lprof:do_collect (options)
   return lprof:post_process (experiment_path, options)
end
