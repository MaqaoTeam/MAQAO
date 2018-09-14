---
--  Copyright (C) 2004 - 2018 Université de Versailles Saint-Quentin-en-Yvelines (UVSQ)
--
--  This file is part of MAQAO.
--
--  MAQAO is free software; you can redistribute it and/or
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

--- @class TextOutput Class

TextOutput = {}

--[[
Calling order of the following functions to print an array:
1) get_columns_width
2) print_array_centered_text if needed
3) print_column_headers
4) for _, dataRow in ipairs(array) do
      print_array_line(dataRow, ...)
   end
5) print_horizontal_border (bottom border only, top border included in step 3)
--]]

-- Constants
TextOutput.STRING_MAX_LENGTH = 50
TextOutput.ARRAY_OUTSIDE_BORDER = "#"
TextOutput.ARRAY_INSIDE_BORDER = "|"

--[[
Gets the width of each column of an array.
The width of a column is the length of its longest string.

@param: columnHeaders = 1D table of which each element is a column header
@param: array = the array made of the columns to search in

@return: table of which i-th element = width of i-th column
--]]
function TextOutput:get_columns_width(columnHeaders, array)

   local columnsWidth = {}

   -- initialize width of each column to its header length
   for idxColumn, header in ipairs(columnHeaders) do
      columnsWidth[idxColumn] = string.len(header)
   end

   -- search longest string of each column
   for idxRow, row in ipairs(array) do
      for idxColumn, str in ipairs(row) do
         local strLength = string.len(tostring(str))
         -- compare length to update width if needed
         if ((columnsWidth[idxColumn] ~= nil) and (strLength ~= nil)) then
            if (strLength > TextOutput.STRING_MAX_LENGTH) then
               columnsWidth[idxColumn] = math.max(columnsWidth[idxColumn], TextOutput.STRING_MAX_LENGTH)
            else
               columnsWidth[idxColumn] = math.max(columnsWidth[idxColumn], strLength)
            end
         end
      end
   end

   return columnsWidth

end

--[[
Gets the total width of an array (= 1st line's width).

@param: columnsWidth = table returned by get_columns_width() function
@param: columnMargin = margin added on both the left and right of a header

@return: total width of the array
--]]
function TextOutput:get_array_width(columnsWidth, columnMargin)

   local totalWidth = 1 -- left border character
   for _, columnWidth in ipairs(columnsWidth) do
      -- add total width of current column: default width + left & right margins + 1 (inside border)
      totalWidth = totalWidth + columnWidth + 2*columnMargin + 1
   end
   
   return totalWidth

end

--[[
Prints a horizontal border of an array.

@param: columnHeaders = 1D table of which each element is a column header
@param: columnsWidth = table returned by get_columns_width() function
@param: columnMargin = margin added on both the left and right of a header
--]]
function TextOutput:print_horizontal_border(columnHeaders, columnsWidth, columnMargin)

   local arrayWidth = TextOutput:get_array_width(columnsWidth, columnMargin)
   io.stdout:write(string.rep(TextOutput.ARRAY_OUTSIDE_BORDER, arrayWidth))
   io.stdout:write("\n")
   
end

--[[
Prints a text centered on an array.

@param: columnHeaders = 1D table of which each element is a column header
@param: columnsWidth = table returned by get_columns_width() function
@param: columnMargin = margin added on both the left and right of a header
@param: text = text to print
--]]
function TextOutput:print_array_centered_text(columnHeaders, columnsWidth, columnMargin, text)

   local arrayWidth = TextOutput:get_array_width(columnsWidth, columnMargin)
   local marginToCenter = arrayWidth/2 - string.len(text)/2;
   io.stdout:write(string.rep(" ", marginToCenter)..tostring(text).."\n");
   
end

--[[
Prints the headers (= 1st row) of an array.

@param: columnHeaders = 1D table of which each element is a column header
@param: columnsWidth = table returned by get_columns_width() function
@param: columnMargin = margin added on both the left and right of a header

Ex. w/ "#" and "|" as outside/inside border character:
###############################
# header1 | header2 | header3 #
###############################
--]]
function TextOutput:print_column_headers(columnHeaders, columnsWidth, columnMargin)

   -- print top border + left border character of 2nd line:
   TextOutput:print_horizontal_border(columnHeaders, columnsWidth, columnMargin)
   io.stdout:write(TextOutput.ARRAY_OUTSIDE_BORDER)
   
   -- print headers one by one
   for idxColumn, header in ipairs(columnHeaders) do
      -- margins to align content of cell
      leftMargin = math.floor((columnsWidth[idxColumn] - string.len(header) + 2*columnMargin)/2)
      rightMargin = math.ceil((columnsWidth[idxColumn] - string.len(header) + 2*columnMargin)/2)
      -- select right border character depending on whether current column is the last one
      -- not last: print "header |"
      -- last: print "header #"
      if (idxColumn < #columnHeaders) then
         io.stdout:write(string.rep(" ",leftMargin)..header..string.rep(" ",rightMargin)..TextOutput.ARRAY_INSIDE_BORDER)
      else
         io.stdout:write(string.rep(" ",leftMargin)..header..string.rep(" ",rightMargin)..TextOutput.ARRAY_OUTSIDE_BORDER)
      end
   end

   -- print bottom border of 1st line
   io.stdout:write("\n")
   TextOutput:print_horizontal_border(columnHeaders, columnsWidth, columnMargin)

end

--[[
Prints a full line of an array.

@param: dataRow = 1D table of which each element is a column value
@param: columnsWidth = table returned by get_columns_width() function
@param: columnMargin = margin added on both the left and right of a value

Ex. w/ "#" and "|" as outside/inside border character, each line is like:
# value1 | value2 | value3 | ... | valueN #
--]]
function TextOutput:print_array_line(dataRow, columnsWidth, columnMargin)

   -- padding to complete right margin to align columns
   local paddingToAlign = 0
   -- print left border character of the line
   io.stdout:write(TextOutput.ARRAY_OUTSIDE_BORDER);
   
   for idxColumn, value in ipairs(dataRow) do
      paddingToAlign = columnsWidth[idxColumn] - string.len(value)
      -- if columns have a limited width:
      -- truncate string to print in current column
      local valueToPrint = string.sub(tostring(value), 1, TextOutput.STRING_MAX_LENGTH)
      
      -- select right border character depending on whether current column is the last one
      -- not last: print "value |"
      -- last: print "value #"
      if (idxColumn < #dataRow) then
         io.stdout:write(string.rep(" ",columnMargin)..valueToPrint..string.rep(" ",paddingToAlign)..string.rep(" ",columnMargin)..TextOutput.ARRAY_INSIDE_BORDER)
      else
         io.stdout:write(string.rep(" ",columnMargin)..valueToPrint..string.rep(" ",paddingToAlign)..string.rep(" ",columnMargin)..TextOutput.ARRAY_OUTSIDE_BORDER)
      end
   end
   io.stdout:write("\n")
   
end

--[[
Exports 2D table to CSV file.

@param: filename = name to give to the file
@param: separator = character to use as separator (ex. ";")
@param: columnsHeaders = 1D table of which each element is a column header
@param: array = 2D table of which each line is a line of the CSV file
--]]
function TextOutput:export_to_csv(filename, separator, columnHeaders, array)

   if (filename ~= nil) then
      local file, errorMessage = io.open(filename..".csv","w")
      if (file ~= nil) then
         src_info_idx = 0
         if (columnHeaders ~= nil) then
            for _, header in ipairs (columnHeaders) do
               file:write(header..separator)
            end
            file:write("\n")
         end
         for _, row in ipairs(array) do
            for _, value in ipairs(row) do
               if (value ~= "-1") then
                  file:write(value..separator)
               else
                  file:write(""..separator)
               end
            end
            file:write("\n")
         end
         Message:info("The CSV file "..file_name..".csv has been generated")
         file:close()
      else
         Message:info("Impossible to create CSV file: "..errorMessage)
      end
   else
      Debug:info("Missing filename")
   end
   
end
