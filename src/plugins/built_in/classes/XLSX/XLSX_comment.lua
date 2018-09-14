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
XLSX_comment = {}

-- Create the class meta
XLSX_commentMeta         = {}
XLSX_commentMeta._NAME   = "XLSX_comment"
XLSX_commentMeta.__index = XLSX_comment


function XLSX_comment:new (text, loc_x, loc_y, id)
   comment = {}
   setmetatable (comment, XLSX_commentMeta)
   comment[XLSX.consts.comment.text]        = text
   comment[XLSX.consts.comment.location]    = XLSX:get_col_from_int (loc_x)..loc_y
   comment[XLSX.consts.comment.x]           = loc_x
   comment[XLSX.consts.comment.y]           = loc_y
   comment[XLSX.consts.comment.id]          = id
   comment[XLSX.consts.comment.color]       = "#ffffe1"
   comment[XLSX.consts.comment.font_color]  = "00000000"
   comment[XLSX.consts.comment.width]       = 400
      
   -- Return the created object
   return comment
end


---
-- @param color a string representing the color to use. Format: #rrggbb
function XLSX_comment:set_color (color)
   if type (color) ~= "string" then
      return
   end
   if  string.match (color, "^#?[0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f]$") == nil then
      return
   end
   
   self[XLSX.consts.comment.color] = color
end


---
-- @param color a string representing the color to use. Format: 00rrggbb
function XLSX_comment:set_font_color (color)
   if type (color) ~= "string" then
      return
   end
   if  string.match (color, "^00[0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f]$") == nil then
      return
   end
   
   self[XLSX.consts.comment.font_color] = color
end


--- 
-- @param width The comment width (integer), in px (default is 400)
function XLSX_comment:set_width (width)
   if  type (width) ~= "number"
   or width <= 0 then
      return
   end
   
   self[XLSX.consts.comment.width] = width
end
