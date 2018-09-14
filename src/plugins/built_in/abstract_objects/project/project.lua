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

-- Project management

-- --> project_name
--    -- original binary
--    -- default config (execution env, command line...)
--    --> module_name (vprof, perf, decan...)
--       --> instru/mode/pass
--          -- config
--          -- binary file
--          --> runs
--             --> runX
--                -- config
--                -- results
--             --> runY
--                -- config
--                -- results

module ("project", package.seeall)

-- Copy a file
function file_copy(src,dst)
   os.execute("cp " .. src .." ".. dst)
end

-- Return filename from a path
local function basename(p) 
   local dir,filename,ext = string.match(p, "(.-)([^\\/]-%.?([^%.\\/]*))$")

   return filename;
end

-- Execute a program and return its output
-- TODO: return exit code (need Lua 5.2 to get it with close)
function os.capture(cmd, raw)
   local f = assert(io.popen(cmd, 'r'))
   local s = assert(f:read('*a'))
   f:close()
   if raw then return s end
   s = string.gsub(s, '^%s+', '')
   s = string.gsub(s, '%s+$', '')
   s = string.gsub(s, '[\n\r]+', ' ')
   return s
end

-- Return path of config file for the project
function project_config_path(path)
   return path .. "/config"
end

-- Return path of a module
function project_module_path(proj,module)
   return proj["project_path"].."/"..module
end

-- Compute and return MD5 of a file
function project:file_md5(path)
   return string.match(os.capture("md5sum "..path), "[0-9a-f]+")
end

-- Read a config file (lines with "key=value"), return hash table
function file_config_read(path)
   local config = {}
   if fs.exists(path) then
      for l in io.lines(path) do
         local k,v = string.match(l, "([^=]*)=(.*)")
         config[k] = v
      end
   end
   return config
end

-- Write a config file (lines with "key=value")
function file_config_write(path,config)
   local config_file = io.open(path, "w")
   for k,v in pairs(config) do
      if v == nil then
         v = ""
      end
      config_file:write(k .."=" .. v .. "\n")
   end
   config_file:close()
end

-- | Write a file
function file_write(path,content)
   local file = io.open(path, "w")
   file:write(content)
   file:close()
end

-- Write a table in a file
function table_write(table,name,path)
   file_write(path, Table:serialize(name,table,true))
end


--- Finds a path to a dynamic library from the current environment (using LD_LIBRARY_PATH)
-- @param lib The library name
-- @param lib_dirs Array of paths that could contain libraries as retrieved from the environment. Will be computed if nil.
-- If set to "", the environment will be considered as not containing any path.
-- @return - The path of the first directory found in the environment containing the library, or nil if none is found
--         - The array of paths that could contain libraries as retrieved from the environment, or "" if none was found
--           (used for further invocations, to avoid checking the environment every time)
function project:find_library_from_env(lib, lib_dirs)

   if lib_dirs == nil then
      local ld_lib_str  = os.getenv ("LD_LIBRARY_PATH")
      local ld_lib      = String:split (ld_lib_str, ":")
      if ld_lib == nil then
         return nil, ""
      end
      lib_dirs = ld_lib
   elseif lib_dirs == "" then
      return nil, lib_dirs
   end

   for _, sub_ld_lib in ipairs (lib_dirs) do
      if fs.exists (sub_ld_lib.."/"..lib) then
         return sub_ld_lib, lib_dirs;
      end
   end
   return nil, lib_dirs;
end

---
--  Import a file into a project directory
--  @param Full path to the file
function project:import_file(proj, file)
   file_copy(file, proj.project_path)
end

---
-- Create a project
-- 
-- @param binary A binary to copy in the project
-- @param libs [Opt] A table of library names to copy in the project
--
function project:create(path, binary, libs)

   if binary == nil then
      Message:critical("Cannot create project, binary file not defined.")
   end

   if fs.exists(path) then
      Message:critical("Directory \""..path.."\" already exists.")
   end

   if not fs.exists(path) then
      local ret = lfs.mkdir(path)
      if ret == nil then
         Message:critical("Cannot create directory \""..path.."\".")
      end
   end

   if lfs.attributes(path, "mode") ~= "directory" then
      Message:critical("Project path exists but is not a directory.")
   end

   if not fs.exists(binary) then
      Message:critical("Cannot find binary file \""..binary.."\".")
   end

   -- Copy binary file into project
   file_copy(binary, path)

   -- Compute md5sum of the binary file
   local md5sum = project:file_md5(binary)

   -- Handle libraries
   if  libs ~= nil 
   and type (libs) == "table" then
      local ld_lib = nil
      for _, lib in ipairs (libs) do
         -- Locate the library using LD_LIBRARY_PATH
         local sub_ld_lib  = nil
         sub_ld_lib, ld_lib = project:find_library_from_env(lib, ld_lib)

         -- If found, copy the library in the project
         if sub_ld_lib ~= nil then
            local full_path = sub_ld_lib.."/"..lib
            file_copy(full_path, path)
         -- Else, display an error but do not quit
         else
            Message:error ("Cannot find dynamic library "..lib)
         end
      end
   end

   -- Create and save project configuration
   local project =
      { project_path = path
      , binary       = basename(binary)
      , binary_md5   = md5sum
      , project_date = os.date("%Y-%m-%d %X")
      }

   -- Write config file
   file_config_write(project_config_path(path), project)

   return project
end


--
-- Open a project
--
function project:open(path)
   -- Check that the project looks correct
   if not fs.exists(path) then
      Message:critical("Cannot open directory \""..path.."\".")
   end
   if not fs.exists(project_config_path(path)) then
      Message:critical("Cannot open project config file \""..project_config_path(path).."\".")
   end

   -- Read config file
   local config = file_config_read(project_config_path(path))

   -- Set new path
   config["project_path"] = path

   return config
end


--
-- Open a module in a project
--
-- Find config for the module in folder with the module name
function project:open_module_config(project,module)

   local path = project_module_path(project,module)  

   -- Create module directory if necessary
   if not fs.exists(path) then
      local ret = lfs.mkdir(path)
      if ret == nil then
         Message:critical("Cannot create directory \""..path.."\".")
      end
   end

   -- Read module configuration
   local config = file_config_read(project_config_path(path))

   return config
end

--
-- Save a module
--
function project:save_module_config(project,module,config)
   local path = project_module_path(project,module)  

   file_config_write(project_config_file(path), config)
end


-- | Search an existing instrumentation with the same parameters
function find_matching_instru(project,module,instru_config)

   -- Module path
   local modpath = project_module_path(project,module)

   local instru = {}
   instru["path"] = ""
   instru["id"] = -1

   local found = false
   while not found do
      instru["id"]  = instru["id"] + 1
      instru["path"] = modpath.."/instru"..string.format("%04d",instru["id"])

      -- If empty directory, create a new instrumentation
      if not fs.exists(instru["path"]) then
         found = true
         break
      end

      -- The directory exists, check if it is exactly the same instrumentation configuration
      local old_config = file_config_read(instru["path"] .. "/instru.conf")
      local same_config = true
      for k,v in pairs(instru_config) do
         if old_config[k] ~= v then
            same_config = false
            break
         end
      end

      -- If it is the same, we reuse it. Otherwise, we test the next directory
      if same_config then
         return instru, true
      end
   end

   return instru,false
end

-- | Perform a custom instrumentation
function project:perform_custom_instru(proj,module,instru_config,instrument, check_reuse, verbose_lvl)

   -- Trye to reuse an existing instrumentation
   local instru,reusing = find_matching_instru(proj,module,instru_config, check_reuse)

   if reusing and check_reuse ~= false then
      Message:print(string.format("[%s] Reusing previous instrumentation %d", string.upper(module), instru["id"]), verbose_lvl, 1)
      return instru
   end

   Message:print(string.format("[%s] Performing instrumentation %d...", string.upper(module), instru["id"]), verbose_lvl, 1)

   if fs.exists (instru["path"]) == false then
      local ret = lfs.mkdir(instru["path"])
      if ret == nil then
         Message:critical("Cannot create directory \""..instru["path"] .."\".")
      end
   end

   -- Perform the instrumentation
   local ret = instrument(instru)

   if ret ~= 0 then
      lfs.rmdir(instru["path"])
      Message:critical("The instrumentation has failed.")
   end

   -- Write instru config
   file_config_write(instru["path"] .. "/instru.conf", instru_config)

   return instru
end

--
-- Perform a MIL instrumentation
--
-- Reuse previous project instrumentation if there is one with the same config
--
function project:perform_mil_instru(proj,module,instru_config,milconfig)

   local mil_instru = function(instru)

      -- Set path of the input binary
      milconfig["events"]["main_bin"]["path"] = proj["project_path"] .. "/" .. proj["binary"]

      -- Set the path of the output binary
      milconfig["events"]["run_dir"] = instru["path"]
      milconfig["events"]["main_bin"]["output_name"] = proj["binary"]

      -- Dump MIL configuration
      local mil_file = instru["path"] .. "/instru.mil"
      table_write(milconfig,"mil", mil_file)
      
      -- Create a fake project to make MIL happy
      -- FIXME: remove this crap
      local projargs = {}
      projargs["module"]               = "dummyname"
      projargs["uarch"]                = nil
      projargs["enable-debug-vars"]    = false
      projargs["disable-debug"]        = false
      projargs["lcore-flow-all"]       = false
      projargs["interleaved-functions-recognition"] = nil

      local proj = utils.init_project(projargs)

      -- Instruments the binary with MIL
      local mil_res = mil:start_ex(proj, milconfig)

   end

   return project:perform_custom_instru(proj,module,instru_config,mil_instru)
end

function project:load_instru(proj,module,instru_id)
   -- Module path
   local modpath = project_module_path(proj,module)

   -- Instru path
   local instru_path = modpath.."/instru"..string.format("%04d",instru_id)

   -- Check that the directory and the binary exist
   if not fs.exists(instru_path) then
      Message:critical("Cannot find instrumentation " .. instru_id)
   end

   local instru = file_config_read(instru_path .. "/instru.conf")
   instru["path"] = instru_path
   instru["id"] = instru_id

   return instru
end

-- | Run an instrumented binary
function project:run_instru(proj, module, instru_id, run_cmd, display_output, verbose_lvl)

   local instru = project:load_instru(proj,module,instru_id)
   local instru_path = instru["path"]

   local bin_path = instru_path .. "/" .. proj["binary"]

   if not fs.exists(bin_path) then
      Message:critical("Cannot find instrumented binary from instrumentation " .. instru_id)
   end

   -- Find run number and create directory
   local run_id = -1
   local run_path = ""
   repeat
      run_id = run_id + 1
      run_path = instru_path .. "/run" .. string.format("%04d",run_id)
   until not fs.exists(run_path)

   lfs.mkdir(run_path)

   -- Dump config (run date, env, etc.)
   local env = os.capture("env", true)
   local file = io.open(run_path .. "/env", "w")
   file:write(env)
   file:close()

   local run_config = {}
   run_config["start-date"] = os.date("%Y-%m-%d %X")
   run_config["run-cmd"] = run_cmd
   file_config_write(run_path .. "/run.conf", run_config)


   -- Set environment variable for run directory
   -- Execute binary
   Message:print(string.format("[%s] Executing instrumented binary (instrumentation %d, run %d)...", 
      string.upper(module), instru_id, run_id), verbose_lvl, 1)

   local ld_lib_path = os.getenv ("LD_LIBRARY_PATH") or ""
   local run_env = "LD_LIBRARY_PATH="..instru_path..":"..ld_lib_path.." "
   local run_bin = string.format("MAQAO_RUN_DIR=%s %s", run_path, string.gsub(run_cmd, "{MAQAO_BIN}", bin_path))
   Message:print("[MAQAO] Command: "..run_env..run_bin, verbose_lvl, 2)
   
   -- Used to update the environment in order to use modified libraries if needed
   -- The path to the instru%d%d%d%d directory must be added to LD_LIBRARY_PATH
   local res = os.capture(run_env..run_bin, true)

   run_config["stop-date"] = os.date("%Y-%m-%d %X")
   file_config_write(run_path .. "/run.conf", run_config)

   -- Saving output
   file_write(run_path .. "/output", res)
   if (display_output == true) then
      Message:print("[MAQAO] Command standard output:", verbose_lvl, 1)
      Message:print(res, verbose_lvl, 0)
   end

   local run = {}
   run["path"] = run_path
   run["id"] = run_id

   return run
end


function project:load_run(proj,module,instru_id,run_id)

   local modpath = project_module_path(proj,module)
   local instru_path = modpath.."/instru"..string.format("%04d",instru_id)
   local run_path = instru_path .. "/run" .. string.format("%04d",run_id)

   if not fs.exists(run_path) then
      Message:critical(string.format("Cannot find run %d from instrumentation %d", run_id, instru_id))
   end

   local run = file_config_read(run_path.."/run.conf")
   run["path"] = run_path
   run["id"] = run_id

   return run
end

function project:list_instrumentations(proj,module)
   local modpath = project_module_path(proj,module)
   local instru = {}
   for file in lfs.dir(modpath) do
      if lfs.attributes(modpath..file,"mode")== "directory" then
         for instru_id in string.gmatch(file, "instru([0-9]*)") do
            table.insert(instru, instru_id)
         end
       end
   end
   return instru
end


function project:list_runs(proj,module,instru_id)
   local modpath = project_module_path(proj,module)
   local runs = {}
   local p = modpath .. "/instru" .. string.format("%04d",instru_id) .. "/"
   for file in lfs.dir(p) do
      if lfs.attributes(p..file,"mode")== "directory" then
         local run_id = file:match("run([0-9]*)")
         table.insert(runs, tonumber(run_id))
       end
   end
   return runs
end

function project:last_run(proj,module,instru_id)
   local runs = project:list_runs(proj,module,instru_id)
   local last = nil
   for _,run_id in pairs(runs) do
      local r = project:load_run(proj,module,instru_id,run_id)
      if last == nil then
         last = r
      else
         if r["stop-date"] > last["stop-date"] then
            last = r
         end
      end
   end

   return last
end

-- Initialises the processor version associated to a project and prints an error message if it could not be identified
function project:init_proc_infos(bin, arch_name, uarch_name, proc_name)

   -- Retrieves the architecture from the input parameters or the binary
   local function get_arch(arch_name, bin)
      local arch, archname;
      if arch_name ~= nil then
         arch = get_arch_by_name(arch_name);
         archname = arch_name;
      elseif bin ~= nil then
         arch = get_file_arch(bin);
         if (arch ~= nil) then
            archname = arch:get_name();
         end
      end
      return arch, archname;
   end

   local res = self:init_proc(bin, arch_name, uarch_name, proc_name);
   
   -- Prints an error message if the processor version could not be successfully initialised
   if res == Consts.errors.ERR_LIBASM_PROC_NAME_INVALID then
      local arch, archname = get_arch(arch_name, bin);
      local str = string.format("%s is not a valid processor version for architecture %s", proc_name, archname)
      if (arch ~= nil) then
         local available_proc_names = arch:get_available_procs_names();
         str = str.."\n          Valid processor versions: "..table.concat (available_proc_names[archname], " ")
      end
      Message:error (str)
      os.exit (-1);
   elseif res == Consts.errors.ERR_LIBASM_UARCH_NAME_INVALID then
      local arch, archname = get_arch(arch_name, bin);
      local str = string.format("%s is not a valid micro-architecture for architecture %s",uarch_name, archname)
      if (arch ~= nil) then
         local available_uarch_names = arch:get_available_uarchs_names();
         str = str.."\n          Valid micro architectures: " .. table.concat (available_uarch_names[archname], " ")
      end
      Message:error (str)
      os.exit (-1);
   end
end
