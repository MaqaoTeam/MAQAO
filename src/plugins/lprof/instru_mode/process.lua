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

-- This file defines lprof:compute_LG_graph() and lprof:compute_FCG_graph(), used for the instru/trace lprof mode
-- This mode is no more maintained since years, it probably does not work today (2018/02/14)

module("lprof.instru_mode.process",package.seeall)

local function get_loop_hlevel (const)
   local hlevel;

   if(const == mil.LOOP_HLEVEL_SINGLE) then
      hlevel = "Single";
   elseif(const == mil.LOOP_HLEVEL_INNERMOST) then
      hlevel = "Innermost";
   elseif(const == mil.LOOP_HLEVEL_OUTERMOST) then
      hlevel = "Outermost";
   else
      hlevel = "InBetween";
   end

   return hlevel;
end

function get_color_by_value(percentage)
   if(percentage >= 0 and percentage < 10) then
      return "#00CCFF";
   elseif(percentage >= 10 and percentage < 20) then
      return "#0099FF";
   elseif(percentage >= 20 and percentage < 30) then
      return "#0066FF";
   elseif(percentage >= 30 and percentage < 40) then
      return "#3333CC";
   elseif(percentage >= 40 and percentage < 50) then
      return "#CC0099";
   elseif(percentage >= 50 and percentage <= 100) then
      return "#FF0000";
   else
      return "#000000";
   end
end

---     Creates a graph structure for FX and FI profiles based on profiling results and representing a given application call graph
--      @param rslts Table containing results from instrumentation
--      @param meta Table containing meta information (resolve intrumentation ids)
--      @param call_functions Table of functions that are not in the program (destination functions) = bin containing the callsite
--  @param tinfo thread specific info (results)
--      @return a Graph object
function lprof:compute_FCG_graph(rslts,meta,call_functions,tinfo,enable_compensation)
   local callsite_edges= rslts.callsite_edges;
   local total_cycles   = rslts.wallcycles;
   local total_time     = rslts.walltime;
   local functions     = meta.functions;
   local calls         = meta.calls;
   local price_fct     = rslts.price_fct;
   local price_call    = rslts.price_call;
   local price_timer   = rslts.price_tprobe;
   local compensation;

   enable_compensation = false;
   cfg = Graph:new();

   if(price_fct == nil or price_call == nil or price_timer == nil) then
      price_fct   = 0;
      price_call  = price_fct;
      price_timer = price_call;
   end

   --Print Functions that are not part of the binary (exclusive time = SUM(IN EDGES) and inclusive time = exclusive time)
   for fname,fcinfo in pairs(call_functions) do
      if(fcinfo == nil) then
         Message:error(fname.." has a null content value");
      elseif(fcinfo.already_defined == nil) then
         local cycles        = fcinfo.in_cycles - (price_call*fcinfo.instances);
         local totc_consumes = Math:round((cycles/total_cycles)*100,2);
         local cycles_color  = get_color_by_value(totc_consumes);

         --cg_cycles_dot_file:write(fcinfo.fid.." [color=\""..cycles_color.."\", fontcolor=\"#ffffff\", fontsize=\"10.00\", label=\""..
         --                                           string.format("0x%x",fname).."\\n"..totc_consumes.."% ("..fcinfo.instances.."x)\"];\n");
      end
   end
   --###############################################################################################
   --## 2 Pass algo to create CFG nodes and edges. Pass 1 for compensation and Pass 2 to actually ##
   --## create nodes + edges. Refer to Pass 2 for comments                                        ##
   --###############################################################################################
   if(enable_compensation == true) then
      --Pass 1: Iterate over nodes + edges and gather compensation information.
      compensation = Table:new({totalc = 0,fct = Table:new()});

      for ifid,finfo in pairs(tinfo.functions) do
         --compensated cycles :
         --    * 1 probe for the function
         --    * 2 probes and 2 timers for each callsite
         local func      = compensation.fct[ifid];
         local calls_sum = 0;

         if(compensation.fct[ifid] == nil) then
            compensation.fct[ifid]               = Table:new();
            compensation.fct[ifid].ccycles   = 0;
            func                             = compensation.fct[ifid];
         end
         --function probe time only needs to get rid of the equivalent of one probe (price_fct)
         func.ccycles          = func.ccycles + (finfo.instances * price_fct);
         --whereas the total function probe time at function level is the equivalent of 2 probes and 2 timers
         compensation.totalc   = compensation.totalc + (finfo.instances * ((price_call + price_timer) * 2));

         for _,cid in pairs(functions[ifid].calls) do
            if(tinfo.calls[cid] ~= nil) then
               if(tinfo.calls[cid].instances ~= -1) then
                  if(func.calls == nil) then
                     func.calls = Table:new()
                  end
                  if(func.calls[cid] == nil) then
                     func.calls[cid] = 0;
                  end
                  --callsite probe time only needs to get rid of the equivalent of one probe (price_call)
                  func.calls[cid] = tinfo.calls[cid].instances * price_call;
                  --whereas the total callsite probe time at function level is the equivalent of 2 probes and 2 timers
                  calls_sum  = calls_sum + (tinfo.calls[cid].instances * ((price_call + price_timer) * 2));
               end
            end
         end
         func.ccycles         = func.ccycles + calls_sum;
         compensation.totalc  = compensation.totalc + calls_sum;
      end

      --compensation:tostring();
      Debug:temp(total_cycles)
      Debug:temp(total_cycles-compensation.totalc.."\t"..compensation.totalc)
      --Remove gobal overhead
      --total_cycles = total_cycles - compensation.totalc;
   end
   --Pass 2: Create nodes and edges, for all the other functions, based on results
   for ifid,finfo in pairs(tinfo.functions) do
      local cycles,totc_consumes,excl_consumes,inc_consumes,fcompensation;

      if(enable_compensation == true) then
         fcompensation = compensation.fct[ifid].ccycles;
      else
         fcompensation = 0;
      end
      cycles        = finfo.elapsed_cycles - fcompensation;
      totc_consumes = Math:round((cycles/total_cycles)*100,2);
      excl_consumes  = Math:round(((finfo.elapsed_cycles - finfo.calls_cycles)/total_cycles)*100,2);
      cycles_color  = get_color_by_value(totc_consumes);

      --Add function Node
      cfg:add_node(functions[ifid].asmh.."_"..functions[ifid].fname,
                   Table:new({id = ifid,asmh = functions[ifid].asmh,
                              asmfn = functions[ifid].asmfn,fname = functions[ifid].fname,
                              totc_consumes = totc_consumes,excl_consumes = excl_consumes,
                              cycles_color = cycles_color,instances = finfo.instances,
                              children_overhead = 0})
      );

      for _,cid in pairs(functions[ifid].calls) do
         local dest;
         local cinfo = calls[cid];
         local call_cycles;
         --IF call instrumentation has no corresponding information (this should never happen since a -1
         --should have been set in the first phase when loading results)
         if(tinfo.calls[cid] ~= nil) then
            --IF call hasn't been instrumented
            if(tinfo.calls[cid].instances == -1) then
               call_cycles = 0;
            else
               local cycles,ccompensation;

               if(enable_compensation == true) then
                  ccompensation = compensation.fct[ifid].calls[cid];
               else
                  ccompensation = 0;
               end
               cycles       = tinfo.calls[cid].elapsed_cycles - ccompensation;
               call_cycles  = Math:round((cycles/total_cycles)*100,2);
            end
         else
            Message:error("callsite "..cid.." @"..cinfo.address.."("..cinfo.dest_name..")  has no time information");
         end
         local cc_color    = get_color_by_value(call_cycles);
         local penwidth    = (call_cycles/100)*2+1;

         --If destination of a call exits or hasn't been filtered then display its info
         if(cinfo.known_dest < 1) then
            --Dest exists
            dest = call_functions[cinfo.address].fid;
            if(dest == nil) then
               Message:error("Dest FCT Addr "..cinfo.address.." not found in call_functions");
               --call_functions:tostring();
            end
         else
            --Filtered
            dest = meta[cinfo.dest_asmh].fct_real2instru[cinfo.dest_fid];
            if(dest == nil) then
               --Debug:warn("Dest FID "..cinfo.dest_fid.." ("..cinfo.dest_name..") not found in meta.fct_real2instru. It may have been filtered (intentionally not instrumented).");
            end
         end
         --Display
         if(dest ~= nil) then
            --ifid = instrumentation src function id
            --dest = instrumentation dest function id
            local fct_from,fct_dest;

            if(functions[ifid] == nil or functions[dest] == nil) then
               if(functions[ifid] == nil) then
                  Message:critical("Expected function (if = "..ifid..") details but information is missing. Please report this issue.");
               end
               if(functions[dest] == nil) then
                  Message:error("Information about destination function (id = "..dest..") not found. The call graph will not be complete. Please report this issue.");
               end
            else
               fct_from = functions[ifid].asmh.."_"..functions[ifid].fname;
               fct_dest = functions[dest].asmh.."_"..functions[dest].fname;

               cfg:add_edge(fct_from,fct_dest,
                            Table:new({src = ifid,dest = dest,cc_color = cc_color,
                                       ct_color = ct_color,call_cycles = call_cycles,
                                       call_time = call_time,penwidth = penwidth,
                                       instances = tinfo.calls[cid].instances,
                                       instrumented = cinfo.real_instru,
                                       is_exit = cinfo.is_exit})
               );
            end
         end
      end
      --Remove overhead due to call sites instrumentation
   end

   --Add special node : WALLTIME which is project wide
   cfg:add_node("WALLTIME",
                Table:new({id = -1,asmh = "App",asmfn = "App",fname = "WALLTIME",
                           totc_consumes = 100,excl_consumes = 0,cycles_color = get_color_by_value(100),
                           instances = 1,children_overhead = 0})
   );
   cfg:set_node_root("WALLTIME");
   --TODO => Add a special box that would contain untracked time (for some reason like calls from a runtime)
   --Currently attached to WALLTIME box
   for nodename,node in pairs(cfg.nodes) do
      --Link nodes having no predecessors to WALLTIME
      if(node["in"]:get_size() == 0 and nodename ~= "WALLTIME") then
         cfg:add_edge("WALLTIME",nodename,
                      Table:new({src = -1,dest = node["data"].id,cc_color = node["data"].cycles_color,
                                 call_cycles = node["data"].excl_consumes,instances = 1,penwidth = 1})
         );
      end
      --Find leaf nodes
      if(node["out"]:get_size() <= 1) then
         --No successor
         if(node["out"]:get_size() == 0) then
            cfg:set_node_leaf(nodename);
            --Self = backedge
         elseif(node["out"]:get_size() == 1) then
            for id in pairs(node["out"]) do
               if(nodename == id) then
                  cfg:set_node_leaf(nodename);
               end
            end
         end
      end
      --Tag nodes with cycles (recursion)
      for nout in pairs(node["out"]) do
         if(nodename == nout) then
            node["data"]["is_recursive"] = true;
         end
      end
   end

   -- Try to guess inclusive time when possible
   -- Note that if all callsites have been instrumented, nothing have to guessed but just computed
   -- Algo :
   --   A node's inclusive time can be determined :
   --    * by substracting all the successors
   --    * only if each successor has one incoming edge (excluding recursive edges)
   local function process_node(node,userdata)
      local valctosub    = 0;
      local valttosub    = 0;
      local nb_real_succ = 0;
      local cansub       = true;

      data = node["data"];
      out  = node["out"];
      --Method 1 = If all successors only have one in edge
      --[[
         for vidout in pairs(out) do
         succdata = cfg.nodes[vidout]["data"];
         succin   = cfg.nodes[vidout]["in"];

         --if each successor has one incoming edge
         if((succin:get_size()) <= 1 or (succin:get_size() == 2 and succdata.is_recursive == true) ) then
         valctosub = valctosub + succdata.totc_consumes;
         else
         cansub = false;
         end
         end
      --]]
      --Method 2 = IF all calls have been timed
      --[
      --Incl = fct_time - SIGMA(callsite_time)
      for vidout,vout in pairs(out) do
         if (data.instances == 0) then
            cansub=false;
         elseif(vout.instrumented == true or vout.is_exit == true) then
            nb_real_succ = nb_real_succ + 1;

            if(vout.src ~= vout.dest) then -- exclude recursion edges
               valctosub = valctosub + vout.call_cycles;
            else
               --print("recursive ",succdata);
            end
         else
            succdata = cfg.nodes[vidout]["data"];
            succin   = cfg.nodes[vidout]["in"];

            --if each successor has one incoming edge
            if((succin:get_size()) <= 1 or (succin:get_size() == 2 and
                                            succdata.is_recursive == true) ) then
               nb_real_succ = nb_real_succ + 1;
               valctosub    = valctosub + succdata.totc_consumes;
            else
               cansub = false;
            end
         end
      end
      if(nb_real_succ ~= out:get_size()) then
         cansub = false;
      end
      --]

      if(cansub == false) then
         Debug:warn(data.fname.." inclusive time can't be determined => switching value to -1");
         data.excl_consumes = 0;
      else
         --print(data.fname.." CALLSITES cycles = "..valctosub);
         data.excl_consumes = Math:round(data.totc_consumes - valctosub,2);
         --temp fix due to compensation issue
         if (data.excl_consumes < 0) then
            data.excl_consumes = 0;
         end
      end
   end

   -- Computes a given node's children overhead (used further for compensation)
   local function compute_children_node_overhead(node,userdata)
      if(compensation ~= nil) then
         local compensation = userdata;

         for vidin,vin in pairs(node["in"]) do
            local preddata = cfg.nodes[vidin]["data"];
            local comp     = compensation.fct[vin.dest].ccycles  * vin.instances;
            --Each predecessor children_overhead field get updated (node.children_overhead = SIGMA(successors instru overhead))
            --[[
               print(vin.dest,vin.instances)
               print(compensation.fct[vin.dest]);
               print(preddata.children_overhead);
               print(vin);
               print(node.data);
               print(preddata);
            --]]
            preddata.children_overhead = preddata.children_overhead + comp;
            if(vin.instrumented == true) then

            end
         end
      end
   end

   local function disp_childreno(node,userdata)
      data = node["data"];
      print(data.fname,data.children_overhead)
   end

   --
   if(not callsite_edges == true) then
      local temp_visit = Table:new();
      for rootname in pairs(cfg.roots) do
         cfg:dfs(rootname,process_node,nil,temp_visit);
      end
      temp_visit = nil;
      --[[
         --Perform compensation
         local temp_visit = Table:new();
         --for leafname in pairs(cfg.leaves) do
         cfg:backbfs(nil,compute_children_node_overhead,compensation,temp_visit);
         --end
         temp_visit = nil;
      --]]
      --[[
         --Temp debug to verify children_overhead values
         local temp_visit = Table:new();
         for rootname in pairs(cfg.roots) do
         cfg:dfs(rootname,disp_childreno,nil,temp_visit);
         end
         temp_visit = nil;

         local temp_visit = Table:new();
         cfg:backbfs(nil,disp_childreno,nil,temp_visit);
         temp_visit = nil;
      --]]
   end

   return cfg;
end

function lprof:compute_LG_graph(rslts,meta,tinfo)
   local total_cycles   = rslts.wallcycles;
   local loops         = meta.loops;
   local output         = Table:new();

   for ilid,linfo in pairs(tinfo.loops) do
      local totc_consumes     = Math:round((linfo.elapsed_cycles/total_cycles)*100,2);
      local str_totc_consumes = totc_consumes;
      local hlevel    = loops[ilid].hlevel;

      hlevel = get_loop_hlevel(hlevel);

      if(totc_consumes > 100) then
         str_totc_consumes = "N.A. (no time)";
         totc_consumes = 0;
      else
         str_totc_consumes = str_totc_consumes.."%";
      end

      if(linfo.instances > 0) then
         local basename = "";
         if (fs.basename(loops[ilid].src_file_path) ~= "") then
            basename =  "-"..fs.basename(loops[ilid].src_file_path);
         end
         output:insert({
                          id        = loops[ilid].lid,
                          src       = loops[ilid].infct_name.." ["..loops[ilid].srcstart..","..loops[ilid].srcstop..basename.."]",
                          hlevel    = hlevel,
                          str_time  = str_totc_consumes,
                          time      = totc_consumes,
                          calls     = #loops[ilid].calls;
                          instances = linfo.instances.."x"
                       });
      end
   end


   return output;
end
