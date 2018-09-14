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
XLSX_file = {}

-- Create the class meta
XLSX_fileMeta         = {}
XLSX_fileMeta._NAME   = "XLSX_file"
XLSX_fileMeta.__index = XLSX_file





function XLSX:escape_str (str)
   if type (str) == "string" then
      local res = ""  
      res = string.gsub (str, "&", "&amp;")
      res = string.gsub (res, "\"", "&quot;")
      res = string.gsub (res, "'", "&apos;")
      res = string.gsub (res, "<", "&lt;")
      res = string.gsub (res, ">", "&gt;")
      
      return res
   else
      return str
   end
end





---
--
--
local function _hash_font (XLSXtext)
   local hash =   "["..XLSXtext[XLSX.consts.text.bold].."]"..
                  "["..XLSXtext[XLSX.consts.text.italic].."]"..
                  "["..XLSXtext[XLSX.consts.text.strike].."]"..
                  "["..XLSXtext[XLSX.consts.text.underline].."]"..
                  "["..XLSXtext[XLSX.consts.text.color].."]"..
                  "["..XLSXtext[XLSX.consts.text.size].."]"..
                  "["..XLSXtext[XLSX.consts.text.font].."]"
   return hash
end





---
--
--
local function _extract_font (XLSXtext)
   local font = {}
   font[XLSX.consts.font.bold]        = XLSXtext[XLSX.consts.text.bold]
   font[XLSX.consts.font.italic]      = XLSXtext[XLSX.consts.text.italic]
   font[XLSX.consts.font.strike]      = XLSXtext[XLSX.consts.text.strike]
   font[XLSX.consts.font.underline]   = XLSXtext[XLSX.consts.text.underline]
   font[XLSX.consts.font.color]       = XLSXtext[XLSX.consts.text.color]
   font[XLSX.consts.font.size]        = XLSXtext[XLSX.consts.text.size]
   font[XLSX.consts.font.name]        = XLSXtext[XLSX.consts.text.font]
   return font
end





---
--
--
local function _hash_border (XLSXtext)
   if XLSXtext == nil
   or XLSXtext[XLSX.consts.text.borders] == nil then
      local hash =   "["..XLSX.consts.borders.none.."]"..
                     "["..XLSX.consts.borders.none.."]"..
                     "["..XLSX.consts.borders.none.."]"..
                     "["..XLSX.consts.borders.none.."]"
      return hash
   else
      local hash =   "["..XLSXtext[XLSX.consts.text.borders][XLSX.consts.borders.top].."]"..
                     "["..XLSXtext[XLSX.consts.text.borders][XLSX.consts.borders.bottom].."]"..
                     "["..XLSXtext[XLSX.consts.text.borders][XLSX.consts.borders.right].."]"..
                     "["..XLSXtext[XLSX.consts.text.borders][XLSX.consts.borders.left].."]"
      return hash
   end
end





---
--
--
local function _extract_border (XLSXtext)
   local border = {}
   if XLSXtext == nil
   or XLSXtext[XLSX.consts.text.borders] == nil then
      border[XLSX.consts.border.top]        = XLSX.consts.borders.none
      border[XLSX.consts.border.bottom]     = XLSX.consts.borders.none
      border[XLSX.consts.border.right]      = XLSX.consts.borders.none
      border[XLSX.consts.border.left]       = XLSX.consts.borders.none
      border[XLSX.consts.border.diagonal]   = XLSX.consts.borders.none
   else
      border[XLSX.consts.border.top]        = XLSXtext[XLSX.consts.text.borders][XLSX.consts.borders.top]
      border[XLSX.consts.border.bottom]     = XLSXtext[XLSX.consts.text.borders][XLSX.consts.borders.bottom]
      border[XLSX.consts.border.right]      = XLSXtext[XLSX.consts.text.borders][XLSX.consts.borders.right]
      border[XLSX.consts.border.left]       = XLSXtext[XLSX.consts.text.borders][XLSX.consts.borders.left]
      border[XLSX.consts.border.diagonal]   = XLSX.consts.borders.none
   end
   return border
end





---
--
--
local function _hash_fill (XLSXtext)
   local hash =   "["..XLSXtext[XLSX.consts.text.background].."]"..
                  "["..XLSXtext[XLSX.consts.text.color].."]"..
                  "["..tostring (XLSXtext[XLSX.consts.text.bg_tint]).."]"
   return hash
end





---
--
--
local function _extract_fill (XLSXtext)
   local fill = {}
   if XLSXtext ~= nil then
      fill[XLSX.consts.fill.background] = "00993300"
      fill[XLSX.consts.fill.foreground] = XLSXtext[XLSX.consts.text.background]
      fill[XLSX.consts.fill.bg_tint]    = XLSXtext[XLSX.consts.text.bg_tint]
   else
      fill[XLSX.consts.fill.background] = ""
      fill[XLSX.consts.fill.foreground] = ""
      fill[XLSX.consts.fill.bg_tint]    = ""
   end
   return fill
end





---
--
--
local function _hash_align (XLSXtext)  
   if XLSXtext == nil
   or XLSXtext[XLSX.consts.text.alignment] == nil then
      hash = "["..XLSX.consts.align.default.."]"
   else
      hash = "["..XLSXtext[XLSX.consts.text.alignment].."]"
   end
   return hash
end





---
--
--
local function _extract_align (XLSXtext)
   if XLSXtext == nil then
      return XLSX.consts.align.default
   else
      return XLSXtext[XLSX.consts.text.alignment]
   end
end





---
--
--
local function _hash_style (XLSXtext)
   local font   = _hash_font (XLSXtext)
   local border = _hash_border (XLSXtext)
   local fill   = _hash_fill (XLSXtext)
   local align  = _hash_align (XLSXtext)
   local hash   = "["..font.."]["..border.."]["..fill.."]["..align.."]"
   
   return hash
end





--- Create an XLSX file
-- @param filename name of the file to create. The extension .xlsx is added if missing
-- @return an initialized XLSX_file structure or nil if the name is not valid
function XLSX_file:create (filename)
   local file = nil
   local name = ""
   local path = ""

   if XLSX.__is_zip ~= true then
      Message:error ("zip is not available, XLSX file can be created")
      return nil
   end

   -- Check if the name is nil or empty
   if filename == nil
   or type (filename) ~= "string"
   or filename == "" then
      -- error message
      return nil
   end

   -- Divide the given name to get the path and the name
   name = string.gsub (filename, "^.*/", "")

   -- Next line is dead code, but the comment is interesting and needs the code to be understandood
   --path = string.gsub (filename, name, "")
   -- This line was used to get the dirname since name is the string basename by removing the basename
   -- from the string.
   -- This code produced wrong results when the basename contains '-' because it is interpreted
   -- by the Lua regex mecanisme as a not standard character. The fix is not windows proofed but
   -- all the code isn't so it will be good enought (just handle both '/' or '\' as separator according
   -- to the current OS => Need to determine it during the compilation process / dynamicaly
   path = string.match(filename, "(.*/)")

   -- If the extension is in the name, remove it
   name = string.gsub (name, "%.xlsx$", "")

   -- If path is empty, set it to local directory
   if path == nil
   or path == "" then
      path = "./"
   end

   -- Initialize the object to return
   file = {}
	setmetatable (file, XLSX_fileMeta)
   file[XLSX.consts.file.name]        = name
   file[XLSX.consts.file.path]        = path
   file[XLSX.consts.file.extension]   = ".xlsx"
   file[XLSX.consts.file.sheets]      = {}
   file[XLSX.consts.file.ustrings]    = {}
   file[XLSX.consts.file.ostrings]    = {}
   file[XLSX.consts.file.nb_ustrings] = 0
   file[XLSX.consts.file.nb_strings]  = 0
   file[XLSX.consts.meta.name]        = XLSX.consts.meta.file_type
   file[XLSX.consts.file.fonts]       = {}
   file[XLSX.consts.file.borders]     = {}
   file[XLSX.consts.file.fills]       = {}
   file[XLSX.consts.file.styles]      = {}
   file[XLSX.consts.file.ofonts]      = {}
   file[XLSX.consts.file.oborders]    = {}
   file[XLSX.consts.file.ofills]      = {}
   file[XLSX.consts.file.ostyles]     = {}
   file[XLSX.consts.file.alignments]  = {}
   file[XLSX.consts.file.is_graph]    = false
   file[XLSX.consts.file.is_comment]  = false
   
   -- Return the created object
   return file
end





--- Add a sheet in the file
-- @param sheet Either the sheet name or an existing sheet
--              If sheet is a string, then a new sheet with this name is created and returned
--              Else if the sheet is an existing sheet, it is added and returned
-- @return The added sheet or nil if the sheet can not be added
function XLSX_file:add_sheet (sheet)
   if sheet == nil then
      -- error message
      return nil
   end

   -- If the parameter is a string, create and add an empty sheet
   if type(sheet) == "string" then
      -- If sheet is an empty string, create a default name
      if sheet == "" then
         sheet = "sheet_"..(table.getn (self[XLSX.consts.file.sheets]) + 1)
      end

      local newsheet = {}
   	setmetatable (newsheet, XLSX_sheetMeta)
      newsheet[XLSX.consts.sheet.name]       = string.sub (sheet, 1, 31)  -- Excel can not handle strings exceding 31 chars
      newsheet[XLSX.consts.sheet.cells]      = {}
      newsheet[XLSX.consts.sheet.position_x] = 1
      newsheet[XLSX.consts.sheet.position_y] = 1
      newsheet[XLSX.consts.meta.name]        = XLSX.consts.meta.sheet_type
      newsheet[XLSX.consts.sheet.ustrings]   = {}
      newsheet[XLSX.consts.sheet.ostrings]   = {}
      newsheet[XLSX.consts.sheet.nb_ustrings]= 0
      newsheet[XLSX.consts.sheet.nb_strings] = 0
      newsheet[XLSX.consts.sheet.column_max] = 1
      newsheet[XLSX.consts.sheet.line_max]   = 1
      newsheet[XLSX.consts.sheet.mergedCells]= {}
      newsheet[XLSX.consts.sheet.graphs]     = {}
      newsheet[XLSX.consts.sheet.colum_size] = {}
      newsheet[XLSX.consts.sheet.row_size]   = {}
      newsheet[XLSX.consts.sheet.hyperlinks] = {}
      newsheet[XLSX.consts.sheet.comments]   = {}
      
      table.insert (self[XLSX.consts.file.sheets], newsheet)
      return newsheet

   -- else if the parameter is an existing sheet, add it
   elseif type(sheet) == "table" 
   and    sheet[XLSX.consts.meta.name] == XLSX.consts.meta.sheet_type then
         table.insert (self[XLSX.consts.file.sheets], sheet)

   -- else error
   else
      -- error message
      return nil
   end
   return nil
end





--- Remove a sheet from the file
-- @param sheet Either the sheet name or an existing sheet
--              If sheet is a string, look for a sheet with this name and remove it
--              Else if the sheet is an existing sheet, look for it and remove it
function XLSX_file:remove_sheet (sheet)
   if sheet == nil then
      -- error message
      return
   end

      -- If the parameter is a string, create and add an empty sheet
   if type(sheet) == "string" then
      -- If sheet is an empty string, create a default name
      if sheet == "" then
         return
      end

      for i, s in pairs (self[XLSX.consts.file.sheets]) do
         if s[XLSX.consts.sheet.name] == sheet then
            table.remove (self[XLSX.consts.file.sheets], i)
            return
         end
      end

   -- else if the parameter is an existing sheet, add it
   elseif type(sheet) == "table" 
   and    sheet[XLSX.consts.meta.name] == XLSX.consts.meta.sheet_type then
      for i, s in pairs (self[XLSX.consts.file.sheets]) do
         if s[XLSX.consts.sheet.name] == sheet[XLSX.consts.sheet.name] then
            table.remove (self[XLSX.consts.file.sheets], i)
            return
         end
      end

   -- else error
   else
      -- error message
      return
   end
end





--- Return an asked sheet
-- @param sheet Either an integer or a string
--              If sheet is an integer, the sheet at position <sheet> is returned
--              If sheet is a string, the sheet with the name <sheet> is returned
--              If no sheet correspond to <sheet>, nil is returned
function XLSX_file:get_sheet (sheet)
   -- if sheet is a number, return the sheet at the corresponding index
   if type(sheet) == "number" then
      if sheet <= 0 then
         return nil
      end
      return self[XLSX.consts.file.sheets][sheet]

   -- else if sheet is a string, return the string with the corresponding name
   elseif type (sheet) == "string" then
      if sheet == "" then
         return nil
      end

      -- iterate over sheets to get the one with the good name
      for i, s in pairs (self[XLSX.consts.file.sheets]) do
         if s[XLSX.consts.sheet.name] == sheet then
            return s[XLSX.consts.sheet.name]
         end
      end

   -- else it is an error
   else
      -- error message
      return nil
   end
   return nil
end





--- Swap the position of two sheets
-- @param sheet1 a sheet to swap
-- @param sheet2 a sheet to swap
-- Both sheet1 and sheet2 can be either an integer or an existing sheet
-- but they have to have the same type
function XLSX_file:swap_sheets (sheet1, sheet2)
   if type(sheet1) ~= type(sheet2) then
      -- error message
      return 
   end
   
   -- if sheet1 and sheet2 are integer
   if type(sheet1) == number then
      if  self[XLSX.consts.file.sheets][sheet1] ~= nil
      and self[XLSX.consts.file.sheets][sheet2] ~= nil then
         local tmp = self[XLSX.consts.file.sheets][sheet1]
         self[XLSX.consts.file.sheets][sheet1] = self[XLSX.consts.file.sheets][sheet2]
         self[XLSX.consts.file.sheets][sheet2] = tmp
      else
         return 
      end
   
   -- else if sheet1 and sheet2 are XLSX_sheets
   elseif type(sheet) == "table" 
   and    sheet[XLSX.consts.meta.name] == XLSX.consts.meta.sheet_type then
      local i1 = 0
      local i2 = 0
      
      -- Get the position of first sheet
      for i, s in pairs (self[XLSX.consts.file.sheets]) do
         if s[XLSX.consts.sheet.name] == sheet1[XLSX.consts.sheet.name] then
            i1 = i
         end
      end
      
      -- Get the position of second sheet
      for i, s in pairs (self[XLSX.consts.file.sheets]) do
         if s[XLSX.consts.sheet.name] == sheet2[XLSX.consts.sheet.name] then
            i2 = i
         end
      end

      -- If both positions have been found, swap sheets
      if  i1 > 0
      and i2 > 0 then
         local tmp = self[XLSX.consts.file.sheets][i1]
         self[XLSX.consts.file.sheets][i1] = self[XLSX.consts.file.sheets][i2]
         self[XLSX.consts.file.sheets][i2] = tmp
      
      -- else, it is an error
      else
         -- error message
         return
      end
   
   -- else this is an error
   else
      -- error message
      return
   end
end





---
-- @param XLSXfile a XLSX_file object
-- @param tmp_dir_name path to a directory where to create the file
local function _create__rels (XLSXfile, tmp_dir_name)
   lfs.mkdir (tmp_dir_name.."/_rels")

   local file = io.open (tmp_dir_name.."/_rels/.rels", "w")
   if file == nil then
      -- error message
      return -1
   end
   file:write ("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n")
   file:write ("<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n")
   file:write ("  <Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/>\n")
   file:write ("  <Relationship Id=\"rId2\" Type=\"http://schemas.openxmlformats.org/package/2006/relationships/metadata/core-properties\" Target=\"docProps/core.xml\"/>\n")
   file:write ("  <Relationship Id=\"rId3\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/extended-properties\" Target=\"docProps/app.xml\"/>\n")
   file:write ("</Relationships>\n")
   file:close ()

   return 0
end





--- Create [Content_Types].xml file
-- @param XLSXfile a XLSX_file object
-- @param tmp_dir_name path to a directory where to create the file
local function _create_ContentTypes_XML (XLSXfile, tmp_dir_name)
   -- This file contains all files in the XLSX document.
   -- It should probably be update with new files when I will knwo better the XLSX format
   
   local file = io.open (tmp_dir_name.."/[Content_Types].xml", "w")   
   if file == nil then
      -- error message
      return -1
   end
   file:write ("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n")
   file:write ("<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">\n")
   if XLSXfile[XLSX.consts.file.is_comment] == true then
      file:write ("<Default Extension=\"vml\" ContentType=\"application/vnd.openxmlformats-officedocument.vmlDrawing\"/>\n")
   end
   file:write ("  <Override PartName=\"/_rels/.rels\" "..
               "ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>\n")
   file:write ("  <Override PartName=\"/xl/_rels/workbook.xml.rels\" "..
               "ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>\n")
   -- Declare one file per sheet
   -- PartName is the name of the file, not the name of the sheet
   for i, sheet in pairs (XLSXfile[XLSX.consts.file.sheets]) do
      file:write ("  <Override PartName=\"/xl/worksheets/sheet"..i..".xml\" "..
                  "ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>\n")   
   end
   -- sharedStrings.xml is present only if there is at least one string
   if XLSXfile[XLSX.consts.file.nb_ustrings] > 0 then
      file:write ("  <Override PartName=\"/xl/sharedStrings.xml\" "..
                  "ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sharedStrings+xml\"/>\n")
   end

   local graph_id = 1
   for i, sheet in pairs (XLSXfile[XLSX.consts.file.sheets]) do
      if table.getn (sheet[XLSX.consts.sheet.graphs]) > 0 
      or sheet[XLSX.consts.sheet.is_remote_hyperlinks] == true 
      or table.getn (sheet[XLSX.consts.sheet.comments]) > 0 then
         file:write ("  <Override PartName=\"/xl/worksheets/_rels/sheet"..i..".xml.rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>\n")
      end
   
      -- Files used for graphs
      if table.getn (sheet[XLSX.consts.sheet.graphs]) > 0 then
         file:write ("  <Override PartName=\"/xl/drawings/drawing"..i..".xml\" ContentType=\"application/vnd.openxmlformats-officedocument.drawing+xml\"/>\n")
         file:write ("  <Override PartName=\"/xl/drawings/_rels/drawing"..i..".xml.rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>\n")
         for j, graph in pairs (sheet[XLSX.consts.sheet.graphs]) do
            file:write ("  <Override PartName=\"/xl/charts/chart"..graph_id..".xml\" ContentType=\"application/vnd.openxmlformats-officedocument.drawingml.chart+xml\"/>\n")
            graph_id = graph_id + 1
         end
      end
      
      -- Files used for graphs
      if table.getn (sheet[XLSX.consts.sheet.comments]) > 0 then
         file:write ("  <Override PartName=\"/xl/comments"..i..".xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.comments+xml\"/>\n")
      end
   end
   
   file:write ("  <Override PartName=\"/xl/workbook.xml\" "..
               "ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>\n")
   file:write ("  <Override PartName=\"/xl/styles.xml\" "..
               "ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml\"/>\n")
   file:write ("  <Override PartName=\"/docProps/app.xml\" "..
               "ContentType=\"application/vnd.openxmlformats-officedocument.extended-properties+xml\"/>\n")
   file:write ("  <Override PartName=\"/docProps/core.xml\" "..
               "ContentType=\"application/vnd.openxmlformats-package.core-properties+xml\"/>\n")
   
   file:write ("</Types>\n")
   file:close ()
   return 0
end





--- Create docProps directory and its content
-- @param XLSXfile a XLSX_file object
-- @param tmp_dir_name path to a directory where to create the file
local function _create_DocProps (XLSXfile, tmp_dir_name)
   -- -------------------------------------------------------------------------
   -- docProps/
   lfs.mkdir (tmp_dir_name.."/docProps")

   -- -------------------------------------------------------------------------
   -- docProps/app.xml
   local file = io.open (tmp_dir_name.."/docProps/app.xml", "w")   
   if file == nil then
      -- error message
      return -1
   end
   file:write ("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n")
   file:write ("<Properties xmlns=\"http://schemas.openxmlformats.org/officeDocument/2006/extended-properties\" "..
               "xmlns:vt=\"http://schemas.openxmlformats.org/officeDocument/2006/docPropsVTypes\">\n")
   file:write ("  <TotalTime>0</TotalTime>\n")
   file:write ("</Properties>\n")
   file:close ()

   -- -------------------------------------------------------------------------
   -- docProps/core.xml
   file = io.open (tmp_dir_name.."/docProps/core.xml", "w")   
   if file == nil then
      -- error message
      return -1
   end
   file:write ("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n")
   file:write ("<cp:coreProperties xmlns:cp=\"http://schemas.openxmlformats.org/package/2006/metadata/core-properties\" "..
               "xmlns:dc=\"http://purl.org/dc/elements/1.1/\" xmlns:dcmitype=\"http://purl.org/dc/dcmitype/\" "..
               "xmlns:dcterms=\"http://purl.org/dc/terms/\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">\n")
   file:write ("  <cp:revision>0</cp:revision>\n")
   file:write ("</cp:coreProperties>\n")
   file:close ()
   return 0
end





local function _create_styles_xml (XLSXfile, filename)
   local file = io.open (filename, "w")   
   if file == nil then
      -- error message
      return -1
   end
   
   file:write ("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n")
   file:write ("<styleSheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">\n")
   
   -- List of fonts 
   file:write ("  <fonts count=\""..table.getn(XLSXfile[XLSX.consts.file.ofonts]).."\">\n")
   for i, font in ipairs (XLSXfile[XLSX.consts.file.ofonts]) do
      file:write ("    <font>\n")
      for attribute, value in pairs (XLSXfile[XLSX.consts.file.fonts][font]) do
         if attribute == XLSX.consts.font.color then
            file:write ("      <"..attribute.." rgb=\""..value.."\"/>\n")
         else
            file:write ("      <"..attribute.." val=\""..value.."\"/>\n")
         end
      end
      file:write ("    </font>\n")
   end
   file:write ("  </fonts>\n")      
   
   -- Fill
   file:write ("  <fills count=\""..(table.getn(XLSXfile[XLSX.consts.file.ofills]) + 2).."\">\n")
   file:write ("    <fill>\n")
   file:write ("      <patternFill patternType=\"none\"/>\n")
   file:write ("    </fill>\n")
   file:write ("    <fill>\n")
   file:write ("      <patternFill patternType=\"gray125\"/>\n")
   file:write ("    </fill>\n")
   for i, fill_hash in ipairs (XLSXfile[XLSX.consts.file.ofills]) do
      local fill = XLSXfile[XLSX.consts.file.fills][fill_hash]
      file:write ("    <fill>\n")
      
      if fill[XLSX.consts.fill.foreground] == "" 
      or fill[XLSX.consts.fill.foreground] == nil then
         file:write ("      <patternFill patternType=\"none\"/>\n")   
      else
         file:write ("      <patternFill patternType=\"solid\">\n")
         if  fill[XLSX.consts.fill.foreground] ~= ""
         and fill[XLSX.consts.fill.foreground] ~= nil then
            file:write ("        <"..XLSX.consts.fill.foreground.." rgb=\""..fill[XLSX.consts.fill.foreground].."\" ")
            if fill[XLSX.consts.fill.bg_tint] ~= "" then
               file:write ("tint=\""..fill[XLSX.consts.fill.bg_tint].."\" ")
            end
            file:write ("/>\n")
         end
         file:write ("      </patternFill>\n")
      end
      file:write ("    </fill>\n")
   end
   file:write ("  </fills>\n")
   
   -- List of borders
   file:write ("  <borders count=\""..(table.getn(XLSXfile[XLSX.consts.file.oborders]) + 1).."\">\n")
   file:write ("    <border>\n")
   file:write ("      <left/>\n")
   file:write ("      <right/>\n")
   file:write ("      <top/>\n")
   file:write ("      <bottom/>\n")
   file:write ("      <diagonal/>\n")   
   file:write ("    </border>\n")
   for i, border in ipairs (XLSXfile[XLSX.consts.file.oborders]) do
      file:write ("    <border>\n")
      for i, attribute in pairs ({"left", "right", "top", "bottom", "diagonal"}) do
         value = XLSXfile[XLSX.consts.file.borders][border][attribute]
         if value == XLSX.consts.borders.none then
            file:write ("      <"..attribute.."/>\n")
         else
            file:write ("      <"..attribute.." style=\""..value.."\"/>\n")
         end
      end
      file:write ("    </border>\n")
   end
   file:write ("  </borders>\n")
   
   -- ??
   file:write ("  <cellStyleXfs count=\"1\">\n")
   file:write ("    <xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\"/>\n")
   file:write ("  </cellStyleXfs>\n")
   
   -- Default style use in all sheet cells
   file:write ("  <cellXfs count=\""..(table.getn(XLSXfile[XLSX.consts.file.ostyles]) + 1).."\">\n")
   file:write ("    <xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\" xfId=\"0\" "..
                       "applyFont=\"0\" applyFill=\"1\" applyBorder=\"0\" applyAlignment=\"0\" />\n")

   -- Styles defined by the user
   for i, hash_style in ipairs (XLSXfile[XLSX.consts.file.ostyles]) do
      local style = XLSXfile[XLSX.consts.file.styles][hash_style]
      file:write ("    <xf numFmtId=\"0\" fontId=\""..style[XLSX.consts.style.font_id].."\" "..
                     "fillId=\""..(style[XLSX.consts.style.fill_id] + 2).."\" "..
                     "borderId=\""..(style[XLSX.consts.style.border_id] + 1).."\" "..
                     "xfId=\"0\" applyFont=\"1\" applyFill=\"1\" applyBorder=\"1\" "..
                     "applyAlignment=\"1\">\n")
      file:write ("      <alignment shrinkToFit=\"false\" vertical=\"center\" wrapText=\"true\" ")
      
      if  style[XLSX.consts.style.align] ~= XLSX.consts.align.default
      and style[XLSX.consts.style.align] ~= nil then
         file:write (" horizontal=\""..style[XLSX.consts.style.align].."\"")
      end
      file:write ("/>\n")
      file:write ("    </xf>\n")
      
   end
   file:write ("  </cellXfs>\n")
   
   -- List of native formats
   file:write ("  <cellStyles count=\"1\">\n")
   file:write ("    <cellStyle name=\"Normal\" xfId=\"0\" builtinId=\"0\"/>\n")
   file:write ("  </cellStyles>\n")
   file:write ("  <dxfs count=\"0\"/>\n")
   file:write ("</styleSheet>\n")
   file:close ()
end





local function _create_worksheet_rels (sheet, filename, id)
   local lid = id

   file = io.open (filename, "w")
   if file == nil then
      -- error message
      return -1
   end
   file:write ("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n")
   file:write ("<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n")
   -- Rels for graphs
   if table.getn (sheet[XLSX.consts.sheet.graphs]) > 0 then
      file:write ("  <Relationship Id=\"rId"..id.."\" "..
                  "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/drawing\" "..
                  "Target=\"../drawings/drawing"..id..".xml\"/>\n")
   end
   
   -- Rels for remote hyperlinks
   if table.getn (sheet[XLSX.consts.sheet.hyperlinks]) > 0 then
      for i, link in ipairs (sheet[XLSX.consts.sheet.hyperlinks]) do
         if link[XLSX.consts.hyperlink.r_file] ~= nil then
            link[XLSX.consts.hyperlink.rel_id] = "rId"..lid
            file:write ("  <Relationship Id=\""..link[XLSX.consts.hyperlink.rel_id].."\" "..
                        "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/hyperlink\" "..
                        "Target=\""..link[XLSX.consts.hyperlink.r_file].."\" TargetMode=\"External\" />\n")
            lid = lid + 1
         end
      end
   end
   
   -- Rels for comments
   if table.getn (sheet[XLSX.consts.sheet.comments]) > 0 then
      file:write ("  <Relationship Id=\"rId"..lid.."\" "..
                  "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/comments\" "..
                  "Target=\"../comments"..id..".xml\" />\n")
      lid = lid + 1
      sheet[XLSX.consts.sheet.comment_id] = "rId"..lid
      file:write ("  <Relationship Id=\"rId"..lid.."\" "..
                  "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/vmlDrawing\" "..
                  "Target=\"../drawings/vmlDrawing"..id..".vml\"/>\n")
   end
   
   file:write ("</Relationships>\n")
   file:close ()
end





local function _create_sheetXml (XLSXfile, sheet, filename, id)
   local file = io.open (filename, "w")   
   if file == nil then
      -- error message
      return -1
   end
   file:write ("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n")
   file:write ("<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">\n")
   file:write ("  <sheetPr filterMode=\"false\">\n")
   file:write ("    <pageSetUpPr fitToPage=\"false\"/>\n")
   file:write ("  </sheetPr>\n")
   -- Get the data dimensions using sheet[XLSX.consts.sheet.column_max] 
   -- and sheet[XLSX.consts.sheet.position_y]
   if sheet[XLSX.consts.sheet.column_max] == 1
   and sheet[XLSX.consts.sheet.position_y] == 1 then
      file:write ("  <dimension ref=\"A1\"/>\n")
   else
      local max = sheet[XLSX.consts.sheet.position_y]      
      file:write ("  <dimension ref=\"A1:"..XLSX:get_col_from_int (max)..table.getn(sheet[XLSX.consts.sheet.cells]).."\"/>\n")
   end
   file:write ("  <sheetViews>\n")
   file:write ("    <sheetView colorId=\"64\" defaultGridColor=\"true\" rightToLeft=\"false\" showFormulas=\"false\" showGridLines=\"true\" showOutlineSymbols=\"true\" showRowColHeaders=\"true\" showZeros=\"true\" tabSelected=\"true\" topLeftCell=\"A1\" view=\"normal\" windowProtection=\"false\" workbookViewId=\"0\" zoomScale=\"100\" zoomScaleNormal=\"100\" zoomScalePageLayoutView=\"100\">\n")
   if sheet[XLSX.consts.sheet.is_raw_fixed] ~= nil then
      file:write ("      <pane ySplit=\""..sheet[XLSX.consts.sheet.is_raw_fixed].."\" topLeftCell=\"A"..(sheet[XLSX.consts.sheet.is_raw_fixed] + 1).."\" activePane=\"bottomLeft\" state=\"frozen\"/>\n")
   end
   file:write ("      <selection activeCell=\"A1\" activeCellId=\"0\" pane=\"topLeft\" sqref=\"A1\"/>\n")
   file:write ("    </sheetView>\n")
   file:write ("  </sheetViews>\n")
   file:write ("  <cols>\n")
   
   local col_idx = 1
   for i = 1, table.getn (sheet[XLSX.consts.sheet.colum_size]) do
      file:write ("    <col collapsed=\"false\" hidden=\"false\" max=\""..i.."\" min=\""..i.."\" style=\"0\" "..
                           "width=\""..(sheet[XLSX.consts.sheet.colum_size][i] * XLSX.consts.size.col_width_rate).."\"/>\n")
      col_idx = col_idx + 1
   end
   file:write ("    <col collapsed=\"false\" hidden=\"false\" max=\"1025\" min=\""..col_idx.."\" style=\"0\" "..
                        "width=\""..(XLSX.consts.size.col_width * XLSX.consts.size.col_width_rate).."\"/>\n")
   file:write ("  </cols>\n")
   file:write ("  <sheetData>\n")
   -- Iterate over data to print them
   local cell = nil
   for i = 1, table.getn(sheet[XLSX.consts.sheet.cells]) do
      if sheet[XLSX.consts.sheet.row_size][i] == nil then
         file:write ("    <row customHeight=\"false\" ht=\""..(XLSX.consts.size.row_height * XLSX.consts.size.cm_to_pt).."\" outlineLevel=\"0\" r=\""..i.."\">\n")   
      else
         file:write ("    <row customHeight=\"true\" ht=\""..(sheet[XLSX.consts.sheet.row_size][i] * XLSX.consts.size.cm_to_pt).."\" outlineLevel=\"0\" r=\""..i.."\">\n")
      end
      for j = 1, sheet[XLSX.consts.sheet.column_max] do
         -- <c> defines a cell
         --    - r=<position> 
         --    - s=<style>
         --    - t=<type> (n=number, s=string)
         --    - value depends on type (n: number value, s: index in sharedStrings.xml)
         cell = sheet[XLSX.consts.sheet.cells][i][j]
         
         -- Get the hash corresponding to cell style
         if cell == nil
         or cell[XLSX.consts.text.value] == nil then
            file:write ("      <c r=\""..XLSX:get_col_from_int (j)..i.."\" s=\"0\" />\n")
         elseif cell[XLSX.consts.text.value] == "" then
            file:write ("      <c r=\""..XLSX:get_col_from_int (j)..i.."\" "..
                        "s=\""..XLSXfile[XLSX.consts.file.styles][_hash_style (cell)][XLSX.consts.style.id].."\" />\n")
         else
            if type (cell[XLSX.consts.text.value]) == "string" then
               file:write ("      <c r=\""..XLSX:get_col_from_int (j)..i.."\" "..
                           "s=\""..XLSXfile[XLSX.consts.file.styles][_hash_style (cell)][XLSX.consts.style.id].."\" "..
                           "t=\"s\">\n")
               file:write ("        <v>"..(XLSXfile[XLSX.consts.file.ustrings][cell[XLSX.consts.text.value]] - 1).."</v>\n")
               file:write ("      </c>\n")
            elseif type (cell[XLSX.consts.text.value]) == "number" then
               file:write ("      <c r=\""..XLSX:get_col_from_int (j)..i.."\" "..
                           "s=\""..XLSXfile[XLSX.consts.file.styles][_hash_style (cell)][XLSX.consts.style.id].."\" "..
                           "t=\"n\">\n")
               file:write ("        <v>"..cell[XLSX.consts.text.value].."</v>\n")
               file:write ("      </c>\n")
            end
         end     
      end
      file:write ("    </row>\n")
   end

   file:write ("  </sheetData>\n")
   
   -- Add merged cells if needed
   if table.getn (sheet[XLSX.consts.sheet.mergedCells]) > 0 then
      file:write ("  <mergeCells count=\""..table.getn (sheet[XLSX.consts.sheet.mergedCells]).."\">\n")
      for i, ref in pairs (sheet[XLSX.consts.sheet.mergedCells]) do
         file:write ("    <mergeCell ref=\""..ref.."\" />\n")
      end
      file:write ("  </mergeCells>\n")
   end
   
   -- Add hyperlinks if needed
   if table.getn (sheet[XLSX.consts.sheet.hyperlinks]) > 0 then
      file:write ("  <hyperlinks>\n")
      for _, link in ipairs (sheet[XLSX.consts.sheet.hyperlinks]) do
         if link[XLSX.consts.hyperlink.dst] ~= nil then
            file:write ("    <hyperlink ref=\""..link[XLSX.consts.hyperlink.ref].."\" "..
                        "location=\""..link[XLSX.consts.hyperlink.dst][XLSX.consts.sheet.name].."!"..link[XLSX.consts.hyperlink.dst_cell].."\" "..
                        "display=\""..link[XLSX.consts.hyperlink.display].."\"/>\n")
         else
            file:write ("    <hyperlink ref=\""..link[XLSX.consts.hyperlink.ref].."\" "..
                        "r:id=\""..link[XLSX.consts.hyperlink.rel_id].."\" "..
                        "location=\""..link[XLSX.consts.hyperlink.r_sheet].."!"..link[XLSX.consts.hyperlink.dst_cell].."\"/>\n")
         end
      end
      file:write ("  </hyperlinks>\n")
   end

   -- Here add graphs
   if table.getn (sheet[XLSX.consts.sheet.graphs]) > 0 then
      file:write ("  <drawing r:id=\"rId"..id.."\" />\n")
   end

   -- Here add comments
   if table.getn (sheet[XLSX.consts.sheet.comments]) > 0 then
      file:write ("  <legacyDrawing r:id=\""..sheet[XLSX.consts.sheet.comment_id].."\" />\n")
   end
   file:write ("</worksheet>\n")
   file:close ()
end





local function _create_drawing (filename, sheet)
   local file = io.open (filename, "w")
   if file == nil then
      -- error message
      return -1
   end

   file:write ("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n")
   file:write ("<xdr:wsDr xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\" "..
               "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\" "..
               "xmlns:xdr=\"http://schemas.openxmlformats.org/drawingml/2006/spreadsheetDrawing\">\n")
               
   -- One <xdr:twoCellAnchor> per graph
   for i, graph in pairs(sheet [XLSX.consts.sheet.graphs]) do
      file:write ("  <xdr:twoCellAnchor>\n")
          
       -- Start position
      file:write ("    <xdr:from>\n")
      file:write ("      <xdr:col>"..(graph[XLSX.consts.graph.x_top] - 1).."</xdr:col>\n")
      file:write ("      <xdr:colOff>0</xdr:colOff>\n")
      file:write ("      <xdr:row>"..(graph[XLSX.consts.graph.y_top] - 1).."</xdr:row>\n")
      file:write ("      <xdr:rowOff>0</xdr:rowOff>\n")
      file:write ("    </xdr:from>\n")

       -- End position
      file:write ("    <xdr:to>\n")
      file:write ("      <xdr:col>"..(graph[XLSX.consts.graph.x_top] + graph[XLSX.consts.graph.size_w_c] - 1).."</xdr:col>\n")
      file:write ("      <xdr:colOff>0</xdr:colOff>\n")
      file:write ("      <xdr:row>"..(graph[XLSX.consts.graph.y_top] + graph[XLSX.consts.graph.size_h_c] - 1).."</xdr:row>\n")
      file:write ("      <xdr:rowOff>0</xdr:rowOff>\n")
      file:write ("    </xdr:to>\n")
      
      file:write ("    <xdr:graphicFrame macro=\"\">\n")
      file:write ("      <xdr:nvGraphicFramePr>\n")
      file:write ("        <xdr:cNvPr id=\""..(i + 1).."\" name=\"Graph"..i.."\"/>\n")
      file:write ("        <xdr:cNvGraphicFramePr/>\n")
      file:write ("      </xdr:nvGraphicFramePr>\n")

      file:write ("      <xdr:xfrm>\n")
      file:write ("        <a:off x=\"0\" y=\"0\"/>\n")
      file:write ("        <a:ext cx=\"0\" cy=\"0\"/>\n")
      file:write ("      </xdr:xfrm>\n")
      file:write ("      <a:graphic>\n")
      file:write ("        <a:graphicData uri=\"http://schemas.openxmlformats.org/drawingml/2006/chart\">\n")
      file:write ("          <c:chart r:id=\"rId"..i.."\" xmlns:c=\"http://schemas.openxmlformats.org/drawingml/2006/chart\" "..
                                                   "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\"/>\n")
      file:write ("        </a:graphicData>\n")
      file:write ("      </a:graphic>\n")
      file:write ("    </xdr:graphicFrame>\n")
      file:write ("    <xdr:clientData/>\n")
      file:write ("  </xdr:twoCellAnchor>\n")
   end
   file:write ("</xdr:wsDr>\n")

   file:close ()
   return 0
end





local function _create_comments (filename, sheet)
   local file = io.open (filename, "w")
   if file == nil then
      -- error message
      return -1
   end
   
   file:write ("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n")
   file:write ("  <comments xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">\n")
   file:write ("    <authors>\n")
   file:write ("      <author>MAQAO</author>\n")
   file:write ("    </authors>\n")
   file:write ("  <commentList>\n")
   
   for i, comment in ipairs (sheet[XLSX.consts.sheet.comments]) do
      file:write ("    <comment ref=\""..comment[XLSX.consts.comment.location].."\" authorId=\"0\" shapeId=\"0\" >\n")
      file:write ("      <text>\n")
      file:write ("        <r>\n")
      file:write ("          <rPr>\n")
      file:write ("            <b/>\n")
      file:write ("            <sz val=\"9\"/>\n")
      file:write ("            <color rgb=\""..comment[XLSX.consts.comment.font_color].."\"/>\n")
      file:write ("            <rFont val=\"Tahoma\"/>\n")
      file:write ("            <charset val=\"1\"/>\n")
      file:write ("          </rPr>\n")
      file:write ("          <t>"..XLSX:escape_str(comment[XLSX.consts.comment.text]).."</t>\n")
      file:write ("        </r>\n")
      file:write ("      </text>\n")
      file:write ("    </comment>\n")
   end
   
   file:write ("  </commentList>\n")
   file:write ("</comments>\n")
   
   file:close ()
   return 0
end





local function _create_vmlDrawing (filename, sheet, id)
   local file = io.open (filename, "w")
   if file == nil then
      -- error message
      return -1
   end
   file:write ("<xml xmlns:v=\"urn:schemas-microsoft-com:vml\"\n")
   file:write ("     xmlns:o=\"urn:schemas-microsoft-com:office:office\" \n")
   file:write ("     xmlns:x=\"urn:schemas-microsoft-com:office:excel\">\n")

   file:write ("  <o:shapelayout v:ext=\"edit\">\n")
   file:write ("    <o:idmap v:ext=\"edit\" data=\"1\"/>\n")
   file:write ("  </o:shapelayout>\n")
   file:write ("  <v:shapetype id=\"_x0000_t202\" coordsize=\"21600,21600\" o:spt=\"202\" path=\"m,l,21600r21600,l21600,xe\">\n")
   file:write ("    <v:stroke joinstyle=\"miter\" />\n")
   file:write ("    <v:path gradientshapeok=\"t\" o:connecttype=\"rect\" />\n")
   file:write ("  </v:shapetype>\n")   

   for i, comment in ipairs (sheet[XLSX.consts.sheet.comments]) do
      -- Compute the number of lines in the comment
      local nb_lines = 1
      for ii = 1, string.len (comment[XLSX.consts.comment.text]) do
         if comment[XLSX.consts.comment.text]:byte(ii) == string.byte("\n", 1) then
            nb_lines = nb_lines + 1
         end
      end
            
      file:write ("  <v:shape id=\"_shape_"..i.."\" type=\"#_x0000_t202\" style='position:absolute;"..
                              "margin-left:0pt;margin-top:0pt;width:"..comment[XLSX.consts.comment.width].."pt;height:"..(13 * nb_lines).."pt;"..
                              "z-index:1;visibility:hidden;mso-wrap-style:tight' fillcolor=\""..comment[XLSX.consts.comment.color].."\" "..
                              "o:insetmode=\"auto\">\n")
      file:write ("    <v:fill color2=\"#ffffe1\"/>\n")
      file:write ("    <v:shadow color=\"black\" obscured=\"t\"/>\n")
      file:write ("    <v:path o:connecttype=\"none\"/>\n")
      file:write ("    <v:textbox style='mso-direction-alt:auto'>\n")
      file:write ("      <div style='text-align:left'></div>\n")
      file:write ("    </v:textbox>\n")
      
      file:write ("    <x:ClientData ObjectType=\"Note\">\n")
      file:write ("      <x:MoveWithCells/>\n")
      file:write ("      <x:SizeWithCells/>\n")
      
      -- According to the documentation:
      -- https://msdn.microsoft.com/en-us/library/documentformat.openxml.vml.spreadsheet.anchor%28v=office.14%29.aspx
      -- LeftColumn: The left anchor column of the object (left-most column is 0)
      -- LeftOffset: The offset of the object's left edge from the left edge of the left anchor column (in px)
      -- TopRow: The top anchor row of the object (top-most column is 0).
      -- TopOffset: The offset of the object's top edge from the top edge of the top anchor row (in px)
      -- RightColumn: The right anchor column of the object (left-most column is 0)
      -- RightOffset: The offset of the object's right edge from the left edge of the right anchor column (in px)
      -- BottomRow: The bottom anchor row of the object (top-most column is 0).
      -- BottomOffset: The offset of the object's bottom edge from the bottom edge of the bottom anchor row (in px)
--      file:write ("      <x:Anchor>1, 15, -1, 0, 6, 58, 1, 2</x:Anchor>\n")
      file:write ("      <x:AutoFill>False</x:AutoFill>\n")
      file:write ("      <x:Row>"..(comment[XLSX.consts.comment.y] - 1).."</x:Row>\n")
      file:write ("      <x:Column>"..(comment[XLSX.consts.comment.x] - 1).."</x:Column>\n")
      file:write ("    </x:ClientData>\n")
      file:write ("  </v:shape>\n")
   end
   file:write ("</xml>\n")
   file:close ()
   return 0
end




local function _create_xl (XLSXfile, tmp_dir_name)
   local file = nil
   -- -------------------------------------------------------------------------
   -- xl/
   lfs.mkdir (tmp_dir_name.."/xl")

   -- xl/styles.xml
    _create_styles_xml (XLSXfile, tmp_dir_name.."/xl/styles.xml")
   
   
   -- -------------------------------------------------------------------------
   -- xl/_rels/
   lfs.mkdir (tmp_dir_name.."/xl/_rels")
   
   -- xl/_rels/workbook.xml.rels
   file = io.open (tmp_dir_name.."/xl/_rels/workbook.xml.rels", "w")
   if file == nil then
      -- error message
      return -1
   end
   file:write ("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n")
   file:write ("<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n")  
   file:write ("  <Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles\" Target=\"styles.xml\"/>\n")
   if XLSXfile[XLSX.consts.file.nb_ustrings] > 0 then
      file:write ("  <Relationship Id=\"rId2\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/sharedStrings\" Target=\"sharedStrings.xml\"/>\n")
   end
   for i, sheet in pairs (XLSXfile[XLSX.consts.file.sheets]) do
      sheet[XLSX.consts.sheet.relation_id] = "rId"..i + 2
      file:write ("  <Relationship Id=\""..sheet[XLSX.consts.sheet.relation_id].."\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet"..i..".xml\"/>\n")
   end
   file:write ("</Relationships>\n")  
   file:close ()

   
   -- xl/workbook.xml
   file = io.open (tmp_dir_name.."/xl/workbook.xml", "w")   
   if file == nil then
      -- error message
      return -1
   end
   file:write ("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n")
   file:write ("<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "..
               "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">\n")
   file:write ("  <fileVersion appName=\"Calc\"/>\n")
   file:write ("  <workbookPr backupFile=\"false\" showObjects=\"all\" date1904=\"false\"/>\n")
   file:write ("  <workbookProtection/>\n")
   file:write ("  <bookViews>\n")
   -- workbookView is used to define the application windows size (windowHeight, windowWidth)
   -- and its position on the screen (xWindow, yWindows)
   file:write ("    <workbookView activeTab=\"0\" firstSheet=\"0\" showHorizontalScroll=\"true\" showSheetTabs=\"true\" "..
                                  "showVerticalScroll=\"true\" tabRatio=\"442\" windowHeight=\"8192\" windowWidth=\"16384\" "..
                                  "xWindow=\"0\" yWindow=\"0\"/>\n")
   file:write ("  </bookViews>\n")
   file:write ("  <sheets>\n")
   -- Declare one file per sheet
   for i, sheet in pairs (XLSXfile[XLSX.consts.file.sheets]) do
      -- name is the name of the sheet
      file:write ("    <sheet name=\""..sheet[XLSX.consts.sheet.name].."\" sheetId=\""..i.."\" state=\"visible\" r:id=\""..sheet[XLSX.consts.sheet.relation_id].."\"/>\n")
   end
   file:write ("  </sheets>\n")
   file:write ("  <calcPr iterateCount=\"100\" refMode=\"A1\" iterate=\"false\" iterateDelta=\"0.001\"/>\n")
   file:write ("</workbook>\n")
   file:close ()


   -- xl/sharedStrings.xml
   if XLSXfile[XLSX.consts.file.nb_ustrings] > 0 then
      file = io.open (tmp_dir_name.."/xl/sharedStrings.xml", "w")
      if file == nil then
         -- error message
         return -1
      end
      file:write ("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n")
      file:write ("<sst count=\""..XLSXfile[XLSX.consts.file.nb_strings].."\" "..
                  "uniqueCount=\""..XLSXfile[XLSX.consts.file.nb_ustrings].."\" "..
                  "xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">\n")
      for idx, str in pairs (XLSXfile[XLSX.consts.file.ostrings]) do
         file:write ("  <si>\n")
         file:write ("    <t xml:space=\"preserve\">"..XLSX:escape_str(str).."</t>\n")
         file:write ("  </si>\n")
      end
      file:write ("</sst>\n")
      file:close ()
   end
   
   
   -- -------------------------------------------------------------------------
   -- xl/worksheets/
   lfs.mkdir (tmp_dir_name.."/xl/worksheets")

   local is_worksheet_rels = false
   for i, sheet in pairs (XLSXfile[XLSX.consts.file.sheets]) do
      -- if needed, xl/worksheets/_rels/ 
      -- and xl/worksheets/_rels/sheet<i>.xml.rels
      if table.getn (sheet[XLSX.consts.sheet.graphs]) > 0
      or table.getn (sheet[XLSX.consts.sheet.comments]) > 0
      or sheet[XLSX.consts.sheet.is_remote_hyperlinks] == true then
         if is_worksheet_rels == false then
            -- xl/drawning/_rels/
            lfs.mkdir (tmp_dir_name.."/xl/worksheets/_rels/")
            is_worksheet_rels = true
         end
         -- xl/worksheets/_rels/sheet<i>.xml.rels
         _create_worksheet_rels (sheet, tmp_dir_name.."/xl/worksheets/_rels/sheet"..i..".xml.rels", i)
      end

      -- xl/worksheets/sheet..i.xml
      _create_sheetXml (XLSXfile, sheet, tmp_dir_name.."/xl/worksheets/sheet"..i..".xml", i)
   end
   
   if XLSXfile[XLSX.consts.file.is_graph] == true 
   or XLSXfile[XLSX.consts.file.is_comment] == true then
      -- xl/drawning/
      lfs.mkdir (tmp_dir_name.."/xl/drawings")
   end
   
   -- -------------------------------------------------------------------------
   -- Handle graphs
   if XLSXfile[XLSX.consts.file.is_graph] == true then
      -- xl/charts/
      lfs.mkdir (tmp_dir_name.."/xl/charts")
      
      -- xl/drawings/_rels/
      lfs.mkdir (tmp_dir_name.."/xl/drawings/_rels")
      
      -- graphs
      local nb = 1
      for i, sheet in pairs (XLSXfile[XLSX.consts.file.sheets]) do
         if table.getn (sheet[XLSX.consts.sheet.graphs]) > 0 then
            -- /xl/drawings/drawing<i>.xml"
            _create_drawing (tmp_dir_name.."/xl/drawings/drawing"..i..".xml", sheet)

            -- xl/drawings/_rels/drawing<i>.xml.rels
            file = io.open (tmp_dir_name.."/xl/drawings/_rels/drawing"..i..".xml.rels", "w")
            if file == nil then
               -- error message
               return -1
            end
            file:write ("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n")
            file:write ("<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n")
            
            for j, graph in pairs (sheet[XLSX.consts.sheet.graphs]) do
               file:write ("  <Relationship Id=\"rId"..j.."\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/chart\" "..
                           "Target=\"../charts/chart"..nb..".xml\"/>\n")
               graph:_save (tmp_dir_name.."/", nb, sheet)
               nb = nb + 1
            end
            
            file:write ("</Relationships>\n")
            file:close ()
         end         
      end
   end
   
   -- -------------------------------------------------------------------------
   -- Handle comments
   for i, sheet in pairs (XLSXfile[XLSX.consts.file.sheets]) do
      if table.getn (sheet[XLSX.consts.sheet.comments]) > 0 then
         -- xl/comments<i>.xml
         _create_comments (tmp_dir_name.."/xl/comments"..i..".xml", sheet)
         
         -- xl/drawings/vmlDrawing<i>.vml
         _create_vmlDrawing (tmp_dir_name.."/xl/drawings/vmlDrawing"..i..".vml", sheet, i)
      end
   end
end






local function _merge_data_from_sheets (XLSXfile)
   local hash = ""
   local font_id = 1
   local border_id = 1
   local fill_id = 1
   local style_id = 1
   
   local x_start = 0
   local y_start = 0
   local x_stop = 0
   local y_stop = 0
      
   for i, sheet in pairs (XLSXfile[XLSX.consts.file.sheets]) do  
      -- Normalize styles in merged cells
      for _, ref in pairs (sheet[XLSX.consts.sheet.mergedCells]) do
         x_start, y_start = XLSX:get_coordinates_from_text (string.match (ref, "^[A-Z]+[0-9]+"))
         x_stop,  y_stop  = XLSX:get_coordinates_from_text (string.match (ref, "[A-Z]+[0-9]+$"))

         for x = x_start, x_stop do
            for y = y_start, y_stop do
               -- Copy the cell style
               sheet[XLSX.consts.sheet.cells][y][x][XLSX.consts.text.bold] = 
                              sheet[XLSX.consts.sheet.cells][y_start][x_start][XLSX.consts.text.bold]
               sheet[XLSX.consts.sheet.cells][y][x][XLSX.consts.text.italic] =
                              sheet[XLSX.consts.sheet.cells][y_start][x_start][XLSX.consts.text.italic]
               sheet[XLSX.consts.sheet.cells][y][x][XLSX.consts.text.strike] =
                             sheet[XLSX.consts.sheet.cells][y_start][x_start][XLSX.consts.text.strike]
               sheet[XLSX.consts.sheet.cells][y][x][XLSX.consts.text.underline] =
                              sheet[XLSX.consts.sheet.cells][y_start][x_start][XLSX.consts.text.underline]
               sheet[XLSX.consts.sheet.cells][y][x][XLSX.consts.text.color] =
                              sheet[XLSX.consts.sheet.cells][y_start][x_start][XLSX.consts.text.color]
               sheet[XLSX.consts.sheet.cells][y][x][XLSX.consts.text.size] =
                              sheet[XLSX.consts.sheet.cells][y_start][x_start][XLSX.consts.text.size]
               sheet[XLSX.consts.sheet.cells][y][x][XLSX.consts.text.background] =
                              sheet[XLSX.consts.sheet.cells][y_start][x_start][XLSX.consts.text.background]
               sheet[XLSX.consts.sheet.cells][y][x][XLSX.consts.text.borders] =
                              sheet[XLSX.consts.sheet.cells][y_start][x_start][XLSX.consts.text.borders]
               sheet[XLSX.consts.sheet.cells][y][x][XLSX.consts.text.font] = 
                              sheet[XLSX.consts.sheet.cells][y_start][x_start][XLSX.consts.text.font]
               sheet[XLSX.consts.sheet.cells][y][x][XLSX.consts.text.alignment] =
                              sheet[XLSX.consts.sheet.cells][y_start][x_start][XLSX.consts.text.alignment]
               sheet[XLSX.consts.sheet.cells][y][x][XLSX.consts.text.bg_tint] =
                              sheet[XLSX.consts.sheet.cells][y_start][x_start][XLSX.consts.text.bg_tint]
            end
         end

      end
      
      -- Load sheets data in the file
      for j, str in pairs (sheet [XLSX.consts.sheet.ostrings]) do
         if XLSXfile[XLSX.consts.file.ustrings][str] == nil then
            XLSXfile[XLSX.consts.file.nb_ustrings] = XLSXfile[XLSX.consts.file.nb_ustrings] + 1
            XLSXfile[XLSX.consts.file.ustrings][str] = XLSXfile[XLSX.consts.file.nb_ustrings]
            XLSXfile[XLSX.consts.file.ostrings][XLSXfile[XLSX.consts.file.nb_ustrings]] = str
         end
         XLSXfile[XLSX.consts.file.nb_strings] = XLSXfile[XLSX.consts.file.nb_strings] + 1         
      end

      -- Load style data in the file            
      for j = 1, sheet[XLSX.consts.sheet.column_max] do
         for i = 1, table.getn(sheet[XLSX.consts.sheet.cells]) do
            cell = sheet[XLSX.consts.sheet.cells][i][j]
            if cell ~= nil then
               -- Fonts
               hash = _hash_font (cell)
               if XLSXfile[XLSX.consts.file.fonts][hash] == nil then
                  XLSXfile[XLSX.consts.file.fonts][hash] = _extract_font (cell)
                  XLSXfile[XLSX.consts.file.ofonts][font_id] = hash
                  font_id = font_id + 1
               end

               -- Borders
               hash = _hash_border (cell)
               if XLSXfile[XLSX.consts.file.borders][hash] == nil then
                  XLSXfile[XLSX.consts.file.borders][hash] = _extract_border (cell)
                  XLSXfile[XLSX.consts.file.oborders][border_id] = hash
                  border_id = border_id + 1
               end
               
               -- Fills
               hash = _hash_fill (cell)
               if XLSXfile[XLSX.consts.file.fills][hash] == nil then
                  XLSXfile[XLSX.consts.file.fills][hash] = _extract_fill (cell)
                  XLSXfile[XLSX.consts.file.ofills][fill_id] = hash
                  fill_id = fill_id + 1
               end
               
               -- Alignment
               hash = _hash_align (cell)
               if XLSXfile[XLSX.consts.file.alignments][hash] == nil then
                  XLSXfile[XLSX.consts.file.alignments][hash] = _extract_align (cell)
               end
            end     
         end
      end
   
      -- Check if there is at least one graph
      if table.getn (sheet[XLSX.consts.sheet.graphs]) > 0 then
         XLSXfile[XLSX.consts.file.is_graph] = true
      end

      -- Check if there is at least one comment
      if table.getn (sheet[XLSX.consts.sheet.comments]) > 0 then
         XLSXfile[XLSX.consts.file.is_comment] = true
      end
   end
   
   -- Here, generate all possible styles
   for i, font in ipairs (XLSXfile[XLSX.consts.file.ofonts]) do
      for j, border in ipairs (XLSXfile[XLSX.consts.file.oborders]) do
         for k, fill in ipairs (XLSXfile[XLSX.consts.file.ofills]) do
            for align, val in pairs (XLSXfile[XLSX.consts.file.alignments]) do
               hash = "["..font.."]["..border.."]["..fill.."]["..align.."]"

               XLSXfile[XLSX.consts.file.styles][hash] = {}
               XLSXfile[XLSX.consts.file.styles][hash][XLSX.consts.style.font_id]   = i - 1
               XLSXfile[XLSX.consts.file.styles][hash][XLSX.consts.style.border_id] = j - 1
               XLSXfile[XLSX.consts.file.styles][hash][XLSX.consts.style.fill_id]   = k - 1
               XLSXfile[XLSX.consts.file.styles][hash][XLSX.consts.style.id]        = style_id
               XLSXfile[XLSX.consts.file.styles][hash][XLSX.consts.style.align]     = val
               XLSXfile[XLSX.consts.file.ostyles][style_id] = hash
               style_id = style_id + 1
            end
         end
      end
   end   
end





--- Save the current structure in a file
function XLSX_file:save ()
   _merge_data_from_sheets (self)  
   
   -- Check if the file's path exists
   if fs.exists (self[XLSX.consts.file.path]) == false then
      -- error message
      return
   end
   
   -- Create tmp directory
   local tmp_dir_name = self[XLSX.consts.file.path].."/"..self[XLSX.consts.file.name]
   lfs.mkdir (tmp_dir_name)
   _create__rels (self, tmp_dir_name)
   _create_DocProps (self, tmp_dir_name)
   _create_xl (self, tmp_dir_name)
   _create_ContentTypes_XML (self, tmp_dir_name)
   
   -- Create the archive
   local xlsx_name      = tmp_dir_name..self[XLSX.consts.file.extension]
   os.execute ("cd "..tmp_dir_name.."; zip -rq "..xlsx_name.." ./*")
   os.execute ("rm -r "..tmp_dir_name)
end






