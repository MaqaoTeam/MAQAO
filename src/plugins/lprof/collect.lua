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

-- This file defines check_and_set_opts() and collect(), used for the collect step (sampling)

module ("lprof.collect", package.seeall)

require "lprof.consts"
require "lprof.strings"
require "lprof.utils" -- get_opt
require "lprof.display" -- print_display_commands_array
require "lprof_c"

local function get_MB_limit (args, name, default_val)
   local val = lprof.utils.get_opt (args, name)
   
   local val_num = tonumber (val)
   if (val_num ~= nil and val_num >= 1) then
      if (math.floor (val_num) < val_num) then
         Message:warn (string.format ("[MAQAO] Decimal part ignored for %s", name))
      end
      return math.floor (val_num)
   end

   if (val ~= nil) then
      Message:error (string.format ("[MAQAO] Expecting positive integer value for %s", name))
      Message:error (string.format ("[MAQAO] Using default value: %u MB", default_val))
   end

   return default_val
end

-- Find the "time event" depending of the arch and uarch
local function select_time_meter_event ()

   local time_meter_event = nil;

   local uarch, arch = get_host_uarch();

   if ( time_meter_event == nil ) then
      Message:critical ("[MAQAO] Unknown CPU architecture!");
      os.exit();
   end

   return time_meter_event;
end

-- Check if the given file/list contains the time meter event
-- Return the sampling period of the time meter event
local function check_hwc_option (options)

   local sampling_period = nil;
   local time_meter_event = select_time_meter_event();

   -- The hwc option is a file or a list ? ( hwc=./my_file.lprof or hwc=EVENT@1000,.. )
   if ( string.find(fs.basename(options["hwc_list"]),"%.") ) then

      -- It's a file
      local file = io.open(options["hwc_list"],"r+");
      local lines = {};
      if ( file ~= nil ) then
         -- Copy the file line by line
         for line in io.lines(options["hwc_list"]) do
            lines[#lines +1] = line;
         end
      else
         Message:critical ("[MAQAO] Impossible to open "..options["hwc_list"]);
         os.exit();
      end

      first_event_line = lprof:split(lines[2],",");
      first_event_name = first_event_line[1];
      first_event_sampling_period = first_event_line[2];

      -- Search if the time event is in first position
      if string.find (first_event_name,time_meter_event) then
         --Get the sampling period value
         sampling_period = first_event_sampling_period;
      else
         -- The time meter event is not present so we need to modify the profile
         -- to add the time meter event value with default sampling period.

         -- Set the file pointer at the beginning of the file.
         file:seek("set",0);
         -- Set the new number of events
         local old_nb_event = tonumber(lines[1]);
         if (old_nb_event == nil) then
            Message:error ("[MAQAO] Bad file format : "..options["hwc_list"]);
            Message:error ("[MAQAO] Missing number of events at the first line." );
            os.exit();
         end
         new_nb_events = tonumber(lines[1]) + 1;
         file:write(new_nb_events, "\n");

         -- Add the time event as the first event
         file:write(time_meter_event..","..lprof.DEFAULT_SAMPLING_PERIOD, "\n");
         -- Then Copy the previous events
         for idx = 2,#lines do
            file:write(lines[idx], "\n");
         end
         sampling_period = lprof.DEFAULT_SAMPLING_PERIOD;
         Message:warn ("[MAQAO] HWC profile file "..options["hwc_list"].." has been modified !");
         Message:warn ("[MAQAO] "..time_meter_event.." has been added. This event need to be put in first.");
      end
      file:close();
   else
      -- It's a list
      eventsList = lprof:split (options["hwc_list"],",");
      if string.find (eventsList[1],time_meter_event) then
         sampling_period_info = lprof:split (eventsList[1],"@");
         sampling_period = sampling_period_info[2];
      else
         options["hwc_list"] = time_meter_event.."@"..lprof.DEFAULT_SAMPLING_PERIOD..","..options["hwc_list"];
         sampling_period = lprof.DEFAULT_SAMPLING_PERIOD;
      end
   end

   if ( sampling_period == nil) then
      Message:critical ("[MAQAO] : Issue with hwc= option");
      os.exit();
   end

   return sampling_period;
end

local function get_sampling_period (sampling_rate, timers)
   if (not timers) then
      if (sampling_rate == "highest") then return lprof.XSMALL_SAMPLING_PERIOD end
      if (sampling_rate == "high"   ) then return lprof.SMALL_SAMPLING_PERIOD  end
      if (sampling_rate == "low"    ) then return lprof.LARGE_SAMPLING_PERIOD  end
      return lprof.DEFAULT_SAMPLING_PERIOD
   else
      if (sampling_rate == "highest") then return lprof.TIMER_XSMALL_SAMPLING_PERIOD end
      if (sampling_rate == "high"   ) then return lprof.TIMER_SMALL_SAMPLING_PERIOD  end
      if (sampling_rate == "low"    ) then return lprof.TIMER_LARGE_SAMPLING_PERIOD  end
      return lprof.TIMER_DEFAULT_SAMPLING_PERIOD
   end
end

local function check_and_set_hwc_list (options)
   local smpl_prof = options.sampling_profile
   
   if (smpl_prof == "dtlb_sandy") then
      options.hwc_list = "CPU_CLK_UNHALTED:REF_P@500003,DTLB_LOAD_MISSES:MISS_CAUSES_A_WALK@500003,DTLB_STORE_MISSES:MISS_CAUSES_A_WALK@500003,DTLB_STORE_MISSES:STLB_HIT@500003,DTLB_LOAD_MISSES:STLB_HIT@500003,MEM_UOPS_RETIRED:STLB_MISS_LOADS@500003,MEM_UOPS_RETIRED:STLB_MISS_STORES@500003"
   elseif (smpl_prof == "memory_sandy") then
      options.hwc_list = "CPU_CLK_UNHALTED:REF_P@500003,L1D:REPLACEMENT@500003,L2_LINES_IN:ANY@500003,L3_LAT_CACHE:REFERENCE@500003,L3_LAT_CACHE:MISS@500003,L2_LINES_OUT:DIRTY_ANY@500003,L2_RQSTS:ALL_DEMAND_DATA_RD@500003,L2_RQSTS:ALL_PF@500003"
   elseif (smpl_prof == "branch_sandy") then
      options.hwc_list = "CPU_CLK_UNHALTED:REF_P@500003,BR_MISP_RETIRED:ALL_BRANCHES@20003,BR_INST_RETIRED:ALL_BRANCHES@20003,BR_INST_RETIRED:CONDITIONAL@20003,BR_MISP_RETIRED:NOT_TAKEN@20003"
   elseif (smpl_prof == "compute_sandy") then
      options.hwc_list = "CPU_CLK_UNHALTED:REF_P@500003,ARITH:FPU_DIV@500003,ARITH:FPU_DIV_ACTIVE@500003,FP_COMP_OPS_EXE:SSE_FP_PACKED_DOUBLE@500003,FP_COMP_OPS_EXE:SSE_FP_SCALAR_SINGLE@500003,FP_COMP_OPS_EXE:X87@500003,SIMD_FP_256:PACKED_SINGLE@500003,SIMD_FP_256:PACKED_DOUBLE@500003"
   elseif (smpl_prof == "dtlb_haswell") then
      options.hwc_list = "CPU_CLK_THREAD_UNHALTED:REF_XCLK@500003,DTLB_LOAD_MISSES:MISS_CAUSES_A_WALK@500003,DTLB_STORE_MISSES:MISS_CAUSES_A_WALK@500003,DTLB_STORE_MISSES:STLB_HIT@500003,DTLB_LOAD_MISSES:STLB_HIT@500003,MEM_UOPS_RETIRED:STLB_MISS_LOADS@500003,MEM_UOPS_RETIRED:STLB_MISS_STORES@500003"
   elseif (smpl_prof == "memory_haswell") then
      options.hwc_list = "CPU_CLK_THREAD_UNHALTED:REF_XCLK@500003,L1D:REPLACEMENT@500003,L2_LINES_IN:ANY@500003,LONGEST_LAT_CACHE:REFERENCE@500003,LONGEST_LAT_CACHE:MISS@500003,L2_LINES_OUT:DEMAND_DIRTY@500003,L2_RQSTS:ALL_DEMAND_DATA_RD@500003,L2_RQSTS:ALL_PF@500003"
   elseif (smpl_prof == "memory2_haswell") then
      options.hwc_list = "CPU_CLK_THREAD_UNHALTED:REF_XCLK@500003,MEM_LOAD_UOPS_RETIRED:L1_HIT@500003,MEM_LOAD_UOPS_RETIRED:L2_HIT@500003,MEM_LOAD_UOPS_RETIRED:L3_HIT@500003,MEM_LOAD_UOPS_RETIRED:L1_MISS@500003,MEM_LOAD_UOPS_RETIRED:L2_MISS@500003,MEM_LOAD_UOPS_RETIRED:L3_MISS@500003,MEM_LOAD_UOPS_RETIRED:HIT_LFB@500003"
   elseif (smpl_prof == "cachel1l2_haswell") then
      options.hwc_list = "CPU_CLK_THREAD_UNHALTED:REF_XCLK@500003,MEM_LOAD_UOPS_RETIRED:L1_HIT@500003,MEM_LOAD_UOPS_RETIRED:L2_HIT@500003,MEM_LOAD_UOPS_RETIRED:L1_MISS@500003,MEM_LOAD_UOPS_RETIRED:L2_MISS@500003,MEM_LOAD_UOPS_RETIRED:HIT_LFB@500003,MEM_UOPS_RETIRED:STLB_MISS_LOADS@500003,MEM_UOPS_RETIRED:STLB_MISS_STORES@500003"
   elseif (smpl_prof == "cachel1l3_haswell") then
      options.hwc_list = "CPU_CLK_THREAD_UNHALTED:REF_XCLK@500003,MEM_LOAD_UOPS_RETIRED:L1_HIT@500003,MEM_LOAD_UOPS_RETIRED:L3_HIT@500003,MEM_LOAD_UOPS_RETIRED:L1_MISS@500003,MEM_LOAD_UOPS_RETIRED:L3_MISS@500003,MEM_LOAD_UOPS_RETIRED:HIT_LFB@500003,DTLB_LOAD_MISSES:MISS_CAUSES_A_WALK@500003,DTLB_STORE_MISSES:MISS_CAUSES_A_WALK@500003"
   elseif (smpl_prof == "cachel2l3_haswell") then
      options.hwc_list = "CPU_CLK_THREAD_UNHALTED:REF_XCLK@500003,MEM_LOAD_UOPS_RETIRED:L2_HIT@500003,MEM_LOAD_UOPS_RETIRED:L3_HIT@500003,MEM_LOAD_UOPS_RETIRED:L2_MISS@500003,MEM_LOAD_UOPS_RETIRED:L3_MISS@500003,MEM_LOAD_UOPS_RETIRED:HIT_LFB@500003,DTLB_LOAD_MISSES:STLB_HIT@500003"
   end

   if (smpl_prof ~= "") then
      Message:info ("[MAQAO] PROFILE "..smpl_prof.." IS ACTIVATED\n");
      options.sampling_period = 500003;
      io.flush();
   end

   if (options.hwc_list ~= "" and smpl_prof == "") then
      options.sampling_period = check_hwc_option (options);
   end
end

function set_opts_from_cmdline (args, options)
   local function is_opt_set (alias1, alias2)
      return lprof.utils.is_opt_set (args, alias1, alias2)
   end
   local function get_opt (alias1, alias2)
      return lprof.utils.get_opt (args, alias1, alias2)
   end
   local function is_feature_set (alias1, alias2, enabled_by_default)
      return lprof.utils.is_feature_set (args, alias1, alias2, enabled_by_default)
   end

   -- Sampling engine
   local target = get_opt ("target", "t") -- DEPRECATED
   if (target == "SX2" or is_opt_set ("use-alternative-engine", "ae")) then
      options.sampling_engine = "SX2"
   elseif (target == "TX" or is_opt_set ("use-OS-timers")) then
      options.sampling_engine = "TX"
   else
      options.sampling_engine = "SX3" -- default
   end

   -- Custom counters list
   options.hwc_list = get_opt ("hardware-counters", "hwc") or ""

   -- CPU set (SX2/SX3 engines only)
   options.cpu_list = get_opt ("cpu-list", "cpu") or ""

   -- MPI target (SX2 engine only)
   options.mpi_target = get_opt ("mpi-target", "mt") or ""

   -- Number of sampler threads (SX2 and SX3 only)
   local nb_smpl_threads = get_opt ("sampler-threads", "st")
   if (nb_smpl_threads == "nprocs") then
      options.sampler_threads = 0
   elseif (tonumber (nb_smpl_threads) ~= nil) then
      options.sampler_threads = tonumber (nb_smpl_threads)
   else
      options.sampler_threads = 1 -- default
   end

   -- Sampling period
   local sampling_rate
   if (is_opt_set ("granularity", "g")) then -- DEPRECATED
      Message:warn ("[MAQAO] granularity/g is deprecated: use sampling-rate.")
      local granularity = get_opt ("granularity", "g")
      if     (granularity == "large" ) then sampling_rate = "low"
      elseif (granularity == "small" ) then sampling_rate = "high"
      elseif (granularity == "xsmall") then sampling_rate = "highest"
      end
   elseif (is_opt_set ("sampling-rate")) then
      sampling_rate = get_opt ("sampling-rate")
   end
   options.sampling_period = get_sampling_period (sampling_rate, (options.sampling_engine == "TX"))

   -- Syncronous tracer (undocumented, SX2 only)
   options ["sync tracer"] = is_opt_set ("sync-tracer", "sy")

   -- Finalization signal
   options.finalize_signal = get_opt ("finalize-signal", "fs") or -1

   -- Library structure analysis (disassembling to get loops)
   options.library_debug_info = get_opt ("library-debug-info", "ldi") or "off"

   -- User guided: start after X seconds or pause/resume counters at CTRL+Z
   local ug = get_opt ("user-guided", "ug")
   if (ug == "on") then
      options.user_guided = 0
      Message:warn ("[MAQAO] ug=on may not correctly control an MPI application, "..
                    "especially if using mpirun (script) instead of mpiexec (executable)")
   elseif (tonumber (ug) ~= nil and tonumber (ug) > 0) then
      options.user_guided = tonumber (ug)
   else
      options.user_guided = -1
   end

   -- Kernel detection bypassing
   options.kernel_detection = is_feature_set ("detect-kernel", nil, true)

   -- Sampling profile (ready-to-use counters lists)
   options.sampling_profile = get_opt ("profile", "p") or ""

   -- Backtrace mode (method to extract callchains)
   local btm = get_opt ("backtrace-mode", "btm")
   if (btm == "call" or btm == nil) then
      -- default backtrace mode
      options.backtrace_mode = lprof.BACKTRACE_MODE_CALL
   elseif (btm == "stack") then
      options.backtrace_mode = lprof.BACKTRACE_MODE_STACK
   elseif (btm == "branch") then
      options.backtrace_mode = lprof.BACKTRACE_MODE_BRANCH
   elseif (btm == "off") then
      options.backtrace_mode = lprof.BACKTRACE_MODE_OFF
   else
      Message:error ("[MAQAO] Unknown backtrace mode "..btm)
      Message:error ("[MAQAO] Available backtrace modes : call (default), stack, branch, off")
      os.exit()
   end

   -- Disabling of check for already existing experiment directory
   if (is_opt_set ("checking-directory")) then -- DEPRECATED
      Message:warn ("[MAQAO] checking-directory is deprecated: use check-directory/cd.")
      options.check_directory = is_feature_set ("checking-directory", nil, true)
   else
      options.check_directory = is_feature_set ("check-directory", "cd", true)
   end

   -- Size limitation for memory buffers and temporary files
   options.max_buf_MB   = get_MB_limit (args, "maximum-buffer-megabytes"  , 1024)
   options.files_buf_MB = get_MB_limit (args, "tmpfiles-buffer-megabytes" ,   20)
   options.max_files_MB = get_MB_limit (args, "maximum-tmpfiles-megabytes", 1024)
   if (options.max_files_MB < options.files_buf_MB) then
      Message:error (
         "maximum-tmpfiles-megabytes cannot be lower than tmpfiles-buffer-megabytes.\n"..
         string.format ("Setting to same value as tmpfiles-buffer-megabytes (%u MB)", options.files_buf_MB)
      )
      options.max_files_MB = options.files_buf_MB
   end

   options.mpi_cmd = get_opt ("mpi-command", "mc")
   if (options.mpi_cmd ~= nil and #(options.mpi_cmd) == 0) then options.mpi_cmd = nil end

   options.batch_script = get_opt ("batch-script", "bs")
   if (options.batch_script ~= nil and #(options.batch_script) == 0) then
      options.batch_script = nil
   elseif (options.batch_script ~= nil) then
      options.batch_command = get_opt ("batch-command", "bc")
      if (options.batch_command == nil or #(options.batch_command) == 0) then
         if (string.find (string.lower (options.batch_script), "%.sbatch$") ~= nil) then -- SLURM
            options.batch_command = "sbatch"
            Message:info ("Assuming SLURM script => using sbatch to submit job.")
         elseif (string.find (string.lower (options.batch_script), "%.pbs$") ~= nil) then -- PBS
            options.batch_command = "qsub"
            Message:info ("Assuming PBS script => using qsub to submit job.")
         else
            Message:critical ("[MAQAO] batch-command/bc is required when using batch-script/bs.")
         end
      end
   end

   options.maqao_path = args ["_maqao_"]

   options ["disable-debug" ] = is_opt_set ("disable-debug")
   options ["lcore-flow-all"] = is_opt_set ("lcore-flow-all")
end

local function kernel_version_detection (options)
   kernel_info = io.popen("uname -r"):read("*l");
   --ex : 3.7.5-1 -->
   --   3 is for major info
   --   7 is for minor info
   --   5 is for revision info
   --   -1 is for sub1 info
   local major, minor, revision, remaining, sub1, sub2;

   --Standard format : 3.7.5-1 (major.minor.revision-detail)
   --Other format
   -- Format : 3.9-1 (major.minor-detail without revision info!)
   major,minor,revision,remaining,sub1,sub2 =  string.match(kernel_info,"(%d)%.*(%d*)%.*(%d*)%.*([^%-]*)%-*(%d*)(.*)");

   major_number = tonumber(major);
   minor_number = tonumber(minor);
   revision_number = tonumber(revision);
   remaining_number = tonumber(remaining);
   sub1_number = tonumber(sub1);
   sub2_number = tonumber(sub2);
   --print("MAJOR = ",major_number,
   --      "\nMINOR = ",minor_number,
   --      "\nREVISION = ",revision_number,
   --      "\nREMAINING = ",remaining_number,
   --      "\nSUB1 = ",sub1_number,
   --      "\nSUB2 = ",sub2_number);

   -- CHECK OPTIONS COMPATIBILITIES
   if ( options ["backtrace_mode"] == lprof.BACKTRACE_MODE_STACK ) then
      if (major_number > 3) then
         -- OK
      elseif (major_number == 3 and minor_number >= 7) then
         -- OK
      elseif (major_number == 2 and sub1_number >= 458) then
         -- OK
      else
         lprof:clean_abort (options["experiment_path"],
                            "[MAQAO] Option btm=stack is not compatible with your kernel version ("..kernel_info..")!")
      end
   end

   -- KERNEL IS OK IF KERNEL >= 2.6.32-279
   if (major_number >= 3) then
      return true,kernel_info;
   else
      if (major_number == 2) then
         if (minor_number < 6) then
            return false,kernel_info;
         elseif (minor_number > 6) then
            return true,kernel_info;
         elseif (revision_number < 32) then
            return false,kernel_info;
         elseif (revision_number > 32) then
            return true,kernel_info;
         elseif (remaining_number ~= nil) then
            return true,kernel_info;
         else
            if (sub1_number ~= nil) then
               if (sub1_number >= 279) then
                  return true,kernel_info;
               else
                  return false,kernel_info;
               end
            else
               return false,kernel_info;
            end
         end
      end
   end
end

local function _get_current_cpuinfo (path, pid)
   -- Read cpuinfo file
   local procs = {}
   if fs.exists ("/proc/cpuinfo") then
      local f = assert (io.popen ("cat /proc/cpuinfo"))
      local proc  = {}

      for line in f:lines() do
         if line == "" then
            table.insert (procs, proc)
            proc = {}
         else
            proc[string.gsub (line, "%s*:.*", "")] = string.gsub (line, ".*:%s*", "")
         end
      end
      f:close()
   else
      Message:error ("/proc/cpuinfo doesn't exist")
   end

   local hostname = io.popen ("hostname"):read("*l")

   local file = io.open (path.."/procinfo_"..hostname.."-"..pid..".lua", "w")
   if file == nil then return end

   local proc = get_host_proc ()
   if (proc ~= nil) then
      local uarch = proc:get_uarch ()
      local arch = uarch:get_arch ()

      file:write ("proc_name          = \""..proc:get_name ().."\"\n")
      file:write ("hostname           = \""..hostname.."\"\n")
      file:write ("architecture_code  = \""..arch:get_code ().."\"\n")
      file:write ("uarchitecture_code = \""..uarch:get_id ().."\"\n")
      file:write ("architecture       = \""..arch:get_name ().."\"\n")
      file:write ("uarchitecture      = \""..uarch:get_name ().."\"\n")
   else
      file:write ("hostname           = \""..hostname.."\"\n")
   end

   if procs[1] ~= nil then
         if procs[1]["model name"] ~= nil then
            file:write ("cpui_model_name    = \""..procs[1]["model name"].."\"\n")
         end
         if procs[1]["cache size"] ~= nil then
            file:write ("cpui_cache_size    = \""..procs[1]["cache size"].."\"\n")
         end
         if procs[1]["cpu cores"] ~= nil then
            file:write ("cpui_cpu_cores     = \""..procs[1]["cpu cores"].."\"\n")
         end
   end

   file:close ()
end

local function create_experiment_directory (options)
   local exp_path = options ["experiment_path"]

   if (fs.exists (exp_path) == false) then
      local ret, err_msg = lfs.mkdir (exp_path)
      if (ret == nil and err_msg == "Permission denied") then
         Message:critical (err_msg)
      end

   elseif (fs.exists (exp_path.."/done") == true and options ["check_directory"] == true) then
      local err_msg = "The experiment directory <"..exp_path.."> already exists\n"..
         "Delete it , change your experiment path (xp=) or disable the check of the directory (cd=off)\n"
      lprof:clean_abort (exp_path, err_msg)

   elseif (fs.exists (exp_path.."/batch_resume") == false) then
      if (xp ~= "." and xp ~= ".." and xp ~= "/") then
         os.execute (string.format ("rm -rf %s/*", exp_path))
      end
   end
end

-- TODO: write this at node level
local function write_info_file (options)
   local file = io.open (options.experiment_path.."/info.lua", "w")

   if (options.hwc_list ~= "") then
      group = "custom_events"
   else
      group = "maqao_events"
   end

   if (options.sampling_engine ~= "TX") then
      local cpuid_elts = lprof:split (project:get_cpu_frequency(), " ")
      local cpu_freq = string.match (cpuid_elts [#cpuid_elts], "%d+%.*%d+")
      local ref_freq = lprof.get_reference_frequency()
      if (cpu_freq == nil and ref_freq == 0.0) then
         Message:warn ("[MAQAO] cannot determine CPU nominal and reference frequency. "..
                       "Assuming 1 GHz for timing in seconds and CPI ratios.")
         cpu_freq = "1000".."000".."000"
         ref_freq = cpu_freq
      elseif (cpu_freq == nil and ref_freq > 0.0) then
         Message:warn ("[MAQAO] cannot determine CPU nominal frequency. "..
                       "Assuming 1 GHz for CPI ratios.")
         cpu_freq = "1000".."000".."000"
      elseif (cpu_freq ~= nil and ref_freq == 0.0) then
         cpu_freq = tonumber (cpu_freq) * 1000 * 1000 * 1000
         ref_freq = cpu_freq
      else
         cpu_freq = tonumber (cpu_freq) * 1000 * 1000 * 1000
      end
      file:write (string.format ("%s:%s:%d:%s:%s",
                                 options.sampling_engine, group,
                                 options.sampling_period, cpu_freq, ref_freq))
   else
      file:write (string.format ("%s:%s:%d:1000:1000", -- 1000 ms per second
                                 options.sampling_engine, group,
                                 options.sampling_period))

   end

   file:close()
end

-- Checks whether a directory entry (as iterated from lfs.readdir) is a directory
local function is_dir (path, e)
   return (e.name ~= "." and e.name ~= ".." and
           lfs.attributes (path.."/"..e.name, "mode") == "directory")
end

local function wait_for_all_instances (exp_path)
   local nodes = {} -- unfinished nodes
   local nb_finished_instances = 0
   local nb_total_instances = 0

   -- for each node
   for _,host_dir_entry in pairs (fs.readdir (exp_path) or {}) do
      if (is_dir (exp_path, host_dir_entry) and host_dir_entry.name ~= "html") then
         local host_dir_name = exp_path.."/"..host_dir_entry.name
         local instances = {} -- unfinished instances

         -- for each lprof instance (process forked by lprof)
         for _,inst_dir_entry in pairs (fs.readdir (host_dir_name) or {}) do
            if (is_dir (host_dir_name, inst_dir_entry) and
                tonumber (inst_dir_entry.name) ~= nil) then
               local instance_dir_name = host_dir_name.."/"..inst_dir_entry.name
               instances [inst_dir_entry.name] = instance_dir_name
               nb_total_instances = nb_total_instances + 1
            end
         end
         
         nodes [host_dir_entry.name] = instances
      end
   end

   local prev_nb_finished_instances = nil
   while true do
      if (maqao_wait_SIGINT (1) ~= 0) then
         Message:info ("Got CTRL+C. Rerun command to resume waiting.")
         os.exit (0)
      end

      for hostname, instances in pairs (nodes) do
         for pid, instance_dir_name in pairs (instances) do
            if (fs.exists (instance_dir_name.."/done") == true) then
               instances [pid] = nil
               nb_finished_instances = nb_finished_instances + 1
            end
         end
         if (next (instances) == nil) then nodes [hostname] = nil end
      end
      if (next (nodes) == nil) then break end
      
      if (prev_nb_finished_instances ~= nb_finished_instances) then
         Message:info (string.format ("%d/%d lprof instances finished",
                                      nb_finished_instances, nb_total_instances))
         io.stdout:flush()
      end

      prev_nb_finished_instances = nb_finished_instances
   end
end

-- Collect hardware events and generate metafiles
function collect_hwc (options, proj, needs_barrier)
   --print ("MPI OPTS IN COLLECT HWC", Table:new (options))

   if (options["kernel_detection"] == true) then
      local kernel_is_ok, kernel_info = kernel_version_detection (options)
      if (kernel_is_ok == false) then
         Message:critical ("Kernel "..kernel_info.." is not compatible with sampling instrumentation (too old)!");
         os.exit();
      end
   else
      Message:info("[MAQAO] Bypass kernel detection !");
   end

   local exp_path = options ["experiment_path"]

   if (options.sampling_engine == "SX2") then
      pid, hostname = lprof.launch (options["bin"], exp_path, options["sampling_period"],
                                    options["hwc_list"], options["user_guided"],
                                    options["backtrace_mode"], "sampling inherit",
                                    options["cpu_list"], options["mpi_target"],
                                    options["sampler_threads"], options.verbose,
                                    options.finalize_signal, options.max_buf_MB,
                                    options.files_buf_MB, options.max_files_MB);
   elseif (options.sampling_engine == "SX3" and options["sync tracer"]) then
      pid, hostname = lprof.launch (options["bin"], exp_path, options["sampling_period"],
                                    options["hwc_list"], options["user_guided"],
                                    options["backtrace_mode"], "sampling ptrace sync",
                                    options["cpu_list"], options["mpi_target"],
                                    options["sampler_threads"], options.verbose,
                                    options.finalize_signal, options.max_buf_MB,
                                    options.files_buf_MB, options.max_files_MB);
   elseif (options.sampling_engine == "SX3") then
      pid, hostname = lprof.launch (options["bin"], exp_path, options["sampling_period"],
                                    options["hwc_list"], options["user_guided"],
                                    options["backtrace_mode"], "sampling ptrace async",
                                    options["cpu_list"], options["mpi_target"],
                                    options["sampler_threads"], options.verbose,
                                    options.finalize_signal, options.max_buf_MB,
                                    options.files_buf_MB, options.max_files_MB);
   elseif (options.sampling_engine == "TX") then
      pid, hostname = lprof.launch (options["bin"], exp_path, options["sampling_period"],
                                    "", options["user_guided"],
                                    options["backtrace_mode"], "sampling timers",
                                    options["cpu_list"], options["mpi_target"],
                                    options["sampler_threads"], options.verbose,
                                    options.finalize_signal, options.max_buf_MB,
                                    options.files_buf_MB, options.max_files_MB);
   end

   if (pid == 0) then lprof:clean_abort (exp_path, "Sampling failed") end

   -- Case of too short process
   local walltime_file = string.format ("%s/%s/%d/walltime", exp_path, hostname, pid)
   if (fs.exists (walltime_file) == false) then
      if (not needs_barrier) then
         options._too_short = true
         return
      else
         Message:info (string.format ("Ignored (host %s, process %d): too short", hostname, pid))
      end
   end

   print(string.format ("%s %s (host %s, process %d)",
                        lprof.strings.MAQAO_TAG, lprof.strings.MSG_STOP_PROFILING, hostname, pid));

   if (fs.exists (exp_path.."/info.lua") == false) then
      write_info_file (options)
   end
   
   -- Create a file containing some data about the current processor. It uses the Lua format
   -- and can be used by other MAQAO modules
   _get_current_cpuinfo (exp_path, pid)

   -- CRITICAL SECTION, NODE LEVEL
   local exp_lock = nil
   local host_dir_name = exp_path.."/"..hostname
   local host_lock = lfs.lock_dir (host_dir_name)
   if (host_lock ~= nil) then
      options._host_lock = host_lock
      lprof.utils.copy_system_maps (proj, hostname, options)

      -- One node master will be master of all nodes
      if (needs_barrier) then
         exp_lock = lfs.lock_dir (exp_path)
      end
   end -- if lock stale

   if (fs.exists (walltime_file) == true) then
      local binary
      if (options.mpi_target ~= nil and options.mpi_target ~= "") then
         binary = options.mpi_target
      else
         local t = lprof:split (options.bin, " ")
         binary = t[1]
      end

      lprof.generate_metafile_binformat_new (exp_path, hostname, pid, binary, options.library_debug_info, proj)
   end

   -- Notify master rank that hostname/pid has finished sampling and metafile generation
   local notify_file_path = string.format ("%s/%s/%d/done", exp_path, hostname, pid)
   lprof:touch_file (notify_file_path)

   if (needs_barrier) then
      -- Waits for all other ranks having finished to continue
      -- Do this here (and not after batch or interactive parallel run command)
      -- to avoid late parallel lprof instances being killed by some launchers
      if (exp_lock ~= nil) then -- master rank (1 for all nodes nodes)
         wait_for_all_instances (exp_path)
         lprof:touch_file (exp_path.."/done")
         exp_lock:free()
      else -- slave, waits for master completion
         while fs.exists (exp_path.."/done") == false do -- O(n) with n = ranks nb
            if (maqao_wait_SIGINT (1) ~= 0) then
               Message:info ("Got CTRL+C. Rerun command to resume waiting.")
               os.exit (0)
            end
         end
      end
   end
end

local function create_lprof_batch_script (lprof_script_name, options, cmd)
   local file = io.open (lprof_script_name, "w")
   if (file == nil) then
      Message:critical ("Cannot create lprof batch script.")
   end

   local mpi_command_found = false
   for line in io.lines (options.batch_script) do
      if (string.find (line, "<mpi_command>") ~= nil or
          string.find (line, "<MPI_COMMAND>") ~= nil) then
         if (options.mpi_cmd == nil) then
            Message:critical ("Undefined MPI command in %s. Use --mpi-command\n", options.batch_script)
         end
         line = string.gsub (line, "<mpi_command>", options.mpi_cmd)
         line = string.gsub (line, "<MPI_COMMAND>", options.mpi_cmd)
         mpi_command_found = true
      end
      line = string.gsub (line, "<run_command>", cmd)
      line = string.gsub (line, "<RUN_COMMAND>", cmd)
      file:write (line.."\n")
   end

   if (mpi_command_found == false and options.mpi_cmd ~= nil) then
      Message:warn ("No <mpi_command> found in %s. mpi-command value ignored", options.batch_script)
   end

   file:close()
end

function collect (options, proj)
   check_and_set_hwc_list (options)
   create_experiment_directory (options)
   local exp_path = options.experiment_path

   local smpl_eng = options.sampling_engine
   if (smpl_eng == "SX2" or smpl_eng == "SX3" or smpl_eng == "TX") then
      if (options.mpi_cmd ~= nil or options.batch_script ~= nil) then
         -- PARALLEL RUN, LPROF REEXECUTED VIA MPIRUN INTERACTIVE COMMAND OR BATCH SCRIPT

         -- Save options to make them accessible for "recursive" lprof call
         Table:new (options):save_output (exp_path.."/options.lua", "options")

         -- Run lprof in parallel thanks to MPI
         -- Get absolute path to experiment directory (required to offload execution on other nodes)
         -- Assuming Unix/Linux ("/")
         local absolute_exp_path
         if (exp_path:sub(1,1) == "/") then -- already absolute path (starts with /)
            absolute_exp_path = exp_path
         else
            absolute_exp_path = string.format ("%s/%s", lfs.currentdir(), exp_path)
         end

         local hostname = io.popen ("hostname"):read("*l")
         local proj_opts = {}
         if (options ["disable-debug" ]) then table.insert (proj_opts, "--disable-debug" ) end
         if (options ["lcore-flow-all"]) then table.insert (proj_opts, "--lcore-flow-all") end

         local cmd
         if (hostname ~= nil) then
            cmd = string.format ("%s lprof %s -collect-hwc abs-xp=%s host=%s",
                                 options.maqao_path, table.concat (proj_opts, " "),
                                 absolute_exp_path, hostname)
         else
            cmd = string.format ("%s lprof %s -collect-hwc abs-xp=%s",
                                 options.maqao_path, table.concat (proj_opts, " "),
                                 absolute_exp_path)
         end
         if (options.batch_script == nil) then
            -- Interactive MPI command
            os.execute (options.mpi_cmd.." "..cmd)
         elseif (fs.exists (exp_path.."/batch_resume") == false) then
            -- batch script invoking MPI command
            local lprof_script_name = string.format ("%s/lprof.batch", exp_path)
            create_lprof_batch_script (lprof_script_name, options, cmd)
            os.execute (options.batch_command.." "..lprof_script_name)
            lprof:touch_file (exp_path.."/batch_resume")
         else
            Message:info ("Resuming data collection for "..options.batch_script)
         end

         -- Wait for results to be ready: on some clusters, some lag can occur between
         -- global filesystem in compute nodes and what can see login/launch nodes
         while fs.exists (exp_path.."/done") == false do
            if (maqao_wait_SIGINT (1) ~= 0) then
               Message:info ("Got CTRL+C. Rerun command to resume waiting.")
               os.exit (0)
            end
         end

         -- Cleanup: remove batch_resume, node lockfiles and directories with no sampling data
         os.remove (exp_path.."/batch_resume")
         -- for each node
         for _,host_dir_entry in pairs (fs.readdir (exp_path) or {}) do
            if (is_dir (exp_path, host_dir_entry) and host_dir_entry.name ~= "html") then
               local host_dir_name = exp_path.."/"..host_dir_entry.name
               os.remove (host_dir_name.."/lockfile.lfs")

               -- for each lprof instance (process forked by lprof)
               local to_remove = {}
               for _,inst_dir_entry in pairs (fs.readdir (host_dir_name) or {}) do
                  if (is_dir (host_dir_name, inst_dir_entry) and
                      tonumber (inst_dir_entry.name) ~= nil) then
                     local process_dir_name = host_dir_name.."/"..inst_dir_entry.name
                     if (fs.exists (process_dir_name.."/walltime") == false) then
                        table.insert (to_remove, process_dir_name)
                     end
                  end
               end -- for each lprof instance
               for _,v in ipairs (to_remove) do os.execute ("rm -rf "..v) end
            end
         end -- for each node

      else
         -- SEQUENTIAL RUN, LPROF DIRECTLY STARTS APPLICATION RUN
         collect_hwc (options, proj, false) -- false: no barrier needed

         -- Cleanup: remove node lockfile
         if (options._host_lock ~= nil) then
            options._host_lock:free()
         end

         if (options._too_short == true) then
            Message:critical ("Too short run. Rerun with a longer workload")
         end
      end
   end

   lprof:touch_file (exp_path.."/done")
   print (lprof.strings.MAQAO_TAG.." "..lprof.strings.MSG_INFO_XP..exp_path)
   if (fs.exists (exp_path.."/binary.lprof") == true) then
      lprof.display.print_display_commands_array (exp_path)
   else
      Message:critical ("Cannot find binary.lprof in experiment path. "..
                        "Try rerun with a longer workload")
   end
end
