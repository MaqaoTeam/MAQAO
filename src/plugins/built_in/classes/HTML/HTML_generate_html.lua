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


--- [PRIVATE] Print the js code for an action
-- @param file A HTML_file object
-- @param action An HTMl_action to print in the file
-- @param i_name An optionnal string used to generate a 'regular action' url. 
--               It replaced the pattern <i> in the action url
-- @param j_name An optionnal string used to generate a 'regular action' url. 
--               It replaced the pattern <j> in the action url
-- @param tparam A string in {"reg", "obj"}.
--               If nil / "reg" (default),
--               this means parameters are i, j and p. This is the action
--               used on charts or by any kind of click
--               If "obj", this means there is only a p parameter
--               and the action must used a global variable called _global_elem
--               which is the element the user click on. This is the action
--               used by right click menus.
--               
local function _print_action (file, action, tparam)
   local url = action.url

   if tparam == nil
   or type (tparam) ~= "string"
   or tparam == "reg" then
      url = string.gsub (url, "<i>", "\"+i+\"")
      url = string.gsub (url, "<j>", "\"+j+\"")
      url = string.gsub (url, "<p>", "\"+p+\"")
   elseif tparam == "obj" then
      url = string.gsub (url, "<i>", "\"+_global_elem.getAttribute(\"_i\")+\"")
      url = string.gsub (url, "<j>", "\"+_global_elem.getAttribute(\"_j\")+\"")
      url = string.gsub (url, "<p>", "\"+_global_elem.getAttribute(p)+\"")
      url = string.gsub (url, "<pm>", "\"+_global_elem.getAttribute(pm)+\"")
   end


   if action.type == "link" then
      if action.new_tab ~= false then
         file:write ("           open(\""..url.."\");\n")
      else
         file:write ("           open(\""..url.."\", \"_self\");\n")
      end

   elseif action.type == "popup" then
      file:write ("           window.open(\""..url.."\",'"..action.name.."','menubar=no, scrollbars=no, top='+300+', left='+300+', width='+950+', height='+500+'');\n")

   elseif action.type == "overlay" then
      file:write ("           document.getElementById(\"maqao_modal-body\").innerHTML=\"<object id=\\\"maqao_modal-body_content\\\" type=\\\"text/html\\\" data=\\\""..url.."\\\"></object>\";\n")
      file:write ("           var modal = document.getElementById('MaqaoModal');\n")
      file:write ("           modal.style.display = \"block\";\n")
   end
end



--- [PRIVATE] Display the header of an HTML file (everything include inside and before <head></head>). 
--  The header contains:
--    - CSS files
--    - Some JS files for JQuery, JQPlot and JQGrid ...
-- @param file A HTML_file object
-- @param parameters A table of parameters, defined in HTML:create_file() function
-- @param prefix A prefix added to all ressource paths
-- @return true if the header has been printed, else false
function HTML:_html_output_print_header(file, parameters, prefix)
   if file == nil then
      return false
   end

   if prefix == nil then
      prefix = ""
   end

   local page_name = "_"
   if type (parameters.title) == "string" then
      page_name = parameters.title
   end
   local is_menu = false
   if type (parameters.menu) == "table" then
      is_menu = true
   end 
   
   file:write ("<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">")
   file:write ("<html><head><title>"..page_name.."</title>\n")
   file:write ("<link rel=\"stylesheet\" type=\"text/css\" media=\"screen\" href=\""..prefix.."css/ui.jqgrid.css\" />\n")
   file:write ("<link rel=\"stylesheet\" type=\"text/css\" href=\""..prefix.."css/themes/redmond_custom/jquery-ui.css\" />\n")
   file:write ("<link rel=\"stylesheet\" type=\"text/css\" href=\""..prefix.."css/themes/redmond_custom/jquery.jqplot.min.css\" />\n")
   file:write ("<link rel=\"stylesheet\" type=\"text/css\" href=\""..prefix.."css/maqao_modal.css\" />\n")
   if parameters.maqao_header ~= false then
      file:write ("<link rel=\"stylesheet\" type=\"text/css\" href=\""..prefix.."css/global.css\" />\n")
      if is_menu == true then
         file:write ("<link rel=\"stylesheet\" type=\"text/css\" href=\""..prefix.."css/maqao_menu.css\" />\n")
      end
   else
      file:write ("<link rel=\"stylesheet\" type=\"text/css\" href=\""..prefix.."css/global_noheader.css\" />\n")
   end
   if  parameters.custom_css ~= nil
   and type (parameters.custom_css) == "table" then
      for _, css in ipairs (parameters.custom_css) do
         file:write ("<link rel=\"stylesheet\" type=\"text/css\" href=\""..prefix.."/"..css.."\" />\n")
      end
   end
   file:write ("<script src=\""..prefix.."js/jquery-1.9.0.min.js\" type=\"text/javascript\"></script>\n")
   file:write ("<script src=\""..prefix.."js/jquery-ui-1.10.3.custom.min.js\" type=\"text/javascript\"></script>\n")
   file:write ("<script src=\""..prefix.."js/grid.locale-en.js\" type=\"text/javascript\"></script>\n")
   file:write ("<script src=\""..prefix.."js/jquery.jqGrid.min.js\" type=\"text/javascript\"></script>\n")
   file:write ("<script src=\""..prefix.."js/d3.min.js\" type=\"text/javascript\"></script>\n")
   file:write ("<script class=\"include\" type=\"text/javascript\" src=\""..prefix.."js/jquery.jqplot.min.js\"></script>\n")
   file:write ("<script class=\"include\" language=\"javascript\" type=\"text/javascript\" src=\""..prefix.."js/jqplot.pieRenderer.min.js\"></script>\n")
   file:write ("<script class=\"include\" language=\"javascript\" type=\"text/javascript\" src=\""..prefix.."js/jqplot.barRenderer.min.js\"></script>\n")
   file:write ("<script class=\"include\" language=\"javascript\" type=\"text/javascript\" src=\""..prefix.."js/jqplot.categoryAxisRenderer.min.js\"></script>\n")
   file:write ("<script class=\"include\" language=\"javascript\" type=\"text/javascript\" src=\""..prefix.."js/jqplot.pointLabels.min.js\"></script>\n")
   file:write ("<script class=\"include\" language=\"javascript\" type=\"text/javascript\" src=\""..prefix.."js/jqplot.cursor.min.js\"></script>\n")
   file:write ("<script class=\"include\" language=\"javascript\" type=\"text/javascript\" src=\""..prefix.."js/jqplot.canvasTextRenderer.min.js\"></script>\n")
   file:write ("<script class=\"include\" language=\"javascript\" type=\"text/javascript\" src=\""..prefix.."js/jqplot.canvasAxisTickRenderer.min.js\"></script>\n")
   file:write ("<script class=\"include\" language=\"javascript\" type=\"text/javascript\" src=\""..prefix.."js/jqplot.canvasAxisLabelRenderer.min.js\"></script>\n")
   file:write ("<script class=\"include\" language=\"javascript\" type=\"text/javascript\" src=\""..prefix.."js/jqplot.enhancedLegendRenderer.min.js\"></script>\n")
   file:write ("<script class=\"include\" language=\"javascript\" type=\"text/javascript\" src=\""..prefix.."js/jqplot.highlighter.min.js\"></script>\n")

   if is_menu == true then
      file:write ("<script class=\"include\" language=\"javascript\" type=\"text/javascript\" src=\""..prefix.."js/maqao_menu.js\"></script>\n")
   end
   file:write ("<script>\n")
   file:write ("$( function() {\n")
   file:write ("  $( document ).tooltip({\n")
   file:write ("    content: function() {\n")
   file:write ("      if ($(this).hasClass (\"tooltip-maqao\")) {\n")
   file:write ("        return $(this).attr('title');\n")
   file:write ("      }\n")
   file:write ("    },\n")
   file:write ("    position: {\n")
   file:write ("      my: \"center bottom-20\",\n")
   file:write ("        at: \"center top\",\n")
   file:write ("        using: function( position, feedback ) {\n")
   file:write ("          $( this ).css( position );\n")
   file:write ("          $( \"<div>\" )\n")
   file:write ("            .addClass( \"arrow-tt\" )\n")
   file:write ("            .addClass( feedback.vertical )\n")
   file:write ("            .addClass( feedback.horizontal )\n")
   file:write ("            .appendTo( this );\n")
   file:write ("        }\n")
   file:write ("      }\n")
   file:write ("  });\n")
   file:write ("} );\n")
   file:write ("\n")
   file:write ("// Code from https://www.htmlgoodies.com/beyond/javascript/article.php/3724571/Using-Multiple-JavaScript-Onload-Functions.htm\n")
   file:write ("function addLoadEvent(func) {\n")
   file:write ("  var oldonload = window.onload;\n")
   file:write ("  if (typeof window.onload != 'function') {\n")
   file:write ("    window.onload = func;\n")
   file:write ("  } else {\n")
   file:write ("    window.onload = function() {\n")
   file:write ("      if (oldonload) {\n")
   file:write ("        oldonload();\n")
   file:write ("      }\n")
   file:write ("      func();\n")
   file:write ("    }\n")
   file:write ("  }\n")
   file:write ("}\n")
   file:write ("</script>\n")
   file:write ("</head><body>\n") 
   
   return true
end



--- [PRIVATE] Display the footer of an HTML file (everything inserted after the user inserts in the file).
-- The footer contains:
--    - Everything needed for the overlay
--    - Some JS scripts which must be defined at the end of the file
--    - The final </html> tag
-- @param file A HTML_file object
-- @return true if the footer has been printed, else false
function HTML:_html_output_print_footer(file)
   if file == nil then
      return false
   end
   
   file:write ("<div id=\"MaqaoModal\" class=\"maqao_modal\">\n")
   file:write ("  <div class=\"maqao_modal-content\">\n")
   file:write ("    <div class=\"maqao_modal-header\">\n")
   file:write ("      <span class=\"maqao_close\">&times;</span>\n")
   file:write ("      <h2></h2>\n")
   file:write ("    </div>\n")
   file:write ("    <div id=\"maqao_modal-body\" class=\"maqao_modal-body\">\n")
   file:write ("    </div>\n")
   file:write ("  </div>\n")
   file:write ("</div>\n")
   file:write ("<script>\n")
   file:write ("\n")
   file:write ("var modal = document.getElementById('MaqaoModal');\n")
   file:write ("var span = document.getElementsByClassName(\"maqao_close\")[0];\n")
   file:write ("span.onclick = function() {\n")
   file:write ("    modal.style.display = \"none\";\n")
   file:write ("}\n")
   file:write ("window.onclick = function(event) {\n")
   file:write ("    if (event.target == modal) {\n")
   file:write ("        modal.style.display = \"none\";\n")
   file:write ("    }\n")
   file:write ("}\n")
   file:write ("</script>\n")
   file:write ("</div>")
   file:write ("</body>")
   file:write ('<script type="text/javascript">\n')

   file:write ('const getCellValue = (tr, idx) => tr.children[idx].innerText || tr.children[idx].textContent;\n')
   file:write ('const comparer = (idx, asc) => (a, b) => ((v1, v2) => \n')
   file:write ('    v1 !== \'\' && v2 !== \'\' && !isNaN(v1) && !isNaN(v2) ? v2 - v1 : v1.toString().localeCompare(v2)\n')
   file:write ('    )(getCellValue(asc ? a : b, idx), getCellValue(asc ? b : a, idx));\n')
   file:write ('addLoadEvent(function(){\n')
   file:write ('  document.querySelectorAll(\'._fixed_table th\').forEach(th => th.addEventListener(\'click\', (() => {\n')
   file:write ('    const table = th.closest(\'table\');\n')
   file:write ('    // Remove all symbols in header\n')
   file:write ('    table.querySelectorAll(\'th\').forEach(function (e) {\n')
   file:write ('      e.firstChild.textContent = "";\n')
   file:write ('    });\n')
   file:write ('    // Change the symbol on the clicked header\n')
   file:write ('    if (this.asc != true) {\n')
   file:write ('      th.firstChild.textContent = "\\u25BC";\n')
   file:write ('    }\n')
   file:write ('    else {\n')
   file:write ('      th.firstChild.textContent = "\\u25B2";\n')
   file:write ('    }\n')
   file:write ('    Array.from(table.querySelectorAll(\'tr:nth-child(n+2)\'))\n')
   file:write ('        .sort(comparer(Array.from(th.parentNode.children).indexOf(th), this.asc = !this.asc))\n')
   file:write ('        .forEach(tr => table.appendChild(tr) );\n')
   file:write ('  })));\n')
   file:write ('});\n')

   file:write ('// Fonction for table selector\n')
   file:write ('function click_table_menu (c) {\n')
   file:write ('  $("."+c).each(function(index, element) {\n')
   file:write ('    element.classList.toggle("_tcol_hidden");\n')
   file:write ('  });\n')
   file:write ('}\n')
   file:write ('// Script for accordion box\n')
   file:write ('function _click_accordion_header(obj) {\n')
   file:write ('  var content_id = \'_accordion_content_\' + obj.id.substr (obj.id.lastIndexOf(\'_\') + 1);\n')
   file:write ('  var content = document.getElementById(content_id);\n')
   file:write ('  content.classList.toggle(\'collapsed\');\n')
   file:write ('  obj.classList.toggle(\'collapsed\');\n')
   file:write ('  obj.parentNode.classList.toggle(\'collapsed\');\n')
   file:write ('  // Check if the box must be openned ...\n')
   file:write ('  if (obj.classList.contains (\'collapsed\'))\n')
   file:write ('  {\n')
   file:write ('    obj.firstChild.textContent = "\\u25B6";\n')
   file:write ('  }\n')
   file:write ('  // or closed.\n')
   file:write ('  else \n')
   file:write ('  {\n')
   file:write ('    obj.firstChild.textContent = "\\u25BC";\n')
   file:write ('  }\n')
   file:write ('}\n')
   file:write ('  // Script for paged table\n')
   file:write ('function _click_paged_prev(obj) {\n')
   file:write ('  var pages_content_id = \'_paged_tables_page_\' + obj.parentNode.parentNode.id.substr (obj.parentNode.parentNode.id.lastIndexOf(\'_\') + 1);\n')
   file:write ('  var pages_content = document.getElementById(pages_content_id).innerHTML;\n')
   file:write ('  var max_pages = pages_content.substr (pages_content.lastIndexOf(\'/\') + 1);\n')
   file:write ('  var cur_page  = pages_content.substring (pages_content.lastIndexOf(\' \') + 1, pages_content.lastIndexOf(\'/\'));\n')
   file:write ('  max_pages = Number (max_pages);\n')
   file:write ('  cur_page  = Number (cur_page);\n')
   file:write ('  if (cur_page > 1) {\n')
   file:write ('    var div_paged = obj.parentNode.parentNode.id;\n')
   file:write ('    var i;\n')
   file:write ('    var x = document.getElementsByClassName(div_paged + \'_\' + (cur_page - 1));\n')
   file:write ('    for (i = 0; i < x.length; i++) {\n')
   file:write ('      x[i].classList.toggle(\'_paged_hidden\');\n')
   file:write ('    }\n')
   file:write ('    x = document.getElementsByClassName(div_paged + \'_\' + (cur_page - 2));\n')
   file:write ('    for (i = 0; i < x.length; i++) {\n')
   file:write ('      x[i].classList.toggle(\'_paged_hidden\');\n')
   file:write ('    }\n')
   file:write ('  document.getElementById(pages_content_id).textContent = "Page " + (cur_page - 1) + "/" + max_pages;\n')
   file:write ('  }\n')
   file:write ('}\n')
   file:write ('function _click_paged_next(obj) {\n')
   file:write ('  var pages_content_id = \'_paged_tables_page_\' + obj.parentNode.parentNode.id.substr (obj.parentNode.parentNode.id.lastIndexOf(\'_\') + 1);\n')
   file:write ('  var pages_content = document.getElementById(pages_content_id).innerHTML;\n')
   file:write ('  var max_pages = pages_content.substr (pages_content.lastIndexOf(\'/\') + 1);\n')
   file:write ('  var cur_page  = pages_content.substring (pages_content.lastIndexOf(\' \') + 1, pages_content.lastIndexOf(\'/\'));\n')
   file:write ('  max_pages = Number (max_pages);\n')
   file:write ('  cur_page  = Number (cur_page);\n')
   file:write ('  if (cur_page < max_pages) {\n')
   file:write ('    var div_paged = obj.parentNode.parentNode.id;\n')
   file:write ('    var i;\n')
   file:write ('    var x = document.getElementsByClassName(div_paged + \'_\' + (cur_page - 1));\n')
   file:write ('    for (i = 0; i < x.length; i++) {\n')
   file:write ('      x[i].classList.toggle(\'_paged_hidden\');\n')
   file:write ('    }\n')
   file:write ('    x = document.getElementsByClassName(div_paged + \'_\' + cur_page);\n')
   file:write ('    for (i = 0; i < x.length; i++) {\n')
   file:write ('      x[i].classList.toggle(\'_paged_hidden\');\n')
   file:write ('    }\n')
   file:write ('    document.getElementById(pages_content_id).textContent = "Page " + (cur_page + 1) + "/" + max_pages;\n')
   file:write ('  }\n')
   file:write ('}\n')
   file:write ('// Function for tree table\n')
   file:write ('function _click_treed(obj) {\n')
   file:write ('  if (obj.innerHTML == "\\u25BA") {\n')
   file:write ('    obj.textContent = "\\u25BC";\n')
   file:write ('    // Open childs\n')
   file:write ('    var x = document.getElementsByClassName(obj.parentNode.parentNode.id);\n')
   file:write ('    for (var i = 0; i < x.length; i++) {\n')
   file:write ('      x[i].classList.toggle(\'_treed_hidden\');\n')
   file:write ('    }\n')
   file:write ('  }\n')
   file:write ('  else {\n')
   file:write ('    obj.textContent = "\\u25BA";\n')
   file:write ('    // Close all childs\n')
   file:write ('    var trs = document.getElementsByTagName(\'tr\');\n')
   file:write ('    var r   = obj.parentNode.parentNode.id+\'_\';\n')
   file:write ('    var re  = new RegExp (r, "g");\n')
   file:write ('    for (var i = 0; i < trs.length; i++) {\n')
   file:write ('      var s = trs[i].id+""\n')
   file:write ('      if (s.match(re)) {\n')
   file:write ('        var o = document.getElementById(s);\n')
   file:write ('        if (! o.classList.contains(\'_treed_hidden\')) {\n')
   file:write ('          o.classList.toggle(\'_treed_hidden\');\n')
   file:write ('        }\n')
   file:write ('        if (o.firstChild.firstChild.textContent == "\\u25BC") {\n')
   file:write ('          o.firstChild.firstChild.textContent = "\\u25BA";\n')
   file:write ('        }\n')
   file:write ('        if (o.firstChild.childNodes.length == 3) {\n')
   file:write ('          o.firstChild.childNodes[2].textContent = \'+\';\n')
   file:write ('        }\n')
   file:write ('      }\n')
   file:write ('    }\n')

   file:write ('    if (obj.parentNode.childNodes.length == 3) {\n')
   file:write ('      obj.parentNode.childNodes[2].textContent = \'+\';\n')
   file:write ('    }\n')

   file:write ('  }\n')
   file:write ('}\n')
   
   file:write ('function _click_expand (obj) {\n')
   file:write ('  var trs = document.getElementsByTagName(\'tr\');\n')
   file:write ('  var r   = obj.parentNode.parentNode.id+\'_\';\n')
   file:write ('  var re  = new RegExp (r, "g");\n')
   file:write ('  // Symbole + => expand all\n')
   file:write ('  if (obj.textContent == "+") {\n')
   file:write ('    for (var i = 0; i < trs.length; i++) {\n')
   file:write ('      var s = trs[i].id+"";\n')
   file:write ('      if (s.match(re)) {\n')
   file:write ('        var o = document.getElementById(s);\n')
   file:write ('        if (o.classList.contains(\'_treed_hidden\')) {\n')
   file:write ('           o.classList.toggle(\'_treed_hidden\');\n')
   file:write ('        }\n')
   file:write ('        if (o.firstChild.firstChild.textContent == "\\u25BA") {\n')
   file:write ('          o.firstChild.firstChild.textContent = "\\u25BC";\n')
   file:write ('        }\n')
   file:write ('        if (o.firstChild.childNodes.length == 3) {\n')
   file:write ('          o.firstChild.childNodes[2].textContent = \'\\u2010\';\n')
   file:write ('        }\n')
   file:write ('      }\n')
   file:write ('    }\n')
   file:write ('    var o = obj.parentNode.parentNode;\n')
   file:write ('    if (o.firstChild.firstChild.textContent == "\\u25BA") {\n')
   file:write ('      o.firstChild.firstChild.textContent = "\\u25BC";\n')
   file:write ('    }\n')
   file:write ('    obj.textContent = "\\u2010";\n')
   file:write ('  }\n')
   file:write ('  // Symbole - => collapse all\n')
   file:write ('  else {\n')
   file:write ('    for (var i = 0; i < trs.length; i++) {\n')
   file:write ('      var s = trs[i].id+"";\n')
   file:write ('      if (s.match(re)) {\n')
   file:write ('        var o = document.getElementById(s);\n')
   file:write ('        if (! o.classList.contains(\'_treed_hidden\')) {\n')
   file:write ('           o.classList.toggle(\'_treed_hidden\');\n')
   file:write ('        }\n')
   file:write ('        if (o.firstChild.firstChild.textContent == "\\u25BC") {\n')
   file:write ('          o.firstChild.firstChild.textContent = "\\u25BA";\n')
   file:write ('        }\n')
   file:write ('        if (o.firstChild.childNodes.length == 3) {\n')
   file:write ('          o.firstChild.childNodes[2].textContent = \'+\';\n')
   file:write ('        }\n')
   file:write ('      }\n')
   file:write ('    }\n')
   file:write ('    var o = obj.parentNode.parentNode;\n')
   file:write ('    if (o.firstChild.firstChild.textContent == "\\u25BC") {\n')
   file:write ('      o.firstChild.firstChild.textContent = "\\u25BA";\n')
   file:write ('    }\n')
   file:write ('    obj.textContent = "+";\n')
   file:write ('  }\n')
   file:write ('}\n')
   
   file:write ('  </script>\n')



     
   file:write ("<script>\n")
   file:write (" $(function() {\n")      
   file:write ("   $(\".cf_level_tabs\").tabs();\n")
   file:write (" });\n")
   file:write ("</script>\n")

   file:write ("</html>\n")
   
   return true
end



--- [PRIVATE] Recursive function used to display the main menu
-- This function is a helper for HTML:_html_output_insert_menu()
-- @param file A HTML_file object
-- @param elem A table representing a menu element to display
local function _rec_html_output_insert_menu (file, elem, prefix)
   local link = elem.link
   local class = ""
   
   if link == nil
   or link == "" then
      link = "#"
   end
   
   if string.match (file.name, "/"..elem.link) then
      class = "class=\"_current_page\""
   end
   
   if prefix == nil then
      file:write ("<li><a "..class.." href=\""..link.."\">"..elem.name.."</a>")
   else
      file:write ("<li><a "..class.." href=\""..prefix.."/"..link.."\">"..elem.name.."</a>")
   end
   
   if elem.sub ~= nil then
      file:write ("<ul>")
      for _, s_elem in ipairs (elem.sub) do
         _rec_html_output_insert_menu (file, s_elem)
      end
      file:write ("</ul>")
   end
   file:write ("</li>")
end



--- [PRIVATE] Print the main menu HTML code
-- @param file A HTML_file object
-- @param menu A table describing the menu to insert, described in
--             HTML:create_file (parameters) help as parameters.menu.
-- @param prefix
function HTML:_html_output_insert_menu (file, menu, prefix)

   file:write ("<div id=\"page_title\">")
   if prefix == nil then
      file:write ("<img id=\"maqao_logo\" src=\"images/MAQAO_small_logo.png\" />")
   else
      file:write ("<img id=\"maqao_logo\" src=\""..prefix.."images/MAQAO_small_logo.png\" />")
   end
   file:write ("<ul id=\"maqao_menu\">")
   for _, elem in ipairs (menu) do
      _rec_html_output_insert_menu (file, elem, prefix)
   end

   file:write ("</ul>")
   file:write ("</div>\n")
end



function HTML:_html_display_pie_chart (file, hchart)
   -- Build the data string needed to create the chart
   local _data = ""
   for i, sdata in ipairs (hchart.data[1]) do
      if type (sdata.key) ~= "string"
      or type (sdata.value) ~= "number" then
         -- TODO error invalid parameter
         return false
      end
      _data = _data.."['"..sdata.key.."',"..sdata.value.."],"
   end
   if _data == "" then
      return false
   end

   file:write ("      <div id=\"chart_cat_"..hchart._id.."\" style=\"height:400px; margin: 0 auto; overflow-x:auto; overflow-y:hidden;\"></div>\n")
   file:write ("      <script class=\"code\" type=\"text/javascript\">\n")
   file:write ("        $(document).ready(function(){\n")
   file:write ("          var data = [\n")
   file:write ("            ".._data.."\n")
   file:write ("          ];\n")
   file:write ("          var plot"..hchart._id.." = jQuery.jqplot ('chart_cat_"..hchart._id.."', [data],\n")
   file:write ("                      {\n")
   file:write ("                        grid: {\n")
   file:write ("                          drawBorder: false,\n")
   file:write ("                          drawGridlines: false,\n")
   file:write ("                          background: '#ffffff',\n")
   file:write ("                          shadow:false\n")
   file:write ("                        },\n")
   file:write ("                        seriesDefaults: {\n")
   file:write ("                          renderer: jQuery.jqplot.PieRenderer,\n")
   file:write ("                          rendererOptions: {\n")
   file:write ("                            showDataLabels: true\n")
   file:write ("                          }\n")
   file:write ("                        },\n")

   if hchart.header.legend == "n"
   or hchart.header.legend == "s"
   or hchart.header.legend == "e"
   or hchart.header.legend == "w" then
      file:write ("                        legend: { show:true, location: '"..hchart.header.legend.."' }\n")
   elseif hchart.header.legend == "none" then
      file:write ("                        legend: { show:false, location: 'e' }\n")
   else
      file:write ("                        legend: { show:true, location: 'e' }\n")
   end
   file:write ("                      }\n")
   file:write ("          );\n")
   file:write ("          $(\"#chart_cat_"..hchart._id.."\").bind('jqplotDataHighlight', function(ev, seriesIndex, pointIndex, data) {\n")
   file:write ("            var $this = $(this);\n")
   file:write ("            $this.attr('title', data[0] + \": \" + data[1]);\n")
   file:write ("          });\n")
   file:write ("          $(\"#chart_cat_"..hchart._id.."\").bind('jqplotDataUnhighlight', function(ev, seriesIndex, pointIndex, data) {\n")
   file:write ("            var $this = $(this);\n")
   file:write ("            $this.attr('title',\"\");\n")
   file:write ("          });\n")

   if  hchart ~= nil
   and hchart.is_action == true then
      file:write ("          $(\"#chart_cat_"..hchart._id.."\").bind('jqplotDataClick', function (ev, seriesIndex, pointIndex, data) {\n")
      for i, line in pairs (hchart.loc_actions) do
         for j, action in pairs (line) do
            file:write ("         if(seriesIndex == "..(i - 1).." && pointIndex == "..(j - 1).."){\n")
            _print_action (file, action)
            file:write ("         }\n")
         end
      end
      
      for i, action in pairs (hchart.row_actions) do
         file:write ("            if(seriesIndex == "..(i - 1).."){\n")
         _print_action (file, action)
         file:write ("            }\n")
      end

      for j, action in pairs (hchart.col_actions) do
         file:write ("            if(pointIndex == "..(j - 1).."){\n")
         _print_action (file, action)
         file:write ("            }\n")
      end
      
      if hchart.reg_action ~= nil then
         _print_action (file, hchart.reg_action)
      end
      file:write ("          });\n");
   end
      
   file:write ("        });\n")
   file:write ("      </script>")
   
   return true
end



function HTML:_html_display_bar_and_line_chart (file, hchart)
   -- Build serie names if possible
   local _series = ""
   for i, _ in ipairs (hchart.data) do
      _series = _series.."{"
      if  hchart.header.series ~= nil
      and hchart.header.series[i] ~= nil then
         _series = _series.."label:'"..hchart.header.series[i].."', "
      end
         
      -- Select the correct JS renderer according to options.type
      -- Only lines are searched because bar it the default renderer
      if  hchart.header.type ~= nil
      and hchart.header.type[i] ~= nil
      and hchart.header.type[i] == "line" then
         _series = _series.."renderer: $.jqplot.LineRenderer, "
      end
      
      -- Select the correct y-axis according to options.yaxis
      if  hchart.header.yaxis ~= nil
      and hchart.header.yaxis[i] ~= nil
      and hchart.header.yaxis[i] == 1 then
         _series = _series.."yaxis:'yaxis', "
      elseif  hchart.header.yaxis ~= nil
      and hchart.header.yaxis[i] ~= nil
      and hchart.header.yaxis[i] == 2 then
         _series = _series.."yaxis:'y2axis', "
      end

      _series = _series.."},"
      
   end
   
   -- Build x-axis legend based on the first serie keys
   local _xlegends = ""
   local is_xticks_diag = false
   for i, serie in ipairs (hchart.data) do
      _xlegends = "["
      for _, sdata in ipairs (serie) do
         if type (sdata.key) ~= "string" then
            -- TODO error invalid parameter
            return false
         end
         _xlegends = _xlegends.."'"..sdata.key.."',"
      end
      _xlegends = _xlegends.."]"

      -- Magic code to select if X legend must be horizontal or leaning
      -- Might not be adapted to all window sizes ...
      if string.len (_xlegends) > 150 then
         is_xticks_diag = true
      end
      break
   end   
   
   -- Build the data string needed to create the chart
   local _data = ""
   for i, serie in ipairs (hchart.data) do
      _data = _data.."["
      for _, sdata in ipairs (serie) do
         if type (sdata.key) ~= "string"
         or type (sdata.value) ~= "number" then
            -- TODO error invalid parameter
            return false
         end
         _data = _data..sdata.value..","
      end
      _data = _data.."],"
   end

   if _data == "" then
      return false
   end

   file:write ("<div style=\"margin: 10px; overflow-x:"..(hchart.header.overflow_x or "auto").."; overflow-y:hidden;\"><div id=\"chart_cat_"..hchart._id.."\" style=\"height:"..(hchart.header.height or "500").."px;\" ></div></div>\n")
   file:write ("<script class=\"code\" type=\"text/javascript\">\n")
   file:write ("  $(document).ready(function(){\n")

   file:write ("  var plot"..hchart._id.."_data = [".._data.."];\n")
   file:write ("  var plot"..hchart._id.."_ticks = ".._xlegends..";\n")

   file:write ("  var plot"..hchart._id.." = jQuery.jqplot ('chart_cat_"..hchart._id.."', plot"..hchart._id.."_data, {\n")
   file:write ("    seriesDefaults: {\n")
   file:write ("      renderer: jQuery.jqplot.BarRenderer,\n")
   file:write ("      rendererOptions: {},\n")
   file:write ("      pointLabels: {\n")
   if hchart.header.is_point_labels == false then
      file:write ("        show: false\n")
   else
      file:write ("        show: true\n")
   end
   file:write ("      },\n")
   file:write ("    },\n")
   file:write ("    axes: {\n")
   -- X axis
   file:write ("      xaxis: {\n")
   file:write ("        renderer: $.jqplot.CategoryAxisRenderer,\n")
   if _xlegends ~= nil then
      file:write ("        ticks: plot"..hchart._id.."_ticks,\n")
   end
   if type (hchart.header.x_name) == "string" then
      file:write ("        label: '"..hchart.header.x_name.."',\n")
      file:write ("        labelRenderer: $.jqplot.CanvasAxisLabelRenderer,\n")
   end
   if is_xticks_diag == true then
      file:write ("        tickRenderer: $.jqplot.CanvasAxisTickRenderer ,\n")
      file:write ("        tickOptions: {\n")
      file:write ("          angle: -30,\n")
      file:write ("          fontSize: '10pt'\n")
      file:write ("        }\n")
   end
   file:write ("      },\n")
   -- Y axis
   file:write ("      yaxis: {\n")
   if type (hchart.header.y_name) == "string" then
      file:write ("        label: '"..hchart.header.y_name.."',\n")
      file:write ("        labelRenderer: $.jqplot.CanvasAxisLabelRenderer,\n")
   end
   file:write ("        autoscale:true,\n")
   if type (hchart.header.y_min) == "number" then
      file:write ("       min:"..hchart.header.y_min..",\n")
   else
      file:write ("       min:0,\n")
   end
   file:write ("      },\n")
   -- Y2 axis
   file:write ("      y2axis: {\n")
   if type (hchart.header.y2_name) == "string" then
      file:write ("        label: '"..hchart.header.y2_name.."',\n")
      file:write ("        labelRenderer: $.jqplot.CanvasAxisLabelRenderer,\n")
   end
   file:write ("        autoscale:true,\n")
   if type (hchart.header.y2_min) == "number" then
      file:write ("       min:"..hchart.header.y2_min..",\n")
   else
      file:write ("       min:0,\n")
   end
   file:write ("      }\n")
   file:write ("    },\n")

   file:write ("    highlighter: {\n")
   file:write ("      tooltipContentEditor: function (str, seriesIndex, pointIndex) {\n")
   file:write ("        return plot"..hchart._id.."_ticks[pointIndex]+' - '+plot"..hchart._id.."_data[seriesIndex][pointIndex];\n")
   file:write ("      },\n")
   file:write ("      show: true,\n")
   file:write ("      showTooltip: true,\n")
   file:write ("      sizeAdjust: 10,\n")
   file:write ("      formatString: '%s',\n")
   file:write ("      tooltipLocation: 'n',\n")
   file:write ("      useAxesFormatters: false,\n")
   file:write ("    },\n")

   if hchart.header.legend == "n"
   or hchart.header.legend == "s"
   or hchart.header.legend == "e"
   or hchart.header.legend == "w" then
      file:write ("    legend: {\n")
      file:write ("       renderer: $.jqplot.EnhancedLegendRenderer,\n")
      file:write ("       show: true, \n")
      file:write ("       location: '"..hchart.header.legend.."', \n")
      file:write ("       placement: 'outsideGrid',\n")
      file:write ("       rendererOptions: {\n")
      file:write ("          numberRows: '1',\n")
      file:write ("       },\n")
      file:write ("    },\n")
   elseif hchart.header.legend == "none" then
      file:write ("    legend: {\n")
      file:write ("       show: false, \n")
      file:write ("    },\n")
   else
      file:write ("    legend: {\n")
      file:write ("       renderer: $.jqplot.EnhancedLegendRenderer,\n")
      file:write ("       show: true, \n")
      file:write ("       location: 's', \n")
      file:write ("       placement: 'outsideGrid',\n")
      file:write ("       rendererOptions: {\n")
      file:write ("          numberRows: '1',\n")
      file:write ("       },\n")
      file:write ("    },\n")
   end
   if _series ~= nil then
      file:write ("    series: [".._series.."],\n")
   end
   file:write ("  });\n")

   -- Handle actions
   if  hchart ~= nil
   and hchart.is_action == true then
      file:write ("  $(\"#chart_cat_"..hchart._id.."\").bind('jqplotDataClick', function (ev, seriesIndex, pointIndex, data) {\n")
      for i, line in pairs (hchart.loc_actions) do
         for j, action in pairs (line) do
            file:write ("    if(seriesIndex == "..(i - 1).." && pointIndex == "..(j - 1).."){\n")
            _print_action (file, action)
            file:write ("    }\n")
         end
      end
      
      for i, action in pairs (hchart.row_actions) do
         file:write ("       if(seriesIndex == "..(i - 1).."){\n")
         _print_action (file, action)
         file:write ("       }\n")
      end
      
      for j, action in pairs (hchart.col_actions) do
         file:write ("       if(pointIndex == "..(j - 1).."){\n")
         _print_action (file, action)
         file:write ("       }\n")
      end
      
      if hchart.reg_action ~= nil then
         _print_action (file, hchart.reg_action)
      end
      file:write ("});\n");
   end
   
   
   file:write ("});\n")
   file:write ("</script>")
end




function HTML:_html_display_zoomable_bar_chart (file, hchart)
   file:write ("<div style=\"margin: 10px; overflow-x:auto; overflow-y:hidden;\"><div id=\"chart_cat_"..hchart._id.."\" style=\"height:500px;\" ></div></div>\n")
   file:write ("<script class=\"code\" type=\"text/javascript\">\n")
   
   local ordinals = "var ordinals = ["
   local data = "var data = ["

   for i, val in ipairs (hchart.data[1]) do
      if i == 1 then
         ordinals   = ordinals.."'"..val.key.."'"
         data       = data.."{value: "..val.value.."}"
      else
         ordinals   = ordinals..",'"..val.key.."'"
         data       = data..",{value: "..val.value.."}"
      end
   end
   ordinals   = ordinals.."];"
   data       = data.."];"
   
   file:write ("  $(document).ready(function(){\n")
   file:write ("    "..ordinals.."\n")
   file:write ("    "..data.."\n")
   file:write ("    var widthgl  = 900;\n")
   file:write ("    var heightgl = 400;\n")
   file:write ("    var widthin  = 900;\n")
   file:write ("    var heightin = 420;\n")
   file:write ("    let margin = {\n")
   file:write ("      top:    10,\n")
   file:write ("      right:  30,\n")
   file:write ("      bottom: 20,\n")
   file:write ("      left:   50\n")
   file:write ("    },\n")
   file:write ("    width = widthgl - margin.left - margin.right,\n")
   file:write ("    height = heightgl - margin.top - margin.bottom,\n")
   file:write ("    radius = (Math.min(width, height) / 2) - 10,\n")
   file:write ("    node;\n")
   file:write ("    const svg = d3.select('#chart_cat_"..hchart._id.."')\n")
   file:write ("        .append('svg')\n")
   file:write ("        .attr('width', widthin)\n")
   file:write ("        .attr('height', heightin)\n")
   file:write ("        .append('g')\n")
   file:write ("        .attr('transform', `translate(${margin.left}, ${margin.top})`)\n")
   file:write ("        .call(\n")
   file:write ("          d3.zoom()\n")
   file:write ("            .translateExtent([[0,0], [width, height] ])\n")
   file:write ("            .extent([[0, 0], [width, height] ])\n")
   file:write ("          .on('zoom', zoom)\n")
   file:write ("        ).on(\"dblclick.zoom\", null);\n")
   file:write ("    let x = d3.scaleLinear().range([0, width])\n")
   file:write ("    let y = d3.scaleLinear().range([height, 0])\n")
   file:write ("    let color = d3.scaleOrdinal(d3.schemeCategory10)\n")
   file:write ("    let xScale = x.domain([-1, ordinals.length])\n")
   file:write ("    let yScale = y.domain([0, d3.max(data, function(d){return d.value})])\n")
   file:write ("    let xBand = d3.scaleBand().domain(d3.range(-1, ordinals.length)).range([0, width])\n")
   file:write ("    svg.append('rect')\n")
   file:write ("      .attr('class', 'zoom-panel')\n")
   file:write ("      .attr('width', width)\n")
   file:write ("      .attr('height', height)\n")
   file:write ("    let xAxis = svg.append('g')\n")
   file:write ("      .attr('class', 'xAxis')\n")
   file:write ("      .attr('transform', `translate(0, ${height})`)\n")
   file:write ("      .call(\n")
   file:write ("        d3.axisBottom(xScale).tickFormat((d, e) => {\n")
   file:write ("          return ordinals[d]\n")
   file:write ("        })\n")
   file:write ("      );\n")
   file:write ("    svg.append(\"text\")\n")
   file:write ("      .attr(\"transform\",\n")
   file:write ("            \"translate(\" + (width/2) + \" ,\" + (height + margin.top + 30) + \")\")\n")
   file:write ("      .style(\"text-anchor\", \"middle\")\n")
   file:write ("      .text(\"MAQAO thread rank\");\n") --TODO
   file:write ("    let yAxis = svg.append('g')\n")
   file:write ("      .attr('class', 'y axis')\n")
   file:write ("      .call(d3.axisLeft(yScale));\n")
   file:write ("    svg.append(\"text\")\n")
   file:write ("      .attr(\"transform\", \"rotate(-90)\")\n")
   file:write ("      .attr(\"y\", 0 - margin.left)\n")
   file:write ("      .attr(\"x\",0 - (height / 2))\n")
   file:write ("      .attr(\"dy\", \"1em\")\n")
   file:write ("      .style(\"text-anchor\", \"middle\")\n")
   file:write ("      .text(\""..hchart.header.series[1].."\");\n")
   file:write ("    let bars = svg.append('g')\n")
   file:write ("      .attr('clip-path','url(#my-clip-path)')\n")
   file:write ("      .selectAll('.bar')\n")
   file:write ("      .data(data)\n")
   file:write ("      .enter()\n")
   file:write ("      .append('rect')\n")
   file:write ("      .attr('class', 'bar')\n")
   file:write ("      .attr('x', function(d, i){\n")
   file:write ("        return xScale(i) - xBand.bandwidth()*0.9/2\n")
   file:write ("      })\n")
   file:write ("      .attr('y', function(d, i){\n")
   file:write ("        return yScale(d.value)\n")
   file:write ("      })\n")
   file:write ("      .attr('width', xBand.bandwidth()*0.9)\n")
   file:write ("      .attr('height', function(d){\n")
   file:write ("        return height - yScale(d.value)\n")
   file:write ("      })\n")
   file:write ("    let defs = svg.append('defs')\n")
   file:write ("      defs.append('clipPath')\n")
   file:write ("        .attr('id', 'my-clip-path')\n")
   file:write ("        .append('rect')\n")
   file:write ("        .attr('width', width)\n")
   file:write ("        .attr('height', height)\n")
   file:write ("      let hideTicksWithoutLabel = function() {\n")
   file:write ("        d3.selectAll('.xAxis .tick text').each(function(d){\n")
   file:write ("          if(this.innerHTML === '') {\n")
   file:write ("            this.parentNode.style.display = 'none';\n")
   file:write ("          }\n")
   file:write ("        })\n")
   file:write ("      }\n")
   file:write ("      function zoom() {\n")
   file:write ("        if (d3.event.transform.k < 1) {\n")
   file:write ("          d3.event.transform.k = 1;\n")
   file:write ("          return;\n")
   file:write ("        }\n")
   file:write ("      xAxis.call(\n")
   file:write ("        d3.axisBottom(d3.event.transform.rescaleX(xScale)).tickFormat((d, e, target) => {\n")
   file:write ("          if (Math.floor(d) === d3.format(\".1f\")(d)) return ordinals[Math.floor(d)]\n")
   file:write ("            return ordinals[d];\n")
   file:write ("        })\n")
   file:write ("      )\n")
   file:write ("      hideTicksWithoutLabel()\n")
   file:write ("        bars.attr(\"transform\", \"translate(\" + d3.event.transform.x+\",0)scale(\" + d3.event.transform.k + \",1)\")\n")
   file:write ("    }\n")
   file:write ("  });\n")

   file:write ("</script>\n")
   
end



local function _print_actions_for_table (file, htable, no_rclic)
   if  htable ~= nil
   and htable.is_action == true then
      file:write ("<script>\n")
      for _, obj in ipairs (no_rclic) do
         file:write ("document.getElementById(\""..obj.."\").addEventListener('contextmenu', event => event.preventDefault());\n")
      end
      for _, action in pairs (htable.actions) do
         file:write ("function _action_"..action.id.." (obj, i, j, p){\n")
         _print_action (file, action, "reg")
         file:write ("}\n")
      end
      file:write ("</script>\n")
   end
end




local function _print_actions_menu_for_table (file, htable, link_menu_ids)
   if htable.is_action_menu then
      -- CSS to hide the menu
      file:write ("<style>\n")
      for i, hmenu in pairs (htable.action_menus) do
         file:write ("#mctxMenu"..hmenu.id.." {\n")
         file:write ("  display:none;\n")
         file:write ("  z-index:100;\n")
         file:write ("}\n")
      end
      file:write ("</style>\n")

      -- HTML code
      for i, hmenu in pairs (htable.action_menus) do
         file:write ("<mrmenu id=\"mctxMenu"..hmenu.id.."\">\n")
         for j, action in pairs (hmenu.actions) do
            file:write ("  <mrmenu id=\"mctxMenu"..hmenu.id.."_"..j.."\" "..
                          "title = \""..action.text.."\" "..
                          "onclick=\"_action_"..action.action.id.."('_p', '_p"..hmenu.id.."_"..j.."')\" "..
                          "oncontextmenu=\"_action_"..action.action.id.."('_p', '_p"..hmenu.id.."_"..j.."')\" "..
                          "></mrmenu>\n")
         end
         file:write ("</mrmenu>\n")
      end

      -- Javascript
      file:write ("<script>\n")
      -- Some global variables
      file:write ("var _global_elem;\n")
      file:write ("var _ids;\n")
      file:write ("\n")

      for i, hmenu in pairs (htable.action_menus) do
         -- Disable built-in right click menu for menu element
         file:write ("document.getElementById(\"mctxMenu"..hmenu.id.."\").addEventListener('contextmenu', event => event.preventDefault());\n")
         for j, haction in ipairs (hmenu.actions) do
            file:write ("document.getElementById(\"mctxMenu"..hmenu.id.."_"..j.."\").addEventListener('contextmenu', event => event.preventDefault());\n")
         end
         file:write ("\n")
         -- List of element the menu is attached to
         file:write ("_ids = [")
         for j, id in ipairs (link_menu_ids[hmenu.id]) do
            if j == 1 then
               file:write ("\""..id.."\"")
            else
               file:write (", \""..id.."\"")
            end
         end
         file:write ("];\n")

         -- Attach the menu to table elements
         file:write ("for (var i = 0; i < _ids.length; i++) {\n")
         file:write ("  var elem = document.getElementById(_ids[i]);\n")
         file:write ("  elem.addEventListener(\"contextmenu\",function(event){\n")
         file:write ("    event.preventDefault();\n")
         file:write ("    var ctxMenu = document.getElementById(\"mctxMenu"..hmenu.id.."\");\n")
         file:write ("    ctxMenu.style.display = \"block\";\n")
         file:write ("    ctxMenu.style.left = (event.pageX - 10)+\"px\";\n")
         file:write ("    ctxMenu.style.top = (event.pageY - 10)+\"px\";\n")
         file:write ("    _global_elem = event.target || event.srcElement;\n")
         for j, hmenu1 in pairs (htable.action_menus) do
            if hmenu1.id ~= hmenu.id then
               file:write ("    ctxMenu = document.getElementById(\"mctxMenu"..hmenu1.id.."\");\n")
               file:write ("    ctxMenu.style.display = \"\";\n")
               file:write ("    ctxMenu.style.left = \"\";\n")
               file:write ("    ctxMenu.style.top = \"\";\n")
            end
         end
         file:write ("  },false);\n")
         file:write ("  document.addEventListener(\"click\",function(event){\n")
         file:write ("    var ctxMenu = document.getElementById(\"mctxMenu"..hmenu.id.."\");\n")
         file:write ("    ctxMenu.style.display = \"\";\n")
         file:write ("    ctxMenu.style.left = \"\";\n")
         file:write ("    ctxMenu.style.top = \"\";\n")
         file:write ("  },false);\n")
         file:write ("}\n")
         file:write ("\n")

         -- Code for each action
         for _, action in pairs (hmenu.actions) do
            file:write ("function _action_"..action.action.id.." (p, pm){\n")
            _print_action (file, action.action, "obj")
            file:write ("}\n")
         end
      end
      file:write ("</script>\n")
   end
end



local function _html_display_table (file, htable)

   -- DIV containing the table
   if htable.type == "paged" then
      file:write ("<div class=\"_div_table\" id=\"_paged_"..htable._id.."\">")
   else
      file:write ("<div class=\"_div_table\">")
   end


   -- If needed, table header used to hide optionnal colums
   local is_optionnal_column = false
   for i, head in ipairs (htable.header) do
      if head.is_optionnal == true then
         is_optionnal_column = true
      end
   end

   if is_optionnal_column == true then
      file:write ("<div><form>")
      for i, head in ipairs (htable.header) do
         if head.is_optionnal == true then
            file:write ("<input class=\"input_check_table_menu\" type=\"checkbox\" "..
                                "onclick=\"click_table_menu('_table"..htable._id.."_opt"..i.."')\" style=\"margin-left: 30px;  vertical-align: middle;\"")
            if head.is_displayed == true then
               file:write (" checked ")
            end
            file:write (" autocomplete=\"off\" >")
            if type (head.col_name) == "string" then
               file:write (head.col_name)
            else
               file:write (head.name)
            end
            file:write ("\n")
         end
      end
      file:write ("</form></div>\n")
   end


   -- TABLE tag
   if htable.type == "paged" then
      file:write ("<table class=\"_paged_table\">")
   else
      file:write ("<table class=\"_fixed_table\">")
   end

   local no_rclic       = {}
   local link_menu_ids  = {}
   
   -- TODO move it into the table structure
   local nb_elem_per_paged = 10
   local page_id           = 0

   -- Print header
   file:write ("<tr>")
   for i, head in ipairs (htable.header) do
      local class = ""
      local title = ""

      if i == 1 then
         class = class.." _left"
      end
      if head.is_optionnal == true then
         class = class.." _table"..htable._id.."_opt"..i
         if head.is_displayed ~= true then
            class = class.." _tcol_hidden"
         end
      end
      if head.desc ~= nil then
         class = class.." tooltip-maqao"
         title = head.desc
      end

      file:write ("<th")
      if class ~= "" then
         file:write (" class=\""..class.."\"")
      end
      if title ~= "" then
         file:write (" title=\""..title.."\"")
      end
      file:write ("><span></span>")

      
      if type (head.col_name) == "string" then 
         file:write (head.col_name)
      else
         file:write (head.name)
      end

      file:write ("</th>")
   end
   file:write ("</tr>")

   -- Print data
   for i, line in ipairs (htable.data) do
      file:write ("<tr")
      local class = ""
      if htable.type == "paged" then
         class = class.." _paged_"..htable._id.."_"..page_id
         if page_id > 0 then
            class = class.." _paged_hidden"
         end
         if i > 0 and i % nb_elem_per_paged == 0 then
            page_id = page_id + 1;
         end
      end
      if class ~= "" then
         file:write (" class=\""..class.."\" ")
      end
      
      file:write (" id=\"_tr_"..htable._id.."_"..i.."\" ")
      local actions_parameters_row = ""
      if htable.row_actions[i] ~= nil then
         for ia, action in ipairs (htable.row_actions[i]) do
            if action.type == "a" then
               if action.action.trigger == "lclick" then
                  file:write (" onclick=\"_action_"..action.action.id.."(this,"..i..",-1, '"..tostring(action.param).."');\" ")
               elseif action.action.trigger == "dblclick" then
                  file:write (" ondblclick=\"_action_"..action.action.id.."(this,"..i..",-1, '"..tostring(action.param).."');\" ")
               elseif action.action.trigger == "rclick" then
                  file:write (" oncontextmenu=\"_action_"..action.action.id.."(this,"..i..",-1, '"..tostring(action.param).."');\" ")
                  table.insert (no_rclic, "_tr_"..htable._id.."_"..i)
               end
            elseif action.type == "m" then
               if link_menu_ids[action.action.id] == nil then
                  link_menu_ids[action.action.id] = {}
               end
               table.insert (link_menu_ids[action.action.id], "_tr_"..htable._id.."_"..i)
               if action.param ~= nil then
                  actions_parameters_row = actions_parameters_row.." _p = \""..tostring (action.param).."\" "
               end

               for j, a in pairs (action.action.actions) do
                  if a.param ~= nil then
                     actions_parameters_row = actions_parameters_row.." _p"..action.action.id.."_"..j.." = \""..tostring (a.param).."\""
                  end
               end
            end
         end
      end
      file:write (">")


      for j, head in ipairs (htable.header) do
         file:write ("<td")
         file:write (" id=\"_td_"..htable._id.."_"..i.."_"..j.."\" ")
         file:write (" _i=\""..i.."\" _j=\""..j.."\" ")
         file:write (actions_parameters_row)

         -- Handle classes
         local class = ""
         if j == 1 then
            class = class.." _left"
         end
         if htable.header[j].is_centered then
            class = class.." _centered"
         end
         if htable.header[j].is_optionnal == true then
            class = class.." _table"..htable._id.."_opt"..j
            if htable.header[j].is_displayed ~= true then
               class = class.." _tcol_hidden"
            end
         end
         if class ~= "" then
            file:write (" class=\""..class.."\" ")
         end
         
         -- Handle styles
         local style = ""
         if  type (htable.header[j].width) == "number"
         and htable.header[j].width > 0 then
            style = style.."width:"..htable.header[j].width.."%;"
         end
         if htable.header[j].word_break == true then
            style = style.."word-break:break-all;"
         end
         
         if htable.custom_style ~= nil then
            local cst_style = htable.custom_style(i, j, line[head.name])
            if cst_style ~= nil then
               style = style..cst_style
            end
         end
         
         if style ~= "" then
            file:write (" style=\""..style.."\" ")
         end

         -- Handle actions
         local lactions = ""
         local dlactions = ""
         local ractions = ""

         if (htable.col_actions[j] ~= nil) then
            for ia, action in ipairs (htable.col_actions[j]) do
               if action.type == "a" then
                  if action.action.trigger == "lclick" then
                     lactions = lactions.."_action_"..action.action.id.."(this,-1,"..j..", '"..tostring (action.param).."');"
                  elseif action.action.trigger == "dblclick" then
                     dlactions = dlactions.."_action_"..action.action.id.."(this,-1,"..j..", '"..tostring (action.param).."');"
                  elseif action.action.trigger == "rclick" then
                     ractions = ractions.."_action_"..action.action.id.."(this,-1,"..j..", '"..tostring (action.param).."');"
                     table.insert (no_rclic, "_tr_"..htable._id.."_"..i)
                  end
               elseif action.type == "m" then
                  if link_menu_ids[action.action.id] == nil then
                     link_menu_ids[action.action.id] = {}
                  end
                  table.insert (link_menu_ids[action.action.id], "_td_"..htable._id.."_"..i.."_"..j)
                  if action.param ~= nil then
                     file:write (" _p = \""..tostring (action.param).."\" ")
                  end

                  for j, a in pairs (action.action.actions) do
                     if a.param ~= nil then
                        file:write (" _p"..action.action.id.."_"..j.." = \""..tostring (a.param).."\" ")
                     end
                  end
               end
            end
         end
         if  htable.loc_actions ~= nil
         and htable.loc_actions[i] ~= nil
         and htable.loc_actions[i][j] ~= nil then
            for ia, action in ipairs (htable.loc_actions[i][j]) do
               if action.type == "a" then
                  if action.action.trigger == "lclick" then
                     lactions = lactions.."_action_"..action.action.id.."(this,"..i..","..j..", '"..tostring (action.param).."');"
                  elseif action.action.trigger == "dblclick" then
                     dlactions = dlactions.."_action_"..action.action.id.."(this,"..i..","..j..", '"..tostring (action.param).."');"
                  elseif action.action.trigger == "rclick" then
                     ractions = ractions.."_action_"..action.action.id.."(this,"..i..","..j..", '"..tostring (action.param).."');"
                     table.insert (no_rclic, "_tr_"..htable._id.."_"..i)
                  end
               elseif action.type == "m" then
                  if link_menu_ids[action.action.id] == nil then
                     link_menu_ids[action.action.id] = {}
                  end
                  table.insert (link_menu_ids[action.action.id], "_td_"..htable._id.."_"..i.."_"..j)
                  if action.param ~= nil then
                     file:write (" _p = \""..tostring (action.param).."\" ")
                  end

                  for j, a in pairs (action.action.actions) do
                     if a.param ~= nil then
                        file:write (" _p"..action.action.id.."_"..j.." = \""..tostring (a.param).."\" ")
                     end
                  end
               end
            end
         end
         if htable.reg_action ~= nil then
            for ia, action in ipairs (htable.reg_action) do
               if action.type == "a" then
                  if action.action.trigger == "lclick" then
                     lactions = lactions.."_action_"..action.action.id.."(this,"..i..","..j..", '"..tostring (action.param).."');"
                  elseif action.action.trigger == "dblclick" then
                     dlactions = dlactions.."_action_"..action.action.id.."(this,"..i..","..j..", '"..tostring (action.param).."');"
                  elseif action.action.trigger == "rclick" then
                     ractions = ractions.."_action_"..action.action.id.."(this,"..i..","..j..", '"..tostring (action.param).."');"
                     table.insert (no_rclic, "_tr_"..htable._id.."_"..i)
                  end
               elseif action.type == "m" then
                  if link_menu_ids[action.action.id] == nil then
                     link_menu_ids[action.action.id] = {}
                  end
                  table.insert (link_menu_ids[action.action.id], "_td_"..htable._id.."_"..i.."_"..j)
                  if action.param ~= nil then
                     file:write (" _p = \""..tostring (action.param).."\" ")
                  end

                  for j, a in pairs (action.action.actions) do
                     if a.param ~= nil then
                        file:write (" _p"..action.action.id.."_"..j.." = \""..tostring (a.param).."\" ")
                     end
                  end
               end
            end
         end
         if dlactions ~= "" then
            file:write (" ondblclick=\""..dlactions.."\" ")
         end
         if lactions ~= "" then
            file:write (" onclick=\""..lactions.."\" ")
         end
         if ractions ~= "" then
            file:write (" oncontextmenu=\""..ractions.."\" ")
         end
         file:write (">")

         file:write (line[head.name])
         file:write ("</td>")
      end
     file:write ("</tr>\n")
   end

   file:write ("</table>")
   if htable.type == "paged" then
      file:write ("<div class=\"_paged_buttons\">")
      file:write ("<div class=\"_paged_table_left_button\" onclick=\"_click_paged_prev(this)\">&#x25C4; Prev</div><div id=\"_paged_tables_page_"..htable._id.."\" class=\"_paged_table_center_button\">Page 1/"..(page_id + 1).."</div><div class=\"_paged_table_right_button\" onclick=\"_click_paged_next(this)\">Next &#x25BA;</div>")
      file:write ("</div>")
   end
   file:write ("</div>")
   
   -- Print action functions
   _print_actions_for_table (file, htable, no_rclic)
   _print_actions_menu_for_table (file, htable, link_menu_ids)
end



function HTML:_html_display_paged_table (file, htable)
   return _html_display_table (file, htable)
end   



function HTML:_html_display_fixed_table (file, htable)
   return _html_display_table (file, htable)
end   




local function _html_display_tree_table_rec (file, htable, data, level, root_id, id, no_rclic, link_menu_ids, is_expended)
   for i, line in ipairs (data) do
      id = id + 1
      file:write ("<tr")
      local class = ""
      if level > 0 then
         if is_expended == true then
            class = root_id
         else
            class = "_treed_hidden "..root_id
         end
      end
      if class ~= "" then
         file:write (" class=\""..class.."\" ")
      end

      file:write (" id=\""..root_id.."_"..i.."\" ")

      local actions_parameters_row = ""
      if htable.row_actions[id] ~= nil then
         for ia, action in ipairs (htable.row_actions[id]) do
            if action.type == "a" then
               if action.action.trigger == "lclick" then
                  file:write (" onclick=\"_action_"..action.action.id.."(this,"..id..",-1, '"..tostring(action.param).."');\" ")
               elseif action.action.trigger == "dblclick" then
                  file:write (" ondblclick=\"_action_"..action.action.id.."(this,"..id..",-1, '"..tostring(action.param).."');\" ")
               elseif action.action.trigger == "rclick" then
                  file:write (" oncontextmenu=\"_action_"..action.action.id.."(this,"..id..",-1, '"..tostring(action.param).."');\" ")
                  table.insert (no_rclic, root_id.."_"..i)
               end
            elseif action.type == "m" then
               if link_menu_ids[action.action.id] == nil then
                  link_menu_ids[action.action.id] = {}
               end
               table.insert (link_menu_ids[action.action.id], root_id.."_"..i)
               if action.param ~= nil then
                  actions_parameters_row = actions_parameters_row.." _p = \""..tostring (action.param).."\""
               end

               for j, a in pairs (action.action.actions) do
                  if a.param ~= nil then
                     actions_parameters_row = actions_parameters_row.." _p"..action.action.id.."_"..j.." = \""..tostring (a.param).."\""
                  end
               end
            end
         end
      end
      file:write (">")

      for j, head in ipairs (htable.header) do
         file:write ("<td")
         file:write (" id=\""..root_id.."_"..i.."-"..j.."\" ")
         file:write (" _i=\""..id.."\" _j=\""..j.."\" ")
         file:write (actions_parameters_row)

         -- Handle classes
         local class = ""
         if j == 1 then
            class = class.." _left"
         end
         if htable.header[j].is_centered then
            class = class.." _centered"
         end
         if htable.header[j].is_optionnal == true then
            class = class.." _table"..htable._id.."_opt"..j
            if htable.header[j].is_displayed ~= true then
               class = class.." _tcol_hidden"
            end
         end
         if class ~= "" then
            file:write (" class=\""..class.."\" ")
         end

         -- Handle styles
         local style = ""
         if  type (htable.header[j].width) == "number"
         and htable.header[j].width > 0 then
            style = style.."width:"..htable.header[j].width.."%;"
         end
         if htable.header[j].word_break == true then
            style = style.."word-break:break-all;"
         end

         if style ~= "" then
            file:write (" style=\""..style.."\" ")
         end

         -- Handle actions
         local lactions = ""
         local dlactions = ""
         local ractions = ""

         if (htable.col_actions[j] ~= nil) then
            for ia, action in ipairs (htable.col_actions[j]) do
               if action.type == "a" then
                  if action.action.trigger == "lclick" then
                     lactions = lactions.."_action_"..action.action.id.."(this,-1,"..j..", '"..tostring (action.param).."');"
                  elseif action.action.trigger == "dblclick" then
                     dlactions = dlactions.."_action_"..action.action.id.."(this,-1,"..j..", '"..tostring (action.param).."');"
                  elseif action.action.trigger == "rclick" then
                     ractions = ractions.."_action_"..action.action.id.."(this,-1,"..j..", '"..tostring (action.param).."');"
                     table.insert (no_rclic, root_id.."_"..i)
                  end
               elseif action.type == "m" then
                  if link_menu_ids[action.action.id] == nil then
                     link_menu_ids[action.action.id] = {}
                  end
                  table.insert (link_menu_ids[action.action.id], root_id.."_"..i.."-"..j)
                  if action.param ~= nil then
                     file:write (" _p = \""..tostring (action.param).."\" ")
                  end

                  for jj, a in pairs (action.action.actions) do
                     if a.param ~= nil then
                        file:write (" _p"..action.action.id.."_"..jj.." = \""..tostring (a.param).."\" ")
                     end
                  end
               end
            end
         end
         if  htable.loc_actions ~= nil
         and htable.loc_actions[id] ~= nil
         and htable.loc_actions[id][j] ~= nil then
            for ia, action in ipairs (htable.loc_actions[id][j]) do
               if action.type == "a" then
                  if action.action.trigger == "lclick" then
                     lactions = lactions.."_action_"..action.action.id.."(this,"..id..","..j..", '"..tostring (action.param).."');"
                  elseif action.action.trigger == "dblclick" then
                     dlactions = dlactions.."_action_"..action.action.id.."(this,"..id..","..j..", '"..tostring (action.param).."');"
                  elseif action.action.trigger == "rclick" then
                     ractions = ractions.."_action_"..action.action.id.."(this,"..id..","..j..", '"..tostring (action.param).."');"
                     table.insert (no_rclic, root_id.."_"..i)
                  end
               elseif action.type == "m" then
                  if link_menu_ids[action.action.id] == nil then
                     link_menu_ids[action.action.id] = {}
                  end
                  table.insert (link_menu_ids[action.action.id], root_id.."_"..i.."-"..j)
                  if action.param ~= nil then
                     file:write (" _p = \""..tostring (action.param).."\" ")
                  end

                  for jj, a in pairs (action.action.actions) do
                     if a.param ~= nil then
                        file:write (" _p"..action.action.id.."_"..jj.." = \""..tostring (a.param).."\" ")
                     end
                  end
               end
            end
         end
         if htable.reg_action ~= nil then
            for ia, action in ipairs (htable.reg_action) do
               if action.type == "a" then
                  if action.action.trigger == "lclick" then
                     lactions = lactions.."_action_"..action.action.id.."(this,"..id..","..j..", '"..tostring (action.param).."');"
                  elseif action.action.trigger == "dblclick" then
                     dlactions = dlactions.."_action_"..action.action.id.."(this,"..id..","..j..", '"..tostring (action.param).."');"
                  elseif action.action.trigger == "rclick" then
                     ractions = ractions.."_action_"..action.action.id.."(this,"..id..","..j..", '"..tostring (action.param).."');"
                     table.insert (no_rclic, root_id.."_"..i)
                  end
               elseif action.type == "m" then
                  if link_menu_ids[action.action.id] == nil then
                     link_menu_ids[action.action.id] = {}
                  end
                  table.insert (link_menu_ids[action.action.id], root_id.."_"..i.."-"..j)
                  if action.param ~= nil then
                     file:write (" _p = \""..tostring (action.param).."\" ")
                  end

                  for jj, a in pairs (action.action.actions) do
                     if a.param ~= nil then
                        file:write (" _p"..action.action.id.."_"..jj.." = \""..tostring (a.param).."\" ")
                     end
                  end
               end
            end
         end
         if dlactions ~= "" then
            file:write (" ondblclick=\""..dlactions.."\" ")
         end
         if lactions ~= "" then
            file:write (" onclick=\""..lactions.."\" ")
         end
         if ractions ~= "" then
            file:write (" oncontextmenu=\""..ractions.."\" ")
         end

         file:write (">")

         if  line.__children ~= nil
         and j == 1 then
            if is_expended == true then
               file:write ("<span onclick=\"_click_treed(this)\" style=\"margin-right:5px;margin-left:"..(15 * level).."px;\" >&#x25BC</span>")
            else
               file:write ("<span onclick=\"_click_treed(this)\" style=\"margin-right:5px;margin-left:"..(15 * level).."px;\" >&#x25BA</span>")
            end
         elseif j == 1 then
            file:write ("<span style=\"margin-right:10px;margin-left:"..(3 + 15 * level).."px;\" >&#x25CB;</span>")
         end

         if line[head.name] ~= nil then
            file:write (line[head.name])
         end

         if  line.__children ~= nil 
         and j == 1 then
            if is_expended == true then
               file:write ("<span onclick=\"_click_expand(this)\" class=\"span_expand\">&#150;</span>")
            else
               file:write ("<span onclick=\"_click_expand(this)\" class=\"span_expand\">&#43;</span>")
            end
         end

         file:write ("</td>")
      end
      file:write ("</tr>\n")

      if  line.__children ~= nil then
         id = _html_display_tree_table_rec (file, htable, line.__children, level + 1, root_id.."_"..i, id, no_rclic, link_menu_ids, is_expended)
      end
   end

   return id
end



function HTML:_html_display_tree_table (file, htable, is_expended)
   local no_rclic       = {}
   local link_menu_ids  = {}

   file:write ("<div class=\"_div_table\">")

   -- If needed, table header used to hide optionnal colums
   local is_optionnal_column = false
   for i, head in ipairs (htable.header) do
      if head.is_optionnal == true then
         is_optionnal_column = true
      end
   end

   if is_optionnal_column == true then
      file:write ("<div><form>")
      for i, head in ipairs (htable.header) do
         if head.is_optionnal == true then
            file:write ("<input class=\"input_check_table_menu\" type=\"checkbox\" "..
                                "onclick=\"click_table_menu('_table"..htable._id.."_opt"..i.."')\" style=\"margin-left: 30px;  vertical-align: middle;\"")
            if head.is_displayed == true then
               file:write (" checked ")
            end
            file:write (">")
            if type (head.col_name) == "string" then
               file:write (head.col_name)
            else
               file:write (head.name)
            end
            file:write ("\n")
         end
      end
      file:write ("</form></div>\n")
   end
   
   file:write ("<table id=\"_treed_table_"..htable._id.."\" class=\"_treed_table\">") 
     
   -- Print header
   file:write ("<tr>")
   for i, head in ipairs (htable.header) do
      local class = ""
      local title = ""

      if i == 1 then
         class = class.." _left"
      end
      if head.is_optionnal == true then
         class = class.." _table"..htable._id.."_opt"..i
         if head.is_displayed ~= true then
            class = class.." _tcol_hidden"
         end
      end
      if head.desc ~= nil then
         class = class.." tooltip-maqao"
         title = head.desc
      end

      file:write ("<th")
      if class ~= "" then
         file:write (" class=\""..class.."\"")
      end
      if title ~= "" then
         file:write (" title=\""..title.."\"")
      end
      file:write (">")
      if type (head.col_name) == "string" then 
         file:write (head.col_name)
      else
         file:write (head.name)
      end

      file:write ("</th>")
   end
   file:write ("</tr>\n")

   -- Print data
   _html_display_tree_table_rec (file, htable, htable.data, 0, "_tr_"..htable._id, 0, no_rclic, link_menu_ids, is_expended)

   file:write ("</table>")
   file:write ("</div>\n")

   -- Print action functions
   _print_actions_for_table (file, htable, no_rclic)
   _print_actions_menu_for_table (file, htable, link_menu_ids)
end


