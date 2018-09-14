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

function XLSX:_create_bar_chart (rootpath, idx, sheet, graph)
   local file = io.open (rootpath.."/xl/charts/chart"..idx..".xml", "w")
   if file == nil then
      -- error message
      return -1
   end

   local tab = graph[XLSX.consts.graph.table]
   local graph_name = tab[XLSX.consts.table.name]
   local nb_lines = table.getn (tab[XLSX.consts.table.data])
   local nb_cols  = table.getn (tab[XLSX.consts.table.header])

   file:write ("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n")
   file:write ("<c:chartSpace xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\" "..
                             "xmlns:c=\"http://schemas.openxmlformats.org/drawingml/2006/chart\" "..
                             "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">\n")
   file:write ("  <c:date1904 val=\"0\"/>\n")
   file:write ("  <c:lang val=\"fr-FR\"/>\n")
   file:write ("  <c:roundedCorners val=\"0\"/>\n")
   file:write ("  <c:chart>\n")
   file:write ("    <c:title>\n")
   file:write ("      <c:tx>\n")
   file:write ("        <c:rich>\n")
   file:write ("          <a:bodyPr/>\n")
   file:write ("          <a:lstStyle/>\n")
   file:write ("          <a:p>\n")
   file:write ("            <a:pPr>\n")
   file:write ("              <a:defRPr/>\n")
   file:write ("            </a:pPr>\n")
   file:write ("            <a:r>\n")
   file:write ("              <a:rPr lang=\"fr-FR\"/>\n")
   file:write ("              <a:t>"..XLSX:escape_str (graph_name).."</a:t>\n")
   file:write ("            </a:r>\n")
   file:write ("          </a:p>\n")
   file:write ("        </c:rich>\n")
   file:write ("      </c:tx>\n")
   file:write ("      <c:layout/>\n")
   file:write ("      <c:overlay val=\"0\"/>\n")
   file:write ("    </c:title>\n")
   file:write ("    <c:autoTitleDeleted val=\"0\"/>\n")
   file:write ("    <c:plotArea>\n")
   file:write ("      <c:layout/>\n")
   file:write ("      <c:barChart>\n")
   file:write ("        <c:barDir val=\"col\"/>\n")
   file:write ("        <c:grouping val=\"clustered\"/>\n")
   file:write ("        <c:varyColors val=\"0\"/>\n")

   if tab[XLSX.consts.table.orientation] == XLSX.consts.table.orientations.columns then
      for i = 2, nb_cols do
         file:write ("        <c:ser>\n")
         file:write ("          <c:idx val=\""..(i - 2).."\"/>\n")
         file:write ("          <c:order val=\""..(i - 2).."\"/>\n")
         file:write ("          <c:tx>\n")
         file:write ("            <c:strRef>\n")
         file:write ("            <c:f>"..sheet[XLSX.consts.sheet.name].."!$"..XLSX:get_col_from_int (tab[XLSX.consts.table.x_head_loc] + i - 1).."$"..tab[XLSX.consts.table.y_head_loc].."</c:f>\n")
         file:write ("              <c:strCache>\n")
         file:write ("                <c:ptCount val=\"1\"/>\n")
         file:write ("                <c:pt idx=\"0\">\n")
         file:write ("                  <c:v>"..XLSX:escape_str (tab[XLSX.consts.table.header][i]).."</c:v>\n")
         file:write ("                </c:pt>\n")
         file:write ("              </c:strCache>\n")
         file:write ("            </c:strRef>\n")
         file:write ("          </c:tx>\n")
         file:write ("          <c:spPr>\n")
         file:write ("            <a:solidFill>\n")
         if graph[XLSX.consts.graph.custom_colors][i - 1] == nil then
            file:write ("              <a:srgbClr val=\""..XLSX.consts.graph.colors[((i - 2) % (table.getn (XLSX.consts.graph.colors))) + 1].."\"/>\n")
         else
            file:write ("              <a:srgbClr val=\""..graph[XLSX.consts.graph.custom_colors][i - 1].."\"/>\n")
         end
         file:write ("            </a:solidFill>\n")
         file:write ("            <a:ln>\n")
         file:write ("              <a:solidFill>\n")
         if graph[XLSX.consts.graph.custom_colors][i - 1] == nil then
            file:write ("                <a:srgbClr val=\""..XLSX.consts.graph.colors[((i - 2) % (table.getn (XLSX.consts.graph.colors))) + 1].."\"/>\n")
         else
            file:write ("                <a:srgbClr val=\""..graph[XLSX.consts.graph.custom_colors][i - 1].."\"/>\n")
         end
         file:write ("              </a:solidFill>\n")
         file:write ("            </a:ln>\n")
         file:write ("          </c:spPr>\n")
         file:write ("          <c:invertIfNegative val=\"0\"/>\n")
         file:write ("          <c:cat>\n")
         file:write ("            <c:strRef>\n")
         file:write ("              <c:f>"..sheet[XLSX.consts.sheet.name]..
                     "!$"..XLSX:get_col_from_int (tab[XLSX.consts.table.x_head_loc] + i - 2).."$"..(tab[XLSX.consts.table.y_head_loc] + 1)..":"..
                     "$"..XLSX:get_col_from_int (tab[XLSX.consts.table.x_head_loc] + i - 2).."$"..(tab[XLSX.consts.table.y_head_loc] + nb_lines).."</c:f>\n")
         file:write ("              <c:strCache>\n")
         file:write ("                <c:ptCount val=\""..nb_lines.."\"/>\n")
         for j = 1, nb_lines do
            file:write ("                <c:pt idx=\""..(j - 1).."\">\n")
            file:write ("                  <c:v>"..XLSX:escape_str (tab[XLSX.consts.table.data][j][1]).."</c:v>\n")
            file:write ("                </c:pt>\n")
         end
         file:write ("              </c:strCache>\n")
         file:write ("            </c:strRef>\n")
         file:write ("          </c:cat>\n")
         file:write ("          <c:val>\n")
         file:write ("            <c:numRef>\n")
         -- Positions of serie's data name
         file:write ("              <c:f>"..sheet[XLSX.consts.sheet.name]..
                     "!$"..XLSX:get_col_from_int (tab[XLSX.consts.table.x_head_loc] + i - 1).."$"..(tab[XLSX.consts.table.y_head_loc] + 1)..":"..
                     "$"..XLSX:get_col_from_int (tab[XLSX.consts.table.x_head_loc] + i - 1).."$"..(tab[XLSX.consts.table.y_head_loc] + nb_lines).."</c:f>\n")
         file:write ("              <c:numCache>\n")
         file:write ("                <c:formatCode>0.00</c:formatCode>\n")
         file:write ("                <c:ptCount val=\""..nb_lines.."\"/>\n")
         for j = 1, nb_lines do
            file:write ("                <c:pt idx=\""..(j - 1).."\">\n")
            file:write ("                  <c:v>"..XLSX:escape_str (tab[XLSX.consts.table.data][j][i]).."</c:v>\n")
            file:write ("                </c:pt>\n")
         end
         file:write ("              </c:numCache>\n")
         file:write ("            </c:numRef>\n")
         file:write ("          </c:val>\n")
         file:write ("        </c:ser>\n")
      end
   elseif tab[XLSX.consts.table.orientation] == XLSX.consts.table.orientations.lines then     
      for i = 1, nb_lines do
         file:write ("        <c:ser>\n")
         file:write ("          <c:idx val=\""..(i - 1).."\"/>\n")
         file:write ("          <c:order val=\""..(i - 1).."\"/>\n")
         file:write ("          <c:tx>\n")
         file:write ("            <c:strRef>\n")
         file:write ("            <c:f>"..sheet[XLSX.consts.sheet.name].."!$"..XLSX:get_col_from_int (tab[XLSX.consts.table.x_head_loc]).."$"..(tab[XLSX.consts.table.y_head_loc] + i).."</c:f>\n")
         file:write ("              <c:strCache>\n")
         file:write ("                <c:ptCount val=\"1\"/>\n")
         file:write ("                <c:pt idx=\"0\">\n")
         file:write ("                  <c:v>"..XLSX:escape_str (tab[XLSX.consts.table.data][i][1]).."</c:v>\n")
         file:write ("                </c:pt>\n")
         file:write ("              </c:strCache>\n")
         file:write ("            </c:strRef>\n")
         file:write ("          </c:tx>\n")
         file:write ("          <c:spPr>\n")
         file:write ("            <a:solidFill>\n")
         if graph[XLSX.consts.graph.custom_colors][i] == nil then
            file:write ("              <a:srgbClr val=\""..XLSX.consts.graph.colors[((i - 1) % (table.getn (XLSX.consts.graph.colors))) + 1].."\"/>\n")
         else
            file:write ("              <a:srgbClr val=\""..graph[XLSX.consts.graph.custom_colors][i].."\"/>\n")
         end
         file:write ("            </a:solidFill>\n")
         file:write ("            <a:ln>\n")
         file:write ("              <a:solidFill>\n")
         if graph[XLSX.consts.graph.custom_colors][i] == nil then
            file:write ("                <a:srgbClr val=\""..XLSX.consts.graph.colors[((i - 1) % (table.getn (XLSX.consts.graph.colors))) + 1].."\"/>\n")
         else
            file:write ("                <a:srgbClr val=\""..graph[XLSX.consts.graph.custom_colors][i].."\"/>\n")
         end
         file:write ("              </a:solidFill>\n")
         file:write ("            </a:ln>\n")
         file:write ("          </c:spPr>\n")
         file:write ("          <c:invertIfNegative val=\"0\"/>\n")
         file:write ("          <c:cat>\n")
         file:write ("            <c:strRef>\n")
         file:write ("              <c:f>"..sheet[XLSX.consts.sheet.name]..
                     "!$"..XLSX:get_col_from_int (tab[XLSX.consts.table.x_head_loc] + 1).."$"..(tab[XLSX.consts.table.y_head_loc])..":"..
                     "$"..XLSX:get_col_from_int (tab[XLSX.consts.table.x_head_loc] + nb_cols - 1).."$"..(tab[XLSX.consts.table.y_head_loc]).."</c:f>\n")
         file:write ("              <c:strCache>\n")
         file:write ("                <c:ptCount val=\""..nb_cols.."\"/>\n")
         for j = 2, nb_cols do
            file:write ("                <c:pt idx=\""..(j - 2).."\">\n")
            file:write ("                  <c:v>"..XLSX:escape_str (tab[XLSX.consts.table.header][j]).."</c:v>\n")
            file:write ("                </c:pt>\n")
         end
         file:write ("              </c:strCache>\n")
         file:write ("            </c:strRef>\n")
         file:write ("          </c:cat>\n")
         file:write ("          <c:val>\n")
         file:write ("            <c:numRef>\n")
         -- Positions of serie's data name
         file:write ("              <c:f>"..sheet[XLSX.consts.sheet.name]..
                     "!$"..XLSX:get_col_from_int (tab[XLSX.consts.table.x_head_loc] + 1).."$"..(tab[XLSX.consts.table.y_head_loc] + i)..":"..
                     "$"..XLSX:get_col_from_int (tab[XLSX.consts.table.x_head_loc] + nb_cols - 1).."$"..(tab[XLSX.consts.table.y_head_loc] + i).."</c:f>\n")
         file:write ("              <c:numCache>\n")
         file:write ("                <c:formatCode>0.00</c:formatCode>\n")
         file:write ("                <c:ptCount val=\""..nb_lines.."\"/>\n")
         for j = 2, nb_cols do
            file:write ("                <c:pt idx=\""..(j - 1).."\">\n")
            file:write ("                  <c:v>"..XLSX:escape_str (tab[XLSX.consts.table.data][i][j]).."</c:v>\n")
            file:write ("                </c:pt>\n")
         end
         file:write ("              </c:numCache>\n")
         file:write ("            </c:numRef>\n")
         file:write ("          </c:val>\n")
         file:write ("        </c:ser>\n")
      end      
   end
   
   file:write ("        <c:dLbls>\n")
   file:write ("          <c:showLegendKey val=\"0\"/>\n")
   file:write ("          <c:showVal val=\"0\"/>\n")
   file:write ("          <c:showCatName val=\"0\"/>\n")
   file:write ("          <c:showSerName val=\"0\"/>\n")
   file:write ("          <c:showPercent val=\"0\"/>\n")
   file:write ("          <c:showBubbleSize val=\"0\"/>\n")
   file:write ("        </c:dLbls>\n")
   file:write ("        <c:gapWidth val=\"150\"/>\n")
   file:write ("        <c:axId val=\"147635432\"/>\n")
   file:write ("        <c:axId val=\"147635824\"/>\n")
   file:write ("      </c:barChart>\n")
   file:write ("      <c:catAx>\n")
   file:write ("        <c:axId val=\"147635432\"/>\n")
   file:write ("        <c:scaling>\n")
   file:write ("          <c:orientation val=\"minMax\"/>\n")
   file:write ("        </c:scaling>\n")
   file:write ("        <c:delete val=\"0\"/>\n")
   file:write ("        <c:axPos val=\"b\"/>\n")
   file:write ("        <c:numFmt formatCode=\"General\" sourceLinked=\"1\"/>\n")
   file:write ("        <c:majorTickMark val=\"none\"/>\n")
   file:write ("        <c:minorTickMark val=\"none\"/>\n")
   file:write ("        <c:tickLblPos val=\"nextTo\"/>\n")
   file:write ("        <c:spPr>\n")
   file:write ("          <a:noFill/>\n")
   file:write ("          <a:ln w=\"9525\" cap=\"flat\" cmpd=\"sng\" algn=\"ctr\">\n")
   file:write ("            <a:solidFill>\n")
   file:write ("              <a:schemeClr val=\"tx1\">\n")
   file:write ("                <a:lumMod val=\"15000\"/>\n")
   file:write ("                <a:lumOff val=\"85000\"/>\n")
   file:write ("              </a:schemeClr>\n")
   file:write ("            </a:solidFill>\n")
   file:write ("            <a:round/>\n")
   file:write ("          </a:ln>\n")
   file:write ("          <a:effectLst/>\n")
   file:write ("        </c:spPr>\n")
   file:write ("        <c:txPr>\n")
   file:write ("          <a:bodyPr rot=\"-60000000\" spcFirstLastPara=\"1\" vertOverflow=\"ellipsis\" vert=\"horz\" wrap=\"square\" anchor=\"ctr\" anchorCtr=\"1\"/>\n")
   file:write ("          <a:lstStyle/>\n")
   file:write ("          <a:p>\n")
   file:write ("            <a:pPr>\n")
   file:write ("              <a:defRPr sz=\"900\" b=\"0\" i=\"0\" u=\"none\" strike=\"noStrike\" kern=\"1200\" baseline=\"0\">\n")
   file:write ("                <a:solidFill>\n")
   file:write ("                  <a:schemeClr val=\"tx1\">\n")
   file:write ("                    <a:lumMod val=\"65000\"/>\n")
   file:write ("                    <a:lumOff val=\"35000\"/>\n")
   file:write ("                  </a:schemeClr>\n")
   file:write ("                </a:solidFill>\n")
   file:write ("                <a:latin typeface=\"+mn-lt\"/>\n")
   file:write ("                <a:ea typeface=\"+mn-ea\"/>\n")
   file:write ("                <a:cs typeface=\"+mn-cs\"/>\n")
   file:write ("              </a:defRPr>\n")
   file:write ("            </a:pPr>\n")
   file:write ("            <a:endParaRPr lang=\"fr-FR\"/>\n")
   file:write ("          </a:p>\n")
   file:write ("        </c:txPr>\n")

   file:write ("        <c:crossAx val=\"147635824\"/>\n")
   file:write ("        <c:crosses val=\"autoZero\"/>\n")
   file:write ("        <c:auto val=\"1\"/>\n")
   file:write ("        <c:lblAlgn val=\"ctr\"/>\n")
   file:write ("        <c:lblOffset val=\"100\"/>\n")
   file:write ("        <c:noMultiLvlLbl val=\"0\"/>\n")
   file:write ("      </c:catAx>\n")
   file:write ("      <c:valAx>\n")
   file:write ("        <c:axId val=\"147635824\"/>\n")
   file:write ("        <c:scaling>\n")
   file:write ("          <c:orientation val=\"minMax\"/>\n")
   file:write ("        </c:scaling>\n")
   file:write ("        <c:delete val=\"0\"/>\n")
   file:write ("        <c:axPos val=\"l\"/>\n")
   file:write ("        <c:majorGridlines/>\n")
   file:write ("        <c:numFmt formatCode=\"0.00\" sourceLinked=\"1\"/>\n")
   file:write ("        <c:majorTickMark val=\"none\"/>\n")
   file:write ("        <c:minorTickMark val=\"none\"/>\n")
   file:write ("        <c:tickLblPos val=\"nextTo\"/>\n")
   
   file:write ("        <c:spPr>\n")
   file:write ("          <a:noFill/>\n")
   file:write ("          <a:ln>\n")
   file:write ("            <a:noFill/>\n")
   file:write ("          </a:ln>\n")
   file:write ("          <a:effectLst/>\n")
   file:write ("        </c:spPr>\n")
   file:write ("        <c:txPr>\n")
   file:write ("          <a:bodyPr rot=\"-60000000\" spcFirstLastPara=\"1\" vertOverflow=\"ellipsis\" vert=\"horz\" wrap=\"square\" anchor=\"ctr\" anchorCtr=\"1\"/>\n")
   file:write ("          <a:lstStyle/>\n")
   file:write ("          <a:p>\n")
   file:write ("            <a:pPr>\n")
   file:write ("              <a:defRPr sz=\"900\" b=\"0\" i=\"0\" u=\"none\" strike=\"noStrike\" kern=\"1200\" baseline=\"0\">\n")
   file:write ("                <a:solidFill>\n")
   file:write ("                  <a:schemeClr val=\"tx1\">\n")
   file:write ("                    <a:lumMod val=\"65000\"/>\n")
   file:write ("                    <a:lumOff val=\"35000\"/>\n")
   file:write ("                  </a:schemeClr>\n")
   file:write ("                </a:solidFill>\n")
   file:write ("                <a:latin typeface=\"+mn-lt\"/>\n")
   file:write ("                <a:ea typeface=\"+mn-ea\"/>\n")
   file:write ("                <a:cs typeface=\"+mn-cs\"/>\n")
   file:write ("              </a:defRPr>\n")
   file:write ("            </a:pPr>\n")
   file:write ("            <a:endParaRPr lang=\"fr-FR\"/>\n")
   file:write ("          </a:p>\n")
   file:write ("        </c:txPr>\n")
      
   file:write ("        <c:crossAx val=\"147635432\"/>\n")
   file:write ("        <c:crosses val=\"autoZero\"/>\n")
   file:write ("        <c:crossBetween val=\"between\"/>\n")
   file:write ("      </c:valAx>\n")
   file:write ("      <c:spPr>\n")
   file:write ("        <a:noFill/>\n")
   file:write ("        <a:ln>\n")
   file:write ("          <a:noFill/>\n")
   file:write ("        </a:ln>\n")
   file:write ("        <a:effectLst/>\n")
   file:write ("      </c:spPr>\n")
   file:write ("    </c:plotArea>\n")
   file:write ("    <c:legend>\n")
   file:write ("      <c:legendPos val=\"r\"/>\n")
   file:write ("      <c:layout/>\n")
   file:write ("      <c:overlay val=\"0\"/>\n")
   file:write ("    </c:legend>\n")
   file:write ("    <c:plotVisOnly val=\"1\"/>\n")
   file:write ("    <c:dispBlanksAs val=\"gap\"/>\n")
   file:write ("    <c:showDLblsOverMax val=\"0\"/>\n")
   file:write ("  </c:chart>\n")
   file:write ("  <c:printSettings>\n")
   file:write ("    <c:headerFooter/>\n")
   file:write ("    <c:pageMargins b=\"0.75\" l=\"0.7\" r=\"0.7\" t=\"0.75\" header=\"0.3\" footer=\"0.3\"/>\n")
   file:write ("    <c:pageSetup/>\n")
   file:write ("  </c:printSettings>\n")
   file:write ("</c:chartSpace>\n")

   file:close ()
   return 0
end
