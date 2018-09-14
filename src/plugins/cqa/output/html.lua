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

--- Module cqa.output.html
-- Defines the print_reports function used to print CQA reports as a html page
module ("cqa.output.html", package.seeall)

require "cqa.consts";
require "cqa.output.txt_html_shared"
require "cqa.api.reports_shared";

require "cqa.api.reports";

-- Returns reports dealing with a given binary loop
local function get_reports (cqa_results)
   local cqa_context = cqa_results.common.context;

   if (cqa_context.for_host) then
      -- cqa_context.freq = tonumber (
      --   string.match (project:get_cpu_frequency(), "(%d+%.%d+)%s*GHz")
      --);
   end

   local reports = cqa:get_reports (cqa_results, cqa_context.tags);

   return reports;
end

-- Prints the static analysis report for a source loop
local function prepare_src_data (last_line, src_loop, cqa_results, src_file_path)
   local src_data_line = Table:new({info = Table:new(), bin = Table:new()});

   if (type(src_file_path) == "string") then
      if (string.len (src_file_path) > 30) then
	 src_data_line.info.src_ending = "Source loop ending at line "..last_line.." in ..."..string.sub (src_file_path, -30);
      else
	 src_data_line.info.src_ending = "Source loop ending at line "..last_line.." in "..src_file_path;
      end
   else
      src_data_line.info.src_ending = "Source loop ending at line "..last_line;
   end
   src_data_line.info.bin_loops = src_loop["loop IDs"];
   --"Composition and unrolling

   -- Prints composition
   local compo = "It is composed of the ";

   if (#src_loop ["loop IDs"] == 1) then
      compo = compo .."loop "..src_loop["loop IDs"][1];
   else
      compo = compo .."following loops [ID (first-last source line)]:\n";

      table.sort (src_loop ["loop IDs"]); -- makes output determinist

      for _,loop in ipairs (src_loop ["loops"]) do
         local src_min, src_max = loop:get_src_lines();
         compo = compo .. string.format (" - %d (%d-%d)\n", loop:get_id(),
                                         src_min, src_max);
      end
   end -- if one bin loop in the src loop
   src_data_line.info.compo = compo;
   -- Add composition to info

   for _,id in pairs (src_loop ["loop IDs"]) do
      src_data_line.bin[id] = get_reports (cqa_results [id]);
   end
   return src_data_line;
end

-- Tests if a list of binary loops contains at least one loop related to a given source loop
local function is_in_src_loop (bin_loops, src_loop)
   for _,bin_loop in pairs (bin_loops) do
      for _,bin_loop_in_src_loop in pairs (src_loop ["loop IDs"]) do
         if (bin_loop == bin_loop_in_src_loop) then
            return true;
         end
      end
   end

   return false;
end

local function print_loop_reports(reports,html_file,cf_level)
   if(type(reports) ~= "table") then
      return;
   end

   html_file:write([[
      <table class="bin_loop_reports">
        ]]);
   Table:new(reports);
   if(reports:get_size() == 0) then
      html_file:write([[
                         <tr><td style="font-size: 10pt;"><b>No reports for this confidence level</b></td></tr>
                ]]);
   end

   for _,report in ipairs(reports) do
      html_file:write([[
                         <tr>
             <td>
                <table class="bin_loop_report ui-widget-content">
                ]]);
      if(type(report.title) == "string" and report.title ~= "") then
         report.title = string.gsub(report.title,"\n","<br/>");
         html_file:write([[
                     <tr><th class="ui-widget-header">]]..report.title..[[</th></tr>
                  ]]);
      end
      if(type(report.txt) == "string" and report.txt ~= "") then
         report.txt = string.gsub(report.txt,"\n","<br/>");
         html_file:write([[
                        <tr><td>]]..report.txt..[[</td></tr>
                     ]]);
      end
      if(type(report.details) == "string" and report.details ~= "") then
         report.details = string.gsub(report.details,"\n","<br/>");
         html_file:write([[
                     <tr><td><i>]]..report.details..[[</i></td></tr>
                  ]]);
      end
      if(type(report.workaround) == "string" and report.workaround ~= "") then
         report.workaround = string.gsub(report.workaround,"\n","<br/>");
         html_file:write([[
                     <tr><td><b>Proposed solution(s):</b></td></tr>
                     <tr><td>]]..report.workaround..[[</td></tr>
                  ]]);
      end
      html_file:write([[
                </table>
             <td>
                         <tr>
                ]]);
   end
   html_file:write([[
      </table>
         ]]);
end

local function print_path(path,html_file)
   --print(path.header,path.gain,path.potential,path.hints,path.expert);
   html_file:write([[
      <div class="bin_loop_reports">
         ]]);
   html_file:write([[
      <table class="bin_loop_reports_header">
         ]]);
   for _,header in ipairs(path.header or {}) do
      --TODO remove String:get_tokenized_table since it should be done in CQA
      headers = String:get_tokenized_table(header,"\n");
      for _,h in ipairs(headers) do
         --TODO: remove this condition since it should be fixed in CQA
         if(string.match(h,"^In the binary file, the address of the loop is.*") == nil) then
            html_file:write('<tr><td><b>'..h..'</b></td></tr>'.."\n");
         end
      end
   end
   html_file:write([[
      </table>
         ]]);

   html_file:write([[
                                <div class="cf_level_tabs">
                                                <ul>
                                                        <li><a href="#cf_level_tabs-1" style="padding: 0.1em 0.5em;font-size: 11pt;">Gain</a></li>
                                                        <li><a href="#cf_level_tabs-2" style="padding: 0.1em 0.5em;font-size: 11pt;">Potential gain</a></li>
                                                        <li><a href="#cf_level_tabs-3" style="padding: 0.1em 0.5em;font-size: 11pt;">Hints</a></li>
                                                        <li><a href="#cf_level_tabs-4" style="padding: 0.1em 0.5em;font-size: 11pt;">Experts only</a></li>
                                                </ul>
                                                <div id="cf_level_tabs-1">
   ]]);
   print_loop_reports(path.gain,html_file,'gain');
   html_file:write([[
                                                        </div>
                                                        <div id="cf_level_tabs-2">
   ]]);
   print_loop_reports(path.potential,html_file,'potential');
   html_file:write([[
                                                        </div>
                                                        <div id="cf_level_tabs-3">
   ]]);
   print_loop_reports(path.hint,html_file,'hint');
   html_file:write([[
                                                        </div>
                                                        <div id="cf_level_tabs-4">
   ]]);
   print_loop_reports(path.expert,html_file,'expert');
   html_file:write([[
                                                        </div>
         </div>
      </div>
         ]]);
end

local function print_reports_blocks(reports, html_file)
   if (type (reports.common.header) == "table") then
      html_file:write([[
					<table class="bin_loop_reports_header">
				  ]]);

      for _,header in ipairs(reports.common.header) do
         header = string.gsub (header, "\n", "<br>");
	 html_file:write('<tr><td><b>'..header..'</b></td></tr>'.."\n");
      end

      html_file:write([[
					</table>
				     ]]);
   end

   if(#reports.paths == 1) then
      print_path(reports.paths[1],html_file);

   elseif(#reports.paths > 1) then
      html_file:write( [[
                                                                <div id="accordion_path_lvl">
                                                ]]);
      for name,path in pairs(reports.paths) do
	 html_file:write( [[
                                                                                         <h3>]].."path #"..name..[[</h3>
                                                                                         <div>
                                                                ]]);

	 print_path(path,html_file);

	 html_file:write( [[
                                                                                   </div>
                                                                ]]);
      end
      html_file:write( [[
                                                                </div>
                                                ]]);
   end
end

local function print_reports_fct_loops(reports,html_file)
   for fct,data_line in pairs(reports) do
      --html_file:write("In Function "..fct..'<br>');
      for _,line in ipairs(data_line.src) do
         --if contains at least one bin loop
         if(line.bin:get_size() > 0) then
            html_file:write([[
                                                <h3>]]..line.info.src_ending..[[</h3>
                                                <div>
                                ]]);
            --Source info
            if (type (line.info.compo) == "string") then
	       html_file:write([[
				     <table class="bin_loop_reports_header">
			       ]]);

	       html_file:write('<tr><td><b>'..string.gsub (line.info.compo, "\n", "<br>")..'</b></td></tr>');

	       html_file:write([[
				     </table>
			       ]]);
            end
            --Source bin loops
            html_file:write( [[
                                  <div id="accordion_bin_lvl">
                                  ]]);
            for lid,reports in pairs(line.bin) do
               html_file:write( [[
                                                         <h3>MAQAO binary loop id: ]]..lid..[[</h3>
                                                         <div>
                                         ]]);

	       print_reports_blocks (reports, html_file);

               html_file:write( [[
                                                  </div>
                                        ]]);
            end
            html_file:write( [[
                                    </div>
                                  </div>
                                ]]);
         end
      end
      --Create a special source container
      if(data_line.bin:get_size() > 0) then
         html_file:write([[
                                  <h3>Binary loops</h3>
                                  <div>
                  ]]);
         for lid,reports in pairs(data_line.bin) do
            html_file:write( [[
                                           <div id="accordion_bin_lvl">
                                                    <h3>MAQAO binary loop id: ]]..lid..[[</h3>
                                                    <div>
                                ]]);
            print_reports_blocks (reports, html_file);
            html_file:write( [[
                                                   </div>
                                      </div>
                                ]]);
         end
         html_file:write( [[
                            </div>
                  ]]);
      end

      --Create a special source container
      if(data_line.failed:get_size() > 0) then
         html_file:write([[
                                  <h3>Binary loops which analysis has failed</h3>
                                  <div>
                  ]]);
         for lid,reports in pairs(data_line.failed) do
            html_file:write( [[
                                        <b>MAQAO binary loop id: ]]..lid..[[</b>
                                ]]);
         end
         html_file:write( [[
                            </div>
                  ]]);
      end
      --TODO handle rem loops
   end--end accordion_src_lvl
end

local function print_reports_fct(reports_for_all_fcts,html_file)
   for fct_name,reports in pairs(reports_for_all_fcts) do
      html_file:write( [[                                                                  
                                  <h3>function name: ]]..fct_name..[[</h3>
                                  <div>
      ]]);

      print_reports_blocks (reports, html_file);

      html_file:write( [[
                                  </div>
      ]]);
   end
end

--Static exit handler
function print_reports_exit(html_reports,op,mode,header)
   --html_reports:tostring();
   require "cqa.output.html_aux";
   local html_file = nil;
   local htmlheadertail,htmlbodyhead,htmlbodytail;

   op = op .. '/';
   html_file = io.open (op.."index.html", "w");
   if (html_file == nil) then
      Message:display(cqa.consts.Errors["CANNOT_WRITE_HTML"], op);
   end
   htmlheadertail = [[
      </head>
   ]];
   htmlbodyhead = [[
      <body>
      <div id="page_title">Code quality analysis</div>
   ]];
   htmlbodytail = [[
      </body>
   </html>
   ]];
   --header
   html_file:write(cqa.output.html_aux.html_output_get_header());
   html_file:write(htmlheadertail.."\n");
   --body start
   html_file:write(htmlbodyhead.."\n");
   --body content
   if (header ~= nil) then
      html_file:write( [[<font size="3" color="white">]] .. "<br>"..table.concat (header, "\n") .. [[</font>]] )
   end
   html_file:write( [[
         <div id="accordion_src_lvl">
   ]]);
   if (mode == "fct-loops" or mode == "loop") then
      print_reports_fct_loops(html_reports,html_file);
   elseif (mode == "fct-body" or mode == "path") then
      print_reports_fct(html_reports,html_file);
   end
   html_file:write( [[
         </div>
   ]]); -- /div id="accordion_src_lvl"
   --body end
   html_file:write(htmlbodytail.."\n");
   --Generate needed UI files
   cqa.output.html_aux.html_generate_helper_files(op);
   html_file:close();
end

--- Prints on the standard output static analysis results
-- @param fct a function
-- @param cqa_results static analysis results for innermost loops of fct
-- @param requested_loops requested binary loops (list of identifiers)
function print_reports (fct, cqa_results, html_reports, requested_loops)
   local params = {}

   params.print_fct_name = function ()
      if (html_reports [params.fct_name] == nil) then
         html_reports [params.fct_name] = { info = {}, src = Table:new(), bin = Table:new(), failed = Table:new() }
      end
   end

   params.print_debug_data_string = function (crc)
      html_reports [params.fct_name].info.dbg = cqa.api.reports.get_debug_data_string (crc)
   end

   params.print_code_specialization = function (str)
      if (str) then html_reports [params.fct_name].info.spe = str end
   end

   params.print_no_loops = function ()
      html_reports [params.fct_name] = get_reports (cqa_results);
   end

   params.print_src_file_path = function (src_file_path)
      html_reports [params.fct_name].info.file_path = src_file_path;
   end

   params.print_src = function (last_line, src_loop, cqa_results, src_file_path)
      local src_data_line = prepare_src_data (last_line, src_loop, cqa_results, src_file_path);
      html_reports [params.fct_name].src:insert (src_data_line);
   end

   params.print_bin_in_src = function (cqa_results, id, is_already_printed)
      local reports = html_reports [params.fct_name].bin

      reports[id] = get_reports (cqa_results[id]);
   end

   params.print_bin_loops = function (bin_loops, is_already_printed)
      -- Binary loops which have no source context.
      if (next (bin_loops) ~= nil) then
         --print_section ("Binary loops in the function named "..fct_name, 2);

         for _,v in pairs (bin_loops) do
            if (cqa_results[v] ~= nil) then
               --print_section ("Binary loop #"..v, 3);
               html_reports [params.fct_name].bin[v] = get_reports (cqa_results[v]);
            end
         end
      end
   end

   params.tags = {
      list_start = "<ul>",
      list_stop = "</ul>",
      list_elem_start = "<li>",
      list_elem_stop = "</li>",

      ordered_list_start = "<ol>",
      ordered_list_stop = "</ol>",
      ordered_list_elem_start = "<li>",
      ordered_list_elem_stop = "</li>",

      lt = "&lt",
      gt = "&gt",
      amp = "&amp",

      table_start = "<table>",
      table_row_start = "<tr>",
      table_header_start = "<th>",
      table_header_stop  = "</th>",
      table_elem_start = "<td>",
      table_elem_stop  = "</td>",
      table_row_stop  = "</tr>",
      table_stop  = "</table>",
   }

   cqa.output.txt_html_shared.print_reports (fct, cqa_results, requested_loops, params)
end
