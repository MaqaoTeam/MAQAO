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

module("lprof.instru_mode.display",package.seeall)

filter_inc_node = nil;
filter_exc_node = nil;

local function can_display_node(data,inc)
   local fname     = data.fname;
   local exc       = data.totc_consumes;
   local instances = data.instances;

   if(instances > 0 or not remove_null_instances == true) then
      return true;
   elseif(type(filter_exc_node) == nil and type(exc) == number and exc > filter_exc_node) then
      return true;
   elseif(type(filter_inc_node) == nil and type(inc) == number and inc > filter_inc_node) then
      return true;
   else
      return false;
   end
end

local function can_display_edge(instances,exc)
   if(instances > 0 or not remove_null_instances == true) then
      return true;
   else
      return false;
   end
end

local function print_FCG_dot_png(graph,rslts,disp_modules,thread_id,options, png_only)

   local cg_cycles_dot_file;
   local remove_null_instances = false;
   local prefix               = rslts.binfile_hash;
   local filenames             = Table:new();

   filenames["cg_cycles_dot"] = options["experiment_path"].."/"..prefix.."_"..thread_id..".cycles.cg.dot";
   filenames["cg_cycles_png"] = options["experiment_path"].."/"..prefix.."_"..thread_id..".cycles.cg."..options["output_format"];
   if (png_only == true) then
      Message:info("Output image file is : "..filenames["cg_cycles_png"]);
   end

   --Local function that uses cg_cycles_dot_file
   local display_dot = function(node,userdata)
      local data = node["data"];
      local out  = node["out"];
      local inc  = node["in"];

      if(data ~= nil) then
         local label;
         local inclusivec;

         if(disp_modules == true) then
            label = "<"..fs.basename(data.asmfn).." | "..data.fname..">";
         else
            label = data.fname;
         end
         if(data.inc_consumes == -1) then
            inclusivec = "-";
         else
            inclusivec = data.excl_consumes.."%";
         end
         --Nodes
         --Special case for all children nodes of WALLTIME because they have no real predecessor
         if(can_display_node(data,inclusivec) == true) then
            cg_cycles_dot_file:write(data.id.." [color=\""..data.cycles_color..
                                        "\", fontcolor=\"#ffffff\", fontsize=\"10.00\", label=\""..
                                        label.."\\n"..data.totc_consumes.."%\\n("..inclusivec..") "..
                                        data.instances.."x\"];\n");
         end
         --Edges
         for vout,vdata in pairs(out) do
            if(can_display_edge(vdata.instances,vdata.call_cycles) == true) then
               cg_cycles_dot_file:write(vdata.src.." -> "..vdata.dest.." [color=\""..
                                           vdata.cc_color.."\", label=\""..vdata.call_cycles.."%\\n"..
                                           vdata.instances.."x\", arrowsize=\"0.53\", "..
                                           "fontsize=\"10.00\", fontcolor=\""..vdata.cc_color..
                                           "\", labeldistance=\"1.13\", penwidth=\""..
                                           vdata.penwidth.."\"];\n");
            end
         end
      end
   end

   local dot_header = "digraph{\n"..
      "graph [ranksep=0.25, fontname=Arial, nodesep=0.125];\n"..
      "node [fontname=Arial, style=filled, height=0, width=0, shape=box3d, fontcolor=white];\n"..
      "edge [fontname=Arial];\n";

   cg_cycles_dot_file  = io.open(filenames["cg_cycles_dot"],"w+");
   cg_cycles_dot_file:write(dot_header);

   local tmp_visit = Table:new();

   for rootname in pairs(graph["roots"]) do
      graph:dfs(rootname,display_dot,nil,tmp_visit);
   end
   tmp_visit = nil;
   cg_cycles_dot_file:write("}\n");
   cg_cycles_dot_file:close();
   if (png_only == true) then
      print("dot -T"..options["output_format"].." -o "..filenames["cg_cycles_png"].." "..filenames["cg_cycles_dot"]);
      os.execute("dot -T"..options["output_format"].." -o "..filenames["cg_cycles_png"].." "..filenames["cg_cycles_dot"]);
      os.execute("rm "..filenames["cg_cycles_dot"]);
   else
      print("Graph generated in file "..filenames["cg_cycles_dot"])
   end
end

local function print_FCG_txt(graph,profile,thread_id)

   --Creation of the output_results Table
   --It's a formatted output array, easier to parse and sort
   local output_results = Table:new();
   for nodename,node in pairs(graph.nodes) do
      local totc_consumes,excl_consumes,incl_consumes,row_text,instances,fname;
      local data = node["data"];
      local inc  = node["in"];

      calleesNb = 0;
      for vidin,vin in pairs (inc) do
         calleesNb = calleesNb + vin.instances
      end

      if (profile == "FX") then
         excl_consumes = data.excl_consumes;
      else -- FI
         excl_consumes = "N/A";
      end

      output_results:insert({
            name       = data.fname,
            excl_time   = excl_consumes,
            incl_time   = data.totc_consumes,
            callees      = calleesNb,
            instances   = data.instances;
      });
   end

   if (profile == "FX") then
      table.sort(output_results, function(a,b) return a.excl_time > b.excl_time end);
   else --FI
      table.sort(output_results, function(a,b) return a.incl_time > b.incl_time end);
   end

   --Now we create the "row" array
   local column_name  = {"Function Name","Time Excl","Time Incl","# Callee","Instances"};
   local output_display = Table:new();
   output_display:insert(column_name);

   --Insert each lines into the row array
   for i,func in ipairs(output_results) do
      output_display:insert({func.name,func.excl_time,func.incl_time,func.callees,func.instances});
   end

   local column_names = output_display[1];
   --array that store the spaces neccesary to align the columns
   local alignment = lprof:search_column_alignment(column_names,output_display);
   --Spaces added on each side of a column value
   local column_marge = 2;


   --The display part himself
   lprof:print_thread_id(thread_id,0,column_names,alignment,column_marge);
   lprof:print_column_header(column_names,alignment,column_marge,thread_id);
   -- print each lines (infos are stored from 2 to N)
   for i=2,table.getn(output_display)  do
      lprof:print_line(output_display[i],alignment,column_marge);
   end
   lprof:print_boundary(column_names,alignment,column_marge);
end

function lprof:print_FCG(graph,rslts,disp_modules,thread_id,options)
   if(options["display_type"] == "txt") then
      print_FCG_txt(graph,options["profile"],thread_id);
   elseif(options["display_type"] == "png") then
      print_FCG_dot_png(graph,rslts,disp_modules,thread_id,options, true);
   elseif(options["display_type"] == "dot") then
      print_FCG_dot_png(graph,rslts,disp_modules,thread_id,options);
   end
end

--TXT output for LI or LO
--output_infos format is:
--output_infos[1]     : Column names
--output_infos[2..n] : information on each loop
local function print_LG_txt(output_results,thread_id)

   --Sort results by time
   --table.sort(output_results, sort_fct);
   table.sort(output_results, function(a,b) return a.time > b.time end);
   local column_name = {"Loop ID","Source info","Level","Time Incl","Calls","Instances"};
   --Now we create a formatted output, easier to parse and display (a CSV like format)
   local output_infos = Table:new();
   --insert column name
   output_infos:insert(column_name);
   -- insert data
   for i,loop in ipairs(output_results) do
      output_infos:insert({loop.id,loop.src,loop.hlevel,loop.str_time,loop.calls,loop.instances});
   end

   loops="";
   column_names = output_infos[1];
   --alignment : array that store the spaces neccesary to align the columns
   local alignment = lprof:search_column_alignment(column_names,output_infos);
   --colomnMarge : Spaces added on each side of a column value
   local column_marge = 2;

   lprof:print_thread_id(thread_id,0,column_names,alignment,column_marge);
   lprof:print_column_header(column_names,alignment,column_marge,thread_id);
   -- print each lines (infos are stored from 2 to N)
   for i=2,table.getn(output_infos)  do
      loops = loops..output_infos[i][1]..",";
      lprof:print_line(output_infos[i],alignment,column_marge);
   end
   lprof:print_boundary(column_names,alignment,column_marge);
   print(loops);
end

local function print_LG_dot()
   Message:info("/!\\ PNG OUTPUT FORMAT FOR LO/LI IS NOT YET AVAILABLE /!\\");
   os.exit();
end

function lprof:print_LG(data_output,disp_type,thread_id)
   if(disp_type == "txt") then
      print_LG_txt(data_output,thread_id);
   elseif(disp_type == "png") then
      print_LG_dot();
   end
end

function check_and_set_opts (args, options)
   -- Display type
   local dt = lprof.utils.get_opt (args, "display-type", "dt")
   if (type (dt) ~= "string" or (dt ~= "txt" and dt ~= "png" and dt ~= "dot")) then
      Debug:warn ("No display type selected, using default text display")
      dt = "txt"
   end
   options.display_type = dt

   -- Thread ID list / filtering
   local ft = lprof.utils.get_opt (args, "filter-thread", "ft")
   if (ft == nil or ft == "") then
      options.thread_id_list = Table:new()
   else
      --Create the subset list of threads specified by the user.
      options.thread_id_list = loadstring ('return Table:new({'..ft..'})')()
      if (options.thread_id_list:get_size() == 0) then
         Message:critical ("Bad format argument! Expected : -ft=0,1,4 (Print Thread #0, #1 and #4)")
      end
   end
end

-- Called by select_display in tracing mode
-- TODO: consider to move this out of lprof...
function display(pname,sid,options)
   local dirinfo;
   local meta_files = Table:new();
   local rslt_files = Table:new();

   if(options["experiment_path"] ~= nil and fs.exists(options["experiment_path"]) == true) then
      dir = options["experiment_path"];
   else
      Message:critical("an invalid experiment_path was specified.");
   end

   dirinfo = fs.readdir(dir);
   --Detect meta files
   if (dirinfo == nil) then
      Message:critical("cannot read dir "..dir);
   else
      for i,d in pairs(dirinfo) do
         --print("Processing file "..d.name.." type = "..d.type);
         if(d.type == 2 and string.match(d.name,pname) ~= nil
            and string.match(d.name,"%.meta$") ~= nil) then
            meta_files:insert(options["experiment_path"].."/"..d.name);
         end
      end
   end

   if(meta_files:get_size() == 0) then
      Message:critical("No meta file found.");
   end

   if (dirinfo == nil) then
      Message:critical("cannot read dir "..dir)
   else
      for i,d in pairs(dirinfo) do
         --print("Processing file "..d.name.." and hash = "..asmh);
         if(d.type == 2 and string.match(d.name,pname) ~= nil
            and string.match(d.name,"%.rslt$") ~= nil) then
            --print("Found result file "..d.name);
            rslt_files:insert(options["experiment_path"].."/"..d.name);
         end
      end
   end

   dofile(meta_files[1]);
   mil.instru_sessions  = {[pname] = mil.__tmp_intru_meta};
   if(rslt_files:get_size() == 0) then
      Message:critical("No result file found");
   end
   dofile(rslt_files[1]);

   local instru_session = mil:project_instru_get_sess(pname,sid);
   local rslts,meta,cf  = lprof:load_instru_session(instru_session);
   local disp_modules   = false;
   local filenames      = Table:new();
   local cg;
   local data_output;
   local default_thread = 0;
   local printed_threads = 0;

   options["output_format"]  = "png";
   for thread_id = 0,rslts.nb_threads - 1 do
      if ( options["thread_id_list"]:get_size() == 0 or options["thread_id_list"]:containsv(thread_id)==true) then
         local tinfo = rslts.threads[thread_id];
         if(options["profile"] == "FI" or options["profile"] == "FX") then
            --rslts : wallcycles walltime binfile_hash
            --meta : information from instrumentation stage
            --call_functions : functions outside application scope => in order to create a time box for them
            --tinfo : default tprofilehread instrumentation results
            cg = lprof:compute_FCG_graph(rslts,meta,cf[thread_id],tinfo);
            --Display graph
            lprof:print_FCG(cg,rslts,disp_modules,thread_id,options);
         elseif(options["profile"] == "LO" or options["profile"] == "LI") then
            data_output = lprof:compute_LG_graph(rslts,meta,tinfo);
            lprof:print_LG(data_output,options["display_type"],thread_id);
         else
            Message:critical("Unsupported display profile.\nAvailable display profile : FI|FX|LI|LO");
         end
         thread_id = thread_id + 1;
         printed_threads = printed_threads + 1;
      end
      cg=nil;
      data_output=nil
   end
end
