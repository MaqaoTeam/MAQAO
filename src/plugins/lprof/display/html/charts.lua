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

--Generates all the bar charts that show how a function behaved on all the involved threads (all nodes)
--Both normal and sorted charts are generated
function gen_html_charts(function_timings, htmloutput_path)
   local function cmp_fct (a,b) return a.time > b.time end

   for k,v in ipairs(function_timings) do
      local tmp_js_file    = io.open(htmloutput_path..'/js/data/chart_hotspotsfcts_'..k..'.js',"w");
      local tmp_js_file_s  = io.open(htmloutput_path..'/js/data/chart_hotspotsfcts_'..k..'_sorted.js',"w");

      if(tmp_js_file == nil) then
         Message:critical("Cannot write to "..htmloutput_path..'/js/data/ folder');
      end
      --The same javascript skeleton code is used to generate both charts. 
      --Only data varies an is set with string.format
      local htmlbodycharts = [[
         $(document).ready(function(){
            var ticks  = [%s];//titles.
            var gids   = [%s];//gids
            var seriex = [%s];//seriex
            var seriey = [%s];//seriey
            var last_id;
            var data     = seriey;
            var ordinals = seriex;

            var widthgl  = 900;
            var heightgl = 300;
            var widthin  = 900;
            var heightin = 320;            

            let margin = {
                  top: 10,
                  right: 30,
                  bottom: 20,
                  left: 50
            },
            width = widthgl - margin.left - margin.right,
            height = heightgl - margin.top - margin.bottom,
            radius = (Math.min(width, height) / 2) - 10,
            node;

            const svg = d3.select('#popup_disp_inner')
                        .append('svg')
                        .attr('width', widthin)
                        .attr('height', heightin)
                        .append('g')
                        .attr('transform', `translate(${margin.left}, ${margin.top})`)
                        .call(                           
                           d3.zoom()
                              .translateExtent([ [0,0], [width, height] ])
                              .extent([ [0, 0], [width, height] ])
                              .on('zoom', zoom)                
                        ).on("dblclick.zoom", null);//disable predefined dblclick event

            // the scale
            let x = d3.scaleLinear().range([0, width])
            //let x = d3.scaleBand().rangeRound([0, width]);
            let y = d3.scaleLinear().range([height, 0])
            //var y = d3.scaleOrdinal().rangeRoundBands([0, height], .1);
               
            let color = d3.scaleOrdinal(d3.schemeCategory10)
            let xScale = x.domain([-1, ordinals.length])
            let yScale = y.domain([0, d3.max(data, function(d){return d.value})])
            // for the width of rect
            let xBand = d3.scaleBand().domain(d3.range(-1, ordinals.length)).range([0, width])

            // zoomable rect
            svg.append('rect')
               .attr('class', 'zoom-panel')
               .attr('width', width)
               .attr('height', height)

            // x axis
            let xAxis = svg.append('g')
               .attr('class', 'xAxis')
               .attr('transform', `translate(0, ${height})`)
               .call(
               d3.axisBottom(xScale).tickFormat((d, e) => {
                  return ordinals[d]
               })
               );
            svg.append("text")             
                  .attr("transform",
                        "translate(" + (width/2) + " ," + 
                                       (height + margin.top + 30) + ")")
                  .style("text-anchor", "middle")
                  .text("MAQAO thread rank");                  
               
            // y axis
            let yAxis = svg.append('g')
                           .attr('class', 'y axis')
                           .call(d3.axisLeft(yScale));                  
            svg.append("text")
                  .attr("transform", "rotate(-90)")
                  .attr("y", 0 - margin.left)
                  .attr("x",0 - (height / 2))
                  .attr("dy", "1em")
                  .style("text-anchor", "middle")
                  .text("%% of Time");                            

            let bars = svg.append('g')
                        .attr('clip-path','url(#my-clip-path)')
                        .selectAll('.bar')
                        .data(data)
                        .enter()
                        .append('rect')
                        .attr('class', 'bar')
                        .attr('x', function(d, i){
                        return xScale(i) - xBand.bandwidth()*0.9/2
                        })
                        .attr('y', function(d, i){
                        return yScale(d.value)
                        })
                        .attr('width', xBand.bandwidth()*0.9)
                        .attr('height', function(d){
                        return height - yScale(d.value)
                        })

            let defs = svg.append('defs')

            // use clipPath
            defs.append('clipPath')
               .attr('id', 'my-clip-path')
               .append('rect')
               .attr('width', width)
               .attr('height', height)

            let hideTicksWithoutLabel = function() {
               d3.selectAll('.xAxis .tick text').each(function(d){
                  if(this.innerHTML === '') {
                     this.parentNode.style.display = 'none';
                  }
               })
            }

            function zoom() {
               if (d3.event.transform.k < 1) {
                     d3.event.transform.k = 1;
                     return;
               }

               xAxis.call(
                  d3.axisBottom(d3.event.transform.rescaleX(xScale)).tickFormat((d, e, target) => {
                     // has bug when the scale is too big
                     if (Math.floor(d) === d3.format(".1f")(d)) return ordinals[Math.floor(d)]
                        return ordinals[d];
                  })
               )

               hideTicksWithoutLabel()

               // the bars transform
               bars.attr("transform", "translate(" + d3.event.transform.x+",0)scale(" + d3.event.transform.k + ",1)")
            }

            //Events

            svg.on("dblclick", function() {
               var gid = gids[last_id];
               var gid_lbl = ticks[last_id];

               $.getScript('js/data/gaccgrid_'+ gid + '.js')
               .done(function( script, textStatus ) {
                  //Load
                  var acc_d = $('#accordion_detail');
                  var acc_dc = $('#accordion_detail_container' +' h3');

                  acc_d.css("visibility","visible");
                  acc_dc.html(gid_lbl);
                  //console.log(acc_dc.attr("aria-selected"));
                  if(acc_dc.attr("aria-selected") == "false"){
                     acc_dc.click();
                  }
                  else
                  {//easiest way to simulate a click that opens the panel because we want automatic scrolling
                     acc_dc.click();
                     acc_dc.click();
                  }
                  var targeted_popup_class = jQuery(this).attr('data-popup-close');
                  $('[data-popup="popup-1"]').fadeOut(350);
                  $('#popup_disp_inner').empty();
               })
               .fail(function( jqxhr, settings, exception ) {
                  window.alert( "Error: an error occured while loading a node profile ");
               });
               //console.log("Opening details for id", last_id);
            });

            b = svg.selectAll(".bar");
            b.attr("y", function(d) {
                  //console.log(d,d.value,y(d.value));
                  return y(d.value);
            })
            .attr("fill", function(d, i) {
                        return color(i);
            })
            .attr("id", function(d, i) {
                     return i;
            })
            .on("mouseover", function() {
                        //d3.select(this).attr("fill", "red");
                        //console.log("On ",this,this.__data__.city);  
                        last_id = this.id;
                  
            }).on("mouseout", function(d, i) {
                        /*d3.select(this).attr("fill", function() {
                           //console.log("" + color(this.id) + "");
                           return "" + color(this.id) + "";
                        });*/
            });
            b.append("title")
               .text(function(d) {
                     return d.letter;
            });         
         });
      ]];

      local t_seriex = {};
      local t_seriey = {};
      local t_gids   = {};
      local t_titles = {};
      --Fill data for normal chart
      for idx,s in ipairs(v.fcts) do
         t_seriey [idx] = "{value: "..s.time..'},';
         t_seriex [idx] = "'T"..idx.."',";
         t_gids   [idx] = "'"..s.gid.."',";
         t_titles [idx] = "'"..s.title.."',";
      end      
      local seriex = table.concat (t_seriex)
      local seriey = table.concat (t_seriey)
      local gids   = table.concat (t_gids)
      local titles = table.concat (t_titles)
      tmp_js_file:write(string.format(htmlbodycharts, titles, gids, seriex, seriey));
      tmp_js_file:close();
      
      --Fill data for sorted chart
      for k in ipairs (t_seriex) do t_seriex [k] = nil end
      for k in ipairs (t_seriey) do t_seriey [k] = nil end
      for k in ipairs (t_gids  ) do t_gids   [k] = nil end
      for k in ipairs (t_titles) do t_titles [k] = nil end
      table.sort(v.fcts, cmp_fct);     
      for idx,s in ipairs(v.fcts) do
         t_seriey [idx] = "{value: "..s.time..'},';
         t_seriex [idx] = "'T"..idx.."',";
         t_gids   [idx] = "'"..s.gid.."',";
         t_titles [idx] = "'"..s.title.."',";
      end  
      seriex = table.concat (t_seriex)
      seriey = table.concat (t_seriey)
      gids   = table.concat (t_gids)
      titles = table.concat (t_titles)
      tmp_js_file_s:write(string.format(htmlbodycharts, titles, gids, seriex, seriey));
      tmp_js_file_s:close();
   end
end

--Generates a node heatmap for each function to show the time behaviour on each node
function gen_html_heatmaps(function_timings, htmloutput_path)
   for k,v in ipairs(function_timings) do
      local xValues, yValues, zValues;
      local nodes_per_line = 5;
      local gids     = "";
      local titles   = "";
      local values   = 0;
      local htmlbody = "";
      local tmp_js_file = io.open(htmloutput_path..'/js/data/heatmap_hotspotsfcts_'..k..'.js',"w");

      if(tmp_js_file == nil) then
         Message:critical("Cannot write to "..htmloutput_path..'/js/data/ folder');
      end
      --[[ Basically a heatmap is a matrix
           x and y tables provide x and y labels while z provides actual values
           As a consequence z has y tables containing x values (2D array)  
           In our case we defined a structure with 'n' nodes per line and then number nodes based on that
           The heatmap is configured to hide x and y values. --]]
      xValues = "["; 
      yValues = "[0,";
      zValues = "[";       
      
      if(#v.nodes <= nodes_per_line) then
         nb_elems = #v.nodes;
      else
         nb_elems = nodes_per_line;
      end
      nb_lines = math.ceil(#v.nodes / nodes_per_line) - 1;--Because 0 is hardcoded 

      for i=1,nb_elems,1 do
         xValues = xValues..i..",";
      end   
      xValues = xValues.."]";
      
      for i=1,nb_lines,1 do
            yValues = yValues..( i * nodes_per_line + 1)..","; 
      end      
      yValues = yValues.."]";
      
      local nbwritten = 1;
      local nbnodes   = 1;
      for nodeid,ndata in ipairs(v.nodes) do
         if(nbwritten == 1) then
            zValues = zValues.."[";
         end
         if(nbwritten <= nodes_per_line) then
            zValues = zValues..ndata.median..","            
            --zValues = zValues..ndata.median.." / "..ndata.stddev..","
         end
         if(nbwritten == nodes_per_line or nbnodes == #v.nodes) then
            zValues = zValues.."],";
            nbwritten = 1;
         else
            nbwritten = nbwritten + 1;
         end  
         nbnodes = nbnodes + 1;
      end
      zValues = zValues.."]";
            
      htmlbody = htmlbody..[[
      $(document).ready(function(){
         var xValues = ]]..xValues..[[; 
         var yValues = ]]..yValues..[[; 
         var zValues = ]]..zValues..[[;                

         var data = [{
            hoverinfo: 'none',
            x: xValues,
            y: yValues,
            z: zValues,
            type: 'heatmap',
            colorscale: 'Portland',
            showscale: true,
            xgap: 5,
            ygap: 5
         }];

         var layout = {
            //title: 'Nodes',
            annotations: [],
            xaxis: {
               ticks: '',
               side: 'top',
               //Hide ticks
               zeroline: false,
               showline: false,
               showticklabels: false,
               showgrid: false
            },
            yaxis: {
               zeroline: false,
               showline: false,
               showticklabels: false,
               showgrid: false,
               ticks: '',
               ticksuffix: ' ',
               //width: 700,
               //height: 700,
               autosize: false,
               autorange: "reversed"
            }
         };

         for ( var i = 0; i < yValues.length; i++ ) {
            for ( var j = 0; j < xValues.length; j++ ) {
               var currentValue = zValues[i][j];
               if (currentValue != 0.0) {
                  var textColor = 'white';
               }else{
                  var textColor = 'black';
               }
               var result = {
                  xref: 'x1',
                  yref: 'y1',
                  x: xValues[j],
                  y: yValues[i],
                  text: zValues[i][j],
                  font: {
                  family: 'Arial',
                  size: 12,
                  color: 'rgb(50, 171, 96)'
                  },
                  showarrow: false,
                  font: {
                  color: textColor
                  }
               };
               layout.annotations.push(result);
            }
         }
         Plotly.newPlot('popup_disp_inner', data, layout);
        /* For future implementations that would need to retrieve node position when clicked
         var myPlot = document.getElementById('popup_disp_inner');
         myPlot.on('plotly_click', function(data){      
            console.log(data.points[0].x,data.points[0].y,data.points[0].z);
         });*/ 
      });
      ]];
      tmp_js_file:write(htmlbody);
      tmp_js_file:close();      
   end
end
