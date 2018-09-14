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

-- This file defines the lprof:lprof_launch() which is the module entry point

module ("lprof", package.seeall)

require "fs"
require "lfs"
require "lprof.help"
require "lprof.consts"
require "lprof.strings"
require "lprof.collect"
require "lprof.display"
require "lprof.instru_mode.instrument"
require "lprof.instru_mode.display"
require "lprof.utils"
require "lprof.api"
require "lprof_c"


local function check_and_set_cmd (args, options)
   -- Get executable name
   local bin = lprof.utils.get_opt (args, "binary", "bin")
   if (bin == nil) then Message:critical ("Binary is missing!") end

   local parsed_binary_cmd = lprof:split (bin, " ")
   if (fs.dirname (parsed_binary_cmd[1]) == ".") then
      bin = "./"..bin
   end

   -- Try to open executable
   local f = io.open (bin, "r")
   if (f ~= nil) then
      io.close (f)
   else
      Message:critical (bin.." does not exist! Exiting...");
   end

   -- Appends optional parameters to executable
   local params = lprof.utils.get_opt (args, "_bin_params")
   if (params ~= nil and params ~= "") then
      bin = bin.." "..params
   end

   -- Save command to options
   options.bin = bin
end

local function check_exp_path (options)
   local xp = options.experiment_path
   if (type (xp) ~= "string" or xp == "") then
      options._default_xp = true
      options.experiment_path = "maqao_lprof_"..os.date ("%Y-%m-%d-%H-%M-%S")
   else
      local basename = fs.basename (xp)
      if (basename == "." or basename == "..") then
         Message:critical ("Invalid experimentation path!\n")
      end
   end
end

-- lprof entry point
function lprof:lprof_launch (args, proj)

   if (args["collect-hwc"]) then
      if (fs.exists (args["abs-xp"]) == true) then
         local opt_path = args["abs-xp"].."/options.lua"
         if (fs.exists (opt_path) == false) then
            Message:critical ("Cannot find "..opt_path)
         end
         dofile (opt_path)
         options.experiment_path = args["abs-xp"]
         lprof.collect.collect_hwc (options, proj, true) -- barrier needed
         os.exit(0)
      else
         local master_host = args.host or "current node"
         local slave_host = io.popen ("hostname"):read("*l") or "current parallel node"
         Message:critical (string.format ("Cannot find %s from %s.\n"..
                                          "Use absolute path to a partition visible to both %s and %s",
                                          args["abs-xp"], slave_host, master_host, slave_host))
      end
   end

   local function is_opt_set (alias1, alias2)
      return lprof.utils.is_opt_set (args, alias1, alias2)
   end
   local function get_opt (alias1, alias2)
      return lprof.utils.get_opt (args, alias1, alias2)
   end

   -- Initialize help and print help page or version
   local help = lprof:init_help()
   if (is_opt_set ("version", "v")) then help:print_version (true, args.verbose) end
   if (is_opt_set ("help"   , "h")) then help:print_help    (true, args.verbose) end

   -- Create table to save lprof options
   local options = {}
   options.verbose = (args.verbose ~= nil)
   options.experiment_path = get_opt ("experiment-path", "xp")

   -- Debug mode
   local dbg = get_opt ("debug", "dbg")
   if (dbg == "1") then
      Debug:enable (true)
   else
      if (type (dbg) == "string" and dbg ~= "1" and dbg ~= "0") then
         Message:critical ("Invalid debug level: "..dbg)
      end
      Debug:enable (false)
   end

   -- Available modes:
   --  * "collect" (sampling)
   --  * "instrument" (probes) [TODO: move to another module]
   --  * "display" (sampling)
   --  * "instru display" [TODO: move to another module]
   local mode
   if (is_opt_set ("instrumentation-profile", "ip")) then
      -- PROBES
      options.profile = get_opt ("instrumentation-profile", "ip")
      if (is_opt_set ("binary", "bin")) then
         mode = "instrument"
      else
         mode = "instru display"
      end
   else
      -- SAMPLING
      if (is_opt_set ("binary", "bin")) then
         mode = "collect"
      elseif (is_opt_set ("display-functions", "df") or
              is_opt_set ("display-loops", "dl") or
              get_opt ("output-format", "of") == "html") then
         mode = "display"
      else
         Message:critical ("[MAQAO] To display sampling results, use df, dl or of=html options.")
      end
   end

   if (mode == "collect") then
      -- Collection step (sampling), using hardware counters or system timer
      lprof.collect.set_opts_from_cmdline (args, options)
      check_exp_path (options)
      check_and_set_cmd (args, options)
      lprof.collect.collect (options, proj)

   elseif (mode == "instrument") then
      -- Instrumentation step (probes)
      lprof.instru_mode.instrument.check_and_set_opts (args, options)
      check_and_set_cmd (args, options)
      check_exp_path (options)
      lprof.instru_mode.instrument.instrument (options)

   elseif (mode == "display") then
      -- Display step (sampling)
      lprof.display.set_opts_from_cmdline (args, options)
      local context = lprof.display.prepare_sampling_display (options)
      lprof.display.print_sampling (context)

   elseif (mode == "instru display") then
      -- Display step (probes)
      lprof.instru_mode.display.check_and_set_opts (args, options)
      lprof.instru_mode.display.display ("instrument", 0, options)
   end
end
