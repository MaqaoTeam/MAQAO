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

-- This file defines functions shared between lprof LUA "modules" (lprof.display,main...)

module ("lprof.utils", package.seeall)

-- For options with 2 aliases, checks whether it is set
function is_opt_set (args, alias1, alias2)
   return (args [alias1] ~= nil or args [alias2] ~= nil)
end

function get_opt (args, alias1, alias2)
   if (args [alias1] ~= nil) then
      return args [alias1]
   end

   return args [alias2]
end

function is_feature_set (args, alias1, alias2, enabled_by_default)
   if (is_opt_set (args, alias1, alias2) == false) then return enabled_by_default end

   local feature = get_opt (args, alias1, alias2)
   if (feature == true) then return true end

   if (feature ~= "on" and feature ~= "off") then
      Message:error (string.format ("[MAQAO] Unknown %s option %s", alias1 or alias2, feature))
      Message:error ("[MAQAO] Available value options: on,off")
      return enabled_by_default
   elseif (feature == "on") then
      return true
   else
      return false
   end
end

-- TODO: reuse already existing function in MAQAO
function lprof:split (s, delim)

   assert (type (delim) == "string" and string.len (delim) > 0, "bad delimiter")

   if(s==nil) then
      return nil;
   end


   local start = 1
   local t = {}  -- results table

   -- find each instance of a string followed by the delimiter
   while true do
      local pos = string.find (s, delim, start, true) -- plain find
      if not pos then
         break
      end

      --check that there is no two consecutive delimiter caracters
      -- example : delim =',' in s : ",," we do not insert
      if (s:sub(start,start) ~= delim) then
         table.insert (t, string.sub (s, start, pos - 1))
      end
      start = pos + string.len (delim)
   end -- while

   -- insert final one (after last delimiter)
   table.insert (t, string.sub (s, start))
   return t
end -- function split

-- factorized display.lua + process.lua
-- dead code
function extendtowidth_withspaces(str,width,center,margin_left)
   local diff = width - string.len(str);
   local rslt = str;

   if(diff > 0) then
      if(type(margin_left) == "number" and margin_left < diff) then
         diff = diff - margin_left;
         rslt = string.rep(" ",margin_left)..rslt;
      end

      if(center == true) then
         rslt = string.rep(" ",diff/2)..rslt..string.rep(" ",diff/2);
      else
         rslt = rslt..string.rep(" ",diff);
      end
   end

   return rslt;
end

function copy_system_maps (proj,hostname,options)
   local system_map_file_name = string.format ("%s/%s/system_map", options["experiment_path"], hostname)
   local system_map_file = nil

   if (proj:get_arch() ~= Consts.ARCH_k1om) then
      os.execute (string.format ("cp -f /boot/System.map-$(uname -r) %s 2> /dev/null",
                                 system_map_file_name))
      system_map_file = io.open (system_map_file_name)
   end

   if (system_map_file == nil) then
      --If the System.map file doesn't exist try this solution
      --Function addresses in the first column could shows zeros
      --instead of the real addresses for a non-root user.
      --It depends of the OS (permission security)
      os.execute (string.format ("cp -f /proc/kallsyms %s 2> /dev/null",
                                 system_map_file_name))
   else
      system_map_file:close()
   end
end

function lprof:touch_file (path)
   local file_desc = io.open (path, "w")
   if (file_desc ~= nil) then file_desc:close() end
end

-- Create an empty file to notify others tools that something went wrong
function lprof:clean_abort (experiment_path, err_str)
   lprof:touch_file (experiment_path.."/done")

   if err_str then Message:critical (err_str) else os.exit (1) end
end

-- Check the format of the path given by the user.
-- It must end with a "/"
function lprof:check_output_path_format (path)

   local sub_dirs = String:split (path, "/")
   local sub_path = ""
   local ret, str_err;
   local checked_path = nil;

   if ( string.sub(path ,#path) ~= "/") then
      checked_path = path.."/";
   else
      checked_path = path;
   end

   return checked_path;
end
