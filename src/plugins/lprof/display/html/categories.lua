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

--Generates the categorization pie chart
function gen_html_categories(categories, binary_name)
   local html_categories = "";
   html_categories = html_categories.."['Application',"..categories[lprof.BIN_CATEGORY].."],";
   html_categories = html_categories.."['MPI',"..categories[lprof.MPI_CATEGORY].."],";
   html_categories = html_categories.."['OpenMP',"..categories[lprof.OMP_CATEGORY].."],";
   html_categories = html_categories.."['Math',"..categories[lprof.MATH_CATEGORY].."],";
   html_categories = html_categories.."['System',"..categories[lprof.SYSTEM_CATEGORY].."],";
   html_categories = html_categories.."['Pthread',"..categories[lprof.PTHREAD_CATEGORY].."],";
   html_categories = html_categories.."['IO',"..categories[lprof.IO_CATEGORY].."],";
   html_categories = html_categories.."['String manipulation',"..categories[lprof.STRING_CATEGORY].."],";
   html_categories = html_categories.."['Memory operations',"..categories[lprof.MEM_CATEGORY].."],";
   html_categories = html_categories.."['Others',"..categories[lprof.OTHERS_CATEGORY].."],";

   htmlbody = [[
    <div id="accordion_cat">]].."\n"..[[
        <div class="group" id="gaccgrid_cat">
            <h3>Time categorization - ]]..binary_name..[[</h3>
            <div style="padding-bottom: 0;padding-top: 0;">
                <div id="chart_cat" style="height:320px; width:500px;margin: 0 auto"></div>
                  <script class="code" type="text/javascript">
                  $(document).ready(function(){
                    var data = [
                      ]]..html_categories..[[
                    ];
                    var plot1 = jQuery.jqplot ('chart_cat', [data],
                      {
                       grid: {
                           drawBorder: false,
                           drawGridlines: false,
                           background: '#ffffff',
                           shadow:false
                        },
                        seriesDefaults: {
                          // Make this a pie chart.
                          renderer: jQuery.jqplot.PieRenderer,
                          rendererOptions: {
                            // Put data labels on the pie slices.
                            // By default, labels show the percentage of the slice.
                            showDataLabels: true
                          }
                        },
                        legend: { show:true, location: 'e' }
                      }
                    );
                    $("#chart_cat").bind('jqplotDataHighlight', function(ev, seriesIndex, pointIndex, data) {
                        var $this = $(this);
                        $this.attr('title', data[0] + ": " + data[1]);
                    });
                    $("#chart_cat").bind('jqplotDataUnhighlight', function(ev, seriesIndex, pointIndex, data) {
                        var $this = $(this);
                        $this.attr('title',"");
                    });
                  });
                  </script>
                </div>
            <div>
        </div>
    <div id="cchart_container"></div>    
   ]];
   return htmlbody;
end
