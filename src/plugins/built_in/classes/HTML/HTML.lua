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

HTML._nb_tabs_table   = 0
HTML._nb_actions      = 0
HTML._nb_action_menus = 0
HTML._debug           = false


-- ----------------------------------------------------------------------------
--- Escape a string to remove forbidden characters
-- @param str A string to escape
-- @return The escaped string
function HTML:escape_str (str)
   if type (str) == "string" then
      local res = ""  
      res = string.gsub (str, "&", "&amp;")
      res = string.gsub (res, "\"", "&quot;")
      res = string.gsub (res, "'", "&apos;")
      res = string.gsub (res, "<", "&lt;")
      res = string.gsub (res, ">", "&gt;")
      res = string.gsub (res, "\n", "<br>")
      res = string.gsub (res, "é", "&eacute;")
      res = string.gsub (res, "è", "&egrave;")
      res = string.gsub (res, "ê", "&ecirc;")
      res = string.gsub (res, "à", "&agrave;")
      res = string.gsub (res, "â", "&acirc;")
      return res
   else
      return str
   end
end





-- ----------------------------------------------------------------------------
--- Initialize an HTML directory to display MAQAO results
-- @param Name of the HTML directory to create
-- @return true if the directory has been created, else false
function HTML:init (html_directory)
   if type (html_directory) ~= "string" then
      -- TODO error invalid parameter
      return false
   end

   -- If the directory exists, remove it
   if fs.exists (html_directory) == true then
      if os.remove (html_directory) == nil then
         -- TODO error directory can be replaced
         return false
      end  
   end
   
   -- Create the directory
   lfs.mkdir (html_directory)
   if fs.exists (html_directory) == false then
      -- TODO error can not create the directory
      return false
   end
   
   -- Initialize the html directory 
   HTML:_html_generate_helper_files(html_directory)
   return true
end





-- ----------------------------------------------------------------------------
--- Create a new html file and write the file header
-- @param parameters A table describing all parameters for the HTML file
--                   - url = <string> Name of the file to create
--                   - title = <string> Title of the page
--                   - [custom_css] = <table> Each entry is the URL of a CSS to load in the header
--                   - [menu] = <table> Described a menu added in the HTML code. Each entry is a table 
--                     with the following structure:
--                     <menu_entry> = {
--                        name = <string>,
--                        link = <string>,
--                        sub  = {<menu_entry>, ...}
--                     }
--                   - [maqao_header] = <boolean> True (default) if the header must be inserted (background
--                                                and menu), else false
-- @param prefix_path An optionnal string added to all ressources paths. Needed if the file is created in
--                    a subdirectory in the HTML main directory. For example, if the file is created into
--                    <HTML_dir>/subdir, prefix_path must be "../"
-- @return an opened html file if the file has been created, else nil
function HTML:create_file (parameters, prefix_path)
   -- Check parameters table
   if type (parameters) ~= "table" then
      return nil
   end
   if type (parameters.url) ~= "string" then
      return nil
   end
   
   local hfile = HTML_file:new (parameters.url)
   if hfile == nil then
      -- TODO error can not create the file
      return nil
   end
   
   HTML:_html_output_print_header(hfile, parameters, prefix_path)
   
   -- If needed, generate the menu
   if parameters.maqao_header ~= false then
      if type (parameters.menu) == "table" then
         HTML:_html_output_insert_menu (hfile, parameters.menu, prefix_path)
      else 
         hfile:write ("<div id=\"page_title\">"..HTML:escape_str (parameters.title).."</div>")
      end
   end
   
   -- Create the main div 
   hfile:write("<div id=\"maqao_content\">")
   if parameters.maqao_header ~= false then
      hfile:write ("<div style=\"height:100px;\" ></div>")
   end
   hfile:write("\n")
   return hfile
end





-- ----------------------------------------------------------------------------
--- Close a HTML file opened with HTML:create_file.
--  The function adds some tags opened by HTML:create_file and close the file.
-- @param file
-- @return true if the file has been close, else false
function HTML:close_file (file)
   if file == nil then
      -- TODO error invalid parameter
      return false
   end
   
   file:write("\n")
   HTML:_html_output_print_footer(file)
      
   file:close ()
   return true
end





-- ----------------------------------------------------------------------------
--- Open an accordion box
-- @param file
-- @param title
-- @param is_active
-- @return true if the HTML code has been written, else false
function HTML:open_accordion_box (file, title, is_active)
   if file == nil then
      -- TODO error invalid parameter
      return false
   end
   
   local _title = ""
   if type (title) == "string" then
      _title = title
   end
   
   if HTML._debug then
      print ("open_accordion_box: "..title)
   end
      
   if is_active == false then
      file:write('<div class="_accordion_box collapsed">')
      file:write('<h3 id="_accordion_header_'..file.nb_accordions..'" class="_header collapsed" onclick="_click_accordion_header(this)" >')
      file:write('<div class="_accordion_box_button">&#x25B6;</div>'..HTML:escape_str (_title)..'</h3>')
      file:write('<div id="_accordion_content_'..file.nb_accordions..'" class="_content collapsed">')

      file.accordion_display[file.nb_accordions] = true
      file.is_acc_active = true

   else
      file:write('<div class="_accordion_box">')
      file:write('<h3 id="_accordion_header_'..file.nb_accordions..'" class="_header" onclick="_click_accordion_header(this)" >')
      file:write('<div class="_accordion_box_button">&#x25BC;</div>'..HTML:escape_str (_title)..'</h3>')
      file:write('<div id="_accordion_content_'..file.nb_accordions..'" class="_content">')

      file.accordion_display[file.nb_accordions] = false
      file.is_acc_not_active = true

   end
   file.nb_accordions = file.nb_accordions + 1
   
   return true
end





-- ----------------------------------------------------------------------------
--- Close the last accordion box
-- @param file
-- @return true if the HTML code has been written, else false
function HTML:close_accordion_box (file)
   if file == nil then
      -- TODO error invalid parameter
      return false
   end
   
   if HTML._debug then
      print ("close_accordion_box")
   end

   file:write("</div></div>")
   return true
end





-- ----------------------------------------------------------------------------
--- Open a fixed box
-- @param file
-- @param title
-- @param is_scrollable
-- @return true if the HTML code has been written, else false
function HTML:open_fixed_box (file, title, is_scrollable)
   if file == nil then
      -- TODO error invalid parameter
      return false
   end
   
   local _title = ""
   if type (title) == "string" then
      _title = title
   end
   
   if HTML._debug then
      print ("open_fixed_box: "..title)
   end
   
   file:write('<div class="_fixed_box" ')
   if is_scrollable then
      file:write('style="overflow-x:scroll;"')
   end
   file:write('>')
   file:write ('<h3 class="_header">'..HTML:escape_str (_title)..'</h3>')
   file:write ('<div class="_content">')

   return true
end





-- ----------------------------------------------------------------------------
--- Close the last fixed box
-- @param file
-- @return true if the HTML code has been written, else false
function HTML:close_fixed_box (file)
   if file == nil then
      -- TODO error invalid parameter
      return false
   end
   
   if HTML._debug then
      print ("close_fixed_box")
   end

   file:write("</div></div>")

   return true
end





-- ----------------------------------------------------------------------------
---
-- @param file An opened HTML file
--
-- @return true if the table has been created, else false
function HTML:create_tabs_table (file, headers)
   if file == nil then
      -- TODO error invalid parameter
      return false
   end
   if type (headers) ~= "table" 
   or headers[1] == nil then
      -- TODO error invalid parameter
      return false
   end

   if HTML._debug then
      print ("create_tabs_table")
   end
   
   file:write("<div class=\"cf_level_tabs\">")
   file:write("<ul>")
   for i, header in ipairs (headers) do
      file:write("<li><a href=\"#cf_level_tabs-"..HTML._nb_tabs_table.."-"..i.."\" style=\"padding: 0.1em 0.5em;font-size: 11pt;\">"..HTML:escape_str (header).."</a></li>")
   end
   file:write("</ul>")

   return true
end





-- ----------------------------------------------------------------------------
---
-- @param file An opened HTML file
--
-- @return true if the tab has been created, else false
function HTML:create_tab_in_tabs_table (file, i)
   if file == nil then
      -- TODO error invalid parameter
      return false
   end
   if type (i) ~= "number" 
   or i < 1 then
      -- TODO error invalid parameter
      return false
   end
   
   if HTML._debug then
      print ("create_tab_in_tabs_table: "..i)
   end
   
   file:write("<div id=\"cf_level_tabs-"..HTML._nb_tabs_table.."-"..i.."\">")

   return true
end





-- ----------------------------------------------------------------------------
---
-- @param file An opened HTML file
-- @return true if the tab has been closed, else false
function HTML:close_tab_in_tabs_table (file)
   if file == nil then
      -- TODO error invalid parameter
      return false
   end
   
   if HTML._debug then
      print ("close_tab_in_tabs_table")
   end
   
   file:write("</div>")

   return true
end





-- ----------------------------------------------------------------------------
---
-- @param file An opened HTML file
-- @return true if the table has been closed, else false
function HTML:close_tabs_table (file)
   if file == nil then
      -- TODO error invalid parameter
      return false
   end
   if HTML._debug then
      print ("close_tabs_table")
   end
   
   file:write("</div>")
   HTML._nb_tabs_table = HTML._nb_tabs_table  + 1

   return true
end





-- ----------------------------------------------------------------------------
--- Insert a tooltip
-- @param file An opened HTML file
-- @param text Text visibible and trigerring the tooltip display
-- @param content Text displayed when the cursor is on the text
-- @return true if the tooltip has been created, else false
function HTML:insert_tooltip (file, text, content)
   if file == nil then
      -- TODO error invalid parameter
      return false
   end
   if type (text) ~= "string" then
      return false
   end
   
   if type (content) ~= "string" then
      return false
   end
   
   file:write("<span class=\"tooltip-maqao\" title=\""..content.."\">"..text.."</span>")
   
   return true
end



