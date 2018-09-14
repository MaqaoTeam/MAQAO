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
XLSX_text = {}

-- Create the class meta
XLSX_textMeta         = {}
XLSX_textMeta._NAME   = "XLSX_text"
XLSX_textMeta.__index = XLSX_text






--- Create a nex text structure
-- @param value A string the copy in as the text value
-- @return An initialized text or nil if there is a problem
function XLSX_text:new (value)
   if  type (value) ~= "string"
   and type (value) ~= "number" then
      -- error message
      return nil
   end
   
   local text = {}
	setmetatable (text, XLSX_textMeta)
   text[XLSX.consts.text.value]      = value
   text[XLSX.consts.text.bold]       = "0"
   text[XLSX.consts.text.underline]  = "none"
   text[XLSX.consts.text.color]      = XLSX.consts.colors.black
   text[XLSX.consts.text.size]       = 12
   text[XLSX.consts.text.background] = ""
   text[XLSX.consts.text.bg_tint]    = 0
   text[XLSX.consts.text.italic]     = "0"
   text[XLSX.consts.text.strike]     = "0"
   text[XLSX.consts.text.font]       = "Arial"
   text[XLSX.consts.text.alignment]  = XLSX.consts.align.default
   text[XLSX.consts.meta.name]       = XLSX.consts.meta.text_type
   

   local borders = {}
   borders [XLSX.consts.borders.top]     = XLSX.consts.borders.none
   borders [XLSX.consts.borders.bottom]  = XLSX.consts.borders.none
   borders [XLSX.consts.borders.right]   = XLSX.consts.borders.none
   borders [XLSX.consts.borders.left]    = XLSX.consts.borders.none
   text[XLSX.consts.text.borders]        = borders

   return text
end





--- Set the text in bold (or not)
-- @param bool A boolean used to define if the text must be in bold (true)
--             or not (false)
function XLSX_text:set_bold (bool)
   if type (bool) == "boolean" then
      if bool == true then
         self[XLSX.consts.text.bold] = "1"
      else
         self[XLSX.consts.text.bold] = "0"
      end
   end
end





--- Set the text in underline (or not)
-- @param bool A boolean used to define if the text must be in underline (true)
--             or not (false)
function XLSX_text:set_underline (bool)
   if type (bool) == "boolean" then
      if bool == true then
         self[XLSX.consts.text.underline] = "thin"
      else
         self[XLSX.consts.text.underline] = "none"
      end
   end
end





--- Set the text stroke (or not)
-- @param bool A boolean used to define if the text must be stroke (true)
--             or not (false)
function XLSX_text:set_strike (bool)
   if type (bool) == "boolean" then
      if bool == true then
         self[XLSX.consts.text.strike] = "1"
      else
         self[XLSX.consts.text.strike] = "0"
      end
   end
end





--- Set the text in italic (or not)
-- @param bool A boolean used to define if the text must be in italic (true)
--             or not (false)
function XLSX_text:set_italic (bool)
   if type (bool) == "boolean" then
      if bool == true then
         self[XLSX.consts.text.italic] = "1"
      else
         self[XLSX.consts.text.italic] = "0"
      end
   end
end






--- Set the text size
-- @param size The text size
function XLSX_text:set_size (size)
   if type (size) == "number" then
      self[XLSX.consts.text.size] = size..""
   end
end





--- Set the font color
-- @param color The font color
function XLSX_text:set_color (color)
   self[XLSX.consts.text.color] = color
end





--- Set the background color
-- @param color The background color
-- @param tint An optionnal number between -1 and 1 applied as shade on color
function XLSX_text:set_background (color, tint)
   self[XLSX.consts.text.background] = color
   
   if  type (tint) == "number"
   and tint >= -1
   and tint <= 1 then
      self[XLSX.consts.text.bg_tint] = tint
   else
      self[XLSX.consts.text.bg_tint] = 0
   end
end





--- Set the font
-- @param font_name The font to use
function XLSX_text:set_font (font_name)
   if  font_name ~= nil
   and font_name ~= "" then
      self[XLSX.consts.text.font] = font_name
   end
end





--- Set the text alignment in the cell
-- @param align A variable in {XLSX.consts.align.default, XLSX.consts.align.center
--              XLSX.consts.align.left, XLSX.consts.align.right} used to define
--              the alignment of the text in the cell
function XLSX_text:set_alignment (align)
   if align == XLSX.consts.align.default
   or align == XLSX.consts.align.center
   or align == XLSX.consts.align.left
   or align == XLSX.consts.align.right then
      self[XLSX.consts.text.alignment] = align
   end
end





--- Define a style for the border arround the text
-- @param location A constant in {XLSX.consts.borders.top
--                 XLSX.consts.borders.bottom, XLSX.consts.borders.right, 
--                 XLSX.consts.borders.left, XLSX.consts.borders.all}
-- @param size A constant in {XLSX.consts.borders.none,
--             XLSX.consts.borders.thin, XLSX.consts.borders.thick, XLSX.consts.borders.hair}
-- @return 0 if no problem, else -1
function XLSX_text:set_border (location, size)
   -- Check location value is valid
   if  location ~= XLSX.consts.borders.top
   and location ~= XLSX.consts.borders.bottom
   and location ~= XLSX.consts.borders.right
   and location ~= XLSX.consts.borders.left 
   and location ~= XLSX.consts.borders.all then
      return -1
   end
   
   -- Check size value is valid
   if  size ~= XLSX.consts.borders.none
   and size ~= XLSX.consts.borders.thin
   and size ~= XLSX.consts.borders.thick 
   and size ~= XLSX.consts.borders.hair then
      return -1
   end
   
   -- If location equals XLSX.consts.borders.all, then set the size
   -- to all locations
   if location == XLSX.consts.borders.all then
      self[XLSX.consts.text.borders][XLSX.consts.borders.top] = size
      self[XLSX.consts.text.borders][XLSX.consts.borders.bottom] = size
      self[XLSX.consts.text.borders][XLSX.consts.borders.right] = size
      self[XLSX.consts.text.borders][XLSX.consts.borders.left] = size
   -- Else set only the asked location
   else
      self[XLSX.consts.text.borders][location] = size
   end
   return 0
end
