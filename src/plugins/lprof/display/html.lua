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

module("lprof.display.html",package.seeall)

require "lprof.display.html.resources";
require "lprof.display.html.categories";
require "lprof.display.html.charts";
require "lprof.display.html.loops";

function sort_fct (a,b)
   for idColumn,value in ipairs(a) do
      if idColumn==lprof.FCT_TIME_PERCENT_IDX then
         time = lprof:split(value,"(")
         k1=time[1];
         break;
      end;
   end;
   for idColumn,value in ipairs(b) do
      if idColumn==lprof.FCT_TIME_PERCENT_IDX then
         time = lprof:split(value,"(")
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
--[[ Nice ideas to be added in the futur:
 * Tooltip displaying GID
 * Group by Node / Process using colors
 * Process vue (insert after node vue) = threads grouped by process
 * From node view: if clicked show process/thread vue
 * In node view add node name (for instance in a tooltip)
 * Sub node view (to deal with 'logical' numa node)
--]]
function print_sampling_html ( output_path,hostname_id,fcts_info,
                               callchain_info, loops_child,
                               outermost_loops,loops_id_to_time,is_library,
                               categories,binary_name, thread_idx_to_thread_id,
                               pid_idx_to_pid
                             )

   Message:info("Generating HTML GUI... Please wait...");

   --LOAD CQA_TABLE
   -- CQA_TABLE LOCATION : OUTPUT_PATH/HOSTNAME/cqa.lua
--   local cqa_file = output_path.."/"..hostname.."/".."cqa.lua";
--   dofile(cqa_file);
--   local level2 = cqa_infos;
--   Table:new(level2):tostring();

   --TODO: Sort the table (on one of the event sampled chosen by the user)
   --output_display:tostring();
   local gridscriptstart,gridscriptend,gridobject,gridobject_hf,grid_data_start,grid_data,grid_data_hf;
   local htmlbodyhead,htmlbodycharts,htmlbody,htmlbodytail;
   local accordion_groups = Table:new();
   local htmloutput_path = output_path.."/html/";
   local htmloutput_filename = "index.html";
   local htmloutput_file;
   local colspan = 3;
   --print(hostname,id_process,tid);output_path,output_file
   if(fs.exists(htmloutput_path) ~= true) then
      ret,err = lfs.mkdir(htmloutput_path);
      if(ret == nil) then
        Message:critical(err);
      end
   else
      --Already exists
   end

   local function_timings = Table:new();
   local loop_timings     = Table:new();

   htmloutput_file = io.open(htmloutput_path..htmloutput_filename,"w+");
   --Generate needed UI files
   lprof.display.html.html_generate_helper_files(htmloutput_path);

   grid_data  = "";
   gridobject = "";
   grid_data_hf  = "";
   gridobject_hf = "";

   --Create grids for each process in the data folder that will be loaded on demand
   for hostname,pids in pairs(fcts_info) do
     --print(hostname_id[hostname]);Table:new():tostring(pids);
     for pid,thread_info in pairs(pids) do
         for tid,output_display in pairs(thread_info) do                  
            --reset grid_data and gridobject because we will write a new file
            grid_data  = "";
            gridobject = "";
            --Check empty table
            if (table.getn(output_display) > 0)  then
               local id      = 1;
               local title   = hostname_id[hostname].." - Process #"..pid_idx_to_pid[pid].." - Thread #"..thread_idx_to_thread_id[hostname][pid][tid];
               local tcallchain = callchain_info[hostname][pid][tid];
               local gid        = string.gsub(hostname,"[%.-]","_dot_")..'_'..pid..'_'..tid;
               table.sort(output_display, sort_fct);
               accordion_groups:insert({title = title, uid = '_'..gid});
               grid_data = grid_data.. '  var ggriddata'..'_'..gid..' = ['.."\n";
               -- print each lines (infos are stored from 2 to N)
               for i=1,table.getn(output_display) do
                  --############################
                  --First pass: fill fct_node  #
                  --############################
                  local last_parent,last_parent_cc,last_parent_loop,parent_info;
                  local fct_name,name,time,abs_time;
                  local outermost_loops_key = output_display[i][lprof.FCT_NAME_IDX];
                  local fct_node;
                  local has_callchains = false;
                  local callchains;

                  if (output_display[i][lprof.FCT_SRC_INFO_IDX] == "-1") then
                     fct_name = output_display[i][lprof.FCT_NAME_IDX];
                  else
                     fct_name = output_display[i][lprof.FCT_NAME_IDX].." "..output_display[i][lprof.FCT_SRC_INFO_IDX];
                  end
                  fct_name_only = output_display[i][lprof.FCT_NAME_IDX];
                  name = fct_name;

                  time = output_display[i][lprof.FCT_TIME_PERCENT_IDX];
                  --TODO: remove string.match(time,"([%d%.]+).*") thanks to optional samples
                  time = tonumber(string.match(time,"([%d%.]+).*"));
                  abs_time  = output_display[i][lprof.FCT_TIME_SECOND_IDX];
                  mod  = output_display[i][lprof.FCT_MODULE_IDX];
                  callchains = tcallchain[fct_name];

                  --Prepare functions' hotspots table
                  if(function_timings[fct_name_only] == nil) then
                     function_timings[fct_name_only] = Table:new();
                     function_timings[fct_name_only].values    = Table:new();
                     function_timings[fct_name_only].functions = Table:new();
                     function_timings[fct_name_only].bynode    = Table:new();
                  end
                  if(function_timings[fct_name_only].bynode[hostname] == nil) then
                     function_timings[fct_name_only].bynode[hostname]          = Table:new();
                     function_timings[fct_name_only].bynode[hostname].values   = Table:new();
                     --function_timings[fct_name_only].bynode[hostname].functions = Table:new();
                     function_timings[fct_name_only].bynode[hostname].hostname = hostname_id[hostname];
                  end
                  function_timings[fct_name_only].values:insert(time);
                  function_timings[fct_name_only].functions:insert({title = title,time = time, gid = gid});
                  function_timings[fct_name_only].bynode[hostname].values:insert(time);
                  --function_timings[fct_name_only].bynode[hostname].functions:insert({title = title,time = time, gid = gid});                 
                  --Prepare loops' hotspots table
                  if(loop_timings[fct_name_only] == nil) then
                     loop_timings[fct_name_only] = Table:new();
                  end

                  if (callchains ~= nil and type(callchains) == "table")
                  --   and     #callchains > 0)
                  then
                     has_callchains = true;
                  end
                  --Function node
                  fct_node  = Table:new({
                        id = id, colspan = 0, name = fct_name, name_only = fct_name_only, time = time,
                        abs_time = abs_time, parent_info = "",callbacks = Table:new(),
                        loops = Table:new(),fct_only = 0
                  });
                  last_parent = id;
                  id = id + 1;
                  if (has_callchains) then
                     --Add callchain nodes
                     fct_node.callbacks.main = { id = id, colspan = colspan, name = "callstacks",
                                                               level = 1, parent = last_parent,
                                                               is_leaf = "false", expanded = "false"};
                     last_parent_cc = id;
                     id = id + 1;
                     fct_node.callbacks.nodes = Table:new();
                     for callchain,value in pairs (callchains) do
                        --local cc_contrib = callchain_percentage[hostname][pid][tid][fct_name][callchain];
                        local cc_contrib = string.format("%.2f", value);
                        --TODO: remove string.match when callchain will be a clean string
                        callchain  = string.match(callchain,'^%s*(.*)');
                        --cc_contrib = string.match(cc_contrib,'(%d+)%%');
                        fct_node.callbacks.nodes:insert({ id = id, colspan = colspan, name = callchain,
                                                                           level = 2, parent = last_parent_cc,contrib =  cc_contrib,
                                                                           is_leaf = "true", expanded = "false"});
                        id = id + 1;
                     end
                  end

                  --Add artificial "loops" node
                  fct_node.loops.main = Table:new({id = id, colspan = 0, name = "loops",level = 1, aggregate_time = 0,
                                         parent = last_parent, is_leaf = "false", expanded = "false" });
                  last_parent_loop = id;
                  id = id + 1;
                  --TODO loops_id_to_time => tid and time value are string and should be integers/floats
                  --Add loop hierarchy nodes
                  local lid_to_time   = loops_id_to_time[hostname][pid][tid];
                  --local loop_children = loops_child[hostname][pid];
                  local loop_children = loops_child;
                  local data = {id = id};

                  if(outermost_loops[outermost_loops_key] ~= nil) then
                     fct_node.loops.nodes = Table:new();
                     data.loop_nodes = fct_node.loops.nodes;
                     for out_lid in pairs(outermost_loops[outermost_loops_key]) do
                        local loops_hierarchy_time = Table:new();
                        local visited = Table:new();
                        --print('LO '..out_lid);
                        data.outer_aggregate_time = 0;

                        _l_traverse_loop_children(loops_hierarchy_time,loop_children,lid_to_time,out_lid,out_lid,visited);
                        _l_print_loop_children_html(loop_children,lid_to_time,out_lid,0,0,data,2,last_parent_loop,loops_hierarchy_time);
                        --loops_hierarchy_time:tostring();
                        --print('LO '..out_lid..' ['..data.outer_aggregate_time..'] --------');
                        fct_node.loops.main.aggregate_time = fct_node.loops.main.aggregate_time + data.outer_aggregate_time
                     end
                  end

                  --get last id from data tables (used in loops)
                  id = data.id
                  --Add fct only node
                  fct_node.fct_only = {id = id, colspan = 0, name = "function only",level = 1,
                                                   parent = last_parent, isLeaf = "true", expanded = "false"};
                  id = id + 1;
                  --Now we update the table content
                  --############################
                  --Second pass: update fileds #
                  --############################
                  if((fct_node.callbacks.nodes ~= nil and #fct_node.callbacks.nodes > 0) or
                        (fct_node.loops.nodes ~= nil and #fct_node.loops.nodes > 0))
                  then
                     fct_node.parent_info = ',level:"0", parent:"null",  isLeaf:false, expanded:false';
                  else
                     --fct_node.name = "     "..name;--because no leaf, to be aligned with the others
                     fct_node.parent_info = ',parent:"null"';
                  end
                  --print html
                  --Function node
                  grid_data = grid_data..
                     '    { id:"'..fct_node.id..'", colspan:0,name:"'..fct_node.name..'",'..
                     'time:"'..fct_node.time..'", abs_time:"'..fct_node.abs_time..'"'..
                     fct_node.parent_info..'},'.."\n";
                  --Callchain nodes
                  ----main
                  if(fct_node.callbacks.nodes ~= nil and #fct_node.callbacks.nodes > 0) then
                     local node = fct_node.callbacks.main;

                     grid_data = grid_data..
                        '    { id:"'..node.id..'", colspan:'..node.colspan..',name:"callstacks",level:"1",'..
                        'parent:"'..node.parent..'", isLeaf:'..node.is_leaf..', expanded:'..node.expanded..' },'.."\n";
                     ----nodes
                     for _,node in ipairs(fct_node.callbacks.nodes) do
                        grid_data = grid_data..
                           '    { id:"'..node.id..'", colspan:0,name:"'..node.name..'",time:'..node.contrib..','..
                           'level:"2", parent:"'..node.parent..'", isLeaf:'..node.is_leaf..', expanded:'..node.expanded..' },'.."\n";
                     end
                  end
                  --Loop nodes
                  ----main
                  if(fct_node.loops.nodes ~= nil and #fct_node.loops.nodes > 0 and 
                     is_library[hostname][pid][fct_name_only] ~= true ) then
                     local node = fct_node.loops.main;
                     local ltf = nil
                     ltf = loop_timings[fct_name_only];
                     
                     if(ltf.main == nil) then
                        ltf.main = fct_node.loops.main:clone();
                        ltf.main.stat_time = Table:new();
                        ltf.main.parent = nil;
                     end
                     ltf.main.stat_time:insert(node.aggregate_time);
                     grid_data = grid_data..
                        '    { id:"'..node.id..'", colspan:'..node.colspan..
                        ',name:"'..node.name..'",time: '..Math:round(node.aggregate_time,2)..',abs_time : "",level:"'..node.level..
                        '", parent:"'..node.parent..'", isLeaf:'..node.is_leaf..', expanded:'..node.expanded..' },'.."\n";
                     ----nodes                     l
                     for _,loop in ipairs(fct_node.loops.nodes) do
                        if(ltf.loops == nil) then
                           ltf.loops = Table:new();
                        end
                        if(ltf.loops[loop.lid] == nil) then
                           ltf.loops[loop.lid] = Table:clone(loop);
                           ltf.loops[loop.lid].stat_time = Table:new();   
                           ltf.loops[loop.lid].parent = nil; ltf.loops[loop.lid].is_leaf = nil; ltf.loops[loop.lid].level = nil;
                        end
                        ltf.loops[loop.lid].stat_time:insert(loop.time);

                        grid_data = grid_data..
                           '    { id:"'..loop.id..'", colspan:'..loop.colspan..
                           ',name:"Loop '..loop.lid..' - '..loop.src_file..':'..
                              loop.src_line_start..'-'..loop.src_line_end..'",time:'..Math:round(loop.time,2)..',abs_time : "", '..
                           'level:"'..loop.level..'", parent:"'..loop.parent..'",  isLeaf:'..
                           loop.is_leaf..', expanded:'..loop.expanded..' },'.."\n";
                     end
                  end
               end
               --print
               grid_data  = grid_data..'  ];'.."\n";
               gridobject = gridobject..'var griddata'..'_'..gid..' = '..'ggriddata'..'_'..gid.."\n";
               gridobject = gridobject..[[
                  grid]]..'_'..gid..[[ = $("#accgrid_accordion_detail");
                  grid]]..'_'..gid..[[.jqGrid({
                        datatype: "local",
                        data: griddata]]..'_'..gid..[[,
                        colNames:["id","colspan","Name","Excl %Time","Excl Time (s)"],
                        colModel:[
                     {name:'id', index:'id', width:1, hidden:true, key:true},
                     {name:'colspan', index:'colspan', width:1, hidden:true},
                     {name:'name', index:'name', width:600,
                        cellattr: function(rowId, value, rowObject, colModel, arrData) {
                           if(rowObject.colspan > 0){return ' colspan='+rowObject.colspan+';style="white-space: normal;';}
                        }
                     },
                     {name:'time', index:'time', width:80, align:"center",sortable: false},
                     {name:'abs_time', index:'abs_time', width:80, align:"right",sortable: false},
                        ],
                        height:'100%',
                        rowNum: 10000,
                        //pager : "#ptreegrid",
                        sortname: 'id',
                        treeGrid: true,
                        treeGridModel: 'adjacency',
                        treedatatype: "local",
                        ExpandColumn: 'name',
                        /*
                        subGridRowExpanded: function() {
                                 var rowIds = $("#testTable").getDataIDs();
                                 $.each(rowIds, function (index, rowId) {
                                                         $("#testTable").expandSubGridRow(rowId);
                                 });
                        },*/
                        jqGridSubGridRowExpanded: function(rowid,iRow,iCol,e) {
                              //if(iCol == 2 && e.target.innerHTML == "loops")
                                 console.log(rowid,iRow,iCol,e);
                        },
                        /*
                        ondblClickRow: function(rowid,iRow,iCol,e) {
                              //$('#cchart3').style.visibility = "visible";
                              if(iCol == 2){
                                 var gridId = e.currentTarget.id.replace(/accgrid/g,'');
                                 console.log(window["ggriddata"+gridId][iRow-1].name);
                                 document.getElementById('cchart3').style.visibility= 'visible' ;
                              }
                        }*/
                  });
                  // we have to use addJSONData to load the data
                  grid]]..'_'..gid..[[[0].addJSONData({
                        total: 1,
                        page: 1,
                        records: griddata]]..'_'..gid..[[.length,
                        rows: griddata]]..'_'..gid.."\n"..[[
                  });
               ]].."\n";
               local tmp_js_file = io.open(htmloutput_path..'/js/data/gaccgrid_'..gid..'.js',"w");
               if(tmp_js_file == nil) then
                  Message:critical("Cannot write to "..htmloutput_path..'/js/data/ folder');
               end
               --grid_data = string.gsub(grid_data,"\n","");
               --gridobject = string.gsub(gridobject,"\n","");
               tmp_js_file:write(grid_data);
               tmp_js_file:write(gridobject);
               tmp_js_file:close();
            end --if non empty thread result
         end --End of threads table
         --break;
      end -- end of pids
      --break;
   end -- end of hostnames
   --##############################
   --### Prepare hotspots table ###
   --##############################
   local function_timings_sorted_helper = Table:new();
   for fct,info in pairs(function_timings) do
      info.median = lprof:get_median(info.values);
      info.stddev = lprof:get_standard_deviation(info.values);
      
      for _,ndata in ipairs(info.bynode) do
         ndata.median = lprof:get_median(ndata.values);
         ndata.stddev = lprof:get_standard_deviation(ndata.values);
      end

      function_timings_sorted_helper:insert({
         name   = fct;
         median = Math:round(info.median,2);
         stddev = Math:round(info.stddev,2);
         fcts   = info.functions;
         nodes  = info.bynode;
      });
   end
   --Sort function_timings table
   table.sort(function_timings_sorted_helper,
              function(a,b) return a.median > b.median;end);

   --Loops   
   --local function_timings_sorted_helper = Table:new();
   for fct,info in pairs(loop_timings) do
      if(type(info) == "table" and type(info.main) == "table") then
         info.main.median = Math:round(lprof:get_median(info.main.stat_time),2);
         info.main.stddev = Math:round(lprof:get_standard_deviation(info.main.stat_time),2);

         for lid, ldata in pairs(info.loops) do         
            ldata.median = lprof:get_median(ldata.stat_time);
            ldata.stddev = lprof:get_standard_deviation(ldata.stat_time);             
         end
      end
   end
            
   --Add hotspots to grid_data table
   local i = 0;
   grid_data_hf = grid_data_hf..[[var ggriddata_hotspotsfcts = []];

   -- table to concat. strings: much faster + much less memory used (than using original string)
   local grid_data_hf_buf = {} -- usage: table.concat (grid_data_hf_buf)

   --loop_timings:tostring();
   for k,info in pairs(function_timings_sorted_helper) do
      i = i + 1;
      local last_parent_loop = i;
      local parent_info;
      local has_loops;

      has_loops = type(loop_timings[info.name]) == "table" and type(loop_timings[info.name].main) == "table";

      if(has_loops == true) then
         parent_info = 'level:"0", parent:"null",  isLeaf:false, expanded:false';
      else
         parent_info = 'parent:"null"';
      end

      grid_data_hf_buf [#grid_data_hf_buf+1] =

         '    { id:"'..i..'", fctid:"'..k..'",colspan:0,'..
         'name:"'..info.name..'",time:'..info.median..','..
         'stddev:'..info.stddev..','..
         'abs_time : "", '..parent_info..'},'.."\n";
      
      local data = {id = i+2, main_id = i+1};
      
      if(outermost_loops[info.name] ~= nil and loop_timings[info.name]:get_size() > 0) then       
         data.loop_nodes  = Table:new();
         --hf_loops[info.name_only].loops;
         for out_lid in pairs(outermost_loops[info.name]) do
            _l_print_hf_loop_children_html(out_lid,loops_child,loop_timings[info.name].loops,data,0,2,data.main_id);                    
         end
      end   
      i = data.id;
      --print
      if(has_loops == true) then         
         node = loop_timings[info.name].main;
         --node:tostring();
         grid_data_hf_buf [#grid_data_hf_buf+1] =
            '    { id:"'..data.main_id..'", colspan:'..node.colspan..
            ',name:"'..node.name..'",time: '..Math:round(node.median,2)..',abs_time : "",level:"'..node.level..
            '", parent:"'..last_parent_loop..'", isLeaf:'..node.is_leaf..', expanded:'..node.expanded..
            ', stddev: '..Math:round(node.stddev,2)..'},'.."\n";
         for _,loop in ipairs(data.loop_nodes) do
            --loop.time is stat_time function result
            grid_data_hf_buf [#grid_data_hf_buf+1] =
               '    { id:"'..loop.id..'", colspan:'..loop.colspan..

               ',name:"Loop '..loop.lid..' - '..loop.src_file..':'..
               loop.src_line_start..'-'..loop.src_line_end..'",time:'..Math:round(loop.median,2)..',abs_time : "", '..
               'level:"'..loop.level..'", parent:"'..loop.parent..'",  isLeaf:'..
               loop.is_leaf..', expanded:'..loop.expanded..', stddev: '..Math:round(loop.stddev,2)..' },'.."\n";                     
         end
      end
   end
   --Table:tostring(outermost_loops);
   grid_data_hf = grid_data_hf..table.concat (grid_data_hf_buf, "")..'  ];'.."\n";
   --Add hotspots to gridobject table
   gridobject_hf = gridobject_hf..[[
          var griddata_hotspotsfcts = ggriddata_hotspotsfcts;

          grid_hotspotsfcts = $("#accgrid_hotspotsfcts");
          grid_hotspotsfcts.jqGrid({
                gridview: true,/*
                rowattr: function (rd) {   
                  if (Number.isInteger(Number(rd.fctid)) === true) { // verify that the testing is correct in your case
                     return {"class": "context-menu-hf"};
                  }
                },*/             
                datatype: "local",
                data: griddata_hotspotsfcts,
                colNames:["id","colspan","Name","Median Excl %Time","Deviation","fctid"],
                colModel:[
                     {name:'id', index:'id', width:1, sorttype:"int", hidden:true, key:true},
                     {name:'colspan', index:'colspan', width:1, sorttype:"int", hidden:true},
                     {name:'name', index:'name', width:600, sorttype:"text",
                        cellattr: function(rowId, value, rowObject, colModel, arrData) {
                           if(rowObject.colspan > 0){return ' colspan='+rowObject.colspan+';style="white-space: normal;';}
                        }
                     },
                     {name:'time', index:'time', width:120, sorttype:"float", align:"center"},
                     {name:'stddev', index:'stddev', width:80, sorttype:"float", align:"center"},
                     {name:'fctid', index:'fctid', hidden: true},                     
                ],
                height:'100%',/*
                pager:'#paccgrid_hotspotsfcts',
                rowNum: 20,
                rowList: [5, 10, 20, 50, 100],*/
                treeGrid: true,
                treeGridModel: 'adjacency',
                treedatatype: "local",                 
                sortname: 'id',
                ExpandColumn: 'name',
                /*
                ondblClickRow: function(rowid,iRow,iCol,e) {                  
                  if(iCol == 2){
                     load_lb_view(griddata_hotspotsfcts[iRow-1].fctid, false);
                  }
                },*/
                onRightClickRow: function (rowid, iRow, iCol, e) {   
                  let fctid = griddata_hotspotsfcts[iRow-1].fctid;
                  
                  if (Number.isInteger(Number(fctid)) === true){
                     let top  = e.pageY - 160; //- value obtained through calibration (handmade)
                     let left = e.offsetX + 70;//+ value obtained through calibration (handmade)
                     
                     $(".context-menu").finish().toggle(100).    
                     css({
                        top: top + "px",
                        left: left + "px"
                     });
                     //Update menu action                     
                     $(".context-menu li").attr("target-fctid", fctid);
                     
                     e.preventDefault();
                  }
               }
          });
          grid_hotspotsfcts[0].addJSONData({
            total: 1,
            page: 1,
            records: grid_hotspotsfcts.length,
            rows: griddata_hotspotsfcts
          });/*          
          grid_hotspotsfcts.jqGrid('navGrid','#paccgrid_hotspotsfcts',{add:false,edit:false,del:false,search:true,refresh:true},
                                    {},{},{},{multipleSearch:true, multipleGroup:true, showQuery: true});*/
          $('[data-popup-close]').on('click', function(e)  {
            var targeted_popup_class = jQuery(this).attr('data-popup-close');
            $('[data-popup="popup-1"]').fadeOut(350);
            $('#popup_disp_inner').empty();

            e.preventDefault();
          });                                    
          $(".context-menu li").click(function(){  
            target_fctid = $(this).attr("target-fctid");
            action       = $(this).attr("data-action");

            switch(action) {            
               case "lb": load_lb_view(target_fctid, false); 
                  break;
               case "lbs": load_lb_view(target_fctid, true);
                  break;
               case "node": 
                  let file = "";
                  $('[data-popup="popup-1"]').fadeIn(350);                       
                  
                  file = 'js/data/heatmap_hotspotsfcts_'+ target_fctid +'.js';                  
                  $.getScript(file)
                  .done(function( script, textStatus ) {
                     //OK
                  }) 
                  .fail(function( jqxhr, settings, exception ) {                     
                  window.alert( "Error: an error occured while loading the node view");  
                     //console.log(exception);
                  });                    
                  break;                  
            }

            $(".context-menu").hide(100);
          });                                    
   ]];
   --End of - Prepare hotspots table

   --### CHARTS - Will generate 3 chart file per hotspot function
   gen_html_charts(function_timings_sorted_helper, htmloutput_path);
   gen_html_heatmaps(function_timings_sorted_helper, htmloutput_path);
   --###############################
   --### Start writing HTML page ###
   --###############################
   --Header
   htmloutput_file:write(lprof.display.html.html_output_get_header().."\n");

   gridscriptstart =    [[
      <script type="text/javascript">
      var v_colspan = ]]..colspan..[[;
   ]];
   htmloutput_file:write(gridscriptstart.."\n");

   --htmloutput_file:write(grid_data);
   htmloutput_file:write(grid_data_hf);
   grid_data_start = [[
      function load_lb_view(fctid, sorted){
         let file = "";
         $('[data-popup="popup-1"]').fadeIn(350);                       
         
         if(sorted === true) {         
            file = 'js/data/chart_hotspotsfcts_'+ fctid +'_sorted.js';   
         } else {
            file = 'js/data/chart_hotspotsfcts_'+ fctid +'.js';
         }
         $.getScript(file)
         .done(function( script, textStatus ) {
            //OK
         }) 
         .fail(function( jqxhr, settings, exception ) {                     
         window.alert( "Error: an error occured while loading the load balancing chart");                     
         });         
      }
      
      //<![CDATA[
      $(function(){
   ]];

   htmloutput_file:write(grid_data_start);
   --htmloutput_file:write(gridobject);--Generated in previous main loop
   htmloutput_file:write(gridobject_hf);
   gridscriptend = [[
                  });
            //] ]>
            </script>
      </head>
      <body>
         ]];
   htmloutput_file:write(gridscriptend.."\n");
   htmlbodyhead = [[
    <div id="page_title">Performance Evaluation - Profiling results</div>
    <script type="text/javascript">
    $(document).ready(function(){
        //$("#accordion").draggable({ cursorAt: { top: -10, left: -10 } });
        $('#accordion_cat').resizable();
        $('#accordion_hotspotsfcts').resizable();
        $('#accordion_detail').resizable();
    });
    </script>    
   ]];
   htmloutput_file:write(htmlbodyhead.."\n");
   --### CATEGORIES
   -- Select the entry in categories that correspond to average over nodes
   -- Should be last entry but want to be sure...
   if (categories [#categories][1] == "AVERAGE") then
      categories = categories [#categories];
   else -- should never happen
      local found = false
      for i,v in ipairs (categories) do
         if (v[1] == "AVERAGE") then
            categories = categories [i]
            found = true
            break
         end
      end
      if (not found) then
         Message:warn ("Cannot find categories average values. Using first node")
         categories = categories [1]
      end
   end
   htmlbody = gen_html_categories(categories, binary_name);
   htmloutput_file:write(htmlbody.."\n");
   --### HOTSPOTS
   local htmltext = [[      
   <div id="accordion_hotspotsfcts">
      <div class="group" id="gaccgrid_hotspotsfcts">
         <h3>Hotspots - Functions</h3>
         <div>
               <table id="accgrid_hotspotsfcts"><tr><td></td></tr></table>
               <div id="paccgrid_hotspotsfcts"></div>
         </div>
      </div>
   </div>
   <div id="accordion_detail" style="visibility: hidden">
   ]];
   htmloutput_file:write(htmltext.."\n");
   local htmltext = [[
		<div class="group" id="accordion_detail_container">
		  <h3>&nbsp;</h3>
		  <div>
               <table id="accgrid_accordion_detail"><tr><td></td></tr></table>
               <div id="paccgrid_accordion_detail"></div>
		  </div>
		</div>
   ]];
   htmloutput_file:write(htmltext.."\n");
      htmlbodytail = [[
            </div>
            <div class="popup" data-popup="popup-1">
               <div class="popup-inner">
                  <!--<p><a data-popup-close="popup-1" href="#">Close</a></p>-->
                  <a class="popup-close" data-popup-close="popup-1" href="#">x</a>
                  <!--<h2>Title</h2>
                  <p>Text</p>-->
                  <div id="popup_disp_inner"></div>
               </div>
            </div> 
            <ul class='context-menu'>
               <li data-action="lb">Load balancing view</li>
               <li data-action="lbs">Sorted Load balancing view</li>
               <li data-action="node">Node view</li>
            </ul>               
      </body>
      </html>
         ]];
   htmloutput_file:write(htmlbodytail.."\n");
   Message:info("Done. Open "..htmloutput_path..htmloutput_filename.." in your browser to view GUI.");
   htmloutput_file:close();
end
