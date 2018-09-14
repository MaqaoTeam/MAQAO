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

-- This file defines the instrument function which is entry point of run step with probes

module ("lprof.instru_mode.instrument", package.seeall)

require "lprof.utils" -- get_opt
require "lprof.instru_mode.profiles";
require "lprof.instru_mode.load";
require "lprof.instru_mode.process";

function check_and_set_opts (args, options)
   local function get_opt (alias1, alias2)
      return lprof.utils.get_opt (args, alias1, alias2)
   end

   -- Parallelism model
   local im = get_opt ("model", "m") -- DEPRECATED
   if (im ~= nil) then
      Message:warn ("[MAQAO] model/m is deprecated: use instrumentation-model/im.")
   end
   options.model = (im or get_opt ("instrumentation-model", "im"))

   -- Function filtering
   local ff = get_opt ("filter-function", "ff") -- DEPRECATED
   if (ff ~= nil) then
      Message:warn ("[MAQAO] filter-function/ff is deprecated: use instrumentation-filter-function/iff.")
   end
   ff = (ff or get_opt ("instrumentation-filter-function", "iff"))
   if (ff == nil or ff == "") then
      options.function_name_list = Table:new()
   else
      options.function_name_list = lprof:split (ff, ",")
      if (#options.function_name_list == 0) then
         Message:critical ("Bad format argument! Expected : -ff=foo,bar (Trace/Sample only functions foo and bar)")
      end
   end

   -- Custom counters list
   options.hwc_list = get_opt ("hardware-counters", "hwc") or ""
end

function instrument (options)
   local text = lprof:load_profile (options);
   local instru_file_text = { text = text or "" };
   local proj = project.new ("instrument");
   mil:start (proj, instru_file_text, Table:new());
end
