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




-- Create the class
HTML_action = {}

-- Create the class meta
HTML_actionMeta         = {}
HTML_actionMeta._NAME   = "HTML_action"
HTML_actionMeta.__index = HTML_action



--- Create an action. It is not link to any object
-- @param parameters A table describing the action
--                   Supported fields are:
--                     type = <string>              Type of the action. 
--                                - "link":     The action open a new page
--                                - "popup":    The action open a popup window
--                                - "overlay":  The action open a page on an overlay over the current page
--                     trigger = <string>           How the action is triggered.
--                                - "lclick":   The action is triggered by a single left click
--                                - "dblclick": The action is triggered by a double left click
--                                - "rclick":   The action is triggered by a single right click
--                     url = <string>               URL of the page to load when the action is triggered
--                     new_tab = <boolean>          If type is "link", open the page in a new tab
-- @return An initialized HTML_action or nil if there is an issue
function HTML:create_action (parameters)
   if type (parameters) ~= "table" then
      return nil
   end
   
   if  parameters.type ~= "link"
   and parameters.type ~= "popup" 
   and parameters.type ~= "overlay" then
      return nil
   end

   local haction = {}
   setmetatable (haction, HTML_actionMeta)


   if parameters.trigger == "lclick" then
     haction.trigger = "lclick"
   elseif  parameters.trigger == "dblclick" then
     haction.trigger = "dblclick"
   elseif  parameters.trigger == "rclick" then
     haction.trigger = "rclick"
   else --dblclick
     haction.trigger = "dblclick"
   end
   
   
   if  parameters.type == "link" then
      haction.type    = parameters.type
      if type (parameters.url) ~= "string" then
         return nil
      else
         haction.url     = parameters.url
      end
      
      if parameters.new_tab == false then
         haction.new_tab = parameters.new_tab
      elseif parameters.new_tab == true then
         haction.new_tab = parameters.new_tab
      else
         haction.new_tab = false
      end
      
   elseif parameters.type == "popup" then
      haction.type    = parameters.type
      if type (parameters.url) ~= "string" then
         return nil
      else
         haction.url     = parameters.url
      end
      
      if type (parameters.name) == "string" then
         haction.name = parameters.name
      else
         haction.name = ""
      end

   elseif parameters.type == "overlay" then
      haction.type    = parameters.type
      if type (parameters.url) ~= "string" then
         return nil
      else
         haction.url     = parameters.url
      end
   end   
   
   haction.id = HTML._nb_actions
   HTML._nb_actions = HTML._nb_actions + 1
   
   return haction
end




-- ----------------------------------------------------------------------------
-- Action Menu

-- Create the class
HTML_action_menu = {}

-- Create the class meta
HTML_action_menuMeta         = {}
HTML_action_menuMeta._NAME   = "HTML_action_menu"
HTML_action_menuMeta.__index = HTML_action_menu


--- Create a menu of actions
--@return an initialized empty action_menu object
function HTML:create_action_menu ()
   local hmenu    = {}
   setmetatable (hmenu, HTML_action_menuMeta)
   hmenu.actions  = {}
   hmenu.id       = HTML._nb_action_menus
   HTML._nb_action_menus = HTML._nb_action_menus + 1

   return hmenu
end


--- Add an action to an action_menu
-- @param action An HTML_action to add
-- @param param A string parameter associated to the action
function HTML_action_menu:add_action (text, action, param)
   if self == nil
   or action == nil
   or text == nil then
      return
   end

   table.insert (self.actions, {text = text, action = action, param = param})
end