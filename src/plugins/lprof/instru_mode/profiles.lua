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

-- This file defines lprof:load_profile() which is used for the instru/trace lprof mode
-- This mode is no more maintained since years, it probably does not work today (2018/02/14)

module("lprof.instru_mode.profiles",package.seeall)

function lprof:load_profile(options)
   local libinstru;

   if(type(options["profile"]) ~= "string") then
      Message:critical("given profile is not a valid string");
   elseif(type(options["bin"]) ~= "string" or fs.exists(options["bin"]) == false)       then
      Message:critical("target binary or library does not exist");
   end

   if(type(options["model"]) == "string") then
      if(options["model"] == "mpi" or options["model"] == "MPI") then
         model     = "mpi";
         libinstru = "libinstru-mpi.so";
         Debug:info("Using MPI model");
      elseif(options["model"] == "iomp" or options["model"] == "IOMP") then
         model     = "openmp";
         libinstru = "libinstru-iomp.so";
         Debug:info("Using Intel OpenMP model");
      elseif(options["model"] == "gomp" or options["model"] == "GOMP") then
         model     = "openmp";
         libinstru = "libinstru-gomp.so";
         Debug:info("Using GNU OpenMP model");

         -- elseif(model == "mpi+openmp" or model == "MPI+OPENMP") then
         --     model     = "mpi+openmp";
         --             libinstru = "libinstru-h_mpi-openmp.so";
         --     Debug:info("Using Hybdrid MPI/OpenMP model");

      else
         model = "unicore";
         libinstru = "libinstru.so";
         Debug:info("Using unicore model (one process/one thead)");
      end
   else
      model = "unicore";
      libinstru = "libinstru.so";
      Debug:info("Using unicore model (one process/one thead)");
   end

   func_whitelist = table.concat(options["function_name_list"],",");

   Debug:temp("model = "..model);

   if(options["profile"] == "WT") then
      return wt(options["experiment_path"],options["bin"],libinstru);
   elseif(options["profile"] == "FI") then
      return fi(options["experiment_path"],options["bin"],libinstru,func_whitelist);
   elseif(options["profile"] == "FX") then
      return fx(options["experiment_path"],options["bin"],libinstru,func_whitelist);
   elseif(options["profile"] == "LO") then
      return lo(options["experiment_path"],options["bin"],libinstru);
   elseif(options["profile"] == "LI") then
      return li(options["experiment_path"],options["bin"],libinstru);
   elseif(options["profile"] == "FILO") then
      return filo(options["experiment_path"],options["bin"],libinstru);
   elseif(options["profile"] == "OPR") then
      return opr(options["experiment_path"],options["bin"],libinstru);
   elseif(options["profile"] == "OFOR") then
      return ofor(options["experiment_path"],options["bin"],libinstru);
   elseif(options["profile"] == "OPRFOR") then
      return oprfor(options["experiment_path"],options["bin"],libinstru);
   elseif(options["profile"] == "sampling") then
      return sampling(options["experiment_path"],options["bin"],libinstru,options["hwc_list"]);
   else
      Message:critical("given profile is not a valid. Please choose one of the existing profiles (see help)");
   end
end

function fi(output_path,bin,libinstru,func_whitelist)
   return [[

   local mil_out_path = "]]..output_path..[[/";
   events = {
      run_dir = mil_out_path,
      main_bin = {
         at_entry={{
            name = "instru_load",
            lib = "]]..libinstru..[[",
            params = { {type = "macro",value = "instru_launch_params"} }
         }},
         properties={
            enable_callsite_instrumentation = false,
            --disable_function_instru_insert = true,
            enable_callsite_instrumentation = false,
            instru_trace_log=true,
            generate_metafile = true,
            distinguish_inlined_functions   = true,
            distinguish_suffix = "_omp",
         },
         path = "]]..bin..[[",
         functions={{

            filters = {{
               type = "whitelist",
               filter = {{subtype ="regexplist", value ={"]]..func_whitelist..[["} }}
            }},

            entries = {{
               name = "instru_fct_tstart",
               lib = "]]..libinstru..[[",
               params = { {type = "macro",value = "profiler_id"} }
            }},
            exits = {{
               name = "instru_fct_tstop",
               lib = "]]..libinstru..[[",
               params = { {type = "macro",value = "profiler_id"} }
            }},
            callsites = {{}}
         }}
      }
   }
   ]]
end

function fx(output_path,bin,libinstru,func_whitelist)
   return [[
   function udf_isnot_recursive(call)
      local call_dest = call:get_target_function();
      if(call_dest ~= nil) then
         if(call_dest:get_name() == call:get_function():get_name()) then
            return false;
         end
      end

      return true;
   end
   local mil_out_path = "]]..output_path..[[/";
   events = {
      run_dir = mil_out_path,
      main_bin = {
         at_entry={{
            name = "instru_load",
            lib = "]]..libinstru..[[",
            params = { {type = "macro",value = "instru_launch_params"} }
         }},
         properties={
            enable_function_instrumentation = true,
            enable_callsite_instrumentation = true,
            generate_metafile = true,
            distinguish_inlined_functions   = true,
            distinguish_suffix = "_omp",
            instru_trace_log = true,
         },
         path = "]]..bin..[[",
         functions={{

            filters = {{
               type = "whitelist",
               filter = {{subtype = "regexplist", value = {"]]..func_whitelist..[["} }}
            }},

            entries = {{
               name = "instru_fct_tstart",
               lib = "]]..libinstru..[[",
               params = { {type = "macro",value = "profiler_id"} }
            }},
            exits = {{
               name = "instru_fct_tstop",
               lib = "]]..libinstru..[[",
               params = { {type = "macro",value = "profiler_id"} }
            }},
            callsites = {{
               filters={{type = "user", filter = udf_isnot_recursive}},
               before = {{
                  name = "instru_fct_call_tstart",
                  lib = "]]..libinstru..[[",
                  params = { {type = "macro",value = "profiler_id"} }
               }},
               after = {{
                  name = "instru_fct_call_tstop",
                  lib = "]]..libinstru..[[",
                  params = { {type = "macro",value = "profiler_id"} }
               }}
            }}
         }}
      }
   }
   ]]
end

function lo(output_path,bin,libinstru)
   return [[

   local mil_out_path = "]]..output_path..[[/";
   events = {
      run_dir = mil_out_path,
      main_bin = {
         at_entry={{
            name = "instru_load",
            lib = "]]..libinstru..[[",
            params = { {type = "macro",value = "instru_launch_params"} }
         }},
         properties={
            enable_loop_instrumentation = true,
            generate_metafile = true,
            distinguish_inlined_functions   = true,
            distinguish_suffix = "_omp",
         },
         path = "]]..bin..[[",
         functions={{
            loops = {{
               filters = {{
                  type = "builtin",
                  filter = {{attribute = "nestlevel",value = "outer"}}
               }},
               entries={{
                  name = "instru_loop_tstart",
                  lib = "]]..libinstru..[[",
                  params = {{type = "macro",value = "profiler_id"} }
               }},
               exits={{
                  name = "instru_loop_tstop",
                  lib = "]]..libinstru..[[",
                  params = {{type = "macro",value = "profiler_id"} }
               }},
            }}
         }}
      }
   }
   ]]
end

function li(output_path,bin,libinstru)
   return [[

   local mil_out_path = "]]..output_path..[[/";
   events = {
      run_dir = mil_out_path,
      main_bin = {
         at_entry={{
            name = "instru_load",
            lib = "]]..libinstru..[[",
            params = { {type = "macro",value = "instru_launch_params"} }
         }},
         properties={
            enable_loop_instrumentation = true,
            generate_metafile = true,
            distinguish_inlined_functions   = true,
            distinguish_suffix = "_omp",
         },
         path = "]]..bin..[[",
         functions={{
            loops = {{
               filters = {
                  --{
                  --          type = "whitelist",
                  --          filter = { {subtype = "numberlist",value = {541,2,49}} }
                  --}
                  {
                     type = "builtin",
                     filter = {{attribute = "nestlevel",value = "inner"}}
               }},
               entries={{
                  name = "instru_loop_tstart",
                  lib = "]]..libinstru..[[",
                  params = {{type = "macro",value = "profiler_id"} }
               }},
               exits={{
                  name = "instru_loop_tstop",
                  lib = "]]..libinstru..[[",
                  params = {{type = "macro",value = "profiler_id"} }
               }},
            }}
         }}
      }
   }
   ]]
end

function filo()
   Message:critical("not yet implemented.");
end

function opr()
   Message:critical("not yet implemented.");
end

function ofor()
   Message:critical("not yet implemented.");
end

function oprfor()
   Message:critical("not yet implemented.");
end

function wt(output_path,bin,libinstru)
   return [[

   local mil_out_path = "]]..output_path..[[/";

   events = {
      run_dir = mil_out_path,
      main_bin = {
         at_entry={{
            name = "instru_load",
            lib = "]]..libinstru..[[",
            params = { {type = "macro",value = "instru_launch_params"} }
         }},
         properties={
            generate_metafile = true,
            distinguish_inlined_functions   = true,
            distinguish_suffix = "_omp",
         },
         path = "]]..bin..[[",
      }
   }
]]
end


function sampling(output_path,bin,libinstru,hwc_parameters)
   return [[

   local mil_out_path = "]]..output_path..[[/";

   function setParams ()
      params = Table:new();
      params:insert{
         type = "string";
         value = "]]..hwc_parameters..[[";
      }
      return params;
   end

   events = {
      run_dir = mil_out_path,
      main_bin = {
         at_entry={{
            name = "instruLoad",
            lib = "]]..libinstru..[[",
            params = setParams() ,
         }},
         at_exit={{
            name = "instruStop",
            lib = "]]..libinstru..[[",
         }},
         properties={
            generate_metafile = true,
            distinguish_inlined_functions   = true,
            distinguish_suffix = "_omp",
         },
         path = "]]..bin..[[",
      }
   }
]]
end
